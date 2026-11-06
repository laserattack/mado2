#include <mado/common/time_utils.hpp>

#include <cerrno>
#include <stdexcept>
#include <system_error>

namespace mado::common {

std::tm utc_time(std::time_t time) {
    std::tm result{};
#if defined(_WIN32)
    const int error = gmtime_s(&result, &time);
#else
    const int error = gmtime_r(&time, &result) ? 0 : (errno ? errno : EOVERFLOW);
#endif
    if (error)
        throw std::runtime_error("Cannot convert time to UTC: " +
                                 std::error_code(error, std::generic_category()).message());
    return result;
}

std::string current_timestamp_utc() {
    const auto now = std::time(nullptr);
    if (now == std::time_t(-1))
        throw std::runtime_error("Cannot read the current time");
    return format_timestamp_utc(now);
}

std::string format_timestamp_utc(std::time_t t) {
    const auto tm = utc_time(t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
    return buf;
}

} // namespace mado::common
