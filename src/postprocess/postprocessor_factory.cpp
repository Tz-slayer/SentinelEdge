#include "sentinel/postprocess/postprocessor_factory.hpp"

#include "sentinel/postprocess/dvpp_yolo_postprocessor.hpp"
#include "sentinel/postprocess/opencv_yolo_postprocessor.hpp"

#include <memory>
#include <stdexcept>

namespace sentinel {

/**
 * @brief 根据配置创建检测后处理策略。
 * @param config 后处理运行配置。
 * @param rules 检测过滤规则。
 * @return 新创建的后处理策略对象。
 */
std::unique_ptr<DetectionPostprocessor> create_detection_postprocessor(
    const PostprocessConfig& config,
    const RuleConfig& rules)
{
    if (config.backend == "opencv") {
        return std::make_unique<OpenCvYoloPostprocessor>(config, rules);
    }
    if (config.backend == "dvpp") {
        return std::make_unique<DvppYoloPostprocessor>(config, rules);
    }

    throw std::runtime_error("unsupported postprocess backend: " + config.backend);
}

} // namespace sentinel
