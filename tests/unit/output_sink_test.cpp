#include "sentinel/output/video_sink_factory.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

#include <linux/videodev2.h>
#include <opencv2/imgcodecs.hpp>

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
 * @brief 构造一帧可被 OpenCV 解码的 MJPEG 测试帧。
 * @return 包含 JPEG 压缩数据的测试帧。
 */
sentinel::Frame make_jpeg_frame()
{
    cv::Mat image(24, 32, CV_8UC3, cv::Scalar(12, 64, 180));
    std::vector<std::uint8_t> encoded;
    cv::imencode(".jpg", image, encoded);

    sentinel::Frame frame;
    frame.sequence = 3;
    frame.camera_id = "sink-camera";
    frame.width = image.cols;
    frame.height = image.rows;
    frame.pixel_format = V4L2_PIX_FMT_MJPEG;
    frame.bytes_used = encoded.size();
    frame.data = std::move(encoded);
    return frame;
}

} // namespace

/**
 * @brief 验证空输出、调试图输出和 RTSP 输出通道工厂。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    sentinel::ServiceConfig service;
    service.data_dir = std::filesystem::temp_directory_path() / "sentinel-output-sink-test";
    std::filesystem::remove_all(service.data_dir);

    sentinel::OverlayConfig overlay;
    overlay.enabled = true;
    overlay.backend = "opencv";

    sentinel::OutputConfig none_output;
    none_output.video_sink = "none";
    auto none_sink = sentinel::create_video_sink(none_output, overlay, service);
    expect(none_sink->kind() == "none", "factory should create null video sink");
    expect(none_sink->open(), "null video sink should open");
    expect(none_sink->write(make_jpeg_frame(), {}), "null video sink should accept frames");
    none_sink->close();

    sentinel::OutputConfig debug_output;
    debug_output.video_sink = "debug_image";
    debug_output.debug_image_dir = "frames";
    debug_output.debug_image_interval = 1;
    auto debug_sink = sentinel::create_video_sink(debug_output, overlay, service);
    expect(debug_sink->kind() == "debug_image", "factory should create debug image sink");
    expect(debug_sink->open(), "debug image sink should open");

    sentinel::Detection detection;
    detection.label = "person";
    detection.confidence = 0.88;
    detection.bounding_box = sentinel::Rect{0.2, 0.2, 0.4, 0.5};
    detection.frame_sequence = 3;
    detection.camera_id = "sink-camera";

    const auto frame = make_jpeg_frame();
    expect(debug_sink->write(frame, {detection}), "debug image sink should write jpeg");
    const auto output_path = service.data_dir / "frames" / "frame-sink-camera-3.jpg";
    expect(std::filesystem::exists(output_path), "debug image sink should create jpeg file");

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
