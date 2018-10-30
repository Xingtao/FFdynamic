#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional>

#include <glog/logging.h>
#include "ffmpegHeaders.h"
#include "davDict.h"

namespace ff_dynamic {
using ::std::map;
using ::std::pair;
using ::std::string;
using ::std::function;
using ::std::type_index;
using ::std::shared_ptr;
using ::std::unique_ptr;

class DavImpl;

class DavImplFactory {
public:
    using DavImplCreateFunc = function<unique_ptr<DavImpl>(const DavWaveOption &)>;
    using DavClassImplMap = map<string, DavImplCreateFunc>;
    using DavClassProcessDataTypesMap = map<DavWaveClassCategory, vector<DavDataType>>;

    static unique_ptr<DavImpl> create(const DavWaveOption & options, DavMsgError & createErr) {
        unique_ptr<DavImpl> davImpl;
        DavWaveClassCategory classCategory((DavWaveClassNotACategory()));
        options.getCategory(DavOptionClassCategory(), classCategory);
        if (classCategory == (DavWaveClassNotACategory())) {
            createErr.setInfo(DAV_ERROR_FACTORY_NO_CLASS_TYPE, " impl's class category not known");
            return davImpl; // call its move constructor. if no-elide-copy set, will call its copy constructor
        }
        const string classCategoryDesc = classCategory.name();
        const string implType = options.get(DavOptionImplType());
        if (implType.empty()) {
            createErr.setInfo(DAV_ERROR_FACTORY_NO_IMPL_TYPE, "impl type of " + classCategoryDesc + " empty");
            return davImpl;
        }

        // find the class that will be instantiated
        auto it = s_classImplMap.find(mkClassImplKey(classCategoryDesc, implType));
        if (it == s_classImplMap.end()) {
            createErr.setInfo(DAV_ERROR_FACTORY_NO_IMPL_TYPE,
                              "class category [" + classCategoryDesc + "] + impl [" + implType + "] not registered");
            return davImpl;
        }
        /* before creation, make sure FFmpeg is initilized*/
        static bool bFFmpegInited = false;
        if (!bFFmpegInited) {
            bFFmpegInited = true;
            #if (LIBAVFORMAT_VERSION_MAJOR < 58)
            av_register_all();
            avfilter_register_all();
            avformat_network_init();
            #endif
        }
        /* create implementation instance */
        try {
            davImpl = it->second(options);
        }
        catch (DavMsgError & err) {
            createErr = err;
            createErr.setInfo(DAV_ERROR_FACTORY_CREATE_IMPL,
                              "create impl [" + implType + "] of [" + classCategoryDesc + "] failed");
        }

        if (davImpl) { // log this event
            DavMessager & event = createErr;
            event.setInfo(DAV_INFO_IMPL_INSTANCE_CREATE_DONE,
                          "create impl [" + implType + "] of [" + classCategoryDesc + "] done");
        }
        return davImpl;
    }

    static inline string mkClassImplKey(const string & c, const string & i) {
        return c + "_" + implTypeToLower(i);
    }
    static inline string implTypeToLower(string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](char c) {return std::tolower(c);});
        return s;
    }

protected:
    static DavClassImplMap s_classImplMap;
    static DavClassProcessDataTypesMap s_classProcessDataTypeMap;
};

////////////////////////////////////////////////////////////////////////////////
// implementation register
class DavImplRegister : DavImplFactory {
public:
    DavImplRegister(const DavOption & classCategory, const string & implType,
                    DavImplCreateFunc func) {
        std::lock_guard<std::mutex> lock(s_mutex);
        m_implRegisterNames.emplace_back(addOneImplType(classCategory, implType, func));
    }
    DavImplRegister(const DavOption & classCategory, const vector<string> & implTypes,
                    DavImplCreateFunc func) {
        std::lock_guard<std::mutex> lock(s_mutex);
        for (const auto & implType : implTypes)
            m_implRegisterNames.emplace_back(addOneImplType(classCategory, implType, func));
    }
    string dumpRegisterNames() {
        string name;
        for (auto & v : m_implRegisterNames)
            name += v + ",";
        return name;
    }

private:
    static std::mutex s_mutex;
    static string addOneImplType(const DavOption & classCategory,
                                 const vector<DavDataType> & processDataTypes,
                          const string & implType, DavImplCreateFunc & func) {
        string registerKey = mkClassImplKey(classCategory.name(), implType);
        s_classImplMap.emplace(registerKey, func);
        s_classProcessDataTypeMap.emplace(classCategory, processDataTypes);
        return registerKey;
    }
    vector<string> m_implRegisterNames;
};

} // namespace ff_dynamic
