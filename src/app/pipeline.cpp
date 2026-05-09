#include "sentinel/app/pipeline.hpp"

#include "sentinel/analytics/event_builder.hpp"
#include "sentinel/inference/detector_factory.hpp"
#include "sentinel/output/video_sink_factory.hpp"
#include "sentinel/perf/performance_stats.hpp"
#include "sentinel/preprocess/preprocessor_factory.hpp"
#include "sentinel/video/video_source_factory.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <stdexcept>
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
           " data_bytes=" + std::to_string(frame.data.size()) +
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
           " bytes=" + std::to_string(tensor.data.size());
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

    for (int frame_index = 0; frame_index < config.service.max_frames; ++frame_index) {
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
            auto processed = preprocessor->process(*frame);
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
