#include "sentinel/preprocess/preprocessor_factory.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

#include <linux/videodev2.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

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
    cv::Mat image(16, 16, CV_8UC3, cv::Scalar(10, 80, 200));
    std::vector<std::uint8_t> encoded;
    cv::imencode(".jpg", image, encoded);

    sentinel::Frame frame;
    frame.sequence = 7;
    frame.camera_id = "unit-camera";
    frame.width = image.cols;
    frame.height = image.rows;
    frame.pixel_format = V4L2_PIX_FMT_MJPEG;
    frame.bytes_used = encoded.size();
    frame.data = std::move(encoded);
    return frame;
}

} // namespace

/**
 * @brief 验证图像预处理策略工厂和 OpenCV 策略的最小行为。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    sentinel::PreprocessConfig config;
    config.backend = "opencv";
    config.output_width = 8;
    config.output_height = 6;
    config.output_layout = "NCHW";
    config.output_dtype = "FP32";
    config.normalize = true;

    auto opencv = sentinel::create_frame_preprocessor(config);
    expect(opencv->kind() == "opencv", "factory should create OpenCV preprocessor");
    expect(opencv->open(), "OpenCV preprocessor should open");

    const auto tensor = opencv->process(make_jpeg_frame());
    expect(tensor.has_value(), "OpenCV preprocessor should decode JPEG frame");
    expect(tensor->shape == std::vector<int>({1, 3, 6, 8}), "tensor shape should be NCHW");
    expect(tensor->layout == "NCHW", "tensor layout should be NCHW");
    expect(tensor->dtype == "FP32", "tensor dtype should be FP32");
    expect(tensor->frame_sequence == 7, "tensor should preserve frame sequence");
    expect(tensor->camera_id == "unit-camera", "tensor should preserve camera id");
    expect(tensor->data.size() == 1U * 3U * 6U * 8U * sizeof(float),
           "tensor byte size should match FP32 NCHW shape");
    opencv->close();

    config.backend = "dvpp";
    auto dvpp = sentinel::create_frame_preprocessor(config);
    expect(dvpp->kind() == "dvpp", "factory should create DVPP preprocessor");
    expect(!dvpp->open(), "DVPP preprocessor should clearly report unimplemented status");
    expect(!dvpp->last_error().empty(), "DVPP preprocessor should expose error text");

    bool unsupported_thrown = false;
    try {
        config.backend = "unknown";
        static_cast<void>(sentinel::create_frame_preprocessor(config));
    } catch (const std::exception&) {
        unsupported_thrown = true;
    }
    expect(unsupported_thrown, "unsupported preprocess backend should throw");

    return 0;
}
