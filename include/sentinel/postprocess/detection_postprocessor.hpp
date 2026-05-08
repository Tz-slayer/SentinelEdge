#pragma once

#include "sentinel/common/types.hpp"

#include <string_view>
#include <vector>

namespace sentinel {

/**
 * @brief 模型输出后处理策略接口。
 *
 * 该接口负责把推理后端输出的原始张量转换为统一的 `Detection` 列表。
 * 不同 YOLO 版本、CPU/OpenCV 后处理或后续硬件相关实现都通过该接口接入。
 */
class DetectionPostprocessor {
public:
    /**
     * @brief 释放后处理策略资源。
     */
    virtual ~DetectionPostprocessor() = default;

    DetectionPostprocessor(const DetectionPostprocessor&) = delete;
    DetectionPostprocessor& operator=(const DetectionPostprocessor&) = delete;

    DetectionPostprocessor(DetectionPostprocessor&&) = delete;
    DetectionPostprocessor& operator=(DetectionPostprocessor&&) = delete;

    /**
     * @brief 初始化后处理策略。
     * @return 成功返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    virtual bool open() = 0;

    /**
     * @brief 释放后处理策略资源。
     */
    virtual void close() noexcept = 0;

    /**
     * @brief 将模型原始输出转换为检测结果。
     * @param outputs 推理后端输出缓冲区列表。
     * @param input_tensor 本次推理对应的输入张量元数据。
     * @return 后处理后的检测结果列表；失败时返回空列表并更新 `last_error()`。
     */
    virtual std::vector<Detection> process(const std::vector<ModelOutputBuffer>& outputs,
                                           const TensorBuffer& input_tensor) = 0;

    /**
     * @brief 返回后处理策略类型标识。
     * @return 例如 `"opencv"` 或 `"dvpp"` 的稳定字符串。
     */
    virtual std::string_view kind() const noexcept = 0;

    /**
     * @brief 返回最近一次错误文本。
     * @return 若当前没有错误则返回空字符串。
     */
    virtual std::string_view last_error() const noexcept = 0;

    /**
     * @brief 返回最近一次后处理调试摘要。
     * @return 调试摘要；若当前没有摘要则返回空字符串。
     */
    virtual std::string_view debug_info() const noexcept = 0;

protected:
    /**
     * @brief 允许派生类默认构造基类。
     */
    DetectionPostprocessor() = default;
};

} // namespace sentinel
