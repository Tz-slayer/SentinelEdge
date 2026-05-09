#include "sentinel/config/config_loader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace sentinel {
namespace {

/**
 * @brief 去掉字符串首尾空白字符。
 * @param value 原始字符串。
 * @return 去掉首尾空白后的字符串。
 */
std::string trim(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    if (first >= last) {
        return {};
    }

    return {first, last};
}

/**
 * @brief 去掉一行配置中的注释部分。
 * @param line 原始配置行。
 * @return 去掉 `#` 之后内容的字符串。
 */
std::string strip_comment(const std::string& line)
{
    const auto position = line.find('#');
    return position == std::string::npos ? line : line.substr(0, position);
}

/**
 * @brief 去掉字符串两端成对的双引号。
 * @param value 原始字符串。
 * @return 去掉外层双引号后的字符串。
 */
std::string strip_quotes(const std::string& value)
{
    const auto trimmed = trim(value);
    if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
        return trimmed.substr(1, trimmed.size() - 2);
    }
    return trimmed;
}

/**
 * @brief 将一行 `key: value` 配置拆分为键值对。
 * @param line 待解析的配置行。
 * @return 若存在键值分隔符则返回键值对，否则返回空。
 */
std::optional<std::pair<std::string, std::string>> split_key_value(const std::string& line)
{
    const auto separator = line.find(':');
    if (separator == std::string::npos) {
        return std::nullopt;
    }

    return std::make_pair(trim(line.substr(0, separator)), strip_quotes(line.substr(separator + 1)));
}

/**
 * @brief 将文本值解析为布尔值。
 * @param value 原始配置字符串。
 * @return 支持 `true`、`1`、`yes`，其余情况返回 `false`。
 */
bool parse_bool(const std::string& value)
{
    const auto normalized = strip_quotes(value);
    return normalized == "true" || normalized == "1" || normalized == "yes";
}

/**
 * @brief 解析形如 `[a, b, c]` 的内联列表。
 * @param value 原始配置字符串。
 * @return 解析出的字符串列表；若格式不合法则返回空列表。
 */
std::vector<std::string> parse_inline_list(const std::string& value)
{
    const auto trimmed = trim(value);
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
        return {};
    }

    std::vector<std::string> items;
    std::stringstream stream(trimmed.substr(1, trimmed.size() - 2));
    std::string item;
    while (std::getline(stream, item, ',')) {
        const auto parsed = strip_quotes(item);
        if (!parsed.empty()) {
            items.push_back(parsed);
        }
    }
    return items;
}

/**
 * @brief 在配置目录中解析某一类配置文件路径。
 * @param config_dir 配置目录。
 * @param name 配置文件基础名，例如 `sentinel`。
 * @return 实际存在的配置文件路径。
 */
std::filesystem::path resolve_config_file(const std::filesystem::path& config_dir,
                                          const std::string& name)
{
    const auto real_file = config_dir / (name + ".yaml");
    if (std::filesystem::exists(real_file)) {
        return real_file;
    }

    const auto example_file = config_dir / (name + ".example.yaml");
    if (std::filesystem::exists(example_file)) {
        return example_file;
    }

    throw std::runtime_error("missing config file: " + real_file.string());
}

/**
 * @brief 读取配置文件并过滤空行与注释。
 * @param path 配置文件路径。
 * @return 清洗后的配置行列表。
 */
std::vector<std::string> read_config_lines(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open config file: " + path.string());
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        const auto cleaned = trim(strip_comment(line));
        if (!cleaned.empty()) {
            lines.push_back(cleaned);
        }
    }
    return lines;
}

/**
 * @brief 载入服务级配置。
 * @param config_dir 配置目录。
 * @param config 待写入的聚合配置对象。
 */
void load_service_config(const std::filesystem::path& config_dir, SentinelConfig& config)
{
    const auto lines = read_config_lines(resolve_config_file(config_dir, "sentinel"));
    std::string section;

    for (const auto& line : lines) {
        // 先识别当前所在的小节，后续键值会按小节归类写入。
        if (line.back() == ':' && line.find(':') == line.size() - 1) {
            section = trim(line.substr(0, line.size() - 1));
            continue;
        }

        const auto key_value = split_key_value(line);
        if (!key_value.has_value()) {
            continue;
        }

        const auto& [key, value] = *key_value;
        if (section == "service" && key == "host") {
            config.service.host = value;
        } else if (section == "service" && key == "port") {
            config.service.port = std::stoi(value);
        } else if (section == "logging" && key == "backend") {
            config.logging.backend = value;
        } else if (section == "logging" && key == "level") {
            config.logging.level = value;
        } else if (section == "logging" && key == "ident") {
            config.logging.ident = value;
        } else if (section == "inference" && key == "backend") {
            config.inference.backend = value;
        } else if (section == "inference" && key == "model_path") {
            config.inference.model_path = value;
        } else if (section == "inference" && key == "device_id") {
            config.inference.device_id = std::stoi(value);
        } else if (section == "preprocess" && key == "backend") {
            config.preprocess.backend = value;
        } else if (section == "preprocess" && key == "output_width") {
            config.preprocess.output_width = std::stoi(value);
        } else if (section == "preprocess" && key == "output_height") {
            config.preprocess.output_height = std::stoi(value);
        } else if (section == "preprocess" && key == "output_layout") {
            config.preprocess.output_layout = value;
        } else if (section == "preprocess" && key == "output_dtype") {
            config.preprocess.output_dtype = value;
        } else if (section == "preprocess" && key == "normalize") {
            config.preprocess.normalize = parse_bool(value);
        } else if (section == "postprocess" && key == "backend") {
            config.postprocess.backend = value;
        } else if (section == "postprocess" && key == "model_type") {
            config.postprocess.model_type = value;
        } else if (section == "postprocess" && key == "output_layout") {
            config.postprocess.output_layout = value;
        } else if (section == "postprocess" && key == "num_classes") {
            config.postprocess.num_classes = std::stoi(value);
        } else if (section == "postprocess" && key == "confidence_threshold") {
            config.postprocess.confidence_threshold = std::stod(value);
        } else if (section == "postprocess" && key == "nms_iou_threshold") {
            config.postprocess.nms_iou_threshold = std::stod(value);
        } else if (section == "postprocess" && key == "max_detections") {
            config.postprocess.max_detections = std::stoi(value);
        } else if (section == "postprocess" && key == "merge_coco_vehicles") {
            config.postprocess.merge_coco_vehicles = parse_bool(value);
        } else if (section == "postprocess" && key == "class_names") {
            const auto parsed_names = parse_inline_list(value);
            if (!parsed_names.empty()) {
                config.postprocess.class_names = parsed_names;
            }
        } else if (section == "overlay" && key == "enabled") {
            config.overlay.enabled = parse_bool(value);
        } else if (section == "overlay" && key == "backend") {
            config.overlay.backend = value;
        } else if (section == "output" && key == "video_sink") {
            config.output.video_sink = value;
        } else if (section == "output" && key == "debug_image_dir") {
            config.output.debug_image_dir = value;
        } else if (section == "output" && key == "debug_image_interval") {
            config.output.debug_image_interval = std::stoi(value);
        } else if (section == "output" && key == "mjpeg_host") {
            config.output.mjpeg_host = value;
        } else if (section == "output" && key == "mjpeg_port") {
            config.output.mjpeg_port = std::stoi(value);
        } else if (section == "output" && key == "mjpeg_path") {
            config.output.mjpeg_path = value;
        } else if (section == "output" && key == "mjpeg_quality") {
            config.output.mjpeg_quality = std::stoi(value);
        } else if (section == "output" && key == "mjpeg_max_clients") {
            config.output.mjpeg_max_clients = std::stoi(value);
        } else if (section == "performance" && key == "enabled") {
            config.performance.enabled = parse_bool(value);
        } else if (section == "performance" && key == "log_interval_frames") {
            config.performance.log_interval_frames = std::stoi(value);
        } else if (section == "performance" && key == "csv_path") {
            config.performance.csv_path = value;
        } else if (section == "runtime" && key == "data_dir") {
            config.service.data_dir = value;
        } else if (section == "pipeline" && key == "max_frames") {
            config.service.max_frames = std::stoi(value);
        }
    }
}

/**
 * @brief 将一条摄像头键值写入摄像头配置对象。
 * @param camera 待更新的摄像头配置。
 * @param key 配置键。
 * @param value 配置值。
 */
void apply_camera_value(CameraConfig& camera, const std::string& key, const std::string& value)
{
    if (key == "id") {
        camera.id = value;
    } else if (key == "name") {
        camera.name = value;
    } else if (key == "type") {
        camera.type = value;
    } else if (key == "uri") {
        camera.uri = value;
    } else if (key == "buffer_mode") {
        camera.buffer_mode = value;
    } else if (key == "enabled") {
        camera.enabled = parse_bool(value);
    } else if (key == "width") {
        camera.width = std::stoi(value);
    } else if (key == "height") {
        camera.height = std::stoi(value);
    } else if (key == "fps") {
        camera.fps = std::stoi(value);
    }
}

/**
 * @brief 载入摄像头列表配置。
 * @param config_dir 配置目录。
 * @param config 待写入的聚合配置对象。
 */
void load_camera_config(const std::filesystem::path& config_dir, SentinelConfig& config)
{
    const auto lines = read_config_lines(resolve_config_file(config_dir, "cameras"));
    std::vector<CameraConfig> cameras;
    CameraConfig current;
    bool has_current = false;

    // 只有在已经开始构造一个摄像头条目后，才允许真正提交到列表。
    const auto commit_current = [&]() {
        if (has_current) {
            cameras.push_back(current);
        }
    };

    for (const auto& raw_line : lines) {
        if (raw_line == "cameras:") {
            continue;
        }

        auto line = raw_line;
        // 以 "- " 开头表示进入一个新的摄像头条目，需要先提交上一个对象。
        if (line.rfind("- ", 0) == 0) {
            commit_current();
            current = CameraConfig{};
            has_current = true;
            line = trim(line.substr(2));
        }

        const auto key_value = split_key_value(line);
        if (!key_value.has_value()) {
            continue;
        }

        const auto& [key, value] = *key_value;
        apply_camera_value(current, key, value);
    }

    commit_current();
    config.cameras = cameras;
}

/**
 * @brief 载入事件规则配置。
 * @param config_dir 配置目录。
 * @param config 待写入的聚合配置对象。
 */
void load_rule_config(const std::filesystem::path& config_dir, SentinelConfig& config)
{
    const auto lines = read_config_lines(resolve_config_file(config_dir, "rules"));
    std::string section;

    for (const auto& line : lines) {
        if (line.back() == ':' && line.find(':') == line.size() - 1) {
            section = trim(line.substr(0, line.size() - 1));
            continue;
        }

        const auto key_value = split_key_value(line);
        if (!key_value.has_value()) {
            continue;
        }

        const auto& [key, value] = *key_value;
        if (section == "detection" && key == "target_classes") {
            const auto parsed_classes = parse_inline_list(value);
            if (!parsed_classes.empty()) {
                config.rules.target_classes = parsed_classes;
            }
        } else if (section == "detection" && key == "min_confidence") {
            config.rules.min_confidence = std::stod(value);
        } else if (section == "events" && key == "hold_frames") {
            config.rules.hold_frames = std::stoi(value);
        } else if (section == "events" && key == "cooldown_frames") {
            config.rules.cooldown_frames = std::stoi(value);
        }
    }
}

} // namespace

/**
 * @brief 载入并校验应用配置。
 * @param config_dir 配置目录。
 * @return 已完成基础校验的聚合配置对象。
 */
SentinelConfig load_config(const std::filesystem::path& config_dir)
{
    SentinelConfig config;
    load_service_config(config_dir, config);
    load_camera_config(config_dir, config);
    load_rule_config(config_dir, config);

    // 先做最基本的运行前校验，避免后续流水线在空配置上继续运行。
    if (config.cameras.empty()) {
        throw std::runtime_error("at least one camera must be configured");
    }
    for (const auto& camera : config.cameras) {
        // buffer_mode 是运行期性能实验开关，只允许明确的已知取值。
        if (camera.buffer_mode != "copy" && camera.buffer_mode != "loaned") {
            throw std::runtime_error("camera.buffer_mode must be either copy or loaned: " +
                                     camera.id);
        }
    }
    if (config.service.max_frames <= 0) {
        throw std::runtime_error("pipeline.max_frames must be greater than zero");
    }
    if (config.logging.backend.empty()) {
        throw std::runtime_error("logging.backend must not be empty");
    }
    if (config.logging.level.empty()) {
        throw std::runtime_error("logging.level must not be empty");
    }
    if (config.inference.backend.empty()) {
        throw std::runtime_error("inference.backend must not be empty");
    }
    if (config.inference.backend == "ascendcl" && config.inference.model_path.empty()) {
        throw std::runtime_error("inference.model_path must not be empty when using AscendCL");
    }
    if (config.inference.device_id < 0) {
        throw std::runtime_error("inference.device_id must not be negative");
    }
    if (config.preprocess.backend.empty()) {
        throw std::runtime_error("preprocess.backend must not be empty");
    }
    if (config.preprocess.backend != "opencv" && config.preprocess.backend != "dvpp") {
        throw std::runtime_error("preprocess.backend must be opencv or dvpp");
    }
    if (config.preprocess.output_width <= 0 || config.preprocess.output_height <= 0) {
        throw std::runtime_error("preprocess output size must be positive");
    }
    if (config.preprocess.output_layout != "NCHW") {
        throw std::runtime_error("preprocess.output_layout currently supports only NCHW");
    }
    if (config.preprocess.output_dtype != "FP32") {
        throw std::runtime_error("preprocess.output_dtype currently supports only FP32");
    }
    if (config.postprocess.backend.empty()) {
        throw std::runtime_error("postprocess.backend must not be empty");
    }
    if (config.postprocess.backend != "opencv" && config.postprocess.backend != "dvpp") {
        throw std::runtime_error("postprocess.backend must be opencv or dvpp");
    }
    if (config.postprocess.model_type != "yolo") {
        throw std::runtime_error("postprocess.model_type currently supports only yolo");
    }
    if (config.postprocess.output_layout != "channels_first" &&
        config.postprocess.output_layout != "anchors_first") {
        throw std::runtime_error("postprocess.output_layout must be channels_first or anchors_first");
    }
    if (config.postprocess.num_classes <= 0) {
        throw std::runtime_error("postprocess.num_classes must be positive");
    }
    if (config.postprocess.confidence_threshold < 0.0 ||
        config.postprocess.confidence_threshold > 1.0) {
        throw std::runtime_error("postprocess.confidence_threshold must be in [0, 1]");
    }
    if (config.postprocess.nms_iou_threshold < 0.0 ||
        config.postprocess.nms_iou_threshold > 1.0) {
        throw std::runtime_error("postprocess.nms_iou_threshold must be in [0, 1]");
    }
    if (config.postprocess.max_detections <= 0) {
        throw std::runtime_error("postprocess.max_detections must be positive");
    }
    if (config.overlay.backend != "opencv" && config.overlay.backend != "dvpp") {
        throw std::runtime_error("overlay.backend must be opencv or dvpp");
    }
    if (config.output.video_sink != "none" && config.output.video_sink != "debug_image" &&
        config.output.video_sink != "mjpeg") {
        throw std::runtime_error("output.video_sink must be none, debug_image or mjpeg");
    }
    if (config.output.debug_image_interval <= 0) {
        throw std::runtime_error("output.debug_image_interval must be positive");
    }
    if (config.output.mjpeg_host.empty()) {
        throw std::runtime_error("output.mjpeg_host must not be empty");
    }
    if (config.output.mjpeg_port <= 0 || config.output.mjpeg_port > 65535) {
        throw std::runtime_error("output.mjpeg_port must be in 1..65535");
    }
    if (config.output.mjpeg_path.empty() || config.output.mjpeg_path.front() != '/') {
        throw std::runtime_error("output.mjpeg_path must start with /");
    }
    if (config.output.mjpeg_quality <= 0 || config.output.mjpeg_quality > 100) {
        throw std::runtime_error("output.mjpeg_quality must be in 1..100");
    }
    if (config.output.mjpeg_max_clients <= 0) {
        throw std::runtime_error("output.mjpeg_max_clients must be positive");
    }
    if (config.performance.log_interval_frames <= 0) {
        throw std::runtime_error("performance.log_interval_frames must be positive");
    }
    if (config.rules.hold_frames <= 0) {
        throw std::runtime_error("events.hold_frames must be greater than zero");
    }

    return config;
}

} // namespace sentinel
