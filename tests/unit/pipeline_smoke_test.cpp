#include "sentinel/app/pipeline.hpp"
#include "sentinel/config/config_loader.hpp"
#include "sentinel/video/mock_video_source.hpp"

#include <cstdlib>
#include <iostream>
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
 * @brief 验证最小演示流水线能够完整跑通。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main(int argc, char** argv)
{
    const auto config_dir = argc > 1 ? argv[1] : "tests/fixtures/mock_config";

    const auto config = sentinel::load_config(config_dir);
    expect(config.service.port == 8080, "service port should come from config");
    expect(config.logging.backend == "stderr", "logging backend should come from config");
    expect(config.inference.backend == "mock", "inference backend should come from config");
    expect(config.pipeline.backend == "dvpp", "pipeline backend should come from config");
    expect(config.pipeline.mode == "serial", "pipeline mode should come from config");
    expect(config.pipeline.detect_fps == 30, "pipeline detect_fps should come from config");
    expect(config.pipeline.stream_slots == 2, "pipeline stream_slots should come from config");
    expect(config.pipeline.output_queue_size == 2,
           "pipeline output_queue_size should come from config");
    expect(config.preprocess.backend == "dvpp", "preprocess backend should be derived from pipeline backend");
    expect(config.postprocess.backend == "dvpp", "postprocess backend should be derived from pipeline backend");
    expect(config.overlay.backend == "dvpp", "overlay backend should be derived from pipeline backend");
    expect(!config.cameras.empty(), "at least one camera should be configured");
    expect(config.cameras.front().buffer_mode == "loaned", "camera buffer mode should come from config");
    expect(config.rules.hold_frames == 2, "event hold frame threshold should come from config");

    const auto default_result = sentinel::run_demo_pipeline(config);
    expect(default_result.frames_processed == 5, "pipeline should process configured frame count");
    expect(default_result.detections_seen >= 5, "mock detector should produce detections");
    expect(!default_result.events.empty(), "pipeline should emit at least one event");
    expect(default_result.events.front().label == "person", "first event should be a person event");

    sentinel::MockVideoSource injected_source(config.cameras.front());
    const auto injected_result = sentinel::run_demo_pipeline(config, injected_source);
    expect(injected_result.frames_processed == 5, "injected strategy should process configured frame count");
    expect(injected_result.detections_seen == default_result.detections_seen,
           "injected strategy should keep detection count stable");
    expect(injected_result.events.size() == default_result.events.size(),
           "injected strategy should keep event count stable");

    return 0;
}
