#include "stringParse.h"

namespace ff_dynamic
{

int parseCaptureUrl(const string & inputUrl, PontusUrl & url)
{
    PontusUrl_Parser<string::const_iterator> parser;
    string::const_iterator iter = inputUrl.begin();
    string::const_iterator end = inputUrl.end();
    bool bOk = phrase_parse(iter, end, parser, ascii::space, url); 
    if (bOk && iter == end)
        return 0;
    else
        cout << "Parse Error: " << inputUrl << "\n";
    return -1;
}

}
