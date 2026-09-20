#include "test_utils.hpp"

#include <mado/interpreter/entry.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>

using namespace mado::interpreter;
using namespace mado::query;

// helpers

static std::unique_ptr<Ast_Node> parse(const std::string &query) {
    Lexer lexer(query);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    return parser.parse();
}

// test

static void test_match_null_filter() {
    Entry e;
    test(e.match_filter(nullptr));
}

static void test_match_all() {
    Entry e;
    auto ast = parse("all");
    test(e.match_filter(ast.get()));
}

static void test_match_untagged_default() {
    Entry e;
    auto ast = parse("untagged");
    test(e.match_filter(ast.get()));
}

static void test_match_untagged_tagged() {
    Entry e;
    e.set_tags({"bug"});
    auto ast = parse("untagged");
    test(!e.match_filter(ast.get()));
}

static void test_match_unstatused_default() {
    Entry e;
    auto ast = parse("unstatused");
    test(e.match_filter(ast.get()));
}

static void test_match_unstatused_statused() {
    Entry e;
    e.set_status("opened");
    auto ast = parse("unstatused");
    test(!e.match_filter(ast.get()));
}

static void test_match_unnamed_default() {
    Entry e;
    auto ast = parse("unnamed");
    test(e.match_filter(ast.get()));
}

static void test_match_unnamed_named() {
    Entry e;
    e.set_name("bug");
    auto ast = parse("unnamed");
    test(!e.match_filter(ast.get()));
}

static void test_match_unprioritized_default() {
    Entry e;
    auto ast = parse("unprioritized");
    test(e.match_filter(ast.get()));
}

static void test_match_unprioritized_prioritized() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("unprioritized");
    test(!e.match_filter(ast.get()));
}

static void test_match_undeadlined_default() {
    Entry e;
    auto ast = parse("undeadlined");
    test(e.match_filter(ast.get()));
}

static void test_match_undeadlined_deadlined() {
    Entry e;
    e.set_deadline("20260101-000000");
    auto ast = parse("undeadlined");
    test(!e.match_filter(ast.get()));
}

static void test_match_and_true() {
    Entry e;
    e.set_priority(5);
    e.set_name("bug");
    auto ast = parse("priority > 3 and name = bug");
    test(e.match_filter(ast.get()));
}

static void test_match_or_true() {
    Entry e;
    e.set_priority(5);
    e.set_name("bug");
    auto ast = parse("priority = 67 or name = bug");
    test(e.match_filter(ast.get()));
}

static void test_match_xor_true() {
    Entry e;
    e.set_priority(1);
    e.set_name("bug");
    auto ast = parse("priority > 3 xor name = bug");
    test(e.match_filter(ast.get()));
}

// unary

static void test_match_not_true() {
    Entry e;
    e.set_name("bug");
    auto ast = parse("not name = bug");
    test(!e.match_filter(ast.get()));
}

// tags

static void test_match_tag_eq_any() {
    Entry e;
    e.set_tags({"bug", "crit"});
    auto ast = parse("tag = crit");
    test(e.match_filter(ast.get()));
}

static void test_match_tag_eq_none() {
    Entry e;
    e.set_tags({"bug", "crit"});
    auto ast = parse("tag = feature");
    test(!e.match_filter(ast.get()));
}

static void test_match_tag_empty_default() {
    Entry e;
    auto ast = parse("tag = ''");
    test(e.match_filter(ast.get()));
}

static void test_match_name_empty_eq() {
    Entry e;
    auto ast = parse("name = ''");
    test(e.match_filter(ast.get()));
}

static void test_match_name_empty_ne() {
    Entry e;
    e.set_name("bug");
    auto ast = parse("name = ''");
    test(!e.match_filter(ast.get()));
}

static void test_match_any_name() {
    Entry e;
    e.set_name("test");
    auto ast = parse("any = test");
    test(e.match_filter(ast.get()));
}

static void test_match_any_number_as_name() {
    Entry e;
    e.set_name("5");
    auto ast = parse("any = 5");
    test(e.match_filter(ast.get()));
}

static void test_match_any_time_as_name() {
    Entry e;
    e.set_name("20260920-101010");
    auto ast = parse("any = 20260920-101010");
    test(e.match_filter(ast.get()));
}

static void test_match_priority_eq() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("priority = 5");
    test(e.match_filter(ast.get()));
}

static void test_match_priority_gt() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("priority > 3");
    test(e.match_filter(ast.get()));
}

static void test_match_status() {
    Entry e;
    e.set_status("opened");
    auto ast = parse("status = opened");
    test(e.match_filter(ast.get()));
}

static void test_match_path() {
    Entry e;
    e.set_path("/home/user/MADO/20260101T120000/MAIN.md");
    auto ast = parse("path ~ MADO");
    test(e.match_filter(ast.get()));
}

static void test_match_deadline() {
    Entry e;
    e.set_deadline("20260201-000000");
    auto ast = parse("deadline > 20260101-000000");
    test(e.match_filter(ast.get()));
}

static void test_match_xor_both_true() {
    Entry e;
    e.set_priority(5);
    e.set_name("bug");
    auto ast = parse("priority > 3 xor name = bug");
    test(!e.match_filter(ast.get()));
}

static void test_fuzzy() {
    Entry e;
    e.set_name("fix bug in lexer");
    auto ast = parse("name ~~ fbeer");
    test(e.match_filter(ast.get()));
}

static void test_match_not_false() {
    Entry e;
    e.set_name("feature");
    auto ast = parse("not name = bug");
    test(e.match_filter(ast.get()));
}

static void test_match_any_number_as_priority() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("any = 5");
    test(e.match_filter(ast.get()));
}

static void test_match_tag_substr() {
    Entry e;
    e.set_tags({"critical"});
    auto ast = parse("tag ~ crit");
    test(e.match_filter(ast.get()));
}

// entry point

int main() {
    std::vector<Test_Case> tests = {
        {"Null filter", "match_filter(nullptr) = true", test_match_null_filter},
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
        {"Xor both true", "xor both true = false", test_match_xor_both_true},
        {"Fuzzy name", "name ~~ fbeer", test_fuzzy},
        {"Not false", "not name = bug on feature", test_match_not_false},
        {"Any number as priority", "any = 5 matches priority", test_match_any_number_as_priority},
        {"Tag substr", "tag ~ crit", test_match_tag_substr},
    };
    return run_tests(tests);
}
