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
 * @brief 单路摄像头或视频流输入配置。
 */
struct CameraConfig {
    std::string id{"demo-camera"};
    std::string name{"Demo Camera"};
    std::string type{"mock"};
    std::string uri{"mock://demo"};
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
