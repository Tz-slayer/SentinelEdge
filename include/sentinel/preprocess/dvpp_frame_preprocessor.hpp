#pragma once

#include "sentinel/preprocess/frame_preprocessor.hpp"

#include <memory>
#include <string>

namespace sentinel {

/**
 * @brief DVPP 图像预处理策略。
 *
 * 当前实现面向 copy 模式：MJPEG 输入帧先在 DVPP 中完成 JPEGD 解码和 VPC
 * 缩放，再拷回 Host 侧打包为 NCHW FP32 Tensor。该阶段用于和 OpenCV
 * 预处理链路做性能对比，后续零拷贝版本会继续把输出保留在 Device 内存中。
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
     * @brief 报告 DVPP 策略尚未实现。
     * @return 固定返回 `false`。
     */
    bool open() override;

    /**
     * @brief 占位释放函数。
     */
    void close() noexcept override;

    /**
     * @brief 占位预处理函数。
     * @param frame 视频源输出的原始帧。
     * @return 固定返回空。
     */
    std::optional<TensorBuffer> process(const Frame& frame) override;

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
