#include <mado/query/parser.hpp>

namespace mado::query {

namespace {

std::string token_repr(const Token &token) {
    if (token.value.empty()) {
        return token_type_to_string(token.type);
    }
    return token_type_to_string(token.type) + ": " + token.value;
}

} // namespace

std::string Parse_Error::format(const std::string &query) const {
    size_t position = token_.position;

    std::string pos_str = std::to_string(position);
    std::string padding(pos_str.size(), ' ');

    return pos_str + " | " + query + "\n" +
           padding + " | " + std::string(position, ' ') + "^\n" +
           padding + " | " + what() + "\n";
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

bool Parser::value_matches_field(Ast_Comparison_Field field, const Token &val) const {
    switch (field) {
    case Ast_Comparison_Field::Priority:
        return val.type == Token_Type::Number;
    case Ast_Comparison_Field::Tag:
    case Ast_Comparison_Field::Status:
    case Ast_Comparison_Field::Path:
    case Ast_Comparison_Field::Name:
        return val.type == Token_Type::String || token_is_keyword(val.type);
    case Ast_Comparison_Field::Time:
    case Ast_Comparison_Field::Deadline:
    case Ast_Comparison_Field::Mtime:
        return val.type == Token_Type::Timestamp;
    case Ast_Comparison_Field::Any:
        return val.type == Token_Type::Number ||
               val.type == Token_Type::String ||
               val.type == Token_Type::Timestamp ||
               token_is_keyword(val.type);
    }
    return false;
}

std::string Parser::value_types_desc(Ast_Comparison_Field field) const {
    switch (field) {
    case Ast_Comparison_Field::Priority:
        return "numeric value";
    case Ast_Comparison_Field::Tag:
    case Ast_Comparison_Field::Status:
    case Ast_Comparison_Field::Path:
    case Ast_Comparison_Field::Name:
        return "string value";
    case Ast_Comparison_Field::Time:
    case Ast_Comparison_Field::Deadline:
    case Ast_Comparison_Field::Mtime:
        return "timestamp value";
    case Ast_Comparison_Field::Any:
        return "value";
    }
    return "value";
}

std::unique_ptr<Ast_Node> Parser::parse() {
    if (is_at_end()) {
        throw Parse_Error("Empty query", get_it());
    }

    auto ast = parse_expression();

    if (!is_at_end()) {
        throw Parse_Error("Expected binary operator, end of query, got " + token_repr(get_it()), get_it());
    }

    return ast;
}

// expr bin_op expr
std::unique_ptr<Ast_Node> Parser::parse_expression() {
    return parse_or();
}

// expr or expr
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

// expr xor expr
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

// expr and expr
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

// not expr
std::unique_ptr<Ast_Node> Parser::parse_not() {
    if (get_it().type == Token_Type::Not) {
        eat_it();
        auto expr = parse_not();
        return ast_make_unary(Ast_Unary_Operator::Not, std::move(expr));
    }

    return parse_primary();
}

std::unique_ptr<Ast_Node> Parser::parse_primary() {
    // (expr)
    if (get_it().type == Token_Type::Lparen) { // (
        eat_it();
        auto expr = parse_expression();

        auto rparen = eat_it(); // )

        if (rparen.type != Token_Type::Rparen) {
            throw Parse_Error("Expected ')', got " + token_repr(rparen), rparen);
        }

        return expr;
    }

    return parse_field_and_special();
}

// expr
std::unique_ptr<Ast_Node> Parser::parse_field_and_special() {
    auto field = eat_it();

    switch (field.type) {

    // expr: special_expr
    case Token_Type::All:
        return ast_make_special(Ast_Node_Type::All);
    case Token_Type::Untagged:
        return ast_make_special(Ast_Node_Type::Untagged);
    case Token_Type::Unstatused:
        return ast_make_special(Ast_Node_Type::Unstatused);
    case Token_Type::Unnamed:
        return ast_make_special(Ast_Node_Type::Unnamed);
    case Token_Type::Unprioritized:
        return ast_make_special(Ast_Node_Type::Unprioritized);
    case Token_Type::Undeadlined:
        return ast_make_special(Ast_Node_Type::Undeadlined);

    // expr: field comp_op value
    // expr: field comp_op anyof(v,v,v,...)
    // expr: field comp_op allof(v,v,v,...)
    // expr: field in (v,v,v,...)
    // expr: field has (v,v,v,...)
    // expr: field in [v..v]
    // expr: field in [v..]
    // expr: field in [..v]
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
        throw Parse_Error("Expected field name, special keyword, got " + token_repr(field), field);
    }
}

std::unique_ptr<Ast_Node> Parser::parse_comparison(Ast_Comparison_Field field) {

    // expr: field in [v..v]
    // expr: field in [v..]
    // expr: field in [..v]
    // expr: field in (v,v,v,...)
    // expr: field has (v,v,v,...)
    if (get_it().type == Token_Type::In || get_it().type == Token_Type::Has) {
        bool combine_and = get_it().type == Token_Type::Has;
        eat_it(); // in / has

        auto next = get_it();

        // After 'in', both '[' (range) and '(' (list) are valid.
        // After 'has', only '(' (list) is valid.
        if (!combine_and && next.type == Token_Type::Lbracket) {
            return parse_range(field);
        }
        if (next.type == Token_Type::Lparen) {
            return parse_list(field, Ast_Comparison_Operator::Eq, combine_and);
        }

        if (combine_and) { // has
            throw Parse_Error("Expected '(', got " + token_repr(next), next);
        } else { // in
            throw Parse_Error("Expected '(', '[', got " + token_repr(next), next);
        }
    }

    auto op = parse_comparison_operator();

    // expr: field comp_op anyof(v,v,v,...)
    // expr: field comp_op allof(v,v,v,...)
    if ((get_it().type == Token_Type::Allof || get_it().type == Token_Type::Anyof) &&
        // sugar only when followed by '(', otherwise they are plain string values
        get_it(1).type == Token_Type::Lparen) {

        bool combine_and = get_it().type == Token_Type::Allof;
        eat_it();
        return parse_list(field, op, combine_and);
    }

    // expr: field comp_op value
    auto tok = get_it();
    if (!value_matches_field(field, tok)) {
        throw Parse_Error("Expected " + value_types_desc(field) + ", got " + token_repr(tok), tok);
    }
    return parse_value(field, op);
}

std::unique_ptr<Ast_Node> Parser::parse_range(Ast_Comparison_Field field) {

    // '[' is guaranteed by the caller
    eat_it(); // [

    std::unique_ptr<Ast_Node> low;
    if (get_it().type != Token_Type::DotDot) {
        auto tok = get_it();
        if (!value_matches_field(field, tok)) {
            throw Parse_Error("Expected " + value_types_desc(field) + ", '..', got " + token_repr(tok), tok);
        }
        low = parse_value(field, Ast_Comparison_Operator::Ge);
    }

    auto dots = eat_it(); // ..
    if (dots.type != Token_Type::DotDot) {
        throw Parse_Error("Expected '..', got " + token_repr(dots), dots);
    }

    std::unique_ptr<Ast_Node> high;
    if (get_it().type != Token_Type::Rbracket) {
        auto tok = get_it();
        if (!value_matches_field(field, tok)) {
            if (low) {
                throw Parse_Error("Expected " + value_types_desc(field) + ", ']', got " + token_repr(tok), tok);
            } else {
                throw Parse_Error("Expected " + value_types_desc(field) + ", got " + token_repr(tok), tok);
            }
        }
        high = parse_value(field, Ast_Comparison_Operator::Le);
    }

    auto rbr = eat_it(); // ]
    if (rbr.type != Token_Type::Rbracket) {
        throw Parse_Error("Expected ']', got " + token_repr(rbr), rbr);
    }

    if (!low && !high) {
        throw Parse_Error("Range must have at least one bound", rbr);
    }

    if (low && high) {
        return ast_make_binary(Ast_Binary_Operator::And, std::move(low), std::move(high));
    }
    return low ? std::move(low) : std::move(high);
}

std::unique_ptr<Ast_Node> Parser::parse_list(
    Ast_Comparison_Field field,
    Ast_Comparison_Operator op,
    bool combine_and) {

    // '(' is guaranteed by the caller
    eat_it(); // (

    if (get_it().type == Token_Type::Rparen) {
        auto rparen = eat_it(); // )
        throw Parse_Error("List cannot be empty, expected value, got " + token_repr(rparen), rparen);
    }

    auto combine = combine_and ? Ast_Binary_Operator::And : Ast_Binary_Operator::Or;
    std::unique_ptr<Ast_Node> result;

    while (true) {
        auto tok = get_it();
        if (!value_matches_field(field, tok)) {
            throw Parse_Error("Expected " + value_types_desc(field) + ", got " + token_repr(tok), tok);
        }
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
        throw Parse_Error("Expected ',', ')', got " + token_repr(next), next);
    }

    return result;
}

std::unique_ptr<Ast_Node> Parser::parse_value(
    Ast_Comparison_Field field,
    Ast_Comparison_Operator op) {

    auto val = eat_it();

    // caller guarantees val matches field (see value_matches_field)
    switch (field) {
    case Ast_Comparison_Field::Priority:
        // Lexer guarantees 1-3 digit positive numbers, so stoi can't overflow
        return ast_make_comparison(field, op, static_cast<uint16_t>(std::stoi(val.value)));

    case Ast_Comparison_Field::Tag:
    case Ast_Comparison_Field::Status:
    case Ast_Comparison_Field::Path:
    case Ast_Comparison_Field::Name:
        return ast_make_comparison(field, op, val.value);

    case Ast_Comparison_Field::Time:
    case Ast_Comparison_Field::Deadline:
    case Ast_Comparison_Field::Mtime:
        return ast_make_comparison(field, op, val.value);

    case Ast_Comparison_Field::Any:
        if (val.type == Token_Type::Number) {
            return ast_make_comparison(field, op, static_cast<uint16_t>(std::stoi(val.value)));
        }
        return ast_make_comparison(field, op, val.value);
    }

    // unreachable
    throw Parse_Error("Unknown field", val);
}

Ast_Comparison_Operator Parser::parse_comparison_operator() {
    auto op = eat_it();

    switch (op.type) {
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
        throw Parse_Error("Expected comparison operator, got " + token_repr(op), op);
    }
}

} // namespace mado::query
