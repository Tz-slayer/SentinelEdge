# Third-Party Dependencies

本目录只保留本机交叉编译所需的最小 AscendCL 文件结构。

## AscendCL 必要目录

当前 CMake 默认读取：

```text
third_party/ascend-cann/8.0.0/
```

必须放置的目录和文件：

```text
third_party/ascend-cann/8.0.0/include/acl/
third_party/ascend-cann/8.0.0/devlib/linux/aarch64/libascendcl.so
```

`include/acl/` 建议整目录拷贝。当前代码直接包含 `acl.h`、`acl_mdl.h`、`acl_rt.h`，
但这些头文件可能继续包含 `acl_base.h`、`error_codes/` 等内部头文件。

## 不需要放入仓库的目录

当前项目的 AscendCL 推理代码只使用基础 ACL 模型加载和执行接口，因此本机交叉编译阶段不需要：

- `bin/`
- `python/`
- `simulator/`
- `ascendc/`
- `tikcpp/`
- `ccec_compiler/`
- `lib64/`
- `include/aclnn*`
- `include/acldvppop`
- `include/graph`
- `include/ge`
- `include/hccl`
- `include/register`

如果后续接入 DVPP、ACLNN、ATC 或自定义算子，再按实际头文件和库依赖补充。

## 注意

不要只拷贝 CANN 安装目录里的符号链接。需要确认上述头文件和 `libascendcl.so` 都是真实可访问的文件，否则 `cmake --preset prod` 会在配置阶段失败。
