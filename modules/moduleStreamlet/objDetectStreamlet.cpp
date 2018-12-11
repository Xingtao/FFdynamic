#include <algorithm>
#include <limits>
#include "objDetectStreamlet.h"

////////////////////////////////////////////////////////////////////////////////
namespace ff_dynamic {
////////////////////////////////////////////////////////////////////////////////
shared_ptr<DavStreamlet>
ObjDetectStreamletBuilder::build(const vector<DavWaveOption> & waveOptions,
                                 const DavStreamletTag & streamletTag,
                                 const DavStreamletOption & streamletOptions) {
    auto streamlet = createStreamlet(waveOptions, streamletTag, streamletOptions);
    if (!streamlet)
        return streamlet;

    /* get waves used in this streamlet */
    auto dataRelaies = streamlet->getWavesByCategory(DavWaveClassDataRelay());
    auto postDraws = streamlet->getWavesByCategory(DavWaveClassCvPostDraw());
    CHECK(dataRelaies.size() == 1 && postDraws.size() == 1)
        << m_logtag << "cv dnn should only have one DataRelay and one PostDraw"
        << dataRelaies.size() << ", " << postDraws.size();
    auto dataRelay = dataRelaies[0];
    auto postDraw = postDraws[0];

    auto objDetectors = streamlet->getWavesByCategory(DavWaveClassObjDetect());
    CHECK(objDetectors.size() > 0)
        << m_logtag << "at lease one detector enabled at the beginning";

    /* connections */
    streamlet->addOneInVideoRawEntry(dataRelay);
    for (auto & d : objDetectors) {
        DavWave::connect(dataRelay.get(), d.get());
        // peer event subscribe: postDraw subscribe detector's result event
        DavWave::subscribe(d.get(), postDraw.get());

    }
    streamlet->addOneOutVideoRawEntry(postDraw);
    DavWave::connect(dataRelay.get(), postDraw.get());
    return streamlet;
}

int ObjDetectStreamletBuilder::
addDetector(shared_ptr<DavStreamlet> & streamlet, const DavWaveOption & detectorOption) {
    auto newDetector = make_shared<DavWave>(detectorOption);
    CHECK(newDetector != nullptr);
    if (newDetector->hasErr()) {
        m_buildInfo = newDetector->getErr();
        LOG(ERROR) << m_logtag << "fail to create wave: " << detectorOption.dump() << ", " << m_buildInfo;
        return AVERROR(EINVAL);
    }
    auto dataRelaies = streamlet->getWavesByCategory(DavWaveClassDataRelay());
    auto postDraws = streamlet->getWavesByCategory(DavWaveClassCvPostDraw());
    CHECK(dataRelaies.size() == 1 && postDraws.size() == 1)
        << m_logtag << "cv dnn should only have one DataRelay and one PostDraw"
        << dataRelaies.size() << ", " << postDraws.size();
    auto dataRelay = dataRelaies[0];
    auto postDraw = postDraws[0];
    DavWave::connect(dataRelay.get(), newDetector.get());

    // peer event subscribe: postDraw subscribe detector's result event
    DavWave::subscribe(newDetector.get(), postDraw.get());
    streamlet->addOneWave(newDetector);
    newDetector->start();
    return 0;
}

int ObjDetectStreamletBuilder::
deleteDetector(shared_ptr<DavStreamlet> & streamlet, const string & detectorName) {
    return 0;
}

} // namespace ff_dynamic
