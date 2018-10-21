#include "ffmpegVideoEncode.h"

namespace ff_dynamic {

////////////////////////////////////
//  [initialization]
// please refers to "init_output_stream_encode", real staffs
int FFmpegVideoEncode::dynamicallyInitialize(const DavImplTravel::TravelStatic & in) {
    int ret = 0;
    LOG(INFO) << m_logtag << "open real ffmpeg VideoEncode: " << m_options.dump();
    const string encName = m_options.get(DavOptionCodecName());
    AVCodec *enc = avcodec_find_encoder_by_name(encName.c_str());
    if (!enc) {
        INFOIT(DAV_ERROR_IMPL_CODEC_NOT_FOUND,
              m_logtag + encName + " video encode name not found, use h264 as default");
        enc = avcodec_find_encoder_by_name("libx264");
        if (!enc) {
            ERRORIT(DAV_ERROR_IMPL_CODEC_NOT_FOUND,
                  m_logtag + "default libx264 encoder not found, create ffmpeg video encoder fail");
            return DAV_ERROR_IMPL_CODEC_NOT_FOUND;
        }
    }

    m_encCtx = avcodec_alloc_context3(enc);
    CHECK(m_encCtx != nullptr) << (m_logtag + " fail alloate encode context");
    // m_encCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; /* TODO: always output this one ?*/

    /* before open, some other parameters need be set */
    /* calculate encode aspect ratio */
    /* policy: change encode aspect ratio to keep display aspect ratio and without padding */
    int outWidth = 0;
    int outHeight = 0;
    ret = m_options.getVideoSize(outWidth, outHeight);
    if (ret < 0) {
        ERRORIT(DAV_ERROR_IMPL_DYNAMIC_INIT, "Cannot get encoder output WxH");
        return DAV_ERROR_IMPL_DYNAMIC_INIT;
    }
    AVRational outSar = av_mul_q(AVRational{outHeight * in.m_width, outWidth * in.m_height}, in.m_sar);
    AVRational setSar = {0, 1};
    ret = m_options.getAVRational("sar", setSar);
    if (ret == 0 && setSar != outSar) {
        LOG(WARNING) << m_logtag << "set sar is not right, use sar " << outSar
                     << " instead of setting one " << setSar << ". in " << in.m_width << "x"
                     << in.m_height << ", out " << outWidth << "x" << outHeight;
    }
    m_options.setAVRational("sar", outSar, 0); /* overwrite sar setting */

    /* framerate */
    AVRational fps = {0, 1};
    ret = m_options.getAVRational("framerate", fps);
    if (fps.num != 0)
        m_encCtx->framerate = fps;
    else if (in.m_framerate.num != 0)
        m_encCtx->framerate = in.m_framerate;
    else
        m_encCtx->framerate = {25, 1};

    /* allow forced idr. overwrite if exist */
    m_options.set("forced-idr", "1", 0);
    if ((ret = avcodec_open2(m_encCtx, enc, m_options.get())) < 0) {
        ERRORIT(ret, m_logtag + " encode open fail");
        return ret;
    }
    recordUnusedOpts();
    INFOIT(DAV_INFO_IMPL_INSTANCE_CREATE_DONE, m_logtag + " create VideoEncode done");
    return 0;
}

int FFmpegVideoEncode::
setupScaleFilter(const DavImplTravel::TravelStatic & in, const DavImplTravel::TravelStatic & out) {
    int ret = 0;
    if (m_scaleFilter) {
        delete m_scaleFilter;
        m_scaleFilter = nullptr;
    }

    if (in.m_codecpar) {
        m_sfp.m_inFormat = (enum AVPixelFormat) in.m_codecpar->format;
        m_sfp.m_inWidth = in.m_codecpar->width;
        m_sfp.m_inHeight = in.m_codecpar->height;
        m_sfp.m_inSar = in.m_codecpar->sample_aspect_ratio;
    }
    else {
        m_sfp.m_inFormat = (enum AVPixelFormat) in.m_pixfmt;
        m_sfp.m_inWidth = in.m_width;
        m_sfp.m_inHeight = in.m_height;
        m_sfp.m_inSar = in.m_sar;
    }
    // !! be note: use out timebase, incoming frame already converts its timestamp to encoder's timebase
    m_sfp.m_inTimebase = out.m_timebase;
    m_sfp.m_inFramerate = in.m_framerate;

    // output and other settings
    m_sfp.m_outWidth = out.m_codecpar->width;
    m_sfp.m_outHeight = out.m_codecpar->height;
    m_sfp.m_outTimebase = out.m_timebase;
    m_sfp.m_outFramerate = out.m_framerate;
    m_sfp.m_hwFramesCtx = out.m_hwFramesCtx;
    m_sfp.m_bFpsScale = true;
    m_sfp.m_logtag = appendLogTag(m_logtag, "-ScaleFilter");

    m_scaleFilter = new ScaleFilter();
    CHECK(m_scaleFilter != nullptr);
    ret = m_scaleFilter->initScaleFilter(m_sfp);
    if (ret < 0) {
        ERRORIT(ret, "encode's scale filter init failed");
        return ret;
    }
    return 0;
}

int FFmpegVideoEncode::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
    int ret = 0;
    if (m_encCtx)
        onDestruct();

    CHECK(m_inputTravelStatic.size() == ctx.m_froms.size() && ctx.m_froms.size() == 1);
    DavImplTravel::TravelStatic & in = m_inputTravelStatic.at(ctx.m_froms[0]);
    if (!in.m_codecpar && (in.m_pixfmt == AV_PIX_FMT_NONE)) {
        ERRORIT(DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR,
                m_logtag + "video encode cannot get valid codecpar or videopar");
        return DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR;
    }

    /* this is crucial: if some fields no set via options (such as width, height, framerate, etc..),
       use values from inTravelstatic */
    in.mergeVideoTravelStaticToDict(m_options);

    // 2. let's open the encoder
    ret = dynamicallyInitialize(in);
    if (ret < 0)
        return ret;

    // 3. ok then, set output infos
    m_timestampMgr.clear();
    m_outputTravelStatic.clear();

    // TODO: hwFramesctx
    DavImplTravel::TravelStatic out;
    out.setupVideoStatic(m_encCtx, m_encCtx->time_base, m_encCtx->framerate, nullptr);
    m_timestampMgr.insert(std::make_pair(ctx.m_froms[0], DavImplTimestamp(in.m_timebase, out.m_timebase)));
    m_outputTravelStatic.insert(std::make_pair(IMPL_SINGLE_OUTPUT_STREAM_INDEX, out));

    // 4. set scaleFilter (do this setup after encoder open)
    if (in.m_width != out.m_width || in.m_height != out.m_height) {
        ret = setupScaleFilter(in, out);
        if (ret < 0)
            return ret;
    } else {
        LOG(INFO) << m_logtag << "no scale filter needed";
    }

    m_bDynamicallyInitialized = true;
    LOG(INFO) << m_logtag << "dynamically create VideoEncode done.\nin static: " << in << ", \nout: " << out;
    return 0;
}

////////////////////////////////////
//  [event process]
/* it is caller's responsibility to avoid racing between event process and data process */
//int FFmpegVideoEncode::keyFrameRequest(const DavEvent & event) {
//    m_bForcedKeyFrame = true;
//    return 0;
//}

////////////////////////////////////
//  [construct - destruct - process]

int FFmpegVideoEncode::onConstruct() {
    LOG(INFO) << m_logtag << "will open after receive first frame, initial opts: " << m_options.dump() ;
    m_outputMediaMap.insert(std::make_pair(IMPL_SINGLE_OUTPUT_STREAM_INDEX, AVMEDIA_TYPE_VIDEO));
    /* register events */
    //int registerEvent(const type_index typeIndex, function<int (const DavEvent &)> & f) {
    // store a call to a member function
    // std::function<void(const Foo&, int)> f_add_display = &Foo::print_add;
    //type_index iFrameRequest = type_index(typeid(DavEventVideoIFrameRequest));
    //LOG(INFO) << typeid(DavEventVideoIFrameRequest).name() << " & " << typeid(AVCodecContext).name();
    //m_implEvent.registerEvent(iFrameRequest, );
    return 0;
}

int FFmpegVideoEncode::onDestruct() {
    if (m_encCtx)
        avcodec_free_context(&m_encCtx);
    ERRORIT(0, m_logtag + "FFmpeg VideoEncode Destruct");
    return 0;
}

int FFmpegVideoEncode::receiveEncodeFrames(DavProcCtx & ctx) {
    int ret = 0;
    do {
        auto outBuf = make_shared<DavProcBuf>();
        outBuf->m_travel.m_static = m_outputTravelStatic.at(IMPL_SINGLE_OUTPUT_STREAM_INDEX);
        AVPacket *pkt = outBuf->mkAVPacket();
        CHECK(pkt != nullptr);
        av_init_packet(pkt);
        ret = avcodec_receive_packet(m_encCtx, pkt);
        if (ret >= 0) {
            ctx.m_outBufs.push_back(outBuf);
            // TODO: if there is dynamic travel info, should only set to one outBuf
            continue;
        }
        else if (ret == AVERROR_EOF) {
            INFOIT(ret, m_logtag + "video encode fully flushed");
        }
        else if (ret < 0 && ret != AVERROR(EAGAIN)) {
            ERRORIT(ret, "video decode failed when receive frame");
            m_discardFrames++;
        }
        break;
    } while (true);

    return ret;
}

int FFmpegVideoEncode::onProcess(DavProcCtx & ctx) {
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!m_encCtx)
        return 0;
    if (!ctx.m_inBuf) {
        ERRORIT(DAV_ERROR_IMPL_UNEXPECT_EMPTY_INBUF,  "video encode should always has input");
        return DAV_ERROR_IMPL_UNEXPECT_EMPTY_INBUF;
    }

    int ret = 0;
    // when upper do flush will output an empty frame
    auto inFrame = ctx.m_inRefFrame;
    if (!inFrame) {
        LOG(INFO) << m_logtag << "video encode reciving flush frame";
        ctx.m_bInputFlush = true;
    }

    vector<shared_ptr<AVFrame>> encodeFrames;
    if (m_scaleFilter) {
        ret = m_scaleFilter->sendFrame(inFrame);
        if (ret < 0 && ret != AVERROR_EOF) {
            ERRORIT(ret, "encode's scale filter send frame failed");
            m_discardFrames++;
        }
        // receive scaled frames
        ret = m_scaleFilter->receiveFrames(encodeFrames);
        if (ret == AVERROR_EOF)
            encodeFrames.push_back(shared_ptr<AVFrame>()); /* frame flush to encoder */
    } else {
        shared_ptr<AVFrame> inSharedFrame(inFrame, [](AVFrame *p) {return;});
        encodeFrames.push_back(inSharedFrame);
    }

    for (size_t k = 0; k < encodeFrames.size(); k++) {
        if (m_bForcedKeyFrame) /* key frame set */
            encodeFrames[k]->pict_type = AV_PICTURE_TYPE_I;
        ret = avcodec_send_frame(m_encCtx, encodeFrames[k].get());
        if (ret == AVERROR_EOF) {
            ERRORIT(ret, "video encode failed, send frames after flush frame. Try receive packet anyway");
            m_bForcedKeyFrame = false;
            break;
        }
        else if (ret == AVERROR(EAGAIN)) {
            // in this case, should receive and then send again
            ret = receiveEncodeFrames(ctx);
            if (ret != AVERROR_EOF)
                k--;
        }
        else if (ret < 0) {
            ERRORIT(ret, "video encode failed when send frame");
            m_discardFrames++;
            continue;
        }
        if (m_bForcedKeyFrame) /* only ok after send_frame ok*/
            m_bForcedKeyFrame = false;
    }

    /* receive it */
    return receiveEncodeFrames(ctx);
}

///////////////////////////////////////
// [Register - auto, ffmpegVideoEncode]
DavImplRegister g_videoEncodeReg(DavWaveClassVideoEncode(),vector<string>({"auto", "ffmpeg"}),
                                 [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                     unique_ptr<FFmpegVideoEncode> p(new FFmpegVideoEncode(options));
                                     return p;
                                 });
} // namespace
