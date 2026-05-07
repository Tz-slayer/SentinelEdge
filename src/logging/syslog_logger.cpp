#include "sentinel/logging/syslog_logger.hpp"

#include <string>

#include <syslog.h>

namespace sentinel {
namespace {

/**
 * @brief 将内部日志级别映射为 syslog 优先级。
 * @param level 内部日志级别。
 * @return 对应的 syslog 优先级常量。
 */
int to_syslog_priority(LogLevel level) noexcept
{
    switch (level) {
    case LogLevel::kDebug:
        return LOG_DEBUG;
    case LogLevel::kInfo:
        return LOG_INFO;
    case LogLevel::kWarn:
        return LOG_WARNING;
    case LogLevel::kError:
        return LOG_ERR;
    }

    return LOG_INFO;
}

} // namespace

/**
 * @brief 构造 syslog 日志器。
 * @param min_level 低于该级别的日志将被丢弃。
 * @param ident 写入 syslog 的程序标识符。
 */
SyslogLogger::SyslogLogger(LogLevel min_level, std::string ident)
    : Logger(min_level)
    , ident_(std::move(ident))
{
    ::openlog(ident_.c_str(), LOG_PID | LOG_NDELAY, LOG_USER);
}

/**
 * @brief 析构时关闭 syslog 连接。
 */
SyslogLogger::~SyslogLogger()
{
    ::closelog();
}

/**
 * @brief 将日志消息写入 syslog。
 * @param level 本条消息的严重级别。
 * @param message 要输出的日志内容。
 */
void SyslogLogger::write(LogLevel level, std::string_view message)
{
    const auto formatted_message = "[" + std::string(to_string(level)) + "] " + std::string(message);
    ::syslog(to_syslog_priority(level), "%s", formatted_message.c_str());
}

} // namespace sentinel
