#pragma once

#include "sentinel/image/image_backend.hpp"

#include <string>

namespace sentinel {

/**
 * @brief 基于 OpenCV 的图像处理后端。
 *
 * 当前实现支持 MJPEG/JPEG、YUYV、RGB24、BGR24 到 Host BGR24 图像的
 * 转换，并提供 CPU 缩放、NCHW FP32 张量打包和检测框绘制能力。
 */
class OpenCvImageBackend final : public ImageBackend {
public:
    /**
     * @brief 构造 OpenCV 图像处理后端。
     */
    OpenCvImageBackend() = default;

    /**
     * @brief 初始化 OpenCV 图像处理后端。
     * @return 成功返回 `true`。
     */
    bool open() override;

    /**
     * @brief 释放 OpenCV 图像处理后端资源。
     */
    void close() noexcept override;

    /**
     * @brief 将视频源帧解码为 BGR24 图像。
     * @param frame 视频源输出的原始帧。
     * @return 成功返回 BGR24 图像；失败返回空。
     */
    std::optional<ImageBuffer> decode(const Frame& frame) override;

    /**
     * @brief 将图像缩放到指定尺寸。
     * @param image 输入图像。
     * @param width 输出宽度。
     * @param height 输出高度。
     * @return 成功返回缩放后的图像。
     */
    std::optional<ImageBuffer> resize(const ImageBuffer& image, int width, int height) override;

    /**
     * @brief 将 BGR24 图像转换为模型输入张量。
     * @param image 输入图像。
     * @param config 预处理输出配置。
     * @param frame_sequence 来源帧序号。
     * @param camera_id 来源摄像头 ID。
     * @return 成功返回 NCHW FP32 张量。
     */
    std::optional<TensorBuffer> to_tensor(const ImageBuffer& image,
                                          const PreprocessConfig& config,
                                          int frame_sequence,
                                          const std::string& camera_id) override;

    /**
     * @brief 在图像上绘制检测框和标签。
     * @param image 输入图像。
     * @param detections 检测结果列表。
     * @return 成功返回绘制后的图像。
     */
    std::optional<ImageBuffer> draw_detections(
        const ImageBuffer& image,
        const std::vector<Detection>& detections) override;

    /**
     * @brief 返回图像后端类型标识。
     * @return 固定返回 `"opencv"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误文本。
     * @return 错误文本。
     */
    std::string_view last_error() const noexcept override;

private:
    std::string last_error_;
    bool is_open_{false};
};

} // namespace sentinel
