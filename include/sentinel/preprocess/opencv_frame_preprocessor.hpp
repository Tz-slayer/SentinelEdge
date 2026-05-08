#pragma once

#include "sentinel/preprocess/frame_preprocessor.hpp"

#include <string>

namespace sentinel {

/**
 * @brief 基于 OpenCV 的图像预处理策略。
 *
 * 当前实现用于开发板调试阶段的软解码、缩放、颜色转换和 NCHW FP32
 * 张量打包。该策略主要用于验证端到端数据格式正确性，后续可与 DVPP
 * 实现进行性能对比。
 */
class OpenCvFramePreprocessor final : public FramePreprocessor {
public:
    /**
     * @brief 使用预处理配置构造 OpenCV 策略。
     * @param config 图像预处理输出尺寸、布局和数据类型配置。
     */
    explicit OpenCvFramePreprocessor(PreprocessConfig config);

    /**
     * @brief 校验配置并初始化 OpenCV 策略。
     * @return 成功返回 `true`，失败返回 `false`。
     */
    bool open() override;

    /**
     * @brief 释放 OpenCV 策略资源。
     */
    void close() noexcept override;

    /**
     * @brief 将视频帧转换为模型输入张量。
     * @param frame 视频源输出的原始帧。
     * @return 成功返回 NCHW FP32 张量；失败返回空。
     */
    std::optional<TensorBuffer> process(const Frame& frame) override;

    /**
     * @brief 返回预处理策略类型标识。
     * @return 固定返回 `"opencv"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误文本。
     * @return 错误文本；若无错误则为空。
     */
    std::string_view last_error() const noexcept override;

private:
    PreprocessConfig config_;
    std::string last_error_;
    bool is_open_{false};
};

} // namespace sentinel
