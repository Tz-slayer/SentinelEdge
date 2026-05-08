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
    int max_frames{5};
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
    int output_width{640};
    int output_height{640};
    std::string output_layout{"NCHW"};
    std::string output_dtype{"FP32"};
    bool normalize{true};
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
