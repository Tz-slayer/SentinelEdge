#include "sentinel/preprocess/opencv_frame_preprocessor.hpp"

#include <utility>

namespace sentinel {

/**
 * @brief 未启用 OpenCV 编译选项时构造占位策略。
 * @param config 图像预处理输出配置。
 */
OpenCvFramePreprocessor::OpenCvFramePreprocessor(PreprocessConfig config)
    : config_(std::move(config))
{
}

/**
 * @brief 报告当前二进制未启用 OpenCV 预处理。
 * @return 固定返回 `false`。
 */
bool OpenCvFramePreprocessor::open()
{
    last_error_ = "OpenCV frame preprocessor is not enabled at build time";
    return false;
}

/**
 * @brief 占位释放函数。
 */
void OpenCvFramePreprocessor::close() noexcept
{
    is_open_ = false;
}

/**
 * @brief 占位预处理函数。
 * @param frame 视频源输出的原始帧。
 * @return 固定返回空。
 */
std::optional<TensorBuffer> OpenCvFramePreprocessor::process(const Frame& frame)
{
    static_cast<void>(frame);
    last_error_ = "OpenCV frame preprocessor is not enabled at build time";
    return std::nullopt;
}

/**
 * @brief 返回预处理策略类型标识。
 * @return 固定返回 `"opencv"`。
 */
std::string_view OpenCvFramePreprocessor::kind() const noexcept
{
    return "opencv";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view OpenCvFramePreprocessor::last_error() const noexcept
{
    return last_error_;
}

} // namespace sentinel
