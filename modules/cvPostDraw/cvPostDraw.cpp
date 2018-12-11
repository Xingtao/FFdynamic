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
int CvPostDraw::processObjDetectResult(const ObjDetectEvent & e) {
    const auto & from = e.getAddress();
    if (m_detectResults.count(from) == 0) {
        m_detectResults.emplace(from, vector<ObjDetectEvent>{e});
        return 0;
    }
    auto & v = m_detectResults.at(from);
    v.emplace_back(e);
    LOG(INFO) << m_logtag << "got result " << e << ", " << m_detectResults.at(from).size();
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
    std::function<int (const ObjDetectEvent &)> f =
        [this] (const ObjDetectEvent & e) {return processObjDetectResult(e);};
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
    /* could process without input buffer */
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectNothing};
    int ret = 0;
    if (ctx.m_inBuf) {
        auto inFrame = ctx.m_inRefFrame;
        if (!inFrame) {
            LOG(INFO) << m_logtag << "cv post draw reciving flush frame";
            ctx.m_bInputFlush = true;
            /* no flush needed, so just return EOF */
            return AVERROR_EOF;
        }
        CHECK((enum AVPixelFormat)inFrame->format == AV_PIX_FMT_YUV420P);
        m_cacheBufFrames.emplace_back(ctx.m_inBuf);
    }

    /* process detect results and draw results to frame */
    auto outs = ctx.m_outBufs;
    int usedFrameCount = 0;
    for (auto & in : m_cacheBufFrames) {
        auto frame = in->getAVFrame(); /* since in/out static are the same */
        /* first remove 'expired' results, also check
           whether this frame is useless (not detected, for detect with interval now) */
        int skipThisFrameCount = 0;
        for (auto & d : m_detectResults) {
            d.second.erase(std::remove_if(d.second.begin(), d.second.end(),
                                          [frame](const ObjDetectEvent & r) {
                                              return r.m_framePts < frame->pts;
                                          }), d.second.end());
            if (d.second.size() && d.second[0].m_framePts > frame->pts) {
                skipThisFrameCount++;
            }
        }

        if (skipThisFrameCount && skipThisFrameCount == m_detectResults.size()) {
            usedFrameCount++;
            continue;
        }

        /* then check whether all results are ready */
        bool bReady = false;
        vector<ObjDetectEvent> results;
        for (auto & d : m_detectResults) {
            if (d.second.size() == 0) /* not comming */ {
                bReady = false;
                usleep(1000);
                break;
            }
            if (d.second[0].m_framePts == frame->pts) {
                results.emplace_back(d.second[0]);
            }
            bReady = true;
        }
        if (!bReady)
            break;
        /* convert this frame to opencv Mat. TODO: directly to bgr mat */
        cv::Mat yuvMat;
        FrameMat::frameToMatYuv420(frame, yuvMat);
        /* draw the frame */
        cv::Mat image;
        cv::cvtColor(yuvMat, image, CV_YUV2BGR_YV12);
        drawResult(image, results);
        /* convert to yuv again (tedious, should have a auto format convertor) */
        auto outBuf = make_shared<DavProcBuf>();
        auto outFrame = outBuf->mkAVFrame();
        FrameMat::bgrMatToFrameYuv420(image, outFrame);
        outFrame->pts = frame->pts;
        outBuf->m_travelStatic = m_outputTravelStatic.at(IMPL_SINGLE_OUTPUT_STREAM_INDEX);
        ctx.m_outBufs.emplace_back(outBuf);
        usedFrameCount++;
    }
    m_cacheBufFrames.erase(m_cacheBufFrames.begin(), m_cacheBufFrames.begin() + usedFrameCount);
    return 0;
}

int CvPostDraw::drawResult(cv::Mat & image, const vector<ObjDetectEvent> & results) {
    const static vector<cv::Scalar> colors {
        {255, 215, 0}, {50, 205, 50}, {0, 0, 255}, {0, 255, 0}, {0, 255, 255}, {255, 0, 255},
        {128, 128, 0}, {128, 0, 128}, {0, 128, 128},
        {128, 128, 128}, {128, 255, 255},{128, 255, 0}, {128, 0, 255}};

    for (size_t k=0; k < results.size(); k++) {
        auto color = colors[k % colors.size()];
        int baseLine;
        cv::Size framework = cv::getTextSize(results[k].m_detectorFrameworkTag,
                                             cv::FONT_HERSHEY_SIMPLEX, 1, 1, &baseLine);
        cv::putText(image, results[k].m_detectorFrameworkTag,
                    cv::Point(framework.height, (k+1) * framework.height + k*2),
                    cv::FONT_HERSHEY_SIMPLEX, 1, color);
        for (const auto & r : results[k].m_results) {
            cv::rectangle(image, cv::Point(r.m_rect.x, r.m_rect.y),
                          cv::Point(r.m_rect.x + r.m_rect.w, r.m_rect.y + r.m_rect.h), color);
            string label = r.m_className + " - " + cv::format("%.2f", r.m_confidence);
            cv::Size labelSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.8, 1, &baseLine);
            putText(image, label, cv::Point(r.m_rect.x, r.m_rect.y + labelSize.height),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, color);
        }
    }
    return 0;
}

} // namespace ff_dynamic
