#include "davStreamlet.h"

namespace ff_dynamic  {

bool operator<(const DavStreamletTag & l, const DavStreamletTag & r) {
    if (l.m_streamletCategory < r.m_streamletCategory)
        return true;
    if (l.m_streamletCategory > r.m_streamletCategory)
        return false;
    if (l.m_streamletName.compare(r.m_streamletName) < 0)
        return true;
    if (l.m_streamletName.compare(r.m_streamletName) > 0)
        return false;
    return false;
}

bool operator==(const DavStreamletTag & l, const DavStreamletTag & r) {
    return (l.m_streamletCategory == r.m_streamletCategory && l.m_streamletName == r.m_streamletName);
}

std::ostream & operator<<(std::ostream & os, const DavStreamletTag & streamletTag) {
    os << streamletTag.m_streamletCategory.name() << "_" << streamletTag.m_streamletName;
    return os;
}

static void streamletConnect(DavStreamlet & from, DavStreamlet & to,
                             bool bAudio, bool bVideo, bool bRaw, bool bBitstream) {
    if (bAudio && bBitstream) {
        auto & audioBsFromOut = from.getOutAudioBitstreamEntries();
        auto & audioBsToIn = to.getInAudioBitstreamEntries();
        for (size_t k=0; k < audioBsToIn.size(); k++)
            if (k < audioBsFromOut.size())
                DavWave::connect(audioBsFromOut[k].get(), audioBsToIn[k].get());
    }
    if (bAudio && bRaw) {
        auto & audioRawFromOut = from.getOutAudioRawEntries();
        auto & audioRawToIn = to.getInAudioRawEntries();
        for (size_t k=0; k < audioRawToIn.size(); k++)
            if (k < audioRawFromOut.size())
                DavWave::connect(audioRawFromOut[k].get(), audioRawToIn[k].get());
    }
    if (bVideo && bBitstream) {
        auto & videoBsFromOut = from.getOutVideoBitstreamEntries();
        auto & videoBsToIn = to.getInVideoBitstreamEntries();
        for (size_t k=0; k < videoBsToIn.size(); k++)
            if (k < videoBsFromOut.size())
                DavWave::connect(videoBsFromOut[k].get(), videoBsToIn[k].get());
    }
    if (bVideo && bRaw) {
        auto & videoRawToIn = to.getInVideoRawEntries();
        auto & videoRawFromOut = from.getOutVideoRawEntries();
        for (size_t k=0; k < videoRawToIn.size(); k++)
            if (k < videoRawFromOut.size())
                DavWave::connect(videoRawFromOut[k].get(), videoRawToIn[k].get());
    }
    return;
}

/* connect all kinds of data types audio/video bitstream/raw */
DavStreamlet & operator>>(DavStreamlet & from, DavStreamlet & to) {
    streamletConnect(from, to, true, true, true, true);
    return to;
}
shared_ptr<DavStreamlet> & operator>>(shared_ptr<DavStreamlet> & from, shared_ptr<DavStreamlet> & to) {
    streamletConnect(*from, *to, true, true, true, true);
    return to;
}

/* connection only video raw or bitstream */
DavStreamlet & operator>=(DavStreamlet & from, DavStreamlet & to) {
    streamletConnect(from, to, false, true, true, true);
    return to;
}
shared_ptr<DavStreamlet> & operator>=(shared_ptr<DavStreamlet> & from, shared_ptr<DavStreamlet> & to) {
    streamletConnect(*from, *to, false, true, true, true);
    return to;
}

/* connection only audio raw or bitstream */
DavStreamlet & operator*=(DavStreamlet & from, DavStreamlet & to) {
    streamletConnect(from, to, true, false, true, true);
    return to;
}
shared_ptr<DavStreamlet> & operator*=(shared_ptr<DavStreamlet> & from, shared_ptr<DavStreamlet> & to) {
    streamletConnect(*from, *to, true, false, true, true);
    return to;
}

/* connect only video bitstream data type (such as: video codec copy) */
DavStreamlet & operator>(DavStreamlet & from, DavStreamlet & to) {
    streamletConnect(from, to, false, true, false, true);
    return to;
}
shared_ptr<DavStreamlet> & operator>(shared_ptr<DavStreamlet> & from, shared_ptr<DavStreamlet> & to) {
    streamletConnect(*from, *to, false, true, false, true);
    return to;
}

/* connect only audio bitstream data type (such as: video codec copy) */
DavStreamlet & operator*(DavStreamlet & from, DavStreamlet & to) {
    streamletConnect(from, to, true, false, false, true);
    return to;
}
shared_ptr<DavStreamlet> & operator*(shared_ptr<DavStreamlet> & from, shared_ptr<DavStreamlet> & to) {
    streamletConnect(*from, *to, true, false, false, true);
    return to;
}

} // namespace ff_dynamic
