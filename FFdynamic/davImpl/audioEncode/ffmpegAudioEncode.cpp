#include <sstream>
#include <functional>
#include "ffmpegAudioEncode.h"

namespace ff_dynamic {

//// Register ////
static DavImplRegister s_auidoEncodeReg(DavWaveClassAudioEncode(), vector<string>({"auto", "ffmpeg"}), {},
                                        [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                            unique_ptr<DavImpl> p(new FFmpegAudioEncode(options));
                                            return p;
                                        });

const DavRegisterProperties & FFmpegAudioEncode::getRegisterProperties() const noexcept {
    return s_auidoEncodeReg.m_properties;
}

//////////////////
static int isFormatSupported(const AVCodec *codec, enum AVSampleFormat sampleFmt) {
    const enum AVSampleFormat *p = codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sampleFmt)
            return 0;
        p++;
    }
    return -1;
}

int FFmpegAudioEncode::sampleFormatSet(AVCodec *codec) {
    int sampleFmtInt = -1;
    int ret = m_options.getInt("sample_fmt", sampleFmtInt, AV_DICT_MATCH_CASE, 0, AV_SAMPLE_FMT_NB - 1);
    if (ret < 0) {
        ERRORIT(ret, "no sample_fmt settings in audio encode");
        return ret;
    }
    if (isFormatSupported(codec, (enum AVSampleFormat)sampleFmtInt) < 0) {
        const enum AVSampleFormat *p = codec->sample_fmts;
        const char *expect = av_get_sample_fmt_name((enum AVSampleFormat)sampleFmtInt);
        LOG(INFO) << m_logtag << " audio encoder does not support sample format "
                  << (expect ? expect : "unknown") << ", use default sample format "
                  << av_get_sample_fmt_name(*p);
        m_encCtx->sample_fmt = *p;
    }
    else
        m_encCtx->sample_fmt = (enum AVSampleFormat) sampleFmtInt;
    return 0;
}

int FFmpegAudioEncode::dynamicallyInitialize() {
    int ret = 0;
    LOG(INFO) << m_logtag << "open real ffmpeg AudioEncode: " << m_options.dump();
    const string encName = m_options.get(DavOptionCodecName());
    AVCodec *enc = avcodec_find_encoder_by_name(encName.c_str());
    if (!enc) {
        ERRORIT(DAV_ERROR_IMPL_CODEC_NOT_FOUND,
              m_logtag + encName + ": audio encode name not found, use aac as default");
        enc = avcodec_find_encoder_by_name("aac");
        if (!enc) {
            ERRORIT(DAV_ERROR_IMPL_CODEC_NOT_FOUND,
                  m_logtag + "default aac encoder not found, create ffmpeg audio encoder fail");
            return DAV_ERROR_IMPL_CODEC_NOT_FOUND;
        }
    }

    m_encCtx = avcodec_alloc_context3(enc);
    CHECK(m_encCtx != nullptr) << (m_logtag + " fail alloate encode context");

    /* before open encoder, do sample format setting */
    ret = sampleFormatSet(enc);
    if (ret < 0)
        return ret;

    /* open encoder */
    if ((ret = avcodec_open2(m_encCtx, enc, m_options.get())) < 0) {
        ERRORIT(ret, m_logtag + " encode open fail");
        return ret;
    }

    recordUnusedOpts();
    if (enc->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) {
        /* If AV_CODEC_CAP_VARIABLE_FRAME_SIZE is set, then each frame can have any number of samples.
           If it is not set, frame->nb_samples must be equal to avctx->frame_size for all frames except the last.
           The final frame may be smaller than avctx->frame_size. */
        LOG(INFO) << m_logtag << "audio codec support AV_CODEC_CAP_VARIABLE_FRAME_SIZE";
        /* this is not important, just always send the required frame size to avoid code verbose */
    }
    return 0;
}

/* could be dynamically re-opened if parameters changed */
int FFmpegAudioEncode::openAudioResample(const DavTravelStatic & in, const DavTravelStatic & out) {
    int ret = 0;
    if (m_resampler) {
        delete m_resampler;
        m_resampler = nullptr;
    }

    AudioResampleParams arp;
    arp.m_logtag = m_logtag;
    arp.m_srcFmt = in.m_samplefmt;
    arp.m_srcSamplerate = in.m_samplerate;
    arp.m_srcLayout = in.m_channelLayout;
    arp.m_dstFmt = out.m_samplefmt;
    arp.m_dstSamplerate = out.m_samplerate;
    arp.m_dstLayout = out.m_channelLayout; /* mostly, it is AV_CH_LAYOUT_STEREO */

    /* if src parameters == dst parameters, then there will be no actual resample happen */
    m_resampler = new AudioResample();
    CHECK(m_resampler !=nullptr);
    ret = m_resampler->initResampler(arp);
    if (ret < 0) {
        ERRORIT(DAV_ERROR_IMPL_CREATE_AUDIO_RESAMPLE, "failed create audio resample: " + davMsg2str(ret));
        return DAV_ERROR_IMPL_CREATE_AUDIO_RESAMPLE;
    }
    return ret;
}

int FFmpegAudioEncode::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
    int ret = 0;
    if (m_encCtx)
        onDestruct();

    CHECK(m_inputTravelStatic.size() == ctx.m_froms.size() && ctx.m_froms.size() == 1);
    auto & in = m_inputTravelStatic.at(ctx.m_froms[0]);
    if (!in->m_codecpar && in->m_samplefmt == AV_SAMPLE_FMT_NONE) {
        ERRORIT(DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR,
                m_logtag + "audio encode cannot get valid codecpar");
        return DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR;
    }

    /* this is crucial: if some fields no set via options (such as samplerate, channels, etc..),
       use values from in Travelstatic */
    in->mergeAudioDavTravelStaticToDict(m_options);

    // 2. let's open the encoder
    ret = dynamicallyInitialize();
    if (ret < 0)
        return ret;

    // 3. set output infos
    m_timestampMgr.clear();
    m_outputTravelStatic.clear();

    auto out = make_shared<DavTravelStatic>();
    out->setupAudioStatic(m_encCtx, {1, m_encCtx->sample_rate});
    m_timestampMgr.emplace(ctx.m_froms[0], DavImplTimestamp(in->m_timebase, out->m_timebase));
    m_outputTravelStatic.emplace(IMPL_SINGLE_OUTPUT_STREAM_INDEX, out);

    // 4. audio resample init if needed
    ret = openAudioResample(*in, *out);
    if (ret < 0)
        return ret;
    else if (ret == 0 && m_resampler == nullptr) {
        // create an audio fifo
        // m_inFifo
    }

    m_bDynamicallyInitialized = true;
    LOG(INFO) << m_logtag << "dynamically create AudioEncode done. encode frame size " << m_encCtx->frame_size
              << ", in static: " << *in << "\nout: " << *out;
    return 0;
}

/////////////////////////////////////////////
//  [construct - destruct - process]

int FFmpegAudioEncode::onConstruct() {
    LOG(INFO) << m_logtag << "will open after receive first packet, initial opts: " << m_options.dump();
    m_outputMediaMap.insert(std::make_pair(IMPL_SINGLE_OUTPUT_STREAM_INDEX, AVMEDIA_TYPE_AUDIO));
    return 0;
}

int FFmpegAudioEncode::onDestruct() {
    if (m_encCtx)
        avcodec_free_context(&m_encCtx);
    if (m_resampler) {
        delete m_resampler;
        m_resampler = nullptr;
    }
    INFOIT(0, m_logtag + "FFmpeg AudioEncode Destruct");
    return 0;
}

int FFmpegAudioEncode::onProcess(DavProcCtx & ctx) {
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!m_encCtx || !ctx.m_inBuf)
        return 0;

    int ret = 0;
    int expectSize = m_encCtx->frame_size;
    AVFrame *inFrame = ctx.m_inRefFrame;
    if (!inFrame) {
        LOG(INFO) << m_logtag << "audio encode reciving flush frame";
        ctx.m_bInputFlush = true;
        expectSize = 0;
    } else {
        m_inFrames++;
        m_totalInputSamples += inFrame->nb_samples;
    }

    ret = m_resampler->sendResampleFrame(inFrame);
    if (ret < 0) {
        ERRORIT(ret, m_logtag + "send resample frame fail");
        m_discardInput++;
    }

    vector<shared_ptr<AVFrame>> resampledFrames;
    ret = m_resampler->receiveResampledFrame(resampledFrames, expectSize);
    if (ret == AVERROR_EOF)
        INFOIT(ret, "resampler return EOF. flush done");

    for (size_t k=0; k < resampledFrames.size(); k++) {
        ret = avcodec_send_frame(m_encCtx, resampledFrames[k].get());
        if (ret == AVERROR_EOF) {
            ERRORIT(ret, "audio encode failed, send frames after flush frame. Try receive packet anyway");
            break;
        } else if (ret == AVERROR(EAGAIN)) { /* receive frames then send this frame again */
            ret = receiveEncodeFrames(ctx);
            if (ret != AVERROR_EOF)
                return ret;
            k--; // send this frame again
        } else if (ret < 0) {
            ERRORIT(ret, "audio encode failed when send frame");
            m_discardOutput++;
        }
    }

    return receiveEncodeFrames(ctx);
}

int FFmpegAudioEncode::receiveEncodeFrames(DavProcCtx & ctx) {
    int ret = 0;
    do
    {
        auto outBuf = make_shared<DavProcBuf>();
        outBuf->m_travelStatic = m_outputTravelStatic.at(IMPL_SINGLE_OUTPUT_STREAM_INDEX);
        AVPacket *pkt = outBuf->mkAVPacket();
        CHECK(pkt != nullptr);
        av_init_packet(pkt);
        ret = avcodec_receive_packet(m_encCtx, pkt);
        if (ret >= 0) {
            ctx.m_outBufs.push_back(outBuf);
            m_outFrames += 1;
            m_totalOutputBytes += pkt->size;
            continue;
        } else if (ret == AVERROR_EOF) {
            INFOIT(ret, m_logtag + "audio encode fully flushed. end process.");
        } else if (ret < 0 && ret != AVERROR(EAGAIN)) {
            ERRORIT(ret, "audio decode failed when receive frame");
        }
        break;
    } while (true);

    LOG_EVERY_N(INFO, 1000) << m_logtag << "audio in " << m_inFrames << ", out " << m_outFrames
                            << ", discard in " << m_discardInput << ", out " << m_discardOutput
                            << ", encode bytes " << m_totalOutputBytes;
    return ret;
}
} // namespace
