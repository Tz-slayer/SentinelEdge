#include "sentinel/inference/ascendcl_detector.hpp"

#include <string>
#include <utility>

namespace sentinel {

/**
 * @brief 未启用 AscendCL 编译选项时的占位实现。
 */
class AscendClDetector::Impl {
public:
    /**
     * @brief 保存推理配置和规则。
     * @param config 推理后端配置。
     * @param rules 检测过滤规则。
     * @param postprocess 后处理配置。
     */
    Impl(InferenceConfig config, RuleConfig rules, PostprocessConfig postprocess)
        : config_(std::move(config))
        , rules_(std::move(rules))
        , postprocess_(std::move(postprocess))
    {
    }

    /**
     * @brief 报告当前二进制未启用 AscendCL。
     * @return 固定返回 `false`。
     */
    bool open()
    {
        last_error_ = "AscendCL backend is not enabled at build time";
        return false;
    }

    /**
     * @brief 占位释放函数。
     */
    void close() noexcept
    {
    }

    /**
     * @brief 占位推理函数。
     * @param tensor 预处理后的模型输入张量。
     * @return 固定返回空检测列表。
     */
    std::vector<Detection> detect(const TensorBuffer& tensor)
    {
        static_cast<void>(tensor);
        last_error_ = "AscendCL backend is not enabled at build time";
        return {};
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
     * @brief 返回占位推理调试摘要。
     * @return 调试摘要文本。
     */
    std::string_view debug_info() const noexcept
    {
        return debug_info_;
    }

private:
    InferenceConfig config_;
    RuleConfig rules_;
    PostprocessConfig postprocess_;
    std::string last_error_;
    std::string debug_info_{"AscendCL backend is not enabled at build time"};
};

/**
 * @brief 构造 AscendCL 检测器占位对象。
 * @param config 推理后端配置。
 * @param rules 检测过滤规则。
 * @param postprocess 后处理配置。
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
 * @brief 释放占位对象。
 */
AscendClDetector::~AscendClDetector() = default;

/**
 * @brief 初始化占位对象。
 * @return 固定返回 `false`。
 */
bool AscendClDetector::open()
{
    return impl_->open();
}

/**
 * @brief 释放占位对象资源。
 */
void AscendClDetector::close() noexcept
{
    impl_->close();
}

/**
 * @brief 执行占位推理。
 * @param tensor 预处理后的模型输入张量。
 * @return 固定返回空检测列表。
 */
std::vector<Detection> AscendClDetector::detect(const TensorBuffer& tensor)
{
    return impl_->detect(tensor);
}

/**
 * @brief 占位实现不暴露模型输入缓冲区。
 * @param metadata 预处理阶段生成的张量元数据。
 * @return 固定返回空。
 */
std::optional<TensorBuffer> AscendClDetector::mutable_input_tensor(const TensorBuffer& metadata)
{
    static_cast<void>(metadata);
    return std::nullopt;
}

/**
 * @brief 占位实现不支持异步推理 slot。
 * @return 固定返回 0。
 */
std::size_t AscendClDetector::async_slot_count() const noexcept
{
    return 0;
}

/**
 * @brief 占位实现不暴露异步 slot 输入缓冲区。
 * @param metadata 预处理阶段生成的张量元数据。
 * @param slot_index 异步 slot 下标。
 * @return 固定返回空。
 */
std::optional<TensorBuffer> AscendClDetector::mutable_input_tensor_for_slot(
    const TensorBuffer& metadata,
    std::size_t slot_index)
{
    static_cast<void>(metadata);
    static_cast<void>(slot_index);
    return std::nullopt;
}

/**
 * @brief 占位实现不支持异步提交。
 * @param slot_index 异步 slot 下标。
 * @param tensor 输入张量。
 * @return 固定返回 `false`。
 */
bool AscendClDetector::submit_async(std::size_t slot_index, const TensorBuffer& tensor)
{
    static_cast<void>(slot_index);
    static_cast<void>(tensor);
    return false;
}

/**
 * @brief 占位实现不支持异步回收。
 * @param slot_index 异步 slot 下标。
 * @return 固定返回空。
 */
std::optional<DetectorAsyncResult> AscendClDetector::collect_async(std::size_t slot_index)
{
    static_cast<void>(slot_index);
    return std::nullopt;
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
 * @brief 返回占位推理调试摘要。
 * @return 调试摘要文本。
 */
std::string_view AscendClDetector::debug_info() const noexcept
{
    return impl_->debug_info();
}

} // namespace sentinel
