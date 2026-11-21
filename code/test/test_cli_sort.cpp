#include "test_utils.hpp"

#include "../cli/entry_sort.hpp"

#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>
#include <mado/repository/repository.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

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
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

bool check_body_loading(const std::string &query, const std::string &sort_spec,
                        bool expect_body) {
    Temp_Dir tmp;
    const auto repo = mado::repository::Repository::init(tmp.path(), true);
    const std::string header = "- NAME: Same\n- PRIORITY: 1\n- TAGS: test\n"
                               "- STATUS: open\n- DEADLINE: 20260101\n";
    const std::string first_body = header + std::string(100, '\n') + "zulu needle\n";
    const std::string second_body = header + std::string(100, '\n') + "alpha needle";
    const auto first_dir = repo.root() / "20260101-000000";
    const auto second_dir = repo.root() / "20260102-000000";
    std::filesystem::create_directory(first_dir);
    std::filesystem::create_directory(second_dir);
    std::ofstream(first_dir / mado::repository::ENTRY_FILE) << first_body;
    std::ofstream(second_dir / mado::repository::ENTRY_FILE) << second_body;

    mado::query::Lexer lexer(query);
    mado::query::Parser parser(lexer.tokenize());
    const auto ast = parser.parse();
    const auto criteria = cli::parse_sort(sort_spec);
    if (!criteria)
        return false;

    auto entries = repo.find(ast.get(), cli::sort_uses_field(*criteria, cli::Sort_Field::Body));
    if (entries.size() != 2)
        return false;

    for (const auto &entry : entries) {
        const auto &expected = entry.path().parent_path() == first_dir ? first_body : second_body;
        if (entry.name() != "Same" || entry.body() != (expect_body ? expected : ""))
            return false;
    }

    cli::sort_entries(entries, *criteria);
    const bool sort_body = cli::sort_uses_field(*criteria, cli::Sort_Field::Body);
    return entries[0].path().parent_path() == (sort_body ? second_dir : first_dir) &&
           entries[1].path().parent_path() == (sort_body ? first_dir : second_dir);
}

} // namespace

static void test_no_body_in_filter_or_sort() {
    test(check_body_loading("name = Same", "+name,+time", false));
}

static void test_body_in_filter_only() {
    test(check_body_loading("name = Same and body ~ needle", "+time", true));
}

static void test_body_in_sort_only() {
    test(check_body_loading("name = Same", "+name,+body,+time", true));
}

static void test_body_in_filter_and_sort() {
    test(check_body_loading("not (body !~ needle)", "+name,+body,+time", true));
}

static void test_sort_uses_field() {
    const std::vector<cli::Sort_Criterion> criteria = {
        {cli::Sort_Field::Name, false}, {cli::Sort_Field::Body, true},
    };

    test(!cli::sort_uses_field({}, cli::Sort_Field::Body) &&
         cli::sort_uses_field(criteria, cli::Sort_Field::Body) &&
         cli::sort_uses_field(criteria, cli::Sort_Field::Name) &&
         !cli::sort_uses_field(criteria, cli::Sort_Field::Time));
}

int main() {
    const std::vector<Test_Case> tests = {
        {"No body", "metadata filter and sort leave body empty", test_no_body_in_filter_or_sort},
        {"Filter body", "body filter loads full text without body sorting", test_body_in_filter_only},
        {"Sort body", "body sorting loads full text without body filtering", test_body_in_sort_only},
        {"Filter and sort body", "both uses load full text", test_body_in_filter_and_sort},
        {"Sort uses field", "empty, present, and absent fields", test_sort_uses_field},
    };
    return run_tests(tests, "cli sort: ");
}
