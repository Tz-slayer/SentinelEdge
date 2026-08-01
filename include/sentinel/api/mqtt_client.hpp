#pragma once

#include "sentinel/logging/logger.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <mosquitto.h>

namespace sentinel {
namespace api {

/**
 * @brief MQTT 客户端，对 libmosquitto 的简单封装
 * 
 * 负责底层连接、重连、断开，以及发布和订阅回调的管理。
 * 遵循 Linux-First 原则，资源生命周期由 RAII 管理。
 */
class MqttClient {
public:
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

    /**
     * @brief 构造 MQTT 客户端
     * @param client_id 客户端唯一标识
     * @param host Broker 地址
     * @param port Broker 端口
     * @param username 认证用户名（空代表无认证）
     * @param password 认证密码（空代表无认证）
     * @param logger 日志策略
     */
    MqttClient(const std::string& client_id, const std::string& host, int port,
               const std::string& username = "", const std::string& password = "",
               Logger* logger = nullptr);

    /**
     * @brief 析构并自动断开连接
     */
    ~MqttClient();

    /**
     * @brief 建立与 Broker 的连接，并在后台启动循环线程
     * @return 成功返回 true，失败返回 false
     */
    bool connect();

    /**
     * @brief 优雅断开连接并停止后台线程
     */
    void disconnect();

    /**
     * @brief 发布消息到指定主题
     * @param topic 主题路径
     * @param payload 消息载荷
     * @param qos 服务质量等级，默认为 0
     * @param retain 是否保留消息，默认为 false
     * @return 成功返回 true，失败返回 false
     */
    bool publish(const std::string& topic, const std::string& payload, int qos = 0, bool retain = false);

    /**
     * @brief 订阅指定主题
     * @param topic 主题路径
     * @param qos 服务质量等级，默认为 0
     * @return 成功返回 true，失败返回 false
     */
    bool subscribe(const std::string& topic, int qos = 0);

    /**
     * @brief 设置接收到订阅消息时的回调函数
     * @param cb 回调函数
     */
    void set_message_callback(MessageCallback cb);

private:
    static void on_connect_static(struct mosquitto* mosq, void* obj, int rc);
    static void on_disconnect_static(struct mosquitto* mosq, void* obj, int rc);
    static void on_message_static(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message);

    void on_connect(int rc);
    void on_disconnect(int rc);
    void on_message(const struct mosquitto_message* message);

    std::string client_id_;
    std::string host_;
    int port_;
    Logger* logger_{nullptr};
    struct mosquitto* mosq_{nullptr};
    MessageCallback message_callback_;
    std::thread loop_thread_;
    std::atomic<bool> running_{false};
};

} // namespace api
} // namespace sentinel
