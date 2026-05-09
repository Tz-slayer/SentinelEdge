#include "sentinel/image/dvpp_image_backend.hpp"

#include "sentinel/ascend/acl_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

#include <linux/videodev2.h>

#if SENTINEL_ENABLE_DVPP
#include <acl/acl.h>
#include <acl/acl_rt.h>
#include <acl/ops/acl_dvpp.h>
#endif

namespace sentinel {
namespace {

#if SENTINEL_ENABLE_DVPP

/**
 * @brief 将数值向上对齐到指定粒度。
 * @param value 待对齐数值。
 * @param alignment 对齐粒度。
 * @return 对齐后的数值。
 */
std::uint32_t align_up(std::uint32_t value, std::uint32_t alignment)
{
    return ((value + alignment - 1U) / alignment) * alignment;
}

/**
 * @brief 判断 AscendCL 返回码是否表示成功。
 * @param error AscendCL 返回码。
 * @return 成功返回 `true`。
 */
bool acl_ok(aclError error) noexcept
{
    return error == ACL_SUCCESS;
}

/**
 * @brief 计算 NV12/YUV420SP 缓冲区大小。
 * @param width_stride Y 平面和 UV 平面的行跨度。
 * @param height_stride Y 平面对齐高度。
 * @return 字节数。
 */
std::uint32_t yuv420sp_size(std::uint32_t width_stride, std::uint32_t height_stride)
{
    return width_stride * height_stride * 3U / 2U;
}

/**
 * @brief 将整数裁剪到 `uint8_t` 可表示范围。
 * @param value 待裁剪整数。
 * @return 裁剪后的 8 位值。
 */
std::uint8_t clamp_u8(int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

/**
 * @brief 将 NV12 Host 缓冲区转换为 BGR24 图像。
 * @param nv12 NV12 Host 缓冲区。
 * @param width 有效宽度。
 * @param height 有效高度。
 * @param width_stride 行跨度。
 * @param height_stride Y 平面对齐高度。
 * @return BGR24 图像缓冲区。
 */
ImageBuffer nv12_to_bgr24(const std::vector<std::uint8_t>& nv12,
                          int width,
                          int height,
                          std::uint32_t width_stride,
                          std::uint32_t height_stride)
{
    ImageBuffer image;
    image.width = width;
    image.height = height;
    image.pixel_format = "BGR24";
    image.memory_type = "host";
    image.data.resize(static_cast<std::size_t>(width) * height * 3U);

    const auto* y_plane = nv12.data();
    const auto* uv_plane = nv12.data() +
                           static_cast<std::size_t>(width_stride) * height_stride;
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const auto y = static_cast<int>(y_plane[static_cast<std::size_t>(row) *
                                                    width_stride + col]);
            const auto uv_index = static_cast<std::size_t>(row / 2) * width_stride +
                                  static_cast<std::size_t>(col / 2) * 2U;
            const auto u = static_cast<int>(uv_plane[uv_index]) - 128;
            const auto v = static_cast<int>(uv_plane[uv_index + 1U]) - 128;

            const auto r = clamp_u8(y + ((1436 * v) >> 10));
            const auto g = clamp_u8(y - ((352 * u + 731 * v) >> 10));
            const auto b = clamp_u8(y + ((1815 * u) >> 10));
            const auto offset = (static_cast<std::size_t>(row) * width + col) * 3U;
            image.data[offset] = b;
            image.data[offset + 1U] = g;
            image.data[offset + 2U] = r;
        }
    }
    return image;
}

/**
 * @brief AscendCL Device 内存 RAII 包装。
 */
class DeviceMemory {
public:
    /**
     * @brief 释放 Device 内存。
     */
    ~DeviceMemory()
    {
        reset();
    }

    DeviceMemory() = default;
    DeviceMemory(const DeviceMemory&) = delete;
    DeviceMemory& operator=(const DeviceMemory&) = delete;

    /**
     * @brief 申请 Device 内存。
     * @param size 申请字节数。
     * @param error 失败时写入错误文本。
     * @return 成功返回 `true`。
     */
    bool allocate(std::size_t size, std::string& error)
    {
        reset();
        auto ret = aclrtMalloc(&data_, size, ACL_MEM_MALLOC_HUGE_FIRST);
        if (!acl_ok(ret)) {
            error = make_acl_error("aclrtMalloc", static_cast<int>(ret));
            return false;
        }
        size_ = size;
        return true;
    }

    /**
     * @brief 确保当前 Device 内存容量至少为指定大小。
     * @param size 需要的最小字节数。
     * @param error 失败时写入错误文本。
     * @return 成功返回 `true`。
     */
    bool ensure_size(std::size_t size, std::string& error)
    {
        if (data_ != nullptr && size_ >= size) {
            return true;
        }
        return allocate(size, error);
    }

    /**
     * @brief 释放当前 Device 内存。
     */
    void reset() noexcept
    {
        if (data_ != nullptr) {
            aclrtFree(data_);
            data_ = nullptr;
            size_ = 0;
        }
    }

    /**
     * @brief 返回 Device 指针。
     * @return Device 内存地址。
     */
    void* data() const noexcept
    {
        return data_;
    }

    /**
     * @brief 返回 Device 内存大小。
     * @return 字节数。
     */
    std::size_t size() const noexcept
    {
        return size_;
    }

private:
    void* data_{nullptr};
    std::size_t size_{0};
};

/**
 * @brief DVPP 图片描述 RAII 包装。
 */
class DvppPicDesc {
public:
    /**
     * @brief 创建 DVPP 图片描述。
     */
    DvppPicDesc()
        : desc_(acldvppCreatePicDesc())
    {
    }

    /**
     * @brief 销毁 DVPP 图片描述。
     */
    ~DvppPicDesc()
    {
        if (desc_ != nullptr) {
            acldvppDestroyPicDesc(desc_);
        }
    }

    DvppPicDesc(const DvppPicDesc&) = delete;
    DvppPicDesc& operator=(const DvppPicDesc&) = delete;

    /**
     * @brief 判断图片描述是否创建成功。
     * @return 成功返回 `true`。
     */
    bool valid() const noexcept
    {
        return desc_ != nullptr;
    }

    /**
     * @brief 返回底层图片描述。
     * @return DVPP 图片描述指针。
     */
    acldvppPicDesc* get() const noexcept
    {
        return desc_;
    }

private:
    acldvppPicDesc* desc_{nullptr};
};

#endif

/**
 * @brief 校验 Host BGR24 图像缓冲区。
 * @param image 待校验图像。
 * @param error 失败时写入错误文本。
 * @return 成功返回 `true`。
 */
bool validate_bgr24_image(const ImageBuffer& image, std::string& error)
{
    if (image.memory_type != "host" || image.pixel_format != "BGR24") {
        error = "DVPP image backend expects host BGR24 image buffers";
        return false;
    }
    if (image.width <= 0 || image.height <= 0) {
        error = "image width and height must be positive";
        return false;
    }
    const auto required = static_cast<std::size_t>(image.width) *
                          static_cast<std::size_t>(image.height) * 3U;
    if (image.data.size() < required) {
        error = "BGR24 image data is smaller than width*height*3";
        return false;
    }
    return true;
}

/**
 * @brief 计算图像像素偏移。
 * @param width 图像宽度。
 * @param x x 坐标。
 * @param y y 坐标。
 * @return BGR24 字节偏移。
 */
std::size_t bgr_offset(int width, int x, int y)
{
    return (static_cast<std::size_t>(y) * width + x) * 3U;
}

/**
 * @brief 设置 BGR24 像素颜色。
 * @param image 待修改图像。
 * @param x x 坐标。
 * @param y y 坐标。
 * @param blue 蓝色分量。
 * @param green 绿色分量。
 * @param red 红色分量。
 */
void set_bgr_pixel(ImageBuffer& image,
                   int x,
                   int y,
                   std::uint8_t blue,
                   std::uint8_t green,
                   std::uint8_t red)
{
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
        return;
    }
    const auto offset = bgr_offset(image.width, x, y);
    image.data[offset] = blue;
    image.data[offset + 1U] = green;
    image.data[offset + 2U] = red;
}

/**
 * @brief 将归一化检测框转换为像素矩形。
 * @param detection 检测结果。
 * @param image_width 图像宽度。
 * @param image_height 图像高度。
 * @return 像素矩形，字段为 x、y、width、height。
 */
Rect detection_to_pixel_rect(const Detection& detection, int image_width, int image_height)
{
    const auto left = std::clamp(detection.bounding_box.x, 0.0, 1.0) * image_width;
    const auto top = std::clamp(detection.bounding_box.y, 0.0, 1.0) * image_height;
    const auto right = std::clamp(detection.bounding_box.x + detection.bounding_box.width,
                                  0.0,
                                  1.0) * image_width;
    const auto bottom = std::clamp(detection.bounding_box.y + detection.bounding_box.height,
                                   0.0,
                                   1.0) * image_height;
    return Rect{
        std::floor(left),
        std::floor(top),
        std::max(1.0, std::ceil(right - left)),
        std::max(1.0, std::ceil(bottom - top)),
    };
}

} // namespace

/**
 * @brief DVPP 图像处理真实实现。
 *
 * 该实现会复用 JPEGD 输出 Device buffer、Host NV12 缓冲区和图片描述。
 * 调试输出链路最终仍需要 Host BGR24 图像用于画框和 JPEG 编码，但不会
 * 在每帧重复创建 DVPP 基础资源。
 */
class DvppImageBackend::Impl {
public:
    /**
     * @brief 析构时释放 DVPP 资源。
     */
    ~Impl()
    {
        close();
    }

    /**
     * @brief 初始化 DVPP 图像处理后端。
     * @return 成功返回 `true`。
     */
    bool open()
    {
        close();
#if SENTINEL_ENABLE_DVPP
        runtime_session_ = AclRuntimeSession::acquire(0, last_error_);
        if (!runtime_session_) {
            return false;
        }

        auto ret = aclrtCreateStream(&stream_);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("aclrtCreateStream", static_cast<int>(ret));
            close();
            return false;
        }

        channel_desc_ = acldvppCreateChannelDesc();
        if (channel_desc_ == nullptr) {
            last_error_ = "acldvppCreateChannelDesc failed";
            close();
            return false;
        }

        ret = acldvppCreateChannel(channel_desc_);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("acldvppCreateChannel", static_cast<int>(ret));
            close();
            return false;
        }
        channel_created_ = true;

        decode_desc_ = std::make_unique<DvppPicDesc>();
        if (!decode_desc_->valid()) {
            last_error_ = "failed to create reusable DVPP decode descriptor";
            close();
            return false;
        }

        last_error_.clear();
        return true;
#else
        last_error_ = "DVPP image backend is not enabled at build time";
        return false;
#endif
    }

    /**
     * @brief 释放 DVPP 图像处理后端资源。
     */
    void close() noexcept
    {
#if SENTINEL_ENABLE_DVPP
        if (stream_ != nullptr) {
            aclrtSynchronizeStream(stream_);
        }
        decode_desc_.reset();
        decode_output_.reset();
        host_nv12_.clear();
        if (channel_created_) {
            acldvppDestroyChannel(channel_desc_);
            channel_created_ = false;
        }
        if (channel_desc_ != nullptr) {
            acldvppDestroyChannelDesc(channel_desc_);
            channel_desc_ = nullptr;
        }
        if (stream_ != nullptr) {
            aclrtDestroyStream(stream_);
            stream_ = nullptr;
        }
        runtime_session_.reset();
#endif
    }

    /**
     * @brief 使用 DVPP 将 MJPEG 帧解码为 Host BGR24 图像。
     * @param frame 视频源输出的原始帧。
     * @return 成功返回图像；失败返回空。
     */
    std::optional<ImageBuffer> decode(const Frame& frame)
    {
#if SENTINEL_ENABLE_DVPP
        if (channel_desc_ == nullptr || stream_ == nullptr) {
            last_error_ = "DVPP image backend is not open";
            return std::nullopt;
        }
        if (frame.pixel_format != V4L2_PIX_FMT_MJPEG && frame.pixel_format != V4L2_PIX_FMT_JPEG) {
            last_error_ = "DVPP image backend currently supports only MJPEG/JPEG frames";
            return std::nullopt;
        }
        const auto* payload = frame.payload_data();
        const auto payload_size = frame.payload_size();
        if (payload == nullptr || payload_size == 0U) {
            last_error_ = "input frame is empty";
            return std::nullopt;
        }
        if (payload_size > std::numeric_limits<std::uint32_t>::max()) {
            last_error_ = "input frame is too large for DVPP JPEGD";
            return std::nullopt;
        }

        std::uint32_t width = 0;
        std::uint32_t height = 0;
        int32_t components = 0;
        auto ret = acldvppJpegGetImageInfo(payload,
                                           static_cast<std::uint32_t>(payload_size),
                                           &width,
                                           &height,
                                           &components);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("acldvppJpegGetImageInfo", static_cast<int>(ret));
            return std::nullopt;
        }
        if (width == 0U || height == 0U) {
            last_error_ = "acldvppJpegGetImageInfo returned empty image size";
            return std::nullopt;
        }

        std::uint32_t decode_size = 0;
        ret = acldvppJpegPredictDecSize(payload,
                                        static_cast<std::uint32_t>(payload_size),
                                        PIXEL_FORMAT_YUV_SEMIPLANAR_420,
                                        &decode_size);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("acldvppJpegPredictDecSize", static_cast<int>(ret));
            return std::nullopt;
        }

        if (!decode_output_.ensure_size(decode_size, last_error_)) {
            return std::nullopt;
        }

        const auto width_stride = align_up(width, 128U);
        const auto height_stride = align_up(height, 16U);
        if (decode_desc_ == nullptr || !decode_desc_->valid() ||
            !setup_pic_desc(decode_desc_->get(),
                            decode_output_.data(),
                            decode_size,
                            width,
                            height,
                            width_stride,
                            height_stride)) {
            return std::nullopt;
        }

        ret = acldvppJpegDecodeAsync(channel_desc_,
                                     payload,
                                     static_cast<std::uint32_t>(payload_size),
                                     decode_desc_->get(),
                                     stream_);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("acldvppJpegDecodeAsync", static_cast<int>(ret));
            return std::nullopt;
        }
        ret = aclrtSynchronizeStream(stream_);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("aclrtSynchronizeStream", static_cast<int>(ret));
            return std::nullopt;
        }

        const auto host_nv12_size = yuv420sp_size(width_stride, height_stride);
        host_nv12_.resize(host_nv12_size);
        ret = aclrtMemcpy(host_nv12_.data(),
                          host_nv12_.size(),
                          decode_output_.data(),
                          decode_size,
                          ACL_MEMCPY_DEVICE_TO_HOST);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("aclrtMemcpy(decode_output)", static_cast<int>(ret));
            return std::nullopt;
        }

        last_error_.clear();
        return nv12_to_bgr24(host_nv12_,
                             static_cast<int>(width),
                             static_cast<int>(height),
                             width_stride,
                             height_stride);
#else
        static_cast<void>(frame);
        last_error_ = "DVPP image backend is not enabled at build time";
        return std::nullopt;
#endif
    }

    /**
     * @brief 返回最近一次错误文本。
     * @return 错误文本。
     */
    std::string_view last_error() const noexcept
    {
        return last_error_;
    }

private:
#if SENTINEL_ENABLE_DVPP
    /**
     * @brief 设置 YUV420SP 图片描述字段。
     * @param desc 待写入的 DVPP 图片描述。
     * @param data Device 图像缓冲区。
     * @param size 图像缓冲区大小。
     * @param width 有效宽度。
     * @param height 有效高度。
     * @param width_stride 对齐后的行跨度。
     * @param height_stride 对齐后的高度。
     * @return 成功返回 `true`。
     */
    bool setup_pic_desc(acldvppPicDesc* desc,
                        void* data,
                        std::uint32_t size,
                        std::uint32_t width,
                        std::uint32_t height,
                        std::uint32_t width_stride,
                        std::uint32_t height_stride)
    {
        const auto set_field = [&](aclError error, std::string_view action) {
            if (!acl_ok(error)) {
                last_error_ = make_acl_error(action, static_cast<int>(error));
                return false;
            }
            return true;
        };

        return set_field(acldvppSetPicDescData(desc, data), "acldvppSetPicDescData") &&
               set_field(acldvppSetPicDescSize(desc, size), "acldvppSetPicDescSize") &&
               set_field(acldvppSetPicDescFormat(desc, PIXEL_FORMAT_YUV_SEMIPLANAR_420),
                         "acldvppSetPicDescFormat") &&
               set_field(acldvppSetPicDescWidth(desc, width), "acldvppSetPicDescWidth") &&
               set_field(acldvppSetPicDescHeight(desc, height), "acldvppSetPicDescHeight") &&
               set_field(acldvppSetPicDescWidthStride(desc, width_stride),
                         "acldvppSetPicDescWidthStride") &&
               set_field(acldvppSetPicDescHeightStride(desc, height_stride),
                         "acldvppSetPicDescHeightStride");
    }
#endif

    std::string last_error_;
#if SENTINEL_ENABLE_DVPP
    std::unique_ptr<AclRuntimeSession> runtime_session_;
    aclrtStream stream_{nullptr};
    acldvppChannelDesc* channel_desc_{nullptr};
    bool channel_created_{false};
    DeviceMemory decode_output_;
    std::vector<std::uint8_t> host_nv12_;
    std::unique_ptr<DvppPicDesc> decode_desc_;
#endif
};

/**
 * @brief 构造 DVPP 图像处理后端对象。
 */
DvppImageBackend::DvppImageBackend()
    : impl_(std::make_unique<Impl>())
{
}

/**
 * @brief 释放 DVPP 图像处理资源。
 */
DvppImageBackend::~DvppImageBackend() = default;

/**
 * @brief 初始化 DVPP 图像后端。
 * @return 成功返回 `true`。
 */
bool DvppImageBackend::open()
{
    const auto opened = impl_->open();
    last_error_ = impl_->last_error();
    return opened;
}

/**
 * @brief 释放 DVPP 图像后端资源。
 */
void DvppImageBackend::close() noexcept
{
    if (impl_) {
        impl_->close();
    }
}

/**
 * @brief 使用 DVPP 将视频帧解码为 Host BGR24 图像。
 * @param frame 视频源输出的原始帧。
 * @return 成功返回图像；失败返回空。
 */
std::optional<ImageBuffer> DvppImageBackend::decode(const Frame& frame)
{
    auto image = impl_->decode(frame);
    last_error_ = impl_->last_error();
    return image;
}

/**
 * @brief 缩放 Host BGR24 图像。
 * @param image 输入图像。
 * @param width 输出宽度。
 * @param height 输出高度。
 * @return 成功返回缩放后的图像。
 */
std::optional<ImageBuffer> DvppImageBackend::resize(const ImageBuffer& image, int width, int height)
{
    if (width <= 0 || height <= 0) {
        last_error_ = "resize output width and height must be positive";
        return std::nullopt;
    }
    std::string error;
    if (!validate_bgr24_image(image, error)) {
        last_error_ = error;
        return std::nullopt;
    }

    ImageBuffer output;
    output.width = width;
    output.height = height;
    output.pixel_format = "BGR24";
    output.memory_type = "host";
    output.data.resize(static_cast<std::size_t>(width) * height * 3U);

    for (int y = 0; y < height; ++y) {
        const auto source_y = std::min(image.height - 1,
                                       static_cast<int>((static_cast<long long>(y) * image.height) /
                                                        height));
        for (int x = 0; x < width; ++x) {
            const auto source_x = std::min(image.width - 1,
                                           static_cast<int>((static_cast<long long>(x) * image.width) /
                                                            width));
            const auto source_offset = bgr_offset(image.width, source_x, source_y);
            const auto output_offset = bgr_offset(width, x, y);
            output.data[output_offset] = image.data[source_offset];
            output.data[output_offset + 1U] = image.data[source_offset + 1U];
            output.data[output_offset + 2U] = image.data[source_offset + 2U];
        }
    }

    last_error_.clear();
    return output;
}

/**
 * @brief 将 Host BGR24 图像转换为模型输入张量。
 * @param image 输入图像。
 * @param config 预处理输出配置。
 * @param frame_sequence 来源帧序号。
 * @param camera_id 来源摄像头 ID。
 * @return 成功返回模型输入张量。
 */
std::optional<TensorBuffer> DvppImageBackend::to_tensor(const ImageBuffer& image,
                                                        const PreprocessConfig& config,
                                                        int frame_sequence,
                                                        const std::string& camera_id)
{
    if (config.output_layout != "NCHW") {
        last_error_ = "DVPP image backend currently supports only NCHW tensor layout";
        return std::nullopt;
    }
    if (config.output_dtype != "FP32") {
        last_error_ = "DVPP image backend currently supports only FP32 tensor dtype";
        return std::nullopt;
    }

    std::string error;
    if (!validate_bgr24_image(image, error)) {
        last_error_ = error;
        return std::nullopt;
    }

    const auto plane_size = static_cast<std::size_t>(image.width) * image.height;
    std::vector<float> chw(plane_size * 3U);
    const auto scale = config.normalize ? (1.0F / 255.0F) : 1.0F;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const auto pixel_index = static_cast<std::size_t>(y) * image.width + x;
            const auto offset = pixel_index * 3U;
            // 输入图像为 BGR24，模型张量按 RGB 通道顺序写入。
            chw[pixel_index] = static_cast<float>(image.data[offset + 2U]) * scale;
            chw[plane_size + pixel_index] = static_cast<float>(image.data[offset + 1U]) * scale;
            chw[plane_size * 2U + pixel_index] = static_cast<float>(image.data[offset]) * scale;
        }
    }

    TensorBuffer tensor;
    tensor.data.resize(chw.size() * sizeof(float));
    std::memcpy(tensor.data.data(), chw.data(), tensor.data.size());
    tensor.shape = {1, 3, image.height, image.width};
    tensor.layout = config.output_layout;
    tensor.dtype = config.output_dtype;
    tensor.frame_sequence = frame_sequence;
    tensor.camera_id = camera_id;

    last_error_.clear();
    return tensor;
}

/**
 * @brief 在 Host BGR24 图像上绘制检测框。
 * @param image 输入图像。
 * @param detections 检测结果列表。
 * @return 成功返回绘制后的图像。
 */
std::optional<ImageBuffer> DvppImageBackend::draw_detections(
    const ImageBuffer& image,
    const std::vector<Detection>& detections)
{
    std::string error;
    if (!validate_bgr24_image(image, error)) {
        last_error_ = error;
        return std::nullopt;
    }

    ImageBuffer rendered = image;
    for (const auto& detection : detections) {
        const auto rect = detection_to_pixel_rect(detection, rendered.width, rendered.height);
        const auto left = static_cast<int>(rect.x);
        const auto top = static_cast<int>(rect.y);
        const auto right = std::min(rendered.width - 1, left + static_cast<int>(rect.width));
        const auto bottom = std::min(rendered.height - 1, top + static_cast<int>(rect.height));
        for (int thickness = 0; thickness < 2; ++thickness) {
            for (int x = left; x <= right; ++x) {
                set_bgr_pixel(rendered, x, top + thickness, 0, 255, 0);
                set_bgr_pixel(rendered, x, bottom - thickness, 0, 255, 0);
            }
            for (int y = top; y <= bottom; ++y) {
                set_bgr_pixel(rendered, left + thickness, y, 0, 255, 0);
                set_bgr_pixel(rendered, right - thickness, y, 0, 255, 0);
            }
        }
    }

    last_error_.clear();
    return rendered;
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

} // namespace sentinel
