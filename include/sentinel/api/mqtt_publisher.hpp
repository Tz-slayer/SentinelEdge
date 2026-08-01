#pragma once

#include "sentinel/api/mqtt_client.hpp"
#include "sentinel/common/types.hpp"
#include "sentinel/logging/logger.hpp"
#include <memory>
#include <string>

namespace sentinel {
namespace api {

/**
 * @brief MQTT 事件发布器
 *
 * 负责将系统内部产生的 Event 序列化为符合协议规范的 JSON，
 * 并通过 MQTT 客户端发布到对应的 topic。
 */
class MqttEventPublisher {
public:
    /**
     * @brief 构造函数
     * @param client 已经连接的 MQTT 客户端指针
     * @param device_id 边缘终端的唯一标识
     * @param logger 日志策略
     */
    MqttEventPublisher(std::shared_ptr<MqttClient> client, const std::string& device_id, Logger* logger = nullptr);

    /**
     * @brief 发布告警事件
     * @param event 系统内部生成的结构化事件
     */
    void publish_event(const Event& event);

    /**
     * @brief 发布设备状态心跳
     * @param config 系统运行配置，用于提取当前模型等信息
     */
    void publish_heartbeat(const SentinelConfig& config);

private:
    std::shared_ptr<MqttClient> client_;
    std::string device_id_;
    Logger* logger_{nullptr};
    std::string event_topic_;
    std::string heartbeat_topic_;
};

} // namespace api
} // namespace sentinel
