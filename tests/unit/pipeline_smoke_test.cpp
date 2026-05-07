#include "sentinel/app/pipeline.hpp"
#include "sentinel/config/config_loader.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main(int argc, char** argv)
{
    const auto config_dir = argc > 1 ? argv[1] : "config";

    const auto config = sentinel::load_config(config_dir);
    expect(config.service.port == 8080, "service port should come from config");
    expect(!config.cameras.empty(), "at least one camera should be configured");
    expect(config.rules.hold_frames == 2, "event hold frame threshold should come from config");

    const auto result = sentinel::run_demo_pipeline(config);
    expect(result.frames_processed == 5, "pipeline should process configured frame count");
    expect(result.detections_seen >= 5, "mock detector should produce detections");
    expect(!result.events.empty(), "pipeline should emit at least one event");
    expect(result.events.front().label == "person", "first event should be a person event");

    return 0;
}
