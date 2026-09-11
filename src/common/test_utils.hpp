#pragma once

#include <cstddef>
#include <vector>

namespace mado::common {

struct Test_Case {
    const char *name;
    const char *description;
    void (*func)();
};

size_t &total_tests();
size_t &failed_tests();
void test(bool check);
int run_tests(const std::vector<Test_Case> &tests);

} // namespace mado::common
