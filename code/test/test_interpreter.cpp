#include "test_utils.hpp"

#include <mado/entry/entry.hpp>
#include <mado/interpreter/interpreter.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>

using namespace mado::entry;
using namespace mado::interpreter;
using namespace mado::query;

// helpers
// Вспомогательные функции.

// Builds the complete query AST. Throws Parse_Error for empty or invalid
// queries and std::bad_alloc on allocation failure.
// Строит AST запроса. Некорректный или пустой запрос вызывает Parse_Error, сбой выделения
// памяти — std::bad_alloc.
static std::unique_ptr<Ast_Node> parse(const std::string &query) {
    Lexer lexer(query);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    return parser.parse();
}

// Evaluates the filter against the fixture. Allocation failures propagate.
// Проверяет фильтр на тестовой записи. Ошибки выделения памяти передаются вызывающему коду.
static bool matches(const Entry &e, const Ast_Node *filter) {
    const Interpreter interp;
    return interp.evaluate(filter, e);
}

// test
// Тесты.

// Checks evaluate(nullptr) = true. Unexpected exceptions fail the test.
// Проверяет совпадение при нулевом фильтре. Неожиданное исключение проваливает тест.
static void test_match_null_filter() {
    Entry e;
    test(matches(e, nullptr));
}

// Checks all node. Unexpected exceptions fail the test.
// Проверяет совпадение с all. Неожиданное исключение проваливает тест.
static void test_match_all() {
    Entry e;
    auto ast = parse("all");
    test(matches(e, ast.get()));
}

// Checks untagged on default. Unexpected exceptions fail the test.
// Проверяет совпадение с untagged при отсутствии тегов. Неожиданное исключение проваливает
// тест.
static void test_match_untagged_default() {
    Entry e;
    auto ast = parse("untagged");
    test(matches(e, ast.get()));
}

// Checks untagged on tagged. Unexpected exceptions fail the test.
// Проверяет отсутствие совпадения с untagged при наличии тегов. Неожиданное исключение
// проваливает тест.
static void test_match_untagged_tagged() {
    Entry e;
    e.set_tags({"bug"});
    auto ast = parse("untagged");
    test(!matches(e, ast.get()));
}

// Checks unstatused on default. Unexpected exceptions fail the test.
// Проверяет совпадение с unstatused при отсутствии статуса. Неожиданное исключение проваливает
// тест.
static void test_match_unstatused_default() {
    Entry e;
    auto ast = parse("unstatused");
    test(matches(e, ast.get()));
}

// Checks unstatused on statused. Unexpected exceptions fail the test.
// Проверяет отсутствие совпадения с unstatused при наличии статуса. Неожиданное исключение
// проваливает тест.
static void test_match_unstatused_statused() {
    Entry e;
    e.set_status("opened");
    auto ast = parse("unstatused");
    test(!matches(e, ast.get()));
}

// Checks unnamed on default. Unexpected exceptions fail the test.
// Проверяет совпадение с unnamed при отсутствии имени. Неожиданное исключение проваливает
// тест.
static void test_match_unnamed_default() {
    Entry e;
    auto ast = parse("unnamed");
    test(matches(e, ast.get()));
}

// Checks unnamed on named. Unexpected exceptions fail the test.
// Проверяет отсутствие совпадения с unnamed при наличии имени. Неожиданное исключение
// проваливает тест.
static void test_match_unnamed_named() {
    Entry e;
    e.set_name("bug");
    auto ast = parse("unnamed");
    test(!matches(e, ast.get()));
}

// Checks unprioritized on default. Unexpected exceptions fail the test.
// Проверяет совпадение с unprioritized при приоритете по умолчанию. Неожиданное исключение
// проваливает тест.
static void test_match_unprioritized_default() {
    Entry e;
    auto ast = parse("unprioritized");
    test(matches(e, ast.get()));
}

// Checks unprioritized on prioritized. Unexpected exceptions fail the test.
// Проверяет отсутствие совпадения с unprioritized при заданном приоритете. Неожиданное
// исключение проваливает тест.
static void test_match_unprioritized_prioritized() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("unprioritized");
    test(!matches(e, ast.get()));
}

// Checks undeadlined on default. Unexpected exceptions fail the test.
// Проверяет совпадение с undeadlined при отсутствии срока. Неожиданное исключение проваливает
// тест.
static void test_match_undeadlined_default() {
    Entry e;
    auto ast = parse("undeadlined");
    test(matches(e, ast.get()));
}

// Checks undeadlined on deadlined. Unexpected exceptions fail the test.
// Проверяет отсутствие совпадения с undeadlined при наличии срока. Неожиданное исключение
// проваливает тест.
static void test_match_undeadlined_deadlined() {
    Entry e;
    e.set_deadline("20260101-000000");
    auto ast = parse("undeadlined");
    test(!matches(e, ast.get()));
}

// Checks priority > 3 and name = bug. Unexpected exceptions fail the test.
// Проверяет истинный AND. Неожиданное исключение проваливает тест.
static void test_match_and_true() {
    Entry e;
    e.set_priority(5);
    e.set_name("bug");
    auto ast = parse("priority > 3 and name = bug");
    test(matches(e, ast.get()));
}

// Checks priority = 67 or name = bug. Unexpected exceptions fail the test.
// Проверяет истинный OR. Неожиданное исключение проваливает тест.
static void test_match_or_true() {
    Entry e;
    e.set_priority(5);
    e.set_name("bug");
    auto ast = parse("priority = 67 or name = bug");
    test(matches(e, ast.get()));
}

// Checks priority > 3 xor name = bug. Unexpected exceptions fail the test.
// Проверяет истинный XOR. Неожиданное исключение проваливает тест.
static void test_match_xor_true() {
    Entry e;
    e.set_priority(1);
    e.set_name("bug");
    auto ast = parse("priority > 3 xor name = bug");
    test(matches(e, ast.get()));
}

// unary
// Унарные операции.

// Checks not name = bug. Unexpected exceptions fail the test.
// Проверяет истинный NOT. Неожиданное исключение проваливает тест.
static void test_match_not_true() {
    Entry e;
    e.set_name("bug");
    auto ast = parse("not name = bug");
    test(!matches(e, ast.get()));
}

// tags
// Теги.

// Checks tag = crit. Unexpected exceptions fail the test.
// Проверяет равенство хотя бы одного тега. Неожиданное исключение проваливает тест.
static void test_match_tag_eq_any() {
    Entry e;
    e.set_tags({"bug", "crit"});
    auto ast = parse("tag = crit");
    test(matches(e, ast.get()));
}

// Checks tag = feature. Unexpected exceptions fail the test.
// Проверяет отсутствие равного тега. Неожиданное исключение проваливает тест.
static void test_match_tag_eq_none() {
    Entry e;
    e.set_tags({"bug", "crit"});
    auto ast = parse("tag = feature");
    test(!matches(e, ast.get()));
}

// Checks tag = ''. Unexpected exceptions fail the test.
// Проверяет сравнение с пустым тегом по умолчанию. Неожиданное исключение проваливает тест.
static void test_match_tag_empty_default() {
    Entry e;
    auto ast = parse("tag = ''");
    test(matches(e, ast.get()));
}

// Checks name = '' on default. Unexpected exceptions fail the test.
// Проверяет равенство пустого имени. Неожиданное исключение проваливает тест.
static void test_match_name_empty_eq() {
    Entry e;
    auto ast = parse("name = ''");
    test(matches(e, ast.get()));
}

// Checks name = '' on named. Unexpected exceptions fail the test.
// Проверяет неравенство пустого имени. Неожиданное исключение проваливает тест.
static void test_match_name_empty_ne() {
    Entry e;
    e.set_name("bug");
    auto ast = parse("name = ''");
    test(!matches(e, ast.get()));
}

// Checks any = test, name matches. Unexpected exceptions fail the test.
// Проверяет поиск имени через any. Неожиданное исключение проваливает тест.
static void test_match_any_name() {
    Entry e;
    e.set_name("test");
    auto ast = parse("any = test");
    test(matches(e, ast.get()));
}

// Checks any = 5, name matches. Unexpected exceptions fail the test.
// Проверяет поиск числа в имени через any. Неожиданное исключение проваливает тест.
static void test_match_any_number_as_name() {
    Entry e;
    e.set_name("5");
    auto ast = parse("any = 5");
    test(matches(e, ast.get()));
}

// Checks any = 20260920-101010, name matches. Unexpected exceptions fail the
// test.
// Проверяет поиск метки времени в имени через any. Неожиданное исключение проваливает тест.
static void test_match_any_time_as_name() {
    Entry e;
    e.set_name("20260920-101010");
    auto ast = parse("any = 20260920-101010");
    test(matches(e, ast.get()));
}

// Checks priority = 5. Unexpected exceptions fail the test.
// Проверяет равенство приоритетов. Неожиданное исключение проваливает тест.
static void test_match_priority_eq() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("priority = 5");
    test(matches(e, ast.get()));
}

// Checks priority > 3. Unexpected exceptions fail the test.
// Проверяет сравнение приоритета по оператору больше. Неожиданное исключение проваливает тест.
static void test_match_priority_gt() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("priority > 3");
    test(matches(e, ast.get()));
}

// Checks status = opened. Unexpected exceptions fail the test.
// Проверяет совпадение статуса. Неожиданное исключение проваливает тест.
static void test_match_status() {
    Entry e;
    e.set_status("opened");
    auto ast = parse("status = opened");
    test(matches(e, ast.get()));
}

// Checks path ~ MADO. Unexpected exceptions fail the test.
// Проверяет совпадение пути. Неожиданное исключение проваливает тест.
static void test_match_path() {
    Entry e;
    e.set_path("/home/user/MADO/20260101-120000/MAIN.md");
    auto ast = parse("path ~ MADO");
    test(matches(e, ast.get()));
}

// Checks deadline > 20260101-000000. Unexpected exceptions fail the test.
// Проверяет сравнение срока. Неожиданное исключение проваливает тест.
static void test_match_deadline() {
    Entry e;
    e.set_deadline("20260201-000000");
    auto ast = parse("deadline > 20260101-000000");
    test(matches(e, ast.get()));
}

// Checks xor both true = false. Unexpected exceptions fail the test.
// Проверяет ложный XOR при двух истинных операндах. Неожиданное исключение проваливает тест.
static void test_match_xor_both_true() {
    Entry e;
    e.set_priority(5);
    e.set_name("bug");
    auto ast = parse("priority > 3 xor name = bug");
    test(!matches(e, ast.get()));
}

// Checks name ~~ fbeer. Unexpected exceptions fail the test.
// Проверяет нечёткое совпадение имени. Неожиданное исключение проваливает тест.
static void test_fuzzy() {
    Entry e;
    e.set_name("fix bug in lexer");
    auto ast = parse("name ~~ fbeer");
    test(matches(e, ast.get()));
}

// Checks glob matching, negation and invalid patterns. Unexpected exceptions
// fail the test.
// Проверяет glob, его отрицание и некорректный шаблон. Неожиданное исключение проваливает
// тест.
static void test_glob() {
    Entry e;
    e.set_name("FIX-42");

    test(matches(e, parse(R"(name %~ "fix-[45]?*")").get()) &&
         !matches(e, parse(R"(name !%~ "fix-[45]?*")").get()) &&
         matches(e, parse(R"(name !%~ "fix-[67]?")").get()) &&
         !matches(e, parse(R"(name %~ "fix-[")").get()) &&
         !matches(e, parse(R"(name !%~ "fix-[")").get()));
}

// Checks not name = bug on feature. Unexpected exceptions fail the test.
// Проверяет ложный NOT. Неожиданное исключение проваливает тест.
static void test_match_not_false() {
    Entry e;
    e.set_name("feature");
    auto ast = parse("not name = bug");
    test(matches(e, ast.get()));
}

// Checks any = 5 matches priority. Unexpected exceptions fail the test.
// Проверяет сравнение числа с приоритетом через any. Неожиданное исключение проваливает тест.
static void test_match_any_number_as_priority() {
    Entry e;
    e.set_priority(5);
    auto ast = parse("any = 5");
    test(matches(e, ast.get()));
}

// Checks tag ~ crit. Unexpected exceptions fail the test.
// Проверяет поиск подстроки в теге. Неожиданное исключение проваливает тест.
static void test_match_tag_substr() {
    Entry e;
    e.set_tags({"critical"});
    auto ast = parse("tag ~ crit");
    test(matches(e, ast.get()));
}

// utf8 case insensitive
// Регистронезависимое сравнение UTF-8.

// Checks name = лалала matches ЛАЛАЛА. Unexpected exceptions fail the test.
// Проверяет равенство имени UTF-8 без учёта регистра. Неожиданное исключение проваливает тест.
static void test_match_name_unicode_ci_eq() {
    Entry e;
    e.set_name("ЛАЛАЛА");
    auto ast = parse("name = 'лалала'");
    test(matches(e, ast.get()));
}

// Checks name != лалала doesn't match ЛАЛАЛА. Unexpected exceptions fail the
// test.
// Проверяет ложное неравенство имени UTF-8 без учёта регистра. Неожиданное исключение
// проваливает тест.
static void test_match_name_unicode_ci_ne() {
    Entry e;
    e.set_name("ЛАЛАЛА");
    auto ast = parse("name != 'лалала'");
    test(!matches(e, ast.get()));
}

// Checks name != ЛЯЛЯЛЯ matches ЛАЛАЛА. Unexpected exceptions fail the test.
// Проверяет истинное неравенство имени UTF-8 без учёта регистра. Неожиданное исключение
// проваливает тест.
static void test_match_name_unicode_ci_ne_true() {
    Entry e;
    e.set_name("ЛАЛАЛА");
    auto ast = parse("name != 'ЛЯЛЯЛЯ'");
    test(matches(e, ast.get()));
}

// Checks name ~ ала matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет подстроку в имени UTF-8 без учёта регистра. Неожиданное исключение проваливает
// тест.
static void test_match_name_unicode_ci_substr() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name ~ 'ала'");
    test(matches(e, ast.get()));
}

// Checks name !~ xyz matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет отсутствие подстроки в имени UTF-8 без учёта регистра. Неожиданное исключение
// проваливает тест.
static void test_match_name_unicode_ci_nsubstr() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name !~ 'xyz'");
    test(matches(e, ast.get()));
}

// Checks name ^~ лала matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет префикс имени UTF-8 без учёта регистра. Неожиданное исключение проваливает тест.
static void test_match_name_unicode_ci_starts() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name ^~ 'лала'");
    test(matches(e, ast.get()));
}

// Checks name !^~ xyz matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет отсутствие префикса имени UTF-8 без учёта регистра. Неожиданное исключение
// проваливает тест.
static void test_match_name_unicode_ci_nstarts() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name !^~ 'xyz'");
    test(matches(e, ast.get()));
}

// Checks name $~ лала matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет суффикс имени UTF-8 без учёта регистра. Неожиданное исключение проваливает тест.
static void test_match_name_unicode_ci_ends() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name $~ 'лала'");
    test(matches(e, ast.get()));
}

// Checks name !$~ xyz matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет отсутствие суффикса имени UTF-8 без учёта регистра. Неожиданное исключение
// проваливает тест.
static void test_match_name_unicode_ci_nends() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name !$~ 'xyz'");
    test(matches(e, ast.get()));
}

// Checks name ~~ ллл matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет нечёткое совпадение имени UTF-8 без учёта регистра. Неожиданное исключение
// проваливает тест.
static void test_match_name_unicode_ci_fuzzy() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name ~~ 'ллл'");
    test(matches(e, ast.get()));
}

// Checks name !~~ xyz matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет отрицание нечёткого совпадения имени UTF-8. Неожиданное исключение проваливает
// тест.
static void test_match_name_unicode_ci_nfuzzy() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name !~~ 'xyz'");
    test(matches(e, ast.get()));
}

// Checks name > ааа matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет сравнение имени UTF-8 по оператору больше. Неожиданное исключение проваливает
// тест.
static void test_match_name_unicode_ci_gt() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name > 'ааа'");
    test(matches(e, ast.get()));
}

// Checks name < яяя matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет сравнение имени UTF-8 по оператору меньше. Неожиданное исключение проваливает
// тест.
static void test_match_name_unicode_ci_lt() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name < 'яяя'");
    test(matches(e, ast.get()));
}

// Checks name >= лалала matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет сравнение имени UTF-8 по оператору больше или равно. Неожиданное исключение
// проваливает тест.
static void test_match_name_unicode_ci_ge() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name >= 'лалала'");
    test(matches(e, ast.get()));
}

// Checks name <= лалала matches ЛаЛаЛа. Unexpected exceptions fail the test.
// Проверяет сравнение имени UTF-8 по оператору меньше или равно. Неожиданное исключение
// проваливает тест.
static void test_match_name_unicode_ci_le() {
    Entry e;
    e.set_name("ЛаЛаЛа");
    auto ast = parse("name <= 'лалала'");
    test(matches(e, ast.get()));
}

// entry point
// Точка входа.

// Runs this test suite and returns 1 on failure or 0 on success.
// Запускает набор тестов. Возвращает 1 при провале, 0 при успехе.
int main() {
    std::vector<Test_Case> tests = {
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
        {"Xor both true", "xor both true = false", test_match_xor_both_true},
        {"Fuzzy name", "name ~~ fbeer", test_fuzzy},
        {"Glob name", "glob matching, negation and invalid patterns", test_glob},
        {"Not false", "not name = bug on feature", test_match_not_false},
        {"Any number as priority", "any = 5 matches priority", test_match_any_number_as_priority},
        {"Tag substr", "tag ~ crit", test_match_tag_substr},
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
