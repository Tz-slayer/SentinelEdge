#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/logging/logger.hpp"

#include <memory>

namespace sentinel {

/**
 * @brief 根据日志配置创建对应的日志策略对象。
 * @param config 日志配置。
 * @return 新创建的日志策略对象。
 */
std::unique_ptr<Logger> create_logger(const LoggingConfig& config);

} // namespace sentinel
