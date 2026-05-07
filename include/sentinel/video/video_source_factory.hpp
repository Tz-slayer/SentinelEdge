#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/video/video_source.hpp"

#include <memory>

namespace sentinel {

/**
 * @brief 根据摄像头配置创建对应的视频源策略对象。
 * @param camera 摄像头配置。
 * @return 新创建的视频源策略对象。
 */
std::unique_ptr<VideoSource> create_video_source(const CameraConfig& camera);

} // namespace sentinel
