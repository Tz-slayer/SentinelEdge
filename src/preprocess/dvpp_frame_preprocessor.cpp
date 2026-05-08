#include "sentinel/preprocess/dvpp_frame_preprocessor.hpp"

#include <utility>

namespace sentinel {

/**
 * @brief 使用预处理配置构造 DVPP 策略。
 * @param config 图像预处理输出配置。
 */
DvppFramePreprocessor::DvppFramePreprocessor(PreprocessConfig config)
    : config_(std::move(config))
{
}

/**
 * @brief 报告 DVPP 策略尚未实现。
 * @return 固定返回 `false`。
 */
bool DvppFramePreprocessor::open()
{
    last_error_ = "DVPP frame preprocessor is not implemented yet";
    return false;
}

/**
 * @brief 占位释放函数。
 */
void DvppFramePreprocessor::close() noexcept
{
}

/**
 * @brief 占位预处理函数。
 * @param frame 视频源输出的原始帧。
 * @return 固定返回空。
 */
std::optional<TensorBuffer> DvppFramePreprocessor::process(const Frame& frame)
{
    static_cast<void>(frame);
    last_error_ = "DVPP frame preprocessor is not implemented yet";
    return std::nullopt;
}

/**
 * @brief 返回预处理策略类型标识。
 * @return 固定返回 `"dvpp"`。
 */
std::string_view DvppFramePreprocessor::kind() const noexcept
{
    return "dvpp";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view DvppFramePreprocessor::last_error() const noexcept
{
    return last_error_;
}

} // namespace sentinel
