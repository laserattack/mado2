/*
 * Based on fuzzy_match by Philip Jones (MIT).
 * https://opensource.org/licenses/MIT
 * На основе fuzzy_match Филипа Джонса, лицензия MIT.
 */

#include <mado/common/fuzzy_match.hpp>

#include <cctype>

namespace mado::common {
namespace {

// Base score adjustment per unmatched input byte.
// Поправка к оценке за каждый несовпавший байт.
constexpr int32_t UNMATCHED_LETTER_PENALTY = -1;
// Reward for adjacent matched characters.
// Бонус за соседние совпавшие символы.
constexpr int32_t ADJACENCY_BONUS = 15;
// Reward for matching after a word separator.
// Бонус за совпадение после разделителя слов.
constexpr int32_t SEPARATOR_BONUS = 30;
// Reward for matching at a camel-case boundary.
// Бонус за совпадение на границе camel case.
constexpr int32_t CAMEL_BONUS = 30;
// Reward for matching at the start of the input.
// Бонус за совпадение в начале строки.
constexpr int32_t FIRST_LETTER_BONUS = 15;
// Penalty per skipped byte before the first match.
// Штраф за каждый байт до первого совпадения.
constexpr int32_t LEADING_LETTER_PENALTY = -5;
// Caps the total leading-byte penalty.
// Ограничивает суммарный штраф за пропущенные начальные байты.
constexpr int32_t MAX_LEADING_LETTER_PENALTY = -15;

// Scores a fuzzy-match step using position and boundary bonuses. No
// exceptions.
// Вычисляет оценку шага нечёткого поиска с учётом позиции и границ слов. Исключений нет.
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

// Finds the best subsequence score from the current search state. Returns
// nullopt when there is no match.
// Ищет лучшую оценку подпоследовательности из текущего состояния. Без совпадений возвращает
// nullopt.
std::optional<int32_t> match_recurse(const std::string &pattern,
                                     const std::string &str,
                                     size_t from, size_t pattern_pos,
                                     int32_t score, bool first_char,
                                     bool ignore_case) {

    if (pattern_pos == pattern.size()) {
        return score;
    }

    std::optional<int32_t> best;

    // Explore each position that can match the next pattern byte.
    // Перебираем позиции, подходящие для следующего байта шаблона.
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

// Returns the best subsequence score or nullopt if the pattern cannot match.
// May throw std::bad_alloc.
// Возвращает лучшую оценку подпоследовательности или nullopt без совпадений. Возможен
// std::bad_alloc.
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
