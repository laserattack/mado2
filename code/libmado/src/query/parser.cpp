#include <mado/common/text_utils.hpp>
#include <mado/query/parser.hpp>

#include <utf8proc/utf8proc.h>

#include <algorithm>

namespace mado::query {

namespace {

// Returns the canonical field name for diagnostics. Unknown fields return "unknown".
// No exceptions.
// Возвращает основное имя поля для диагностики или "unknown". Исключений нет.
const char *comparison_field_name(Ast_Comparison_Field field) {
    switch (field) {
    case Ast_Comparison_Field::Priority: return "priority";
    case Ast_Comparison_Field::Tag: return "tag";
    case Ast_Comparison_Field::Status: return "status";
    case Ast_Comparison_Field::Path: return "path";
    case Ast_Comparison_Field::Name: return "name";
    case Ast_Comparison_Field::Time: return "time";
    case Ast_Comparison_Field::Deadline: return "deadline";
    case Ast_Comparison_Field::Mtime: return "mtime";
    case Ast_Comparison_Field::Any: return "any";
    }
    return "unknown";
}

// Describes the unexpected input and retains its internal token kind.
// Allocation failures may throw std::bad_alloc.
// Описывает неожиданный ввод с сохранением внутреннего типа токена.
// Сбой выделения памяти может вызвать std::bad_alloc.
std::string unexpected_input(const Token &token) {
    if (token.type == Token_Type::End)
        return ", but the query ended (recognized as End)";
    if (token.type == Token_Type::Invalid && token.value.empty())
        return ", got an invalid token (recognized as Invalid)";
    // Escape quotes and control characters so the token remains on one line.
    // Экранируем кавычки и управляющие символы, чтобы токен оставался на одной строке.
    std::string text;
    for (char c : token.value) {
        switch (c) {
        case '\\': text += "\\\\"; break;
        case '\'': text += "\\'"; break;
        case '\n': text += "\\n"; break;
        case '\r': text += "\\r"; break;
        case '\t': text += "\\t"; break;
        default: text += c; break;
        }
    }
    return ", got '" + text + "' (recognized as " + token_type_to_string(token.type) + ")";
}

// Stores codepoint and display-column counts for a query fragment.
// Хранит число кодовых точек и экранных колонок фрагмента запроса.
struct Display_Metrics {
    size_t codepoints;
    size_t columns;
};

// Codepoints and terminal columns occupied by the first byte_len
// bytes of str. Invalid UTF-8 bytes count as one column and are
// skipped as codepoints.
// Считает кодовые точки и колонки первых byte_len байт. Некорректный байт UTF-8 занимает одну
// колонку и не считается кодовой точкой.
Display_Metrics utf8proc_metrics(const std::string &str, size_t byte_len) {
    Display_Metrics m{0, 0};
    size_t i = 0;
    const size_t n = std::min(byte_len, str.size());

    while (i < n) {
        utf8proc_int32_t codepoint = 0;
        utf8proc_ssize_t consumed = utf8proc_iterate(
            reinterpret_cast<const utf8proc_uint8_t *>(str.data() + i),
            static_cast<utf8proc_ssize_t>(n - i),
            &codepoint);

        if (consumed < 0) {
            // Invalid UTF-8: one byte, one column, not a codepoint
            // Некорректный байт UTF-8 занимает одну колонку без кодовой точки.
            m.columns += 1;
            i += 1;
            continue;
        }

        m.codepoints += 1;
        int w = utf8proc_charwidth(codepoint);
        if (w > 0) {
            m.columns += static_cast<size_t>(w);
        }
        i += static_cast<size_t>(consumed);
    }

    return m;
}

} // namespace

// Formats the query diagnostic with a caret at the error location. May throw
// std::bad_alloc.
// Форматирует ошибку запроса с указателем на её позицию. Возможен std::bad_alloc.
std::string Parse_Error::format(const std::string &query) const {
    size_t byte_pos = std::min(token_.position, query.size());

    Display_Metrics m = utf8proc_metrics(query, byte_pos);

    // Measure the original lexeme, including quotes and macro syntax.
    // End-of-input and zero-width fragments still receive one caret.
    // Измеряем исходную лексему вместе с кавычками и синтаксисом макроса.
    // Конец ввода и фрагменты нулевой ширины отмечаем одним указателем.
    const size_t source_length = token_.length ? token_.length : token_.value.size();
    size_t span_length = std::min(source_length, query.size() - byte_pos);
    const auto line_end = query.find_first_of("\r\n", byte_pos);
    if (line_end != std::string::npos)
        span_length = std::min(span_length, line_end - byte_pos);
    const auto span = utf8proc_metrics(query.substr(byte_pos, span_length), span_length);
    const size_t width = token_.type == Token_Type::End ? 1 : std::max(size_t{1}, span.columns);

    std::string pos_str = std::to_string(m.codepoints + 1);
    std::string padding(pos_str.size(), ' ');

    // Keep every diagnostic line aligned with the query gutter.
    // Выравниваем каждую строку диагностики по полю слева от запроса.
    std::string message = what();
    const std::string continuation = "\n" + padding + " | ";
    size_t newline = message.find('\n');
    while (newline != std::string::npos) {
        message.replace(newline, 1, continuation);
        newline = message.find('\n', newline + continuation.size());
    }

    return pos_str + " | " + query + "\n" +
           padding + " | " + std::string(m.columns, ' ') + std::string(width, '^') + "\n" +
           padding + " | " + message + "\n";
}

// Checks for End. Copying a token may throw std::bad_alloc.
// Проверяет End. Копирование токена может вызвать std::bad_alloc.
bool Parser::is_at_end() const {
    return get_it().type == Token_Type::End;
}

// Peeks ahead without consuming input and returns the end sentinel past the
// input. Copying a token may allocate.
// Смотрит вперёд без продвижения. За концом возвращает End. Копирование токена может выделять
// память.
Token Parser::get_it(size_t n) const {
    if (current_ + n >= tokens_.size()) {
        return Token{Token_Type::End, "", tokens_.back().position};
    }
    return tokens_[current_ + n];
}

// Consumes input up to its end and returns the current item when applicable.
// Copying a token may allocate.
// Возвращает текущий токен и продвигается до конца. Копирование токена может выделять память.
Token Parser::eat_it() {
    Token token = get_it();

    if (!is_at_end()) {
        current_++;
    }

    return token;
}

// Checks whether the token type is accepted by the field. Returns false on
// mismatch. No exceptions.
// Проверяет, допустим ли тип токена для поля. При несовпадении возвращает false. Исключений
// нет.
bool Parser::value_matches_field(Ast_Comparison_Field field, const Token &val) const {
    switch (field) {
    case Ast_Comparison_Field::Priority:
        return val.type == Token_Type::Number;
    case Ast_Comparison_Field::Time:
    case Ast_Comparison_Field::Deadline:
    case Ast_Comparison_Field::Mtime:
        return val.type == Token_Type::Timestamp;
    // String fields accept any value token and keep its text in the AST.
    // Строковые поля принимают любой токен значения и сохраняют его текст в AST.
    case Ast_Comparison_Field::Tag:
    case Ast_Comparison_Field::Status:
    case Ast_Comparison_Field::Path:
    case Ast_Comparison_Field::Name:
    case Ast_Comparison_Field::Any:
        return val.type == Token_Type::Number ||
               val.type == Token_Type::String ||
               val.type == Token_Type::Timestamp ||
               token_is_keyword(val.type);
    }
    return false;
}

// Returns the expected operand type for diagnostics. May throw std::bad_alloc.
// Возвращает ожидаемый тип операнда для диагностики. Возможен std::bad_alloc.
std::string Parser::value_type_desc(Ast_Comparison_Field field) const {
    switch (field) {
    case Ast_Comparison_Field::Priority:
        return "a number";
    case Ast_Comparison_Field::Tag:
    case Ast_Comparison_Field::Status:
    case Ast_Comparison_Field::Path:
    case Ast_Comparison_Field::Name:
        return "a string";
    case Ast_Comparison_Field::Time:
    case Ast_Comparison_Field::Deadline:
    case Ast_Comparison_Field::Mtime:
        return "a timestamp";
    case Ast_Comparison_Field::Any:
        return "a value";
    }
    return "a value";
}

// Builds the complete query AST. Throws Parse_Error for empty or invalid
// queries and std::bad_alloc on allocation failure.
// Строит AST запроса. Некорректный или пустой запрос вызывает Parse_Error, сбой выделения
// памяти — std::bad_alloc.
std::unique_ptr<Ast_Node> Parser::parse() {
    if (is_at_end()) {
        throw Parse_Error("Empty query", get_it());
    }

    auto ast = parse_expression();

    if (!is_at_end()) {
        throw Parse_Error("Expected a binary operator or the end of the query" + unexpected_input(get_it()), get_it());
    }

    return ast;
}

// expr bin_op expr
// Начинаем разбор с наименее приоритетного бинарного оператора.
std::unique_ptr<Ast_Node> Parser::parse_expression() {
    return parse_or();
}

// expr or expr
// На этом уровне объединяем операнды OR.
std::unique_ptr<Ast_Node> Parser::parse_or() {
    auto left = parse_xor();

    // Fold successive OR operands from left to right.
    // Сворачиваем последовательные операнды OR слева направо.
    while (get_it().type == Token_Type::Or) {
        eat_it();
        auto right = parse_xor();
        left = ast_make_binary(Ast_Binary_Operator::Or,
                               std::move(left), std::move(right));
    }

    return left;
}

// expr xor expr
// На этом уровне объединяем операнды XOR.
std::unique_ptr<Ast_Node> Parser::parse_xor() {
    auto left = parse_and();

    // Fold successive XOR operands from left to right.
    // Сворачиваем последовательные операнды XOR слева направо.
    while (get_it().type == Token_Type::Xor) {
        eat_it();
        auto right = parse_and();
        left = ast_make_binary(Ast_Binary_Operator::Xor,
                               std::move(left), std::move(right));
    }

    return left;
}

// expr and expr
// На этом уровне объединяем операнды AND.
std::unique_ptr<Ast_Node> Parser::parse_and() {
    auto left = parse_not();

    // Fold successive AND operands from left to right.
    // Сворачиваем последовательные операнды AND слева направо.
    while (get_it().type == Token_Type::And) {
        eat_it();
        auto right = parse_not();
        left = ast_make_binary(Ast_Binary_Operator::And,
                               std::move(left), std::move(right));
    }

    return left;
}

// not expr
// Логическое НЕ.
std::unique_ptr<Ast_Node> Parser::parse_not() {
    if (get_it().type == Token_Type::Not) {
        eat_it();
        auto expr = parse_not();
        return ast_make_unary(Ast_Unary_Operator::Not, std::move(expr));
    }

    return parse_primary();
}

// Parses a grouped expression or a field or keyword. Throws Parse_Error for
// invalid syntax.
// Разбирает выражение в скобках, поле или ключевое слово. Ошибка синтаксиса вызывает
// Parse_Error.
std::unique_ptr<Ast_Node> Parser::parse_primary() {
    // (expr)
    // Скобки запускают разбор вложенного выражения.
    if (get_it().type == Token_Type::Lparen) { // ( / Распознаём оператор или разделитель `(`.
        eat_it();
        auto expr = parse_expression();

        auto rparen = eat_it(); // ) / Распознаём оператор или разделитель `)`.

        if (rparen.type != Token_Type::Rparen) {
            throw Parse_Error("Expected ')'" + unexpected_input(rparen), rparen);
        }

        return expr;
    }

    return parse_field_and_special();
}

// expr
// Первый токен определяет вид выражения.
std::unique_ptr<Ast_Node> Parser::parse_field_and_special() {
    auto field = eat_it();

    // Annotate failed comparisons only when the field name was fuzzy-matched.
    // Preserve the offending token so the caret still points to the error.
    // Дополняем ошибку сравнения только при нечётком распознавании имени поля.
    // Сохраняем проблемный токен, чтобы указатель остался на месте ошибки.
    const auto comparison = [&](Ast_Comparison_Field resolved_field) {
        try {
            return parse_comparison(resolved_field);
        } catch (const Parse_Error &error) {
            const auto name = mado::common::utf8_tolower(token_type_to_string(field.type));
            if (mado::common::equals_ignore_case(field.value, name))
                throw;
            throw Parse_Error(std::string(error.what()) + "\nField '" + name +
                                  "' matched from '" + field.value + "'",
                              error.token());
        }
    };

    switch (field.type) {

    // expr: special_expr
    // Специальное выражение не требует поля и операнда.
    case Token_Type::All:
        return ast_make_special(Ast_Node_Type::All);
    case Token_Type::Untagged:
        return ast_make_special(Ast_Node_Type::Untagged);
    case Token_Type::Unstatused:
        return ast_make_special(Ast_Node_Type::Unstatused);
    case Token_Type::Unnamed:
        return ast_make_special(Ast_Node_Type::Unnamed);
    case Token_Type::Unprioritized:
        return ast_make_special(Ast_Node_Type::Unprioritized);
    case Token_Type::Undeadlined:
        return ast_make_special(Ast_Node_Type::Undeadlined);

    // expr: field comp_op value
    // Одиночное сравнение задаётся полем, оператором и значением.
    // expr: field comp_op anyof(v,v,v,...)
    // expr: field comp_op allof(v,v,v,...)
    // anyof и allof задают список значений для одного сравнения.
    // expr: field in (v,v,v,...)
    // expr: field has (v,v,v,...)
    // expr: field in [v..v]
    // expr: field in [v..]
    // expr: field in [..v]
    // Для in и has доступны сокращения со списком, а для in также диапазоны.
    case Token_Type::Priority:
        return comparison(Ast_Comparison_Field::Priority);
    case Token_Type::Tag:
        return comparison(Ast_Comparison_Field::Tag);
    case Token_Type::Status:
        return comparison(Ast_Comparison_Field::Status);
    case Token_Type::Path:
        return comparison(Ast_Comparison_Field::Path);
    case Token_Type::Name:
        return comparison(Ast_Comparison_Field::Name);
    case Token_Type::Time:
        return comparison(Ast_Comparison_Field::Time);
    case Token_Type::Deadline:
        return comparison(Ast_Comparison_Field::Deadline);
    case Token_Type::Mtime:
        return comparison(Ast_Comparison_Field::Mtime);
    case Token_Type::Any:
        return comparison(Ast_Comparison_Field::Any);

    default:
        throw Parse_Error("Expected a field name or a special keyword" + unexpected_input(field), field);
    }
}

// Parses a comparison, list, or range. Throws Parse_Error for invalid syntax
// or types.
// Разбирает сравнение, список или диапазон. Неверный синтаксис или тип вызывают Parse_Error.
std::unique_ptr<Ast_Node> Parser::parse_comparison(Ast_Comparison_Field field) {
    const std::string context = " for '" + std::string(comparison_field_name(field)) + "'";

    // expr: field in [v..v]
    // expr: field in [v..]
    // expr: field in [..v]
    // expr: field in (v,v,v,...)
    // expr: field has (v,v,v,...)
    // После in или has выбираем разбор списка либо диапазона.
    if (get_it().type == Token_Type::In || get_it().type == Token_Type::Has) {
        bool combine_and = get_it().type == Token_Type::Has;
        eat_it(); // in / has / Оператор списка или диапазона.

        auto next = get_it();

        // After 'in', both '[' (range) and '(' (list) are valid.
        // After 'has', only '(' (list) is valid.
        // После in допустимы диапазон в квадратных скобках или список в круглых. После has
        // допустим только список.
        if (!combine_and && next.type == Token_Type::Lbracket) {
            return parse_range(field);
        }
        if (next.type == Token_Type::Lparen) {
            return parse_list(field, Ast_Comparison_Operator::Eq, combine_and);
        }

        if (combine_and) { // has / Содержит все значения.
            throw Parse_Error("Expected '(' after 'has'" + context + unexpected_input(next), next);
        } else { // in / Проверка вхождения.
            throw Parse_Error("Expected '(' or '[' after 'in'" + context + unexpected_input(next), next);
        }
    }

    const auto operator_token = get_it();
    Ast_Comparison_Operator op;
    // Report all operators accepted at the field boundary.
    // На границе поля перечисляем все допустимые операторы.
    try {
        op = parse_comparison_operator();
    } catch (const Parse_Error &error) {
        throw Parse_Error("Expected a comparison operator, 'in', or 'has'" +
                              context + unexpected_input(error.token()),
                          error.token());
    }

    // expr: field comp_op anyof(v,v,v,...)
    // expr: field comp_op allof(v,v,v,...)
    // После оператора сравнения распознаём anyof и allof со списком.
    if ((get_it().type == Token_Type::Allof || get_it().type == Token_Type::Anyof) &&
        // sugar only when followed by '(', otherwise they are plain string values
        // Сокращённый синтаксис действует только перед открывающей скобкой. Иначе это строки.
        get_it(1).type == Token_Type::Lparen) {

        bool combine_and = get_it().type == Token_Type::Allof;
        eat_it();
        return parse_list(field, op, combine_and);
    }

    // expr: field comp_op value
    // Иначе ожидаем одиночное значение подходящего типа.
    auto tok = get_it();
    if (!value_matches_field(field, tok)) {
        // A comparison accepts either one value or a list modifier.
        // Сравнение принимает одиночное значение или модификатор списка.
        throw Parse_Error("Expected " + value_type_desc(field) +
                              ", anyof(...), or allof(...) after '" + operator_token.value +
                              "'" + context + unexpected_input(tok),
                          tok);
    }
    return parse_value(field, op);
}

// Expands inclusive range bounds into comparisons. Throws Parse_Error for
// malformed or empty ranges.
// Преобразует включительные границы в сравнения. Пустой или неверный диапазон вызывает
// Parse_Error.
std::unique_ptr<Ast_Node> Parser::parse_range(Ast_Comparison_Field field) {
    const std::string context = " in the range for '" + std::string(comparison_field_name(field)) + "'";

    // '[' is guaranteed by the caller
    // Вызывающий код гарантирует открывающую квадратную скобку.
    eat_it(); // [ / Распознаём оператор или разделитель `[`.

    std::unique_ptr<Ast_Node> low;
    if (get_it().type != Token_Type::DotDot) {
        auto tok = get_it();
        if (!value_matches_field(field, tok)) {
            throw Parse_Error("Expected " + value_type_desc(field) + " or '..' after '['" +
                                  context + unexpected_input(tok), tok);
        }
        low = parse_value(field, Ast_Comparison_Operator::Ge);
    }

    auto dots = eat_it(); // .. / Распознаём оператор или разделитель `..`.
    if (dots.type != Token_Type::DotDot) {
        throw Parse_Error("Expected '..' after the lower bound" + context + unexpected_input(dots), dots);
    }

    std::unique_ptr<Ast_Node> high;
    if (get_it().type != Token_Type::Rbracket) {
        auto tok = get_it();
        if (!value_matches_field(field, tok)) {
            if (low) {
                throw Parse_Error("Expected " + value_type_desc(field) + " or ']' after '..'" +
                                      context + unexpected_input(tok), tok);
            } else {
                throw Parse_Error("Expected " + value_type_desc(field) + " after '..'" +
                                      context + unexpected_input(tok), tok);
            }
        }
        high = parse_value(field, Ast_Comparison_Operator::Le);
    }

    auto rbr = eat_it(); // ] / Распознаём оператор или разделитель `]`.
    if (rbr.type != Token_Type::Rbracket) {
        throw Parse_Error("Expected ']' after the upper bound" + context + unexpected_input(rbr), rbr);
    }

    if (!low && !high) {
        throw Parse_Error("Range must have at least one bound. Expected " +
                              value_type_desc(field) + context,
                          rbr);
    }

    // Join two bounds with AND and keep a lone bound as a single comparison.
    // Соединяем две границы через AND, а одну оставляем отдельным сравнением.
    if (low && high) {
        return ast_make_binary(Ast_Binary_Operator::And, std::move(low), std::move(high));
    }
    return low ? std::move(low) : std::move(high);
}

// Expands a nonempty value list into AND or OR nodes. Throws Parse_Error for
// invalid values or delimiters.
// Преобразует непустой список в узлы AND или OR. Неверные значения и разделители вызывают
// Parse_Error.
std::unique_ptr<Ast_Node> Parser::parse_list(
    Ast_Comparison_Field field,
    Ast_Comparison_Operator op,
    bool combine_and) {
    const std::string context = " in the value list for '" +
                                std::string(comparison_field_name(field)) + "'";

    // '(' is guaranteed by the caller
    // Вызывающий код гарантирует открывающую круглую скобку.
    eat_it(); // ( / Распознаём оператор или разделитель `(`.

    if (get_it().type == Token_Type::Rparen) {
        auto rparen = eat_it(); // ) / Распознаём оператор или разделитель `)`.
        throw Parse_Error("List cannot be empty, expected " + value_type_desc(field) +
                              " after '('" + context + unexpected_input(rparen),
                          rparen);
    }

    auto combine = combine_and ? Ast_Binary_Operator::And : Ast_Binary_Operator::Or;
    std::unique_ptr<Ast_Node> result;

    // Build the value-list expression until its closing delimiter.
    // Собираем выражение списка значений до закрывающего разделителя.
    while (true) {
        auto tok = get_it();
        if (!value_matches_field(field, tok)) {
            const std::string delimiter = result ? "," : "(";
            throw Parse_Error("Expected " + value_type_desc(field) + " after '" + delimiter +
                                  "'" + context + unexpected_input(tok), tok);
        }
        auto cmp = parse_value(field, op);

        result = result
                     ? ast_make_binary(combine, std::move(result), std::move(cmp))
                     : std::move(cmp);

        auto next = eat_it(); // , or ) / Запятая или закрывающая скобка.
        if (next.type == Token_Type::Comma) {
            continue;
        }
        if (next.type == Token_Type::Rparen) {
            break;
        }
        throw Parse_Error("Expected ',' or ')' after a value" + context + unexpected_input(next), next);
    }

    return result;
}

// Consumes a value and builds a comparison node. Throws Parse_Error for an
// unknown field.
// Читает значение и строит сравнение. Неизвестное поле вызывает Parse_Error.
std::unique_ptr<Ast_Node> Parser::parse_value(
    Ast_Comparison_Field field,
    Ast_Comparison_Operator op) {

    auto val = eat_it();

    // caller guarantees val matches field (see value_matches_field)
    // Вызывающий код гарантирует соответствие значения полю через value_matches_field.
    switch (field) {
    case Ast_Comparison_Field::Priority:
        // Lexer guarantees 1-3 digit positive numbers, so stoi can't overflow
        // Лексер гарантирует от одной до трёх цифр, поэтому stoi не переполняется.
        return ast_make_comparison(field, op, static_cast<uint16_t>(std::stoi(val.value)));

    case Ast_Comparison_Field::Tag:
    case Ast_Comparison_Field::Status:
    case Ast_Comparison_Field::Path:
    case Ast_Comparison_Field::Name:
        return ast_make_comparison(field, op, val.value);

    case Ast_Comparison_Field::Time:
    case Ast_Comparison_Field::Deadline:
    case Ast_Comparison_Field::Mtime:
        return ast_make_comparison(field, op, val.value);

    case Ast_Comparison_Field::Any:
        if (val.type == Token_Type::Number) {
            return ast_make_comparison(field, op, static_cast<uint16_t>(std::stoi(val.value)));
        }
        return ast_make_comparison(field, op, val.value);
    }

    // unreachable
    // Недостижимая ветка.
    throw Parse_Error("Unknown field", val);
}

// Consumes and maps a comparison operator. Throws Parse_Error for other
// tokens.
// Читает и распознаёт оператор сравнения. Другой токен вызывает Parse_Error.
Ast_Comparison_Operator Parser::parse_comparison_operator() {
    auto op = eat_it();

    switch (op.type) {
    case Token_Type::Gt:
        return Ast_Comparison_Operator::Gt;
    case Token_Type::Lt:
        return Ast_Comparison_Operator::Lt;
    case Token_Type::Ge:
        return Ast_Comparison_Operator::Ge;
    case Token_Type::Le:
        return Ast_Comparison_Operator::Le;
    case Token_Type::Eq:
        return Ast_Comparison_Operator::Eq;
    case Token_Type::Ne:
        return Ast_Comparison_Operator::Ne;
    case Token_Type::Substr:
        return Ast_Comparison_Operator::Substr;
    case Token_Type::Nsubstr:
        return Ast_Comparison_Operator::Nsubstr;
    case Token_Type::Fuzzy:
        return Ast_Comparison_Operator::Fuzzy;
    case Token_Type::Nfuzzy:
        return Ast_Comparison_Operator::Nfuzzy;
    case Token_Type::Starts:
        return Ast_Comparison_Operator::Starts;
    case Token_Type::Nstarts:
        return Ast_Comparison_Operator::Nstarts;
    case Token_Type::Ends:
        return Ast_Comparison_Operator::Ends;
    case Token_Type::Nends:
        return Ast_Comparison_Operator::Nends;
    case Token_Type::Glob:
        return Ast_Comparison_Operator::Glob;
    case Token_Type::Nglob:
        return Ast_Comparison_Operator::Nglob;
    default:
        throw Parse_Error("Expected a comparison operator" + unexpected_input(op), op);
    }
}

} // namespace mado::query
