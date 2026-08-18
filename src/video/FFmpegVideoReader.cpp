#include "video/FFmpegVideoReader.h"

#include <opencv2/imgproc.hpp>
#include <cmath>
#include <cstdlib>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

namespace {
const AVRational kUsTb = {1, 1000000};

int64_t tsToUs(int64_t ts, const AVRational& tb)
{
    if (ts == AV_NOPTS_VALUE)
        return 0;
    return av_rescale_q(ts, tb, kUsTb);
}

int64_t usToTs(int64_t us, const AVRational& tb)
{
    return av_rescale_q(us, kUsTb, tb);
}
} // namespace

struct FFmpegVideoReader::Impl
{
    AVFormatContext* fmt = nullptr;
    AVCodecContext* codec = nullptr;
    AVStream* stream = nullptr;
    SwsContext* sws = nullptr;
    int streamIndex = -1;
    int64_t durationUs = 0;
    int64_t frameCount = 0;
    double fps = 0.0;
    bool open = false;

    ~Impl()
    {
        if (sws)
            sws_freeContext(sws);
        if (codec)
            avcodec_free_context(&codec);
        if (fmt)
            avformat_close_input(&fmt);
        sws = nullptr;
        codec = nullptr;
        fmt = nullptr;
        stream = nullptr;
    }
};

FFmpegVideoReader::FFmpegVideoReader()
    : d(std::make_unique<Impl>())
{
}

FFmpegVideoReader::~FFmpegVideoReader()
{
    close();
}

bool FFmpegVideoReader::open(const std::string& path)
{
    close();

    if (avformat_open_input(&d->fmt, path.c_str(), nullptr, nullptr) < 0)
        return false;
    if (avformat_find_stream_info(d->fmt, nullptr) < 0)
        return false;

    d->streamIndex = av_find_best_stream(d->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (d->streamIndex < 0)
        return false;
    d->stream = d->fmt->streams[d->streamIndex];

    const AVCodec* dec = avcodec_find_decoder(d->stream->codecpar->codec_id);
    if (!dec)
        return false;

    d->codec = avcodec_alloc_context3(dec);
    if (!d->codec)
        return false;
    if (avcodec_parameters_to_context(d->codec, d->stream->codecpar) < 0)
        return false;
    if (avcodec_open2(d->codec, dec, nullptr) < 0)
        return false;

    if (d->stream->duration != AV_NOPTS_VALUE)
        d->durationUs = tsToUs(d->stream->duration, d->stream->time_base);
    else if (d->fmt->duration != AV_NOPTS_VALUE)
        d->durationUs = d->fmt->duration;

    const AVRational fr = av_guess_frame_rate(d->fmt, d->stream, nullptr);
    d->fps = (fr.num && fr.den) ? av_q2d(fr) : 25.0;
    if (d->fps > 0.0)
        d->frameCount = static_cast<int64_t>(d->durationUs * d->fps / 1000000.0);

    d->sws = sws_getContext(d->codec->width, d->codec->height, d->codec->pix_fmt,
                            d->codec->width, d->codec->height, AV_PIX_FMT_BGR24,
                            SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!d->sws)
        return false;

    d->open = true;
    return true;
}

void FFmpegVideoReader::close()
{
    if (d->fmt) {
        avformat_close_input(&d->fmt);
        d->fmt = nullptr;
    }
    if (d->codec) {
        avcodec_free_context(&d->codec);
        d->codec = nullptr;
    }
    if (d->sws) {
        sws_freeContext(d->sws);
        d->sws = nullptr;
    }
    d->stream = nullptr;
    d->streamIndex = -1;
    d->durationUs = 0;
    d->frameCount = 0;
    d->fps = 0.0;
    d->open = false;
}

bool FFmpegVideoReader::isOpen() const
{
    return d->open;
}

bool FFmpegVideoReader::decodeLoop(Frame& out)
{
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!pkt || !frame) {
        av_packet_free(&pkt);
        av_frame_free(&frame);
        return false;
    }

    bool got = false;
    while (!got) {
        const int r = av_read_frame(d->fmt, pkt);
        if (r < 0)
            break;
        if (pkt->stream_index == d->streamIndex) {
            if (avcodec_send_packet(d->codec, pkt) >= 0) {
                while (avcodec_receive_frame(d->codec, frame) >= 0) {
                    convertFrame(frame, out.image);
                    out.ptsUs = tsToUs(frame->pts, d->stream->time_base);
                    got = true;
                    break;
                }
            }
        }
        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    return got;
}

bool FFmpegVideoReader::readNext(Frame& out)
{
    if (!isOpen())
        return false;
    if (!decodeLoop(out))
        return false;
    out.index = static_cast<int64_t>(std::llround(out.ptsUs * d->fps / 1000000.0));
    return true;
}

bool FFmpegVideoReader::seekToUs(int64_t us)
{
    if (!isOpen())
        return false;

    if (av_seek_frame(d->fmt, d->streamIndex, usToTs(us, d->stream->time_base),
                      AVSEEK_FLAG_BACKWARD) < 0)
        return false;
    avcodec_flush_buffers(d->codec);

    Frame f;
    int guard = 0;
    while (decodeLoop(f)) {
        if (f.ptsUs >= us || ++guard > 100000)
            break;
    }
    return true;
}

int64_t FFmpegVideoReader::durationUs() const
{
    return d->durationUs;
}

int64_t FFmpegVideoReader::frameCount() const
{
    return d->frameCount;
}

double FFmpegVideoReader::fps() const
{
    return d->fps;
}

int FFmpegVideoReader::width() const
{
    return d->codec ? d->codec->width : 0;
}

int FFmpegVideoReader::height() const
{
    return d->codec ? d->codec->height : 0;
}

void FFmpegVideoReader::convertFrame(void* srcVoid, cv::Mat& dst)
{
    auto* src = static_cast<AVFrame*>(srcVoid);
    const int w = d->codec->width;
    const int h = d->codec->height;

    uint8_t* dstData[4] = {nullptr, nullptr, nullptr, nullptr};
    int dstLinesize[4] = {0, 0, 0, 0};

    const int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, w, h, 1);
    uint8_t* buffer = static_cast<uint8_t*>(av_malloc(static_cast<size_t>(numBytes)));
    if (!buffer)
        return;

    av_image_fill_arrays(dstData, dstLinesize, buffer, AV_PIX_FMT_BGR24, w, h, 1);
    sws_scale(d->sws, src->data, src->linesize, 0, h, dstData, dstLinesize);

    cv::Mat(h, w, CV_8UC3, dstData[0], dstLinesize[0]).copyTo(dst);
    av_free(buffer);
}