#pragma once

#include <cstdint>
#include <iterator>
#include <optional>
#include <string>

#include <mado/common/text_utils.hpp>

namespace mado::common {

/// Returns a score when every character in `pattern` occurs in order in `str`;
/// comparisons honor `ignore_case`. Otherwise returns `std::nullopt`. An empty
/// pattern matches with score 100.
std::optional<int32_t> fuzzy_match(const std::string &pattern, const std::string &str, bool ignore_case);

/// Returns the candidate iterator with the highest fuzzy score, preferring an
/// exact case-insensitive match immediately. Returns `std::end(candidates)` if
/// the pattern is empty or no candidate matches.
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
