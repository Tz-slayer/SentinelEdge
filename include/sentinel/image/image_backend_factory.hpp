#pragma once

#include "sentinel/image/image_backend.hpp"

#include <memory>
#include <string>

namespace sentinel {

/**
 * @brief 根据后端名称创建图像处理后端。
 * @param backend 后端名称，例如 `"opencv"` 或 `"dvpp"`。
 * @return 新创建的图像处理后端对象。
 * @throws std::runtime_error 当后端名称不受支持时抛出。
 */
std::unique_ptr<ImageBackend> create_image_backend(const std::string& backend);

} // namespace sentinel
