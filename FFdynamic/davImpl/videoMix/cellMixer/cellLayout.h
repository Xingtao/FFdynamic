#pragma once

#include <map>
#include <string>
#include <vector>

#include <glog/logging.h>
#include "ffmpegHeaders.h"
#include "davUtil.h"
#include "davDynamicEvent.h"

namespace ff_dynamic {
using ::std::map;
using ::std::string;
using ::std::vector;

///////////////////
template <> const map<EDavVideoMixLayout, string> EnumString<EDavVideoMixLayout>::s_enumStringMap;

class CellLayout {
public:
    static const string getLayoutTypeString(EDavVideoMixLayout layoutType) noexcept {
        return toStringViaOss(layoutType);
    }
    static const string getLayoutTypeStringByIdx(const int idx) {
        return toStringViaOss(static_cast<EDavVideoMixLayout>(idx));
    };
    static EDavVideoMixLayout getAutoLayoutViaCellNum(const size_t cellNum) noexcept {
        if (s_cellNumToLayoutMap.count(cellNum) == 0)
            return EDavVideoMixLayout::eLayoutUnknown;
        return s_cellNumToLayoutMap.at(cellNum);
    }
    static int getCellNumViaLayout(const EDavVideoMixLayout layout) {
        CHECK(s_layoutToCellNumMap.count(layout) > 0) << " layout corresponding cell num not found";
        return s_layoutToCellNumMap.at(layout);
    }
    static vector<int> getCoordinateOfLayoutAtPos(EDavVideoMixLayout layout,
                                                  const int pos) noexcept {
        if (pos >= s_layoutToCellNumMap.at(layout))
            return {}; /* an empty one tells this pos doesn't have valid coors */
        vector<int> coor;
        const vector<vector<int>> & coors = s_layoutCoordinates.at(layout);
        for (auto & c : coors)
            coor.push_back(c[pos]);
        return coor;
    }

    /* static members */
    static const map<int, EDavVideoMixLayout> s_cellNumToLayoutMap;
    static const map<EDavVideoMixLayout, int> s_layoutToCellNumMap;
    /* layout pattern idx as key; value: Xs, Yx, Ws, Hs for each cell */
    static const map<EDavVideoMixLayout, vector<vector<int>>> s_layoutCoordinates;
};


} // namespace ff_dynamic
