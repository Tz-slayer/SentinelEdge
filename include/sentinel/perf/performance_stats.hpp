#pragma once

#include <cstddef>
#include <string>

namespace sentinel {

/**
 * @brief 单帧 pipeline 性能采样。
 *
 * 该结构保存一帧在各阶段的耗时。当前 `detect_ms` 包含检测器对外暴露的完整
 * `Detector::detect()` 调用耗时；若后续需要拆分 AscendCL 推理和 YOLO 后处理，
 * 应继续扩展检测器接口或调试信息。
 */
struct FramePerformanceSample {
    int frame_sequence{0};
    int detections{0};
    double capture_ms{0.0};
    double preprocess_ms{0.0};
    double detect_ms{0.0};
    double output_ms{0.0};
    double frame_total_ms{0.0};
};

/**
 * @brief pipeline 性能窗口聚合器。
 *
 * 该类按固定帧数窗口累计耗时，生成平均值、最大值和估算 FPS。它不直接写日志，
 * 调用方负责把 `make_report_and_reset()` 返回的文本交给日志系统。
 */
class PerformanceStats {
public:
    /**
     * @brief 构造性能统计器。
     * @param log_interval_frames 每多少帧输出一次聚合报告。
     */
    explicit PerformanceStats(int log_interval_frames);

    /**
     * @brief 记录一帧性能数据。
     * @param sample 单帧性能采样。
     */
    void observe(const FramePerformanceSample& sample);

    /**
     * @brief 判断当前窗口是否达到输出条件。
     * @return 达到输出间隔返回 `true`。
     */
    bool should_report() const noexcept;

    /**
     * @brief 判断窗口内是否存在尚未输出的采样。
     * @return 存在待输出采样返回 `true`。
     */
    bool has_pending_samples() const noexcept;

    /**
     * @brief 生成当前窗口报告并清空窗口统计。
     * @return 可直接写入日志的性能摘要文本。
     */
    std::string make_report_and_reset();

    /**
     * @brief 生成 CSV 表头。
     * @return CSV 表头文本。
     */
    static std::string csv_header();

    /**
     * @brief 将单帧采样转换为 CSV 行。
     * @param sample 单帧性能采样。
     * @return CSV 数据行。
     */
    static std::string csv_row(const FramePerformanceSample& sample);

private:
    /**
     * @brief 单个阶段的窗口聚合值。
     */
    struct StageAccumulator {
        double total_ms{0.0};
        double max_ms{0.0};
    };

    /**
     * @brief 累加单个阶段耗时。
     * @param accumulator 待更新的阶段聚合器。
     * @param value_ms 本次耗时。
     */
    static void add_stage(StageAccumulator& accumulator, double value_ms) noexcept;

    /**
     * @brief 计算阶段平均耗时。
     * @param accumulator 阶段聚合器。
     * @param frames 窗口帧数。
     * @return 平均耗时，单位毫秒。
     */
    static double average_stage(const StageAccumulator& accumulator, int frames) noexcept;

    int log_interval_frames_{1};
    int window_frames_{0};
    int total_frames_{0};
    StageAccumulator capture_;
    StageAccumulator preprocess_;
    StageAccumulator detect_;
    StageAccumulator output_;
    StageAccumulator frame_total_;
};

} // namespace sentinel
