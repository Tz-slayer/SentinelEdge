#include "sentinel/app/linux_signal_fd.hpp"
#include "sentinel/video/camera_video_source.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

/**
 * @brief 本地摄像头探针程序的命令行选项。
 */
struct ProbeOptions {
    std::string device{"/dev/video0"};
    int width{1280};
    int height{720};
    int fps{30};
    int frame_count{5};
    std::filesystem::path output_dir{"./data/captures"};
};

/**
 * @brief 向标准输出打印命令行帮助。
 * @param program_name 当前程序名。
 */
void print_usage(std::string_view program_name)
{
    std::cout
        << "Usage: " << program_name << " [options]\n"
        << "  --device <path>       V4L2 device path, default /dev/video0\n"
        << "  --width <number>      requested capture width, default 1280\n"
        << "  --height <number>     requested capture height, default 720\n"
        << "  --fps <number>        requested capture fps, default 30\n"
        << "  --frames <number>     frames to capture, default 5\n"
        << "  --output-dir <path>   directory for saving first frame, default ./data/captures\n"
        << "  --help                show this message\n";
}

/**
 * @brief 将字符串解析为正整数。
 * @param value 待解析的字符串。
 * @param field_name 当前命令行字段名称，用于生成错误消息。
 * @return 成功时返回解析出的整数，失败时抛出异常。
 */
int parse_positive_int(std::string_view value, std::string_view field_name)
{
    std::size_t parsed_length = 0;
    const auto parsed = std::stoi(std::string(value), &parsed_length);
    if (parsed_length != value.size()) {
        throw std::runtime_error("invalid integer for " + std::string(field_name) + ": " +
                                 std::string(value));
    }
    if (parsed <= 0) {
        throw std::runtime_error(std::string(field_name) + " must be greater than zero");
    }
    return parsed;
}

/**
 * @brief 从命令行参数中解析探针配置。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 成功时返回探针配置；若用户请求帮助，则返回空值。
 */
std::optional<ProbeOptions> parse_arguments(int argc, char** argv)
{
    ProbeOptions options;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--help") {
            print_usage(argv[0]);
            return std::nullopt;
        }

        if (index + 1 >= argc) {
            throw std::runtime_error("missing value for argument: " + std::string(argument));
        }

        const std::string_view value(argv[++index]);
        if (argument == "--device") {
            options.device = std::string(value);
        } else if (argument == "--width") {
            options.width = parse_positive_int(value, "width");
        } else if (argument == "--height") {
            options.height = parse_positive_int(value, "height");
        } else if (argument == "--fps") {
            options.fps = parse_positive_int(value, "fps");
        } else if (argument == "--frames") {
            options.frame_count = parse_positive_int(value, "frames");
        } else if (argument == "--output-dir") {
            options.output_dir = value;
        } else {
            throw std::runtime_error("unknown argument: " + std::string(argument));
        }
    }

    return options;
}

/**
 * @brief 将四字符编码转换为可读字符串。
 * @param pixel_format V4L2 像素格式四字符编码。
 * @return 例如 `"MJPG"`、`"YUYV"` 的字符串。
 */
std::string pixel_format_to_string(std::uint32_t pixel_format)
{
    std::string name(4, ' ');
    name[0] = static_cast<char>(pixel_format & 0xFFU);
    name[1] = static_cast<char>((pixel_format >> 8U) & 0xFFU);
    name[2] = static_cast<char>((pixel_format >> 16U) & 0xFFU);
    name[3] = static_cast<char>((pixel_format >> 24U) & 0xFFU);
    return name;
}

/**
 * @brief 判断当前帧是否可直接按 JPEG 文件保存。
 * @param pixel_format 当前帧的 V4L2 像素格式。
 * @return 若格式为 `MJPEG` 或 `JPEG` 则返回 `true`。
 */
bool is_jpeg_payload(std::uint32_t pixel_format) noexcept
{
    return pixel_format == V4L2_PIX_FMT_MJPEG || pixel_format == V4L2_PIX_FMT_JPEG;
}

/**
 * @brief 确保输出目录存在。
 * @param directory 待创建的输出目录。
 */
void ensure_directory_exists(const std::filesystem::path& directory)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        throw std::system_error(error, "failed to create output directory: " + directory.string());
    }
}

/**
 * @brief 将缓冲区内容完整写入文件描述符。
 * @param fd 已打开的目标文件描述符。
 * @param buffer 待写入数据的首地址。
 * @param length 待写入的字节数。
 *
 * 若 `write(2)` 被信号中断，则自动重试；其余错误会抛出异常。
 */
void write_all(int fd, const std::uint8_t* buffer, std::size_t length)
{
    std::size_t written = 0;
    while (written < length) {
        const auto bytes_written =
            ::write(fd, buffer + written, static_cast<size_t>(length - written));
        if (bytes_written > 0) {
            written += static_cast<std::size_t>(bytes_written);
            continue;
        }

        if (bytes_written == -1 && errno == EINTR) {
            continue;
        }

        throw std::system_error(errno, std::generic_category(), "write probe frame failed");
    }
}

/**
 * @brief 将捕获到的首帧写入文件，供人工打开验证。
 * @param frame 已采集的首帧。
 * @param output_dir 输出目录。
 * @return 实际写入的文件路径。
 *
 * 若帧格式是 `MJPEG/JPEG`，则直接保存为 `.jpg`；否则原样保存为 `.raw`。
 */
std::filesystem::path save_first_frame(const sentinel::Frame& frame,
                                       const std::filesystem::path& output_dir)
{
    ensure_directory_exists(output_dir);

    const auto extension = is_jpeg_payload(frame.pixel_format) ? ".jpg" : ".raw";
    const auto file_path = output_dir / ("frame-" + std::to_string(frame.sequence) + extension);

    const int fd = ::open(file_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0644);
    if (fd == -1) {
        throw std::system_error(errno,
                                std::generic_category(),
                                "open output file failed: " + file_path.string());
    }

    try {
        if (!frame.data.empty()) {
            // 把驱动返回的原始帧载荷原样落盘，便于后续用图片查看器或十六进制工具检查。
            write_all(fd, frame.data.data(), frame.data.size());
        }
    } catch (...) {
        ::close(fd);
        throw;
    }

    if (::close(fd) == -1) {
        throw std::system_error(errno,
                                std::generic_category(),
                                "close output file failed: " + file_path.string());
    }

    return file_path;
}

/**
 * @brief 根据探针选项构造摄像头配置。
 * @param options 命令行解析后的探针选项。
 * @return 用于初始化 `CameraVideoSource` 的摄像头配置。
 */
sentinel::CameraConfig make_camera_config(const ProbeOptions& options)
{
    sentinel::CameraConfig config;
    config.id = "camera-probe";
    config.name = "Camera Probe";
    config.type = "v4l2";
    config.uri = options.device;
    config.width = options.width;
    config.height = options.height;
    config.fps = options.fps;
    return config;
}

/**
 * @brief 打印单帧采集结果摘要。
 * @param frame 刚刚采集到的一帧。
 */
void print_frame_summary(const sentinel::Frame& frame)
{
    std::cout << "frame=" << frame.sequence << " size=" << frame.width << "x" << frame.height
              << " pixel_format=" << pixel_format_to_string(frame.pixel_format)
              << " bytes_used=" << frame.bytes_used << " timestamp_ns=" << frame.timestamp_ns
              << '\n';
}

/**
 * @brief 运行本地摄像头探针逻辑。
 * @param options 命令行解析后的探针选项。
 * @return 成功返回 `0`，失败返回非零值。
 */
int run_probe(const ProbeOptions& options)
{
    sentinel::LinuxSignalFd signal_fd;
    sentinel::CameraVideoSource source(make_camera_config(options));

    if (!source.open()) {
        std::cerr << "open camera failed: " << source.last_error() << '\n';
        return 1;
    }

    std::cout << "camera opened successfully: device=" << options.device << " requested="
              << options.width << "x" << options.height << "@" << options.fps << '\n';

    bool captured_any_frame = false;
    std::optional<std::filesystem::path> saved_file;

    for (int index = 0; index < options.frame_count; ++index) {
        // 探针程序也遵循项目统一的 signalfd 停止路径，便于在板端安全中止采集。
        if (signal_fd.consume_stop_signal()) {
            std::cout << "probe interrupted by stop signal\n";
            break;
        }

        const auto frame = source.read_frame();
        if (!frame.has_value()) {
            std::cerr << "read frame failed: " << source.last_error() << '\n';
            source.close();
            return 1;
        }

        captured_any_frame = true;
        print_frame_summary(*frame);

        if (!saved_file.has_value()) {
            saved_file = save_first_frame(*frame, options.output_dir);
            std::cout << "saved first frame to " << saved_file->string() << '\n';
        }
    }

    source.close();

    if (!captured_any_frame) {
        std::cerr << "no frame captured\n";
        return 1;
    }

    std::cout << "probe finished successfully\n";
    return 0;
}

} // namespace

/**
 * @brief 本地摄像头真机探针程序入口。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main(int argc, char** argv)
{
    try {
        const auto options = parse_arguments(argc, argv);
        if (!options.has_value()) {
            return 0;
        }

        return run_probe(*options);
    } catch (const std::exception& error) {
        std::cerr << "camera probe failed: " << error.what() << '\n';
        return 1;
    }
}
