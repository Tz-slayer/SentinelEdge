#pragma once

#include "sentinel/preprocess/frame_preprocessor.hpp"

#include <string>

namespace sentinel {

/**
 * @brief DVPP 图像预处理策略占位实现。
 *
 * 该类先固定返回未实现错误，用于把策略边界落到代码结构中。后续接入
 * Ascend DVPP 时，应在该类内部管理 DVPP 通道、Device 内存和硬件解码资源。
 */
class DvppFramePreprocessor final : public FramePreprocessor {
public:
    /**
     * @brief 使用预处理配置构造 DVPP 策略。
     * @param config 图像预处理输出尺寸、布局和数据类型配置。
     */
    explicit DvppFramePreprocessor(PreprocessConfig config);

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
    PreprocessConfig config_;
    std::string last_error_;
};

} // namespace sentinel
