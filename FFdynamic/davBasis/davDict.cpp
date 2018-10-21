#include "davDict.h"

namespace ff_dynamic {

//////////////////////////////////////////////////////////////////////////////////////////
int DavDict::getAVRational(const string & key, AVRational & r) const {
    // av_dict_set(opts, "time_base", "%d/%d", 0)
    const char *rationalStr = flagGetInternal(key, AV_DICT_MATCH_CASE);
    if (!rationalStr)
        return DAV_ERROR_DICT_NO_SUCH_KEY;
    const string val(rationalStr);
    try { // to int
        string delimiter = "/";
        string numStr = val.substr(0, val.find(delimiter));
        string denStr = val.substr(val.find(delimiter) + delimiter.length());
        int num = std::stoi(numStr);
        int den = std::stoi(denStr);
        r = {num, den};
    }
    catch (std::invalid_argument &) {
        return DAV_ERROR_DICT_VAL_INVALID;
    }
    catch (std::out_of_range &) {
        return DAV_ERROR_DICT_VAL_OUT_RANGE;
    }
    catch (...) {
        return DAV_ERROR_DICT_VAL_UNKNOWN;
    }
    return 0;
}

int DavDict::getVideoSize(int & width, int & height) const {
    // av_dict_set(opts, "video_size", "720x576")
    const char *videoSizeStr = flagGetInternal("video_size", AV_DICT_MATCH_CASE);
    if (!videoSizeStr)
        return DAV_ERROR_DICT_NO_SUCH_KEY;
    const string val(videoSizeStr);
    try { // to int
        string delimiter = "x";
        string widthStr = val.substr(0, val.find(delimiter));
        string heightStr = val.substr(val.find(delimiter) + delimiter.length());
        width = std::stoi(widthStr);
        height = std::stoi(heightStr);
    }
    catch (std::invalid_argument &) {
        return DAV_ERROR_DICT_VAL_INVALID;
    }
    catch (std::out_of_range &) {
        return DAV_ERROR_DICT_VAL_OUT_RANGE;
    }
    catch (...) {
        return DAV_ERROR_DICT_VAL_UNKNOWN;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////
string DavDict::dump() const {
    string dumpStr(" options : {");
    for (auto & c : m_categoryOptions)
        dumpStr += c.first.name() + " : " + c.second.name() + ", ";
    for (auto & o : m_davOptions) {
        dumpStr += o.first.name() + ":" + o.second + ", ";
    }
    dumpStr += dumpAVDict();
    dumpStr += "}";
    return dumpStr;
}
string DavDict::dumpAVDict() const {
    string dict;
    char *buf = nullptr;
    av_dict_get_string(m_d, &buf, ':', ',');
    if (buf)
        dict = buf;
    av_freep(&buf);
    return dict;
}

int DavDict::getIntInternal(const char *retVal, int & val, const int minV, const int maxV) const {
    try { // to int
        val = std::stoi(retVal);
    }
    catch (std::invalid_argument &) {
        return DAV_ERROR_DICT_VAL_INVALID;
    }
    catch (std::out_of_range &) {
        return DAV_ERROR_DICT_VAL_OUT_RANGE;
    }
    catch (...) {
        return DAV_ERROR_DICT_VAL_UNKNOWN;
    }
    if (val > maxV || val < minV)
        return DAV_ERROR_DICT_VAL_OUT_RANGE;
    return 0;
}

int DavDict::getDoubleInternal(const char* retVal, double & val,
                               const double minV, const double maxV) const noexcept {
    try {
        val = std::stod(retVal);
    }
    catch (std::invalid_argument &) {
        return DAV_ERROR_DICT_VAL_INVALID;
    }
    catch (std::out_of_range &) {
        return DAV_ERROR_DICT_VAL_OUT_RANGE;
    }
    catch (...) {
        return DAV_ERROR_DICT_VAL_UNKNOWN;
    }
    if (val > maxV || val < minV)
        return DAV_ERROR_DICT_VAL_OUT_RANGE;
    return 0;
}

int DavDict::getBoolInternal(const char* retVal, bool & val) const noexcept {
    size_t retValLen = strlen(retVal);
    if (strncmp(retVal, "true", retValLen) == 0) {
        val = true;
    } else if (strncmp(retVal, "false", retValLen) == 0) {
        val = false;
    } else {
        return DAV_ERROR_DICT_VAL_INVALID;
    }
    return 0;
}

} // namespace ff_dynamic
