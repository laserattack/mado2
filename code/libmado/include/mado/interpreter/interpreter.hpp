#pragma once

#include <mado/entry/entry.hpp>
#include <mado/query/ast.hpp>

namespace mado::interpreter {

// Evaluates query ASTs against individual entries.
// Вычисляет AST запроса для отдельных записей.
class Interpreter {
  public:
    // Returns true if the entry matches the filter.
    // A nullptr filter matches every entry. May throw std::bad_alloc.
    // Возвращает true для подходящей записи. Нулевой фильтр пропускает всё. Возможен
    // std::bad_alloc.
    bool evaluate(const mado::query::Ast_Node *filter,
                  const mado::entry::Entry &entry) const;
};

} // namespace mado::interpreter
