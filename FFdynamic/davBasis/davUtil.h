#pragma once

#include <map>
#include <vector>
#include <string>
#include <limits>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <typeinfo>
#include <type_traits>

#include "davDynamicEvent.h"
#include "ffmpegHeaders.h"

namespace ff_dynamic {
using ::std::map;
using ::std::pair;
using ::std::vector;
using ::std::string;
using ::std::istream;
using ::std::ostream;

/////////////////////////
/* AVRational settings */
inline std::ostream & operator<<(std::ostream & os, const AVRational & r) {
    os << "{'num':" << r.num << ", 'den': '" << r.den << "'}";
    return os;
}
inline bool operator==(const AVRational & l, const AVRational & r) {
    return l.num == r.num && l.den == r.den;
}
inline bool operator!=(const AVRational & l, const AVRational & r) {
    return !(l == r);
}
inline bool operator<(const AVRational & l, const AVRational & r) {
    return l.num * 1.0 / l.den + std::numeric_limits<double>::epsilon() < l.den * 1.0 / r.den;
}

/* logtag */
inline string mkLogTag(const string & logtag) {
    return "[" + logtag + "] ";
}
inline string mkLogTag(const string & logtag1, const string & logtag2) {
    return "[" + logtag1 + "-" + logtag2 + "] ";
}

inline string appendLogTag(const string & logtag, const string & suffix) {
    return logtag.substr(0, logtag.find("]")) + suffix + "] ";
}

////////////////////
/* ostream relate */
inline string ostreamToString(std::ostream & ostr) {
    std::ostringstream ss;
    ss << ostr.rdbuf();
    return ss.str();
}

template<typename T>
string toStringViaOss(const T & t) {
    std::ostringstream ss;
    ss << t;
    return ss.str();
}

template<typename T>
string vectorToStringViaOss(const vector<T> & t) {
    std::ostringstream ss;
    for (auto & e : t)
        ss << e << " ";
    ss << "\n";
    return ss.str();
}

//////////////////////
/* enum to string */
template<typename T>
struct EnumString {static const map<T, string> s_enumStringMap;};

template<typename T, typename std::enable_if<std::is_enum<T>::value>::type* = nullptr>
ostream & operator<<(ostream & os, const T & t) {
    return os << EnumString<T>::s_enumStringMap.at(t);
}

template<typename T, typename std::enable_if<std::is_enum<T>::value>::type* = nullptr>
string & operator<<(string & str, const T & t) {
    str = EnumString<T>::s_enumStringMap.at(t);
    return str;
}

/* string to enum */
template<typename T, typename std::enable_if<std::is_enum<T>::value>::type* = nullptr>
void operator>>(const string & str, T & t) {
    static auto begin = EnumString<T>::s_enumStringMap.begin();
    static auto end = EnumString<T>::s_enumStringMap.end();
    auto it = std::find_if(begin, end, [str](const pair<T, string> & e){return e.second == str;});
    if (it != end)
        t = it->first;
}

template<typename T, typename std::enable_if<std::is_enum<T>::value>::type* = nullptr>
istream & operator>>(istream & val, T & t) {
    string str;
    val >> str;
    str >> t;
    return val;
}

//////////////////////////////////////////////////////////////////////////////////////////

/* not used right now */
enum class EVideoParams {
    eVideoMinWidth = 256,
    eVideoMaxWidth = 4096,
    eVideoMinHeight = 256,
    eVideoMaxHeight = 2160,
    eVideoMinBitrate = 100 * 1024,
    eVideoMaxBitrate = 256*1024*1024, // 256Mbps
    eVideoMinFps = 4,
    eVideoMaxFps = 256,
};

// misc helpers
enum class ETimeUs : int {
    e1ms = 1000, e2ms = 2000, e5ms = 5000,
    e10ms = 10000, e20ms = 20000, e50ms = 50000,
    e100ms = 10000, e200ms = 200000, e500ms = 500000,
    e1s = 1000000, e2s = 2000000, e5s = 5000000
};

//////////////////////
extern bool all (const vector<bool> & bs);
extern bool any (const vector<bool> & bs);
extern string trimStr(const string & str, const string & whitespace = " \t");

} // namespace ff_dynamic
