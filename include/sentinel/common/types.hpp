#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace sentinel {

struct ServiceConfig {
    std::string host{"127.0.0.1"};
    int port{8080};
    std::filesystem::path data_dir{"./data"};
    int max_frames{5};
};

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

struct RuleConfig {
    std::vector<std::string> target_classes{"person"};
    double min_confidence{0.5};
    int hold_frames{2};
    int cooldown_frames{10};
};

struct SentinelConfig {
    ServiceConfig service;
    std::vector<CameraConfig> cameras;
    RuleConfig rules;
};

struct Rect {
    double x{0.0};
    double y{0.0};
    double width{0.0};
    double height{0.0};
};

struct Frame {
    int sequence{0};
    std::string camera_id;
    int width{0};
    int height{0};
};

struct Detection {
    std::string label;
    double confidence{0.0};
    Rect bounding_box;
    int frame_sequence{0};
    std::string camera_id;
};

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
