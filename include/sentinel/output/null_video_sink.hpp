#pragma once

#include "sentinel/output/video_sink.hpp"

#include <string>

namespace sentinel {

/**
 * @brief 不执行任何输出的空视频输出通道。
 */
class NullVideoSink final : public VideoSink {
public:
    /**
     * @brief 初始化空输出通道。
     * @return 固定返回 `true`。
     */
    bool open() override;

    /**
     * @brief 关闭空输出通道。
     */
    void close() noexcept override;

    /**
     * @brief 丢弃当前帧和检测结果。
     * @param frame 视频源输出的原始帧。
     * @param detections 当前帧检测结果。
     * @return 固定返回 `true`。
     */
    bool write(const Frame& frame, const std::vector<Detection>& detections) override;

    /**
     * @brief 返回输出通道类型标识。
     * @return 固定返回 `"none"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误文本。
     * @return 当前实现始终返回空字符串。
     */
    std::string_view last_error() const noexcept override;

private:
    std::string last_error_;
};

} // namespace sentinel
