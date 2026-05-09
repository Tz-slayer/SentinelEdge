#pragma once

#include "sentinel/image/image_backend.hpp"

#include <memory>
#include <string>

namespace sentinel {

/**
 * @brief DVPP 图像处理后端。
 *
 * 当前实现使用 DVPP 完成 MJPEG 解码，返回 Host BGR24 图像给调试图和
 * MJPEG 输出链路。主线推理预处理不经过该类；该类只服务可视化调试输出。
 */
class DvppImageBackend final : public ImageBackend {
public:
    /**
     * @brief 构造 DVPP 图像处理后端对象。
     */
    DvppImageBackend();

    /**
     * @brief 释放 DVPP 图像处理资源。
     */
    ~DvppImageBackend() override;

    /**
     * @brief 初始化 DVPP 图像后端。
     * @return 成功返回 `true`，失败返回 `false`。
     */
    bool open() override;

    /**
     * @brief 释放 DVPP 图像后端资源。
     */
    void close() noexcept override;

    /**
     * @brief 使用 DVPP 将视频帧解码为 Host BGR24 图像。
     * @param frame 视频源输出的原始帧。
     * @return 成功返回图像；失败返回空。
     */
    std::optional<ImageBuffer> decode(const Frame& frame) override;

    /**
     * @brief 缩放 Host BGR24 图像。
     * @param image 输入图像。
     * @param width 输出宽度。
     * @param height 输出高度。
     * @return 成功返回缩放后的图像。
     */
    std::optional<ImageBuffer> resize(const ImageBuffer& image, int width, int height) override;

    /**
     * @brief 在 Host BGR24 图像上绘制检测框。
     * @param image 输入图像。
     * @param detections 检测结果列表。
     * @return 成功返回绘制后的图像。
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
     * @brief DVPP 图像处理实现细节。
     */
    class Impl;

    std::unique_ptr<Impl> impl_;
    std::string last_error_;
};

} // namespace sentinel
