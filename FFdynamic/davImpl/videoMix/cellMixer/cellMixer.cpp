#include <algorithm>
#include "videoMix.h"
#include "cellMixer.h"
#include "imageToRawFrame.h"

namespace ff_dynamic {

//////////////////////////////////////////////////////////////////////////////////////////
// TODO: should disctinct flush and real left

// [cell paramters calculdate]
int CellMixer::updateOneMixCellSettings(unique_ptr<OneMixCell> & oneMixCell, const int pos,
                                        shared_ptr<DavTravelStatic> & in, shared_ptr<DavTravelStatic> & out) {
    vector<int> coors = CellLayout::getCoordinateOfLayoutAtPos(m_layout, pos);
    if (coors.size() == 0) /* this cell won't be shown on screen */
        return 0;

    oneMixCell->m_archor.init(out->m_width, out->m_height, coors, pos);
    LOG(INFO) << m_logtag << "update one cell settings: pos " << pos << ", coors "
              <<  vectorToStringViaOss(coors) <<", in " << *in << ", out " << *out;

    ScaleFilterParams sfp;
    // in
    sfp.m_inFormat = in->m_pixfmt;
    sfp.m_inWidth = in->m_width;
    sfp.m_inHeight = in->m_height;
    sfp.m_inTimebase = out->m_timebase; /* NOTE: in frame's timestamp already convert to out's timebase */
    sfp.m_inFramerate = in->m_framerate;
    sfp.m_inSar = in->m_sar;
    // out
    sfp.m_outTimebase = out->m_timebase;
    sfp.m_outFramerate = out->m_framerate;
    sfp.m_hwFramesCtx = nullptr; /* TODO! */
    sfp.m_bFpsScale = true; /* whether do fps conversion */

    /* formular to calculate remains:
       1. keep scaled image has the same DAR with incoming image,
            inWidth * inSar.num / (inHeight * inSar.den) = (scaledWidth * outSar.num)  / (scaledHeight * outSar.den)
          namely:
            scaledDar = av_div_q(av_mul_q(inSar, AVRational {inWidth, inHeight}), outSar)
       2. then, pad only in one direction, namely, either padX = 0 or padY = 0;
          but margin is always there, so
            cellWidth = scaledWidth + 2 * margin + 2 * padX;   // assume: padLeft = padRight = padX
            cellHeight = scaledHeight + 2 * margin + 2 * padY; // assume: padTop = padBottom = padY
       3. if padding happens in X direction, then we have:
            scaledHeight = cellHeight - 2 * margin
            scaledWidth = scaledHeight * scaledDar.den / scaledDar.num
            then, padX = (cellWidth - scaledWidth - 2 * margin) / 2
            finaly if padX >= 0, it done, otherwise when padX < 0, we know padding happens
            in Y direction, just repeat the step 3.
          Of cause, we can use following formular to know the padding direction:
            let: dar = AVRational(cellWidth - 2 * margin, cellHeight - 2 * margin)
            if dar > scaledDar, then padding in X, otherwise in Y;

        NOTE: there is round error during calculate.
    */
    const int cellWidth = oneMixCell->m_archor.m_w;
    const int cellHeight = oneMixCell->m_archor.m_h;
    const int margin = m_adornment.m_marginSize;
    const AVRational scaledDar = av_div_q(av_mul_q(in->m_sar, AVRational{in->m_width, in->m_height}) , out->m_sar);
    CHECK(scaledDar.num != 0 && scaledDar.den != 0);

    int scaledHeight = cellHeight - 2 * margin;
    int scaledWidth = ((int)round(scaledHeight * scaledDar.num * 1.0 / scaledDar.den / 2.0)) << 1;
    int padY = 0;
    int padX = (cellWidth - scaledWidth - 2 * margin) >> 1;
    if (padX < 0) { /* recalculate, padding happens in Y direction */
        padX = 0;
        scaledWidth = cellWidth - 2 * margin;
        scaledHeight = ((int)round(scaledWidth * scaledDar.den * 1.0 / scaledDar.num / 2.0)) << 1;
        padY = (cellHeight - scaledHeight - 2 * margin) >> 1;
    }

    /* finally, get all parameters */
    sfp.m_outWidth = scaledWidth;
    sfp.m_outHeight = scaledHeight;
    /* setup cell paster */
    oneMixCell->m_cellPaster.update(oneMixCell->m_archor, m_adornment, padX, padY, in->m_pixfmt, out->m_pixfmt);
    oneMixCell->m_syncer->updateSyncerScaleParam(sfp);
    LOG(INFO) << m_logtag << "OneCell's params update done: scaledDar " << scaledDar << ", scaled WxH "
              << scaledWidth << "x" << scaledHeight << ", padX " << padX << ", padY " << padY << ", "
              << oneMixCell->m_archor;
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// [Event Process]

int CellMixer::onJoin(const DavProcFrom & from, shared_ptr<DavTravelStatic> & in) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int curCellNum = (int)m_cells.size();
    int totalCellNum = curCellNum + 1;
    if (m_bStartAfterAllJoin)
        totalCellNum = m_fixedInputNum;

    if (m_bAutoLayout) {
        EDavVideoMixLayout newLayout = CellLayout::getAutoLayoutViaCellNum(totalCellNum);
        if (newLayout != m_layout) {
            LOG(INFO) << m_logtag << "layout change: from " << CellLayout::getLayoutTypeString(m_layout)
                      << " to " << CellLayout::getLayoutTypeString(newLayout);
            m_layout = newLayout;
            m_bUpdateCellSettings = true;
        } else
            LOG(INFO) << m_logtag << "add one cell and no layout change: "
                      << CellLayout::getLayoutTypeString(m_layout);
    }
    /* TODO: else, calculate according to the setup */

    m_cells.emplace(from, unique_ptr<OneMixCell>(new OneMixCell()));
    auto & oneMixCell = m_cells.at(from);
    unique_ptr<CellScaleSyncer> syncer(new CellScaleSyncer(trimStr(m_logtag) +
                                                           "-CellScaleSyncer-" + from.m_descFrom));
    CHECK(syncer != nullptr);
    oneMixCell->m_syncer = std::move(syncer);
    oneMixCell->m_in = in;

    oneMixCell->m_archor.m_atPos = curCellNum;

    /* update cell settings */
    if (m_bUpdateCellSettings)
        updateCellSettings([](int & pos) {return 0;}); /* do nothing to existing cell pos */
    else
        updateOneMixCellSettings(oneMixCell, curCellNum, in, m_outStatic);
    LOG(INFO) << m_logtag << "Add one new stream done, total now " << m_cells.size();
    return 0;
}

int CellMixer::onLeft(const DavProcFrom & from) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cells.size() == 0) {
        LOG(WARNING) << m_logtag << "No cell at all, cannot process onLeft: " << from;
        return 0;
    }
    const int totalCellNum = (int)m_cells.size() - 1;
    if (m_bAutoLayout) {
        EDavVideoMixLayout newLayout = CellLayout::getAutoLayoutViaCellNum(totalCellNum);
        if (newLayout != m_layout) {
            LOG(INFO) << m_logtag << "layout change: from " << CellLayout::getLayoutTypeString(m_layout)
                      << " to " << CellLayout::getLayoutTypeString(newLayout);
            m_layout = newLayout;
            m_bUpdateCellSettings = true;
        } else
            LOG(INFO) << m_logtag << "left one cell and no layout change: "
                      << CellLayout::getLayoutTypeString(m_layout);
    }
    /* TODO: else, calculate according to the setup */

    const int atPos = m_cells.at(from)->m_archor.m_atPos;
    m_cells.erase(from);
    /* update cell settings */
    if (m_bUpdateCellSettings)
        updateCellSettings([atPos](int & pos) {if (pos > atPos) pos--; return 0;});
    LOG(INFO) << m_logtag << "one input left " << from << ", atPos " << atPos;
    return 0;
}

int CellMixer::onUpdateLayoutEvent(const DavDynaEventVideoMixLayoutUpdate & event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto newLayout = event.m_layout;
    if (newLayout == EDavVideoMixLayout::eLayoutSpecific) {
        //DavVideoCellCoordinate m_cells;
        LOG(INFO) << m_logtag << "TODO: specific coordinates for each cell not support now";
        return 0;
    }

    int ret = 0;
    if (newLayout == m_layout) {
        LOG(INFO) << m_logtag << "unchanged layout update, do nothing"
                  << CellLayout::getLayoutTypeString(m_layout);
        return 0;
    }  else {
        LOG(INFO) << m_logtag << "layout change: from " << CellLayout::getLayoutTypeString(m_layout)
                  << " to " << CellLayout::getLayoutTypeString(newLayout);
        m_layout = newLayout;
    }
    /* two cases occur: cells num less/more than layout cell num */
    ret = updateCellSettings([](int & pos) {return 0;}); /* do nothin to position */
    if (ret < 0) {
        LOG(ERROR) << m_logtag << "fail when do layout change: from " << CellLayout::getLayoutTypeString(m_layout)
                   << " to " << CellLayout::getLayoutTypeString(newLayout);
        return DAV_ERROR_EVENT_LAYOUT_UPDATE;
    }
    LOG(INFO) << m_logtag << "layout change done " << CellLayout::getLayoutTypeString(m_layout);
    return 0;
}

int CellMixer::onUpdateBackgroudEvent(const DavDynaEventVideoMixSetNewBackgroud & event) {
    auto newBgFrame = ImageToRawFrame::loadImageToRawFrame(event.m_backgroudUrl);
    if (!newBgFrame) {
        LOG(ERROR) << "cannot laod backgroud url " + event.m_backgroudUrl;
        return DAV_ERROR_EVENT_BACKGROUD_UPDATE;
    }
    /* lock here to avoid long time wait caused by image load */
    std::lock_guard<std::mutex> lock(m_mutex);
    if ((newBgFrame->width != m_canvasFrame->width) || (newBgFrame->height != m_canvasFrame->height) ||
        (newBgFrame->format != m_canvasFrame->format)) {
        auto frame = FmtScale::fmtScale(newBgFrame, m_canvasFrame->width, m_canvasFrame->height,
                                        (enum AVPixelFormat)m_canvasFrame->format);
        if (!frame) {
            LOG(ERROR) << "laoded backgroud frame cannot convert to canvas frame format: " + event.m_backgroudUrl;
            return DAV_ERROR_EVENT_BACKGROUD_UPDATE;
        }
        m_backgroudFrame.swap(frame);
    } else
        m_backgroudFrame.swap(newBgFrame);

    if (!m_canvasFrame) {
        LOG(INFO) << m_logtag << "update backgroud done; not assign to canvas frame";
        return 0;
    }
    if (m_backgroudFrame)
        av_frame_copy(m_canvasFrame, m_backgroudFrame.get());
    else
        setToDark(m_canvasFrame);
    LOG(INFO) << m_logtag << "update backgroud done";
    return 0;
}

////////////////////////
// [init]
int CellMixer::initMixer(const CellMixerParams & cmp) {
    m_outStatic = cmp.m_outStatic;
    m_layout = cmp.m_initLayout;
    m_adornment = cmp.m_adornment;
    m_bReGeneratePts = cmp.m_bReGeneratePts;
    m_bStartAfterAllJoin = cmp.m_bStartAfterAllJoin;
    m_logtag = cmp.m_logtag;

    /* this PtsInc has round error, may use av_rescale_q directly in future */
    m_oneFramePtsInc = av_rescale_q(1, av_inv_q(m_outStatic->m_framerate), m_outStatic->m_timebase);
    /* this frame will cache the backgroud until it re-freshed or layout change */
    m_canvasFrame = allocMixedOutputFrame();
    if (m_backgroudFrame)
        av_frame_copy(m_canvasFrame, m_backgroudFrame.get());
    else
        setToDark(m_canvasFrame);
    CHECK(m_canvasFrame != nullptr);
    LOG(INFO) << m_logtag << "Cell Mixer init with " << m_outStatic << ", ReGeneratePts " << m_bReGeneratePts
              << ", oneFramePtsInc " << m_oneFramePtsInc;
    return 0;
}

//////////////////
// [Layout change]
int CellMixer::updateCellSettings(std::function<int (int & cellArchorPos)> posOp) {
    for (auto & c : m_cells) {/* normally, should be called after onFlush */
        posOp(c.second->m_archor.m_atPos);
        updateOneMixCellSettings(c.second, c.second->m_archor.m_atPos, c.second->m_in, m_outStatic);
    }
    m_bUpdateCellSettings = false;
    /* clear backgroud when layout change */
    if (m_backgroudFrame)
        av_frame_copy(m_canvasFrame, m_backgroudFrame.get());
    else
        setToDark(m_canvasFrame);
    return 0;
}

//////////////////////////////
// [Mix Cell Process]
int CellMixer::sendFrame(const DavProcFrom & from, AVFrame *inFrame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int ret = 0;
    if (m_startMixPts == AV_NOPTS_VALUE && !inFrame) {
        return AVERROR(EAGAIN);
    }
    else if (m_startMixPts == AV_NOPTS_VALUE && inFrame) { /* the first incoming frame */
        m_curMixPts = m_startMixPts = (m_bReGeneratePts ? 0 : inFrame->pts);
    }

    ret = m_cells.at(from)->m_syncer->sendFrame(inFrame); /* always */
    if (ret < 0 && ret != AVERROR_EOF) {
        LOG(ERROR) << m_logtag << "failed to send one frame to cellScaleSyncer " << m_curMixPts;
        m_discardInput++;
        return ret;
    }
    return doMixCells();
}

int CellMixer::receiveFrames(vector<AVFrame *> & outFrames, vector<shared_ptr<DavPeerEvent>> & pubEvents) {
    std::lock_guard<std::mutex> lock(m_mutex);
    outFrames = m_mixedFrames;
    for (auto & e : m_mixerPeerEvents)
        pubEvents.push_back(std::move(e));
    m_mixedFrames.clear();
    m_mixerPeerEvents.clear();
    return 0;
}

//////////////
// [Mix Cells]
int CellMixer::doMixCells() {
    int ret = 0;
    if (m_bStartAfterAllJoin && (int)m_cells.size() != m_fixedInputNum)
        return 0;

    do {
        vector<bool> bMixContinue;
        for (auto & c : m_cells) {
            bool bContinue = c.second->m_syncer->processSync(m_curMixPts);
            bMixContinue.push_back(bContinue);
        }
        if (!all(bMixContinue)) {
            // TODO:
            // for (auto & c : m_cells) {
                // TODO: check data length, if one of them exceed MAX_JITTER, kick it out, countinue mix;
                // c.second->m_syncer->getAccumulateDataTime(m_curMixPts);
                // if exceed max jitter time, erase from m_cells (send this event to audio?)
            //}
            break;
        }

        /* the order matters, increase pts after mix this frame */
        ret = mixOneFrame();
        if (ret == AVERROR(EAGAIN))
            break; /* no frame mixed */
        else if (ret < 0) {
            LOG(ERROR) << m_logtag << "fail to mix one frame, discard " << ++m_discardOutput;
            continue;
        }
        // m_curMixPts = m_startMixPts + (int64_t)m_outputMixFrameCount * m_oneFramePtsInc;
        m_curMixPts = m_startMixPts + av_rescale_q(m_outputMixFrameCount,
                                                   av_inv_q(m_outStatic->m_framerate), m_outStatic->m_timebase);
        m_outputMixFrameCount++;
    } while (true);

    return 0;
}

int CellMixer::mixOneFrame() {
    int ret = 0;
    shared_ptr<DavEventVideoMixSync> videoSyncEvent(new DavEventVideoMixSync);
    /* currently layer order is not used */
    vector<std::pair<int, DavProcFrom>> mixOrderByLayer;
    getMixProcOrderByLayer(mixOrderByLayer);
    int cellPasteCount = 0;
    for (auto & orderedFrom : mixOrderByLayer) {
        auto & oneMixCell = m_cells.at(orderedFrom.second);
        shared_ptr<AVFrame> frame = oneMixCell->m_syncer->receiveFrame(m_curMixPts);
        if (!frame)
            continue;
        /* here we skip cells that won't be shown in current layout */
        if (oneMixCell->m_archor.m_atPos >= CellLayout::getCellNumViaLayout(m_layout))
            continue;
        ret = oneMixCell->m_cellPaster.paste(m_canvasFrame, frame.get());
        if (ret < 0) {
            LOG(ERROR) << m_logtag << davMsg2str(ret) << "fail mix one cell's frame: mixPts "
                       << m_curMixPts << ", " << orderedFrom.second << ", layerNo "
                       << orderedFrom.first << ", frame pts " << frame->pts;
            continue;
        }
        cellPasteCount++;
        videoSyncEvent->m_videoStreamInfos.emplace_back(DavEventVideoMixSync::VideoStreamInfo());
        auto & videoStreamSyncInfo = videoSyncEvent->m_videoStreamInfos.back();
        videoStreamSyncInfo.m_from = orderedFrom.second;
        videoStreamSyncInfo.m_curPts = frame->pts;
    }

    if (cellPasteCount == 0)
        return AVERROR(EAGAIN);

    videoSyncEvent->m_videoMixCurPts = m_curMixPts;
    m_canvasFrame->pts = m_curMixPts;
    /* put ready mixed frame */
    auto outFrame = copyMixedFrame();
    m_mixedFrames.emplace_back(outFrame);
    m_mixerPeerEvents.emplace_back(videoSyncEvent);
    return 0;
}

///////////////////
// [trival helpers]

int CellMixer::closeMixer() {
    if (m_canvasFrame)
        av_frame_free(&m_canvasFrame);
    return 0;
}

int CellMixer::getMixProcOrderByLayer(vector<pair<int, DavProcFrom>> & mixOrder) {
    /* layer = -1, indicates we can ignore its order; put those in front then. */
    for (auto & cell : m_cells)
        mixOrder.push_back(std::make_pair(cell.second->m_archor.m_layer, cell.first));
    std::sort(mixOrder.begin(), mixOrder.end()); /* pair compares its first element by default */
    return 0;
}

AVFrame *CellMixer::allocMixedOutputFrame() {
    AVFrame *outFrame = av_frame_alloc();
    CHECK(outFrame != nullptr);
    outFrame->width = m_outStatic->m_width;
    outFrame->height = m_outStatic->m_height;
    outFrame->format = m_outStatic->m_pixfmt;
    /* TODO: cuda frame allocate */
    if(av_frame_get_buffer(outFrame, 0) < 0)
        av_frame_free(&outFrame);
    return outFrame;
}

AVFrame *CellMixer::copyMixedFrame() {
    /* copy canvasFrame to this mixFrame */
    int ret = 0;
    AVFrame *outFrame = allocMixedOutputFrame();
    CHECK(outFrame != nullptr);
    ret = av_frame_copy(outFrame, m_canvasFrame);
    CHECK(ret >= 0);
    ret = av_frame_copy_props(outFrame, m_canvasFrame);
    CHECK(ret >= 0);
    return outFrame;
}

int CellMixer::setToDark(AVFrame *frame) {
    CHECK(frame != nullptr);
    const int ySize = frame->width * frame->height;
    /* TODO: here we assume it is YUV420P */
    memset(frame->data[0], 16, ySize);
    memset(frame->data[1], 128, ySize >> 2);
    memset(frame->data[2], 128, ySize >> 2);
    return 0;
}

} // namespace
