#pragma once

// system
#include <cstdint>
#include <algorithm>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <limits>
// third
#include <glog/logging.h>
#include "ffmpegHeaders.h"
#include "davImplTravel.h"

namespace ff_dynamic {
using std::shared_ptr;

class DavProc;
class DavProcBufLimiter;

////////// DavProcBuf
struct DavProcFrom {
    DavProcFrom() = default;
    DavProcFrom(DavProc*from, const int fromIndex = -1) noexcept;
    DavProcFrom(DavProc*from, size_t groupId, const int fromIndex = -1) noexcept;
    inline void setFromStreamIndex(const int idx = -1) noexcept {m_fromStreamIndex = idx;}
    void setGroupFrom(DavProc *thisProc, size_t groupId) noexcept;
    virtual ~DavProcFrom() = default;
    DavProc* m_from = nullptr;
    size_t m_groupId = 0;
    int m_fromStreamIndex = -1;
    string m_descFrom;
    static const int s_flushIndex = 0xFEDCBA98;
};

extern bool operator<(const DavProcFrom & l, const DavProcFrom & r);
extern bool operator==(const DavProcFrom & l, const DavProcFrom & r);
extern std::ostream & operator<<(std::ostream & os, const DavProcFrom & f);

struct DavProcBuf {
    DavProcBuf() = default;
    virtual ~DavProcBuf();
    inline bool isEmptyData() const noexcept {return !m_pkt && !m_frame;}
    /* this is awkward, it should have been captured by class dependency.
       may asbstract DavProcBuf in future to avoid this 'getAddress' */
    inline DavProcFrom & getAddress() noexcept {return m_buffrom;}
    inline const DavProcFrom & getAddress() const noexcept {return m_buffrom;}
    inline void setAddress(const DavProcFrom & buffrom) noexcept {m_buffrom = buffrom;}
    inline void setBufLimitor(shared_ptr<DavProcBufLimiter> limiter) {m_limiter = limiter;}
    friend std::ostream & operator<<(std::ostream & os, const DavProcBuf & buf);

    /* Data */
    inline AVFrame *mkAVFrame () noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_frame ? m_frame : (m_frame = av_frame_alloc());
    }
    inline AVPacket *mkAVPacket () noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_pkt ? m_pkt : (m_pkt = av_packet_alloc());
    }
    /* Take ownership of provided data pointer, and release current hold one */
    inline AVFrame *mkAVFrame (AVFrame *frame) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_frame)
            av_frame_free(&m_frame);
        return m_frame = frame;
    }
    inline AVPacket *mkAVPacket (AVPacket *packet) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pkt)
            av_packet_free(&m_pkt);
        return m_pkt = packet;
    }
    inline AVFrame *releaseAVFrameOwner () noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto frame = m_frame;
        m_frame = nullptr;
        return frame;
    }
    inline AVPacket *releaseAVPacketOwner () noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto pkt = m_pkt;
        m_pkt = nullptr;
        return pkt;
    }
    inline const AVFrame *getAVFrame () const noexcept {return m_frame;}
    inline const AVPacket *getAVPacket () const noexcept {return m_pkt;}

    /* please called only when you know exactly what this functin does */
    void unlimit();

public:
    /* travel info passing to connected peers */
    shared_ptr<DavTravelStatic> m_travelStatic;
    shared_ptr<DavTravelDynamic> m_travelDynamic;

private:
    std::mutex m_mutex;
    size_t m_groupId = 0;
    DavProcFrom m_buffrom;
    AVPacket *m_pkt = nullptr;
    AVFrame *m_frame = nullptr;
    shared_ptr<DavProcBufLimiter> m_limiter;
};

extern std::ostream & operator<<(std::ostream &, const DavProcBuf &);

/* DavProcBufLimiter: when do output, check whether process limit needed. */
struct DavProcBufLimiter : std::enable_shared_from_this<DavProcBufLimiter> {
    DavProcBufLimiter() = default;
    ~DavProcBufLimiter() = default;
    explicit DavProcBufLimiter (const int maxNumOfProcBuf) : m_maxNum(maxNumOfProcBuf) {}
    /* called when do output of DavProc */
    void limit(shared_ptr<DavProcBuf> & procBuf) {
        std::unique_lock<std::mutex> guard(m_mutex);
        procBuf->setBufLimitor(this->shared_from_this());
        m_condVar.wait(guard, [this]() {return this->finishLimit();});
        m_curNum++;
    }

    void limit(vector<shared_ptr<DavProcBuf>> & procBufs) {
        for (auto procBuf : procBufs) {
            std::unique_lock<std::mutex> guard(m_mutex);
            procBuf->setBufLimitor(this->shared_from_this());
            m_condVar.wait(guard, [this]() {return this->finishLimit();});
            m_curNum++;
        }
    }

    /* be called by ProcBuf release callback */
    inline void notify() {
        std::unique_lock<std::mutex> guard(m_mutex);
        m_curNum--;
        m_condVar.notify_one();
    }
    inline void setMaxNumOfProcBuf(const int maxNum) {
        std::unique_lock<std::mutex> guard(m_mutex);
        m_maxNum = maxNum;
    }
    inline int getCurNumOfProcBuf() {
        std::unique_lock<std::mutex> guard(m_mutex);
        return m_curNum;
    }

private:
    inline bool finishLimit() {return m_maxNum >= m_curNum;}
    int m_maxNum = std::numeric_limits<int>::max();
    int m_curNum = 0;
    std::mutex m_mutex;
    std::condition_variable m_condVar;
};

} // namespace ff_dynamic
