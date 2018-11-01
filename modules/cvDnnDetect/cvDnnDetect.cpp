#include "cvDnnDetector.h"

namespace ff_dynamic {
/////////////////////////////////
// [Register - auto, cvDnnDetect] There is no data output but only peer events
static DavImplRegister s_cvDnnDetectReg(DavWaveClassCvDnnDetect(), vector<string>({"auto", "cvDnnDetect"}), {},
                                        [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                            unique_ptr<CvDnnDetect> p(new CvDnnDetect(options));
                                            return p;
                                        });
const DavRegisterProperties & CvDnnDetect::getRegisterProperties() const noexcept {
    return s_cvDnnDetectReg.m_properties;
}

////////////////////////////////////
//  [event process]
int CvDnnDetect::processChangeConfThreshold(const CvDynaEventChangeConfThreshold & e) {
    m_dps.m_confThreshold = e.m_confThreshold;
    return 0;
}

////////////////////////////////////
//  [construct - destruct - process]
int CvDnnDetect::onConstruct() {
    LOG(INFO) << m_logtag << "Creating open CvDnnDetect " << m_options.dump();
    std::function<int (const CvDynaEventChangeConfThreshold &)> f =
        [this] (const CvDynaEventChangeConfThreshold & e) {return processChangeConfThreshold(e);};
    m_implEvent.registerEvent(f);

    m_dps.m_detectorType = m_options.get("detector_type");
    m_dps.m_detectorFrameworkTag = m_options.get("detector_framework_tag");
    m_dps.m_modelPath = m_options.get("model_path");
    m_dps.m_configPath = m_options.get("config_path");
    m_dps.m_classnamePath = m_options.get("classname_path");

    m_options.getInt("backend_id", m_dps.m_backendID);
    m_options.getInt("target_id", m_dps.m_targetId);
    m_options.getDouble("scale_factor", m_dps.m_scaleFactor);
    m_options.getBool("swap_rb", m_dps.m_bSwapRb);
    m_options.getInt("width", m_dps.m_width);
    m_options.getInt("height", m_dps.m_height);
    m_options.getDouble("conf_threshold", m_dps.m_confThreshold);
    m_options.getDouble("conf_threshold", m_dps.m_confThreshold);
    vector<double> means;
    m_options.getDoubleArray("means", means);
    for (auto & m : means)
        m_dps.m_means.emplace_back(m);
    // what about fail ?
    m_net = readNet(m_dps.m_modelPath, m_dps.m_configPath, m_dps.m_detectorFrameworkTag);
    m_net.setPreferableBackend(m_dps.m_backendId);
    m_net.setPreferableTarget(m_dps.m_targetId);
    LOG(INFO) << m_logtag << "CvDnnDetect create done: " << m_dps;
    return 0;
}

int CvDnnDetect::onDestruct() {
    LOG(INFO) << m_logtag << "CvDnnDetect Destruct";
    return 0;
}

////////////////////////////////////
//  [dynamic initialization]
int CvDnnDetect::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
    CHECK(m_inputTravelStatic.size() == ctx.m_froms.size() && ctx.m_froms.size() == 1);
    DavImplTravel::TravelStatic & in = m_inputTravelStatic.at(ctx.m_froms[0]);
    if (!in.m_codecpar && (in.m_pixfmt == AV_PIX_FMT_NONE)) {
        ERRORIT(DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR,
                m_logtag + "dehaze cannot get valid codecpar or videopar");
        return DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR;
    }

    /* Ok, get input parameters, for some implementations may need those paramters do init */
    m_net.reset(new Dehazor());
    /* dehaze's options: if get fail, will use default one */
    double fogFactor = 0.95;
    m_options.getDouble(DavOptionDehazeFogFactor(), fogFactor);
    m_net->setFogFactor(fogFactor);

    /* set output infos */
    m_timestampMgr.clear();
    m_outputTravelStatic.clear();
    DavImplTravel::TravelStatic out(in); /* use the same timebase with the input */
    LOG(INFO) << m_logtag << "travel static input/output for dehaze is the same: " << in;
    m_timestampMgr.insert(std::make_pair(ctx.m_froms[0], DavImplTimestamp(in.m_timebase, in.m_timebase)));
    /* output only one stream: dehazed video stream */
    m_outputTravelStatic.insert(std::make_pair(IMPL_SINGLE_OUTPUT_STREAM_INDEX, out));

    /* ok, no other works needed */
    /* must set this one after init done */
    m_bDynamicallyInitialized = true;
    LOG(INFO) << m_logtag << "dynamically create dehazor done.\nin static: " << in << ", \nout: " << out;
    return 0;
}

////////////////////////////
//  [dynamic initialization]
/* here is the data process */
int CvDnnDetect::onProcess(DavProcCtx & ctx) {
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!ctx.m_inBuf) {
        ERRORIT(DAV_ERROR_IMPL_UNEXPECT_EMPTY_INBUF, "CvDnnDetect should always have input");
        return DAV_ERROR_IMPL_UNEXPECT_EMPTY_INBUF;
    }

    int ret = 0;
    /* ref frame is a frame ref to original frame (data shared),
       but timestamp is convert to current impl's timebase */
    auto inFrame = ctx.m_inRefFrame;
    if (!inFrame) {
        LOG(INFO) << m_logtag << "video dehaze reciving flush frame";
        ctx.m_bInputFlush = true;
        /* no implementation flush needed, so just return EOF */
        return AVERROR_EOF;
    }
    /* const DavProcFrom & from = ctx.m_inBuf->getAddress(); */

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

    cv::Mat bgrMat;
    cv::cvtColor(yuvMat, bgrMat, CV_YUV2BGR_I420);
    cv::Mat dehazedMat = m_net->process(bgrMat); /* do dehaze */
    cv::cvtColor(dehazedMat, yuvMat, CV_BGR2YUV_I420);

    /* then convert back to YUV420p (because our output travel static says so);
       if we state it is AV_PIX_FMT_BGR24, then no convertion needed here. */
    auto outBuf = make_shared<DavProcBuf>();
    auto outFrame = outBuf->mkAVFrame();
    outFrame->width = inFrame->width;
    outFrame->height = inFrame->height;
    outFrame->format = inFrame->format;
    av_frame_get_buffer(outFrame, 16);
    for (int k=0; k < outFrame->height; k++)
        memcpy(outFrame->data[0] + k * outFrame->linesize[0], yuvMat.data + k * outFrame->width, outFrame->width);
    for (int k=0; k < outFrame->height/2; k++) {
        memcpy(outFrame->data[1] + k * outFrame->linesize[1], u + k * outFrame->width/2, outFrame->width/2);
        memcpy(outFrame->data[2] + k * outFrame->linesize[2], v + k * outFrame->width/2, outFrame->width/2);
    }

    /* prepare output */
    outFrame->pts = inFrame->pts;
    outBuf->m_travel.m_static = m_outputTravelStatic.at(IMPL_SINGLE_OUTPUT_STREAM_INDEX);
    ctx.m_outBufs.push_back(outBuf);
    return 0;
}

///////////////////
// [Trival helpers]
std::ostream & operator<<(std::ostream & os, const DetectParams & p) {
    os << "[dectorType " << p.m_detectorType << ", detectorFrameworkTag " << p.m_detectorFrameworkTag
       << ", modelPath " << p.m_modelPath << ", configPath " << p.m_configPath
       << ", classnamePath " << p.m_classnamePath << ", backendId " << p.m_backendId
       << ", targetId " << p.m_targetId << ", scaleFactor " << p.m_scaleFactor
       << ", means " << Scalar << ", bSwapRb " << p.m_bSwapRb << ", width " << p.m_width
       << ", height " << p.m_height << ", confThreshold " << m_confThreshold;
    return os;
}

} // namespace ff_dynamic
