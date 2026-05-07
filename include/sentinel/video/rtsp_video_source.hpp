#pragma once

#include "sentinel/common/types.hpp"
#include "sentinel/video/video_source.hpp"

#include <string>

namespace sentinel {

/**
 * @brief 预留给后续 RTSP 传输实现的视频源骨架。
 */
class RtspVideoSource final : public VideoSource {
public:
    /**
     * @brief 使用摄像头配置构造 RTSP 视频源。
     * @param config 摄像头配置，其中 `uri` 应为 RTSP 地址。
     */
    explicit RtspVideoSource(CameraConfig config);

    /**
     * @brief 校验配置中的 RTSP 地址。
     * @return 成功返回 `true`，失败返回 `false`，并更新 `last_error()`。
     */
    bool open() override;

    /**
     * @brief 关闭 RTSP 视频源状态。
     */
    void close() noexcept override;

    /**
     * @brief 尝试读取一帧 RTSP 数据。
     * @return 在 RTSP 传输尚未实现前固定返回 `std::nullopt`。
     */
    std::optional<Frame> read_frame() override;

    /**
     * @brief 返回视频源类型字符串。
     * @return 固定返回 `"rtsp"`。
     */
    std::string_view kind() const noexcept override;

    /**
     * @brief 返回最近一次错误的可读文本。
     * @return 若当前没有错误则返回空字符串。
     */
    std::string_view last_error() const noexcept override;

private:
    CameraConfig config_;
    std::string last_error_;
    bool is_open_{false};
};

} // namespace sentinel
