/*
 * Based on fuzzy_match by Philip Jones (MIT).
 * https://opensource.org/licenses/MIT
 */

#include "fuzzy_match.hpp"

#include <cctype>

namespace mado::common {
namespace {

constexpr int32_t UNMATCHED_LETTER_PENALTY = -1;
constexpr int32_t ADJACENCY_BONUS = 15;
constexpr int32_t SEPARATOR_BONUS = 30;
constexpr int32_t CAMEL_BONUS = 30;
constexpr int32_t FIRST_LETTER_BONUS = 15;
constexpr int32_t LEADING_LETTER_PENALTY = -5;
constexpr int32_t MAX_LEADING_LETTER_PENALTY = -15;

int32_t compute_score(size_t jump, bool first_char,
                      char current, char previous) {

    int32_t score = 0;

    if (!first_char && jump == 0) {
        score += ADJACENCY_BONUS;
    }
    if (!first_char || jump > 0) {
        if (std::isupper(static_cast<unsigned char>(current)) &&
            std::islower(static_cast<unsigned char>(previous))) {
            score += CAMEL_BONUS;
        }
        if (std::isalnum(static_cast<unsigned char>(current)) &&
            !std::isalnum(static_cast<unsigned char>(previous))) {
            score += SEPARATOR_BONUS;
        }
    }
    if (first_char && jump == 0) {
        score += FIRST_LETTER_BONUS;
    }

    if (first_char) {
        score += std::max(LEADING_LETTER_PENALTY * static_cast<int32_t>(jump),
                          MAX_LEADING_LETTER_PENALTY);
    }

    return score;
}

std::optional<int32_t> match_recurse(const std::string &pattern,
                                     const std::string &str,
                                     size_t from, size_t pattern_pos,
                                     int32_t score, bool first_char,
                                     bool ignore_case) {

    if (pattern_pos == pattern.size()) {
        return score;
    }

    std::optional<int32_t> best;

    for (size_t pos = from; pos < str.size(); ++pos) {
        const char p = pattern[pattern_pos];
        const char s = str[pos];

        const bool same = ignore_case
                              ? (std::tolower(static_cast<unsigned char>(p)) ==
                                 std::tolower(static_cast<unsigned char>(s)))
                              : (p == s);
        if (!same) {
            continue;
        }

        const char previous = (pos > 0) ? str[pos - 1] : '\0';

        auto subscore = match_recurse(
            pattern, str,
            pos + 1, pattern_pos + 1,
            compute_score(pos, first_char, s, previous),
            false,
            ignore_case);

        if (subscore && (!best || *subscore > *best)) {
            best = *subscore;
        }
    }

    if (!best) {
        return std::nullopt;
    }
    return score + *best;
}

} // namespace

std::optional<int32_t> fuzzy_match(const std::string &pattern,
                                   const std::string &str,
                                   bool ignore_case) {

    if (pattern.empty()) {
        return 100;
    }
    if (str.size() < pattern.size()) {
        return std::nullopt;
    }

    const int32_t score = 100 + UNMATCHED_LETTER_PENALTY *
                                    static_cast<int32_t>(str.size() - pattern.size());

    return match_recurse(pattern, str, 0, 0, score, true, ignore_case);
}

} // namespace mado::common
