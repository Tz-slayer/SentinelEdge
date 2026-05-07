#include "sentinel/inference/mock_detector.hpp"

#include <algorithm>

namespace sentinel {

MockDetector::MockDetector(RuleConfig rules)
    : rules_(std::move(rules))
{
}

std::vector<Detection> MockDetector::detect(const Frame& frame) const
{
    std::vector<Detection> detections;

    if (is_target_class("person")) {
        detections.push_back(Detection{
            "person",
            0.91,
            Rect{0.22, 0.18, 0.20, 0.46},
            frame.sequence,
            frame.camera_id,
        });
    }

    if (frame.sequence % 3 == 0 && is_target_class("vehicle")) {
        detections.push_back(Detection{
            "vehicle",
            0.86,
            Rect{0.52, 0.42, 0.34, 0.28},
            frame.sequence,
            frame.camera_id,
        });
    }

    return detections;
}

bool MockDetector::is_target_class(std::string_view label) const
{
    return std::find(rules_.target_classes.begin(), rules_.target_classes.end(), label) !=
           rules_.target_classes.end();
}

} // namespace sentinel
