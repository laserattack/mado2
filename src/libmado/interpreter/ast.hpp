#pragma once

#include <memory>
#include <string>
#include <variant>

namespace mado::interpreter {

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
    virtual ~Ast_Node() = default;

  protected:
    Ast_Node(Ast_Node_Type t) : type(t) {}
};

// TODO: not int. number in range [0,999]
using Ast_Value = std::variant<int, std::string>;

struct Ast_Comparison_Operator_Node : Ast_Node {
    Ast_Comparison_Field field;
    Ast_Comparison_Operator op;
    Ast_Value value;

    Ast_Comparison_Operator_Node(Ast_Comparison_Field f,
                                 Ast_Comparison_Operator o,
                                 Ast_Value v)
        : Ast_Node(Ast_Node_Type::Comparison_Operator),
          field(f), op(o), value(std::move(v)) {}

    bool is_number() const;

    bool is_string() const;

    int as_number() const;

    const std::string &as_string() const;
};

struct Ast_Binary_Operator_Node : Ast_Node {
    Ast_Binary_Operator op;
    std::unique_ptr<Ast_Node> left;
    std::unique_ptr<Ast_Node> right;

    Ast_Binary_Operator_Node(Ast_Binary_Operator o,
                             std::unique_ptr<Ast_Node> l,
                             std::unique_ptr<Ast_Node> r)
        : Ast_Node(Ast_Node_Type::Binary_Operator),
          op(o), left(std::move(l)), right(std::move(r)) {}
};

struct Ast_Unary_Operator_Node : Ast_Node {
    Ast_Unary_Operator op;
    std::unique_ptr<Ast_Node> expr;

    Ast_Unary_Operator_Node(Ast_Unary_Operator o,
                            std::unique_ptr<Ast_Node> e)
        : Ast_Node(Ast_Node_Type::Unary_Operator),
          op(o), expr(std::move(e)) {}
};

struct Ast_Special_Node : Ast_Node {
    Ast_Special_Node(Ast_Node_Type t) : Ast_Node(t) {}
};

inline std::unique_ptr<Ast_Node> ast_make_binary(
    Ast_Binary_Operator op,
    std::unique_ptr<Ast_Node> left,
    std::unique_ptr<Ast_Node> right) {
    return std::make_unique<Ast_Binary_Operator_Node>(op, std::move(left), std::move(right));
}

inline std::unique_ptr<Ast_Node> ast_make_unary(
    Ast_Unary_Operator op,
    std::unique_ptr<Ast_Node> expr) {
    return std::make_unique<Ast_Unary_Operator_Node>(op, std::move(expr));
}

inline std::unique_ptr<Ast_Node> ast_make_special(Ast_Node_Type type) {
    return std::make_unique<Ast_Special_Node>(type);
}

inline std::unique_ptr<Ast_Node> ast_make_comparison(
    Ast_Comparison_Field field,
    Ast_Comparison_Operator op,
    Ast_Value value) {
    return std::make_unique<Ast_Comparison_Operator_Node>(field, op, std::move(value));
}

void ast_print(const Ast_Node *node, int depth = 0);

} // namespace mado::interpreter
