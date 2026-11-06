#pragma once

#include <ctime>
#include <string>

namespace mado::common {

// Returns the current UTC time in YYYYMMDD-HHMMSS. May throw std::bad_alloc.
// Возвращает текущее время UTC как YYYYMMDD-HHMMSS. Возможен std::bad_alloc.
std::string current_timestamp_utc();

// Formats a supported time_t in UTC. May throw std::bad_alloc.
// Форматирует поддерживаемое значение time_t в UTC. Возможен std::bad_alloc.
std::string format_timestamp_utc(std::time_t t);

} // namespace mado::common
