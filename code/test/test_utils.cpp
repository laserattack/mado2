#include "test_utils.hpp"

#include <cstdio>
#include <exception>

// Returns the assertion counter for the current process. No exceptions.
size_t &total_tests() {
    static size_t count = 0;
    return count;
}

// Returns the failed assertion counter for the current process. No exceptions.
size_t &failed_tests() {
    static size_t count = 0;
    return count;
}

// Records one assertion and prints PASS or FAIL. Ignores output errors.
void test(bool check) {
    total_tests()++;
    if (!check) {
        failed_tests()++;
        fprintf(stderr, "FAIL\n");
    } else {
        fprintf(stderr, "PASS\n");
    }
}

// Runs all registered tests, counting assertion and exception failures.
// Returns 0 on success or 1 on failure.
int run_tests(const std::vector<Test_Case> &tests, const char *prefix) {
    for (const auto &test_case : tests) {
        printf("\n%s: %s\n", test_case.name, test_case.description);
        try {
            test_case.func();
        } catch (const std::exception &e) {
            fprintf(stderr, "%s: %s\n", test_case.name, e.what());
            test(false);
        } catch (...) {
            fprintf(stderr, "%s: unexpected exception\n", test_case.name);
            test(false);
        }
    }

    printf("\n%s(%zu / %zu) tests passed\n",
           prefix, total_tests() - failed_tests(), total_tests());

    return failed_tests() > 0 ? 1 : 0;
}
