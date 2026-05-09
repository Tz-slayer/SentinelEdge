#pragma once

#include "sentinel/preprocess/frame_preprocessor.hpp"

#include <memory>
#include <string>

namespace sentinel {

/**
 * @brief DVPP 图像预处理策略。
 *
 * MJPEG 输入帧先在 DVPP 中完成 JPEGD 解码和 VPC 缩放。当前主线固定面向
 * 静态 AIPP 模型输出 `NV12`/`UINT8`；当检测器提供 Device 输入缓冲区时，
 * 预处理结果会直接写入 AscendCL Device 内存。
 */
class DvppFramePreprocessor final : public FramePreprocessor {
public:
    /**
     * @brief 使用预处理配置构造 DVPP 策略。
     * @param config 图像预处理输出尺寸、布局和数据类型配置。
     */
    explicit DvppFramePreprocessor(PreprocessConfig config);

    /**
     * @brief 释放 DVPP 策略资源。
     */
    ~DvppFramePreprocessor() override;

    /**
     * @brief 初始化 DVPP 运行时、stream 和处理通道。
     * @return 成功返回 `true`，失败返回 `false`。
     */
    bool open() override;

    /**
     * @brief 释放 DVPP 运行时资源。
     */
    void close() noexcept override;

    /**
     * @brief 将 MJPEG 帧转换为模型输入张量。
     * @param frame 视频源输出的原始帧。
     * @return 成功返回张量；失败返回空。
     */
    std::optional<TensorBuffer> process(const Frame& frame) override;

    /**
     * @brief 将 MJPEG 帧通过 DVPP 直接写入目标张量。
     * @param frame 视频源输出的原始帧。
     * @param target 目标张量；静态 AIPP 路径下可为 AscendCL Device 输入缓冲区。
     * @return 成功返回目标张量；失败返回空。
     */
    std::optional<TensorBuffer> process_into(const Frame& frame, TensorBuffer target) override;

    /**
     * @brief 返回预处理策略类型标识。
     * @return 固定返回 `"dvpp"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误文本。
     * @return 错误文本。
     */
    std::string_view last_error() const noexcept override;

private:
    /**
     * @brief DVPP 资源和硬件 API 细节。
     */
    class Impl;

    PreprocessConfig config_;
    std::unique_ptr<Impl> impl_;
    std::string last_error_;
};

} // namespace sentinel
