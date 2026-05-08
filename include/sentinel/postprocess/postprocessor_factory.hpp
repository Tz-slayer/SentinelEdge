#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/postprocess/detection_postprocessor.hpp"

#include <memory>

namespace sentinel {

/**
 * @brief 根据配置创建检测后处理策略。
 * @param config 后处理运行配置。
 * @param rules 检测过滤规则。
 * @return 新创建的后处理策略对象。
 * @throws std::runtime_error 当后端名称不受支持时抛出。
 */
std::unique_ptr<DetectionPostprocessor> create_detection_postprocessor(
    const PostprocessConfig& config,
    const RuleConfig& rules);

} // namespace sentinel
