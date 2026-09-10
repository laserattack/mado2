#include "parser.hpp"

namespace mado::query {

void Parse_Error::print(const std::string &query, std::ostream &os) const {
    size_t position = token_.position;
    size_t length = token_.value.empty() ? 1 : token_.value.size();

    std::string pos_str = std::to_string(position);
    std::string padding(pos_str.size(), ' ');

    os << pos_str << " | " << query << "\n";
    os << padding << " | " << std::string(position, ' ') << std::string(length, '^') << "\n";
    os << padding << " | " << what() << "\n";
}

std::string Parser::token_repr(const Token &token) const {
    if (token.value.empty()) {
        return token_type_to_string(token.type);
    }
    return token_type_to_string(token.type) + ": " + token.value;
}

bool Parser::is_at_end() const {
    return get_it().type == Token_Type::End;
}

Token Parser::get_it(size_t n) const {
    if (current_ + n >= tokens_.size()) {
        return Token{Token_Type::End, "", tokens_.back().position};
    }
    return tokens_[current_ + n];
}

Token Parser::eat_it() {
    Token token = get_it();

    if (!is_at_end()) {
        current_++;
    }

    return token;
}

void Parser::eat_it(size_t n) {
    current_ = std::min(current_ + n, tokens_.size());
}

Token Parser::expect(Token_Type type, const std::string &error_msg) {
    if (get_it().type != type) {
        throw Parse_Error(error_msg + ", got " + token_repr(get_it()), get_it());
    }
    return eat_it();
}

std::unique_ptr<Ast_Node> Parser::parse() {
    if (is_at_end()) {
        throw Parse_Error("Empty query", get_it());
    }

    auto ast = parse_expression();

    if (!is_at_end()) {
        throw Parse_Error("Expected binary operator or end of query, got " +
                              token_repr(get_it()),
                          get_it());
    }

    return ast;
}

std::unique_ptr<Ast_Node> Parser::parse_expression() {
    return parse_or();
}

std::unique_ptr<Ast_Node> Parser::parse_or() {
    auto left = parse_xor();

    while (get_it().type == Token_Type::Or) {
        eat_it();
        auto right = parse_xor();
        left = ast_make_binary(Ast_Binary_Operator::Or,
                               std::move(left), std::move(right));
    }

    return left;
}

std::unique_ptr<Ast_Node> Parser::parse_xor() {
    auto left = parse_and();

    while (get_it().type == Token_Type::Xor) {
        eat_it();
        auto right = parse_and();
        left = ast_make_binary(Ast_Binary_Operator::Xor,
                               std::move(left), std::move(right));
    }

    return left;
}

std::unique_ptr<Ast_Node> Parser::parse_and() {
    auto left = parse_not();

    while (get_it().type == Token_Type::And) {
        eat_it();
        auto right = parse_not();
        left = ast_make_binary(Ast_Binary_Operator::And,
                               std::move(left), std::move(right));
    }

    return left;
}

std::unique_ptr<Ast_Node> Parser::parse_not() {
    if (get_it().type == Token_Type::Not) {
        eat_it();
        auto expr = parse_not();
        return ast_make_unary(Ast_Unary_Operator::Not, std::move(expr));
    }

    return parse_primary();
}

std::unique_ptr<Ast_Node> Parser::parse_primary() {
    // Parentheses
    if (get_it().type == Token_Type::Lparen) {
        eat_it();
        auto expr = parse_expression();
        expect(Token_Type::Rparen, "Expected ')'");
        return expr;
    }

    // Special expressions
    switch (get_it().type) {
    case Token_Type::All:
        eat_it();
        return ast_make_special(Ast_Node_Type::All);
    case Token_Type::Untagged:
        eat_it();
        return ast_make_special(Ast_Node_Type::Untagged);
    case Token_Type::Unstatused:
        eat_it();
        return ast_make_special(Ast_Node_Type::Unstatused);
    case Token_Type::Unnamed:
        eat_it();
        return ast_make_special(Ast_Node_Type::Unnamed);
    case Token_Type::Unprioritized:
        eat_it();
        return ast_make_special(Ast_Node_Type::Unprioritized);
    case Token_Type::Undeadlined:
        eat_it();
        return ast_make_special(Ast_Node_Type::Undeadlined);
    default:
        break;
    }

    return parse_condition();
}

std::unique_ptr<Ast_Node> Parser::parse_condition() {
    auto field_token = eat_it();

    Ast_Comparison_Field field;
    switch (field_token.type) {
    case Token_Type::Priority:
        field = Ast_Comparison_Field::Priority;
        break;
    case Token_Type::Tag:
        field = Ast_Comparison_Field::Tag;
        break;
    case Token_Type::Status:
        field = Ast_Comparison_Field::Status;
        break;
    case Token_Type::Path:
        field = Ast_Comparison_Field::Path;
        break;
    case Token_Type::Name:
        field = Ast_Comparison_Field::Name;
        break;
    case Token_Type::Time:
        field = Ast_Comparison_Field::Time;
        break;
    case Token_Type::Deadline:
        field = Ast_Comparison_Field::Deadline;
        break;
    case Token_Type::Mtime:
        field = Ast_Comparison_Field::Mtime;
        break;
    case Token_Type::Any:
        field = Ast_Comparison_Field::Any;
        break;
    default:
        throw Parse_Error("Expected field name or special keyword, got " + token_repr(field_token),
                          field_token);
    }

    switch (field) {
    case Ast_Comparison_Field::Priority:
        return parse_number_condition(field);
    case Ast_Comparison_Field::Time:
    case Ast_Comparison_Field::Deadline:
    case Ast_Comparison_Field::Mtime:
        return parse_time_condition(field);
    case Ast_Comparison_Field::Any:
        return parse_any_condition();
    default:
        return parse_string_condition(field);
    }
}

std::unique_ptr<Ast_Node> Parser::parse_number_condition(Ast_Comparison_Field field) {
    auto op = parse_comparison_operator();
    auto value_token = expect(Token_Type::Number, "Expected number value");
    return ast_make_comparison(field, op, std::stoi(value_token.value));
}

std::unique_ptr<Ast_Node> Parser::parse_string_condition(Ast_Comparison_Field field) {
    auto op = parse_comparison_operator();

    auto token = eat_it();

    if (token.type == Token_Type::String || token_is_keyword(token.type)) {
        return ast_make_comparison(field, op, token.value);
    }

    throw Parse_Error("Expected string value, got " + token_repr(token),
                      token);
}

std::unique_ptr<Ast_Node> Parser::parse_time_condition(Ast_Comparison_Field field) {
    auto op = parse_comparison_operator();
    auto value_token = expect(Token_Type::Timestamp, "Expected timestamp value");
    return ast_make_comparison(field, op, value_token.value);
}

std::unique_ptr<Ast_Node> Parser::parse_any_condition() {
    auto op = parse_comparison_operator();

    auto value_token = eat_it();

    switch (value_token.type) {
    case Token_Type::Number:
        return ast_make_comparison(Ast_Comparison_Field::Any, op, std::stoi(value_token.value));
    case Token_Type::String:
    case Token_Type::Timestamp:
        return ast_make_comparison(Ast_Comparison_Field::Any, op, value_token.value);
    default:
        if (token_is_keyword(value_token.type)) {
            return ast_make_comparison(Ast_Comparison_Field::Any, op, value_token.value);
        }
        throw Parse_Error("Expected value, got " + token_repr(value_token),
                          value_token);
    }
}

Ast_Comparison_Operator Parser::parse_comparison_operator() {
    auto token = eat_it();

    switch (token.type) {
    case Token_Type::Gt:
        return Ast_Comparison_Operator::Gt;
    case Token_Type::Lt:
        return Ast_Comparison_Operator::Lt;
    case Token_Type::Ge:
        return Ast_Comparison_Operator::Ge;
    case Token_Type::Le:
        return Ast_Comparison_Operator::Le;
    case Token_Type::Eq:
        return Ast_Comparison_Operator::Eq;
    case Token_Type::Ne:
        return Ast_Comparison_Operator::Ne;
    case Token_Type::Substr:
        return Ast_Comparison_Operator::Substr;
    case Token_Type::Nsubstr:
        return Ast_Comparison_Operator::Nsubstr;
    case Token_Type::Fuzzy:
        return Ast_Comparison_Operator::Fuzzy;
    case Token_Type::Nfuzzy:
        return Ast_Comparison_Operator::Nfuzzy;
    case Token_Type::Starts:
        return Ast_Comparison_Operator::Starts;
    case Token_Type::Nstarts:
        return Ast_Comparison_Operator::Nstarts;
    case Token_Type::Ends:
        return Ast_Comparison_Operator::Ends;
    case Token_Type::Nends:
        return Ast_Comparison_Operator::Nends;
    case Token_Type::Glob:
        return Ast_Comparison_Operator::Glob;
    case Token_Type::Nglob:
        return Ast_Comparison_Operator::Nglob;
    default:
        throw Parse_Error("Expected comparison operator, got " + token_repr(token),
                          token);
    }
}

} // namespace mado::query
