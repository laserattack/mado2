#pragma once

/// Public API for libmado.
/// In addition to documented exceptions, operations that allocate memory
/// can throw std::bad_alloc. Operations that grow strings or containers
/// can throw std::length_error if the result exceeds their maximum size.
namespace mado {

constexpr const char *VERSION = "0.1.0";

} // namespace mado
