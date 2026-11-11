#pragma once

#include <memory>
#include <string>
#include <variant>

namespace mado::query {

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

/// Returns the enum name for `field`, or "Unknown" for an unrecognized value.
const char *comparison_field_to_string(Ast_Comparison_Field field);

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

enum class Ast_Binary_Operator {
    And,
    Or,
    Xor,
};

enum class Ast_Unary_Operator {
    Not,
};

struct Ast_Node {
    Ast_Node_Type type;
    /// Destroys the node through its base type.
    virtual ~Ast_Node() = default;

  protected:
    Ast_Node(Ast_Node_Type t) : type(t) {}
};

// number in range [0,999]
using Ast_Value = std::variant<uint16_t, std::string, std::monostate>;

struct Ast_Comparison_Operator_Node : Ast_Node {
    Ast_Comparison_Field field;
    Ast_Comparison_Operator op;
    Ast_Value value;

    /// Creates a comparison node and takes ownership of its value.
    Ast_Comparison_Operator_Node(Ast_Comparison_Field f,
                                 Ast_Comparison_Operator o,
                                 Ast_Value v)
        : Ast_Node(Ast_Node_Type::Comparison_Operator),
          field(f), op(o), value(std::move(v)) {}

    /// Returns whether `value` holds a uint16_t.
    bool is_number() const;

    /// Returns whether `value` holds a string.
    bool is_string() const;

    /// @throws std::bad_variant_access If the value does not hold a number.
    uint16_t as_number() const;

    /// @throws std::bad_variant_access If the value does not hold a string.
    const std::string &as_string() const;
};

struct Ast_Binary_Operator_Node : Ast_Node {
    Ast_Binary_Operator op;
    std::unique_ptr<Ast_Node> left;
    std::unique_ptr<Ast_Node> right;

    /// Creates a binary operator node and takes ownership of both operands.
    Ast_Binary_Operator_Node(Ast_Binary_Operator o,
                             std::unique_ptr<Ast_Node> l,
                             std::unique_ptr<Ast_Node> r)
        : Ast_Node(Ast_Node_Type::Binary_Operator),
          op(o), left(std::move(l)), right(std::move(r)) {}
};

struct Ast_Unary_Operator_Node : Ast_Node {
    Ast_Unary_Operator op;
    std::unique_ptr<Ast_Node> expr;

    /// Creates a unary operator node and takes ownership of its operand.
    Ast_Unary_Operator_Node(Ast_Unary_Operator o,
                            std::unique_ptr<Ast_Node> e)
        : Ast_Node(Ast_Node_Type::Unary_Operator),
          op(o), expr(std::move(e)) {}
};

struct Ast_Special_Node : Ast_Node {
    /// Creates a special-expression node with the supplied type.
    Ast_Special_Node(Ast_Node_Type t) : Ast_Node(t) {}
};

/// Constructs a binary AST node and transfers ownership of both operands.
std::unique_ptr<Ast_Node> ast_make_binary(
    Ast_Binary_Operator op,
    std::unique_ptr<Ast_Node> left,
    std::unique_ptr<Ast_Node> right);

/// Constructs a unary AST node and transfers ownership of its operand.
std::unique_ptr<Ast_Node> ast_make_unary(
    Ast_Unary_Operator op,
    std::unique_ptr<Ast_Node> expr);

/// Constructs a node for a special expression such as `all` or `untagged`.
std::unique_ptr<Ast_Node> ast_make_special(Ast_Node_Type type);

/// Constructs a comparison AST node with the supplied field, operator, and value.
std::unique_ptr<Ast_Node> ast_make_comparison(
    Ast_Comparison_Field field,
    Ast_Comparison_Operator op,
    Ast_Value value);

/// Prints the AST to standard output, indenting descendants by `depth` levels.
/// Does nothing when `node` is null.
void ast_print(const Ast_Node *node, size_t depth = 0);

} // namespace mado::query
