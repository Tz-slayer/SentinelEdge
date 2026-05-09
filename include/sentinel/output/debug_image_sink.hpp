#pragma once

#include "sentinel/image/image_backend.hpp"
#include "sentinel/output/video_sink.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace sentinel {

/**
 * @brief 将带框调试图保存为 JPEG 的输出通道。
 *
 * 该输出通道用于在 RTSP 推流前验证检测框坐标、类别和置信度是否正确。
 * 它会复用 `ImageBackend` 完成解码和画框，最后通过 OpenCV 保存 JPEG。
 */
class DebugImageSink final : public VideoSink {
public:
    /**
     * @brief 构造调试图输出通道。
     * @param output 输出通道配置。
     * @param overlay 画框叠加配置。
     * @param data_dir 运行数据根目录。
     */
    DebugImageSink(OutputConfig output, OverlayConfig overlay, std::filesystem::path data_dir);

    /**
     * @brief 初始化图像后端并创建输出目录。
     * @return 成功返回 `true`。
     */
    bool open() override;

    /**
     * @brief 关闭图像后端。
     */
    void close() noexcept override;

    /**
     * @brief 保存一帧调试 JPEG。
     * @param frame 视频源输出的原始帧。
     * @param detections 当前帧检测结果。
     * @return 成功返回 `true`。
     */
    bool write(const Frame& frame, const std::vector<Detection>& detections) override;

    /**
     * @brief 返回输出通道类型标识。
     * @return 固定返回 `"debug_image"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误文本。
     * @return 错误文本。
     */
    std::string_view last_error() const noexcept override;

private:
    /**
     * @brief 生成当前帧的输出文件路径。
     * @param frame 当前视频帧。
     * @return JPEG 文件路径。
     */
    std::filesystem::path make_output_path(const Frame& frame) const;

    OutputConfig output_;
    OverlayConfig overlay_;
    std::filesystem::path output_dir_;
    std::unique_ptr<ImageBackend> image_backend_;
    std::string last_error_;
    int frames_seen_{0};
    bool is_open_{false};
};

} // namespace sentinel
