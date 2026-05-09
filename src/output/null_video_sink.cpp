#include "sentinel/output/null_video_sink.hpp"

namespace sentinel {

/**
 * @brief 初始化空输出通道。
 * @return 固定返回 `true`。
 */
bool NullVideoSink::open()
{
    last_error_.clear();
    return true;
}

/**
 * @brief 关闭空输出通道。
 */
void NullVideoSink::close() noexcept
{
}

/**
 * @brief 丢弃当前帧和检测结果。
 * @param frame 视频源输出的原始帧。
 * @param detections 当前帧检测结果。
 * @return 固定返回 `true`。
 */
bool NullVideoSink::write(const Frame& frame, const std::vector<Detection>& detections)
{
    static_cast<void>(frame);
    static_cast<void>(detections);
    return true;
}

/**
 * @brief 返回输出通道类型标识。
 * @return 固定返回 `"none"`。
 */
std::string_view NullVideoSink::kind() const noexcept
{
    return "none";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 当前实现始终返回空字符串。
 */
std::string_view NullVideoSink::last_error() const noexcept
{
    return last_error_;
}

} // namespace sentinel
