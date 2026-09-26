#include <mado/interpreter/interpreter.hpp>

#include <mado/common/fuzzy_match.hpp>

namespace mado::interpreter {

namespace {

bool check_string(const std::string &value,
                  const std::string &pattern,
                  mado::query::Ast_Comparison_Operator op) {
    switch (op) {
    case mado::query::Ast_Comparison_Operator::Eq:
        return value == pattern;
    case mado::query::Ast_Comparison_Operator::Ne:
        return value != pattern;
    case mado::query::Ast_Comparison_Operator::Substr:
        return value.find(pattern) != std::string::npos;
    case mado::query::Ast_Comparison_Operator::Nsubstr:
        return value.find(pattern) == std::string::npos;
    case mado::query::Ast_Comparison_Operator::Starts:
        return value.rfind(pattern, 0) == 0;
    case mado::query::Ast_Comparison_Operator::Nstarts:
        return value.rfind(pattern, 0) != 0;
    case mado::query::Ast_Comparison_Operator::Ends:
        return value.size() >= pattern.size() &&
               value.compare(value.size() - pattern.size(),
                             pattern.size(), pattern) == 0;
    case mado::query::Ast_Comparison_Operator::Nends:
        return !(value.size() >= pattern.size() &&
                 value.compare(value.size() - pattern.size(),
                               pattern.size(), pattern) == 0);
    case mado::query::Ast_Comparison_Operator::Fuzzy:
        return mado::common::fuzzy_match(pattern, value, true).has_value();
    case mado::query::Ast_Comparison_Operator::Nfuzzy:
        return !mado::common::fuzzy_match(pattern, value, true).has_value();
    case mado::query::Ast_Comparison_Operator::Gt:
        return value > pattern;
    case mado::query::Ast_Comparison_Operator::Lt:
        return value < pattern;
    case mado::query::Ast_Comparison_Operator::Ge:
        return value >= pattern;
    case mado::query::Ast_Comparison_Operator::Le:
        return value <= pattern;
    case mado::query::Ast_Comparison_Operator::Glob:
    case mado::query::Ast_Comparison_Operator::Nglob:
        // TODO: glob matching
        return false;
    }
    return false;
}

bool check_number(uint16_t value,
                  uint16_t pattern,
                  mado::query::Ast_Comparison_Operator op) {
    switch (op) {
    case mado::query::Ast_Comparison_Operator::Gt:
        return value > pattern;
    case mado::query::Ast_Comparison_Operator::Lt:
        return value < pattern;
    case mado::query::Ast_Comparison_Operator::Ge:
        return value >= pattern;
    case mado::query::Ast_Comparison_Operator::Le:
        return value <= pattern;
    case mado::query::Ast_Comparison_Operator::Eq:
    case mado::query::Ast_Comparison_Operator::Fuzzy:
    case mado::query::Ast_Comparison_Operator::Substr:
    case mado::query::Ast_Comparison_Operator::Starts:
    case mado::query::Ast_Comparison_Operator::Ends:
    case mado::query::Ast_Comparison_Operator::Glob:
        return value == pattern;
    case mado::query::Ast_Comparison_Operator::Ne:
    case mado::query::Ast_Comparison_Operator::Nfuzzy:
    case mado::query::Ast_Comparison_Operator::Nsubstr:
    case mado::query::Ast_Comparison_Operator::Nstarts:
    case mado::query::Ast_Comparison_Operator::Nends:
    case mado::query::Ast_Comparison_Operator::Nglob:
        return value != pattern;
    }
    return false;
}

} // namespace

bool Interpreter::evaluate(const mado::query::Ast_Node *node,
                           const mado::entry::Entry &entry) const {
    if (!node)
        return true;

    switch (node->type) {
    case mado::query::Ast_Node_Type::All:
        return true;

    case mado::query::Ast_Node_Type::Untagged:
        return entry.tags().size() == 1 && entry.tags()[0].empty();

    case mado::query::Ast_Node_Type::Unstatused:
        return entry.status().empty();

    case mado::query::Ast_Node_Type::Unnamed:
        return entry.name().empty();

    case mado::query::Ast_Node_Type::Unprioritized:
        return entry.priority() == 0;

    case mado::query::Ast_Node_Type::Undeadlined:
        return entry.deadline() == "99990101-000000";

    case mado::query::Ast_Node_Type::Binary_Operator: {
        auto *n = static_cast<const mado::query::Ast_Binary_Operator_Node *>(node);
        switch (n->op) {
        case mado::query::Ast_Binary_Operator::And:
            return evaluate(n->left.get(), entry) &&
                   evaluate(n->right.get(), entry);
        case mado::query::Ast_Binary_Operator::Or:
            return evaluate(n->left.get(), entry) ||
                   evaluate(n->right.get(), entry);
        case mado::query::Ast_Binary_Operator::Xor:
            return evaluate(n->left.get(), entry) !=
                   evaluate(n->right.get(), entry);
        }
        return false;
    }

    case mado::query::Ast_Node_Type::Unary_Operator: {
        auto *n = static_cast<const mado::query::Ast_Unary_Operator_Node *>(node);
        if (n->op == mado::query::Ast_Unary_Operator::Not)
            return !evaluate(n->expr.get(), entry);
        return false;
    }

    case mado::query::Ast_Node_Type::Comparison_Operator: {
        auto *n = static_cast<const mado::query::Ast_Comparison_Operator_Node *>(node);

        switch (n->field) {
        case mado::query::Ast_Comparison_Field::Priority:
            if (!n->is_number())
                return false;
            return check_number(entry.priority(), n->as_number(), n->op);

        case mado::query::Ast_Comparison_Field::Tag: {
            if (!n->is_string())
                return false;
            const std::string &pat = n->as_string();
            for (const auto &tag : entry.tags()) {
                if (check_string(tag, pat, n->op))
                    return true;
            }
            return false;
        }

        case mado::query::Ast_Comparison_Field::Status:
            if (!n->is_string())
                return false;
            return check_string(entry.status(), n->as_string(), n->op);

        case mado::query::Ast_Comparison_Field::Name:
            if (!n->is_string())
                return false;
            return check_string(entry.name(), n->as_string(), n->op);

        case mado::query::Ast_Comparison_Field::Path:
            if (!n->is_string())
                return false;
            return check_string(entry.path().string(), n->as_string(), n->op);

        case mado::query::Ast_Comparison_Field::Time:
            if (!n->is_string())
                return false;
            return check_string(entry.time(), n->as_string(), n->op);

        case mado::query::Ast_Comparison_Field::Deadline:
            if (!n->is_string())
                return false;
            return check_string(entry.deadline(), n->as_string(), n->op);

        case mado::query::Ast_Comparison_Field::Mtime:
            if (!n->is_string())
                return false;
            return check_string(entry.mtime(), n->as_string(), n->op);

        case mado::query::Ast_Comparison_Field::Any: {
            const std::string path_str = entry.path().string();

            auto match_string_fields = [&](const std::string &pat) -> bool {
                if (check_string(entry.name(), pat, n->op))
                    return true;
                if (check_string(entry.status(), pat, n->op))
                    return true;
                if (check_string(path_str, pat, n->op))
                    return true;
                if (check_string(entry.time(), pat, n->op))
                    return true;
                if (check_string(entry.mtime(), pat, n->op))
                    return true;
                if (check_string(entry.deadline(), pat, n->op))
                    return true;
                for (const auto &tag : entry.tags()) {
                    if (check_string(tag, pat, n->op))
                        return true;
                }
                return false;
            };

            if (n->is_number()) {
                uint16_t num = n->as_number();
                if (check_number(entry.priority(), num, n->op))
                    return true;
                if (match_string_fields(std::to_string(num)))
                    return true;
            } else {
                if (match_string_fields(n->as_string()))
                    return true;
            }
            return false;
        }
        }
        return false;
    }
    }

    return false;
}

} // namespace mado::interpreter
