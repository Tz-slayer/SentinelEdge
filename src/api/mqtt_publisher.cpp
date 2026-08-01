#include "sentinel/api/mqtt_publisher.hpp"
#include <nlohmann/json.hpp>
#include <chrono>

using json = nlohmann::json;

namespace sentinel {
namespace api {

MqttEventPublisher::MqttEventPublisher(std::shared_ptr<MqttClient> client, const std::string& device_id, Logger* logger)
    : client_(std::move(client)), device_id_(device_id), logger_(logger) 
{
    event_topic_ = "edge/" + device_id_ + "/event/alarm";
    heartbeat_topic_ = "edge/" + device_id_ + "/sys/heartbeat";
}

void MqttEventPublisher::publish_event(const Event& event) 
{
    if (!client_) return;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    json j;
    j["event_id"] = event.id;
    j["timestamp"] = timestamp;
    j["camera_id"] = event.camera_id;
    j["rule_id"] = "default_rule"; // MVP 占位，后续可从事件元数据中获取
    j["event_type"] = event.type;
    
    // 我们从 Event 聚合结构反向构造一个单目标的 detections 数组
    json det;
    det["class"] = event.label;
    det["confidence"] = event.confidence;
    det["bbox"] = {0, 0, 0, 0}; // MVP 占位：当前 Event 尚未包含触发框，后续可扩展 Event 结构

    j["detections"] = json::array({det});
    
    // MVP 占位：组装虚拟的 OSS 地址
    j["snapshot_url"] = "http://oss.example.com/snapshots/" + device_id_ + "/" + event.id + ".jpg";
    
    // 示例策略：置信度偏低时建议大模型复核
    j["trigger_large_model"] = (event.confidence < 0.7);

    std::string payload = j.dump();
    if (logger_) logger_->info("Publishing MQTT event: " + payload);
    
    // 告警事件使用 QoS 1 确保至少到达一次
    client_->publish(event_topic_, payload, 1, false);
}

void MqttEventPublisher::publish_heartbeat(const SentinelConfig& config) 
{
    if (!client_) return;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    json j;
    j["timestamp"] = timestamp;
    j["status"] = "online";
    
    // MVP 占位：可以通过读取 /proc/stat 获取真实 CPU 和内存
    j["hardware"] = {
        {"cpu_usage", 45.0}, 
        {"mem_usage", 60.0},
        {"npu_temp", 55.0}
    };
    
    j["pipeline"] = {
        {"fps", config.pipeline.detect_fps}, 
        {"inference_latency_ms", 15}
    };
    
    j["active_model_version"] = config.inference.model_path.string();

    std::string payload = j.dump();
    if (logger_) logger_->debug("Publishing MQTT heartbeat: " + payload);
    
    // 心跳使用 QoS 0 即可
    client_->publish(heartbeat_topic_, payload, 0, false);
}

} // namespace api
} // namespace sentinel
