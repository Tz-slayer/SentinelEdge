# Edge AI Video Sentinel

边缘 AI 视频哨兵系统，用 Orange Pi AI Pro 接入摄像头或 RTSP 视频流，在本地实时识别人、车、异常行为或指定目标，并把结果推送到网页看板。

第一阶段目标不是完整安防平台，而是跑通最小闭环：

```text
配置加载 -> 视频帧输入 -> AI 推理 -> 事件生成 -> 本地输出/API -> Web 看板
```

## 当前状态

当前仓库已经落地项目目录和 C++ MVP 骨架：

- `docs/`：项目背景与结构设计
- `config/`：示例配置
- `src/`：C++ 服务端模块
- `include/`：跨模块头文件
- `tests/`：最小 smoke test
- `web/`：后续网页看板
- `models/`：模型放置说明
- `data/`：本地运行数据目录

当前 C++ 程序使用 mock 视频源和 mock 检测器，先验证工程结构、配置加载、事件生成和命令行输出。真实 RTSP、Orange Pi AI Pro 推理后端、HTTP API 和 Web 看板将在后续迭代中接入。

## 构建要求

- CMake 3.16+
- 支持 C++17 的编译器

## 构建与测试

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 运行

```bash
./build/video_sentinel config
```

预期输出会显示已加载的摄像头、处理帧数、检测数量和生成的事件。

## 配置

仓库只提交示例配置：

- `config/sentinel.example.yaml`
- `config/cameras.example.yaml`
- `config/rules.example.yaml`

生产环境中的真实配置建议放在设备本地，例如：

```text
/etc/video-sentinel/sentinel.yaml
/etc/video-sentinel/cameras.yaml
/etc/video-sentinel/rules.yaml
```

不要把真实 RTSP 地址、账号、密码、现场位置或密钥提交到仓库。

## 文档

- [项目背景](docs/project-background.md)
- [项目结构](docs/project-structure.md)
