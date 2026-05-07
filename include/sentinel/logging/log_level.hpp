#pragma once

#include <string_view>

namespace sentinel {

/**
 * @brief 表示日志消息的严重级别。
 */
enum class LogLevel {
    kDebug = 0,
    kInfo = 1,
    kWarn = 2,
    kError = 3,
};

/**
 * @brief 返回日志级别对应的稳定文本。
 * @param level 日志级别枚举值。
 * @return 对应的级别名称，例如 `"INFO"`。
 */
inline std::string_view to_string(LogLevel level) noexcept
{
    switch (level) {
    case LogLevel::kDebug:
        return "DEBUG";
    case LogLevel::kInfo:
        return "INFO";
    case LogLevel::kWarn:
        return "WARN";
    case LogLevel::kError:
        return "ERROR";
    }

    return "UNKNOWN";
}

} // namespace sentinel
