#ifndef __types_hpp__f0dffdb8_a3f8_4ac7_889c_0926a61babb6__
#define __types_hpp__f0dffdb8_a3f8_4ac7_889c_0926a61babb6__

#include <chrono>

namespace media {

using namespace std::literals::chrono_literals;

using timestamp = std::chrono::duration<std::int64_t, std::ratio<1, 90000>>;

}

#endif
