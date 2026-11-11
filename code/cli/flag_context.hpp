#pragma once

#include <initializer_list>

// RAII wrapper around flag.h's opaque flag context `void *`.
//
// flag.h provides `flag_c_new()` / `flag_c_free()` for explicit lifetime
// management. This wrapper calls `flag_c_free()` automatically when it
// goes out of scope, so commands don't have to sprinkle `flag_c_free()`
// on every error path.

extern "C" {
#include "flag.h/flag.h"
}

namespace cli {

class Flag_Context {
  public:
    /// Creates an owning flag context. `name` prevents flag.h from consuming
    /// the first argv element as the program name.
    explicit Flag_Context(const char *name)
        : ctx_(flag_c_new(name)) {}

    /// Copying is disabled because this object owns the flag context.
    Flag_Context(const Flag_Context &) = delete;
    /// Copy assignment is disabled because this object owns the flag context.
    Flag_Context &operator=(const Flag_Context &) = delete;

    /// Frees the owned flag context.
    ~Flag_Context() {
        flag_c_free(ctx_);
    }

    /// Returns the underlying context for flag_c_* calls.
    operator void *() const { return ctx_; }

    /// Registers a string flag for each name, initialized with the default.
    void add_string(char **value, std::initializer_list<const char *> names,
                    const char *default_value, const char *description) {
        for (const auto *name : names)
            flag_c_str_var(ctx_, value, name, default_value, description);
    }

    /// Registers a false-by-default boolean flag for each name.
    void add_bool(bool *value, std::initializer_list<const char *> names,
                  const char *description) {
        for (const auto *name : names)
            flag_c_bool_var(ctx_, value, name, false, description);
    }

  private:
    void *ctx_;
};

} // namespace cli
