#pragma once

#include <signal.h>

namespace sentinel {

/**
 * @brief 通过 Linux `signalfd` 接收停止信号。
 *
 * 构造时会为当前线程屏蔽 `SIGINT` 和 `SIGTERM`，并创建非阻塞
 * `signalfd`；析构时会关闭文件描述符并恢复之前的信号掩码。
 */
class LinuxSignalFd {
public:
    /**
     * @brief 为 `SIGINT` 和 `SIGTERM` 创建 `signalfd`。
     */
    LinuxSignalFd();

    /**
     * @brief 关闭 `signalfd` 并恢复之前的信号掩码。
     */
    ~LinuxSignalFd();

    LinuxSignalFd(const LinuxSignalFd&) = delete;
    LinuxSignalFd& operator=(const LinuxSignalFd&) = delete;

    LinuxSignalFd(LinuxSignalFd&&) = delete;
    LinuxSignalFd& operator=(LinuxSignalFd&&) = delete;

    /**
     * @brief 消费一个待处理的停止信号。
     * @return 若收到了 `SIGINT` 或 `SIGTERM` 则返回 `true`，否则返回 `false`。
     */
    bool consume_stop_signal();

private:
    sigset_t signal_mask_{};
    sigset_t previous_mask_{};
    int fd_{-1};
};

} // namespace sentinel
