#pragma once

#include <string>
#include <glog/logging.h>
#include "davStreamlet.h"
#include "davStreamletBuilder.h"

////////////////////////////////////////////////////////////////////////////////
namespace ff_dynamic {

struct DavCVDnnDetectStreamletTag : public DavStreamletTag {
    DavCVDnnDetectStreamletTag() :
        DavStreamletTag("CVDnnDetectStreamlet", type_index(typeid(DavCVDnnDetectStreamletTag))) {}
    explicit DavCVDnnDetectStreamletTag(const string & streamletName)
        : DavStreamletTag(streamletName, type_index(typeid(DavCVDnnDetectStreamletTag))) {}
};

struct DavCvObjClassifyStreamletTag : public DavStreamletTag {
    DavCvObjClassifyStreamletTag() :
        DavStreamletTag("CvObjClassifyStreamlet", type_index(typeid(DavCvObjClassifyStreamletTag))) {}
    explicit DavCvObjClassifyStreamletTag(const string & streamletName)
        : DavStreamletTag(streamletName, type_index(typeid(DavCvObjClassifyStreamletTag))) {}
};

/* Customize cv streamlet builders */
class DavCVDnnDetectStreamletBuilder : public DavStreamletBuilder {
public:
    virtual shared_ptr<DavStreamlet> build(const vector<DavWaveOption> & waveOptions,
                                           const DavStreamletTag & streamletTag,
                                           const DavStreamletOption & streamletOptions = DavStreamletOption());
};

/* Customize your builders if it does't meet the scenario */
class DavCvObjClassifyStreamletBuilder : public DavStreamletBuilder {
public:
    virtual shared_ptr<DavStreamlet> build(const vector<DavWaveOption> & waveOptions,
                                           const DavStreamletTag & streamletTag,
                                           const DavStreamletOption & streamletOptions = DavStreamletOption());
};


} // namespace ff_dynamic
