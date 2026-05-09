#pragma once

#include "sentinel/postprocess/detection_postprocessor.hpp"

#include <string>

namespace sentinel {

/**
 * @brief DVPP 链路使用的 YOLO 后处理策略。
 *
 * DVPP 本身主要负责图像解码、缩放和格式转换。当前类提供 `dvpp`
 * 配置入口下的 YOLO 输出解析和纯 C++ NMS，使完整 DVPP 配置链路可以
 * 跑通；该阶段不宣称使用 DVPP 硬件执行 NMS。
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
     * @brief 校验后处理配置。
     * @return 成功返回 `true`，失败返回 `false`。
     */
    bool open() override;

    /**
     * @brief 占位释放函数。
     */
    void close() noexcept override;

    /**
     * @brief 将 YOLO 原始输出转换为检测结果。
     * @param outputs 模型输出缓冲区列表。
     * @param input_tensor 本次推理输入张量。
     * @return 检测结果列表；失败时返回空列表并更新 `last_error()`。
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
    bool is_open_{false};
};

} // namespace sentinel
