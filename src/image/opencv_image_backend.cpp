#include "sentinel/image/opencv_image_backend.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <linux/videodev2.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace sentinel {
namespace {

/**
 * @brief 判断帧载荷是否具有 JPEG 文件头。
 * @param data 原始帧字节起始地址。
 * @param size 原始帧字节数。
 * @return 若前两个字节为 JPEG SOI 标记则返回 `true`。
 */
bool looks_like_jpeg(const std::uint8_t* data, std::size_t size) noexcept
{
    return data != nullptr && size >= 2U && data[0] == 0xFFU && data[1] == 0xD8U;
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
 * @brief 从 BGR `cv::Mat` 构造图像缓冲区。
 * @param bgr BGR 排列的 OpenCV 图像。
 * @return Host BGR24 图像缓冲区。
 */
ImageBuffer image_from_bgr_mat(const cv::Mat& bgr)
{
    const auto continuous = bgr.isContinuous() ? bgr : bgr.clone();
    ImageBuffer image;
    image.width = continuous.cols;
    image.height = continuous.rows;
    image.pixel_format = "BGR24";
    image.memory_type = "host";
    image.data.assign(continuous.datastart, continuous.dataend);
    return image;
}

/**
 * @brief 将图像缓冲区映射为 BGR `cv::Mat`。
 * @param image 图像缓冲区。
 * @param error 失败时写入错误文本。
 * @return BGR 排列的 `cv::Mat`；失败时为空矩阵。
 */
cv::Mat bgr_mat_from_image(const ImageBuffer& image, std::string& error)
{
    if (image.memory_type != "host") {
        error = "OpenCV image backend supports only host image buffers";
        return {};
    }
    if (image.pixel_format != "BGR24") {
        error = "OpenCV image backend expects BGR24 image buffers";
        return {};
    }
    if (image.width <= 0 || image.height <= 0) {
        error = "image width and height must be positive";
        return {};
    }

    const auto required = static_cast<std::size_t>(image.width) *
                          static_cast<std::size_t>(image.height) * 3U;
    if (image.data.size() < required) {
        error = "BGR24 image data is smaller than width*height*3";
        return {};
    }

    return cv::Mat(image.height,
                   image.width,
                   CV_8UC3,
                   const_cast<std::uint8_t*>(image.data.data()));
}

/**
 * @brief 使用 OpenCV 解码压缩图像。
 * @param frame 视频源输出的原始帧。
 * @return BGR 排列的 `cv::Mat`；失败时为空矩阵。
 */
cv::Mat decode_compressed_image(const Frame& frame)
{
    const cv::Mat encoded(1,
                          static_cast<int>(frame.payload_size()),
                          CV_8UC1,
                          const_cast<std::uint8_t*>(frame.payload_data()));
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
    const auto* payload = frame.payload_data();
    const auto payload_size = frame.payload_size();
    if (payload == nullptr || payload_size == 0U) {
        error = "frame data is empty";
        return {};
    }

    if (frame.pixel_format == V4L2_PIX_FMT_MJPEG || frame.pixel_format == V4L2_PIX_FMT_JPEG ||
        looks_like_jpeg(payload, payload_size)) {
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
        if (payload_size < required) {
            error = "YUYV frame bytes are smaller than width*height*2";
            return {};
        }

        const cv::Mat yuyv(frame.height,
                           frame.width,
                           CV_8UC2,
                           const_cast<std::uint8_t*>(payload));
        cv::Mat bgr;
        cv::cvtColor(yuyv, bgr, cv::COLOR_YUV2BGR_YUY2);
        return bgr;
    }

    if (frame.pixel_format == V4L2_PIX_FMT_RGB24 || frame.pixel_format == V4L2_PIX_FMT_BGR24) {
        const auto required = static_cast<std::size_t>(frame.width) *
                              static_cast<std::size_t>(frame.height) * 3U;
        if (payload_size < required) {
            error = "RGB/BGR frame bytes are smaller than width*height*3";
            return {};
        }

        const cv::Mat raw(frame.height,
                          frame.width,
                          CV_8UC3,
                          const_cast<std::uint8_t*>(payload));
        if (frame.pixel_format == V4L2_PIX_FMT_BGR24) {
            return raw.clone();
        }

        cv::Mat bgr;
        cv::cvtColor(raw, bgr, cv::COLOR_RGB2BGR);
        return bgr;
    }

    error = "unsupported pixel format for OpenCV image backend: " +
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

/**
 * @brief 将归一化检测框转换为像素矩形。
 * @param detection 检测结果。
 * @param width 图像宽度。
 * @param height 图像高度。
 * @return OpenCV 像素矩形。
 */
cv::Rect detection_to_rect(const Detection& detection, int width, int height)
{
    const auto left = static_cast<int>(detection.bounding_box.x * width);
    const auto top = static_cast<int>(detection.bounding_box.y * height);
    const auto box_width = static_cast<int>(detection.bounding_box.width * width);
    const auto box_height = static_cast<int>(detection.bounding_box.height * height);
    return cv::Rect{
        std::max(0, left),
        std::max(0, top),
        std::max(1, box_width),
        std::max(1, box_height),
    } & cv::Rect{0, 0, width, height};
}

/**
 * @brief 生成检测框标签文本。
 * @param detection 检测结果。
 * @return 类别和置信度文本。
 */
std::string detection_label(const Detection& detection)
{
    char confidence[16] {};
    std::snprintf(confidence, sizeof(confidence), "%.2f", detection.confidence);
    return detection.label + " " + confidence;
}

} // namespace

/**
 * @brief 初始化 OpenCV 图像处理后端。
 * @return 成功返回 `true`。
 */
bool OpenCvImageBackend::open()
{
    is_open_ = true;
    last_error_.clear();
    return true;
}

/**
 * @brief 释放 OpenCV 图像处理后端资源。
 */
void OpenCvImageBackend::close() noexcept
{
    is_open_ = false;
}

/**
 * @brief 将视频源帧解码为 BGR24 图像。
 * @param frame 视频源输出的原始帧。
 * @return 成功返回 BGR24 图像；失败返回空。
 */
std::optional<ImageBuffer> OpenCvImageBackend::decode(const Frame& frame)
{
    if (!is_open_) {
        last_error_ = "OpenCV image backend is not open";
        return std::nullopt;
    }

    std::string error;
    const auto bgr = decode_frame_to_bgr(frame, error);
    if (bgr.empty()) {
        last_error_ = error;
        return std::nullopt;
    }

    last_error_.clear();
    return image_from_bgr_mat(bgr);
}

/**
 * @brief 将图像缩放到指定尺寸。
 * @param image 输入图像。
 * @param width 输出宽度。
 * @param height 输出高度。
 * @return 成功返回缩放后的图像。
 */
std::optional<ImageBuffer> OpenCvImageBackend::resize(const ImageBuffer& image, int width, int height)
{
    std::string error;
    const auto bgr = bgr_mat_from_image(image, error);
    if (bgr.empty()) {
        last_error_ = error;
        return std::nullopt;
    }
    if (width <= 0 || height <= 0) {
        last_error_ = "resize output width and height must be positive";
        return std::nullopt;
    }

    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(width, height));

    last_error_.clear();
    return image_from_bgr_mat(resized);
}

/**
 * @brief 将 BGR24 图像转换为模型输入张量。
 * @param image 输入图像。
 * @param config 预处理输出配置。
 * @param frame_sequence 来源帧序号。
 * @param camera_id 来源摄像头 ID。
 * @return 成功返回 NCHW FP32 张量。
 */
std::optional<TensorBuffer> OpenCvImageBackend::to_tensor(const ImageBuffer& image,
                                                          const PreprocessConfig& config,
                                                          int frame_sequence,
                                                          const std::string& camera_id)
{
    if (config.output_layout != "NCHW") {
        last_error_ = "OpenCV image backend currently supports only NCHW tensor layout";
        return std::nullopt;
    }
    if (config.output_dtype != "FP32") {
        last_error_ = "OpenCV image backend currently supports only FP32 tensor dtype";
        return std::nullopt;
    }

    std::string error;
    const auto bgr = bgr_mat_from_image(image, error);
    if (bgr.empty()) {
        last_error_ = error;
        return std::nullopt;
    }

    // 以 RGB 约定喂给 YOLO 张量；如果模型导出时使用 BGR，需要在配置层明确扩展。
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);

    cv::Mat rgb_float;
    const auto scale = config.normalize ? 1.0 / 255.0 : 1.0;
    rgb.convertTo(rgb_float, CV_32FC3, scale);

    TensorBuffer tensor;
    tensor.data = pack_nchw_fp32(rgb_float);
    tensor.shape = {1, 3, image.height, image.width};
    tensor.layout = config.output_layout;
    tensor.dtype = config.output_dtype;
    tensor.frame_sequence = frame_sequence;
    tensor.camera_id = camera_id;

    last_error_.clear();
    return tensor;
}

/**
 * @brief 在图像上绘制检测框和标签。
 * @param image 输入图像。
 * @param detections 检测结果列表。
 * @return 成功返回绘制后的图像。
 */
std::optional<ImageBuffer> OpenCvImageBackend::draw_detections(
    const ImageBuffer& image,
    const std::vector<Detection>& detections)
{
    std::string error;
    const auto bgr_view = bgr_mat_from_image(image, error);
    if (bgr_view.empty()) {
        last_error_ = error;
        return std::nullopt;
    }

    cv::Mat rendered = bgr_view.clone();
    for (const auto& detection : detections) {
        const auto box = detection_to_rect(detection, rendered.cols, rendered.rows);
        if (box.empty()) {
            continue;
        }

        const cv::Scalar color(0, 255, 0);
        cv::rectangle(rendered, box, color, 2);
        const auto label = detection_label(detection);
        const auto label_origin = cv::Point(box.x, std::max(12, box.y - 4));
        cv::putText(rendered, label, label_origin, cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
    }

    last_error_.clear();
    return image_from_bgr_mat(rendered);
}

/**
 * @brief 返回图像后端类型标识。
 * @return 固定返回 `"opencv"`。
 */
std::string_view OpenCvImageBackend::kind() const noexcept
{
    return "opencv";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view OpenCvImageBackend::last_error() const noexcept
{
    return last_error_;
}

} // namespace sentinel
