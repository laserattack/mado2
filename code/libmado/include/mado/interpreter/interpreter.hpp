#pragma once

#include <mado/entry/entry.hpp>
#include <mado/query/ast.hpp>

namespace mado::interpreter {

class Interpreter {
  public:
    // Returns true if the entry matches the filter.
    // A nullptr filter matches every entry.
    bool evaluate(const mado::query::Ast_Node *filter,
                  const mado::entry::Entry &entry) const;
};

} // namespace mado::interpreter
