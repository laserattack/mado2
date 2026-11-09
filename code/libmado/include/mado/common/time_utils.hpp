#pragma once

#include <ctime>
#include <string>

namespace mado::common {

inline constexpr char NEVER_TIMESTAMP[] = "99990101-000000";


std::tm utc_time(std::time_t time);

// Returns the current UTC time in YYYYMMDD-HHMMSS.
std::string current_timestamp_utc();

// Formats the given UTC time_t as YYYYMMDD-HHMMSS.
std::string format_timestamp_utc(std::time_t t);

} // namespace mado::common
