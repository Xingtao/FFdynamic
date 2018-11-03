#include "davImpl.h"

namespace ff_dynamic {

DavMsgCollector & DavImpl::s_msgCollector = DavMsgCollector::getInstance();
DavImplFactory::DavClassImplMap DavImplFactory::s_classImplMap;
std::mutex DavImplRegister::s_mutex;

//////////////////////////////////////////////////////////////////////////////////////////
int DavImpl::process(DavProcCtx & ctx) {
    int ret = onPreProcess(ctx);
    if (ret < 0)
        return ret;
    ret = onProcess(ctx);
    if (ret < 0)
        return ret;
    return onPostProcess(ctx);
}

/* impl could override this method */
int DavImpl::onPreProcess(DavProcCtx & ctx) {
    int ret = 0;
    if (!ctx.m_inBuf) // no pre-process needed
        return 0;
    const DavProcFrom & from = ctx.m_inBuf->getAddress();
    if (m_inputTravelStatic.count(from) == 0)
        m_inputTravelStatic.emplace(from, ctx.m_inBuf->m_travelStatic);
    // /* else TODO: update, in case static changed before dynamicallly initialized */

    /* already initialized, convert its timestamp to impl's timebase then */
    if (m_bDynamicallyInitialized) {
        if (m_bDataRelay) /* no timestamp mgr and travel static output needed, just relay the data */
            return 0;
        if (m_timestampMgr.count(from) == 0) { /* newly connected peer */
            if (m_outputTravelStatic.size() > 1) /* multiple output streams, process inside implementation */
                return 0;
            /* single output stream, add its timestamp mgr here */
            for (auto & o : m_outputTravelStatic)
                if (o.first == IMPL_SINGLE_OUTPUT_STREAM_INDEX)
                    m_timestampMgr.emplace(from,
                       DavImplTimestamp(m_inputTravelStatic.at(from)->m_timebase,
                                        m_outputTravelStatic.at(IMPL_SINGLE_OUTPUT_STREAM_INDEX)->m_timebase));
        }

        const auto pkt = ctx.m_inBuf->getAVPacket();
        if (pkt) { /* shortcut for av_alloc_packet and av_packet_ref (copy props & ref buf) */
            ctx.m_inRefPkt = av_packet_clone(pkt);
            CHECK(ctx.m_inRefPkt != nullptr);
            ret = m_timestampMgr.at(from).packetRescaleTs(ctx.m_inRefPkt);
            if (ret == DAV_ERROR_IMPL_PKT_NO_DTS)
                return onPktNoValidDts(ctx, m_timestampMgr.at(from), ctx.m_inRefPkt);
            else if (ret == DAV_ERROR_IMPL_DTS_NOT_MONOTONIC)
                return onPktNonMonotonicDts(ctx, m_timestampMgr.at(from), ctx.m_inRefPkt);
        }
        const auto frame = ctx.m_inBuf->getAVFrame();
        if (frame) {
            /* shortcut for av_alloc_frame & av_frame_ref (copy props & ref buf); no acctual data copy */
            ctx.m_inRefFrame = av_frame_clone(frame);
            m_timestampMgr.at(from).frameRescaleTs(ctx.m_inRefFrame);
        }
        return ret;
    }

    /* not initialized yet, check peer validity or cache data to 'preInitCache queue' */
    if (ctx.m_inBuf->isEmptyData()) {
        ctx.m_bInputFlush = true;
        auto & buf = ctx.m_inBuf;
        m_preInitCacheInBufs.erase(std::remove_if(m_preInitCacheInBufs.begin(), m_preInitCacheInBufs.end(),
                                                  [&buf](const shared_ptr<DavProcBuf> & b) {
                                                      return b->getAddress() == buf->getAddress();
                                                  }),
                                   m_preInitCacheInBufs.end());
        INFOIT(DAV_ERROR_IMPL_CLEAR_CACHE_BUFFER,
               m_logtag + " not initialized and got flush buffer. delete all data from [" +
               buf->getAddress().m_descFrom + "] in cache");
        if (ctx.m_froms.size() == 1) /* the only one input peer finished */
            return AVERROR_EOF;
        return 0;
    }

    // TODO: should put this one to implementation
    if (ctx.m_froms.size() > m_inputTravelStatic.size()) {
        ctx.m_inBuf->unlimit();
        m_preInitCacheInBufs.push_back(ctx.m_inBuf);
        LOG_EVERY_N(WARNING, 100)
            << m_logtag + "cache data to preInitCache queue " + std::to_string(m_preInitCacheInBufs.size());
        // if (m_preInitCacheInBufs)
        // TODO: in some case, we should discard those caches
        return ret;
    }

    /* collect enough infos. 'm_timestampMgr' will be filled in this call */
    ret = onDynamicallyInitializeViaTravelStatic(ctx);
    if (ret < 0) {
        string implErr = m_implErr.m_msgDetail;
        ERRORIT(ret, implErr);
        return DAV_ERROR_IMPL_DYNAMIC_INIT;
    }

    // do initialized pre-process then
    return onPreProcess(ctx);
}

int DavImpl::onPostProcess(DavProcCtx & ctx) {
    /* remove flushed input peer from inputDavTravelStatic and timestammgr */
    if (ctx.m_inRefPkt)
        av_packet_free(&ctx.m_inRefPkt);
    if (ctx.m_inRefFrame)
        av_frame_free(&ctx.m_inRefFrame);
    if (ctx.m_bInputFlush) {
        const DavProcFrom & from = ctx.m_inBuf->getAddress();
        m_inputTravelStatic.erase(from);
        m_timestampMgr.erase(from);
    }
    return 0;
}

int DavImpl::onPktNonMonotonicDts(DavProcCtx & ctx, DavImplTimestamp & tsm, AVPacket *pkt) {
    LOG(WARNING) << m_logtag << " ==> non monotonic dts " << tsm.getLastDts() << " : " << pkt->dts;
    return 0;
}

int DavImpl::onPktNoValidDts(DavProcCtx & ctx, DavImplTimestamp & tsm, AVPacket *pkt) {
    LOG(WARNING) << m_logtag << " ==> no valid dts " << tsm.getLastDts() << " : " << pkt->dts;
    return 0;
}

} // namespace ff_dynamic
