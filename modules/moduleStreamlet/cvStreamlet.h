#pragma once

#include <string>
#include <glog/logging.h>
#include "davStreamlet.h"
#include "davStreamletBuilder.h"

////////////////////////////////////////////////////////////////////////////////
namespace ff_dynamic {

struct DavCvDnnDetectStreamletTag : public DavStreamletTag {
    DavCvDnnDetectStreamletTag() :
        DavStreamletTag("CvDnnDetectStreamlet", type_index(typeid(DavCvDnnDetectStreamletTag))) {}
    explicit DavCvDnnDetectStreamletTag(const string & streamletName)
        : DavStreamletTag(streamletName, type_index(typeid(DavCvDnnDetectStreamletTag))) {}
};

/* Customize cv streamlet builders */
class DavCvDnnDetectStreamletBuilder : public DavStreamletBuilder {
public:
    virtual shared_ptr<DavStreamlet> build(const vector<DavWaveOption> & waveOptions,
                                           const DavStreamletTag & streamletTag,
                                           const DavStreamletOption & streamletOptions = DavStreamletOption());
};

/* follow other cv streamlet builders */

} // namespace ff_dynamic
