#!/usr/bin/env python3
"""从 pipeline 性能 CSV 自动生成 Markdown 报告。"""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from statistics import mean


@dataclass(frozen=True)
class RunSummary:
    """单次 pipeline 性能测试的聚合结果。"""

    name: str
    frames: int
    detections: int
    capture_ms: float
    preprocess_ms: float
    detect_ms: float
    output_ms: float
    frame_total_ms: float
    max_frame_total_ms: float
    fps: float


def parse_args() -> argparse.Namespace:
    """解析命令行参数。"""
    parser = argparse.ArgumentParser(description="Generate SentinelEdge pipeline perf report")
    parser.add_argument("--csv-dir", required=True, help="包含性能 CSV 文件的目录")
    parser.add_argument("--output", required=True, help="输出 Markdown 报告路径")
    parser.add_argument("--title", default="SentinelEdge Pipeline 性能测试报告")
    parser.add_argument("--warmup", type=int, default=0, help="忽略前 N 帧作为预热")
    return parser.parse_args()


def load_csv(path: Path, warmup: int) -> RunSummary | None:
    """读取单个 CSV 并生成聚合结果。"""
    with path.open("r", encoding="utf-8", newline="") as file:
        rows = list(csv.DictReader(file))

    if warmup > 0:
        rows = rows[warmup:]
    if not rows:
        return None

    def values(field: str) -> list[float]:
        return [float(row[field]) for row in rows]

    frame_total = values("frame_total_ms")
    avg_total = mean(frame_total)
    fps = 1000.0 / avg_total if avg_total > 0.0 else 0.0

    return RunSummary(
        name=path.stem,
        frames=len(rows),
        detections=sum(int(row["detections"]) for row in rows),
        capture_ms=mean(values("capture_ms")),
        preprocess_ms=mean(values("preprocess_ms")),
        detect_ms=mean(values("detect_ms")),
        output_ms=mean(values("output_ms")),
        frame_total_ms=avg_total,
        max_frame_total_ms=max(frame_total),
        fps=fps,
    )


def format_float(value: float) -> str:
    """格式化浮点数，保留两位小数。"""
    return f"{value:.2f}"


def make_report(title: str, summaries: list[RunSummary], warmup: int) -> str:
    """生成 Markdown 报告正文。"""
    generated_at = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    lines = [
        f"# {title}",
        "",
        f"- 生成时间：`{generated_at}`",
        f"- 预热丢弃帧数：`{warmup}`",
        f"- 测试组数量：`{len(summaries)}`",
        "",
        "## 汇总",
        "",
        "| 测试组 | 有效帧数 | detections | capture_ms | preprocess_ms | detect_ms | output_ms | frame_total_ms | max_frame_total_ms | FPS |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]

    for item in summaries:
        lines.append(
            "| "
            f"`{item.name}` | "
            f"{item.frames} | "
            f"{item.detections} | "
            f"{format_float(item.capture_ms)} | "
            f"{format_float(item.preprocess_ms)} | "
            f"{format_float(item.detect_ms)} | "
            f"{format_float(item.output_ms)} | "
            f"{format_float(item.frame_total_ms)} | "
            f"{format_float(item.max_frame_total_ms)} | "
            f"{format_float(item.fps)} |"
        )

    lines.extend(
        [
            "",
            "## 指标说明",
            "",
            "- `capture_ms`：从视频源取一帧的耗时。",
            "- `preprocess_ms`：解码、缩放和生成模型输入张量的耗时。",
            "- `detect_ms`：Detector 对外暴露的一次检测耗时，当前包含 AscendCL 推理和 YOLO 后处理。",
            "- `output_ms`：视频输出通道耗时；本矩阵固定 `video_sink=none`，该值应接近 0。",
            "- `frame_total_ms`：单帧端到端耗时。",
            "- `FPS`：按 `1000 / frame_total_ms` 估算的处理吞吐。",
            "",
            "## 注意事项",
            "",
            "- 本报告只比较当前配置文件定义的矩阵变量。",
            "- 输出通道固定为 `none`，报告不覆盖 `debug_image` 或 `mjpeg` 的预览开销。",
            "- 若加入 `opencv` 后端，请确认模型输入格式、模型文件和预处理配置匹配。",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    """脚本入口。"""
    args = parse_args()
    csv_dir = Path(args.csv_dir)
    output = Path(args.output)

    summaries = [
        summary
        for summary in (load_csv(path, args.warmup) for path in sorted(csv_dir.glob("*.csv")))
        if summary is not None
    ]
    if not summaries:
        raise SystemExit(f"no valid CSV files found in {csv_dir}")

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(make_report(args.title, summaries, args.warmup), encoding="utf-8")
    print(f"report: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
