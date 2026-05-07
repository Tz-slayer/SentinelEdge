#include "sentinel/app/pipeline.hpp"

#include "sentinel/analytics/event_builder.hpp"
#include "sentinel/inference/mock_detector.hpp"
#include "sentinel/video/mock_video_source.hpp"
#include "sentinel/video/rtsp_video_source.hpp"
#include "sentinel/video/video_source.hpp"

#include <memory>
#include <stdexcept>

namespace sentinel {
namespace {

/**
 * @brief 从摄像头配置列表中找到第一个启用的摄像头
 * @param cameras 摄像头配置列表
 * @return CameraConfig 
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

/**
 * @brief 根据摄像头配置创建视频源
 * @param camera 摄像头配置
 * @return std::unique_ptr<VideoSource> 
 */
std::unique_ptr<VideoSource> make_video_source(const CameraConfig& camera)
{
    if (camera.type == "mock") {
        return std::make_unique<MockVideoSource>(camera);
    }
    if (camera.type == "rtsp") {
        return std::make_unique<RtspVideoSource>(camera);
    }

    throw std::runtime_error("unsupported camera type: " + camera.type);
}

} // namespace

/**
 * @brief 示例管道：从视频源读取帧，使用检测器进行检测，并通过事件构建器生成事件
 * @param config 
 * @param stop_requested 
 * @return PipelineResult 
 */
PipelineResult run_demo_pipeline(const SentinelConfig& config,
                                 const std::function<bool()>& stop_requested)
{
    // 1. 选择第一个启用的摄像头，并创建视频源
    const auto camera = first_enabled_camera(config.cameras);
    auto video_source = make_video_source(camera);
    if (!video_source->open()) {
        throw std::runtime_error("unable to open " + std::string(video_source->kind()) + " source: " +
                                 std::string(video_source->last_error()));
    }

    const MockDetector detector(config.rules);
    EventBuilder event_builder(config.rules);

    PipelineResult result;

    for (int frame_index = 0; frame_index < config.service.max_frames; ++frame_index) {
        if (stop_requested && stop_requested()) {
            break;
        }

        const auto frame = video_source->read_frame();
        if (!frame.has_value()) {
            break;
        }

        const auto detections = detector.detect(*frame);
        result.frames_processed += 1;
        result.detections_seen += static_cast<int>(detections.size());

        auto events = event_builder.observe(*frame, detections);
        result.events.insert(result.events.end(), events.begin(), events.end());
    }

    video_source->close();
    return result;
}

} // namespace sentinel
