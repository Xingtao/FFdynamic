#pragma once

#include "ffmpegHeaders.h"
#include "davUtil.h"
#include "davDict.h"
#include "davImpl.h"
#include "davImplTravel.h"

namespace ff_dynamic {

class DataRelay : public DavImpl {
public:
    DataRelay(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~DataRelay() {onDestruct();}

private:
    DataRelay(const DataRelay &) = delete;
    DataRelay & operator= (const DataRelay &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx) {return 0;};
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;
};

} //namespace ff_dynamic
