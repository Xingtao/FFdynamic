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
    auto & audioFromOut = from.getAudioOutEntries();
    auto & videoFromOut = from.getVideoOutEntries();
    auto & audioToIn = to.getAudioInEntries();
    auto & videoToIn = to.getVideoInEntries();
    for (size_t k=0; k < audioToIn.size(); k++)
        if (k < audioFromOut.size())
            DavWave::connect(audioFromOut[k].get(), audioToIn[k].get());
    for (size_t k=0; k < videoToIn.size(); k++)
        if (k < videoFromOut.size())
            DavWave::connect(videoFromOut[k].get(), videoToIn[k].get());
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
