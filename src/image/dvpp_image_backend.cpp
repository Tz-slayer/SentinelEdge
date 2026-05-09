#include "sentinel/image/dvpp_image_backend.hpp"

namespace sentinel {

/**
 * @brief 报告 DVPP 图像后端尚未实现。
 * @return 固定返回 `false`。
 */
bool DvppImageBackend::open()
{
    set_unimplemented_error();
    return false;
}

/**
 * @brief 占位释放函数。
 */
void DvppImageBackend::close() noexcept
{
}

/**
 * @brief 占位解码函数。
 * @param frame 视频源输出的原始帧。
 * @return 固定返回空。
 */
std::optional<ImageBuffer> DvppImageBackend::decode(const Frame& frame)
{
    static_cast<void>(frame);
    set_unimplemented_error();
    return std::nullopt;
}

/**
 * @brief 占位缩放函数。
 * @param image 输入图像。
 * @param width 输出宽度。
 * @param height 输出高度。
 * @return 固定返回空。
 */
std::optional<ImageBuffer> DvppImageBackend::resize(const ImageBuffer& image, int width, int height)
{
    static_cast<void>(image);
    static_cast<void>(width);
    static_cast<void>(height);
    set_unimplemented_error();
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
std::optional<TensorBuffer> DvppImageBackend::to_tensor(const ImageBuffer& image,
                                                        const PreprocessConfig& config,
                                                        int frame_sequence,
                                                        const std::string& camera_id)
{
    static_cast<void>(image);
    static_cast<void>(config);
    static_cast<void>(frame_sequence);
    static_cast<void>(camera_id);
    set_unimplemented_error();
    return std::nullopt;
}

/**
 * @brief 占位检测框绘制函数。
 * @param image 输入图像。
 * @param detections 检测结果列表。
 * @return 固定返回空。
 */
std::optional<ImageBuffer> DvppImageBackend::draw_detections(
    const ImageBuffer& image,
    const std::vector<Detection>& detections)
{
    static_cast<void>(image);
    static_cast<void>(detections);
    set_unimplemented_error();
    return std::nullopt;
}

/**
 * @brief 返回图像后端类型标识。
 * @return 固定返回 `"dvpp"`。
 */
std::string_view DvppImageBackend::kind() const noexcept
{
    return "dvpp";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view DvppImageBackend::last_error() const noexcept
{
    return last_error_;
}

/**
 * @brief 设置统一的未实现错误。
 */
void DvppImageBackend::set_unimplemented_error()
{
    last_error_ = "DVPP image backend is not implemented yet";
}

} // namespace sentinel
