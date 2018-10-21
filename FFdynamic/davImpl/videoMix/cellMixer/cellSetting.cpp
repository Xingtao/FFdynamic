#include "cellSetting.h"

namespace ff_dynamic {

std::ostream & operator<<(std::ostream & os, const CellArchor & c) {
    os << "CellArchor (" << c.m_x << ", " << c.m_y << ", " << c.m_w << ", " << c.m_h << "), pos "
       << c.m_atPos << ", layer " << c.m_layer;
    return os;
}

std::ostream & operator<<(std::ostream & os, const CellAdornment & c) {
    os << "CellAdornment: margin " << c.m_marginSize << ", fillet radius "
       << c.m_filletRadius << ", border color " << c.m_borderLineColor << ", border width" << c.m_borderLineWidth;
    return os;
}

////////////////////////
// [Video Cell Settings]

//////////////////////////////////////////////////////////////////////////////////////////
int CellPaster::paste(AVFrame *canvasFrame, const AVFrame *cellFrame) {
    /* copy data to canvasFrame according to the position */
    CHECK(cellFrame->width == (m_archor.m_w - (m_adornment.m_marginSize << 1) - (m_padX << 1)) &&
          cellFrame->height == (m_archor.m_h - (m_adornment.m_marginSize << 1) - (m_padY << 1)) &&
          canvasFrame->width >= (m_archor.m_x + m_archor.m_w) &&
          canvasFrame->height >= (m_archor.m_y + m_archor.m_h))
        << canvasFrame->width << "| cell width " << cellFrame->width << "="
        << (m_archor.m_w - m_adornment.m_marginSize - (m_padX << 1)) << ", cell height "
        << cellFrame->height << "=" << (m_archor.m_h - m_adornment.m_marginSize - (m_padY << 1)) << ". "
        << "padX " << m_padX << ", padY " << m_padY << ", margin " << m_adornment.m_marginSize << ", archor "
        << m_archor;


    /* TODO: abstract a VideoDataCompose for data mix of any pixfmt */
    uint8_t *dstData[4] = {0};
    /* right now, yuv420p only */
    const int yStartOffset = canvasFrame->linesize[0] * (m_archor.m_y + m_padY + m_adornment.m_marginSize) +
        (m_archor.m_x + m_padX + m_adornment.m_marginSize);
    const int uStartOffset = canvasFrame->linesize[1] * ((m_archor.m_y + m_padY + m_adornment.m_marginSize) >> 1) +
        (m_archor.m_x + m_padX + m_adornment.m_marginSize) / 2;
    const int vStartOffset = canvasFrame->linesize[2] * ((m_archor.m_y + m_padY + m_adornment.m_marginSize) >> 1) +
        (m_archor.m_x + m_padX + m_adornment.m_marginSize) / 2;

    dstData[0] = canvasFrame->data[0] + yStartOffset;
    dstData[1] = canvasFrame->data[1] + uStartOffset;
    dstData[2] = canvasFrame->data[2] + vStartOffset;

    av_image_copy(dstData, canvasFrame->linesize,
                  (const uint8_t **)cellFrame->data, cellFrame->linesize,
                  (enum AVPixelFormat)cellFrame->format, cellFrame->width, cellFrame->height);
    // LOG(INFO) << "paste done " << m_archor << ", " << cellFrame->width << ", " << cellFrame->height;
    return 0;
}

} // namespace ff_dynamic
