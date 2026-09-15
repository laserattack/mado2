#pragma once

#include <ctime>

namespace mado::common {

// Thread-safe localtime.
// localtime_r on POSIX, localtime_s on MSVC.
std::tm localtime_threadsafe(std::time_t t);

} // namespace mado::common
