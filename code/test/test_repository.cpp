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

// Owns a unique project-local test directory and removes it at scope exit.
// Владеет уникальным тестовым каталогом внутри проекта и удаляет его при выходе из области
// видимости.
class Temp_Dir {
  public:
    // Creates a unique fixture directory. I/O failures throw filesystem_error.
    // Создаёт уникальный тестовый каталог. Ошибки ввода-вывода выбрасывают filesystem_error.
    Temp_Dir() {
        const std::filesystem::path root(MADO_TEST_ROOT);
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto prefix = ".mado-test-" + std::to_string(stamp) + "-";
        // Claim a unique fixture directory even during concurrent runs.
        // Занимаем уникальный каталог теста даже при параллельных запусках.
        for (unsigned attempt = 0;; ++attempt) {
            path_ = root / (prefix + std::to_string(attempt));
            if (std::filesystem::create_directory(path_))
                break;
        }
    }

    Temp_Dir(const Temp_Dir &) = delete;
    Temp_Dir &operator=(const Temp_Dir &) = delete;

    // Removes the fixture tree. Suppresses filesystem cleanup errors.
    // Удаляет дерево тестовых файлов. Ошибки очистки подавляются.
    ~Temp_Dir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    // Returns the fixture directory. No exceptions.
    // Возвращает тестовый каталог. Исключений нет.
    const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

// Creates a fixture entry and writes its contents. Directory creation can
// throw filesystem_error.
// Создаёт тестовую запись и записывает содержимое. Создание каталога может выбросить
// filesystem_error.
std::filesystem::path make_entry(const std::filesystem::path &repo_root,
                                 const std::string &timestamp,
                                 const std::string &content) {

    auto entry_dir = repo_root / "MADO" / timestamp;
    std::filesystem::create_directories(entry_dir);
    std::ofstream out(entry_dir / "MAIN.md");
    out << content;
    return entry_dir;
}

// Builds the complete query AST. Throws Parse_Error for empty or invalid
// queries and std::bad_alloc on allocation failure.
// Строит AST запроса. Некорректный или пустой запрос вызывает Parse_Error, сбой выделения
// памяти — std::bad_alloc.
std::unique_ptr<Ast_Node> parse(const std::string &query) {
    Lexer lexer(query);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    return parser.parse();
}

} // namespace

// Checks Repository::open finds MADO in cwd. Unexpected exceptions fail the
// test.
// Проверяет поиск MADO в текущем каталоге. Неожиданное исключение проваливает тест.
static void test_open_finds_mado() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");

    auto repo = Repository::open(tmp.path());
    test(repo.has_value());
}

// Checks Repository::open walks up the tree. Unexpected exceptions fail the
// test.
// Проверяет поиск MADO выше текущего каталога. Неожиданное исключение проваливает тест.
static void test_open_finds_mado_above() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");
    auto nested = tmp.path() / "a" / "b" / "c";
    std::filesystem::create_directories(nested);

    auto repo = Repository::open(nested);
    test(repo.has_value());
}

// Checks no MADO anywhere. Unexpected exceptions fail the test.
// Проверяет результат поиска при отсутствии MADO. Неожиданное исключение проваливает тест.
static void test_open_returns_nullopt_when_missing() {
    // Start at the filesystem root so the project repository cannot
    // affect the result and no test directory needs to be created.
    // Начинаем с корня файловой системы, чтобы репозиторий проекта не влиял на результат и не
    // требовался временный каталог.
    const auto root = std::filesystem::path(MADO_TEST_ROOT).root_path();
    const auto mado_dir = root / "MADO";
    const auto repo = Repository::open(root);
    test(std::filesystem::is_directory(mado_dir)
             ? repo && repo->root() == mado_dir
             : !repo);
}

// Checks no entries. Unexpected exceptions fail the test.
// Проверяет поиск в пустом репозитории. Неожиданное исключение проваливает тест.
static void test_find_empty_repo() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");

    auto repo = Repository::open(tmp.path());
    test(repo.has_value() && repo->find(nullptr).empty());
}

// Checks single entry with fields. Unexpected exceptions fail the test.
// Проверяет чтение одной записи. Неожиданное исключение проваливает тест.
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

// Checks non-timestamp dirs ignored. Unexpected exceptions fail the test.
// Проверяет пропуск каталогов без метки времени. Неожиданное исключение проваливает тест.
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

// Checks missing MAIN.md. Unexpected exceptions fail the test.
// Проверяет пропуск записи без MAIN.md. Неожиданное исключение проваливает тест.
static void test_find_skips_entry_without_main_md() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: x\n");

    auto empty = tmp.path() / "MADO" / "20260202-130000";
    std::filesystem::create_directories(empty);

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1);
}

// Checks 3 entries. Unexpected exceptions fail the test.
// Проверяет чтение нескольких записей. Неожиданное исключение проваливает тест.
static void test_find_multiple_entries() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: first\n");
    make_entry(tmp.path(), "20260202-130000", "- NAME: second\n");
    make_entry(tmp.path(), "20260303-140000", "- NAME: third\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 3);
}

// Checks name = 'fix bug'. Unexpected exceptions fail the test.
// Проверяет фильтрацию по имени. Неожиданное исключение проваливает тест.
static void test_find_filter_name() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: fix bug\n");
    make_entry(tmp.path(), "20260202-130000", "- NAME: feature\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("name = 'fix bug'");
    auto entries = repo->find(ast.get());

    test(entries.size() == 1 && entries[0].name() == "fix bug");
}

// Checks status = opened. Unexpected exceptions fail the test.
// Проверяет фильтрацию по статусу. Неожиданное исключение проваливает тест.
static void test_find_filter_status() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: a\n- STATUS: opened\n");
    make_entry(tmp.path(), "20260202-130000", "- NAME: b\n- STATUS: closed\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("status = opened");
    auto entries = repo->find(ast.get());

    test(entries.size() == 1 && entries[0].name() == "a");
}

// Checks tag = crit. Unexpected exceptions fail the test.
// Проверяет фильтрацию по тегам. Неожиданное исключение проваливает тест.
static void test_find_filter_tags() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: a\n- TAGS: bug, crit\n");
    make_entry(tmp.path(), "20260202-130000", "- NAME: b\n- TAGS: feature\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("tag = crit");
    auto entries = repo->find(ast.get());

    test(entries.size() == 1 && entries[0].name() == "a");
}

// Checks no entries match. Unexpected exceptions fail the test.
// Проверяет отсутствие совпадений при фильтрации. Неожиданное исключение проваливает тест.
static void test_find_filter_no_match() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: fix\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("name = 'nonexistent'");
    auto entries = repo->find(ast.get());

    test(entries.empty());
}

// Checks NAME, PRIORITY, TAGS, STATUS, DEADLINE. Unexpected exceptions fail
// the test.
// Проверяет загрузку всех полей. Неожиданное исключение проваливает тест.
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

// Checks only NAME, rest are defaults. Unexpected exceptions fail the test.
// Проверяет значения по умолчанию для отсутствующих полей. Неожиданное исключение проваливает
// тест.
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

// Checks PRIORITY: abc -> 0. Unexpected exceptions fail the test.
// Проверяет загрузку пустых полей. Неожиданное исключение проваливает тест.
static void test_load_invalid_priority_ignored() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: x\n"
               "- PRIORITY: abc\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 && entries[0].priority() == 0);
}

// Checks PRIORITY: 1000 -> 0. Unexpected exceptions fail the test.
// Проверяет загрузку полей из пробелов. Неожиданное исключение проваливает тест.
static void test_load_priority_too_large_ignored() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000",
               "- NAME: x\n"
               "- PRIORITY: 1000\n");

    auto repo = Repository::open(tmp.path());
    auto entries = repo->find(nullptr);

    test(entries.size() == 1 && entries[0].priority() == 0);
}

// Checks DEADLINE: not-a-date -> default. Unexpected exceptions fail the test.
// Проверяет пропуск некорректного приоритета. Неожиданное исключение проваливает тест.
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

// Checks TAGS: -> ['']. Unexpected exceptions fail the test.
// Проверяет пропуск слишком большого приоритета. Неожиданное исключение проваливает тест.
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

// Checks TAGS:  bug ,  crit , feature. Unexpected exceptions fail the test.
// Проверяет пропуск некорректного срока. Неожиданное исключение проваливает тест.
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

// Checks name = лалала matches ЛАЛАЛА. Unexpected exceptions fail the test.
// Проверяет загрузку пустых тегов. Неожиданное исключение проваливает тест.
static void test_load_unicode_name() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: ЛАЛАЛА\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("name = 'лалала'");
    auto entries = repo->find(ast.get());

    test(entries.size() == 1);
}

// Checks TAGS: bug, bug, crit, bug -> [bug, crit]. Unexpected exceptions fail
// the test.
// Проверяет обрезку пробелов в тегах. Неожиданное исключение проваливает тест.
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

// Checks TAGS: tag1,,tag2 -> [tag1, '', tag2]. Unexpected exceptions fail the
// test.
// Проверяет загрузку имени Unicode. Неожиданное исключение проваливает тест.
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

// Checks TAGS: tag1, -> [tag1, '']. Unexpected exceptions fail the test.
// Проверяет удаление дубликатов тегов. Неожиданное исключение проваливает тест.
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

// Checks TAGS: ,tag1 -> ['', tag1]. Unexpected exceptions fail the test.
// Проверяет пустой тег внутри списка. Неожиданное исключение проваливает тест.
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

// Checks NAME/STATUS taken from first occurrence. Unexpected exceptions fail
// the test.
// Проверяет пустой тег в конце списка. Неожиданное исключение проваливает тест.
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

// Checks 20260920-215549-serr. Unexpected exceptions fail the test.
// Проверяет пустой тег в начале списка. Неожиданное исключение проваливает тест.
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

// Checks 20260920T215549 -> 20260920-215549. Unexpected exceptions fail the
// test.
// Проверяет пропуск повторного поля. Неожиданное исключение проваливает тест.
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

// Checks name = b removes only b. Unexpected exceptions fail the test.
// Проверяет чтение записи с суффиксом каталога. Неожиданное исключение проваливает тест.
static void test_remove() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: a\n");
    make_entry(tmp.path(), "20260202-130000", "- NAME: b\n");
    make_entry(tmp.path(), "20260303-140000", "- NAME: c\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("name = b");
    auto removed = repo->remove(ast.get());

    test(removed.size() == 1 &&
         removed[0].name() == "b" &&
         !std::filesystem::exists(tmp.path() / "MADO" / "20260202-130000") &&
         std::filesystem::exists(tmp.path() / "MADO" / "20260101-120000") &&
         std::filesystem::exists(tmp.path() / "MADO" / "20260303-140000"));
}

// Checks nothing removed. Unexpected exceptions fail the test.
// Проверяет чтение старого разделителя T. Неожиданное исключение проваливает тест.
static void test_remove_no_match() {
    Temp_Dir tmp;
    make_entry(tmp.path(), "20260101-120000", "- NAME: a\n");

    auto repo = Repository::open(tmp.path());
    auto ast = parse("name = nonexistent");
    auto removed = repo->remove(ast.get());

    test(removed.empty() &&
         std::filesystem::exists(tmp.path() / "MADO" / "20260101-120000"));
}

// Checks init creates MADO/ and returns repo. Unexpected exceptions fail the
// test.
// Проверяет удаление записи. Неожиданное исключение проваливает тест.
static void test_init_creates_mado() {
    Temp_Dir tmp;

    auto repo = Repository::init(tmp.path(), true);
    test(std::filesystem::is_directory(tmp.path() / "MADO") &&
         repo.root() == tmp.path() / "MADO" &&
         std::filesystem::is_regular_file(repo.root() / "templates" / "note.md") &&
         std::filesystem::is_regular_file(repo.root() / "templates" / "task.md"));
}

// Checks init restores templates in existing MADO/. Unexpected exceptions fail
// the test.
// Проверяет отсутствие удаления без совпадений. Неожиданное исключение проваливает тест.
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

// Checks init restores templates in ancestor MADO/. Unexpected exceptions fail
// the test.
// Проверяет создание MADO и стандартных шаблонов. Неожиданное исключение проваливает тест.
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

// Checks init -force creates nested MADO/. Unexpected exceptions fail the
// test.
// Проверяет восстановление шаблонов в текущем MADO. Неожиданное исключение проваливает тест.
static void test_init_force_allows_nested() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");
    auto nested = tmp.path() / "a" / "b";
    std::filesystem::create_directories(nested);

    auto repo = Repository::init(nested, true);
    test(std::filesystem::is_directory(nested / "MADO") &&
         repo.root() == nested / "MADO");
}

// Checks open finds MADO/ created by init. Unexpected exceptions fail the
// test.
// Проверяет восстановление шаблонов в родительском MADO. Неожиданное исключение проваливает
// тест.
static void test_init_then_open() {
    Temp_Dir tmp;

    // Initializes the nearest repository and restores its default templates.
    // Force selects the given directory. I/O failures throw std::runtime_error
    // or a filesystem_error.
    // Инициализирует ближайший репозиторий и восстанавливает стандартные шаблоны. Force
    // выбирает указанный каталог. Ошибки ввода-вывода выбрасывают std::runtime_error или
    // filesystem_error.
    Repository::init(tmp.path(), true);
    auto repo = Repository::open(tmp.path());
    test(repo.has_value() &&
         repo->root() == tmp.path() / "MADO");
}

// Checks creation of makes MAIN.md. Unexpected exceptions fail the test.
// Проверяет принудительное создание вложенного репозитория. Неожиданное исключение проваливает
// тест.
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

// Checks created entry is found by find. Unexpected exceptions fail the test.
// Проверяет открытие репозитория после init. Неожиданное исключение проваливает тест.
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

// Checks all 5 fields present but empty -> defaults. Unexpected exceptions
// fail the test.
// Проверяет создание записи. Неожиданное исключение проваливает тест.
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

// Checks all 5 fields present but whitespace -> defaults. Unexpected
// exceptions fail the test.
// Проверяет поиск созданной записи. Неожиданное исключение проваливает тест.
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

// Checks ../ rejected, no leftover dir. Unexpected exceptions fail the test.
// Проверяет отклонение ../ в суффиксе без создания записи. Неожиданное исключение проваливает
// тест.
static void test_create_rejects_dotdot_slash_in_suffix() {
    Temp_Dir tmp;
    std::filesystem::create_directories(tmp.path() / "MADO");
    auto repo = Repository::open(tmp.path());

    try {
        repo->create("../");
    } catch (const std::runtime_error &) { /* expected
 * Ожидаемый результат.
 */
    }

    auto it = std::filesystem::directory_iterator(tmp.path() / "MADO");
    test(it == std::filesystem::directory_iterator{});
}

// Checks template lookup failures without leaving a partially created entry.
// Проверяет ошибки поиска шаблона без создания неполной записи.
static void test_create_rejects_missing_or_non_file_template() {
    bool rejected = true;
    // Use a fresh repository for each kind of missing template.
    // Для каждого случая отсутствия шаблона создаём отдельный репозиторий.
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

// entry point
// Точка входа.

// Runs this test suite and returns 1 on failure or 0 on success.
// Запускает набор тестов. Возвращает 1 при провале, 0 при успехе.
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
