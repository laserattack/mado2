#pragma once

#include <string>

namespace mado::common {

/// Compares equal-length byte strings using the current C locale's case
/// conversion; returns false when their lengths differ.
bool equals_ignore_case(const std::string &str1, const std::string &str2);

/// Returns a copy of `str` with Unicode codepoints converted to lowercase via
/// utf8proc. Invalid UTF-8 bytes and codepoints that cannot be re-encoded are
/// passed through unchanged.
std::string utf8_tolower(const std::string &str);

/// Returns whether `c` is an ASCII digit (`0` through `9`).
bool is_digit(char c);

/// Returns whether `c` is an ASCII letter or underscore.
bool is_letter_or_underscore(char c);

/// Returns whether `c` is an ASCII digit, letter, or underscore.
bool is_identifier_char(char c);

/// Returns whether `str` is a valid timestamp in one of the supported forms:
/// YYYY, YYYYMM, YYYYMMDD, YYYYMMDD-, YYYYMMDD-HH, YYYYMMDD-HHMM, or
/// YYYYMMDD-HHMMSS. Calendar and clock fields are range-checked.
bool is_timestamp(const std::string &str);

/// Returns a copy with leading and trailing spaces, tabs, CR, and LF removed.
std::string trim(const std::string &s);
/// Removes leading and trailing spaces, tabs, CR, and LF from `s` in place.
void trim_inplace(std::string &s);

} // namespace mado::common
