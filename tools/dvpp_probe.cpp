#include "sentinel/preprocess/dvpp_frame_preprocessor.hpp"

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <linux/videodev2.h>

namespace {

/**
 * @brief DVPP 探针命令行选项。
 */
struct ProbeOptions {
    int device_id{0};
    int output_width{640};
    int output_height{640};
    std::filesystem::path jpeg_path;
};

/**
 * @brief 打印命令行帮助。
 * @param program_name 当前程序名。
 */
void print_usage(std::string_view program_name)
{
    std::cout
        << "Usage: " << program_name << " [options]\n"
        << "  --device-id <number>  Ascend device id, default 0\n"
        << "  --width <number>      output tensor width, default 640\n"
        << "  --height <number>     output tensor height, default 640\n"
        << "  --jpeg <path>         optional MJPEG/JPEG frame to decode and preprocess\n"
        << "  --help                show this message\n";
}

/**
 * @brief 将字符串解析为非负整数。
 * @param value 待解析字符串。
 * @param field_name 字段名，用于错误提示。
 * @return 解析后的整数。
 */
int parse_non_negative_int(std::string_view value, std::string_view field_name)
{
    std::size_t parsed_length = 0;
    const auto parsed = std::stoi(std::string(value), &parsed_length);
    if (parsed_length != value.size()) {
        throw std::runtime_error("invalid integer for " + std::string(field_name) + ": " +
                                 std::string(value));
    }
    if (parsed < 0) {
        throw std::runtime_error(std::string(field_name) + " must not be negative");
    }
    return parsed;
}

/**
 * @brief 将字符串解析为正整数。
 * @param value 待解析字符串。
 * @param field_name 字段名，用于错误提示。
 * @return 解析后的整数。
 */
int parse_positive_int(std::string_view value, std::string_view field_name)
{
    const auto parsed = parse_non_negative_int(value, field_name);
    if (parsed <= 0) {
        throw std::runtime_error(std::string(field_name) + " must be greater than zero");
    }
    return parsed;
}

/**
 * @brief 解析命令行参数。
 * @param argc 参数数量。
 * @param argv 参数数组。
 * @return 成功返回配置；用户请求帮助时返回空。
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
        if (argument == "--device-id") {
            options.device_id = parse_non_negative_int(value, "device-id");
        } else if (argument == "--width") {
            options.output_width = parse_positive_int(value, "width");
        } else if (argument == "--height") {
            options.output_height = parse_positive_int(value, "height");
        } else if (argument == "--jpeg") {
            options.jpeg_path = value;
        } else {
            throw std::runtime_error("unknown argument: " + std::string(argument));
        }
    }
    return options;
}

/**
 * @brief 读取 JPEG 文件为字节数组。
 * @param path JPEG 文件路径。
 * @return 文件内容。
 */
std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot open jpeg file: " + path.string());
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file),
                                     std::istreambuf_iterator<char>());
}

/**
 * @brief 构造用于 DVPP 预处理的帧对象。
 * @param bytes JPEG 压缩数据。
 * @return MJPEG 帧对象。
 */
sentinel::Frame make_jpeg_frame(std::vector<std::uint8_t> bytes)
{
    sentinel::Frame frame;
    frame.sequence = 1;
    frame.camera_id = "dvpp-probe";
    frame.pixel_format = V4L2_PIX_FMT_MJPEG;
    frame.bytes_used = bytes.size();
    frame.data = std::move(bytes);
    return frame;
}

} // namespace

/**
 * @brief DVPP 预处理探针入口。
 * @param argc 参数数量。
 * @param argv 参数数组。
 * @return 成功返回 0，失败返回非零值。
 */
int main(int argc, char** argv)
{
    try {
        const auto options = parse_arguments(argc, argv);
        if (!options.has_value()) {
            return 0;
        }

        sentinel::PreprocessConfig config;
        config.backend = "dvpp";
        config.device_id = options->device_id;
        config.output_width = options->output_width;
        config.output_height = options->output_height;
        config.output_layout = "NCHW";
        config.output_dtype = "FP32";
        config.normalize = true;

        sentinel::DvppFramePreprocessor preprocessor(config);
        if (!preprocessor.open()) {
            std::cerr << "DVPP open failed: " << preprocessor.last_error() << '\n';
            return 1;
        }
        std::cout << "DVPP open ok: device_id=" << options->device_id << '\n';

        if (!options->jpeg_path.empty()) {
            auto tensor = preprocessor.process(make_jpeg_frame(read_file_bytes(options->jpeg_path)));
            if (!tensor.has_value()) {
                std::cerr << "DVPP preprocess failed: " << preprocessor.last_error() << '\n';
                return 1;
            }
            std::cout << "DVPP preprocess ok: shape=[";
            for (std::size_t index = 0; index < tensor->shape.size(); ++index) {
                if (index > 0U) {
                    std::cout << ',';
                }
                std::cout << tensor->shape[index];
            }
            std::cout << "] bytes=" << tensor->data.size() << '\n';
        }

        preprocessor.close();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "sentinel_dvpp_probe failed: " << error.what() << '\n';
        return 1;
    }
}
