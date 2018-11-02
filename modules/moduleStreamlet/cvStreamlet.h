#pragma once

#include <string>
#include <glog/logging.h>
#include "davStreamlet.h"
#include "davStreamletBuilder.h"

////////////////////////////////////////////////////////////////////////////////
namespace ff_dynamic {

struct CvDnnDetectStreamletTag : public DavStreamletTag {
    CvDnnDetectStreamletTag() :
        DavStreamletTag("CvDnnDetectStreamlet", type_index(typeid(CvDnnDetectStreamletTag))) {}
    explicit CvDnnDetectStreamletTag(const string & streamletName)
        : DavStreamletTag(streamletName, type_index(typeid(CvDnnDetectStreamletTag))) {}
};

/* Customize cv streamlet builders */
class CvDnnDetectStreamletBuilder : public DavStreamletBuilder {
public:
    virtual shared_ptr<DavStreamlet> build(const vector<DavWaveOption> & waveOptions,
                                           const DavStreamletTag & streamletTag,
                                           const DavStreamletOption & streamletOptions = DavStreamletOption());
};

/* follow other cv streamlet builders */

} // namespace ff_dynamic
