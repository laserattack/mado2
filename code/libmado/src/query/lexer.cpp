#include <mado/common/fuzzy_match.hpp>
#include <mado/common/text_utils.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/time_macro.hpp>

#include <cassert>
#include <cctype>
#include <optional>

namespace mado::query {

// Scans the query and appends End. Malformed lexemes produce Invalid tokens.
// May throw std::bad_alloc.
// Разбирает запрос и добавляет End. Некорректные лексемы дают Invalid. Возможен
// std::bad_alloc.
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
            // All macros in the query share one reference time (UTC).
            // Все макросы запроса используют одно исходное время (UTC).
            if (!now)
                now = std::time(nullptr);
            tokens.push_back(parse_time_macro(*now));
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

// Scans @name and an optional one-to-six-digit signed offset. Returns
// Invalid for malformed input or unresolved macros. Resolver exceptions propagate.
// Читает @имя и необязательное смещение из одной–шести цифр со знаком.
// Неверный или неизвестный макрос даёт Invalid. Исключения передаются дальше.
Token Lexer::parse_time_macro(std::time_t now) {
    const size_t start = pos_;
    eat_it();
    const size_t name_start = pos_;
    if (!mado::common::is_letter_or_underscore(get_it()))
        return make_token(Token_Type::Invalid, start);
    while (mado::common::is_identifier_char(get_it()))
        eat_it();
    const auto name = query_.substr(name_start, pos_ - name_start);

    // Look past spacing without consuming it when there is no offset.
    // Смотрим за пробелы, сохраняя их при отсутствии смещения.
    size_t sign_pos = 0;
    while (get_it(sign_pos) == ' ' || get_it(sign_pos) == '\t' || get_it(sign_pos) == '\r')
        ++sign_pos;
    const char sign = get_it(sign_pos);
    int offset = 0;
    if (sign == '+' || sign == '-') {
        eat_it(sign_pos + 1);
        while (get_it() == ' ' || get_it() == '\t' || get_it() == '\r')
            eat_it();

        // Consume an overlong offset without overflowing the accumulator.
        // Считываем слишком длинное смещение без переполнения накопителя.
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

// Advances past whitespace. No exceptions.
// Пропускает пробельные символы. Исключений нет.
void Lexer::skip_whitespace() {
    while (!is_at_end() && std::isspace(get_it())) {
        eat_it();
    }
}

// Reports whether the input has been exhausted. No exceptions.
// Проверяет, закончился ли ввод. Исключений нет.
bool Lexer::is_at_end() const {
    return pos_ >= query_.size();
}

// Peeks ahead without consuming input. Returns '\0' past the end. No exceptions.
// Смотрит вперёд без продвижения. За концом возвращает нулевой символ. Исключений нет.
char Lexer::get_it(size_t n) const {
    if (pos_ + n >= query_.size())
        return '\0';
    return query_[pos_ + n];
}

// Consumes one character or returns '\0' at the end. No exceptions.
// Читает один символ или возвращает нулевой символ в конце. Исключений нет.
char Lexer::eat_it() {
    if (is_at_end())
        return '\0';
    return query_[pos_++];
}

// Advances by up to n characters, stopping at the end. No exceptions.
// Продвигается не более чем на n символов, останавливаясь на конце. Исключений нет.
void Lexer::eat_it(size_t n) {
    pos_ = std::min(pos_ + n, query_.size());
}

// Builds a token with the given kind, text, and starting offset. May throw
// std::bad_alloc.
// Создаёт токен с типом, текстом и начальным смещением. Возможен std::bad_alloc.
Token Lexer::make_token(Token_Type type, size_t start) const {
    return Token{type, query_.substr(start, pos_ - start), start};
}

// Builds a token with the given kind, text, and starting offset. May throw
// std::bad_alloc.
// Создаёт токен с типом, текстом и начальным смещением. Возможен std::bad_alloc.
Token Lexer::make_token(Token_Type type, std::string value, size_t start) const {
    return Token{type, std::move(value), start};
}

// Scans a number or timestamp and emits Invalid for malformed values. May
// throw std::bad_alloc.
// Разбирает число или метку времени. Некорректное значение даёт Invalid. Возможен
// std::bad_alloc.
Token Lexer::parse_number_and_timestamp() {
    size_t start = pos_;

    assert(mado::common::is_digit(get_it()) &&
           "parse_number_and_timestamp called without digit");

    while (!is_at_end() && mado::common::is_digit(get_it())) {
        eat_it();
    }

    // If exactly 8 digits are followed by '-', continue as timestamp
    // Если после восьми цифр идёт дефис, продолжаем читать метку времени.
    if (get_it() == '-' && pos_ - start == 8) {
        eat_it(); // - / Распознаём оператор или разделитель `-`.

        while (!is_at_end() && mado::common::is_digit(get_it())) {
            eat_it();
        }
    }

    size_t length = pos_ - start;

    // 1-3 digits: number (0-999)
    // Одна–три цифры обозначают число от 0 до 999.
    if (length <= 3) {
        return make_token(Token_Type::Number, start);
    }

    // 4+ digits: maybe timestamp (YYYY, YYYYMM, YYYYMMDD, YYYYMMDD-...)
    // Четыре цифры и более могут образовать метку времени.
    std::string value = query_.substr(start, length);
    if (mado::common::is_timestamp(value)) {
        return make_token(Token_Type::Timestamp, start);
    }

    return make_token(Token_Type::Invalid, start);
}

// Scans quoted text and emits Invalid when the quote is unterminated. May
// throw std::bad_alloc.
// Читает строку в кавычках. Незакрытая строка даёт Invalid. Возможен std::bad_alloc.
Token Lexer::parse_quoted_string() {
    size_t start = pos_;
    char quote = eat_it();

    assert((quote == '"' || quote == '\'') &&
           "parse_quoted_string called without quote");

    std::string result;

    // Consume the quoted value without treating escaped quotes as delimiters.
    // Читаем строку, не считая экранированные кавычки разделителями.
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
        // Незакрытая строка.
        return make_token(Token_Type::Invalid, std::move(result), start);
    }

    // Consume the closing quote
    // Пропускаем закрывающую кавычку.
    eat_it();

    return make_token(Token_Type::String, std::move(result), start);
}

// Scans an identifier and prefers exact keyword matches over fuzzy matches.
// May throw std::bad_alloc.
// Разбирает идентификатор и предпочитает точное совпадение ключевого слова нечёткому. Возможен
// std::bad_alloc.
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

    // Search keywords in token order and retain the best fuzzy match.
    // Проверяем ключевые слова по порядку токенов и сохраняем лучшее нечёткое совпадение.
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

// Scans an operator or punctuation, emitting Invalid for unknown input. May
// throw std::bad_alloc.
// Разбирает оператор или знак пунктуации. Неизвестный ввод даёт Invalid. Возможен
// std::bad_alloc.
Token Lexer::parse_operator_and_punctuation() {
    size_t start = pos_;

    // >=
    // Распознаём оператор или разделитель `>=`.
    if (get_it() == '>' && get_it(1) == '=') {
        eat_it(2);
        return make_token(Token_Type::Ge, start);
    }

    // >
    // Распознаём оператор или разделитель `>`.
    if (get_it() == '>') {
        eat_it();
        return make_token(Token_Type::Gt, start);
    }

    // <=
    // Распознаём оператор или разделитель `<=`.
    if (get_it() == '<' && get_it(1) == '=') {
        eat_it(2);
        return make_token(Token_Type::Le, start);
    }

    // <
    // Распознаём оператор или разделитель `<`.
    if (get_it() == '<') {
        eat_it();
        return make_token(Token_Type::Lt, start);
    }

    // =
    // Распознаём оператор или разделитель `=`.
    if (get_it() == '=') {
        eat_it();
        return make_token(Token_Type::Eq, start);
    }

    // ~~
    // Распознаём оператор или разделитель `~~`.
    if (get_it() == '~' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Fuzzy, start);
    }

    // ~
    // Распознаём оператор или разделитель `~`.
    if (get_it() == '~') {
        eat_it();
        return make_token(Token_Type::Substr, start);
    }

    // ^~
    // Распознаём оператор или разделитель `^~`.
    if (get_it() == '^' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Starts, start);
    }

    // $~
    // Распознаём оператор или разделитель `$~`.
    if (get_it() == '$' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Ends, start);
    }

    // %~
    // Распознаём оператор или разделитель `%~`.
    if (get_it() == '%' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Glob, start);
    }

    // (
    // Распознаём оператор или разделитель `(`.
    if (get_it() == '(') {
        eat_it();
        return make_token(Token_Type::Lparen, start);
    }

    // )
    // Распознаём оператор или разделитель `)`.
    if (get_it() == ')') {
        eat_it();
        return make_token(Token_Type::Rparen, start);
    }

    // ,
    // Распознаём оператор или разделитель `,`.
    if (get_it() == ',') {
        eat_it();
        return make_token(Token_Type::Comma, start);
    }

    // [
    // Распознаём оператор или разделитель `[`.
    if (get_it() == '[') {
        eat_it();
        return make_token(Token_Type::Lbracket, start);
    }

    // ]
    // Распознаём оператор или разделитель `]`.
    if (get_it() == ']') {
        eat_it();
        return make_token(Token_Type::Rbracket, start);
    }

    // ..
    // Распознаём оператор или разделитель `..`.
    if (get_it() == '.' && get_it(1) == '.') {
        eat_it(2);
        return make_token(Token_Type::DotDot, start);
    }

    // !=
    // Распознаём оператор или разделитель `!=`.
    if (get_it() == '!' && get_it(1) == '=') {
        eat_it(2);
        return make_token(Token_Type::Ne, start);
    }

    // !~~
    // Распознаём оператор или разделитель `!~~`.
    if (get_it() == '!' && get_it(1) == '~' && get_it(2) == '~') {
        eat_it(3);
        return make_token(Token_Type::Nfuzzy, start);
    }

    // !~
    // Распознаём оператор или разделитель `!~`.
    if (get_it() == '!' && get_it(1) == '~') {
        eat_it(2);
        return make_token(Token_Type::Nsubstr, start);
    }

    // !%~
    // Распознаём оператор или разделитель `!%~`.
    if (get_it() == '!' && get_it(1) == '%' && get_it(2) == '~') {
        eat_it(3);
        return make_token(Token_Type::Nglob, start);
    }

    // !^~
    // Распознаём оператор или разделитель `!^~`.
    if (get_it() == '!' && get_it(1) == '^' && get_it(2) == '~') {
        eat_it(3);
        return make_token(Token_Type::Nstarts, start);
    }

    // !$~
    // Распознаём оператор или разделитель `!$~`.
    if (get_it() == '!' && get_it(1) == '$' && get_it(2) == '~') {
        eat_it(3);
        return make_token(Token_Type::Nends, start);
    }

    // Eat unknown character
    // Пропускаем неизвестный байт.
    eat_it();

    // Don't include the byte in value: we can't tell if it's a valid character
    // in the current encoding. A single byte may be part of a multi-byte UTF-8
    // sequence, and slicing it would produce invalid text. So we only report
    // the error location
    // Сообщаем только позицию ошибки. Отдельный байт может быть частью многобайтового UTF-8 и
    // не образовывать корректный текст.
    return make_token(Token_Type::Invalid, "", start);
}

} // namespace mado::query
