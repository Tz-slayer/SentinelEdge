#include "sentinel/preprocess/dvpp_frame_preprocessor.hpp"

#include "sentinel/ascend/acl_runtime.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>

#include <linux/videodev2.h>

#if SENTINEL_ENABLE_DVPP
#include <acl/acl.h>
#include <acl/acl_rt.h>
#include <acl/ops/acl_dvpp.h>
#endif

namespace sentinel {
namespace {

/**
 * @brief 判断预处理配置是否请求输出 AIPP 可消费的 NV12 张量。
 * @param config 预处理配置。
 * @return 请求 NV12/UINT8 输出返回 `true`。
 */
bool wants_nv12_tensor(const PreprocessConfig& config) noexcept
{
    return config.output_layout == "NV12" && config.output_dtype == "UINT8";
}

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
     * @brief 确保当前 Device 内存容量至少为指定大小。
     * @param size 需要的最小字节数。
     * @param error 失败时写入错误文本。
     * @return 成功返回 `true`。
     *
     * 该函数用于按帧处理中的缓冲区复用。已有容量足够时不会重新
     * 调用 `aclrtMalloc`，从而减少 DVPP 主链路上的内存申请释放成本。
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

/**
 * @brief DVPP 缩放配置 RAII 包装。
 */
class DvppResizeConfig {
public:
    /**
     * @brief 创建 DVPP 缩放配置。
     */
    DvppResizeConfig()
        : config_(acldvppCreateResizeConfig())
    {
    }

    /**
     * @brief 销毁 DVPP 缩放配置。
     */
    ~DvppResizeConfig()
    {
        if (config_ != nullptr) {
            acldvppDestroyResizeConfig(config_);
        }
    }

    DvppResizeConfig(const DvppResizeConfig&) = delete;
    DvppResizeConfig& operator=(const DvppResizeConfig&) = delete;

    /**
     * @brief 判断缩放配置是否创建成功。
     * @return 成功返回 `true`。
     */
    bool valid() const noexcept
    {
        return config_ != nullptr;
    }

    /**
     * @brief 返回底层 DVPP 缩放配置。
     * @return 缩放配置指针。
     */
    acldvppResizeConfig* get() const noexcept
    {
        return config_;
    }

private:
    acldvppResizeConfig* config_{nullptr};
};

/**
 * @brief 在释放异步任务关联缓冲区前等待 stream 清空。
 * @param stream DVPP 异步任务使用的 AscendCL stream。
 * @return 无返回值；清理路径不覆盖主错误信息。
 */
void synchronize_stream_before_releasing_async_buffers(aclrtStream stream) noexcept
{
    if (stream != nullptr) {
        static_cast<void>(aclrtSynchronizeStream(stream));
    }
}

#endif

} // namespace

/**
 * @brief DVPP 预处理真实实现。
 *
 * 该实现持有可复用的 JPEGD 输出 Device buffer、Host fallback 的 VPC 输出
 * Device buffer、DVPP 图片描述和 resize 配置。`process_into()` 的静态 AIPP
 * 路径会把 VPC 输出直接写入 AscendCL 模型输入 Device buffer。
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
        if (!wants_nv12_tensor(config_)) {
            last_error_ = "DVPP preprocessor supports only NV12/UINT8 output";
            return false;
        }
        if (config_.normalize) {
            last_error_ = "NV12/UINT8 output requires normalize=false";
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

        decode_desc_ = std::make_unique<DvppPicDesc>();
        host_resize_desc_ = std::make_unique<DvppPicDesc>();
        device_resize_desc_ = std::make_unique<DvppPicDesc>();
        resize_config_ = std::make_unique<DvppResizeConfig>();
        if (!decode_desc_->valid() || !host_resize_desc_->valid() ||
            !device_resize_desc_->valid() || !resize_config_->valid()) {
            last_error_ = "failed to create reusable DVPP descriptors";
            close();
            return false;
        }

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
        decode_desc_.reset();
        host_resize_desc_.reset();
        device_resize_desc_.reset();
        resize_config_.reset();
        decode_output_.reset();
        host_resize_output_.reset();
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
            last_error_ = "DVPP preprocessor currently supports only MJPEG frames";
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

        if (!decode_output_.ensure_size(decode_size, last_error_)) {
            return std::nullopt;
        }

        const auto decode_width_stride = align_up(source_width, 128U);
        const auto decode_height_stride = align_up(source_height, 16U);
        if (decode_desc_ == nullptr || !decode_desc_->valid() ||
            !setup_pic_desc(decode_desc_->get(),
                            decode_output_.data(),
                            decode_size,
                            source_width,
                            source_height,
                            decode_width_stride,
                            decode_height_stride)) {
            return std::nullopt;
        }

        const auto output_width = static_cast<std::uint32_t>(config_.output_width);
        const auto output_height = static_cast<std::uint32_t>(config_.output_height);
        const auto output_width_stride = align_up(output_width, 16U);
        const auto output_height_stride = align_up(output_height, 2U);
        const auto resize_size = yuv420sp_size(output_width_stride, output_height_stride);

        if (!host_resize_output_.ensure_size(resize_size, last_error_)) {
            return std::nullopt;
        }

        if (host_resize_desc_ == nullptr || !host_resize_desc_->valid() ||
            !setup_pic_desc(host_resize_desc_->get(),
                            host_resize_output_.data(),
                            resize_size,
                            output_width,
                            output_height,
                            output_width_stride,
                            output_height_stride)) {
            return std::nullopt;
        }

        if (resize_config_ == nullptr || !resize_config_->valid()) {
            last_error_ = "acldvppCreateResizeConfig failed";
            return std::nullopt;
        }

        // 从这里开始会把异步任务压入 stream，后续失败路径必须先同步再释放 Device buffer。
        ret = acldvppJpegDecodeAsync(channel_desc_,
                                     payload,
                                     static_cast<std::uint32_t>(payload_size),
                                     decode_desc_->get(),
                                     stream_);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("acldvppJpegDecodeAsync", static_cast<int>(ret));
            return std::nullopt;
        }

        ret = acldvppVpcResizeAsync(channel_desc_,
                                    decode_desc_->get(),
                                    host_resize_desc_->get(),
                                    resize_config_->get(),
                                    stream_);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("acldvppVpcResizeAsync", static_cast<int>(ret));
            synchronize_stream_before_releasing_async_buffers(stream_);
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
                          host_resize_output_.data(),
                          resize_size,
                          ACL_MEMCPY_DEVICE_TO_HOST);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("aclrtMemcpy(resize_output)", static_cast<int>(ret));
            return std::nullopt;
        }

        TensorBuffer tensor;
        // 静态 AIPP 模型直接消费 NV12/YUV420SP_U8，避免 CPU 侧 RGB/NCHW/FP32 打包。
        tensor.data = std::move(host_nv12);
        tensor.shape = {1, 1, config_.output_height, config_.output_width};
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
     * @brief 将 MJPEG 帧通过 DVPP 直接写入目标 Device 张量。
     * @param frame 视频源输出的原始帧。
     * @param target 目标张量，必须是 Device 侧 NV12/UINT8 输入缓冲区。
     * @return 成功返回已写入的目标张量；失败返回空。
     */
    std::optional<TensorBuffer> process_into(const Frame& frame, TensorBuffer target)
    {
#if SENTINEL_ENABLE_DVPP
        if (!target.is_device()) {
            return process(frame);
        }
        if (target.device_data == nullptr || target.device_bytes == 0U) {
            last_error_ = "target device tensor is empty";
            return std::nullopt;
        }
        if (channel_desc_ == nullptr || stream_ == nullptr) {
            last_error_ = "DVPP preprocessor is not open";
            return std::nullopt;
        }
        if (!is_mjpeg_frame(frame)) {
            last_error_ = "DVPP device preprocessor currently supports only MJPEG frames";
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

        if (!decode_output_.ensure_size(decode_size, last_error_)) {
            return std::nullopt;
        }

        const auto decode_width_stride = align_up(source_width, 128U);
        const auto decode_height_stride = align_up(source_height, 16U);
        if (decode_desc_ == nullptr || !decode_desc_->valid() ||
            !setup_pic_desc(decode_desc_->get(),
                            decode_output_.data(),
                            decode_size,
                            source_width,
                            source_height,
                            decode_width_stride,
                            decode_height_stride)) {
            return std::nullopt;
        }

        const auto output_width = static_cast<std::uint32_t>(config_.output_width);
        const auto output_height = static_cast<std::uint32_t>(config_.output_height);
        const auto output_width_stride = align_up(output_width, 16U);
        const auto output_height_stride = align_up(output_height, 2U);
        const auto resize_size = yuv420sp_size(output_width_stride, output_height_stride);
        if (target.device_bytes != resize_size ||
            resize_size > std::numeric_limits<std::uint32_t>::max()) {
            last_error_ = "target device tensor bytes mismatch for NV12 resize output: expected " +
                          std::to_string(resize_size) + ", got " +
                          std::to_string(target.device_bytes);
            return std::nullopt;
        }

        if (device_resize_desc_ == nullptr || !device_resize_desc_->valid() ||
            !setup_pic_desc(device_resize_desc_->get(),
                            target.device_data,
                            static_cast<std::uint32_t>(target.device_bytes),
                            output_width,
                            output_height,
                            output_width_stride,
                            output_height_stride)) {
            return std::nullopt;
        }

        if (resize_config_ == nullptr || !resize_config_->valid()) {
            last_error_ = "acldvppCreateResizeConfig failed";
            return std::nullopt;
        }

        // 从这里开始会把异步任务压入 stream，后续失败路径必须先同步再释放 Device buffer。
        ret = acldvppJpegDecodeAsync(channel_desc_,
                                     payload,
                                     static_cast<std::uint32_t>(payload_size),
                                     decode_desc_->get(),
                                     stream_);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("acldvppJpegDecodeAsync", static_cast<int>(ret));
            return std::nullopt;
        }

        ret = acldvppVpcResizeAsync(channel_desc_,
                                    decode_desc_->get(),
                                    device_resize_desc_->get(),
                                    resize_config_->get(),
                                    stream_);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("acldvppVpcResizeAsync", static_cast<int>(ret));
            synchronize_stream_before_releasing_async_buffers(stream_);
            return std::nullopt;
        }

        ret = aclrtSynchronizeStream(stream_);
        if (!acl_ok(ret)) {
            last_error_ = make_acl_error("aclrtSynchronizeStream", static_cast<int>(ret));
            return std::nullopt;
        }

        target.shape = {1, 1, config_.output_height, config_.output_width};
        target.layout = config_.output_layout;
        target.dtype = config_.output_dtype;
        target.frame_sequence = frame.sequence;
        target.camera_id = frame.camera_id;
        last_error_.clear();
        return target;
#else
        static_cast<void>(frame);
        static_cast<void>(target);
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
    DeviceMemory decode_output_;
    DeviceMemory host_resize_output_;
    std::unique_ptr<DvppPicDesc> decode_desc_;
    std::unique_ptr<DvppPicDesc> host_resize_desc_;
    std::unique_ptr<DvppPicDesc> device_resize_desc_;
    std::unique_ptr<DvppResizeConfig> resize_config_;
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
 * @return 成功返回配置指定格式的张量；失败返回空。
 */
std::optional<TensorBuffer> DvppFramePreprocessor::process(const Frame& frame)
{
    auto tensor = impl_->process(frame);
    last_error_ = impl_->last_error();
    return tensor;
}

/**
 * @brief 将视频帧转换并写入目标张量。
 * @param frame 视频源输出的原始帧。
 * @param target 目标张量，静态 AIPP 路径下可为 Device 输入缓冲区。
 * @return 成功返回已写入的张量；失败返回空。
 */
std::optional<TensorBuffer> DvppFramePreprocessor::process_into(const Frame& frame,
                                                                TensorBuffer target)
{
    auto tensor = impl_->process_into(frame, std::move(target));
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
