#include <math.h>
#include <utility>
#include "ffmpegMux.h"

namespace ff_dynamic {
//// Register ////
static DavImplRegister s_muxReg(DavWaveClassMux(), vector<string>({"auto", "ffmpeg"}), {},
                                [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                    unique_ptr<DavImpl> p(new FFmpegMux(options));
                                    return p;
                                });

const DavRegisterProperties & FFmpegMux::getRegisterProperties() const noexcept {
    return s_muxReg.m_properties;
}

//////////////////////////////////////////////////////////////////////////////////////////
//int FFmpegMux::interruptCallback(void *p) {
//    return 0;
//}

AVStream* FFmpegMux::addOneStream(const DavTravelStatic & travelStatic) {
    int ret = 0;
    AVStream *st = avformat_new_stream(m_fmtCtx, nullptr);
    CHECK(st != nullptr);
    st->time_base = travelStatic.m_timebase;
    ret = avcodec_parameters_copy(st->codecpar, travelStatic.m_codecpar.get());
    CHECK (ret >= 0);
    const AVCodecDescriptor *desc = avcodec_descriptor_get(st->codecpar->codec_id);
    string codecName = desc ? desc->name : "unknown";
    LOG(INFO) << m_logtag << "add '" << av_get_media_type_string(st->codecpar->codec_type) << "' stream "
              << st->index << " of codec " << codecName << " with params: \n"
              << "In static " << travelStatic;
    return st;
}

int FFmpegMux::onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {
    if (m_fmtCtx)
        onDestruct();
    m_timestampMgr.clear();
    m_muxStreamsMap.clear();
    const string outFmt = m_options.get(DavOptionContainerFmt());
    LOG(INFO) << m_logtag << "open Muxer options: " << m_options.dump();
    int ret = avformat_alloc_output_context2(&m_fmtCtx, nullptr,
                                             outFmt.empty() ? nullptr : outFmt.c_str(), m_outputUrl.c_str());
    if (ret < 0) {
        ERRORIT(ret, "failed create mux format context: " + m_outputUrl);
        return ret;
    }
    recordUnusedOpts();

    /* add streams */
    for (auto & s :  m_inputTravelStatic) {
        AVStream *st = addOneStream(*s.second);
        m_muxStreamsMap.insert(std::make_pair(s.first, st));
    }

    /* TODO: avio options */
    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open2(&m_fmtCtx->pb, m_outputUrl.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
        if (ret < 0) {
            ERRORIT(ret, "Could not open output file " + m_outputUrl);
            return ret;
        }
    }

    /* write header: TODO: may add header options */
    ret = avformat_write_header(m_fmtCtx, nullptr);
    if (ret < 0) {
        ERRORIT(ret, m_logtag + "failed to write header " + m_outputUrl);
        return ret;
    }
    av_dump_format(m_fmtCtx, 0, m_outputUrl.c_str(), 1);

    /* set timestamp info after write header, no outputTravelStatic needed */
    for (auto & s : m_inputTravelStatic) {
        auto st = m_muxStreamsMap.at(s.first);
        m_timestampMgr.insert(std::make_pair(s.first, DavImplTimestamp(s.second->m_timebase, st->time_base)));
        LOG(INFO) << m_logtag << "stream " << st->index << " final timebase " << st->time_base;
    }

    /* write cache data out */
    auto cacheDataSize = m_preInitCacheInBufs.size();
    while (m_preInitCacheInBufs.size() > 0) { /* cache buf won't have flush packet */
        auto & buf = m_preInitCacheInBufs.front();
        AVPacket *pkt = av_packet_clone(buf->getAVPacket());
        CHECK(pkt != nullptr);
        /* cache buffer haven't scale its timestamp yet */
        ret = m_timestampMgr.at(buf->getAddress()).packetRescaleTs(pkt);
        if (ret < 0) {// non-monotonic
            LOG(WARNING) << m_logtag << " non-monotonic packet in";
            continue; // TODO!
        }
        pkt->stream_index = m_muxStreamsMap.at(buf->getAddress())->index;
        ret = av_interleaved_write_frame(m_fmtCtx, pkt);
        av_packet_free(&pkt);
        m_preInitCacheInBufs.pop_front();
        if (ret < 0) {
            ERRORIT(ret, "mux do interleave write fail");
            return ret;
        }
    }

    m_bDynamicallyInitialized = true;
    LOG(INFO) << m_logtag << "open FFmpeg mux done. write header and cached " << cacheDataSize
              << " packets to " << m_outputUrl;
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
int FFmpegMux::onConstruct() {
    m_outputUrl = m_options.get(DavOptionOutputUrl());
    if (m_outputUrl.empty()) {
        ERRORIT(DAV_ERROR_DICT_MISS_OUTPUTURL, m_logtag + "mux missing output url");
        return DAV_ERROR_DICT_MISS_OUTPUTURL;
    }
    LOG(INFO) << m_logtag << "will open after receive all stream info, initial opts: " << m_options.dump();
    return 0;
}

int FFmpegMux::onDestruct() {
    if (m_fmtCtx) {
        if (m_fmtCtx->pb)
            avio_close(m_fmtCtx->pb);
        avformat_free_context(m_fmtCtx);
        m_fmtCtx = nullptr;
    }
    m_muxStreamsMap.clear();
    return 0;
}

int FFmpegMux::onProcess(DavProcCtx & ctx) {
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!m_bDynamicallyInitialized || !ctx.m_inBuf) {
        return 0;
    }

    auto pkt = ctx.m_inRefPkt;
    if (!pkt) {
        ctx.m_bInputFlush = true;
        LOG(INFO) << m_logtag << "receve one flush packet from " << *ctx.m_inBuf;
    }
    else
        pkt->stream_index = m_muxStreamsMap.at(ctx.m_inBuf->getAddress())->index;

    int ret = av_interleaved_write_frame(m_fmtCtx, pkt);
    if (ret < 0) {
        m_outputDiscardCount++;
        // TODO: potential bug here: if pkt == null (flush packet), we shouldn't return here.
        ERRORIT(ret, "mux do interleave write fail, total discard " + std::to_string(m_outputDiscardCount));
        return ret;
    }
    if (ctx.m_bInputFlush && ctx.m_froms.size() == 1) {
        av_write_trailer(m_fmtCtx);
        ret = AVERROR_EOF;
        LOG(INFO) << m_logtag << (m_logtag + m_outputUrl + " end process, write file trailer done");
    }
    // TODO: make audio/video separately
    m_outputCount++;
    LOG_EVERY_N(INFO, 500) << m_logtag << "mux frames " << m_outputCount;
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////
int FFmpegMux::muxMetaDataSettings() {
    av_dict_set(&m_fmtCtx->metadata, "encoder", "ff_dynamic", 0);
    av_dict_set(&m_fmtCtx->metadata, "encoded_by", "ff_dynamic", 0);
    av_dict_set(&m_fmtCtx->metadata, "publisher", "ff_dynamic", 0);
    av_dict_set(&m_fmtCtx->metadata, "service_name", "ff_dynamic", 0);
    av_dict_set(&m_fmtCtx->metadata, "service_provider", "ff_dynamic", 0);
    av_dict_set(&m_fmtCtx->metadata, "title", "ff_dynamic", 0);
    return 0;
}
} //namespace
