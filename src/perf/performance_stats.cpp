#include "sentinel/perf/performance_stats.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace sentinel {
namespace {

/**
 * @brief 将浮点数格式化为固定两位小数。
 * @param value 待格式化数值。
 * @return 文本形式数值。
 */
std::string format_ms(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}

} // namespace

/**
 * @brief 构造性能统计器。
 * @param log_interval_frames 每多少帧输出一次聚合报告。
 */
PerformanceStats::PerformanceStats(int log_interval_frames)
    : log_interval_frames_(std::max(1, log_interval_frames))
{
}

/**
 * @brief 记录一帧性能数据。
 * @param sample 单帧性能采样。
 */
void PerformanceStats::observe(const FramePerformanceSample& sample)
{
    ++window_frames_;
    ++total_frames_;
    add_stage(capture_, sample.capture_ms);
    add_stage(preprocess_, sample.preprocess_ms);
    add_stage(detect_, sample.detect_ms);
    add_stage(output_, sample.output_ms);
    add_stage(frame_total_, sample.frame_total_ms);
}

/**
 * @brief 判断当前窗口是否达到输出条件。
 * @return 达到输出间隔返回 `true`。
 */
bool PerformanceStats::should_report() const noexcept
{
    return window_frames_ >= log_interval_frames_;
}

/**
 * @brief 判断窗口内是否存在尚未输出的采样。
 * @return 存在待输出采样返回 `true`。
 */
bool PerformanceStats::has_pending_samples() const noexcept
{
    return window_frames_ > 0;
}

/**
 * @brief 生成当前窗口报告并清空窗口统计。
 * @return 可直接写入日志的性能摘要文本。
 */
std::string PerformanceStats::make_report_and_reset()
{
    const auto frames = std::max(1, window_frames_);
    const auto fps = frame_total_.total_ms > 0.0
                         ? static_cast<double>(window_frames_) * 1000.0 / frame_total_.total_ms
                         : 0.0;

    std::ostringstream stream;
    stream << "perf window_frames=" << window_frames_
           << " total_frames=" << total_frames_
           << " fps=" << format_ms(fps)
           << " capture_avg_ms=" << format_ms(average_stage(capture_, frames))
           << " capture_max_ms=" << format_ms(capture_.max_ms)
           << " preprocess_avg_ms=" << format_ms(average_stage(preprocess_, frames))
           << " preprocess_max_ms=" << format_ms(preprocess_.max_ms)
           << " detect_avg_ms=" << format_ms(average_stage(detect_, frames))
           << " detect_max_ms=" << format_ms(detect_.max_ms)
           << " output_avg_ms=" << format_ms(average_stage(output_, frames))
           << " output_max_ms=" << format_ms(output_.max_ms)
           << " frame_avg_ms=" << format_ms(average_stage(frame_total_, frames))
           << " frame_max_ms=" << format_ms(frame_total_.max_ms);

    window_frames_ = 0;
    capture_ = StageAccumulator{};
    preprocess_ = StageAccumulator{};
    detect_ = StageAccumulator{};
    output_ = StageAccumulator{};
    frame_total_ = StageAccumulator{};
    return stream.str();
}

/**
 * @brief 生成 CSV 表头。
 * @return CSV 表头文本。
 */
std::string PerformanceStats::csv_header()
{
    return "frame_sequence,detections,capture_ms,preprocess_ms,detect_ms,output_ms,frame_total_ms\n";
}

/**
 * @brief 将单帧采样转换为 CSV 行。
 * @param sample 单帧性能采样。
 * @return CSV 数据行。
 */
std::string PerformanceStats::csv_row(const FramePerformanceSample& sample)
{
    std::ostringstream stream;
    stream << sample.frame_sequence << ','
           << sample.detections << ','
           << format_ms(sample.capture_ms) << ','
           << format_ms(sample.preprocess_ms) << ','
           << format_ms(sample.detect_ms) << ','
           << format_ms(sample.output_ms) << ','
           << format_ms(sample.frame_total_ms) << '\n';
    return stream.str();
}

/**
 * @brief 累加单个阶段耗时。
 * @param accumulator 待更新的阶段聚合器。
 * @param value_ms 本次耗时。
 */
void PerformanceStats::add_stage(StageAccumulator& accumulator, double value_ms) noexcept
{
    accumulator.total_ms += value_ms;
    accumulator.max_ms = std::max(accumulator.max_ms, value_ms);
}

/**
 * @brief 计算阶段平均耗时。
 * @param accumulator 阶段聚合器。
 * @param frames 窗口帧数。
 * @return 平均耗时，单位毫秒。
 */
double PerformanceStats::average_stage(const StageAccumulator& accumulator, int frames) noexcept
{
    return frames > 0 ? accumulator.total_ms / static_cast<double>(frames) : 0.0;
}

} // namespace sentinel
