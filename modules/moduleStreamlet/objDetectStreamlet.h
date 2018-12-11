#pragma once

#include <string>
#include <glog/logging.h>
#include "davStreamlet.h"
#include "davStreamletBuilder.h"

#include "objDetect.h"
#include "cvPostDraw.h"

////////////////////////////////////////////////////////////////////////////////
namespace ff_dynamic {

struct ObjDetectStreamletTag : public DavStreamletTag {
    ObjDetectStreamletTag() :
        DavStreamletTag("ObjDetectStreamletTag", type_index(typeid(ObjDetectStreamletTag))) {}
    explicit ObjDetectStreamletTag(const string & streamletName)
        : DavStreamletTag(streamletName, type_index(typeid(ObjDetectStreamletTag))) {}
};

/* Customize cv streamlet builders */
class ObjDetectStreamletBuilder : public DavStreamletBuilder {
public:
    virtual shared_ptr<DavStreamlet> build(const vector<DavWaveOption> & waveOptions,
                                           const DavStreamletTag & streamletTag,
                                           const DavStreamletOption & streamletOptions = DavStreamletOption());
    /* additional one put here */
    int addDetector(shared_ptr<DavStreamlet> & streamlet, const DavWaveOption & detectorOption);
    int deleteDetector(shared_ptr<DavStreamlet> & streamlet, const string & detectorName);
};

/* follow other cv streamlet builders */

} // namespace ff_dynamic
