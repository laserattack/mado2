#include <cassert>
#include <unordered_map>

#include "lexer.hpp"

namespace mado::interpreter {

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!is_at_end()) {
        skip_whitespace();

        if (is_at_end())
            break;

        char c = get_it();

        if (is_digit(c)) {
            tokens.push_back(parse_number_and_timestamp());
        } else if (c == '"' || c == '\'') {
            tokens.push_back(parse_quoted_string());
        } else if (is_letter_or_underscore(c)) {
            tokens.push_back(parse_identifier_and_keyword());
        } else {
            tokens.push_back(parse_operator_and_punctuation());
        }
    }

    tokens.push_back({Token_Type::End, "", pos_});

    return tokens;
}

void Lexer::skip_whitespace() {
    while (!is_at_end() && std::isspace(get_it())) {
        eat_it();
    }
}

bool Lexer::is_at_end() const {
    return pos_ >= query_.size();
}

char Lexer::get_it(size_t n) const {
    if (pos_ + n >= query_.size())
        return '\0';
    return query_[pos_ + n];
}

char Lexer::eat_it() {
    if (is_at_end())
        return '\0';
    return query_[pos_++];
}

void Lexer::eat_it(size_t n) {
    pos_ = std::min(pos_ + n, query_.size());
}

Token Lexer::make_token(Token_Type type, size_t start) const {
    return Token{type, query_.substr(start, pos_ - start), start};
}

Token Lexer::make_token(Token_Type type, std::string value, size_t start) const {
    return Token{type, std::move(value), start};
}

bool Lexer::is_identifier_char(char c) {
    return is_digit(c) || is_letter_or_underscore(c);
}

bool Lexer::is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::is_letter_or_underscore(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

Token Lexer::parse_number_and_timestamp() {
    size_t start = pos_;

    assert(is_digit(get_it()) && "parse_number_and_timestamp called without digit");

    while (!is_at_end() && is_digit(get_it())) {
        eat_it();
    }

    size_t length = pos_ - start;

    // Number must be 1-3 digits (0-999)
    if (length > 3) {
        assert(false && "TODO: implement timestamp parsing");
    }

    return make_token(Token_Type::Number, start);
}

Token Lexer::parse_quoted_string() {
    size_t start = pos_;
    char quote = eat_it();

    assert((quote == '"' || quote == '\'') &&
           "parse_quoted_string called without quote");

    std::string result;

    while (!is_at_end() && get_it() != quote) {
        if (get_it() == '\\' && get_it(1) != '\0') {
            eat_it();
            result += eat_it();
        } else {
            result += eat_it();
        }
    }

    if (is_at_end()) {
        // Unterminated string
        return make_token(Token_Type::Invalid, std::move(result), start);
    }

    // Consume the closing quote
    eat_it();

    return make_token(Token_Type::String, std::move(result), start);
}

Token Lexer::parse_identifier_and_keyword() {
    size_t start = pos_;

    assert(is_letter_or_underscore(get_it()) &&
           "parse_identifier_and_keyword called without letter or underscore");

    while (!is_at_end() && is_identifier_char(get_it())) {
        eat_it();
    }

    std::string result = query_.substr(start, pos_ - start);

    static const std::unordered_map<std::string, Token_Type> keywords = {
        {"priority", Token_Type::Priority},
        {"tag", Token_Type::Tag},
        {"status", Token_Type::Status},
        {"name", Token_Type::Name},
        {"path", Token_Type::Path},
        {"time", Token_Type::Time},
        {"deadline", Token_Type::Deadline},
        {"mtime", Token_Type::Mtime},
        {"any", Token_Type::Any},
        {"all", Token_Type::All},
        {"untagged", Token_Type::Untagged},
        {"unstatused", Token_Type::Unstatused},
        {"unnamed", Token_Type::Unnamed},
        {"unprioritized", Token_Type::Unprioritized},
        {"undeadlined", Token_Type::Undeadlined},
        {"and", Token_Type::And},
        {"or", Token_Type::Or},
        {"xor", Token_Type::Xor},
        {"not", Token_Type::Not},
        {"allof", Token_Type::Allof},
        {"anyof", Token_Type::Anyof},
        {"in", Token_Type::In},
        {"has", Token_Type::Has},
    };

    // TODO: fuzzy and ignore case
    auto it = keywords.find(result);
    if (it != keywords.end()) {
        return make_token(it->second, std::move(result), start);
    }

    return make_token(Token_Type::String, std::move(result), start);
}

Token Lexer::parse_operator_and_punctuation() {
    size_t start = pos_;

    // >=
    if (get_it() == '>' && get_it(1) == '=') {
        eat_it(2);
        return make_token(Token_Type::Ge, start);
    }

    // >
    if (get_it() == '>') {
        eat_it();
        return make_token(Token_Type::Gt, start);
    }

    // <=
    if (get_it() == '<' && get_it(1) == '=') {
        eat_it(2);
        return make_token(Token_Type::Le, start);
    }

    // <
    if (get_it() == '<') {
        eat_it();
        return make_token(Token_Type::Lt, start);
    }

    // =
    if (get_it() == '=') {
        eat_it();
        return make_token(Token_Type::Eq, start);
    }

    // ~~
    if (get_it() == '~' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Fuzzy, start);
    }

    // ~
    if (get_it() == '~') {
        eat_it();
        return make_token(Token_Type::Substr, start);
    }

    // ^~
    if (get_it() == '^' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Starts, start);
    }

    // $~
    if (get_it() == '$' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Ends, start);
    }

    // %~
    if (get_it() == '%' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Glob, start);
    }

    // f~
    if (get_it() == 'f' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Fuzzy, start);
    }

    // g~
    if (get_it() == 'g' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Glob, start);
    }

    // (
    if (get_it() == '(') {
        eat_it();
        return make_token(Token_Type::Lparen, start);
    }

    // )
    if (get_it() == ')') {
        eat_it();
        return make_token(Token_Type::Rparen, start);
    }

    // ,
    if (get_it() == ',') {
        eat_it();
        return make_token(Token_Type::Comma, start);
    }

    // [
    if (get_it() == '[') {
        eat_it();
        return make_token(Token_Type::Lbracket, start);
    }

    // ]
    if (get_it() == ']') {
        eat_it();
        return make_token(Token_Type::Rbracket, start);
    }

    // ..
    if (get_it() == '.' && get_it(1) == '.') {
        eat_it(2);
        return make_token(Token_Type::DotDot, start);
    }

    // !=
    if (get_it() == '!' && get_it(1) == '=') {
        eat_it(2);
        return make_token(Token_Type::Ne, start);
    }

    // !~~
    if (get_it() == '!' && get_it(1) == '~' && get_it(2) == '~') {
        eat_it(3);
        return make_token(Token_Type::Nfuzzy, start);
    }

    // !~
    if (get_it() == '!' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Nsubstr, start);
    }

    // !%~
    if (get_it() == '!' && get_it(1) == '%' && get_it(2) == '~') {
        eat_it(3);
        return make_token(Token_Type::Nglob, start);
    }

    // !^~
    if (get_it() == '!' && get_it(1) == '^' && get_it(2) == '~') {
        eat_it(3);
        return make_token(Token_Type::Nstarts, start);
    }

    // !$~
    if (get_it() == '!' && get_it(1) == '$' && get_it(2) == '~') {
        eat_it(3);
        return make_token(Token_Type::Nends, start);
    }

    // !f~
    if (get_it() == '!' && get_it(1) == 'f' && get_it(2) == '~') {
        eat_it(3);
        return make_token(Token_Type::Nfuzzy, start);
    }

    // !g~
    if (get_it() == '!' && get_it(1) == 'g' && get_it(2) == '~') {
        eat_it(3);
        return make_token(Token_Type::Nglob, start);
    }

    // Unknown character
    char c = eat_it();
    return make_token(Token_Type::Invalid, std::string(1, c), start);
}

} // namespace mado::interpreter
