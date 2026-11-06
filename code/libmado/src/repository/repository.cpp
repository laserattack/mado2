#include <mado/repository/repository.hpp>

#include <mado/common/text_utils.hpp>
#include <mado/common/time_utils.hpp>
#include <mado/interpreter/interpreter.hpp>

#include <chrono>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace mado::repository {

namespace {

// Directory name that identifies a repository.
// Имя каталога репозитория.
constexpr const char *ENTRY_DIR = "MADO";
// Filename that stores an entry and its metadata.
// Имя файла записи и её метаданных.
constexpr const char *ENTRY_FILE = "MAIN.md";
// Built-in task header and default task template contents.
// Встроенный заголовок задачи и содержимое стандартного шаблона задачи.
constexpr const char *TASK_TEMPLATE = "- NAME:\n- PRIORITY:\n- TAGS:\n- STATUS:\n- DEADLINE:\n";
// Default note template contents.
// Содержимое стандартного шаблона заметки.
constexpr const char *NOTE_TEMPLATE = "- NAME:\n- TAGS:\n";
// Caps metadata scanning per entry.
// Ограничивает число просматриваемых строк метаданных.
constexpr int MAX_HEADER_LINES = 30;

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

// Returns the first supported template directory or nullopt. Filesystem lookup
// errors throw std::filesystem::filesystem_error.
// Возвращает первый поддерживаемый каталог шаблонов или nullopt. Сбои поиска вызывают
// filesystem_error.
std::optional<std::filesystem::path> find_templates(const std::filesystem::path &root) {
    // Search supported template folders in precedence order.
    // Проверяем допустимые папки шаблонов в порядке приоритета.
    for (const auto *name : {"templates", ".templates", "_templates"}) {
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

// Converts a std::filesystem::file_time_type to
// std::chrono::system_clock::time_point.
//
// file_clock and system_clock have different epochs and durations,
// and C++17 provides no direct conversion. The workaround: take the
// difference between the given time point and "now" in the file clock
// (this is a duration and does not depend on the epoch), then apply
// that duration to "now" in the system clock.
// Переводит файловое время в системное через разницу с текущим временем, чтобы обойти различие
// эпох часов.
std::chrono::system_clock::time_point
file_time_to_system_clock(std::filesystem::file_time_type ft) {
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ft - std::filesystem::file_time_type::clock::now() +
        std::chrono::system_clock::now());
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

    const auto templates = find_templates(mado_dir).value_or(mado_dir / "templates");
    std::filesystem::create_directory(templates);
    write_default_template(templates / "note.md", NOTE_TEMPLATE);
    write_default_template(templates / "task.md", TASK_TEMPLATE);
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

// Removes matching entries and returns successful removals. Skips failed
// deletions. I/O failures encountered while loading can throw
// std::runtime_error.
// Удаляет отобранные записи и возвращает успешно удалённые. Неудачные удаления пропускает.
// Сбой чтения может вызвать std::runtime_error.
std::vector<mado::entry::Entry> Repository::remove(
    const mado::query::Ast_Node *filter) const {

    std::vector<mado::entry::Entry> result;
    for_each_matching(filter, [&](mado::entry::Entry e) {
        std::error_code ec;
        std::filesystem::remove_all(e.path().parent_path(), ec);
        // TODO: decide what to do when remove_all fails. Options:
        //   - skip silently (current)
        //   - throw std::filesystem::filesystem_error
        //   - return something like Remove_Result with both removed and failed entries
        // TODO: выбрать обработку сбоя remove_all. Сейчас запись пропускается. Возможные
        // варианты — исключение filesystem_error или результат со списком сбоев.
        if (!ec)
            result.push_back(std::move(e));
        return true;
    });
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
    auto is_safe_suffix = [](const std::string &s) {
        for (char c : s) {
            if (c == '\\' || c == '/' || c == '.' || c == ':')
                return false;
        }
        return true;
    };

    if (!is_safe_suffix(suffix))
        throw Repository_Error("Invalid suffix '" + suffix + "': must not contain '/', '\\', '.', or ':'");

    std::string content = TASK_TEMPLATE;
    // Read the entire named template before creating an entry directory.
    // Читаем именованный шаблон целиком до создания каталога записи.
    if (template_name) {
        if (template_name->empty() || template_name->find_first_of("/\\:") != std::string::npos ||
            template_name->find('\0') != std::string::npos)
            throw Repository_Error("Invalid template name: " + *template_name);

        const auto templates = find_templates(root_);
        if (!templates)
            throw Repository_Error("No templates directory found. Run 'mado init' to create it.");

        const auto path = *templates / (*template_name + ".md");
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

        if (!has_name && line.starts_with("- NAME:")) {
            line.erase(0, 7);
            mado::common::trim_inplace(line);
            e.set_name(std::move(line));
            has_name = true;
        } else if (!has_tags && line.starts_with("- TAGS:")) {
            line.erase(0, 7);
            mado::common::trim_inplace(line);
            e.set_tags(split_tags(line));
            has_tags = true;
        } else if (!has_status && line.starts_with("- STATUS:")) {
            line.erase(0, 9);
            mado::common::trim_inplace(line);
            e.set_status(std::move(line));
            has_status = true;
        } else if (!has_priority && line.starts_with("- PRIORITY:")) {
            line.erase(0, 11);
            mado::common::trim_inplace(line);
            if (auto p = parse_priority(line))
                e.set_priority(*p);
            has_priority = true;
        } else if (!has_deadline && line.starts_with("- DEADLINE:")) {
            line.erase(0, 11);
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

} // namespace mado::repository
