#pragma once

#include "sentinel/image/image_backend.hpp"
#include "sentinel/output/video_sink.hpp"

#include <memory>
#include <string>
#include <vector>

namespace sentinel {

/**
 * @brief 通过 HTTP MJPEG 提供浏览器调试预览的视频输出通道。
 *
 * 该输出通道使用 Linux socket 系统调用监听本地端口，把画框后的 JPEG 帧按
 * `multipart/x-mixed-replace` 格式推给浏览器。它只用于本地调试预览，不承担
 * 生产级流媒体网关职责。
 */
class MjpegHttpSink final : public VideoSink {
public:
    /**
     * @brief 构造 MJPEG HTTP 输出通道。
     * @param output 输出通道配置。
     * @param overlay 画框叠加配置。
     */
    MjpegHttpSink(OutputConfig output, OverlayConfig overlay);

    /**
     * @brief 创建监听 socket 并初始化图像后端。
     * @return 成功返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    bool open() override;

    /**
     * @brief 关闭监听 socket、客户端连接和图像后端。
     */
    void close() noexcept override;

    /**
     * @brief 将当前帧编码为 JPEG 并广播给已连接的浏览器。
     * @param frame 视频源输出的原始帧。
     * @param detections 当前帧检测结果。
     * @return 成功返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    bool write(const Frame& frame, const std::vector<Detection>& detections) override;

    /**
     * @brief 返回输出通道类型标识。
     * @return 固定返回 `"mjpeg"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误文本。
     * @return 错误文本。
     */
    std::string_view last_error() const noexcept override;

private:
    /**
     * @brief 接收当前所有待处理客户端连接。
     */
    void accept_pending_clients();

    /**
     * @brief 将一张 JPEG 帧发送给所有客户端。
     * @param jpeg 已编码 JPEG 字节。
     */
    void broadcast_jpeg(const std::vector<std::uint8_t>& jpeg);

    /**
     * @brief 从客户端列表中移除指定下标。
     * @param index 客户端下标。
     */
    void remove_client(std::size_t index) noexcept;

    OutputConfig output_;
    OverlayConfig overlay_;
    std::unique_ptr<ImageBackend> image_backend_;
    std::vector<int> clients_;
    std::string last_error_;
    int listen_fd_{-1};
    bool is_open_{false};
};

} // namespace sentinel
