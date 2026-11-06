#include <mado/query/ast.hpp>

#include <cstdio>

namespace mado::query {

// Creates a binary node and takes ownership of its operands. May throw
// std::bad_alloc.
// Создаёт бинарный узел и принимает владение операндами. Возможен std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_binary(
    Ast_Binary_Operator op,
    std::unique_ptr<Ast_Node> left,
    std::unique_ptr<Ast_Node> right) {
    return std::make_unique<Ast_Binary_Operator_Node>(op, std::move(left), std::move(right));
}

// Creates a unary node and takes ownership of its operand. May throw
// std::bad_alloc.
// Создаёт унарный узел и принимает владение операндом. Возможен std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_unary(
    Ast_Unary_Operator op,
    std::unique_ptr<Ast_Node> expr) {
    return std::make_unique<Ast_Unary_Operator_Node>(op, std::move(expr));
}

// Creates a node for a special keyword. May throw std::bad_alloc.
// Создаёт узел специального ключевого слова. Возможен std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_special(Ast_Node_Type type) {
    return std::make_unique<Ast_Special_Node>(type);
}

// Creates a comparison node for a numeric or string value. May throw
// std::bad_alloc.
// Создаёт узел сравнения с числом или строкой. Возможен std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_comparison(
    Ast_Comparison_Field field,
    Ast_Comparison_Operator op,
    Ast_Value value) {
    return std::make_unique<Ast_Comparison_Operator_Node>(field, op, std::move(value));
}

// Reports whether the comparison stores a number. No exceptions.
// Проверяет, хранит ли сравнение число. Исключений нет.
bool Ast_Comparison_Operator_Node::is_number() const {
    return std::holds_alternative<uint16_t>(value);
}

// Reports whether the comparison stores a string. No exceptions.
// Проверяет, хранит ли сравнение строку. Исключений нет.
bool Ast_Comparison_Operator_Node::is_string() const {
    return std::holds_alternative<std::string>(value);
}

// Returns the numeric operand. Throws std::bad_variant_access for a string.
// Возвращает числовой операнд. Для строки выбрасывает std::bad_variant_access.
uint16_t Ast_Comparison_Operator_Node::as_number() const {
    return std::get<uint16_t>(value);
}

// Returns the string operand. Throws std::bad_variant_access for a number.
// Возвращает строковый операнд. Для числа выбрасывает std::bad_variant_access.
const std::string &Ast_Comparison_Operator_Node::as_string() const {
    return std::get<std::string>(value);
}

// Maps the enum to its diagnostic label. Unknown values use a fallback label.
// Возвращает диагностическую метку перечисления, включая запасную метку для неизвестных
// значений.
static const char *comparison_field_to_string(Ast_Comparison_Field field) {
    switch (field) {
    case Ast_Comparison_Field::Priority:
        return "Priority";
    case Ast_Comparison_Field::Tag:
        return "Tag";
    case Ast_Comparison_Field::Status:
        return "Status";
    case Ast_Comparison_Field::Path:
        return "Path";
    case Ast_Comparison_Field::Name:
        return "Name";
    case Ast_Comparison_Field::Time:
        return "Time";
    case Ast_Comparison_Field::Deadline:
        return "Deadline";
    case Ast_Comparison_Field::Mtime:
        return "Mtime";
    case Ast_Comparison_Field::Any:
        return "Any";
    }
    return "Unknown";
}

// Maps the enum to its diagnostic label. Unknown values use a fallback label.
// Возвращает диагностическую метку перечисления, включая запасную метку для неизвестных
// значений.
static const char *comparison_operator_to_string(Ast_Comparison_Operator op) {
    switch (op) {
    case Ast_Comparison_Operator::Gt:
        return "Gt";
    case Ast_Comparison_Operator::Lt:
        return "Lt";
    case Ast_Comparison_Operator::Ge:
        return "Ge";
    case Ast_Comparison_Operator::Le:
        return "Le";
    case Ast_Comparison_Operator::Eq:
        return "Eq";
    case Ast_Comparison_Operator::Ne:
        return "Ne";
    case Ast_Comparison_Operator::Substr:
        return "Substr";
    case Ast_Comparison_Operator::Nsubstr:
        return "Nsubstr";
    case Ast_Comparison_Operator::Fuzzy:
        return "Fuzzy";
    case Ast_Comparison_Operator::Nfuzzy:
        return "Nfuzzy";
    case Ast_Comparison_Operator::Starts:
        return "Starts";
    case Ast_Comparison_Operator::Nstarts:
        return "Nstarts";
    case Ast_Comparison_Operator::Ends:
        return "Ends";
    case Ast_Comparison_Operator::Nends:
        return "Nends";
    case Ast_Comparison_Operator::Glob:
        return "Glob";
    case Ast_Comparison_Operator::Nglob:
        return "Nglob";
    }
    return "Unknown";
}

// Maps the enum to its diagnostic label. Unknown values use a fallback label.
// Возвращает диагностическую метку перечисления, включая запасную метку для неизвестных
// значений.
static const char *binary_operator_to_string(Ast_Binary_Operator op) {
    switch (op) {
    case Ast_Binary_Operator::And:
        return "And";
    case Ast_Binary_Operator::Or:
        return "Or";
    case Ast_Binary_Operator::Xor:
        return "Xor";
    }
    return "Unknown";
}

// Maps the enum to its diagnostic label. Unknown values use a fallback label.
// Возвращает диагностическую метку перечисления, включая запасную метку для неизвестных
// значений.
static const char *unary_operator_to_string(Ast_Unary_Operator op) {
    switch (op) {
    case Ast_Unary_Operator::Not:
        return "Not";
    }
    return "Unknown";
}

// Maps the enum to its diagnostic label. Unknown values use a fallback label.
// Возвращает диагностическую метку перечисления, включая запасную метку для неизвестных
// значений.
static const char *node_type_to_string(Ast_Node_Type type) {
    switch (type) {
    case Ast_Node_Type::Comparison_Operator:
        return "Comparison";
    case Ast_Node_Type::Binary_Operator:
        return "Binary";
    case Ast_Node_Type::Unary_Operator:
        return "Unary";
    case Ast_Node_Type::All:
        return "All";
    case Ast_Node_Type::Untagged:
        return "Untagged";
    case Ast_Node_Type::Unstatused:
        return "Unstatused";
    case Ast_Node_Type::Unnamed:
        return "Unnamed";
    case Ast_Node_Type::Unprioritized:
        return "Unprioritized";
    case Ast_Node_Type::Undeadlined:
        return "Undeadlined";
    }
    return "Unknown";
}

// Prints an AST recursively with indentation. A null node prints nothing.
// Ignores output errors.
// Рекурсивно печатает AST с отступами. Нулевой узел пропускается, ошибки вывода игнорируются.
void ast_print(const Ast_Node *node, size_t depth) {
    if (!node)
        return;

    for (size_t i = 0; i < depth; ++i)
        printf("  ");

    switch (node->type) {
    case Ast_Node_Type::Comparison_Operator: {
        auto *n = static_cast<const Ast_Comparison_Operator_Node *>(node);
        printf("%s: %s %s ",
               node_type_to_string(node->type),
               comparison_field_to_string(n->field),
               comparison_operator_to_string(n->op));

        if (n->is_number()) {
            printf("%d\n", n->as_number());
        } else {
            printf("%s\n", n->as_string().c_str());
        }
        break;
    }
    case Ast_Node_Type::Binary_Operator: {
        auto *n = static_cast<const Ast_Binary_Operator_Node *>(node);
        printf("%s: %s\n",
               node_type_to_string(node->type),
               binary_operator_to_string(n->op));
        ast_print(n->left.get(), depth + 1);
        ast_print(n->right.get(), depth + 1);
        break;
    }
    case Ast_Node_Type::Unary_Operator: {
        auto *n = static_cast<const Ast_Unary_Operator_Node *>(node);
        printf("%s: %s\n",
               node_type_to_string(node->type),
               unary_operator_to_string(n->op));
        ast_print(n->expr.get(), depth + 1);
        break;
    }
    case Ast_Node_Type::All:
    case Ast_Node_Type::Untagged:
    case Ast_Node_Type::Unstatused:
    case Ast_Node_Type::Unnamed:
    case Ast_Node_Type::Unprioritized:
    case Ast_Node_Type::Undeadlined:
        printf("%s\n", node_type_to_string(node->type));
        break;
    }
}

} // namespace mado::query
