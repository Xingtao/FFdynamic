#pragma once

#include "davImpl.h"
#include "davPeerEvent.h"
#include "davImplTravel.h"
#include "davImplEventProcess.h"
#include "cellScaleSyncer.h"
#include "cellSetting.h"
#include "cellLayout.h"
#include "ffmpegHeaders.h"

namespace ff_dynamic {

///////////////////////////////
/* Video Cell Sync & Compose */
struct CellMixerParams {
    shared_ptr<DavTravelStatic> m_outStatic;
    EDavVideoMixLayout m_initLayout;
    CellAdornment m_adornment;
    bool m_bReGeneratePts = true;
    bool m_bStartAfterAllJoin = false;
    string m_logtag;
};

class CellMixer {
public:
    CellMixer() = default;
    virtual ~CellMixer() {closeMixer();}
    struct OneMixCell {
        shared_ptr<DavTravelStatic> m_in;
        unique_ptr<CellScaleSyncer> m_syncer;
        CellPaster m_cellPaster;
        CellArchor m_archor;
    };

public: /* called with lock */
    int initMixer(const CellMixerParams & cmp);
    void setFixedInputNum (const int fixedNum) {m_fixedInputNum = fixedNum;;}
    int sendFrame(const DavProcFrom & from, AVFrame *frame);
    int receiveFrames(vector<AVFrame *> & outFrames, vector<shared_ptr<DavPeerEvent>> & pubEvents);
    int onJoin(const DavProcFrom & from, shared_ptr<DavTravelStatic> & in);
    int onLeft(const DavProcFrom & from);
    int onUpdateLayoutEvent(const DavDynaEventVideoMixLayoutUpdate & event);
    int onUpdateBackgroudEvent(const DavDynaEventVideoMixSetNewBackgroud & event);

public: /* trival helpers */
    inline bool isNewcomer(const DavProcFrom & from) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_cells.count(from) == 0;
    }

private:
    int closeMixer();
    int updateCellSettings(std::function<int (int & cellArchorPos)> posOp);
    int updateOneMixCellSettings(unique_ptr<OneMixCell> & oneMixCell, const int pos,
                                 shared_ptr<DavTravelStatic> & in, shared_ptr<DavTravelStatic> & out);

private: /* process mix syncers */
    int doMixCells();
    int mixOneFrame();

private: // trival helpers
    int getMixProcOrderByLayer(vector<std::pair<int, DavProcFrom>> & mixOrder);
    AVFrame *allocMixedOutputFrame();
    AVFrame *copyMixedFrame();
    int setToDark(AVFrame *frame);

///////////////////////
private:
    CellAdornment m_adornment; /* each cell use the same adornment */
    map<DavProcFrom, unique_ptr<OneMixCell>> m_cells;

private:
    string m_logtag;
    std::mutex m_mutex;

    /* mix video sync related */
    shared_ptr<DavTravelStatic> m_outStatic;
    bool m_bReGeneratePts = true; /* by default generate pts from 0. if use stream timestamp, set this as false */
    bool m_bStartAfterAllJoin = false; /* be default, start mixing just has input */
    int m_fixedInputNum = -1;
    int64_t m_startMixPts = AV_NOPTS_VALUE;
    int64_t m_curMixPts = AV_NOPTS_VALUE;
    int64_t m_oneFramePtsInc = AV_NOPTS_VALUE;
    /* stat */
    uint64_t m_outputMixFrameCount = 0;
    uint64_t m_discardInput = 0;
    uint64_t m_discardOutput = 0;

private: /* mix frame & layout settings */
    shared_ptr<AVFrame> m_backgroudFrame;
    /* init this frame at first incoming frame (for we may use its hw_frame_ctx).
       when 'm_canvasFrame' is mixed by cell frames, it is copied to a new output frame.
       'm_canvasFrame' also refresh its backgroud when layout change */
    AVFrame *m_canvasFrame = nullptr;
    vector<AVFrame *> m_mixedFrames;
    vector<shared_ptr<DavEventVideoMixSync>> m_mixerPeerEvents;

    bool m_bAutoLayout = true; /* auto change layout or specific layout with specific coordinates */
    EDavVideoMixLayout m_layout = EDavVideoMixLayout::eLayoutAuto;
    bool m_bUpdateCellSettings = false;
    bool m_bFlushVideoMix = false;
};

} //namespace ff_dynamic
