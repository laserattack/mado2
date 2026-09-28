#pragma once

#include <string>

namespace mado::common {

// Case-insensitive string comparison
bool equals_ignore_case(const std::string &str1, const std::string &str2);

// Returns a copy of str with all codepoints converted to lowercase
// using Unicode simple case folding (via utf8proc). Invalid UTF-8
// bytes are passed through unchanged.
std::string utf8_tolower(const std::string &str);

// Checks whether the character is a digit (0-9)
bool is_digit(char c);

// Checks whether the character is a letter (a-z, A-Z) or underscore (_)
bool is_letter_or_underscore(char c);

// Checks whether the character is a digit, letter, or underscore
bool is_identifier_char(char c);

// YYYYMMDD-HHMMSS with optional shorter forms: YYYY, YYYYMM, YYYYMMDD, YYYYMMDD-, YYYYMMDD-HH, YYYYMMDD-HHMM, YYYYMMDD-HHMMSS
bool is_timestamp(const std::string &str);

// Trims leading and trailing whitespace
std::string trim(const std::string &s);

} // namespace mado::common
