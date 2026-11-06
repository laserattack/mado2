#pragma once

#include <string>
#include <vector>

#include <mado/query/token.hpp>

namespace mado::query {

// Converts a query string to positioned tokens.
// Преобразует строку запроса в токены с позициями.
class Lexer {
  public:
    // Takes ownership of the query string. Moving it does not throw.
    // Принимает строку запроса. Перемещение не выбрасывает исключений.
    explicit Lexer(std::string query) : query_(std::move(query)) {}

    // Tokenizes the query string
    // Appends End and uses Invalid for malformed input. May throw std::bad_alloc.
    // Разбирает запрос. Добавляет End, а для некорректного ввода использует Invalid. Возможен
    // std::bad_alloc.
    std::vector<Token> tokenize();

  private:
    std::string query_; // the source query string / Исходная строка запроса.
    size_t pos_{0};     // current position in the string / Текущая позиция в строке.

    // Skips whitespace characters. No exceptions.
    // Пропускает пробельные символы. Исключений нет.
    void skip_whitespace();

    // Checks whether the input is exhausted. No exceptions.
    // Проверяет конец ввода. Исключений нет.
    bool is_at_end() const;

    // Returns the character at offset n from the current position
    // without advancing. get_it() returns the current character.
    // Returns '\0' past the end. No exceptions.
    // Возвращает символ на смещении n без продвижения. По умолчанию возвращает текущий символ,
    // за концом — нулевой. Исключений нет.
    char get_it(size_t n = 0) const;

    // Returns the current character and advances forward.
    // Returns '\0' if at the end
    // Возвращает текущий символ и продвигается вперёд. В конце возвращает нулевой символ.
    char eat_it();

    // Advances forward by n characters.
    // Stops at the end. No exceptions.
    // Продвигается на n символов, останавливаясь в конце. Исключений нет.
    void eat_it(size_t n);

    // Creates a token of the given type from the substring [start, pos_)
    // Создаёт токен заданного типа из диапазона [start, pos_).
    Token make_token(Token_Type type, size_t start) const;

    // Moves an explicit value into a token. No exceptions while moving.
    // Перемещает готовое значение в токен. Перемещение не выбрасывает исключений.
    Token make_token(Token_Type type, std::string value, size_t start) const;

    // Scans a number or timestamp and emits Invalid for malformed values. May
    // throw std::bad_alloc.
    // Scans numbers and timestamps. Invalid input yields Invalid.
    // May throw std::bad_alloc.
    // Разбирает число или метку времени. Некорректное значение даёт Invalid. Возможен
    // std::bad_alloc.
    Token parse_number_and_timestamp();
    // Scans quoted text and emits Invalid when the quote is unterminated. May
    // throw std::bad_alloc.
    // Scans quoted text. Unterminated input yields Invalid.
    // May throw std::bad_alloc.
    // Читает строку в кавычках. Незакрытая строка даёт Invalid. Возможен std::bad_alloc.
    Token parse_quoted_string();
    // Scans an identifier and prefers exact keyword matches over fuzzy
    // matches. May throw std::bad_alloc.
    // Scans a name and resolves exact or fuzzy keywords.
    // May throw std::bad_alloc.
    // Разбирает идентификатор и предпочитает точное совпадение ключевого слова нечёткому.
    // Возможен std::bad_alloc.
    Token parse_identifier_and_keyword();
    // Scans an operator or punctuation, emitting Invalid for unknown input.
    // May throw std::bad_alloc.
    // Scans punctuation. Unknown input yields Invalid.
    // May throw std::bad_alloc.
    // Разбирает оператор или знак пунктуации. Неизвестный ввод даёт Invalid. Возможен
    // std::bad_alloc.
    Token parse_operator_and_punctuation();
};

} // namespace mado::query
