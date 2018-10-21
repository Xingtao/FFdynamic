#pragma once
#include <map>
#include <memory>
#include <functional>
#include <cmath>

#include <glog/logging.h>
#include "ffmpegHeaders.h"
#include "scaleFilter.h"
#include "davImplTravel.h"

namespace ff_dynamic {
using ::std::map;
using ::std::string;
using ::std::vector;
using ::std::function;
using ::std::unique_ptr;

struct CellAdornment {
    /* for video mix, set margin to avoid encoding block effect between two cells */
    /* for now, only 8 is valid values */
    int m_marginSize = 8; // 8
    /* not supported for now */
    int m_filletRadius = 0;
    int m_borderLineWidth = 2;
    int m_borderLineColor = 0x00;
};

struct CellArchor {
    CellArchor() = default;
    CellArchor(const int w, const int h, const vector<int> & coors,
               const size_t pos, const int layer = -1) noexcept {
        init(w, h, coors, pos, layer);
    }
    int init(const int w, const int h, const vector<int> & coors,
             const size_t pos, const int layer = -1) noexcept {
        CHECK(coors.size() == 4);
        m_x = ((int)round(coors[0] / 120.0 * w) >> 1) << 1;
        m_y = ((int)round(coors[1] / 120.0 * h) >> 1) << 1;
        m_w = ((int)round(coors[2] / 120.0 * w) >> 1) << 1;
        m_h = ((int)round(coors[3] / 120.0 * h) >> 1) << 1;;
        m_atPos = pos;
        m_layer = layer;
        return 0;
    }
    int m_x = -1;
    int m_y = -1;
    int m_w = -1;
    int m_h = -1;
    int m_atPos = -1; /* negative value (< 0) indicates this cell is invisible, and  */
    int m_layer = -1; /* negative value (< 0) indicates ignore layer order */
};

struct CellPaster {
    CellPaster() = default;
    CellPaster(const CellArchor & archor, const CellAdornment & adornment,
               const int padX = 0, const int padY = 0,
               enum AVPixelFormat inPixfmt = AV_PIX_FMT_YUV420P,
               enum AVPixelFormat outPixfmt = AV_PIX_FMT_YUV420P) noexcept {
        update(archor, adornment, padX, padY, inPixfmt, outPixfmt);
    }
    int update(const CellArchor & archor, const CellAdornment & adornment,
               const int padX = 0, const int padY = 0,
               const enum AVPixelFormat inPixfmt = AV_PIX_FMT_YUV420P,
               const enum AVPixelFormat outPixfmt = AV_PIX_FMT_YUV420P) noexcept {
        m_archor = archor;
        m_adornment = adornment;
        m_padX = padX;
        m_padY = padY;
        m_inPixfmt = inPixfmt;
        m_outPixfmt = outPixfmt;
        return 0;
    };

    int paste(AVFrame *srcFrame, const AVFrame *cellFrame);
    enum AVPixelFormat m_inPixfmt = AV_PIX_FMT_YUV420P;
    enum AVPixelFormat m_outPixfmt = AV_PIX_FMT_YUV420P;
    int m_padX = 0;
    int m_padY = 0;
    CellArchor m_archor;
    CellAdornment m_adornment;
};

extern std::ostream & operator<<(std::ostream & os, const CellArchor & ca);
extern std::ostream & operator<<(std::ostream & os, const CellAdornment & ca);
extern std::ostream & operator<<(std::ostream & os, const CellPaster & cp);

} // namespace ff_dynamic
