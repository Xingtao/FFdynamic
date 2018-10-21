#ifndef _MISC_UTIL_H_
#define _MISC_UTIL_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <pthread.h>
#include <iostream>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>

using::std::cout;
using::std::endl;
using::std::string;
using::std::vector;
using::std::map;

namespace Hv_Utils {
namespace Misc_Util {

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
///// Util Functions
extern void hvMisc_quickLogErr(const int ret);
extern bool hvMisc_validateIpAddrestStr(const char * ipStr);
extern int  hvMisc_getSockErrorNumber(const int sock);
extern int  hvMisc_setSockTimeout(const int sock, const int sec, const int usec);
extern int  hvMisc_setSockSendTimeout(const int sock, const int sec, const int usec);
extern int  hvMisc_setSockReuse(const int sock);
extern int  hvMisc_setSockKeepAlive(const int sock);
extern int  hvMisc_setBlockSocket (const int sfd);
extern int  hvMisc_setNonblockSocket (const int sfd);
extern bool hvMisc_isNonblockSocket (const int sfd);
extern bool hvMisc_isFileExisting(char const * fileName);
extern bool hvMisc_isDirWritable(char const * dirpath);
extern bool hvMisc_isDirExisting(char const * dirpath);
extern char*hvMisc_skipLeadingSpaces(const char * input);
extern string hvMisc_trimSpace(const string & str); // skip leading & cut off trailings
extern int hvMisc_removeDoubleSlashOfUrl(string & url);
extern bool hvMisc_floatEqual(const double a, const double b, const double epsilon);

extern string hvMisc_getCurrentTimeStr();
extern string hvMisc_intToString(const int n);
extern string hvMisc_pointerToString(const void *p);
extern std::vector<string> hvMisc_splitStrByDelimiter(char * str, const char * delim);

extern bool hvMisc_isFdValid(int fd);
extern bool hvMisc_isFpValid(FILE* pFp);

extern string hvMisc_getADeviceValidIp();
extern std::map<string, string> hvMisc_getDeviceValidIps();

extern int hvMisc_getHomeDir(string & homeDir);
extern string hvMisc_backtrace();
extern int hvMisc_dumpbytes(const unsigned char *pData, const int size);

//// the water mark buffer
extern void hvMisc_frameWaterMarking(const unsigned char * pSrcY,
                                     const int width, const int height, const int stride,
                                     const int top, const int left);

extern string hvMisc_getPathWithoutExtension(const string & path);
extern string hvMisc_getFileNameFromPath(const string & path);
extern string hvMisc_getDirFromPath(const string & path);

extern string hvMisc_getBodyFromUrl(const string & url);
extern bool hvMisc_isAvcKeyFrame(const unsigned char *bs, const int ns);
extern bool hvMisc_isHevcKeyFrame(const unsigned char *bs, const int ns);

extern double hvMisc_tsToTime(const double ts, 
                              const int timebaseNum, const int timebaseDen);
extern string hvMisc_getNumFromStr(string & logtag);

// live streaming from url check
extern bool hvMisc_isStreamingOf(const string & inputUrl, const string & proto);
extern bool hvMisc_isLiveStreamingUrl(const string & url);

extern int hvMisc_generateAudioBar(unsigned char *yuv, int width, int height, 
                                   double volumeLeft, double volumeRight);
// 
extern bool hvMisc_any(const vector<bool> & vec);
extern bool hvMisc_all(const vector<bool> & vec);

template<typename T> 
void hvMisc_dumpVectorElemToStd(const vector<T> & v)
{
    for (unsigned int k=0; k < v.size(); k++)
        cout << v[k] << " ";
    cout << endl;
}

template<typename T> 
string hvMisc_vectorDumpToString(const vector<T> & v)
{
    std::ostringstream info;
    for (unsigned int k=0; k < v.size(); k++)
        info << v[k] << ", ";
    return info.str();
}

template<typename T> 
bool hvMisc_isElemIn(const vector<T> & v, const T & e)
{
    for (unsigned int k=0; k < v.size(); k++)
        if (v[k] == e)
            return true;
    return false;
}

template<typename T> 
void hvMisc_deleteElemInVec(vector<T> & v, const T & e)
{
    v.erase(std::remove(v.begin(), v.end(), e), v.end());
    return;
}

} // namespace
} // namespace


#endif // _MISC_UTIL_H
