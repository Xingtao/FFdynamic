#include <string>
#include "stringParse.h"

using std :: string;
using namespace ff_dynamic;

int main(int argc, char ** argv)
{
    PontusUrl url;
    PontusUrl url2;
    PontusUrl url3;
    string testStr("rtmp://192.168.1.20:8020/live/test");
    string testStr2("rtmp://192.168.1.20/live/test");
    string testStr3("rtmp://192.168.1.20/");
    parseCaptureUrl(testStr, url);
    url.dumpUrl();
    
    parseCaptureUrl(testStr2, url2);
    url2.dumpUrl();

    parseCaptureUrl(testStr3, url3);
    url3.dumpUrl();

    return 0;
}
