#pragma once

#include <glog/logging.h>
#include "davDict.h"
#include "dynaDnnDetect.pb.h"

#include "objDetect.h"

namespace ff_dynamic {
using namespace DynaDnnDetect;

class PbDnnDetectSettingToDavOption {
public:
    static int toDnnDetectOption(const DnnDetectSetting & dds, DavWaveOption & o, const string & detectorName = "") {
        if (detectorName.empty())
            o.setCategory(DavOptionClassCategory(), DavWaveClassObjDetect());
        else
            o.setCategory(DavOptionClassCategory(), DavWaveClassObjDetect(detectorName));
        o.set(DavOptionImplType(), dds.detector_type());
        // TODO: wiil be good to use 'field descriptor'
        o.set("detect_or_classify", dds.detect_or_classify());
        o.set("detector_framework_tag", dds.detector_framework_tag());
        o.set("model_path", dds.model_path());
        o.set("config_path", dds.config_path());
        o.set("classname_path", dds.classname_path());
        o.setInt("backend_id", dds.backend_id());
        o.setInt("target_id", dds.target_id());
        o.setDouble("scale_factor", dds.scale_factor());
        o.setBool("swap_rb", dds.swap_rb());
        o.setInt("width", dds.width());
        o.setInt("height", dds.height());
        o.setDouble("conf_threshold", (double)dds.conf_threshold());
        const int detect_interval = dds.detect_interval() <= 0 ? 1 : dds.detect_interval();
        o.setInt("detect_interval", detect_interval);
        vector<double> means;
        for (auto & m : dds.means())
            means.emplace_back((double)m);
        o.setDoubleArray("means", means);
        return 0;
    }
};

} // namespace ff_dynamic
