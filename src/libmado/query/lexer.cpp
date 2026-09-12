#include <cassert>
#include <ctime>
#include <optional>

#include "../../common/fuzzy_match.hpp"
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

        if (c == '@') {
            auto resolved = parse_macro();
            tokens.insert(tokens.end(),
                          std::make_move_iterator(resolved.begin()),
                          std::make_move_iterator(resolved.end()));
        } else if (mado::common::is_digit(c)) {
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

    // If exactly 8 digits are followed by '-', continue as timestamp
    if ((get_it() == '-' || get_it() == 'T') && pos_ - start == 8) {
        eat_it(); // -

        while (!is_at_end() && mado::common::is_digit(get_it())) {
            eat_it();
        }
    }

    size_t length = pos_ - start;

    // 1-3 digits: number (0-999)
    if (length <= 3) {
        return make_token(Token_Type::Number, start);
    }

    // 4+ digits: maybe timestamp (YYYY, YYYYMM, YYYYMMDD, YYYYMMDD-...)
    std::string value = query_.substr(start, length);
    if (mado::common::is_timestamp(value)) {
        return make_token(Token_Type::Timestamp, start);
    }

    return make_token(Token_Type::Invalid, start);
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

    assert(mado::common::is_letter_or_underscore(get_it()) &&
           "parse_identifier_and_keyword called without letter or underscore");

    while (!is_at_end() && mado::common::is_identifier_char(get_it())) {
        eat_it();
    }

    std::string result = query_.substr(start, pos_ - start);

    std::optional<int32_t> best_score;
    Token_Type best_type = Token_Type::Invalid;

    for (int i = 0;; i++) {
        Token_Type type = static_cast<Token_Type>(i);

        if (type == Token_Type::Invalid)
            break;

        if (!token_is_keyword(type))
            continue;

        std::string keyword = token_type_to_string(type);

        if (mado::common::equals_ignore_case(keyword, result)) {
            return make_token(type, std::move(result), start);
        }

        auto score = mado::common::fuzzy_match(result, keyword, true);

        if (score && (!best_score || *score > *best_score)) {
            best_score = *score;
            best_type = type;
        }
    }

    if (best_type != Token_Type::Invalid) {
        return make_token(best_type, std::move(result), start);
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

std::string Lexer::macro_type_to_string(Macro_Type type) {
    switch (type) {
    // time
    case Macro_Type::Today:
        return "Today";
    case Macro_Type::Now:
        return "Now";
    case Macro_Type::Yesterday:
        return "Yesterday";
    case Macro_Type::Tomorrow:
        return "Tomorrow";
    case Macro_Type::Week:
        return "Week";
    case Macro_Type::Month:
        return "Month";
    case Macro_Type::Year:
        return "Year";

    // special
    case Macro_Type::Invalid:
        return "Invalid";
    }

    return "Unknown";
}

std::vector<Token> Lexer::parse_macro() {
    size_t start = pos_;

    assert(get_it() == '@' &&
           "parse_macro called without '@'");

    eat_it(); // @

    if (!mado::common::is_letter_or_underscore(get_it())) {
        return {make_token(Token_Type::Invalid, start)};
    }

    std::string name;
    while (!is_at_end() && mado::common::is_identifier_char(get_it())) {
        name += eat_it();
    }

    std::vector<std::string> args;
    if (get_it() == '(') {

        eat_it(); // (

        while (true) {

            // @macro(    ) - without args
            while (!is_at_end() && std::isspace(get_it())) {
                eat_it();
            }
            if (get_it() == ')')
                break;

            std::string arg;
            while (!is_at_end() && get_it() != ',' && get_it() != ')') {
                char c = eat_it();
                if (std::isspace(c))
                    continue;
                arg += c;
            }
            args.push_back(std::move(arg));

            // arg,
            if (get_it() == ',') {
                eat_it();
                continue; // next arg
            }

            // arg) or argEND
            break;
        }

        if (get_it() != ')') {
            return {make_token(Token_Type::Invalid, start)}; // argEND
        }

        eat_it(); // arg)
    }

    return resolve_macro(name, args, start);
}

std::vector<Token> Lexer::resolve_macro(const std::string &name,
                                        const std::vector<std::string> &args,
                                        size_t start) const {

    std::optional<Macro_Type> type;
    std::optional<int32_t> best_score;

    // finding macro type
    for (int i = 0;; ++i) {
        Macro_Type mt = static_cast<Macro_Type>(i);

        if (mt == Macro_Type::Invalid)
            break;

        std::string macro_name = macro_type_to_string(mt);

        // Exact match
        if (mado::common::equals_ignore_case(name, macro_name)) {
            type = mt;
            break;
        }

        // Fuzzy fallback
        auto score = mado::common::fuzzy_match(name, macro_name, true);
        if (score && (!best_score || *score > *best_score)) {
            best_score = *score;
            type = mt;
        }
    }

    // not found :(
    if (!type) {
        return {make_token(Token_Type::Invalid, start)};
    }

    switch (*type) {
    // time macro
    case Macro_Type::Today:
    case Macro_Type::Now:
    case Macro_Type::Yesterday:
    case Macro_Type::Tomorrow:
    case Macro_Type::Week:
    case Macro_Type::Month:
    case Macro_Type::Year:
        return resolve_time_macro(*type, args, start);
    default:
        return {make_token(Token_Type::Invalid, start)};
    }
}

std::vector<Token> Lexer::resolve_time_macro(Macro_Type type,
                                             const std::vector<std::string> &args,
                                             size_t start) const {

    // Parse optional offset (at most one numeric argument)

    int offset = 0;

    if (args.size() > 1) {
        return {make_token(Token_Type::Invalid, start)};
    }

    if (args.size() == 1) {
        const std::string &arg = args[0];

        try {
            size_t pos = 0;
            offset = std::stoi(arg, &pos);
            if (pos != arg.size()) {
                // Trailing garbage after the number
                return {make_token(Token_Type::Invalid, start)};
            }
        } catch (...) {
            return {make_token(Token_Type::Invalid, start)};
        }
    }

    // Current local time

    time_t now = std::time(nullptr);        // get current time
    std::tm tm_buf = *std::localtime(&now); // and parse it

    // Apply offset depending on the macro type
    switch (type) {
    case Macro_Type::Now:
        tm_buf.tm_mday += offset;
        break;

    case Macro_Type::Today:
        tm_buf.tm_mday += offset;
        break;

    case Macro_Type::Yesterday:
        tm_buf.tm_mday += offset - 1;
        break;

    case Macro_Type::Tomorrow:
        tm_buf.tm_mday += offset + 1;
        break;

    case Macro_Type::Week: {
        // Move to Monday of the current week, then offset by weeks
        int days_since_monday = (tm_buf.tm_wday + 6) % 7;
        tm_buf.tm_mday -= days_since_monday;
        tm_buf.tm_mday += offset * 7;
        break;
    }

    case Macro_Type::Month:
        tm_buf.tm_mon += offset;
        break;

    case Macro_Type::Year:
        tm_buf.tm_year += offset;
        break;

    default:
        return {make_token(Token_Type::Invalid, start)};
    }

    // Normalize date (mktime handles overflow of days/months/years)
    tm_buf.tm_isdst = -1; // A negative value of time->tm_isdst causes
                          // mktime to attempt to determine if
                          // Daylight Saving Time was in effect
    if (std::mktime(&tm_buf) == -1) {
        // Time since epoch as a std::time_t object on success or -1
        // if time cannot be represented as a std::time_t object
        return {make_token(Token_Type::Invalid, start)};
    }

    // Format depending on the macro type
    char buf[16]; // YYYYMMDD-HHMMSS = 15 + \0
    const char *fmt;

    switch (type) {
    case Macro_Type::Now:
        fmt = "%Y%m%d-%H%M%S";
        break;
    case Macro_Type::Month:
        fmt = "%Y%m";
        break;
    case Macro_Type::Year:
        fmt = "%Y";
        break;
    default:
        fmt = "%Y%m%d";
        break;
    }

    std::strftime(buf, sizeof(buf), fmt, &tm_buf);

    // maybe invalid timestamp like YYYYYYMM.. etc
    if (!mado::common::is_timestamp(buf)) {
        return {make_token(Token_Type::Invalid, start)};
    }

    return {make_token(Token_Type::Timestamp, buf, start)};
}

} // namespace mado::query
