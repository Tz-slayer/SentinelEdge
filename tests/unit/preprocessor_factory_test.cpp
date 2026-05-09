#include "sentinel/preprocess/preprocessor_factory.hpp"

#include <cstdlib>
#include <exception>
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
 * @brief 验证图像预处理策略工厂只暴露 DVPP 主线策略。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    sentinel::PreprocessConfig config;
    config.backend = "dvpp";
    config.output_width = 8;
    config.output_height = 6;
    config.output_layout = "NV12";
    config.output_dtype = "UINT8";
    config.normalize = false;

    auto dvpp = sentinel::create_frame_preprocessor(config);
    expect(dvpp->kind() == "dvpp", "factory should create DVPP preprocessor");
    expect(!dvpp->open(), "host build without DVPP should clearly report unavailable status");
    expect(!dvpp->last_error().empty(), "DVPP preprocessor should expose error text");

    bool unsupported_thrown = false;
    try {
        config.backend = "opencv";
        static_cast<void>(sentinel::create_frame_preprocessor(config));
    } catch (const std::exception&) {
        unsupported_thrown = true;
    }
    expect(unsupported_thrown, "non-DVPP preprocess backend should throw");

    return 0;
}
