#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/video/video_source.hpp"

#include <functional>
#include <vector>

namespace sentinel {

/**
 * @brief 汇总一次流水线执行结果。
 */
struct PipelineResult {
    int frames_processed{0};
    int detections_seen{0};
    std::vector<Event> events;
};

/**
 * @brief 运行当前演示流水线，从视频源读取一直到事件生成。
 * @param config 已完成校验的应用配置。
 * @param stop_requested 可选的停止回调，会在每帧处理前检查是否需要优雅退出。
 * @return 本次流水线运行的统计结果和事件列表。
 */
PipelineResult run_demo_pipeline(const SentinelConfig& config,
                                const std::function<bool()>& stop_requested = {});

/**
 * @brief 使用外部注入的视频源策略运行演示流水线。
 * @param config 已完成校验的应用配置。
 * @param video_source 由调用方选择并注入的视频源策略对象。
 * @param stop_requested 可选的停止回调，会在每帧处理前检查是否需要优雅退出。
 * @return 本次流水线运行的统计结果和事件列表。
 */
PipelineResult run_demo_pipeline(const SentinelConfig& config,
                                VideoSource& video_source,
                                const std::function<bool()>& stop_requested = {});

} // namespace sentinel
