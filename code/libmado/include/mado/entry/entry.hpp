#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace mado::entry {

// Reports invalid entry metadata.
// Сообщает о некорректных метаданных записи.
class Entry_Error : public std::runtime_error {
  public:
    // Stores the metadata diagnostic. May throw std::bad_alloc.
    // Сохраняет диагностику метаданных. Возможен std::bad_alloc.
    explicit Entry_Error(const std::string &message)
        : std::runtime_error(message) {}
};

// Holds the metadata of a single entry, including defaults for absent fields.
// Хранит метаданные одной записи, включая значения по умолчанию.
class Entry {
  public:
    // Getters
    // Returns the entry file path. No exceptions.
    // Методы чтения. Возвращают сохранённые поля без исключений.
    const std::filesystem::path &path() const { return path_; }
    // Returns the name, or an empty string if absent. No exceptions.
    // Возвращает имя или пустую строку при его отсутствии. Исключений нет.
    const std::string &name() const { return name_; }
    // Returns the status, or an empty string if absent. No exceptions.
    // Возвращает статус или пустую строку при его отсутствии. Исключений нет.
    const std::string &status() const { return status_; }
    // Returns the priority, defaulting to zero. No exceptions.
    // Возвращает приоритет, по умолчанию ноль. Исключений нет.
    uint16_t priority() const { return priority_; }
    // Returns tags, using one empty tag when absent. No exceptions.
    // Возвращает теги или один пустой тег при их отсутствии. Исключений нет.
    const std::vector<std::string> &tags() const { return tags_; }
    // Returns the entry creation timestamp. No exceptions.
    // Возвращает время создания записи. Исключений нет.
    const std::string &time() const { return time_; }
    // Returns the entry modification timestamp. No exceptions.
    // Возвращает время изменения записи. Исключений нет.
    const std::string &mtime() const { return mtime_; }
    // Returns the deadline or the never timestamp. No exceptions.
    // Возвращает срок или метку отсутствия срока. Исключений нет.
    const std::string &deadline() const { return deadline_; }

    // Setters
    // Stores the supplied path. No exceptions.
    // Методы изменения. Сохраняют новые значения с проверкой там, где она предусмотрена.
    void set_path(std::filesystem::path p);
    // Stores the supplied name, including an empty name. No error result or
    // exceptions.
    // Сохраняет имя, в том числе пустое. Ошибок и исключений нет.
    void set_name(std::string n);
    // Stores the supplied status, including an empty status. No error result
    // or exceptions.
    // Сохраняет статус, в том числе пустой. Ошибок и исключений нет.
    void set_status(std::string s);
    // Stores a priority from 0 to 999. Throws Entry_Error outside that range.
    // Сохраняет приоритет от 0 до 999. Выход за диапазон вызывает Entry_Error.
    void set_priority(uint16_t p);
    // Stores the tags. Throws Entry_Error for an empty vector.
    // Сохраняет теги. Пустой вектор вызывает Entry_Error.
    void set_tags(std::vector<std::string> t);
    // Stores a timestamp. Throws Entry_Error for invalid input.
    // Сохраняет метку времени. Неверное значение вызывает Entry_Error.
    void set_time(std::string t);
    // Stores a modification timestamp. Throws Entry_Error for invalid input.
    // Сохраняет время изменения. Неверное значение вызывает Entry_Error.
    void set_mtime(std::string t);
    // Stores a deadline timestamp. Throws Entry_Error for invalid input.
    // Сохраняет срок. Неверное значение вызывает Entry_Error.
    void set_deadline(std::string t);

  private:
    std::filesystem::path path_ = "";
    std::string name_ = "";
    std::string status_ = "";
    uint16_t priority_ = 0;
    std::vector<std::string> tags_ = {""};
    std::string time_ = "99990101-000000";
    std::string mtime_ = "99990101-000000";
    std::string deadline_ = "99990101-000000";
};

} // namespace mado::entry
