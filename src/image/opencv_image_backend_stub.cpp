#include "sentinel/image/opencv_image_backend.hpp"

namespace sentinel {

/**
 * @brief 报告当前二进制未启用 OpenCV 图像后端。
 * @return 固定返回 `false`。
 */
bool OpenCvImageBackend::open()
{
    last_error_ = "OpenCV image backend is not enabled at build time";
    return false;
}

/**
 * @brief 占位释放函数。
 */
void OpenCvImageBackend::close() noexcept
{
    is_open_ = false;
}

/**
 * @brief 占位解码函数。
 * @param frame 视频源输出的原始帧。
 * @return 固定返回空。
 */
std::optional<ImageBuffer> OpenCvImageBackend::decode(const Frame& frame)
{
    static_cast<void>(frame);
    last_error_ = "OpenCV image backend is not enabled at build time";
    return std::nullopt;
}

/**
 * @brief 占位缩放函数。
 * @param image 输入图像。
 * @param width 输出宽度。
 * @param height 输出高度。
 * @return 固定返回空。
 */
std::optional<ImageBuffer> OpenCvImageBackend::resize(const ImageBuffer& image, int width, int height)
{
    static_cast<void>(image);
    static_cast<void>(width);
    static_cast<void>(height);
    last_error_ = "OpenCV image backend is not enabled at build time";
    return std::nullopt;
}

/**
 * @brief 占位张量转换函数。
 * @param image 输入图像。
 * @param config 预处理输出配置。
 * @param frame_sequence 来源帧序号。
 * @param camera_id 来源摄像头 ID。
 * @return 固定返回空。
 */
std::optional<TensorBuffer> OpenCvImageBackend::to_tensor(const ImageBuffer& image,
                                                          const PreprocessConfig& config,
                                                          int frame_sequence,
                                                          const std::string& camera_id)
{
    static_cast<void>(image);
    static_cast<void>(config);
    static_cast<void>(frame_sequence);
    static_cast<void>(camera_id);
    last_error_ = "OpenCV image backend is not enabled at build time";
    return std::nullopt;
}

/**
 * @brief 占位检测框绘制函数。
 * @param image 输入图像。
 * @param detections 检测结果列表。
 * @return 固定返回空。
 */
std::optional<ImageBuffer> OpenCvImageBackend::draw_detections(
    const ImageBuffer& image,
    const std::vector<Detection>& detections)
{
    static_cast<void>(image);
    static_cast<void>(detections);
    last_error_ = "OpenCV image backend is not enabled at build time";
    return std::nullopt;
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
