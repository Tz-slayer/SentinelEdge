# DVPP 预处理链路

## 当前目标

OpenCV 链路已经跑通后，DVPP 第一阶段先实现 `copy` / `loaned` 两种 V4L2 输入模式下的对照链路：

```text
V4L2 MJPEG Frame
  -> DVPP JPEGD 解码为 YUV420SP/NV12
  -> DVPP VPC 缩放到模型输入尺寸
  -> AscendCL 模型输入 Device buffer
  -> 静态 AIPP OM
```

当前默认面向 `models/yolo/yolo26n_aipp_nv12.om`。`loaned` 模式避免了 V4L2 `mmap`
缓冲区到 `Frame::data` 的用户态复制；静态 AIPP 模型避免了 CPU 侧 NV12->RGB、归一化、
NCHW FP32 打包。pipeline 会优先向 AscendCL detector 申请模型输入 Device buffer，
DVPP VPC 会直接写入该 buffer，detector 执行时跳过输入 Host->Device 拷贝。
DVPP 预处理器会在 `open()` 后复用 JPEGD 输出 Device buffer、Host fallback 的 VPC 输出
Device buffer、`acldvppPicDesc` 和 `acldvppResizeConfig`，避免在主循环内按帧创建和销毁这些资源。
用于 `debug_image` / `mjpeg` 的 DVPP 图像后端也会复用 JPEGD 输出 Device buffer、
Host NV12 缓冲区和 `acldvppPicDesc`。

## 全链路 DVPP 配置

现在只需要将 `pipeline.backend` 切到 `dvpp`：

```yaml
pipeline:
  backend: "dvpp"
  max_frames: 300

preprocess:
  output_width: 640
  output_height: 640
  output_layout: "NV12"
  output_dtype: "UINT8"
  normalize: false

overlay:
  enabled: true
```

配置加载器会把 `pipeline.backend: "dvpp"` 统一映射到预处理、后处理和画框策略。需要注意这里的“DVPP 全链路”含义是：DVPP 能加速的图像处理环节优先走 DVPP，其他环节仍使用 CPU 侧实现：

- 预处理：使用 DVPP JPEGD 解码和 VPC 缩放。
- 后处理：使用独立的纯 C++ YOLO 解码和 NMS。YOLO NMS 不属于 DVPP 图像硬件能力，因此不伪装成硬件加速。
- 画框输出：使用 `DvppImageBackend` 解码摄像头 MJPEG 帧，随后在 Host BGR24 缓冲区绘制检测框。

## 零拷贝与资源复用优化

当前已经完成的是 DVPP 推理主链路内能稳定落地的零拷贝和资源复用优化：

```text
V4L2 loaned MJPEG payload
  -> DVPP JPEGD 复用输出 Device buffer
  -> DVPP VPC 直接写入 AscendCL 模型输入 Device buffer
  -> AscendCL detector 跳过 Host-to-Device 输入拷贝
  -> 静态 AIPP OM
```

具体优化点：

- `camera.buffer_mode: "loaned"` 避免 V4L2 `mmap` 缓冲区到 `Frame::data` 的用户态复制。
- `Detector::mutable_input_tensor()` 暴露 AscendCL 模型输入 Device buffer 视图。
- `FramePreprocessor::process_into()` 允许 DVPP 预处理直接写入调用方提供的目标张量。
- `DvppFramePreprocessor` 在静态 AIPP 路径下让 VPC 输出直接落到模型输入 Device buffer。
- `AscendClDetector::detect()` 收到同一块模型输入 Device buffer 时跳过输入拷贝；收到其他 Device buffer 时只做 Device-to-Device 拷贝。
- `DvppFramePreprocessor` 复用 JPEGD 输出 Device buffer、Host fallback 的 VPC 输出 Device buffer、`acldvppPicDesc` 和 `acldvppResizeConfig`。
- `DvppImageBackend` 复用调试输出链路的 JPEGD 输出 Device buffer、Host NV12 缓冲区和 `acldvppPicDesc`。
- debug 日志会打印模型输入张量 `memory=host/device`，用于确认当前是否走到 Device buffer 直写路径。

这部分优化的核心收益是减少 `DVPP -> Host -> AscendCL input` 往返拷贝，以及减少主循环中每帧 `aclrtMalloc/free` 和 DVPP 描述对象创建销毁。

## 当前限制

- `pipeline.backend: "dvpp"` 当前要求摄像头输入为 `V4L2_PIX_FMT_MJPEG`，因为 DVPP 预处理路径使用 JPEGD 解码。
- DVPP 预处理支持 `NV12` + `UINT8`，用于静态 AIPP OM；也保留 `NCHW` + `FP32` 兼容旧模型。
- `camera.buffer_mode: "loaned"` 只减少 V4L2 出队后到 pipeline 的帧复制；V4L2 MJPEG 码流本身仍位于 Host。
- 使用静态 AIPP OM 时，NV12 到 RGB、归一化等动作由模型内 AIPP 完成，不再由 CPU 打包 NCHW FP32。
- 静态 AIPP 路径下，DVPP VPC 输出会直接写入 AscendCL 模型输入 Device buffer，避免 `DVPP -> Host -> AscendCL input` 往返拷贝。
- JPEGD 输出 Device buffer、Host fallback resize buffer、DVPP PicDesc 和 ResizeConfig 已复用，不再按帧申请释放。
- DVPP 图像后端的调试输出链路已复用 JPEGD buffer、Host NV12 buffer 和 PicDesc。
- DVPP 画框链路的解码使用 DVPP，但画框当前在 Host BGR24 缓冲区上完成，不是 DVPP OSD。
- `mjpeg` 和 `debug_image` 输出最终 JPEG 编码仍复用当前 OpenCV 输出实现。

因此，在当前 USB 摄像头 MJPEG、静态 AIPP OM、CPU YOLO 后处理和 OpenCV 画框架构下，DVPP/零拷贝方向已经接近边界。继续优化主要不再属于 DVPP 零拷贝问题，而是后处理逻辑、输出调度或整体架构调整。

如果后续要继续追求更彻底的硬件化，需要改变链路形态：

- 摄像头输入改成支持 DMA-BUF 或硬件视频解码直接接入的路径。
- YOLO decode/NMS 下沉到模型、Ascend 自定义算子或其他 Device 侧后处理。
- 可视化输出改为硬件 OSD、硬件编码或独立的低频调试输出链路。
- 主推理链路和 Web 预览链路彻底解耦，避免调试预览影响推理吞吐。

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
  output_layout: "NV12"
  output_dtype: "UINT8"
  normalize: false
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

## 性能测试

当前阶段先只测试一条 pipeline，避免把 backend、输出通道和配置变更混在一起：

```bash
scripts/run-board-pipeline-perf.sh build/board-native-debug-package config/dev
```

脚本会临时修改安装包中的 `sentinel.yaml`，强制开启 `performance.enabled`，设置
`pipeline.max_frames` 和 `performance.csv_path`，运行结束后自动恢复原配置。默认输出：

- 日志：`build/board-native-debug-package/data/dev/perf/pipeline.log`
- CSV：`build/board-native-debug-package/data/dev/perf/pipeline.csv`

常用参数只保留这几个：

```bash
FRAMES=500 INTERVAL=50 \
  scripts/run-board-pipeline-perf.sh build/board-native-debug-package config/dev
```

如果确实需要临时覆盖当前配置，也只跑一条 pipeline：

```bash
BACKEND=dvpp SINK=none FRAMES=300 \
  scripts/run-board-pipeline-perf.sh build/board-native-debug-package config/dev
```

重点看 CSV 中的：

- `preprocess_ms`
- `detect_ms`
- `output_ms`
- `frame_total_ms`

## 后续演进

1. 先在开发板确认 `pipeline.backend: "dvpp"` 能跑完整 pipeline，并在 debug 日志中看到 `memory=device`。
2. 观察 `preprocess_ms`、`detect_ms`、CPU 占用和端到端帧率，确认 Device buffer 直写路径对实际开发板是否有收益。
3. 如果链路稳定，再考虑模型输出 Device 侧后处理、DVPP OSD 或硬件编码，减少输出可视化链路的 Host 侧处理。
4. 最后再考虑 MediaMTX 推流，不和本阶段混在一起。
