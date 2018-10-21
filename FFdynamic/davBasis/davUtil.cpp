#include "davUtil.h"

namespace ff_dynamic {
//////////////////////
bool all (const vector<bool> & bs) {
    if (bs.size() == 0)
        return false;
    for (auto b : bs)
        if (b == false)
            return false;
    return true;
}

bool any (const vector<bool> & bs) {
    if (bs.size() == 0)
        return false;
    for (auto b : bs)
        if (b == true)
            return true;
    return false;
}

//
string trimStr(const string & str, const string & whitespace) {
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

} // namespace ff_dynamic
