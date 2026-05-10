#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/inference/detector.hpp"

#include <cstddef>
#include <memory>
#include <optional>
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
     * @param postprocess 后处理配置。
     */
    AscendClDetector(InferenceConfig config, RuleConfig rules, PostprocessConfig postprocess);

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
     * @return 成功返回后处理后的检测结果列表；失败返回空列表。
     */
    std::vector<Detection> detect(const TensorBuffer& tensor) override;

    /**
     * @brief 返回 AscendCL 模型输入 Device buffer 视图。
     * @param metadata 预处理阶段生成的张量元数据。
     * @return 成功返回 Device 张量视图；当前模型输入不满足条件时返回空。
     */
    std::optional<TensorBuffer> mutable_input_tensor(const TensorBuffer& metadata) override;

    /**
     * @brief 返回 AscendCL 异步推理 slot 数量。
     * @return 返回按 `pipeline.stream_slots` 创建的 slot 数量。
     */
    std::size_t async_slot_count() const noexcept override;

    /**
     * @brief 返回指定异步 slot 的模型输入 Device buffer 视图。
     * @param metadata 预处理阶段生成的张量元数据。
     * @param slot_index 异步 slot 下标。
     * @return 成功返回 Device 张量视图；slot 不可用时返回空。
     */
    std::optional<TensorBuffer> mutable_input_tensor_for_slot(
        const TensorBuffer& metadata,
        std::size_t slot_index) override;

    /**
     * @brief 返回指定异步 slot 的 AscendCL stream。
     * @param slot_index 异步 slot 下标。
     * @return slot 存在时返回 `aclrtStream` 的不透明指针，否则返回空指针。
     */
    void* native_stream_for_slot(std::size_t slot_index) noexcept override;

    /**
     * @brief 向指定 AscendCL stream slot 提交异步推理。
     * @param slot_index 异步 slot 下标。
     * @param tensor 已完成预处理的模型输入张量。
     * @return 成功提交返回 `true`。
     */
    bool submit_async(std::size_t slot_index, const TensorBuffer& tensor) override;

    /**
     * @brief 同步并回收指定 AscendCL stream slot 的推理结果。
     * @param slot_index 异步 slot 下标。
     * @return 成功返回检测结果；失败返回空。
     */
    std::optional<DetectorAsyncResult> collect_async(std::size_t slot_index) override;

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

    /**
     * @brief 返回最近一次 AscendCL 推理的调试摘要。
     * @return 调试摘要，包含模型输入输出缓冲区大小和输出预览。
     */
    std::string_view debug_info() const noexcept override;

private:
    /**
     * @brief AscendCL 实现细节，避免公共头文件直接依赖 ACL 头文件。
     */
    class Impl;

    std::unique_ptr<Impl> impl_;
};

} // namespace sentinel
