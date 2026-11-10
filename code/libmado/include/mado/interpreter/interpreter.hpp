#pragma once

#include <mado/entry/entry.hpp>
#include <mado/query/ast.hpp>

namespace mado::interpreter {

class Interpreter {
  public:
    /// Returns true if the entry matches the filter.
    /// A nullptr filter matches every entry.
    /// @throws std::bad_variant_access If an Any comparison has neither
    /// a numeric nor a string value.
    /// @throws std::runtime_error If the entry path cannot be converted to
    /// a string for a Path or Any comparison.
    bool evaluate(const mado::query::Ast_Node *filter,
                  const mado::entry::Entry &entry) const;
};

} // namespace mado::interpreter
