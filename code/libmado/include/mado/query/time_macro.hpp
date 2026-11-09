#pragma once

#include <ctime>
#include <optional>
#include <string>

namespace mado::query {

std::optional<std::string> resolve_time_macro(const std::string &name,
                                              int offset, std::time_t now);

}
