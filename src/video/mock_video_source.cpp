#include "sentinel/video/mock_video_source.hpp"

#include <chrono>
#include <optional>
#include <utility>

namespace sentinel {

/**
 * @brief 使用摄像头配置构造模拟视频源。
 * @param config 摄像头配置。
 */
MockVideoSource::MockVideoSource(CameraConfig config)
    : config_(std::move(config))
{
}

/**
 * @brief 打开模拟视频源并重置内部状态。
 * @return 固定返回 `true`。
 */
bool MockVideoSource::open()
{
    next_sequence_ = 1;
    is_open_ = true;
    last_error_.clear();
    return true;
}

/**
 * @brief 关闭模拟视频源。
 */
void MockVideoSource::close() noexcept
{
    is_open_ = false;
}

/**
 * @brief 生成一帧模拟数据。
 * @return 打开状态下返回一帧，否则返回 `std::nullopt`。
 */
std::optional<Frame> MockVideoSource::read_frame()
{
    if (!is_open_) {
        return std::nullopt;
    }

    // 模拟源不提供真实像素数据，只提供足够驱动流程的帧元信息。
    Frame frame;
    frame.sequence = next_sequence_++;
    frame.camera_id = config_.id;
    frame.width = config_.width;
    frame.height = config_.height;
    frame.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();
    return frame;
}

/**
 * @brief 返回视频源类型标识。
 * @return 固定返回 `"mock"`。
 */
std::string_view MockVideoSource::kind() const noexcept
{
    return "mock";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 当前实现始终为空字符串。
 */
std::string_view MockVideoSource::last_error() const noexcept
{
    return last_error_;
}

} // namespace sentinel
