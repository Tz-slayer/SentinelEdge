#include "sentinel/app/linux_signal_fd.hpp"

#include <cerrno>
#include <system_error>

#include <sys/signalfd.h>
#include <unistd.h>

namespace sentinel {

LinuxSignalFd::LinuxSignalFd()
{
    if (::sigemptyset(&signal_mask_) == -1) {
        throw std::system_error(errno, std::generic_category(), "sigemptyset failed");
    }
    if (::sigaddset(&signal_mask_, SIGINT) == -1) {
        throw std::system_error(errno, std::generic_category(), "sigaddset(SIGINT) failed");
    }
    if (::sigaddset(&signal_mask_, SIGTERM) == -1) {
        throw std::system_error(errno, std::generic_category(), "sigaddset(SIGTERM) failed");
    }
    if (::sigprocmask(SIG_BLOCK, &signal_mask_, &previous_mask_) == -1) {
        throw std::system_error(errno, std::generic_category(), "sigprocmask failed");
    }

    fd_ = ::signalfd(-1, &signal_mask_, SFD_CLOEXEC | SFD_NONBLOCK);
    if (fd_ == -1) {
        const auto saved_errno = errno;
        ::sigprocmask(SIG_SETMASK, &previous_mask_, nullptr);
        throw std::system_error(saved_errno, std::generic_category(), "signalfd failed");
    }
}

LinuxSignalFd::~LinuxSignalFd()
{
    if (fd_ != -1) {
        ::close(fd_);
    }
    ::sigprocmask(SIG_SETMASK, &previous_mask_, nullptr);
}

bool LinuxSignalFd::consume_stop_signal()
{
    signalfd_siginfo signal_info {};
    const auto bytes_read = ::read(fd_, &signal_info, sizeof(signal_info));

    if (bytes_read == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return false;
        }
        throw std::system_error(errno, std::generic_category(), "read(signalfd) failed");
    }

    if (bytes_read != static_cast<ssize_t>(sizeof(signal_info))) {
        return false;
    }

    return signal_info.ssi_signo == SIGINT || signal_info.ssi_signo == SIGTERM;
}

} // namespace sentinel
