#pragma once

#include <ctime>
#include <string>

namespace mado::common {

inline constexpr char NEVER_TIMESTAMP[] = "99990101-000000";


/// Converts `time` to a UTC broken-down time.
/// @throws std::runtime_error If the given time cannot be converted to UTC.
std::tm utc_time(std::time_t time);

/// Returns the current UTC time in YYYYMMDD-HHMMSS.
/// @throws std::runtime_error If the system clock cannot be read or its
/// time cannot be converted to UTC.
std::string current_timestamp_utc();

/// Formats the given UTC time_t as YYYYMMDD-HHMMSS.
/// @throws std::runtime_error If the given time cannot be converted to UTC.
std::string format_timestamp_utc(std::time_t t);

} // namespace mado::common
