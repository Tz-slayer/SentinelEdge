#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/inference/detector.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace sentinel {

/**
 * @brief 基于 AscendCL 加载 `.om` 模型执行推理的检测策略。
 *
 * 当前实现负责 AscendCL 初始化、模型加载、输入输出 Device 内存管理
 * 和同步执行推理；YOLO 输出解析会作为下一阶段按模型族继续细化。
 */
class AscendClDetector final : public Detector {
public:
    /**
     * @brief 使用推理配置和检测规则构造 AscendCL 检测器。
     * @param config 推理后端配置，包含模型路径和设备编号。
     * @param rules 检测过滤规则。
     */
    AscendClDetector(InferenceConfig config, RuleConfig rules);

    /**
     * @brief 释放 AscendCL 资源。
     */
    ~AscendClDetector() override;

    /**
     * @brief 初始化 AscendCL、加载模型并准备输入输出数据集。
     * @return 成功返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    bool open() override;

    /**
     * @brief 释放模型、数据集、Device 内存和 AscendCL 上下文。
     */
    void close() noexcept override;

    /**
     * @brief 使用当前张量数据执行一次 AscendCL 同步推理。
     * @param tensor 待推理的模型输入张量，字节流必须与 `.om` 输入大小一致。
     * @return 当前阶段暂不做 YOLO 后处理，成功推理后返回空检测列表。
     */
    std::vector<Detection> detect(const TensorBuffer& tensor) override;

    /**
     * @brief 返回检测器后端类型。
     * @return 固定返回 `"ascendcl"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误文本。
     * @return 若当前没有错误则返回空字符串。
     */
    std::string_view last_error() const noexcept override;

private:
    /**
     * @brief AscendCL 实现细节，避免公共头文件直接依赖 ACL 头文件。
     */
    class Impl;

    std::unique_ptr<Impl> impl_;
};

} // namespace sentinel
