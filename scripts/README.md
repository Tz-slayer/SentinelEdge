

## 1. 将 .pt 模型导出为 ONNX 格式

1. 首先创建 python 虚拟环境

```bash
python -m venv .venv
source .venv/bin/activate

pip install -U ultralytics onnx onnxsim onnxruntime
```

Utralytics 官方导出命令为

```bash
yolo export model=yolo26n.pt format=onnx opset=11
```

> 注意这里的 opset 版本尽量低一点，否则后续使用 atc 转换成 om 格式过程中可能出现报错，实测 11 是可以的。

## 2. 使用 atc 将 ONNX 模型转换为 OM 格式

```bash
atc --framework=5 --model=yolo26n.onnx --output=yolo26n --input_format=NCHW --input_shape="images:1,3,640,640" --ouput_type=FP32 --log=debug --soc_version=Ascend310B4
```

如果运行成功会看到如下输出：

```bash
(base) root@orangepiaipro:~/Downloads# atc \
  --framework=5 \
  --model=./yolo26n.onnx \
  --input_format=NCHW \
  --input_shape="images:1,3,640,640" \
  --output=./yolo26n \
  --soc_version=Ascend310B4 \
  --log=info
ATC start working now, please wait for a moment.
..................................
ATC run success, welcome to the next use.
```

使用如下命令可以查看生成的 OM 模型文件：

```bash
(base) root@orangepiaipro:~/Downloads# atc --mode=6 --om=./yolo26n.om
ATC start working now, please wait for a moment.
============ Display Model Info start ============
Original Atc command line: /usr/local/Ascend/ascend-toolkit/8.0.0/aarch64-linux/bin/atc.bin --framework=5 --model=./yolo26n.onnx --input_format=NCHW --input_shape=images:1,3,640,640 --output=./yolo26n_bs1 --soc_version=Ascend310B4 --log=info
system   info: atc_version[7.6.0.1.220], soc_version[Ascend310B4], framework_type[Onnx].
resource info: memory_size[24123392 B], weight_size[5170176 B], stream_num[1], event_num[0].
om       info: modeldef_size[752511 B], weight_data_size[5170176 B], tbe_kernels_size[393461 B], cust_aicpu_kernel_store_size[0 B], task_info_size[56254 B], so_store_size[0 B].
============ Display Model Info end   ============
...
ATC run success, welcome to the next use.
```

`soc_version[Ascend310B4]` 表示生成的 OM 模型文件是适用于 Ascend 310B4 芯片，`memory_size[24123392 B]` 表示生成的 OM 模型文件占用的内存大小为 24MB，`weight_size[5170176 B]` 表示生成的 OM 模型文件中权重数据占用的大小为 5MB。
