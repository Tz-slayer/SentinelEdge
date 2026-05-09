# 图像处理后端设计

## 背景

项目中有多处需要图像处理能力：

- 摄像头帧解码，例如 MJPEG、YUYV 到可处理的图像。
- 推理前预处理，例如 resize、颜色转换、归一化和 NCHW FP32 张量打包。
- 推理后可视化，例如在原图或输出帧上绘制检测框、类别和置信度。
- 后续 RTSP 输出前的格式转换、缩放和硬件编码前准备。

这些能力都可能存在 OpenCV CPU 实现和 DVPP 硬件实现。如果每个阶段都各自封装
OpenCV/DVPP，会导致重复代码、策略数量膨胀，也不利于后续对比性能。

## 设计结论

图像能力统一抽象为 `ImageBackend`，但 pipeline 阶段不合并成一个大类。

也就是说：

```text
ImageBackend:
  opencv / dvpp

Pipeline Stage:
  FramePreprocessor
  VideoSink
```

`ImageBackend` 负责“图像能力”，例如解码、缩放、张量打包、绘制检测框。
`FramePreprocessor`、`VideoSink` 等阶段对象负责“业务语义”。

这种设计避免出现一个大而全的 `ImageProcessor`：

```cpp
class ImageProcessor {
public:
    decode();
    preprocess_to_tensor();
    draw_boxes();
    convert_for_encoder();
};
```

上面的类会越来越胖，并且会让 OpenCV/DVPP 实现被迫实现自己不关心的阶段逻辑。

## 当前接口

当前代码中的核心接口是：

```text
include/sentinel/image/image_backend.hpp
```

它提供：

```text
decode(Frame) -> ImageBuffer
resize(ImageBuffer, width, height) -> ImageBuffer
to_tensor(ImageBuffer, PreprocessConfig, frame_sequence, camera_id) -> TensorBuffer
draw_detections(ImageBuffer, detections) -> ImageBuffer
```

当前实现：

```text
OpenCvImageBackend
  已实现 MJPEG/JPEG、YUYV、RGB24、BGR24 解码
  已实现 resize
  已实现 NCHW FP32 张量打包
  已实现检测框绘制

DvppImageBackend
  已预留接口
  当前明确返回未实现
```

## 与 FramePreprocessor 的关系

`FramePreprocessor` 仍然保留，因为它表达的是 pipeline 阶段：

```text
Frame -> TensorBuffer
```

但 OpenCV 预处理实现内部不再直接写 OpenCV 解码和张量打包细节，而是组合
`OpenCvImageBackend`：

```text
OpenCvFramePreprocessor
  -> ImageBackend::decode
  -> ImageBackend::resize
  -> ImageBackend::to_tensor
```

这样后续如果实现 `DvppFramePreprocessor`，可以复用 `DvppImageBackend` 的硬件解码、
缩放和张量准备能力。

## 与画框的关系

画框不应该直接写死在 RTSP 推流或 Web 模块中。当前第一版通过
`DebugImageSink` 组合 `ImageBackend`，先把带框 JPEG 保存到本地，验证坐标、类别和
置信度正确后，再推进 RTSP 推流：

```text
Frame + Detection[]
  -> ImageBackend::decode
  -> ImageBackend::draw_detections
  -> VideoSink
```

第一版可用 OpenCV 绘制框，后续如果板端 DVPP/OSD 提供硬件绘制能力，则把实现放到
`DvppImageBackend::draw_detections` 中。

当前已落地：

```text
output.video_sink: "debug_image"
  -> DebugImageSink
  -> ImageBackend::decode
  -> ImageBackend::draw_detections
  -> 保存 JPEG 到 runtime.data_dir / output.debug_image_dir
```

生产配置默认仍使用 `output.video_sink: "none"`，避免现场运行时持续写磁盘。开发配置
默认使用 `debug_image`，便于在开发板上直接查看每一帧的画框结果。

## 配置建议

当前仍保留阶段级配置：

```yaml
preprocess:
  backend: "opencv"

overlay:
  enabled: true
  backend: "opencv"

output:
  video_sink: "debug_image"      # none / debug_image
  debug_image_dir: "debug/frames"
  debug_image_interval: 1
```

这样后续可以按阶段分别对比：

```text
预处理使用 DVPP，画框使用 OpenCV
预处理使用 OpenCV，画框使用 DVPP
全链路使用 OpenCV
全链路使用 DVPP
```

如果后续希望统一控制，也可以新增：

```yaml
image:
  default_backend: "opencv"
```

但不建议删除阶段级配置，因为不同阶段的硬件支持程度和性能瓶颈可能不同。

## 后续推进顺序

1. 保持 `OpenCvFramePreprocessor` 通过 `ImageBackend` 跑通 copy 模式。
2. 使用 `OpenCvImageBackend::draw_detections` 实现 OpenCV 画框。
3. 使用 `DebugImageSink` 保存带框 JPEG，验证坐标正确性。
4. 在开发板上用真实摄像头和 `.om` 模型确认调试图、日志和后处理结果一致。
5. 接入 RTSP 推流输出，把带框视频作为实时预览流暴露给 Web 页面。
6. 最后实现 `DvppImageBackend` 的硬件解码、缩放、张量准备或 OSD 能力。
