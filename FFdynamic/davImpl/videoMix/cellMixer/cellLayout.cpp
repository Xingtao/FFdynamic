#include "cellLayout.h"

namespace ff_dynamic {

static constexpr auto eLayoutUnknown = EDavVideoMixLayout::eLayoutUnknown;
static constexpr auto eLayoutAuto = EDavVideoMixLayout::eLayoutAuto;
static constexpr auto eSingle_1 = EDavVideoMixLayout::eSingle_1;
static constexpr auto eHorizonal_2 = EDavVideoMixLayout::eHorizonal_2;
static constexpr auto eLeft1SmallRight1Big_2 = EDavVideoMixLayout::eLeft1SmallRight1Big_2;
static constexpr auto eLeft2SmallRight1Big_3 = EDavVideoMixLayout::eLeft2SmallRight1Big_3;
static constexpr auto eEqual_4 = EDavVideoMixLayout::eEqual_4;
static constexpr auto eLeft1BigRight3Small_4 = EDavVideoMixLayout::eLeft1BigRight3Small_4;
static constexpr auto eRow2Col3_6 = EDavVideoMixLayout::eRow2Col3_6;
static constexpr auto eEqual_9 = EDavVideoMixLayout::eEqual_9;
static constexpr auto eRow3Col4_12 = EDavVideoMixLayout::eRow3Col4_12;
static constexpr auto eEqual_16 = EDavVideoMixLayout::eEqual_16;
static constexpr auto eEqual_25 = EDavVideoMixLayout::eEqual_25;
static constexpr auto eEqual_36 = EDavVideoMixLayout::eEqual_36;
static constexpr auto eLayoutSpecific = EDavVideoMixLayout::eLayoutSpecific;

template <> const map<EDavVideoMixLayout, string> EnumString<EDavVideoMixLayout>::s_enumStringMap = {
    {eLayoutAuto,            "autoLayout"},
    {eLayoutUnknown,         "unknownLayout"},
    {eSingle_1,              "single_1"},
    {eHorizonal_2,           "horizonal_2"},
    {eLeft1SmallRight1Big_2, "left1SmallRight1Big_2"},
    {eLeft2SmallRight1Big_3, "eLeft2SmallRight1Big_3"},
    {eEqual_4,               "equal4_4"},
    {eLeft1BigRight3Small_4, "left1BigRight3Small_4"},
    {eRow2Col3_6,            "row2Col3_6"},
    {eEqual_9,               "equal_9"},
    {eRow3Col4_12,           "row3Col4_12"},
    {eEqual_16,              "equal_16"},
    {eEqual_25,              "eEqual_25"},
    {eEqual_36,              "eEqual_36"},
    {eLayoutSpecific,        "eLayoutSpecific"}
};

const map<EDavVideoMixLayout, int> CellLayout::s_layoutToCellNumMap = {
    {eLayoutAuto,0},
    {eSingle_1, 1},
    {eHorizonal_2, 2},
    {eLeft1SmallRight1Big_2, 2},
    {eLeft2SmallRight1Big_3, 3},
    {eEqual_4, 4},
    {eLeft1BigRight3Small_4, 4},
    {eRow2Col3_6, 6},
    {eEqual_9, 9},
    {eRow3Col4_12, 12},
    {eEqual_16, 16},
    {eEqual_25, 25},
    {eEqual_36, 36}
};

const map<int, EDavVideoMixLayout> CellLayout::s_cellNumToLayoutMap = {
    {0, eLayoutAuto}, {1, eSingle_1},   {2, eHorizonal_2}, {3, eLeft2SmallRight1Big_3},
    {4, eEqual_4},    {5, eRow2Col3_6}, {6, eRow2Col3_6},  {7, eEqual_9},
    {8, eEqual_9},    {9, eEqual_9},    {10,eEqual_16},    {11,eEqual_16},
    {12,eEqual_16},   {13,eEqual_16},   {14,eEqual_16},    {15,eEqual_16},
    {16,eEqual_16},   {17,eEqual_25},   {18,eEqual_25},    {19,eEqual_25},
    {20,eEqual_25},   {21,eEqual_25},   {22,eEqual_25},    {23,eEqual_25},
    {24,eEqual_25},   {25,eEqual_25},
    {26,eEqual_36},   {27,eEqual_36},   {28,eEqual_36},    {29,eEqual_36},
    {30,eEqual_36},   {31,eEqual_36},   {32,eEqual_36},    {33,eEqual_36},
    {34,eEqual_36},   {35,eEqual_36},   {36,eEqual_36}
};

/* layout pattern idx as key; value: Xs, Yx, Ws, Hs for each cell
    eLayoutAuto = 0;            eLayoutUnknown = -1;
    eSingle_1 = 1;              eHorizonal_2 = 2;
    eLeft1SmallRight1Big_2 = 3; eLeft2SmallRight1Big_3 = 4;
    eEqual_4 = 5;               eLeft1BigRight3Small_4 = 6;
    eRow2Col3_6 = 7;            eEqual_9 = 8;
    eRow3Col4_12 = 9;           eEqual_16 = 10;
    eEqual_25 = 11; eEqual_36 = 12; eLayoutSpecific = 13;
*/
const map<EDavVideoMixLayout, vector<vector<int>>> CellLayout::s_layoutCoordinates = {
    // {0, {}},
    {eSingle_1,              {{0},{0},{120},{120}}},
    {eHorizonal_2,           {{0,60},{30,30},{60,60},{60,60}}},
    {eLeft1SmallRight1Big_2, {{0,24},{0,0},{24,96},{24,120}}},
    {eLeft2SmallRight1Big_3, {{0,40,0},{20,20,60},{40,80,40},{40,80,40}}},
    {eEqual_4,               {{0,60,0,60},{0,0,60,60},{60,60,60,60},{60,60,60,60}}},
    {eLeft1BigRight3Small_4, {{0,80,80,80},{20,0,40,80},{80,40,40,40},{80,40,40,40}}},

    {eRow2Col3_6,  {{0,40,80,0,40,80},{0,0,0,60,60,60},{40,40,40,40,40,40},{60,60,60,60,60,60}}},
    {eEqual_9,     {{0,40,80,0,40,80,0,40,80},{0,0,0,40,40,40,80,80,80},
                   {40,40,40,40,40,40,40,40,40},{40,40,40,40,40,40,40,40,40}}},

    {eRow3Col4_12, {{0,30,60,90,0,30,60,90,0,30,60,90},{0,0,0,0,40,40,40,40,80,80,80,80},
                   {30,30,30,30,30,30,30,30,30,30,30,30},{40,40,40,40,40,40,40,40,40,40,40,40}}},

    {eEqual_16,    {{0,30,60,90,0,30,60,90,0,30,60,90,0,30,60,90},{0,0,0,0,30,30,30,30,60,60,60,60,90,90,90,90},
                    {30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30},
                    {30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30}}},

    {eEqual_25, {{0,24,48,72,96,0,24,48,72,96,0,24,48,72,96,0,24,48,72,96,0,24,48,72,96},
                 {0,0,0,0,0,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24},
                 {24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24},
                 {24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24}}},

    {eEqual_36, {{0,20,40,60,80,100,0,20,40,60,80,100,0,20,40,60,80,100,
                  0,20,40,60,80,100,0,20,40,60,80,100,0,20,40,60,80,100},
                 {0,0,0,0,0,0,20,20,20,20,20,20,40,40,40,40,40,40,
                  60,60,60,60,60,60,80,80,80,80,80,80,100,100,100,100,100,100},
                 {20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
                  20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20},
                 {20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
                  20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20}}}
};

} // namespace ff_dynamic
