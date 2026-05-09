#include "sentinel/output/mjpeg_http_sink.hpp"

#include <utility>

namespace sentinel {

/**
 * @brief 构造未启用 OpenCV 时的 MJPEG 输出占位对象。
 * @param output 输出通道配置。
 * @param overlay 画框叠加配置。
 */
MjpegHttpSink::MjpegHttpSink(OutputConfig output, OverlayConfig overlay)
    : output_(std::move(output))
    , overlay_(std::move(overlay))
{
}

/**
 * @brief 报告当前二进制未启用 MJPEG 输出所需的 OpenCV 图像能力。
 * @return 固定返回 `false`。
 */
bool MjpegHttpSink::open()
{
    last_error_ = "mjpeg sink requires OpenCV image backend at build time";
    return false;
}

/**
 * @brief 占位关闭函数。
 */
void MjpegHttpSink::close() noexcept
{
    is_open_ = false;
}

/**
 * @brief 占位写入函数。
 * @param frame 视频源输出的原始帧。
 * @param detections 当前帧检测结果。
 * @return 固定返回 `false`。
 */
bool MjpegHttpSink::write(const Frame& frame, const std::vector<Detection>& detections)
{
    static_cast<void>(frame);
    static_cast<void>(detections);
    last_error_ = "mjpeg sink requires OpenCV image backend at build time";
    return false;
}

/**
 * @brief 返回输出通道类型标识。
 * @return 固定返回 `"mjpeg"`。
 */
std::string_view MjpegHttpSink::kind() const noexcept
{
    return "mjpeg";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view MjpegHttpSink::last_error() const noexcept
{
    return last_error_;
}

/**
 * @brief 占位接收客户端函数。
 */
void MjpegHttpSink::accept_pending_clients()
{
}

/**
 * @brief 占位广播函数。
 * @param jpeg 已编码 JPEG 字节。
 */
void MjpegHttpSink::broadcast_jpeg(const std::vector<std::uint8_t>& jpeg)
{
    static_cast<void>(jpeg);
}

/**
 * @brief 占位移除客户端函数。
 * @param index 客户端下标。
 */
void MjpegHttpSink::remove_client(std::size_t index) noexcept
{
    static_cast<void>(index);
}

} // namespace sentinel
