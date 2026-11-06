#include "test_utils.hpp"

#include <cstdio>
#include <exception>

// Returns the assertion counter for the current process. No exceptions.
// Возвращает счётчик проверок текущего процесса. Исключений нет.
size_t &total_tests() {
    static size_t count = 0;
    return count;
}

// Returns the failed assertion counter for the current process. No exceptions.
// Возвращает счётчик неудачных проверок текущего процесса. Исключений нет.
size_t &failed_tests() {
    static size_t count = 0;
    return count;
}

// Records one assertion and prints PASS or FAIL. Ignores output errors.
// Регистрирует проверку и печатает PASS или FAIL. Ошибки вывода игнорируются.
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
// Запускает все зарегистрированные тесты. Возвращает 0 при успехе или 1 при провале проверки
// либо исключении.
int run_tests(const std::vector<Test_Case> &tests, const char *prefix) {
    // Report each test independently and continue after an exception.
    // Отчитываемся по каждому тесту отдельно и продолжаем после исключения.
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
