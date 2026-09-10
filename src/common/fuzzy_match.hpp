#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace mado::common {

// Returns a match score if each character in pattern is found
// sequentially within str. Returns std::nullopt otherwise
std::optional<int32_t> fuzzy_match(const std::string &pattern, const std::string &str, bool ignore_case);

} // namespace mado::common
