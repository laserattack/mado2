#include <iostream>
#include <string>

#include "../../common/test_utils.hpp"
#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "token.hpp"

using namespace mado::query;
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

static void test_token_type_to_string() {
    test(token_type_to_string(Token_Type::Number) == "Number" &&
         token_type_to_string(Token_Type::Gt) == "Gt" &&
         token_type_to_string(Token_Type::Lparen) == "Lparen" &&
         token_type_to_string(Token_Type::And) == "And");
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
         tokens[0].value == "" &&
         tokens[1].type == Token_Type::Invalid &&
         tokens[1].value == "" &&
         tokens[2].type == Token_Type::Invalid &&
         tokens[2].value == "" &&
         tokens[3].type == Token_Type::String &&
         tokens[3].value == "f" &&
         tokens[4].type == Token_Type::String &&
         tokens[4].value == "g" &&
         tokens[5].type == Token_Type::Invalid &&
         tokens[5].value == "" &&
         tokens[6].type == Token_Type::Invalid &&
         tokens[6].value == "" &&
         tokens[7].type == Token_Type::Invalid &&
         tokens[7].value == "" &&
         tokens[8].type == Token_Type::Nsubstr &&
         tokens[8].value == "!~" &&
         tokens[9].type == Token_Type::Invalid &&
         tokens[9].value == "" &&
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

    test(node->type == Ast_Node_Type::Comparison_Operator &&
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

    test(node->type == Ast_Node_Type::Binary_Operator &&
         bin->op == Ast_Binary_Operator::And &&
         bin->left != nullptr &&
         bin->right != nullptr &&
         bin->left->type == Ast_Node_Type::Comparison_Operator &&
         bin->right->type == Ast_Node_Type::Comparison_Operator);
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

    test(node->type == Ast_Node_Type::Unary_Operator &&
         un->op == Ast_Unary_Operator::Not &&
         un->expr != nullptr &&
         un->expr->type == Ast_Node_Type::Comparison_Operator);
}

static void test_ast_special_nodes() {
    auto all_node = ast_make_special(Ast_Node_Type::All);
    auto untagged_node = ast_make_special(Ast_Node_Type::Untagged);
    auto unnamed_node = ast_make_special(Ast_Node_Type::Unnamed);

    test(all_node->type == Ast_Node_Type::All &&
         untagged_node->type == Ast_Node_Type::Untagged &&
         unnamed_node->type == Ast_Node_Type::Unnamed);
}

// parser

static void test_parser_number_condition() {
    Lexer lexer("priority > 5");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *comp = static_cast<Ast_Comparison_Operator_Node *>(ast.get());
    test(ast->type == Ast_Node_Type::Comparison_Operator &&
         comp->field == Ast_Comparison_Field::Priority &&
         comp->op == Ast_Comparison_Operator::Gt &&
         comp->is_number() &&
         comp->as_number() == 5);
}

static void test_parser_string_condition() {
    Lexer lexer("tag = bug");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *comp = static_cast<Ast_Comparison_Operator_Node *>(ast.get());
    test(comp->field == Ast_Comparison_Field::Tag &&
         comp->op == Ast_Comparison_Operator::Eq &&
         comp->is_string() &&
         comp->as_string() == "bug");
}

static void test_parser_keyword_as_string_value() {
    Lexer lexer("tag = all and status = and and name = or and any = not");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *inner1 = static_cast<Ast_Binary_Operator_Node *>(root->left.get());
    auto *inner2 = static_cast<Ast_Binary_Operator_Node *>(inner1->left.get());
    auto *left = static_cast<Ast_Comparison_Operator_Node *>(inner2->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(inner2->right.get());
    auto *name = static_cast<Ast_Comparison_Operator_Node *>(inner1->right.get());
    auto *any = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(ast->type == Ast_Node_Type::Binary_Operator &&
         root->op == Ast_Binary_Operator::And &&

         left->field == Ast_Comparison_Field::Tag &&
         left->op == Ast_Comparison_Operator::Eq &&
         left->is_string() &&
         left->as_string() == "all" &&

         right->field == Ast_Comparison_Field::Status &&
         right->op == Ast_Comparison_Operator::Eq &&
         right->is_string() &&
         right->as_string() == "and" &&

         name->field == Ast_Comparison_Field::Name &&
         name->op == Ast_Comparison_Operator::Eq &&
         name->is_string() &&
         name->as_string() == "or" &&

         any->field == Ast_Comparison_Field::Any &&
         any->op == Ast_Comparison_Operator::Eq &&
         any->is_string() &&
         any->as_string() == "not");
}

static void test_parser_binary_and() {
    Lexer lexer("priority > 5 and tag = bug");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *bin = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Comparison_Operator_Node *>(bin->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(bin->right.get());

    test(ast->type == Ast_Node_Type::Binary_Operator &&
         bin->op == Ast_Binary_Operator::And &&

         left->type == Ast_Node_Type::Comparison_Operator &&
         left->field == Ast_Comparison_Field::Priority &&
         left->op == Ast_Comparison_Operator::Gt &&
         left->is_number() &&
         left->as_number() == 5 &&

         right->type == Ast_Node_Type::Comparison_Operator &&
         right->field == Ast_Comparison_Field::Tag &&
         right->op == Ast_Comparison_Operator::Eq &&
         right->is_string() &&
         right->as_string() == "bug");
}

static void test_parser_not() {
    Lexer lexer("not status = closed");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *un = static_cast<Ast_Unary_Operator_Node *>(ast.get());
    test(ast->type == Ast_Node_Type::Unary_Operator &&
         un->op == Ast_Unary_Operator::Not &&
         un->expr->type == Ast_Node_Type::Comparison_Operator);
}

static void test_parser_parentheses() {
    Lexer lexer("(priority > 5 or priority < 2) and tag = bug");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Binary_Operator_Node *>(root->left.get());
    auto *ll = static_cast<Ast_Comparison_Operator_Node *>(left->left.get());
    auto *lr = static_cast<Ast_Comparison_Operator_Node *>(left->right.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(ast->type == Ast_Node_Type::Binary_Operator &&
         root->op == Ast_Binary_Operator::And &&

         left->type == Ast_Node_Type::Binary_Operator &&
         left->op == Ast_Binary_Operator::Or &&

         ll->type == Ast_Node_Type::Comparison_Operator &&
         ll->field == Ast_Comparison_Field::Priority &&
         ll->op == Ast_Comparison_Operator::Gt &&
         ll->is_number() &&
         ll->as_number() == 5 &&

         lr->type == Ast_Node_Type::Comparison_Operator &&
         lr->field == Ast_Comparison_Field::Priority &&
         lr->op == Ast_Comparison_Operator::Lt &&
         lr->is_number() &&
         lr->as_number() == 2 &&

         right->type == Ast_Node_Type::Comparison_Operator &&
         right->field == Ast_Comparison_Field::Tag &&
         right->op == Ast_Comparison_Operator::Eq &&
         right->is_string() &&
         right->as_string() == "bug");
}

static void test_parser_special() {
    Lexer lexer("all");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    test(ast->type == Ast_Node_Type::All);
}

static void test_parser_priority() {
    /*
        Or
       /  \
    not    Xor
     |    /   \
    not  b    And
     |       /   \
     a      c    not
                  |
                  d
    */
    Lexer lexer("not not priority > 1 or priority > 2 xor priority > 3 and not priority > 4");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());

    auto *outer_not = static_cast<Ast_Unary_Operator_Node *>(root->left.get());
    auto *inner_not = static_cast<Ast_Unary_Operator_Node *>(outer_not->expr.get());
    auto *a = static_cast<Ast_Comparison_Operator_Node *>(inner_not->expr.get());

    auto *or_right = static_cast<Ast_Binary_Operator_Node *>(root->right.get());
    auto *b = static_cast<Ast_Comparison_Operator_Node *>(or_right->left.get());

    auto *xor_right = static_cast<Ast_Binary_Operator_Node *>(or_right->right.get());
    auto *c = static_cast<Ast_Comparison_Operator_Node *>(xor_right->left.get());
    auto *right_not = static_cast<Ast_Unary_Operator_Node *>(xor_right->right.get());
    auto *d = static_cast<Ast_Comparison_Operator_Node *>(right_not->expr.get());

    test(ast->type == Ast_Node_Type::Binary_Operator &&
         root->op == Ast_Binary_Operator::Or &&

         outer_not->type == Ast_Node_Type::Unary_Operator &&
         outer_not->op == Ast_Unary_Operator::Not &&
         inner_not->type == Ast_Node_Type::Unary_Operator &&
         inner_not->op == Ast_Unary_Operator::Not &&

         a->type == Ast_Node_Type::Comparison_Operator &&
         a->field == Ast_Comparison_Field::Priority &&
         a->op == Ast_Comparison_Operator::Gt &&
         a->is_number() &&
         a->as_number() == 1 &&

         or_right->type == Ast_Node_Type::Binary_Operator &&
         or_right->op == Ast_Binary_Operator::Xor &&

         b->type == Ast_Node_Type::Comparison_Operator &&
         b->field == Ast_Comparison_Field::Priority &&
         b->op == Ast_Comparison_Operator::Gt &&
         b->is_number() &&
         b->as_number() == 2 &&

         xor_right->type == Ast_Node_Type::Binary_Operator &&
         xor_right->op == Ast_Binary_Operator::And &&

         c->type == Ast_Node_Type::Comparison_Operator &&
         c->field == Ast_Comparison_Field::Priority &&
         c->op == Ast_Comparison_Operator::Gt &&
         c->is_number() &&
         c->as_number() == 3 &&

         right_not->type == Ast_Node_Type::Unary_Operator &&
         right_not->op == Ast_Unary_Operator::Not &&

         d->type == Ast_Node_Type::Comparison_Operator &&
         d->field == Ast_Comparison_Field::Priority &&
         d->op == Ast_Comparison_Operator::Gt &&
         d->is_number() &&
         d->as_number() == 4);
}

static void test_parser_empty_throws() {
    Lexer lexer("");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    bool thrown = false;
    try {
        parser.parse();
    } catch (const Parse_Error &) {
        thrown = true;
    }
    test(thrown);
}

static void test_parser_unexpected_token_throws() {
    Lexer lexer("priority > ");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    bool thrown = false;
    try {
        parser.parse();
    } catch (const Parse_Error &) {
        thrown = true;
    }
    test(thrown);
}

// entry point

int main(int argc, char **argv) {

    if (argc >= 2 && std::string(argv[1]) == "--query") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " --query <query>" << std::endl;
            return 1;
        }

        std::string query = argv[2];

        Lexer lexer(query);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));

        try {
            auto ast = parser.parse();
            ast_print(ast.get());
        } catch (const Parse_Error &e) {
            std::cerr << e.format(query);
            return 1;
        }

        return 0;
    }

    std::vector<Test_Case> tests = {
        // token
        {"Token creation", "create token with values", test_token_creation},
        {"Token default", "create default token", test_token_creation_default},
        {"Token to_string", "convert type to string", test_token_type_to_string},
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
        // parser
        {"Parser number", "parse 'priority > 5'", test_parser_number_condition},
        {"Parser string", "parse 'tag = bug'", test_parser_string_condition},
        {"Parser keyword as string", "parse 'tag = all and status = and and name = or and any = not'", test_parser_keyword_as_string_value},
        {"Parser and", "parse 'priority > 5 and tag = bug'", test_parser_binary_and},
        {"Parser not", "parse 'not status = closed'", test_parser_not},
        {"Parser parentheses", "parse '(priority > 5 or priority < 2) and tag = bug'", test_parser_parentheses},
        {"Parser special", "parse 'all'", test_parser_special},
        {"Parser empty", "empty query throws", test_parser_empty_throws},
        {"Parser unexpected", "unexpected token throws", test_parser_unexpected_token_throws},
        {"Parser priority", "parse 'not not priority > 1 or priority > 2 xor priority > 3 and not priority > 4'", test_parser_priority},
    };

    return run_tests(tests);
}
