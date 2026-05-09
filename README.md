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

当前 C++ 程序已经接入 V4L2 摄像头、AscendCL 推理后端骨架、日志策略和部署包生成流程。mock 输入只保留给本机单元测试夹具，开发板调试配置不再使用 mock。
图像预处理已经抽象为策略接口，用户通过 `pipeline.backend` 在 `opencv` 和 `dvpp` 之间切换。`opencv` 使用 CPU/OpenCV 完成解码、缩放和 NCHW FP32 张量打包；`dvpp` 面向静态 AIPP 模型时会执行 JPEGD 解码和 VPC 缩放，并输出 NV12/YUV420SP UINT8 输入给 `.om` 模型。
模型后处理已经抽象为策略接口，当前支持 YOLO FP32 输出解析、置信度过滤和 NMS；`dvpp` 配置路径下的 YOLO NMS 仍是 CPU 侧纯 C++ 实现，不伪装为 DVPP 硬件能力。
摄像头配置已经支持运行期缓冲区模式开关：`buffer_mode: "copy"` 表示稳定的复制模式，`buffer_mode: "loaned"` 表示 V4L2 `mmap` 缓冲区租借模式，可减少摄像头帧进入 pipeline 时的一次用户态复制。
图像处理能力已经统一抽象为 `ImageBackend`，当前 `opencv` 后端支持解码、缩放、张量打包和检测框绘制，`debug_image` 输出通道可以在开发板上保存带框 JPEG，用于验证检测结果是否可视化正确。
视频输出已经抽象为 `VideoSink`，当前支持 `none`、`debug_image` 和 `mjpeg`。`mjpeg` 使用 Linux socket 启动本地 HTTP MJPEG 调试预览，浏览器可以直接查看带框画面；后续再接入 MediaMTX 做生产级 RTSP/ WebRTC 网关。

## 构建要求

- CMake 3.16+
- 若使用 `CMakePresets.json`，建议 CMake 3.21+
- 支持 C++17 的编译器
- 使用 `pipeline.backend: "opencv"` 或启用 `debug_image` / `mjpeg` 输出时，需要安装 OpenCV 开发库，至少包含 `core`、`imgproc`、`imgcodecs` 组件
- 使用 `pipeline.backend: "dvpp"` 和静态 AIPP 模型时，`preprocess.output_layout` 应设置为 `NV12`，`preprocess.output_dtype` 应设置为 `UINT8`

## 构建与测试

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 开发与生产构建

本项目不维护两份 `CMakeLists.txt`。
当前的阶段划分是：

- `dev`：本机 x86_64 测试构建，用 `tests/fixtures/mock_config` 跑单元测试。
- `board-debug`：本机交叉编译 aarch64 Debug 产物，部署到测试开发板调试，使用真实 V4L2 和 AscendCL。
- `board-native-debug`：在开发板本机编译 Debug 产物，源码路径天然匹配 GDB，适合深入单步调试。
- `prod`：本机交叉编译 aarch64 Release 产物，部署到开发板上运行。

开发板不负责源码编译，只接收本机编译出的可执行文件、配置和模型。

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
- `ENABLE_ASCENDCL`
  是否编译 AscendCL 推理后端；开发机可以关闭，部署到 Orange Pi AI Pro 时启用
- `ENABLE_DVPP`
  是否编译 Ascend DVPP 图像链路；需要同时开启 `ENABLE_ASCENDCL`。运行期通过
  `pipeline.backend: "dvpp"` 启用 DVPP 可加速的图像处理环节
- `ENABLE_OPENCV_PREPROCESSOR`
  是否编译 OpenCV 图像预处理后端；使用当前 `config/dev` 和 `config/prod` 时应保持开启
- `ENABLE_OPENCV_POSTPROCESSOR`
  是否编译 OpenCV YOLO 后处理后端；使用当前 `config/dev` 和 `config/prod` 时应保持开启

### 推荐使用方式

本机开发构建：

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

测试开发板调试构建：

```bash
cmake --preset board-debug
cmake --build --preset board-debug
cmake --install build/board-debug
```

开发板本机调试构建：

```bash
cmake --preset board-native-debug
cmake --build --preset board-native-debug
cmake --install build/board-native-debug
```

`board-native-debug` 不使用交叉编译工具链，默认使用开发板上的 Ascend Toolkit：

```text
/usr/local/Ascend/ascend-toolkit/8.0.0/aarch64-linux
```

该路径下必须存在：

```text
/usr/local/Ascend/ascend-toolkit/8.0.0/aarch64-linux/include/acl/acl.h
```

`libascendcl.so` 会从以下常见目录自动查找：

```text
${ASCENDCL_ROOT}/lib64
${ASCENDCL_ROOT}/lib
${ASCENDCL_ROOT}/devlib/linux/aarch64
${ASCENDCL_ROOT}/devlib/aarch64
${ASCENDCL_ROOT}/devlib
```

如果你的库目录不在这些位置，可以显式指定：

```bash
cmake --preset board-native-debug \
  -DASCENDCL_LIB_DIR=/your/actual/lib/dir
```

摄像头缓冲区模式在 `config/*/cameras.yaml` 中配置：

```yaml
cameras:
  - id: "usb-camera-0"
    type: "v4l2"
    uri: "/dev/video0"
    buffer_mode: "copy"    # copy / loaned
```

`copy` 会在 `VIDIOC_DQBUF` 后把驱动缓冲区复制到 `Frame::data`，随后立即 `VIDIOC_QBUF`
归还给驱动。`loaned` 不复制帧载荷，而是让 `Frame` 持有 V4L2 buffer 租约，最后一个
`Frame` 释放时自动 `VIDIOC_QBUF`。当前 `loaned` 仍会在 DVPP/OpenCV 预处理阶段产生后续
格式转换和模型输入拷贝，它优化的是“摄像头帧进入 pipeline”这一段。

YOLO 后处理在 `config/*/sentinel.yaml` 中配置：

```yaml
pipeline:
  backend: "dvpp"            # opencv / dvpp

preprocess:
  output_width: 640
  output_height: 640
  output_layout: "NV12"      # 静态 AIPP OM 使用 NV12/YUV420SP_U8 输入
  output_dtype: "UINT8"
  normalize: false

postprocess:
  model_type: "yolo"
  output_layout: "channels_first"  # channels_first=[1,84,8400], anchors_first=[1,8400,84]
  num_classes: 80
  confidence_threshold: 0.25
  nms_iou_threshold: 0.45
  max_detections: 100
  merge_coco_vehicles: true
```

当前 OpenCV 后处理支持 YOLOv8/YOLO11 常见 FP32 输出形态：

- `channels_first`：属性维在前，例如 `[1, 84, 8400]`
- `anchors_first`：候选框维在前，例如 `[1, 8400, 84]`

如果开发板日志中 `after_threshold=0`，但 raw output 预览值明显不为空，优先检查
`output_layout`、`num_classes` 和置信度阈值是否与实际模型一致。

视频结果输出在 `config/*/sentinel.yaml` 中配置：

```yaml
pipeline:
  backend: "opencv"          # opencv / dvpp

overlay:
  enabled: true

output:
  video_sink: "mjpeg"        # none / debug_image / mjpeg
  debug_image_dir: "debug/frames"
  debug_image_interval: 1
  mjpeg_host: "0.0.0.0"
  mjpeg_port: 8081
  mjpeg_path: "/stream"
  mjpeg_quality: 80
  mjpeg_max_clients: 4
```

如果把 `video_sink` 改成 `debug_image`，运行后会把带框 JPEG 写到：

```text
./data/dev/debug/frames/
```

文件名格式为：

```text
frame-<camera_id>-<frame_sequence>.jpg
```

`config/dev` 当前默认启用 `mjpeg`。在开发板运行后，可以用浏览器访问：

```text
http://<开发板IP>:8081/stream
```

MJPEG 只用于本地调试预览，优点是简单、浏览器直接支持；缺点是带宽高、没有音频、不是生产级流媒体协议。生产配置默认仍使用 `output.video_sink: "none"`，后续需要正式流媒体网关时再接入 MediaMTX。

本机交叉编译部署构建：

```bash
cmake --preset prod
cmake --build --preset prod
```

`prod` preset 会在本机使用 `cmake/toolchains/aarch64-linux-gnu.cmake`
和 `aarch64-linux-gnu-g++` 生成 aarch64 可执行文件。开发板只需要接收构建产物并运行。
由于当前生产配置使用 OpenCV 预处理，交叉编译 `prod` 时还需要能被交叉工具链找到的 aarch64 OpenCV 开发库；如果没有目标架构 OpenCV sysroot，建议直接在开发板上使用 `board-native-debug` 先调试链路。

如果你的工具链前缀不同，可以显式覆盖：

```bash
cmake --preset prod -DAARCH64_TOOLCHAIN_PREFIX=<your-prefix>
cmake --build --preset prod
```

生成部署目录：

```bash
cmake --install build/prod
```

默认会把可执行文件和生产配置安装到：

```text
build/prod-package/
```

部署包目录包含：

- `bin/`
  生产可执行文件，例如 `video_sentinel` 和 `sentinel_camera_probe`
- `config/`
  完整配置目录，包含 `config/dev`、`config/prod` 和示例配置
- `models/`
  模型目录，目前只自动打包 `.om` 文件，不打包 `.onnx`、`.pt` 等训练或转换中间产物

### Docker Compose 交叉编译

如果本机交叉编译器版本过新，可能会生成依赖 `GLIBC_2.38`、`GLIBCXX_3.4.32`
等新版运行库符号的 aarch64 程序，而 Orange Pi AI Pro 当前系统只有
`GLIBC 2.35`。这种情况下应使用 Ubuntu 22.04 容器进行生产构建，让构建环境和板端
运行库版本保持一致。

本项目提供了 `compose.yaml` 和 `docker/cross-aarch64-ubuntu22.04/Dockerfile`。
它们只用于构建 aarch64 部署包，不会在本机运行 aarch64 可执行文件。

构建镜像并生成生产部署包：

```bash
docker compose run --rm --build prod-aarch64-builder
```

构建镜像并生成测试开发板调试包：

```bash
docker compose run --rm --build board-debug-aarch64-builder
```

生产构建服务会在容器内执行：

- `rm -rf build/prod build/prod-package`
- `cmake --preset prod`
- `cmake --build --preset prod`
- `cmake --install build/prod`
- `file` 检查产物架构
- `aarch64-linux-gnu-readelf` 检查 GLIBC / GLIBCXX 版本需求

最终产物仍然输出到宿主机仓库内：

```text
build/prod-package/
```

测试开发板调试包输出到：

```text
build/board-debug-package/
```

调试包会额外包含：

- `source/`
  精简源码快照，用于 GDB 显示源码和按源码行单步
- `debug/gdbinit`
  GDB 初始化脚本，内置 `/work -> ./source` 的源码路径映射

在开发板上可以这样启动 GDB：

```bash
cd ~/board-debug-package
gdb -x debug/gdbinit --args ./bin/video_sentinel config/dev
```

如果 `models/yolo/yolo26n_aipp_nv12.om` 存在，部署包中会生成：

```text
build/prod-package/models/yolo/yolo26n_aipp_nv12.om
```

这样 `config/prod/sentinel.yaml` 里的相对路径 `models/yolo/yolo26n_aipp_nv12.om`
在开发板上仍然可以从 `~/prod-package` 目录直接解析。

如果只想查看 Compose 展开后的实际配置，可以运行：

```bash
docker compose config
```

### 当前 preset 的行为差异

- `dev`
  本机 `Debug` 测试构建，默认读取 `tests/fixtures/mock_config`；mock 仅用于本机单元测试
- `asan`
  在 `dev` 基础上额外开启 sanitizers
- `board-debug`
  本机交叉编译 aarch64 `Debug`，关闭测试和 mock，开启 AscendCL 推理后端，默认读取 `config/dev`
- `board-native-debug`
  开发板本机 `Debug` 构建，关闭测试和 mock，开启 AscendCL 推理后端，默认读取 `config/dev`
- `prod`
  本机交叉编译 aarch64 `Release`，关闭测试和 mock，开启 AscendCL 推理后端

### 生产构建需要注意

`prod` preset 是部署构建，不是在开发板上构建。它默认关闭 mock 视频源，并启用 AscendCL，因此：

- 生产构建不会编译 `sentinel_tests`
- 若运行时仍然使用 `type: "mock"` 的摄像头配置，工厂会拒绝创建该视频源
- 本机构建时必须能访问 `ASCENDCL_ROOT/include/acl/acl.h` 和 `ASCENDCL_LIB_DIR/libascendcl.so`
- 当前配置使用 OpenCV 预处理，本机交叉编译时还必须提供目标架构的 OpenCV 库
- 当前默认 `ASCENDCL_ROOT` 是 `third_party/ascend-cann/8.0.0`
- 当前默认 `ASCENDCL_LIB_DIR` 是 `third_party/ascend-cann/8.0.0/devlib/linux/aarch64`
- 开发板运行时也必须能找到 AscendCL 运行时依赖，可以通过系统安装路径或 `LD_LIBRARY_PATH` 指向板端库目录

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
  面向测试开发板调试，默认启用 `v4l2` 摄像头和 `ascendcl` 推理，日志输出到 `stderr`
- `config/prod/`
  面向生产部署，默认启用 `v4l2` 摄像头和 `ascendcl` 推理，日志输出到 `syslog`
- `tests/fixtures/mock_config/`
  仅用于本机单元测试，保留 mock 摄像头和 mock 推理

默认加载规则：

- `dev` / `asan` preset 主要用于本机单元测试，测试命令显式读取 `tests/fixtures/mock_config`
- `board-debug` preset 构建出的程序默认读取 `config/dev`
- `board-native-debug` preset 构建出的程序默认读取 `config/dev`
- `prod` preset 构建出的程序默认读取 `config/prod`
- 你仍然可以通过命令行参数显式指定配置目录

`sentinel.yaml` 中目前除了 `service`、`runtime`、`pipeline`，还支持：

- `logging.backend`
  日志后端，当前支持 `stderr` 和 `syslog`
- `logging.level`
  最小输出级别，当前支持 `debug`、`info`、`warn`、`error`
- `logging.ident`
  当后端为 `syslog` 时使用的程序标识
- `inference.backend`
  推理后端，当前支持 `mock` 和 `ascendcl`
- `inference.model_path`
  `.om` 模型路径，AscendCL 后端会加载该文件
- `inference.device_id`
  AscendCL 设备编号，单设备通常为 `0`
- `pipeline.backend`
  图像处理链路后端，当前支持 `opencv` 和 `dvpp`。选择 `opencv` 时默认不开启 DVPP；选择 `dvpp` 时，系统会在 DVPP 能加速的环节优先使用 DVPP
- `preprocess.output_width` / `preprocess.output_height`
  模型输入图像尺寸，必须与 `.om` 模型输入匹配
- `preprocess.output_layout` / `preprocess.output_dtype`
  模型输入张量布局和数据类型。OpenCV 路径使用 `NCHW` / `FP32`；DVPP + 静态 AIPP 路径使用 `NV12` / `UINT8`
- `overlay.enabled`
  是否在输出图像上绘制检测框
- `output.video_sink`
  视频结果输出通道，当前支持 `none`、`debug_image` 和 `mjpeg`
- `output.debug_image_dir`
  `debug_image` 输出目录，相对于 `runtime.data_dir`
- `output.debug_image_interval`
  `debug_image` 每隔多少帧保存一张图
- `output.mjpeg_host`
  MJPEG 调试预览监听地址，默认 `0.0.0.0`
- `output.mjpeg_port`
  MJPEG 调试预览监听端口，默认 `8081`
- `output.mjpeg_path`
  浏览器访问路径，默认 `/stream`
- `output.mjpeg_quality`
  JPEG 编码质量，范围 `1..100`
- `output.mjpeg_max_clients`
  最大同时调试客户端数量
- `performance.enabled`
  是否启用 pipeline 性能统计
- `performance.log_interval_frames`
  每多少帧输出一次性能聚合日志
- `performance.csv_path`
  性能 CSV 输出路径；相对路径会写到 `runtime.data_dir` 下，空字符串表示不输出 CSV

例如：

```bash
./build/board-debug/video_sentinel config/dev
./build/board-native-debug/video_sentinel config/dev
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
- [图像处理后端设计](docs/image-processing-backend.md)
- [DVPP 预处理链路](docs/dvpp-preprocess.md)
- [MJPEG 调试预览设计](docs/mjpeg-preview.md)
- [测试策略讨论稿](docs/testing-strategy.md)

## 常用脚本

- `scripts/board-native-build.sh`
  在开发板本机执行 Debug 构建并安装到 `build/board-native-debug-package`
- `scripts/run-board-mjpeg-preview.sh`
  临时启用 MJPEG 预览并运行 `video_sentinel`
- `scripts/run-board-perf-matrix.sh`
  依次运行流水线后端和输出通道性能对照，并生成日志和 CSV；可用 `BACKENDS="opencv dvpp"` 控制
- `scripts/run-board-dvpp-probe.sh`
  在开发板上运行 `sentinel_dvpp_probe`，验证 DVPP runtime 和可选 JPEG 预处理
