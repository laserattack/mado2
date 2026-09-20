#include <mado/common/fuzzy_match.hpp>
#include <mado/common/text_utils.hpp>
#include <mado/interpreter/entry.hpp>

namespace mado::interpreter {

namespace {

using namespace mado::query;

bool check_string(const std::string &value, const std::string &pattern,
                  Ast_Comparison_Operator op) {
    switch (op) {
    case Ast_Comparison_Operator::Eq:
        return value == pattern;
    case Ast_Comparison_Operator::Ne:
        return value != pattern;
    case Ast_Comparison_Operator::Substr:
        return value.find(pattern) != std::string::npos;
    case Ast_Comparison_Operator::Nsubstr:
        return value.find(pattern) == std::string::npos;
    case Ast_Comparison_Operator::Starts:
        return value.rfind(pattern, 0) == 0;
    case Ast_Comparison_Operator::Nstarts:
        return value.rfind(pattern, 0) != 0;
    case Ast_Comparison_Operator::Ends:
        return value.size() >= pattern.size() &&
               value.compare(value.size() - pattern.size(), pattern.size(), pattern) == 0;
    case Ast_Comparison_Operator::Nends:
        return !(value.size() >= pattern.size() &&
                 value.compare(value.size() - pattern.size(), pattern.size(), pattern) == 0);
    case Ast_Comparison_Operator::Fuzzy:
        return mado::common::fuzzy_match(pattern, value, true).has_value();
    case Ast_Comparison_Operator::Nfuzzy:
        return !mado::common::fuzzy_match(pattern, value, true).has_value();
    case Ast_Comparison_Operator::Gt:
        return value > pattern;
    case Ast_Comparison_Operator::Lt:
        return value < pattern;
    case Ast_Comparison_Operator::Ge:
        return value >= pattern;
    case Ast_Comparison_Operator::Le:
        return value <= pattern;
    case Ast_Comparison_Operator::Glob:
    case Ast_Comparison_Operator::Nglob:
        // TODO: glob matching
        return false;
    }
    return false;
}

bool check_number(uint16_t value, uint16_t pattern, Ast_Comparison_Operator op) {
    switch (op) {
    case Ast_Comparison_Operator::Gt:
        return value > pattern;
    case Ast_Comparison_Operator::Lt:
        return value < pattern;
    case Ast_Comparison_Operator::Ge:
        return value >= pattern;
    case Ast_Comparison_Operator::Le:
        return value <= pattern;
    case Ast_Comparison_Operator::Eq:
    case Ast_Comparison_Operator::Fuzzy:
    case Ast_Comparison_Operator::Substr:
    case Ast_Comparison_Operator::Starts:
    case Ast_Comparison_Operator::Ends:
    case Ast_Comparison_Operator::Glob:
        return value == pattern;
    case Ast_Comparison_Operator::Ne:
    case Ast_Comparison_Operator::Nfuzzy:
    case Ast_Comparison_Operator::Nsubstr:
    case Ast_Comparison_Operator::Nstarts:
    case Ast_Comparison_Operator::Nends:
    case Ast_Comparison_Operator::Nglob:
        return value != pattern;
    }
    return false;
}

} // namespace

void Entry::set_path(const std::filesystem::path &p) {
    path_ = p;
}

void Entry::set_name(const std::string &n) {
    name_ = n;
}

void Entry::set_status(const std::string &s) {
    status_ = s;
}

void Entry::set_priority(uint16_t p) {
    if (p > 999)
        throw Entry_Error("Priority must be in [0, 999], got " + std::to_string(p));
    priority_ = p;
}

// {""} represents "no tags"; an empty vector is invalid.
void Entry::set_tags(const std::vector<std::string> &t) {
    if (t.empty())
        throw Entry_Error("Tags must not be empty");
    tags_ = t;
}

void Entry::set_time(const std::string &t) {
    if (!mado::common::is_timestamp(t))
        throw Entry_Error("Invalid timestamp: " + t);
    time_ = t;
}

void Entry::set_mtime(const std::string &t) {
    if (!mado::common::is_timestamp(t))
        throw Entry_Error("Invalid timestamp: " + t);
    mtime_ = t;
}

void Entry::set_deadline(const std::string &t) {
    if (!mado::common::is_timestamp(t))
        throw Entry_Error("Invalid timestamp: " + t);
    deadline_ = t;
}

bool Entry::match_filter(const Ast_Node *filter) const {
    if (!filter)
        return true;

    switch (filter->type) {
    case Ast_Node_Type::All:
        return true;

    case Ast_Node_Type::Untagged:
        return tags_.size() == 1 && tags_[0].empty();

    case Ast_Node_Type::Unstatused:
        return status_.empty();

    case Ast_Node_Type::Unnamed:
        return name_.empty();

    case Ast_Node_Type::Unprioritized:
        return priority_ == 0;

    case Ast_Node_Type::Undeadlined:
        return deadline_ == "99990101-000000";

    case Ast_Node_Type::Binary_Operator: {
        auto *n = static_cast<const Ast_Binary_Operator_Node *>(filter);
        switch (n->op) {
        case Ast_Binary_Operator::And:
            return match_filter(n->left.get()) && match_filter(n->right.get());
        case Ast_Binary_Operator::Or:
            return match_filter(n->left.get()) || match_filter(n->right.get());
        case Ast_Binary_Operator::Xor:
            return match_filter(n->left.get()) != match_filter(n->right.get());
        }
        return false;
    }

    case Ast_Node_Type::Unary_Operator: {
        auto *n = static_cast<const Ast_Unary_Operator_Node *>(filter);
        if (n->op == Ast_Unary_Operator::Not)
            return !match_filter(n->expr.get());
        return false;
    }

    case Ast_Node_Type::Comparison_Operator: {
        auto *n = static_cast<const Ast_Comparison_Operator_Node *>(filter);

        switch (n->field) {
        case Ast_Comparison_Field::Priority:
            if (!n->is_number())
                return false;
            return check_number(priority_, n->as_number(), n->op);

        case Ast_Comparison_Field::Tag: {
            if (!n->is_string())
                return false;
            const std::string &pat = n->as_string();
            for (const auto &tag : tags_) {
                if (check_string(tag, pat, n->op))
                    return true;
            }
            return false;
        }

        case Ast_Comparison_Field::Status:
            if (!n->is_string())
                return false;
            return check_string(status_, n->as_string(), n->op);

        case Ast_Comparison_Field::Name:
            if (!n->is_string())
                return false;
            return check_string(name_, n->as_string(), n->op);

        case Ast_Comparison_Field::Path:
            if (!n->is_string())
                return false;
            return check_string(path_.string(), n->as_string(), n->op);

        case Ast_Comparison_Field::Time:
            if (!n->is_string())
                return false;
            return check_string(time_, n->as_string(), n->op);

        case Ast_Comparison_Field::Deadline:
            if (!n->is_string())
                return false;
            return check_string(deadline_, n->as_string(), n->op);

        case Ast_Comparison_Field::Mtime:
            if (!n->is_string())
                return false;
            return check_string(mtime_, n->as_string(), n->op);

        case Ast_Comparison_Field::Any: {
            const std::string path_str = path_.string();

            auto match_string_fields = [&](const std::string &pat) -> bool {
                if (check_string(name_, pat, n->op))
                    return true;
                if (check_string(status_, pat, n->op))
                    return true;
                if (check_string(path_str, pat, n->op))
                    return true;
                if (check_string(time_, pat, n->op))
                    return true;
                if (check_string(mtime_, pat, n->op))
                    return true;
                if (check_string(deadline_, pat, n->op))
                    return true;
                for (const auto &tag : tags_) {
                    if (check_string(tag, pat, n->op))
                        return true;
                }
                return false;
            };

            if (n->is_number()) {
                uint16_t num = n->as_number();
                if (check_number(priority_, num, n->op))
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
