#include "sentinel/postprocess/opencv_yolo_postprocessor.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <opencv2/dnn/dnn.hpp>

namespace sentinel {
namespace {

/**
 * @brief 表示解析出的候选检测框。
 */
struct Candidate {
    Rect normalized_box;
    cv::Rect nms_box;
    std::string label;
    float confidence{0.0F};
    int class_id{0};
};

/**
 * @brief 返回内置 COCO 类别名称。
 * @return COCO 80 类名称列表。
 */
const std::vector<std::string>& default_coco_class_names()
{
    static const std::vector<std::string> names{
        "person",      "bicycle",    "car",          "motorcycle", "airplane",
        "bus",         "train",      "truck",        "boat",       "traffic light",
        "fire hydrant","stop sign",  "parking meter","bench",      "bird",
        "cat",         "dog",        "horse",        "sheep",      "cow",
        "elephant",    "bear",       "zebra",        "giraffe",    "backpack",
        "umbrella",    "handbag",    "tie",          "suitcase",   "frisbee",
        "skis",        "snowboard",  "sports ball",  "kite",       "baseball bat",
        "baseball glove","skateboard","surfboard",   "tennis racket","bottle",
        "wine glass",  "cup",        "fork",         "knife",      "spoon",
        "bowl",        "banana",     "apple",        "sandwich",   "orange",
        "broccoli",    "carrot",     "hot dog",      "pizza",      "donut",
        "cake",        "chair",      "couch",        "potted plant","bed",
        "dining table","toilet",     "tv",           "laptop",     "mouse",
        "remote",      "keyboard",   "cell phone",   "microwave",  "oven",
        "toaster",     "sink",       "refrigerator", "book",       "clock",
        "vase",        "scissors",   "teddy bear",   "hair drier", "toothbrush",
    };
    return names;
}

/**
 * @brief 判断类别是否属于 COCO 车辆集合。
 * @param label 类别名称。
 * @return 若是车辆相关类别则返回 `true`。
 */
bool is_coco_vehicle_label(const std::string& label)
{
    return label == "car" || label == "motorcycle" || label == "bus" || label == "truck";
}

/**
 * @brief 判断规则中是否包含某个目标类别。
 * @param rules 检测规则配置。
 * @param label 类别名称。
 * @return 若规则允许该类别则返回 `true`。
 */
bool is_target_label(const RuleConfig& rules, const std::string& label)
{
    return std::find(rules.target_classes.begin(), rules.target_classes.end(), label) !=
           rules.target_classes.end();
}

/**
 * @brief 根据类别编号解析标签。
 * @param config 后处理配置。
 * @param rules 检测规则配置。
 * @param class_id 模型输出类别编号。
 * @return 可读标签名称。
 */
std::string resolve_label(const PostprocessConfig& config, const RuleConfig& rules, int class_id)
{
    const auto& names = config.class_names.empty() ? default_coco_class_names() : config.class_names;
    std::string label = class_id >= 0 && static_cast<std::size_t>(class_id) < names.size()
                            ? names[static_cast<std::size_t>(class_id)]
                            : "class_" + std::to_string(class_id);

    if (config.merge_coco_vehicles && is_coco_vehicle_label(label) && is_target_label(rules, "vehicle")) {
        label = "vehicle";
    }
    return label;
}

/**
 * @brief 将输出字节解析为 FP32 数组。
 * @param output 模型输出缓冲区。
 * @return 浮点值列表。
 * @throws std::runtime_error 当字节数不是 FP32 整数倍时抛出。
 */
std::vector<float> output_to_floats(const ModelOutputBuffer& output)
{
    if (output.dtype != "FP32") {
        throw std::runtime_error("YOLO postprocessor supports only FP32 output dtype");
    }
    if (output.data.size() % sizeof(float) != 0U) {
        throw std::runtime_error("model output bytes are not aligned to FP32");
    }

    std::vector<float> values(output.data.size() / sizeof(float));
    if (!values.empty()) {
        std::memcpy(values.data(), output.data.data(), output.data.size());
    }
    return values;
}

/**
 * @brief 从输入张量形状中取出模型输入宽度。
 * @param tensor 模型输入张量。
 * @return 输入宽度，无法识别时返回 640。
 */
int input_width(const TensorBuffer& tensor)
{
    return tensor.shape.size() >= 4U ? std::max(tensor.shape[3], 1) : 640;
}

/**
 * @brief 从输入张量形状中取出模型输入高度。
 * @param tensor 模型输入张量。
 * @return 输入高度，无法识别时返回 640。
 */
int input_height(const TensorBuffer& tensor)
{
    return tensor.shape.size() >= 4U ? std::max(tensor.shape[2], 1) : 640;
}

/**
 * @brief 将 YOLO 坐标值转换为归一化坐标。
 * @param value 原始坐标值。
 * @param scale 输入图像对应边长。
 * @return 归一化后的值。
 */
double normalize_coordinate(float value, int scale)
{
    if (std::fabs(value) <= 2.0F) {
        return static_cast<double>(value);
    }
    return static_cast<double>(value) / static_cast<double>(std::max(scale, 1));
}

/**
 * @brief 将数值限制到 `[0, 1]`。
 * @param value 原始数值。
 * @return 限制后的数值。
 */
double clamp01(double value)
{
    return std::max(0.0, std::min(value, 1.0));
}

/**
 * @brief 判断浮点值是否可用于候选框。
 * @param value 待检查数值。
 * @return 若有限则返回 `true`。
 */
bool valid_number(float value)
{
    return std::isfinite(value);
}

/**
 * @brief 从候选值构造归一化检测框。
 * @param center_x 中心点 x。
 * @param center_y 中心点 y。
 * @param width 框宽。
 * @param height 框高。
 * @param image_width 模型输入宽度。
 * @param image_height 模型输入高度。
 * @return 归一化矩形框。
 */
Rect make_normalized_box(float center_x,
                         float center_y,
                         float width,
                         float height,
                         int image_width,
                         int image_height)
{
    const auto normalized_center_x = normalize_coordinate(center_x, image_width);
    const auto normalized_center_y = normalize_coordinate(center_y, image_height);
    const auto normalized_width = normalize_coordinate(width, image_width);
    const auto normalized_height = normalize_coordinate(height, image_height);

    const auto left = clamp01(normalized_center_x - normalized_width / 2.0);
    const auto top = clamp01(normalized_center_y - normalized_height / 2.0);
    const auto right = clamp01(normalized_center_x + normalized_width / 2.0);
    const auto bottom = clamp01(normalized_center_y + normalized_height / 2.0);

    return Rect{left, top, std::max(0.0, right - left), std::max(0.0, bottom - top)};
}

/**
 * @brief 将归一化框转换为 OpenCV NMS 使用的像素矩形。
 * @param box 归一化矩形框。
 * @param image_width 输入宽度。
 * @param image_height 输入高度。
 * @return 像素矩形。
 */
cv::Rect to_nms_rect(const Rect& box, int image_width, int image_height)
{
    return cv::Rect{
        static_cast<int>(std::round(box.x * image_width)),
        static_cast<int>(std::round(box.y * image_height)),
        std::max(1, static_cast<int>(std::round(box.width * image_width))),
        std::max(1, static_cast<int>(std::round(box.height * image_height))),
    };
}

/**
 * @brief 计算 YOLO 输出每个候选框的属性数量。
 * @param config 后处理配置。
 * @param value_count 输出浮点值总数。
 * @return 每个候选框属性数量，无法推断时返回 0。
 */
int infer_attributes_per_candidate(const PostprocessConfig& config, std::size_t value_count)
{
    const auto without_objectness = config.num_classes + 4;
    const auto with_objectness = config.num_classes + 5;
    if (without_objectness > 0 && value_count % static_cast<std::size_t>(without_objectness) == 0U) {
        return without_objectness;
    }
    if (with_objectness > 0 && value_count % static_cast<std::size_t>(with_objectness) == 0U) {
        return with_objectness;
    }
    return 0;
}

/**
 * @brief 从扁平 YOLO 输出中读取一个属性值。
 * @param values 扁平输出浮点数组。
 * @param layout 输出布局。
 * @param candidate_index 候选框下标。
 * @param attribute_index 属性下标。
 * @param candidate_count 候选框数量。
 * @param attributes_per_candidate 每个候选框属性数量。
 * @return 指定位置的属性值。
 */
float read_attribute(const std::vector<float>& values,
                     const std::string& layout,
                     std::size_t candidate_index,
                     std::size_t attribute_index,
                     std::size_t candidate_count,
                     std::size_t attributes_per_candidate)
{
    if (layout == "anchors_first") {
        return values[candidate_index * attributes_per_candidate + attribute_index];
    }
    return values[attribute_index * candidate_count + candidate_index];
}

/**
 * @brief 判断布局配置是否有效。
 * @param layout 配置中的布局文本。
 * @return 若布局受支持则返回 `true`。
 */
bool supported_layout(const std::string& layout)
{
    return layout == "channels_first" || layout == "anchors_first";
}

} // namespace

/**
 * @brief 使用后处理配置和检测规则构造 OpenCV YOLO 后处理器。
 * @param config 后处理运行配置。
 * @param rules 检测过滤规则。
 */
OpenCvYoloPostprocessor::OpenCvYoloPostprocessor(PostprocessConfig config, RuleConfig rules)
    : config_(std::move(config))
    , rules_(std::move(rules))
{
}

/**
 * @brief 校验后处理配置。
 * @return 成功返回 `true`，失败返回 `false`。
 */
bool OpenCvYoloPostprocessor::open()
{
    if (config_.model_type != "yolo") {
        last_error_ = "OpenCV postprocessor currently supports only yolo model_type";
        return false;
    }
    if (!supported_layout(config_.output_layout)) {
        last_error_ = "postprocess.output_layout must be channels_first or anchors_first";
        return false;
    }
    if (config_.num_classes <= 0) {
        last_error_ = "postprocess.num_classes must be positive";
        return false;
    }
    if (config_.confidence_threshold < 0.0 || config_.confidence_threshold > 1.0) {
        last_error_ = "postprocess.confidence_threshold must be in [0, 1]";
        return false;
    }
    if (config_.nms_iou_threshold < 0.0 || config_.nms_iou_threshold > 1.0) {
        last_error_ = "postprocess.nms_iou_threshold must be in [0, 1]";
        return false;
    }
    if (config_.max_detections <= 0) {
        last_error_ = "postprocess.max_detections must be positive";
        return false;
    }

    is_open_ = true;
    last_error_.clear();
    return true;
}

/**
 * @brief 释放后处理资源。
 */
void OpenCvYoloPostprocessor::close() noexcept
{
    is_open_ = false;
}

/**
 * @brief 将 YOLO 原始输出转换为检测结果。
 * @param outputs 模型输出缓冲区列表。
 * @param input_tensor 本次推理输入张量。
 * @return 后处理后的检测结果列表。
 */
std::vector<Detection> OpenCvYoloPostprocessor::process(
    const std::vector<ModelOutputBuffer>& outputs,
    const TensorBuffer& input_tensor)
{
    if (!is_open_) {
        last_error_ = "OpenCV YOLO postprocessor is not open";
        return {};
    }
    if (outputs.empty()) {
        last_error_ = "model outputs are empty";
        return {};
    }

    std::vector<float> values;
    try {
        values = output_to_floats(outputs.front());
    } catch (const std::exception& error) {
        last_error_ = error.what();
        return {};
    }

    const auto attributes_per_candidate = infer_attributes_per_candidate(config_, values.size());
    if (attributes_per_candidate == 0) {
        last_error_ = "cannot infer YOLO attributes per candidate from output size";
        return {};
    }

    const auto candidate_count = values.size() / static_cast<std::size_t>(attributes_per_candidate);
    const auto has_objectness = attributes_per_candidate == config_.num_classes + 5;
    const auto class_offset = has_objectness ? 5U : 4U;
    const auto image_width = input_width(input_tensor);
    const auto image_height = input_height(input_tensor);
    const auto confidence_threshold = static_cast<float>(
        std::max(config_.confidence_threshold, rules_.min_confidence));

    std::vector<Candidate> candidates;
    candidates.reserve(candidate_count);

    for (std::size_t candidate_index = 0; candidate_index < candidate_count; ++candidate_index) {
        const auto center_x = read_attribute(values,
                                             config_.output_layout,
                                             candidate_index,
                                             0,
                                             candidate_count,
                                             attributes_per_candidate);
        const auto center_y = read_attribute(values,
                                             config_.output_layout,
                                             candidate_index,
                                             1,
                                             candidate_count,
                                             attributes_per_candidate);
        const auto box_width = read_attribute(values,
                                              config_.output_layout,
                                              candidate_index,
                                              2,
                                              candidate_count,
                                              attributes_per_candidate);
        const auto box_height = read_attribute(values,
                                               config_.output_layout,
                                               candidate_index,
                                               3,
                                               candidate_count,
                                               attributes_per_candidate);
        if (!valid_number(center_x) || !valid_number(center_y) || !valid_number(box_width) ||
            !valid_number(box_height) || box_width <= 0.0F || box_height <= 0.0F) {
            continue;
        }

        float best_score = 0.0F;
        int best_class_id = 0;
        for (int class_id = 0; class_id < config_.num_classes; ++class_id) {
            const auto score = read_attribute(values,
                                              config_.output_layout,
                                              candidate_index,
                                              class_offset + static_cast<std::size_t>(class_id),
                                              candidate_count,
                                              attributes_per_candidate);
            if (valid_number(score) && score > best_score) {
                best_score = score;
                best_class_id = class_id;
            }
        }

        const auto objectness = has_objectness
                                    ? read_attribute(values,
                                                     config_.output_layout,
                                                     candidate_index,
                                                     4,
                                                     candidate_count,
                                                     attributes_per_candidate)
                                    : 1.0F;
        const auto confidence = best_score * objectness;
        if (!valid_number(confidence) || confidence < confidence_threshold) {
            continue;
        }

        const auto label = resolve_label(config_, rules_, best_class_id);
        if (!rules_.target_classes.empty() && !is_target_label(rules_, label)) {
            continue;
        }

        const auto normalized_box = make_normalized_box(center_x,
                                                        center_y,
                                                        box_width,
                                                        box_height,
                                                        image_width,
                                                        image_height);
        if (normalized_box.width <= 0.0 || normalized_box.height <= 0.0) {
            continue;
        }

        candidates.push_back(Candidate{
            normalized_box,
            to_nms_rect(normalized_box, image_width, image_height),
            label,
            confidence,
            best_class_id,
        });
    }

    std::vector<int> kept_indices;
    std::vector<std::string> visited_labels;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto& label = candidates[index].label;
        if (std::find(visited_labels.begin(), visited_labels.end(), label) != visited_labels.end()) {
            continue;
        }
        visited_labels.push_back(label);

        std::vector<cv::Rect> boxes;
        std::vector<float> scores;
        std::vector<int> original_indices;
        for (std::size_t candidate_index = 0; candidate_index < candidates.size(); ++candidate_index) {
            const auto& candidate = candidates[candidate_index];
            if (candidate.label != label) {
                continue;
            }
            boxes.push_back(candidate.nms_box);
            scores.push_back(candidate.confidence);
            original_indices.push_back(static_cast<int>(candidate_index));
        }

        std::vector<int> local_kept_indices;
        cv::dnn::NMSBoxes(boxes,
                          scores,
                          confidence_threshold,
                          static_cast<float>(config_.nms_iou_threshold),
                          local_kept_indices);
        for (const auto local_index : local_kept_indices) {
            kept_indices.push_back(original_indices[static_cast<std::size_t>(local_index)]);
        }
    }

    std::sort(kept_indices.begin(), kept_indices.end(), [&candidates](int lhs, int rhs) {
        return candidates[static_cast<std::size_t>(lhs)].confidence >
               candidates[static_cast<std::size_t>(rhs)].confidence;
    });

    std::vector<Detection> detections;
    detections.reserve(std::min<std::size_t>(kept_indices.size(),
                                             static_cast<std::size_t>(config_.max_detections)));
    for (const auto kept_index : kept_indices) {
        if (detections.size() >= static_cast<std::size_t>(config_.max_detections)) {
            break;
        }
        const auto& candidate = candidates[static_cast<std::size_t>(kept_index)];
        detections.push_back(Detection{
            candidate.label,
            candidate.confidence,
            candidate.normalized_box,
            input_tensor.frame_sequence,
            input_tensor.camera_id,
        });
    }

    debug_info_ = "opencv yolo postprocess output_bytes=" + std::to_string(outputs.front().data.size()) +
                  " values=" + std::to_string(values.size()) +
                  " layout=" + config_.output_layout +
                  " attrs=" + std::to_string(attributes_per_candidate) +
                  " candidates=" + std::to_string(candidate_count) +
                  " after_threshold=" + std::to_string(candidates.size()) +
                  " after_nms=" + std::to_string(detections.size());
    last_error_.clear();
    return detections;
}

/**
 * @brief 返回后处理策略类型。
 * @return 固定返回 `"opencv"`。
 */
std::string_view OpenCvYoloPostprocessor::kind() const noexcept
{
    return "opencv";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 错误文本。
 */
std::string_view OpenCvYoloPostprocessor::last_error() const noexcept
{
    return last_error_;
}

/**
 * @brief 返回最近一次后处理调试摘要。
 * @return 调试摘要文本。
 */
std::string_view OpenCvYoloPostprocessor::debug_info() const noexcept
{
    return debug_info_;
}

} // namespace sentinel
