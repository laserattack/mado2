#include <mado/common/time_utils.hpp>

#include <ctime>
#include <string>

namespace mado::common {

namespace {

// Converts a time to UTC with the platform API. The API's error status is
// ignored.
std::tm gmtime(std::time_t t) {
    std::tm bt{};
#if defined(_MSC_VER)
    gmtime_s(&bt, &t);
#else
    gmtime_r(&t, &bt);
#endif
    return bt;
}

} // namespace

// Formats the current UTC time as an entry timestamp. May throw
// std::bad_alloc.
std::string current_timestamp_utc() {
    return format_timestamp_utc(std::time(nullptr));
}

// Formats a UTC time as YYYYMMDD-HHMMSS. May throw std::bad_alloc.
std::string format_timestamp_utc(std::time_t t) {
    std::tm tm = gmtime(t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
    return buf;
}

} // namespace mado::common
