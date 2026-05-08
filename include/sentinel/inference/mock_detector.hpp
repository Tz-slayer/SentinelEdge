#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/inference/detector.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace sentinel {

/**
 * @brief 为流水线开发和测试生成确定性的模拟检测结果。
 */
class MockDetector final : public Detector {
public:
    /**
     * @brief 使用规则配置构造模拟检测器。
     * @param rules 目标类别过滤和置信度阈值配置。
     */
    explicit MockDetector(RuleConfig rules);

    /**
     * @brief 初始化模拟检测器。
     * @return 固定返回 `true`。
     */
    bool open() override;

    /**
     * @brief 关闭模拟检测器。
     */
    void close() noexcept override;

    /**
     * @brief 为一帧数据生成模拟检测结果。
     * @param frame 用于派生固定检测结果的帧元数据。
     * @return 当前帧的模拟检测结果列表。
     */
    std::vector<Detection> detect(const Frame& frame) override;

    /**
     * @brief 返回检测器类型标识。
     * @return 固定返回 `"mock"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误文本。
     * @return 当前实现始终返回空字符串。
     */
    std::string_view last_error() const noexcept override;

private:
    /**
     * @brief 判断某个类别是否在规则配置中被启用。
     * @param label 待判断的检测类别名称。
     * @return 若该类别允许被模拟检测器输出则返回 `true`。
     */
    bool is_target_class(std::string_view label) const;

    RuleConfig rules_;
    std::string last_error_;
};

} // namespace sentinel
