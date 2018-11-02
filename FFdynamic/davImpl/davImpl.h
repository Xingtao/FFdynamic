#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <memory>
#include <utility>
#include <algorithm>
#include <typeinfo>
#include <typeindex>

#include <glog/logging.h>

#include "ffmpegHeaders.h"
#include "davUtil.h"
#include "davProcCtx.h"
#include "davMessager.h"
#include "davDict.h"
#include "davImplUtil.h"
#include "davImplFactory.h"
#include "davPeerEvent.h"
#include "davImplEventProcess.h"
#include "davDynamicEvent.h"

namespace ff_dynamic {
using ::std::string;
using ::std::vector;
using ::std::deque;
using ::std::pair;
using ::std::make_shared;
using ::std::shared_ptr;

//////////////////////////////////////////////////////////////////////////////////////////
// DavImpl Class: the basis for real audio/video process.
class DavImpl {
public:
    explicit DavImpl(const DavWaveOption & options) : m_options(options) {
        m_implType = m_options.get(DavOptionImplType(), "unknown");
        m_logtag = m_options.get(DavOptionLogtag(), "[DavImpl] ");
    }
    virtual ~DavImpl() {
        clearInputOutputInfo();
    }

private:
    /* when implementation first called, it is ok do nothing.
       most implementations will dynamically be initialized (onDynamicallyInitializeViaTravelStatic)*/
    virtual int onConstruct() = 0;
    /* resources release part */
    virtual int onDestruct() = 0;
    /* implementation's real data process function */
    virtual int onProcess(DavProcCtx & ctx) = 0;

    /* take input peers travel static info to do dynamically initialization  */
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) = 0;
    /* process input peer's dynamically change. normally new input join or pkt's with side data changing */
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) = 0;

private: /* the following ones could be optional overrided depends on implementations */
    /* pre-process set inputDavTravelStatic info and input data's timestamp */
    virtual int onPreProcess(DavProcCtx & ctx);
    /* post-porcess remove inputTravelstatic and timestamp info after this peer is flushed */
    virtual int onPostProcess(DavProcCtx & ctx);
    /* set option dict with different non-monotonic / invalid dts strategy, it is per-implementation based */
    virtual int onPktNonMonotonicDts(DavProcCtx & ctx, DavImplTimestamp & tsm, AVPacket *pkt);
    virtual int onPktNoValidDts(DavProcCtx & ctx, DavImplTimestamp & tsm, AVPacket *pkt);
    /* special process if onProcess call cannot handle data flush */
    virtual int onFlush(DavProcCtx & ctx) {return 0;}

public: /* public apis */
    /* process interface with DavWave */
    int process(DavProcCtx & ctx);

    /* external dynamic events process */
    template<typename T>
    int processExternalEvent(const T & event) {
        return m_implEvent.processExternal(event);
    }

    /* internal dynamic events process */
    int processPeerEvent(const DavPeerEvent & event) {
        return m_implEvent.processPeer(event);
    }

    /* use AVDictionary for implementation's statistics (avoid introduce json dependency) */
    virtual int statistics(AVDictionary & stat) {return 0;};
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept = 0;

    /* trival helpers */
    inline const DavMsgError & getImplErr() const {return m_implErr;}
    inline bool isDynamicallyInitialized() const {return m_bDynamicallyInitialized;}
    inline const map<int, enum AVMediaType> & getOutputMediaMap() const {return m_outputMediaMap;}

protected: /* will call this in implementation's constructor */
    virtual void implDefaultInstantiate() {
        if (onConstruct() < 0) {
            onDestruct();
            throw m_implErr;
        }
    }
    int clearInputOutputInfo() {
        m_bDynamicallyInitialized = false;
        m_outputMediaMap.clear();
        m_outputTravelStatic.clear();
        m_inputTravelStatic.clear();
        m_timestampMgr.clear();
        while (m_preInitCacheInBufs.size())
            m_preInitCacheInBufs.pop_front();
        return 0;
    }

protected:
    string m_implType;
    string m_logtag;
    /* ok not all necessary options are given, could also refer to incoming DavTravelStatic info */
    DavWaveOption m_options;
    DavMsgError m_implErr;
    /* external / subscribed events process */
    DavImplEventProcess m_implEvent;
    /* dynamic initialization after receive enough data from peers */
    std::atomic<bool> m_bDynamicallyInitialized = ATOMIC_VAR_INIT(false);
    /* some impls' may cache bufs before fully initialized */
    deque<shared_ptr<DavProcBuf>> m_preInitCacheInBufs;
    map<DavProcFrom, shared_ptr<DavTravelStatic>> m_inputTravelStatic;
    map<DavProcFrom, DavImplTimestamp> m_timestampMgr;
    map<int, shared_ptr<DavTravelStatic>> m_outputTravelStatic;
    /* for some impls, their travel static outputs is the same as inputs */
    std::atomic<bool> m_bDataRelay = ATOMIC_VAR_INIT(false);
    /* impl output one or more audio/video streams to next peers */
    map<int, enum AVMediaType> m_outputMediaMap; /* TODO: may kill this one later*/

protected: /* msg collector */
    static DavMsgCollector & s_msgCollector;
    /* Those macros is for reserving the correct function name and line number.
       Be aware only called within 'DavImpl' and its derived classes */
    #define ERRORIT(code, detail) {m_implErr.setInfo(code, detail); \
            s_msgCollector.addMsg(m_implErr); LOG(ERROR) << m_logtag << m_implErr;}
    #define INFOIT(code, detail) {m_implErr.setInfo(code, detail); \
            s_msgCollector.addMsg(m_implErr);LOG(INFO) << m_logtag << m_implErr;}

protected: /* trival helpers */
    void recordUnusedOpts() {
        string unusedOpts = m_options.dumpAVDict();
        if (unusedOpts.size() > 0)
            INFOIT(DAV_WARNING_IMPL_UNUSED_OPTS, unusedOpts);
    }
};

static constexpr int IMPL_SINGLE_OUTPUT_STREAM_INDEX = -1;

} // namespace ff_dynamic
