#pragma once

#include <cstddef>
#include <vector>

// Associates a test function with its diagnostic labels.
// Связывает тестовую функцию с её диагностическими метками.
struct Test_Case {
    const char *name;
    const char *description;
    void (*func)();
};

// Returns the assertion counter for the current process. No exceptions.
// Возвращает счётчик проверок текущего процесса. Исключений нет.
size_t &total_tests();
// Returns the failed assertion counter for the current process. No exceptions.
// Возвращает счётчик неудачных проверок текущего процесса. Исключений нет.
size_t &failed_tests();
// Records one assertion and prints PASS or FAIL. Ignores output errors.
// Регистрирует проверку и печатает PASS или FAIL. Ошибки вывода игнорируются.
void test(bool check);
// Runs all registered tests, counting assertion and exception failures.
// Returns 0 on success or 1 on failure.
// Запускает все зарегистрированные тесты. Возвращает 0 при успехе или 1 при провале проверки
// либо исключении.
int run_tests(const std::vector<Test_Case> &tests, const char *prefix);
