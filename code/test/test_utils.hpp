#pragma once

#include <cstddef>
#include <vector>

// Associates a test function with its diagnostic labels.
struct Test_Case {
    const char *name;
    const char *description;
    void (*func)();
};

// Returns the assertion counter for the current process. No exceptions.
size_t &total_tests();
// Returns the failed assertion counter for the current process. No exceptions.
size_t &failed_tests();
// Records one assertion and prints PASS or FAIL. Ignores output errors.
void test(bool check);
// Runs all registered tests, counting assertion and exception failures.
// Returns 0 on success or 1 on failure.
int run_tests(const std::vector<Test_Case> &tests, const char *prefix);
