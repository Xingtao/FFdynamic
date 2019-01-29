#include <sstream>
#include "davProc.h"

namespace ff_dynamic {
using ::std::mutex;

//////////////////////////////////////////////////////////////////////////////////////////
/* static functions: generate an unique DavProc index for log reading convnience */
mutex DavProc::s_idxMutex;
int DavProc::s_autoIdx = 1;
DavMsgCollector & DavProc::s_msgCollector = DavMsgCollector::getInstance();

/////////////////////////////////////////////////////////////////////////////////////////
DavProc::DavProc (const DavWaveOption & options) noexcept
    : m_idx([](){std::lock_guard<mutex> lock(s_idxMutex); return s_autoIdx++;}()) {

    if (options.isCategoryOptionsEmpty()) {
        ERROR(DAV_ERROR_BASE_EMPTY_OPTION, "please check creation ClassCategory options");
        return;
    }
    if (options.isDavOptionsEmpty()) {
        ERROR(DAV_ERROR_BASE_EMPTY_OPTION, "please check creation ImplType options");
        return;
    }

    const string implType = options.get(DavOptionImplType(), "");
    options.getCategory(DavOptionClassCategory(), m_waveCategory);
    m_className = m_waveCategory.name();
    m_logtag = mkLogTag(m_className + "-" + implType + std::to_string(m_idx));

    // do the creation
    DavWaveOption createOptions(options); /* will do deep copy */
    createOptions.set(DavOptionLogtag(), m_logtag);
    m_impl = DavImplFactory::create(createOptions, m_procInfo);
    s_msgCollector.addMsg(m_procInfo);
    LOG(INFO) << m_logtag << m_procInfo;

    m_dataTransmitor = make_shared<DavTransmitor<DavProcBuf, DavProcFrom>>(m_logtag);
    m_pubsubTransmitor = make_shared<DavTransmitor<DavPeerEvent, DavProcFrom>>(m_logtag + "[Event]");
    m_outbufLimiter = make_shared<DavProcBufLimiter>();

    DavProcFrom selfAddreess(this);
    m_dataTransmitor->setSelfAddress(selfAddreess);
    m_pubsubTransmitor->setSelfAddress(selfAddreess);
    return;
}

DavProc::~DavProc() {
    if (m_processThread)
        m_processThread->join();
    const auto inputStat = m_dataTransmitor->getReceiveCountStat();
    const auto outputStat = m_dataTransmitor->getSendCountStat();
    INFO(DAV_INFO_BASE_DESTRUCT_DONE,
         "Base destruct done. Total in stat: " + inputStat + "\nout stat: " + outputStat);
    m_dataTransmitor->clear();
    m_pubsubTransmitor->clear();
    m_bAlive = false;
    m_bOnFire = false;
    return;
}

/////////////////////////////////////////////////////////////////////////////////////////
// prerocess. could be overrided
int DavProc::preProcess(shared_ptr<DavProcBuf> & inBuf) {
    /* sub-pub event process and input data get */
    int ret = 0;
    bool bGet = false;
    do {
        if (!m_bAlive)
            return AVERROR_EOF;
        shared_ptr<DavPeerEvent> event = m_pubsubTransmitor->retrive();
        if (event) {
            ret = m_impl->processPeerEvent(*event);
            if (ret < 0) {
                m_procInfo = m_impl->getImplErr();
                ERROR(ret, "Fail process one event: " + m_procInfo.m_msgDetail);
            }
        }
        /* get input data */
        bGet = m_dataTransmitor->expect(inBuf, m_expectInput, (int)ETimeUs::e5ms);
    } while (!bGet);
    return 0;
}

int DavProc::process(DavProcCtx & ctx) {
    if (m_impl) {
        /* do data prefilter here */
        int ret = 0;
        for (auto & prefilter : m_prefilters) {
            // TODO: preserve data from and other bookkeepings
            // shared_ptr<DavProcBuf> inBuf = ;
            // auto filterrdInBuf = prefilter(ctx.m_inBuf);
        }
        ret = m_impl->process(ctx);
        if (ret < 0)
            m_procInfo = m_impl->getImplErr();
        return ret;
    }
    ERROR(DAV_ERROR_BASE_EMPTY_IMPL, m_logtag + "DavProcBase encounters empty implementation");
    return DAV_ERROR_BASE_EMPTY_IMPL;
}

int DavProc::postProcess(DavProcCtx & ctx) {
    /* post process */
    if (ctx.m_bInputFlush) {
        m_dataTransmitor->deleteSender(ctx.m_inBuf->getAddress());
        INFO(DAV_INFO_BASE_DELETE_ONE_RECEIVER, m_logtag + "one input end " + toStringViaOss(*ctx.m_inBuf));
    }
    m_dataTransmitor->farwell(ctx.m_inBuf); /* with 100ms timeout */
    m_expectInput = ctx.m_expect; /* next process expected input */

    /* post process for event */
    for (const auto & e : ctx.m_pubEvents) {
        /* impl only know event's streamIndex, so we fill other fields here */
        e->getAddress().setGroupFrom(this, m_groupId);
        m_pubsubTransmitor->broadcast(e);
    }

    /* post process for output */
    for (auto & buf : ctx.m_outBufs) {
        for (auto & prefilter : m_postfilters) {
            // TODO: preserve data from and other bookkeepings
            // shared_ptr<DavProcBuf> inBuf = ;
            // auto filterrdInBuf = prefilter(ctx.m_inBuf);
        }
        buf->getAddress().setGroupFrom(this, m_groupId);
        for (int k=0; k < ctx.m_outputTimes; k++) {
            m_dataTransmitor->delivery(buf);
            auto r = m_dataTransmitor->getRecipients();
        }
    }
    /* limit output buf number if needed.
       TODO: if stop at this point, may block here; should use timeout wait */
    m_outbufLimiter->limit(ctx.m_outBufs); /* with 100ms timeout */
    return 0;
}

int DavProc::runDavProcThread() {
    INFO(DAV_INFO_RUN_PROCESS_THREAD, m_logtag + " run DavProc processing thread");
    int ret = 0;

    while (m_bAlive) {
        shared_ptr<DavProcBuf> inBuf;
        ret = preProcess(inBuf);
        if (ret == AVERROR(EAGAIN)) /* check after pre-process to make sure we would like to continue process */
            continue;
        else if (ret == AVERROR_EOF)
            break;

        DavProcCtx ctx(m_dataTransmitor->getSenders());
        {   /* unique_lock with cv */
            std::unique_lock<mutex> lock(m_runLock);
            m_runCondVar.wait(lock, [this](){return (this->m_bOnFire ? true : false);});
            getProcessStartTime();
            /*
            if (inBuf && !m_dataTransmitor->isSenderStillValid(inBuf->getAddress())) {
                m_dataTransmitor->farwell(inBuf);
                continue; // external event process may delete input peer
            } */
            ctx.m_inBuf = inBuf;
            ret = process(ctx);
            getProcessEndTime();
            if (ret == AVERROR_EOF) {
                m_bImplProcessEof = true;
            } else if (ret == DAV_ERROR_IMPL_DYNAMIC_INIT) {
                ERROR(ret, "Fail with proc's process, quit proc thread. " + m_procInfo.m_msgDetail);
                m_bImplProcessEof = true;
                break;
            } else if (ret < 0 && ret != AVERROR(EAGAIN))
                ERROR(ret, "Fail with proc one frame; try again");
        }

        postProcess(ctx);

        /* impl process finish */
        if (m_bImplProcessEof) {
            INFO(DAV_INFO_BASE_END_PROCESS, m_logtag + "self process done, will quit run process");
            break;
        }
    }

    /* broadcast EOF to downstream peers */
    DavProcFrom flushFrom(this, m_groupId, DavProcFrom::s_flushIndex);
    auto flushBuf = make_shared<DavProcBuf>();
    flushBuf->setAddress(flushFrom);
    m_dataTransmitor->broadcast(flushBuf);
    /* will make upstream peer clear this proc as output */
    m_dataTransmitor->clear(); /* release remaining input buffers right away */

    /* broadcast publish finish event to subscribers */
    auto stopPubEvent = make_shared<DavStopPubEvent>();
    stopPubEvent->setAddress(flushFrom); /* reuse, although streamIndex is not needed */
    m_pubsubTransmitor->broadcast(stopPubEvent);
    m_pubsubTransmitor->clear();

    /* reserve the procInfo, so set another event */
    DavMessager threadFinishMsg(DAV_INFO_END_PROCESS_THREAD,
                                m_logtag + "thread terminated state => stop");
    s_msgCollector.addMsg(threadFinishMsg);
    LOG(INFO) << m_logtag << threadFinishMsg;
    m_state = EDavState::eStop;
    return m_procInfo.m_msgCode;
}

////////////////////////////////////////////////////////////////////////////////
// State Transitions
int DavProc::start() noexcept {
    std::lock_guard<mutex> lock(m_runLock);
    if (!m_impl) {
        ERROR(DAV_ERROR_BASE_EMPTY_IMPL, m_logtag + "no impl instance when start");
        return DAV_ERROR_BASE_EMPTY_IMPL;
    }
    m_state = EDavState::eStart;
    m_bAlive = true;
    m_bOnFire = true;

    /* TODO: may use future - async */
    m_processThread.reset(new std::thread(&DavProc::runDavProcThread, this));
    if (!m_processThread) {
        m_bAlive = false;
        m_bOnFire = false;
        ERROR(DAV_ERROR_BASE_CREATE_PROC_THREAD, m_logtag + " fail creating impl process thread");
        return DAV_ERROR_BASE_CREATE_PROC_THREAD;
    }
    return 0;
}

/* stop may also be called inside runDavProcThread (after flush done) */
void DavProc::stop() noexcept {
    std::lock_guard<mutex> lock(m_runLock);
    LOG(INFO) << m_logtag << "set stop";
    m_bAlive = false;
}

void DavProc::pause() noexcept {
    std::lock_guard<mutex> lock(m_runLock);
    LOG(INFO) << m_logtag << "set pase";
    m_state = EDavState::ePause;
    m_bOnFire = false;
}

void DavProc::resume() noexcept {
    std::lock_guard<mutex> lock(m_runLock);
    LOG(INFO) << m_logtag << "set resume";
    m_state = EDavState::eStart;
    m_bOnFire = true;
    m_runCondVar.notify_one();
}

}  // namespace ff_dynamic
