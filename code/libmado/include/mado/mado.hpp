#pragma once

/// Public API for libmado. Unless an API documents otherwise, C++ operations
/// that allocate memory can throw std::bad_alloc, and operations that grow
/// strings or containers can throw std::length_error if the result exceeds
/// their maximum size. C APIs report their documented errors through return
/// values and do not throw C++ exceptions.
namespace mado {

constexpr const char *VERSION = "0.1.0";

} // namespace mado
