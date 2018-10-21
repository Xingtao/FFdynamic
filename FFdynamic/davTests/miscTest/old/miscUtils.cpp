#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#include <ifaddrs.h>
#include <pwd.h>
#include <ctype.h>
#include "miscUtil.h"
#include <glog/logging.h>

 ;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//// Util Functions
namespace Hv_Utils
{
namespace Misc_Util
{

void hvMisc_quickLogErr(const int ret)
{
    if (ret < 0)
        NLogE("MiscUtil", "QuickLogErr: For Just Indicating The Error.");
}

bool hvMisc_validateIpAddrestStr(const char * ipStr)
{
    char buf[sizeof(struct in_addr)];
    return inet_pton(AF_INET, ipStr, buf) == 1; // 0: invalid, 1:valid
}

int hvMisc_setSockTimeout(const int sock, const int sec, const int usec)
{
    struct timeval time;
    time.tv_sec  = sec; 
    time.tv_usec = usec;
    /// 0-On Success, -1-On Fail and errno is set.
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(struct timeval));
}

int hvMisc_setSockSendTimeout(const int sock, const int sec, const int usec)
{
    struct timeval time;
    time.tv_sec  = sec; 
    time.tv_usec = usec;
    /// 0-On Success, -1-On Fail and errno is set.
    return setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof(struct timeval));
}

int hvMisc_setSockReuse(const int sock)
{
    int reuse = 1;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
}

int hvMisc_setSockKeepAlive(const int sock)
{
    int ka = 1;
    return setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&ka, sizeof(ka));
}

int hvMisc_getSockErrorNumber(const int sock)
{
    int err = 0;
    socklen_t len = sizeof(int);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        return errno; // if cannot get SO_ERROR
    return err;
}

int hvMisc_setNonblockSocket (const int sfd)
{
    int flags, s;
    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror ("fcntl get error.");
        return -1;
    }
    flags |= O_NONBLOCK;
    s = fcntl (sfd, F_SETFL, flags);
    if (s == -1)
    {
        perror ("fcntl set error.");
        return -1;
    }
    return 0;
}

int hvMisc_setBlockSocket (const int sfd)
{
    int flags, s;
    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror ("fcntl get error.");
        return -1;
    }
    flags ^= O_NONBLOCK;
    s = fcntl (sfd, F_SETFL, flags);
    if (s == -1)
    {
        perror ("fcntl set error.");
        return -1;
    }
    return 0;
}

bool hvMisc_isNonblockSocket (const int sfd)
{
    int flags;
    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror ("fcntl get error.");
        return false;
    }
    return flags &= O_NONBLOCK ? true : false;
}


bool hvMisc_floatEqual(const double a, const double b, const double epsilon)
{
    return (fabs(a - b) < epsilon);
}

std::string hvMisc_getCurrentTimeStr()
{   /// YYYY-MM-DD-HH-MM-SS
    time_t timeNow = time(NULL);
    struct tm tm = * localtime(&timeNow);
    char timeStr[200] = {0};
    snprintf(timeStr, sizeof(timeStr), "%d_%d_%d_%d_%d_%d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    std::string formatTimeStr(timeStr);
    return formatTimeStr;
}

std :: string hvMisc_intToString(const int n)
{
    char nameStr[20] = {0};
    snprintf(nameStr, sizeof(nameStr), "%d", n);
    std::string formatedStr(nameStr);
    return formatedStr; 
}

std :: string hvMisc_pointerToString(const void *p)
{
    char nameStr[20] = {0};
    snprintf(nameStr, sizeof(nameStr), "%p", p);
    std::string formatedStr(nameStr);
    return formatedStr; 
}

//||
std::vector<std::string> hvMisc_splitStrByDelimiter(char * str, const char * delim)
{
    char * token = strtok(str, delim);
    std::vector<std::string> result;
    while(token != NULL)
    {
        result.push_back(token);
        token = strtok(NULL, delim);
    }
    return result;
}

bool hvMisc_isFileExisting(char const * fileName)
{
    struct stat sb;
    if (-1 == stat(fileName, &sb))
        return false;
    
    return S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode);
}

bool hvMisc_isDirWritable(char const * filepath)
{
    if (access(filepath, W_OK) == 0)
        return true;
    return false;
}

bool hvMisc_isDirExisting(char const * dirpath)
{   
    DIR* dir = opendir(dirpath);
    if (dir)
    {
        closedir(dir);
        return true;
    }
    else if (ENOENT == errno)
        return false;
    return false; // cannot open for some reason ??
}

char * hvMisc_skipLeadingSpaces(const char * input)
{
    int i = 0;
    const int len = strlen(input);
    while (i < len) 
    {
        if (input[i] == ' ')        
            i++;
        else
            break;
    }
    if (i >= len) 
        return NULL;

    return (char *)(input + i);
}

// |><| ************************************************************************
// isFdValid - check whether a given fd(socket) is still valid.
//       Using fcntl to check it.
// Returns:
//      int :  < 0, not valid; >=0 valid
// Args:
//      fd  :  the fd we would like to check on.
// *****************************************************************************
bool hvMisc_isFdValid(int fd)
{   // can get fd. if cannot, err set is not EBADF, we all take it as valid.
    return ((fcntl(fd, F_GETFD) != -1) || (errno != EBADF));
}

bool hvMisc_isFpValid(FILE * pFp)
{
    return hvMisc_isFdValid(fileno(pFp));
}

map<string, string> hvMisc_getDeviceValidIps()
{
    struct ifaddrs * ifAddrStruct = NULL;
    struct ifaddrs * ifa = NULL;
    void * tmpAddrPtr = NULL;
    string ethIpStr;
    string wlanIpStr;
    map<string, string> ips;

    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) 
    {   // check valid IP4 Address
        if (ifa->ifa_addr->sa_family == AF_INET)  
        { 
            tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            NLogD("miscUtil", "%s IP Address %s.", ifa->ifa_name, addressBuffer);
            if (strncmp(ifa->ifa_name, "control", sizeof("control")) == 0)
            {
                ethIpStr.clear();
                ethIpStr = addressBuffer;
                if (wlanIpStr.compare("0.0.0.0") != 0)
                    ips.insert(std::pair<string, string> (ifa->ifa_name, ethIpStr));
            }
            if (strncmp(ifa->ifa_name, "data", sizeof("data")) == 0)
            {
                ethIpStr.clear();
                ethIpStr = addressBuffer;
                if (wlanIpStr.compare("0.0.0.0") != 0)
                    ips.insert(std::pair<string, string> (ifa->ifa_name, ethIpStr));
            }

            if (strncmp(ifa->ifa_name, "eth", 3) == 0)
            {
                ethIpStr.clear();
                ethIpStr = addressBuffer;
                if (wlanIpStr.compare("0.0.0.0") != 0)
                    ips.insert(std::pair<string, string> (ifa->ifa_name, ethIpStr));
            }
            if (strncmp(ifa->ifa_name, "wlan", 4) == 0)
            {
                wlanIpStr.clear();
                wlanIpStr = addressBuffer;
                if (wlanIpStr.compare("0.0.0.0") != 0)
                    ips.insert(std::pair<string, string> (ifa->ifa_name, wlanIpStr));
            }
            if (strncmp(ifa->ifa_name, "wlp", 3) == 0)
            {
                wlanIpStr.clear();
                wlanIpStr = addressBuffer;
                if (wlanIpStr.compare("0.0.0.0") != 0)
                    ips.insert(std::pair<string, string> (ifa->ifa_name, wlanIpStr));
            }
        }
        /* // check it is IP6 
        else if (ifa->ifa_addr->sa_family == AF_INET6) 
        {       
            tmpAddrPtr = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            LOG(INFO) << m_logtag << "%s IP Address %s\n", ifa->ifa_name, addressBuffer); 
        } */
    }
    if (ifAddrStruct != NULL) 
        freeifaddrs(ifAddrStruct);

    if ((int)ips.size() == 0)
    {
        NLogW("miscUtil", "We Cannot Get External Valid IpAddress. Using lo.");
        ips.insert(std::pair<string, string> ("lo", "127.0.0.1"));
    }    
    return ips;
}

string hvMisc_getADeviceValidIp()
{
    map<string, string> :: iterator it;
    map<string, string> ips = hvMisc_getDeviceValidIps();
    it = ips.begin();
    return it->second;
}


int hvMisc_getHomeDir(string & homeDir)
{
    const char *homedir;
    if ((homedir = getenv("HOME")) == NULL) 
    {
        NLogI ("miscUtil", "No home dir set.\n");
        homedir = getpwuid(getuid())->pw_dir;
    }
    NLogI("miscUtil", "HomeDir: %s\n", homedir);
    homeDir.clear();
    homeDir = homedir;
    return 0;
}

int hvMisc_dumpbytes(const unsigned char *pData, const int size)
{
    for (int k = 0 ; k < size; k++)
        NLogI("DumpBytes", "0x%x ", pData[k]);
    return 0;
}

string hvMisc_backtrace()
{
    int j, nptrs;
    void *buffer[100];
    char **strings;
    string backString;
    nptrs = backtrace(buffer, 100);
   /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
       would produce similar output to the following: */
    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) 
    {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }
    for (j = 0; j < nptrs; j++)
        backString.append(strings[j]);
    free(strings);
    return backString;
}

string hvMisc_getPathWithoutExtension(const string & path)
{
    const size_t pos = path.find_last_of('.');
    return path.substr(0, pos);
}

string hvMisc_getFileNameFromPath(const string & path)
{
    const size_t pos = path.find_last_of('/');
    return path.substr(pos + 1);
}

string hvMisc_getDirFromPath(const string & path)
{
    const size_t pos = path.find_last_of('/');
    return path.substr(0, pos);
}

// TODO: not fully functional, polish later.
string hvMisc_getBodyFromUrl(const string & url)
{   // rtmp://192.1.1.1/ree:b1 will get '192.1.1.1'
    // http://192.1.1.1:8020/ree:b1 will get '192.1.1.1'
    const size_t startpos = url.find_first_of('/') + 2;
    string startstr = url.substr(startpos);
    const size_t endpos1 = startstr.find_first_of(':');
    const size_t endpos2 = startstr.find_first_of('/');
    return startstr.substr(0, std::min(endpos1, endpos2));
}

bool hvMisc_isAvcKeyFrame(const unsigned char *bs, const int ns)
{
    int nal_type = -1, idx = 0;
    while(idx < ns - 4)
    {
        if (bs[idx] == 0 && bs[idx+1] == 0 && bs[idx+2] == 0 && bs[idx+3] == 1)
        {
            nal_type = bs[idx+4] & 0x1f;
            idx += 4;
        }
        else if(bs[idx] == 0 && bs[idx+1] == 0 && bs[idx+2] == 1)
        {
            nal_type = bs[idx+3] & 0x1f;
            idx += 3;
        }
        else 
            idx ++;
        
        if (nal_type != -1)
        {
            if (nal_type == 5 || nal_type == 7 || nal_type == 8)
                return true;
            nal_type = -1;
        }
    }  
    return false;
}

bool hvMisc_isHevcKeyFrame(const unsigned char *bs, const int ns)
{
    int nal_type, idx;
    nal_type = -1;
    idx = 0;
    while(idx < ns - 4)
    {
        if (bs[idx] == 0 && bs[idx+1] == 0 && bs[idx+2] == 0 && bs[idx+3] == 1)
        {
            nal_type = (bs[idx+4] & 0x7e) >> 1;
            idx += 4;
        }
        else if(bs[idx] == 0 && bs[idx+1] == 0 && bs[idx+2] == 1)
        {
            nal_type = (bs[idx+3] & 0x7e) >> 1;
            idx += 3;
        }
        else 
            idx ++;
        
        if (nal_type != -1)
        {
            if (nal_type == 32 || nal_type == 33 || nal_type == 34 ||
                nal_type == 19 || nal_type == 20)
                return true;
            nal_type = -1;
        }
    }  
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
//// cut leading & trailing spaces
string hvMisc_trimSpace(const string & str)
{
    if(str.empty())
        return str;
    std::size_t firstScan = str.find_first_not_of(' ');
    std::size_t first     = firstScan == std::string::npos ? str.length() : firstScan;
    std::size_t last      = str.find_last_not_of(' ');
    return str.substr(first, last-first+1);
}

int hvMisc_removeDoubleSlashOfUrl(string & url)
{
    char last = url[0];
    for (auto iter = url.begin() + 1; iter != url.end() - 1;)
    {
        if (last == '/' && *iter == '/')
        {
            url.erase(iter);
            continue;
        }
        last = *iter;
        iter++;
    }
    return 0;
}


const string supportLiveEnumsStr("rtsp rtmp rtmps http https udp tcp");
const static char * livePrefixEnums[] = 
    {"rtsp://", "rtmp://", "rtmps://", "http://", "https://", "udp://", "tcp://"};

bool hvMisc_isStreamingOf(const string & inputUrl, const string & proto)
{
    return inputUrl.find(proto) == 0;
}

bool hvMisc_isLiveStreamingUrl(const string & url)
{
    for (unsigned int k=0; k < sizeof(livePrefixEnums) / sizeof(char *); k++)
    {
        if (url.find(livePrefixEnums[k]) == 0) 
            return true;
    }
    return false;
}

// all true 
bool hvMisc_all(const vector<bool> & vec)
{
    if (vec.size() == 0)
        return false;
    for (const auto & v : vec)
        if (v == false) 
            return false;
    return true;
}
// is there any true
bool hvMisc_any(const vector<bool> & vec)
{
    if (vec.size() == 0)
        return false;
    for (const auto & v : vec)
        if (v == true) 
            return true;
    return false;
}

///////////////////////
#define WM_WIDTH  96
#define WM_HEIGHT 30
const unsigned char hv_wm_mask[WM_HEIGHT][WM_WIDTH] =
{
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 238, 213, 194, 181, 173, 170, 170, 174, 180, 188, 198, 209, 219, 231, 244, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 252, 204, 168, 163, 165, 167, 168, 169, 166, 163, 164, 166, 168, 168, 168, 169, 173, 183, 195, 211, 231, 247, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 209, 161, 168, 169, 169, 169, 168, 165, 181, 208, 229, 239, 244, 246, 245, 243, 238, 230, 218, 205, 193, 192, 197, 213, 232, 248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 250, 176, 167, 169, 169, 169, 169, 166, 209, 251, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 252, 242, 225, 210, 205, 211, 225, 243, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 251, 172, 168, 169, 169, 169, 164, 200, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 248, 232, 222, 220, 230, 248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 226, 220, 221, 220, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 244, 220, 221, 220, 226, 181, 166, 169, 169, 169, 165, 205, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 230, 231, 233, 232, 224, 211, 199, 194, 225, 239, 227, 227, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 232, 239, 251, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 74, 25, 31, 26, 201, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 188, 23, 32, 23, 88, 211, 164, 169, 169, 169, 173, 122, 85, 90, 90, 90, 90, 90, 88, 82, 81, 81, 81, 86, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 92, 93, 86, 94, 118, 115, 106, 92, 87, 88, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 91, 96, 110, 137, 178, 223, 255, 255, 255, 255, 255, 255, 255 },
    { 67, 15, 22, 17, 199, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 185, 14, 23, 14, 79, 244, 171, 168, 169, 169, 173, 138, 78, 83, 83, 83, 83, 80, 84, 132, 136, 136, 141, 111, 80, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 84, 82, 80, 86, 99, 105, 104, 90, 82, 81, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 82, 80, 76, 74, 86, 134, 216, 255, 255, 255, 255, 255 },
    { 69, 18, 25, 20, 200, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 186, 17, 26, 17, 78, 255, 209, 163, 169, 169, 169, 167, 99, 82, 85, 85, 83, 79, 186, 255, 255, 255, 230, 105, 81, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 84, 83, 84, 92, 102, 106, 97, 86, 83, 84, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 84, 75, 83, 168, 254, 255, 255, 255 },
    { 69, 18, 25, 20, 200, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 186, 17, 26, 17, 78, 255, 252, 184, 165, 169, 169, 173, 145, 82, 84, 84, 76, 176, 255, 255, 255, 230, 102, 78, 86, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 84, 83, 87, 97, 103, 98, 88, 84, 84, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 82, 73, 155, 254, 255, 255 },
    { 69, 18, 25, 20, 200, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 186, 17, 26, 17, 78, 255, 255, 240, 173, 166, 169, 169, 173, 126, 79, 75, 165, 255, 255, 255, 234, 108, 76, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 84, 83, 84, 93, 98, 94, 86, 84, 84, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 84, 74, 181, 255, 255 },
    { 69, 18, 25, 20, 210, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 195, 16, 26, 17, 78, 255, 255, 255, 231, 169, 166, 169, 169, 171, 112, 153, 254, 255, 255, 238, 114, 75, 86, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 84, 84, 89, 95, 93, 86, 84, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 81, 91, 231, 255 },
    { 69, 18, 26, 23, 128, 160, 159, 159, 159, 159, 159, 159, 159, 159, 159, 160, 120, 20, 26, 17, 78, 255, 255, 255, 255, 228, 169, 166, 169, 169, 166, 221, 255, 255, 242, 120, 75, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 84, 89, 94, 92, 86, 84, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 73, 160, 255 },
    { 69, 18, 26, 26, 17, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 17, 27, 26, 17, 78, 255, 255, 255, 255, 255, 230, 172, 167, 169, 168, 167, 218, 240, 126, 75, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 84, 90, 94, 91, 85, 84, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 80, 103, 245 },
    { 69, 18, 26, 26, 18, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 18, 27, 26, 17, 78, 255, 255, 255, 255, 255, 255, 242, 178, 172, 169, 168, 171, 156, 79, 81, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 84, 85, 90, 95, 87, 84, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 84, 81, 215 },
    { 69, 18, 26, 24, 79, 97, 96, 96, 96, 96, 96, 96, 96, 96, 96, 97, 75, 23, 26, 17, 78, 255, 255, 255, 255, 255, 255, 215, 61, 128, 178, 170, 168, 170, 150, 96, 80, 84, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 83, 84, 85, 85, 85, 85, 84, 83, 84, 83, 84, 85, 85, 84, 83, 83, 83, 83, 84, 85, 85, 84, 82, 85, 86, 80, 76, 76, 76, 76, 76, 76, 78, 84, 85, 84, 84, 85, 85, 85, 85, 84, 84, 85, 75, 181 },
    { 69, 18, 25, 20, 207, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 192, 17, 26, 17, 78, 255, 255, 255, 255, 255, 224, 51, 11, 26, 98, 166, 167, 169, 173, 164, 116, 81, 82, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 86, 93, 86, 82, 85, 85, 84, 84, 93, 87, 90, 87, 85, 83, 82, 91, 92, 92, 92, 85, 81, 85, 86, 90, 86, 81, 109, 143, 149, 148, 148, 148, 147, 124, 84, 84, 89, 87, 79, 85, 85, 85, 89, 89, 85, 75, 155 },
    { 69, 18, 25, 20, 200, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 186, 17, 26, 17, 78, 255, 255, 255, 255, 233, 65, 14, 27, 10, 85, 218, 180, 164, 168, 171, 172, 140, 94, 80, 84, 85, 85, 85, 85, 85, 85, 85, 84, 81, 188, 216, 92, 82, 85, 78, 190, 195, 98, 229, 144, 74, 95, 204, 228, 228, 228, 228, 224, 120, 72, 124, 231, 113, 89, 236, 250, 236, 237, 237, 236, 243, 254, 129, 71, 181, 222, 119, 75, 85, 78, 185, 185, 78, 76, 140 },
    { 69, 18, 25, 20, 200, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 186, 17, 26, 17, 78, 255, 255, 255, 240, 73, 13, 27, 13, 74, 240, 255, 247, 200, 166, 166, 169, 174, 164, 121, 85, 80, 84, 85, 85, 85, 85, 85, 85, 78, 119, 255, 169, 74, 78, 121, 254, 129, 97, 255, 156, 69, 121, 254, 132, 105, 108, 110, 205, 149, 69, 132, 255, 117, 102, 255, 174, 80, 91, 91, 83, 127, 255, 156, 67, 198, 255, 248, 129, 74, 76, 205, 205, 77, 77, 133 },
    { 69, 18, 25, 20, 200, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 186, 17, 26, 17, 78, 255, 255, 246, 86, 12, 27, 14, 61, 232, 255, 255, 255, 255, 228, 181, 164, 167, 171, 173, 151, 107, 81, 81, 85, 85, 85, 85, 85, 85, 75, 184, 251, 101, 73, 214, 208, 73, 103, 254, 154, 69, 117, 254, 191, 176, 178, 179, 166, 98, 75, 130, 255, 116, 101, 255, 170, 73, 83, 83, 75, 122, 255, 154, 68, 202, 206, 199, 254, 141, 66, 201, 202, 77, 77, 133 },
    { 69, 18, 25, 20, 200, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 186, 17, 26, 17, 78, 255, 252, 100, 11, 27, 16, 49, 223, 255, 255, 255, 255, 255, 255, 251, 211, 172, 163, 168, 173, 171, 143, 100, 81, 82, 85, 85, 85, 85, 81, 97, 245, 183, 127, 254, 114, 74, 103, 254, 154, 75, 84, 165, 209, 211, 211, 210, 254, 167, 67, 131, 255, 116, 101, 255, 171, 74, 85, 85, 78, 124, 255, 154, 68, 205, 195, 75, 193, 255, 145, 198, 203, 77, 77, 133 },
    { 67, 16, 23, 18, 199, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 185, 14, 24, 14, 79, 254, 113, 9, 23, 17, 39, 212, 255, 255, 255, 255, 255, 255, 255, 255, 255, 243, 201, 169, 164, 168, 174, 168, 137, 99, 80, 82, 84, 83, 84, 72, 155, 252, 238, 184, 73, 79, 102, 254, 153, 67, 132, 168, 83, 82, 82, 79, 225, 183, 65, 129, 255, 115, 100, 255, 168, 67, 78, 78, 71, 119, 255, 153, 66, 204, 200, 71, 78, 179, 254, 244, 195, 75, 75, 132 },
    { 72, 22, 29, 24, 201, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 187, 21, 30, 22, 86, 138, 17, 29, 27, 38, 199, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 236, 196, 168, 164, 169, 174, 167, 137, 100, 90, 92, 89, 86, 88, 224, 247, 104, 83, 84, 106, 254, 156, 70, 145, 254, 228, 229, 228, 228, 250, 158, 72, 134, 255, 120, 99, 246, 230, 201, 205, 205, 202, 215, 255, 145, 73, 205, 202, 82, 86, 82, 173, 255, 196, 82, 82, 136 },
    { 223, 215, 217, 216, 245, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 243, 215, 217, 216, 221, 213, 215, 217, 215, 229, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 235, 196, 169, 163, 168, 174, 165, 168, 206, 227, 231, 225, 241, 247, 223, 228, 227, 230, 254, 238, 225, 229, 254, 255, 255, 255, 255, 254, 231, 226, 235, 255, 233, 227, 250, 255, 255, 254, 255, 255, 255, 255, 233, 227, 247, 247, 226, 228, 228, 225, 253, 246, 228, 228, 236 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 238, 201, 172, 163, 167, 169, 172, 194, 228, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 231, 240, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 243, 212, 180, 166, 164, 164, 164, 179, 206, 237, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 246, 222, 237, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 251, 228, 196, 174, 164, 162, 162, 168, 185, 212, 237, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 246, 225, 220, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 247, 221, 196, 177, 165, 161, 160, 166, 179, 200, 220, 234, 247, 254, 255, 255, 255, 255, 255, 254, 244, 227, 215, 213, 236, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 249, 231, 213, 193, 177, 168, 162, 160, 162, 169, 179, 187, 192, 194, 195, 194, 194, 201, 217, 239, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 250, 242, 233, 223, 214, 207, 203, 202, 205, 212, 223, 234, 245, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
};

void hvMisc_frameWaterMarking(const unsigned char * pSrcY, 
                              const int width, const int height, const int stride,
                              const int top, const int left)
{
    int i=0, j=0;
    unsigned char *pb = (unsigned char *)hv_wm_mask;
    unsigned char *pf = (unsigned char *)(pSrcY + stride * top + left);

    for(j = 0; j < WM_HEIGHT; j ++)
    {
        for(i = 0; i < WM_WIDTH; i ++)
            pf[i] = (pf[i] + pb[i]) >> 1;
        pf += stride;
        pb += WM_WIDTH;
    }
    return;
}

double hvMisc_tsToTime(const double ts, const int timebaseNum, const int timebaseDen)
{
    return ts / (1.0 * timebaseDen) * timebaseNum;
}

string hvMisc_getNumFromStr(string & logtag)
{
    string strNum;
    const char *p = logtag.c_str();
    while (*p != '\0')
    {
        if (isdigit(*p))
            strNum.append(1, *p);
        p++;
    }    
    return strNum;
}



#define BIN_COUNT 20
#define BIN_HEIGHT 5
int hvMisc_generateAudioBarContinue(unsigned char *yuv, int width, int height, double volumeLeft, double volumeRight)
{
    int y;

    memset(yuv, 0, width * height * sizeof(unsigned char) * 3/2);  // black green

    int top = 1;
    int bottom = 1;
    int left = 1;
    int right = 1;
    int middle = 2;
  

    int barWidth = (width - left - middle - right) >> 1;
    int posLeft = left;
    int posRight = width - right - barWidth;

    /////////////////////////////////////////////////
    // channel left firstly
    int yOffset = (1.0 - volumeLeft) * (height - top - bottom) + top;
    int xOffset = posLeft;

    unsigned char *py = yuv + width * yOffset + xOffset;
    for (y = yOffset; y < height - bottom; y ++)
    {
        memset(py, 240, barWidth * sizeof(unsigned char));
        py += width;
    }


    ////////////////////////////////////////////////
    // channel right 
    yOffset = (1.0 - volumeRight) * (height - top - bottom) + top;
    xOffset = posRight;
   
    py = yuv + width * yOffset + xOffset;
    
    for (y = yOffset; y < height - bottom; y ++)
    {
        memset(py, 240, barWidth * sizeof(unsigned char));
        py += width;
    }


    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//// audio bar 
static int hvMisc_generateAudioBarDiscreteSameHeight(unsigned char *yuv, int width, int height, 
                                                     double volumeLeft, double volumeRight)
{
    int top = 1;
    int bottom = 1;
    int left = 1;
    int right = 1;
    int middle = 2;

    int barWidth = (width - left - middle - right) >> 1;
    int posLeft = left;
    int posRight = width - right - barWidth;

    int barHeight = height - top - bottom;
    int binHeight = BIN_HEIGHT;
    int binCount = barHeight/binHeight;

    if (binCount < 2) return -1;

    top = top + (barHeight - binHeight * binCount);
    barHeight = binHeight * binCount;

    int leftCount = (int)(volumeLeft * binCount + 0.5);
    int rightCount = (int)(volumeRight * binCount + 0.5);
    
    if (leftCount > binCount) leftCount = binCount;
    if (rightCount > binCount) rightCount = binCount;
    int n, y;

    memset(yuv, 0, width * height * sizeof(unsigned char) * 3/2);  // black green

    // left volume
    int yOffset = (barHeight - leftCount * binHeight) + top;
    int xOffset = posLeft;
    unsigned char *py = yuv + width * yOffset + xOffset;
    for (n = 0, y = yOffset; y < height - bottom; y ++, n ++)
    {
        if ((n + binHeight) % binHeight != 0)
            memset(py, 240, barWidth * sizeof(unsigned char));
        py += width;
    }
    
    // right volume
    yOffset = (barHeight - rightCount * binHeight) + top;
    xOffset = posRight;
    py = yuv + width * yOffset + xOffset;
    for (n = 0, y = yOffset; y < height - bottom; y ++, n ++)
    {
        if ((n + binHeight) % binHeight != 0)
            memset(py, 240, barWidth * sizeof(unsigned char));
        py += width;
    }

    return 0;
}

static int hvMisc_generateAudioBarDiscreteSameCount(unsigned char *yuv, int width, int height, 
                                                    double volumeLeft, double volumeRight)
{
    int top = 1;
    int bottom = 1;
    int left = 1;
    int right = 1;
    int middle = 2;

    int barWidth = (width - left - middle - right) >> 1;
    int posLeft = left;
    int posRight = width - right - barWidth;

    int binCount = BIN_COUNT;
    int barHeight = height - top - bottom;
    int binHeight = barHeight/binCount;

    if (binHeight < 4) return -1;

    top = top + (barHeight - binHeight * binCount);
    barHeight = binHeight * binCount;

    int leftCount = (int)(volumeLeft * binCount + 0.5);
    int rightCount = (int)(volumeRight * binCount + 0.5);
    
    if (leftCount > binCount) leftCount = binCount;
    if (rightCount > binCount) rightCount = binCount;
    int n, y;

    memset(yuv, 0, width * height * sizeof(unsigned char) * 3/2);  // black green

    // left volume
    int yOffset = (barHeight - leftCount * binHeight) + top;
    int xOffset = posLeft;
    unsigned char *py = yuv + width * yOffset + xOffset;
    for (n = 0, y = yOffset; y < height - bottom; y ++, n ++)
    {
        if ((n + binHeight) % binHeight != 0)
            memset(py, 240, barWidth * sizeof(unsigned char));
        py += width;
    }
    
    // right volume
    yOffset = (barHeight - rightCount * binHeight) + top;
    xOffset = posRight;
    py = yuv + width * yOffset + xOffset;
    for (n = 0, y = yOffset; y < height - bottom; y ++, n ++)
    {
        if ((n + binHeight) % binHeight != 0)
            memset(py, 240, barWidth * sizeof(unsigned char));
        py += width;
    }

    return 0;
}

int hvMisc_generateAudioBar(unsigned char *yuv, int width, int height, double volumeLeft, double volumeRight)
{
   int ret = hvMisc_generateAudioBarDiscreteSameHeight(yuv, width, height, volumeLeft, volumeRight);
   if (ret < 0) 
       ret = hvMisc_generateAudioBarContinue(yuv, width, height, volumeLeft, volumeRight);
   return ret;
}

} // namespace 
} // namespace 
