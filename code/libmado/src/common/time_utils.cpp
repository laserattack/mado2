#include <mado/common/time_utils.hpp>

namespace mado::common {

std::tm localtime_threadsafe(std::time_t t) {
    std::tm bt{};
#if defined(_MSC_VER)
    localtime_s(&bt, &t);
#else
    localtime_r(&t, &bt);
#endif
    return bt;
}

} // namespace mado::common
