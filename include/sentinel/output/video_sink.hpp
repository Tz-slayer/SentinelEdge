#pragma once

#include "sentinel/common/types.hpp"

#include <string_view>
#include <vector>

namespace sentinel {

/**
 * @brief 视频结果输出通道接口。
 *
 * 该接口用于把原始帧和检测结果输出到不同目标，例如空输出、调试 JPEG、
 * RTSP 推流或后续 WebRTC/HLS 输出。它位于推理和后处理之后。
 */
class VideoSink {
public:
    /**
     * @brief 释放输出通道资源。
     */
    virtual ~VideoSink() = default;

    VideoSink(const VideoSink&) = delete;
    VideoSink& operator=(const VideoSink&) = delete;

    VideoSink(VideoSink&&) = delete;
    VideoSink& operator=(VideoSink&&) = delete;

    /**
     * @brief 初始化输出通道。
     * @return 成功返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    virtual bool open() = 0;

    /**
     * @brief 关闭输出通道并释放资源。
     */
    virtual void close() noexcept = 0;

    /**
     * @brief 写入一帧及其检测结果。
     * @param frame 视频源输出的原始帧。
     * @param detections 当前帧检测结果。
     * @return 成功返回 `true`，失败返回 `false` 并更新 `last_error()`。
     */
    virtual bool write(const Frame& frame, const std::vector<Detection>& detections) = 0;

    /**
     * @brief 返回输出通道类型标识。
     * @return 例如 `"none"` 或 `"debug_image"` 的稳定字符串。
     */
    virtual std::string_view kind() const noexcept = 0;

    /**
     * @brief 返回最近一次错误文本。
     * @return 若当前没有错误则返回空字符串。
     */
    virtual std::string_view last_error() const noexcept = 0;

protected:
    /**
     * @brief 允许派生类默认构造基类。
     */
    VideoSink() = default;
};

} // namespace sentinel
