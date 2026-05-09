#include "sentinel/output/video_sink_factory.hpp"

#include "sentinel/output/debug_image_sink.hpp"
#include "sentinel/output/mjpeg_http_sink.hpp"
#include "sentinel/output/null_video_sink.hpp"

#include <memory>
#include <stdexcept>

namespace sentinel {

/**
 * @brief 根据配置创建视频输出通道。
 * @param output 输出通道配置。
 * @param overlay 画框叠加配置。
 * @param service 服务级配置，提供数据目录。
 * @return 新创建的视频输出通道。
 */
std::unique_ptr<VideoSink> create_video_sink(const OutputConfig& output,
                                             const OverlayConfig& overlay,
                                             const ServiceConfig& service)
{
    if (output.video_sink == "none") {
        return std::make_unique<NullVideoSink>();
    }
    if (output.video_sink == "debug_image") {
        return std::make_unique<DebugImageSink>(output, overlay, service.data_dir);
    }
    if (output.video_sink == "mjpeg") {
        return std::make_unique<MjpegHttpSink>(output, overlay);
    }

    throw std::runtime_error("unsupported output.video_sink: " + output.video_sink);
}

} // namespace sentinel
