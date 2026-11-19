#include <mado/common/fuzzy_match.hpp>
#include <mado/common/text_utils.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/time_macro.hpp>

#include <algorithm>
#include <cassert>
#include <optional>

#include <utf8proc/utf8proc.h>

namespace mado::query {

namespace {

constexpr utf8proc_int32_t ASCII_LIMIT = 0x80;
constexpr utf8proc_int32_t NEXT_LINE = 0x85;

struct Unicode_Character {
    utf8proc_int32_t value;
    size_t size;
};

Unicode_Character read_character(const std::string &text, size_t position) {
    if (position >= text.size())
        return {0, 0};

    utf8proc_int32_t value = 0;
    const auto size = utf8proc_iterate(
        reinterpret_cast<const utf8proc_uint8_t *>(text.data() + position),
        static_cast<utf8proc_ssize_t>(text.size() - position), &value);
    if (size <= 0)
        return {0, 0};

    return {value, static_cast<size_t>(size)};
}

bool is_whitespace(utf8proc_int32_t value) {
    const auto category = utf8proc_category(value);
    return (value >= '\t' && value <= '\r') || value == NEXT_LINE ||
           category == UTF8PROC_CATEGORY_ZS || category == UTF8PROC_CATEGORY_ZL ||
           category == UTF8PROC_CATEGORY_ZP;
}

} // namespace

size_t Lexer::unquoted_value_character_byte_length() const {
    const auto character = read_character(query_, pos_);
    if (character.size == 0)
        return 0;

    if (character.value < ASCII_LIMIT) {
        const char value = static_cast<char>(character.value);
        return mado::common::is_identifier_char(value) ? character.size : 0;
    }

    // Unicode belongs to literal values, not to field or keyword identifiers.
    if (is_whitespace(character.value) ||
        utf8proc_category(character.value) == UTF8PROC_CATEGORY_CC)
        return 0;

    return character.size;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    std::optional<std::time_t> now;

    while (!is_at_end()) {
        skip_whitespace();

        if (is_at_end())
            break;

        char c = get_it();

        if (mado::common::is_digit(c)) {
            tokens.push_back(parse_number_and_timestamp());
        } else if (c == '@') {
            if (!now)
                now = std::time(nullptr);
            tokens.push_back(parse_time_macro(*now));
        } else if (c == '"' || c == '\'') {
            tokens.push_back(parse_quoted_string());
        } else if (unquoted_value_character_byte_length() != 0) {
            tokens.push_back(parse_unquoted_value());
        } else {
            tokens.push_back(parse_operator_and_punctuation());
        }
    }

    tokens.push_back({Token_Type::End, "", pos_});

    return tokens;
}

Token Lexer::parse_time_macro(std::time_t now) {
    const size_t start = pos_;
    eat_it();
    const size_t name_start = pos_;
    if (!mado::common::is_letter_or_underscore(get_it()))
        return make_token(Token_Type::Invalid, start);
    while (const size_t size = unquoted_value_character_byte_length())
        eat_it(size);
    const auto name = query_.substr(name_start, pos_ - name_start);
    if (!std::all_of(name.begin(), name.end(), mado::common::is_identifier_char))
        return make_token(Token_Type::Invalid, start);

    size_t sign_pos = 0;
    while (get_it(sign_pos) == ' ' || get_it(sign_pos) == '\t' || get_it(sign_pos) == '\r')
        ++sign_pos;
    const char sign = get_it(sign_pos);
    int offset = 0;
    if (sign == '+' || sign == '-') {
        eat_it(sign_pos + 1);
        while (get_it() == ' ' || get_it() == '\t' || get_it() == '\r')
            eat_it();

        const size_t digits_start = pos_;
        while (mado::common::is_digit(get_it())) {
            if (pos_ - digits_start < 6)
                offset = offset * 10 + (get_it() - '0');
            eat_it();
        }
        const size_t digits = pos_ - digits_start;
        if (digits == 0 || digits > 6)
            return make_token(Token_Type::Invalid, start);
        if (sign == '-')
            offset = -offset;
    }

    auto value = resolve_time_macro(name, offset, now);
    if (!value)
        return make_token(Token_Type::Invalid, start);
    return make_token(Token_Type::Timestamp, std::move(*value), start);
}

void Lexer::skip_whitespace() {
    while (!is_at_end()) {
        const auto character = read_character(query_, pos_);
        if (character.size == 0 || !is_whitespace(character.value))
            break;
        eat_it(character.size);
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
    return Token{type, query_.substr(start, pos_ - start), start, pos_ - start};
}

Token Lexer::make_token(Token_Type type, std::string value, size_t start) const {
    return Token{type, std::move(value), start, pos_ - start};
}

Token Lexer::parse_number_and_timestamp() {
    size_t start = pos_;

    assert(mado::common::is_digit(get_it()) &&
           "parse_number_and_timestamp called without digit");

    while (unquoted_value_character_byte_length() != 0 || get_it() == '-') {
        eat_it(get_it() == '-' ? 1 : unquoted_value_character_byte_length());
    }

    size_t length = pos_ - start;
    const std::string value = query_.substr(start, length);

    // 1-3 digits: number (0-999)
    if (length <= 3 && std::all_of(value.begin(), value.end(), mado::common::is_digit)) {
        return make_token(Token_Type::Number, start);
    }

    // 4+ digits: maybe timestamp (YYYY, YYYYMM, YYYYMMDD, YYYYMMDD-...)
    if (mado::common::is_timestamp(value)) {
        return make_token(Token_Type::Timestamp, start);
    }

    return make_token(Token_Type::String, start);
}

Token Lexer::parse_quoted_string() {
    size_t start = pos_;
    char quote = eat_it();

    assert((quote == '"' || quote == '\'') &&
           "parse_quoted_string called without quote");

    std::string result;

    while (!is_at_end() && get_it() != quote) {
        if (get_it() == '\\' && (get_it(1) == '\'' || get_it(1) == '"')) {
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

Token Lexer::parse_unquoted_value() {
    size_t start = pos_;

    assert(unquoted_value_character_byte_length() != 0 &&
           "parse_unquoted_value called without value character");

    while (unquoted_value_character_byte_length() != 0 || get_it() == '-') {
        eat_it(get_it() == '-' ? 1 : unquoted_value_character_byte_length());
    }

    std::string result = query_.substr(start, pos_ - start);

    // Only the original ASCII identifier syntax participates in keyword matching.
    const bool identifier = mado::common::is_letter_or_underscore(result.front()) &&
        std::all_of(result.begin(), result.end(), [](char value) {
            return mado::common::is_identifier_char(value) || value == '-';
        });
    if (!identifier)
        return make_token(Token_Type::String, std::move(result), start);

    static const auto KEYWORDS = [] {
        std::vector<std::pair<std::string, Token_Type>> keywords;
        for (int i = 0; i < static_cast<int>(Token_Type::Invalid); ++i) {
            const auto type = static_cast<Token_Type>(i);
            if (token_is_keyword(type))
                keywords.emplace_back(token_type_to_string(type), type);
        }
        return keywords;
    }();

    const auto found = mado::common::find_fuzzy_match(
        result, KEYWORDS, [](const auto &item) -> const std::string & { return item.first; });
    const auto type = found == KEYWORDS.end() ? Token_Type::String : found->second;
    return make_token(type, std::move(result), start);
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

    // Eat unknown character
    eat_it();

    // Don't include the byte in value: we can't tell if it's a valid character
    // in the current encoding. A single byte may be part of a multi-byte UTF-8
    // sequence, and slicing it would produce invalid text. So we only report
    // the error location
    return make_token(Token_Type::Invalid, "", start);
}

} // namespace mado::query
