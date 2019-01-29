#pragma once
// system
#include <sys/time.h>
#include <thread>
#include <memory>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <typeinfo>
#include <typeindex>
#include <chrono>
#include <condition_variable>
// third
#include <glog/logging.h>
// project
#include "davUtil.h"
#include "davMessager.h"
#include "davProcBuf.h"
#include "davProcCtx.h"
#include "davTransmitor.h"
#include "davImpl.h"
#include "davImplFactory.h"
#include "davPeerEvent.h"

//////////////////////////////////////////////////////////////////////////////////////////
namespace ff_dynamic {

using DavProcFilter = std::function<shared_ptr<DavProcBuf>(shared_ptr<DavProcBuf> & dataBuf)>;

/* DavProc: the basic structure for audio/video process. */
class DavProc {
public:
    explicit DavProc(const DavWaveOption & options) noexcept;
    virtual ~DavProc();

public: /* state transition */
    enum class EDavState {eCreate, eStart, ePause, eStop, eRelease};
    int start() noexcept ;
    void stop() noexcept ;/* stop may also be called inside runDavProcThread (after flush done) */
    void pause() noexcept ;
    void resume() noexcept ;
    void reset() noexcept {return;} /* TODO */
    inline bool isStarted() noexcept {return m_state == EDavState::eStart;}
    inline bool isPaused() noexcept {return m_state == EDavState::ePause;}
    inline bool isStopped() noexcept {return m_state == EDavState::eStop;}

public: /* properties set & get*/
    // by default, max proc buffer is -1, unlmited */
    inline void setMaxNumOfProcBuf(int limitNum) noexcept {m_outbufLimiter->setMaxNumOfProcBuf(limitNum);}
    inline shared_ptr<DavTransmitor<DavProcBuf, DavProcFrom>> getDataTransmitor() {return m_dataTransmitor;}
    inline shared_ptr<DavTransmitor<DavPeerEvent, DavProcFrom>> getPubsubTransmitor() {return m_pubsubTransmitor;}
    inline const string & getClassName() const noexcept {return m_className;}
    inline const string & getLogTag() const noexcept {return m_logtag;}
    inline const DavWaveClassCategory & getDavWaveCategory() const noexcept {return m_waveCategory;}
    inline const DavRegisterProperties & getDavRegisterProperties() const noexcept {
        return m_impl->getRegisterProperties();
    }
    /* for pre/post data filter process */
    inline void setPrefilters(const vector<DavProcFilter> & prefilters) {m_prefilters = prefilters;}
    inline void setPostfilters(const vector<DavProcFilter> & postfilters) {m_postfilters = postfilters;}

protected: /* process */
    int runDavProcThread();
    /* 1. get expected input buffer from peer 2. process subscribed events */
    virtual int preProcess(shared_ptr<DavProcBuf> & inBuf);
    /* do implementation process with one input (or none) */
    virtual int process(DavProcCtx & ctx);
    /* output processed data to proper peers, also limit output buffer number in this call */
    virtual int postProcess (DavProcCtx & ctx);

protected: /* real implementation with dynamic reopen */
    unique_ptr<DavImpl> m_impl;
    int reopenImpl(DavWaveOption & options) {
        m_impl.reset();
        m_impl = DavImplFactory::create(options, m_procInfo);
        return m_procInfo.m_msgCode;
    }

protected:
    std::mutex m_runLock;
    std::condition_variable m_runCondVar;
    size_t m_groupId = 0;
    void updateGroupId(size_t groupId) noexcept {
        m_groupId = groupId;
        DavProcFrom selfAddreess(this, groupId);
        m_dataTransmitor->setSelfAddress(selfAddreess);
        m_pubsubTransmitor->setSelfAddress(selfAddreess);
    }

protected:
    inline bool isImplOnFire() noexcept {return m_impl != nullptr;}
    inline const DavMessager & getProcInfo() const noexcept {return m_procInfo;}
    inline double processTimeConsumeThisFrame() noexcept {
        return ( (m_processEnd.tv_sec - m_processStart.tv_sec) * 1000.0   +
                 (m_processEnd.tv_usec - m_processStart.tv_usec) / 1000.0 );
    }

protected:
    const int m_idx;
    DavWaveClassCategory m_waveCategory = DavWaveClassNotACategory();
    string m_className;
    string m_logtag;

private:
    DavProc(DavProc const &) = delete;
    DavProc operator= (const DavProc &) = delete;
    unique_ptr<std::thread> m_processThread;
    EDavState m_state = EDavState::eCreate;
    bool m_bImplProcessEof = false;
    std::atomic<bool> m_bAlive = ATOMIC_VAR_INIT(true);
    std::atomic<bool> m_bOnFire = ATOMIC_VAR_INIT(false);

private:
    vector<DavProcFilter> m_prefilters;
    vector<DavProcFilter> m_postfilters;
    /* input/output buffer flow: receive & deliver  */
    shared_ptr<DavTransmitor<DavProcBuf, DavProcFrom>> m_dataTransmitor;
    shared_ptr<DavTransmitor<DavPeerEvent, DavProcFrom>> m_pubsubTransmitor;
    DavExpect<DavProcFrom> m_expectInput;
    /* extending its scope, for limitor will travel with ProcBuf */
    shared_ptr<DavProcBufLimiter> m_outbufLimiter;
    DavMsgError m_procInfo;

private: /* trvial */
    struct timeval m_processStart;
    struct timeval m_processEnd;
    inline void getProcessStartTime() noexcept {gettimeofday(&m_processStart, nullptr);}
    inline void getProcessEndTime() noexcept {gettimeofday(&m_processEnd, nullptr);}

//// static helpers
private: /* generate unique increase class index number */
    static std::mutex s_idxMutex;
    static int s_autoIdx;
    static int makeNewDavProcIndex();

protected:
    static DavMsgCollector & s_msgCollector;
    /* Those macros is for reserving the correct function name and line number.
       Be aware only called within 'DavProc' and its derived classes */
    #define ERROR(code, detail) {m_procInfo.setInfo(code, detail);\
            s_msgCollector.addMsg(m_procInfo);LOG(ERROR) << m_logtag << m_procInfo;}
    #define INFO(code, detail) {m_procInfo.setInfo(code, detail); \
            s_msgCollector.addMsg(m_procInfo);LOG(INFO) << m_logtag << m_procInfo;}
};

} // namespace
