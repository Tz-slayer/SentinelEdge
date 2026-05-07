#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/video/video_source.hpp"

#include <string>

namespace sentinel {

class RtspVideoSource final : public VideoSource {
public:
    explicit RtspVideoSource(CameraConfig config);

    bool open() override;
    void close() noexcept override;
    std::optional<Frame> read_frame() override;
    std::string_view kind() const noexcept override;
    std::string_view last_error() const noexcept override;

private:
    CameraConfig config_;
    std::string last_error_;
    bool is_open_{false};
};

} // namespace sentinel
