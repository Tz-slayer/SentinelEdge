#include "sentinel/api/mqtt_client.hpp"
#include <stdexcept>

namespace sentinel {
namespace api {

namespace {
class MosquittoInit {
public:
    MosquittoInit() { mosquitto_lib_init(); }
    ~MosquittoInit() { mosquitto_lib_cleanup(); }
};
// 静态初始化保证 libmosquitto 在程序运行期间初始化一次
static MosquittoInit g_mosquitto_init;
} // namespace

MqttClient::MqttClient(const std::string& client_id, const std::string& host, int port,
                       const std::string& username, const std::string& password,
                       Logger* logger)
    : client_id_(client_id), host_(host), port_(port), logger_(logger)
{
    mosq_ = mosquitto_new(client_id_.c_str(), true, this);
    if (!mosq_) {
        throw std::runtime_error("Failed to create mosquitto instance");
    }

    if (!username.empty()) {
        mosquitto_username_pw_set(mosq_, username.c_str(), password.empty() ? nullptr : password.c_str());
    }

    std::string will_topic = "edge/" + client_id_ + "/sys/status";
    std::string will_payload = "{\"status\":\"offline\",\"reason\":\"unexpected_disconnect\"}";
    mosquitto_will_set(mosq_, will_topic.c_str(), will_payload.size(), will_payload.c_str(), 1, true);

    mosquitto_connect_callback_set(mosq_, on_connect_static);
    mosquitto_disconnect_callback_set(mosq_, on_disconnect_static);
    mosquitto_message_callback_set(mosq_, on_message_static);
}

MqttClient::~MqttClient() 
{
    disconnect();
    if (mosq_) {
        mosquitto_destroy(mosq_);
    }
}

bool MqttClient::connect() 
{
    if (running_) {
        return true;
    }
    
    int rc = mosquitto_connect_async(mosq_, host_.c_str(), port_, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        if (logger_) logger_->error("MQTT connection failed: " + std::string(mosquitto_strerror(rc)));
        return false;
    }

    running_ = true;
    loop_thread_ = std::thread([this]() {
        mosquitto_loop_forever(mosq_, -1, 1);
    });

    return true;
}

void MqttClient::disconnect() 
{
    if (running_) {
        running_ = false;
        mosquitto_disconnect(mosq_);
        if (loop_thread_.joinable()) {
            loop_thread_.join();
        }
    }
}

bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos, bool retain) 
{
    int rc = mosquitto_publish(mosq_, nullptr, topic.c_str(), payload.size(), payload.c_str(), qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        if (logger_) logger_->error("MQTT publish failed: " + std::string(mosquitto_strerror(rc)));
        return false;
    }
    return true;
}

bool MqttClient::subscribe(const std::string& topic, int qos) 
{
    int rc = mosquitto_subscribe(mosq_, nullptr, topic.c_str(), qos);
    if (rc != MOSQ_ERR_SUCCESS) {
        if (logger_) logger_->error("MQTT subscribe failed: " + std::string(mosquitto_strerror(rc)));
        return false;
    }
    return true;
}

void MqttClient::set_message_callback(MessageCallback cb) 
{
    message_callback_ = std::move(cb);
}

void MqttClient::on_connect_static(struct mosquitto*, void* obj, int rc) 
{
    auto* client = static_cast<MqttClient*>(obj);
    client->on_connect(rc);
}

void MqttClient::on_disconnect_static(struct mosquitto*, void* obj, int rc) 
{
    auto* client = static_cast<MqttClient*>(obj);
    client->on_disconnect(rc);
}

void MqttClient::on_message_static(struct mosquitto*, void* obj, const struct mosquitto_message* message) 
{
    auto* client = static_cast<MqttClient*>(obj);
    client->on_message(message);
}

void MqttClient::on_connect(int rc) 
{
    if (rc == 0) {
        if (logger_) logger_->info("MQTT connected to " + host_ + ":" + std::to_string(port_));
    } else {
        if (logger_) logger_->error("MQTT connect callback error: " + std::string(mosquitto_strerror(rc)));
    }
}

void MqttClient::on_disconnect(int rc) 
{
    if (logger_) logger_->info("MQTT disconnected, rc: " + std::to_string(rc));
}

void MqttClient::on_message(const struct mosquitto_message* message) 
{
    if (!message_callback_) return;
    
    std::string topic(message->topic);
    std::string payload;
    if (message->payload && message->payloadlen > 0) {
        payload.assign(static_cast<const char*>(message->payload), message->payloadlen);
    }
    
    message_callback_(topic, payload);
}

} // namespace api
} // namespace sentinel
