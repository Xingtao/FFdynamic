#include "ffdynaDehazor.h"
#include "fmtScale.h"

namespace ff_dynamic {
///////////////////////////////////////
// [Register - auto, dehaze]: this will create PluginDehazor instance for dehaze
static DavImplRegister s_dehazeReg(DavWaveClassDehaze(), vector<string>({"auto", "dehaze"}), {},
                                   [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                       unique_ptr<PluginDehazor> p(new PluginDehazor(options));
                                       return p;
                                   });
const DavRegisterProperties & PluginDehazor::getRegisterProperties() const noexcept {
    return s_dehazeReg.m_properties;
}

/* Register Explaination */
/* If we have another dehaze implementation, let's say DehazorImpl2, we could register it as:
static DavImplRegister s_regDehazeP2(DavWaveClassVideoDehaze(), vector<string>({"dehazeImpl2"}), {},
                                     [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                         unique_ptr<DehazorImpl2> p(new DehazorImpl2(options));
                                         return p;
                                     });
   And, we could select them by set options:
       options..setDavWaveCategory((DavWaveClassDehaze()));
       options.set(EDavWaveOption::eImplType, "dehazeImpl2");
*/

////////////////////////////////////
//  [initialization]

int PluginDehazor::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
    CHECK(m_inputTravelStatic.size() == ctx.m_froms.size() && ctx.m_froms.size() == 1);
    const auto & in = m_inputTravelStatic.at(ctx.m_froms[0]);
    if (!in->m_codecpar && (in->m_pixfmt == AV_PIX_FMT_NONE)) {
        ERRORIT(DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR,
                m_logtag + "dehaze cannot get valid codecpar or videopar");
        return DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR;
    }

    /* Ok, get input parameters, for some implementations may need those paramters do init */
    m_dehazor.reset(new Dehazor());
    /* dehaze's options: if get fail, will use default one */
    double fogFactor = 0.95;
    m_options.getDouble(DavOptionDehazeFogFactor(), fogFactor);
    m_dehazor->setFogFactor(fogFactor);

    /* set output infos */
    m_timestampMgr.clear();
    m_outputTravelStatic.clear();
    auto out = make_shared<DavTravelStatic>(*in); /* use the same parameters with input */
    LOG(INFO) << m_logtag << "travel static input/output for dehaze is the same: " << in;
    m_timestampMgr.insert(std::make_pair(ctx.m_froms[0], DavImplTimestamp(in->m_timebase, in->m_timebase)));
    /* output only one stream: dehazed video stream */
    m_outputTravelStatic.insert(std::make_pair(IMPL_SINGLE_OUTPUT_STREAM_INDEX, out));

    /* ok, no other works needed */
    /* must set this one after init done */
    m_bDynamicallyInitialized = true;
    LOG(INFO) << m_logtag << "dynamically create dehazor done.\nin static: " << *in << ", \nout: " << *out;
    return 0;
}

////////////////////////////////////
//  [event process]
int PluginDehazor::processFogFactorUpdate(const FogFactorChangeEvent & e) {
    /* event process is in the same thread with data process thread, so no lock need here */
    if (m_dehazor)
        m_dehazor->setFogFactor(e.m_newFogFactor);
    return 0;
}
////////////////////////////////////
//  [construct - destruct - process]
int PluginDehazor::onConstruct() {
    /* construct phrase could do nothing, and put all iniitialization work in onDynamicallyInitializeViaTravelStatic,
       for some implementations rely on input peer's parameters to do the iniitialization */
    LOG(INFO) << m_logtag << "will open PluginDehazor after first frame " << m_options.dump();

    /* register events: also could put this one in dynamicallyInit part.
       Let's say we require dynamically change some dehaze's parameters, 'FogFactor':
       We can register an handler to process this.
     */
    std::function<int (const FogFactorChangeEvent &)> f =
        [this] (const FogFactorChangeEvent & e) {return processFogFactorUpdate(e);};
    m_implEvent.registerEvent(f);

    /* tells we only output one video stream */
    m_outputMediaMap.insert(std::make_pair(IMPL_SINGLE_OUTPUT_STREAM_INDEX, AVMEDIA_TYPE_VIDEO));
    return 0;
}

int PluginDehazor::onDestruct() {
    if (m_dehazor) {
        m_dehazor.reset();
    }
    INFOIT(0, m_logtag + "Plugin Dehazor Destruct");
    return 0;
}

/* here is the data process */
int PluginDehazor::onProcess(DavProcCtx & ctx) {
    /* for most of the cases, the process just wants one input data to process;
       few compplicate implementations may require data from certain peer, then can setup this one */
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!m_dehazor)
        return 0;
    if (!ctx.m_inBuf) {
        ERRORIT(DAV_ERROR_IMPL_UNEXPECT_EMPTY_INBUF, "video dehaze should always have input");
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
    cv::cvtColor(yuvMat, bgrMat, CV_YUV2BGR_YV12);
    cv::Mat dehazedMat = m_dehazor->process(bgrMat); /* do dehaze */
    cv::cvtColor(dehazedMat, yuvMat, CV_BGR2YUV_YV12);
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
    outBuf->m_travelStatic = m_outputTravelStatic.at(IMPL_SINGLE_OUTPUT_STREAM_INDEX);
    ctx.m_outBufs.push_back(outBuf);
    return 0;
}

} // namespace ff_dynamic
