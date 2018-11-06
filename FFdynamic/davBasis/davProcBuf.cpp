#include <iostream>
#include "davProc.h"
#include "davProcBuf.h"

namespace ff_dynamic {
using ::std::make_shared;

/* for use DavProcFrom as key of map */
bool operator<(const DavProcFrom & l, const DavProcFrom & r) {
    const uintptr_t lfrom = reinterpret_cast<uintptr_t>(l.m_from);
    const uintptr_t rfrom = reinterpret_cast<uintptr_t>(r.m_from);
    if (lfrom < rfrom) return true;
    if (lfrom > rfrom) return false;
    // Otherwise from the same DavProc

    /* take flush buf as equal to any exising streamIdx */
    if (l.m_fromStreamIndex == DavProcFrom::s_flushIndex ||
        r.m_fromStreamIndex == DavProcFrom::s_flushIndex)
        return false;

    if (l.m_fromStreamIndex < r.m_fromStreamIndex) return true;
    if (l.m_fromStreamIndex > r.m_fromStreamIndex) return false;
    // both equal
    return false;
}

/* won't check group id, this field may not be set. also, 'from' and 'streamIdx' are enough */
bool operator==(const DavProcFrom & l, const DavProcFrom & r) {
    return (l.m_from == r.m_from &&
            (l.m_fromStreamIndex == r.m_fromStreamIndex ||
             l.m_fromStreamIndex == DavProcFrom::s_flushIndex ||
             r.m_fromStreamIndex == DavProcFrom::s_flushIndex));
}

std::ostream & operator<<(std::ostream & os, const DavProcFrom & f) {
    os << "[group " << f.m_groupId << " " << f.m_descFrom << " stream " << f.m_fromStreamIndex << "]";
    return os;
}

////////////////////////////////////////////////
std::ostream & operator<<(std::ostream & os, const DavProcBuf & buf) {
    os << buf.m_buffrom << ", pkt " << buf.m_pkt << ", frame " << buf.m_frame;
    return os;
}

DavProcFrom::DavProcFrom(DavProc *from, const int fromIndex) noexcept
    : m_from(from), m_fromStreamIndex(fromIndex), m_descFrom(from->getLogTag()) {
}

DavProcFrom::DavProcFrom(DavProc *from, size_t groupId, const int fromIndex) noexcept {
    setFromStreamIndex(fromIndex);
    setGroupFrom(from, groupId);
}

void DavProcFrom::setGroupFrom(DavProc *thisProc, size_t groupId) noexcept {
    m_groupId = groupId;
    m_from = thisProc;
    m_descFrom = thisProc->getLogTag();
}

/////////////////////////////////////////
// manul reference with DavProcBufLimiter
void DavProcBuf::unlimit() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_limiter) {
        m_limiter->notify();
        m_limiter.reset();
    }
}

DavProcBuf::~DavProcBuf() {
    if (m_limiter)
        m_limiter->notify();
    if (m_pkt)
        av_packet_free(&m_pkt);
    if (m_frame)
        av_frame_free(&m_frame);
}

} // namespace ff_dynamic
