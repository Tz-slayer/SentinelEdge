# Video Sentinel 配置指南

本项目使用 YAML 格式的配置文件（通常为 `sentinel.yaml`）来管理系统运行行为。配置结构映射到代码中的 `SentinelConfig` 结构体。

## 1. 流水线配置 (`pipeline`)

该部分控制整个视觉处理链路的运行方式。

| 字段 | 默认值 | 说明 |
| :--- | :--- | :--- |
| `mode` | `threaded` | **运行模式**。`threaded` 开启多线程模式（采集、推理、输出物理隔离）；其他值开启同步单线程模式。 |
| `detect_fps` | `30` | **目标推理频率**。限制推理线程每秒处理的帧数，防止 NPU 空转。 |
| `stream_slots` | `2` | **NPU 并发槽位**。在 AscendCL 中开启的 Stream 数量，设为 2 可利用双缓冲覆盖上传延迟。 |
| `max_frames` | `5` | **最大运行帧数**。达到此数量后程序自动安全退出。 |
| `output_queue_size`| `2` | **输出队列容量**。推理结果发送给输出线程的缓冲区大小，满时会丢弃旧帧以保证实时性。 |

## 2. 摄像头配置 (`cameras`)

配置视频输入源。系统会读取列表中第一个 `enabled: true` 的摄像头。

| 字段 | 默认值 | 说明 |
| :--- | :--- | :--- |
| `type` | `mock` | **源类型**。`v4l2` 用于真实开发板摄像头，`mock` 用于自动化测试或无头运行。 |
| `uri` | `mock://demo` | **设备路径**。对于 V4L2，通常为 `/dev/video0`。 |
| `buffer_mode` | `loaned` | **缓冲区模式**。建议保留 `loaned`，实现零拷贝 V4L2 内存租借。 |
| `width` / `height` | 1280 / 720 | 采集的分辨率。 |
| `fps` | `10` | 摄像头底层硬件采集速度。 |

## 3. 推理相关配置

### 预处理 (`preprocess`)
控制图像进入模型前的格式转换。
- `output_layout`: 如 `NCHW` 或 `NHWC`。
- `output_dtype`: 如 `FP32` 或 `UINT8` (常用于量化模型)。

### 推理引擎 (`inference`)
- `backend`: 运行引擎。开发板必选 `acl`。
- `model_path`: `.om` 模型文件的绝对路径或相对路径。

### 后处理 (`postprocess`)
- `confidence_threshold`: **置信度阈值 (0.0~1.0)**。过滤掉分数过低的目标。
- `nms_iou_threshold`: **NMS 阈值**。用于抑制重叠的检测框。

## 4. 性能分析 (`performance`)

| 字段 | 默认值 | 说明 |
| :--- | :--- | :--- |
| `enabled` | `true` | 是否开启耗时统计。 |
| `log_interval_frames`| `30` | 每隔多少帧在控制台打印一次性能摘要。 |
| `csv_path` | (空) | 若配置，则将每一帧的详细耗时（采集、预处理、推理、绘图）写入 CSV 文件。 |

## 5. 规则与事件 (`rules`)

| 字段 | 默认值 | 说明 |
| :--- | :--- | :--- |
| `target_classes` | `["person"]` | 需要重点关注的类别列表。 |
| `min_confidence` | `0.5` | 触发事件的最低分数要求。 |
| `hold_frames` | `2` | 目标连续出现多少帧才判定为有效事件。 |
