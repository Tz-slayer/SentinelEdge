#include "sentinel/app/linux_signal_fd.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <unistd.h>

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
 * @brief 验证 `signalfd` 能正确消费本进程发送的 `SIGINT`。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    sentinel::LinuxSignalFd signal_fd;

    expect(::kill(::getpid(), SIGINT) == 0, "kill(SIGINT) should succeed");
    expect(signal_fd.consume_stop_signal(), "signalfd should consume SIGINT");
    expect(!signal_fd.consume_stop_signal(), "signal queue should be empty after consume");

    return 0;
}
