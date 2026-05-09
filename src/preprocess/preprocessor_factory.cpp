#include "sentinel/preprocess/preprocessor_factory.hpp"

#include "sentinel/preprocess/dvpp_frame_preprocessor.hpp"

#include <memory>
#include <stdexcept>

namespace sentinel {

/**
 * @brief 根据配置创建图像预处理策略。
 * @param config 预处理运行配置。
 * @return 新创建的预处理策略对象。
 */
std::unique_ptr<FramePreprocessor> create_frame_preprocessor(const PreprocessConfig& config)
{
    if (config.backend == "dvpp") {
        return std::make_unique<DvppFramePreprocessor>(config);
    }

    throw std::runtime_error("unsupported preprocess backend, expected dvpp: " + config.backend);
}

} // namespace sentinel
