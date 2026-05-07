#pragma once

#include "sentinel/logging/log_level.hpp"

#include <string_view>

namespace sentinel {

/**
 * @brief 日志输出策略的抽象接口。
 *
 * 该接口负责在统一的级别过滤之后，把日志消息写入具体后端，
 * 例如标准错误输出或 Linux syslog。
 */
class Logger {
public:
    /**
     * @brief 使用最小输出级别构造日志策略。
     * @param min_level 低于该级别的日志将被直接丢弃。
     */
    explicit Logger(LogLevel min_level) noexcept;

    /**
     * @brief 释放日志策略对象。
     */
    virtual ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    /**
     * @brief 记录一条指定级别的日志消息。
     * @param level 本条消息的严重级别。
     * @param message 要输出的日志内容。
     */
    void log(LogLevel level, std::string_view message);

    /**
     * @brief 记录一条调试级别日志。
     * @param message 要输出的日志内容。
     */
    void debug(std::string_view message);

    /**
     * @brief 记录一条信息级别日志。
     * @param message 要输出的日志内容。
     */
    void info(std::string_view message);

    /**
     * @brief 记录一条警告级别日志。
     * @param message 要输出的日志内容。
     */
    void warn(std::string_view message);

    /**
     * @brief 记录一条错误级别日志。
     * @param message 要输出的日志内容。
     */
    void error(std::string_view message);

    /**
     * @brief 返回当前日志器的最小输出级别。
     * @return 当前生效的级别阈值。
     */
    LogLevel min_level() const noexcept;

protected:
    /**
     * @brief 将已经通过级别过滤的日志写入具体后端。
     * @param level 本条消息的严重级别。
     * @param message 要输出的日志内容。
     */
    virtual void write(LogLevel level, std::string_view message) = 0;

private:
    LogLevel min_level_{LogLevel::kInfo};
};

} // namespace sentinel
