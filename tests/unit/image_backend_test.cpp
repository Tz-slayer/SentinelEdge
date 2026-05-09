#include "sentinel/image/image_backend_factory.hpp"

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
 * @brief 验证图像处理后端工厂只暴露 DVPP 主线策略。
 * @return 成功返回 `0`，失败返回非零值。
 */
int main()
{
    auto dvpp = sentinel::create_image_backend("dvpp");
    expect(dvpp->kind() == "dvpp", "factory should create DVPP image backend");
    expect(!dvpp->open(), "host build without DVPP should report unavailable status");
    expect(!dvpp->last_error().empty(), "DVPP image backend should expose error text");

    bool unsupported_thrown = false;
    try {
        static_cast<void>(sentinel::create_image_backend("opencv"));
    } catch (const std::exception&) {
        unsupported_thrown = true;
    }
    expect(unsupported_thrown, "non-DVPP image backend should throw");

    return 0;
}
