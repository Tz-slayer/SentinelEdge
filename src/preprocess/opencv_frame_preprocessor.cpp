#include "sentinel/preprocess/opencv_frame_preprocessor.hpp"

#include "sentinel/image/image_backend_factory.hpp"

#include <utility>

namespace sentinel {

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

    image_backend_ = create_image_backend("opencv");
    if (!image_backend_->open()) {
        last_error_ = "unable to open OpenCV image backend: " +
                      std::string(image_backend_->last_error());
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
    if (image_backend_) {
        image_backend_->close();
        image_backend_.reset();
    }
    is_open_ = false;
}

/**
 * @brief 将视频帧转换为模型输入张量。
 * @param frame 视频源输出的原始帧。
 * @return 成功返回 NCHW FP32 张量；失败返回空。
 */
std::optional<TensorBuffer> OpenCvFramePreprocessor::process(const Frame& frame)
{
    if (!is_open_ || !image_backend_) {
        last_error_ = "OpenCV preprocessor is not open";
        return std::nullopt;
    }

    auto decoded = image_backend_->decode(frame);
    if (!decoded.has_value()) {
        last_error_ = image_backend_->last_error();
        return std::nullopt;
    }

    auto resized = image_backend_->resize(*decoded, config_.output_width, config_.output_height);
    if (!resized.has_value()) {
        last_error_ = image_backend_->last_error();
        return std::nullopt;
    }

    auto tensor = image_backend_->to_tensor(*resized, config_, frame.sequence, frame.camera_id);
    if (!tensor.has_value()) {
        last_error_ = image_backend_->last_error();
        return std::nullopt;
    }

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
