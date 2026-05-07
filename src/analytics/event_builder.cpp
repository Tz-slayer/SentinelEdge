#include "sentinel/analytics/event_builder.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace sentinel {

/**
 * @brief 使用规则配置构造事件构建器。
 * @param rules 检测过滤和事件触发阈值配置。
 */
EventBuilder::EventBuilder(RuleConfig rules)
    : rules_(std::move(rules))
{
}

/**
 * @brief 处理一帧检测结果，并按规则生成事件。
 * @param frame 当前帧元数据。
 * @param detections 当前帧检测结果列表。
 * @return 当前帧新生成的事件列表。
 */
std::vector<Event> EventBuilder::observe(const Frame& frame, const std::vector<Detection>& detections)
{
    std::vector<Event> events;
    std::set<std::string> observed_tracks;

    for (const auto& detection : detections) {
        // 先过滤掉低置信度或不在规则范围内的检测结果。
        if (detection.confidence < rules_.min_confidence || !is_target_class(detection.label)) {
            continue;
        }

        const auto key = make_track_key(detection);
        observed_tracks.insert(key);

        auto& state = tracks_[key];
        // 首次观测时记录起始帧，并在后续帧中维护最大置信度。
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

    // 本帧未再次出现的跟踪项要清空连续计数，避免跨空洞帧继续累计。
    for (auto& [key, state] : tracks_) {
        if (key.rfind(frame.camera_id + ":", 0) == 0 && observed_tracks.count(key) == 0) {
            state.consecutive_frames = 0;
            state.max_confidence = 0.0;
            state.first_frame = frame.sequence;
        }
    }

    return events;
}

/**
 * @brief 判断目标类别是否允许参与事件生成。
 * @param label 检测类别名称。
 * @return 若类别存在于规则配置中则返回 `true`。
 */
bool EventBuilder::is_target_class(const std::string& label) const
{
    return std::find(rules_.target_classes.begin(), rules_.target_classes.end(), label) !=
           rules_.target_classes.end();
}

/**
 * @brief 生成用于跟踪同类检测流的键。
 * @param detection 当前检测结果。
 * @return 由摄像头编号和类别组成的稳定键。
 */
std::string EventBuilder::make_track_key(const Detection& detection) const
{
    return detection.camera_id + ":" + detection.label;
}

/**
 * @brief 根据当前检测和聚合状态构造事件。
 * @param detection 触发事件的检测结果。
 * @param state 当前检测流的聚合状态。
 * @return 新创建的事件对象。
 */
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
