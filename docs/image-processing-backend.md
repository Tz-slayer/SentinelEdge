# 图像处理后端设计

## 当前结论

经过开发板性能测试，项目主链路已经收敛为：

```text
V4L2 MJPEG loaned buffer
  -> DVPP JPEGD
  -> DVPP VPC 输出 NV12/YUV420SP
  -> 静态 AIPP OM
  -> AscendCL 推理
  -> 纯 C++ YOLO 后处理
```

因此当前代码不再维护 OpenCV 预处理、OpenCV 图像后端、OpenCV YOLO 后处理和 V4L2 copy 模式。`pipeline.backend` 保留为配置字段，但只接受 `dvpp`，用于启动阶段明确校验。

## 接口边界

`FramePreprocessor` 负责把摄像头帧转换为模型输入。当前唯一实现是 `DvppFramePreprocessor`：

- 输入：V4L2 MJPEG 帧，推荐来自 `loaned` 缓冲区。
- 输出：静态 AIPP 模型消费的 `NV12` / `UINT8`。
- 优先路径：通过 `process_into()` 直接写入 AscendCL detector 暴露的 Device 输入缓冲区。
- 回退路径：当 detector 未提供 Device 输入时，输出 Host NV12 缓冲区，仅用于诊断和兼容。

`DetectionPostprocessor` 负责把模型输出解析为检测框。当前 `DvppYoloPostprocessor` 名称表示它属于 DVPP 主配置链路，但 YOLO decode/NMS 是纯 C++ CPU 逻辑，DVPP 本身不执行 NMS。

`ImageBackend` 只服务调试输出链路，负责把原始帧解码为 Host BGR24 并绘制检测框。它不再负责模型输入张量打包，避免调试图像链路反向影响推理主链路。

## 调试输出

`debug_image` 和 `mjpeg` 仍然可用于开发板本地调试：

- 解码和画框使用 `DvppImageBackend`。
- JPEG 文件保存和 MJPEG 帧编码暂时使用 OpenCV `imgcodecs`。
- 这部分属于调试输出能力，不属于主推理链路的 OpenCV 预处理路径。

后续如果要进一步减少 OpenCV 依赖，可以把 JPEG 编码替换为 DVPP JPEGE 或其他轻量编码器。
