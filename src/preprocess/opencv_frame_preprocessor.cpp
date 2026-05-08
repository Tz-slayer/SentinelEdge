#include "sentinel/preprocess/opencv_frame_preprocessor.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <linux/videodev2.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace sentinel {
namespace {

/**
 * @brief 判断帧数据是否具有 JPEG 文件头。
 * @param data 原始帧字节。
 * @return 若前两个字节为 JPEG SOI 标记则返回 `true`。
 */
bool looks_like_jpeg(const std::vector<std::uint8_t>& data) noexcept
{
    return data.size() >= 2U && data[0] == 0xFFU && data[1] == 0xD8U;
}

/**
 * @brief 生成可读的 V4L2 fourcc 字符串。
 * @param pixel_format V4L2 像素格式整数。
 * @return fourcc 文本。
 */
std::string fourcc_to_string(std::uint32_t pixel_format)
{
    std::string value(4, ' ');
    value[0] = static_cast<char>(pixel_format & 0xFFU);
    value[1] = static_cast<char>((pixel_format >> 8U) & 0xFFU);
    value[2] = static_cast<char>((pixel_format >> 16U) & 0xFFU);
    value[3] = static_cast<char>((pixel_format >> 24U) & 0xFFU);
    return value;
}

/**
 * @brief 使用 OpenCV 解码压缩图像。
 * @param frame 视频源输出的原始帧。
 * @return BGR 排列的 `cv::Mat`；失败时为空矩阵。
 */
cv::Mat decode_compressed_image(const Frame& frame)
{
    const cv::Mat encoded(1,
                          static_cast<int>(frame.data.size()),
                          CV_8UC1,
                          const_cast<std::uint8_t*>(frame.data.data()));
    return cv::imdecode(encoded, cv::IMREAD_COLOR);
}

/**
 * @brief 将 V4L2 帧转换为 OpenCV BGR 图像。
 * @param frame 视频源输出的原始帧。
 * @param error 失败时写入错误文本。
 * @return BGR 排列的 `cv::Mat`；失败时为空矩阵。
 */
cv::Mat decode_frame_to_bgr(const Frame& frame, std::string& error)
{
    if (frame.data.empty()) {
        error = "frame data is empty";
        return {};
    }

    if (frame.pixel_format == V4L2_PIX_FMT_MJPEG || frame.pixel_format == V4L2_PIX_FMT_JPEG ||
        looks_like_jpeg(frame.data)) {
        auto bgr = decode_compressed_image(frame);
        if (bgr.empty()) {
            error = "cv::imdecode failed for compressed frame";
        }
        return bgr;
    }

    if (frame.width <= 0 || frame.height <= 0) {
        error = "raw frame requires positive width and height";
        return {};
    }

    if (frame.pixel_format == V4L2_PIX_FMT_YUYV) {
        const auto required = static_cast<std::size_t>(frame.width) *
                              static_cast<std::size_t>(frame.height) * 2U;
        if (frame.data.size() < required) {
            error = "YUYV frame bytes are smaller than width*height*2";
            return {};
        }

        const cv::Mat yuyv(frame.height,
                           frame.width,
                           CV_8UC2,
                           const_cast<std::uint8_t*>(frame.data.data()));
        cv::Mat bgr;
        cv::cvtColor(yuyv, bgr, cv::COLOR_YUV2BGR_YUY2);
        return bgr;
    }

    if (frame.pixel_format == V4L2_PIX_FMT_RGB24 || frame.pixel_format == V4L2_PIX_FMT_BGR24) {
        const auto required = static_cast<std::size_t>(frame.width) *
                              static_cast<std::size_t>(frame.height) * 3U;
        if (frame.data.size() < required) {
            error = "RGB/BGR frame bytes are smaller than width*height*3";
            return {};
        }

        const cv::Mat raw(frame.height,
                          frame.width,
                          CV_8UC3,
                          const_cast<std::uint8_t*>(frame.data.data()));
        if (frame.pixel_format == V4L2_PIX_FMT_BGR24) {
            return raw.clone();
        }

        cv::Mat bgr;
        cv::cvtColor(raw, bgr, cv::COLOR_RGB2BGR);
        return bgr;
    }

    error = "unsupported pixel format for OpenCV preprocessor: " +
            fourcc_to_string(frame.pixel_format);
    return {};
}

/**
 * @brief 将 RGB FP32 图像打包为 NCHW 字节流。
 * @param rgb_float `CV_32FC3` 的 RGB 图像。
 * @return 按 NCHW 顺序存放的 FP32 字节。
 */
std::vector<std::uint8_t> pack_nchw_fp32(const cv::Mat& rgb_float)
{
    std::vector<cv::Mat> channels;
    cv::split(rgb_float, channels);

    const auto channel_values = static_cast<std::size_t>(rgb_float.rows) *
                                static_cast<std::size_t>(rgb_float.cols);
    std::vector<float> tensor_values(channel_values * channels.size());

    for (std::size_t channel = 0; channel < channels.size(); ++channel) {
        const auto& channel_mat = channels[channel];
        const auto* source = channel_mat.ptr<float>(0);
        auto* destination = tensor_values.data() + channel * channel_values;
        std::copy(source, source + channel_values, destination);
    }

    std::vector<std::uint8_t> bytes(tensor_values.size() * sizeof(float));
    std::memcpy(bytes.data(), tensor_values.data(), bytes.size());
    return bytes;
}

} // namespace

/**
 * @brief 使用预处理配置构造 OpenCV 策略。
 * @param config 图像预处理输出配置。
 */
OpenCvFramePreprocessor::OpenCvFramePreprocessor(PreprocessConfig config)
    : config_(std::move(config))
{
}

/**
 * @brief 校验配置并初始化 OpenCV 策略。
 * @return 成功返回 `true`，失败返回 `false`。
 */
bool OpenCvFramePreprocessor::open()
{
    if (config_.output_width <= 0 || config_.output_height <= 0) {
        last_error_ = "preprocess output size must be positive";
        return false;
    }
    if (config_.output_layout != "NCHW") {
        last_error_ = "OpenCV preprocessor currently supports only NCHW layout";
        return false;
    }
    if (config_.output_dtype != "FP32") {
        last_error_ = "OpenCV preprocessor currently supports only FP32 dtype";
        return false;
    }

    is_open_ = true;
    last_error_.clear();
    return true;
}

/**
 * @brief 释放 OpenCV 策略资源。
 */
void OpenCvFramePreprocessor::close() noexcept
{
    is_open_ = false;
}

/**
 * @brief 将视频帧转换为模型输入张量。
 * @param frame 视频源输出的原始帧。
 * @return 成功返回 NCHW FP32 张量；失败返回空。
 */
std::optional<TensorBuffer> OpenCvFramePreprocessor::process(const Frame& frame)
{
    if (!is_open_) {
        last_error_ = "OpenCV preprocessor is not open";
        return std::nullopt;
    }

    std::string decode_error;
    auto bgr = decode_frame_to_bgr(frame, decode_error);
    if (bgr.empty()) {
        last_error_ = decode_error;
        return std::nullopt;
    }

    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(config_.output_width, config_.output_height));

    // 以 RGB 约定喂给 YOLO 张量；如果模型导出时使用 BGR，需要在配置层明确扩展。
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

    cv::Mat rgb_float;
    const auto scale = config_.normalize ? 1.0 / 255.0 : 1.0;
    rgb.convertTo(rgb_float, CV_32FC3, scale);

    TensorBuffer tensor;
    tensor.data = pack_nchw_fp32(rgb_float);
    tensor.shape = {1, 3, config_.output_height, config_.output_width};
    tensor.layout = config_.output_layout;
    tensor.dtype = config_.output_dtype;
    tensor.frame_sequence = frame.sequence;
    tensor.camera_id = frame.camera_id;

    last_error_.clear();
    return tensor;
}

/**
 * @brief 返回预处理策略类型标识。
 * @return 固定返回 `"opencv"`。
 */
std::string_view OpenCvFramePreprocessor::kind() const noexcept
{
    return "opencv";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本；若无错误则为空。
 */
std::string_view OpenCvFramePreprocessor::last_error() const noexcept
{
    return last_error_;
}

} // namespace sentinel
