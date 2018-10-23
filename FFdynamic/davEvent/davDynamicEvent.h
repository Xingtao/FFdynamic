#pragma once

#include <vector>
#include <string>

namespace ff_dynamic {
using ::std::vector;
using ::std::string;

/* Dynamic event process
  Dynamic Events are runtime requests get from external, normally through http;
  The Events defined here will be passed to FFdynamic's components and change their behavior on the fly;

  Application level options passing.
  Normally, static starting configurations and dynamic request get from external, is usually using Json or Protobuf;
  Then we needs convert those formats to following FFdynamic's 'static option' and 'dynamic event'.

  For example, the Ial app (ship with the FFdynamic) uses json as http payload and configure file,
  it first  auto transform those json to Protos; then, for 'static option', there is a small module called
  'pbToDavWaveOption' do the translation works; and for 'dynamic event', it is done in each request.

  Why not all use protobuf ?
  For 'static options', no, for the underling audio/video process are ffmpeg's libraries, then use AVDictionary.
  For 'dynamic event', we could define all 'dynamic event' in protobuf format, then no convertion is required.
  But this will introdue Protobuf3 dependency. It is not needed and friendly (need manually
  compile for most system) for some users
*/

/* To get rid of protobuf3 dependency for FFdynamic,
   few structures are defined bothe in here and application level proto files */
/* used for dynamic layout change */
enum class EDavVideoMixLayout {
    eLayoutUnknown = -1,    eLayoutAuto = 0,        eSingle_1 = 1, eHorizonal_2 = 2,
    eLeft1SmallRight1Big_2, eLeft2SmallRight1Big_3, eEqual_4,      eLeft1BigRight3Small_4,
    eRow2Col3_6,            eEqual_9,               eRow3Col4_12,  eEqual_16,
    eEqual_25,              eEqual_36,              eLayoutSpecific
};

struct DavVideoCellCoordinate {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int layer = -1;
};

struct DavDynaEventVideoMixLayoutUpdate {
    EDavVideoMixLayout m_layout = EDavVideoMixLayout::eLayoutAuto;
    /* besides fixed layout, could set layout by explicitly set each cell or some cells' position */
    vector<DavVideoCellCoordinate> m_cells;
};

struct DavDynaEventAudioMixMuteUnmute {
    vector<size_t> m_muteGroupIds;
    vector<size_t> m_unmuteGroupIds;
};

struct DavDynaEventVideoMixSetNewBackgroud {
    string m_backgroudUrl;
};

struct DavDynaEventVideoKeyFrameRequest {
    bool m_bForceIdr;
};

} // namespace ff_dynamic
