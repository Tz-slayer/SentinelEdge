#include "sentinel/image/image_backend_factory.hpp"

#include "sentinel/image/dvpp_image_backend.hpp"
#include "sentinel/image/opencv_image_backend.hpp"

#include <memory>
#include <stdexcept>

namespace sentinel {

/**
 * @brief 根据后端名称创建图像处理后端。
 * @param backend 后端名称。
 * @return 新创建的图像处理后端对象。
 */
std::unique_ptr<ImageBackend> create_image_backend(const std::string& backend)
{
    if (backend == "opencv") {
        return std::make_unique<OpenCvImageBackend>();
    }
    if (backend == "dvpp") {
        return std::make_unique<DvppImageBackend>();
    }

    throw std::runtime_error("unsupported image backend: " + backend);
}

} // namespace sentinel
