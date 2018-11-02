#pragma once
// system
#include <memory>
#include <vector>
#include <utility>
// project
#include "ffmpegHeaders.h"
#include "davProcBuf.h"
#include "davTransmitor.h"
#include "davPeerEvent.h"

namespace ff_dynamic {
using ::std::pair;
using ::std::vector;
using ::std::shared_ptr;

class DavProc;

struct DavProcCtx {
    DavProcCtx() = default;
    explicit DavProcCtx (const vector<DavProcFrom> & froms) noexcept : m_froms(froms) {}
    virtual ~DavProcCtx() {
        if (m_inRefPkt) /* in case process failed without release */
            av_packet_free(&m_inRefPkt);
        if (m_inRefFrame)
            av_frame_free(&m_inRefFrame);
    }

    const vector<DavProcFrom> & m_froms; /* all input peers at the moment */
    shared_ptr<DavProcBuf> m_inBuf;
    /* ref pkt/frame is used for one buffer output to several peers that have different timebase,
       so use ref frame to refer to converted timestamps */
    AVPacket *m_inRefPkt = nullptr;
    AVFrame *m_inRefFrame = nullptr;
    bool m_bInputFlush = false;

    /* may output several frames in one process. also when do flush */
    vector<shared_ptr<DavProcBuf>> m_outBufs;
    vector<shared_ptr<DavPeerEvent>> m_pubEvents;
    int m_outputTimes = 1; /* output frame many times, could be 0 */
    int m_curStreamIndex = 0;
    DavExpect<DavProcFrom> m_expect; /* next process buffer expectation */
    DavMsgError m_implErr;
};

}//namespace
