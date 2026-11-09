#pragma once

#include <mado/entry/entry.hpp>

#include <optional>
#include <string>
#include <vector>

namespace cli {

enum class Sort_Field { Time,
                        Mtime,
                        Path,
                        Name,
                        Priority,
                        Deadline,
                        Status,
                        Tags };

struct Sort_Criterion {
    Sort_Field field;
    bool descending;
};

std::optional<std::vector<Sort_Criterion>> parse_sort(const std::string &spec);

void sort_entries(std::vector<mado::entry::Entry> &entries,
                  const std::vector<Sort_Criterion> &criteria);

} // namespace cli
