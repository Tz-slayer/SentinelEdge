#include "sentinel/perf/performance_stats.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

/**
 * @brief 断言条件成立，否则输出错误并退出。
 * @param condition 待检查条件。
 * @param message 失败时输出的错误消息。
 */
void expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

/**
 * @brief 验证性能统计窗口聚合和 CSV 输出格式。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    sentinel::PerformanceStats stats(2);
    expect(!stats.should_report(), "empty stats should not report");
    expect(!stats.has_pending_samples(), "empty stats should have no pending samples");

    sentinel::FramePerformanceSample first;
    first.frame_sequence = 1;
    first.detections = 2;
    first.capture_ms = 1.0;
    first.preprocess_ms = 2.0;
    first.detect_ms = 3.0;
    first.output_ms = 4.0;
    first.frame_total_ms = 10.0;
    stats.observe(first);

    expect(!stats.should_report(), "one frame should not reach interval");
    expect(stats.has_pending_samples(), "one frame should be pending");

    sentinel::FramePerformanceSample second = first;
    second.frame_sequence = 2;
    second.detections = 1;
    second.capture_ms = 2.0;
    second.frame_total_ms = 20.0;
    stats.observe(second);

    expect(stats.should_report(), "two frames should reach interval");
    const auto report = stats.make_report_and_reset();
    expect(report.find("window_frames=2") != std::string::npos,
           "report should include window frame count");
    expect(report.find("throughput_fps=") != std::string::npos,
           "report should include wall-clock throughput fps");
    expect(report.find("latency_fps=66.67") != std::string::npos,
           "report should include latency-derived fps");
    expect(report.find("capture_avg_ms=1.50") != std::string::npos,
           "report should include averaged capture time");
    expect(report.find("frame_max_ms=20.00") != std::string::npos,
           "report should include max frame time");
    expect(!stats.has_pending_samples(), "report should reset pending window");

    const auto header = sentinel::PerformanceStats::csv_header();
    expect(header.find("frame_sequence,detections") == 0, "csv header should start with fields");
    const auto row = sentinel::PerformanceStats::csv_row(first);
    expect(row.find("1,2,1.00,2.00,3.00,4.00,10.00") == 0,
           "csv row should serialize sample values");

    return 0;
}
