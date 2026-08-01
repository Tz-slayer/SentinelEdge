# 云边协同系统 MQTT 协议设计文档

本文档定义了边缘计算终端（C++ Video Sentinel）与云端中心控制平台（Java 后端）之间的 MQTT 通信协议。
所有的消息 Payload 统一采用 **JSON** 格式，并要求 UTF-8 编码。

---

## 1. Topic 设计规范

为了保证系统的可扩展性和权限隔离，Topic 采用以下层级结构设计：

*   **上行数据 (Edge -> Cloud)**: `edge/{device_id}/<module>/<action>`
*   **下行控制 (Cloud -> Edge)**: `cmd/{device_id}/<module>/<action>`
*   **设备响应 (Edge -> Cloud)**: `edge/{device_id}/<module>/<action>_reply`

> 注：`{device_id}` 为边缘终端的唯一标识符（如 MAC 地址或注册时分配的 UUID）。

---

## 2. 系统心跳与状态 (System Status)

边缘设备需要周期性上报其健康状态，供云端维护设备台账。同时利用 MQTT 的遗嘱机制（LWT）处理异常掉线。

### 2.1 遗嘱消息 (Broker 自动代发)
设备在连接 Broker 时需设置遗嘱消息。当设备意外断电或断网时，Broker 会自动向云端发布此消息。
*   **Topic**: `edge/{device_id}/sys/status`
*   **QoS**: 1
*   **Retain**: true
*   **Payload**:
```json
{
  "timestamp": 1722425000,
  "status": "offline",
  "reason": "unexpected_disconnect"
}
```

### 2.2 状态心跳上报 (上行)
*   **Topic**: `edge/{device_id}/sys/heartbeat`
*   **频率**: 建议每 30 秒上报一次。
*   **Payload**:
```json
{
  "timestamp": 1722425000,
  "status": "online",
  "hardware": {
    "cpu_usage": 45.2,
    "mem_usage": 60.1,
    "npu_temp": 55.4
  },
  "pipeline": {
    "fps": 29.5,
    "inference_latency_ms": 15
  },
  "active_model_version": "v1.2.0"
}
```

---

## 3. 告警事件流 (Event & Alarm)

当边缘端的 C++ 程序触发了本地的检测规则时，将告警元数据和截图上传到云端。
*注：为了节省 MQTT 宽带，截图建议通过 HTTP(S) 直传对象存储，MQTT 仅传递 URL。如果图片极小，也可选择 Base64 编码直接放入 MQTT。以下示例采用图片直传模式。*

### 3.1 告警事件上报 (上行)
*   **Topic**: `edge/{device_id}/event/alarm`
*   **Payload**:
```json
{
  "event_id": "evt_8a9b7c6d",
  "timestamp": 1722425123,
  "camera_id": "cam_01",
  "rule_id": "rule_intrusion_001",
  "event_type": "region_intrusion",
  "detections": [
    {
      "class": "person",
      "confidence": 0.89,
      "bbox": [120, 45, 300, 400]  // [x1, y1, x2, y2]
    }
  ],
  "snapshot_url": "http://oss.example.com/snapshots/{device_id}/evt_8a9b7c6d.jpg",
  "trigger_large_model": true  // 边缘端建议此事件是否需要提交大模型复核
}
```

---

## 4. 任务与规则配置下发 (Config & Tasks)

云端在前端画好 ROI 或配置好告警策略后，将 JSON 配置通过 MQTT 实时下发给边缘终端。

### 4.1 配置下发指令 (下行)
*   **Topic**: `cmd/{device_id}/config/update`
*   **Payload**:
```json
{
  "msg_id": "msg_001",
  "timestamp": 1722425200,
  "cameras": [
    {
      "id": "cam_01",
      "uri": "/dev/video0"
    }
  ],
  "rules": [
    {
      "rule_id": "rule_intrusion_001",
      "camera_id": "cam_01",
      "type": "intrusion",
      "target_classes": ["person", "vehicle"],
      "roi_polygon": [[100, 100], [500, 100], [500, 500], [100, 500]],
      "active_time": "22:00-06:00"
    }
  ]
}
```

### 4.2 配置下发响应 (上行)
*   **Topic**: `edge/{device_id}/config/update_reply`
*   **Payload**:
```json
{
  "msg_id": "msg_001",
  "code": 200,
  "message": "success"
}
```

---

## 5. 模型 OTA 推送 (Model OTA Update)

云端重新训练好了 YOLO 小模型，通知边缘设备去下载并热重载。

### 5.1 OTA 升级指令 (下行)
*   **Topic**: `cmd/{device_id}/model/ota`
*   **Payload**:
```json
{
  "msg_id": "msg_002",
  "model_name": "yolo_v8_aipp_nv12",
  "version": "v1.3.0",
  "download_url": "http://oss.example.com/models/yolo_v8_aipp_nv12_v1.3.0.om",
  "md5": "d41d8cd98f00b204e9800998ecf8427e",
  "action": "download_and_reload" // 或 "download_only"
}
```

### 5.2 OTA 进度与结果上报 (上行)
*   **Topic**: `edge/{device_id}/model/ota_reply`
*   **Payload**:
```json
{
  "msg_id": "msg_002",
  "status": "downloading",  // downloading / verifying / reloading / success / failed
  "progress": 45,
  "message": ""
}
```

---

## 6. 视频推流控制 (Stream Control)

云端用户想要在网页上观看某路摄像头的实时视频时，动态通知边缘设备开始推流到服务器。

### 6.1 推流启停指令 (下行)
*   **Topic**: `cmd/{device_id}/stream/control`
*   **Payload**:
```json
{
  "msg_id": "msg_003",
  "camera_id": "cam_01",
  "action": "start",  // start / stop
  "push_url": "rtmp://srs.example.com/live/{device_id}_cam_01",
  "with_overlay": true // 是否在视频中绘制 AI 检测框
}
```

### 6.2 推流指令响应 (上行)
*   **Topic**: `edge/{device_id}/stream/control_reply`
*   **Payload**:
```json
{
  "msg_id": "msg_003",
  "code": 200,
  "message": "success",
  "current_status": "streaming"
}
```
