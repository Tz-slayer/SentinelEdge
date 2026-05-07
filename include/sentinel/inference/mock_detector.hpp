#pragma once

#include "sentinel/common/types.hpp"

#include <string_view>
#include <vector>

namespace sentinel {

class MockDetector {
public:
    explicit MockDetector(RuleConfig rules);

    std::vector<Detection> detect(const Frame& frame) const;

private:
    bool is_target_class(std::string_view label) const;

    RuleConfig rules_;
};

} // namespace sentinel
