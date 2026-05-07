#pragma once

#include "sentinel/common/types.hpp"

#include <functional>
#include <vector>

namespace sentinel {

struct PipelineResult {
    int frames_processed{0};
    int detections_seen{0};
    std::vector<Event> events;
};

PipelineResult run_demo_pipeline(const SentinelConfig& config,
                                const std::function<bool()>& stop_requested = {});

} // namespace sentinel
