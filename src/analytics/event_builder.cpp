#include "sentinel/analytics/event_builder.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace sentinel {

EventBuilder::EventBuilder(RuleConfig rules)
    : rules_(std::move(rules))
{
}

std::vector<Event> EventBuilder::observe(const Frame& frame, const std::vector<Detection>& detections)
{
    std::vector<Event> events;
    std::set<std::string> observed_tracks;

    for (const auto& detection : detections) {
        if (detection.confidence < rules_.min_confidence || !is_target_class(detection.label)) {
            continue;
        }

        const auto key = make_track_key(detection);
        observed_tracks.insert(key);

        auto& state = tracks_[key];
        if (state.consecutive_frames == 0) {
            state.first_frame = frame.sequence;
            state.max_confidence = detection.confidence;
        } else {
            state.max_confidence = std::max(state.max_confidence, detection.confidence);
        }
        state.consecutive_frames += 1;

        const auto held_long_enough = state.consecutive_frames >= rules_.hold_frames;
        const auto cooldown_elapsed = frame.sequence - state.last_event_frame > rules_.cooldown_frames;
        if (held_long_enough && cooldown_elapsed) {
            events.push_back(make_event(detection, state));
            state.last_event_frame = frame.sequence;
        }
    }

    for (auto& [key, state] : tracks_) {
        if (key.rfind(frame.camera_id + ":", 0) == 0 && observed_tracks.count(key) == 0) {
            state.consecutive_frames = 0;
            state.max_confidence = 0.0;
            state.first_frame = frame.sequence;
        }
    }

    return events;
}

bool EventBuilder::is_target_class(const std::string& label) const
{
    return std::find(rules_.target_classes.begin(), rules_.target_classes.end(), label) !=
           rules_.target_classes.end();
}

std::string EventBuilder::make_track_key(const Detection& detection) const
{
    return detection.camera_id + ":" + detection.label;
}

Event EventBuilder::make_event(const Detection& detection, const TrackState& state)
{
    const auto event_id = "evt-" + std::to_string(next_event_id_++);

    std::ostringstream message;
    message << detection.label << " detected on " << detection.camera_id << " for "
            << state.consecutive_frames << " consecutive frames";

    return Event{
        event_id,
        "target_present",
        detection.camera_id,
        detection.label,
        state.first_frame,
        detection.frame_sequence,
        state.max_confidence,
        message.str(),
    };
}

} // namespace sentinel
