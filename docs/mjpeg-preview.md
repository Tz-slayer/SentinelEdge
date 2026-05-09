# MJPEG 调试预览设计

## 目标

开发板本地网页预览当前只承担调试职责：确认摄像头画面、检测框、类别和置信度是否正确。
因此第一版不接入 ffmpeg，也不启动 RTSP server，而是使用浏览器原生支持的 MJPEG。

调试链路：

```text
V4L2/RTSP 输入
  -> 图像预处理
  -> AscendCL 推理
  -> YOLO 后处理
  -> ImageBackend 画框
  -> JPEG 编码
  -> HTTP multipart/x-mixed-replace
  -> 浏览器
```

## 模块职责

`MjpegHttpSink` 位于输出层：

```text
include/sentinel/output/mjpeg_http_sink.hpp
src/output/mjpeg_http_sink.cpp
```

它负责：

- 使用 `socket`、`setsockopt`、`bind`、`listen` 创建 HTTP 监听 socket。
- 使用 `accept4` 接收浏览器连接。
- 使用 `ImageBackend` 解码视频帧和绘制检测框。
- 使用 OpenCV 把 BGR24 图像编码为 JPEG。
- 使用 `send` 发送 `multipart/x-mixed-replace` MJPEG 数据。
- 使用非阻塞 socket，慢客户端会被断开，避免阻塞推理主循环。

它不负责生产级流媒体分发，也不替代后续 MediaMTX。

## 配置

```yaml
output:
  video_sink: "mjpeg"
  mjpeg_host: "0.0.0.0"
  mjpeg_port: 8081
  mjpeg_path: "/stream"
  mjpeg_quality: 80
  mjpeg_max_clients: 4
```

字段说明：

- `video_sink`：选择输出策略，`mjpeg` 表示启用 HTTP MJPEG 调试预览。
- `mjpeg_host`：监听地址。`0.0.0.0` 表示监听所有网卡。
- `mjpeg_port`：监听端口。
- `mjpeg_path`：浏览器访问路径，当前实现不做复杂路由，保留该字段用于日志和后续 Web 集成。
- `mjpeg_quality`：JPEG 编码质量，范围 `1..100`。
- `mjpeg_max_clients`：最大调试客户端数量。

## 开发板验证

启动程序：

```bash
./bin/video_sentinel config/dev
```

浏览器访问：

```text
http://<开发板IP>:8081/stream
```

MJPEG 的优点是简单、浏览器直接支持、不需要 ffmpeg 或流媒体网关。缺点是带宽高、没有音频、
没有拥塞控制，不适合作为最终生产流媒体方案。

## 后续演进

后续需要正式流媒体能力时，再引入 MediaMTX：

```text
video_sentinel
  -> 编码/推送
  -> MediaMTX
  -> RTSP / HLS / WebRTC
```

这部分应作为独立阶段设计，不应和当前 MJPEG 调试预览混在一起。
