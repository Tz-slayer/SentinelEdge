#include "sentinel/postprocess/opencv_yolo_postprocessor.hpp"

#include <utility>

namespace sentinel {

/**
 * @brief 未启用 OpenCV 后处理编译选项时构造占位对象。
 * @param config 后处理配置。
 * @param rules 检测过滤规则。
 */
OpenCvYoloPostprocessor::OpenCvYoloPostprocessor(PostprocessConfig config, RuleConfig rules)
    : config_(std::move(config))
    , rules_(std::move(rules))
{
}

/**
 * @brief 报告当前二进制未启用 OpenCV 后处理。
 * @return 固定返回 `false`。
 */
bool OpenCvYoloPostprocessor::open()
{
    last_error_ = "OpenCV YOLO postprocessor is not enabled at build time";
    debug_info_ = last_error_;
    return false;
}

/**
 * @brief 占位释放函数。
 */
void OpenCvYoloPostprocessor::close() noexcept
{
    is_open_ = false;
}

/**
 * @brief 占位后处理函数。
 * @param outputs 模型输出缓冲区列表。
 * @param input_tensor 本次推理输入张量。
 * @return 固定返回空列表。
 */
std::vector<Detection> OpenCvYoloPostprocessor::process(
    const std::vector<ModelOutputBuffer>& outputs,
    const TensorBuffer& input_tensor)
{
    static_cast<void>(outputs);
    static_cast<void>(input_tensor);
    last_error_ = "OpenCV YOLO postprocessor is not enabled at build time";
    debug_info_ = last_error_;
    return {};
}

/**
 * @brief 返回后处理策略类型。
 * @return 固定返回 `"opencv"`。
 */
std::string_view OpenCvYoloPostprocessor::kind() const noexcept
{
    return "opencv";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view OpenCvYoloPostprocessor::last_error() const noexcept
{
    return last_error_;
}

/**
 * @brief 返回最近一次后处理调试摘要。
 * @return 调试摘要文本。
 */
std::string_view OpenCvYoloPostprocessor::debug_info() const noexcept
{
    return debug_info_;
}

} // namespace sentinel
