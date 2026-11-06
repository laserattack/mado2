#pragma once

// RAII wrapper around flag.h's opaque flag context `void *`.
//
// flag.h provides `flag_c_new()` / `flag_c_free()` for explicit lifetime
// management. This wrapper calls `flag_c_free()` automatically when it
// goes out of scope, so commands don't have to sprinkle `flag_c_free()`
// on every error path.
// Обёртка RAII для контекста flag.h. Создаёт его через flag_c_new() и освобождает через
// flag_c_free() при выходе из области видимости, включая ветки ошибок.

extern "C" {
#include "flag.h/flag.h"
}

namespace cli {

// Owns a flag parser context and releases it at scope exit.
// Владеет контекстом разбора флагов и освобождает его при выходе из области видимости.
class Flag_Context {
  public:
    // Creates a new flag context. The name is used by flag.h so it
    // doesn't try to consume the first argv element as the program
    // name. Allocation failure is handled by flag.h.
    // Создаёт контекст флагов. Переданное имя позволяет flag.h не брать имя программы из argv.
    // Сбой выделения памяти обрабатывает flag.h.
    explicit Flag_Context(const char *name)
        : ctx_(flag_c_new(name)) {}

    Flag_Context(const Flag_Context &) = delete;
    Flag_Context &operator=(const Flag_Context &) = delete;

    // Releases the flag parser context. No exceptions.
    // Освобождает контекст разбора флагов. Исключений нет.
    ~Flag_Context() {
        flag_c_free(ctx_);
    }

    // Implicit conversion to `void *` so it can be passed to `flag_c_*`
    // functions directly. No exceptions.
    // Преобразуется в void * для вызова flag_c_*. Исключений нет.
    operator void *() const { return ctx_; }

  private:
    void *ctx_;
};

} // namespace cli
