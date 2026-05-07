#pragma once

#include "sentinel/common/types.hpp"

#include <map>
#include <string>
#include <vector>

namespace sentinel {

class EventBuilder {
public:
    explicit EventBuilder(RuleConfig rules);

    std::vector<Event> observe(const Frame& frame, const std::vector<Detection>& detections);

private:
    struct TrackState {
        int consecutive_frames{0};
        int first_frame{0};
        int last_event_frame{-1'000'000};
        double max_confidence{0.0};
    };

    bool is_target_class(const std::string& label) const;
    std::string make_track_key(const Detection& detection) const;
    Event make_event(const Detection& detection, const TrackState& state);

    RuleConfig rules_;
    std::map<std::string, TrackState> tracks_;
    int next_event_id_{1};
};

} // namespace sentinel
