#pragma once

#include "sentinel/common/types.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace sentinel {

/**
 * @brief 通用图像处理后端接口。
 *
 * 该接口承载解码、缩放、张量打包和绘制检测框等图像能力。它不直接
 * 表达 pipeline 阶段语义；`FramePreprocessor`、后续 `OverlayRenderer`
 * 等阶段对象应组合该接口完成具体工作。
 */
class ImageBackend {
public:
    /**
     * @brief 释放图像处理后端资源。
     */
    virtual ~ImageBackend() = default;

    ImageBackend(const ImageBackend&) = delete;
    ImageBackend& operator=(const ImageBackend&) = delete;

    ImageBackend(ImageBackend&&) = delete;
    ImageBackend& operator=(ImageBackend&&) = delete;

    /**
     * @brief 初始化图像处理后端。
     * @return 成功返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    virtual bool open() = 0;

    /**
     * @brief 释放后端持有的资源。
     */
    virtual void close() noexcept = 0;

    /**
     * @brief 将视频源帧解码为普通图像缓冲区。
     * @param frame 视频源输出的原始帧。
     * @return 成功返回图像缓冲区；失败返回空并更新 `last_error()`。
     */
    virtual std::optional<ImageBuffer> decode(const Frame& frame) = 0;

    /**
     * @brief 将图像缩放到指定尺寸。
     * @param image 输入图像缓冲区。
     * @param width 输出宽度。
     * @param height 输出高度。
     * @return 成功返回缩放后的图像；失败返回空并更新 `last_error()`。
     */
    virtual std::optional<ImageBuffer> resize(const ImageBuffer& image, int width, int height) = 0;

    /**
     * @brief 将图像转换为模型输入张量。
     * @param image 输入图像缓冲区。
     * @param config 预处理输出配置。
     * @param frame_sequence 来源帧序号。
     * @param camera_id 来源摄像头 ID。
     * @return 成功返回模型输入张量；失败返回空并更新 `last_error()`。
     */
    virtual std::optional<TensorBuffer> to_tensor(const ImageBuffer& image,
                                                  const PreprocessConfig& config,
                                                  int frame_sequence,
                                                  const std::string& camera_id) = 0;

    /**
     * @brief 在图像上绘制检测结果。
     * @param image 输入图像缓冲区。
     * @param detections 待绘制的检测结果，坐标使用归一化矩形。
     * @return 成功返回绘制后的图像；失败返回空并更新 `last_error()`。
     */
    virtual std::optional<ImageBuffer> draw_detections(
        const ImageBuffer& image,
        const std::vector<Detection>& detections) = 0;

    /**
     * @brief 返回图像后端类型标识。
     * @return 例如 `"opencv"` 或 `"dvpp"` 的稳定字符串。
     */
    virtual std::string_view kind() const noexcept = 0;

    /**
     * @brief 返回最近一次错误文本。
     * @return 若当前没有错误则返回空字符串。
     */
    virtual std::string_view last_error() const noexcept = 0;

protected:
    /**
     * @brief 允许派生类默认构造基类。
     */
    ImageBackend() = default;
};

} // namespace sentinel
