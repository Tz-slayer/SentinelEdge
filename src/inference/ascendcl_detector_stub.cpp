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
     */
    Impl(InferenceConfig config, RuleConfig rules)
        : config_(std::move(config))
        , rules_(std::move(rules))
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
     * @param frame 待推理的视频帧。
     * @return 固定返回空检测列表。
     */
    std::vector<Detection> detect(const Frame& frame)
    {
        static_cast<void>(frame);
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

private:
    InferenceConfig config_;
    RuleConfig rules_;
    std::string last_error_;
};

/**
 * @brief 构造 AscendCL 检测器占位对象。
 * @param config 推理后端配置。
 * @param rules 检测过滤规则。
 */
AscendClDetector::AscendClDetector(InferenceConfig config, RuleConfig rules)
    : impl_(std::make_unique<Impl>(std::move(config), std::move(rules)))
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
 * @param frame 待推理的视频帧。
 * @return 固定返回空检测列表。
 */
std::vector<Detection> AscendClDetector::detect(const Frame& frame)
{
    return impl_->detect(frame);
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

} // namespace sentinel
