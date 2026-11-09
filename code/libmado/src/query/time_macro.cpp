#include <mado/query/time_macro.hpp>

#include <mado/common/fuzzy_match.hpp>
#include <mado/common/time_utils.hpp>

#include <chrono>
#include <cstdio>

namespace mado::query {

namespace {

using namespace std::chrono;

enum class Time_Macro { Now, Today, Yesterday, Tomorrow, Week, Month, Year, Never };

std::optional<Time_Macro> find_time_macro(const std::string &name) {
    static const std::pair<std::string, Time_Macro> macros[] = {
        {"now", Time_Macro::Now}, {"today", Time_Macro::Today},
        {"yesterday", Time_Macro::Yesterday}, {"tomorrow", Time_Macro::Tomorrow},
        {"week", Time_Macro::Week}, {"month", Time_Macro::Month},
        {"year", Time_Macro::Year}, {"never", Time_Macro::Never},
    };

    const auto found = mado::common::find_fuzzy_match(
        name, macros, [](const auto &item) -> const std::string & { return item.first; });
    if (found == std::end(macros))
        return std::nullopt;
    return found->second;
}

std::optional<std::string> format_macro_date(sys_days date, const std::tm *clock = nullptr) {
    if (date < sys_days{year{0}/January/1} || date > sys_days{year{9999}/December/31})
        return std::nullopt;
    const year_month_day parts{date};
    char text[32];
    if (clock) {
        std::snprintf(text, sizeof(text), "%04d%02u%02u-%02d%02d%02d",
                      int(parts.year()), unsigned(parts.month()), unsigned(parts.day()),
                      clock->tm_hour, clock->tm_min, clock->tm_sec);
    } else {
        std::snprintf(text, sizeof(text), "%04d%02u%02u",
                      int(parts.year()), unsigned(parts.month()), unsigned(parts.day()));
    }
    return std::string(text);
}

}

std::optional<std::string> resolve_time_macro(const std::string &name,
                                              int offset, std::time_t now) {
    if (name.empty() || offset < -999999 || offset > 999999)
        return std::nullopt;
    const auto macro = find_time_macro(name);
    if (!macro)
        return std::nullopt;

    if (*macro == Time_Macro::Never) {
        const std::tm midnight{};
        return format_macro_date(sys_days{year{9999}/January/1} + days{offset}, &midnight);
    }

    const auto clock = mado::common::utc_time(now);
    const int utc_year = clock.tm_year + 1900;
    const sys_days today{year{utc_year}/month{unsigned(clock.tm_mon + 1)}/
                         day{unsigned(clock.tm_mday)}};

    switch (*macro) {
    case Time_Macro::Now:
        return format_macro_date(today + days{offset}, &clock);
    case Time_Macro::Today:
        return format_macro_date(today + days{offset});
    case Time_Macro::Yesterday:
        return format_macro_date(today + days{offset - 1});
    case Time_Macro::Tomorrow:
        return format_macro_date(today + days{offset + 1});
    case Time_Macro::Week:
        return format_macro_date(today - days{(weekday{today}.iso_encoding() - 1)} +
                                 weeks{offset});
    case Time_Macro::Month:
    case Time_Macro::Year: {
        const bool by_month = *macro == Time_Macro::Month;
        const int total_months = utc_year * 12 + clock.tm_mon +
                                 (by_month ? offset : offset * 12);
        if (total_months < 0 || total_months >= 10000 * 12)
            return std::nullopt;
        char text[16];
        if (by_month)
            std::snprintf(text, sizeof(text), "%04d%02d", total_months / 12, total_months % 12 + 1);
        else
            std::snprintf(text, sizeof(text), "%04d", total_months / 12);
        return std::string(text);
    }
    case Time_Macro::Never:
        return std::nullopt;
    }
    return std::nullopt;
}

}
