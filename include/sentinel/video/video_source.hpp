#pragma once

#include "sentinel/common/types.hpp"

#include <optional>
#include <string_view>

namespace sentinel {

class VideoSource {
public:
    virtual ~VideoSource() = default;

    virtual bool open() = 0;
    virtual void close() noexcept = 0;
    virtual std::optional<Frame> read_frame() = 0;
    virtual std::string_view kind() const noexcept = 0;
    virtual std::string_view last_error() const noexcept = 0;
};

} // namespace sentinel
