#include <iostream>
#include <iomanip>
#include <memory>
#include "dataRelay.h"

namespace ff_dynamic {
using ::std::shared_ptr;

//// Register ////
static DavImplRegister s_dataRelay(DavWaveClassDemux(), vector<string>({"auto", "dataRelay"}), {},
                                   [](const DavWaveOption & options) -> unique_ptr<DavImpl> {
                                       unique_ptr<DataRelay> p(new DataRelay(options));
                                       return p;
                                   });

const DavRegisterProperties & DataRelay::getRegisterProperties() const noexcept {
    return s_dataRelay.m_properties;
}
//////////////////////////////////////////////////////////////////////////////////////////
int DataRelay::onConstruct() {
    int ret = 0;
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
    int ret = 0;
    if (ctx.m_inBuf->isEmptyData()) {
        LOG(INFO) << "data source recevei flush data" << (*ctx.m_inBuf);
        ctx.m_bInputFlush = true;
        /* just remove this sender and goon until no connected senders */
        return 0;
    }
    // relay input data
    ctx.m_outBufs.emplace_back(ctx.m_inBuf);
    return 0;
}

} // namespace
