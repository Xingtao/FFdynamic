#pragma once

#include <limits>
#include <cstring>
#include <sstream>
#include <utility>
#include <functional>
#include <typeinfo>
#include <typeindex>

#include "glog/logging.h"
#include "ffmpegHeaders.h"
#include "davUtil.h"
#include "davOption.h"
#include "davMessager.h"

namespace ff_dynamic {
using ::std::type_index;

//////////////////////////////////////////////////////////////////////////////////////////
// Wrap and extend AVDictionary
class DavDict {
public:
    DavDict() = default;
    DavDict(const DavWaveClassCategory & cat, const string & implType = "auto") noexcept {
        int ret = setCategory(DavOptionClassCategory(), cat);
        LOG_IF(ERROR, ret < 0) << "cannot set class category " << davMsg2str(ret);
        ret = set(DavOptionImplType(), implType);
        LOG_IF(ERROR, ret < 0) << "cannot set impl type" << davMsg2str(ret);
    }
    explicit DavDict(AVDictionary *d) noexcept {m_d = d;}
    // deep copy
    DavDict(const DavDict & r) {deepCopy(r);}
    DavDict & operator=(const DavDict & r) {deepCopy(r); return *this;}
    virtual ~DavDict() {if (m_d) av_dict_free(&m_d);}

public:
    // getters
    inline AVDictionary **get() {return &m_d;}
    inline const AVDictionary *get() const {return m_d;}
    inline string get(const string & key, const int flag = AV_DICT_MATCH_CASE) const {
        const char *val = flagGetInternal(key, flag);
        return (val ? val : "");
    }
    inline string get(const string & key, const string & defaultVal,
                      const int flag = AV_DICT_MATCH_CASE) const {
        const char *val = flagGetInternal(key, flag);
        return (val ? val : defaultVal);
    }
    int getInt(const string & key, int & val, const int flag = AV_DICT_MATCH_CASE,
               const int minV = std::numeric_limits<int>::min(),
               const int maxV = std::numeric_limits<int>::max()) const {
        const char *retVal = flagGetInternal(key, flag);
        if (!retVal)
            return DAV_ERROR_DICT_NO_SUCH_KEY;
        return getIntInternal(retVal, val, minV, maxV);
    }
    int getDouble(const string & key, double & val, const int flag = AV_DICT_MATCH_CASE,
                  const double minV = std::numeric_limits<double>::min(),
                  const double maxV = std::numeric_limits<double>::max()) const {
        const char *retVal = flagGetInternal(key, flag);
        if (!retVal)
            return DAV_ERROR_DICT_NO_SUCH_KEY;
        return getDoubleInternal(retVal, val, minV, maxV);
    }
    int getBool(const string & key, bool & val, const int flag = AV_DICT_MATCH_CASE) const noexcept {
        const char *retVal = flagGetInternal(key, flag);
        if (!retVal)
            return DAV_ERROR_DICT_NO_SUCH_KEY;
        return getBoolInternal(retVal, val);
    }
    int getAVRational(const string & key, AVRational & r) const;
    int getVideoSize(int & width, int & height) const;

    ///////////////////
    /* DavOption Get */
    inline string get(const DavOption & davOpt) const {
        return m_davOptions.count(davOpt) == 0 ? "" : m_davOptions.at(davOpt);
    }
    inline string get(const DavOption & davOpt, const string & defaultVal) const {
        return m_davOptions.count(davOpt) == 0 ? defaultVal : m_davOptions.at(davOpt);
    }
    int getBool(const DavOption & davOpt, bool & val) const noexcept {
        if (m_davOptions.count(davOpt) == 0)
            return DAV_ERROR_DICT_NO_SUCH_KEY;
        const string retVal(m_davOptions.at(davOpt));
        return getBoolInternal(retVal.c_str(), val);
    }
    int getInt(const DavOption & davOpt, int & val,
               const int minV = std::numeric_limits<int>::min(),
               const int maxV = std::numeric_limits<int>::max()) const {
        const char *retVal = m_davOptions.count(davOpt) == 0 ? nullptr : m_davOptions.at(davOpt).c_str();
        if (!retVal)
            return DAV_ERROR_DICT_NO_SUCH_KEY;
        return getIntInternal(retVal, val, minV, maxV);
    }
    inline int getDouble(const DavOption & davOpt, double & val,
                         const double minV = std::numeric_limits<double>::min(),
                         const double maxV = std::numeric_limits<double>::max()) const {
        const char *retVal = m_davOptions.count(davOpt) == 0 ? nullptr : m_davOptions.at(davOpt).c_str();
        if (!retVal)
            return DAV_ERROR_DICT_NO_SUCH_KEY;
        return getDoubleInternal(retVal, val, minV, maxV);
    }
    inline int getCategory(const DavOption & o, DavOption & c) const {
        if (!m_categoryOptions.count(o))
            return DAV_ERROR_DICT_KEY_EXIST;
        if (o.valueType() != type_index(typeid(c)))
            return DAV_ERROR_DICT_TYPE_MISMATCH;
        c = m_categoryOptions.at(o);
        return 0;
    }
    /////////////////////////
    /* DavOption Array Get */
    inline int getIntArray(const string & o, vector<int> & a) const {
        if (!m_intArrayOptions.count(o))
            return DAV_ERROR_DICT_KEY_EXIST;
        a = m_intArrayOptions.at(o);
        return 0;
    }
    inline int getDoubleArray(const string & o, vector<double> & a) const {
        if (!m_doubleArrayOptions.count(o))
            return DAV_ERROR_DICT_KEY_EXIST;
        a = m_doubleArrayOptions.at(o);
        return 0;
    }

public:
    /* setters */
    inline int set(const string & key, const string & val, const int flag = AV_DICT_DONT_OVERWRITE) {
        return av_dict_set(&m_d, key.c_str(), val.c_str(), flag);
    }
    inline int set(const char *key, const char *val, const int flag = AV_DICT_DONT_OVERWRITE) {
        return av_dict_set(&m_d, key, val, flag);
    }
    inline int setInt(const string & key, const int val, const int flag = AV_DICT_DONT_OVERWRITE) {
        return av_dict_set(&m_d, key.c_str(), std::to_string(val).c_str(), flag);
    }
    inline int setDouble(const string & key, const double val, const int flag = AV_DICT_DONT_OVERWRITE) {
        return av_dict_set(&m_d, key.c_str(), std::to_string(val).c_str(), flag);
    }
   inline int setBool(const string & key, bool val, const int flag = AV_DICT_DONT_OVERWRITE) {
        return av_dict_set(&m_d, key.c_str(), val ? "true" : "false", flag);
    }
    inline int setVideoSize(const int width, const int height, const int flag = AV_DICT_DONT_OVERWRITE) {
        string w = std::to_string(width);
        string h = std::to_string(height);
        return av_dict_set(&m_d, "video_size", (w + "x" + h).c_str(), flag);
    }
    inline int setAVRational(const string & key, const AVRational & r,  int flag = AV_DICT_DONT_OVERWRITE) {
        string num = std::to_string(r.num);
        string den = std::to_string(r.den);
        return av_dict_set(&m_d, key.c_str(), (num + "/" + den).c_str(), flag);
    }
    //////////////////////
    /* DavOption setter */
    inline int set(const DavOption & o, const string & val) {
        if (m_davOptions.count(o))
            return DAV_ERROR_DICT_KEY_EXIST;
        m_davOptions.emplace(o, val);
        return 0;
    }
    inline int setInt(const DavOption & o, const int val) {
        if (m_davOptions.count(o))
            return DAV_ERROR_DICT_KEY_EXIST;
        m_davOptions.emplace(o, std::to_string(val));
        return 0;
    }
    inline int setDouble(const DavOption & o, const double val) {
        if (m_davOptions.count(o))
            return DAV_ERROR_DICT_KEY_EXIST;
        m_davOptions.emplace(o, std::to_string(val));
        return 0;
    }
    inline int setBool(const DavOption & o, const bool val) {
        if (m_davOptions.count(o))
            return DAV_ERROR_DICT_KEY_EXIST;
        m_davOptions.emplace(o, val ? "true" : "false");
        return 0;
    }
    inline int setCategory(const DavOption & o, const DavOption & v) {
        if (m_categoryOptions.count(o))
            return DAV_ERROR_DICT_KEY_EXIST;
        if (o.valueType() != type_index(typeid(DavOption)))
            return DAV_ERROR_DICT_TYPE_MISMATCH;
        m_categoryOptions.emplace(o, v);
        return 0;
    }
    /* array settings */
    inline int setIntArray(const string & key, const vector<int> & a) {
        if (m_intArrayOptions.count(key))
            return DAV_ERROR_DICT_KEY_EXIST;
        m_intArrayOptions.emplace(key, a);
        return 0;
    }
    inline int setDoubleArray(const string & key, const vector<double> & a) {
        if (m_doubleArrayOptions.count(key))
            return DAV_ERROR_DICT_KEY_EXIST;
        m_doubleArrayOptions.emplace(key, a);
        return 0;
    }

    /* delete option */
    inline int erase(const DavOption & o) {
        if (m_davOptions.count(o))
            m_davOptions.erase(o);
        if (m_categoryOptions.count(o))
            m_categoryOptions.erase(o);
        return 0;
    }

////////////////////////////////////////////////////////////////////////////////////////////
public:
    inline const map<DavOption, string> & getDavOptions() const {return m_davOptions;}
    inline const map<DavOption, DavOption> & getCategoryOptions() const {return m_categoryOptions;}
    inline const map<string, vector<int>> & getIntArrayOptions() const {return m_intArrayOptions;}
    inline const map<string, vector<double>> & getDoubleArrayOptions() const {return m_doubleArrayOptions;}

    inline bool isDavOptionsEmpty() const {return m_davOptions.size() == 0;}
    inline bool isCategoryOptionsEmpty() const {return m_categoryOptions.size() == 0;}
    string dump() const;
    string dumpAVDict() const;

private:
    // helpers
    inline const char *flagGetInternal(const string & key, const int flag) const {
        AVDictionaryEntry *t = av_dict_get(m_d, key.c_str(), nullptr, flag);
        return (t ? t->value : nullptr);
    }
    int getIntInternal(const char *retVal, int & val, const int minV, const int maxV) const;
    int getDoubleInternal(const char* retVal, double & val, const double minV, const double maxV) const noexcept;
    int getBoolInternal(const char* retVal, bool & val) const noexcept;
    void deepCopy(const DavDict & r) {
        av_dict_copy(&m_d, r.get(), 0);
        m_categoryOptions = r.getCategoryOptions();
        m_davOptions = r.getDavOptions();
        m_intArrayOptions = r.getIntArrayOptions();
        m_doubleArrayOptions = r.getDoubleArrayOptions();
    }

private:
    AVDictionary *m_d = nullptr; /* ffmpeg's options passing structure */
    map<DavOption, string> m_davOptions; /* primitives: int double string */
    map<DavOption, DavOption> m_categoryOptions; /* DavOption derived class as category (enum) */
    map<string, vector<int>> m_intArrayOptions;
    map<string, vector<double>> m_doubleArrayOptions;
    std::mutex m_mutex; /* TODO: mutex needed ? Not right now */
};

using DavWaveOption = DavDict;

} // namespace ff_dynamic
