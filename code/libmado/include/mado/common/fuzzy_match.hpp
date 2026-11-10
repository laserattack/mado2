#pragma once

#include <cstdint>
#include <iterator>
#include <optional>
#include <string>

#include <mado/common/text_utils.hpp>

namespace mado::common {

// Returns a match score if each character in pattern is found
// sequentially within str. Returns std::nullopt otherwise
std::optional<int32_t> fuzzy_match(const std::string &pattern, const std::string &str, bool ignore_case);

/// @throws ... Any exception from accessing or iterating over the range
/// or invoking `get_name`.
template <class Range, class Name>
auto find_fuzzy_match(const std::string &pattern, const Range &candidates, Name get_name) {
    const auto end = std::end(candidates);
    auto best = end;
    std::optional<int32_t> best_score;
    if (pattern.empty())
        return best;

    for (auto candidate = std::begin(candidates); candidate != end; ++candidate) {
        const std::string &name = get_name(*candidate);
        if (equals_ignore_case(pattern, name))
            return candidate;
        const auto score = fuzzy_match(pattern, name, true);
        if (score && (!best_score || *score > *best_score)) {
            best_score = score;
            best = candidate;
        }
    }
    return best;
}

} // namespace mado::common
