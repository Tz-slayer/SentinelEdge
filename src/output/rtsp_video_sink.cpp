#include "sentinel/output/rtsp_video_sink.hpp"

#include "sentinel/image/image_backend_factory.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace sentinel {
namespace {

/**
 * @brief 判断命令文本是否包含路径分隔符。
 * @param command 命令或路径文本。
 * @return 包含 `/` 返回 `true`。
 */
bool contains_path_separator(const std::string& command)
{
    return command.find('/') != std::string::npos;
}

/**
 * @brief 判断路径是否指向可执行文件。
 * @param path 待检查路径。
 * @return 当前进程可执行该文件返回 `true`。
 */
bool is_executable_file(const std::filesystem::path& path)
{
    return ::access(path.c_str(), X_OK) == 0;
}

/**
 * @brief 在 PATH 中查找可执行文件。
 * @param command 命令名或显式路径。
 * @return 找到时返回可执行路径，否则返回空。
 */
std::optional<std::string> find_executable(const std::string& command)
{
    if (contains_path_separator(command)) {
        if (is_executable_file(command)) {
            return command;
        }
        return std::nullopt;
    }

    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr) {
        return std::nullopt;
    }

    std::stringstream paths(path_env);
    std::string dir;
    while (std::getline(paths, dir, ':')) {
        if (dir.empty()) {
            continue;
        }

        const auto candidate = std::filesystem::path(dir) / command;
        if (is_executable_file(candidate)) {
            return candidate.string();
        }
    }

    return std::nullopt;
}

/**
 * @brief 把 `errno` 转为带上下文的错误文本。
 * @param action 正在执行的系统调用或动作。
 * @return 错误描述。
 */
std::string errno_message(const std::string& action)
{
    return action + " failed: " + std::strerror(errno);
}

/**
 * @brief 构造 ffmpeg 命令行参数。
 * @param ffmpeg ffmpeg 可执行文件路径。
 * @param output 输出配置。
 * @param width 输入帧宽度。
 * @param height 输入帧高度。
 * @return `execvp` 可用的参数字符串列表，第一个元素为程序路径。
 */
std::vector<std::string> make_ffmpeg_arguments(const std::string& ffmpeg,
                                               const OutputConfig& output,
                                               int width,
                                               int height)
{
    const auto video_size = std::to_string(width) + "x" + std::to_string(height);
    return {
        ffmpeg,
        "-hide_banner",
        "-loglevel",
        "warning",
        "-f",
        "rawvideo",
        "-pixel_format",
        "bgr24",
        "-video_size",
        video_size,
        "-framerate",
        std::to_string(output.rtsp_fps),
        "-i",
        "pipe:0",
        "-an",
        "-vf",
        "format=yuv420p",
        "-c:v",
        output.rtsp_encoder,
        "-preset",
        "ultrafast",
        "-tune",
        "zerolatency",
        "-f",
        "rtsp",
        "-rtsp_flags",
        "listen",
        output.rtsp_url,
    };
}

/**
 * @brief 将字符串参数转换为 `execvp` 需要的可变 C 字符串数组。
 * @param arguments 参数字符串列表。
 * @return 以 `nullptr` 结尾的参数指针数组。
 */
std::vector<char*> make_exec_argv(std::vector<std::string>& arguments)
{
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1U);
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);
    return argv;
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
 * @brief 等待文件描述符可写。
 * @param fd 文件描述符。
 * @param timeout_ms 超时时间，单位毫秒。
 * @param error 失败时写入错误文本。
 * @return 可写返回 `true`，超时或失败返回 `false`。
 */
bool wait_writable(int fd, int timeout_ms, std::string& error)
{
    pollfd descriptor {};
    descriptor.fd = fd;
    descriptor.events = POLLOUT;

    while (true) {
        const auto result = ::poll(&descriptor, 1, timeout_ms);
        if (result > 0) {
            if ((descriptor.revents & POLLOUT) != 0) {
                return true;
            }
            error = "ffmpeg stdin is not writable";
            return false;
        }
        if (result == 0) {
            error = "timed out waiting for ffmpeg stdin";
            return false;
        }
        if (errno == EINTR) {
            continue;
        }
        error = errno_message("poll ffmpeg stdin");
        return false;
    }
}

} // namespace

/**
 * @brief 构造 RTSP 输出通道。
 * @param output 输出通道配置。
 * @param overlay 画框叠加配置。
 */
RtspVideoSink::RtspVideoSink(OutputConfig output, OverlayConfig overlay)
    : output_(std::move(output))
    , overlay_(std::move(overlay))
{
}

/**
 * @brief 初始化图像后端并检查 ffmpeg 可执行文件。
 * @return 成功返回 `true`。
 */
bool RtspVideoSink::open()
{
    if (output_.rtsp_url.empty()) {
        last_error_ = "output.rtsp_url must not be empty";
        return false;
    }
    if (output_.rtsp_fps <= 0) {
        last_error_ = "output.rtsp_fps must be positive";
        return false;
    }

    const auto ffmpeg = find_executable(output_.ffmpeg_path);
    if (!ffmpeg.has_value()) {
        last_error_ = "unable to find executable ffmpeg_path=" + output_.ffmpeg_path;
        return false;
    }
    ffmpeg_executable_ = *ffmpeg;

    // 管道读端关闭时 write 会触发 SIGPIPE；这里忽略该信号，改由 write 返回 EPIPE。
    struct sigaction action {};
    action.sa_handler = SIG_IGN;
    if (::sigemptyset(&action.sa_mask) != 0 || ::sigaction(SIGPIPE, &action, nullptr) != 0) {
        last_error_ = errno_message("sigaction(SIGPIPE)");
        return false;
    }

    image_backend_ = create_image_backend(overlay_.backend);
    if (!image_backend_->open()) {
        last_error_ = "unable to open " + std::string(image_backend_->kind()) +
                      " image backend: " + std::string(image_backend_->last_error());
        return false;
    }

    is_open_ = true;
    last_error_.clear();
    return true;
}

/**
 * @brief 关闭 RTSP 输出通道并回收子进程。
 */
void RtspVideoSink::close() noexcept
{
    close_fd_noexcept(stdin_fd_);
    stdin_fd_ = -1;

    if (child_pid_ > 0) {
        int status = 0;
        const auto wait_result = ::waitpid(child_pid_, &status, WNOHANG);
        if (wait_result == 0) {
            static_cast<void>(::kill(child_pid_, SIGTERM));
            static_cast<void>(::waitpid(child_pid_, &status, 0));
        }
        child_pid_ = -1;
    }

    if (image_backend_) {
        image_backend_->close();
        image_backend_.reset();
    }

    stream_width_ = 0;
    stream_height_ = 0;
    is_open_ = false;
}

/**
 * @brief 写入一帧到 RTSP 输出流。
 * @param frame 视频源输出的原始帧。
 * @param detections 当前帧检测结果。
 * @return 成功返回 `true`。
 */
bool RtspVideoSink::write(const Frame& frame, const std::vector<Detection>& detections)
{
    if (!is_open_ || !image_backend_) {
        last_error_ = "rtsp video sink is not open";
        return false;
    }

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

    if (image_to_send.memory_type != "host" || image_to_send.pixel_format != "BGR24") {
        last_error_ = "rtsp video sink requires host BGR24 image buffers";
        return false;
    }

    const auto expected_size = static_cast<std::size_t>(image_to_send.width) *
                               static_cast<std::size_t>(image_to_send.height) * 3U;
    if (image_to_send.width <= 0 || image_to_send.height <= 0 ||
        image_to_send.data.size() < expected_size) {
        last_error_ = "rtsp video sink received invalid BGR24 image buffer";
        return false;
    }

    if (child_pid_ <= 0 && !start_ffmpeg(image_to_send.width, image_to_send.height)) {
        return false;
    }
    if (image_to_send.width != stream_width_ || image_to_send.height != stream_height_) {
        last_error_ = "rtsp video sink does not support runtime resolution changes";
        return false;
    }
    if (!check_child_alive()) {
        return false;
    }

    if (!write_all(image_to_send.data.data(), expected_size)) {
        return false;
    }

    last_error_.clear();
    return true;
}

/**
 * @brief 返回输出通道类型标识。
 * @return 固定返回 `"rtsp"`。
 */
std::string_view RtspVideoSink::kind() const noexcept
{
    return "rtsp";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view RtspVideoSink::last_error() const noexcept
{
    return last_error_;
}

/**
 * @brief 启动 ffmpeg 子进程并建立标准输入管道。
 * @param width 原始 BGR24 帧宽度。
 * @param height 原始 BGR24 帧高度。
 * @return 成功返回 `true`。
 */
bool RtspVideoSink::start_ffmpeg(int width, int height)
{
    int pipe_fds[2] {-1, -1};
    if (::pipe2(pipe_fds, O_CLOEXEC) != 0) {
        last_error_ = errno_message("pipe2");
        return false;
    }

    auto arguments = make_ffmpeg_arguments(ffmpeg_executable_, output_, width, height);
    const auto pid = ::fork();
    if (pid < 0) {
        last_error_ = errno_message("fork");
        close_fd_noexcept(pipe_fds[0]);
        close_fd_noexcept(pipe_fds[1]);
        return false;
    }

    if (pid == 0) {
        // 子进程只保留管道读端作为 stdin，然后用 execvp 切换为 ffmpeg。
        static_cast<void>(::dup2(pipe_fds[0], STDIN_FILENO));
        close_fd_noexcept(pipe_fds[0]);
        close_fd_noexcept(pipe_fds[1]);
        auto argv = make_exec_argv(arguments);
        ::execvp(argv[0], argv.data());
        _exit(127);
    }

    close_fd_noexcept(pipe_fds[0]);
    stdin_fd_ = pipe_fds[1];
    if (!set_nonblocking(stdin_fd_, last_error_)) {
        close_fd_noexcept(stdin_fd_);
        stdin_fd_ = -1;
        static_cast<void>(::kill(pid, SIGTERM));
        static_cast<void>(::waitpid(pid, nullptr, 0));
        return false;
    }

    child_pid_ = pid;
    stream_width_ = width;
    stream_height_ = height;
    last_error_.clear();
    return true;
}

/**
 * @brief 检查 ffmpeg 子进程是否仍在运行。
 * @return 子进程仍在运行返回 `true`。
 */
bool RtspVideoSink::check_child_alive()
{
    if (child_pid_ <= 0) {
        last_error_ = "ffmpeg process is not running";
        return false;
    }

    int status = 0;
    const auto wait_result = ::waitpid(child_pid_, &status, WNOHANG);
    if (wait_result == 0) {
        return true;
    }
    if (wait_result < 0) {
        last_error_ = errno_message("waitpid");
        return false;
    }

    child_pid_ = -1;
    close_fd_noexcept(stdin_fd_);
    stdin_fd_ = -1;
    if (WIFEXITED(status)) {
        last_error_ = "ffmpeg process exited with code " + std::to_string(WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        last_error_ = "ffmpeg process terminated by signal " + std::to_string(WTERMSIG(status));
    } else {
        last_error_ = "ffmpeg process stopped unexpectedly";
    }
    return false;
}

/**
 * @brief 向 ffmpeg 标准输入完整写入一段字节。
 * @param data 待写入字节起始地址。
 * @param size 待写入字节数。
 * @return 成功写完返回 `true`。
 */
bool RtspVideoSink::write_all(const std::uint8_t* data, std::size_t size)
{
    std::size_t offset = 0;
    while (offset < size) {
        if (!wait_writable(stdin_fd_, output_.rtsp_write_timeout_ms, last_error_)) {
            return false;
        }

        const auto written = ::write(stdin_fd_, data + offset, size - offset);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            last_error_ = errno_message("write ffmpeg stdin");
            return false;
        }
        if (written == 0) {
            last_error_ = "write ffmpeg stdin returned zero bytes";
            return false;
        }
        offset += static_cast<std::size_t>(written);
    }

    return true;
}

} // namespace sentinel
