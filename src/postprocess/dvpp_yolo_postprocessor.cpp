#include "sentinel/postprocess/dvpp_yolo_postprocessor.hpp"

#include <utility>

namespace sentinel {

/**
 * @brief 使用配置和规则构造 DVPP 后处理占位对象。
 * @param config 后处理配置。
 * @param rules 检测过滤规则。
 */
DvppYoloPostprocessor::DvppYoloPostprocessor(PostprocessConfig config, RuleConfig rules)
    : config_(std::move(config))
    , rules_(std::move(rules))
{
}

/**
 * @brief 报告 DVPP 后处理尚未实现。
 * @return 固定返回 `false`。
 */
bool DvppYoloPostprocessor::open()
{
    last_error_ = "DVPP YOLO postprocessor is not implemented; use opencv backend";
    debug_info_ = last_error_;
    return false;
}

/**
 * @brief 占位释放函数。
 */
void DvppYoloPostprocessor::close() noexcept
{
}

/**
 * @brief 占位后处理函数。
 * @param outputs 模型输出缓冲区列表。
 * @param input_tensor 本次推理输入张量。
 * @return 固定返回空列表。
 */
std::vector<Detection> DvppYoloPostprocessor::process(
    const std::vector<ModelOutputBuffer>& outputs,
    const TensorBuffer& input_tensor)
{
    static_cast<void>(outputs);
    static_cast<void>(input_tensor);
    last_error_ = "DVPP YOLO postprocessor is not implemented; use opencv backend";
    debug_info_ = last_error_;
    return {};
}

/**
 * @brief 返回后处理策略类型。
 * @return 固定返回 `"dvpp"`。
 */
std::string_view DvppYoloPostprocessor::kind() const noexcept
{
    return "dvpp";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view DvppYoloPostprocessor::last_error() const noexcept
{
    return last_error_;
}

/**
 * @brief 返回最近一次调试摘要。
 * @return 调试摘要文本。
 */
std::string_view DvppYoloPostprocessor::debug_info() const noexcept
{
    return debug_info_;
}

} // namespace sentinel
