#include <algorithm>
#include <limits>
#include "davStreamletBuilder.h"

////////////////////////////////////////////////////////////////////////////////
namespace ff_dynamic {
////////////////////////////////////////////////////////////////////////////////
shared_ptr<DavStreamlet>
DavCvDnnDetectStreamletBuilder::build(const vector<DavWaveOption> & waveOptions,
                                      const DavStreamletTag & streamletTag,
                                      const DavStreamletOption & streamletOptions) {
    auto streamlet = createStreamlet(waveOptions, streamletTag, streamletOptions);
    if (!streamlet)
        return streamlet;

    /* get waves used in this streamlet */
    auto videoMixes = streamlet->getWavesByCategory(DavWaveClassVideoMix());
    auto audioMixes = streamlet->getWavesByCategory(DavWaveClassAudioMix());
    CHECK(videoMixes.size() == 1 && audioMixes.size() <= 1)
        << m_logtag << "mix streamlet should have exactly 1 video mix and 0 or 1 audio mix: "
        << audioMixes.size() << ", " << videoMixes.size();
    auto videoMix = videoMixes[0];
    shared_ptr<DavWave> audioMix;
    if (audioMixes.size() > 0)
        audioMix = audioMixes[0];
    streamlet->addOneVideoInEntry(videoMix);
    if (audioMix)
        streamlet->addOneAudioInEntry(audioMix);

    auto videoFilters = streamlet->getWavesByCategory(DavWaveClassVideoFilter());
    auto audioFilters = streamlet->getWavesByCategory(DavWaveClassAudioFilter());
    CHECK(videoFilters.size() <= 1 && audioFilters.size() <= 1)
        << m_logtag << "mix streamlet cannot have audio/video filters more than one";

    if (videoFilters.size()) {
        DavWave::connect(videoMix.get(), videoFilters[0].get());
        streamlet->addOneVideoOutEntry(videoFilters[0]);
     } else {
        streamlet->addOneVideoOutEntry(videoMix);
    }

    if (audioMix) {
        if (audioFilters.size()) {
            DavWave::connect(audioMix.get(), audioFilters[0].get());
            streamlet->addOneAudioOutEntry(audioFilters[0]);
        } else {
            streamlet->addOneAudioOutEntry(audioMix);
        }
        // peer event subscribe: audio mix subscribe video mix
        DavWave::subscribe(videoMix.get(), audioMix.get());
    }
    return streamlet;
}

} // namespace ff_dynamic
