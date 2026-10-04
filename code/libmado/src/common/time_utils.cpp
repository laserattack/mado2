#include <mado/common/time_utils.hpp>

#include <ctime>
#include <string>

namespace mado::common {

namespace {

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

std::string current_timestamp_utc() {
    return format_timestamp_utc(std::time(nullptr));
}

std::string format_timestamp_utc(std::time_t t) {
    std::tm tm = gmtime(t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
    return buf;
}

} // namespace mado::common
