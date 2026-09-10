#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace mado::query {

// Token_Type values are split into two ranges:
// - Keywords ([0, MAX_KEYWORD_VALUE]): tokens that look like identifiers but have
//   special meaning in the query syntax (e.g. "priority", "and", "all")
// - Non-keywords ((MAX_KEYWORD_VALUE, inf)): tokens that are operators, punctuation,
//   values, or system tokens (e.g. ">", "(", "42", End, Invalid)
constexpr uint16_t MAX_KEYWORD_VALUE = 1000;

enum class Token_Type {
    // keywords

    // fields
    Priority = 0, // priority
    Tag,          // tag
    Status,       // status
    Name,         // name
    Path,         // path
    Time,         // time
    Deadline,     // deadline
    Mtime,        // mtime
    Any,          // any

    // special expressions
    All,           // all
    Untagged,      // untagged
    Unstatused,    // unstatused
    Unnamed,       // unnamed
    Unprioritized, // unprioritized
    Undeadlined,   // undeadlined

    // sugar
    Allof, // allof
    Anyof, // anyof
    In,    // in
    Has,   // has

    // logical operators
    And, // and
    Or,  // or
    Xor, // xor
    Not, // not

    // not keywords

    // comparison operators
    Gt = MAX_KEYWORD_VALUE + 1, // >
    Lt,                         // <
    Ge,                         // >=
    Le,                         // <=
    Eq,                         // =
    Ne,                         // !=
    Substr,                     // ~
    Nsubstr,                    // !~
    Fuzzy,                      // ~~ or f~
    Nfuzzy,                     // !~~ or !f~
    Starts,                     // ^~
    Nstarts,                    // !^~
    Ends,                       // $~
    Nends,                      // !$~
    Glob,                       // %~ or g~
    Nglob,                      // !%~ or !g~

    // punctuation
    Lparen,   // (
    Rparen,   // )
    Comma,    // ,
    DotDot,   // ..
    Lbracket, // [
    Rbracket, // ]

    // values
    Number,    // 0-999
    String,    // [a-zA-Z_][a-zA-Z0-9_-]* or "..." or '...'
    Timestamp, // YYYYMMDDTHHMMSS with optional shorter forms: YYYY, YYYYMM, YYYYMMDD, YYYYMMDDT, YYYYMMDDTHH, YYYYMMDDTHHMM, YYYYMMDDTHHMMSS

    // the end of the character stream
    End,

    // invalid token. Must remain the last entry in the enum:
    // it is used as a loop termination condition in several places
    Invalid,
};

struct Token {
    Token_Type type{Token_Type::Invalid};
    std::string value;
    size_t position{0}; // character offset from the start of the
    // query string Length of the token in the query string. Needed
    // for error underlining: value may differ from the raw source
    // text (e.g. a quoted string strips quotes and processes
    // escapes), so value.size() cannot be used
    size_t length{0};

    Token() = default;

    Token(Token_Type t, std::string v, size_t pos, size_t len)
        : type(t), value(std::move(v)), position(pos), length(len) {}
};

std::string token_type_to_string(Token_Type type);

bool token_is_keyword(Token_Type type);

} // namespace mado::query
