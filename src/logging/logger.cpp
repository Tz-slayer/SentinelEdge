#include "sentinel/logging/logger.hpp"

namespace sentinel {

/**
 * @brief 使用最小日志级别构造基类。
 * @param min_level 低于该级别的日志将被丢弃。
 */
Logger::Logger(LogLevel min_level) noexcept
    : min_level_(min_level)
{
}

/**
 * @brief 记录一条通用日志。
 * @param level 本条消息的严重级别。
 * @param message 要输出的日志内容。
 */
void Logger::log(LogLevel level, std::string_view message)
{
    if (static_cast<int>(level) < static_cast<int>(min_level_)) {
        return;
    }

    write(level, message);
}

/**
 * @brief 记录调试级别日志。
 * @param message 要输出的日志内容。
 */
void Logger::debug(std::string_view message)
{
    log(LogLevel::kDebug, message);
}

/**
 * @brief 记录信息级别日志。
 * @param message 要输出的日志内容。
 */
void Logger::info(std::string_view message)
{
    log(LogLevel::kInfo, message);
}

/**
 * @brief 记录警告级别日志。
 * @param message 要输出的日志内容。
 */
void Logger::warn(std::string_view message)
{
    log(LogLevel::kWarn, message);
}

/**
 * @brief 记录错误级别日志。
 * @param message 要输出的日志内容。
 */
void Logger::error(std::string_view message)
{
    log(LogLevel::kError, message);
}

/**
 * @brief 返回当前最小日志级别。
 * @return 当前生效的级别阈值。
 */
LogLevel Logger::min_level() const noexcept
{
    return min_level_;
}

} // namespace sentinel
