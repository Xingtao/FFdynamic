#ifndef _FFBASED_UTILS_H_
#define _FFBASED_UTILS_H_

#ifdef __cplusplus
extern "C"
{
    #include "ffheaders.h"
}
#endif

#include <string>

#include <glog/logging.h>
using::std::string;

extern string hv_av_err2str(const int errNum);
extern int initBitStreamFilter(AVBSFContext **avbsfContext, AVStream *avStream, 
                               const char *filterName, const string & logtag = "ffUtil");
extern int processVideoBitstreamFilter(AVBSFContext *avbsfContext, AVPacket & inPkt, 
                                       string & data, const string & logtag = "ffUtil");
#endif //
