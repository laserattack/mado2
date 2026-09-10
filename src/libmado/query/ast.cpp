#include "ast.hpp"

#include <cstdio>

namespace mado::query {

bool Ast_Comparison_Operator_Node::is_number() const {
    return std::holds_alternative<int>(value);
}

bool Ast_Comparison_Operator_Node::is_string() const {
    return std::holds_alternative<std::string>(value);
}

int Ast_Comparison_Operator_Node::as_number() const {
    return std::get<int>(value);
}

const std::string &Ast_Comparison_Operator_Node::as_string() const {
    return std::get<std::string>(value);
}

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

static const char *unary_operator_to_string(Ast_Unary_Operator op) {
    switch (op) {
    case Ast_Unary_Operator::Not:
        return "Not";
    }
    return "Unknown";
}

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

void ast_print(const Ast_Node *node, int depth) {
    if (!node)
        return;

    for (int i = 0; i < depth; ++i)
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
