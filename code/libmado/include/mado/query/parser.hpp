#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <mado/query/ast.hpp>
#include <mado/query/token.hpp>

namespace mado::query {

// Carries a query syntax error and its offending token.
class Parse_Error : public std::runtime_error {
  public:
    // Stores the diagnostic and offending token. May throw std::bad_alloc.
    Parse_Error(const std::string &message, const Token &token)
        : std::runtime_error(message), token_(token) {}

    // Returns the offending token. No exceptions.
    const Token &token() const { return token_; }

    // Formats the error with the query and a caret at the error position
    std::string format(const std::string &query) const;

  private:
    Token token_;
};

// Converts a lexer token stream to a query AST.
class Parser {
  public:
    // The vector always contains at least one End token; the lexer guarantees this
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    // Builds the complete query AST. Throws Parse_Error for empty or invalid
    // queries and std::bad_alloc on allocation failure.
    std::unique_ptr<Ast_Node> parse(); // parse AST

  private:
    std::vector<Token> tokens_; // Tokens from lexer
    size_t current_{0};         // Index of the current token in tokens_

    // Parses a logical expression starting at the lowest precedence.
    // Parse_Error and allocation failures propagate.
    std::unique_ptr<Ast_Node> parse_expression(); // any expression
    // Combines OR operands. Parse_Error and allocation failures propagate.
    std::unique_ptr<Ast_Node> parse_or();         // lowest priority
    // Combines XOR operands. Parse_Error and allocation failures propagate.
    std::unique_ptr<Ast_Node> parse_xor();        // priority > or
    // Combines AND operands. Parse_Error and allocation failures propagate.
    std::unique_ptr<Ast_Node> parse_and();        // priority > xor
    // Parses a NOT expression or its next higher-precedence operand.
    // Parse_Error and allocation failures propagate.
    std::unique_ptr<Ast_Node> parse_not();        // priority > and
    // Parses a grouped expression or a field or keyword. Throws Parse_Error
    // for invalid syntax.
    std::unique_ptr<Ast_Node> parse_primary();    // highest priority

    // Consumes and maps a comparison operator. Throws Parse_Error for other
    // tokens.
    Ast_Comparison_Operator parse_comparison_operator();

    // Parses a field comparison or special keyword. Throws Parse_Error for an
    // unexpected token.
    std::unique_ptr<Ast_Node> parse_field_and_special();
    // Parses a comparison, list, or range. Throws Parse_Error for invalid
    // syntax or types.
    std::unique_ptr<Ast_Node> parse_comparison(Ast_Comparison_Field field);
    // Expands a nonempty value list into AND or OR nodes. Throws Parse_Error
    // for invalid values or delimiters.
    std::unique_ptr<Ast_Node> parse_list(Ast_Comparison_Field field, Ast_Comparison_Operator op, bool is_allof);
    // Expands inclusive range bounds into comparisons. Throws Parse_Error for
    // malformed or empty ranges.
    std::unique_ptr<Ast_Node> parse_range(Ast_Comparison_Field field);
    // Consumes a value and builds a comparison node. Throws Parse_Error for an
    // unknown field.
    std::unique_ptr<Ast_Node> parse_value(Ast_Comparison_Field field, Ast_Comparison_Operator op);

    // Whether the token can be a value for the given field
    bool value_matches_field(Ast_Comparison_Field field, const Token &val) const;

    // Human-readable description of the value type expected for the field
    std::string value_type_desc(Ast_Comparison_Field field) const;

    // Whether the end of the token stream has been reached
    bool is_at_end() const;

    // Returns the token at offset n from the current position
    // without advancing. get_it(0) returns the current token.
    // Returns End token if the position is past the end of the stream
    Token get_it(size_t n = 0) const;

    // Returns the current token and advances forward.
    // Returns End token if at the end of the stream
    Token eat_it();
};

} // namespace mado::query
