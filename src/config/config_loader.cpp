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

std::string strip_comment(const std::string& line)
{
    const auto position = line.find('#');
    return position == std::string::npos ? line : line.substr(0, position);
}

std::string strip_quotes(const std::string& value)
{
    const auto trimmed = trim(value);
    if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
        return trimmed.substr(1, trimmed.size() - 2);
    }
    return trimmed;
}

std::optional<std::pair<std::string, std::string>> split_key_value(const std::string& line)
{
    const auto separator = line.find(':');
    if (separator == std::string::npos) {
        return std::nullopt;
    }

    return std::make_pair(trim(line.substr(0, separator)), strip_quotes(line.substr(separator + 1)));
}

bool parse_bool(const std::string& value)
{
    const auto normalized = strip_quotes(value);
    return normalized == "true" || normalized == "1" || normalized == "yes";
}

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

void load_service_config(const std::filesystem::path& config_dir, SentinelConfig& config)
{
    const auto lines = read_config_lines(resolve_config_file(config_dir, "sentinel"));
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
        if (section == "service" && key == "host") {
            config.service.host = value;
        } else if (section == "service" && key == "port") {
            config.service.port = std::stoi(value);
        } else if (section == "runtime" && key == "data_dir") {
            config.service.data_dir = value;
        } else if (section == "pipeline" && key == "max_frames") {
            config.service.max_frames = std::stoi(value);
        }
    }
}

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

void load_camera_config(const std::filesystem::path& config_dir, SentinelConfig& config)
{
    const auto lines = read_config_lines(resolve_config_file(config_dir, "cameras"));
    std::vector<CameraConfig> cameras;
    CameraConfig current;
    bool has_current = false;

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

SentinelConfig load_config(const std::filesystem::path& config_dir)
{
    SentinelConfig config;
    load_service_config(config_dir, config);
    load_camera_config(config_dir, config);
    load_rule_config(config_dir, config);

    if (config.cameras.empty()) {
        throw std::runtime_error("at least one camera must be configured");
    }
    if (config.service.max_frames <= 0) {
        throw std::runtime_error("pipeline.max_frames must be greater than zero");
    }
    if (config.rules.hold_frames <= 0) {
        throw std::runtime_error("events.hold_frames must be greater than zero");
    }

    return config;
}

} // namespace sentinel
