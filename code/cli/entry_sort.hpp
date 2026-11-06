#pragma once

#include <mado/entry/entry.hpp>

#include <optional>
#include <string>
#include <vector>

namespace cli {

// Identifies a field that can be used for ordering entries.
enum class Sort_Field { Time,
                        Mtime,
                        Path,
                        Name,
                        Priority,
                        Deadline,
                        Status,
                        Tags };

// Stores one sort field and its direction.
struct Sort_Criterion {
    Sort_Field field;
    bool descending;
};

// Parses comma-separated fields and optional signs. Skips blank fields and
// returns nullopt for empty input or an invalid field. May throw
// std::bad_alloc.
std::optional<std::vector<Sort_Criterion>> parse_sort(const std::string &spec);

// Stably sorts entries by the given keys, leaving them alone when there are
// none. Comparison and allocation exceptions propagate.
void sort_entries(std::vector<mado::entry::Entry> &entries,
                  const std::vector<Sort_Criterion> &criteria);

} // namespace cli
