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

static void streamletConnect(DavStreamlet & from, DavStreamlet & to) {
    auto & audioBsFromOut = from.getOutAudioBitstreamEntries();
    auto & videoBsFromOut = from.getOutVideoBitstreamEntries();
    auto & audioBsToIn = to.getInAudioBitstreamEntries();
    auto & videoBsToIn = to.getInVideoBitstreamEntries();

    auto & audioRawFromOut = from.getOutAudioRawEntries();
    auto & videoRawFromOut = from.getOutVideoRawEntries();
    auto & audioRawToIn = to.getInAudioRawEntries();
    auto & videoRawToIn = to.getInVideoRawEntries();

    for (size_t k=0; k < audioBsToIn.size(); k++)
        if (k < audioBsFromOut.size())
            DavWave::connect(audioBsFromOut[k].get(), audioBsToIn[k].get());
    for (size_t k=0; k < videoBsToIn.size(); k++)
        if (k < videoBsFromOut.size())
            DavWave::connect(videoBsFromOut[k].get(), videoBsToIn[k].get());
    for (size_t k=0; k < audioRawToIn.size(); k++)
        if (k < audioRawFromOut.size())
            DavWave::connect(audioRawFromOut[k].get(), audioRawToIn[k].get());
    for (size_t k=0; k < videoRawToIn.size(); k++)
        if (k < videoRawFromOut.size())
            DavWave::connect(videoRawFromOut[k].get(), videoRawToIn[k].get());
    return;
}

/* default connection will connect in order */
DavStreamlet & operator>>(DavStreamlet & from, DavStreamlet & to) {
    streamletConnect(from, to);
    return to;
}

shared_ptr<DavStreamlet> & operator>>(shared_ptr<DavStreamlet> & from, shared_ptr<DavStreamlet> & to) {
    streamletConnect(*from, *to);
    return to;
}

} // namespace ff_dynamic
