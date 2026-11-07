#pragma once

#include <memory>
#include <string>
#include <variant>

namespace mado::query {

// Identifies the concrete AST node kind.
// Обозначает конкретный тип узла AST.
enum class Ast_Node_Type {
    Comparison_Operator, // field op value / Поле, оператор, значение.
    Binary_Operator,     // expr op expr / Грамматика разбираемого выражения приведена выше.
    Unary_Operator,      // op expr / Оператор и выражение.
    All,                 // all / Все записи.
    Untagged,            // untagged / Записи без тегов.
    Unstatused,          // unstatused / Записи без статуса.
    Unnamed,             // unnamed / Записи без имени.
    Unprioritized,       // unprioritized / Записи без приоритета.
    Undeadlined,         // undeadlined / Записи без срока.
};

// Identifies an entry field available to query comparisons.
// Обозначает поле записи, доступное для сравнения.
enum class Ast_Comparison_Field {
    Priority,
    Tag,
    Status,
    Path,
    Name,
    Time,
    Deadline,
    Mtime,
    Any,
};

// Returns the field's diagnostic name or "Unknown". No exceptions.
// Возвращает диагностическое имя поля или "Unknown". Исключений нет.
const char *comparison_field_to_string(Ast_Comparison_Field field);

// Identifies the comparison operation applied to an entry value.
// Обозначает операцию сравнения значения записи.
enum class Ast_Comparison_Operator {
    Gt,
    Lt,
    Ge,
    Le,
    Eq,
    Ne,
    Substr,
    Nsubstr,
    Fuzzy,
    Nfuzzy,
    Starts,
    Nstarts,
    Ends,
    Nends,
    Glob,
    Nglob,
};

// Identifies a binary logical operation.
// Обозначает бинарную логическую операцию.
enum class Ast_Binary_Operator {
    And,
    Or,
    Xor,
};

// Identifies a unary logical operation.
// Обозначает унарную логическую операцию.
enum class Ast_Unary_Operator {
    Not,
};

// Base node tagged with its concrete AST kind.
// Базовый узел с указанием конкретного типа AST.
struct Ast_Node {
    Ast_Node_Type type;
    // Destroys a derived node through its base pointer. No exceptions.
    // Уничтожает узел через базовый указатель. Исключений нет.
    virtual ~Ast_Node() = default;

  protected:
    // Tags the node with its concrete kind. No exceptions.
    // Устанавливает конкретный тип узла. Исключений нет.
    Ast_Node(Ast_Node_Type t) : type(t) {}
};

// A number, string, or missing operand in a diagnostic partial AST.
// Число, строка или отсутствующий операнд в частичном AST для диагностики.
using Ast_Value = std::variant<uint16_t, std::string, std::monostate>;

// Stores a field comparison and its numeric or string operand.
// Хранит сравнение поля и числовой либо строковый операнд.
struct Ast_Comparison_Operator_Node : Ast_Node {
    Ast_Comparison_Field field;
    Ast_Comparison_Operator op;
    Ast_Value value;

    // Stores the field, operation, and value. No exceptions while moving the value.
    // Хранит поле, операцию и значение. Перемещение значения не выбрасывает исключений.
    Ast_Comparison_Operator_Node(Ast_Comparison_Field f,
                                 Ast_Comparison_Operator o,
                                 Ast_Value v)
        : Ast_Node(Ast_Node_Type::Comparison_Operator),
          field(f), op(o), value(std::move(v)) {}

    // Reports whether the comparison stores a number. No exceptions.
    // Проверяет, хранит ли сравнение число. Исключений нет.
    bool is_number() const;

    // Reports whether the comparison stores a string. No exceptions.
    // Проверяет, хранит ли сравнение строку. Исключений нет.
    bool is_string() const;

    // Returns the numeric operand. Throws std::bad_variant_access for a
    // string.
    // Возвращает числовой операнд. Для строки выбрасывает std::bad_variant_access.
    uint16_t as_number() const;

    // Returns the string operand. Throws std::bad_variant_access for a number.
    // Возвращает строковый операнд. Для числа выбрасывает std::bad_variant_access.
    const std::string &as_string() const;
};

// Owns the operands and operation of a binary expression.
// Владеет операндами бинарного выражения и хранит операцию.
struct Ast_Binary_Operator_Node : Ast_Node {
    Ast_Binary_Operator op;
    std::unique_ptr<Ast_Node> left;
    std::unique_ptr<Ast_Node> right;

    // Takes ownership of both operands. No exceptions.
    // Принимает владение обоими операндами. Исключений нет.
    Ast_Binary_Operator_Node(Ast_Binary_Operator o,
                             std::unique_ptr<Ast_Node> l,
                             std::unique_ptr<Ast_Node> r)
        : Ast_Node(Ast_Node_Type::Binary_Operator),
          op(o), left(std::move(l)), right(std::move(r)) {}
};

// Owns the operand and operation of a unary expression.
// Владеет операндом унарного выражения и хранит операцию.
struct Ast_Unary_Operator_Node : Ast_Node {
    Ast_Unary_Operator op;
    std::unique_ptr<Ast_Node> expr;

    // Takes ownership of the operand. No exceptions.
    // Принимает владение операндом. Исключений нет.
    Ast_Unary_Operator_Node(Ast_Unary_Operator o,
                            std::unique_ptr<Ast_Node> e)
        : Ast_Node(Ast_Node_Type::Unary_Operator),
          op(o), expr(std::move(e)) {}
};

// Represents a special keyword with no operands.
// Представляет специальное ключевое слово без операндов.
struct Ast_Special_Node : Ast_Node {
    // Stores the special keyword kind. No exceptions.
    // Хранит тип специального ключевого слова. Исключений нет.
    Ast_Special_Node(Ast_Node_Type t) : Ast_Node(t) {}
};

// Creates a binary node and takes ownership of its operands. May throw
// std::bad_alloc.
// Создаёт бинарный узел и принимает владение операндами. Возможен std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_binary(
    Ast_Binary_Operator op,
    std::unique_ptr<Ast_Node> left,
    std::unique_ptr<Ast_Node> right);

// Creates a unary node and takes ownership of its operand. May throw
// std::bad_alloc.
// Создаёт унарный узел и принимает владение операндом. Возможен std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_unary(
    Ast_Unary_Operator op,
    std::unique_ptr<Ast_Node> expr);

// Creates a node for a special keyword. May throw std::bad_alloc.
// Создаёт узел специального ключевого слова. Возможен std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_special(Ast_Node_Type type);

// Creates a comparison node for a numeric or string value. May throw
// std::bad_alloc.
// Создаёт узел сравнения с числом или строкой. Возможен std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_comparison(
    Ast_Comparison_Field field,
    Ast_Comparison_Operator op,
    Ast_Value value);

// Prints an AST recursively with indentation. A null node prints nothing.
// Ignores output errors.
// Рекурсивно печатает AST с отступами. Нулевой узел пропускается, ошибки вывода игнорируются.
void ast_print(const Ast_Node *node, size_t depth = 0);

} // namespace mado::query
