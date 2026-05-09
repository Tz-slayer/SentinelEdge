#include "sentinel/preprocess/dvpp_frame_preprocessor.hpp"

#include "sentinel/ascend/acl_runtime.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
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
 * @param alignment 对齐粒度，必须大于 0。
 * @return 对齐后的数值。
 */
std::uint32_t align_up(std::uint32_t value, std::uint32_t alignment)
{
    return ((value + alignment - 1U) / alignment) * alignment;
}

/**
 * @brief 将整数裁剪到 `uint8_t` 可表示范围。
 * @param value 待裁剪整数。
 * @return 裁剪后的 8 位无符号值。
 */
std::uint8_t clamp_u8(int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

/**
 * @brief 判断视频帧是否是 MJPEG 压缩帧。
 * @param frame 待判断的视频帧。
 * @return 是 MJPEG 返回 `true`。
 */
bool is_mjpeg_frame(const Frame& frame)
{
    return frame.pixel_format == V4L2_PIX_FMT_MJPEG;
}

/**
 * @brief 计算 NV12/YUV420SP 缓冲区大小。
 * @param width_stride Y 平面和 UV 平面的行跨度。
 * @param height_stride Y 平面的对齐高度。
 * @return 需要分配的字节数。
 */
std::uint32_t yuv420sp_size(std::uint32_t width_stride, std::uint32_t height_stride)
{
    return width_stride * height_stride * 3U / 2U;
}

/**
 * @brief 将一帧 NV12 数据转换并打包为 RGB NCHW FP32。
 * @param nv12 包含 Y 平面和 UV 交错平面的 Host 缓冲区。
 * @param width 有效图像宽度。
 * @param height 有效图像高度。
 * @param width_stride Y/UV 平面行跨度。
 * @param height_stride Y 平面对齐高度。
 * @param normalize 是否按 1/255 归一化。
 * @return NCHW FP32 字节缓冲区，通道顺序为 RGB。
 */
std::vector<std::uint8_t> pack_nv12_to_rgb_nchw_fp32(const std::vector<std::uint8_t>& nv12,
                                                     int width,
                                                     int height,
                                                     std::uint32_t width_stride,
                                                     std::uint32_t height_stride,
                                                     bool normalize)
{
    const auto plane_size = static_cast<std::size_t>(width) *
                            static_cast<std::size_t>(height);
    std::vector<float> chw(plane_size * 3U);
    const auto* y_plane = nv12.data();
    const auto* uv_plane = nv12.data() +
                           static_cast<std::size_t>(width_stride) * height_stride;
    const auto scale = normalize ? (1.0F / 255.0F) : 1.0F;

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
            const auto pixel_index = static_cast<std::size_t>(row) * width + col;

            chw[pixel_index] = static_cast<float>(r) * scale;
            chw[plane_size + pixel_index] = static_cast<float>(g) * scale;
            chw[plane_size * 2U + pixel_index] = static_cast<float>(b) * scale;
        }
    }

    std::vector<std::uint8_t> bytes(chw.size() * sizeof(float));
    std::memcpy(bytes.data(), chw.data(), bytes.size());
    return bytes;
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
 * @brief AscendCL Device 内存 RAII 包装。
 */
class DeviceMemory {
public:
    /**
     * @brief 构造空 Device 内存对象。
     */
    DeviceMemory() = default;

    /**
     * @brief 释放 Device 内存。
     */
    ~DeviceMemory()
    {
        reset();
    }

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
     * @brief 返回底层 DVPP 图片描述。
     * @return 图片描述指针。
     */
    acldvppPicDesc* get() const noexcept
    {
        return desc_;
    }

private:
    acldvppPicDesc* desc_{nullptr};
};

#endif

} // namespace

/**
 * @brief DVPP 预处理真实实现。
 */
class DvppFramePreprocessor::Impl {
public:
    /**
     * @brief 保存 DVPP 预处理配置。
     * @param config 预处理配置。
     */
    explicit Impl(PreprocessConfig config)
        : config_(std::move(config))
    {
    }

    /**
     * @brief 析构时释放 DVPP 资源。
     */
    ~Impl()
    {
        close();
    }

    /**
     * @brief 初始化 DVPP 运行时资源。
     * @return 成功返回 `true`。
     */
    bool open()
    {
        close();
        if (config_.output_width <= 0 || config_.output_height <= 0) {
            last_error_ = "preprocess output size must be positive";
            return false;
        }
        if (config_.output_layout != "NCHW") {
            last_error_ = "DVPP preprocessor currently supports only NCHW layout";
            return false;
        }
        if (config_.output_dtype != "FP32") {
            last_error_ = "DVPP preprocessor currently supports only FP32 dtype";
            return false;
        }

#if SENTINEL_ENABLE_DVPP
        runtime_session_ = AclRuntimeSession::acquire(config_.device_id, last_error_);
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
        last_error_.clear();
        return true;
#else
        last_error_ = "DVPP preprocessor is not enabled at build time";
        return false;
#endif
    }

    /**
     * @brief 释放 DVPP 运行时资源。
     */
    void close() noexcept
    {
#if SENTINEL_ENABLE_DVPP
        if (stream_ != nullptr) {
            aclrtSynchronizeStream(stream_);
        }
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
     * @brief 将 MJPEG 帧通过 DVPP 解码缩放后转换为模型输入张量。
     * @param frame 视频源输出的原始帧。
     * @return 成功返回模型输入张量。
     */
    std::optional<TensorBuffer> process(const Frame& frame)
    {
#if SENTINEL_ENABLE_DVPP
        if (channel_desc_ == nullptr || stream_ == nullptr) {
            last_error_ = "DVPP preprocessor is not open";
            return std::nullopt;
        }
        if (!is_mjpeg_frame(frame)) {
            last_error_ = "DVPP copy preprocessor currently supports only MJPEG frames";
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

        std::uint32_t source_width = 0;
        std::uint32_t source_height = 0;
        int32_t components = 0;
        auto ret = acldvppJpegGetImageInfo(payload,
                                           static_cast<std::uint32_t>(payload_size),
                                           &source_width,
                                           &source_height,
                                           &components);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("acldvppJpegGetImageInfo", static_cast<int>(ret));
            return std::nullopt;
        }
        if (source_width == 0U || source_height == 0U) {
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

        DeviceMemory decode_output;
        if (!decode_output.allocate(decode_size, last_error_)) {
            return std::nullopt;
        }

        const auto decode_width_stride = align_up(source_width, 128U);
        const auto decode_height_stride = align_up(source_height, 16U);
        DvppPicDesc decode_desc;
        if (!decode_desc.valid() ||
            !setup_pic_desc(decode_desc.get(),
                            decode_output.data(),
                            decode_size,
                            source_width,
                            source_height,
                            decode_width_stride,
                            decode_height_stride)) {
            return std::nullopt;
        }

        ret = acldvppJpegDecodeAsync(channel_desc_,
                                     payload,
                                     static_cast<std::uint32_t>(payload_size),
                                     decode_desc.get(),
                                     stream_);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("acldvppJpegDecodeAsync", static_cast<int>(ret));
            return std::nullopt;
        }

        const auto output_width = static_cast<std::uint32_t>(config_.output_width);
        const auto output_height = static_cast<std::uint32_t>(config_.output_height);
        const auto output_width_stride = align_up(output_width, 16U);
        const auto output_height_stride = align_up(output_height, 2U);
        const auto resize_size = yuv420sp_size(output_width_stride, output_height_stride);

        DeviceMemory resize_output;
        if (!resize_output.allocate(resize_size, last_error_)) {
            return std::nullopt;
        }

        DvppPicDesc resize_desc;
        if (!resize_desc.valid() ||
            !setup_pic_desc(resize_desc.get(),
                            resize_output.data(),
                            resize_size,
                            output_width,
                            output_height,
                            output_width_stride,
                            output_height_stride)) {
            return std::nullopt;
        }

        auto* resize_config = acldvppCreateResizeConfig();
        if (resize_config == nullptr) {
            last_error_ = "acldvppCreateResizeConfig failed";
            return std::nullopt;
        }

        ret = acldvppVpcResizeAsync(channel_desc_,
                                    decode_desc.get(),
                                    resize_desc.get(),
                                    resize_config,
                                    stream_);
        acldvppDestroyResizeConfig(resize_config);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("acldvppVpcResizeAsync", static_cast<int>(ret));
            return std::nullopt;
        }

        ret = aclrtSynchronizeStream(stream_);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("aclrtSynchronizeStream", static_cast<int>(ret));
            return std::nullopt;
        }

        std::vector<std::uint8_t> host_nv12(resize_size);
        ret = aclrtMemcpy(host_nv12.data(),
                          host_nv12.size(),
                          resize_output.data(),
                          resize_output.size(),
                          ACL_MEMCPY_DEVICE_TO_HOST);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("aclrtMemcpy(resize_output)", static_cast<int>(ret));
            return std::nullopt;
        }

        TensorBuffer tensor;
        tensor.data = pack_nv12_to_rgb_nchw_fp32(host_nv12,
                                                 config_.output_width,
                                                 config_.output_height,
                                                 output_width_stride,
                                                 output_height_stride,
                                                 config_.normalize);
        tensor.shape = {1, 3, config_.output_height, config_.output_width};
        tensor.layout = config_.output_layout;
        tensor.dtype = config_.output_dtype;
        tensor.frame_sequence = frame.sequence;
        tensor.camera_id = frame.camera_id;

        last_error_.clear();
        return tensor;
#else
        static_cast<void>(frame);
        last_error_ = "DVPP preprocessor is not enabled at build time";
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

    PreprocessConfig config_;
    std::string last_error_;
#if SENTINEL_ENABLE_DVPP
    std::unique_ptr<AclRuntimeSession> runtime_session_;
    aclrtStream stream_{nullptr};
    acldvppChannelDesc* channel_desc_{nullptr};
    bool channel_created_{false};
#endif
};

/**
 * @brief 使用预处理配置构造 DVPP 策略。
 * @param config 图像预处理输出配置。
 */
DvppFramePreprocessor::DvppFramePreprocessor(PreprocessConfig config)
    : config_(std::move(config))
    , impl_(std::make_unique<Impl>(config_))
{
}

/**
 * @brief 释放 DVPP 策略资源。
 */
DvppFramePreprocessor::~DvppFramePreprocessor() = default;

/**
 * @brief 初始化 DVPP 预处理策略。
 * @return 成功返回 `true`。
 */
bool DvppFramePreprocessor::open()
{
    const auto opened = impl_->open();
    last_error_ = impl_->last_error();
    return opened;
}

/**
 * @brief 释放 DVPP 预处理策略资源。
 */
void DvppFramePreprocessor::close() noexcept
{
    if (impl_) {
        impl_->close();
    }
}

/**
 * @brief 将视频帧转换为模型输入张量。
 * @param frame 视频源输出的原始帧。
 * @return 成功返回 NCHW FP32 张量；失败返回空。
 */
std::optional<TensorBuffer> DvppFramePreprocessor::process(const Frame& frame)
{
    auto tensor = impl_->process(frame);
    last_error_ = impl_->last_error();
    return tensor;
}

/**
 * @brief 返回预处理策略类型标识。
 * @return 固定返回 `"dvpp"`。
 */
std::string_view DvppFramePreprocessor::kind() const noexcept
{
    return "dvpp";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view DvppFramePreprocessor::last_error() const noexcept
{
    return last_error_;
}

} // namespace sentinel
