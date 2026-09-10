#include <cassert>

#include "../../common/text_utils.hpp"
#include "lexer.hpp"

namespace mado::query {

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!is_at_end()) {
        skip_whitespace();

        if (is_at_end())
            break;

        char c = get_it();

        if (mado::common::is_digit(c)) {
            tokens.push_back(parse_number_and_timestamp());
        } else if (c == '"' || c == '\'') {
            tokens.push_back(parse_quoted_string());
        } else if (mado::common::is_letter_or_underscore(c)) {
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

Token Lexer::parse_number_and_timestamp() {
    size_t start = pos_;

    assert(mado::common::is_digit(get_it()) &&
           "parse_number_and_timestamp called without digit");

    while (!is_at_end() && mado::common::is_digit(get_it())) {
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
    char quote = eat_it();

    assert((quote == '"' || quote == '\'') &&
           "parse_quoted_string called without quote");

    size_t start = pos_;
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

    assert(mado::common::is_letter_or_underscore(get_it()) &&
           "parse_identifier_and_keyword called without letter or underscore");

    while (!is_at_end() && mado::common::is_identifier_char(get_it())) {
        eat_it();
    }

    std::string result = query_.substr(start, pos_ - start);

    for (int i = 0;; i++) {
        Token_Type type = static_cast<Token_Type>(i);

        // last Token_Type enum field
        if (type == Token_Type::Invalid)
            break;

        if (!token_is_keyword(type))
            continue;

        std::string keyword = token_type_to_string(type);

        // TODO: fuzzy match
        if (mado::common::equals_ignore_case(keyword, result)) {
            return make_token(type, std::move(result), start);
        }
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

} // namespace mado::query
