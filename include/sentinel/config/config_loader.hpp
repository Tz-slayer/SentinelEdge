#pragma once

#include "sentinel/common/types.hpp"

#include <filesystem>

namespace sentinel {

/**
 * @brief 从配置目录加载应用配置。
 * @param config_dir 包含 `sentinel`、`cameras`、`rules` YAML 文件的目录。
 * @return 解析完成后的应用配置对象。
 */
SentinelConfig load_config(const std::filesystem::path& config_dir);

} // namespace sentinel
