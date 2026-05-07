#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/video/video_source.hpp"

namespace sentinel {

class MockVideoSource final : public VideoSource {
public:
    explicit MockVideoSource(CameraConfig config);

    bool open() override;
    void close() noexcept override;
    std::optional<Frame> read_frame() override;
    std::string_view kind() const noexcept override;
    std::string_view last_error() const noexcept override;

private:
    CameraConfig config_;
    int next_sequence_{1};
    bool is_open_{false};
};

} // namespace sentinel
