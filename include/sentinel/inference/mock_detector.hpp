#pragma once

#include "sentinel/common/types.hpp"

#include <string_view>
#include <vector>

namespace sentinel {

/**
 * @brief 为流水线开发和测试生成确定性的模拟检测结果。
 */
class MockDetector {
public:
    /**
     * @brief 使用规则配置构造模拟检测器。
     * @param rules 目标类别过滤和置信度阈值配置。
     */
    explicit MockDetector(RuleConfig rules);

    /**
     * @brief 为一帧数据生成模拟检测结果。
     * @param frame 用于派生固定检测结果的帧元数据。
     * @return 当前帧的模拟检测结果列表。
     */
    std::vector<Detection> detect(const Frame& frame) const;

private:
    /**
     * @brief 判断某个类别是否在规则配置中被启用。
     * @param label 待判断的检测类别名称。
     * @return 若该类别允许被模拟检测器输出则返回 `true`。
     */
    bool is_target_class(std::string_view label) const;

    RuleConfig rules_;
};

} // namespace sentinel
