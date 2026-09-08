#include "../../common/test_utils.hpp"
#include "ast.hpp"
#include "lexer.hpp"
#include "token.hpp"

using namespace mado::interpreter;
using namespace mado::common;

// token

static void test_token_creation() {
    Token t{Token_Type::Number, "42", 0};

    test(t.type == Token_Type::Number &&
         t.value == "42" &&
         t.position == 0);
}

static void test_token_creation_default() {
    Token t;

    test(t.type == Token_Type::Invalid &&
         t.value.empty() &&
         t.position == 0);
}

static void test_token_to_string() {
    test(token_to_string(Token_Type::Number) == "Number" &&
         token_to_string(Token_Type::Gt) == "Gt" &&
         token_to_string(Token_Type::Lparen) == "Lparen" &&
         token_to_string(Token_Type::And) == "And");
}

// lexer

static void test_lexer_empty_query() {
    Lexer lexer("");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 1 &&
         tokens[0].type == Token_Type::End);
}

static void test_lexer_numbers() {
    Lexer lexer("42 999 0");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 4 &&
         tokens[0].type == Token_Type::Number &&
         tokens[0].value == "42" &&
         tokens[1].type == Token_Type::Number &&
         tokens[1].value == "999" &&
         tokens[2].type == Token_Type::Number &&
         tokens[2].value == "0" &&
         tokens[3].type == Token_Type::End);
}

static void test_lexer_keywords() {
    Lexer lexer("priority and tag");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 4 &&
         tokens[0].type == Token_Type::Priority &&
         tokens[0].value == "priority" &&
         tokens[1].type == Token_Type::And &&
         tokens[1].value == "and" &&
         tokens[2].type == Token_Type::Tag &&
         tokens[2].value == "tag" &&
         tokens[3].type == Token_Type::End);
}

static void test_lexer_strings() {
    Lexer lexer("\"hello\" 'world'");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 3 &&
         tokens[0].type == Token_Type::String &&
         tokens[0].value == "hello" &&
         tokens[1].type == Token_Type::String &&
         tokens[1].value == "world" &&
         tokens[2].type == Token_Type::End);
}

static void test_lexer_operators() {
    Lexer lexer("> >= < <= = != ~ !~ ~~ !~~");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 11 &&
         tokens[0].type == Token_Type::Gt &&
         tokens[1].type == Token_Type::Ge &&
         tokens[2].type == Token_Type::Lt &&
         tokens[3].type == Token_Type::Le &&
         tokens[4].type == Token_Type::Eq &&
         tokens[5].type == Token_Type::Ne &&
         tokens[6].type == Token_Type::Substr &&
         tokens[7].type == Token_Type::Nsubstr &&
         tokens[8].type == Token_Type::Fuzzy &&
         tokens[9].type == Token_Type::Nfuzzy &&
         tokens[10].type == Token_Type::End);
}

static void test_lexer_punctuation() {
    Lexer lexer("( ) , [ ] ..");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 7 &&
         tokens[0].type == Token_Type::Lparen &&
         tokens[1].type == Token_Type::Rparen &&
         tokens[2].type == Token_Type::Comma &&
         tokens[3].type == Token_Type::Lbracket &&
         tokens[4].type == Token_Type::Rbracket &&
         tokens[5].type == Token_Type::DotDot &&
         tokens[6].type == Token_Type::End);
}

static void test_lexer_query() {
    Lexer lexer("priority > 5 and tag = bug");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 8 &&
         tokens[0].type == Token_Type::Priority &&
         tokens[1].type == Token_Type::Gt &&
         tokens[2].type == Token_Type::Number &&
         tokens[2].value == "5" &&
         tokens[3].type == Token_Type::And &&
         tokens[4].type == Token_Type::Tag &&
         tokens[5].type == Token_Type::Eq &&
         tokens[6].type == Token_Type::String &&
         tokens[6].value == "bug" &&
         tokens[7].type == Token_Type::End);
}

static void test_lexer_whitespace() {
    Lexer lexer("    priority\t   >\n  5  ");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 4 &&
         tokens[0].type == Token_Type::Priority &&
         tokens[1].type == Token_Type::Gt &&
         tokens[2].type == Token_Type::Number &&
         tokens[3].type == Token_Type::End);
}

static void test_lexer_position() {
    Lexer lexer("priority > 5");
    auto tokens = lexer.tokenize();

    test(tokens[0].position == 0 &&
         tokens[1].position == 9 &&
         tokens[2].position == 11);
}

static void test_lexer_invalid_operators() {
    Lexer lexer("^ $ % f g . * ! !~&");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 11 &&
         tokens[0].type == Token_Type::Invalid &&
         tokens[0].value == "^" &&
         tokens[1].type == Token_Type::Invalid &&
         tokens[1].value == "$" &&
         tokens[2].type == Token_Type::Invalid &&
         tokens[2].value == "%" &&
         tokens[3].type == Token_Type::String &&
         tokens[3].value == "f" &&
         tokens[4].type == Token_Type::String &&
         tokens[4].value == "g" &&
         tokens[5].type == Token_Type::Invalid &&
         tokens[5].value == "." &&
         tokens[6].type == Token_Type::Invalid &&
         tokens[6].value == "*" &&
         tokens[7].type == Token_Type::Invalid &&
         tokens[7].value == "!" &&
         tokens[8].type == Token_Type::Nsubstr &&
         tokens[8].value == "!~" &&
         tokens[9].type == Token_Type::Invalid &&
         tokens[9].value == "&" &&
         tokens[10].type == Token_Type::End);
}

static void test_lexer_unterminated_string() {
    Lexer lexer("\"hello");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Invalid &&
         tokens[0].value == "hello" &&
         tokens[1].type == Token_Type::End);
}

// ast

static void test_ast_comparison_node() {
    auto node = ast_make_comparison(
        Ast_Comparison_Field::Priority,
        Ast_Comparison_Operator::Gt,
        5);

    auto *comp = static_cast<Ast_Comparison_Operator_Node *>(node.get());

    test(node->type == Ast_Node_Type::Comparison_Op &&
         comp->field == Ast_Comparison_Field::Priority &&
         comp->op == Ast_Comparison_Operator::Gt &&
         comp->is_number() &&
         comp->as_number() == 5);
}

static void test_ast_comparison_node_string() {
    auto node = ast_make_comparison(
        Ast_Comparison_Field::Tag,
        Ast_Comparison_Operator::Eq,
        std::string("bug"));

    auto *comp = static_cast<Ast_Comparison_Operator_Node *>(node.get());

    test(comp->is_string() &&
         comp->as_string() == "bug");
}

static void test_ast_binary_node() {
    auto left = ast_make_comparison(
        Ast_Comparison_Field::Priority,
        Ast_Comparison_Operator::Gt,
        5);

    auto right = ast_make_comparison(
        Ast_Comparison_Field::Tag,
        Ast_Comparison_Operator::Eq,
        std::string("bug"));

    auto node = ast_make_binary(
        Ast_Binary_Operator::And,
        std::move(left),
        std::move(right));

    auto *bin = static_cast<Ast_Binary_Operator_Node *>(node.get());

    test(node->type == Ast_Node_Type::Binary_Op &&
         bin->op == Ast_Binary_Operator::And &&
         bin->left != nullptr &&
         bin->right != nullptr &&
         bin->left->type == Ast_Node_Type::Comparison_Op &&
         bin->right->type == Ast_Node_Type::Comparison_Op);
}

static void test_ast_unary_node() {
    auto expr = ast_make_comparison(
        Ast_Comparison_Field::Status,
        Ast_Comparison_Operator::Eq,
        std::string("opened"));

    auto node = ast_make_unary(
        Ast_Unary_Operator::Not,
        std::move(expr));

    auto *un = static_cast<Ast_Unary_Operator_Node *>(node.get());

    test(node->type == Ast_Node_Type::Unary_Op &&
         un->op == Ast_Unary_Operator::Not &&
         un->expr != nullptr &&
         un->expr->type == Ast_Node_Type::Comparison_Op);
}

static void test_ast_special_nodes() {
    auto all_node = ast_make_special(Ast_Node_Type::All);
    auto untagged_node = ast_make_special(Ast_Node_Type::Untagged);
    auto unnamed_node = ast_make_special(Ast_Node_Type::Unnamed);

    test(all_node->type == Ast_Node_Type::All &&
         untagged_node->type == Ast_Node_Type::Untagged &&
         unnamed_node->type == Ast_Node_Type::Unnamed);
}

// entry point

int main() {
    std::vector<Test_Case> tests = {
        // token
        {"Token creation", "create token with values", test_token_creation},
        {"Token default", "create default token", test_token_creation_default},
        {"Token to_string", "convert type to string", test_token_to_string},
        // lexer
        {"Lexer empty", "tokenize empty query", test_lexer_empty_query},
        {"Lexer numbers", "tokenize numbers", test_lexer_numbers},
        {"Lexer keywords", "tokenize keywords", test_lexer_keywords},
        {"Lexer strings", "tokenize quoted strings", test_lexer_strings},
        {"Lexer operators", "tokenize operators", test_lexer_operators},
        {"Lexer punctuation", "tokenize punctuation", test_lexer_punctuation},
        {"Lexer query", "tokenize query", test_lexer_query},
        {"Lexer whitespace", "tokenize with whitespace", test_lexer_whitespace},
        {"Lexer position", "token positions", test_lexer_position},
        {"Lexer invalid operator", "tokenize invalid operators", test_lexer_invalid_operators},
        {"Lexer unterminated string", "tokenize unterminated string", test_lexer_unterminated_string},
        // ast
        {"AST comparison", "create comparison node", test_ast_comparison_node},
        {"AST comparison string", "create comparison node with string", test_ast_comparison_node_string},
        {"AST binary", "create binary node", test_ast_binary_node},
        {"AST unary", "create unary node", test_ast_unary_node},
        {"AST special", "create special nodes", test_ast_special_nodes},
    };

    return run_tests(tests);
}
