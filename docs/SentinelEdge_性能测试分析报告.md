# SentinelEdge Pipeline 性能测试报告

- 生成时间：`2026-05-09 22:05:05`
- 预热丢弃帧数：`5`
- 测试组数量：`4`

## 汇总

| 测试组 | 有效帧数 | detections | capture_ms | preprocess_ms | detect_ms | output_ms | frame_total_ms | max_frame_total_ms | FPS |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `backend-dvpp-buffer-copy` | 295 | 295 | 0.04 | 0.98 | 59.94 | 0.00 | 61.07 | 65.02 | 16.38 |
| `backend-dvpp-buffer-loaned` | 295 | 60 | 0.01 | 0.97 | 58.09 | 0.00 | 59.16 | 60.53 | 16.90 |
| `backend-opencv-buffer-copy` | 295 | 85 | 0.05 | 20.52 | 60.23 | 0.00 | 80.98 | 82.42 | 12.35 |
| `backend-opencv-buffer-loaned` | 295 | 6 | 0.02 | 20.61 | 59.88 | 0.00 | 80.67 | 81.70 | 12.40 |

## 指标说明

- `capture_ms`：从视频源取一帧的耗时。
- `preprocess_ms`：解码、缩放和生成模型输入张量的耗时。
- `detect_ms`：Detector 对外暴露的一次检测耗时，当前包含 AscendCL 推理和 YOLO 后处理。
- `output_ms`：视频输出通道耗时；本矩阵固定 `video_sink=none`，该值应接近 0。
- `frame_total_ms`：单帧端到端耗时。
- `FPS`：按 `1000 / frame_total_ms` 估算的处理吞吐。

## 注意事项

- 本报告只比较当前配置文件定义的矩阵变量。
- 输出通道固定为 `none`，报告不覆盖 `debug_image` 或 `mjpeg` 的预览开销。
- 若加入 `opencv` 后端，请确认模型输入格式、模型文件和预处理配置匹配。