# 多线程 Pipeline 设计草案

## 目标

当前 pipeline 是串行结构：

```text
capture -> preprocess -> detect -> output
```

在当前性能报告中，摄像头输入是 30 FPS，但完整处理链路约 16 FPS。下一阶段目标不是强行处理每一帧，而是让系统保持实时：

- 摄像头线程持续从 V4L2 取最新帧。
- 推理线程按 `detect_fps` 从最新帧槽取帧，内部管理多个 Ascend stream slot 提高设备吞吐。
- 输出线程异步消费推理线程发布的 `Frame + DetectionResult` 绑定包，不阻塞采集和推理。
- 主链路默认围绕 `dvpp + loaned + static AIPP` 最优路径继续优化。

## 线程划分

第一版只拆三类线程：

```text
CaptureThread
  -> latest frame slot
InferenceThread
  -> stream slot pool
  -> bounded output packet queue
OutputThread
```

### CaptureThread

职责：

- 独占 `VideoSource`。
- 调用 `read_frame()`。
- 将最新 `Frame` 发布给推理线程。
- 统计采集帧数、丢帧数和最近一次错误。

约束：

- `CameraVideoSource` 的 V4L2 `VIDIOC_DQBUF` / `VIDIOC_QBUF` 只在采集线程触发。
- `buffer_mode: loaned` 下，`Frame` 会持有 V4L2 buffer 租约；推理线程释放 `Frame` 时才归还驱动缓冲区。
- 采集线程不能因为推理慢而阻塞太久，否则 V4L2 驱动缓冲区会耗尽。

### InferenceThread

职责：

- 独占推理调度逻辑，按 `detect_fps` 从 latest frame slot 获取最新帧。
- 管理多个 stream slot，每个 slot 包含独立 Ascend stream、DVPP 资源和模型输入/输出缓冲区。
- 对空闲 slot 提交 `DVPP preprocess -> AscendCL inference` 异步任务。
- 回收已完成 slot，执行 YOLO 后处理和事件生成。
- 将同一帧对应的 `Frame + DetectionResult` 输出包投递给输出线程。

约束：

- AscendCL runtime 可以共享会话，但 stream、model input buffer、model output buffer、DVPP channel/desc/buffer 不跨 slot 共享。
- 同一 slot 内必须保证 DVPP VPC 写完模型 input buffer 后，再执行 `aclmdlExecuteAsync`。
- 推理线程处理不过来时，允许跳过旧帧，只处理最新帧。
- 事件生成按实际处理帧推进，不以摄像头原始 30 FPS 作为强约束。
- 第一版推荐 `stream_slots=2`，确认有效后再测试 3；slot 太多会增加 Device 内存占用和 V4L2 loaned buffer 持有时间。

### OutputThread

职责：

- 独占 `VideoSink`。
- 消费推理线程输出的 `PipelineOutputPacket`。
- 在输出线程执行画框、JPEG 编码、`debug_image` 或 `mjpeg` 输出。
- 输出队列满时丢弃旧结果，避免输出链路拖慢推理。

约束：

- 输出线程不反向阻塞推理线程。
- `mjpeg` 是调试预览，不作为生产主链路性能目标。
- 如果 `video_sink=none`，输出线程可以退化为空消费或不启动。
- 第一版输出线程只消费推理线程发布的绑定包，保证检测框画在对应帧上；“最新画面 + 最新检测结果”的插值式预览后续再单独设计。

## 线程间通信

### Capture -> Inference：LatestFrameSlot

采集到推理不使用普通无限队列，而使用“最新帧槽”：

```cpp
struct LatestFrameSlot {
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<Frame> frame;
    std::uint64_t generation{0};
    bool closed{false};
};
```

写入语义：

- 采集线程拿到新帧后加锁。
- 如果槽里已有未消费帧，直接覆盖，计入 `frames_overwritten`。
- 更新 `generation`。
- `notify_one()` 唤醒推理线程。

读取语义：

- 推理线程按 `detect_fps` 和空闲 stream slot 情况读取最新帧。
- 取走当前最新帧，槽置空。
- 推理线程只提交最新帧，不补处理历史帧。

选择这个结构的原因：

- 该系统是实时哨兵，不是离线视频分析。
- 处理不过来时，低延迟比逐帧完整处理更重要。
- 可以避免队列积压导致“画面越来越旧”。

### Inference 内部：StreamSlotPool

推理线程内部不是开启多个推理线程，而是使用一个线程管理多个异步 stream slot：

```cpp
struct StreamSlot {
    int slot_id{0};
    bool busy{false};
    Frame frame;
    TensorBuffer input;
    std::vector<ModelOutputBuffer> outputs;
    FramePerformanceSample perf;
};
```

每个 slot 的真实实现需要额外持有：

- `aclrtStream`
- AscendCL 模型输入 Device buffer 视图
- AscendCL 模型输出 buffer
- DVPP JPEGD/VPC 临时 buffer
- DVPP picture desc 和 resize config

调度语义：

- 有空闲 slot 且到达 `detect_fps` 取帧周期时，从 `LatestFrameSlot` 取最新帧并提交。
- 提交流程为 `DVPP JPEGD -> DVPP VPC -> aclmdlExecuteAsync`。
- 不允许提交后立即同步整个 stream，否则多 stream 会退化成串行。
- 推理线程循环检查 busy slot，回收已完成 slot 后再做后处理。
- 如果所有 slot 都忙，推理线程优先回收完成 slot；仍无完成 slot 时跳过本轮取帧。

顺序约束：

- 如果 DVPP 和模型执行使用同一个 stream，按 stream 内任务顺序保证 VPC 先于模型执行。
- 如果后续拆成不同 stream，必须通过 event 或显式同步保证模型读取 input buffer 前 VPC 已完成。
- 每个 slot 独占自己的输入/输出 buffer，避免不同帧之间覆盖 Device 内存。

### Inference -> Output：BoundedOutputQueue

推理到输出使用小容量有界队列：

```cpp
struct DetectionResult {
    int frame_sequence{0};
    std::string camera_id;
    std::int64_t timestamp_ns{0};
    std::vector<Detection> detections;
    std::vector<Event> events;
    FramePerformanceSample perf;
};

struct PipelineOutputPacket {
    Frame frame;
    DetectionResult result;
};

class BoundedOutputQueue {
public:
    bool push_drop_oldest(PipelineOutputPacket packet);
    std::optional<PipelineOutputPacket> pop_wait();
    void close();
};
```

推荐容量：

- `video_sink=none`：容量 1。
- `debug_image`：容量 1 或 2。
- `mjpeg`：容量 2。

队列策略：

- 队列满时丢弃最旧结果，不阻塞推理线程。
- 被丢弃的旧包析构后释放其中 `Frame`，尽快归还 V4L2 loaned buffer。
- 输出线程画框时只使用包内 `frame` 和包内 `result`，不混用全局最新帧。
- 记录 `output_packets_dropped` 作为运行指标。

## 数据结构

### Frame

沿用当前 `Frame`：

```cpp
struct Frame {
    int sequence;
    std::string camera_id;
    int width;
    int height;
    std::uint32_t pixel_format;
    std::int64_t timestamp_ns;
    std::size_t bytes_used;
    std::vector<std::uint8_t> data;
    const std::uint8_t* loaned_data;
    std::size_t loaned_size;
    std::shared_ptr<FramePayloadLease> payload_lease;
};
```

关键点：

- `Frame` 可以跨线程移动。
- `loaned` 模式依赖 `payload_lease` 的 RAII 生命周期归还 V4L2 buffer。
- 不要在多个线程同时读写同一个 `Frame` 实例；线程间通过 move 传递所有权。

### DetectionResult

推理完成后先生成结构化检测结果：

```cpp
struct DetectionResult {
    int frame_sequence{0};
    std::string camera_id;
    std::int64_t timestamp_ns{0};
    std::vector<Detection> detections;
    std::vector<Event> events;
    FramePerformanceSample perf;
};
```

如果输出线程需要画框，发布包必须保留同一帧的原始 `Frame`：

```cpp
struct PipelineOutputPacket {
    Frame frame;
    DetectionResult result;
};
```

这样可以避免“`frame 100` 的检测框画到 `frame 106` 上”的错位问题。

如果 `output.video_sink=none`，推理线程不应该把 `Frame` 继续交给输出线程，只发布统计和事件，或者直接在推理线程内汇总；这样可以缩短 V4L2 loaned buffer 持有时间。

### PipelineStats

多线程后需要补充新的统计：

```cpp
struct PipelineRuntimeStats {
    std::uint64_t frames_captured{0};
    std::uint64_t frames_overwritten_before_inference{0};
    std::uint64_t frames_submitted_to_inference{0};
    std::uint64_t frames_inferred{0};
    std::uint64_t stream_slots_busy_drops{0};
    std::uint64_t output_packets_dropped{0};
    std::uint64_t output_packets_written{0};
};
```

这些指标用于判断：

- 是采集太快，推理跟不上。
- 还是输出太慢，结果队列积压。
- stream slot 数量是否不足或过多。
- loaned buffer 是否被下游持有太久。

### 推荐配置

第一版建议增加这些配置：

```yaml
pipeline:
  mode: "threaded"
  detect_fps: 30
  stream_slots: 2
  output_queue_size: 2
```

含义：

- `detect_fps`：推理线程从 latest frame slot 取帧并提交推理的目标频率。
- `stream_slots`：推理线程内部可同时挂起的异步 AscendCL slot 数量，当前第一版固定为 `2`。
- `output_queue_size`：推理到输出的有界队列容量，满了丢最旧包。

## 关闭流程

建议关闭顺序：

1. 主线程收到 SIGINT/SIGTERM，设置 `stop_requested=true`。
2. 通知 `LatestFrameSlot::closed=true`。
3. 采集线程停止读取视频源，关闭 `VideoSource`。
4. 推理线程停止提交新帧，回收或取消所有 busy stream slot 后关闭输出队列。
5. 输出线程消费完当前可用结果后关闭 `VideoSink`。
6. 主线程 `join()` 三个线程并汇总统计。

资源所有权：

- `VideoSource` 只属于采集线程。
- stream slot、`FramePreprocessor`、`Detector`、`EventBuilder` 只属于推理线程。
- `VideoSink` 只属于输出线程。
- `Logger` 可以共享，但具体实现需要保证线程安全；第一版可以在外层加日志互斥锁。

## 关键风险

- `loaned` 帧跨线程后，如果输出线程也持有 `Frame`，V4L2 buffer 归还会变晚。
- 如果 V4L2 buffer 数量太少，多个 busy stream slot 和输出线程持有帧可能影响采集。
- AscendCL 和 DVPP 对多线程上下文敏感，第一版必须让推理调度和 slot 资源都留在同一个推理线程。
- 多 stream 不保证线性加速；如果单设备已经被模型推理吃满，`stream_slots=2` 也可能收益有限。
- 如果每次提交后立即 `aclrtSynchronizeStream()`，多 stream 会退化为串行。
- debug 日志过多会影响性能，性能测试时应减少逐帧日志。
- 多线程性能指标不能再简单用单线程 `frame_total_ms` 表示系统吞吐，需要同时看 capture、inference、output 三条线程的速率。

## 第一版落地建议

第一版只做最小可控改造：

1. 新增 `LatestFrameSlot`。
2. 新增 `BoundedOutputQueue`。
3. 保留当前串行 `run_demo_pipeline()`，新增 `run_threaded_pipeline()`。
4. 配置增加 `pipeline.mode: "serial" | "threaded"`，开发和生产配置默认使用 `threaded`，单元测试 fixture 保持 `serial`。
5. `threaded` 模式下固定单摄像头、单推理调度线程、单输出线程。
6. 推理线程内部先固定 `stream_slots=2`，每个 slot 独占一条 AscendCL stream、一套模型输入 Device buffer 和一套模型输出 Device buffer。
7. 输出线程第一版只消费 `PipelineOutputPacket{Frame, DetectionResult}`，保证帧和框严格匹配。
8. `video_sink=none` 时不启动输出线程，也不把 `Frame` 放入输出队列；推理线程完成事件和统计后立即释放帧租约，减少 V4L2 buffer 占用时间。
9. 性能报告增加：
   - captured FPS
   - submitted FPS
   - inferred FPS
   - output FPS
   - overwritten frames
   - busy slot drops
   - output dropped packets

## 后续可选模式

后续可以单独设计“最新画面 + 最新检测结果”的低延迟预览模式：

```text
OutputThread:
  latest Frame + latest DetectionResult
```

该模式类似把检测结果插到更新的画面上，延迟更低，但可能出现框和画面不完全匹配。若引入该模式，必须用 `frame_sequence` 或 `timestamp_ns` 判断检测结果是否过期，例如超过 3 帧或超过 100ms 就不画框。

仍需继续验证的问题：

- V4L2 mmap buffer 数量是否需要从当前值增加到 6 或 8，避免 loaned 跨线程导致驱动队列饥饿。
- `EventBuilder` 是否应该按处理帧编号工作，还是按真实时间戳工作。
- 是否需要把 MJPEG 预览降频，例如每 N 个推理结果输出一次。
