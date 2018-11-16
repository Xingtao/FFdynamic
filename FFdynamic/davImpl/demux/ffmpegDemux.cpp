#include <iostream>
#include <iomanip>
#include <memory>
#include "ffmpegDemux.h"

namespace ff_dynamic {
/* Limitations:
 1. Cannot deal with 'nb_program' > 1 (such as mpeg's MPTS)
 2. Won't demux streams other than audio/video, such as subtitle or data
 3. m_outputMediaMap's stream index (key) may not continuous since we skip subtitle or data streams
*/
//// Register ////
static DavImplRegister s_demuxReg(DavWaveClassDemux(), vector<string>({"auto", "ffmpeg"}),
                                  {}, /* static properties */
                                  [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                      unique_ptr<DavImpl> p(new FFmpegDemux(options));
                                      return p;
                                  });

const DavRegisterProperties & FFmpegDemux::getRegisterProperties() const noexcept {
    return s_demuxReg.m_properties;
}

//////////////////////////////////////////////////////////////////////////////////////////
int FFmpegDemux::dynamicallyInitialize () {
    // NOTE: for demux already initialized , here we just set up the output infos. also ignore 'ctx'
    m_outputMediaMap.clear();
    m_outputTravelStatic.clear();
    for (unsigned int k=0; k < m_fmtCtx->nb_streams; k++) {
        const AVStream *st = m_fmtCtx->streams[k];
        // only deal with audio/video, ignore subtitle or data stream
        if (st->codecpar->codec_type != AVMEDIA_TYPE_VIDEO && st->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
            continue;
        m_outputMediaMap.emplace(k, st->codecpar->codec_type);
        auto outStatic = make_shared<DavTravelStatic>();
        outStatic->m_timebase = st->time_base;
        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            /* r_fps is the base frame rate, it is just a guess */
            AVRational outFramerate = st->avg_frame_rate.num != 0 ? st->avg_frame_rate : st->r_frame_rate;
            outStatic->setupVideoStatic(st->codecpar, st->time_base, outFramerate);
        } else {
            outStatic->setupAudioStatic(st->codecpar, st->time_base);
        }
        // skip other fields and use codecpar
        LOG(INFO) << m_logtag << "Demux add one stream output: " << outStatic;
        m_outputTravelStatic.emplace(k, outStatic);
    }
    m_bDynamicallyInitialized = true;
    return 0;
}

int FFmpegDemux::onConstruct() {
    int ret = 0;
    LOG(INFO) << "Starting create FFmpegDemux: " << m_options.dump();
    m_inputUrl = m_options.get(DavOptionInputUrl());
    if (m_inputUrl.size() == 0) {
        ERRORIT(DAV_ERROR_DICT_MISS_INPUTURL, "Demux missing input url");
        return DAV_ERROR_DICT_MISS_INPUTURL;
    }

    m_options.getBool(DavOptionInputFpsEmulate(), m_bInputFpsEmulate);
    m_options.getInt(DavOptionReconnectRetries(), m_reconnectRetries);
    m_options.getInt(DavOptionRWTimeout(), m_rwTimeoutMs);

    m_fmtCtx = avformat_alloc_context();
    CHECK(m_fmtCtx != nullptr) << "Fail alloate fmt context";

    hardSettings();
    ret = avformat_open_input(&m_fmtCtx, m_inputUrl.c_str(), nullptr, m_options.get());
    if (ret < 0) {
        ERRORIT(ret, m_logtag + " Demux open input " + m_inputUrl + " failed");
        return ret;
    }
    recordUnusedOpts();

    /* TODO: block time set */
    ret = avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0) {
        ERRORIT(ret, m_logtag + " Find streams info of " + m_inputUrl + " failed");
        return ret;
    }

    /* can setup all output infos in onConstruct */
    dynamicallyInitialize();

    m_streamStartTime.resize(m_fmtCtx->nb_streams, -1);

    // TODO: save this to log.
    av_dump_format(m_fmtCtx, 1, m_inputUrl.c_str(), 0);
    LOG(INFO) << m_logtag << "Create FFmpegDemux Done: " << m_inputUrl;
    return 0;
}

int FFmpegDemux::onDestruct() {
    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
    }
    INFOIT(DAV_INFO_IMPL_INSTANCE_DESTROY_DONE,
           m_logtag + "General Demux closed: audio read " + std::to_string(m_outPacket[1]) + ", discard " +
           std::to_string(m_discardPacket[1]) + ", video read  " + std::to_string(m_outPacket[0]) +
           ", discard " + std::to_string(m_discardPacket[0]));
    return 0;
}

int FFmpegDemux::hardSettings() {
    // m_fmtCtx->flags |= AVFMT_FLAG_NONBLOCK; // won't block input
    if (LIBAVFORMAT_VERSION_MAJOR < 59)
        m_fmtCtx->flags |= AVFMT_FLAG_KEEP_SIDE_DATA;
    // TODO set timeout
    //m_fmtCtx->interrupt_callback.callback = interrupt_cb;
    //m_fmtCtx->interrupt_callback.opaque = (void*)this;
    return 0;
}

int FFmpegDemux::onProcess(DavProcCtx & ctx) {
    auto outBuf = make_shared<DavProcBuf>();
    AVPacket *pkt = outBuf->mkAVPacket();
    CHECK(pkt != nullptr);
    av_init_packet(pkt);
    int ret = 0;
    do {
        ret = av_read_frame(m_fmtCtx, pkt);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
                // TODO:  stat this event
                ERRORIT(ret, m_logtag + m_inputUrl + " demux read return EAGAIN");
                continue;
            }
            if (ret == AVERROR_EOF) {
                INFOIT(ret, m_logtag + m_inputUrl + " demux read eof");
                return ret;
            }
            /* TODO: restart read input, add logic here */
            ERRORIT(ret, m_logtag + m_inputUrl + " av_read_fream fail");
            ctx.m_implErr = m_implErr;
            ctx.m_outputTimes = 0;
            return ret;
        }

        m_inBytes += pkt->size;
        auto pktType = m_fmtCtx->streams[pkt->stream_index]->codecpar->codec_type;
        if (pktType == AVMEDIA_TYPE_AUDIO || pktType == AVMEDIA_TYPE_VIDEO)
            m_outPacket[(int)pktType]++;
        else { // ignore non audio/video packet
            av_packet_unref(pkt);
            continue;
        }

        if (pkt->dts == AV_NOPTS_VALUE) { // TODO: just throw away ?
            m_discardPacket[(int)pktType]++;
            m_discardBytes += pkt->size;
            ERRORIT(DAV_WARNING_IMPL_DROP_DATA,
                  (m_logtag + m_inputUrl + " read an invalid packet (no dts). discrad " +
                   av_get_media_type_string((enum AVMediaType)pktType) +
                   std::to_string(m_discardPacket[(int)pktType])));
            av_packet_unref(pkt);
            continue;
        }

        if (m_streamStartTime[pkt->stream_index] == -1) {
            m_streamStartTime[pkt->stream_index] = av_gettime_relative();
            LOG(INFO) << m_logtag << m_inputUrl + " stream " << pkt->stream_index
                      << " start at relative time " << m_streamStartTime[pkt->stream_index];
        }
        // ok then, set up travel static info
        outBuf->m_travelStatic = m_outputTravelStatic.at(pkt->stream_index);
        outBuf->getAddress().setFromStreamIndex(pkt->stream_index);
        break;
    } while(true);

    /* TODO: check side_data for dynamic change, then set to m_travelDynamic; */
    /* TODO: not accurate for some cases */
    const auto & st = m_fmtCtx->streams[pkt->stream_index];
    int64_t timeNow = av_gettime_relative();
    int64_t timeDiff = timeNow - m_streamStartTime[pkt->stream_index];
    if (m_bInputFpsEmulate && st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        /* TODO: avi container won't set start_time  */
        if (st->start_time != AV_NOPTS_VALUE && pkt->pts != AV_NOPTS_VALUE &&
            (pkt->pts - st->start_time > 0)) {
            const int64_t streamDiff = av_rescale_q(pkt->pts - st->start_time, st->time_base, AV_TIME_BASE_Q);
            if (streamDiff - timeDiff > 2000)
                av_usleep(streamDiff - timeDiff - 1000);
        }
    }

    ctx.m_outBufs.push_back(outBuf);
    LOG_IF(INFO, timeNow - m_lastLogTime >= 4*AV_TIME_BASE &&
           (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO))
        << m_logtag << " output video " << m_outPacket[(int)AVMEDIA_TYPE_VIDEO] << ", output audio "
        << m_outPacket[(int)AVMEDIA_TYPE_AUDIO] << ", this pkt size " << pkt->size << " pts " << pkt->pts
        << ", dst " << pkt->dts << ", fps " << std::setprecision(3)
        << (m_outPacket[(int)AVMEDIA_TYPE_VIDEO] * AV_TIME_BASE * 1.0 / timeDiff) << ", "
        << (m_lastLogTime = timeNow);

    /* ctx.m_expect not needed, default is eDavProcExpectNothing */
    return 0;
}

} // namespace
