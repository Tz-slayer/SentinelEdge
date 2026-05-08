#include "sentinel/inference/ascendcl_detector.hpp"

#include "sentinel/postprocess/postprocessor_factory.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

#include <acl/acl.h>
#include <acl/acl_mdl.h>
#include <acl/acl_rt.h>

namespace sentinel {
namespace {

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
 * @brief 拼接 AscendCL 错误上下文。
 * @param action 失败的操作名称。
 * @param error AscendCL API 返回码。
 * @return 可读错误文本。
 */
std::string make_acl_error(std::string_view action, aclError error)
{
    return std::string(action) + " failed, aclError=" + std::to_string(static_cast<int>(error));
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

        auto ret = aclInit(nullptr);
        if (!acl_ok(ret)) {
            set_error("aclInit", ret);
            return false;
        }
        acl_initialized_ = true;

        ret = aclrtSetDevice(config_.device_id);
        if (!acl_ok(ret)) {
            set_error("aclrtSetDevice", ret);
            close();
            return false;
        }
        device_set_ = true;

        ret = aclrtCreateContext(&context_, config_.device_id);
        if (!acl_ok(ret)) {
            set_error("aclrtCreateContext", ret);
            close();
            return false;
        }

        ret = aclmdlLoadFromFile(config_.model_path.c_str(), &model_id_);
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

        if (!create_datasets()) {
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
        destroy_dataset(input_dataset_, input_buffers_);
        destroy_dataset(output_dataset_, output_buffers_);
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

        if (context_ != nullptr) {
            aclrtDestroyContext(context_);
            context_ = nullptr;
        }

        if (device_set_) {
            aclrtResetDevice(config_.device_id);
            device_set_ = false;
        }

        if (acl_initialized_) {
            aclFinalize();
            acl_initialized_ = false;
        }
    }

    /**
     * @brief 执行一次同步推理。
     * @param tensor 预处理后的模型输入张量。
     * @return 当前阶段暂不解析 YOLO 输出，成功推理后返回空列表。
     */
    std::vector<Detection> detect(const TensorBuffer& tensor)
    {
        if (input_buffers_.size() != 1U) {
            last_error_ = "AscendCL detector currently supports exactly one model input";
            debug_info_ = "detect failed before execute: " + last_error_;
            return {};
        }

        if (tensor.data.size() != input_buffers_.front().size) {
            last_error_ = "tensor input bytes mismatch: expected " +
                          std::to_string(input_buffers_.front().size) + ", got " +
                          std::to_string(tensor.data.size());
            debug_info_ = "detect failed before execute: " + last_error_;
            return {};
        }

        auto ret = aclrtMemcpy(input_buffers_.front().data,
                               input_buffers_.front().size,
                               tensor.data.data(),
                               tensor.data.size(),
                               ACL_MEMCPY_HOST_TO_DEVICE);
        if (!acl_ok(ret)) {
            set_error("aclrtMemcpy(input)", ret);
            debug_info_ = "detect failed while copying input: " + last_error_;
            return {};
        }

        ret = aclmdlExecute(model_id_, input_dataset_, output_dataset_);
        if (!acl_ok(ret)) {
            set_error("aclmdlExecute", ret);
            debug_info_ = "detect failed during model execute: " + last_error_;
            return {};
        }

        raw_outputs_.clear();
        raw_outputs_.reserve(output_buffers_.size());
        for (std::size_t index = 0; index < output_buffers_.size(); ++index) {
            const auto& output = output_buffers_[index];
            std::vector<std::uint8_t> host_output(output.size);
            ret = aclrtMemcpy(host_output.data(),
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
     * @brief 生成一次 AscendCL 推理后的轻量调试摘要。
     * @param tensor 本次推理输入张量。
     * @return 包含输入和输出 buffer 概况的文本。
     */
    std::string make_debug_summary(const TensorBuffer& tensor) const
    {
        std::string summary = "ascendcl inference frame_sequence=" +
                              std::to_string(tensor.frame_sequence) +
                              " camera_id=" + tensor.camera_id +
                              " input_bytes=" + std::to_string(tensor.data.size()) +
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
     * @brief 创建模型输入输出数据集。
     * @return 成功返回 `true`。
     */
    bool create_datasets()
    {
        input_dataset_ = aclmdlCreateDataset();
        if (input_dataset_ == nullptr) {
            last_error_ = "aclmdlCreateDataset(input) failed";
            return false;
        }

        output_dataset_ = aclmdlCreateDataset();
        if (output_dataset_ == nullptr) {
            last_error_ = "aclmdlCreateDataset(output) failed";
            return false;
        }

        if (!create_buffers_for_dataset(input_dataset_, input_buffers_, true)) {
            return false;
        }
        if (!create_buffers_for_dataset(output_dataset_, output_buffers_, false)) {
            return false;
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
     * @brief 设置最近一次 AscendCL 错误文本。
     * @param action 失败的操作名称。
     * @param error AscendCL 返回码。
     */
    void set_error(std::string_view action, aclError error)
    {
        last_error_ = make_acl_error(action, error);
    }

    InferenceConfig config_;
    RuleConfig rules_;
    PostprocessConfig postprocess_config_;
    std::string last_error_;
    std::string debug_info_;
    std::vector<DeviceBuffer> input_buffers_;
    std::vector<DeviceBuffer> output_buffers_;
    std::vector<ModelOutputBuffer> raw_outputs_;
    std::unique_ptr<DetectionPostprocessor> postprocessor_;
    aclrtContext context_{nullptr};
    aclmdlDesc* model_desc_{nullptr};
    aclmdlDataset* input_dataset_{nullptr};
    aclmdlDataset* output_dataset_{nullptr};
    uint32_t model_id_{0};
    bool acl_initialized_{false};
    bool device_set_{false};
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
 * @return 当前阶段暂不解析 YOLO 输出，成功推理后返回空列表。
 */
std::vector<Detection> AscendClDetector::detect(const TensorBuffer& tensor)
{
    return impl_->detect(tensor);
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
