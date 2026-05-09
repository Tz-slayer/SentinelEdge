# 多线程 Pipeline 设计草案

## 目标

当前 pipeline 是串行结构：

```text
capture -> preprocess -> detect -> output
```

在当前性能报告中，摄像头输入是 30 FPS，但完整处理链路约 16 FPS。下一阶段目标不是强行处理每一帧，而是让系统保持实时：

- 摄像头线程持续从 V4L2 取最新帧。
- 推理线程处理当前最新帧，处理不过来时主动丢弃旧帧。
- 输出线程异步消费推理结果，不阻塞采集和推理。
- 主链路默认围绕 `dvpp + loaned + static AIPP` 最优路径继续优化。

## 线程划分

第一版只拆三类线程：

```text
CaptureThread
  -> latest frame slot
InferenceThread
  -> bounded result queue
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

- 独占 `FramePreprocessor`、`Detector`、`EventBuilder`。
- 从 latest frame slot 获取最新帧。
- 执行 DVPP 预处理、AscendCL 推理、YOLO 后处理和事件生成。
- 将带检测结果的输出包投递给输出线程。

约束：

- AscendCL runtime、stream、model input buffer、DVPP channel 先不跨线程共享。
- 推理线程处理不过来时，允许跳过旧帧，只处理最新帧。
- 事件生成按实际处理帧推进，不以摄像头原始 30 FPS 作为强约束。

### OutputThread

职责：

- 独占 `VideoSink`。
- 消费推理线程输出的结果包。
- 执行 `none`、`debug_image` 或 `mjpeg` 输出。
- 输出队列满时丢弃旧结果，避免输出链路拖慢推理。

约束：

- 输出线程不反向阻塞推理线程。
- `mjpeg` 是调试预览，不作为生产主链路性能目标。
- 如果 `video_sink=none`，输出线程可以退化为空消费或不启动。

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

- 推理线程等待 `generation` 变化或 `closed=true`。
- 取走当前最新帧，槽置空。
- 推理线程只处理最新帧，不补处理历史帧。

选择这个结构的原因：

- 该系统是实时哨兵，不是离线视频分析。
- 处理不过来时，低延迟比逐帧完整处理更重要。
- 可以避免队列积压导致“画面越来越旧”。

### Inference -> Output：BoundedResultQueue

推理到输出使用小容量有界队列：

```cpp
struct PipelineOutputPacket {
    Frame frame;
    std::vector<Detection> detections;
    std::vector<Event> events;
    FramePerformanceSample perf;
};

class BoundedResultQueue {
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
- 输出线程总是尽量显示最近结果。
- 记录 `results_dropped_by_output` 作为运行指标。

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

### InferenceResult

推理线程内部可以使用更轻的中间结构：

```cpp
struct InferenceResult {
    int frame_sequence{0};
    std::string camera_id;
    std::vector<Detection> detections;
    std::vector<Event> events;
    FramePerformanceSample perf;
};
```

如果输出线程需要画框，必须保留原始 `Frame`：

```cpp
struct PipelineOutputPacket {
    Frame frame;
    std::vector<Detection> detections;
    std::vector<Event> events;
    FramePerformanceSample perf;
};
```

后续如果生产环境不需要实时带框视频，可以让 `output.video_sink=none` 时不传递 `Frame` 到输出线程，进一步减少 V4L2 loaned buffer 占用时间。

### PipelineStats

多线程后需要补充新的统计：

```cpp
struct PipelineRuntimeStats {
    std::uint64_t frames_captured{0};
    std::uint64_t frames_overwritten_before_inference{0};
    std::uint64_t frames_inferred{0};
    std::uint64_t output_packets_dropped{0};
    std::uint64_t output_packets_written{0};
};
```

这些指标用于判断：

- 是采集太快，推理跟不上。
- 还是输出太慢，结果队列积压。
- loaned buffer 是否被下游持有太久。

## 关闭流程

建议关闭顺序：

1. 主线程收到 SIGINT/SIGTERM，设置 `stop_requested=true`。
2. 通知 `LatestFrameSlot::closed=true`。
3. 采集线程停止读取视频源，关闭 `VideoSource`。
4. 推理线程处理完当前帧后关闭输出队列。
5. 输出线程消费完当前可用结果后关闭 `VideoSink`。
6. 主线程 `join()` 三个线程并汇总统计。

资源所有权：

- `VideoSource` 只属于采集线程。
- `FramePreprocessor`、`Detector`、`EventBuilder` 只属于推理线程。
- `VideoSink` 只属于输出线程。
- `Logger` 可以共享，但具体实现需要保证线程安全；第一版可以在外层加日志互斥锁。

## 关键风险

- `loaned` 帧跨线程后，如果输出线程也持有 `Frame`，V4L2 buffer 归还会变晚。
- 如果 V4L2 buffer 数量太少，输出线程持有帧可能影响采集。
- AscendCL 和 DVPP 对多线程上下文敏感，第一版必须让推理相关对象都留在同一个线程。
- debug 日志过多会影响性能，性能测试时应减少逐帧日志。
- 多线程性能指标不能再简单用单线程 `frame_total_ms` 表示系统吞吐，需要同时看 capture、inference、output 三条线程的速率。

## 第一版落地建议

第一版只做最小可控改造：

1. 新增 `LatestFrameSlot`。
2. 新增 `BoundedResultQueue`。
3. 保留当前串行 `run_demo_pipeline()`，新增 `run_threaded_pipeline()`。
4. 配置增加 `pipeline.mode: "serial" | "threaded"`，默认先保持 `serial`。
5. `threaded` 模式下固定单摄像头、单推理线程、单输出线程。
6. 性能报告增加：
   - captured FPS
   - inferred FPS
   - output FPS
   - overwritten frames
   - output dropped packets

## 待讨论问题

- 输出线程是否必须持有原始 `Frame`，还是只在 `debug_image/mjpeg` 时持有。
- `LatestFrameSlot` 覆盖旧帧时，是否需要显式记录被覆盖帧的 sequence。
- V4L2 mmap buffer 数量是否需要从当前值增加，避免 loaned 跨线程导致驱动队列饥饿。
- `EventBuilder` 是否应该按处理帧编号工作，还是按真实时间戳工作。
- 是否需要把 MJPEG 预览降频，例如每 N 个推理结果输出一次。
