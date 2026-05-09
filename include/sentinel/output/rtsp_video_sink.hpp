#pragma once

#include "sentinel/image/image_backend.hpp"
#include "sentinel/output/video_sink.hpp"

#include <memory>
#include <string>
#include <sys/types.h>

namespace sentinel {

/**
 * @brief 通过外部 ffmpeg 进程输出 RTSP 预览流的视频通道。
 *
 * 该实现复用 `ImageBackend` 完成解码和画框，然后使用 Linux `pipe/fork/exec/write`
 * 把 BGR24 原始帧写入 ffmpeg 的标准输入。ffmpeg 负责 H.264 编码和 RTSP listen
 * 服务。当前实现是 RTSP 功能的第一版，目标是先跑通实时预览闭环。
 */
class RtspVideoSink final : public VideoSink {
public:
    /**
     * @brief 构造 RTSP 输出通道。
     * @param output 输出通道配置。
     * @param overlay 画框叠加配置。
     */
    RtspVideoSink(OutputConfig output, OverlayConfig overlay);

    /**
     * @brief 初始化图像后端并检查 ffmpeg 可执行文件。
     *
     * 该函数会忽略进程级 `SIGPIPE`，避免 ffmpeg 异常退出后主进程因管道写入而被杀死。
     *
     * @return 成功返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    bool open() override;

    /**
     * @brief 关闭 RTSP 输出通道并回收子进程。
     */
    void close() noexcept override;

    /**
     * @brief 写入一帧到 RTSP 输出流。
     * @param frame 视频源输出的原始帧。
     * @param detections 当前帧检测结果。
     * @return 成功返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    bool write(const Frame& frame, const std::vector<Detection>& detections) override;

    /**
     * @brief 返回输出通道类型标识。
     * @return 固定返回 `"rtsp"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误文本。
     * @return 错误文本。
     */
    std::string_view last_error() const noexcept override;

private:
    /**
     * @brief 启动 ffmpeg 子进程并建立标准输入管道。
     * @param width 原始 BGR24 帧宽度。
     * @param height 原始 BGR24 帧高度。
     * @return 成功返回 `true`。
     */
    bool start_ffmpeg(int width, int height);

    /**
     * @brief 检查 ffmpeg 子进程是否仍在运行。
     * @return 子进程仍在运行返回 `true`，已经退出或异常返回 `false`。
     */
    bool check_child_alive();

    /**
     * @brief 向 ffmpeg 标准输入完整写入一段字节。
     * @param data 待写入字节起始地址。
     * @param size 待写入字节数。
     * @return 成功写完返回 `true`。
     */
    bool write_all(const std::uint8_t* data, std::size_t size);

    OutputConfig output_;
    OverlayConfig overlay_;
    std::unique_ptr<ImageBackend> image_backend_;
    std::string ffmpeg_executable_;
    std::string last_error_;
    pid_t child_pid_{-1};
    int stdin_fd_{-1};
    int stream_width_{0};
    int stream_height_{0};
    bool is_open_{false};
};

} // namespace sentinel
