#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <mado/entry/entry.hpp>
#include <mado/query/ast.hpp>

namespace mado::repository {

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

    // Finds entries matching the filter, removes their directories, and
    // returns successful removals. Skips failed deletions. I/O failures
    // while loading and allocation exceptions can propagate.
    // Удаляет каталоги подходящих записей и возвращает успешные удаления. Ошибки удаления
    // пропускаются, исключения чтения и выделения памяти передаются вызывающему коду.
    std::vector<mado::entry::Entry> remove(const mado::query::Ast_Node *filter) const;

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
