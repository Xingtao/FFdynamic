#include <assert.h>
#include "cuvidReader.h"

namespace cuvid_reader {

int CuvidReader::getPacket(AVPacket *pkt) {
    int ret = 0;
    do {
        av_init_packet(pkt);
        ret = av_read_frame(m_fmtCtx, pkt);
        if (ret < 0) {
            av_packet_unref(pkt);
            if (ret == AVERROR(EAGAIN))
                continue;
            return ret;
        }
        if (m_fmtCtx->streams[pkt->stream_index]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
            av_packet_unref(pkt);
            continue;
        }
        break;
    } while(true);
    return 0;
}

int CuvidReader::getFrames(vector<shared_ptr<AVFrame>> & frames) {
    frames.clear();
    AVPacket pkt;
    int ret = 0;
    if (!m_bFlush) {
        ret = getPacket(&pkt);
        if (ret == AVERROR_EOF)
            m_bFlush = true;
    }
    if (ret >= 0) { /* send a valid packet */
        avcodec_send_packet(m_decCtx, &pkt);
        av_packet_unref(&pkt);
    }
    do {
        shared_ptr<AVFrame> frame;
        frame.reset(av_frame_alloc(), [](AVFrame *p){av_frame_free(&p);});
        assert(frame != nullptr);
        ret = avcodec_receive_frame(m_decCtx, frame.get());
        if (ret == AVERROR(EAGAIN))
            return 0;
        else if (ret < 0) {// include EOF & EAGAIN
            return ret;
        }
        if (frame->best_effort_timestamp != AV_NOPTS_VALUE)
            frame->pts = frame->best_effort_timestamp;
        frames.emplace_back(frame);
    } while (true);

    return 0;
}

int CuvidReader::getCudaFrames(vector<shared_ptr<uint8_t>> & frames) {
    frames.clear();
    AVPacket pkt;
    int ret = 0;
    if (!m_bFlush) {
        ret = getPacket(&pkt);
        if (ret == AVERROR_EOF)
            m_bFlush = true;
    }

    uint8_t **ppFrame;
    int nFrameReturned = 0;
    m_nvDecoder->DecodeLockFrame(pkt.data, pkt.size, &ppFrame, &nFrameReturned);
    frames.resize(nFrameReturned);
    for (int i = 0; i < nFrameReturned; i++) {
        if (m_ndp.m_bToBgr) {
            const int width = m_nvDecoder->GetWidth();
            const int height = m_nvDecoder->GetHeight();
            uint8_t *pBgr = nullptr;
            CUDA_DRVAPI_CALL(cuMemAlloc((CUdeviceptr *)&pBgr, width * height * 3));
            Nv12ToBgrPlanar(ppFrame[i], width, pBgr, width, height);
            m_nvDecoder->UnlockFrame(ppFrame[i]);
            frames[i].reset(pBgr, [](uint8_t *dptr) {cuMemFree((CUdeviceptr)dptr);});
        } else {
            frames[i].reset(ppFrame[i], [this](uint8_t *dptr) {m_nvDecoder->UnlockFrame(dptr);});
        }
    }
    return 0;
}

//// [Init] ///////////
int CuvidReader::initDemux(const string & streamUrl, AVDictionary *options) {
    if (streamUrl.size() == 0)
        return AVERROR(EINVAL);
    m_inputUrl = streamUrl;
    m_fmtCtx = avformat_alloc_context();
    if (!m_fmtCtx)
        return AVERROR(ENOMEM);
    int ret = avformat_open_input(&m_fmtCtx, m_inputUrl.c_str(), nullptr, &options);
    if (ret < 0)
        return ret;
    /* TODO: block time set */
    ret = avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0)
        return ret;
    av_dump_format(m_fmtCtx, 1, m_inputUrl.c_str(), 0);

    // NOTE: for demux already initialized , here we just set up the output infos. also ignore 'ctx'
    for (unsigned int k=0; k < m_fmtCtx->nb_streams; k++) {
        const AVStream *st = m_fmtCtx->streams[k];
        // only deal with audio/video, ignore subtitle or data stream
        if (st->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
            continue;
        m_videoCodecpar = st->codecpar;
        break; // just use the first video stream
    }
    std::cout << "open stream " << m_inputUrl << " done\n";
    return 0;
}

int CuvidReader::initDecode(AVDictionary *options) {
    int ret = 0;
    AVCodec *dec = nullptr;
    if (m_videoCodecpar->codec_id == AV_CODEC_ID_H264)
        dec = avcodec_find_decoder_by_name("h264_cuvid");
    else if (m_videoCodecpar->codec_id == AV_CODEC_ID_H265)
        dec = avcodec_find_decoder_by_name("hevc_cuvid");
    if (!dec) {
        std::cerr << "[CuvidReader] Cannot use Cuda Hwaccel Decoder. Try default\n";
        dec = avcodec_find_decoder(m_videoCodecpar->codec_id);
        if (!dec) {
            std::cerr << "[CuvidReader] Cannot find suitable Decoder\n";
            return AVERROR(EINVAL);
        }
    }

    m_decCtx = avcodec_alloc_context3(dec);
    if (!m_decCtx) {
        std::cerr << "[CuvidReader] Fail alloate decode context\n";
        return AVERROR(ENOMEM);
    }
    /* hw context set */
    m_decCtx->get_format = NvidiaCudaHwAccel::getFormat;
    if ((ret = avcodec_parameters_to_context(m_decCtx, m_videoCodecpar)) < 0) {
        std::cerr << ret << " [CuvidReader]  codecpar to context fail\n";
        return ret;
    }
    if ((ret = avcodec_open2(m_decCtx, dec, &options)) < 0) {
        std::cerr << ret << " [CuvidReader]  video decode open fail \n";
        return ret;
    }

    std::cout << "[CuvidReader] Open cuvid done \n";
    return 0;
}

int CuvidReader::initNvDecoderInterface() {
    ck(cuInit(0));
    int nGpu = 0;
    ck(cuDeviceGetCount(&nGpu));
    if (ndp.m_deviceIdx < 0 || ndp.m_deviceIdx >= nGpu) {
        std::cerr << "GPU ordinal out of range. Should be within [" << 0 << ", " << nGpu - 1 << "]" << std::endl;
        return AVERROR(EINVAL);
    }
    // Rect cropRect = {};
    Dim resizeDim = {m_ndp.m_targetWidth, m_ndp.m_targetHeight};
    ck(cuDeviceGet(&m_cuDevice, ndp.m_deviceIdx));
    char szDeviceName[80];
    ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), m_cuDevice));
    std::cout << "GPU in use: " << szDeviceName << std::endl;
    ck(cuCtxCreate(&m_cuContext, 0, cuDevice));
    m_nvDeocder = new NvDecoder(m_cuContext, m_videoCodecpar->width, m_videoCodecpar->height,
                                true, cudaVideoCodec_H264, nullptr, false, false, nullptr,
                                m_ndp.m_targetWidth < 0 ? nullptr : &resizeDim);
    if (!m_nvDecoder) {
        std::cerr << "Createa nvDecoder interface failed\n";
        return AVERROR(EINVAL);
    }
    return 0;
}

int CuvidReader::init(const string & streamUrl, const NvDecoderParams & ndp,
                      AVDictionary *demuxOptions, AVDictionary *decodeOptions) {
    int ret = initDemux(streamUrl, demuxOptions);
    if (ret < 0)
        return ret;

    m_ndp = ndp;
    if (m_bUseNvDecoderInterface)
        return initNvDecoderInterface(ndp);
    return initDecode(decodeOptions);
}

int CuvidReader::close() {
    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
    }
    if (m_decCtx)
        avcodec_free_context(&m_decCtx);
    if (m_nvDecoder) {
        delete m_nvDecoder;
        m_nvDecoder = nullptr;
    }
    return 0;
}

} // namespace cuvid_reader
