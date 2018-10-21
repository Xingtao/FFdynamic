#include "ffBasedUtils.h"

//////////////////////////////////////////////////////////
// bitstream filter
int initBitStreamFilter(AVBSFContext **avbsfContext, AVStream *avStream,
                        const char *filterName, const string & logtag)
{
    int ret = 0;
    const AVBitStreamFilter *avbsf = av_bsf_get_by_name(filterName);
    if (avbsf == nullptr)
    {
        NLogW(logtag, "Cannot find filter %s.", filterName);
        return -1;
    }

    ret = av_bsf_alloc(avbsf, avbsfContext);
    if (ret < 0)
    {
        NLogW(logtag, "Cannot allocate avbsf context: %s.", hv_av_err2str(ret).c_str());
        *avbsfContext = nullptr;
        return ret;
    }

    if (avStream != nullptr)
    {
        avcodec_parameters_copy((*avbsfContext)->par_in, avStream->codecpar);
        (*avbsfContext)->time_base_in = avStream->time_base;
    }
    else
    {
        NLogW(logtag, "No Codec Parameters.");
        return -1;
    }

    ret = av_bsf_init(*avbsfContext);
    if (ret < 0)
    {
        NLogW(logtag, "init avbsf context failed %s.", hv_av_err2str(ret).c_str());
        *avbsfContext = nullptr;
        return ret;
    }

    return 0;
}

int processVideoBitstreamFilter(AVBSFContext *avbsfContext, AVPacket & inPkt,
                                string & data, const string & logtag)
{
    int ret = 0;
    if ((ret = av_bsf_send_packet(avbsfContext, &inPkt)) < 0)
    {
        NLogW(logtag, "Bitstream filter send packet fail %s.",
              hv_av_err2str(ret).c_str());
        return -1;
    }

    int filterLen = 0;
    AVPacket pkt;
    do
    {
        av_init_packet(&pkt);
        pkt.data = nullptr;
        pkt.size = 0;
        ret = av_bsf_receive_packet(avbsfContext, &pkt);
        if (ret >= 0)
        {
            av_packet_split_side_data(&pkt);
            data.resize(filterLen + pkt.size);
            memcpy((unsigned char *)data.data() + filterLen, pkt.data, pkt.size);
            filterLen += pkt.size;
            av_packet_unref(&pkt);
        }
        else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else
        {
            NLogW(logtag, "Av filter receive packet fail %s. Try output filtered data.",
                  hv_av_err2str(ret).c_str());
            break;
        }
    } while (1);

    av_packet_unref(&pkt);
    return filterLen;
}
