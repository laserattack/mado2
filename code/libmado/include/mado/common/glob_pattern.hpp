#pragma once

#include <mado/common/glob.h>

#include <cstdlib>
#include <optional>
#include <string_view>

namespace mado::common {

class Glob_Pattern {
  public:
    /// Compiles a non-null glob pattern. An invalid pattern is retained as a
    /// failed compilation and causes `matches` to return `std::nullopt`.
    explicit Glob_Pattern(const char *pattern)
        : result_(glob_compile(pattern, &glob_)) {}

    /// Copying is disabled because the object owns the compiled command array.
    Glob_Pattern(const Glob_Pattern &) = delete;
    /// Copy assignment is disabled because the object owns the compiled command array.
    Glob_Pattern &operator=(const Glob_Pattern &) = delete;

    /// Releases storage allocated while compiling the pattern.
    ~Glob_Pattern() {
        std::free(glob_.items);
    }

    /// Returns whether `text` matches the compiled pattern, or `std::nullopt`
    /// if the pattern was invalid.
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
