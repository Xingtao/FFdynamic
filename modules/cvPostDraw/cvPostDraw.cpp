#include <iostream>
#include <fstream>
#include "cvPostDraw.h"

namespace ff_dynamic {
/////////////////////////////////
// [Register - auto, cvPostDraw]
static DavImplRegister s_cvPostDrawReg(DavWaveClassCvPostDraw(), vector<string>({"auto", "cvPostDraw"}), {},
                                        [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                            unique_ptr<CvPostDraw> p(new CvPostDraw(options));
                                            return p;
                                        });

const DavRegisterProperties & CvPostDraw::getRegisterProperties() const noexcept {
    return s_cvPostDrawReg.m_properties;
}

////////////////////////////////////
//  [event process]
int CvPostDraw::processDnnDetectResult(const CvDnnDetectEvent & e) {
    return 0;
}

////////////////////////////////////
//  [construct - destruct - process]
int CvPostDraw::onConstruct() {
    LOG(INFO) << m_logtag << "Creating CvPostDraw " << m_options.dump();
    std::function<int (const CvDnnDetectEvent &)> f =
        [this] (const CvDnnDetectEvent & e) {return processDnnDetectResult(e);};
    m_implEvent.registerEvent(f);

    /* only output one video stream */
    m_outputMediaMap.emplace(IMPL_SINGLE_OUTPUT_STREAM_INDEX, AVMEDIA_TYPE_VIDEO);

    LOG(INFO) << m_logtag << "CvPostDraw create done: " << m_dps;
    return 0;
}

int CvPostDraw::onDestruct() {
    // clear works
    LOG(INFO) << m_logtag << "CvPostDraw Destruct";
    return 0;
}

////////////////////////////////////
//  [dynamic initialization]

int CvPostDraw::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
    DavImplTravel::TravelStatic & in = m_inputTravelStatic.at(ctx.m_froms[0]);
    if (!in.m_codecpar && (in.m_pixfmt == AV_PIX_FMT_NONE)) {
        ERRORIT(DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR,
                m_logtag + "dehaze cannot get valid codecpar or videopar");
        return DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR;
    }
    /* set output infos */
    m_timestampMgr.clear();
    m_outputTravelStatic.clear();
    m_timestampMgr.insert(std::make_pair(ctx.m_froms[0], DavImplTimestamp(in.m_timebase, in.m_timebase)));

    auto out = make_shared<DavTravelStatic>();
    *out = *in;
    m_outputTravelStatic.emplace(IMPL_SINGLE_OUTPUT_STREAM_INDEX, out);
    m_bDynamicallyInitialized = true;
    return 0;
}

////////////////////////////
//  [dynamic initialization]

int CvPostDraw::onProcess(DavProcCtx & ctx) {
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!ctx.m_inBuf) {
        ERRORIT(DAV_ERROR_IMPL_UNEXPECT_EMPTY_INBUF, "CvPostDraw should always have input");
        return DAV_ERROR_IMPL_UNEXPECT_EMPTY_INBUF;
    }

    int ret = 0;
    /* ref frame is a frame ref to original frame (data shared),
       but timestamp is convert to current impl's timebase */
    auto inFrame = ctx.m_inRefFrame;
    if (!inFrame) {
        LOG(INFO) << m_logtag << "cv post draw reciving flush frame";
        ctx.m_bInputFlush = true;
        /* no flush needed, so just return EOF */
        return AVERROR_EOF;
    }

    // convert this frame to opencv Mat
    CHECK((enum AVPixelFormat)inFrame->format == AV_PIX_FMT_YUV420P);
    cv::Mat yuvMat;
    yuvMat.create(inFrame->height * 3 / 2, inFrame->width, CV_8UC1);
    for (int k=0; k < inFrame->height; k++)
        memcpy(yuvMat.data + k * inFrame->width, inFrame->data[0] + k * inFrame->linesize[0], inFrame->width);
    const auto u = yuvMat.data + inFrame->width * inFrame->height;
    const auto v = yuvMat.data + inFrame->width * inFrame->height * 5 / 4 ;
    for (int k=0; k < inFrame->height/2; k++) {
        memcpy(u + k * inFrame->width/2, inFrame->data[1] + k * inFrame->linesize[1], inFrame->width/2);
        memcpy(v + k * inFrame->width/2, inFrame->data[2] + k * inFrame->linesize[2], inFrame->width/2);
    }

    // draw the frame
    cv::Mat bgrImage;
    cv::cvtColor(yuvMat, bgrImage, CV_YUV2BGR_I420);
    ctx.m_pubEvents.empalce_back(detectEvent);
    /* may cache frame in case not all detect results are present */
    return 0;
}

} // namespace ff_dynamic
