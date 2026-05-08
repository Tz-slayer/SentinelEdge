#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/video/video_source.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sentinel {

/**
 * @brief 从本地 V4L2 摄像头设备采集视频帧。
 *
 * 该实现使用 Linux V4L2 流式采集接口，核心流程包括 `mmap`、
 * `poll`、`VIDIOC_DQBUF` 和 `VIDIOC_QBUF`。当前稳定实现支持
 * `buffer_mode: "copy"`；`loaned` 零拷贝模式需要后续 FrameView
 * 生命周期改造完成后再启用。
 */
class CameraVideoSource final : public VideoSource {
public:
    /**
     * @brief 使用摄像头配置构造 V4L2 视频源。
     * @param config 摄像头配置，其中 `uri` 应指向 `/dev/video*` 设备节点。
     */
    explicit CameraVideoSource(CameraConfig config);

    /**
     * @brief 停止流式采集并释放所有映射缓冲区。
     */
    ~CameraVideoSource() override;

    CameraVideoSource(const CameraVideoSource&) = delete;
    CameraVideoSource& operator=(const CameraVideoSource&) = delete;

    /**
     * @brief 打开并配置 V4L2 设备，使其进入流式采集状态。
     * @return 成功返回 `true`，失败返回 `false`，并更新 `last_error()`。
     */
    bool open() override;

    /**
     * @brief 停止采集并释放文件描述符与映射缓冲区。
     */
    void close() noexcept override;

    /**
     * @brief 使用 `poll` 和 V4L2 队列读取一帧数据。
     * @return 成功时返回一帧，若当前无帧或发生错误则返回 `std::nullopt`。
     */
    std::optional<Frame> read_frame() override;

    /**
     * @brief 返回视频源类型字符串。
     * @return 固定返回 `"v4l2"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误的可读文本。
     * @return 若当前没有错误则返回空字符串。
     */
    std::string_view last_error() const noexcept override;

private:
    /**
     * @brief 一块通过 `mmap` 映射到用户空间的 V4L2 缓冲区。
     */
    struct BufferView {
        void* start{nullptr};
        std::size_t length{0};
    };

    /**
     * @brief 配置设备的视频格式和采样参数。
     * @return 成功返回 `true`，失败返回 `false`。
     */
    bool configure_device();

    /**
     * @brief 请求 V4L2 流式缓冲区，并映射到用户空间。
     * @return 成功返回 `true`，失败返回 `false`。
     */
    bool request_and_map_buffers();

    /**
     * @brief 在开启采集前，将所有映射缓冲区重新压回驱动队列。
     * @return 成功返回 `true`，失败返回 `false`。
     */
    bool queue_all_buffers();

    /**
     * @brief 调用 `VIDIOC_STREAMON` 启动流式采集。
     * @return 成功返回 `true`，失败返回 `false`。
     */
    bool start_streaming();

    /**
     * @brief 解除所有已映射的 V4L2 缓冲区。
     */
    void release_buffers() noexcept;

    /**
     * @brief 等待设备进入可读状态。
     * @return 若已有帧可出队则返回 `true`，否则返回 `false`。
     */
    bool wait_for_frame();

    /**
     * @brief 将当前 `errno` 包装为带上下文的错误信息。
     * @param prefix 失败 Linux 调用的上下文前缀。
     */
    void set_error_from_errno(const std::string& prefix);

    /**
     * @brief 封装 `ioctl` 并在 `EINTR` 时自动重试。
     * @param fd 已打开的设备文件描述符。
     * @param request V4L2 ioctl 请求号。
     * @param arg 请求载荷地址。
     * @return 原始 `ioctl` 返回值。
     */
    static int xioctl(int fd, unsigned long request, void* arg);

    CameraConfig config_;
    std::vector<BufferView> buffers_;
    std::string last_error_;
    int fd_{-1};
    int next_sequence_{1};
    std::uint32_t pixel_format_{0};
    bool streaming_{false};
};

} // namespace sentinel
