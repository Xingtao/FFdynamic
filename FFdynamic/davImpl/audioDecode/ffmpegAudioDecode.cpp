#include "ffmpegAudioDecode.h"
#include "davProcCtx.h"
#include "davImplTravel.h"

namespace ff_dynamic {
//// Register ////
static DavImplRegister s_audioDecodeReg(DavWaveClassAudioDecode(), vector<string>({"auto", "ffmpeg"}), {},
                                        [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                            unique_ptr<DavImpl> p(new FFmpegAudioDecode(options));
                                            return p;
                                        });

const DavRegisterProperties & FFmpegAudioDecode::getRegisterProperties() const noexcept {
    return s_audioDecodeReg.m_properties;
}

//////////////////////////////////
// after got the first input, retrieve the travel static info to do the initialize
int FFmpegAudioDecode::dynamicallyInitialize(const AVCodecParameters *codecpar) {
    int ret = 0;
    LOG(INFO) << m_logtag << "open ffmpeg AudioDecode: " << m_options.dump();
    const string decName = m_options.get(DavOptionCodecName());
    AVCodec *dec = nullptr;
    if (decName.size() > 0)
        dec = avcodec_find_decoder_by_name(decName.c_str());
    else
        dec = avcodec_find_decoder(codecpar->codec_id);
    if (!dec) {
        ERRORIT(DAV_ERROR_IMPL_CODEC_NOT_FOUND,
              m_logtag + std::to_string(codecpar->codec_id) + " no found");
        return DAV_ERROR_IMPL_CODEC_NOT_FOUND;
    }

    m_decCtx = avcodec_alloc_context3(dec);
    CHECK(m_decCtx != nullptr) << "Fail alloate decode context";
    if ((ret = avcodec_parameters_to_context(m_decCtx, codecpar)) < 0) {
        ERRORIT(ret, m_logtag + "codecpar to context fail");
        return ret;
    }

    if ((ret = avcodec_open2(m_decCtx, dec, m_options.get())) < 0) {
        ERRORIT(ret, m_logtag + "decode open fail");
        return ret;
    }

    recordUnusedOpts();
    return 0;
}

int FFmpegAudioDecode::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
    if (m_decCtx)
        onDestruct();

    CHECK(m_inputTravelStatic.size() == ctx.m_froms.size() && ctx.m_froms.size() == 1);
    auto & in = m_inputTravelStatic.at(ctx.m_froms[0]);
    if (!in->m_codecpar && in->m_samplefmt == AV_SAMPLE_FMT_NONE) {
        ERRORIT(DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR,
                m_logtag + "audio decode cannot get valid codecpar");
        return DAV_ERROR_TRAVEL_STATIC_INVALID_CODECPAR;
    }

    // 2. let's open the decoder
    int ret = dynamicallyInitialize(in->m_codecpar.get());
    if (ret < 0)
        return ret;

    // 3. ok then, set output infos
    m_timestampMgr.clear();
    m_outputTravelStatic.clear();
    auto out = make_shared<DavTravelStatic>();
    AVRational outTimebase = {1, m_decCtx->sample_rate};
    out->setupAudioStatic(m_decCtx, outTimebase);
    m_timestampMgr.emplace(ctx.m_froms[0], DavImplTimestamp(in->m_timebase, outTimebase));
    m_outputTravelStatic.emplace(IMPL_SINGLE_OUTPUT_STREAM_INDEX, out);

    m_bDynamicallyInitialized = true;
    LOG(INFO) << m_logtag << "dynamically create AudioDecode done. in static: " << *in << "\nout: " << *out;
    return 0;
}

///////////////////////////////////
// [construct - destruct - process]

int FFmpegAudioDecode::onConstruct() {
    LOG(INFO) << m_logtag << "will open after receive first packet. 'FFmpegAudioDecode': " << m_options.dump() ;
    m_outputMediaMap.insert(std::make_pair(IMPL_SINGLE_OUTPUT_STREAM_INDEX, AVMEDIA_TYPE_AUDIO));
    return 0;
}

int FFmpegAudioDecode::onDestruct() {
    if (m_decCtx)
        avcodec_free_context(&m_decCtx);
    LOG(INFO) << m_logtag << "FFmpeg AudioDecode destruct";
    return 0;
}

int FFmpegAudioDecode::onProcess(DavProcCtx & ctx) {
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!m_decCtx || !ctx.m_inBuf)
        return 0;

    int ret = 0;
    auto pkt = ctx.m_inRefPkt;
    if (!pkt) {
        ctx.m_bInputFlush = true;
        LOG(INFO) << "audio decoding receive flush packet " << *ctx.m_inBuf;
    }

    ret = avcodec_send_packet(m_decCtx, pkt);
    if (ret < 0 && ret != AVERROR_EOF) {
        ERRORIT(ret, "audio send packet fail for decoding");
        return ret;
    }
    else if (ret == AVERROR_EOF)
        ERRORIT(ret, "audio send packet for decoding after flush packet");

    do
    {
        auto outBuf = make_shared<DavProcBuf>();
        outBuf->m_travelStatic = m_outputTravelStatic.at(IMPL_SINGLE_OUTPUT_STREAM_INDEX);
        AVFrame *frame = outBuf->mkAVFrame();
        CHECK(frame != nullptr);
        ret = avcodec_receive_frame(m_decCtx, frame);
        if (ret >= 0) {
            // TODO: if there is dynamic travel info, should only set to one outBuf
            ctx.m_outBufs.push_back(outBuf);
            m_outputFrames++;
            if (frame->best_effort_timestamp != AV_NOPTS_VALUE)
                frame->pts = frame->best_effort_timestamp;
            continue;
        }
        else if (ret == AVERROR_EOF) {
            ERRORIT(ret, m_logtag + "audio decode fully flushed");
            return ret;
        }
        else if (ret < 0 && ret != AVERROR(EAGAIN))
            ERRORIT(ret, "audio decode failed when receive frame");
        break;
    } while (true);

    LOG_IF(INFO, m_outputFrames % 1000 == 1) << m_logtag << "audio decode output frame " << m_outputFrames;
    return 0;
}
} // namespace
