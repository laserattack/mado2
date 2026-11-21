#include "test_utils.hpp"

#include <mado/common/glob_pattern.hpp>
#include <mado/common/text_utils.hpp>

#include <cstdlib>
#include <memory>
#include <utility>

using namespace mado::common;

static void test_glob_matching() {
    struct Match_Case {
        const char *pattern;
        const char *matching;
        const char *different;
    };
    const Match_Case cases[] = {
        {"", "", "x"}, {"*", "anything", nullptr},
        {"?", "a", ""}, {"?", "a", "ab"},
        {"a*b*c", "axbyc", "axbyd"}, {"a**", "a", "b"},
        {"[ab]", "a", "c"}, {"[]", nullptr, "a"},
        {R"(\*\?\[\]\\)", R"(*?[]\)", "other"},
        {R"([\*\?\[\]\\])", "*", "a"},
    };

    bool correct = true;
    for (const auto &item : cases) {
        Glob_Pattern pattern(item.pattern);
        if (item.matching)
            correct &= pattern.matches(item.matching) == true;
        if (item.different)
            correct &= pattern.matches(item.different) == false;
    }
    const Glob_Pattern bytes("a?b");
    test(correct && bytes.matches(std::string("a\0b", 3)) == true);
}

static void test_glob_long_patterns() {
    constexpr size_t LONG_PATTERN_LENGTH = 1024;
    const std::string literal(LONG_PATTERN_LENGTH, 'a');
    const Glob_Pattern pattern(literal.c_str());
    const Glob_Pattern wildcards((std::string(LONG_PATTERN_LENGTH, '?') + "*z").c_str());

    test(pattern.matches(literal) == true && pattern.matches(literal + "a") == false &&
         pattern.matches(literal.substr(1)) == false &&
         wildcards.matches(literal + "z") == true && wildcards.matches(literal) == false);
}

static void test_glob_syntax_errors() {
    struct Error_Case {
        const char *pattern;
        const char *error;
        size_t location;
    };
    const Error_Case cases[] = {
        {R"(ab\)", GLOB_ERROR_UNFINISHED_ESCAPE, 3},
        {R"(ab\x)", GLOB_ERROR_INVALID_ESCAPE, 3},
        {R"([a\)", GLOB_ERROR_UNFINISHED_ESCAPE, 3},
        {R"([a\x])", GLOB_ERROR_INVALID_ESCAPE, 3},
        {"[ab", GLOB_ERROR_UNCLOSED_BLOCK, 3},
        {"[a*]", GLOB_ERROR_UNESCAPED_SPECIAL, 2},
        {"[a?]", GLOB_ERROR_UNESCAPED_SPECIAL, 2},
        {"[a[]", GLOB_ERROR_UNESCAPED_SPECIAL, 2},
        {"]", GLOB_ERROR_UNESCAPED_SPECIAL, 0},
    };

    bool correct = true;
    for (const auto &item : cases) {
        Glob glob{};
        const auto result = glob_compile(item.pattern, &glob);
        const std::unique_ptr<Glob_Cmd, decltype(&std::free)> storage(glob.items, &std::free);
        const Glob_Pattern pattern(item.pattern);
        correct &= result.error == item.error && result.location == item.location &&
                   !pattern.matches("anything").has_value();
    }
    test(correct);
}

static void test_trim() {
    const std::pair<const char *, const char *> cases[] = {
        {"", ""}, {" \t\r\n", ""}, {"value", "value"},
        {" \tvalue\r\n", "value"}, {"a b", "a b"}, {"\n a b \t", "a b"},
        {"\u00a0value\u00a0", "\u00a0value\u00a0"},
    };

    bool correct = true;
    for (const auto &[value, expected] : cases) {
        std::string inplace = value;
        trim_inplace(inplace);
        correct &= trim(value) == expected && inplace == expected;
    }
    test(correct);
}

static void test_lowercase_invalid_utf8() {
    bool correct = true;
    for (const std::string &invalid : {std::string("\xff"), std::string("\xc3"),
                                      std::string("\xc0\xaf"), std::string("\xed\xa0\x80"),
                                      std::string("\xf4\x90\x80\x80")}) {
        correct &= utf8_tolower("A" + invalid + "Я") == "a" + invalid + "я";
    }
    test(correct && utf8_tolower(std::string("A\0B", 3)) == std::string("a\0b", 3));
}

static void test_timestamp_validation() {
    bool correct = true;
    for (const std::string value : {"0000", "20000229", "20240229", "19000228",
                                    "20260430", "20261231-235959"})
        correct &= is_timestamp(value);
    for (const std::string value : {"", "202", "20240", "2024xx", "202400", "202413",
                                    "202402xx", "20240200", "20240230", "19000229",
                                    "20230229", "20260431", "20240101T", "20240101-xx",
                                    "20240101-24", "20240101-12xx", "20240101-1260",
                                    "20240101-1234xx", "20240101-123460", "20240101-1234567"})
        correct &= !is_timestamp(value);
    test(correct);
}

int main() {
    const std::vector<Test_Case> tests = {
        {"Glob matching", "wildcards, sets, escapes and full byte matching", test_glob_matching},
        {"Long glob patterns", "grow command storage and backtrack through wildcards", test_glob_long_patterns},
        {"Glob syntax errors", "error types, byte positions and failed wrapper matching", test_glob_syntax_errors},
        {"Trim", "empty, whitespace, padded and unchanged strings", test_trim},
        {"Invalid UTF-8", "preserve malformed bytes while lowercasing adjacent text", test_lowercase_invalid_utf8},
        {"Timestamps", "calendar boundaries, digit fields and separators", test_timestamp_validation},
    };
    return run_tests(tests, "common: ");
}
