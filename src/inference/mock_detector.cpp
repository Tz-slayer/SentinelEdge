#include "sentinel/inference/mock_detector.hpp"

#include <algorithm>

namespace sentinel {

/**
 * @brief 使用规则配置构造模拟检测器。
 * @param rules 目标类别过滤和置信度阈值配置。
 */
MockDetector::MockDetector(RuleConfig rules)
    : rules_(std::move(rules))
{
}

/**
 * @brief 初始化模拟检测器。
 * @return 固定返回 `true`。
 */
bool MockDetector::open()
{
    last_error_.clear();
    return true;
}

/**
 * @brief 关闭模拟检测器。
 */
void MockDetector::close() noexcept
{
}

/**
 * @brief 为一个输入张量生成稳定的模拟检测结果。
 * @param tensor 当前张量携带的来源帧元数据。
 * @return 模拟检测结果列表。
 */
std::vector<Detection> MockDetector::detect(const TensorBuffer& tensor)
{
    std::vector<Detection> detections;

    // 人员目标每一帧都出现，便于驱动事件聚合逻辑。
    if (is_target_class("person")) {
        detections.push_back(Detection{
            "person",
            0.91,
            Rect{0.22, 0.18, 0.20, 0.46},
            tensor.frame_sequence,
            tensor.camera_id,
        });
    }

    // 车辆目标按固定节奏出现，用于模拟间歇性检测结果。
    if (tensor.frame_sequence % 3 == 0 && is_target_class("vehicle")) {
        detections.push_back(Detection{
            "vehicle",
            0.86,
            Rect{0.52, 0.42, 0.34, 0.28},
            tensor.frame_sequence,
            tensor.camera_id,
        });
    }

    return detections;
}

/**
 * @brief 返回检测器类型标识。
 * @return 固定返回 `"mock"`。
 */
std::string_view MockDetector::kind() const noexcept
{
    return "mock";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 当前实现始终返回空字符串。
 */
std::string_view MockDetector::last_error() const noexcept
{
    return last_error_;
}

/**
 * @brief 判断某个类别是否允许被模拟检测器输出。
 * @param label 待判断的类别名称。
 * @return 若类别存在于规则配置中则返回 `true`。
 */
bool MockDetector::is_target_class(std::string_view label) const
{
    return std::find(rules_.target_classes.begin(), rules_.target_classes.end(), label) !=
           rules_.target_classes.end();
}

} // namespace sentinel
