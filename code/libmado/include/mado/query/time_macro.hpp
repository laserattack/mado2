#pragma once

#include <ctime>
#include <optional>
#include <string>

namespace mado::query {

/// @throws std::runtime_error If the macro needs the current time and
/// `now` cannot be converted to UTC.
std::optional<std::string> resolve_time_macro(const std::string &name,
                                              int offset, std::time_t now);

}
