#include "sentinel/inference/detector_factory.hpp"

#include "sentinel/inference/ascendcl_detector.hpp"
#include "sentinel/inference/mock_detector.hpp"

#include <memory>
#include <stdexcept>

namespace sentinel {

/**
 * @brief 根据推理后端配置创建检测器策略。
 * @param inference 推理后端配置。
 * @param rules 检测规则配置。
 * @return 新创建的检测器策略对象。
 */
std::unique_ptr<Detector> create_detector(const InferenceConfig& inference, const RuleConfig& rules)
{
#if defined(SENTINEL_ENABLE_MOCK_SOURCES) && SENTINEL_ENABLE_MOCK_SOURCES
    if (inference.backend == "mock") {
        return std::make_unique<MockDetector>(rules);
    }
#endif
    if (inference.backend == "ascendcl") {
        return std::make_unique<AscendClDetector>(inference, rules);
    }

    throw std::runtime_error("unsupported inference backend: " + inference.backend);
}

} // namespace sentinel
