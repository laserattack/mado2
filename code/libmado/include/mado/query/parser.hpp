#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <mado/query/ast.hpp>
#include <mado/query/token.hpp>

namespace mado::query {

// Carries a query syntax error and its offending token.
// Хранит ошибку синтаксиса запроса и проблемный токен.
class Parse_Error : public std::runtime_error {
  public:
    // Stores the diagnostic and offending token. May throw std::bad_alloc.
    // Сохраняет диагностику и проблемный токен. Возможен std::bad_alloc.
    Parse_Error(const std::string &message, const Token &token)
        : std::runtime_error(message), token_(token) {}

    // Returns the offending token. No exceptions.
    // Возвращает проблемный токен. Исключений нет.
    const Token &token() const { return token_; }

    // Formats the error with the query and a caret at the error position
    // Форматирует ошибку с запросом и указателем позиции.
    std::string format(const std::string &query) const;

  private:
    Token token_;
};

// Converts a lexer token stream to a query AST.
// Преобразует токены лексера в AST запроса.
class Parser {
  public:
    // The vector always contains at least one End token; the lexer guarantees this
    // Лексер гарантирует хотя бы один токен End в векторе.
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    // Builds the complete query AST. Throws Parse_Error for empty or invalid
    // queries and std::bad_alloc on allocation failure.
    // Строит AST запроса. Некорректный или пустой запрос вызывает Parse_Error, сбой выделения
    // памяти — std::bad_alloc.
    std::unique_ptr<Ast_Node> parse(); // parse AST / Строит AST. Некорректный запрос вызывает Parse_Error.

  private:
    std::vector<Token> tokens_; // Tokens from lexer / Токены от лексера.
    size_t current_{0};         // Index of the current token in tokens_ / Индекс текущего токена в tokens_.

    // Parses a logical expression starting at the lowest precedence.
    // Parse_Error and allocation failures propagate.
    // Разбирает логическое выражение с самого низкого приоритета. Parse_Error и сбои выделения
    // памяти передаются вызывающему коду.
    std::unique_ptr<Ast_Node> parse_expression(); // any expression / Любое выражение.
    // Combines OR operands. Parse_Error and allocation failures propagate.
    // Соединяет операнды OR. Parse_Error и сбои выделения памяти передаются вызывающему коду.
    std::unique_ptr<Ast_Node> parse_or();         // lowest priority / Наименьший приоритет.
    // Combines XOR operands. Parse_Error and allocation failures propagate.
    // Соединяет операнды XOR. Parse_Error и сбои выделения памяти передаются вызывающему коду.
    std::unique_ptr<Ast_Node> parse_xor();        // priority > or / Приоритет выше OR.
    // Combines AND operands. Parse_Error and allocation failures propagate.
    // Соединяет операнды AND. Parse_Error и сбои выделения памяти передаются вызывающему коду.
    std::unique_ptr<Ast_Node> parse_and();        // priority > xor / Приоритет выше XOR.
    // Parses a NOT expression or its next higher-precedence operand.
    // Parse_Error and allocation failures propagate.
    // Разбирает NOT или операнд более высокого приоритета. Parse_Error и сбои выделения памяти
    // передаются вызывающему коду.
    std::unique_ptr<Ast_Node> parse_not();        // priority > and / Приоритет выше AND.
    // Parses a grouped expression or a field or keyword. Throws Parse_Error
    // for invalid syntax.
    // Разбирает выражение в скобках, поле или ключевое слово. Ошибка синтаксиса вызывает
    // Parse_Error.
    std::unique_ptr<Ast_Node> parse_primary();    // highest priority / Наивысший приоритет.

    // Consumes and maps a comparison operator. Throws Parse_Error for other
    // tokens.
    // Читает и распознаёт оператор сравнения. Другой токен вызывает Parse_Error.
    Ast_Comparison_Operator parse_comparison_operator();

    // Parses a field comparison or special keyword. Throws Parse_Error for an
    // unexpected token.
    // Разбирает сравнение поля или специальное ключевое слово. Неожиданный токен вызывает
    // Parse_Error.
    std::unique_ptr<Ast_Node> parse_field_and_special();
    // Parses a comparison, list, or range. Throws Parse_Error for invalid
    // syntax or types.
    // Разбирает сравнение, список или диапазон. Неверный синтаксис или тип вызывают
    // Parse_Error.
    std::unique_ptr<Ast_Node> parse_comparison(Ast_Comparison_Field field);
    // Expands a nonempty value list into AND or OR nodes. Throws Parse_Error
    // for invalid values or delimiters.
    // Преобразует непустой список в узлы AND или OR. Неверные значения и разделители вызывают
    // Parse_Error.
    std::unique_ptr<Ast_Node> parse_list(Ast_Comparison_Field field, Ast_Comparison_Operator op, bool is_allof);
    // Expands inclusive range bounds into comparisons. Throws Parse_Error for
    // malformed or empty ranges.
    // Преобразует включительные границы в сравнения. Пустой или неверный диапазон вызывает
    // Parse_Error.
    std::unique_ptr<Ast_Node> parse_range(Ast_Comparison_Field field);
    // Consumes a value and builds a comparison node. Throws Parse_Error for an
    // unknown field.
    // Читает значение и строит сравнение. Неизвестное поле вызывает Parse_Error.
    std::unique_ptr<Ast_Node> parse_value(Ast_Comparison_Field field, Ast_Comparison_Operator op);

    // Whether the token can be a value for the given field
    // Проверяет, подходит ли токен в качестве значения поля.
    bool value_matches_field(Ast_Comparison_Field field, const Token &val) const;

    // Human-readable description of the value type expected for the field
    // Человекочитаемое описание ожидаемого типа значения.
    std::string value_type_desc(Ast_Comparison_Field field) const;

    // Whether the end of the token stream has been reached
    // Проверяет конец потока токенов.
    bool is_at_end() const;

    // Returns the token at offset n from the current position
    // without advancing. get_it(0) returns the current token.
    // Returns End token if the position is past the end of the stream
    // Возвращает токен на смещении n без продвижения. Нулевое смещение означает текущий токен,
    // выход за конец даёт End.
    Token get_it(size_t n = 0) const;

    // Returns the current token and advances forward.
    // Returns End token if at the end of the stream
    // Возвращает текущий токен и продвигается вперёд. В конце возвращает End.
    Token eat_it();
};

} // namespace mado::query
