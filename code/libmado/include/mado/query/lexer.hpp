#pragma once

#include <ctime>
#include <string>
#include <vector>

#include <mado/query/token.hpp>

namespace mado::query {

class Lexer {
  public:
    /// Creates a lexer that reads tokens from `query`.
    explicit Lexer(std::string query) : query_(std::move(query)) {}

    /// Converts the query into tokens and appends a Token_Type::End token.
    /// Invalid syntax is represented by Invalid tokens for the parser to report.
    /// @throws std::runtime_error If a time macro needs the current time
    /// and that time cannot be converted to UTC.
    std::vector<Token> tokenize();

  private:
    std::string query_; // the source query string
    size_t pos_{0};     // current position in the string

    // Skips whitespace characters
    void skip_whitespace();

    // Whether the end of the string has been reached
    bool is_at_end() const;

    // Returns the character at offset n from the current position
    // without advancing. get_it() returns the current character.
    // Returns '\0' if the position is past the end
    char get_it(size_t n = 0) const;

    // Returns the current character and advances forward.
    // Returns '\0' if at the end
    char eat_it();

    // Advances forward by n characters.
    void eat_it(size_t n);

    // Creates a token of the given type from the substring [start, pos_)
    Token make_token(Token_Type type, size_t start) const;

    // Creates a token with explicit value
    Token make_token(Token_Type type, std::string value, size_t start) const;

    Token parse_number_and_timestamp();     // Parses numbers and timestamps

    Token parse_time_macro(std::time_t now);
    Token parse_quoted_string();            // Parses a quoted string (single or double quotes)
    Token parse_identifier_and_keyword();   // Parses identifiers and a keywords
    Token parse_operator_and_punctuation(); // Parses operators and punctuation
};

} // namespace mado::query
