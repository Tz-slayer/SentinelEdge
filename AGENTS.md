# Project Agent Instructions

## C++ Documentation

- 所有新增或修改的 C++ 类、结构体、函数、方法以及重要内部辅助函数，都必须使用 Doxygen 风格注释。
- 注释统一使用中文，采用 `/** ... */` 形式。
- 注释中必须使用 `@brief` 描述用途，使用 `@param` 描述参数，使用 `@return` 描述返回值。
- 涉及副作用、资源所有权、Linux 系统调用约束、失败路径和线程/信号边界时，必须在 Doxygen 注释中明确说明。
- 函数体内部在流程不直观、资源切换、状态机、V4L2 队列操作、配置解析分支等关键位置，必须补必要的中文行内注释。
- 注释必须准确、具体，不允许空话、套话，也不要把显而易见的代码逐行翻译成注释。

## Linux-First Development

- This project is primarily for learning Linux application development.
- Prefer Linux-native interfaces for process control, signals, file descriptors, device I/O, polling, and buffer management when they are appropriate for the task.
- Do not introduce heavyweight abstraction layers such as OpenCV for camera capture when the same learning goal is better served by Linux-native interfaces such as V4L2.
