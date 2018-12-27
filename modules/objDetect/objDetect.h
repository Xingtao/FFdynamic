#pragma once

#include <iostream>
#include "davWave.h"

namespace ff_dynamic {

/* create DarknetDetect class category */
struct DavWaveClassObjDetect : public DavWaveClassCategory {
    DavWaveClassObjDetect (const string & nameTag = "ObjDetect") :
        DavWaveClassCategory(type_index(typeid(*this)), type_index(typeid(std::string)), nameTag) {}
};

struct ObjDetectParams {
    string m_detectOrClassify;
    string m_detectorFrameworkTag;
    string m_modelPath;
    string m_configPath;
    string m_classnamePath;
    int m_backendId = 3;
    int m_targetId = 0;
    double m_scaleFactor;
    // make this an array
    // Scalar mean, BGR order
    double m_means[3];
    bool m_bSwapRb = false;
    int m_width = -1;
    int m_height = -1;
    double m_confThreshold = 0.7;
    int m_detectInterval = 1;
};

inline std::ostream & operator<<(std::ostream & os, const ObjDetectParams & p) {
    os << "[dectorOrClassify " << p.m_detectOrClassify << ", detectorFrameworkTag " << p.m_detectorFrameworkTag
       << ", modelPath " << p.m_modelPath << ", configPath " << p.m_configPath
       << ", classnamePath " << p.m_classnamePath << ", backendId " << p.m_backendId
       << ", targetId " << p.m_targetId << ", scaleFactor " << p.m_scaleFactor
       << ", means [" << p.m_means[0] << ", " << p.m_means[1] << ", " << p.m_means[2]
       << "], bSwapRb " << p.m_bSwapRb << ", width " << p.m_width
       << ", height " << p.m_height << ", confThreshold " << p.m_confThreshold
       << ", detectInterval" << p.m_detectInterval;
    return os;
}

} // namespace
