#include <iostream>
#include <sstream>
#include "davMessager.h"
#include "scaleFilter.h"

namespace ff_dynamic {

std::ostream & operator<<(std::ostream & os, const ScaleFilterParams & p) {
    os << p.m_logtag << " of ScaleFilter Params: \nIn:  width " << p.m_inWidth << ", height "
       << p.m_inHeight << ", timebase " << p.m_inTimebase << ", framerate "
       << p.m_inFramerate << ", sar " << p.m_inSar
       << "\nOut: width " << p.m_outWidth << ", height " << p.m_outHeight << ", timebase "
       << p.m_outTimebase << ", framerate " << p.m_outFramerate
       << ", hardware frame ? " << (p.m_hwFramesCtx == nullptr ? "No" : "Yes");
    return os;
}

bool operator==(const ScaleFilterParams & l, const ScaleFilterParams & r) {
    return !(l.m_inFormat == r.m_inFormat && l.m_inWidth == r.m_inWidth && l.m_inHeight == r.m_inHeight &&
        l.m_outWidth == r.m_outWidth && l.m_outHeight == r.m_outHeight &&
        l.m_inTimebase == r.m_inTimebase && l.m_inFramerate == r.m_inFramerate && l.m_inSar == r.m_inSar &&
        l.m_outTimebase == r.m_outTimebase && l.m_outFramerate == r.m_outFramerate &&
        l.m_bFpsScale == r.m_bFpsScale);
}

int ScaleFilter::initScaleFilter(const ScaleFilterParams & sfp) {
    int ret = 0;
    m_sfp = sfp;
    m_logtag = m_sfp.m_logtag.empty() ? "[ScaleFilter] " : m_sfp.m_logtag;
    LOG(INFO) << m_sfp;

    FilterGeneralParams fgp;
    // 1. filter base parameters settings
    fgp.m_inMediaType = AVMEDIA_TYPE_VIDEO;
    fgp.m_outMediaType = AVMEDIA_TYPE_VIDEO;
    fgp.m_bufsrcParams.reset(av_buffersrc_parameters_alloc(), [](AVBufferSrcParameters *p) {av_freep(&p);});
    fgp.m_bufsrcParams->format = (int)m_sfp.m_inFormat;
    fgp.m_bufsrcParams->width = m_sfp.m_inWidth;
    fgp.m_bufsrcParams->height = m_sfp.m_inHeight;
    fgp.m_bufsrcParams->sample_aspect_ratio = m_sfp.m_inSar;
    fgp.m_bufsrcParams->time_base = m_sfp.m_inTimebase;
    // TODO: seems bug here, framerate should not be this one!
    fgp.m_bufsrcParams->frame_rate = m_sfp.m_inFramerate;
    fgp.m_bufsrcParams->hw_frames_ctx = m_sfp.m_hwFramesCtx;
    if (!m_sfp.m_logtag.empty())
        fgp.m_logtag = m_sfp.m_logtag;
    else
        fgp.m_logtag = "[ScaleFilter] ";

    /* 2. default empty setting for bufsink of 'ScaleFilter' is fine */
    /* m_fgp.m_bufsinkParams */

    /* 3. calculate the filter description */
    std::stringstream fpsConvertDesc;
    /* vf_fps filter to convert to a fixed output framerate. << std::setprecision(2) << std::fixed */
    if (m_sfp.m_bFpsScale) {
        CHECK(m_sfp.m_outFramerate.num != 0) << m_logtag << "fps convert require output framerate > 0";
        fpsConvertDesc << "fps=fps=" << m_sfp.m_outFramerate.num << "/" << m_sfp.m_outFramerate.den
                       << ":round=near:eof_action=round";
    }

    /* format for hardware (only support cuda for now) */
    std::stringstream scaleDesc;
    if (m_sfp.m_inFormat == AV_PIX_FMT_CUDA) {
        scaleDesc << "scale_npp=" << sfp.m_outWidth << ":" << sfp.m_outHeight << ":same";
    } else {
        scaleDesc << "scale=" << sfp.m_outWidth << ":" << sfp.m_outHeight;
    }

    string filterDesc;
    if (m_sfp.m_inFramerate < m_sfp.m_outFramerate) {
        filterDesc = scaleDesc.str() + ", " + fpsConvertDesc.str();
    } else {
        filterDesc = fpsConvertDesc.str() + ", " + scaleDesc.str();
    }

    /* filter description string */
    fgp.m_filterDesc = filterDesc;
    ret = m_filterGeneral.initFilter(fgp);
    if (ret < 0)
        LOG(ERROR) << m_logtag << "failed to init filterGeneral of scaleFilter " << fgp;
    return ret;
}

int ScaleFilter::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filterGeneral.close();
    m_scaledFrames.clear();
    return 0;
}

////////////
//// process
int ScaleFilter::sendFrame(AVFrame *inFrame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int ret = m_filterGeneral.sendFilterFrame(inFrame);
    if (ret == AVERROR_EOF)
        LOG(WARNING) << m_logtag << "send to filter general return EOF";
    else if (ret < 0)
        LOG(WARNING) << m_logtag << davMsg2str(ret) << " drop one frame";

    vector<shared_ptr<AVFrame>> filterFrames;
    ret = m_filterGeneral.receiveFilterFrames(filterFrames);
    if (ret == AVERROR_EOF) {
        m_bFlushDone = true;
        LOG(INFO) << m_logtag << "filter general return EOF, flush done";
        // filter general will insert one null frame to indicate this EOF event
    }
    else if (ret < 0)
        LOG(ERROR) << m_logtag << davMsg2str(ret);

    for (auto & outFrame : filterFrames)
        m_scaledFrames.push_back(outFrame);
    return ret;
}

/* api for encoder: get scaled frame out */
int ScaleFilter::receiveFrames(vector<shared_ptr<AVFrame>> & scaleFrames) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sfp.m_bFpsScale)
        for (auto & f : m_scaledFrames)
            f->pts = av_rescale_q(f->pts, av_inv_q(m_sfp.m_outFramerate), m_sfp.m_outTimebase);
    scaleFrames = m_scaledFrames;
    m_scaledFrames.clear();
    return m_bFlushDone ? AVERROR_EOF : 0;
}

} // namespace ff_dynamic
