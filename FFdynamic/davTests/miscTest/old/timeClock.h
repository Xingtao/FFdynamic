#ifndef _TIME_CLOCK_H_
#define _TIME_CLOCK_H_

#include <iostream>
#include <string>
#include <chrono>
#include <ctime>

namespace ff_dynamic
{
extern std::string durationToTimeString(std::chrono::milliseconds time);

    
} // namespace

#endif // _TIME_CLOCK_H_
