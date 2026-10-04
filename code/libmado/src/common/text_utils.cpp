#include <mado/common/text_utils.hpp>

#include <utf8proc/utf8proc.h>

#include <cctype>
#include <string>

namespace mado::common {

namespace {

constexpr int DAYS_PER_MONTH[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Checks whether year/month/day is a valid calendar date
bool is_valid_date(int year, int month, int day) {
    if (month < 1 || month > 12)
        return false;
    if (day < 1)
        return false;

    int max_day = DAYS_PER_MONTH[month - 1];

    // Leap year
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (leap)
            max_day = 29;
    }

    return day <= max_day;
}

} // namespace

void trim_inplace(std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        s.clear();
        return;
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    s.erase(end + 1);
    s.erase(0, start);
}

std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    return s.substr(start, end - start + 1);
}

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

std::string utf8_tolower(const std::string &str) {
    std::string result;
    result.reserve(str.size());

    size_t i = 0;
    while (i < str.size()) {
        utf8proc_int32_t cp = 0;
        utf8proc_ssize_t n = utf8proc_iterate(
            reinterpret_cast<const utf8proc_uint8_t *>(str.data() + i),
            static_cast<utf8proc_ssize_t>(str.size() - i),
            &cp);

        if (n < 0) {
            // Invalid UTF-8: pass the byte through unchanged
            result.push_back(str[i]);
            i += 1;
            continue;
        }

        utf8proc_int32_t lower = utf8proc_tolower(cp);

        utf8proc_uint8_t buf[4];
        utf8proc_ssize_t written = utf8proc_encode_char(lower, buf);
        if (written <= 0) {
            // Encoding failure: pass the original bytes through
            result.append(str, i, static_cast<size_t>(n));
        } else {
            result.append(reinterpret_cast<const char *>(buf),
                          static_cast<size_t>(written));
        }

        i += static_cast<size_t>(n);
    }

    return result;
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

// TODO: maybe utf8 support here
bool is_letter_or_underscore(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_identifier_char(char c) {
    return is_digit(c) || is_letter_or_underscore(c);
}

bool is_timestamp(const std::string &str) {
    size_t n = str.size();

    // YYYY
    // YYYYMM
    // YYYYMMDD
    // YYYYMMDD-
    // YYYYMMDD-HH
    // YYYYMMDD-HHMM
    // YYYYMMDD-HHMMSS
    if (n != 4 && n != 6 && n != 8 && n != 9 &&
        n != 11 && n != 13 && n != 15) {
        return false;
    }

    auto is_digits = [&](size_t from, size_t len) {
        for (size_t i = from; i < from + len; ++i) {
            if (!is_digit(str[i]))
                return false;
        }
        return true;
    };

    auto to_int = [&](size_t from, size_t len) {
        // All callers pass len <= 4, so int can't overflow
        return std::stoi(str.substr(from, len));
    };

    // YYYY: 0000-9999
    if (!is_digits(0, 4))
        return false;

    // YYYYMM
    if (n >= 6) {
        if (!is_digits(4, 2))
            return false;

        int month = to_int(4, 2);
        if (month < 1 || month > 12)
            return false;
    }

    // YYYYMMDD
    if (n >= 8) {
        if (!is_digits(6, 2))
            return false;

        int year = to_int(0, 4);
        int month = to_int(4, 2);
        int day = to_int(6, 2);

        if (!is_valid_date(year, month, day))
            return false;
    }

    // YYYYMMDD-
    if (n >= 9) {
        if (str[8] != '-')
            return false;
    }

    // YYYYMMDD-HH
    if (n >= 11) {
        if (!is_digits(9, 2))
            return false;

        int hour = to_int(9, 2);
        if (hour > 23)
            return false;
    }

    // YYYYMMDD-HHMM
    if (n >= 13) {
        if (!is_digits(11, 2))
            return false;

        int minute = to_int(11, 2);
        if (minute > 59)
            return false;
    }

    // YYYYMMDD-HHMMSS
    if (n >= 15) {
        if (!is_digits(13, 2))
            return false;

        int second = to_int(13, 2);
        if (second > 59)
            return false;
    }

    return true;
}

} // namespace mado::common
