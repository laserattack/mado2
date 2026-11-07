#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <mado/entry/entry.hpp>
#include <mado/query/ast.hpp>

namespace mado::repository {

// Template directories in lookup order; the first is the default. No errors or exceptions.
// Каталоги шаблонов в порядке поиска; первый используется по умолчанию. Ошибок и исключений нет.
inline constexpr const char *TEMPLATE_DIR_NAMES[] = {"templates", ".templates", "_templates"};

// Repository directory name shared by storage and CLI messages. No errors or exceptions.
// Имя каталога репозитория для хранения и сообщений CLI. Ошибок и исключений нет.
inline constexpr char ENTRY_DIR[] = "MADO";

// Mtime index filenames in lookup order; the first is the dump default. No errors or exceptions.
// Имена индекса mtime в порядке поиска; первое — стандартное для дампа. Ошибок и исключений нет.
inline constexpr const char *MTIMES_FILE_NAMES[] = {"mtimes", ".mtimes", "_mtimes"};

// Extension of Markdown entry and template files. No errors or exceptions.
// Расширение файлов записей и шаблонов Markdown. Ошибок и исключений нет.
inline constexpr char MARKDOWN_EXTENSION[] = ".md";

// Entry filename derived from the shared Markdown extension. May throw std::bad_alloc.
// Имя файла записи из общего расширения Markdown. Возможен std::bad_alloc.
inline const std::string ENTRY_FILE = std::string("MAIN") + MARKDOWN_EXTENSION;

// Retains a failed deletion and its OS error; the directory may be partially deleted.
// Stores data only; no returned errors or exceptions of its own.
// Сохраняет неудачное удаление и ошибку ОС; каталог может быть удалён частично.
// Только хранит данные; собственных возвращаемых ошибок и исключений нет.
struct Remove_Failure {
    mado::entry::Entry entry;
    std::error_code error;
};

// Separates successful deletions from failures so callers can report partial success.
// Stores data only; no returned errors or exceptions of its own.
// Разделяет успешные удаления и сбои для сообщения о частичном успехе.
// Только хранит данные; собственных возвращаемых ошибок и исключений нет.
struct Remove_Result {
    std::vector<mado::entry::Entry> removed;
    std::vector<Remove_Failure> failed;
};

// Represents an opened MADO directory and manages its entries and templates.
// Представляет открытый MADO и управляет записями и шаблонами.
class Repository {
  public:
    // Finds the nearest MADO directory from `from`. Returns nullopt if
    // none is found or the path cannot be resolved. May throw std::bad_alloc.
    // Ищет ближайший MADO. Если каталог не найден или путь не разрешается, возвращает nullopt.
    // Возможен std::bad_alloc.
    static std::optional<Repository> open(const std::filesystem::path &from);

    // Initializes the nearest repository and restores its default templates.
    // With force, initializes in `where` even if one exists above.
    // I/O failures throw std::runtime_error or filesystem_error.
    // Инициализирует ближайший репозиторий и восстанавливает стандартные шаблоны. Force
    // выбирает указанный каталог. Ошибки ввода-вывода выбрасывают std::runtime_error или
    // filesystem_error.
    static Repository init(const std::filesystem::path &where, bool force = false);

    // Load entries matching the filter.
    // Skips invalid entries. I/O and allocation exceptions propagate.
    // Загружает подходящие записи, пропуская некорректные. Исключения ввода-вывода и выделения
    // памяти передаются вызывающему коду.
    std::vector<mado::entry::Entry> find(const mado::query::Ast_Node *filter) const;

    // Loads matching entries, then attempts every deletion and returns successes and OS errors.
    // Failed directories may be partially deleted. Loading and allocation exceptions propagate.
    // Загружает подходящие записи, пробует удалить каждую и возвращает успехи и ошибки ОС.
    // При сбое каталог может быть удалён частично. Исключения чтения и памяти передаются выше.
    Remove_Result remove(const mado::query::Ast_Node *filter) const;

    // Creates a UTC timestamped entry with an optional suffix. Uses a named
    // template when supplied and the built-in task header otherwise.
    // Invalid names, missing templates, and collisions throw Repository_Error.
    // I/O failures throw std::runtime_error or filesystem_error.
    // Создаёт запись с меткой UTC и необязательным суффиксом. Использует указанный шаблон или
    // встроенный заголовок. Некорректные имена, отсутствующие шаблоны и коллизии вызывают
    // Repository_Error. Сбои ввода-вывода вызывают std::runtime_error или filesystem_error.
    mado::entry::Entry create(
        const std::string &suffix = "",
        const std::optional<std::string> &template_name = std::nullopt) const;

    // Iterates over entries matching the filter and calls `action` for
    // each. The action receives the entry by value, so it can move it
    // out. False stops iteration. I/O and callback exceptions propagate.
    // Вызывает action для каждой подходящей записи, передавая её по значению. False
    // останавливает обход. Исключения ввода-вывода и обработчика передаются вызывающему коду.
    template <class Action>
    // Visits matching entries until the action returns false. Skips malformed
    // entries. I/O and callback exceptions can propagate.
    // Обходит подходящие записи до false от обработчика. Пропускает некорректные записи.
    // Исключения ввода-вывода и обработчика передаются вызывающему коду.
    void for_each_matching(const mado::query::Ast_Node *filter, Action action) const;

    // Saves MAIN.md modification times as Unix nanoseconds; an empty path uses MTIMES_FILE_NAMES[0].
    // Returns the absolute destination path. Invalid destinations throw Repository_Error;
    // I/O and allocation exceptions propagate.
    // Сохраняет время MAIN.md в наносекундах Unix; пустой путь выбирает MTIMES_FILE_NAMES[0].
    // Возвращает абсолютный путь назначения. Неверный путь вызывает Repository_Error;
    // исключения ввода-вывода и памяти передаются выше.
    std::filesystem::path dump_mtimes(const std::filesystem::path &file = {}) const;

    // Restores MAIN.md times; an empty path searches MTIMES_FILE_NAMES in order.
    // Returns the absolute selected index path after success.
    // Missing files are skipped unless strict is true; keep_newer preserves newer file times.
    // Invalid data/targets throw Repository_Error;
    // I/O and allocation exceptions propagate. Strict validation finishes before any writes.
    // Восстанавливает время MAIN.md; пустой путь ищет файл по порядку MTIMES_FILE_NAMES.
    // После успеха возвращает абсолютный путь выбранного индекса.
    // Отсутствующие файлы пропускаются при strict=false; keep_newer сохраняет более новые времена.
    // Неверные данные и цели вызывают
    // Repository_Error; сбои ОС и памяти передаются выше. Строгая проверка завершается до записи.
    std::filesystem::path restore_mtimes(const std::filesystem::path &file = {}, bool strict = false,
                        bool keep_newer = false) const;

    // Returns the MADO directory. No exceptions.
    // Возвращает каталог MADO. Исключений нет.
    const std::filesystem::path &root() const { return root_; }

  private:
    // Represents an opened MADO directory and manages its entries and
    // templates.
    // Takes ownership of the resolved repository path. No exceptions.
    // Представляет открытый MADO и управляет записями и шаблонами.
    explicit Repository(std::filesystem::path root) : root_(std::move(root)) {}

    // Loads metadata from MAIN.md. Missing files throw Repository_Error,
    // invalid timestamps throw Entry_Error, and I/O failures throw runtime_error.
    // Загружает метаданные MAIN.md. Отсутствующий файл вызывает Repository_Error, неверное
    // время — Entry_Error, сбой ввода-вывода — runtime_error.
    mado::entry::Entry load_one(const std::filesystem::path &entry_dir) const;

    std::filesystem::path root_;
};

} // namespace mado::repository
