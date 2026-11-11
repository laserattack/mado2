#pragma once

#include <mado/entry/entry.hpp>
#include <mado/query/ast.hpp>

namespace mado::interpreter {

class Interpreter {
  public:
    /// Evaluates a query AST against an entry. A null filter matches every
    /// entry. Logical nodes use AND, OR, XOR, and NOT; special nodes test for
    /// empty name/status/tags, zero priority, or the never deadline. Field
    /// comparisons use numeric ordering for priority and string operators for
    /// text fields; string comparisons are case-insensitive. Glob syntax errors
    /// produce a non-match (or a match for a negated glob operator).
    /// @throws std::bad_variant_access If an Any comparison has neither
    /// a numeric nor a string value.
    /// @throws std::runtime_error If the entry path cannot be converted to
    /// a string for a Path or Any comparison.
    bool evaluate(const mado::query::Ast_Node *filter,
                  const mado::entry::Entry &entry) const;
};

} // namespace mado::interpreter
