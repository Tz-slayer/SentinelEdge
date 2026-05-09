#include "sentinel/image/image_backend_factory.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

#include <linux/videodev2.h>
#include <opencv2/imgcodecs.hpp>

namespace {

/**
 * @brief 断言条件成立，否则输出错误并退出。
 * @param condition 待检查条件。
 * @param message 失败时输出的错误消息。
 */
void expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

/**
 * @brief 构造一帧可被 OpenCV 解码的 MJPEG 测试帧。
 * @return 包含 JPEG 压缩数据的测试帧。
 */
sentinel::Frame make_jpeg_frame()
{
    cv::Mat image(16, 16, CV_8UC3, cv::Scalar(32, 96, 160));
    std::vector<std::uint8_t> encoded;
    cv::imencode(".jpg", image, encoded);

    sentinel::Frame frame;
    frame.sequence = 9;
    frame.camera_id = "image-backend-camera";
    frame.width = image.cols;
    frame.height = image.rows;
    frame.pixel_format = V4L2_PIX_FMT_MJPEG;
    frame.bytes_used = encoded.size();
    frame.data = std::move(encoded);
    return frame;
}

} // namespace

/**
 * @brief 验证 OpenCV 图像后端的解码、缩放、张量打包和画框能力。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    auto backend = sentinel::create_image_backend("opencv");
    expect(backend->kind() == "opencv", "factory should create OpenCV image backend");
    expect(backend->open(), "OpenCV image backend should open");

    const auto decoded = backend->decode(make_jpeg_frame());
    expect(decoded.has_value(), "OpenCV image backend should decode MJPEG frame");
    expect(decoded->width == 16 && decoded->height == 16, "decoded image size should match source");
    expect(decoded->pixel_format == "BGR24", "decoded image should be BGR24");
    expect(decoded->memory_type == "host", "decoded image should use host memory");

    const auto resized = backend->resize(*decoded, 8, 6);
    expect(resized.has_value(), "OpenCV image backend should resize image");
    expect(resized->width == 8 && resized->height == 6, "resized image size should match request");

    sentinel::PreprocessConfig preprocess;
    preprocess.output_width = 8;
    preprocess.output_height = 6;
    preprocess.output_layout = "NCHW";
    preprocess.output_dtype = "FP32";
    preprocess.normalize = true;

    const auto tensor = backend->to_tensor(*resized, preprocess, 9, "image-backend-camera");
    expect(tensor.has_value(), "OpenCV image backend should create tensor");
    expect(tensor->shape == std::vector<int>({1, 3, 6, 8}), "tensor shape should be NCHW");
    expect(tensor->data.size() == 1U * 3U * 6U * 8U * sizeof(float),
           "tensor bytes should match FP32 NCHW shape");

    sentinel::Detection detection;
    detection.label = "person";
    detection.confidence = 0.92;
    detection.bounding_box = sentinel::Rect{0.25, 0.25, 0.5, 0.5};
    detection.frame_sequence = 9;
    detection.camera_id = "image-backend-camera";

    const auto rendered = backend->draw_detections(*decoded, {detection});
    expect(rendered.has_value(), "OpenCV image backend should draw detections");
    expect(rendered->width == decoded->width && rendered->height == decoded->height,
           "rendered image should keep source size");

    backend->close();

    auto dvpp = sentinel::create_image_backend("dvpp");
    expect(dvpp->kind() == "dvpp", "factory should create DVPP image backend");
    expect(!dvpp->open(), "DVPP image backend should report unimplemented status");

    return 0;
}
