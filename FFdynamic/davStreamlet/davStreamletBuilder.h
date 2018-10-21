#pragma once

#include <string>
#include <glog/logging.h>
#include "davStreamlet.h"

////////////////////////////////////////////////////////////////////////////////
namespace ff_dynamic {
using ::std::string;

/* Dav Standard Streamlet Builders
   Customize your builders if it does't meet the scenario
 */
class DavStreamletBuilder {
public:
    DavStreamletBuilder() = default;
    virtual ~DavStreamletBuilder() = default;
    virtual shared_ptr<DavStreamlet> build(const vector<DavWaveOption> & waveOptions,
                                           const DavStreamletTag & streamletTag = DavUnknownStreamletTag(),
                                           const DavStreamletOption & streamletOptions = DavStreamletOption()) = 0;
    virtual shared_ptr<DavStreamlet> createStreamlet(const vector<DavWaveOption> & waveOptions,
                                                     const DavStreamletTag & streamletTag = DavUnknownStreamletTag(),
                                                     const DavStreamletOption & streamletOptions = DavStreamletOption());
    DavMessager m_buildInfo;
    string m_logtag {"[StreamletBuilder] "};
};

class DavDefaultInputStreamletBuilder : public DavStreamletBuilder {
public:
    virtual shared_ptr<DavStreamlet> build(const vector<DavWaveOption> & waveOptions,
                                           const DavStreamletTag & streamletTag = DavDefaultInputStreamletTag(),
                                           const DavStreamletOption & streamletOptions = DavStreamletOption());
};

class DavDefaultOutputStreamletBuilder : public DavStreamletBuilder {
public:
    virtual shared_ptr<DavStreamlet> build(const vector<DavWaveOption> & waveOptions,
                                           const DavStreamletTag & streamletTag = DavDefaultOutputStreamletTag(),
                                           const DavStreamletOption & streamletOptions = DavStreamletOption());
};

class DavMixStreamletBuilder : public DavStreamletBuilder {
public:
    virtual shared_ptr<DavStreamlet> build(const vector<DavWaveOption> & waveOptions,
                                           const DavStreamletTag & streamletTag = DavMixStreamletTag(),
                                           const DavStreamletOption & streamletOptions = DavStreamletOption());
};

class DavSingleWaveStreamletBuilder : public DavStreamletBuilder {
public:
    virtual shared_ptr<DavStreamlet> build(const vector<DavWaveOption> & waveOptions,
                                           const DavStreamletTag & streamletTag,
                                           const DavStreamletOption & streamletOptions = DavStreamletOption());
};

} // namespace ff_dynamic
