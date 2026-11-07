#pragma once

#include <cstdint>
#include <iterator>
#include <optional>
#include <string>

#include <mado/common/text_utils.hpp>

namespace mado::common {

// Returns a match score if each character in pattern is found
// sequentially within str. Returns std::nullopt otherwise.
// Возвращает оценку, если символы шаблона встречаются в строке по порядку. Иначе возвращает
// nullopt.
std::optional<int32_t> fuzzy_match(const std::string &pattern, const std::string &str, bool ignore_case);

// Returns the exact case-insensitive match, otherwise the highest fuzzy score.
// Equal scores keep the first candidate. Empty or unmatched input returns end.
// The name projection and allocations may throw. Defined here for arbitrary ranges.
// Возвращает точное совпадение без учёта регистра либо лучшую нечёткую оценку.
// При равенстве сохраняется первый кандидат. Пустой ввод и отсутствие совпадений
// дают end. Проекция имени и выделение памяти могут бросать исключения.
// Определена в заголовке для работы с произвольными диапазонами.
template <class Range, class Name>
auto find_fuzzy_match(const std::string &pattern, const Range &candidates, Name get_name) {
    const auto end = std::end(candidates);
    auto best = end;
    std::optional<int32_t> best_score;
    if (pattern.empty())
        return best;

    // Exact names override any earlier fuzzy candidate.
    // Точное имя имеет приоритет над любым ранее найденным нечётким совпадением.
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
