#include <iostream>
#include <iomanip>
#include <memory>
#include "dataRelay.h"

namespace ff_dynamic {
using ::std::shared_ptr;

//// Register ////
static DavImplRegister s_dataRelay(DavWaveClassDataRelay(), vector<string>({"auto", "dataRelay"}), {},
                                   [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                       unique_ptr<DavImpl> p(new DataRelay(options));
                                       return p;
                                   });

const DavRegisterProperties & DataRelay::getRegisterProperties() const noexcept {
    return s_dataRelay.m_properties;
}

//////////////////////////////////////////////////////////////////////////////////////////
int DataRelay::onConstruct() {
    LOG(INFO) << "DataRelay just do data relay " << m_options.dump();
    m_bDataRelay = true;
    m_bDynamicallyInitialized = true;
    return 0;
}

int DataRelay::onDestruct() {
    return 0;
}

int DataRelay::onProcess(DavProcCtx & ctx) {
    ctx.m_expect.m_expectOrder = {EDavExpect::eDavExpectAnyOne};
    if (!ctx.m_inBuf)
        return 0;
    if (ctx.m_inBuf->isEmptyData()) {
        LOG(INFO) << "data relay recevei flush frame, quit. " << (*ctx.m_inBuf);
        ctx.m_bInputFlush = true;
        return AVERROR_EOF;
    }
    // relay input data
    auto frame = ctx.m_inBuf->releaseAVFrameOwner();
    auto pkt = ctx.m_inBuf->releaseAVPacketOwner();
    auto outBuf = make_shared<DavProcBuf>();
    outBuf->mkAVFrame(frame);
    outBuf->mkAVPacket(pkt);
    outBuf->m_travelStatic = ctx.m_inBuf->m_travelStatic;
    ctx.m_outBufs.emplace_back(outBuf);
    return 0;
}

} // namespace
