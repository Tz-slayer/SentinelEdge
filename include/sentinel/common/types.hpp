#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sentinel {

/**
 * @brief 服务级运行配置。
 */
struct ServiceConfig {
    std::string host{"127.0.0.1"};
    int port{8080};
    std::filesystem::path data_dir{"./data"};
};

/**
 * @brief 日志系统运行配置。
 */
struct LoggingConfig {
    std::string backend{"stderr"};
    std::string level{"info"};
    std::string ident{"video_sentinel"};
};

/**
 * @brief AI 推理后端运行配置。
 */
struct InferenceConfig {
    std::string backend{"mock"};
    std::filesystem::path model_path{"models/yolo/yolo26n_aipp_nv12.om"};
    int device_id{0};
    int stream_slots{2};
};

/**
 * @brief 图像预处理策略运行配置。
 *
 * 该配置描述从视频帧到模型输入张量的转换方式。当前主线固定为
 * DVPP JPEGD/VPC 输出 `NV12`/`UINT8`，由静态 AIPP OM 在 AI Core 内完成
 * 色域转换和归一化。
 */
struct PreprocessConfig {
    std::string backend{"dvpp"};
    int device_id{0};
    int stream_slots{2};
    int output_width{640};
    int output_height{640};
    std::string output_layout{"NV12"};
    std::string output_dtype{"UINT8"};
    bool normalize{false};
};

/**
 * @brief 目标检测后处理策略运行配置。
 *
 * 该配置描述如何把模型原始输出张量解析为 `Detection`。当前主线使用
 * `dvpp` 配置入口承载 YOLO 解码与纯 C++ NMS；DVPP 本身不执行 NMS。
 */
struct PostprocessConfig {
    std::string backend{"dvpp"};
    std::string model_type{"yolo"};
    std::string output_layout{"channels_first"};
    int num_classes{80};
    double confidence_threshold{0.25};
    double nms_iou_threshold{0.45};
    int max_detections{100};
    bool merge_coco_vehicles{true};
    std::vector<std::string> class_names;
};

/**
 * @brief 检测框叠加绘制配置。
 */
struct OverlayConfig {
    bool enabled{false};
    std::string backend{"dvpp"};
};

/**
 * @brief 视频输出通道配置。
 *
 * 当前支持 `none`、`debug_image` 和 `mjpeg`。`debug_image` 会把画框后的 JPEG
 * 保存到 `ServiceConfig::data_dir / debug_image_dir`，`mjpeg` 会启动本地 HTTP
 * MJPEG 调试预览服务，浏览器可直接访问。
 */
struct OutputConfig {
    std::string video_sink{"none"};
    std::filesystem::path debug_image_dir{"debug/frames"};
    int debug_image_interval{1};
    std::string mjpeg_host{"0.0.0.0"};
    int mjpeg_port{8081};
    std::string mjpeg_path{"/stream"};
    int mjpeg_quality{80};
    int mjpeg_max_clients{4};
};

/**
 * @brief 性能统计配置。
 *
 * 该配置控制 pipeline 是否输出阶段耗时。`csv_path` 为空时只输出聚合日志；
 * 非空时逐帧写入 CSV，便于开发板上观察主线链路的采集、预处理、推理和输出耗时。
 */
struct PerformanceConfig {
    bool enabled{true};
    int log_interval_frames{30};
    std::filesystem::path csv_path;
};

/**
 * @brief 流水线级运行配置。
 *
 * `backend` 保留为配置字段用于启动校验和日志可读性。当前主线只接受
 * `dvpp`，并在配置加载阶段映射到预处理、后处理和画框策略。
 */
struct PipelineConfig {
    std::string backend{"dvpp"};
    std::string mode{"threaded"};
    int max_frames{5};
    int detect_fps{30};
    int stream_slots{2};
    int output_queue_size{2};
};

/**
 * @brief 单路摄像头或视频流输入配置。
 *
 * `buffer_mode` 当前固定为 `loaned`，表示 V4L2 `mmap` 缓冲区由 `Frame`
 * 租借持有，最后一个租约释放时再通过 `VIDIOC_QBUF` 归还驱动。
 */
struct CameraConfig {
    std::string id{"demo-camera"};
    std::string name{"Demo Camera"};
    std::string type{"mock"};
    std::string uri{"mock://demo"};
    std::string buffer_mode{"loaned"};
    bool enabled{true};
    int width{1280};
    int height{720};
    int fps{10};
};

/**
 * @brief 检测过滤和事件生成阈值配置。
 */
struct RuleConfig {
    std::vector<std::string> target_classes{"person"};
    double min_confidence{0.5};
    int hold_frames{2};
    int cooldown_frames{10};
};

/**
 * @brief 应用聚合配置。
 */
struct SentinelConfig {
    ServiceConfig service;
    LoggingConfig logging;
    InferenceConfig inference;
    PreprocessConfig preprocess;
    PostprocessConfig postprocess;
    OverlayConfig overlay;
    OutputConfig output;
    PerformanceConfig performance;
    PipelineConfig pipeline;
    std::vector<CameraConfig> cameras;
    RuleConfig rules;
};

/**
 * @brief 图像空间中的归一化矩形框。
 */
struct Rect {
    double x{0.0};
    double y{0.0};
    double width{0.0};
    double height{0.0};
};

/**
 * @brief 视频帧外部载荷的生命周期租约。
 *
 * 该接口用于表达非 `Frame::data` 拥有的帧载荷生命周期，例如 V4L2
 * `mmap` 缓冲区。最后一个持有租约的 `Frame` 销毁时，具体实现负责把
 * 底层缓冲区归还给驱动或释放相关资源。析构函数必须保持 `noexcept`，
 * 不允许在信号边界或清理路径抛出异常。
 */
class FramePayloadLease {
public:
    /**
     * @brief 释放帧载荷租约。
     */
    virtual ~FramePayloadLease() = default;

protected:
    /**
     * @brief 只允许派生类构造租约对象。
     */
    FramePayloadLease() = default;

    FramePayloadLease(const FramePayloadLease&) = delete;
    FramePayloadLease& operator=(const FramePayloadLease&) = delete;
    FramePayloadLease(FramePayloadLease&&) = delete;
    FramePayloadLease& operator=(FramePayloadLease&&) = delete;
};

/**
 * @brief 一帧采集结果及其关联元数据。
 */
struct Frame {
    int sequence{0};
    std::string camera_id;
    int width{0};
    int height{0};
    std::uint32_t pixel_format{0};
    std::int64_t timestamp_ns{0};
    std::size_t bytes_used{0};
    std::vector<std::uint8_t> data;
    const std::uint8_t* loaned_data{nullptr};
    std::size_t loaned_size{0};
    std::shared_ptr<FramePayloadLease> payload_lease;

    /**
     * @brief 返回当前帧载荷的只读起始地址。
     * @return `loaned` 模式返回驱动缓冲区地址；测试或 mock 帧返回 `data.data()`。
     */
    const std::uint8_t* payload_data() const noexcept
    {
        return loaned_data != nullptr ? loaned_data : data.data();
    }

    /**
     * @brief 返回当前帧载荷的有效字节数。
     * @return `loaned` 模式返回租借缓冲区有效长度；测试或 mock 帧返回 `data` 长度。
     */
    std::size_t payload_size() const noexcept
    {
        return loaned_data != nullptr ? loaned_size : data.size();
    }

    /**
     * @brief 判断当前帧是否持有外部缓冲区租约。
     * @return 使用 `loaned` 外部载荷返回 `true`。
     */
    bool is_loaned() const noexcept
    {
        return loaned_data != nullptr;
    }
};

/**
 * @brief 解码或渲染后的图像缓冲区。
 *
 * 该结构用于调试输出链路中的解码和画框结果。主线推理输入优先保持在
 * Ascend Device 内存中；只有 `debug_image` 或 `mjpeg` 预览需要 Host BGR24。
 */
struct ImageBuffer {
    int width{0};
    int height{0};
    std::string pixel_format{"BGR24"};
    std::string memory_type{"host"};
    std::vector<std::uint8_t> data;
};

/**
 * @brief 张量缓冲区所在内存位置。
 */
enum class TensorMemoryLocation {
    /** @brief Host 侧普通内存，通常由 `std::vector<std::uint8_t>` 持有。 */
    kHost,
    /** @brief Ascend Device 侧内存，生命周期由推理后端或硬件处理后端持有。 */
    kDevice,
};

/**
 * @brief 模型输入张量及其来源帧元数据。
 *
 * `data` 按 `dtype` 和 `layout` 描述的格式存放 Host 原始字节。
 * `device_data` 用于描述已经位于 Device 侧的模型输入缓冲区。预处理策略
 * 必须保证尺寸、布局和数据类型与 `.om` 模型输入一致。
 */
struct TensorBuffer {
    TensorMemoryLocation memory_location{TensorMemoryLocation::kHost};
    std::vector<std::uint8_t> data;
    void* device_data{nullptr};
    std::size_t device_bytes{0};
    std::vector<int> shape;
    std::string layout;
    std::string dtype;
    int frame_sequence{0};
    std::string camera_id;

    /**
     * @brief 返回张量有效字节数。
     * @return Host 张量返回 `data.size()`，Device 张量返回 `device_bytes`。
     */
    std::size_t byte_size() const noexcept
    {
        return memory_location == TensorMemoryLocation::kDevice ? device_bytes : data.size();
    }

    /**
     * @brief 判断张量是否位于 Device 内存。
     * @return Device 张量返回 `true`。
     */
    bool is_device() const noexcept
    {
        return memory_location == TensorMemoryLocation::kDevice;
    }
};

/**
 * @brief 单个模型输出缓冲区。
 *
 * `data` 保存从推理后端拷贝回 Host 的原始输出字节；`shape` 若可从推理
 * 框架获取则保存维度信息，无法获取时允许为空，后处理策略会按配置推断。
 */
struct ModelOutputBuffer {
    std::vector<std::uint8_t> data;
    std::vector<int> shape;
    std::string dtype{"FP32"};
    std::size_t index{0};
};

/**
 * @brief 与某一帧绑定的一条检测结果。
 */
struct Detection {
    std::string label;
    double confidence{0.0};
    Rect bounding_box;
    int frame_sequence{0};
    std::string camera_id;
};

/**
 * @brief 分析层输出的一条高层事件。
 */
struct Event {
    std::string id;
    std::string type;
    std::string camera_id;
    std::string label;
    int start_frame{0};
    int end_frame{0};
    double confidence{0.0};
    std::string message;
};

} // namespace sentinel
