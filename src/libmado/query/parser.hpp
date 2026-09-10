#pragma once

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "ast.hpp"
#include "token.hpp"

namespace mado::query {

class Parse_Error : public std::runtime_error {
  public:
    Parse_Error(const std::string &message, const Token &token)
        : std::runtime_error(message), token_(token) {}

    const Token &token() const { return token_; }

    // Prints the error with the query and carets under the error token
    void print(const std::string &query, std::ostream &os = std::cerr) const;

  private:
    Token token_;
};

class Parser {
  public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    std::unique_ptr<Ast_Node> parse(); // parse AST

  private:
    std::vector<Token> tokens_; // Tokens from lexer
    size_t current_{0};         // Index of the current token in tokens_

    std::unique_ptr<Ast_Node> parse_expression(); // any expression
    std::unique_ptr<Ast_Node> parse_or();         // lowest priority
    std::unique_ptr<Ast_Node> parse_xor();        // priority > or
    std::unique_ptr<Ast_Node> parse_and();        // priority > xor
    std::unique_ptr<Ast_Node> parse_not();        // priority > and
    std::unique_ptr<Ast_Node> parse_primary();    // highest priority

    Ast_Comparison_Operator parse_comparison_operator();

    // Determines which parse_*_condition to call based on the current field token
    std::unique_ptr<Ast_Node> parse_condition();
    // field op number_value
    std::unique_ptr<Ast_Node> parse_number_condition(Ast_Comparison_Field field);
    // field op string_value
    std::unique_ptr<Ast_Node> parse_string_condition(Ast_Comparison_Field field);
    // field op timestamp_value
    std::unique_ptr<Ast_Node> parse_time_condition(Ast_Comparison_Field field);
    // any op number_value/string_value/timestamp
    std::unique_ptr<Ast_Node> parse_any_condition();

    // Whether the end of the token stream has been reached
    bool is_at_end() const;

    // Returns the token at offset n from the current position
    // without advancing. get_it(0) returns the current token.
    // Returns End token if the position is past the end of the stream
    Token get_it(size_t n = 0) const;

    // Returns the current token and advances forward.
    // Returns End token if at the end of the stream
    Token eat_it();

    // Advances forward by n tokens.
    // Does nothing if n would move past the end
    void eat_it(size_t n);

    // Returns a debug representation of the token: Type(value)
    // e.g. And(and), Number(42)
    std::string token_repr(const Token &token) const;
};

} // namespace mado::query
