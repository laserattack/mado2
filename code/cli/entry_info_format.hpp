#pragma once

#include <mado/entry/entry.hpp>

#include <memory>
#include <optional>
#include <ostream>
#include <string>

namespace cli {

// Writes a quoted JSON string and escapes quotes and backslashes. Stream
// failures follow the output stream's exception mask.
// Пишет JSON-строку в кавычках, экранируя кавычки и обратные косые черты. Исключения
// определяет маска потока.
void print_json_string(const std::string &str, std::ostream &os);

// Selects the representation used to print entries.
// Задаёт формат вывода записей.
enum class Entry_Info_Format {
    Default, // compatible with emacs compile buffer / Совместимо с буфером компиляции Emacs.
    Path,    // only paths / Только пути.
    Jsonl,   // 1 line = 1 json / Один объект JSON на строку.
};

// Records fields omitted from output. False keeps a field visible.
// Хранит скрываемые поля. False оставляет поле видимым.
struct Hidden_Entry_Fields {
    bool path = false;
    bool time = false;
    bool mtime = false;
    bool name = false;
    bool priority = false;
    bool deadline = false;
    bool status = false;
    bool tags = false;
};

// Base class for all entry info formatters. Each formatter knows how
// to write one Entry to a stream in its own format.
// Базовый класс форматтеров. Каждый выводит одну запись в своём формате.
class Entry_Info_Formatter {
  public:
    // Defines the output interface for a single entry.
    // Destroys a formatter through its base pointer. No exceptions.
    // Определяет интерфейс вывода записи. Удаление через базовый указатель не выбрасывает
    // исключений.
    virtual ~Entry_Info_Formatter() = default;

    // Writes one entry with the chosen visibility. Allocation and stream
    // exceptions propagate.
    // Пишет одну запись с выбранной видимостью. Исключения выделения памяти и потока
    // передаются вызывающему коду.
    virtual void write(const mado::entry::Entry &e, std::ostream &os,
                       const Hidden_Entry_Fields &hidden = {}) const = 0;
};

// Prefers an exact name, then fuzzy matching. Returns nullopt if unknown.
// May throw std::bad_alloc.
// Сначала ищет точное имя, затем нечёткое совпадение. Если совпадения нет, возвращает nullopt.
// Возможен std::bad_alloc.
std::optional<Entry_Info_Format> parse_entry_info_format(const std::string &name);

// Allocates a formatter or returns nullptr for an unknown format.
// May throw std::bad_alloc.
// Создаёт форматтер или возвращает nullptr для неизвестного формата. Возможен std::bad_alloc.
std::unique_ptr<Entry_Info_Formatter> make_entry_info_formatter(Entry_Info_Format fmt);

} // namespace cli
