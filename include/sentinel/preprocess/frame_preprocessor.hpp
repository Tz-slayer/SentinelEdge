#pragma once

#include "sentinel/common/types.hpp"

#include <optional>
#include <string_view>

namespace sentinel {

/**
 * @brief 视频帧预处理策略接口。
 *
 * 该接口负责把视频源输出的 `Frame` 转换为推理后端可直接消费的
 * `TensorBuffer`。当前主线实现为 DVPP JPEGD/VPC 输出静态 AIPP 可消费的
 * NV12/UINT8 张量，并优先写入推理后端暴露的 Device 输入缓冲区。
 */
class FramePreprocessor {
public:
    /**
     * @brief 释放预处理策略资源。
     */
    virtual ~FramePreprocessor() = default;

    FramePreprocessor(const FramePreprocessor&) = delete;
    FramePreprocessor& operator=(const FramePreprocessor&) = delete;

    FramePreprocessor(FramePreprocessor&&) = delete;
    FramePreprocessor& operator=(FramePreprocessor&&) = delete;

    /**
     * @brief 初始化预处理策略需要的运行时资源。
     * @return 成功返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    virtual bool open() = 0;

    /**
     * @brief 释放预处理策略持有的资源。
     */
    virtual void close() noexcept = 0;

    /**
     * @brief 将一帧视频数据转换为模型输入张量。
     * @param frame 视频源输出的原始帧，可能是 MJPEG、YUYV 等格式。
     * @return 成功返回张量；失败返回空并更新 `last_error()`。
     */
    virtual std::optional<TensorBuffer> process(const Frame& frame) = 0;

    /**
     * @brief 将一帧视频数据转换到调用方提供的目标张量。
     * @param frame 视频源输出的原始帧。
     * @param target 目标张量缓冲区，通常是推理后端暴露的 Device 输入。
     * @return 成功返回已写入的张量；失败返回空并更新 `last_error()`。
     *
     * 默认实现忽略目标缓冲区并回退到 `process()`，便于测试替身或后续
     * 其他实现先按 Host 输出接入。
     */
    virtual std::optional<TensorBuffer> process_into(const Frame& frame, TensorBuffer target)
    {
        static_cast<void>(target);
        return process(frame);
    }

    /**
     * @brief 返回预处理策略类型标识。
     * @return 固定后端标识，例如 `"dvpp"`。
     */
    virtual std::string_view kind() const noexcept = 0;

    /**
     * @brief 返回最近一次错误的可读文本。
     * @return 若当前没有错误则返回空字符串。
     */
    virtual std::string_view last_error() const noexcept = 0;

protected:
    /**
     * @brief 允许派生类默认构造基类。
     */
    FramePreprocessor() = default;
};

} // namespace sentinel
