#pragma once

#include "sentinel/postprocess/detection_postprocessor.hpp"

#include <string>

namespace sentinel {

/**
 * @brief DVPP YOLO 后处理策略占位实现。
 *
 * DVPP 更适合硬解码、缩放和色彩空间转换。YOLO 后处理一般是 CPU 或
 * 专用算子完成，因此当前类只提供明确的未实现边界，避免误用。
 */
class DvppYoloPostprocessor final : public DetectionPostprocessor {
public:
    /**
     * @brief 使用配置和规则构造 DVPP 后处理占位对象。
     * @param config 后处理配置。
     * @param rules 检测过滤规则。
     */
    DvppYoloPostprocessor(PostprocessConfig config, RuleConfig rules);

    /**
     * @brief 报告 DVPP 后处理尚未实现。
     * @return 固定返回 `false`。
     */
    bool open() override;

    /**
     * @brief 占位释放函数。
     */
    void close() noexcept override;

    /**
     * @brief 占位后处理函数。
     * @param outputs 模型输出缓冲区列表。
     * @param input_tensor 本次推理输入张量。
     * @return 固定返回空列表。
     */
    std::vector<Detection> process(const std::vector<ModelOutputBuffer>& outputs,
                                   const TensorBuffer& input_tensor) override;

    /**
     * @brief 返回后处理策略类型。
     * @return 固定返回 `"dvpp"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误文本。
     * @return 错误文本。
     */
    std::string_view last_error() const noexcept override;

    /**
     * @brief 返回最近一次调试摘要。
     * @return 调试摘要文本。
     */
    std::string_view debug_info() const noexcept override;

private:
    PostprocessConfig config_;
    RuleConfig rules_;
    std::string last_error_;
    std::string debug_info_;
};

} // namespace sentinel
