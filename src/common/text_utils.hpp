#pragma once

#include <string>

namespace mado::common {

// Case-insensitive string comparison
bool equals_ignore_case(const std::string &str1, const std::string &str2);

// Checks whether the character is a digit (0-9)
bool is_digit(char c);

// Checks whether the character is a letter (a-z, A-Z) or underscore (_)
bool is_letter_or_underscore(char c);

// Checks whether the character is a digit, letter, or underscore
bool is_identifier_char(char c);

// YYYYMMDDTHHMMSS with optional shorter forms: YYYY, YYYYMM, YYYYMMDD, YYYYMMDDT, YYYYMMDDTHH, YYYYMMDDTHHMM, YYYYMMDDTHHMMSS
bool is_timestamp(const std::string &str);

} // namespace mado::common
