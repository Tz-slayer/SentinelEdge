#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/inference/detector.hpp"

#include <memory>

namespace sentinel {

/**
 * @brief 根据推理配置创建目标检测策略对象。
 * @param inference 推理后端配置。
 * @param rules 检测规则配置。
 * @return 新创建的目标检测策略对象。
 */
std::unique_ptr<Detector> create_detector(const InferenceConfig& inference, const RuleConfig& rules);

} // namespace sentinel
