#include "sentinel/output/video_sink_factory.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

/**
 * @brief 断言条件成立，否则输出错误并退出。
 * @param condition 待检查条件。
 * @param message 失败时输出的错误消息。
 */
void expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

/**
 * @brief 构造一帧只用于空输出通道的测试帧。
 * @return 带基本元数据的测试帧。
 */
sentinel::Frame make_test_frame()
{
    sentinel::Frame frame;
    frame.sequence = 3;
    frame.camera_id = "sink-camera";
    frame.width = 32;
    frame.height = 24;
    return frame;
}

} // namespace

/**
 * @brief 验证空输出、调试图输出和 MJPEG 输出通道工厂。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    sentinel::ServiceConfig service;
    service.data_dir = std::filesystem::temp_directory_path() / "sentinel-output-sink-test";
    std::filesystem::remove_all(service.data_dir);

    sentinel::OverlayConfig overlay;
    overlay.enabled = true;
    overlay.backend = "dvpp";

    sentinel::OutputConfig none_output;
    none_output.video_sink = "none";
    auto none_sink = sentinel::create_video_sink(none_output, overlay, service);
    expect(none_sink->kind() == "none", "factory should create null video sink");
    expect(none_sink->open(), "null video sink should open");
    expect(none_sink->write(make_test_frame(), {}), "null video sink should accept frames");
    none_sink->close();

    sentinel::OutputConfig debug_output;
    debug_output.video_sink = "debug_image";
    debug_output.debug_image_dir = "frames";
    debug_output.debug_image_interval = 1;
    auto debug_sink = sentinel::create_video_sink(debug_output, overlay, service);
    expect(debug_sink->kind() == "debug_image", "factory should create debug image sink");
    expect(!debug_sink->open(), "host build without DVPP should not open debug image sink");
    expect(!debug_sink->last_error().empty(), "debug image sink should expose backend error");
    debug_sink->close();

    sentinel::OutputConfig mjpeg_output;
    mjpeg_output.video_sink = "mjpeg";
    mjpeg_output.mjpeg_host = "127.0.0.1";
    mjpeg_output.mjpeg_port = 0;
    mjpeg_output.mjpeg_path = "/stream";
    mjpeg_output.mjpeg_quality = 80;
    mjpeg_output.mjpeg_max_clients = 2;
    auto mjpeg_sink = sentinel::create_video_sink(mjpeg_output, overlay, service);
    expect(mjpeg_sink->kind() == "mjpeg", "factory should create MJPEG HTTP sink");
    expect(!mjpeg_sink->open(), "MJPEG sink should reject invalid port");
    expect(!mjpeg_sink->last_error().empty(), "MJPEG sink should expose config error");

    std::filesystem::remove_all(service.data_dir);
    return 0;
}
