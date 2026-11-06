#pragma once

#include <ctime>
#include <string>

namespace mado::common {

// Converts a timestamp to UTC calendar fields. Conversion failures throw std::runtime_error.
// Преобразует метку в календарные поля UTC. Сбой вызывает std::runtime_error.
std::tm utc_time(std::time_t time);

// Returns the current UTC timestamp. Clock or conversion failures throw std::runtime_error.
// String allocation may throw std::bad_alloc.
// Возвращает текущую метку UTC. Сбой часов или преобразования вызывает std::runtime_error.
// Выделение памяти для строки может вызвать std::bad_alloc.
std::string current_timestamp_utc();

// Formats time as YYYYMMDD-HHMMSS in UTC. Conversion failures throw std::runtime_error.
// String allocation may throw std::bad_alloc.
// Форматирует время в UTC как YYYYMMDD-HHMMSS. Сбой преобразования вызывает std::runtime_error.
// Выделение памяти для строки может вызвать std::bad_alloc.
std::string format_timestamp_utc(std::time_t t);

} // namespace mado::common
