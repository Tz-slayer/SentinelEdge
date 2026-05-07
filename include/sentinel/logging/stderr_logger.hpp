#pragma once

#include "sentinel/logging/logger.hpp"

namespace sentinel {

/**
 * @brief 将日志写入标准错误输出的策略实现。
 *
 * 该实现使用 Linux `write(2)` 写入 `STDERR_FILENO`，适合前台运行、
 * 调试阶段或 systemd 捕获标准错误输出的场景。
 */
class StderrLogger final : public Logger {
public:
    /**
     * @brief 构造标准错误日志器。
     * @param min_level 低于该级别的日志将被丢弃。
     */
    explicit StderrLogger(LogLevel min_level) noexcept;

private:
    /**
     * @brief 将日志格式化后写入标准错误输出。
     * @param level 本条消息的严重级别。
     * @param message 要输出的日志内容。
     */
    void write(LogLevel level, std::string_view message) override;
};

} // namespace sentinel
