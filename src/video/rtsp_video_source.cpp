#include "sentinel/video/rtsp_video_source.hpp"

#include <chrono>
#include <linux/videodev2.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
}

namespace sentinel {

/**
 * @brief 使用摄像头配置构造 RTSP 视频源。
 * @param config 摄像头配置。
 */
RtspVideoSource::RtspVideoSource(CameraConfig config)
    : config_(std::move(config))
{
    avformat_network_init();
}

/**
 * @brief 校验 RTSP 地址，初始化 FFmpeg 容器、解码器与 MJPEG 编码器。
 * @return 成功初始化返回 `true`，失败返回 `false` 并设置 last_error_。
 *
 * 涉及资源所有权：该方法负责分配和持有 FFmpeg 相关的 Context 和 Packet/Frame。
 */
bool RtspVideoSource::open()
{
    close();

    // 校验是否以 rtsp:// 开头，避免 FFmpeg 误解析其他协议。
    if (config_.uri.rfind("rtsp://", 0) != 0) {
        last_error_ = "camera uri must start with rtsp://";
        return false;
    }

    // 配置 RTSP 连接参数
    AVDictionary* options = nullptr;
    av_dict_set(&options, "rtsp_transport", "udp", 0);
    av_dict_set(&options, "stimeout", "5000000", 0); // 5 seconds

    // 打开 RTSP 流
    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, config_.uri.c_str(), nullptr, &options);
    av_dict_free(&options);

    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        last_error_ = std::string("avformat_open_input failed: ") + errbuf;
        return false;
    }
    format_context_ = fmt_ctx;

    // 查找流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        last_error_ = "avformat_find_stream_info failed";
        close();
        return false;
    }

    // 遍历流，找到第一个视频流
    video_stream_index_ = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        // 检查流类型是否为视频
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }

    // 没有找到视频流
    if (video_stream_index_ == -1) {
        last_error_ = "no video stream found in RTSP URL";
        close();
        return false;
    }

    // 查找解码器
    AVStream* video_stream = fmt_ctx->streams[video_stream_index_];
    const AVCodec* decoder = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!decoder) {
        last_error_ = "video decoder not found";
        close();
        return false;
    }

    // 分配解码器上下文
    AVCodecContext* dec_ctx = avcodec_alloc_context3(decoder);
    if (!dec_ctx) {
        last_error_ = "failed to allocate decoder context";
        close();
        return false;
    }
    decoder_context_ = dec_ctx;

    // 拷贝参数
    avcodec_parameters_to_context(dec_ctx, video_stream->codecpar);
    // 打开解码器
    if (avcodec_open2(dec_ctx, decoder, nullptr) < 0) {
        last_error_ = "failed to open video decoder";
        close();
        return false;
    }

    // 初始化 MJPEG 编码器
    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!encoder) {
        last_error_ = "MJPEG encoder not found";
        close();
        return false;
    }

    AVCodecContext* enc_ctx = avcodec_alloc_context3(encoder);
    if (!enc_ctx) {
        last_error_ = "failed to allocate MJPEG encoder context";
        close();
        return false;
    }
    encoder_context_ = enc_ctx;

    enc_ctx->width = dec_ctx->width;
    enc_ctx->height = dec_ctx->height;
    enc_ctx->time_base = {1, config_.fps > 0 ? config_.fps : 30};
    
    // Convert generic YUV to JPEG-compatible YUV
    AVPixelFormat pix_fmt = dec_ctx->pix_fmt;
    if (pix_fmt == AV_PIX_FMT_YUV420P) pix_fmt = AV_PIX_FMT_YUVJ420P;
    else if (pix_fmt == AV_PIX_FMT_YUV422P) pix_fmt = AV_PIX_FMT_YUVJ422P;
    else if (pix_fmt == AV_PIX_FMT_YUV444P) pix_fmt = AV_PIX_FMT_YUVJ444P;
    // Default fallback to AV_PIX_FMT_YUVJ420P if original format is unhandled or not set
    if (pix_fmt == AV_PIX_FMT_NONE) pix_fmt = AV_PIX_FMT_YUVJ420P;
    
    enc_ctx->pix_fmt = pix_fmt;
    enc_ctx->color_range = AVCOL_RANGE_JPEG;
    
    // Set high quality
    enc_ctx->qmin = 2;
    enc_ctx->qmax = 5;

    if (avcodec_open2(enc_ctx, encoder, nullptr) < 0) {
        last_error_ = "failed to open MJPEG encoder";
        close();
        return false;
    }

    packet_ = av_packet_alloc();
    frame_ = av_frame_alloc();
    if (!packet_ || !frame_) {
        last_error_ = "failed to allocate packet or frame";
        close();
        return false;
    }

    is_open_ = true;
    last_error_.clear();
    return true;
}

/**
 * @brief 关闭 RTSP 视频源，释放所有 FFmpeg 相关资源。
 */
void RtspVideoSource::close() noexcept
{
    is_open_ = false;

    if (packet_) {
        av_packet_free((AVPacket**)&packet_);
    }
    if (frame_) {
        av_frame_free((AVFrame**)&frame_);
    }
    if (encoder_context_) {
        avcodec_free_context((AVCodecContext**)&encoder_context_);
    }
    if (decoder_context_) {
        avcodec_free_context((AVCodecContext**)&decoder_context_);
    }
    if (format_context_) {
        avformat_close_input((AVFormatContext**)&format_context_);
    }
}

/**
 * @brief 将接收到的 AVPacket 送入解码器，并将解码后的帧编码为 MJPEG 格式。
 * @param in_packet 包含压缩流数据的 AVPacket 裸指针
 * @param out_jpeg 用于接收输出 MJPEG 数据的 vector
 * @return 成功生成了一帧 MJPEG 返回 `true`，否则返回 `false`
 *
 * 这是一个重要的内部辅助函数，它处理了格式容错和收发逻辑，避免外层代码过于臃肿。
 */
bool RtspVideoSource::decode_and_encode(void* in_packet, std::vector<std::uint8_t>& out_jpeg)
{
    AVCodecContext* dec_ctx = static_cast<AVCodecContext*>(decoder_context_);
    AVCodecContext* enc_ctx = static_cast<AVCodecContext*>(encoder_context_);
    AVPacket* pkt = static_cast<AVPacket*>(in_packet);
    AVFrame* frame = static_cast<AVFrame*>(frame_);

    int ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) return false;

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return false;
        } else if (ret < 0) {
            return false;
        }

        // We successfully decoded one frame. Now encode it to MJPEG.
        frame->pts = frame->best_effort_timestamp;
        
        // Handle format mismatch if stream changed dynamically
        if (frame->format == AV_PIX_FMT_YUV420P) frame->format = AV_PIX_FMT_YUVJ420P;
        else if (frame->format == AV_PIX_FMT_YUV422P) frame->format = AV_PIX_FMT_YUVJ422P;
        else if (frame->format == AV_PIX_FMT_YUV444P) frame->format = AV_PIX_FMT_YUVJ444P;

        int enc_ret = avcodec_send_frame(enc_ctx, frame);
        if (enc_ret < 0) {
            av_frame_unref(frame);
            continue;
        }

        AVPacket* enc_pkt = av_packet_alloc();
        enc_ret = avcodec_receive_packet(enc_ctx, enc_pkt);
        if (enc_ret >= 0) {
            out_jpeg.assign(enc_pkt->data, enc_pkt->data + enc_pkt->size);
            av_packet_free(&enc_pkt);
            av_frame_unref(frame);
            return true; // Return the first successfully encoded JPEG frame
        }
        av_packet_free(&enc_pkt);
        av_frame_unref(frame);
    }
    return false;
}

/**
 * @brief 阻塞读取一帧 RTSP 数据并返回转码后的 MJPEG 帧。
 * @return 成功时返回 Frame 对象，读取出错或 EOF 返回 `std::nullopt`。
 *
 * 在读取循环中，非视频流的包会被直接丢弃。
 */
std::optional<Frame> RtspVideoSource::read_frame()
{
    if (!is_open_) {
        return std::nullopt;
    }

    AVFormatContext* fmt_ctx = static_cast<AVFormatContext*>(format_context_);
    AVPacket* pkt = static_cast<AVPacket*>(packet_);

    std::vector<std::uint8_t> jpeg_data;
    bool frame_ready = false;

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_index_) {
            if (decode_and_encode(pkt, jpeg_data)) {
                frame_ready = true;
                av_packet_unref(pkt);
                break;
            }
        }
        av_packet_unref(pkt);
    }

    if (!frame_ready) {
        // EOF or read error
        return std::nullopt;
    }

    Frame out_frame;
    out_frame.sequence = ++frame_sequence_;
    out_frame.camera_id = config_.id;
    
    AVCodecContext* enc_ctx = static_cast<AVCodecContext*>(encoder_context_);
    out_frame.width = enc_ctx->width;
    out_frame.height = enc_ctx->height;
    out_frame.pixel_format = V4L2_PIX_FMT_MJPEG;
    out_frame.data = std::move(jpeg_data);
    out_frame.bytes_used = out_frame.data.size();
    
    auto now = std::chrono::steady_clock::now();
    out_frame.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();

    return out_frame;
}

/**
 * @brief 返回视频源类型标识。
 * @return 固定返回 `"rtsp"`。
 */
std::string_view RtspVideoSource::kind() const noexcept
{
    return "rtsp";
}

/**
 * @brief 返回最近一次错误文本。
 * @return 最近一次错误消息。
 */
std::string_view RtspVideoSource::last_error() const noexcept
{
    return last_error_;
}

} // namespace sentinel
