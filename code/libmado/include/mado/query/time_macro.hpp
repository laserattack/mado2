#pragma once

#include <ctime>
#include <optional>
#include <string>

namespace mado::query {

/// Resolves a supported time macro name (including fuzzy matches) using UTC
/// `now` and a day, week, month, or year offset as appropriate. Returns
/// `std::nullopt` when no macro matches, the offset is invalid, or the result
/// is outside the supported timestamp range.
/// @throws std::runtime_error If the macro needs the current time and
/// `now` cannot be converted to UTC.
std::optional<std::string> resolve_time_macro(const std::string &name,
                                              int offset, std::time_t now);

}
