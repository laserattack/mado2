#pragma once

#include <ctime>
#include <string>

namespace mado::common {

// Returns the current UTC time in YYYYMMDD-HHMMSS. May throw std::bad_alloc.
std::string current_timestamp_utc();

// Formats a supported time_t in UTC. May throw std::bad_alloc.
std::string format_timestamp_utc(std::time_t t);

} // namespace mado::common
