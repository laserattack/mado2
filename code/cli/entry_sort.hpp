#pragma once

#include <mado/entry/entry.hpp>

#include <optional>
#include <string>
#include <vector>

namespace cli {

// Identifies a field that can be used for ordering entries.
// Задаёт поле, по которому можно сортировать записи.
enum class Sort_Field { Time,
                        Mtime,
                        Path,
                        Name,
                        Priority,
                        Deadline,
                        Status,
                        Tags };

// Stores one sort field and its direction.
// Хранит поле сортировки и её направление.
struct Sort_Criterion {
    Sort_Field field;
    bool descending;
};

// Parses comma-separated fields and optional signs. Skips blank fields and
// returns nullopt for empty input or an invalid field. May throw
// std::bad_alloc.
// Разбирает поля через запятую и необязательные знаки. Пропускает пустые поля, возвращает
// nullopt при пустом вводе или неверном поле. Возможен std::bad_alloc.
std::optional<std::vector<Sort_Criterion>> parse_sort(const std::string &spec);

// Stably sorts entries by the given keys, leaving them alone when there are
// none. Comparison and allocation exceptions propagate.
// Стабильно сортирует записи по ключам. Без ключей порядок сохраняется. Исключения сравнения и
// выделения памяти передаются вызывающему коду.
void sort_entries(std::vector<mado::entry::Entry> &entries,
                  const std::vector<Sort_Criterion> &criteria);

} // namespace cli
