#include "sentinel/output/debug_image_sink.hpp"

#include "sentinel/image/image_backend_factory.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace sentinel {
namespace {

/**
 * @brief 将 BGR24 图像缓冲区保存为 JPEG。
 * @param image 待保存图像。
 * @param path 输出路径。
 * @return 成功返回 `true`。
 */
bool write_bgr24_jpeg(const ImageBuffer& image, const std::filesystem::path& path)
{
    if (image.pixel_format != "BGR24" || image.memory_type != "host") {
        return false;
    }

    const auto required = static_cast<std::size_t>(image.width) *
                          static_cast<std::size_t>(image.height) * 3U;
    if (image.width <= 0 || image.height <= 0 || image.data.size() < required) {
        return false;
    }

    const cv::Mat bgr(image.height,
                      image.width,
                      CV_8UC3,
                      const_cast<std::uint8_t*>(image.data.data()));
    return cv::imwrite(path.string(), bgr);
}

} // namespace

/**
 * @brief 构造调试图输出通道。
 * @param output 输出通道配置。
 * @param overlay 画框叠加配置。
 * @param data_dir 运行数据根目录。
 */
DebugImageSink::DebugImageSink(OutputConfig output,
                               OverlayConfig overlay,
                               std::filesystem::path data_dir)
    : output_(std::move(output))
    , overlay_(std::move(overlay))
    , output_dir_(std::move(data_dir) / output_.debug_image_dir)
{
}

/**
 * @brief 初始化图像后端并创建输出目录。
 * @return 成功返回 `true`。
 */
bool DebugImageSink::open()
{
    if (output_.debug_image_interval <= 0) {
        last_error_ = "output.debug_image_interval must be positive";
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(output_dir_, error);
    if (error) {
        last_error_ = "failed to create debug image directory: " + error.message();
        return false;
    }

    image_backend_ = create_image_backend(overlay_.backend);
    if (!image_backend_->open()) {
        last_error_ = "unable to open " + std::string(image_backend_->kind()) +
                      " image backend: " + std::string(image_backend_->last_error());
        return false;
    }

    frames_seen_ = 0;
    is_open_ = true;
    last_error_.clear();
    return true;
}

/**
 * @brief 关闭图像后端。
 */
void DebugImageSink::close() noexcept
{
    if (image_backend_) {
        image_backend_->close();
        image_backend_.reset();
    }
    is_open_ = false;
}

/**
 * @brief 保存一帧调试 JPEG。
 * @param frame 视频源输出的原始帧。
 * @param detections 当前帧检测结果。
 * @return 成功返回 `true`。
 */
bool DebugImageSink::write(const Frame& frame, const std::vector<Detection>& detections)
{
    if (!is_open_ || !image_backend_) {
        last_error_ = "debug image sink is not open";
        return false;
    }

    ++frames_seen_;
    if ((frames_seen_ - 1) % output_.debug_image_interval != 0) {
        return true;
    }

    auto decoded = image_backend_->decode(frame);
    if (!decoded.has_value()) {
        last_error_ = image_backend_->last_error();
        return false;
    }

    ImageBuffer image_to_save = std::move(*decoded);
    if (overlay_.enabled) {
        auto rendered = image_backend_->draw_detections(image_to_save, detections);
        if (!rendered.has_value()) {
            last_error_ = image_backend_->last_error();
            return false;
        }
        image_to_save = std::move(*rendered);
    }

    const auto output_path = make_output_path(frame);
    if (!write_bgr24_jpeg(image_to_save, output_path)) {
        last_error_ = "failed to write debug image: " + output_path.string();
        return false;
    }

    last_error_.clear();
    return true;
}

/**
 * @brief 返回输出通道类型标识。
 * @return 固定返回 `"debug_image"`。
 */
std::string_view DebugImageSink::kind() const noexcept
{
    return "debug_image";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view DebugImageSink::last_error() const noexcept
{
    return last_error_;
}

/**
 * @brief 生成当前帧的输出文件路径。
 * @param frame 当前视频帧。
 * @return JPEG 文件路径。
 */
std::filesystem::path DebugImageSink::make_output_path(const Frame& frame) const
{
    const auto filename = "frame-" + frame.camera_id + "-" +
                          std::to_string(frame.sequence) + ".jpg";
    return output_dir_ / filename;
}

} // namespace sentinel
