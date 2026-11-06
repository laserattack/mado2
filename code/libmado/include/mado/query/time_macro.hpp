#pragma once

#include <ctime>
#include <optional>
#include <string>

namespace mado::query {

// Expands a time macro against a fixed UTC clock value. Returns nullopt for
// unknown names or dates outside 0000–9999. Clock conversion failures
// throw std::runtime_error. Allocation failures may throw std::bad_alloc.
// Раскрывает макрос относительно фиксированного времени UTC. Возвращает nullopt
// для неизвестного имени или даты вне 0000–9999. Сбой преобразования времени
// вызывает std::runtime_error, а сбой выделения памяти — std::bad_alloc.
std::optional<std::string> resolve_time_macro(const std::string &name,
                                              int offset, std::time_t now);

} // namespace mado::query
