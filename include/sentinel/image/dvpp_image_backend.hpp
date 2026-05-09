#pragma once

#include "sentinel/image/image_backend.hpp"

#include <string>

namespace sentinel {

/**
 * @brief DVPP 图像处理后端占位实现。
 *
 * 该类统一承载未来 DVPP 解码、缩放、格式转换、OSD/画框等硬件能力。
 * 当前先明确返回未实现错误，避免上层误以为已经启用硬件图像处理。
 */
class DvppImageBackend final : public ImageBackend {
public:
    /**
     * @brief 构造 DVPP 图像处理后端占位对象。
     */
    DvppImageBackend() = default;

    /**
     * @brief 报告 DVPP 图像后端尚未实现。
     * @return 固定返回 `false`。
     */
    bool open() override;

    /**
     * @brief 占位释放函数。
     */
    void close() noexcept override;

    /**
     * @brief 占位解码函数。
     * @param frame 视频源输出的原始帧。
     * @return 固定返回空。
     */
    std::optional<ImageBuffer> decode(const Frame& frame) override;

    /**
     * @brief 占位缩放函数。
     * @param image 输入图像。
     * @param width 输出宽度。
     * @param height 输出高度。
     * @return 固定返回空。
     */
    std::optional<ImageBuffer> resize(const ImageBuffer& image, int width, int height) override;

    /**
     * @brief 占位张量转换函数。
     * @param image 输入图像。
     * @param config 预处理输出配置。
     * @param frame_sequence 来源帧序号。
     * @param camera_id 来源摄像头 ID。
     * @return 固定返回空。
     */
    std::optional<TensorBuffer> to_tensor(const ImageBuffer& image,
                                          const PreprocessConfig& config,
                                          int frame_sequence,
                                          const std::string& camera_id) override;

    /**
     * @brief 占位检测框绘制函数。
     * @param image 输入图像。
     * @param detections 检测结果列表。
     * @return 固定返回空。
     */
    std::optional<ImageBuffer> draw_detections(
        const ImageBuffer& image,
        const std::vector<Detection>& detections) override;

    /**
     * @brief 返回图像后端类型标识。
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
     * @brief 设置统一的未实现错误。
     */
    void set_unimplemented_error();

    std::string last_error_;
};

} // namespace sentinel
