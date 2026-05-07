#include "sentinel/app/pipeline.hpp"

#include "sentinel/analytics/event_builder.hpp"
#include "sentinel/inference/mock_detector.hpp"
#include "sentinel/video/video_source_factory.hpp"

#include <stdexcept>

namespace sentinel {
namespace {

/**
 * @brief 从配置中找到第一路启用的摄像头。
 * @param cameras 摄像头配置列表。
 * @return 第一条启用状态的摄像头配置。
 */
CameraConfig first_enabled_camera(const std::vector<CameraConfig>& cameras)
{
    for (const auto& camera : cameras) {
        if (camera.enabled) {
            return camera;
        }
    }

    throw std::runtime_error("no enabled camera configured");
}

} // namespace

/**
 * @brief 运行默认演示流水线，并按配置自动选择视频源策略。
 * @param config 已通过校验的应用配置。
 * @param stop_requested 可选的停止回调，每轮循环开始前检查一次。
 * @return 本次流水线执行统计结果。
 */
PipelineResult run_demo_pipeline(const SentinelConfig& config,
                                 const std::function<bool()>& stop_requested)
{
    const auto camera = first_enabled_camera(config.cameras);
    auto video_source = create_video_source(camera);
    return run_demo_pipeline(config, *video_source, stop_requested);
}

/**
 * @brief 使用外部注入的视频源策略运行演示流水线。
 * @param config 已通过校验的应用配置。
 * @param video_source 由调用方注入的视频源策略对象。
 * @param stop_requested 可选的停止回调，每轮循环开始前检查一次。
 * @return 本次流水线执行统计结果。
 */
PipelineResult run_demo_pipeline(const SentinelConfig& config,
                                 VideoSource& video_source,
                                 const std::function<bool()>& stop_requested)
{
    if (!video_source.open()) {
        throw std::runtime_error("unable to open " + std::string(video_source.kind()) + " source: " +
                                 std::string(video_source.last_error()));
    }

    const MockDetector detector(config.rules);
    EventBuilder event_builder(config.rules);

    PipelineResult result;

    for (int frame_index = 0; frame_index < config.service.max_frames; ++frame_index) {
        // 停止回调放在每轮开头，确保主循环能尽快响应 SIGINT/SIGTERM。
        if (stop_requested && stop_requested()) {
            break;
        }

        const auto frame = video_source.read_frame();
        // 当前无帧或底层视频源出错时，结束本轮演示流水线。
        if (!frame.has_value()) {
            break;
        }

        const auto detections = detector.detect(*frame);
        result.frames_processed += 1;
        result.detections_seen += static_cast<int>(detections.size());

        auto events = event_builder.observe(*frame, detections);
        result.events.insert(result.events.end(), events.begin(), events.end());
    }

    video_source.close();
    return result;
}

} // namespace sentinel
