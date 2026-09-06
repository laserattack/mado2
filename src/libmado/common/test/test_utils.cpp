#include "test_utils.hpp"

#include <cstdio>

namespace mado::common {

void test(bool check, const char *description) {
    if (!check) {
        failed_tests()++;
        fprintf(stderr, "%-50s FAIL\n", description);
    } else {
        fprintf(stderr, "%-50s PASS\n", description);
    }
}

int run_tests(const std::vector<Test_Case> &tests) {
    for (const auto &test_case : tests) {
        printf("\n--- %s: %s ---\n", test_case.name, test_case.description);
        test_case.func();
    }

    printf("\n(%zu / %zu) tests passed\n",
           tests.size() - failed_tests(), tests.size());

    return failed_tests() > 0 ? 1 : 0;
}

} // namespace mado::common
