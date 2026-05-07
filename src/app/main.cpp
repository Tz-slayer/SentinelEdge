#include "sentinel/app/linux_signal_fd.hpp"
#include "sentinel/app/pipeline.hpp"
#include "sentinel/config/config_loader.hpp"

#include <exception>
#include <iostream>

/**
 * @brief 程序主入口。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main(int argc, char** argv)
{
    const auto config_dir = argc > 1 ? argv[1] : "config";

    try {
        sentinel::LinuxSignalFd signal_fd;

        // 先加载配置，再把停止信号回调注入流水线，保证主循环可优雅退出。
        const auto config = sentinel::load_config(config_dir);
        const auto result = sentinel::run_demo_pipeline(config, [&signal_fd]() {
            return signal_fd.consume_stop_signal();
        });

        std::cout << "video_sentinel started\n";
        std::cout << "config_dir: " << config_dir << '\n';
#if defined(SENTINEL_ENABLE_DEV_LOGGING) && SENTINEL_ENABLE_DEV_LOGGING
        std::cout << "build_profile: development\n";
#else
        std::cout << "build_profile: production\n";
#endif
        std::cout << "service: " << config.service.host << ':' << config.service.port << '\n';
        std::cout << "camera: " << config.cameras.front().id << " (" << config.cameras.front().type
                      << ")\n";
        std::cout << "frames_processed: " << result.frames_processed << '\n';
        std::cout << "detections_seen: " << result.detections_seen << '\n';
        std::cout << "events_emitted: " << result.events.size() << '\n';

        for (const auto& event : result.events) {
            std::cout << "- " << event.id << ' ' << event.type << " camera=" << event.camera_id
                      << " label=" << event.label << " frames=" << event.start_frame << '-'
                      << event.end_frame << " confidence=" << event.confidence << '\n';
        }

        if (result.frames_processed < config.service.max_frames) {
            std::cout << "shutdown: received stop signal\n";
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "video_sentinel failed: " << error.what() << '\n';
        return 1;
    }
}
