#include "video/FFmpegVideoWriter.h"

#include <cmath>
#include <cstdio>
#include <deque>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

struct FFmpegVideoWriter::Impl
{
    AVFormatContext* fmt = nullptr;
    AVCodecContext* codec = nullptr;
    AVStream* stream = nullptr;
    SwsContext* sws = nullptr;
    int width = 0;
    int height = 0;
    int64_t ptsCounter = 0;
    int64_t packetIndex = 0;
    std::deque<AVFrame*> pendingFrames;

    ~Impl()
    {
        for (AVFrame* f : pendingFrames)
            av_frame_free(&f);
        pendingFrames.clear();
        if (sws)
            sws_freeContext(sws);
        if (codec)
            avcodec_free_context(&codec);
        if (fmt) {
            if (fmt->pb)
                avio_closep(&fmt->pb);
            avformat_free_context(fmt);
        }
        sws = nullptr;
        codec = nullptr;
        fmt = nullptr;
        stream = nullptr;
    }
};

FFmpegVideoWriter::FFmpegVideoWriter()
    : d(std::make_unique<Impl>())
{
}

FFmpegVideoWriter::~FFmpegVideoWriter()
{
    close();
}

bool FFmpegVideoWriter::open(const std::string& path, int width, int height, double fps)
{
    close();

    if (width <= 0 || height <= 0 || fps <= 0.0)
        return false;

    if (avformat_alloc_output_context2(&d->fmt, nullptr, nullptr, path.c_str()) < 0)
        return false;
    if (!d->fmt)
        return false;

    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        std::fprintf(stderr, "FFmpegVideoWriter: libx264 no disponible\n");
        return false;
    }

    d->stream = avformat_new_stream(d->fmt, codec);
    if (!d->stream)
        return false;

    d->codec = avcodec_alloc_context3(codec);
    if (!d->codec)
        return false;

    const AVRational tb = {1, static_cast<int>(std::lround(fps))};
    d->codec->width = width;
    d->codec->height = height;
    d->codec->time_base = tb;
    d->codec->framerate = {static_cast<int>(std::lround(fps)), 1};
    d->codec->pix_fmt = AV_PIX_FMT_YUV420P;
    d->codec->gop_size = 12;
    d->codec->max_b_frames = 0; // sin B-frames: orden de presentación == orden de codificación
    d->codec->thread_count = 1; // mono-hilo, sin buffering de frames
    av_opt_set(d->codec->priv_data, "preset", "fast", 0);
    av_opt_set(d->codec->priv_data, "tune", "zerolatency", 0);
    d->codec->bit_rate = 4000000;
    if (d->fmt->oformat->flags & AVFMT_GLOBALHEADER)
        d->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(d->codec, codec, nullptr) < 0)
        return false;
    if (avcodec_parameters_from_context(d->stream->codecpar, d->codec) < 0)
        return false;

    d->stream->time_base = tb;

    d->sws = sws_getContext(width, height, AV_PIX_FMT_BGR24,
                            width, height, AV_PIX_FMT_YUV420P,
                            SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!d->sws)
        return false;

    if (avio_open(&d->fmt->pb, path.c_str(), AVIO_FLAG_WRITE) < 0)
        return false;
    if (avformat_write_header(d->fmt, nullptr) < 0)
        return false;

    d->width = width;
    d->height = height;
    return true;
}

bool FFmpegVideoWriter::write(const cv::Mat& frameBgr)
{
    if (!d->codec || frameBgr.empty() ||
        frameBgr.cols != d->width || frameBgr.rows != d->height)
        return false;

    AVFrame* fr = av_frame_alloc();
    if (!fr)
        return false;
    fr->format = AV_PIX_FMT_YUV420P;
    fr->width = d->width;
    fr->height = d->height;
    if (av_frame_get_buffer(fr, 32) < 0) {
        av_frame_free(&fr);
        return false;
    }

    const uint8_t* srcData[4] = {frameBgr.data, nullptr, nullptr, nullptr};
    const int srcLinesize[4] = {static_cast<int>(frameBgr.step), 0, 0, 0};
    sws_scale(d->sws, srcData, srcLinesize, 0, d->height, fr->data, fr->linesize);
    fr->pts = d->ptsCounter++;

    d->pendingFrames.push_back(fr);
    if (avcodec_send_frame(d->codec, fr) < 0)
        return false;

    AVPacket* pkt = av_packet_alloc();
    bool written = true;
    while (avcodec_receive_packet(d->codec, pkt) >= 0) {
        // Sin B-frames el orden de salida es el de presentación; se asigna el
        // PTS por índice de paquete para que el flush no produzca AV_NOPTS_VALUE.
        pkt->pts = d->packetIndex;
        pkt->dts = d->packetIndex;
        pkt->duration = 1; // una trama en la time_base del codec (1/fps)
        ++d->packetIndex;
        av_packet_rescale_ts(pkt, d->codec->time_base, d->stream->time_base);
        pkt->stream_index = d->stream->index;
        if (av_interleaved_write_frame(d->fmt, pkt) < 0)
            written = false;
        av_packet_unref(pkt);

        // El frame deja de ser necesario cuando su paquete ya se ha emitido.
        if (!d->pendingFrames.empty()) {
            av_frame_free(&d->pendingFrames.front());
            d->pendingFrames.pop_front();
        }
    }
    av_packet_free(&pkt);

    if (written)
        ++framesWritten_;
    return written;
}

void FFmpegVideoWriter::close()
{
    if (d->codec) {
        avcodec_send_frame(d->codec, nullptr);
        AVPacket* pkt = av_packet_alloc();
        while (avcodec_receive_packet(d->codec, pkt) >= 0) {
            pkt->pts = d->packetIndex;
            pkt->dts = d->packetIndex;
            pkt->duration = 1; // una trama en la time_base del codec (1/fps)
            ++d->packetIndex;
            av_packet_rescale_ts(pkt, d->codec->time_base, d->stream->time_base);
            pkt->stream_index = d->stream->index;
            av_interleaved_write_frame(d->fmt, pkt);
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }

    for (AVFrame* f : d->pendingFrames)
        av_frame_free(&f);
    d->pendingFrames.clear();

    if (d->fmt && d->fmt->pb)
        av_write_trailer(d->fmt);

    if (d->sws) {
        sws_freeContext(d->sws);
        d->sws = nullptr;
    }
    if (d->codec) {
        avcodec_free_context(&d->codec);
        d->codec = nullptr;
    }
    if (d->fmt) {
        if (d->fmt->pb)
            avio_closep(&d->fmt->pb);
        avformat_free_context(d->fmt);
        d->fmt = nullptr;
    }
    d->stream = nullptr;
    d->width = 0;
    d->height = 0;
    d->ptsCounter = 0;
}

bool FFmpegVideoWriter::isOpen() const
{
    return d->codec != nullptr;
}