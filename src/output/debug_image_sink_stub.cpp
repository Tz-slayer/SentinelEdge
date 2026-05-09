#include "sentinel/output/debug_image_sink.hpp"

#include <utility>

namespace sentinel {

/**
 * @brief 构造未启用调试视频输出时的调试图占位对象。
 * @param output 输出通道配置。
 * @param overlay 画框叠加配置。
 * @param data_dir 运行数据根目录。
 */
DebugImageSink::DebugImageSink(OutputConfig output,
                               OverlayConfig overlay,
                               std::filesystem::path data_dir)
    : output_(std::move(output))
    , overlay_(std::move(overlay))
    , output_dir_(std::move(data_dir) / output_.debug_image_dir)
{
}

/**
 * @brief 报告当前二进制未启用调试图输出。
 * @return 固定返回 `false`。
 */
bool DebugImageSink::open()
{
    last_error_ = "debug_image sink requires ENABLE_DEBUG_VIDEO_SINKS=ON at build time";
    return false;
}

/**
 * @brief 占位关闭函数。
 */
void DebugImageSink::close() noexcept
{
    is_open_ = false;
}

/**
 * @brief 占位写入函数。
 * @param frame 视频源输出的原始帧。
 * @param detections 当前帧检测结果。
 * @return 固定返回 `false`。
 */
bool DebugImageSink::write(const Frame& frame, const std::vector<Detection>& detections)
{
    static_cast<void>(frame);
    static_cast<void>(detections);
    last_error_ = "debug_image sink requires ENABLE_DEBUG_VIDEO_SINKS=ON at build time";
    return false;
}

/**
 * @brief 返回输出通道类型标识。
 * @return 固定返回 `"debug_image"`。
 */
std::string_view DebugImageSink::kind() const noexcept
{
    return "debug_image";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view DebugImageSink::last_error() const noexcept
{
    return last_error_;
}

/**
 * @brief 生成当前帧的输出文件路径。
 * @param frame 当前视频帧。
 * @return JPEG 文件路径。
 */
std::filesystem::path DebugImageSink::make_output_path(const Frame& frame) const
{
    return output_dir_ / ("frame-" + frame.camera_id + "-" + std::to_string(frame.sequence) + ".jpg");
}

} // namespace sentinel
