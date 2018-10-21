#include <stdio.h>
#include <stdlib.h>

#include <glog/logging.h>
#include "audioResample.h"

using namespace ff_dynamic;

int main(int argc, char ** argv) {

    google::InitGoogleLogging("audio-resample");
    FLAGS_stderrthreshold = 0;
    FLAGS_logtostderr = 1;

    AudioResampleParams aParams;
    aParams.m_logtag = "[audio-resample]";
    aParams.m_srcFmt = AV_SAMPLE_FMT_S16;
    aParams.m_dstFmt = AV_SAMPLE_FMT_FLTP;
    aParams.m_srcSamplerate = 48000;
    aParams.m_dstSamplerate = 22050;
    aParams.m_srcLayout = AV_CH_LAYOUT_STEREO;
    aParams.m_dstLayout = AV_CH_LAYOUT_5POINT0;
    aParams.m_logtag = "audio-resample-test";
    AudioResample ar(aParams);
    int count = 0;
    // alloc a random data input frame
    AVFrame *frame = av_frame_alloc();
    CHECK(frame != nullptr);
    frame->nb_samples = 1024;
    frame->channel_layout = aParams.m_srcLayout;
    frame->format = aParams.m_srcFmt;
    frame->sample_rate = aParams.m_srcSamplerate;
    av_frame_get_buffer(frame, 0);
    int ret = 0;
    do
    {
        ar.sendResampleData(frame);
        AVFrame *out = nullptr;
        ret = ar.receiveResampledData(out, 1024);
        if (ret < 0 && ret != AVERROR(EAGAIN))
            LOG(INFO) << "fail receive " << ret;
        else if (ret > 0)
            LOG(INFO) << "got one frame " << ret;
        av_frame_free(&out);
    } while(count++ < 10);

    return 0;
}
