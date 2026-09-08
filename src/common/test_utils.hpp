#pragma once

#include <cstddef>
#include <vector>

namespace mado::common {

struct Test_Case {
    const char *name;
    const char *description;
    void (*func)();
};

inline size_t &total_tests() {
    static size_t count = 0;
    return count;
}

inline size_t &failed_tests() {
    static size_t count = 0;
    return count;
}

void test(bool check);
int run_tests(const std::vector<Test_Case> &tests);

} // namespace mado::common
