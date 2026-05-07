#include "sentinel/app/linux_signal_fd.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <unistd.h>

namespace {

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    sentinel::LinuxSignalFd signal_fd;

    expect(::kill(::getpid(), SIGINT) == 0, "kill(SIGINT) should succeed");
    expect(signal_fd.consume_stop_signal(), "signalfd should consume SIGINT");
    expect(!signal_fd.consume_stop_signal(), "signal queue should be empty after consume");

    return 0;
}
