#pragma once

#include "sentinel/common/types.hpp"

#include <optional>
#include <string_view>

namespace sentinel {

/**
 * @brief 所有产出视频帧的数据源抽象策略接口。
 *
 * 各种具体视频源，例如本地摄像头、RTSP、测试源，都通过实现该接口
 * 以策略模式接入上层流水线。
 */
class VideoSource {
public:
    virtual ~VideoSource() = default;

    /**
     * @brief 打开底层视频源并准备开始读帧。
     * @return 成功返回 `true`，失败返回 `false`。
     */
    virtual bool open() = 0;

    /**
     * @brief 释放所有持有资源并停止继续产出帧。
     */
    virtual void close() noexcept = 0;

    /**
     * @brief 从视频源读取一帧数据。
     * @return 成功时返回一帧，否则返回 `std::nullopt`。
     */
    virtual std::optional<Frame> read_frame() = 0;

    /**
     * @brief 返回稳定的视频源类型标识。
     * @return 例如 `"mock"`、`"v4l2"` 等类型字符串。
     */
    virtual std::string_view kind() const noexcept = 0;

    /**
     * @brief 返回最近一次错误的可读文本。
     * @return 若当前没有错误则返回空字符串。
     */
    virtual std::string_view last_error() const noexcept = 0;
};

} // namespace sentinel
