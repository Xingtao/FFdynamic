#pragma once

#include <chrono>
#include "davProc.h"

namespace ff_dynamic {

//////////////////////////////////////////////////////////////////////////////////////////
class DavWave : public DavProc {
public:
    explicit DavWave (const DavWaveOption & options) noexcept : DavProc(options) {}
    virtual ~DavWave () = default;

public: /* DavWave connection and disconnection */
    static std::mutex s_linkMutex;
    static int connect(DavWave *from, DavWave *to, int fromStreamIndex = -1) {
        std::lock_guard<std::mutex> lock(s_linkMutex);
        DavProcFrom fromSender(from, from->getGroupId(), fromStreamIndex);
        link(from->getDataTransmitor(), to->getDataTransmitor(), fromSender);
        string detail = "from stream " + std::to_string(fromStreamIndex) + " of " + from->getLogTag() +
            " => to stream of " + to->getLogTag();
        DavMessager e(DAV_INFO_DYNAMIC_CONNECT, detail);
        s_msgCollector.addMsg(e);
        LOG(INFO) << e;
        return 0;
    }

    static int disconnect(DavWave *from, DavWave *to, int fromStreamIndex = -1) {
        std::lock_guard<std::mutex> lock(s_linkMutex);
        DavProcFrom sender(from, from->getGroupId(), fromStreamIndex);
        unlink(from->getDataTransmitor(), to->getDataTransmitor(), sender);
        string detail = "from stream " + std::to_string(fromStreamIndex) + " of " + from->getLogTag() +
            " <= disconnect => " +  to->getLogTag();
        DavMessager e(DAV_INFO_DYNAMIC_DISCONNECT, detail);
        s_msgCollector.addMsg(e);
        LOG(INFO) << e;
        return 0;
    }

    static int subscribe(DavWave *from, DavWave *to, int fromStreamIndex = -1) {
        std::lock_guard<std::mutex> lock(s_linkMutex);
        DavProcFrom fromSender(from, from->getGroupId(), fromStreamIndex);
        link(from->getPubsubTransmitor(), to->getPubsubTransmitor(), fromSender);
        string detail = to->getLogTag() + " subcribe event => from stream " +
            std::to_string(fromStreamIndex) + " of " + from->getLogTag();
        DavMessager e(DAV_INFO_DYNAMIC_SUBSCRIBE, detail);
        s_msgCollector.addMsg(e);
        LOG(INFO) << e;
        return 0;
    }

    static int unSubscribe(DavWave *from, DavWave *to, int fromStreamIndex = -1) {
        std::lock_guard<std::mutex> lock(s_linkMutex);
        DavProcFrom fromSender(from, from->getGroupId(), fromStreamIndex);
        unlink(from->getPubsubTransmitor(), to->getPubsubTransmitor(), fromSender);
        string detail = to->getLogTag() + " un-subcribe event => from stream " +
            std::to_string(fromStreamIndex) + " of " + from->getLogTag();
        DavMessager e(DAV_INFO_DYNAMIC_UNSUBSCRIBE, detail);
        s_msgCollector.addMsg(e);
        LOG(INFO) << e;
        return 0;
    }

private:
    template<typename T>
    static int link(T && from, T && to, const DavProcFrom & sender) {
        from->addRecipient(sender, to);
        to->addSender(sender, from);
        return 0;
    }

    template<typename T>
    static int unlink(T && from, T && to, const DavProcFrom & sender) {
        from->deleteRecipient(to);
        to->deleteSender(sender);
        return 0;
    }

public: /* trivial helpers */
    inline void setGroupId(size_t groupId) noexcept {updateGroupId(groupId);}
    inline size_t getGroupId() noexcept {return m_groupId;}
    inline const DavRegisterProperties & getDavRegisterProperties() const noexcept {
        return getDavRegisterProperties();
    }
    map<int, AVMediaType> getOutputMediaMap() {
        if (m_impl)
            return m_impl->getOutputMediaMap();
        return {};
    }
    /* check creation or run time process error */
    inline bool hasErr() const noexcept {return getProcInfo().hasErr();}
    inline const DavMsgError & getErr() const noexcept {return getProcInfo();}

public:
    template<typename T>
    int processDynamicEvent(const T & event) {
        if (!m_impl)
            return -1;
        /* each impl uses this lock and won't race with impl process */
        std::unique_lock<std::mutex> lock(m_runLock);
        return m_impl->processExternalEvent(event);
    }

private:
    DavWave(const DavWave &) = delete;
    DavWave operator=(const DavWave &) = delete;
};

} // namespace ff_dynamic
