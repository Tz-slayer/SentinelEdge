#pragma once

#include <string_view>

namespace sentinel {

/**
 * @brief 表示日志消息的严重级别。
 */
enum class LogLevel {
    kDebug = 0,         // 调试级别，适合开发阶段输出的详细信息。
    kInfo = 1,          // 信息级别，适合生产环境输出的常规运行信息。
    kWarn = 2,          // 警告级别，适合输出需要关注但不紧急的问题。
    kError = 3,         // 错误级别，适合输出严重问题。
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
