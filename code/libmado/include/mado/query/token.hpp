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
// Типы токенов разделены на ключевые слова до MAX_KEYWORD_VALUE включительно и остальные
// токены выше этой границы.
constexpr uint16_t MAX_KEYWORD_VALUE = 1000;

// Identifies query lexemes, including invalid input and the end sentinel.
// Обозначает лексемы запроса, включая неверный ввод и маркер конца.
enum class Token_Type {
    // keywords
    // Ключевые слова.

    // Keywords must consist only of letters, so the lexer can safely
    // distinguish them from operators.
    // Ключевые слова состоят из букв, чтобы лексер отличал их от операторов.

    // fields
    // Поля записи.
    Priority = 0, // priority / Приоритет.
    Tag,          // tag / Тег.
    Status,       // status / Статус.
    Name,         // name / Имя.
    Path,         // path / Путь.
    Time,         // time / Время создания.
    Deadline,     // deadline / Срок.
    Mtime,        // mtime / Время изменения.
    Any,          // any / Любое поле.

    // special expressions
    // Специальные выражения.
    All,           // all / Все записи.
    Untagged,      // untagged / Записи без тегов.
    Unstatused,    // unstatused / Записи без статуса.
    Unnamed,       // unnamed / Записи без имени.
    Unprioritized, // unprioritized / Записи без приоритета.
    Undeadlined,   // undeadlined / Записи без срока.

    // sugar
    // Сокращённый синтаксис.
    Allof, // allof / Все значения списка.
    Anyof, // anyof / Любое значение списка.
    In,    // in / Проверка вхождения.
    Has,   // has / Содержит все значения.

    // logical operators
    // Логические операторы.
    And, // and / Логическое И.
    Or,  // or / Логическое ИЛИ.
    Xor, // xor / Исключающее ИЛИ.
    Not, // not / Логическое НЕ.

    // not keywords
    // Токены, не являющиеся ключевыми словами.

    // Non-keywords must not contain letters, so the lexer does not
    // confuse them with identifiers. Values (Number, String,
    // Timestamp) are lexer output, not source text, and are matched
    // separately.
    // Операторы без букв отличаются от идентификаторов. Number, String и Timestamp
    // распознаются отдельно как результаты лексера.

    // comparison operators
    // Операторы сравнения.
    Gt = MAX_KEYWORD_VALUE + 1, // > / Распознаём оператор или разделитель `>`.
    Lt,                         // < / Распознаём оператор или разделитель `<`.
    Ge,                         // >= / Распознаём оператор или разделитель `>=`.
    Le,                         // <= / Распознаём оператор или разделитель `<=`.
    Eq,                         // = / Распознаём оператор или разделитель `=`.
    Ne,                         // != / Распознаём оператор или разделитель `!=`.
    Substr,                     // ~ / Распознаём оператор или разделитель `~`.
    Nsubstr,                    // !~ / Распознаём оператор или разделитель `!~`.
    Fuzzy,                      // ~~ / Распознаём оператор или разделитель `~~`.
    Nfuzzy,                     // !~~ / Распознаём оператор или разделитель `!~~`.
    Starts,                     // ^~ / Распознаём оператор или разделитель `^~`.
    Nstarts,                    // !^~ / Распознаём оператор или разделитель `!^~`.
    Ends,                       // $~ / Распознаём оператор или разделитель `$~`.
    Nends,                      // !$~ / Распознаём оператор или разделитель `!$~`.
    Glob,                       // %~ / Распознаём оператор или разделитель `%~`.
    Nglob,                      // !%~ / Распознаём оператор или разделитель `!%~`.

    // punctuation
    // Пунктуация.
    Lparen,   // ( / Распознаём оператор или разделитель `(`.
    Rparen,   // ) / Распознаём оператор или разделитель `)`.
    Comma,    // , / Распознаём оператор или разделитель `,`.
    DotDot,   // .. / Распознаём оператор или разделитель `..`.
    Lbracket, // [ / Распознаём оператор или разделитель `[`.
    Rbracket, // ] / Распознаём оператор или разделитель `]`.

    // values
    // Значения.
    Number,    // 0-999 / От 0 до 999.
    String,    // [a-zA-Z_][a-zA-Z0-9_-]* or "..." or '...' / Идентификатор или строка в одинарных либо двойных кавычках.
    Timestamp, // YYYYMMDD-HHMMSS with optional shorter forms: YYYY, YYYYMM, YYYYMMDD, YYYYMMDD-, YYYYMMDD-HH, YYYYMMDD-HHMM, YYYYMMDD-HHMMSS / Поддерживаемые форматы даты и времени указаны выше.

    // the end of the character stream
    // Конец потока символов.
    End,

    // invalid token. Must remain the last entry in the enum:
    // it is used as a loop termination condition in several places
    // Некорректный токен должен оставаться последним элементом перечисления. На этом основано
    // завершение обхода.
    Invalid,
};

// Stores a token kind, source value, and byte position.
// Хранит тип токена, исходное значение и смещение в байтах.
struct Token {
    Token_Type type{Token_Type::Invalid};
    std::string value;
    size_t position{0}; // character offset from the start of the query string / Смещение символа от начала запроса.
    // Original source length in bytes, independent of decoded or expanded value.
    // Длина исходного фрагмента в байтах независимо от раскрытого значения.
    size_t length{0};

    Token() = default;

    // Stores the token and optional source span. Moving the value does not throw.
    // Сохраняет токен и необязательную длину фрагмента. Перемещение не бросает исключений.
    Token(Token_Type t, std::string v, size_t pos, size_t source_length = 0)
        : type(t), value(std::move(v)), position(pos), length(source_length) {}
};

// Returns the diagnostic name for a token kind. May throw std::bad_alloc.
// Возвращает диагностическое имя типа токена. Возможен std::bad_alloc.
std::string token_type_to_string(Token_Type type);

// Tests whether the token kind lies in the keyword range. No exceptions.
// Проверяет принадлежность типа токена диапазону ключевых слов. Исключений нет.
bool token_is_keyword(Token_Type type);

} // namespace mado::query
