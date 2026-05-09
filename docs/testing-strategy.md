# 测试策略讨论稿

## 目标

本项目的测试目标不是只证明“程序能跑”，而是分层确认以下问题：

- 摄像头采集是否稳定，输出格式是否符合预期。
- 图像预处理是否把摄像头帧转换成模型需要的输入。
- AscendCL 推理是否能稳定加载 `.om` 模型并输出结果。
- YOLO 后处理是否能把模型输出解析成正确检测框。
- 本地画框、调试图和 MJPEG 预览是否能反映真实检测结果。
- 每个阶段的耗时是否可观测，后续能对比 OpenCV、DVPP、copy、loaned 等方案。

## 测试分层

### 本机自动化测试

本机测试只验证不依赖开发板硬件的逻辑，默认使用 mock 视频源和 mock 推理。

推荐命令：

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

当前重点覆盖：

- 配置加载和校验。
- 日志策略创建。
- 信号处理封装。
- 视频源工厂。
- 图像后端工厂。
- 预处理策略工厂。
- 后处理策略工厂。
- 输出通道工厂。
- mock pipeline 冒烟测试。

这类测试应该在每次提交前运行，目标是快速发现接口破坏、配置字段遗漏和基础逻辑回归。

### 开发板原生构建测试

开发板测试验证真实运行环境，包括 V4L2、AscendCL、OpenCV、模型文件和系统库。

推荐命令：

```bash
cmake --preset board-native-debug
cmake --build --preset board-native-debug -j$(nproc)
cmake --install build/board-native-debug

cd build/board-native-debug-package
./bin/video_sentinel config/dev
```

验收点：

- 程序能加载 `config/dev`。
- 能打开 `/dev/video0`。
- 能打开 OpenCV 图像预处理后端。
- 能加载 `.om` 模型。
- 能输出每帧 debug 日志。
- 能完成配置中的 `pipeline.max_frames`。
- 无崩溃、无死锁、无明显资源泄漏。

## 功能验收流程

### 摄像头采集验收

先单独跑摄像头探针，避免把摄像头问题和推理问题混在一起。

```bash
./bin/sentinel_camera_probe --device /dev/video0 --width 1280 --height 720 --fps 30 --frames 10
```

需要记录：

- 实际分辨率。
- 实际像素格式，例如 MJPEG、YUYV。
- 每帧 `bytes_used` 是否稳定。
- 首帧保存文件是否能打开。
- 摄像头不存在或被占用时错误信息是否清晰。

### 预处理验收

运行完整 pipeline，观察日志中的摄像头帧和张量信息。

需要确认：

- 摄像头输出 `width x height` 与预期一致。
- `pixel_format` 与摄像头探针一致。
- 模型输入张量 shape 为 `[1,3,H,W]`。
- 张量字节数等于 `1 * 3 * H * W * sizeof(float)`。
- 如果摄像头输出 MJPEG，解码后没有空帧。

### 推理和后处理验收

需要确认：

- AscendCL 模型能成功加载。
- 推理输出 buffer 数量和大小合理。
- 后处理日志包含候选框数量、阈值过滤数量、NMS 后数量。
- 人、车等类别能在实际画面中产生检测框。
- `confidence_threshold` 调高后检测数减少，调低后检测数增加。

### MJPEG 预览验收

`config/dev` 默认启用 MJPEG 调试预览：

```yaml
output:
  video_sink: "mjpeg"
  mjpeg_host: "0.0.0.0"
  mjpeg_port: 8081
  mjpeg_path: "/stream"
```

启动程序后，在浏览器访问：

```text
http://<开发板IP>:8081/stream
```

需要确认：

- 浏览器能看到实时画面。
- 检测框位置和调试 JPEG 中一致。
- 没有检测结果时仍能看到原始画面。
- 关闭浏览器后程序继续运行。
- 多个浏览器连接时不影响推理主循环。

MJPEG 只是调试预览，不作为生产流媒体方案。后续生产级预览再讨论 MediaMTX。

## 性能测试

性能测试必须覆盖阶段耗时，而不是只看总 FPS。

当前已落地第一版 pipeline 性能统计，采集：

```text
capture_ms       摄像头取帧耗时
preprocess_ms    解码、缩放、转张量耗时
detect_ms        Detector::detect() 总耗时，当前包含推理和后处理
output_ms        debug_image 或 MJPEG 输出耗时
frame_total_ms   单帧总耗时
fps              实际处理帧率
```

建议每 30 帧输出一次聚合数据：

```text
avg_ms
max_ms
min_ms
fps
```

后续需要对比的实验组：

- `output.video_sink: none`
- `output.video_sink: debug_image`
- `output.video_sink: mjpeg`
- `pipeline.backend: opencv`
- `pipeline.backend: dvpp`
- `camera.buffer_mode: copy`
- `camera.buffer_mode: loaned`
- 不同模型输入尺寸，例如 640、512、320。

none
  作为基线，表示不输出画面

debug_image - none
  约等于本地保存调试图带来的开销

mjpeg - none
  约等于网页预览带来的开销

mjpeg - debug_image
  可以对比网络发送和磁盘写入哪个更重


测试时必须固定摄像头分辨率、fps、模型、阈值和画面场景，否则性能数据不可比较。

常用脚本：

```bash
scripts/board-native-build.sh
scripts/run-board-perf-matrix.sh build/board-native-debug-package config/dev
scripts/run-board-mjpeg-preview.sh build/board-native-debug-package config/dev
```

`run-board-perf-matrix.sh` 默认依次运行 `none`、`debug_image`、`mjpeg` 三组输出通道，
并把日志和 CSV 写到安装包的 `data/dev/perf/` 目录。

## 当前缺口

当前还缺少以下测试能力：

- 开发板端长时间运行测试脚本。
- MJPEG 客户端连接和断开自动化测试。
- AscendCL 内部推理耗时和 YOLO 后处理耗时拆分。
- AscendCL 推理结果的固定样本回归测试。

建议下一步补 MJPEG 客户端连接测试，再拆分 AscendCL detector 内部耗时。
