#include <mado/interpreter/interpreter.hpp>

#include <mado/common/fuzzy_match.hpp>
#include <mado/common/glob_pattern.hpp>
#include <mado/common/text_utils.hpp>

namespace mado::interpreter {

namespace {

// Applies a case-insensitive string comparison. An invalid glob returns false
// for either glob operator. May throw std::bad_alloc.
// Сравнивает строки без учёта регистра. Некорректный glob даёт false для обоих
// glob-операторов. Возможен std::bad_alloc.
bool check_string(const std::string &value,
                  const std::string &pattern,
                  mado::query::Ast_Comparison_Operator op) {

    const std::string value_lower = mado::common::utf8_tolower(value);
    const std::string pattern_lower = mado::common::utf8_tolower(pattern);

    // Dispatch on the operator after applying case folding.
    // Выбираем оператор после приведения регистра.
    switch (op) {
    case mado::query::Ast_Comparison_Operator::Eq: // = / Распознаём оператор или разделитель `=`.
        return value_lower == pattern_lower;
    case mado::query::Ast_Comparison_Operator::Ne: // != / Распознаём оператор или разделитель `!=`.
        return value_lower != pattern_lower;

    case mado::query::Ast_Comparison_Operator::Substr: // ~ / Распознаём оператор или разделитель `~`.
        return value_lower.find(pattern_lower) != std::string::npos;
    case mado::query::Ast_Comparison_Operator::Nsubstr: // !~ / Распознаём оператор или разделитель `!~`.
        return value_lower.find(pattern_lower) == std::string::npos;

    case mado::query::Ast_Comparison_Operator::Starts: // ^~ / Распознаём оператор или разделитель `^~`.
        return value_lower.starts_with(pattern_lower);
    case mado::query::Ast_Comparison_Operator::Nstarts: // !^~ / Распознаём оператор или разделитель `!^~`.
        return !value_lower.starts_with(pattern_lower);

    case mado::query::Ast_Comparison_Operator::Ends: // $~ / Распознаём оператор или разделитель `$~`.
        return value_lower.ends_with(pattern_lower);
    case mado::query::Ast_Comparison_Operator::Nends: // !$~ / Распознаём оператор или разделитель `!$~`.
        return !value_lower.ends_with(pattern_lower);

    case mado::query::Ast_Comparison_Operator::Fuzzy: // ~~ / Распознаём оператор или разделитель `~~`.
        return mado::common::fuzzy_match(pattern_lower, value_lower, false).has_value();
    case mado::query::Ast_Comparison_Operator::Nfuzzy: // !~~ / Распознаём оператор или разделитель `!~~`.
        return !mado::common::fuzzy_match(pattern_lower, value_lower, false).has_value();

    case mado::query::Ast_Comparison_Operator::Gt: // > / Распознаём оператор или разделитель `>`.
        return value_lower > pattern_lower;
    case mado::query::Ast_Comparison_Operator::Lt: // < / Распознаём оператор или разделитель `<`.
        return value_lower < pattern_lower;

    case mado::query::Ast_Comparison_Operator::Ge: // >= / Распознаём оператор или разделитель `>=`.
        return value_lower >= pattern_lower;
    case mado::query::Ast_Comparison_Operator::Le: // <= / Распознаём оператор или разделитель `<=`.
        return value_lower <= pattern_lower;

    case mado::query::Ast_Comparison_Operator::Glob: // %~ / Распознаём оператор или разделитель `%~`.
        return mado::common::Glob_Pattern(pattern_lower.c_str()).matches(value_lower) == true;
    case mado::query::Ast_Comparison_Operator::Nglob: // !%~ / Распознаём оператор или разделитель `!%~`.
        return mado::common::Glob_Pattern(pattern_lower.c_str()).matches(value_lower) == false;
    }
    return false;
}

// Applies a comparison to numeric operands, using equality for text operators.
// No exceptions.
// Сравнивает числа, сводя текстовые операторы к равенству или неравенству. Исключений нет.
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

// Tests the filter against an entry. A null filter matches every entry. String
// comparison may throw std::bad_alloc.
// Проверяет запись по фильтру. Нулевой фильтр пропускает всё. Сравнение строк может вызвать
// std::bad_alloc.
bool Interpreter::evaluate(const mado::query::Ast_Node *node,
                           const mado::entry::Entry &entry) const {
    if (!node)
        return true;

    // Evaluate the AST node according to its concrete kind.
    // Вычисляем узел AST по его конкретному типу.
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

        // Try every compatible field until one matches.
        // Проверяем совместимые поля до первого совпадения.
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
