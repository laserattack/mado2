#include "test_utils.hpp"

#include <cstdio>

namespace mado::common {

void test(bool check) {
    total_tests()++;
    if (!check) {
        failed_tests()++;
        fprintf(stderr, "FAIL\n");
    } else {
        fprintf(stderr, "PASS\n");
    }
}

int run_tests(const std::vector<Test_Case> &tests) {
    for (const auto &test_case : tests) {
        printf("\n%s: %s\n", test_case.name, test_case.description);
        test_case.func();
    }

    printf("\n(%zu / %zu) tests passed\n",
           total_tests() - failed_tests(), total_tests());

    return failed_tests() > 0 ? 1 : 0;
}

} // namespace mado::common
