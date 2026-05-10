#include "sentinel/app/pipeline.hpp"

#include "sentinel/analytics/event_builder.hpp"
#include "sentinel/inference/detector_factory.hpp"
#include "sentinel/output/video_sink_factory.hpp"
#include "sentinel/perf/performance_stats.hpp"
#include "sentinel/preprocess/preprocessor_factory.hpp"
#include "sentinel/video/video_source_factory.hpp"

#include <chrono>
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace sentinel {
namespace {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

/**
 * @brief 计算从起点到当前时刻的毫秒数。
 * @param start 起始时间点。
 * @return 已经过的毫秒数。
 */
double elapsed_ms_since(TimePoint start)
{
    const auto elapsed = Clock::now() - start;
    return std::chrono::duration<double, std::milli>(elapsed).count();
}

/**
 * @brief 从配置中找到第一路启用的摄像头。
 * @param cameras 摄像头配置列表。
 * @return 第一条启用状态的摄像头配置。
 */
CameraConfig first_enabled_camera(const std::vector<CameraConfig>& cameras)
{
    for (const auto& camera : cameras) {
        if (camera.enabled) {
            return camera;
        }
    }

    throw std::runtime_error("no enabled camera configured");
}

/**
 * @brief 判断当前推理后端是否需要真实图像预处理。
 * @param inference 推理后端配置。
 * @return mock 后端返回 `false`，真实推理后端返回 `true`。
 */
bool needs_frame_preprocessor(const InferenceConfig& inference)
{
    return inference.backend != "mock";
}

/**
 * @brief 为 mock 推理构造只携带元数据的张量。
 * @param frame 视频源输出的原始帧。
 * @return 用于驱动 mock 检测器的轻量张量对象。
 */
TensorBuffer make_metadata_tensor(const Frame& frame)
{
    TensorBuffer tensor;
    tensor.frame_sequence = frame.sequence;
    tensor.camera_id = frame.camera_id;
    return tensor;
}

/**
 * @brief 生成 V4L2 fourcc 可读文本。
 * @param pixel_format 像素格式整数。
 * @return fourcc 字符串；若格式为 0 则返回 `"unknown"`。
 */
std::string pixel_format_to_string(std::uint32_t pixel_format)
{
    if (pixel_format == 0U) {
        return "unknown";
    }

    std::string value(4, ' ');
    value[0] = static_cast<char>(pixel_format & 0xFFU);
    value[1] = static_cast<char>((pixel_format >> 8U) & 0xFFU);
    value[2] = static_cast<char>((pixel_format >> 16U) & 0xFFU);
    value[3] = static_cast<char>((pixel_format >> 24U) & 0xFFU);
    return value;
}

/**
 * @brief 将张量形状转换为日志文本。
 * @param shape 张量维度列表。
 * @return 形如 `[1,3,640,640]` 的文本。
 */
std::string shape_to_string(const std::vector<int>& shape)
{
    std::string text{"["};
    for (std::size_t index = 0; index < shape.size(); ++index) {
        if (index > 0U) {
            text += ",";
        }
        text += std::to_string(shape[index]);
    }
    text += "]";
    return text;
}

/**
 * @brief 生成视频帧调试摘要。
 * @param frame 视频源输出的帧。
 * @return 包含尺寸、像素格式和字节数的摘要文本。
 */
std::string make_frame_debug_message(const Frame& frame)
{
    return "camera frame sequence=" + std::to_string(frame.sequence) +
           " camera_id=" + frame.camera_id +
           " size=" + std::to_string(frame.width) + "x" + std::to_string(frame.height) +
           " pixel_format=" + pixel_format_to_string(frame.pixel_format) +
           " bytes_used=" + std::to_string(frame.bytes_used) +
           " payload_bytes=" + std::to_string(frame.payload_size()) +
           " payload_mode=" + (frame.is_loaned() ? "loaned" : "owned") +
           " timestamp_ns=" + std::to_string(frame.timestamp_ns);
}

/**
 * @brief 生成模型输入张量调试摘要。
 * @param tensor 预处理后的模型输入张量。
 * @return 包含 shape、layout、dtype 和字节数的摘要文本。
 */
std::string make_tensor_debug_message(const TensorBuffer& tensor)
{
    if (tensor.shape.empty()) {
        return "detector metadata frame_sequence=" + std::to_string(tensor.frame_sequence) +
               " camera_id=" + tensor.camera_id + " tensor=not_required_for_mock";
    }

    return "preprocess tensor frame_sequence=" + std::to_string(tensor.frame_sequence) +
           " camera_id=" + tensor.camera_id +
           " shape=" + shape_to_string(tensor.shape) +
           " layout=" + tensor.layout +
           " dtype=" + tensor.dtype +
           " memory=" + (tensor.is_device() ? "device" : "host") +
           " bytes=" + std::to_string(tensor.byte_size());
}

/**
 * @brief 根据预处理配置生成目标张量元数据。
 * @param config 预处理配置。
 * @param frame 当前视频帧。
 * @return 只包含 shape、layout、dtype 和来源帧信息的张量元数据。
 */
TensorBuffer make_preprocess_target_metadata(const PreprocessConfig& config, const Frame& frame)
{
    TensorBuffer metadata;
    metadata.layout = config.output_layout;
    metadata.dtype = config.output_dtype;
    metadata.frame_sequence = frame.sequence;
    metadata.camera_id = frame.camera_id;

    if (config.output_layout == "NV12" && config.output_dtype == "UINT8") {
        metadata.shape = {1, 1, config.output_height, config.output_width};
    } else {
        metadata.shape = {1, 3, config.output_height, config.output_width};
    }
    return metadata;
}

/**
 * @brief 解析性能 CSV 输出路径。
 * @param service 服务配置，提供运行数据根目录。
 * @param csv_path 配置中的 CSV 路径。
 * @return 空路径表示不输出 CSV；相对路径会解析到 `service.data_dir` 下。
 */
std::filesystem::path resolve_perf_csv_path(const ServiceConfig& service,
                                            const std::filesystem::path& csv_path)
{
    if (csv_path.empty()) {
        return {};
    }
    if (csv_path.is_absolute()) {
        return csv_path;
    }
    return service.data_dir / csv_path;
}

/**
 * @brief 打开性能 CSV 文件并写入表头。
 * @param path CSV 输出路径。
 * @param logger 日志策略对象。
 * @return 打开的文件流；路径为空或打开失败时返回未打开流。
 */
std::ofstream open_perf_csv(const std::filesystem::path& path, Logger& logger)
{
    std::ofstream file;
    if (path.empty()) {
        return file;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        logger.warn("unable to create performance csv directory: " + error.message());
        return file;
    }

    file.open(path, std::ios::out | std::ios::trunc);
    if (!file) {
        logger.warn("unable to open performance csv: " + path.string());
        return file;
    }

    file << PerformanceStats::csv_header();
    logger.info("performance csv_path=" + path.string());
    return file;
}

/**
 * @brief 在无需输出日志的场景下提供一个空实现。
 */
class NullLogger final : public Logger {
public:
    /**
     * @brief 构造空日志器。
     */
    NullLogger()
        : Logger(LogLevel::kError)
    {
    }

private:
    /**
     * @brief 丢弃所有通过基类过滤后的日志消息。
     * @param level 本条消息的严重级别。
     * @param message 要输出的日志内容。
     */
    void write(LogLevel level, std::string_view message) override
    {
        static_cast<void>(level);
        static_cast<void>(message);
    }
};

/**
 * @brief 采集线程发布给推理线程的帧包。
 */
struct CapturedFramePacket {
    Frame frame;
    double capture_ms{0.0};
    TimePoint frame_start;
};

/**
 * @brief 推理线程发布给输出线程的绑定结果包。
 *
 * 该结构把原始帧和同一帧的检测结果绑定在一起，避免输出线程把
 * frame N 的检测框画到 frame N+k 上。
 */
struct PipelineOutputPacket {
    Frame frame;
    std::vector<Detection> detections;
};

/**
 * @brief latest-frame 单槽缓冲区。
 *
 * 采集线程只保留最新帧；当推理线程消费速度低于采集速度时，旧帧会被
 * 覆盖并释放其 V4L2 loaned 租约，从而把缓冲区尽快归还给驱动。
 */
class LatestFrameSlot {
public:
    /**
     * @brief 发布一帧最新采集结果。
     * @param packet 待发布的帧包，会移动进单槽缓冲区。
     */
    void publish(CapturedFramePacket packet)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            latest_ = std::move(packet);
            ++generation_;
        }
        condition_.notify_all();
    }

    /**
     * @brief 等待并取出比指定版本更新的帧。
     * @param last_generation 调用方上一次消费到的版本号。
     * @param stop_requested 全局停止标志。
     * @return 有新帧时返回帧包和版本号；关闭或停止后返回空。
     */
    std::optional<std::pair<CapturedFramePacket, std::uint64_t>> wait_newer(
        std::uint64_t last_generation,
        const std::atomic_bool& stop_requested)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [&]() {
            return closed_ || stop_requested.load() || generation_ != last_generation;
        });

        if (stop_requested.load() || !latest_.has_value() || generation_ == last_generation) {
            return std::nullopt;
        }

        return std::make_pair(*latest_, generation_);
    }

    /**
     * @brief 关闭单槽缓冲区并唤醒等待线程。
     */
    void close()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        condition_.notify_all();
    }

    /**
     * @brief 判断单槽缓冲区是否已经关闭。
     * @return 已关闭返回 `true`。
     */
    bool closed() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::optional<CapturedFramePacket> latest_;
    std::uint64_t generation_{0};
    bool closed_{false};
};

/**
 * @brief 输出线程使用的有界队列。
 *
 * 当输出端慢于推理端时，队列会丢弃最旧的结果包，优先保留最新画面，
 * 避免 MJPEG 调试预览反向拖慢推理主链路。
 */
class BoundedOutputQueue {
public:
    /**
     * @brief 构造指定容量的输出队列。
     * @param capacity 队列容量，必须大于 0。
     */
    explicit BoundedOutputQueue(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    /**
     * @brief 推入一条输出包，满队列时丢弃最旧包。
     * @param packet 待写入的输出包。
     */
    void push(PipelineOutputPacket packet)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_) {
                return;
            }
            while (queue_.size() >= capacity_) {
                queue_.pop_front();
            }
            queue_.push_back(std::move(packet));
        }
        condition_.notify_one();
    }

    /**
     * @brief 等待并弹出一条输出包。
     * @return 队列关闭且无剩余数据时返回空。
     */
    std::optional<PipelineOutputPacket> pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [&]() {
            return closed_ || !queue_.empty();
        });

        if (queue_.empty()) {
            return std::nullopt;
        }

        auto packet = std::move(queue_.front());
        queue_.pop_front();
        return packet;
    }

    /**
     * @brief 关闭队列并唤醒输出线程。
     */
    void close()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        condition_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<PipelineOutputPacket> queue_;
    std::size_t capacity_{1};
    bool closed_{false};
};

/**
 * @brief 推理线程内部记录的 AscendCL stream slot 状态。
 */
struct PipelineStreamSlot {
    bool busy{false};
    CapturedFramePacket captured;
    double preprocess_ms{0.0};
};

/**
 * @brief 用两个 AscendCL stream slot 运行线程化主线流水线。
 * @param config 已通过校验的应用配置。
 * @param video_source 视频源策略对象，仅由采集线程访问。
 * @param logger 日志策略对象。
 * @param stop_requested 外部停止回调，用于响应信号。
 * @return 本次流水线运行结果。
 */
PipelineResult run_threaded_pipeline(const SentinelConfig& config,
                                     VideoSource& video_source,
                                     Logger& logger,
                                     const std::function<bool()>& stop_requested)
{
    LatestFrameSlot latest_frame;
    BoundedOutputQueue output_queue(static_cast<std::size_t>(config.pipeline.output_queue_size));
    std::atomic_bool stop{false};
    std::mutex error_mutex;
    std::string thread_error;
    PipelineResult result;
    const bool output_enabled = config.output.video_sink != "none";

    const auto request_stop = [&]() {
        stop.store(true);
        latest_frame.close();
        output_queue.close();
    };

    const auto set_thread_error = [&](std::string message) {
        {
            std::lock_guard<std::mutex> lock(error_mutex);
            if (thread_error.empty()) {
                thread_error = std::move(message);
            }
        }
        request_stop();
    };

    logger.info("pipeline threaded mode enabled stream_slots=" +
                std::to_string(config.pipeline.stream_slots) +
                " detect_fps=" + std::to_string(config.pipeline.detect_fps) +
                " output_queue_size=" + std::to_string(config.pipeline.output_queue_size));

    std::thread capture_thread([&]() {
        try {
            if (!video_source.open()) {
                set_thread_error("unable to open " + std::string(video_source.kind()) +
                                 " source: " + std::string(video_source.last_error()));
                return;
            }

            while (!stop.load()) {
                if (stop_requested && stop_requested()) {
                    logger.warn("pipeline stop requested by external signal");
                    request_stop();
                    break;
                }

                const auto frame_start = Clock::now();
                const auto frame = video_source.read_frame();
                const auto capture_ms = elapsed_ms_since(frame_start);
                if (!frame.has_value()) {
                    if (!video_source.last_error().empty()) {
                        set_thread_error("video source read failed: " +
                                         std::string(video_source.last_error()));
                    } else {
                        logger.info("video source returned no frame, capture thread will stop");
                        request_stop();
                    }
                    break;
                }

                logger.debug(make_frame_debug_message(*frame));
                latest_frame.publish(CapturedFramePacket{*frame, capture_ms, frame_start});
            }

            video_source.close();
            latest_frame.close();
        } catch (const std::exception& error) {
            video_source.close();
            set_thread_error("capture thread failed: " + std::string(error.what()));
        }
    });

    std::thread inference_thread([&]() {
        std::unique_ptr<FramePreprocessor> preprocessor;
        std::unique_ptr<Detector> detector;
        std::ofstream performance_csv;
        const auto close_resources = [&]() {
            if (detector) {
                detector->close();
            }
            if (preprocessor) {
                preprocessor->close();
            }
        };

        try {
            if (needs_frame_preprocessor(config.inference)) {
                preprocessor = create_frame_preprocessor(config.preprocess);
                logger.info("open frame preprocessor backend=" +
                            std::string(preprocessor->kind()) +
                            " layout=" + config.preprocess.output_layout +
                            " dtype=" + config.preprocess.output_dtype);
                if (!preprocessor->open()) {
                    set_thread_error("unable to open " + std::string(preprocessor->kind()) +
                                     " preprocessor: " +
                                     std::string(preprocessor->last_error()));
                    close_resources();
                    return;
                }
            }

            detector = create_detector(config.inference, config.rules, config.postprocess);
            if (!detector->open()) {
                set_thread_error("unable to open " + std::string(detector->kind()) +
                                 " detector: " + std::string(detector->last_error()));
                close_resources();
                return;
            }

            if (detector->async_slot_count() <
                static_cast<std::size_t>(config.pipeline.stream_slots)) {
                set_thread_error("detector async slot count is smaller than pipeline.stream_slots");
                close_resources();
                return;
            }

            EventBuilder event_builder(config.rules);
            PerformanceStats performance_stats(config.performance.log_interval_frames);
            if (config.performance.enabled) {
                performance_csv = open_perf_csv(resolve_perf_csv_path(config.service,
                                                                     config.performance.csv_path),
                                                logger);
                logger.info("performance logging enabled interval_frames=" +
                            std::to_string(config.performance.log_interval_frames));
            }

            std::array<PipelineStreamSlot, 2> stream_slots;
            std::uint64_t last_generation = 0;
            int submitted_frames = 0;
            const auto submit_interval = std::chrono::duration<double>(
                1.0 / static_cast<double>(config.pipeline.detect_fps));
            auto next_submit_time = Clock::now();

            const auto collect_slot = [&](std::size_t slot_index) -> bool {
                auto& slot = stream_slots[slot_index];
                if (!slot.busy) {
                    return true;
                }

                const auto detect_start = Clock::now();
                auto async_result = detector->collect_async(slot_index);
                const auto detect_ms = elapsed_ms_since(detect_start);
                slot.busy = false;
                if (!async_result.has_value()) {
                    set_thread_error("detector async collect failed: " +
                                     std::string(detector->last_error()));
                    return false;
                }

                if (!async_result->debug_info.empty()) {
                    logger.debug("detector result: " + async_result->debug_info);
                }

                result.frames_processed += 1;
                result.detections_seen += static_cast<int>(async_result->detections.size());
                auto events = event_builder.observe(slot.captured.frame,
                                                    async_result->detections);
                result.events.insert(result.events.end(), events.begin(), events.end());

                if (config.performance.enabled) {
                    FramePerformanceSample sample;
                    sample.frame_sequence = slot.captured.frame.sequence;
                    sample.detections = static_cast<int>(async_result->detections.size());
                    sample.capture_ms = slot.captured.capture_ms;
                    sample.preprocess_ms = slot.preprocess_ms;
                    sample.detect_ms = detect_ms;
                    sample.output_ms = 0.0;
                    sample.frame_total_ms = elapsed_ms_since(slot.captured.frame_start);
                    performance_stats.observe(sample);
                    if (performance_csv) {
                        performance_csv << PerformanceStats::csv_row(sample);
                    }
                    if (performance_stats.should_report()) {
                        logger.info(performance_stats.make_report_and_reset());
                    }
                }

                if (output_enabled) {
                    output_queue.push(PipelineOutputPacket{
                        std::move(slot.captured.frame),
                        std::move(async_result->detections),
                    });
                }
                slot.captured = CapturedFramePacket{};
                slot.preprocess_ms = 0.0;
                return true;
            };

            while (!stop.load() && result.frames_processed < config.pipeline.max_frames) {
                for (std::size_t slot_index = 0;
                     slot_index < static_cast<std::size_t>(config.pipeline.stream_slots) &&
                     submitted_frames < config.pipeline.max_frames;
                     ++slot_index) {
                    auto& slot = stream_slots[slot_index];
                    if (slot.busy) {
                        continue;
                    }

                    std::this_thread::sleep_until(next_submit_time);
                    next_submit_time = Clock::now() +
                                       std::chrono::duration_cast<Clock::duration>(
                                           submit_interval);

                    auto captured = latest_frame.wait_newer(last_generation, stop);
                    if (!captured.has_value()) {
                        break;
                    }
                    last_generation = captured->second;
                    slot.captured = std::move(captured->first);

                    TensorBuffer tensor;
                    double preprocess_ms = 0.0;
                    if (preprocessor) {
                        const auto preprocess_start = Clock::now();
                        const auto target_metadata = make_preprocess_target_metadata(
                            config.preprocess,
                            slot.captured.frame);
                        auto target_tensor = detector->mutable_input_tensor_for_slot(
                            target_metadata,
                            slot_index);
                        if (!target_tensor.has_value()) {
                            set_thread_error("detector slot input tensor unavailable");
                            return;
                        }
                        auto* native_stream = detector->native_stream_for_slot(slot_index);
                        if (native_stream == nullptr) {
                            set_thread_error("detector native stream unavailable for slot");
                            return;
                        }
                        auto processed = preprocessor->process_into_slot(slot.captured.frame,
                                                                         std::move(*target_tensor),
                                                                         slot_index,
                                                                         native_stream);
                        preprocess_ms = elapsed_ms_since(preprocess_start);
                        if (!processed.has_value()) {
                            set_thread_error("frame preprocessor failed: " +
                                             std::string(preprocessor->last_error()));
                            return;
                        }
                        tensor = std::move(*processed);
                    } else {
                        tensor = make_metadata_tensor(slot.captured.frame);
                    }
                    logger.debug(make_tensor_debug_message(tensor));

                    if (!detector->submit_async(slot_index, tensor)) {
                        set_thread_error("detector async submit failed: " +
                                         std::string(detector->last_error()));
                        return;
                    }
                    slot.busy = true;
                    slot.preprocess_ms = preprocess_ms;
                    ++submitted_frames;
                }

                bool collected_any = false;
                for (std::size_t slot_index = 0;
                     slot_index < static_cast<std::size_t>(config.pipeline.stream_slots);
                     ++slot_index) {
                    if (stream_slots[slot_index].busy) {
                        if (!collect_slot(slot_index)) {
                            return;
                        }
                        collected_any = true;
                    }
                }

                if (!collected_any && latest_frame.closed()) {
                    break;
                }
            }

            for (std::size_t slot_index = 0;
                 slot_index < static_cast<std::size_t>(config.pipeline.stream_slots);
                 ++slot_index) {
                if (!collect_slot(slot_index)) {
                    return;
                }
            }

            if (config.performance.enabled && performance_stats.has_pending_samples()) {
                logger.info(performance_stats.make_report_and_reset());
            }

            output_queue.close();
            request_stop();
            close_resources();
        } catch (const std::exception& error) {
            close_resources();
            set_thread_error("inference thread failed: " + std::string(error.what()));
        }
    });

    std::thread output_thread;
    if (output_enabled) {
        output_thread = std::thread([&]() {
            try {
                auto video_sink = create_video_sink(config.output, config.overlay, config.service);
                logger.info("open video sink type=" + std::string(video_sink->kind()) +
                            " overlay=" + (config.overlay.enabled ? "enabled" : "disabled") +
                            " overlay_backend=" + config.overlay.backend);
                if (!video_sink->open()) {
                    set_thread_error("unable to open " + std::string(video_sink->kind()) +
                                     " video sink: " + std::string(video_sink->last_error()));
                    return;
                }

                while (true) {
                    auto packet = output_queue.pop();
                    if (!packet.has_value()) {
                        break;
                    }
                    if (!video_sink->write(packet->frame, packet->detections)) {
                        set_thread_error("video sink failed: " +
                                         std::string(video_sink->last_error()));
                        break;
                    }
                }

                video_sink->close();
            } catch (const std::exception& error) {
                set_thread_error("output thread failed: " + std::string(error.what()));
            }
        });
    } else {
        logger.info("video sink disabled, inference thread releases frames after result handling");
    }

    capture_thread.join();
    inference_thread.join();
    if (output_thread.joinable()) {
        output_thread.join();
    }

    {
        std::lock_guard<std::mutex> lock(error_mutex);
        if (!thread_error.empty()) {
            logger.error(thread_error);
            throw std::runtime_error(thread_error);
        }
    }

    logger.info("pipeline finished frames=" + std::to_string(result.frames_processed) +
                " detections=" + std::to_string(result.detections_seen) +
                " events=" + std::to_string(result.events.size()));
    return result;
}

} // namespace

/**
 * @brief 运行默认演示流水线，并按配置自动选择视频源策略。
 * @param config 已通过校验的应用配置。
 * @param stop_requested 可选的停止回调，每轮循环开始前检查一次。
 * @return 本次流水线执行统计结果。
 */
PipelineResult run_demo_pipeline(const SentinelConfig& config,
                                 const std::function<bool()>& stop_requested)
{
    NullLogger logger;
    return run_demo_pipeline(config, logger, stop_requested);
}

/**
 * @brief 运行默认演示流水线，并按配置自动选择视频源策略。
 * @param config 已通过校验的应用配置。
 * @param logger 由调用方注入的日志策略对象。
 * @param stop_requested 可选的停止回调，每轮循环开始前检查一次。
 * @return 本次流水线执行统计结果。
 */
PipelineResult run_demo_pipeline(const SentinelConfig& config,
                                 Logger& logger,
                                 const std::function<bool()>& stop_requested)
{
    const auto camera = first_enabled_camera(config.cameras);
    auto video_source = create_video_source(camera);
    logger.info("open video source camera=" + camera.id + " type=" + camera.type +
                " uri=" + camera.uri + " buffer_mode=" + camera.buffer_mode);
    return run_demo_pipeline(config, *video_source, logger, stop_requested);
}

/**
 * @brief 使用外部注入的视频源策略运行演示流水线。
 * @param config 已通过校验的应用配置。
 * @param video_source 由调用方注入的视频源策略对象。
 * @param stop_requested 可选的停止回调，每轮循环开始前检查一次。
 * @return 本次流水线执行统计结果。
 */
PipelineResult run_demo_pipeline(const SentinelConfig& config,
                                 VideoSource& video_source,
                                 const std::function<bool()>& stop_requested)
{
    NullLogger logger;
    return run_demo_pipeline(config, video_source, logger, stop_requested);
}

/**
 * @brief 使用外部注入的视频源策略运行演示流水线。
 * @param config 已通过校验的应用配置。
 * @param video_source 由调用方注入的视频源策略对象。
 * @param logger 由调用方注入的日志策略对象。
 * @param stop_requested 可选的停止回调，每轮循环开始前检查一次。
 * @return 本次流水线执行统计结果。
 */
PipelineResult run_demo_pipeline(const SentinelConfig& config,
                                 VideoSource& video_source,
                                 Logger& logger,
                                 const std::function<bool()>& stop_requested)
{
    if (config.pipeline.mode == "threaded") {
        return run_threaded_pipeline(config, video_source, logger, stop_requested);
    }

    if (!video_source.open()) {
        const auto error_message = "unable to open " + std::string(video_source.kind()) +
                                   " source: " + std::string(video_source.last_error());
        logger.error(error_message);
        throw std::runtime_error(error_message);
    }

    std::unique_ptr<FramePreprocessor> preprocessor;
    if (needs_frame_preprocessor(config.inference)) {
        preprocessor = create_frame_preprocessor(config.preprocess);
        logger.info("open frame preprocessor backend=" + std::string(preprocessor->kind()) +
                    " layout=" + config.preprocess.output_layout +
                    " dtype=" + config.preprocess.output_dtype);
        if (!preprocessor->open()) {
            const auto error_message = "unable to open " + std::string(preprocessor->kind()) +
                                       " preprocessor: " +
                                       std::string(preprocessor->last_error());
            logger.error(error_message);
            video_source.close();
            throw std::runtime_error(error_message);
        }
    }

    auto detector = create_detector(config.inference, config.rules, config.postprocess);
    if (!detector->open()) {
        const auto error_message = "unable to open " + std::string(detector->kind()) +
                                   " detector: " + std::string(detector->last_error());
        logger.error(error_message);
        if (preprocessor) {
            preprocessor->close();
        }
        video_source.close();
        throw std::runtime_error(error_message);
    }

    auto video_sink = create_video_sink(config.output, config.overlay, config.service);
    logger.info("open video sink type=" + std::string(video_sink->kind()) +
                " overlay=" + (config.overlay.enabled ? "enabled" : "disabled") +
                " overlay_backend=" + config.overlay.backend);
    if (config.output.video_sink == "debug_image") {
        const auto debug_output_dir = config.service.data_dir / config.output.debug_image_dir;
        logger.info("debug image output_dir=" + debug_output_dir.string() +
                    " interval=" + std::to_string(config.output.debug_image_interval));
    } else if (config.output.video_sink == "mjpeg") {
        logger.info("mjpeg bind=" + config.output.mjpeg_host + ":" +
                    std::to_string(config.output.mjpeg_port) +
                    " path=" + config.output.mjpeg_path +
                    " quality=" + std::to_string(config.output.mjpeg_quality) +
                    " max_clients=" + std::to_string(config.output.mjpeg_max_clients));
    }
    if (!video_sink->open()) {
        const auto error_message = "unable to open " + std::string(video_sink->kind()) +
                                   " video sink: " + std::string(video_sink->last_error());
        logger.error(error_message);
        detector->close();
        if (preprocessor) {
            preprocessor->close();
        }
        video_source.close();
        throw std::runtime_error(error_message);
    }

    EventBuilder event_builder(config.rules);
    PerformanceStats performance_stats(config.performance.log_interval_frames);
    std::ofstream performance_csv;
    if (config.performance.enabled) {
        performance_csv = open_perf_csv(resolve_perf_csv_path(config.service,
                                                             config.performance.csv_path),
                                        logger);
    }
    if (config.performance.enabled) {
        logger.info("performance logging enabled interval_frames=" +
                    std::to_string(config.performance.log_interval_frames));
    }

    PipelineResult result;

    for (int frame_index = 0; frame_index < config.pipeline.max_frames; ++frame_index) {
        const auto frame_start = Clock::now();

        // 停止回调放在每轮开头，确保主循环能尽快响应 SIGINT/SIGTERM。
        if (stop_requested && stop_requested()) {
            logger.warn("pipeline stop requested by external signal");
            break;
        }

        const auto capture_start = Clock::now();
        const auto frame = video_source.read_frame();
        const auto capture_ms = elapsed_ms_since(capture_start);
        // 当前无帧或底层视频源出错时，结束本轮演示流水线。
        if (!frame.has_value()) {
            if (!video_source.last_error().empty()) {
                logger.error("video source read failed: " + std::string(video_source.last_error()));
            } else {
                logger.info("video source returned no frame, pipeline will stop");
            }
            break;
        }
        logger.debug(make_frame_debug_message(*frame));

        TensorBuffer tensor;
        double preprocess_ms = 0.0;
        if (preprocessor) {
            // 真实推理路径先将摄像头帧转换成模型输入张量，再交给检测器。
            const auto preprocess_start = Clock::now();
            const auto target_metadata = make_preprocess_target_metadata(config.preprocess, *frame);
            auto target_tensor = detector->mutable_input_tensor(target_metadata);
            auto processed = target_tensor.has_value()
                                 ? preprocessor->process_into(*frame, std::move(*target_tensor))
                                 : preprocessor->process(*frame);
            preprocess_ms = elapsed_ms_since(preprocess_start);
            if (!processed.has_value()) {
                logger.error("frame preprocessor failed: " +
                             std::string(preprocessor->last_error()));
                break;
            }
            tensor = std::move(*processed);
        } else {
            tensor = make_metadata_tensor(*frame);
        }
        logger.debug(make_tensor_debug_message(tensor));

        const auto detect_start = Clock::now();
        const auto detections = detector->detect(tensor);
        const auto detect_ms = elapsed_ms_since(detect_start);
        if (!detector->last_error().empty()) {
            logger.error("detector failed: " + std::string(detector->last_error()));
            break;
        }
        if (!detector->debug_info().empty()) {
            logger.debug("detector result: " + std::string(detector->debug_info()));
        }

        const auto output_start = Clock::now();
        if (!video_sink->write(*frame, detections)) {
            logger.error("video sink failed: " + std::string(video_sink->last_error()));
            break;
        }
        const auto output_ms = elapsed_ms_since(output_start);

        result.frames_processed += 1;
        result.detections_seen += static_cast<int>(detections.size());

        auto events = event_builder.observe(*frame, detections);
        result.events.insert(result.events.end(), events.begin(), events.end());

        if (config.performance.enabled) {
            FramePerformanceSample sample;
            sample.frame_sequence = frame->sequence;
            sample.detections = static_cast<int>(detections.size());
            sample.capture_ms = capture_ms;
            sample.preprocess_ms = preprocess_ms;
            sample.detect_ms = detect_ms;
            sample.output_ms = output_ms;
            sample.frame_total_ms = elapsed_ms_since(frame_start);

            performance_stats.observe(sample);
            if (performance_csv) {
                performance_csv << PerformanceStats::csv_row(sample);
            }
            if (performance_stats.should_report()) {
                logger.info(performance_stats.make_report_and_reset());
            }
        }
    }

    if (config.performance.enabled && performance_stats.has_pending_samples()) {
        logger.info(performance_stats.make_report_and_reset());
    }

    video_sink->close();
    detector->close();
    if (preprocessor) {
        preprocessor->close();
    }
    video_source.close();
    logger.info("pipeline finished frames=" + std::to_string(result.frames_processed) +
                " detections=" + std::to_string(result.detections_seen) +
                " events=" + std::to_string(result.events.size()));
    return result;
}

} // namespace sentinel
