#include "sentinel/app/linux_signal_fd.hpp"
#include "sentinel/app/pipeline.hpp"
#include "sentinel/build/build_config.hpp"
#include "sentinel/config/config_loader.hpp"
#include "sentinel/logging/logger_factory.hpp"
#include "sentinel/logging/stderr_logger.hpp"

#include <exception>
#include <filesystem>
#include <memory>
#include <string>

/**
 * @brief 程序主入口。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main(int argc, char** argv)
{
    const std::filesystem::path config_dir = argc > 1 ? argv[1] : sentinel::kDefaultConfigDir;
    sentinel::StderrLogger bootstrap_logger(sentinel::LogLevel::kInfo);

    try {
        sentinel::LinuxSignalFd signal_fd;

        // 先加载配置，再把停止信号回调注入流水线，保证主循环可优雅退出。
        const auto config = sentinel::load_config(config_dir);
        auto logger = sentinel::create_logger(config.logging);

        logger->info("video_sentinel started");
        logger->info("config_dir: " + config_dir.string());
#if defined(SENTINEL_ENABLE_DEV_LOGGING) && SENTINEL_ENABLE_DEV_LOGGING
        logger->info("build_profile: development");
#else
        logger->info("build_profile: production");
#endif
        logger->info("service: " + config.service.host + ":" + std::to_string(config.service.port));
        logger->info("logging: backend=" + config.logging.backend + " level=" + config.logging.level);
        logger->debug("debug logging enabled");
        logger->info("camera: " + config.cameras.front().id + " type=" + config.cameras.front().type);

        // demo 示例 pipeline
        const auto result = sentinel::run_demo_pipeline(config, *logger, [&signal_fd]() {
            return signal_fd.consume_stop_signal();
        });

        logger->info("frames_processed: " + std::to_string(result.frames_processed));
        logger->info("detections_seen: " + std::to_string(result.detections_seen));
        logger->info("events_emitted: " + std::to_string(result.events.size()));

        for (const auto& event : result.events) {
            logger->info("event: id=" + event.id + " type=" + event.type + " camera=" +
                         event.camera_id + " label=" + event.label + " frames=" +
                         std::to_string(event.start_frame) + "-" +
                         std::to_string(event.end_frame) + " confidence=" +
                         std::to_string(event.confidence));
        }

        if (result.frames_processed < config.service.max_frames) {
            logger->warn("shutdown before reaching configured max_frames");
        }

        return 0;
    } catch (const std::exception& error) {
        bootstrap_logger.error("video_sentinel failed: " + std::string(error.what()));
        return 1;
    }
}
