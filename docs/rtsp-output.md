# RTSP 输出设计

## 目标

本阶段目标是把已经验证过的“本地画框结果”输出成实时视频流，让 VLC、ffplay 或后续
Web 转码网关能够看到带框画面。

第一版不直接在项目内实现完整 RTSP 协议栈，而是新增 `RtspVideoSink`：

```text
Frame + Detection[]
  -> ImageBackend::decode
  -> ImageBackend::draw_detections
  -> Linux pipe
  -> ffmpeg stdin
  -> H.264
  -> RTSP listen
```

这样可以先验证端到端效果，同时保留后续替换硬件编码、DVPP/OSD 或自研推流服务的边界。

## 模块职责

`RtspVideoSink` 位于输出层：

```text
include/sentinel/output/rtsp_video_sink.hpp
src/output/rtsp_video_sink.cpp
```

它负责：

- 通过 `ImageBackend` 把摄像头帧解码成 Host BGR24 图像。
- 根据 `overlay.enabled` 决定是否绘制检测框。
- 使用 `pipe2` 创建到 ffmpeg 的标准输入管道。
- 使用 `fork` 创建子进程。
- 使用 `dup2` 把管道读端绑定到子进程 `stdin`。
- 使用 `execvp` 启动 ffmpeg。
- 使用 `poll` 和非阻塞 `write` 把每帧 BGR24 原始数据写入 ffmpeg。
- 使用 `waitpid` 检测和回收 ffmpeg 子进程。

`RtspVideoSink` 不负责模型推理、后处理、事件生成或 Web 页面展示。

## 配置

```yaml
output:
  video_sink: "rtsp"
  rtsp_url: "rtsp://0.0.0.0:8554/sentinel"
  rtsp_fps: 10
  rtsp_encoder: "libx264"
  rtsp_write_timeout_ms: 1000
  ffmpeg_path: "ffmpeg"
```

字段说明：

- `video_sink`：选择输出策略，`rtsp` 表示启用 RTSP 输出。
- `rtsp_url`：ffmpeg listen 地址。`0.0.0.0` 表示监听所有网卡。
- `rtsp_fps`：传给 ffmpeg 的原始帧率。
- `rtsp_encoder`：ffmpeg 编码器。当前默认 `libx264`，后续可替换为硬件编码器。
- `rtsp_write_timeout_ms`：写 ffmpeg 标准输入的超时时间，避免网络或客户端阻塞拖死主循环。
- `ffmpeg_path`：ffmpeg 可执行文件路径或命令名。

## 开发板验证

开发板需要安装 ffmpeg：

```bash
ffmpeg -version
```

启动程序：

```bash
./bin/video_sentinel config/dev
```

同网段电脑使用 VLC 或 ffplay 访问：

```text
rtsp://<开发板IP>:8554/sentinel
```

调试时建议先打开播放器，再启动 `video_sentinel`。ffmpeg listen 模式可能会等待客户端建连；
如果没有客户端消费，`RtspVideoSink` 会在 `rtsp_write_timeout_ms` 后返回失败，避免主循环长期卡死。

## 后续演进

当前实现的优势是简单、可验证、Linux 系统调用边界清晰。后续优化方向：

1. 将写入 ffmpeg 的逻辑改成独立输出线程和有界队列，慢客户端只丢帧，不阻塞推理。
2. 把 `libx264` 替换为开发板可用的硬件编码器。
3. 实现 DVPP/OSD 画框和硬件编码链路，减少 Host 内存拷贝。
4. 增加 Web 可直接播放的 HLS、WebRTC 或 RTSP-to-Web 网关。
