#pragma once

#include "sentinel/common/types.hpp"

#include <filesystem>

namespace sentinel {

SentinelConfig load_config(const std::filesystem::path& config_dir);

} // namespace sentinel
