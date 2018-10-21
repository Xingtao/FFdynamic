#ifndef _STRING_PARSE_H_
#define _STRING_PARSE_H_

#include <iostream>
#include <string>
#include "boost/spirit/include/qi.hpp"
#include "boost/spirit/include/phoenix_core.hpp"
#include "boost/spirit/include/phoenix_operator.hpp"
#include "boost/spirit/include/phoenix_object.hpp"
#include "boost/fusion/include/io.hpp"
#include "boost/fusion/include/adapt_struct.hpp"

using std::cout;
using std::string;
namespace qi = boost::spirit::qi;
namespace phoenix = boost::phoenix;
namespace ascii = boost::spirit::ascii;

namespace ff_dynamic
{   
    struct PontusUrl
    {
        string type;
        string path;
        int idx = -1;
        string suffix;
        void dumpUrl() {
            cout << "UrlType: " << type << ", path: " << path << ", idx: " 
                 << idx << ", suffix: " << suffix << "\n";
        }
    };
} // namespace

BOOST_FUSION_ADAPT_STRUCT(
    EngineV::PontusUrl,
    (string, type) (string, path) (int, idx) (string, suffix)
)
     
namespace ff_dynamic
{
    template <typename Iterator>
    struct PontusUrl_Parser : qi::grammar<Iterator, PontusUrl(), ascii::space_type>
    {
        PontusUrl_Parser() : PontusUrl_Parser::base_type(urlRule)
        {
            using qi::int_;
            using qi::on_error;
            using qi::fail;
            using namespace qi::labels;
            using ascii::char_;
            using phoenix::val;
            //urlRule %= +(char_ - ':') >> ':' >> +(char_ - ':') >> ':' >> int_ ;
            urlRule %= +(char_ - ':') >> string("://") >> 
                       +(char_ - (char_(':') | char_('/'))) >> 
                       ((':' >> int_ >> *char_('/')) ^ *char_('/')) >>
                       *char_;
            //on_error<fail>(urlRule, std::cout << val("Error Expecting ") << _4);
            on_error<fail>(urlRule, std::cout << val("Error Expecting "));
        }
        qi::rule<Iterator, PontusUrl(), ascii::space_type> urlRule;    
    };

    int parseCaptureUrl(const string & inputUrl, PontusUrl & url);
    
} // namespace

#endif // _STRING_PARSE_H_
