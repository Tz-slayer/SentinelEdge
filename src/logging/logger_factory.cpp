#include "sentinel/logging/logger_factory.hpp"

#include "sentinel/logging/stderr_logger.hpp"
#include "sentinel/logging/syslog_logger.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace sentinel {
namespace {

/**
 * @brief 将配置字符串标准化为小写 ASCII 形式。
 * @param value 原始配置值。
 * @return 归一化后的字符串。
 */
std::string to_lower_ascii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

/**
 * @brief 将文本配置解析为日志级别。
 * @param value 配置中的级别文本。
 * @return 对应的日志级别枚举值。
 */
LogLevel parse_log_level(const std::string& value)
{
    const auto normalized = to_lower_ascii(value);
    if (normalized == "debug") {
        return LogLevel::kDebug;
    }
    if (normalized == "info") {
        return LogLevel::kInfo;
    }
    if (normalized == "warn" || normalized == "warning") {
        return LogLevel::kWarn;
    }
    if (normalized == "error") {
        return LogLevel::kError;
    }

    throw std::runtime_error("unsupported log level: " + value);
}

} // namespace

/**
 * @brief 根据日志配置创建日志策略对象。
 * @param config 日志配置。
 * @return 新创建的日志策略对象。
 */
std::unique_ptr<Logger> create_logger(const LoggingConfig& config)
{
    const auto min_level = parse_log_level(config.level);
    const auto backend = to_lower_ascii(config.backend);

    if (backend == "stderr") {
        return std::make_unique<StderrLogger>(min_level);
    }
    if (backend == "syslog") {
        return std::make_unique<SyslogLogger>(min_level,
                                              config.ident.empty() ? "video_sentinel" : config.ident);
    }

    throw std::runtime_error("unsupported log backend: " + config.backend);
}

} // namespace sentinel
