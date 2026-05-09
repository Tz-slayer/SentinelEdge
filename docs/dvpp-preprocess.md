# DVPP 预处理链路

## 当前目标

OpenCV 链路已经跑通后，DVPP 第一阶段先实现 `copy` 模式对照链路：

```text
V4L2 MJPEG Frame
  -> DVPP JPEGD 解码为 YUV420SP/NV12
  -> DVPP VPC 缩放到模型输入尺寸
  -> Host 拷回
  -> CPU 侧 NV12 转 RGB
  -> CPU 侧打包 NCHW FP32 Tensor
  -> AscendCL Detector
```

这版没有做零拷贝，也没有把预处理输出直接交给模型 Device 输入。这样做的原因是先让
DVPP 和 OpenCV 在同一个 pipeline 中可切换、可计时、可回退，便于确认摄像头输入、
模型尺寸、后处理和日志都没有被一起改坏。

## 全链路 DVPP 配置

现在只需要将 `pipeline.backend` 切到 `dvpp`：

```yaml
pipeline:
  backend: "dvpp"
  max_frames: 300

preprocess:
  output_width: 640
  output_height: 640
  output_layout: "NCHW"
  output_dtype: "FP32"
  normalize: true

overlay:
  enabled: true
```

配置加载器会把 `pipeline.backend: "dvpp"` 统一映射到预处理、后处理和画框策略。需要注意这里的“DVPP 全链路”含义是：DVPP 能加速的图像处理环节优先走 DVPP，其他环节仍使用 CPU 侧实现：

- 预处理：使用 DVPP JPEGD 解码和 VPC 缩放。
- 后处理：使用独立的纯 C++ YOLO 解码和 NMS。YOLO NMS 不属于 DVPP 图像硬件能力，因此不伪装成硬件加速。
- 画框输出：使用 `DvppImageBackend` 解码摄像头 MJPEG 帧，随后在 Host BGR24 缓冲区绘制检测框。

## 当前限制

- `pipeline.backend: "dvpp"` 当前要求摄像头输入为 `V4L2_PIX_FMT_MJPEG`，因为 DVPP 预处理路径使用 JPEGD 解码。
- 输出固定支持 `NCHW` + `FP32`，与 OpenCV 预处理链路保持一致。
- DVPP 预处理只负责 JPEG 解码和缩放；NV12 到 RGB、归一化、NCHW 打包仍在 CPU 侧完成。
- DVPP 画框链路的解码使用 DVPP，但画框当前在 Host BGR24 缓冲区上完成，不是 DVPP OSD。
- `mjpeg` 和 `debug_image` 输出最终 JPEG 编码仍复用当前 OpenCV 输出实现。

## 配置方式

开发板本机 Debug 构建默认开启 DVPP 编译：

```bash
cmake --preset board-native-debug
cmake --build --preset board-native-debug -j"$(nproc)"
cmake --install build/board-native-debug
```

切换流水线后端：

```yaml
pipeline:
  backend: "dvpp"
```

模型输入尺寸仍在 `preprocess` 中配置：

```yaml
preprocess:
  output_width: 640
  output_height: 640
  output_layout: "NCHW"
  output_dtype: "FP32"
  normalize: true
```

DVPP 设备号会跟随 `inference.device_id`。代码中通过共享 `AclRuntimeSession`
避免 DVPP 图像处理和 AscendCL detector 重复初始化/释放 ACL runtime。

## 探针工具

安装包会包含：

```text
bin/sentinel_dvpp_probe
```

只验证 DVPP runtime、stream 和 channel 是否能打开：

```bash
./bin/sentinel_dvpp_probe --device-id 0
```

使用一张 JPEG/MJPEG 帧验证 DVPP 解码、缩放和 Tensor 打包：

```bash
./bin/sentinel_dvpp_probe --device-id 0 --width 640 --height 640 --jpeg /path/to/frame.jpg
```

也可以通过脚本运行：

```bash
scripts/run-board-dvpp-probe.sh build/board-native-debug-package /path/to/frame.jpg
```

## 性能对照

`scripts/run-board-perf-matrix.sh` 支持 `BACKENDS` 环境变量：

```bash
BACKENDS="opencv dvpp" SINKS="none" FRAMES=300 \
  scripts/run-board-perf-matrix.sh build/board-native-debug-package config/dev
```

建议先用 `SINKS="none"` 对比纯 pipeline 成本，再加入 `debug_image` 或 `mjpeg` 观察输出
链路额外开销。重点看 CSV 中的：

- `preprocess_ms`
- `detect_ms`
- `output_ms`
- `frame_total_ms`

## 后续演进

1. 先在开发板确认 `sentinel_dvpp_probe --jpeg` 能输出正确 Tensor 尺寸。
2. 再将 `config/dev/sentinel.yaml` 的 `pipeline.backend` 切到 `dvpp` 跑完整 pipeline。
3. 如果 DVPP 解码缩放稳定，再设计 Device Tensor 交接，减少 Host 拷回和 CPU 打包。
4. 最后再考虑 DVPP OSD/画框或 MediaMTX 推流，不和本阶段混在一起。
