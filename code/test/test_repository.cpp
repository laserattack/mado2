#include "test_utils.hpp"

#include <mado/query/ast.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>
#include <mado/repository/repository.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

using namespace mado::query;
using namespace mado::repository;

namespace {

class Temp_Dir {
  public:
    Temp_Dir() {
        const std::filesystem::path root(MADO_TEST_ROOT);
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto prefix = ".mado-test-" + std::to_string(stamp) + "-";
        for (unsigned attempt = 0;; ++attempt) {
            path_ = root / (prefix + std::to_string(attempt));
            if (std::filesystem::create_directory(path_))
                break;
        }
    }

    Temp_Dir(const Temp_Dir &) = delete;
    Temp_Dir &operator=(const Temp_Dir &) = delete;

    ~Temp_Dir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

class Read_Only_Path {
  public:
    explicit Read_Only_Path(std::filesystem::path path)
        : path_(std::move(path)), permissions_(std::filesystem::status(path_).permissions()) {
        std::filesystem::permissions(path_, std::filesystem::perms::owner_write |
            std::filesystem::perms::group_write | std::filesystem::perms::others_write,
            std::filesystem::perm_options::remove);
    }

    ~Read_Only_Path() {
        std::error_code error;
        std::filesystem::permissions(path_, permissions_, error);
    }

    Read_Only_Path(const Read_Only_Path &) = delete;
    Read_Only_Path &operator=(const Read_Only_Path &) = delete;

  private:
    std::filesystem::path path_;
    std::filesystem::perms permissions_;
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
    // Start at the filesystem root so the project repository cannot
    // affect the result and no test directory needs to be created.
    const auto root = std::filesystem::path(MADO_TEST_ROOT).root_path();
    const auto mado_dir = root / "MADO";
    const auto repo = Repository::open(root);
    test(std::filesystem::is_directory(mado_dir)
             ? repo && repo->root() == mado_dir
             : !repo);
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

static void test_load_entry_old_t_separator() {
    Temp_Dir tmp;
    auto entry_dir = make_entry(tmp.path(), "20260920T215549", "- NAME: x\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].name() == "x" &&
         entries[0].time() == "20260920-215549" &&
         entries[0].path() == entry_dir / "MAIN.md");
}

static void test_remove() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: a\n");
    make_entry(tmp.path(), "20260202-130000", "- NAME: b\n");
    make_entry(tmp.path(), "20260303-140000", "- NAME: c\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("name = b");
    auto removed = repo->remove(ast.get());

    test(removed.removed.size() == 1 && removed.failed.empty() &&
         removed.removed[0].name() == "b" &&
         !std::filesystem::exists(tmp.path() / "MADO" / "20260202-130000") &&
         std::filesystem::exists(tmp.path() / "MADO" / "20260101-120000") &&
         std::filesystem::exists(tmp.path() / "MADO" / "20260303-140000"));
}

static bool check_remove_failures(bool mixed) {
    Temp_Dir tmp;
    const auto blocked = make_entry(tmp.path(), "20260101-000000", "- NAME: blocked\n");
    const auto main = blocked / "MAIN.md";
    const auto repo = Repository::open(tmp.path());

#if defined(_WIN32)
    Read_Only_Path permissions(main);
#else
    Read_Only_Path permissions(blocked);
#endif

    std::error_code probe_error;
    std::filesystem::remove(main, probe_error);
    if (!probe_error) {
        std::printf("SKIP: filesystem does not enforce deletion permissions\n");
        return true;
    }

    std::filesystem::path removable;
    if (mixed)
        removable = make_entry(tmp.path(), "20260102-000000", "- NAME: removable\n");

    const auto result = repo->remove(nullptr);
    return result.failed.size() == 1 && result.failed[0].entry.path() == main &&
           result.failed[0].entry.name() == "blocked" && bool(result.failed[0].error) &&
           std::filesystem::is_regular_file(main) &&
           (mixed ? result.removed.size() == 1 && result.removed[0].name() == "removable" &&
                        !std::filesystem::exists(removable)
                  : result.removed.empty());
}

static void test_remove_partial_failure() {
    test(check_remove_failures(true));
}

static void test_remove_all_failed() {
    test(check_remove_failures(false));
}

static void test_remove_no_match() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: a\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("name = nonexistent");
    auto removed = repo->remove(ast.get());

    test(removed.removed.empty() && removed.failed.empty() &&
         std::filesystem::exists(tmp.path() / "MADO" / "20260101-120000"));
}

static void test_init_creates_mado() {
    Temp_Dir tmp;

    auto repo = Repository::init(tmp.path(), true);
    test(std::filesystem::is_directory(tmp.path() / "MADO") &&
         repo.root() == tmp.path() / "MADO" &&
         std::filesystem::is_regular_file(repo.root() / "templates" / "note.md") &&
         std::filesystem::is_regular_file(repo.root() / "templates" / "task.md"));
}

static void test_init_restores_templates_here() {
    Temp_Dir tmp;
    const auto templates = tmp.path() / "MADO" / ".templates";
    std::filesystem::create_directories(templates);
    std::ofstream(templates / "note.md") << "changed";

    auto repo = Repository::init(tmp.path());
    std::ifstream note(templates / "note.md");
    std::string first_line;
    std::getline(note, first_line);
    test(repo.root() == tmp.path() / "MADO" &&
         first_line == "- NAME:" &&
         std::filesystem::is_regular_file(templates / "task.md"));
}

static void test_init_restores_templates_above() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");
    auto nested = tmp.path() / "a" / "b";
    std::filesystem::create_directories(nested);

    auto repo = Repository::init(nested);
    test(repo.root() == tmp.path() / "MADO" &&
         std::filesystem::is_regular_file(repo.root() / "templates" / "note.md") &&
         !std::filesystem::exists(nested / "MADO"));
}

static void test_init_force_allows_nested() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");
    auto nested = tmp.path() / "a" / "b";
    std::filesystem::create_directories(nested);

    auto repo = Repository::init(nested, true);
    test(std::filesystem::is_directory(nested / "MADO") &&
         repo.root() == nested / "MADO");
}

static void test_init_then_open() {
    Temp_Dir tmp;

    Repository::init(tmp.path(), true);
    auto repo = Repository::open(tmp.path());
    test(repo.has_value() &&
         repo->root() == tmp.path() / "MADO");
}

static void test_create() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");

    auto repo = Repository::open(tmp.path());
    auto e = repo->create();

    test(std::filesystem::exists(e.path()) &&
         std::filesystem::is_regular_file(e.path()) &&
         e.path().filename() == "MAIN.md" &&
         e.path().parent_path().parent_path() == tmp.path() / "MADO");
}

static void test_create_then_find() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");

    auto repo = Repository::open(tmp.path());
    repo->create();

    auto entries = repo->find(nullptr);
    test(entries.size() == 1 &&
         entries[0].name().empty() &&
         entries[0].priority() == 0 &&
         entries[0].status().empty() &&
         entries[0].tags().size() == 1 &&
         entries[0].tags()[0].empty() &&
         entries[0].deadline() == "99990101-000000");
}

static void test_load_all_fields_empty() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME:\n"
               "- PRIORITY:\n"
               "- TAGS:\n"
               "- STATUS:\n"
               "- DEADLINE:\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].name().empty() &&
         entries[0].priority() == 0 &&
         entries[0].tags().size() == 1 &&
         entries[0].tags()[0].empty() &&
         entries[0].status().empty() &&
         entries[0].deadline() == "99990101-000000" &&
         entries[0].time() == "20260101-120000");
}

static void test_load_all_fields_whitespace() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME:   \n"
               "- PRIORITY:   \n"
               "- TAGS:   \n"
               "- STATUS:   \n"
               "- DEADLINE:   \n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 &&
         entries[0].name().empty() &&
         entries[0].priority() == 0 &&
         entries[0].tags().size() == 1 &&
         entries[0].tags()[0].empty() &&
         entries[0].status().empty() &&
         entries[0].deadline() == "99990101-000000");
}

static void test_create_rejects_dotdot_slash_in_suffix() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");
    auto repo = Repository::open(tmp.path());

    try {
        repo->create("../");
    } catch (const std::runtime_error &) { /* expected */
    }

    auto it = std::filesystem::directory_iterator(tmp.path() / "MADO");
    test(it == std::filesystem::directory_iterator{});
}

static void test_create_rejects_missing_or_non_file_template() {
    bool rejected = true;
    for (bool directory_instead_of_file : {false, true}) {
        Temp_Dir tmp;
        const auto templates = tmp.path() / "MADO" / "templates";
        std::filesystem::create_directories(templates);
        if (directory_instead_of_file)
            std::filesystem::create_directory(templates / "missing.md");

        auto repo = Repository::open(tmp.path());
        try {
            repo->create("", "missing");
            rejected = false;
        } catch (const std::runtime_error &error) {
            rejected &= std::string(error.what()).find("not found") != std::string::npos;
        }
        const auto first = std::filesystem::directory_iterator(repo->root());
        rejected &= first != std::filesystem::directory_iterator{} &&
                    first->path() == templates &&
                    std::distance(first, std::filesystem::directory_iterator{}) == 1;
    }
    test(rejected);
}

static void test_suffix_error_characters() {
    Temp_Dir tmp;
    const auto repo = Repository::init(tmp.path(), true);
    std::string message;

    try {
        repo.create("bad.");
    } catch (const std::runtime_error &error) {
        message = error.what();
    }

    test(message == R"(Invalid suffix 'bad.': must not contain '/', '\\', ':', '\r', '\n', '.')");
}

static void test_create_rejects_line_breaks() {
    Temp_Dir tmp;
    const auto repo = Repository::init(tmp.path(), true);
    bool rejected = true;

    for (const auto *name : {"bad\rname", "bad\nname"}) {
        try {
            repo.create(name);
            rejected = false;
        } catch (const std::runtime_error &error) {
            rejected &= std::string(error.what()).starts_with("Invalid suffix");
        }

        try {
            repo.create("", std::string(name));
            rejected = false;
        } catch (const std::runtime_error &error) {
            rejected &= std::string(error.what()).starts_with("Invalid template name");
        }
    }

    test(rejected && std::distance(std::filesystem::directory_iterator(repo.root()),
                                  std::filesystem::directory_iterator{}) == 1);
}

static void test_create_dotted_template_name() {
    Temp_Dir tmp;
    const auto repo = Repository::init(tmp.path(), true);
    std::ofstream(repo.root() / "templates" / "note.work.md") << "- NAME: dotted template\n";

    const auto entry = repo.create("", std::string("note.work"));

    test(entry.name() == "dotted template" && std::filesystem::is_regular_file(entry.path()));
}

static void test_mtimes_round_trip() {
    Temp_Dir tmp;
    const auto second = make_entry(tmp.path(), "20260102-000000 suffix.v1", "second");
    const auto first = make_entry(tmp.path(), "20260101-000000", "first");
    const auto repo = Repository::open(tmp.path());
    const auto time = std::filesystem::file_time_type::clock::from_sys(
        std::chrono::sys_time<std::chrono::nanoseconds>{std::chrono::nanoseconds{1700000000123456789LL}});
    std::filesystem::last_write_time(first / "MAIN.md", time);
    std::filesystem::last_write_time(second / "MAIN.md",
        std::filesystem::file_time_type::clock::from_sys(
            std::chrono::sys_time<std::chrono::nanoseconds>{std::chrono::nanoseconds{-123456789012345600LL}}));
    const auto original_first = std::filesystem::last_write_time(first / "MAIN.md");
    const auto original_second = std::filesystem::last_write_time(second / "MAIN.md");
    repo->dump_mtimes();
    std::ifstream in(repo->root() / "mtimes", std::ios::binary);
    const std::string contents(std::istreambuf_iterator<char>(in), {});
    std::filesystem::last_write_time(first / "MAIN.md", time + std::chrono::hours(24));
    std::filesystem::last_write_time(second / "MAIN.md", time + std::chrono::hours(48));
    repo->restore_mtimes();
    std::ifstream entry(first / "MAIN.md");
    std::string text;
    entry >> text;
    test(std::filesystem::last_write_time(first / "MAIN.md") == original_first &&
         std::filesystem::last_write_time(second / "MAIN.md") == original_second &&
         contents.starts_with("mado-mtimes-v1 unix-nanoseconds\n") &&
         contents.find("20260101-000000") < contents.find("20260102-000000 suffix.v1") &&
         text == "first");
}

static void test_mtimes_keep_newer() {
    Temp_Dir tmp;
    const auto newer = make_entry(tmp.path(), "20260101-000000", "newer") / "MAIN.md";
    const auto older = make_entry(tmp.path(), "20260102-000000", "older") / "MAIN.md";
    const auto equal = make_entry(tmp.path(), "20260103-000000", "equal") / "MAIN.md";
    const auto repo = Repository::open(tmp.path());
    const auto time = std::filesystem::file_time_type::clock::now() - std::chrono::hours(48);

    for (const auto &main : {newer, older, equal})
        std::filesystem::last_write_time(main, time);
    const auto indexed = std::filesystem::last_write_time(equal);
    repo->dump_mtimes();

    std::filesystem::last_write_time(newer, indexed + std::chrono::hours(24));
    std::filesystem::last_write_time(older, indexed - std::chrono::hours(24));
    const auto newer_time = std::filesystem::last_write_time(newer);
    repo->restore_mtimes({}, false, true);

    test(std::filesystem::last_write_time(newer) == newer_time &&
         std::filesystem::last_write_time(older) == indexed &&
         std::filesystem::last_write_time(equal) == indexed);
}

static void test_mtimes_index_precedence() {
    Temp_Dir tmp;
    const auto main = make_entry(tmp.path(), "20260101-000000", "entry") / "MAIN.md";
    const auto repo = Repository::open(tmp.path());
    const auto base = std::filesystem::last_write_time(main) - std::chrono::hours(24);
    const char *names[] = {"mtimes", ".mtimes", "_mtimes"};
    std::vector<std::filesystem::file_time_type> saved;

    for (unsigned i = 0; i < 3; ++i) {
        std::filesystem::last_write_time(main, base + std::chrono::hours(i));
        saved.push_back(std::filesystem::last_write_time(main));
        repo->dump_mtimes(repo->root() / names[i]);
    }

    bool ordered = true;
    for (unsigned i = 0; i < 3; ++i) {
        std::filesystem::last_write_time(main, base + std::chrono::hours(10));
        repo->restore_mtimes();
        ordered &= std::filesystem::last_write_time(main) == saved[i];
        std::filesystem::remove(repo->root() / names[i]);
    }

    test(ordered);
}

static void test_mtimes_index_path_selection() {
    Temp_Dir tmp;
    const auto main = make_entry(tmp.path(), "20260101-000000", "entry") / "MAIN.md";
    const auto repo = Repository::open(tmp.path());
    const auto original = std::filesystem::last_write_time(main);
    repo->dump_mtimes(repo->root() / ".mtimes");
    std::filesystem::create_directory(repo->root() / "mtimes");
    std::filesystem::last_write_time(main, original + std::chrono::hours(24));
    repo->restore_mtimes();
    const bool restored = std::filesystem::last_write_time(main) == original;

    bool rejected = false;
    try {
        repo->restore_mtimes(tmp.path() / "missing-index");
    } catch (const std::runtime_error &error) {
        rejected = std::string(error.what()).starts_with("Cannot read mtime index");
    }

    test(restored && rejected);
}

static void test_mtimes_invalid_preferred_index() {
    Temp_Dir tmp;
    const auto main = make_entry(tmp.path(), "20260101-000000", "entry") / "MAIN.md";
    const auto repo = Repository::open(tmp.path());
    repo->dump_mtimes(repo->root() / ".mtimes");
    std::ofstream(repo->root() / "mtimes") << "invalid header\n";
    std::filesystem::last_write_time(main, std::filesystem::last_write_time(main) + std::chrono::hours(24));
    const auto changed = std::filesystem::last_write_time(main);

    bool rejected = false;
    try {
        repo->restore_mtimes();
    } catch (const std::runtime_error &error) {
        rejected = std::string(error.what()).starts_with("Invalid mtime index header");
    }

    test(rejected && std::filesystem::last_write_time(main) == changed);
}

static void test_mtimes_empty_custom_index() {
    Temp_Dir tmp;
    const auto repo = Repository::init(tmp.path(), true);
    const auto file = tmp.path() / "backup times";
    const auto dumped = repo.dump_mtimes(file);
    const auto restored = repo.restore_mtimes(file);
    std::ifstream in(file);
    const std::string contents(std::istreambuf_iterator<char>(in), {});
    test(dumped == file && restored == file && contents == "mado-mtimes-v1 unix-nanoseconds\n" &&
         !std::filesystem::exists(repo.root() / "mtimes"));
}

static void test_mtimes_relocated_repository() {
    Temp_Dir tmp;
    const auto dir = make_entry(tmp.path(), "20260101-000000", "original");
    const auto repo = Repository::open(tmp.path());
    const auto original = std::filesystem::last_write_time(dir / "MAIN.md");
    repo->dump_mtimes();
    const auto extra = make_entry(tmp.path(), "20260102-000000", "not in index");
    const auto extra_time = std::filesystem::last_write_time(extra / "MAIN.md");
    const auto moved = tmp.path() / "moved";
    std::filesystem::create_directory(moved);
    std::filesystem::rename(repo->root(), moved / "MADO");
    const auto relocated = Repository::open(moved);
    const auto main = relocated->root() / dir.filename() / "MAIN.md";
    std::filesystem::last_write_time(main, original + std::chrono::hours(24));
    relocated->restore_mtimes();
    test(std::filesystem::last_write_time(main) == original &&
         std::filesystem::last_write_time(relocated->root() / extra.filename() / "MAIN.md") == extra_time);
}

static void test_mtimes_missing_main_file() {
    Temp_Dir tmp;
    const auto first = make_entry(tmp.path(), "20260101-000000", "first");
    const auto second = make_entry(tmp.path(), "20260102-000000", "second");
    const auto repo = Repository::open(tmp.path());
    repo->dump_mtimes();
    const auto changed = std::filesystem::last_write_time(first / "MAIN.md") + std::chrono::hours(24);
    std::filesystem::last_write_time(first / "MAIN.md", changed);
    const auto expected = std::filesystem::last_write_time(first / "MAIN.md");
    std::filesystem::remove(second / "MAIN.md");
    bool rejected = false;
    try {
        repo->restore_mtimes({}, true);
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    test(rejected && std::filesystem::last_write_time(first / "MAIN.md") == expected);
}

static void test_mtimes_skips_missing_files() {
    Temp_Dir tmp;
    const auto first = make_entry(tmp.path(), "20260101-000000", "deleted directory");
    const auto middle = make_entry(tmp.path(), "20260102-000000", "existing");
    const auto last = make_entry(tmp.path(), "20260103-000000", "deleted file");
    const auto repo = Repository::open(tmp.path());
    const auto original = std::filesystem::last_write_time(middle / "MAIN.md");
    repo->dump_mtimes();
    std::filesystem::remove_all(first);
    std::filesystem::remove(last / "MAIN.md");
    std::filesystem::last_write_time(middle / "MAIN.md", original + std::chrono::hours(24));
    repo->restore_mtimes();
    test(std::filesystem::last_write_time(middle / "MAIN.md") == original &&
         !std::filesystem::exists(first) && !std::filesystem::exists(last / "MAIN.md"));
}

static void test_mtimes_invalid_records() {
    Temp_Dir tmp;
    const auto dir = make_entry(tmp.path(), "20260101-000000", "original");
    const auto repo = Repository::open(tmp.path());
    const auto original = std::filesystem::last_write_time(dir / "MAIN.md");
    const auto index = repo->root() / "mtimes";
    const std::string valid = "\"20260101-000000\" 1700000000000000000\n";
    const std::string invalid[] = {
        "", "wrong header\n",
        "mado-mtimes-v1 unix-nanoseconds\n" + valid + valid,
        "mado-mtimes-v1 unix-nanoseconds\n" + valid + "\"../outside\" 1\n",
        "mado-mtimes-v1 unix-nanoseconds\n" + valid + "\"bad\\\\name\" 1\n",
        "mado-mtimes-v1 unix-nanoseconds\n" + valid + "\"bad:name\" 1\n",
        "mado-mtimes-v1 unix-nanoseconds\n" + valid + "\"\" 1\n",
        "mado-mtimes-v1 unix-nanoseconds\n" + valid + "\".\" 1\n",
        "mado-mtimes-v1 unix-nanoseconds\n" + valid + "\"broken\" nope\n",
        "mado-mtimes-v1 unix-nanoseconds\n" + valid + "\"broken\" 99999999999999999999999999999\n",
        "mado-mtimes-v1 unix-nanoseconds\n" + valid + "\"broken\" 1 trailing\n",
    };
    bool rejected = true;
    for (const auto &contents : invalid) {
        std::ofstream(index, std::ios::binary) << contents;
        try {
            repo->restore_mtimes();
            rejected = false;
        } catch (const std::runtime_error &) {
            rejected &= std::filesystem::last_write_time(dir / "MAIN.md") == original;
        }
    }
    test(rejected);
}

static void test_mtimes_io_errors() {
    Temp_Dir tmp;
    const auto dir = make_entry(tmp.path(), "20260101-000000", "original");
    const auto repo = Repository::open(tmp.path());
    bool missing = false;
    bool unwritable = false;
    bool protected_entry = false;
    try {
        repo->restore_mtimes();
    } catch (const std::runtime_error &) {
        missing = true;
    }
    try {
        repo->dump_mtimes(tmp.path() / "missing" / "index");
    } catch (const std::runtime_error &) {
        unwritable = true;
    }
    try {
        repo->dump_mtimes(dir / "MAIN.md");
    } catch (const std::runtime_error &) {
        protected_entry = true;
    }
    std::ifstream in(dir / "MAIN.md");
    std::string contents;
    in >> contents;
    test(missing && unwritable && protected_entry && contents == "original");
}

// entry point

int main() {
    std::vector<Test_Case> tests = {
        {"Suffix error characters", "describe the validation set without hidden control characters", test_suffix_error_characters},
        {"Create rejects line breaks", "reject CR and LF before creating files", test_create_rejects_line_breaks},
        {"Create dotted template", "retain dots in template names", test_create_dotted_template_name},
        {"Mtimes round trip", "preserve exact times and contents", test_mtimes_round_trip},
        {"Mtimes keep newer", "preserve newer times and restore older or equal ones", test_mtimes_keep_newer},
        {"Mtimes index precedence", "discover default, hidden, and underscored indexes in order", test_mtimes_index_precedence},
        {"Mtimes index path selection", "skip directories and respect explicit paths", test_mtimes_index_path_selection},
        {"Mtimes invalid preferred index", "do not fall back from corrupt indexes", test_mtimes_invalid_preferred_index},
        {"Mtimes empty custom index", "explicit destination and empty repository", test_mtimes_empty_custom_index},
        {"Mtimes relocated repository", "restore after moving without absolute paths", test_mtimes_relocated_repository},
        {"Mtimes missing MAIN.md", "strict validation finishes before restoring", test_mtimes_missing_main_file},
        {"Mtimes skips missing files", "restore existing entries despite missing targets", test_mtimes_skips_missing_files},
        {"Mtimes invalid records", "reject the entire invalid index before writes", test_mtimes_invalid_records},
        {"Mtimes I/O errors", "missing input and protected destinations", test_mtimes_io_errors},
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
        {"Load all fields empty", "all 5 fields present but empty -> defaults", test_load_all_fields_empty},
        {"Load all fields whitespace", "all 5 fields present but whitespace -> defaults", test_load_all_fields_whitespace},
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
        {"Load entry old T separator", "20260920T215549 -> 20260920-215549", test_load_entry_old_t_separator},
        {"Remove", "name = b removes only b", test_remove},
        {"Remove partial failure", "continue after an OS deletion error", test_remove_partial_failure},
        {"Remove all failed", "return failed entries and OS errors", test_remove_all_failed},
        {"Remove no match", "nothing removed", test_remove_no_match},
        {"Init creates MADO", "init creates MADO/ and returns repo", test_init_creates_mado},
        {"Init restores templates here", "init restores templates in existing MADO/", test_init_restores_templates_here},
        {"Init restores templates above", "init restores templates in ancestor MADO/", test_init_restores_templates_above},
        {"Init force allows nested", "init -force creates nested MADO/", test_init_force_allows_nested},
        {"Init then open", "open finds MADO/ created by init", test_init_then_open},
        {"Create", "create makes MAIN.md", test_create},
        {"Create then find", "created entry is found by find", test_create_then_find},
        {"Create rejects ../ suffix", "../ rejected, no leftover dir", test_create_rejects_dotdot_slash_in_suffix},
        {"Create rejects unusable templates", "missing or non-file template leaves no entry", test_create_rejects_missing_or_non_file_template},
    };
    return run_tests(tests, "repository: ");
}
