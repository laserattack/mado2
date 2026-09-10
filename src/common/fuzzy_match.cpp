/*
 * Based on fuzzy_match by Philip Jones (MIT).
 * https://opensource.org/licenses/MIT
 */

#include "fuzzy_match.hpp"

#include <cctype>

namespace mado::common {
namespace {

constexpr int32_t kUnmatchedLetterPenalty = -1;
constexpr int32_t kAdjacencyBonus = 15;
constexpr int32_t kSeparatorBonus = 30;
constexpr int32_t kCamelBonus = 30;
constexpr int32_t kFirstLetterBonus = 15;
constexpr int32_t kLeadingLetterPenalty = -5;
constexpr int32_t kMaxLeadingLetterPenalty = -15;

int32_t compute_score(size_t jump, bool first_char,
                      char current, char previous) {

    int32_t score = 0;

    if (!first_char && jump == 0) {
        score += kAdjacencyBonus;
    }
    if (!first_char || jump > 0) {
        if (std::isupper(static_cast<unsigned char>(current)) &&
            std::islower(static_cast<unsigned char>(previous))) {
            score += kCamelBonus;
        }
        if (std::isalnum(static_cast<unsigned char>(current)) &&
            !std::isalnum(static_cast<unsigned char>(previous))) {
            score += kSeparatorBonus;
        }
    }
    if (first_char && jump == 0) {
        score += kFirstLetterBonus;
    }

    if (first_char) {
        score += std::max(kLeadingLetterPenalty * static_cast<int32_t>(jump),
                          kMaxLeadingLetterPenalty);
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

    const int32_t score = 100 + kUnmatchedLetterPenalty *
                                    static_cast<int32_t>(str.size() - pattern.size());

    return match_recurse(pattern, str, 0, 0, score, true, ignore_case);
}

} // namespace mado::common
