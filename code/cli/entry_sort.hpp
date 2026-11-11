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

/// Parses comma-separated sort fields with optional `+`/`-` direction
/// prefixes. Returns nullopt if the specification is empty or contains an
/// unknown/empty field.
std::optional<std::vector<Sort_Criterion>> parse_sort(const std::string &spec);

/// Stable-sorts entries by the given criteria in order; leaves them unchanged
/// when `criteria` is empty.
void sort_entries(std::vector<mado::entry::Entry> &entries,
                  const std::vector<Sort_Criterion> &criteria);

} // namespace cli
