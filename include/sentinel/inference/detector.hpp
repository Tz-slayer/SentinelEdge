#pragma once

#include "sentinel/common/types.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sentinel {

/**
 * @brief 异步推理 slot 回收结果。
 */
struct DetectorAsyncResult {
    std::size_t slot_index{0};
    TensorBuffer input_tensor;
    std::vector<Detection> detections;
    std::string debug_info;
};

/**
 * @brief 目标检测策略接口。
 *
 * 不同推理后端或不同 YOLO 模型都通过该接口接入上层流水线，
 * 以便在不改动视频采集和事件聚合代码的情况下切换实现。
 */
class Detector {
public:
    /**
     * @brief 释放检测器资源。
     */
    virtual ~Detector() = default;

    Detector(const Detector&) = delete;
    Detector& operator=(const Detector&) = delete;

    Detector(Detector&&) = delete;
    Detector& operator=(Detector&&) = delete;

    /**
     * @brief 初始化检测器资源。
     * @return 成功返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    virtual bool open() = 0;

    /**
     * @brief 释放检测器持有的运行时资源。
     */
    virtual void close() noexcept = 0;

    /**
     * @brief 对模型输入张量执行目标检测。
     * @param tensor 已完成预处理的模型输入张量。
     * @return 当前帧的检测结果列表；失败时返回空列表并更新 `last_error()`。
     */
    virtual std::vector<Detection> detect(const TensorBuffer& tensor) = 0;

    /**
     * @brief 尝试获取检测器自有的可写模型输入缓冲区。
     * @param metadata 预处理阶段需要保留的张量元数据。
     * @return 支持外部写入时返回 Device 张量视图；否则返回空。
     *
     * 该接口用于 DVPP 等硬件预处理直接写入模型输入 Device buffer，
     * 避免 `Device -> Host -> Device` 往返拷贝。默认实现返回空，表示
     * 检测器不支持该优化路径。
     */
    virtual std::optional<TensorBuffer> mutable_input_tensor(const TensorBuffer& metadata)
    {
        static_cast<void>(metadata);
        return std::nullopt;
    }

    /**
     * @brief 返回检测器支持的异步推理 slot 数量。
     * @return 默认返回 0，表示该检测器不支持异步 slot 调度。
     */
    virtual std::size_t async_slot_count() const noexcept
    {
        return 0;
    }

    /**
     * @brief 获取指定异步 slot 的可写模型输入缓冲区。
     * @param metadata 预处理阶段需要保留的张量元数据。
     * @param slot_index 异步 slot 下标。
     * @return 支持外部写入且 slot 空闲时返回 Device 张量视图，否则返回空。
     */
    virtual std::optional<TensorBuffer> mutable_input_tensor_for_slot(
        const TensorBuffer& metadata,
        std::size_t slot_index)
    {
        static_cast<void>(metadata);
        static_cast<void>(slot_index);
        return std::nullopt;
    }

    /**
     * @brief 返回指定异步 slot 的原生执行流句柄。
     * @param slot_index 异步 slot 下标。
     * @return 支持硬件异步串联时返回原生 stream 指针，否则返回空指针。
     *
     * 该接口用于让 DVPP 预处理和 AscendCL 推理排入同一条 stream。
     * 上层只能把该值作为不透明句柄传给硬件预处理策略，不拥有该资源，
     * 也不能跨线程销毁或缓存到 slot 生命周期之外。
     */
    virtual void* native_stream_for_slot(std::size_t slot_index) noexcept
    {
        static_cast<void>(slot_index);
        return nullptr;
    }

    /**
     * @brief 向指定异步 slot 提交一次模型推理。
     * @param slot_index 异步 slot 下标。
     * @param tensor 已完成预处理的模型输入张量。
     * @return 成功提交返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    virtual bool submit_async(std::size_t slot_index, const TensorBuffer& tensor)
    {
        static_cast<void>(slot_index);
        static_cast<void>(tensor);
        return false;
    }

    /**
     * @brief 回收指定异步 slot 的推理结果。
     * @param slot_index 异步 slot 下标。
     * @return slot 完成且回收成功返回结果；slot 不忙或失败返回空并更新 `last_error()`。
     */
    virtual std::optional<DetectorAsyncResult> collect_async(std::size_t slot_index)
    {
        static_cast<void>(slot_index);
        return std::nullopt;
    }

    /**
     * @brief 返回检测器后端类型标识。
     * @return 例如 `"mock"`、`"ascendcl"` 的稳定字符串。
     */
    virtual std::string_view kind() const noexcept = 0;

    /**
     * @brief 返回最近一次错误的可读文本。
     * @return 若当前没有错误则返回空字符串。
     */
    virtual std::string_view last_error() const noexcept = 0;

    /**
     * @brief 返回最近一次推理的调试摘要。
     * @return 调试摘要；若当前没有可用调试信息则返回空字符串。
     */
    virtual std::string_view debug_info() const noexcept = 0;

protected:
    /**
     * @brief 允许派生类默认构造基类。
     */
    Detector() = default;
};

} // namespace sentinel
