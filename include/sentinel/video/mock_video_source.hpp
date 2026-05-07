#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/video/video_source.hpp"

namespace sentinel {

/**
 * @brief 为本地开发和测试生成模拟帧。
 */
class MockVideoSource final : public VideoSource {
public:
    /**
     * @brief 使用摄像头配置构造模拟视频源。
     * @param config 会被复制到输出帧中的摄像头元数据。
     */
    explicit MockVideoSource(CameraConfig config);

    /**
     * @brief 将模拟视频源置为打开状态，并重置帧序号。
     * @return 固定返回 `true`。
     */
    bool open() override;

    /**
     * @brief 将模拟视频源置为关闭状态。
     */
    void close() noexcept override;

    /**
     * @brief 返回下一帧模拟数据。
     * @return 打开状态下返回生成的帧，否则返回 `std::nullopt`。
     */
    std::optional<Frame> read_frame() override;

    /**
     * @brief 返回视频源类型字符串。
     * @return 固定返回 `"mock"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误的可读文本。
     * @return 若当前没有错误则返回空字符串。
     */
    std::string_view last_error() const noexcept override;

private:
    CameraConfig config_;
    int next_sequence_{1};
    bool is_open_{false};
    std::string last_error_;
};

} // namespace sentinel
