#pragma once

#include "sentinel/common/types.hpp"

#include <map>
#include <string>
#include <vector>

namespace sentinel {

/**
 * @brief 将逐帧检测结果提升为更高层的事件。
 *
 * 该类会按照摄像头和目标类别跟踪连续检测结果，并结合保持帧数、
 * 冷却帧数等规则决定何时真正发出事件。
 */
class EventBuilder {
public:
    /**
     * @brief 使用规则配置构造事件构建器。
     * @param rules 检测过滤和事件触发阈值配置。
     */
    explicit EventBuilder(RuleConfig rules);

    /**
     * @brief 处理一帧检测结果，并返回本帧新触发的事件。
     * @param frame 当前帧的元数据。
     * @param detections 当前帧对应的检测结果列表。
     * @return 当前帧新生成的事件列表。
     */
    std::vector<Event> observe(const Frame& frame, const std::vector<Detection>& detections);

private:
    /**
     * @brief 记录同一摄像头、同一目标类别的连续观测状态。
     */
    struct TrackState {
        int consecutive_frames{0};
        int first_frame{0};
        int last_event_frame{-1'000'000};
        double max_confidence{0.0};
    };

    /**
     * @brief 判断目标类别是否在事件规则允许范围内。
     * @param label 检测类别名称。
     * @return 若该类别允许参与事件生成则返回 `true`。
     */
    bool is_target_class(const std::string& label) const;

    /**
     * @brief 为一条检测流生成内存中的跟踪键。
     * @param detection 用于生成跟踪键的检测结果。
     * @return 按摄像头和类别拼出的稳定跟踪键。
     */
    std::string make_track_key(const Detection& detection) const;

    /**
     * @brief 根据满足条件的检测状态构造高层事件。
     * @param detection 触发事件生成的检测结果。
     * @param state 当前检测流的聚合状态。
     * @return 新构造出的事件对象。
     */
    Event make_event(const Detection& detection, const TrackState& state);

    RuleConfig rules_;
    std::map<std::string, TrackState> tracks_;
    int next_event_id_{1};
};

} // namespace sentinel
