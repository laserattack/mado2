#pragma once

#include <mado/common/glob.h>

#include <cstdlib>
#include <optional>
#include <string_view>

namespace mado::common {

// Owns compiled glob commands and retains syntax errors for matching.
class Glob_Pattern {
  public:
    // Owns compiled glob commands and retains syntax errors for matching.
    // Compiles a non-null pattern. Syntax errors are retained and
    // allocation failure is handled by the C glob compiler.
    explicit Glob_Pattern(const char *pattern)
        : result_(glob_compile(pattern, &glob_)) {}

    Glob_Pattern(const Glob_Pattern &) = delete;
    Glob_Pattern &operator=(const Glob_Pattern &) = delete;

    // Releases compiled commands, including partially compiled patterns.
    // No exceptions.
    ~Glob_Pattern() {
        std::free(glob_.items);
    }

    // Returns the match result or nullopt for a malformed glob. Requires a
    // valid text view. No exceptions.
    std::optional<bool> matches(std::string_view text) const {
        if (result_.error)
            return std::nullopt;
        return glob_match(text.data(), text.size(), glob_.items, glob_.count);
    }

  private:
    Glob glob_{};
    Glob_Result result_;
};

} // namespace mado::common
