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
- 若使用 `CMakePresets.json`，建议 CMake 3.21+
- 支持 C++17 的编译器

## 构建与测试

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 开发与生产构建

本项目不维护两份 `CMakeLists.txt`。  
开发阶段和生产阶段的区别通过 **一套工程定义 + 多组构建开关 + CMake Preset** 来管理。

### 当前构建开关

- `ENABLE_TESTS`
  是否编译测试目标并启用 `ctest`
- `ENABLE_MOCK_SOURCES`
  是否编译 mock 视频源策略
- `ENABLE_DEV_WARNINGS`
  是否启用额外编译警告
- `ENABLE_DEV_LOGGING`
  是否启用开发阶段运行时标识输出
- `ENABLE_SANITIZERS`
  是否启用 AddressSanitizer 和 UndefinedBehaviorSanitizer

### 推荐使用方式

开发构建：

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

带 Sanitizer 的开发构建：

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

生产构建：

```bash
cmake --preset prod
cmake --build --preset prod
```

### 当前 preset 的行为差异

- `dev`
  `Debug` 构建，开启测试、mock 视频源、开发日志
- `asan`
  在 `dev` 基础上额外开启 sanitizers
- `prod`
  `Release` 构建，关闭测试、关闭 mock 视频源、关闭开发日志

### 生产构建需要注意

`prod` preset 默认关闭了 mock 视频源，因此：

- 生产构建不会编译 `sentinel_tests`
- 若运行时仍然使用 `type: "mock"` 的摄像头配置，工厂会拒绝创建该视频源

这意味着开发配置和生产配置必须分开管理。

## CMake 构建产物说明

第一次接触 CMake 时，最容易困惑的是：为什么执行一次构建命令，仓库里会突然多出一堆文件和目录。

这里的关键点是：**CMake 会把“构建过程需要的中间文件”和“最终编译出来的产物”都放到构建目录里**。  
本项目采用的是“out-of-source build”，也就是：

- 源码仍然放在仓库根目录、`src/`、`include/`、`tests/` 等位置
- 构建过程中生成的文件放在单独的目录里，例如 `build/` 或 `build-v4l2/`

例如下面这条命令：

```bash
cmake -S . -B build-v4l2
```

含义是：

- `-S .`：源码目录是当前项目根目录
- `-B build-v4l2`：把所有构建相关文件输出到 `build-v4l2/`

所以你看到的 `build-v4l2/` 不是源码目录，而是一个**构建工作目录**。

### 常见构建产物

在 `build/` 或 `build-v4l2/` 里，常见文件大致分成几类：

1. CMake 自己的配置文件

- `CMakeCache.txt`
  保存本次配置结果，例如编译器路径、选项、缓存变量。
- `CMakeFiles/`
  保存 CMake 内部使用的大量中间文件，例如依赖关系、规则文件、配置日志。
- `Makefile`
  如果当前生成器是 Makefiles，这个文件就是后续 `cmake --build` 实际调用的构建入口。
- `cmake_install.cmake`
  安装阶段用到的脚本。

这些文件的作用是让 CMake 记住“这次工程是怎么配置出来的”，下次增量构建时就不用从零开始分析。

2. 测试描述文件

- `CTestTestfile.cmake`
  记录测试目标，供 `ctest` 使用。

它的作用是告诉 `ctest`：

- 有哪些测试
- 每个测试运行哪个可执行文件
- 测试参数是什么

3. 编译产物

- `libsentinel_core.a`
  这是静态库，里面打包了当前项目的核心 C++ 代码。
- `video_sentinel`
  主程序可执行文件。
- `sentinel_tests`
  流水线冒烟测试程序，用来验证配置加载、默认视频源选择、策略注入和事件生成链路是否正常。
- `sentinel_signal_tests`
  信号处理测试程序，用来验证 `LinuxSignalFd` 是否能正确接收并消费 `SIGINT`。
- `sentinel_camera_tests`
  本地摄像头输入层测试程序，用来验证 `CameraVideoSource` 在设备不存在等失败路径下是否能稳定返回错误。

其中：

- `video_sentinel` 是真正运行的应用程序
- 其余三个 `sentinel_*tests` 都是测试程序，不是给最终用户直接运行的主程序

4. 依赖和日志类中间文件

例如：

- `CMakeConfigureLog.yaml`
- `progress.marks`
- 各种 `.cmake` 规则文件

这类文件主要用于：

- 记录配置过程
- 描述编译依赖
- 支持增量构建
- 排查 CMake 配置问题

### 为什么这些文件不提交到 Git

这些文件都属于**构建产物**，不是项目源码的一部分。

原因有几个：

- 它们会随着编译器、系统环境、CMake 版本不同而变化
- 其中很多文件是机器相关的，换一台电脑就不一定还能复用
- 可执行文件和中间文件会让仓库变大、diff 变脏
- 真正应该被版本控制的是源码、配置样例、文档和测试，而不是本地编译结果

所以项目里的 `.gitignore` 已经忽略了：

- `build/`
- `build-*/`
- `CMakeFiles/`
- `CMakeCache.txt`
- `Testing/`
- 编译出来的 `.o`、`.a`、可执行文件等

### 什么时候会看到不同的构建目录

你可以为不同目的使用不同的构建目录，例如：

- `build/`
  默认本地开发构建目录
- `build-v4l2/`
  我在做 V4L2 相关重构和验证时单独使用的构建目录
- `build-debug/`
  以后如果你想专门做调试版构建，也可以这样命名

这是一种很常见的做法，因为：

- 不同目录可以保留不同配置结果
- 避免互相污染缓存
- 出现奇怪构建问题时，可以直接删除某个构建目录重新生成

### 实践建议

如果你怀疑某次 CMake 缓存脏了，最直接的做法通常不是手动删某几个文件，而是直接删整个构建目录再重新配置：

```bash
rm -rf build
cmake -S . -B build
cmake --build build
```

对这个项目来说，你可以把源码目录理解为“长期资产”，把 `build/`、`build-v4l2/` 这类目录理解为“随时可以删除并重新生成的工作目录”。

## 运行

```bash
./build/video_sentinel config
```

预期输出会显示已加载的摄像头、处理帧数、检测数量和生成的事件。

### 本地摄像头真机探针

如果你要在嵌入式设备上先验证本地摄像头采集链路，而不是直接跑完整 pipeline，可以使用：

```bash
./build/dev/sentinel_camera_probe --device /dev/video0 --width 1280 --height 720 --fps 30 --frames 5
```

这个工具会：

- 打开指定的 V4L2 设备
- 连续抓取若干帧
- 打印每一帧的分辨率、像素格式、字节数、时间戳
- 把首帧保存到 `./data/captures/`

当前 `CameraVideoSource` 优先请求 `MJPEG`。因此如果驱动最终返回的是 `MJPEG/JPEG`，首帧会直接保存为 `.jpg`，你可以在设备上把文件拷出来查看；如果驱动协商成其他格式，则会保存为 `.raw`，用于后续格式分析。

## 配置

当前仓库提供两套实际配置目录：

- `config/dev/`
  面向开发和测试，默认启用 `mock` 摄像头，便于在没有真实设备时跑通流程
- `config/prod/`
  面向生产构建，默认启用 `v4l2` 摄像头，禁用 mock 输入

默认加载规则：

- `dev` / `asan` preset 构建出的程序默认读取 `config/dev`
- `prod` preset 构建出的程序默认读取 `config/prod`
- 你仍然可以通过命令行参数显式指定配置目录

`sentinel.yaml` 中目前除了 `service`、`runtime`、`pipeline`，还支持：

- `logging.backend`
  日志后端，当前支持 `stderr` 和 `syslog`
- `logging.level`
  最小输出级别，当前支持 `debug`、`info`、`warn`、`error`
- `logging.ident`
  当后端为 `syslog` 时使用的程序标识

例如：

```bash
./build/dev/video_sentinel config/dev
./build/prod/video_sentinel config/prod
```

仓库根目录还保留了一组示例配置模板：

- `config/sentinel.example.yaml`
- `config/cameras.example.yaml`
- `config/rules.example.yaml`

它们更适合拿来参考字段格式，不再作为默认运行配置目录。

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
