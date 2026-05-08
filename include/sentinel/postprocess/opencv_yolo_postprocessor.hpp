#pragma once

#include "sentinel/postprocess/detection_postprocessor.hpp"

#include <string>

namespace sentinel {

/**
 * @brief 基于 OpenCV 的 YOLO 后处理策略。
 *
 * 当前实现解析 FP32 YOLO 输出，支持 `channels_first` 和 `anchors_first`
 * 两种常见布局，并使用 OpenCV DNN 的 NMS 过滤重叠框。
 */
class OpenCvYoloPostprocessor final : public DetectionPostprocessor {
public:
    /**
     * @brief 使用后处理配置和检测规则构造 OpenCV YOLO 后处理器。
     * @param config 后处理运行配置。
     * @param rules 检测过滤规则。
     */
    OpenCvYoloPostprocessor(PostprocessConfig config, RuleConfig rules);

    /**
     * @brief 校验后处理配置。
     * @return 成功返回 `true`，失败返回 `false`。
     */
    bool open() override;

    /**
     * @brief 释放后处理资源。
     */
    void close() noexcept override;

    /**
     * @brief 将 YOLO 原始输出转换为检测结果。
     * @param outputs 模型输出缓冲区列表。
     * @param input_tensor 本次推理输入张量。
     * @return 后处理后的检测结果列表。
     */
    std::vector<Detection> process(const std::vector<ModelOutputBuffer>& outputs,
                                   const TensorBuffer& input_tensor) override;

    /**
     * @brief 返回后处理策略类型。
     * @return 固定返回 `"opencv"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误文本。
     * @return 错误文本。
     */
    std::string_view last_error() const noexcept override;

    /**
     * @brief 返回最近一次后处理调试摘要。
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
