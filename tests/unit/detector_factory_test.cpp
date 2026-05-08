#include "sentinel/inference/detector_factory.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>

namespace {

/**
 * @brief 断言条件成立，否则输出错误并退出。
 * @param condition 待检查条件。
 * @param message 失败时输出的错误消息。
 */
void expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

/**
 * @brief 验证检测器工厂的基础策略选择逻辑。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    sentinel::InferenceConfig mock_config;
    mock_config.backend = "mock";

    sentinel::RuleConfig rules;
    sentinel::PostprocessConfig postprocess;
    const auto detector = sentinel::create_detector(mock_config, rules, postprocess);
    expect(detector != nullptr, "mock detector should be created");
    expect(detector->kind() == "mock", "mock detector should report mock kind");
    expect(detector->open(), "mock detector should open");

    sentinel::InferenceConfig ascend_config;
    ascend_config.backend = "ascendcl";
    ascend_config.model_path = "models/yolo/yolo26n.om";

    const auto ascend_detector = sentinel::create_detector(ascend_config, rules, postprocess);
    expect(ascend_detector != nullptr, "ascendcl detector strategy should be created");
    expect(ascend_detector->kind() == "ascendcl", "ascendcl detector should report ascendcl kind");

    sentinel::InferenceConfig invalid_config;
    invalid_config.backend = "unknown";

    bool invalid_backend_failed = false;
    try {
        static_cast<void>(sentinel::create_detector(invalid_config, rules, postprocess));
    } catch (const std::exception&) {
        invalid_backend_failed = true;
    }
    expect(invalid_backend_failed, "unknown detector backend should fail");

    return 0;
}
