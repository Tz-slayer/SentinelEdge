#pragma once

#include <cstdint>
#include <filesystem>
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
    std::filesystem::path model_path{"models/yolo/yolo26n.om"};
    int device_id{0};
};

/**
 * @brief 图像预处理策略运行配置。
 *
 * 该配置描述从视频帧到模型输入张量的转换方式。当前生产链路优先使用
 * OpenCV 实现软解码和缩放，后续可切换为 DVPP 以对比硬件预处理性能。
 */
struct PreprocessConfig {
    std::string backend{"opencv"};
    int device_id{0};
    int output_width{640};
    int output_height{640};
    std::string output_layout{"NCHW"};
    std::string output_dtype{"FP32"};
    bool normalize{true};
};

/**
 * @brief 目标检测后处理策略运行配置。
 *
 * 该配置描述如何把模型原始输出张量解析为 `Detection`。OpenCV 后端
 * 负责 CPU 侧 YOLO 解码与 NMS，DVPP 后端预留给后续硬件相关实验入口。
 */
struct PostprocessConfig {
    std::string backend{"opencv"};
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
    std::string backend{"opencv"};
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
 * 非空时逐帧写入 CSV，便于开发板上做 `none/debug_image/mjpeg` 等对照实验。
 */
struct PerformanceConfig {
    bool enabled{true};
    int log_interval_frames{30};
    std::filesystem::path csv_path;
};

/**
 * @brief 流水线级运行配置。
 *
 * `backend` 是用户可见的图像处理后端选择。配置加载阶段会把它映射到
 * 预处理、后处理和画框策略：`opencv` 表示全链路使用 CPU/OpenCV 路径，
 * `dvpp` 表示在 DVPP 能加速的图像处理环节优先使用 DVPP，非图像硬件能力
 * 仍由 CPU 侧代码完成。
 */
struct PipelineConfig {
    std::string backend{"opencv"};
    int max_frames{5};
};

/**
 * @brief 单路摄像头或视频流输入配置。
 *
 * `buffer_mode` 用于控制视频帧缓冲区的数据通路：`copy` 表示 V4L2
 * 出队后复制到 `Frame::data`，`loaned` 预留给后续零拷贝租借缓冲区模式。
 */
struct CameraConfig {
    std::string id{"demo-camera"};
    std::string name{"Demo Camera"};
    std::string type{"mock"};
    std::string uri{"mock://demo"};
    std::string buffer_mode{"copy"};
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
};

/**
 * @brief 解码或渲染后的图像缓冲区。
 *
 * 该结构用于统一 OpenCV、DVPP 等图像处理后端的中间结果。当前稳定
 * 路径使用 Host 内存和 BGR24/RGB24 等普通像素格式；后续接入硬件
 * 加速或零拷贝时，可在不改变上层阶段语义的前提下扩展内存类型。
 */
struct ImageBuffer {
    int width{0};
    int height{0};
    std::string pixel_format{"BGR24"};
    std::string memory_type{"host"};
    std::vector<std::uint8_t> data;
};

/**
 * @brief 模型输入张量及其来源帧元数据。
 *
 * `data` 按 `dtype` 和 `layout` 描述的格式存放原始字节。对于 AscendCL
 * 推理后端，该字节流会直接拷贝到 Device 输入缓冲区，因此预处理策略必须
 * 保证尺寸、布局和数据类型与 `.om` 模型输入一致。
 */
struct TensorBuffer {
    std::vector<std::uint8_t> data;
    std::vector<int> shape;
    std::string layout;
    std::string dtype;
    int frame_sequence{0};
    std::string camera_id;
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
