#include <algorithm>
#include <thread>
#include "videoMix.h"

namespace ff_dynamic {
//// Register ////
static DavImplRegister s_videoMixReg(DavWaveClassVideoMix(), vector<string>({"auto", "VideoMix"}), {},
                                     [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                         unique_ptr<DavImpl> p(new VideoMix(options));
                                         return p;
                                     });

const DavRegisterProperties & VideoMix::getRegisterProperties() const noexcept {
    return s_videoMixReg.m_properties;
}

 ////////////////////////////////
/* The base assumptions is:
     0. video send sync event, then audio can output with pts bigger than sync pts;
     1. video's sync pts monotonic increase
   Then we have following rules:
     1. each video stream maintain its own pts, could be smaller then video's CurMixPts;
     2. if video not coming and audio data is accumulated bigger than a max value, oldest audio data will be discarded
     3. jitter buffer is on video side, which will use 300ms - 500ms jitter
     4. each video stream may chase the mix stream, in case it is delayed and we don't want to  throw away too many farmes
*/

/* layout change or newly join/left update
     1. mininal cache: output as much as possible when all cells are present;
     2. max cache size is 500ms;
     3. cache process:
        1) output existing caches
        2) each stream maintain its own pts relative to mixPts to do the sync;
*/

//////////////////////////////////////////////////////////////////////////////////////////
constexpr AVRational VideoMix::s_sar;
constexpr AVRational VideoMix::s_timebase;

//////////////////////////////////////////////////////////////////////////////////////////
// [Event Process]

int VideoMix::onUpdateLayoutEvent(const DavDynaEventVideoMixLayoutUpdate & event) {
    std::thread([this, event]() {
            int ret = m_cellMixer.onUpdateLayoutEvent(event); /* in case this call block */
            if (ret < 0) {
                ERRORIT(ret, "when do layout update to ");
                return ret;
            }
            return 0;
        }).detach();
    return 0;
}

int VideoMix::onUpdateBackgroudEvent(const DavDynaEventVideoMixSetNewBackgroud & event) {
    int ret = m_cellMixer.onUpdateBackgroudEvent(event);
    if (ret < 0) {
        ERRORIT(ret, "do update backgroud fail");
        return ret;
    }
    return 0;
}

////////////////////////////////////
int VideoMix::constructVideoMixWithOptions() {
    int ret = 0;
    LOG(INFO) << m_logtag << "VideoMix options: " << m_options.dump();
    /* video size */
    ret = m_options.getVideoSize(m_width, m_height);
    if (ret == DAV_ERROR_DICT_NO_SUCH_KEY) {
        LOG(INFO) << m_logtag << "no video_size set, use default " << m_width << "x" << m_height;
    } else if (ret < 0) {
        ERRORIT(ret, "video size set not right");
        return ret;
    }
    /* framerate */
    AVRational framerate {0, 1};
    ret = m_options.getAVRational("framerate", framerate);
    if (ret == DAV_ERROR_DICT_NO_SUCH_KEY) {
        LOG(INFO) << m_logtag << "no framerate set, use default " << m_framerate;
    } else if (ret < 0) {
        ERRORIT(ret, "framerate set not right");
        return ret;
    }
    if (framerate.num)
        m_framerate = framerate;

    /* pixel fmt, right now only support AV_PIX_FMT_YUV420P */
    // av_dict_set(opts, "pixel_format", AV_PIX_FMT_NONE, 0)

    /* check whether do pts re-generate */
    ret = m_options.getBool(DavOptionVideoMixRegeneratePts(), m_bReGeneratePts);
    if (ret == DAV_ERROR_DICT_NO_SUCH_KEY) {
        LOG(INFO) << m_logtag << "no bReGeneratePts set, use default " << m_bReGeneratePts;
    } else if (ret < 0) {
        ERRORIT(ret, "video bReGeneratePts setting invalid");
        return ret;
    }
    /* check whether start mixing after all current participants joined, useful for static fixed number of inputs */
    ret = m_options.getBool(DavOptionVideoMixStartAfterAllJoin(), m_bStartAfterAllJoin);
    if (ret == DAV_ERROR_DICT_NO_SUCH_KEY) {
        LOG(INFO) << m_logtag << "no m_bStartAfterAllJoin set, use default " << m_bStartAfterAllJoin;
    } else if (ret < 0) {
        ERRORIT(ret, "video m_bStartAfterAllJoin setting invalid");
        return ret;
    }
    /* check whether start mixing after all current participants joined, useful for static fixed number of inputs */
    ret = m_options.getBool(DavOptionVideoMixQuitIfNoInputs(), m_bQuitIfNoInput);
    if (ret == DAV_ERROR_DICT_NO_SUCH_KEY) {
        LOG(INFO) << m_logtag << "no m_bQuitIfNoInput set, use default " << m_bQuitIfNoInput;
    } else if (ret < 0) {
        ERRORIT(ret, "video m_bQuitIfNoInput setting invalid");
        return ret;
    }

    /* check video cell adornmment settings */
    m_options.get(DavOptionVideoMixLayout()) >> m_layout;
    m_options.getInt("margin", m_adornment.m_marginSize);
    m_options.getInt("border_width", m_adornment.m_borderLineWidth);
    m_options.getInt("border_color", m_adornment.m_borderLineColor);
    m_options.getInt("fillet_radius", m_adornment.m_filletRadius);
    m_backgroudPath = m_options.get("backgroud_image_path");
    LOG(INFO) << m_logtag << "construct options: " << m_options.dump() << ", WxH=" << m_width << "x" << m_height;
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// [construct - destruct - process]
int VideoMix::onConstruct() {
    int ret = constructVideoMixWithOptions();
    if (ret < 0)
        return ret;

    /* output */
    auto out = make_shared<DavTravelStatic>();
    out->setupVideoStatic(m_pixfmt, m_width, m_height, s_timebase, m_framerate, s_sar, nullptr);
    m_outputMediaMap.emplace(IMPL_SINGLE_OUTPUT_STREAM_INDEX, AVMEDIA_TYPE_VIDEO);
    m_outputTravelStatic.emplace(IMPL_SINGLE_OUTPUT_STREAM_INDEX, out);

    /* register event: setLayout */
    std::function<int (const DavDynaEventVideoMixLayoutUpdate &)> f =
        [this](const DavDynaEventVideoMixLayoutUpdate & e) {return onUpdateLayoutEvent(e);};
    std::function<int (const DavDynaEventVideoMixSetNewBackgroud &)> g =
        [this](const DavDynaEventVideoMixSetNewBackgroud & e) {return onUpdateBackgroudEvent(e);};
    m_implEvent.registerEvent(f);
    m_implEvent.registerEvent(g);

    CellMixerParams cmp;
    cmp.m_outStatic = m_outputTravelStatic.at(IMPL_SINGLE_OUTPUT_STREAM_INDEX);
    cmp.m_initLayout = m_layout;
    cmp.m_adornment = m_adornment;
    cmp.m_bReGeneratePts = m_bReGeneratePts;
    cmp.m_bStartAfterAllJoin = m_bStartAfterAllJoin;
    cmp.m_logtag = trimStr(m_logtag) + "-CellMixer ";
    m_cellMixer.initMixer(cmp);
    /* TODO: bug here. could remove to onConstruct.
       if dynamic change backgroud request is coming before init done. then it is lost */
    if (!m_backgroudPath.empty()) {
        DavDynaEventVideoMixSetNewBackgroud event {m_backgroudPath};
        ret = m_cellMixer.onUpdateBackgroudEvent(event);
        if (ret < 0) /* not a fatal error */
            ERRORIT(ret, "do init update backgroud fail");
    }
   /* mark as initialized and process dynamic input peer in onProcess */
    m_bDynamicallyInitialized = true;
    LOG(INFO) << m_logtag << "VideoMix will deal with dynamic join in";
    return 0;
}

int VideoMix::onDestruct() {
    return 0;
}

int VideoMix::onProcess(DavProcCtx & ctx) {
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!ctx.m_inBuf)
        return 0;
    int ret = 0;
    const DavProcFrom & from = ctx.m_inBuf->getAddress();
    if (m_cellMixer.isNewcomer(from)) {
        if (m_bStartAfterAllJoin)
            m_cellMixer.setFixedInputNum((int)ctx.m_froms.size());
        ret = m_cellMixer.onJoin(from, m_inputTravelStatic.at(from));
        if (ret < 0) {
            ERRORIT(ret, "fail to join one input video cell " + toStringViaOss(from));
            return AVERROR(EAGAIN);
        }
        INFOIT(DAV_INFO_ADD_ONE_MIX_STREAM, toStringViaOss(from) + " -> to video mixer");
    }

    auto & inFrame = ctx.m_inRefFrame;
    if (!inFrame) {
        LOG(INFO) << m_logtag << "video mix recive one flush frame from " << from;
        ctx.m_bInputFlush = true;
    }

    ret = m_cellMixer.sendFrame(from, inFrame);
    if (ret < 0)
        ERRORIT(ret, "send frame to cell mixer fail " + toStringViaOss(from));

    if (ctx.m_bInputFlush) {
        LOG(INFO) << m_logtag << "Input flush: will process on left --> " << from;
        ret = m_cellMixer.onLeft(from);
        if (ret < 0)
            ERRORIT(ret, "video mix's cell mixer process onLeft fail " + toStringViaOss(from));
    }

    vector<AVFrame *> outFrames;
    m_cellMixer.receiveFrames(outFrames, ctx.m_pubEvents);
    for (auto & e : ctx.m_pubEvents) {
        e->getAddress().setFromStreamIndex(IMPL_SINGLE_OUTPUT_STREAM_INDEX);
    }
    for (auto & f : outFrames) {
        auto outBuf = make_shared<DavProcBuf>();
        outBuf->mkAVFrame(f);
        outBuf->m_travelStatic = m_outputTravelStatic.at(IMPL_SINGLE_OUTPUT_STREAM_INDEX);
        ctx.m_outBufs.push_back(outBuf);
        m_outputCount++;
        if (m_outputCount == 1) {
            LOG(INFO) << m_logtag << "video mix first output count; current input from " << from;
        }
    }
    /* finally, check quit */
    if (ctx.m_bInputFlush) {
        if (ctx.m_froms.size() > 1) /* still has valid inputs */
            return 0;
        if (!m_bQuitIfNoInput) { /* the only input left */
            INFOIT(ret, "video mix has no inputs, auto quit is false, stay still");
            ret = 0;
        } else {
            ret = AVERROR_EOF;
            INFOIT(ret, "video mix fully flushed, send EOF");
        }
    }
    return ret;
}

} // namespace
