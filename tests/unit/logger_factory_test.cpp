#include "sentinel/logging/logger_factory.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

/**
 * @brief 断言条件成立，否则输出错误并退出。
 * @param condition 待检查条件。
 * @param message 失败时输出的错误消息。
 */
void expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

/**
 * @brief 验证日志工厂能够创建最小策略集合。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    sentinel::LoggingConfig stderr_config;
    stderr_config.backend = "stderr";
    stderr_config.level = "debug";

    const auto stderr_logger = sentinel::create_logger(stderr_config);
    expect(stderr_logger != nullptr, "stderr logger should be created");

    sentinel::LoggingConfig syslog_config;
    syslog_config.backend = "syslog";
    syslog_config.level = "info";
    syslog_config.ident = "sentinel_test";

    const auto syslog_logger = sentinel::create_logger(syslog_config);
    expect(syslog_logger != nullptr, "syslog logger should be created");

    sentinel::LoggingConfig invalid_backend_config;
    invalid_backend_config.backend = "file";
    invalid_backend_config.level = "info";

    bool invalid_backend_failed = false;
    try {
        static_cast<void>(sentinel::create_logger(invalid_backend_config));
    } catch (const std::exception&) {
        invalid_backend_failed = true;
    }
    expect(invalid_backend_failed, "unknown log backend should fail");

    sentinel::LoggingConfig invalid_level_config;
    invalid_level_config.backend = "stderr";
    invalid_level_config.level = "trace";

    bool invalid_level_failed = false;
    try {
        static_cast<void>(sentinel::create_logger(invalid_level_config));
    } catch (const std::exception&) {
        invalid_level_failed = true;
    }
    expect(invalid_level_failed, "unknown log level should fail");

    return 0;
}
