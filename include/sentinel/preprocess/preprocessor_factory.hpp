#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/preprocess/frame_preprocessor.hpp"

#include <memory>

namespace sentinel {

/**
 * @brief 根据配置创建图像预处理策略。
 * @param config 预处理运行配置。
 * @return 新创建的预处理策略对象。
 * @throws std::runtime_error 当后端名称不受支持时抛出。
 */
std::unique_ptr<FramePreprocessor> create_frame_preprocessor(const PreprocessConfig& config);

} // namespace sentinel
