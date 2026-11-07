#include <mado/repository/repository.hpp>

#include <mado/common/text_utils.hpp>
#include <mado/common/time_utils.hpp>
#include <mado/interpreter/interpreter.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace mado::repository {

namespace {

// Versioned header shared by the index reader and writer. No errors or exceptions.
// Версионный заголовок для чтения и записи индекса. Ошибок и исключений нет.
constexpr const char *MTIMES_HEADER = "mado-mtimes-v1 unix-nanoseconds";

// Path/drive separators and line breaks forbidden in single names. No errors or exceptions.
// Разделители путей и дисков и переводы строк, запрещённые в именах. Ошибок и исключений нет.
constexpr const char *INVALID_PATH_CHARS = "/\\:\r\n";

// Name metadata prefix shared by parsing and built-in templates. No errors or exceptions.
// Префикс имени для разбора и встроенных шаблонов. Ошибок и исключений нет.
constexpr std::string_view NAME_PREFIX = "- NAME:";

// Priority metadata prefix shared by parsing and templates. No errors or exceptions.
// Префикс приоритета для разбора и шаблонов. Ошибок и исключений нет.
constexpr std::string_view PRIORITY_PREFIX = "- PRIORITY:";

// Tags metadata prefix shared by parsing and templates. No errors or exceptions.
// Префикс тегов для разбора и шаблонов. Ошибок и исключений нет.
constexpr std::string_view TAGS_PREFIX = "- TAGS:";

// Status metadata prefix shared by parsing and templates. No errors or exceptions.
// Префикс статуса для разбора и шаблонов. Ошибок и исключений нет.
constexpr std::string_view STATUS_PREFIX = "- STATUS:";

// Deadline metadata prefix shared by parsing and templates. No errors or exceptions.
// Префикс срока для разбора и шаблонов. Ошибок и исключений нет.
constexpr std::string_view DEADLINE_PREFIX = "- DEADLINE:";

// Caps metadata scanning per entry.
// Ограничивает число просматриваемых строк метаданных.
// Static configuration data; no errors or exceptions.
// Статические данные настройки; ошибок и исключений нет.
constexpr int MAX_HEADER_LINES = 30;

// Suffix restrictions extend the shared name restrictions. May throw std::bad_alloc.
// Ограничения суффиксов дополняют общие ограничения имён. Возможен std::bad_alloc.
const std::string INVALID_SUFFIX_CHARS = std::string(INVALID_PATH_CHARS) + '.';

// Note template filename derived from the Markdown extension. May throw std::bad_alloc.
// Имя шаблона заметки из расширения Markdown. Возможен std::bad_alloc.
const std::string NOTE_TEMPLATE_FILE = std::string("note") + MARKDOWN_EXTENSION;
// Task template filename derived from the Markdown extension. May throw std::bad_alloc.
// Имя шаблона задачи из расширения Markdown. Возможен std::bad_alloc.
const std::string TASK_TEMPLATE_FILE = std::string("task") + MARKDOWN_EXTENSION;

// Default task contents assembled from the parsed field prefixes. Allocation can throw bad_alloc.
// Стандартная задача из распознаваемых префиксов полей. Выделение памяти может вызвать bad_alloc.
const std::string TASK_TEMPLATE = std::string(NAME_PREFIX) + "\n" +
                                  std::string(PRIORITY_PREFIX) + "\n" + std::string(TAGS_PREFIX) + "\n" +
                                  std::string(STATUS_PREFIX) + "\n" + std::string(DEADLINE_PREFIX) + "\n";
// Default note contents assembled from field prefixes. Allocation can throw bad_alloc.
// Стандартная заметка из префиксов полей. Выделение памяти может вызвать bad_alloc.
const std::string NOTE_TEMPLATE = std::string(NAME_PREFIX) + "\n" + std::string(TAGS_PREFIX) + "\n";

// Reports an invalid repository operation or missing repository data.
// Обозначает недопустимую операцию или отсутствие данных репозитория.
class Repository_Error : public std::runtime_error {
  public:
    // Reports an invalid repository operation or missing repository data.
    // Обозначает недопустимую операцию или отсутствие данных репозитория.
    // Stores the repository diagnostic. May throw std::bad_alloc.
    explicit Repository_Error(const std::string &message)
        : std::runtime_error(message) {}
};

// Describes each forbidden character with visible escapes for control characters.
// Empty input returns an empty string. Allocation failures throw std::bad_alloc.
// Описывает запрещённые символы с видимыми обозначениями управляющих символов.
// Для пустого ввода возвращает пустую строку. Сбой памяти вызывает std::bad_alloc.
std::string describe_invalid_chars(std::string_view chars) {
    std::string result;

    // Generate the diagnostic from exactly the same character set as validation.
    // Формируем сообщение из того же набора символов, что используется при проверке.
    for (char c : chars) {
        if (!result.empty())
            result += ", ";

        result += '\'';
        // Escape characters that would break the diagnostic's quoted representation.
        // Экранируем символы, нарушающие представление в кавычках внутри сообщения.
        switch (c) {
        case '\r':
            result += "\\r";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\'':
            result += "\\'";
            break;
        default:
            result += c;
            break;
        }
        result += '\'';
    }

    return result;
}

// Returns the first supported template directory or nullopt. Filesystem lookup
// errors throw std::filesystem::filesystem_error.
// Возвращает первый поддерживаемый каталог шаблонов или nullopt. Сбои поиска вызывают
// filesystem_error.
std::optional<std::filesystem::path> find_templates(const std::filesystem::path &root) {
    // Search supported template folders in precedence order.
    // Проверяем допустимые папки шаблонов в порядке приоритета.
    for (const auto *name : TEMPLATE_DIR_NAMES) {
        auto dir = root / name;
        if (std::filesystem::is_directory(dir))
            return dir;
    }
    return std::nullopt;
}

// Creates or restores the built-in template, skipping identical contents.
// Throws std::runtime_error if writing fails.
// Создаёт или восстанавливает стандартный шаблон, пропуская совпадающее содержимое. Сбой
// записи вызывает std::runtime_error.
void write_default_template(const std::filesystem::path &path, const std::string &content) {
    {
        std::ifstream in(path, std::ios::binary);
        if (in && std::string(std::istreambuf_iterator<char>(in), {}) == content)
            return;
    }
    std::ofstream out(path, std::ios::binary);
    out << content;
    out.close();
    if (!out)
        throw std::runtime_error("Cannot write template " + path.string());
}

// Converts file time to system time using the portable C++20 clock conversion. No exceptions.
// Переводит файловое время в системное переносимым преобразованием C++20. Исключений нет.
std::chrono::system_clock::time_point
file_time_to_system_clock(std::filesystem::file_time_type ft) {
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        std::filesystem::file_time_type::clock::to_sys(ft));
}

// Splits and deduplicates comma-separated tags, retaining an empty tag for
// empty input. May throw std::bad_alloc.
// Разбивает теги по запятым, убирает дубликаты и сохраняет пустой тег при пустом вводе.
// Возможен std::bad_alloc.
std::vector<std::string> split_tags(const std::string &s) {
    std::vector<std::string> tags;
    std::unordered_set<std::string> seen;

    std::stringstream ss(s);
    std::string item;
    // Keep the first occurrence of each normalized tag.
    // Сохраняем первое вхождение каждого нормализованного тега.
    while (std::getline(ss, item, ',')) {
        mado::common::trim_inplace(item);
        if (seen.insert(item).second)
            tags.push_back(item);
    }

    // getline does not produce a trailing empty token for "a,"
    // getline не возвращает завершающий пустой элемент для «a,».
    if (!s.empty() && s.back() == ',') {
        if (seen.insert("").second)
            tags.push_back("");
    }

    if (tags.empty())
        tags.push_back("");

    return tags;
}

// Parses "0".."999" into uint16_t. Returns std::nullopt on any error.
// Разбирает число от 0 до 999. При неверном вводе возвращает nullopt.
std::optional<uint16_t> parse_priority(const std::string &s) {
    if (s.empty() || s.size() > 3)
        return std::nullopt;

    for (char c : s) {
        if (!mado::common::is_digit(c))
            return std::nullopt;
    }

    return static_cast<uint16_t>(std::stoi(s));
}

} // namespace

// Finds the nearest MADO directory, returning nullopt when none can be found
// or the input cannot be resolved. May throw std::bad_alloc.
// Ищет ближайший MADO. Если каталог не найден или путь не разрешается, возвращает nullopt.
// Возможен std::bad_alloc.
std::optional<Repository> Repository::open(const std::filesystem::path &from) {
    std::error_code ec;

    std::filesystem::path dir = std::filesystem::absolute(from, ec);
    if (ec)
        return std::nullopt;

    if (!std::filesystem::is_directory(dir, ec))
        dir = dir.parent_path();

    // Walk toward the filesystem root and stop at the nearest repository.
    // Идём к корню файловой системы и останавливаемся на ближайшем репозитории.
    while (!dir.empty()) {
        auto candidate = dir / ENTRY_DIR;
        if (std::filesystem::is_directory(candidate, ec))
            return Repository(candidate);

        auto parent = dir.parent_path();
        if (parent == dir)
            break;
        dir = parent;
    }

    return std::nullopt;
}

// Initializes the nearest repository and restores its default templates. Force
// selects the given directory. I/O failures throw std::runtime_error or a
// filesystem_error.
// Инициализирует ближайший репозиторий и восстанавливает стандартные шаблоны. Force выбирает
// указанный каталог. Ошибки ввода-вывода выбрасывают std::runtime_error или filesystem_error.
Repository Repository::init(const std::filesystem::path &where, bool force) {
    std::error_code ec;

    const auto abs_where = std::filesystem::absolute(where, ec);
    if (ec)
        throw std::runtime_error("Cannot resolve path " + where.string() + ": " + ec.message());

    auto mado_dir = abs_where / ENTRY_DIR;
    // Reuse an ancestor repository unless a local repository was requested.
    // Переиспользуем родительский репозиторий, если не запрошен локальный.
    if (!force && !std::filesystem::exists(mado_dir)) {
        if (auto repo = open(abs_where))
            mado_dir = repo->root();
    }
    std::filesystem::create_directory(mado_dir);

    const auto templates = find_templates(mado_dir).value_or(mado_dir / TEMPLATE_DIR_NAMES[0]);
    std::filesystem::create_directory(templates);
    write_default_template(templates / NOTE_TEMPLATE_FILE, NOTE_TEMPLATE);
    write_default_template(templates / TASK_TEMPLATE_FILE, TASK_TEMPLATE);
    return Repository(mado_dir);
}

// Returns entries matching the filter, skipping malformed entries. I/O
// failures encountered while loading can throw std::runtime_error.
// Возвращает записи, подходящие под фильтр, пропуская некорректные. Ошибки ввода-вывода при
// загрузке могут вызвать std::runtime_error.
std::vector<mado::entry::Entry> Repository::find(
    const mado::query::Ast_Node *filter) const {

    std::vector<mado::entry::Entry> result;
    for_each_matching(filter, [&](mado::entry::Entry e) {
        result.push_back(std::move(e));
        return true;
    });
    return result;
}

// Attempts all deletions after loading matching entries, recording each OS failure.
// Failed directories may be partially deleted. Loading and allocation exceptions propagate.
// Пробует все удаления после загрузки записей, сохраняя каждый сбой ОС.
// При сбое каталог может быть удалён частично. Исключения чтения и памяти передаются выше.
Remove_Result Repository::remove(const mado::query::Ast_Node *filter) const {
    auto entries = find(filter);
    Remove_Result result;
    result.removed.reserve(entries.size());
    result.failed.reserve(entries.size());

    // Snapshot the matches first so deletions cannot disturb directory iteration.
    // Сначала получаем снимок записей, чтобы удаления не нарушали обход каталога.
    for (auto &entry : entries) {
        std::error_code error;
        std::filesystem::remove_all(entry.path().parent_path(), error);

        // Keep failed entries and their error codes while continuing the batch.
        // Сохраняем неудачные записи и коды ошибок, продолжая пакетное удаление.
        if (error)
            result.failed.push_back({std::move(entry), error});
        else
            result.removed.push_back(std::move(entry));
    }

    return result;
}

template <class Action>
// Visits matching entries until the action returns false. Skips malformed
// entries. I/O and callback exceptions can propagate.
// Обходит подходящие записи до false от обработчика. Пропускает некорректные записи.
// Исключения ввода-вывода и обработчика передаются вызывающему коду.
void Repository::for_each_matching(const mado::query::Ast_Node *filter,
                                   Action action) const {

    std::error_code ec;
    if (!std::filesystem::is_directory(root_, ec))
        return;

    mado::interpreter::Interpreter interp;

    // Visit valid entry directories and stop when the action requests it.
    // Обходим корректные каталоги записей, пока обработчик не остановит обход.
    for (const auto &dir_entry : std::filesystem::directory_iterator(root_, ec)) {
        if (ec)
            break;

        if (!dir_entry.is_directory())
            continue;

        try {
            mado::entry::Entry e = load_one(dir_entry.path());
            if (interp.evaluate(filter, e)) {
                if (!action(std::move(e)))
                    return;
            }
        } catch (const Repository_Error &) {
            // MAIN.md is missing or is not a regular file.
            // MAIN.md отсутствует или не является обычным файлом.
        } catch (const mado::entry::Entry_Error &) {
            // The directory name is not a valid timestamp
            // Имя каталога не является допустимой меткой времени.
        }
    }
}

// Creates an entry from the built-in header or the named template. Invalid
// names, missing templates, and collisions throw Repository_Error. I/O
// failures throw std::runtime_error.
// Создаёт запись из встроенного заголовка или шаблона. Неверные имена, отсутствующие шаблоны и
// коллизии вызывают Repository_Error. Сбой ввода-вывода вызывает std::runtime_error.
mado::entry::Entry Repository::create(
    const std::string &suffix, const std::optional<std::string> &template_name) const {
    // Reject path components and dots in suffixes before creating any files.
    // Отклоняем части пути и точки в суффиксах до создания любых файлов.
    if (suffix.find_first_of(INVALID_SUFFIX_CHARS) != std::string::npos)
        throw Repository_Error("Invalid suffix '" + suffix + "': must not contain " +
                               describe_invalid_chars(INVALID_SUFFIX_CHARS));

    std::string content = TASK_TEMPLATE;
    // Read the entire named template before creating an entry directory.
    // Читаем именованный шаблон целиком до создания каталога записи.
    if (template_name) {
        if (template_name->empty() || template_name->find_first_of(INVALID_PATH_CHARS) != std::string::npos ||
            template_name->find('\0') != std::string::npos)
            throw Repository_Error("Invalid template name: " + *template_name);

        const auto templates = find_templates(root_);
        if (!templates)
            throw Repository_Error("No templates directory found. Run 'mado init' to create it.");

        const auto path = *templates / (*template_name + MARKDOWN_EXTENSION);
        if (!std::filesystem::is_regular_file(path))
            throw Repository_Error("Template '" + *template_name +
                                   "' not found. Run 'mado init' to restore default templates.");
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("Cannot read template " + path.string());
        std::ostringstream buffer;
        buffer << in.rdbuf();
        if (buffer.bad() || in.bad())
            throw std::runtime_error("Cannot read template " + path.string());
        content = std::move(buffer).str();
    }

    std::string dir_name = mado::common::current_timestamp_utc();
    if (!suffix.empty())
        dir_name += suffix;

    const auto entry_dir = root_ / dir_name;
    const auto main_md = entry_dir / ENTRY_FILE;

    std::error_code ec;

    // Refuse if the directory already exists. This can happen if two
    // entries are created within the same second.
    // Отклоняем существующий каталог. Такое возможно при создании двух записей за одну
    // секунду.
    const bool exists = std::filesystem::exists(entry_dir, ec);
    if (ec)
        throw std::runtime_error("Cannot check " + entry_dir.string() + ": " + ec.message());
    if (exists)
        throw Repository_Error("Entry " + dir_name + " already exists; try again in a second");

    // Create MADO/<timestamp>/.
    // Создаём каталог MADO/<timestamp>/.
    const bool created = std::filesystem::create_directory(entry_dir, ec);
    if (ec)
        throw std::runtime_error("Cannot create " + entry_dir.string() + ": " + ec.message());
    if (!created)
        throw Repository_Error("Entry " + dir_name + " already exists; try again in a second");

    std::ofstream out(main_md, std::ios::binary);
    out << content;
    out.close();
    // Roll back the new entry when writing its file fails.
    // Откатываем создание записи, если не удалось записать её файл.
    if (!out) {
        std::filesystem::remove(main_md, ec);
        std::filesystem::remove(entry_dir, ec);
        throw std::runtime_error("Cannot write " + main_md.string());
    }

    return load_one(entry_dir);
}

// Loads one entry from MADO/<timestamp>/.
//
// The entry directory name must be a valid timestamp; otherwise
// set_time throws Entry_Error. The directory must contain a regular
// file named MAIN.md, otherwise Repository_Error is thrown.
//
// The entry's TIME is taken from the directory name and MTIME from
// the modification time of MAIN.md.
//
// MAIN.md is parsed as a header of "- KEY: value" lines, at most
// MAX_HEADER_LINES of them. Only the first occurrence of each known
// field (NAME, TAGS, STATUS, PRIORITY, DEADLINE) is used; later
// duplicates are ignored. Unknown keys and lines that do not start
// with "- " are skipped. Invalid values (e.g. non-numeric PRIORITY,
// malformed DEADLINE) are ignored, and the field keeps its default.
//
// Errors:
//   - Repository_Error: MAIN.md is missing or is not a regular file.
//   - std::runtime_error: an I/O operation on MAIN.md failed.
//   - Entry_Error: the directory name is not a valid timestamp.
// Читает до MAX_HEADER_LINES строк метаданных MAIN.md. Использует только первое вхождение
// каждого поля, пропуская неизвестные поля и неверные значения. TIME берётся из имени
// каталога, MTIME — из файла. Отсутствующий или неподходящий файл вызывает Repository_Error,
// неверная метка — Entry_Error, сбой ввода-вывода — std::runtime_error.
mado::entry::Entry Repository::load_one(const std::filesystem::path &entry_dir) const {
    mado::entry::Entry e;

    auto main_md = entry_dir / ENTRY_FILE;

    std::ifstream in(main_md);
    if (!in) {
        if (!std::filesystem::is_regular_file(main_md))
            throw Repository_Error("Missing regular file " + main_md.string());
        throw std::runtime_error("Cannot open " + main_md.string());
    }

    // Time comes from the directory name.
    // Время создания берётся из имени каталога.
    std::string dir_name = entry_dir.filename().string();

    // Allow suffixes in dir names after timestamp part.
    // Допускаем суффикс после метки времени в имени каталога.
    if (dir_name.size() > 15)
        dir_name.resize(15);

    // Old format uses 'T' as the date/time separator - accept it too.
    // Поддерживаем старый разделитель даты и времени T.
    if (dir_name.size() > 8 && dir_name[8] == 'T')
        dir_name[8] = '-';

    e.set_time(dir_name); // exception if not valid timestamp / Недопустимая метка вызывает исключение.

    // mtime comes from the MAIN.md modification time.
    // Время изменения берётся из MAIN.md.
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(main_md, ec);
    if (ec)
        throw std::runtime_error("Cannot stat " + main_md.string() + ": " + ec.message());

    e.set_mtime(mado::common::format_timestamp_utc(
        std::chrono::system_clock::to_time_t(
            std::chrono::floor<std::chrono::seconds>(
                file_time_to_system_clock(ftime)))));

    e.set_path(std::move(main_md));

    std::string line;
    int line_number = 0;

    bool has_name = false;
    bool has_tags = false;
    bool has_status = false;
    bool has_priority = false;
    bool has_deadline = false;

    // Read only the bounded metadata header and ignore duplicate fields.
    // Читаем только ограниченный заголовок метаданных и пропускаем дубли полей.
    while (line_number < MAX_HEADER_LINES && std::getline(in, line)) {
        ++line_number;

        if (has_name && has_tags && has_status && has_priority && has_deadline)
            break;

        mado::common::trim_inplace(line);
        if (line.empty())
            continue;

        if (!has_name && line.starts_with(NAME_PREFIX)) {
            line.erase(0, NAME_PREFIX.size());
            mado::common::trim_inplace(line);
            e.set_name(std::move(line));
            has_name = true;
        } else if (!has_tags && line.starts_with(TAGS_PREFIX)) {
            line.erase(0, TAGS_PREFIX.size());
            mado::common::trim_inplace(line);
            e.set_tags(split_tags(line));
            has_tags = true;
        } else if (!has_status && line.starts_with(STATUS_PREFIX)) {
            line.erase(0, STATUS_PREFIX.size());
            mado::common::trim_inplace(line);
            e.set_status(std::move(line));
            has_status = true;
        } else if (!has_priority && line.starts_with(PRIORITY_PREFIX)) {
            line.erase(0, PRIORITY_PREFIX.size());
            mado::common::trim_inplace(line);
            if (auto p = parse_priority(line))
                e.set_priority(*p);
            has_priority = true;
        } else if (!has_deadline && line.starts_with(DEADLINE_PREFIX)) {
            line.erase(0, DEADLINE_PREFIX.size());
            mado::common::trim_inplace(line);
            if (mado::common::is_timestamp(line))
                e.set_deadline(std::move(line));
            has_deadline = true;
        }
    }

    // Separate malformed entries from a failed read of a regular file.
    // Отличаем некорректную запись от ошибки чтения обычного файла.
    if (in.bad()) {
        if (!std::filesystem::is_regular_file(e.path()))
            throw Repository_Error("Missing regular file " + e.path().string());
        throw std::runtime_error("Cannot read " + e.path().string());
    }

    return e;
}

// Collects a deterministic, portable snapshot before opening the destination.
// I/O failures throw runtime_error/filesystem_error; unsafe destinations throw Repository_Error.
// Собирает переносимый снимок в стабильном порядке до открытия файла назначения.
// Сбои ОС вызывают runtime_error/filesystem_error; опасный путь — Repository_Error.
void Repository::dump_mtimes(const std::filesystem::path &file) const {
    const auto destination = file.empty() ? root_ / MTIMES_FILE_NAMES[0] : file;
    std::vector<std::pair<std::string, int64_t>> records;

    // Include every real entry file, independently of its metadata contents.
    // Включаем каждый настоящий файл записи независимо от содержимого метаданных.
    for (const auto &dir : std::filesystem::directory_iterator(root_)) {
        const auto main = dir.path() / ENTRY_FILE;
        if (dir.is_symlink() || !dir.is_directory() ||
            std::filesystem::is_symlink(main) || !std::filesystem::is_regular_file(main))
            continue;

        // Check that the dump destination is not an entry's MAIN.md file.
        // Проверяем, что файл назначения дампа — не MAIN.md одной из заметок.
        if (std::filesystem::exists(destination) && std::filesystem::equivalent(destination, main))
            throw Repository_Error("Mtime index cannot overwrite entry " + main.string());

        const auto file_time = std::filesystem::last_write_time(main);

        // Reject dates outside the index range before narrowing to Unix nanoseconds.
        // Отклоняем даты вне диапазона индекса до сужения к наносекундам Unix.
        const auto wide_time = std::filesystem::file_time_type::clock::to_sys(
            std::chrono::time_point<std::filesystem::file_time_type::clock,
                                    std::chrono::duration<long double>>{file_time});
        const auto unix_ticks = std::chrono::duration<long double, std::nano>(
                                    wide_time.time_since_epoch())
                                    .count();
        if (unix_ticks <= static_cast<long double>(std::numeric_limits<int64_t>::lowest()) ||
            unix_ticks >= static_cast<long double>(std::numeric_limits<int64_t>::max()))
            throw Repository_Error("Mtime outside index range for " + main.string());

        const auto time = std::filesystem::file_time_type::clock::to_sys(file_time);

        const auto id = dir.path().filename().string();
        if (id.find_first_of(INVALID_PATH_CHARS) != std::string::npos)
            throw Repository_Error("Invalid entry identifier " + dir.path().string());

        records.emplace_back(id,
                             std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count());
    }

    std::sort(records.begin(), records.end());

    std::ofstream out(destination, std::ios::binary);
    out << MTIMES_HEADER << '\n';

    // Quoted identifiers preserve spaces in directory suffixes.
    // Кавычки сохраняют пробелы в суффиксах имён каталогов.
    for (const auto &[id, time] : records)
        out << std::quoted(id) << '\t' << time << '\n';

    out.close();
    if (!out)
        throw std::runtime_error("Cannot write mtime index " + destination.string());
}

// Validates all records before restoring times, optionally preserving newer modifications.
// Domain errors throw Repository_Error; filesystem/I/O and allocation exceptions propagate.
// Проверяет все записи до восстановления, при необходимости сохраняя более новые изменения.
// Ошибки данных вызывают Repository_Error; исключения ОС и памяти передаются выше.
void Repository::restore_mtimes(const std::filesystem::path &file, bool strict, bool keep_newer) const {
    auto source = file.empty() ? root_ / MTIMES_FILE_NAMES[0] : file;

    // Search supported names only when no explicit path was supplied.
    // Ищем поддерживаемые имена только при отсутствии явно заданного пути.
    if (file.empty()) {
        for (const auto *name : MTIMES_FILE_NAMES) {
            const auto candidate = root_ / name;
            if (std::filesystem::is_regular_file(candidate)) {
                source = candidate;
                break;
            }
        }
    }

    std::ifstream in(source, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot read mtime index " + source.string());

    std::string line;
    if (!std::getline(in, line) || line != MTIMES_HEADER)
        throw Repository_Error("Invalid mtime index header in " + source.string());

    std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> records;
    std::unordered_set<std::string> seen;

    // Reject invalid data and unsafe targets; missing files are optional unless strict.
    // Отклоняем неверные данные и опасные цели; отсутствующие файлы обязательны только при strict.
    while (std::getline(in, line)) {
        std::istringstream row(line);
        std::string id;
        int64_t ticks;
        if (!(row >> std::quoted(id) >> ticks) || !(row >> std::ws).eof() ||
            id.empty() || id == "." || id == ".." ||
            id.find_first_of(INVALID_PATH_CHARS) != std::string::npos || !seen.insert(id).second)
            throw Repository_Error("Invalid mtime index record in " + source.string());

        const auto dir = root_ / id;
        const auto main = dir / ENTRY_FILE;

        // Symbolic links are unsafe targets in either mode, including dangling links.
        // Символические ссылки опасны в обоих режимах, в том числе ссылки без цели.
        if (std::filesystem::is_symlink(dir) || std::filesystem::is_symlink(main))
            throw Repository_Error("Invalid symbolic link entry " + main.string());

        // A partial copy or deleted entry must not block ordinary restoration.
        // Неполная копия или удалённая запись не должны мешать обычному восстановлению.
        if (!std::filesystem::exists(main) && !strict)
            continue;
        if (!std::filesystem::is_regular_file(main))
            throw Repository_Error("Missing regular entry file " + main.string());

        // Check native clock bounds using a wide duration before integral clock arithmetic.
        // Проверяем границы файловых часов широким типом до целочисленных преобразований.
        using File_Duration = std::filesystem::file_time_type::duration;
        const auto wide_time = std::filesystem::file_time_type::clock::from_sys(
            std::chrono::sys_time<std::chrono::duration<long double>>{
                std::chrono::duration<long double>{std::chrono::nanoseconds{ticks}}});
        const auto native_ticks = std::chrono::duration<long double, File_Duration::period>(
                                      wide_time.time_since_epoch())
                                      .count();
        if (native_ticks <= static_cast<long double>(std::numeric_limits<File_Duration::rep>::lowest()) ||
            native_ticks >= static_cast<long double>(std::numeric_limits<File_Duration::rep>::max()))
            throw Repository_Error("Mtime outside filesystem clock range in " + source.string());

        const auto time = std::filesystem::file_time_type::clock::from_sys(
            std::chrono::sys_time<std::chrono::nanoseconds>{std::chrono::nanoseconds{ticks}});

        records.emplace_back(main, time);
    }

    if (in.bad())
        throw std::runtime_error("Cannot read mtime index " + source.string());

    // Only validated records reach the write phase; OS failures propagate with their path.
    // В фазу записи попадают только проверенные строки; сбои ОС передаются с путём.
    for (const auto &[main, time] : records) {
        // Compare native file times immediately before writing to preserve more recent edits.
        // Сравниваем файловые времена прямо перед записью, сохраняя более свежие изменения.
        if (keep_newer && std::filesystem::last_write_time(main) > time)
            continue;

        std::filesystem::last_write_time(main, time);
    }
}

} // namespace mado::repository
