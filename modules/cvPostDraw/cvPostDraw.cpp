#include <algorithm>
#include <iostream>
#include <fstream>
#include "frameMat.h"
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
    LOG(INFO) << m_logtag << "got result " << e.m_framePts << ", " << e.m_detectorFrameworkTag;
    auto & from = e.getAddress();
    if (m_detectResults.count(from) == 0) {
        m_detectResults.emplace(from, {e});
        return 0;
    }
    auto & v = m_detectResults.at(from);
    v.emplace_back(e);
    return 0;
}

int CvPostDraw::processStopPubEvent(const DavStopPubEvent & e) {
    auto & from = e.getAddress();
    LOG(INFO) << m_logtag << "stop event pub " << from;
    m_detectResults.erase(from);
    return 0;
}

////////////////////////////////////
//  [construct - destruct - process]
int CvPostDraw::onConstruct() {
    LOG(INFO) << m_logtag << "Creating CvPostDraw " << m_options.dump();
    std::function<int (const CvDnnDetectEvent &)> f =
        [this] (const CvDnnDetectEvent & e) {return processDnnDetectResult(e);};
    m_implEvent.registerEvent(f);

    std::function<int (const DavStopPubEvent &)> g =
        [this] (const DavStopPubEvent & e) {return processStopPubEvent(e);};
    m_implEvent.registerEvent(g);

    /* only output one video stream */
    m_outputMediaMap.emplace(IMPL_SINGLE_OUTPUT_STREAM_INDEX, AVMEDIA_TYPE_VIDEO);

    LOG(INFO) << m_logtag << "CvPostDraw create done";
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
    auto in = m_inputTravelStatic.at(ctx.m_froms[0]);
    if (!in->m_codecpar && (in->m_pixfmt == AV_PIX_FMT_NONE)) {
        ERRORIT(DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR,
                m_logtag + "dehaze cannot get valid codecpar or videopar");
        return DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR;
    }
    /* set output infos */
    m_timestampMgr.clear();
    m_outputTravelStatic.clear();
    m_timestampMgr.insert(std::make_pair(ctx.m_froms[0], DavImplTimestamp(in->m_timebase, in->m_timebase)));

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
    if (!ctx.m_inBuf)
        return 0;

    int ret = 0;
    auto inFrame = ctx.m_inRefFrame;
    if (!inFrame) {
        LOG(INFO) << m_logtag << "cv post draw reciving flush frame";
        ctx.m_bInputFlush = true;
        /* no flush needed, so just return EOF */
        return AVERROR_EOF;
    }

    CHECK((enum AVPixelFormat)inFrame->format == AV_PIX_FMT_YUV420P);
    m_cacheBufFrames.emplace_back(ctx.m_inBuf);

    /* process detect results and draw results to frame */
    auto outs = m_ctx.m_outBufs;
    for (auto & in : m_cacheBufFrames) {
        auto frame = ctx.m_inRefFrame;
        /* first remove 'expired' results */
        for (auto & d : m_detectResults) {
            d.second.erase(std::remove_if(d.second.begin(), d.second.end(),
                                          [&frame](const CvDnnDetectEvent & r) {
                                              return r.m_framePts < frame->pts;
                                          }), d.second.end());
        }
        /* then check whether all results are ready */
        bool bReady = true;
        vector<CvDnnDetectEvent> results;
        for (auto & d : m_detectResutls) {
            if (d.second.size() == 0) /* not comming */{
                bReady = false;
                break;
            }
            if (d.second[0].m_framePts == frame->pts) {
                results.emplace_back(d.second[0]);
            }
        }
        if (!bReady)
            break;
        /* convert this frame to opencv Mat. TODO: directly to bgr mat */
        cv::Mat yuvMat;
        FrameMat::frameToMatY420(frame, yuvMat);
        /* draw the frame */
        cv::Mat image;
        cv::cvtColor(yuvMat, image, CV_YUV2BGR_I420);
        drawResult(image, results)
        /* convert to yuv again (tedious, should have a auto format convertor) */
        auto outBuf = make_shared<DavProcBuf>();
        auto outFrame = outBuf->mkAVFrame();
        FrameMat::matToFrameYuv420(yuvMat, outFrame);
        outFrame->pts = inFrame->pts;
        m_ctx.m_outBufs.emplace_back(outBuf);
    }
    return 0;
}

int CvPostDraw::drawResult(cv::Mat & image, const vector<CvDnnDetectEvent> & results) {
    const static vector<cv::CvScalar> colors {
        {255, 0, 0}, {0, 0, 255}, {0, 255, 0}, {0, 255, 255}, {255, 0, 255},
        {255, 255, 0}, {128, 128, 0},  {128, 0, 128}, {0, 128, 128},
        {128, 128, 128}, {128, 255, 255},{128, 255, 0}, {128, 0, 255}};

    for (size_t k=0; k < results.size(); k++) {
        auto color = colors[k % colors.size()];
        int baseLine;
        cv::Size framework = cv::getTextSize(results[k].m_detectorFrameworkTag,
                                             cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
        // TODO: may exceed image's width and height
        putText(image, results[k].m_detectorFrameworkTag,
                cv::Point(framework.height, framework.height + k * framework.height top),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, color);
        cv::line(image, cv::Point(framework.height + framework.width + 2, k * framework.height top),
                 cv::Point(framework.height + framework.width + 4), color);
        for (const auto & r : results[k].m_results) {
            cv::rectangle(image, cv::Point(r.x, r.y), cv::Point(r.x + r.w, r.y + r.h), color);
            string lable = r.m_className + " - " + cv::format("%.2f", r.m_confidence);
            putText(image, label, cv::Point(r.x, r.y), FONT_HERSHEY_SIMPLEX, 0.5, color);
        }
    }
    return 0;
}

} // namespace ff_dynamic
