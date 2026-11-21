#include "test_utils.hpp"

#include <mado/entry/entry.hpp>
#include <mado/interpreter/interpreter.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>

#include <array>
#include <cstdio>

using namespace mado::entry;
using namespace mado::interpreter;
using namespace mado::query;

// helpers

static std::unique_ptr<Ast_Node> parse(const std::string &query) {
    Lexer lexer(query);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    return parser.parse();
}

static bool matches(const Entry &e, const Ast_Node *filter) {
    const Interpreter interp;
    return interp.evaluate(filter, e);
}

struct Comparison_Case {
    const char *op;
    std::array<bool, 3> expected; // Entry value below, equal to, or above the operand.
};

static constexpr Comparison_Case COMPARISON_CASES[] = {
    {"=", {false, true, false}}, {"!=", {true, false, true}},
    {">", {false, false, true}}, {"<", {true, false, false}},
    {">=", {false, true, true}}, {"<=", {true, true, false}},
    {"~", {false, true, false}}, {"!~", {true, false, true}},
    {"^~", {false, true, false}}, {"!^~", {true, false, true}},
    {"$~", {false, true, false}}, {"!$~", {true, false, true}},
    {"~~", {false, true, false}}, {"!~~", {true, false, true}},
    {"%~", {false, true, false}}, {"!%~", {true, false, true}},
};

static constexpr const char *TIMESTAMP_VALUES[] = {
    "20260201-121212", "20260202-121212", "20260203-121212",
};

static void set_string_field(Entry &entry, const std::string &field,
                            const std::string &value) {
    if (field == "body")
        entry.set_body(value);
    else if (field == "name")
        entry.set_name(value);
    else if (field == "tag")
        entry.set_tags({value});
    else if (field == "status")
        entry.set_status(value);
    else if (field == "path")
        entry.set_path(value);
    else if (field == "time")
        entry.set_time(value);
    else if (field == "mtime")
        entry.set_mtime(value);
    else if (field == "deadline")
        entry.set_deadline(value);
}

static Entry entry_with_string_values(const std::string &value) {
    Entry entry;
    for (const auto *field : {"body", "name", "tag", "status", "path", "time", "mtime", "deadline"})
        set_string_field(entry, field, value);
    return entry;
}

static bool query_matches_expected(const Entry &entry, const std::string &query,
                                   bool expected) {
    const auto ast = parse(query);
    const bool actual = matches(entry, ast.get());
    if (actual != expected)
        std::fprintf(stderr, "Query '%s': expected %d, got %d\n",
                     query.c_str(), expected, actual);
    return actual == expected;
}

// test

static void test_match_field_operator_matrix() {
    bool correct = true;
    for (const std::string field : {"priority", "body", "name", "tag", "status", "path",
                                    "time", "mtime", "deadline", "any"}) {
        for (const auto &comparison : COMPARISON_CASES) {
            const std::string operand = field == "priority" ? "5" : TIMESTAMP_VALUES[1];
            const std::string query = field + " " + comparison.op + " " + operand;
            for (size_t i = 0; i < comparison.expected.size(); ++i) {
                Entry entry = entry_with_string_values(TIMESTAMP_VALUES[i]);
                entry.set_priority(static_cast<uint16_t>(4 + i));
                correct &= query_matches_expected(entry, query, comparison.expected[i]);
            }
        }
    }
    test(correct);
}

static void test_match_any_string_field_routes() {
    bool correct = true;
    for (const std::string field : {"name", "tag", "status", "path", "time", "mtime", "deadline"}) {
        for (const auto &comparison : COMPARISON_CASES) {
            size_t matching_index = 0;
            while (!comparison.expected[matching_index])
                ++matching_index;
            size_t missing_index = 0;
            while (comparison.expected[missing_index])
                ++missing_index;

            Entry entry = entry_with_string_values(TIMESTAMP_VALUES[missing_index]);
            const std::string query = std::string("any ") + comparison.op + " " + TIMESTAMP_VALUES[1];
            correct &= query_matches_expected(entry, query, false);

            set_string_field(entry, field, TIMESTAMP_VALUES[matching_index]);
            correct &= query_matches_expected(entry, query, true);
        }
    }
    test(correct);
}

static void test_match_any_numeric_operators() {
    const std::pair<const char *, std::array<bool, 3>> cases[] = {
        {"=", {false, true, false}}, {"!=", {true, true, true}},
        {">", {false, false, true}}, {"<", {true, true, true}},
        {">=", {false, true, true}}, {"<=", {true, true, true}},
        {"~", {false, true, false}}, {"!~", {true, true, true}},
        {"^~", {false, true, false}}, {"!^~", {true, true, true}},
        {"$~", {false, true, false}}, {"!$~", {true, true, true}},
        {"~~", {false, true, false}}, {"!~~", {true, true, true}},
        {"%~", {false, true, false}}, {"!%~", {true, true, true}},
    };
    bool correct = true;
    for (const auto &[op, expected] : cases) {
        for (size_t i = 0; i < expected.size(); ++i) {
            Entry entry = entry_with_string_values("20000101");
            entry.set_priority(static_cast<uint16_t>(4 + i));
            correct &= query_matches_expected(entry, std::string("any ") + op + " 5", expected[i]);
        }
    }

    for (const auto *field : {"name", "tag", "status", "path"}) {
        Entry entry = entry_with_string_values("20000101");
        set_string_field(entry, field, "5");
        correct &= query_matches_expected(entry, "any = 005", true) &&
                   query_matches_expected(entry, "any = '005'", false);
    }

    Entry priority_only = entry_with_string_values("20000101");
    priority_only.set_priority(5);
    correct &= query_matches_expected(priority_only, "any = 5", true) &&
               query_matches_expected(priority_only, "any = '5'", false);
    test(correct);
}

static void test_match_any_numeric_text_patterns() {
    const std::pair<const char *, const char *> cases[] = {
        {"~", "abc5def"}, {"^~", "5abc"}, {"$~", "abc5"},
        {"~~", "abc5def"}, {"%~", "5"}, {">", "6"}, {">=", "5"},
    };
    bool correct = true;
    for (const auto *field : {"name", "tag", "status", "path"}) {
        for (const auto &[op, value] : cases) {
            Entry entry = entry_with_string_values("20000101");
            const std::string query = std::string("any ") + op + " 005";
            correct &= query_matches_expected(entry, query, false);

            set_string_field(entry, field, value);
            correct &= query_matches_expected(entry, query, true);
        }
    }
    test(correct);
}

static void test_match_timestamp_partial_patterns() {
    const std::pair<const char *, bool> cases[] = {
        {"= 2026", false}, {"!= 2026", true},
        {"> 2026", true}, {"< 2027", true},
        {"~ 202602", true}, {"!~ 202602", false},
        {"~ 202603", false}, {"!~ 202603", true},
        {"^~ 2026", true}, {"!^~ 2026", false},
        {"^~ 2027", false}, {"!^~ 2027", true},
        {"$~ 121212", true}, {"!$~ 121212", false},
        {"$~ 121211", false}, {"!$~ 121211", true},
        {"~~ 202212", true}, {"!~~ 202212", false},
        {"~~ 9999", false}, {"!~~ 9999", true},
    };
    const Entry entry = entry_with_string_values(TIMESTAMP_VALUES[1]);
    bool correct = true;
    for (const std::string field : {"time", "mtime", "deadline"}) {
        for (const auto &[suffix, expected] : cases)
            correct &= query_matches_expected(entry, field + " " + suffix, expected);
    }
    test(correct);
}

static void test_match_unquoted_unicode() {
    Entry entry;
    entry.set_name("что такое Unicode");
    entry.set_tags({"漢字"});
    entry.set_status("открыто");
    const auto ast = parse("name ~~ что and tag = 漢字 and status = открыто");

    test(matches(entry, ast.get()));
}

static void test_match_collections_by_field() {
    bool correct = true;
    for (const std::string field : {"priority", "body", "name", "tag", "status", "path",
                                    "time", "mtime", "deadline", "any"}) {
        const bool numeric = field == "priority";
        const std::string low = numeric ? "4" : TIMESTAMP_VALUES[0];
        const std::string equal = numeric ? "5" : TIMESTAMP_VALUES[1];
        const std::string high = numeric ? "6" : TIMESTAMP_VALUES[2];
        const std::pair<std::string, bool> cases[] = {
            {"= anyof(" + low + "," + equal + ")", true},
            {"= anyof(" + low + "," + high + ")", false},
            {"= allof(" + equal + "," + equal + ")", true},
            {"= allof(" + low + "," + equal + ")", false},
            {"> anyof(" + low + "," + high + ")", true},
            {"> allof(" + low + "," + equal + ")", false},
            {"!= allof(" + low + "," + high + ")", true},
            {"!= anyof(" + equal + "," + equal + ")", false},
            {"in (" + low + "," + equal + ")", true},
            {"in (" + low + "," + high + ")", false},
            {"has (" + equal + "," + equal + ")", true},
            {"has (" + low + "," + equal + ")", false},
            {"in [" + low + ".." + high + "]", true},
            {"in [" + equal + ".." + equal + "]", true},
            {"in [" + low + ".." + equal + "]", true},
            {"in [" + equal + ".." + high + "]", true},
            {"in [" + high + ".." + low + "]", false},
            {"in [" + equal + "..]", true},
            {"in [.." + equal + "]", true},
            {"in [" + high + "..]", false},
            {"in [.." + low + "]", false},
        };
        Entry entry = entry_with_string_values(TIMESTAMP_VALUES[1]);
        entry.set_priority(5);
        for (const auto &[suffix, expected] : cases)
            correct &= query_matches_expected(entry, field + " " + suffix, expected);
    }
    test(correct);
}

static void test_match_multiple_tag_operators() {
    const std::pair<const char *, bool> cases[] = {
        {"tag = 漢字", true}, {"tag != 漢字", true},
        {"tag ~ 漢", true}, {"tag !~ 漢", true},
        {"tag ~~ 漢字", true}, {"tag !~~ 漢字", true},
        {"tag ^~ 漢", true}, {"tag !^~ 漢", true},
        {"tag $~ 字", true}, {"tag !$~ 字", true},
        {"tag %~ '漢*'", true}, {"tag !%~ '漢*'", true},
        {"tag > 漢字", false}, {"tag < 漢字", true},
        {"tag >= 漢字", true}, {"tag <= 漢字", true},
        {"not (tag = 漢字)", false}, {"not (tag ~ 漢)", false},
        {"tag has (漢字,🙂)", false},
        {"tag has (漢字,ЛАЛА)", true},
        {"tag = allof(漢字,ЛАЛА)", true},
        {"tag = anyof(🙂,漢字)", true},
        {"tag in [🙂..]", false},
    };
    bool correct = true;
    for (const auto &tags : {std::vector<std::string>{"ЛаЛа", "漢字"},
                             std::vector<std::string>{"漢字", "ЛаЛа"}}) {
        Entry entry;
        entry.set_tags(tags);
        for (const auto &[query, expected] : cases)
            correct &= query_matches_expected(entry, query, expected);
    }
    test(correct);
}

static void test_match_numeric_any_boundaries() {
    Entry lower = entry_with_string_values("9999");
    lower.set_priority(4);
    Entry equal = lower;
    equal.set_priority(5);
    Entry higher = lower;
    higher.set_priority(6);

    test(query_matches_expected(lower, "any < 5", true) &&
         query_matches_expected(equal, "any < 5", false) &&
         query_matches_expected(higher, "any < 5", false) &&
         query_matches_expected(lower, "any <= 5", true) &&
         query_matches_expected(equal, "any <= 5", true) &&
         query_matches_expected(higher, "any <= 5", false) &&
         query_matches_expected(equal, "any != 5", true) &&
         query_matches_expected(equal, "not (any = 5)", false));
}

static void test_match_priority_limits() {
    Entry minimum;
    Entry maximum;
    maximum.set_priority(MAX_PRIORITY);
    const auto limit = std::to_string(MAX_PRIORITY);

    test(query_matches_expected(minimum, "priority = 0", true) &&
         query_matches_expected(minimum, "priority < 0", false) &&
         query_matches_expected(minimum, "priority >= 0", true) &&
         query_matches_expected(maximum, "priority = " + limit, true) &&
         query_matches_expected(maximum, "priority > " + limit, false) &&
         query_matches_expected(maximum, "priority <= " + limit, true));
}

static void test_match_logic_truth_tables() {
    const std::pair<const char *, std::array<bool, 4>> cases[] = {
        {"name = 漢字 and status = открыто", {false, false, false, true}},
        {"name = 漢字 or status = открыто", {false, true, true, true}},
        {"name = 漢字 xor status = открыто", {false, true, true, false}},
        {"not name = 漢字", {true, true, false, false}},
        {"not not name = 漢字", {false, false, true, true}},
        {"not (name = 漢字 and status = открыто)", {true, true, true, false}},
        {"name = 漢字 or status = открыто and priority = 5", {false, false, true, true}},
        {"(name = 漢字 or status = открыто) and priority = 5", {false, false, false, false}},
        {"name = 漢字 xor status = открыто and priority = 5", {false, false, true, true}},
        {"all or name = 漢字 xor status = открыто", {true, true, true, true}},
        {"all xor name = 漢字 xor status = открыто", {true, false, false, true}},
    };
    bool correct = true;
    for (size_t i = 0; i < 4; ++i) {
        Entry entry;
        entry.set_name(i >= 2 ? "漢字" : "🙂");
        entry.set_status(i % 2 != 0 ? "открыто" : "закрыто");
        for (const auto &[query, expected] : cases)
            correct &= query_matches_expected(entry, query, expected[i]);
    }
    test(correct);
}

static void test_match_unicode_field_operators() {
    const std::pair<const char *, bool> cases[] = {
        {"= лала🙂", true}, {"= 漢字", false},
        {"!= лала🙂", false}, {"!= 漢字", true},
        {"~ ла🙂", true}, {"!~ ла🙂", false},
        {"~ 漢字", false}, {"!~ 漢字", true},
        {"~~ лл🙂", true}, {"!~~ лл🙂", false},
        {"~~ 漢字", false}, {"!~~ 漢字", true},
        {"^~ лала", true}, {"!^~ лала", false},
        {"^~ ла🙂", false}, {"!^~ ла🙂", true},
        {"$~ ла🙂", true}, {"!$~ ла🙂", false},
        {"$~ лала", false}, {"!$~ лала", true},
        {"%~ лала🙂", true}, {"!%~ лала🙂", false},
        {"%~ 'ла*🙂'", true}, {"!%~ 'ла*🙂'", false},
        {"%~ '漢*'", false}, {"!%~ '漢*'", true},
        {"> а", true}, {"> я", false}, {"> лала🙂", false},
        {"< я", true}, {"< а", false}, {"< лала🙂", false},
        {">= а", true}, {">= лала🙂", true}, {">= я", false},
        {"<= я", true}, {"<= лала🙂", true}, {"<= а", false},
        {"in (漢字,лала🙂)", true}, {"has (лала🙂)", true},
        {"in [а..я]", true}, {"in [..а]", false},
    };
    bool correct = true;
    for (const std::string field : {"name", "tag", "status", "path"}) {
        Entry entry;
        if (field == "name")
            entry.set_name("ЛаЛа🙂");
        else if (field == "tag")
            entry.set_tags({"ЛаЛа🙂"});
        else if (field == "status")
            entry.set_status("ЛаЛа🙂");
        else
            entry.set_path(std::filesystem::path(u8"ЛаЛа🙂"));

        for (const auto &[suffix, expected] : cases) {
            const auto ast = parse(field + " " + suffix);
            correct &= matches(entry, ast.get()) == expected;
        }
        const auto any = parse("any = лала🙂");
        const auto missing = parse("any = 漢字");
        correct &= matches(entry, any.get()) && !matches(entry, missing.get());
    }
    test(correct);
}

static void test_match_unicode_values_preserved() {
    bool correct = true;
    for (const std::string value : {"漢字", "🙂", "é", "👩‍💻", "42что", "что，漢字"}) {
        Entry entry;
        entry.set_name(value);
        const auto unquoted = parse("name = " + value);
        const auto quoted = parse("name = '" + value + "'");
        const auto different = parse("name = другое");
        correct &= matches(entry, unquoted.get()) && matches(entry, quoted.get()) &&
                   !matches(entry, different.get());
    }
    test(correct);
}

static void test_match_null_filter() {
    Entry e;
    test(matches(e, nullptr));
}

static void test_match_all() {
    Entry e;
    auto ast = parse("all");
    test(matches(e, ast.get()));
}

static void test_match_untagged_default() {
    Entry e;
    auto ast = parse("untagged");
    test(matches(e, ast.get()));
}

static void test_match_untagged_tagged() {
    Entry e;
    e.set_tags({"bug"});
    auto ast = parse("untagged");
    test(!matches(e, ast.get()));
}

static void test_match_unstatused_default() {
    Entry e;
    auto ast = parse("unstatused");
    test(matches(e, ast.get()));
}

static void test_match_unstatused_statused() {
    Entry e;
    e.set_status("opened");
    auto ast = parse("unstatused");
    test(!matches(e, ast.get()));
}

static void test_match_unnamed_default() {
    Entry e;
    auto ast = parse("unnamed");
    test(matches(e, ast.get()));
}

static void test_match_unnamed_named() {
    Entry e;
    e.set_name("bug");
    auto ast = parse("unnamed");
    test(!matches(e, ast.get()));
}

static void test_match_unprioritized_default() {
    Entry e;
    auto ast = parse("unprioritized");
    test(matches(e, ast.get()));
}

static void test_match_unprioritized_prioritized() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("unprioritized");
    test(!matches(e, ast.get()));
}

static void test_match_undeadlined_default() {
    Entry e;
    auto ast = parse("undeadlined");
    test(matches(e, ast.get()));
}

static void test_match_undeadlined_deadlined() {
    Entry e;
    e.set_deadline("20260101-000000");
    auto ast = parse("undeadlined");
    test(!matches(e, ast.get()));
}

static void test_match_and_true() {
    Entry e;
    e.set_priority(5);
    e.set_name("bug");
    auto ast = parse("priority > 3 and name = bug");
    test(matches(e, ast.get()));
}

static void test_match_or_true() {
    Entry e;
    e.set_priority(5);
    e.set_name("bug");
    auto ast = parse("priority = 67 or name = bug");
    test(matches(e, ast.get()));
}

static void test_match_xor_true() {
    Entry e;
    e.set_priority(1);
    e.set_name("bug");
    auto ast = parse("priority > 3 xor name = bug");
    test(matches(e, ast.get()));
}

// unary

static void test_match_not_true() {
    Entry e;
    e.set_name("bug");
    auto ast = parse("not name = bug");
    test(!matches(e, ast.get()));
}

// tags

static void test_match_tag_eq_any() {
    Entry e;
    e.set_tags({"bug", "crit"});
    auto ast = parse("tag = crit");
    test(matches(e, ast.get()));
}

static void test_match_tag_eq_none() {
    Entry e;
    e.set_tags({"bug", "crit"});
    auto ast = parse("tag = feature");
    test(!matches(e, ast.get()));
}

static void test_match_tag_empty_default() {
    Entry e;
    auto ast = parse("tag = ''");
    test(matches(e, ast.get()));
}

static void test_match_name_empty_eq() {
    Entry e;
    auto ast = parse("name = ''");
    test(matches(e, ast.get()));
}

static void test_match_name_empty_ne() {
    Entry e;
    e.set_name("bug");
    auto ast = parse("name = ''");
    test(!matches(e, ast.get()));
}

static void test_match_any_name() {
    Entry e;
    e.set_name("test");
    auto ast = parse("any = test");
    test(matches(e, ast.get()));
}

static void test_match_any_number_as_name() {
    Entry e;
    e.set_name("5");
    auto ast = parse("any = 5");
    test(matches(e, ast.get()));
}

static void test_match_any_time_as_name() {
    Entry e;
    e.set_name("20260920-101010");
    auto ast = parse("any = 20260920-101010");
    test(matches(e, ast.get()));
}

static void test_match_priority_eq() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("priority = 5");
    test(matches(e, ast.get()));
}

static void test_match_priority_gt() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("priority > 3");
    test(matches(e, ast.get()));
}

static void test_match_status() {
    Entry e;
    e.set_status("opened");
    auto ast = parse("status = opened");
    test(matches(e, ast.get()));
}

static void test_match_path() {
    Entry e;
    e.set_path("/home/user/MADO/20260101-120000/MAIN.md");
    auto ast = parse("path ~ MADO");
    test(matches(e, ast.get()));
}

static void test_match_numeric_name_as_text() {
    Entry matching;
    matching.set_name("007");
    Entry other;
    other.set_name("7");
    auto ast = parse("name = 007");
    test(matches(matching, ast.get()) && !matches(other, ast.get()));
}

static void test_match_deadline_macro_range() {
    Entry inside;
    inside.set_deadline("99981231-120000");
    Entry before;
    before.set_deadline("99981230-120000");
    Entry after;
    after.set_deadline("99990101-120000");
    auto ast = parse("deadline in [@never-1..@never]");
    test(matches(inside, ast.get()) && !matches(before, ast.get()) &&
         !matches(after, ast.get()));
}

static void test_match_deadline() {
    Entry e;
    e.set_deadline("20260201-000000");
    auto ast = parse("deadline > 20260101-000000");
    test(matches(e, ast.get()));
}

static void test_match_xor_both_true() {
    Entry e;
    e.set_priority(5);
    e.set_name("bug");
    auto ast = parse("priority > 3 xor name = bug");
    test(!matches(e, ast.get()));
}

static void test_fuzzy() {
    Entry e;
    e.set_name("fix bug in lexer");
    auto ast = parse("name ~~ fbeer");
    test(matches(e, ast.get()));
}

static void test_glob() {
    Entry e;
    e.set_name("FIX-42");

    test(matches(e, parse(R"(name %~ "fix-[45]?*")").get()) &&
         !matches(e, parse(R"(name !%~ "fix-[45]?*")").get()) &&
         matches(e, parse(R"(name !%~ "fix-[67]?")").get()) &&
         !matches(e, parse(R"(name %~ "fix-[")").get()) &&
         !matches(e, parse(R"(name !%~ "fix-[")").get()));
}

static void test_match_not_false() {
    Entry e;
    e.set_name("feature");
    auto ast = parse("not name = bug");
    test(matches(e, ast.get()));
}

static void test_match_any_number_as_priority() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("any = 5");
    test(matches(e, ast.get()));
}

static void test_match_tag_substr() {
    Entry e;
    e.set_tags({"critical"});
    auto ast = parse("tag ~ crit");
    test(matches(e, ast.get()));
}

// utf8 case insensitive

static void test_match_name_unicode_ci_eq() {
    Entry e;
    e.set_name("ЛАЛАЛА");
    auto ast = parse("name = 'лалала'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_ne() {
    Entry e;
    e.set_name("ЛАЛАЛА");
    auto ast = parse("name != 'лалала'");
    test(!matches(e, ast.get()));
}

static void test_match_name_unicode_ci_ne_true() {
    Entry e;
    e.set_name("ЛАЛАЛА");
    auto ast = parse("name != 'ЛЯЛЯЛЯ'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_substr() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name ~ 'ала'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_nsubstr() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name !~ 'xyz'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_starts() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name ^~ 'лала'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_nstarts() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name !^~ 'xyz'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_ends() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name $~ 'лала'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_nends() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name !$~ 'xyz'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_fuzzy() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name ~~ 'ллл'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_nfuzzy() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name !~~ 'xyz'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_gt() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name > 'ааа'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_lt() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name < 'яяя'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_ge() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name >= 'лалала'");
    test(matches(e, ast.get()));
}

static void test_match_name_unicode_ci_le() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name <= 'лалала'");
    test(matches(e, ast.get()));
}

static void test_mismatched_comparison_values() {
    const Entry entry;
    bool correct = true;
    const auto text_priority = ast_make_comparison(Ast_Comparison_Field::Priority,
        Ast_Comparison_Operator::Eq, std::string("0"));
    correct &= !matches(entry, text_priority.get());

    for (const auto field : {Ast_Comparison_Field::Priority, Ast_Comparison_Field::Tag,
                             Ast_Comparison_Field::Status, Ast_Comparison_Field::Body,
                             Ast_Comparison_Field::Name, Ast_Comparison_Field::Path,
                             Ast_Comparison_Field::Time, Ast_Comparison_Field::Mtime,
                             Ast_Comparison_Field::Deadline}) {
        const auto empty_value = ast_make_comparison(field, Ast_Comparison_Operator::Eq, std::monostate{});
        correct &= !matches(entry, empty_value.get());
        if (field != Ast_Comparison_Field::Priority) {
            const auto numeric_value = ast_make_comparison(field, Ast_Comparison_Operator::Eq, uint16_t(0));
            correct &= !matches(entry, numeric_value.get());
        }
    }
    test(correct);
}

static void test_any_missing_value_throws() {
    const Entry entry;
    const auto ast = ast_make_comparison(Ast_Comparison_Field::Any,
        Ast_Comparison_Operator::Eq, std::monostate{});
    bool rejected = false;
    try {
        matches(entry, ast.get());
    } catch (const std::bad_variant_access &) {
        rejected = true;
    }

    test(rejected);
}

// entry point

int main() {
    std::vector<Test_Case> tests = {
        {"Mismatched values", "manually constructed comparisons reject incompatible variants", test_mismatched_comparison_values},
        {"Missing Any value", "propagate the documented variant error", test_any_missing_value_throws},
        {"All fields and comparison operators", "check lower, equal, and higher operands", test_match_field_operator_matrix},
        {"Any field routes", "isolate each string field for every operator", test_match_any_string_field_routes},
        {"Any numeric operators", "check priority and numeric text fallback", test_match_any_numeric_operators},
        {"Any numeric text patterns", "apply numeric operands to text with each matching operator", test_match_any_numeric_text_patterns},
        {"Timestamp patterns", "compare full timestamps with partial operands", test_match_timestamp_partial_patterns},
        {"Collections by field", "lists, quantifiers and inclusive range boundaries", test_match_collections_by_field},
        {"Multiple tag operators", "existential matching and distributed tag requirements", test_match_multiple_tag_operators},
        {"Any numeric boundaries", "isolate priority inequalities and negative matching", test_match_numeric_any_boundaries},
        {"Priority limits", "compare minimum and maximum priorities", test_match_priority_limits},
        {"Logical truth tables", "all boolean inputs, precedence and grouping", test_match_logic_truth_tables},
        {"Unicode field operators", "Unicode matching in each string field and any", test_match_unicode_field_operators},
        {"Unicode values preserved", "match complete Unicode literals", test_match_unicode_values_preserved},
        {"Null filter", "evaluate(nullptr) = true", test_match_null_filter},
        {"All", "all node", test_match_all},
        {"Untagged default", "untagged on default", test_match_untagged_default},
        {"Untagged tagged", "untagged on tagged", test_match_untagged_tagged},
        {"Unstatused default", "unstatused on default", test_match_unstatused_default},
        {"Unstatused statused", "unstatused on statused", test_match_unstatused_statused},
        {"Unnamed default", "unnamed on default", test_match_unnamed_default},
        {"Unnamed named", "unnamed on named", test_match_unnamed_named},
        {"Unprioritized default", "unprioritized on default", test_match_unprioritized_default},
        {"Unprioritized prioritized", "unprioritized on prioritized", test_match_unprioritized_prioritized},
        {"Undeadlined default", "undeadlined on default", test_match_undeadlined_default},
        {"Undeadlined deadlined", "undeadlined on deadlined", test_match_undeadlined_deadlined},
        {"And true", "priority > 3 and name = bug", test_match_and_true},
        {"Or true", "priority = 67 or name = bug", test_match_or_true},
        {"Xor true", "priority > 3 xor name = bug", test_match_xor_true},
        {"Not true", "not name = bug", test_match_not_true},
        {"Tag eq any", "tag = crit", test_match_tag_eq_any},
        {"Tag eq none", "tag = feature", test_match_tag_eq_none},
        {"Tag empty default", "tag = ''", test_match_tag_empty_default},
        {"Name empty eq", "name = '' on default", test_match_name_empty_eq},
        {"Name empty ne", "name = '' on named", test_match_name_empty_ne},
        {"Any name", "any = test, name matches", test_match_any_name},
        {"Any number as name", "any = 5, name matches", test_match_any_number_as_name},
        {"Any time as name", "any = 20260920-101010, name matches", test_match_any_time_as_name},
        {"Priority eq", "priority = 5", test_match_priority_eq},
        {"Priority gt", "priority > 3", test_match_priority_gt},
        {"Status", "status = opened", test_match_status},
        {"Path", "path ~ MADO", test_match_path},
        {"Deadline", "deadline > 20260101-000000", test_match_deadline},
        {"Numeric name", "name = 007 keeps leading zeroes", test_match_numeric_name_as_text},
        {"Deadline macro range", "match a range with macro bounds", test_match_deadline_macro_range},
        {"Xor both true", "xor both true = false", test_match_xor_both_true},
        {"Fuzzy name", "name ~~ fbeer", test_fuzzy},
        {"Glob name", "glob matching, negation and invalid patterns", test_glob},
        {"Not false", "not name = bug on feature", test_match_not_false},
        {"Any number as priority", "any = 5 matches priority", test_match_any_number_as_priority},
        {"Tag substr", "tag ~ crit", test_match_tag_substr},
        {"Unquoted Unicode", "Unicode values across fields", test_match_unquoted_unicode},
        {"Name unicode CI eq", "name = лалала matches ЛАЛАЛА", test_match_name_unicode_ci_eq},
        {"Name unicode CI ne", "name != лалала doesn't match ЛАЛАЛА", test_match_name_unicode_ci_ne},
        {"Name unicode CI ne true", "name != ЛЯЛЯЛЯ matches ЛАЛАЛА", test_match_name_unicode_ci_ne_true},
        {"Name unicode CI substr", "name ~ ала matches ЛаЛаЛа", test_match_name_unicode_ci_substr},
        {"Name unicode CI nsubstr", "name !~ xyz matches ЛаЛаЛа", test_match_name_unicode_ci_nsubstr},
        {"Name unicode CI starts", "name ^~ лала matches ЛаЛаЛа", test_match_name_unicode_ci_starts},
        {"Name unicode CI nstarts", "name !^~ xyz matches ЛаЛаЛа", test_match_name_unicode_ci_nstarts},
        {"Name unicode CI ends", "name $~ лала matches ЛаЛаЛа", test_match_name_unicode_ci_ends},
        {"Name unicode CI nends", "name !$~ xyz matches ЛаЛаЛа", test_match_name_unicode_ci_nends},
        {"Name unicode CI fuzzy", "name ~~ ллл matches ЛаЛаЛа", test_match_name_unicode_ci_fuzzy},
        {"Name unicode CI nfuzzy", "name !~~ xyz matches ЛаЛаЛа", test_match_name_unicode_ci_nfuzzy},
        {"Name unicode CI gt", "name > ааа matches ЛаЛаЛа", test_match_name_unicode_ci_gt},
        {"Name unicode CI lt", "name < яяя matches ЛаЛаЛа", test_match_name_unicode_ci_lt},
        {"Name unicode CI ge", "name >= лалала matches ЛаЛаЛа", test_match_name_unicode_ci_ge},
        {"Name unicode CI le", "name <= лалала matches ЛаЛаЛа", test_match_name_unicode_ci_le},
    };
    return run_tests(tests, "interpreter: ");
}
