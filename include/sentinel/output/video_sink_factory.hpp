#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/output/video_sink.hpp"

#include <memory>

namespace sentinel {

/**
 * @brief 根据配置创建视频输出通道。
 * @param output 输出通道配置。
 * @param overlay 画框叠加配置。
 * @param service 服务级配置，提供数据目录。
 * @return 新创建的视频输出通道。
 * @throws std::runtime_error 当输出通道名称不受支持时抛出。
 */
std::unique_ptr<VideoSink> create_video_sink(const OutputConfig& output,
                                             const OverlayConfig& overlay,
                                             const ServiceConfig& service);

} // namespace sentinel
