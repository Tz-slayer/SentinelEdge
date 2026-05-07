#include "sentinel/video/video_source_factory.hpp"

#include "sentinel/video/camera_video_source.hpp"
#if defined(SENTINEL_ENABLE_MOCK_SOURCES) && SENTINEL_ENABLE_MOCK_SOURCES
#include "sentinel/video/mock_video_source.hpp"
#endif
#include "sentinel/video/rtsp_video_source.hpp"

#include <memory>
#include <stdexcept>

namespace sentinel {

/**
 * @brief 根据摄像头类型创建具体视频源策略。
 * @param camera 摄像头配置。
 * @return 新创建的视频源策略对象。
 */
std::unique_ptr<VideoSource> create_video_source(const CameraConfig& camera)
{
#if defined(SENTINEL_ENABLE_MOCK_SOURCES) && SENTINEL_ENABLE_MOCK_SOURCES
    if (camera.type == "mock") {
        return std::make_unique<MockVideoSource>(camera);
    }
#endif
    if (camera.type == "v4l2") {
        return std::make_unique<CameraVideoSource>(camera);
    }
    if (camera.type == "rtsp") {
        return std::make_unique<RtspVideoSource>(camera);
    }

    throw std::runtime_error("unsupported camera type: " + camera.type);
}

} // namespace sentinel
