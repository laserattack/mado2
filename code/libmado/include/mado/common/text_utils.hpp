#pragma once

#include <string>

namespace mado::common {

// Compares strings ignoring ASCII case. No exceptions.
bool equals_ignore_case(const std::string &str1, const std::string &str2);

// Returns a copy of str with all codepoints converted to lowercase
// using Unicode simple case folding (via utf8proc). Invalid UTF-8
// bytes are passed through unchanged. May throw std::bad_alloc.
std::string utf8_tolower(const std::string &str);

// Tests for an ASCII digit. No exceptions.
bool is_digit(char c);

// Tests for an ASCII letter or underscore. No exceptions.
bool is_letter_or_underscore(char c);

// Tests for an ASCII digit, letter, or underscore. No exceptions.
bool is_identifier_char(char c);

// YYYYMMDD-HHMMSS with optional shorter forms: YYYY, YYYYMM, YYYYMMDD, YYYYMMDD-, YYYYMMDD-HH, YYYYMMDD-HHMM, YYYYMMDD-HHMMSS
bool is_timestamp(const std::string &str);

// Returns a trimmed copy. May throw std::bad_alloc.
std::string trim(const std::string &s);
// Removes surrounding whitespace, clearing an all-whitespace string. No error
// result.
void trim_inplace(std::string &s);

} // namespace mado::common
