#pragma once

#include "davWave.h"
#include "davImpl.h"
#include "dehazor.h"

namespace ff_dynamic {
using namespace dehaze;

/* create dehaze component category */
struct DavWaveClassDehaze : public DavWaveClassCategory {
    DavWaveClassDehaze (const string & nameTag = "Dehaze") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};

/* define dehaze options to illustrate how to use option passing */
struct DavOptionDehazeFogFactor : public DavOption {
    DavOptionDehazeFogFactor() :
        DavOption(type_index(typeid(*this)), type_index(typeid(double)), "DehazeFogFactor") {}
};

/* define an event to illustrate how to use dynamic event */
struct FogFactorChangeEvent {
    double m_newFogFactor = 0.1;
};

/* dehaze component may have diffrent implementation;
   so we would also register impl later, refer to register part at bottom of ffdynaDehazor.cpp's file */

/* here is one implementation called, PluginDehazor */
class PluginDehazor : public DavImpl {
public:
    PluginDehazor(const DavWaveOption & options) : DavImpl(options) {
        implDefaultInstantiate();
    }
    virtual ~PluginDehazor() {onDestruct();}

private: /* Interface we should implement */
    PluginDehazor(const PluginDehazor &) = delete;
    PluginDehazor & operator= (const PluginDehazor &) = delete;
    virtual int onConstruct();
    virtual int onDestruct();
    virtual int onProcess(DavProcCtx & ctx);
    virtual int onDynamicallyInitializeViaTravelStatic(DavProcCtx & ctx);
    /* if no travel dynamic needed, leave it empty */
    virtual int onProcessTravelDynamic(DavProcCtx & ctx) {return 0;}
    virtual const DavRegisterProperties & getRegisterProperties() const noexcept;

private:
    int processFogFactorUpdate(const FogFactorChangeEvent & e);

private:
    unique_ptr<Dehazor> m_dehazor;
};

} //namespace ff_dynamic
