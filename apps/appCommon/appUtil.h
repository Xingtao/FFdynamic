#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <streambuf>

#include <glog/logging.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>

/* to avoid introduce more dependency, read yuv file directly (used as video mix backgroup) */
#include "davProcBuf.h"
#include "ffmpegHeaders.h"

namespace app_common {
using ::std::string;
using ::std::vector;
using ::std::shared_ptr;

class AppUtil {
public:
    static int readFileContent(const string & filePath, string & content) {
        std::ifstream file(filePath);
        if (file) {
            file.seekg(0, std::ios::end);
            content.reserve(file.tellg());
            file.seekg(0, std::ios::beg);
            content.assign((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        } else
            return -1;
        file.close();
        return 0;
    }

    template<typename T>
    static vector<T> pbRepeatedToVector(const google::protobuf::RepeatedPtrField<T> & repeatField) {
        vector<T> v;
        for (auto rf : repeatField)
            v.emplace_back(*rf);
        return v;
    }
};

} // namespace app_common
