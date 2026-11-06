#pragma once

#include <mado/common/glob.h>

#include <cstdlib>
#include <optional>
#include <string_view>

namespace mado::common {

class Glob_Pattern {
  public:
    explicit Glob_Pattern(const char *pattern)
        : result_(glob_compile(pattern, &glob_)) {}

    Glob_Pattern(const Glob_Pattern &) = delete;
    Glob_Pattern &operator=(const Glob_Pattern &) = delete;

    ~Glob_Pattern() {
        std::free(glob_.items);
    }

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
