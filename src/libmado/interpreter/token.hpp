#pragma once

#include <cstddef>
#include <string>

namespace mado::interpreter {

enum class Token_Type {
    // fields
    Priority, // priority
    Tag,      // tag
    Status,   // status
    Name,     // name
    Path,     // path
    Time,     // time
    Deadline, // deadline
    Mtime,    // mtime
    Any,      // any

    // values
    Number,    // 0-999
    String,    // [a-zA-Z_][a-zA-Z0-9_-]* or "..." or '...'
    Timestamp, // YYYYMMDDTHHMMSS with optional shorter forms: YYYY, YYYYMM, YYYYMMDD, YYYYMMDDT, YYYYMMDDTHH, YYYYMMDDTHHMM, YYYYMMDDTHHMMSS

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

    // comparison operators
    Gt,      // >
    Lt,      // <
    Ge,      // >=
    Le,      // <=
    Eq,      // =
    Ne,      // !=
    Substr,  // ~
    Nsubstr, // !~
    Fuzzy,   // ~~ or f~
    Nfuzzy,  // !~~ or !f~
    Starts,  // ^~
    Nstarts, // !^~
    Ends,    // $~
    Nends,   // !$~
    Glob,    // %~ or g~
    Nglob,   // !%~ or !g~

    // punctuation
    Lparen,   // (
    Rparen,   // )
    Comma,    // ,
    DotDot,   // ..
    Lbracket, // [
    Rbracket, // ]

    // system
    End,     // the end of the character stream
    Invalid, // invalid token
};

struct Token {
    Token_Type type{Token_Type::Invalid};
    std::string value;
    size_t position{0}; // The position of the token’s start in the character stream

    Token() = default;

    Token(Token_Type t, std::string v, size_t pos)
        : type(t), value(std::move(v)), position(pos) {}
};

std::string
to_string(Token_Type type);

} // namespace mado::interpreter
