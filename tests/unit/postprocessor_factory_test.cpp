#include "sentinel/postprocess/postprocessor_factory.hpp"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

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

/**
 * @brief 将 FP32 数组打包为模型输出缓冲区。
 * @param values 浮点值列表。
 * @return 模型输出缓冲区。
 */
sentinel::ModelOutputBuffer make_output(const std::vector<float>& values)
{
    sentinel::ModelOutputBuffer output;
    output.dtype = "FP32";
    output.index = 0;
    output.data.resize(values.size() * sizeof(float));
    std::memcpy(output.data.data(), values.data(), output.data.size());
    return output;
}

/**
 * @brief 写入 channels_first 布局下的单个属性。
 * @param values 输出浮点数组。
 * @param candidate_count 候选框数量。
 * @param candidate_index 候选框下标。
 * @param attribute_index 属性下标。
 * @param value 待写入值。
 */
void set_channel_first(std::vector<float>& values,
                       std::size_t candidate_count,
                       std::size_t candidate_index,
                       std::size_t attribute_index,
                       float value)
{
    values[attribute_index * candidate_count + candidate_index] = value;
}

} // namespace

/**
 * @brief 验证后处理策略工厂和 DVPP 主线 YOLO 后处理基础行为。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    sentinel::PostprocessConfig config;
    config.backend = "dvpp";
    config.model_type = "yolo";
    config.output_layout = "channels_first";
    config.num_classes = 2;
    config.confidence_threshold = 0.25;
    config.nms_iou_threshold = 0.45;
    config.max_detections = 10;
    config.class_names = {"person", "vehicle"};

    sentinel::RuleConfig rules;
    rules.target_classes = {"person", "vehicle"};
    rules.min_confidence = 0.5;

    auto postprocessor = sentinel::create_detection_postprocessor(config, rules);
    expect(postprocessor->kind() == "dvpp", "factory should create DVPP postprocessor");
    expect(postprocessor->open(), "DVPP postprocessor should open");

    constexpr std::size_t candidate_count = 3;
    constexpr std::size_t attributes = 6;
    std::vector<float> values(candidate_count * attributes, 0.0F);

    set_channel_first(values, candidate_count, 0, 0, 320.0F);
    set_channel_first(values, candidate_count, 0, 1, 320.0F);
    set_channel_first(values, candidate_count, 0, 2, 100.0F);
    set_channel_first(values, candidate_count, 0, 3, 100.0F);
    set_channel_first(values, candidate_count, 0, 4, 0.90F);
    set_channel_first(values, candidate_count, 0, 5, 0.10F);

    set_channel_first(values, candidate_count, 1, 0, 325.0F);
    set_channel_first(values, candidate_count, 1, 1, 325.0F);
    set_channel_first(values, candidate_count, 1, 2, 100.0F);
    set_channel_first(values, candidate_count, 1, 3, 100.0F);
    set_channel_first(values, candidate_count, 1, 4, 0.80F);
    set_channel_first(values, candidate_count, 1, 5, 0.20F);

    set_channel_first(values, candidate_count, 2, 0, 120.0F);
    set_channel_first(values, candidate_count, 2, 1, 120.0F);
    set_channel_first(values, candidate_count, 2, 2, 60.0F);
    set_channel_first(values, candidate_count, 2, 3, 60.0F);
    set_channel_first(values, candidate_count, 2, 4, 0.10F);
    set_channel_first(values, candidate_count, 2, 5, 0.70F);

    sentinel::TensorBuffer input;
    input.shape = {1, 3, 640, 640};
    input.frame_sequence = 42;
    input.camera_id = "unit-camera";

    const auto output = make_output(values);
    const auto detections = postprocessor->process({output}, input);
    expect(detections.size() == 2U, "NMS should keep one person and one vehicle detection");
    expect(detections.front().frame_sequence == 42, "detection should preserve frame sequence");
    expect(detections.front().camera_id == "unit-camera", "detection should preserve camera id");
    expect(!postprocessor->debug_info().empty(), "postprocessor should expose debug summary");
    postprocessor->close();

    bool unsupported_thrown = false;
    try {
        config.backend = "opencv";
        static_cast<void>(sentinel::create_detection_postprocessor(config, rules));
    } catch (const std::exception&) {
        unsupported_thrown = true;
    }
    expect(unsupported_thrown, "non-DVPP postprocess backend should throw");

    return 0;
}
