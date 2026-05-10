#include "sentinel/inference/ascendcl_detector.hpp"

#include "sentinel/ascend/acl_runtime.hpp"
#include "sentinel/postprocess/postprocessor_factory.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

#include <acl/acl.h>
#include <acl/acl_mdl.h>
#include <acl/acl_rt.h>

namespace sentinel {
namespace {

/**
 * @brief 第一版固定启用的 AscendCL 异步 stream slot 数量。
 */
constexpr std::size_t kAsyncSlotCount = 2;

/**
 * @brief 判断 AscendCL 调用是否成功。
 * @param error AscendCL API 返回码。
 * @return 成功返回 `true`。
 */
bool acl_ok(aclError error) noexcept
{
    return error == ACL_SUCCESS;
}

/**
 * @brief 将字节输出按 FP32 解释并生成少量预览值。
 * @param output 模型输出的主机侧字节缓冲区。
 * @param max_values 最多预览的浮点数数量。
 * @return 形如 `[0.1,0.2]` 的预览文本。
 */
std::string preview_fp32_values(const std::vector<std::uint8_t>& output, std::size_t max_values)
{
    const auto value_count = output.size() / sizeof(float);
    const auto preview_count = std::min(value_count, max_values);

    std::string preview{"["};
    for (std::size_t index = 0; index < preview_count; ++index) {
        float value = 0.0F;
        std::memcpy(&value, output.data() + index * sizeof(float), sizeof(float));
        if (index > 0U) {
            preview += ",";
        }
        preview += std::to_string(value);
    }
    if (value_count > preview_count) {
        preview += ",...";
    }
    preview += "]";
    return preview;
}

} // namespace

/**
 * @brief AscendCL Device 内存缓冲区。
 */
struct DeviceBuffer {
    void* data{nullptr};
    std::size_t size{0};
};

/**
 * @brief AscendCL 异步推理 slot。
 *
 * 每个 slot 独占一条 stream、一套模型输入 Device buffer 和一套模型输出
 * Device buffer，避免多 stream 并发时互相覆盖模型输入或输出。
 */
struct InferenceSlot {
    aclrtStream stream{nullptr};
    aclmdlDataset* input_dataset{nullptr};
    aclmdlDataset* output_dataset{nullptr};
    std::vector<DeviceBuffer> input_buffers;
    std::vector<DeviceBuffer> output_buffers;
    TensorBuffer input_tensor;
    bool busy{false};
};

/**
 * @brief AscendCL 真实推理实现。
 */
class AscendClDetector::Impl {
public:
    /**
     * @brief 保存推理配置和检测规则。
     * @param config 推理后端配置。
     * @param rules 检测过滤规则。
     * @param postprocess 后处理配置。
     */
    Impl(InferenceConfig config, RuleConfig rules, PostprocessConfig postprocess)
        : config_(std::move(config))
        , rules_(std::move(rules))
        , postprocess_config_(std::move(postprocess))
    {
    }

    /**
     * @brief 释放 AscendCL 资源。
     */
    ~Impl()
    {
        close();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    /**
     * @brief 初始化 AscendCL、加载模型并准备数据集。
     * @return 成功返回 `true`。
     */
    bool open()
    {
        close();

        runtime_session_ = AclRuntimeSession::acquire(config_.device_id, last_error_);
        if (!runtime_session_) {
            return false;
        }

        auto ret = aclmdlLoadFromFile(config_.model_path.c_str(), &model_id_);
        if (!acl_ok(ret)) {
            set_error("aclmdlLoadFromFile", ret);
            close();
            return false;
        }
        model_loaded_ = true;

        model_desc_ = aclmdlCreateDesc();
        if (model_desc_ == nullptr) {
            last_error_ = "aclmdlCreateDesc failed";
            close();
            return false;
        }

        ret = aclmdlGetDesc(model_desc_, model_id_);
        if (!acl_ok(ret)) {
            set_error("aclmdlGetDesc", ret);
            close();
            return false;
        }

        if (!create_slots()) {
            close();
            return false;
        }

        postprocessor_ = create_detection_postprocessor(postprocess_config_, rules_);
        if (!postprocessor_->open()) {
            last_error_ = "unable to open " + std::string(postprocessor_->kind()) +
                          " postprocessor: " + std::string(postprocessor_->last_error());
            close();
            return false;
        }

        last_error_.clear();
        return true;
    }

    /**
     * @brief 释放 AscendCL 模型、数据集、Device 内存和上下文。
     */
    void close() noexcept
    {
        for (auto& slot : slots_) {
            destroy_slot(slot);
        }
        slots_.clear();
        if (postprocessor_) {
            postprocessor_->close();
            postprocessor_.reset();
        }

        if (model_desc_ != nullptr) {
            aclmdlDestroyDesc(model_desc_);
            model_desc_ = nullptr;
        }

        if (model_loaded_) {
            aclmdlUnload(model_id_);
            model_loaded_ = false;
            model_id_ = 0;
        }

        runtime_session_.reset();
    }

    /**
     * @brief 执行一次同步推理。
     * @param tensor 预处理后的模型输入张量。
     * @return 当前阶段暂不解析 YOLO 输出，成功推理后返回空列表。
     */
    std::vector<Detection> detect(const TensorBuffer& tensor)
    {
        if (slots_.empty()) {
            last_error_ = "AscendCL detector has no initialized stream slots";
            debug_info_ = "detect failed before execute: " + last_error_;
            return {};
        }

        auto& slot = slots_.front();
        if (slot.input_buffers.size() != 1U) {
            last_error_ = "AscendCL detector currently supports exactly one model input";
            debug_info_ = "detect failed before execute: " + last_error_;
            return {};
        }

        if (!copy_input_to_slot(slot, tensor)) {
            debug_info_ = "detect failed while preparing input: " + last_error_;
            return {};
        }

        const auto ret = aclmdlExecute(model_id_, slot.input_dataset, slot.output_dataset);
        if (!acl_ok(ret)) {
            set_error("aclmdlExecute", ret);
            debug_info_ = "detect failed during model execute: " + last_error_;
            return {};
        }

        auto detections = collect_slot_outputs(slot, tensor);
        if (!last_error_.empty()) {
            return {};
        }
        return detections;
    }

    /**
     * @brief 返回模型输入 Device buffer 视图。
     * @param metadata 预处理阶段保留的张量元数据。
     * @return 单输入模型返回 Device 张量视图，否则返回空。
     */
    std::optional<TensorBuffer> mutable_input_tensor(const TensorBuffer& metadata)
    {
        return mutable_input_tensor_for_slot(metadata, 0);
    }

    /**
     * @brief 返回异步推理 slot 数量。
     * @return 当前固定返回 2。
     */
    std::size_t async_slot_count() const noexcept
    {
        return slots_.size();
    }

    /**
     * @brief 返回指定 slot 的模型输入 Device buffer 视图。
     * @param metadata 预处理阶段保留的张量元数据。
     * @param slot_index 异步 slot 下标。
     * @return slot 空闲且输入 buffer 可用时返回 Device 张量视图。
     */
    std::optional<TensorBuffer> mutable_input_tensor_for_slot(const TensorBuffer& metadata,
                                                              std::size_t slot_index)
    {
        if (slot_index >= slots_.size()) {
            return std::nullopt;
        }
        const auto& slot = slots_[slot_index];
        if (slot.busy || slot.input_buffers.size() != 1U ||
            slot.input_buffers.front().data == nullptr ||
            slot.input_buffers.front().size == 0U) {
            return std::nullopt;
        }

        TensorBuffer tensor = metadata;
        tensor.memory_location = TensorMemoryLocation::kDevice;
        tensor.data.clear();
        tensor.device_data = slot.input_buffers.front().data;
        tensor.device_bytes = slot.input_buffers.front().size;
        return tensor;
    }

    /**
     * @brief 返回指定 slot 的 AscendCL stream 句柄。
     * @param slot_index 异步 slot 下标。
     * @return slot 存在时返回 stream，不存在返回空指针。
     */
    void* native_stream_for_slot(std::size_t slot_index) noexcept
    {
        if (slot_index >= slots_.size()) {
            return nullptr;
        }
        return slots_[slot_index].stream;
    }

    /**
     * @brief 向指定 slot 提交异步推理。
     * @param slot_index 异步 slot 下标。
     * @param tensor 已完成预处理的输入张量。
     * @return 成功提交返回 `true`。
     */
    bool submit_async(std::size_t slot_index, const TensorBuffer& tensor)
    {
        if (slot_index >= slots_.size()) {
            last_error_ = "async slot index out of range: " + std::to_string(slot_index);
            debug_info_ = "submit_async failed before execute: " + last_error_;
            return false;
        }

        auto& slot = slots_[slot_index];
        if (slot.busy) {
            last_error_ = "async slot is busy: " + std::to_string(slot_index);
            debug_info_ = "submit_async failed before execute: " + last_error_;
            return false;
        }
        if (!copy_input_to_slot(slot, tensor)) {
            debug_info_ = "submit_async failed while preparing input: " + last_error_;
            return false;
        }

        const auto ret = aclmdlExecuteAsync(model_id_,
                                            slot.input_dataset,
                                            slot.output_dataset,
                                            slot.stream);
        if (!acl_ok(ret)) {
            set_error("aclmdlExecuteAsync", ret);
            debug_info_ = "submit_async failed during model execute: " + last_error_;
            return false;
        }

        slot.input_tensor = tensor;
        slot.busy = true;
        last_error_.clear();
        return true;
    }

    /**
     * @brief 同步并回收指定异步 slot 的推理结果。
     * @param slot_index 异步 slot 下标。
     * @return 成功返回异步检测结果。
     */
    std::optional<DetectorAsyncResult> collect_async(std::size_t slot_index)
    {
        if (slot_index >= slots_.size()) {
            last_error_ = "async slot index out of range: " + std::to_string(slot_index);
            return std::nullopt;
        }

        auto& slot = slots_[slot_index];
        if (!slot.busy) {
            last_error_ = "async slot is not busy: " + std::to_string(slot_index);
            return std::nullopt;
        }

        const auto ret = aclrtSynchronizeStream(slot.stream);
        if (!acl_ok(ret)) {
            set_error("aclrtSynchronizeStream(slot)", ret);
            slot.busy = false;
            return std::nullopt;
        }

        auto input_tensor = slot.input_tensor;
        auto detections = collect_slot_outputs(slot, input_tensor);
        const auto summary = debug_info_;
        slot.input_tensor = TensorBuffer{};
        slot.busy = false;
        if (!last_error_.empty()) {
            return std::nullopt;
        }

        return DetectorAsyncResult{
            slot_index,
            std::move(input_tensor),
            std::move(detections),
            summary,
        };
    }

    /**
     * @brief 返回最近一次错误文本。
     * @return 错误文本。
     */
    std::string_view last_error() const noexcept
    {
        return last_error_;
    }

    /**
     * @brief 返回最近一次推理的调试摘要。
     * @return 调试摘要文本。
     */
    std::string_view debug_info() const noexcept
    {
        return debug_info_;
    }

private:
    /**
     * @brief 将输入张量复制或确认到 slot 的模型输入 Device buffer。
     * @param slot 目标异步 slot。
     * @param tensor 输入张量。
     * @return 成功返回 `true`。
     */
    bool copy_input_to_slot(InferenceSlot& slot, const TensorBuffer& tensor)
    {
        if (slot.input_buffers.size() != 1U) {
            last_error_ = "AscendCL detector currently supports exactly one model input";
            return false;
        }

        if (tensor.byte_size() != slot.input_buffers.front().size) {
            last_error_ = "tensor input bytes mismatch: expected " +
                          std::to_string(slot.input_buffers.front().size) + ", got " +
                          std::to_string(tensor.byte_size());
            return false;
        }

        auto ret = ACL_SUCCESS;
        if (tensor.is_device()) {
            if (tensor.device_data == nullptr) {
                last_error_ = "device tensor input pointer is null";
                return false;
            }
            if (tensor.device_data != slot.input_buffers.front().data) {
                ret = aclrtMemcpy(slot.input_buffers.front().data,
                                  slot.input_buffers.front().size,
                                  tensor.device_data,
                                  tensor.device_bytes,
                                  ACL_MEMCPY_DEVICE_TO_DEVICE);
                if (!acl_ok(ret)) {
                    set_error("aclrtMemcpy(input device-to-device)", ret);
                    return false;
                }
            }
        } else {
            ret = aclrtMemcpy(slot.input_buffers.front().data,
                              slot.input_buffers.front().size,
                              tensor.data.data(),
                              tensor.data.size(),
                              ACL_MEMCPY_HOST_TO_DEVICE);
            if (!acl_ok(ret)) {
                set_error("aclrtMemcpy(input)", ret);
                return false;
            }
        }

        return true;
    }

    /**
     * @brief 从 slot 输出 Device buffer 拷回 Host 并执行后处理。
     * @param slot 已完成模型执行的 slot。
     * @param tensor 本次推理输入张量元数据。
     * @return 成功返回检测结果。
     */
    std::vector<Detection> collect_slot_outputs(InferenceSlot& slot, const TensorBuffer& tensor)
    {
        raw_outputs_.clear();
        raw_outputs_.reserve(slot.output_buffers.size());
        for (std::size_t index = 0; index < slot.output_buffers.size(); ++index) {
            const auto& output = slot.output_buffers[index];
            std::vector<std::uint8_t> host_output(output.size);
            const auto ret = aclrtMemcpy(host_output.data(),
                                         host_output.size(),
                                         output.data,
                                         output.size,
                                         ACL_MEMCPY_DEVICE_TO_HOST);
            if (!acl_ok(ret)) {
                set_error("aclrtMemcpy(output)", ret);
                debug_info_ = "detect failed while copying output: " + last_error_;
                return {};
            }
            raw_outputs_.push_back(ModelOutputBuffer{
                std::move(host_output),
                {},
                "FP32",
                index,
            });
        }

        if (!postprocessor_) {
            last_error_ = "postprocessor is not initialized";
            debug_info_ = "detect failed after execute: " + last_error_;
            return {};
        }

        auto detections = postprocessor_->process(raw_outputs_, tensor);
        if (!postprocessor_->last_error().empty()) {
            last_error_ = "postprocessor failed: " + std::string(postprocessor_->last_error());
            debug_info_ = make_debug_summary(tensor) + " " +
                          std::string(postprocessor_->debug_info());
            return {};
        }

        debug_info_ = make_debug_summary(tensor) + " " + std::string(postprocessor_->debug_info());
        last_error_.clear();
        return detections;
    }
    /**
     * @brief 生成一次 AscendCL 推理后的轻量调试摘要。
     * @param tensor 本次推理输入张量。
     * @return 包含输入和输出 buffer 概况的文本。
     */
    std::string make_debug_summary(const TensorBuffer& tensor) const
    {
        std::string summary = "ascendcl inference frame_sequence=" +
                              std::to_string(tensor.frame_sequence) +
                              " camera_id=" + tensor.camera_id +
                              " input_bytes=" + std::to_string(tensor.byte_size()) +
                              " input_memory=" + (tensor.is_device() ? "device" : "host") +
                              " outputs=" + std::to_string(raw_outputs_.size());

        for (std::size_t index = 0; index < raw_outputs_.size(); ++index) {
            const auto& output = raw_outputs_[index];
            summary += " output[" + std::to_string(index) + "].bytes=" +
                       std::to_string(output.data.size());
            if (!output.data.empty()) {
                summary += " fp32_preview=" + preview_fp32_values(output.data, 6);
            }
        }
        return summary;
    }

    /**
     * @brief 创建固定数量的异步推理 slot。
     * @return 成功返回 `true`。
     */
    bool create_slots()
    {
        slots_.resize(kAsyncSlotCount);
        for (auto& slot : slots_) {
            auto ret = aclrtCreateStream(&slot.stream);
            if (!acl_ok(ret)) {
                set_error("aclrtCreateStream(slot)", ret);
                return false;
            }

            slot.input_dataset = aclmdlCreateDataset();
            if (slot.input_dataset == nullptr) {
                last_error_ = "aclmdlCreateDataset(input) failed";
                return false;
            }

            slot.output_dataset = aclmdlCreateDataset();
            if (slot.output_dataset == nullptr) {
                last_error_ = "aclmdlCreateDataset(output) failed";
                return false;
            }

            if (!create_buffers_for_dataset(slot.input_dataset, slot.input_buffers, true)) {
                return false;
            }
            if (!create_buffers_for_dataset(slot.output_dataset, slot.output_buffers, false)) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief 为指定数据集创建 Device 缓冲区。
     * @param dataset AscendCL 数据集对象。
     * @param buffers 保存已创建 Device 缓冲区的列表。
     * @param input 若为 `true` 则按模型输入创建，否则按模型输出创建。
     * @return 成功返回 `true`。
     */
    bool create_buffers_for_dataset(aclmdlDataset* dataset,
                                    std::vector<DeviceBuffer>& buffers,
                                    bool input)
    {
        const auto count = input ? aclmdlGetNumInputs(model_desc_) : aclmdlGetNumOutputs(model_desc_);
        buffers.reserve(count);

        for (std::size_t index = 0; index < count; ++index) {
            const auto size = input ? aclmdlGetInputSizeByIndex(model_desc_, index)
                                    : aclmdlGetOutputSizeByIndex(model_desc_, index);
            void* device_buffer = nullptr;
            auto ret = aclrtMalloc(&device_buffer, size, ACL_MEM_MALLOC_HUGE_FIRST);
            if (!acl_ok(ret)) {
                set_error(input ? "aclrtMalloc(input)" : "aclrtMalloc(output)", ret);
                return false;
            }

            auto* data_buffer = aclCreateDataBuffer(device_buffer, size);
            if (data_buffer == nullptr) {
                aclrtFree(device_buffer);
                last_error_ = input ? "aclCreateDataBuffer(input) failed"
                                    : "aclCreateDataBuffer(output) failed";
                return false;
            }

            ret = aclmdlAddDatasetBuffer(dataset, data_buffer);
            if (!acl_ok(ret)) {
                aclDestroyDataBuffer(data_buffer);
                aclrtFree(device_buffer);
                set_error(input ? "aclmdlAddDatasetBuffer(input)" : "aclmdlAddDatasetBuffer(output)",
                          ret);
                return false;
            }

            buffers.push_back(DeviceBuffer{device_buffer, size});
        }

        return true;
    }

    /**
     * @brief 销毁数据集和关联 Device 缓冲区。
     * @param dataset 待销毁的数据集。
     * @param buffers 待释放的 Device 缓冲区列表。
     */
    static void destroy_dataset(aclmdlDataset*& dataset, std::vector<DeviceBuffer>& buffers) noexcept
    {
        if (dataset != nullptr) {
            const auto buffer_count = aclmdlGetDatasetNumBuffers(dataset);
            for (std::size_t index = 0; index < buffer_count; ++index) {
                auto* data_buffer = aclmdlGetDatasetBuffer(dataset, index);
                if (data_buffer != nullptr) {
                    aclDestroyDataBuffer(data_buffer);
                }
            }
            aclmdlDestroyDataset(dataset);
            dataset = nullptr;
        }

        for (const auto& buffer : buffers) {
            if (buffer.data != nullptr) {
                aclrtFree(buffer.data);
            }
        }
        buffers.clear();
    }

    /**
     * @brief 销毁单个异步推理 slot 及其独占资源。
     * @param slot 待销毁的 slot。
     */
    static void destroy_slot(InferenceSlot& slot) noexcept
    {
        if (slot.stream != nullptr) {
            aclrtSynchronizeStream(slot.stream);
        }
        destroy_dataset(slot.input_dataset, slot.input_buffers);
        destroy_dataset(slot.output_dataset, slot.output_buffers);
        if (slot.stream != nullptr) {
            aclrtDestroyStream(slot.stream);
            slot.stream = nullptr;
        }
        slot.input_tensor = TensorBuffer{};
        slot.busy = false;
    }

    /**
     * @brief 设置最近一次 AscendCL 错误文本。
     * @param action 失败的操作名称。
     * @param error AscendCL 返回码。
     */
    void set_error(std::string_view action, aclError error)
    {
        last_error_ = make_acl_error(action, static_cast<int>(error));
    }

    InferenceConfig config_;
    RuleConfig rules_;
    PostprocessConfig postprocess_config_;
    std::string last_error_;
    std::string debug_info_;
    std::vector<InferenceSlot> slots_;
    std::vector<ModelOutputBuffer> raw_outputs_;
    std::unique_ptr<DetectionPostprocessor> postprocessor_;
    std::unique_ptr<AclRuntimeSession> runtime_session_;
    aclmdlDesc* model_desc_{nullptr};
    uint32_t model_id_{0};
    bool model_loaded_{false};
};

/**
 * @brief 构造 AscendCL 检测器。
 * @param config 推理后端配置。
 * @param rules 检测过滤规则。
 */
AscendClDetector::AscendClDetector(InferenceConfig config,
                                   RuleConfig rules,
                                   PostprocessConfig postprocess)
    : impl_(std::make_unique<Impl>(std::move(config),
                                   std::move(rules),
                                   std::move(postprocess)))
{
}

/**
 * @brief 析构时释放 AscendCL 资源。
 */
AscendClDetector::~AscendClDetector() = default;

/**
 * @brief 初始化 AscendCL 检测器。
 * @return 成功返回 `true`。
 */
bool AscendClDetector::open()
{
    return impl_->open();
}

/**
 * @brief 释放 AscendCL 检测器资源。
 */
void AscendClDetector::close() noexcept
{
    impl_->close();
}

/**
 * @brief 执行一次 AscendCL 推理。
 * @param tensor 预处理后的模型输入张量。
 * @return 成功返回后处理后的检测结果列表；失败返回空列表。
 */
std::vector<Detection> AscendClDetector::detect(const TensorBuffer& tensor)
{
    return impl_->detect(tensor);
}

/**
 * @brief 返回 AscendCL 模型输入 Device buffer 视图。
 * @param metadata 预处理阶段生成的张量元数据。
 * @return 成功返回 Device 张量视图；当前模型输入不满足条件时返回空。
 */
std::optional<TensorBuffer> AscendClDetector::mutable_input_tensor(const TensorBuffer& metadata)
{
    return impl_->mutable_input_tensor(metadata);
}

/**
 * @brief 返回 AscendCL 异步推理 slot 数量。
 * @return 当前固定返回 2。
 */
std::size_t AscendClDetector::async_slot_count() const noexcept
{
    return impl_->async_slot_count();
}

/**
 * @brief 返回指定异步 slot 的模型输入 Device buffer 视图。
 * @param metadata 预处理阶段生成的张量元数据。
 * @param slot_index 异步 slot 下标。
 * @return 成功返回 Device 张量视图；slot 不可用时返回空。
 */
std::optional<TensorBuffer> AscendClDetector::mutable_input_tensor_for_slot(
    const TensorBuffer& metadata,
    std::size_t slot_index)
{
    return impl_->mutable_input_tensor_for_slot(metadata, slot_index);
}

/**
 * @brief 返回指定异步 slot 的 AscendCL stream。
 * @param slot_index 异步 slot 下标。
 * @return slot 存在时返回 `aclrtStream` 的不透明指针，否则返回空指针。
 */
void* AscendClDetector::native_stream_for_slot(std::size_t slot_index) noexcept
{
    return impl_->native_stream_for_slot(slot_index);
}

/**
 * @brief 向指定 AscendCL stream slot 提交异步推理。
 * @param slot_index 异步 slot 下标。
 * @param tensor 已完成预处理的模型输入张量。
 * @return 成功提交返回 `true`。
 */
bool AscendClDetector::submit_async(std::size_t slot_index, const TensorBuffer& tensor)
{
    return impl_->submit_async(slot_index, tensor);
}

/**
 * @brief 同步并回收指定 AscendCL stream slot 的推理结果。
 * @param slot_index 异步 slot 下标。
 * @return 成功返回检测结果；失败返回空。
 */
std::optional<DetectorAsyncResult> AscendClDetector::collect_async(std::size_t slot_index)
{
    return impl_->collect_async(slot_index);
}

/**
 * @brief 返回检测器后端类型。
 * @return 固定返回 `"ascendcl"`。
 */
std::string_view AscendClDetector::kind() const noexcept
{
    return "ascendcl";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view AscendClDetector::last_error() const noexcept
{
    return impl_->last_error();
}

/**
 * @brief 返回最近一次 AscendCL 推理的调试摘要。
 * @return 调试摘要文本。
 */
std::string_view AscendClDetector::debug_info() const noexcept
{
    return impl_->debug_info();
}

} // namespace sentinel
