#include "sentinel/logging/stderr_logger.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include <time.h>
#include <unistd.h>

namespace sentinel {
namespace {

/**
 * @brief 将缓冲区内容完整写入指定文件描述符。
 * @param fd 目标文件描述符。
 * @param buffer 待写入的文本缓冲区。
 *
 * 若 `write(2)` 被信号中断则自动重试；若出现其他错误则静默放弃，
 * 避免日志输出反向干扰主业务流程。
 */
void write_all(int fd, std::string_view buffer) noexcept
{
    std::size_t written_total = 0;
    while (written_total < buffer.size()) {
        const auto* start = buffer.data() + written_total;
        const auto remaining = buffer.size() - written_total;
        const auto bytes_written = ::write(fd, start, remaining);

        if (bytes_written > 0) {
            written_total += static_cast<std::size_t>(bytes_written);
            continue;
        }

        if (bytes_written == -1 && errno == EINTR) {
            continue;
        }

        return;
    }
}

/**
 * @brief 生成当前本地时间的文本前缀。
 * @return 形如 `2026-05-07 10:15:30.123` 的时间字符串。
 */
std::string make_timestamp()
{
    timespec current_time {};
    if (::clock_gettime(CLOCK_REALTIME, &current_time) == -1) {
        return "1970-01-01 00:00:00.000";
    }

    std::tm local_time {};
    const auto seconds = static_cast<std::time_t>(current_time.tv_sec);
    if (::localtime_r(&seconds, &local_time) == nullptr) {
        return "1970-01-01 00:00:00.000";
    }

    char date_buffer[32] {};
    if (std::strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d %H:%M:%S", &local_time) == 0U) {
        return "1970-01-01 00:00:00.000";
    }

    char millisecond_buffer[5] {};
    const auto milliseconds = static_cast<int>(current_time.tv_nsec / 1000000L);
    std::snprintf(millisecond_buffer, sizeof(millisecond_buffer), "%03d", milliseconds);

    return std::string(date_buffer) + "." + millisecond_buffer;
}

} // namespace

/**
 * @brief 构造标准错误日志器。
 * @param min_level 低于该级别的日志将被丢弃。
 */
StderrLogger::StderrLogger(LogLevel min_level) noexcept
    : Logger(min_level)
{
}

/**
 * @brief 将日志格式化后写入标准错误输出。
 * @param level 本条消息的严重级别。
 * @param message 要输出的日志内容。
 */
void StderrLogger::write(LogLevel level, std::string_view message)
{
    const auto line = make_timestamp() + " [" + std::string(to_string(level)) + "] " +
                      std::string(message) + "\n";
    write_all(STDERR_FILENO, line);
}

} // namespace sentinel
