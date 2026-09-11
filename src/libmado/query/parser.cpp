#include "parser.hpp"

namespace mado::query {

std::string Parse_Error::format(const std::string &query) const {
    size_t position = token_.position;

    std::string pos_str = std::to_string(position);
    std::string padding(pos_str.size(), ' ');

    return pos_str + " | " + query + "\n" +
           padding + " | " + std::string(position, ' ') + "^\n" +
           padding + " | " + what() + "\n";
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

std::unique_ptr<Ast_Node> Parser::parse() {
    if (is_at_end()) {
        throw Parse_Error("Empty query", get_it());
    }

    auto ast = parse_expression();

    if (!is_at_end()) {
        throw Parse_Error("Expected 'and', 'or', 'xor' or end of query, got " + token_repr(get_it()), get_it());
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

        auto token = eat_it();

        if (token.type != Token_Type::Rparen) {
            throw Parse_Error("Expected ')', got " + token_repr(token), token);
        }

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

    return parse_field();
}

std::unique_ptr<Ast_Node> Parser::parse_field() {
    auto token = eat_it();

    switch (token.type) {
    case Token_Type::Priority:
        return parse_comparison(Ast_Comparison_Field::Priority);
    case Token_Type::Tag:
        return parse_comparison(Ast_Comparison_Field::Tag);
    case Token_Type::Status:
        return parse_comparison(Ast_Comparison_Field::Status);
    case Token_Type::Path:
        return parse_comparison(Ast_Comparison_Field::Path);
    case Token_Type::Name:
        return parse_comparison(Ast_Comparison_Field::Name);
    case Token_Type::Time:
        return parse_comparison(Ast_Comparison_Field::Time);
    case Token_Type::Deadline:
        return parse_comparison(Ast_Comparison_Field::Deadline);
    case Token_Type::Mtime:
        return parse_comparison(Ast_Comparison_Field::Mtime);
    case Token_Type::Any:
        return parse_comparison(Ast_Comparison_Field::Any);
    default:
        throw Parse_Error("Expected field name or special keyword, got " + token_repr(token), token);
    }
}

std::unique_ptr<Ast_Node> Parser::parse_comparison(Ast_Comparison_Field field) {
    auto op = parse_comparison_operator();

    // anyof / allof
    if (get_it().type == Token_Type::Allof || get_it().type == Token_Type::Anyof) {
        bool is_allof = get_it().type == Token_Type::Allof;
        eat_it();
        return parse_list(field, op, is_allof);
    }

    return parse_value(field, op);
}

std::unique_ptr<Ast_Node> Parser::parse_list(
    Ast_Comparison_Field field,
    Ast_Comparison_Operator op,
    bool is_allof) {

    auto lparen = eat_it(); // (
    if (lparen.type != Token_Type::Lparen) {
        throw Parse_Error("Expected '(', got " + token_repr(lparen), lparen);
    }

    if (get_it().type == Token_Type::Rparen) {
        auto rparen = eat_it(); // )
        throw Parse_Error("List cannot be empty, expected value, got " + token_repr(rparen), rparen);
    }

    auto combine = is_allof ? Ast_Binary_Operator::And : Ast_Binary_Operator::Or;
    std::unique_ptr<Ast_Node> result;

    while (true) {
        auto cmp = parse_value(field, op);

        result = result
                     ? ast_make_binary(combine, std::move(result), std::move(cmp))
                     : std::move(cmp);

        auto next = eat_it(); // , or )
        if (next.type == Token_Type::Comma) {
            continue;
        }
        if (next.type == Token_Type::Rparen) {
            break;
        }
        throw Parse_Error("Expected ',' or ')', got " + token_repr(next), next);
    }

    return result;
}

std::unique_ptr<Ast_Node> Parser::parse_value(
    Ast_Comparison_Field field,
    Ast_Comparison_Operator op) {

    auto token = eat_it(); // value

    switch (field) {
    case Ast_Comparison_Field::Priority:
        if (token.type != Token_Type::Number) {
            throw Parse_Error("Expected Number, got " + token_repr(token), token);
        }
        // Lexer guarantees 1-3 digit positive numbers, so stoi can't overflow
        return ast_make_comparison(field, op, static_cast<uint16_t>(std::stoi(token.value)));

    case Ast_Comparison_Field::Tag:
    case Ast_Comparison_Field::Status:
    case Ast_Comparison_Field::Path:
    case Ast_Comparison_Field::Name:
        if (token.type == Token_Type::String || token_is_keyword(token.type)) {
            return ast_make_comparison(field, op, token.value);
        }
        throw Parse_Error("Expected String, got " + token_repr(token), token);

    case Ast_Comparison_Field::Time:
    case Ast_Comparison_Field::Deadline:
    case Ast_Comparison_Field::Mtime:
        if (token.type != Token_Type::Timestamp) {
            throw Parse_Error("Expected Timestamp, got " + token_repr(token), token);
        }
        return ast_make_comparison(field, op, token.value);

    case Ast_Comparison_Field::Any:
        switch (token.type) {
        case Token_Type::Number:
            return ast_make_comparison(field, op, static_cast<uint16_t>(std::stoi(token.value)));
        case Token_Type::String:
        case Token_Type::Timestamp:
            return ast_make_comparison(field, op, token.value);
        default:
            if (token_is_keyword(token.type)) {
                return ast_make_comparison(field, op, token.value);
            }
            throw Parse_Error("Expected Number, String, Timestamp, got " + token_repr(token), token);
        }
    }

    throw Parse_Error("Unknown field", token);
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
        throw Parse_Error("Expected comparison operator, got " + token_repr(token), token);
    }
}

} // namespace mado::query
