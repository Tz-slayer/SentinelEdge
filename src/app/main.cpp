#include "sentinel/app/linux_signal_fd.hpp"
#include "sentinel/app/pipeline.hpp"
#include "sentinel/build/build_config.hpp"
#include "sentinel/config/config_loader.hpp"
#include "sentinel/logging/logger_factory.hpp"
#include "sentinel/logging/stderr_logger.hpp"
#include "sentinel/api/mqtt_client.hpp"
#include "sentinel/api/mqtt_publisher.hpp"

#include <atomic>
#include <chrono>
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
    // 从命令行参数或默认路径加载配置，并初始化日志系统。
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
        logger->info("pipeline: backend=" + config.pipeline.backend +
                     " mode=" + config.pipeline.mode +
                     " max_frames=" + std::to_string(config.pipeline.max_frames) +
                     " detect_fps=" + std::to_string(config.pipeline.detect_fps) +
                     " stream_slots=" + std::to_string(config.pipeline.stream_slots));
        logger->debug("debug logging enabled");
        logger->info("camera: " + config.cameras.front().id + " type=" + config.cameras.front().type);

        std::shared_ptr<sentinel::api::MqttClient> mqtt_client;
        std::shared_ptr<sentinel::api::MqttEventPublisher> mqtt_publisher;
        std::thread heartbeat_thread;
        std::atomic<bool> stop_heartbeat{false};

        if (config.mqtt.enabled) {
            logger->info("initializing mqtt client: host=" + config.mqtt.host + 
                         " port=" + std::to_string(config.mqtt.port) + 
                         " client_id=" + config.mqtt.client_id);
            
            mqtt_client = std::make_shared<sentinel::api::MqttClient>(
                config.mqtt.client_id, config.mqtt.host, config.mqtt.port,
                config.mqtt.username, config.mqtt.password, logger.get());
                
            if (mqtt_client->connect()) {
                logger->info("mqtt client connected");
                mqtt_publisher = std::make_shared<sentinel::api::MqttEventPublisher>(
                    mqtt_client, config.mqtt.client_id, logger.get());
                    
                // 启动心跳后台线程
                heartbeat_thread = std::thread([&]() {
                    while (!stop_heartbeat.load()) {
                        mqtt_publisher->publish_heartbeat(config);
                        
                        // 细粒度睡眠，以便在退出时能迅速响应
                        for (int i = 0; i < config.mqtt.heartbeat_interval_s * 10; ++i) {
                            if (stop_heartbeat.load()) break;
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                    }
                });
            } else {
                logger->error("mqtt client failed to connect, continuing without mqtt");
            }
        }

        // demo 示例 pipeline
        const auto result = sentinel::run_demo_pipeline(config, *logger, [&signal_fd]() {
            return signal_fd.consume_stop_signal();
        });

        stop_heartbeat = true;
        if (heartbeat_thread.joinable()) {
            heartbeat_thread.join();
        }

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

        if (result.frames_processed < config.pipeline.max_frames) {
            logger->warn("shutdown before reaching configured max_frames");
        }

        return 0;
    } catch (const std::exception& error) {
        bootstrap_logger.error("video_sentinel failed: " + std::string(error.what()));
        return 1;
    }
}
