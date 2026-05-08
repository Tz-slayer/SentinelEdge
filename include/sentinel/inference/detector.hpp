#pragma once

#include "sentinel/common/types.hpp"

#include <string_view>
#include <vector>

namespace sentinel {

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
