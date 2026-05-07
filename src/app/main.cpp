#include "sentinel/app/linux_signal_fd.hpp"
#include "sentinel/app/pipeline.hpp"
#include "sentinel/config/config_loader.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv)
{
    // 如果用户传了命令行参数，使用传入路径作为配置目录，否则默认使用 config 目录
    const auto config_dir = argc > 1 ? argv[1] : "config";

    try {
        sentinel::LinuxSignalFd signal_fd;

        // 载入配置
        const auto config = sentinel::load_config(config_dir);
        const auto result = sentinel::run_demo_pipeline(config, [&signal_fd]() {
            return signal_fd.consume_stop_signal();
        });

        std::cout << "video_sentinel started\n";
        std::cout << "config_dir: " << config_dir << '\n';
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
