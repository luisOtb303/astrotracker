#include "raw/RawDecoder.h"

#include "libraw/libraw.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <vector>

bool RawDecoder::isRawFile(const std::string& path)
{
    LibRaw raw;
    const int rc = raw.open_file(path.c_str());
    raw.recycle();
    return rc == LIBRAW_SUCCESS;
}

bool RawDecoder::dimensions(const std::string& path, int& w, int& h)
{
    LibRaw raw;
    if (raw.open_file(path.c_str()) != LIBRAW_SUCCESS)
        return false;
    w = raw.imgdata.sizes.width;
    h = raw.imgdata.sizes.height;
    raw.recycle();
    return w > 0 && h > 0;
}

bool RawDecoder::decode(const std::string& path, cv::Mat& out, bool want16, int maxDim)
{
    LibRaw raw;
    if (raw.open_file(path.c_str()) != LIBRAW_SUCCESS)
        return false;
    if (raw.unpack() != LIBRAW_SUCCESS) {
        raw.recycle();
        return false;
    }

    const int full = std::max(raw.imgdata.sizes.width, raw.imgdata.sizes.height);
    raw.imgdata.params.output_bps = want16 ? 16 : 8;
    raw.imgdata.params.half_size = (maxDim > 0 && full / 2 > maxDim) ? 1 : 0;

    if (raw.dcraw_process() != LIBRAW_SUCCESS) {
        raw.recycle();
        return false;
    }

    int err = LIBRAW_SUCCESS;
    libraw_processed_image_t* img = raw.dcraw_make_mem_image(&err);
    if (!img || err != LIBRAW_SUCCESS) {
        if (img)
            raw.dcraw_clear_mem(img);
        raw.recycle();
        return false;
    }

    const int cn = (img->colors == 4) ? 4 : 3;
    const int depth = (img->bits == 16) ? CV_16U : CV_8U;
    cv::Mat m(img->height, img->width, CV_MAKETYPE(depth, cn), img->data);
    m = m.clone();

    raw.dcraw_clear_mem(img);
    raw.recycle();

    if (cn == 4)
        cv::cvtColor(m, m, cv::COLOR_RGBA2BGR);
    else if (cn == 3)
        cv::cvtColor(m, m, cv::COLOR_RGB2BGR);

    if (maxDim > 0) {
        const int longest = std::max(m.cols, m.rows);
        if (longest > maxDim) {
            const double scale = static_cast<double>(maxDim) / longest;
            cv::resize(m, m, cv::Size(), scale, scale, cv::INTER_AREA);
        }
    }

    out = m;
    return true;
}

bool RawDecoder::thumbnail(const std::string& path, cv::Mat& out, int maxDim)
{
    LibRaw raw;
    if (raw.open_file(path.c_str()) != LIBRAW_SUCCESS)
        return false;
    if (raw.unpack_thumb() != LIBRAW_SUCCESS) {
        raw.recycle();
        return false;
    }

    const libraw_thumbnail_t& t = raw.imgdata.thumbnail;
    cv::Mat thumb;
    if (t.tformat == LIBRAW_THUMBNAIL_JPEG && t.thumb && t.tlength > 0) {
        const cv::Mat buf(1, static_cast<int>(t.tlength), CV_8U,
                          static_cast<void*>(t.thumb));
        thumb = cv::imdecode(buf, cv::IMREAD_COLOR);
    } else if (t.tformat == LIBRAW_THUMBNAIL_BITMAP && t.thumb && t.twidth > 0) {
        thumb = cv::Mat(t.theight, t.twidth, CV_8UC3, static_cast<void*>(t.thumb))
                    .clone();
    } else {
        raw.recycle();
        return false;
    }

    raw.recycle();
    if (thumb.empty())
        return false;

    if (maxDim > 0) {
        const int longest = std::max(thumb.cols, thumb.rows);
        if (longest > maxDim) {
            const double scale = static_cast<double>(maxDim) / longest;
            cv::resize(thumb, thumb, cv::Size(), scale, scale, cv::INTER_AREA);
        }
    }

    out = thumb;
    return true;
}