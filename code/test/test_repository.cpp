#include "test_utils.hpp"

#include <mado/query/ast.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>
#include <mado/repository/repository.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace mado::query;
using namespace mado::repository;

namespace {

class Temp_Dir {
  public:
    Temp_Dir() {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("mado_test_" + std::to_string(::getpid()) + "_" +
                 std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }

    ~Temp_Dir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

std::filesystem::path make_entry(const std::filesystem::path &repo_root,
                                 const std::string &timestamp,
                                 const std::string &content) {

    auto entry_dir = repo_root / "MADO" / timestamp;
    std::filesystem::create_directories(entry_dir);
    std::ofstream out(entry_dir / "MAIN.md");
    out << content;
    return entry_dir;
}

std::unique_ptr<Ast_Node> parse(const std::string &query) {
    Lexer lexer(query);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    return parser.parse();
}

} // namespace

static void test_open_finds_mado() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");

    auto repo = Repository::open(tmp.path());
    test(repo.has_value());
}

static void test_open_finds_mado_above() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");
    auto nested = tmp.path() / "a" / "b" / "c";
    std::filesystem::create_directories(nested);

    auto repo = Repository::open(nested);
    test(repo.has_value());
}

static void test_open_returns_nullopt_when_missing() {
    Temp_Dir tmp;

    auto repo = Repository::open(tmp.path());
    test(!repo.has_value());
}

static void test_find_empty_repo() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");

    auto repo = Repository::open(tmp.path());
    test(repo.has_value() && repo->find(nullptr).empty());
}

static void test_find_one_entry() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: fix bug\n"
               "- STATUS: opened\n"
               "- TAGS: bug\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].name() == "fix bug" &&
         entries[0].status() == "opened" &&
         entries[0].tags().size() == 1 &&
         entries[0].tags()[0] == "bug" &&
         entries[0].time() == "20260101-120000");
}

static void test_find_skips_non_timestamp_dirs() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: x\n");

    auto bad = tmp.path() / "MADO" / "not-a-timestamp";
    std::filesystem::create_directories(bad);
    std::ofstream(bad / "MAIN.md") << "- NAME: y\n";

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 && entries[0].name() == "x");
}

static void test_find_skips_entry_without_main_md() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: x\n");

    auto empty = tmp.path() / "MADO" / "20260202-130000";
    std::filesystem::create_directories(empty);

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1);
}

static void test_find_multiple_entries() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: first\n");
    make_entry(tmp.path(), "20260202-130000", "- NAME: second\n");
    make_entry(tmp.path(), "20260303-140000", "- NAME: third\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 3);
}

static void test_find_filter_name() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: fix bug\n");
    make_entry(tmp.path(), "20260202-130000", "- NAME: feature\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("name = 'fix bug'");
    auto entries = repo->find(ast.get());

    test(entries.size() == 1 && entries[0].name() == "fix bug");
}

static void test_find_filter_status() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: a\n- STATUS: opened\n");
    make_entry(tmp.path(), "20260202-130000", "- NAME: b\n- STATUS: closed\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("status = opened");
    auto entries = repo->find(ast.get());

    test(entries.size() == 1 && entries[0].name() == "a");
}

static void test_find_filter_tags() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: a\n- TAGS: bug, crit\n");
    make_entry(tmp.path(), "20260202-130000", "- NAME: b\n- TAGS: feature\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("tag = crit");
    auto entries = repo->find(ast.get());

    test(entries.size() == 1 && entries[0].name() == "a");
}

static void test_find_filter_no_match() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: fix\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("name = 'nonexistent'");
    auto entries = repo->find(ast.get());

    test(entries.empty());
}

static void test_load_all_fields() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: full\n"
               "- PRIORITY: 5\n"
               "- TAGS: a, b, c\n"
               "- STATUS: opened\n"
               "- DEADLINE: 20260202-000000\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].name() == "full" &&
         entries[0].priority() == 5 &&
         entries[0].tags().size() == 3 &&
         entries[0].tags()[0] == "a" &&
         entries[0].tags()[1] == "b" &&
         entries[0].tags()[2] == "c" &&
         entries[0].status() == "opened" &&
         entries[0].deadline() == "20260202-000000" &&
         entries[0].time() == "20260101-120000");
}

static void test_load_defaults() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: minimal\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].name() == "minimal" &&
         entries[0].priority() == 0 &&
         entries[0].tags().size() == 1 &&
         entries[0].tags()[0].empty() &&
         entries[0].status().empty() &&
         entries[0].deadline() == "99990101-000000");
}

static void test_load_invalid_priority_ignored() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: x\n"
               "- PRIORITY: abc\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 && entries[0].priority() == 0);
}

static void test_load_priority_too_large_ignored() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: x\n"
               "- PRIORITY: 1000\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 && entries[0].priority() == 0);
}

static void test_load_invalid_deadline_ignored() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: x\n"
               "- DEADLINE: not-a-date\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].deadline() == "99990101-000000");
}

static void test_load_empty_tags() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: x\n"
               "- TAGS:\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].tags().size() == 1 &&
         entries[0].tags()[0].empty());
}

static void test_load_tags_with_spaces() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: x\n"
               "- TAGS:  bug ,  crit , feature \n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].tags().size() == 3 &&
         entries[0].tags()[0] == "bug" &&
         entries[0].tags()[1] == "crit" &&
         entries[0].tags()[2] == "feature");
}

static void test_load_unicode_name() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: ЛАЛАЛА\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("name = 'лалала'");
    auto entries = repo->find(ast.get());

    test(entries.size() == 1);
}

static void test_load_duplicate_tags_ignored() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: x\n"
               "- TAGS: bug, bug, crit, bug\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].tags().size() == 2 &&
         entries[0].tags()[0] == "bug" &&
         entries[0].tags()[1] == "crit");
}

static void test_load_tags_with_empty_token() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: x\n"
               "- TAGS: tag1,,tag2\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].tags().size() == 3 &&
         entries[0].tags()[0] == "tag1" &&
         entries[0].tags()[1].empty() &&
         entries[0].tags()[2] == "tag2");
}

static void test_load_tags_with_empty_token_at_end() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: x\n"
               "- TAGS: tag1,\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].tags().size() == 2 &&
         entries[0].tags()[0] == "tag1" &&
         entries[0].tags()[1].empty());
}

static void test_load_tags_with_empty_token_at_start() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: x\n"
               "- TAGS: ,tag1\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].tags().size() == 2 &&
         entries[0].tags()[0].empty() &&
         entries[0].tags()[1] == "tag1");
}

static void test_load_repeated_field_ignored() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: first\n"
               "- NAME: second\n"
               "- STATUS: opened\n"
               "- STATUS: closed\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].name() == "first" &&
         entries[0].status() == "opened");
}

static void test_load_entry_suffix_dir() {
    Temp_Dir tmp;
    auto entry_dir = make_entry(tmp.path(), "20260920-215549-serr", "- NAME: x\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].name() == "x" &&
         entries[0].time() == "20260920-215549" &&
         entries[0].path() == entry_dir / "MAIN.md");
}

// entry point

int main() {
    std::vector<Test_Case> tests = {
        {"Open finds MADO", "Repository::open finds MADO in cwd", test_open_finds_mado},
        {"Open finds MADO above", "Repository::open walks up the tree", test_open_finds_mado_above},
        {"Open returns nullopt", "no MADO anywhere", test_open_returns_nullopt_when_missing},
        {"Find empty repo", "no entries", test_find_empty_repo},
        {"Find one entry", "single entry with fields", test_find_one_entry},
        {"Find skips non-timestamp dirs", "non-timestamp dirs ignored", test_find_skips_non_timestamp_dirs},
        {"Find skips entries without MAIN.md", "missing MAIN.md", test_find_skips_entry_without_main_md},
        {"Find multiple entries", "3 entries", test_find_multiple_entries},
        {"Find filter name", "name = 'fix bug'", test_find_filter_name},
        {"Find filter status", "status = opened", test_find_filter_status},
        {"Find filter tags", "tag = crit", test_find_filter_tags},
        {"Find filter no match", "no entries match", test_find_filter_no_match},
        {"Load all fields", "NAME, PRIORITY, TAGS, STATUS, DEADLINE", test_load_all_fields},
        {"Load defaults", "only NAME, rest are defaults", test_load_defaults},
        {"Load invalid priority ignored", "PRIORITY: abc -> 0", test_load_invalid_priority_ignored},
        {"Load priority too large ignored", "PRIORITY: 1000 -> 0", test_load_priority_too_large_ignored},
        {"Load invalid deadline ignored", "DEADLINE: not-a-date -> default", test_load_invalid_deadline_ignored},
        {"Load empty tags", "TAGS: -> ['']", test_load_empty_tags},
        {"Load tags with spaces", "TAGS:  bug ,  crit , feature", test_load_tags_with_spaces},
        {"Load unicode name", "name = лалала matches ЛАЛАЛА", test_load_unicode_name},
        {"Load duplicate tags", "TAGS: bug, bug, crit, bug -> [bug, crit]", test_load_duplicate_tags_ignored},
        {"Load tags empty token", "TAGS: tag1,,tag2 -> [tag1, '', tag2]", test_load_tags_with_empty_token},
        {"Load tags empty token at end", "TAGS: tag1, -> [tag1, '']", test_load_tags_with_empty_token_at_end},
        {"Load tags empty token at start", "TAGS: ,tag1 -> ['', tag1]", test_load_tags_with_empty_token_at_start},
        {"Load repeated field ignored", "NAME/STATUS taken from first occurrence", test_load_repeated_field_ignored},
        {"Load entry dir suffix", "20260920-215549-serr", test_load_entry_suffix_dir},
    };
    return run_tests(tests);
}
