#pragma once

#include <glog/logging.h>

#include "davDict.h"
#include "davStreamlet.h"
#include "davDynamicEvent.h"
#include "davWaveSetting.pb.h"
#include "davStreamletSetting.pb.h"

namespace ff_dynamic {
using namespace DavWaveSetting;
using namespace DavStreamletSetting;

class PbWaveSettingToDavOption {
public:
    static int toDemuxOption(const DemuxSetting & ds, DavWaveOption & o, const string & inputUrl = "") {
        o.setCategory(DavOptionClassCategory(), DavWaveClassDemux());
        o.set(DavOptionImplType(), ds.demux_type().empty() ? "auto" : ds.demux_type());
        o.set(DavOptionInputFpsEmulate(), ds.input_fps_emulate() ? "true" : "false");
        o.set(DavOptionReconnectRetries(), std::to_string(ds.reconnect_times()));
        o.set(DavOptionRWTimeout(), std::to_string(ds.read_timeout()));
        if (!inputUrl.empty())
            o.set(DavOptionInputUrl(), inputUrl);
        for (const auto & m : ds.avdict_demux_option())
            o.set(m.first, m.second, 0); // can overwrite
        return 0;
    }

    static int toVideoFilterOption(const VideoFilterSetting & vfs, DavWaveOption & o) {
        o.setCategory(DavOptionClassCategory(), DavWaveClassVideoFilter());
        o.set(DavOptionImplType(), vfs.filter_type().empty() ? "auto" : vfs.filter_type());
        o.set(DavOptionFilterDesc(), vfs.filter_arg());
        return 0;
    }

    static int toAudioFilterOption(const AudioFilterSetting & afs, DavWaveOption & o) {
        o.setCategory(DavOptionClassCategory(), DavWaveClassAudioFilter());
        o.set(DavOptionImplType(), afs.filter_type().empty() ? "auto" : afs.filter_type());
        o.set(DavOptionFilterDesc(), afs.filter_arg());
        return 0;
    }

    static int toVideoDecodeOption(const VideoDecodeSetting & vds, DavWaveOption & o) {
        o.setCategory(DavOptionClassCategory(), DavWaveClassVideoDecode());
        o.set(DavOptionImplType(), vds.decode_type().empty() ? "auto" : vds.decode_type());
        for (const auto & m : vds.avdict_decode_option())
            o.set(m.first, m.second, 0);
        return 0;
    }

    static int toAudioDecodeOption(const AudioDecodeSetting & ads, DavWaveOption & o) {
        o.setCategory(DavOptionClassCategory(), DavWaveClassAudioDecode());
        o.set(DavOptionImplType(), ads.decode_type().empty() ? "auto" : ads.decode_type());
        for (const auto & m : ads.avdict_decode_option())
            o.set(m.first, m.second, 0);
        return 0;
    }

    static int toVideoMixOption(const VideoMixSetting & vms, DavWaveOption & o) {
        o.setCategory(DavOptionClassCategory(), DavWaveClassVideoMix());
        o.setBool(DavOptionVideoMixRegeneratePts(), vms.b_regenerate_pts());
        o.setBool(DavOptionVideoMixQuitIfNoInputs(), vms.b_quit_if_no_input());
        o.setBool(DavOptionVideoMixStartAfterAllJoin(), vms.b_start_after_all_join());
        o.set(DavOptionImplType(), "auto");
        o.setVideoSize(vms.width(), vms.height());
        o.setAVRational("framerate", {vms.fps_num(), vms.fps_den()});
        o.setInt("margin", vms.margin());
        o.setInt("border_width", vms.border_width());
        o.setInt("border_color", vms.border_color());
        o.setInt("fillet_radius", vms.fillet_radius());
        o.set("backgroud_image_path", vms.backgroud_image_path());
        if (vms.has_layout_info()) {
            string layoutStr = DavWaveSetting::EVideoMixLayout_Name(vms.layout_info().layout());
            o.set(DavOptionVideoMixLayout(), layoutStr);
            // TODO: for specificLayout, need check initial coordinates;
            //repeated VideoCellCoordinate cells = 2;
        }
        return 0;
    }

    static int toAudioMixOption(const AudioMixSetting & ams, DavWaveOption & o) {
        o.setCategory(DavOptionClassCategory(), DavWaveClassAudioMix());
        o.set(DavOptionImplType(), "auto");
        o.setInt("frame_size", ams.frame_size(), 0);
        o.setBool("b_mute_at_start", ams.b_mute_at_start());
        return 0;
    }

    static int toVideoEncodeOption(const VideoEncodeSetting & ves, DavWaveOption & o) {
        o.setCategory(DavOptionClassCategory(), DavWaveClassVideoEncode());
        o.set(DavOptionImplType(), ves.encode_type().empty() ? "auto" : ves.encode_type());
        o.set(DavOptionCodecName(), ves.codec_name());
        o.setAVRational("framerate", {ves.fps_num(), ves.fps_den()});
        for (auto & d : ves.avdict_encode_option())
            o.set(d.first, d.second, 0);
        return 0;
    }

    static int toAudioEncodeOption(const AudioEncodeSetting & aes, DavWaveOption & o) {
        o.setCategory(DavOptionClassCategory(), DavWaveClassAudioEncode());
        o.set(DavOptionImplType(), aes.encode_type().empty() ? "auto" : aes.encode_type());
        o.set(DavOptionCodecName(), aes.codec_name());
        for (auto & d : aes.avdict_encode_option())
            o.set(d.first, d.second, 0);
        return 0;
    }

    static int toMuxOption(const MuxSetting & ms, const string & outputUrl, DavWaveOption & o) {
        o.setCategory(DavOptionClassCategory(), DavWaveClassMux());
        o.set(DavOptionImplType(), ms.mux_type().empty() ? "auto" :ms.mux_type());
        o.set(DavOptionContainerFmt(), ms.mux_fmt());
        o.set(DavOptionOutputUrl(), outputUrl);
        for (auto & m : ms.avdict_mux_option())
            o.set(m.first, m.second, 0);
        return 0;
    }
};

class PbStreamletSettingToDavOption {
public:
    static int mkInputStreamletWaveOptions (const string & inputUrl, /* also use input url as streamlet name */
                                            const InputStreamletSetting & inputSetting,
                                            vector<DavWaveOption> & waveOptions) {
        waveOptions.clear();
        if (inputSetting.has_demux()) {
            DavWaveOption demuxOption;
            PbWaveSettingToDavOption::toDemuxOption(inputSetting.demux(), demuxOption, inputUrl);
            waveOptions.emplace_back(demuxOption);
        }
        if (inputSetting.has_video_decode()) {
            DavWaveOption videoDecodeOption;
            PbWaveSettingToDavOption::toVideoDecodeOption(inputSetting.video_decode(), videoDecodeOption);
            waveOptions.emplace_back(videoDecodeOption);
        }
        if (inputSetting.has_audio_decode()) {
            DavWaveOption audioDecodeOption;
            PbWaveSettingToDavOption::toAudioDecodeOption(inputSetting.audio_decode(), audioDecodeOption);
            waveOptions.emplace_back(audioDecodeOption);
        }
        if (0 && inputSetting.has_post_decode_video_filter()) {
            if (!inputSetting.post_decode_video_filter().filter_type().empty()) {
                DavWaveOption videoFilterOption;
                PbWaveSettingToDavOption::toVideoFilterOption(inputSetting.post_decode_video_filter(),
                                                              videoFilterOption);
                waveOptions.emplace_back(videoFilterOption);
            }
        }
        if (0 && inputSetting.has_post_decode_audio_filter()) {
            if (!inputSetting.post_decode_audio_filter().filter_type().empty()) {
                DavWaveOption audioFilterOption;
                PbWaveSettingToDavOption::toAudioFilterOption(inputSetting.post_decode_audio_filter(),
                                                              audioFilterOption);
                waveOptions.emplace_back(audioFilterOption);
            }
        }
        return 0;
    }

    static int mkMixStreamletWaveOptions (const string & mixStreamletName,
                                          const MixStreamletSetting & mixSetting,
                                          vector<DavWaveOption> & waveOptions) {
        waveOptions.clear();
        if (!mixSetting.has_video_mix() || !mixSetting.has_audio_mix())
            return AVERROR(EINVAL);
        DavWaveOption videoMixOption;
        PbWaveSettingToDavOption::toVideoMixOption(mixSetting.video_mix(), videoMixOption);
        waveOptions.emplace_back(videoMixOption);
        DavWaveOption audioMixOption;
        PbWaveSettingToDavOption::toAudioMixOption(mixSetting.audio_mix(), audioMixOption);
        waveOptions.emplace_back(audioMixOption);
        if (0 && mixSetting.has_post_mix_video_filter()) {
            DavWaveOption videoFilterOption;
            PbWaveSettingToDavOption::toVideoFilterOption(mixSetting.post_mix_video_filter(), videoFilterOption);
            waveOptions.emplace_back(videoFilterOption);
        }
        if (0 && mixSetting.has_post_mix_audio_filter()) {
            DavWaveOption audioFilterOption;
            PbWaveSettingToDavOption::toAudioFilterOption(mixSetting.post_mix_audio_filter(), audioFilterOption);
            waveOptions.emplace_back(audioFilterOption);
        }
        return 0;
    }

    static int mkOutputStreamletWaveOptions (const vector<string> & fullOutputUrls,
                                             const OutputStreamletSetting & outputSetting,
                                             vector<DavWaveOption> & waveOptions) {
        waveOptions.clear();
        if (outputSetting.has_video_encode()) {
            DavWaveOption videoEncodeOption;
            PbWaveSettingToDavOption::toVideoEncodeOption(outputSetting.video_encode(), videoEncodeOption);
            waveOptions.emplace_back(videoEncodeOption);
        }
        if (outputSetting.has_audio_encode()) {
            DavWaveOption audioEncodeOption;
            PbWaveSettingToDavOption::toAudioEncodeOption(outputSetting.audio_encode(), audioEncodeOption);
            waveOptions.emplace_back(audioEncodeOption);
        }
        if (0 && outputSetting.has_pre_encode_video_filter()) {
            DavWaveOption videoFilterOption;
            PbWaveSettingToDavOption::toVideoFilterOption(outputSetting.pre_encode_video_filter(), videoFilterOption);
            waveOptions.emplace_back(videoFilterOption);
        }
        if (0 && outputSetting.has_pre_encode_audio_filter()) {
            DavWaveOption audioFilterOption;
            PbWaveSettingToDavOption::toAudioFilterOption(outputSetting.pre_encode_audio_filter(), audioFilterOption);
            waveOptions.emplace_back(audioFilterOption);
        }
        /* use min(the muxer number, output url number) */
        const int outNum = outputSetting.mux_outputs().size() > (int)fullOutputUrls.size() ?
            fullOutputUrls.size() : outputSetting.mux_outputs().size();
        int k = 0;
        for (const auto & m : outputSetting.mux_outputs()) {
            if (k >= outNum)
                break;
            DavWaveOption muxOption;
            PbWaveSettingToDavOption::toMuxOption(m, fullOutputUrls[k], muxOption);
            waveOptions.emplace_back(muxOption);
            k++;
        }
        return 0;
    }
};


////////////////////////////////
// pb event -> dav dynamic event
class PbEventToDavDynamicEvent {
public:
    /* Dynamic Events convertion */
    static int toLayoutUpdateEvent(const DavWaveSetting::VideoMixLayoutUpdate & pbe,
                                   DavDynaEventVideoMixLayoutUpdate & e) {
        for (auto & c : pbe.cells()) {
            DavVideoCellCoordinate coor;
            coor.x = c.x(); coor.y = c.y(); coor.w = c.w(); coor.h = c.h(); coor.layer = c.layer();
            e.m_cells.emplace_back(coor);
        }
        // string layoutStr = DavWaveSetting::EVideoMixLayout_Name(pbe.layout());
        e.m_layout = static_cast<EDavVideoMixLayout>(static_cast<int>(pbe.layout()));
        return 0;
    }
};

} // namespace ff_dynamic
