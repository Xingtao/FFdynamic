#include <algorithm>
#include <limits>
#include "davStreamletBuilder.h"

////////////////////////////////////////////////////////////////////////////////
namespace ff_dynamic {

/* TODO: abstract and create a rule for connection, like a connection guide:
         demuxer -> audio/video Decode -> a/v filter -> a/v mix -> a/v filter -> a/v encoder -> muxer */

////////////////////////////////////////////////////////////////////////////////
shared_ptr<DavStreamlet> DavStreamletBuilder::
createStreamlet(const vector<DavWaveOption> & waveOptions,
                const DavStreamletTag & streamletTag,
                const DavStreamletOption & streamletOptions) {
    int bufLimitNum = std::numeric_limits<int>::max(); // default value
    streamletOptions.getInt(DavOptionBufLimitNum(), bufLimitNum); // may not set
    bufLimitNum = bufLimitNum <= 0 ? std::numeric_limits<int>::max() : bufLimitNum;
    auto streamlet = make_shared<DavStreamlet>(streamletTag);
    for (auto & o : waveOptions) {
        auto wave = make_shared<DavWave>(o);
        CHECK(wave != nullptr);
        if (wave->hasErr()) {
            m_buildInfo = wave->getErr();
            LOG(ERROR) << m_logtag << "fail to create wave: " << o.dump() << ", " << m_buildInfo;
            streamlet.reset();
            break;
        }
        streamlet->addOneWave(wave);
        wave->setMaxNumOfProcBuf(bufLimitNum);
    }
    return streamlet;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<DavStreamlet> DavDefaultInputStreamletBuilder::
build(const vector<DavWaveOption> & waveOptions,
      const DavStreamletTag & streamletTag,
      const DavStreamletOption & streamletOptions) {
    auto streamlet = createStreamlet(waveOptions, streamletTag, streamletOptions);
    if (!streamlet)
        return streamlet;

    /* get waves used in this streamlet */
    auto demuxers = streamlet->getWavesByCategory(DavWaveClassDemux());
    CHECK(demuxers.size() == 1) << m_logtag << "input stream must have exactly 1 demuxer";
    auto demux = demuxers[0];
    auto demuxOutMedias = demux->getOutputMediaMap();
    CHECK(demuxOutMedias.size() > 0) << m_logtag << "input should have valid a/v streams";

    auto videoDecoders = streamlet->getWavesByCategory(DavWaveClassVideoDecode());
    auto audioDecoders = streamlet->getWavesByCategory(DavWaveClassAudioDecode());
    auto videoFilters = streamlet->getWavesByCategory(DavWaveClassVideoFilter());
    auto audioFilters = streamlet->getWavesByCategory(DavWaveClassAudioFilter());
    CHECK(videoDecoders.size() > 0 || audioDecoders.size() > 0) << "there must be one audio or video stream";

    /* set in out entries and do streamlet internal connection (connect in order) */
    int videoConnectCount = 0;
    int videoTotalCount = 0;
    int audioConnectCount = 0;
    int audioTotalCount = 0;
    for (auto & outMedia : demuxOutMedias) {
        if (outMedia.second == AVMEDIA_TYPE_VIDEO) {
            if ((int)videoDecoders.size() > videoConnectCount) {
                DavWave::connect(demux.get(), videoDecoders[videoConnectCount].get(), outMedia.first);
                videoConnectCount++;
            }
            videoTotalCount++;
        } else if (outMedia.second == AVMEDIA_TYPE_AUDIO) {
            if ((int)audioDecoders.size() > audioConnectCount) {
                DavWave::connect(demux.get(), audioDecoders[audioConnectCount].get(), outMedia.first);
                audioConnectCount++;
            }
            audioTotalCount++;
       }
    }
    LOG(INFO) << m_logtag << "Input has " << videoTotalCount << " video streams, connected with "
              << videoConnectCount << " decoders; total " << audioTotalCount << " audio streams, connect with "
              << audioConnectCount << ". Create audio decoder num " << audioDecoders.size()
              << ", video decoder num " << videoDecoders.size();

    /* abandon decoders ? */
    //videoDecoders.resize(videoConnectCount);
    //audioDecoders.resize(audioConnectCount);

    int vfCount = 0;
    for (auto & vf : videoFilters) {
        if (videoConnectCount > vfCount) {
            DavWave::connect(videoDecoders[vfCount].get(), vf.get());
            streamlet->addOneOutVideoRawEntry(vf);
            vfCount++;
        }
    }
    int afCount = 0;
    for (auto & af : audioFilters) {
        if (audioConnectCount > afCount) {
            DavWave::connect(audioDecoders[afCount].get(), af.get());
            streamlet->addOneOutAudioRawEntry(af);
            afCount++;
        }
    }
    /* other decoders that not connected to a vf */
    for (int k=vfCount; videoConnectCount > k; k++)
        streamlet->addOneOutVideoRawEntry(videoDecoders[k]);
    for (int k=vfCount; audioConnectCount > k; k++)
        streamlet->addOneOutAudioRawEntry(audioDecoders[k]);
    return streamlet;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<DavStreamlet>
DavMixStreamletBuilder::build(const vector<DavWaveOption> & waveOptions,
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
    streamlet->addOneInVideoRawEntry(videoMix);
    if (audioMix)
        streamlet->addOneInAudioRawEntry(audioMix);

    auto videoFilters = streamlet->getWavesByCategory(DavWaveClassVideoFilter());
    auto audioFilters = streamlet->getWavesByCategory(DavWaveClassAudioFilter());
    CHECK(videoFilters.size() <= 1 && audioFilters.size() <= 1)
        << m_logtag << "mix streamlet cannot have audio/video filters more than one";

    if (videoFilters.size()) {
        DavWave::connect(videoMix.get(), videoFilters[0].get());
        streamlet->addOneOutVideoRawEntry(videoFilters[0]);
     } else {
        streamlet->addOneOutVideoRawEntry(videoMix);
    }

    if (audioMix) {
        if (audioFilters.size()) {
            DavWave::connect(audioMix.get(), audioFilters[0].get());
            streamlet->addOneOutAudioRawEntry(audioFilters[0]);
        } else {
            streamlet->addOneOutAudioRawEntry(audioMix);
        }
        // peer event subscribe: audio mix subscribe video mix
        DavWave::subscribe(videoMix.get(), audioMix.get());
    }
    return streamlet;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<DavStreamlet>
DavDefaultOutputStreamletBuilder::build(const vector<DavWaveOption> & waveOptions,
                                        const DavStreamletTag & streamletTag,
                                        const DavStreamletOption & streamletOptions) {
    auto streamlet = createStreamlet(waveOptions, streamletTag, streamletOptions);
    if (!streamlet)
        return streamlet;

    auto preVideoFilters = streamlet->getWavesByCategory(DavWaveClassVideoFilter());
    auto preAudioFilters = streamlet->getWavesByCategory(DavWaveClassAudioFilter());
    CHECK(preVideoFilters.size() <= 1 && preAudioFilters.size() <= 1)
        << m_logtag << "output streamlet cannot have audio/video filters more than 1";

    /* get waves used in this streamlet */
    auto videoEncodes = streamlet->getWavesByCategory(DavWaveClassVideoEncode());
    auto audioEncodes = streamlet->getWavesByCategory(DavWaveClassAudioEncode());
    CHECK(audioEncodes.size() <= 1 && videoEncodes.size() == 1)
        << m_logtag << "one output streamlet must have exactly (1 video) and (0 or 1 audio) encoder"
        << audioEncodes.size() << " | " << videoEncodes.size();
    auto & videoEncode = videoEncodes[0];
    auto muxers = streamlet->getWavesByCategory(DavWaveClassMux());
    CHECK(muxers.size() > 0) << m_logtag << "output streamlet at least has one muxer";

    if (preVideoFilters.size()) {
        streamlet->addOneInVideoRawEntry(preVideoFilters[0]);
        DavWave::connect(preVideoFilters[0].get(), videoEncode.get());
    } else {
        streamlet->addOneInVideoRawEntry(videoEncode);
    }

    if (audioEncodes.size() > 0 && preAudioFilters.size() > 0) {
        streamlet->addOneInAudioRawEntry(preAudioFilters[0]);
        DavWave::connect(preAudioFilters[0].get(), audioEncodes[0].get());
    } else if (audioEncodes.size() > 0) {
        streamlet->addOneInAudioRawEntry(audioEncodes[0]);
    }
    for (auto & m : muxers) {
        DavWave::connect(videoEncode.get(), m.get());
        if (audioEncodes.size() > 0)
            DavWave::connect(audioEncodes[0].get(), m.get());
    }
    return streamlet;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<DavStreamlet>
DavSingleWaveStreamletBuilder::build(const vector<DavWaveOption> & waveOptions,
                                     const DavStreamletTag & streamletTag,
                                     const DavStreamletOption & streamletOptions) {
    auto streamlet = createStreamlet(waveOptions, streamletTag, streamletOptions);
    if (!streamlet)
        return streamlet;

    auto waves = streamlet->getWaves();
    CHECK(waves.size() == 1) << "SingleWaveStreamlet should contain only one DavWave";
    auto wave = waves[0];
    DavDataType inDataType((DavDataTypeUndefined()));
    DavDataType outDataType((DavDataTypeUndefined()));
    streamletOptions.getCategory(DavOptionInputDataTypeCategory(), inDataType);
    streamletOptions.getCategory(DavOptionOutputDataTypeCategory(), outDataType);

    if (inDataType == DavDataTypeUndefined() || outDataType == DavDataTypeUndefined())
        return {};

    if (inDataType == DavDataInVideoBitstream()) {
        streamlet->addOneInVideoBitstreamEntry(wave);
    } else if (inDataType == DavDataInVideoRaw()) {
        streamlet->addOneInVideoRawEntry(wave);
    } else if (inDataType == DavDataInAudioBitstream()) {
        streamlet->addOneInAudioBitstreamEntry(wave);
    } else if (inDataType == DavDataInAudioRaw()) {
        streamlet->addOneInAudioRawEntry(wave);
    }

    if (outDataType == DavDataOutVideoBitstream()) {
        streamlet->addOneOutVideoBitstreamEntry(wave);
    } else if (outDataType == DavDataOutVideoRaw()) {
        streamlet->addOneOutVideoRawEntry(wave);
    } else if (outDataType == DavDataOutAudioBitstream()) {
        streamlet->addOneOutAudioBitstreamEntry(wave);
    } else if (outDataType == DavDataOutAudioRaw()) {
        streamlet->addOneOutAudioRawEntry(wave);
    }

    return streamlet;
}

} // namespace ff_dynamic
