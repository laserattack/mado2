#pragma once

#include <memory>
#include <string>
#include <variant>

namespace mado::query {

// Identifies the concrete AST node kind.
enum class Ast_Node_Type {
    Comparison_Operator, // field op value
    Binary_Operator,     // expr op expr
    Unary_Operator,      // op expr
    All,                 // all
    Untagged,            // untagged
    Unstatused,          // unstatused
    Unnamed,             // unnamed
    Unprioritized,       // unprioritized
    Undeadlined,         // undeadlined
};

// Identifies an entry field available to query comparisons.
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

// Identifies the comparison operation applied to an entry value.
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
enum class Ast_Binary_Operator {
    And,
    Or,
    Xor,
};

// Identifies a unary logical operation.
enum class Ast_Unary_Operator {
    Not,
};

// Base node tagged with its concrete AST kind.
struct Ast_Node {
    Ast_Node_Type type;
    // Destroys a derived node through its base pointer. No exceptions.
    virtual ~Ast_Node() = default;

  protected:
    // Tags the node with its concrete kind. No exceptions.
    Ast_Node(Ast_Node_Type t) : type(t) {}
};

// number in range [0,999]
using Ast_Value = std::variant<uint16_t, std::string>;

// Stores a field comparison and its numeric or string operand.
struct Ast_Comparison_Operator_Node : Ast_Node {
    Ast_Comparison_Field field;
    Ast_Comparison_Operator op;
    Ast_Value value;

    // Stores the field, operation, and value. No exceptions while moving the value.
    Ast_Comparison_Operator_Node(Ast_Comparison_Field f,
                                 Ast_Comparison_Operator o,
                                 Ast_Value v)
        : Ast_Node(Ast_Node_Type::Comparison_Operator),
          field(f), op(o), value(std::move(v)) {}

    // Reports whether the comparison stores a number. No exceptions.
    bool is_number() const;

    // Reports whether the comparison stores a string. No exceptions.
    bool is_string() const;

    // Returns the numeric operand. Throws std::bad_variant_access for a
    // string.
    uint16_t as_number() const;

    // Returns the string operand. Throws std::bad_variant_access for a number.
    const std::string &as_string() const;
};

// Owns the operands and operation of a binary expression.
struct Ast_Binary_Operator_Node : Ast_Node {
    Ast_Binary_Operator op;
    std::unique_ptr<Ast_Node> left;
    std::unique_ptr<Ast_Node> right;

    // Takes ownership of both operands. No exceptions.
    Ast_Binary_Operator_Node(Ast_Binary_Operator o,
                             std::unique_ptr<Ast_Node> l,
                             std::unique_ptr<Ast_Node> r)
        : Ast_Node(Ast_Node_Type::Binary_Operator),
          op(o), left(std::move(l)), right(std::move(r)) {}
};

// Owns the operand and operation of a unary expression.
struct Ast_Unary_Operator_Node : Ast_Node {
    Ast_Unary_Operator op;
    std::unique_ptr<Ast_Node> expr;

    // Takes ownership of the operand. No exceptions.
    Ast_Unary_Operator_Node(Ast_Unary_Operator o,
                            std::unique_ptr<Ast_Node> e)
        : Ast_Node(Ast_Node_Type::Unary_Operator),
          op(o), expr(std::move(e)) {}
};

// Represents a special keyword with no operands.
struct Ast_Special_Node : Ast_Node {
    // Stores the special keyword kind. No exceptions.
    Ast_Special_Node(Ast_Node_Type t) : Ast_Node(t) {}
};

// Creates a binary node and takes ownership of its operands. May throw
// std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_binary(
    Ast_Binary_Operator op,
    std::unique_ptr<Ast_Node> left,
    std::unique_ptr<Ast_Node> right);

// Creates a unary node and takes ownership of its operand. May throw
// std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_unary(
    Ast_Unary_Operator op,
    std::unique_ptr<Ast_Node> expr);

// Creates a node for a special keyword. May throw std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_special(Ast_Node_Type type);

// Creates a comparison node for a numeric or string value. May throw
// std::bad_alloc.
std::unique_ptr<Ast_Node> ast_make_comparison(
    Ast_Comparison_Field field,
    Ast_Comparison_Operator op,
    Ast_Value value);

// Prints an AST recursively with indentation. A null node prints nothing.
// Ignores output errors.
void ast_print(const Ast_Node *node, size_t depth = 0);

} // namespace mado::query
