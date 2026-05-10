# 边缘 AI 视频哨兵系统：项目结构

## 结构原则

第一版采用单仓库、模块化单体结构：

- C++ 负责视频接入、解码、推理、事件生成、存储和本地 API
- Web 前端负责实时看板、事件列表、配置界面和状态展示
- 配置、模型、部署脚本和文档独立放置，避免业务代码混在一起
- 当前优先支持一路视频闭环；多路视频不作为近期目标，只保留必要的结构弹性
- 不把模型文件、运行数据、构建产物和设备私有配置提交到仓库

这个结构的目标是让 MVP 能快速落地，同时保持后续扩展边界清楚。

## 推荐目录树

```text
project/
├── CMakeLists.txt
├── README.md
├── docs/
│   ├── project-background.md
│   ├── project-structure.md
│   ├── architecture.md
│   ├── api.md
│   ├── deployment.md
│   └── model-notes.md
├── config/
│   ├── sentinel.example.yaml
│   ├── cameras.example.yaml
│   └── rules.example.yaml
├── src/
│   ├── app/
│   ├── common/
│   ├── config/
│   ├── video/
│   ├── inference/
│   ├── analytics/
│   ├── events/
│   ├── storage/
│   ├── api/
│   └── media/
├── include/
│   └── sentinel/
├── web/
│   ├── package.json
│   ├── index.html
│   └── src/
├── tests/
│   ├── unit/
│   ├── integration/
│   └── fixtures/
├── scripts/
│   ├── setup-orange-pi.sh
│   ├── run-local.sh
│   └── benchmark-inference.sh
├── deploy/
│   ├── systemd/
│   └── docker/
├── models/
│   └── README.md
├── data/
│   └── README.md
├── third_party/
└── cmake/
```

## 顶层目录职责

| 目录 | 职责 |
| --- | --- |
| `docs/` | 项目背景、架构、接口、部署、模型说明等长期文档 |
| `config/` | 可提交的示例配置，不放现场真实摄像头地址和密钥 |
| `src/` | C++ 服务端实现，是系统核心运行时 |
| `include/` | 对外或跨模块共享的 C++ 头文件 |
| `web/` | 网页看板前端项目 |
| `tests/` | 单元测试、集成测试和测试视频/图片夹具 |
| `scripts/` | 本地开发、设备初始化、性能测试脚本 |
| `deploy/` | systemd、Docker、设备部署相关文件 |
| `models/` | 模型放置说明和占位，不直接提交大模型文件 |
| `data/` | 运行数据说明和占位，不提交事件数据库、截图和视频 |
| `third_party/` | 必要的第三方源码或本地补丁 |
| `cmake/` | CMake 模块、工具链文件和依赖查找脚本 |

## C++ 服务模块

`src/` 下按运行链路拆模块，而不是按技术类型堆文件。

```text
src/
├── app/
│   ├── main.cpp
│   ├── application.cpp
│   └── application.hpp
├── common/
│   ├── clock.hpp
│   ├── logger.hpp
│   ├── result.hpp
│   └── types.hpp
├── config/
│   ├── config_loader.cpp
│   ├── config_loader.hpp
│   └── sentinel_config.hpp
├── video/
│   ├── frame.hpp
│   ├── video_source.hpp
│   ├── rtsp_source.cpp
│   ├── camera_source.cpp
│   └── frame_decoder.cpp
├── inference/
│   ├── detector.hpp
│   ├── detection.hpp
│   ├── orange_pi_detector.cpp
│   ├── cpu_detector.cpp
│   └── postprocess.cpp
├── analytics/
│   ├── roi.hpp
│   ├── tracker.cpp
│   ├── rule_engine.cpp
│   └── event_builder.cpp
├── events/
│   ├── event.hpp
│   ├── event_bus.cpp
│   └── event_dispatcher.cpp
├── storage/
│   ├── event_store.hpp
│   ├── sqlite_event_store.cpp
│   └── snapshot_store.cpp
├── api/
│   ├── http_server.cpp
│   ├── websocket_hub.cpp
│   ├── camera_routes.cpp
│   ├── event_routes.cpp
│   └── health_routes.cpp
└── media/
    ├── overlay_renderer.cpp
    ├── snapshot_encoder.cpp
    └── stream_publisher.cpp
```

### 模块边界

- `app/` 只负责启动、依赖装配、生命周期管理和优雅退出
- `common/` 只能放无业务含义的基础工具，不能变成杂物目录
- `config/` 负责加载和校验配置，运行时模块只依赖已验证的配置对象
- `video/` 负责从摄像头或 RTSP 获取帧，输出统一的 `Frame`
- `inference/` 负责模型推理和后处理，输出统一的 `Detection`
- `analytics/` 负责区域、跟踪、去抖、停留时间和事件规则
- `events/` 负责事件发布、订阅和分发，不直接耦合具体存储或 UI
- `storage/` 负责 SQLite、截图、短片段和事件元数据持久化
- `api/` 负责 HTTP 和 WebSocket 接口，不写推理或规则逻辑
- `media/` 负责画框、截图编码和预览流输出

## Web 看板结构

第一版 Web 看板建议保持轻量，不做复杂后台系统。

```text
web/
├── package.json
├── index.html
├── public/
└── src/
    ├── app/
    ├── api/
    ├── components/
    ├── features/
    │   ├── live-view/
    │   ├── events/
    │   ├── rules/
    │   └── system-status/
    ├── hooks/
    ├── styles/
    └── types/
```

Web 端按功能拆分：

- `live-view/`：实时预览、检测框、当前告警状态
- `events/`：事件时间线、事件详情、截图查看
- `rules/`：区域、阈值、目标类别和冷却时间配置
- `system-status/`：摄像头连接、推理延迟、CPU、内存、温度和存储状态

## 配置文件结构

配置拆成三类，便于现场部署时独立调整：

```text
config/
├── sentinel.example.yaml  # 服务端口、日志、数据目录、性能参数
├── cameras.example.yaml   # 摄像头/RTSP 输入源
└── rules.example.yaml     # 检测目标、区域、阈值、事件策略
```

真实配置建议放在设备本地，例如：

```text
/etc/video-sentinel/sentinel.yaml
/etc/video-sentinel/cameras.yaml
/etc/video-sentinel/rules.yaml
```

仓库只提交 `.example.yaml`，避免泄露 RTSP 地址、账号、密码或现场位置信息。

## 运行数据结构

运行数据不进入版本库。默认可以放在：

```text
data/
├── events.sqlite
├── snapshots/
├── clips/
└── logs/
```

其中：

- `events.sqlite` 保存事件、目标、摄像头和规则命中记录
- `snapshots/` 保存事件截图
- `clips/` 后续用于保存事件前后短视频
- `logs/` 仅用于本地开发，生产环境可以交给 systemd journal 或日志服务

## 模型目录约定

模型文件通常较大，也可能受授权限制，不直接提交到仓库。

```text
models/
├── README.md
├── detector/
│   └── .gitkeep
└── classifier/
    └── .gitkeep
```

`models/README.md` 记录：

- 模型名称和版本
- 输入尺寸
- 类别列表
- 推理后端
- 转换命令
- 在 Orange Pi AI Pro 上的性能记录

## 测试结构

测试跟随模块边界组织：

```text
tests/
├── unit/
│   ├── config/
│   ├── analytics/
│   ├── events/
│   └── storage/
├── integration/
│   ├── video_pipeline/
│   ├── inference_pipeline/
│   └── api/
└── fixtures/
    ├── images/
    ├── videos/
    └── configs/
```

第一阶段优先覆盖：

- 配置解析和非法配置拒绝
- 规则引擎的去抖、冷却时间和区域入侵判断
- 事件合并逻辑，避免每帧生成一个事件
- 存储层写入和查询
- API 健康检查、事件查询和实时状态接口

## MVP 文件创建顺序

为了避免一开始铺太大，建议按下面顺序落地：

1. `README.md`
2. `CMakeLists.txt`
3. `config/*.example.yaml`
4. `src/app/`
5. `src/common/`
6. `src/config/`
7. `src/video/`
8. `src/inference/`
9. `src/analytics/`
10. `src/events/`
11. `src/storage/`
12. `src/api/`
13. `web/`
14. `tests/`
15. `deploy/`

先完成“视频输入 -> 抽帧 -> 推理 -> 事件 -> API -> 看板”的单路纵向闭环，再补齐复杂规则、部署自动化和运行可观测性。受当前开发板单 NPU 资源限制，多路实时推理不作为近期结构目标。

## 暂不引入的复杂度

第一版不建议过早引入：

- 微服务拆分
- 分布式消息队列
- 云端多租户后台
- 复杂权限系统
- 跨摄像头目标追踪
- 大规模视频存储
- 自动模型训练平台

这些能力只有在 MVP 跑通并确认真实场景需求后再扩展，否则会拖慢最关键的本地推理闭环。
