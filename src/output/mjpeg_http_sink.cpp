#include "sentinel/output/mjpeg_http_sink.hpp"

#include "sentinel/image/image_backend_factory.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace sentinel {
namespace {

constexpr const char* kBoundary = "sentinel-mjpeg-boundary";

/**
 * @brief 把 `errno` 转换为带上下文的错误文本。
 * @param action 正在执行的系统调用或动作。
 * @return 错误描述。
 */
std::string errno_message(const std::string& action)
{
    return action + " failed: " + std::strerror(errno);
}

/**
 * @brief 安静关闭文件描述符。
 * @param fd 待关闭文件描述符。
 */
void close_fd_noexcept(int fd) noexcept
{
    if (fd >= 0) {
        static_cast<void>(::close(fd));
    }
}

/**
 * @brief 将文件描述符设置为非阻塞模式。
 * @param fd 文件描述符。
 * @param error 失败时写入错误文本。
 * @return 成功返回 `true`。
 */
bool set_nonblocking(int fd, std::string& error)
{
    const auto flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        error = errno_message("fcntl(F_GETFL)");
        return false;
    }
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        error = errno_message("fcntl(F_SETFL)");
        return false;
    }
    return true;
}

/**
 * @brief 将主机字节序 IPv4 地址转换为监听地址。
 * @param host 配置中的监听地址。
 * @return 网络字节序 IPv4 地址；当前仅支持 `0.0.0.0` 和 `127.0.0.1`。
 */
std::uint32_t parse_bind_address(const std::string& host)
{
    if (host == "127.0.0.1" || host == "localhost") {
        return htonl(INADDR_LOOPBACK);
    }
    return htonl(INADDR_ANY);
}

/**
 * @brief 向 socket 写入一段字节。
 * @param fd 客户端 socket。
 * @param data 待写入数据。
 * @param size 待写入字节数。
 * @return 写入成功返回 `true`；客户端阻塞或断开返回 `false`。
 */
bool send_all_or_drop(int fd, const std::uint8_t* data, std::size_t size)
{
    std::size_t offset = 0;
    while (offset < size) {
        const auto sent = ::send(fd, data + offset, size - offset, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (sent == 0) {
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

/**
 * @brief 向 socket 写入字符串。
 * @param fd 客户端 socket。
 * @param text 待写入文本。
 * @return 写入成功返回 `true`。
 */
bool send_text_or_drop(int fd, const std::string& text)
{
    return send_all_or_drop(fd,
                            reinterpret_cast<const std::uint8_t*>(text.data()),
                            text.size());
}

/**
 * @brief 将 BGR24 图像编码为 JPEG。
 * @param image 输入图像。
 * @param quality JPEG 质量，范围 1 到 100。
 * @param error 失败时写入错误文本。
 * @return 成功返回 JPEG 字节；失败返回空。
 */
std::vector<std::uint8_t> encode_bgr24_jpeg(const ImageBuffer& image,
                                            int quality,
                                            std::string& error)
{
    if (image.pixel_format != "BGR24" || image.memory_type != "host") {
        error = "MJPEG sink requires host BGR24 image buffers";
        return {};
    }
    if (image.width <= 0 || image.height <= 0) {
        error = "MJPEG sink received invalid image size";
        return {};
    }

    const auto required = static_cast<std::size_t>(image.width) *
                          static_cast<std::size_t>(image.height) * 3U;
    if (image.data.size() < required) {
        error = "MJPEG sink received incomplete BGR24 image buffer";
        return {};
    }

    const cv::Mat bgr(image.height,
                      image.width,
                      CV_8UC3,
                      const_cast<std::uint8_t*>(image.data.data()));
    std::vector<std::uint8_t> encoded;
    const std::vector<int> params {cv::IMWRITE_JPEG_QUALITY, std::clamp(quality, 1, 100)};
    if (!cv::imencode(".jpg", bgr, encoded, params)) {
        error = "cv::imencode failed for MJPEG frame";
        return {};
    }
    return encoded;
}

} // namespace

/**
 * @brief 构造 MJPEG HTTP 输出通道。
 * @param output 输出通道配置。
 * @param overlay 画框叠加配置。
 */
MjpegHttpSink::MjpegHttpSink(OutputConfig output, OverlayConfig overlay)
    : output_(std::move(output))
    , overlay_(std::move(overlay))
{
}

/**
 * @brief 创建监听 socket 并初始化图像后端。
 * @return 成功返回 `true`。
 */
bool MjpegHttpSink::open()
{
    if (output_.mjpeg_port <= 0 || output_.mjpeg_port > 65535) {
        last_error_ = "output.mjpeg_port must be in 1..65535";
        return false;
    }
    if (output_.mjpeg_quality <= 0 || output_.mjpeg_quality > 100) {
        last_error_ = "output.mjpeg_quality must be in 1..100";
        return false;
    }
    if (output_.mjpeg_max_clients <= 0) {
        last_error_ = "output.mjpeg_max_clients must be positive";
        return false;
    }

    struct sigaction action {};
    action.sa_handler = SIG_IGN;
    if (::sigemptyset(&action.sa_mask) != 0 || ::sigaction(SIGPIPE, &action, nullptr) != 0) {
        last_error_ = errno_message("sigaction(SIGPIPE)");
        return false;
    }

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listen_fd_ < 0) {
        last_error_ = errno_message("socket");
        return false;
    }

    int reuse = 1;
    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        last_error_ = errno_message("setsockopt(SO_REUSEADDR)");
        close();
        return false;
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = parse_bind_address(output_.mjpeg_host);
    address.sin_port = htons(static_cast<std::uint16_t>(output_.mjpeg_port));
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        last_error_ = errno_message("bind");
        close();
        return false;
    }
    if (::listen(listen_fd_, output_.mjpeg_max_clients) != 0) {
        last_error_ = errno_message("listen");
        close();
        return false;
    }
    if (!set_nonblocking(listen_fd_, last_error_)) {
        close();
        return false;
    }

    image_backend_ = create_image_backend(overlay_.backend);
    if (!image_backend_->open()) {
        last_error_ = "unable to open " + std::string(image_backend_->kind()) +
                      " image backend: " + std::string(image_backend_->last_error());
        close();
        return false;
    }

    is_open_ = true;
    last_error_.clear();
    return true;
}

/**
 * @brief 关闭监听 socket、客户端连接和图像后端。
 */
void MjpegHttpSink::close() noexcept
{
    for (const auto fd : clients_) {
        close_fd_noexcept(fd);
    }
    clients_.clear();
    close_fd_noexcept(listen_fd_);
    listen_fd_ = -1;
    if (image_backend_) {
        image_backend_->close();
        image_backend_.reset();
    }
    is_open_ = false;
}

/**
 * @brief 将当前帧编码为 JPEG 并广播给已连接的浏览器。
 * @param frame 视频源输出的原始帧。
 * @param detections 当前帧检测结果。
 * @return 成功返回 `true`。
 */
bool MjpegHttpSink::write(const Frame& frame, const std::vector<Detection>& detections)
{
    if (!is_open_ || !image_backend_) {
        last_error_ = "MJPEG HTTP sink is not open";
        return false;
    }

    accept_pending_clients();

    auto decoded = image_backend_->decode(frame);
    if (!decoded.has_value()) {
        last_error_ = image_backend_->last_error();
        return false;
    }

    ImageBuffer image_to_send = std::move(*decoded);
    if (overlay_.enabled) {
        auto rendered = image_backend_->draw_detections(image_to_send, detections);
        if (!rendered.has_value()) {
            last_error_ = image_backend_->last_error();
            return false;
        }
        image_to_send = std::move(*rendered);
    }

    auto jpeg = encode_bgr24_jpeg(image_to_send, output_.mjpeg_quality, last_error_);
    if (jpeg.empty()) {
        return false;
    }

    broadcast_jpeg(jpeg);
    last_error_.clear();
    return true;
}

/**
 * @brief 返回输出通道类型标识。
 * @return 固定返回 `"mjpeg"`。
 */
std::string_view MjpegHttpSink::kind() const noexcept
{
    return "mjpeg";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view MjpegHttpSink::last_error() const noexcept
{
    return last_error_;
}

/**
 * @brief 接收当前所有待处理客户端连接。
 */
void MjpegHttpSink::accept_pending_clients()
{
    while (true) {
        const auto client_fd = ::accept4(listen_fd_, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                return;
            }
            last_error_ = errno_message("accept4");
            return;
        }

        if (clients_.size() >= static_cast<std::size_t>(output_.mjpeg_max_clients)) {
            close_fd_noexcept(client_fd);
            continue;
        }

        const std::string header =
            "HTTP/1.1 200 OK\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Pragma: no-cache\r\n"
            "Connection: close\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=" +
            std::string(kBoundary) + "\r\n\r\n";
        if (!send_text_or_drop(client_fd, header)) {
            close_fd_noexcept(client_fd);
            continue;
        }
        clients_.push_back(client_fd);
    }
}

/**
 * @brief 将一张 JPEG 帧发送给所有客户端。
 * @param jpeg 已编码 JPEG 字节。
 */
void MjpegHttpSink::broadcast_jpeg(const std::vector<std::uint8_t>& jpeg)
{
    std::size_t index = 0;
    while (index < clients_.size()) {
        const std::string part_header =
            "--" + std::string(kBoundary) + "\r\n"
            "Content-Type: image/jpeg\r\n"
            "Content-Length: " + std::to_string(jpeg.size()) + "\r\n\r\n";

        const auto fd = clients_[index];
        const auto ok = send_text_or_drop(fd, part_header) &&
                        send_all_or_drop(fd, jpeg.data(), jpeg.size()) &&
                        send_text_or_drop(fd, "\r\n");
        if (!ok) {
            remove_client(index);
            continue;
        }
        ++index;
    }
}

/**
 * @brief 从客户端列表中移除指定下标。
 * @param index 客户端下标。
 */
void MjpegHttpSink::remove_client(std::size_t index) noexcept
{
    if (index >= clients_.size()) {
        return;
    }
    close_fd_noexcept(clients_[index]);
    clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(index));
}

} // namespace sentinel
