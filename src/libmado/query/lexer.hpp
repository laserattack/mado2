#pragma once

#include <string>
#include <vector>

#include "token.hpp"

namespace mado::query {

class Lexer {
  public:
    // Creates a lexer for the given query string
    explicit Lexer(std::string query) : query_(std::move(query)) {}

    // Tokenizes the query string
    // Always appends a Token_Type::End token at the end
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
    // Does nothing if n would move past the end
    void eat_it(size_t n);

    // Creates a token of the given type from the substring [start, pos_)
    Token make_token(Token_Type type, size_t start) const;

    // Creates a token with explicit value
    Token make_token(Token_Type type, std::string value, size_t start) const;

    Token parse_number_and_timestamp();     // Parses numbers and timestamps
    Token parse_quoted_string();            // Parses a quoted string (single or double quotes)
    Token parse_identifier_and_keyword();   // Parses identifiers and a keywords
    Token parse_operator_and_punctuation(); // Parses operators and punctuation

    enum class Macro_Type {
        // time macros
        Today,     // @today, @today(-1), @today(5)...
        Now,       // @now, @now(-1), @now(5)...
        Yesterday, // @yesterday, @yesterday(-1), @yesterday(5)...
        Tomorrow,  // @tomorrow, @tomorrow(-1), @tomorrow(5)...
        Week,      // @week, @week(-1), @week(5)...
        Month,     // @month, @month(-1), @month(5)...
        Year,      // @year, @year(-1), @year(5)...

        // number macros
        Max,
        Min,

        // Invalid must be the last entry in the enum:
        // resolve_macro iterates over Macro_Type values until it reaches Invalid
        Invalid,
    };

    static std::string macro_type_to_string(Macro_Type type);

    // Parses a macro starting with '@' and expands it into one or more tokens
    std::vector<Token> parse_macro();

    std::vector<Token> resolve_macro(const std::string &name,
                                     const std::vector<std::string> &args,
                                     size_t start) const;

    std::vector<Token> resolve_time_macro(Macro_Type type,
                                          const std::vector<std::string> &args,
                                          size_t start) const;
};

} // namespace mado::query
