#pragma once

#include <signal.h>

namespace sentinel {

class LinuxSignalFd {
public:
    LinuxSignalFd();
    ~LinuxSignalFd();

    LinuxSignalFd(const LinuxSignalFd&) = delete;
    LinuxSignalFd& operator=(const LinuxSignalFd&) = delete;

    LinuxSignalFd(LinuxSignalFd&&) = delete;
    LinuxSignalFd& operator=(LinuxSignalFd&&) = delete;

    bool consume_stop_signal();

private:
    sigset_t signal_mask_{};
    sigset_t previous_mask_{};
    int fd_{-1};
};

} // namespace sentinel
