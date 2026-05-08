#include "sentinel/video/camera_video_source.hpp"

#include <cstdlib>
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

} // namespace

/**
 * @brief 验证 V4L2 视频源在设备不存在时能稳定失败。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    sentinel::CameraConfig config;
    config.id = "test-camera";
    config.name = "Missing Camera";
    config.type = "v4l2";
    config.uri = "/dev/video-does-not-exist";
    config.width = 640;
    config.height = 480;
    config.fps = 30;

    sentinel::CameraVideoSource source(config);

    expect(!source.open(), "opening a missing V4L2 device should fail");
    expect(!source.last_error().empty(), "missing V4L2 device should set an error message");
    expect(!source.read_frame().has_value(), "closed source should not return frames");

    source.close();

    config.buffer_mode = "loaned";
    sentinel::CameraVideoSource loaned_source(config);
    expect(!loaned_source.open(), "loaned buffer mode should fail until FrameView support lands");
    expect(loaned_source.last_error().find("FrameView") != std::string_view::npos,
           "loaned buffer mode should explain the missing FrameView contract");

    loaned_source.close();
    return 0;
}
