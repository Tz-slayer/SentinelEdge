#include "sentinel/video/rtsp_video_source.hpp"

#include <optional>
#include <utility>

namespace sentinel {

/**
 * @brief 使用摄像头配置构造 RTSP 视频源。
 * @param config 摄像头配置。
 */
RtspVideoSource::RtspVideoSource(CameraConfig config)
    : config_(std::move(config))
{
}

/**
 * @brief 校验 RTSP 地址并报告当前未实现状态。
 * @return 当前固定返回 `false`。
 */
bool RtspVideoSource::open()
{
    if (config_.uri.rfind("rtsp://", 0) != 0) {
        last_error_ = "camera uri must start with rtsp://";
        return false;
    }

    last_error_ = "RtspVideoSource skeleton exists, but RTSP transport and decoder are not implemented yet";
    is_open_ = false;
    return false;
}

/**
 * @brief 关闭 RTSP 视频源状态。
 */
void RtspVideoSource::close() noexcept
{
    is_open_ = false;
}

/**
 * @brief 尝试读取一帧 RTSP 数据。
 * @return 当前实现固定返回 `std::nullopt`。
 */
std::optional<Frame> RtspVideoSource::read_frame()
{
    if (!is_open_) {
        return std::nullopt;
    }

    return std::nullopt;
}

/**
 * @brief 返回视频源类型标识。
 * @return 固定返回 `"rtsp"`。
 */
std::string_view RtspVideoSource::kind() const noexcept
{
    return "rtsp";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 最近一次错误消息。
 */
std::string_view RtspVideoSource::last_error() const noexcept
{
    return last_error_;
}

} // namespace sentinel
