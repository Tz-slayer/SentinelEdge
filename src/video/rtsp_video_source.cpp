#include "sentinel/video/rtsp_video_source.hpp"

#include <optional>
#include <utility>

namespace sentinel {

RtspVideoSource::RtspVideoSource(CameraConfig config)
    : config_(std::move(config))
{
}

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

void RtspVideoSource::close() noexcept
{
    is_open_ = false;
}

std::optional<Frame> RtspVideoSource::read_frame()
{
    if (!is_open_) {
        return std::nullopt;
    }

    return std::nullopt;
}

std::string_view RtspVideoSource::kind() const noexcept
{
    return "rtsp";
}

std::string_view RtspVideoSource::last_error() const noexcept
{
    return last_error_;
}

} // namespace sentinel
