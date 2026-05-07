#include "sentinel/video/mock_video_source.hpp"

#include <optional>
#include <utility>

namespace sentinel {

MockVideoSource::MockVideoSource(CameraConfig config)
    : config_(std::move(config))
{
}

bool MockVideoSource::open()
{
    next_sequence_ = 1;
    is_open_ = true;
    return true;
}

void MockVideoSource::close() noexcept
{
    is_open_ = false;
}

std::optional<Frame> MockVideoSource::read_frame()
{
    if (!is_open_) {
        return std::nullopt;
    }

    return Frame{
        next_sequence_++,
        config_.id,
        config_.width,
        config_.height,
    };
}

std::string_view MockVideoSource::kind() const noexcept
{
    return "mock";
}

std::string_view MockVideoSource::last_error() const noexcept
{
    return {};
}

} // namespace sentinel
