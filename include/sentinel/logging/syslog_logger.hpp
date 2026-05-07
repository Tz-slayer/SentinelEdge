#pragma once

#include "sentinel/logging/logger.hpp"

#include <string>

namespace sentinel {

/**
 * @brief 将日志写入 Linux syslog 的策略实现。
 *
 * 该实现适合守护进程、systemd 服务或集中日志收集场景。
 */
class SyslogLogger final : public Logger {
public:
    /**
     * @brief 构造 syslog 日志器并调用 `openlog`。
     * @param min_level 低于该级别的日志将被丢弃。
     * @param ident 写入 syslog 的程序标识符，生命周期由对象持有。
     */
    SyslogLogger(LogLevel min_level, std::string ident);

    /**
     * @brief 关闭 syslog 连接。
     */
    ~SyslogLogger() override;

private:
    /**
     * @brief 将日志消息映射到 syslog 优先级并写入系统日志。
     * @param level 本条消息的严重级别。
     * @param message 要输出的日志内容。
     */
    void write(LogLevel level, std::string_view message) override;

    std::string ident_;
};

} // namespace sentinel
