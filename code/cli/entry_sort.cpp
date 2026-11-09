#include "entry_sort.hpp"

#include <mado/common/fuzzy_match.hpp>
#include <mado/common/text_utils.hpp>

#include <algorithm>
#include <compare>
#include <sstream>

namespace cli {

namespace {

std::optional<Sort_Field> parse_sort_field(const std::string &name) {
    static const std::pair<std::string, Sort_Field> fields[] = {
        {"time", Sort_Field::Time},
        {"mtime", Sort_Field::Mtime},
        {"path", Sort_Field::Path},
        {"name", Sort_Field::Name},
        {"priority", Sort_Field::Priority},
        {"deadline", Sort_Field::Deadline},
        {"status", Sort_Field::Status},
        {"tags", Sort_Field::Tags},
    };

    const auto found = mado::common::find_fuzzy_match(
        name, fields, [](const auto &item) -> const std::string & { return item.first; });
    if (found == std::end(fields))
        return std::nullopt;
    return found->second;
}

std::string join_tags(const mado::entry::Entry &entry) {
    std::string result;
    for (const auto &tag : entry.tags())
        result += tag;
    return result;
}

std::strong_ordering compare_entries(const mado::entry::Entry &a,
                                     const mado::entry::Entry &b,
                                     Sort_Field field) {
    switch (field) {
    case Sort_Field::Time:
        return a.time() <=> b.time();
    case Sort_Field::Mtime:
        return a.mtime() <=> b.mtime();
    case Sort_Field::Path:
        return a.path() <=> b.path();
    case Sort_Field::Name:
        return a.name() <=> b.name();
    case Sort_Field::Priority:
        return a.priority() <=> b.priority();
    case Sort_Field::Deadline:
        return a.deadline() <=> b.deadline();
    case Sort_Field::Status:
        return a.status() <=> b.status();
    case Sort_Field::Tags:
        return join_tags(a) <=> join_tags(b);
    }

    // Equal means the entries tie on this field, so sorting checks the next
    // criterion. If all criteria tie, stable_sort preserves their original order.
    return std::strong_ordering::equal;
}

} // namespace

std::optional<std::vector<Sort_Criterion>> parse_sort(const std::string &spec) {
    if (spec.empty())
        return std::nullopt;

    std::vector<Sort_Criterion> criteria;
    std::istringstream input(spec);
    std::string token;
    while (std::getline(input, token, ',')) {
        mado::common::trim_inplace(token);
        if (token.empty())
            continue;

        const bool descending = token.front() == '-';
        if (descending || token.front() == '+') {
            token.erase(0, 1);
            mado::common::trim_inplace(token);
        }
        if (token.empty())
            return std::nullopt;

        auto field = parse_sort_field(token);
        if (!field)
            return std::nullopt;
        criteria.push_back({*field, descending});
    }
    return criteria;
}

void sort_entries(std::vector<mado::entry::Entry> &entries,
                  const std::vector<Sort_Criterion> &criteria) {
    if (!criteria.empty()) {
        std::stable_sort(entries.begin(), entries.end(), [&](const auto &a, const auto &b) {
            for (const auto &criterion : criteria) {
                const auto order = compare_entries(a, b, criterion.field);
                if (order != 0)
                    return criterion.descending ? order > 0 : order < 0;
            }
            return false;
        });
    }
}

} // namespace cli
