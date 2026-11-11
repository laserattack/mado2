#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <mado/query/ast.hpp>
#include <mado/query/token.hpp>

namespace mado::query {

class Parse_Error : public std::runtime_error {
  public:
    /// Creates a parse error associated with the offending token.
    Parse_Error(const std::string &message, const Token &token)
        : std::runtime_error(message), token_(token) {}

    /// Returns the token associated with this error.
    const Token &token() const { return token_; }

    /// Formats the diagnostic with the query and a caret at the token position.
    std::string format(const std::string &query) const;

  private:
    Token token_;
};

class Parser {
  public:
    /// Creates a parser for a token stream. Reads past the supplied tokens as
    /// the End token.
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    /// Parses the token stream into an AST. On a Parse_Error after partial
    /// parsing, `partial_ast()` exposes the successfully parsed prefix.
    /// @throws Parse_Error If the query is empty or syntactically invalid.
    /// @throws std::invalid_argument If a Number token used in a comparison
    /// contains no convertible integer.
    /// @throws std::out_of_range If a Number token used in a comparison
    /// contains an integer outside the range of int.
    std::unique_ptr<Ast_Node> parse();

    /// Returns the partial AST from the most recent failed parse, or null if
    /// parsing has not failed after producing a partial tree.
    const Ast_Node *partial_ast() const { return partial_ast_.get(); }

  private:
    std::unique_ptr<Ast_Node> partial_ast_;
    std::vector<Token> tokens_; // Tokens from lexer
    size_t current_{0};         // Index of the current token in tokens_

    std::unique_ptr<Ast_Node> parse_expression(); // any expression
    std::unique_ptr<Ast_Node> parse_or();         // lowest priority
    std::unique_ptr<Ast_Node> parse_xor();        // priority > or
    std::unique_ptr<Ast_Node> parse_and();        // priority > xor
    std::unique_ptr<Ast_Node> parse_not();        // priority > and
    std::unique_ptr<Ast_Node> parse_primary();    // highest priority

    Ast_Comparison_Operator parse_comparison_operator();

    std::unique_ptr<Ast_Node> parse_field_and_special();
    std::unique_ptr<Ast_Node> parse_comparison(Ast_Comparison_Field field);
    std::unique_ptr<Ast_Node> parse_list(Ast_Comparison_Field field, Ast_Comparison_Operator op, bool is_allof);
    std::unique_ptr<Ast_Node> parse_range(Ast_Comparison_Field field);
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
