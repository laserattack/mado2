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

static void test_sort_field_matrix() {
    using String_Setter = void (mado::entry::Entry::*)(std::string);
    struct Sort_Case {
        cli::Sort_Field field;
        String_Setter set;
        const char *low;
        const char *high;
    };
    const Sort_Case cases[] = {
        {cli::Sort_Field::Time, &mado::entry::Entry::set_time, "20260101", "20261231"},
        {cli::Sort_Field::Mtime, &mado::entry::Entry::set_mtime, "20260101", "20261231"},
        {cli::Sort_Field::Deadline, &mado::entry::Entry::set_deadline, "20260101", "20261231"},
        {cli::Sort_Field::Name, &mado::entry::Entry::set_name, "alpha", "zulu"},
        {cli::Sort_Field::Status, &mado::entry::Entry::set_status, "closed", "open"},
        {cli::Sort_Field::Body, &mado::entry::Entry::set_body, "alpha body", "zulu body"},
        {cli::Sort_Field::Priority, nullptr, nullptr, nullptr},
        {cli::Sort_Field::Path, nullptr, nullptr, nullptr},
        {cli::Sort_Field::Tags, nullptr, nullptr, nullptr},
    };

    bool correct = true;
    for (const auto &item : cases) {
        mado::entry::Entry low;
        mado::entry::Entry high;
        // Reverse path order so accidentally comparing paths cannot satisfy other fields.
        low.set_path("z-low");
        high.set_path("a-high");
        if (item.set) {
            (low.*item.set)(item.low);
            (high.*item.set)(item.high);
        } else if (item.field == cli::Sort_Field::Priority) {
            low.set_priority(0);
            high.set_priority(mado::entry::MAX_PRIORITY);
        } else if (item.field == cli::Sort_Field::Path) {
            low.set_path("a");
            high.set_path("z");
        } else {
            low.set_tags({"a", "b"});
            high.set_tags({"z"});
        }

        for (const bool descending : {false, true}) {
            std::vector<mado::entry::Entry> entries = {high, low, high, low};
            cli::sort_entries(entries, {{item.field, descending}});
            correct &= entries[0].path() == (descending ? high.path() : low.path()) &&
                       entries[1].path() == entries[0].path() &&
                       entries[2].path() == (descending ? low.path() : high.path()) &&
                       entries[3].path() == entries[2].path();
        }
    }
    test(correct);
}

static void test_sort_ties_and_empty_criteria() {
    std::vector<mado::entry::Entry> entries(3);
    entries[0].set_path("third");
    entries[1].set_path("first");
    entries[2].set_path("second");

    cli::sort_entries(entries, {});
    const bool unchanged = entries[0].path() == "third" &&
        entries[1].path() == "first" && entries[2].path() == "second";
    cli::sort_entries(entries, {{cli::Sort_Field::Name, false}});
    const bool stable = entries[0].path() == "third" &&
        entries[1].path() == "first" && entries[2].path() == "second";

    cli::sort_entries(entries, {{cli::Sort_Field::Name, false}, {cli::Sort_Field::Path, false}});
    test(unchanged && stable && entries[0].path() == "first" &&
         entries[1].path() == "second" && entries[2].path() == "third");
}

int main() {
    const std::vector<Test_Case> tests = {
        {"Sort field matrix", "all fields in both directions, including equal values", test_sort_field_matrix},
        {"Sort ties", "empty criteria, stable ties and secondary criteria", test_sort_ties_and_empty_criteria},
        {"No body", "metadata filter and sort leave body empty", test_no_body_in_filter_or_sort},
        {"Filter body", "body filter loads full text without body sorting", test_body_in_filter_only},
        {"Sort body", "body sorting loads full text without body filtering", test_body_in_sort_only},
        {"Filter and sort body", "both uses load full text", test_body_in_filter_and_sort},
        {"Sort uses field", "empty, present, and absent fields", test_sort_uses_field},
    };
    return run_tests(tests, "cli sort: ");
}
