#include <cctype>
#include <string>

#include "text_utils.hpp"

namespace mado::common {

bool equals_ignore_case(const std::string &str1, const std::string &str2) {
    if (str1.length() != str2.length())
        return false;

    for (size_t i = 0; i < str1.length(); ++i) {
        if (std::tolower(static_cast<unsigned char>(str1[i])) !=
            std::tolower(static_cast<unsigned char>(str2[i])))
            return false;
    }

    return true;
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_letter_or_underscore(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_identifier_char(char c) {
    return is_digit(c) || is_letter_or_underscore(c);
}

} // namespace mado::common
