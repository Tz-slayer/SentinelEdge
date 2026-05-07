#include "sentinel/video/camera_video_source.hpp"

#include <cerrno>
#include <cstring>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <linux/videodev2.h>

namespace sentinel {
namespace {

/**
 * @brief 默认申请的 V4L2 流式缓冲区数量。
 */
constexpr int kBufferCount = 4;

/**
 * @brief 等待摄像头帧到达时使用的 `poll` 超时时间，单位毫秒。
 */
constexpr int kPollTimeoutMs = 2000;

/**
 * @brief 将 `timeval` 转换为纳秒时间戳。
 * @param value 驱动返回的时间戳。
 * @return 以纳秒表示的时间戳。
 */
std::int64_t to_timestamp_ns(const timeval& value)
{
    return static_cast<std::int64_t>(value.tv_sec) * 1000000000LL +
           static_cast<std::int64_t>(value.tv_usec) * 1000LL;
}

} // namespace

/**
 * @brief 使用摄像头配置构造 V4L2 视频源。
 * @param config 摄像头配置。
 */
CameraVideoSource::CameraVideoSource(CameraConfig config)
    : config_(std::move(config))
{
}

/**
 * @brief 析构时释放设备和缓冲区资源。
 */
CameraVideoSource::~CameraVideoSource()
{
    close();
}

/**
 * @brief 打开 V4L2 设备并完成流式采集初始化。
 * @return 成功返回 `true`，失败返回 `false`。
 */
bool CameraVideoSource::open()
{
    close();

    // 设备使用非阻塞方式打开，后续由 poll 控制等待时机。
    fd_ = ::open(config_.uri.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd_ == -1) {
        set_error_from_errno("open(" + config_.uri + ") failed");
        return false;
    }

    // 任一初始化阶段失败时都统一走 close，确保 fd 和 mmap 不泄漏。
    if (!configure_device() || !request_and_map_buffers() || !queue_all_buffers() || !start_streaming()) {
        close();
        return false;
    }

    next_sequence_ = 1;
    last_error_.clear();
    return true;
}

/**
 * @brief 停止采集并释放 V4L2 资源。
 */
void CameraVideoSource::close() noexcept
{
    if (fd_ != -1 && streaming_) {
        // 先停止驱动侧流式采集，再回收用户态映射缓冲区。
        auto type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ::ioctl(fd_, VIDIOC_STREAMOFF, &type);
        streaming_ = false;
    }

    release_buffers();

    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
}

/**
 * @brief 从 V4L2 队列中读取一帧。
 * @return 成功时返回一帧，失败或无数据时返回 `std::nullopt`。
 */
std::optional<Frame> CameraVideoSource::read_frame()
{
    if (fd_ == -1 || !streaming_) {
        return std::nullopt;
    }

    if (!wait_for_frame()) {
        return std::nullopt;
    }

    v4l2_buffer buffer {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;

    // 先从驱动队列中取出一块已经写满数据的缓冲区。
    if (xioctl(fd_, VIDIOC_DQBUF, &buffer) == -1) {
        if (errno == EAGAIN) {
            return std::nullopt;
        }
        set_error_from_errno("VIDIOC_DQBUF failed");
        return std::nullopt;
    }

    if (buffer.index >= buffers_.size()) {
        last_error_ = "VIDIOC_DQBUF returned an out-of-range buffer index";
        return std::nullopt;
    }

    const auto bytes_used = std::min<std::size_t>(buffer.bytesused, buffers_[buffer.index].length);
    auto* start = static_cast<std::uint8_t*>(buffers_[buffer.index].start);

    Frame frame;
    frame.sequence = next_sequence_++;
    frame.camera_id = config_.id;
    frame.width = config_.width;
    frame.height = config_.height;
    frame.pixel_format = pixel_format_;
    frame.timestamp_ns = to_timestamp_ns(buffer.timestamp);
    frame.bytes_used = bytes_used;

    // 当前实现先把驱动缓冲区复制到 Frame 对象中，后续再考虑零拷贝优化。
    frame.data.assign(start, start + bytes_used);

    // 使用完成后必须立即把缓冲区放回驱动队列，供后续帧继续复用。
    if (xioctl(fd_, VIDIOC_QBUF, &buffer) == -1) {
        set_error_from_errno("VIDIOC_QBUF failed");
        return std::nullopt;
    }

    return frame;
}

/**
 * @brief 返回视频源类型标识。
 * @return 固定返回 `"v4l2"`。
 */
std::string_view CameraVideoSource::kind() const noexcept
{
    return "v4l2";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本；若无错误则为空。
 */
std::string_view CameraVideoSource::last_error() const noexcept
{
    return last_error_;
}

/**
 * @brief 配置设备能力、图像格式和帧率参数。
 * @return 成功返回 `true`，失败返回 `false`。
 */
bool CameraVideoSource::configure_device()
{
    v4l2_capability capability {};
    if (xioctl(fd_, VIDIOC_QUERYCAP, &capability) == -1) {
        set_error_from_errno("VIDIOC_QUERYCAP failed");
        return false;
    }

    if ((capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0U) {
        last_error_ = "device does not support V4L2 video capture";
        return false;
    }
    if ((capability.capabilities & V4L2_CAP_STREAMING) == 0U) {
        last_error_ = "device does not support V4L2 streaming I/O";
        return false;
    }

    v4l2_format format {};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = static_cast<std::uint32_t>(config_.width);
    format.fmt.pix.height = static_cast<std::uint32_t>(config_.height);
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.field = V4L2_FIELD_ANY;

    // 第一版优先使用 MJPEG，减轻 USB 带宽和 CPU 压力。
    if (xioctl(fd_, VIDIOC_S_FMT, &format) == -1) {
        set_error_from_errno("VIDIOC_S_FMT failed");
        return false;
    }

    config_.width = static_cast<int>(format.fmt.pix.width);
    config_.height = static_cast<int>(format.fmt.pix.height);
    pixel_format_ = format.fmt.pix.pixelformat;

    v4l2_streamparm stream_parameters {};
    stream_parameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    stream_parameters.parm.capture.timeperframe.numerator = 1;
    stream_parameters.parm.capture.timeperframe.denominator =
        static_cast<std::uint32_t>(std::max(config_.fps, 1));

    if (xioctl(fd_, VIDIOC_S_PARM, &stream_parameters) == -1 && errno != EINVAL) {
        set_error_from_errno("VIDIOC_S_PARM failed");
        return false;
    }

    return true;
}

/**
 * @brief 请求并映射驱动缓冲区。
 * @return 成功返回 `true`，失败返回 `false`。
 */
bool CameraVideoSource::request_and_map_buffers()
{
    v4l2_requestbuffers request {};
    request.count = kBufferCount;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd_, VIDIOC_REQBUFS, &request) == -1) {
        set_error_from_errno("VIDIOC_REQBUFS failed");
        return false;
    }

    if (request.count < 2U) {
        last_error_ = "V4L2 device returned too few buffers for streaming";
        return false;
    }

    buffers_.clear();
    buffers_.reserve(request.count);

    for (std::uint32_t index = 0; index < request.count; ++index) {
        v4l2_buffer buffer {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = index;

        if (xioctl(fd_, VIDIOC_QUERYBUF, &buffer) == -1) {
            set_error_from_errno("VIDIOC_QUERYBUF failed");
            return false;
        }

        // 驱动返回每块缓冲区的偏移和长度，用户态通过 mmap 建立访问映射。
        void* start = ::mmap(nullptr,
                             buffer.length,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             fd_,
                             static_cast<off_t>(buffer.m.offset));
        if (start == MAP_FAILED) {
            set_error_from_errno("mmap failed");
            return false;
        }

        buffers_.push_back(BufferView{start, buffer.length});
    }

    return true;
}

/**
 * @brief 将全部缓冲区压回驱动输入队列。
 * @return 成功返回 `true`，失败返回 `false`。
 */
bool CameraVideoSource::queue_all_buffers()
{
    for (std::uint32_t index = 0; index < buffers_.size(); ++index) {
        v4l2_buffer buffer {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = index;

        if (xioctl(fd_, VIDIOC_QBUF, &buffer) == -1) {
            set_error_from_errno("VIDIOC_QBUF failed during startup");
            return false;
        }
    }

    return true;
}

/**
 * @brief 启动 V4L2 流式采集。
 * @return 成功返回 `true`，失败返回 `false`。
 */
bool CameraVideoSource::start_streaming()
{
    auto type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd_, VIDIOC_STREAMON, &type) == -1) {
        set_error_from_errno("VIDIOC_STREAMON failed");
        return false;
    }

    streaming_ = true;
    return true;
}

/**
 * @brief 解除所有用户态缓冲区映射。
 */
void CameraVideoSource::release_buffers() noexcept
{
    for (const auto& buffer : buffers_) {
        if (buffer.start != nullptr && buffer.length > 0U) {
            ::munmap(buffer.start, buffer.length);
        }
    }
    buffers_.clear();
}

/**
 * @brief 等待摄像头变为可读。
 * @return 若当前已有帧可读取则返回 `true`。
 */
bool CameraVideoSource::wait_for_frame()
{
    pollfd descriptor {};
    descriptor.fd = fd_;
    descriptor.events = POLLIN;

    // 采集循环不做忙等，而是通过 poll 等待驱动通知有帧可取。
    const auto ready = ::poll(&descriptor, 1, kPollTimeoutMs);
    if (ready == -1) {
        set_error_from_errno("poll failed");
        return false;
    }
    if (ready == 0) {
        last_error_ = "poll timed out while waiting for a camera frame";
        return false;
    }
    if ((descriptor.revents & POLLIN) == 0) {
        last_error_ = "camera poll returned without POLLIN";
        return false;
    }

    return true;
}

/**
 * @brief 记录带有 errno 语义的错误文本。
 * @param prefix 错误上下文前缀。
 */
void CameraVideoSource::set_error_from_errno(const std::string& prefix)
{
    last_error_ = prefix + ": " + std::strerror(errno);
}

/**
 * @brief 对 `ioctl` 进行 `EINTR` 重试封装。
 * @param fd 打开的设备文件描述符。
 * @param request ioctl 请求号。
 * @param arg 请求参数地址。
 * @return 原始 `ioctl` 返回值。
 */
int CameraVideoSource::xioctl(int fd, unsigned long request, void* arg)
{
    int result = 0;
    do {
        result = ::ioctl(fd, request, arg);
    } while (result == -1 && errno == EINTR);

    return result;
}

} // namespace sentinel
