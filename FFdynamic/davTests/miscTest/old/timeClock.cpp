#include <tuple>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "timeClock.h"

using::std::string;

namespace ff_dynamic
{

using T = std::tuple<std::chrono::milliseconds, int, const char*>;

constexpr T formats[] = {
    T {std::chrono::hours(1), 2, ""},
    T {std::chrono::minutes(1), 2, ":"},
    T {std::chrono::seconds(1), 2, ":"}
    // T {std::chrono::milliseconds(1), 3, "."}
};

template <typename Container, typename Fun>
void tuple_for_each(const Container & c, Fun fun)
{
    for (auto & e : c)
        fun(std::get<0>(e), std::get<1>(e), std::get<2>(e));
    return;
}

string durationToTimeString(std::chrono::milliseconds time)
{
    std::ostringstream o;
    o << "[";
    for (auto & e : formats)
    {
        o << std::get<2>(e) << std::setw(std::get<1>(e)) << (time / std::get<0>(e));
        time = time % std::get<0>(e);
    }
    o << "]";
    return o.str();
}

//boost::posix_time::ptime start, stop;
//boost::posix_time::time_duration diff;
//start = boost::posix_time::microsec_clock::local_time();
//sleep(5);
//stop = boost::posix_time::microsec_clock::local_time();
//diff = stop - stop;
//std::cout << to_simple_string( diff ) << std::endl;

} // namespace
