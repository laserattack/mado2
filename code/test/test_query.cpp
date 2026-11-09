#include "test_utils.hpp"

#include <mado/common/time_utils.hpp>
#include <mado/common/fuzzy_match.hpp>
#include <mado/query/ast.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>
#include <mado/query/time_macro.hpp>
#include <mado/query/token.hpp>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace mado::query;

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
         tokens[3].type == Token_Type::Allof && // fuzzy
         tokens[3].value == "f" &&
         tokens[4].type == Token_Type::Tag && // fuzzy
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

static void test_lexer_timestamp_yyyy() {
    Lexer lexer("2026");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Timestamp &&
         tokens[0].value == "2026" &&
         tokens[1].type == Token_Type::End);
}

static void test_lexer_timestamp_yyyymm() {
    Lexer lexer("202609");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Timestamp &&
         tokens[0].value == "202609" &&
         tokens[1].type == Token_Type::End);
}

static void test_lexer_timestamp_yyyymmdd() {
    Lexer lexer("20260911");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Timestamp &&
         tokens[0].value == "20260911" &&
         tokens[1].type == Token_Type::End);
}

static void test_lexer_timestamp_full() {
    Lexer lexer("20260911-123045");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Timestamp &&
         tokens[0].value == "20260911-123045" &&
         tokens[1].type == Token_Type::End);
}

static void test_lexer_timestamp_with_t_only() {
    Lexer lexer("20260911-");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Timestamp &&
         tokens[0].value == "20260911-" &&
         tokens[1].type == Token_Type::End);
}

static void test_lexer_timestamp_invalid_date() {
    Lexer lexer("20260230");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::String &&
         tokens[1].type == Token_Type::End);
}

static void test_lexer_timestamp_invalid_month() {
    Lexer lexer("202613");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::String &&
         tokens[1].type == Token_Type::End);
}

static void test_lexer_timestamp_invalid_length() {
    Lexer lexer("12345");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::String &&
         tokens[1].type == Token_Type::End);
}

static void test_lexer_timestamp_in_query() {
    Lexer lexer("deadline < 20260911-123045");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 4 &&
         tokens[0].type == Token_Type::Deadline &&
         tokens[1].type == Token_Type::Lt &&
         tokens[2].type == Token_Type::Timestamp &&
         tokens[2].value == "20260911-123045" &&
         tokens[3].type == Token_Type::End);
}

static void test_lexer_timestamp_numbers_still_work() {
    Lexer lexer("42 20260911 0");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 4 &&
         tokens[0].type == Token_Type::Number &&
         tokens[0].value == "42" &&
         tokens[1].type == Token_Type::Timestamp &&
         tokens[1].value == "20260911" &&
         tokens[2].type == Token_Type::Number &&
         tokens[2].value == "0" &&
         tokens[3].type == Token_Type::End);
}

// ast

static void test_ast_comparison_node() {
    auto node = ast_make_comparison(
        Ast_Comparison_Field::Priority,
        Ast_Comparison_Operator::Gt,
        uint16_t{5});

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
        uint16_t{5});

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

static void test_parser_anyof_number() {
    Lexer lexer("priority = anyof(1, 2, 3)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *inner = static_cast<Ast_Binary_Operator_Node *>(root->left.get());
    auto *one = static_cast<Ast_Comparison_Operator_Node *>(inner->left.get());
    auto *two = static_cast<Ast_Comparison_Operator_Node *>(inner->right.get());
    auto *three = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(ast->type == Ast_Node_Type::Binary_Operator &&
         root->op == Ast_Binary_Operator::Or &&

         inner->type == Ast_Node_Type::Binary_Operator &&
         inner->op == Ast_Binary_Operator::Or &&

         one->field == Ast_Comparison_Field::Priority &&
         one->op == Ast_Comparison_Operator::Eq &&
         one->is_number() &&
         one->as_number() == 1 &&

         two->field == Ast_Comparison_Field::Priority &&
         two->op == Ast_Comparison_Operator::Eq &&
         two->is_number() &&
         two->as_number() == 2 &&

         three->field == Ast_Comparison_Field::Priority &&
         three->op == Ast_Comparison_Operator::Eq &&
         three->is_number() &&
         three->as_number() == 3);
}

static void test_parser_allof_number() {
    Lexer lexer("priority = allof(1, 2)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Comparison_Operator_Node *>(root->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(root->op == Ast_Binary_Operator::And &&
         left->is_number() && left->as_number() == 1 &&
         right->is_number() && right->as_number() == 2);
}

static void test_parser_anyof_string() {
    Lexer lexer("tag = anyof(bug, crit)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Comparison_Operator_Node *>(root->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(root->op == Ast_Binary_Operator::Or &&
         left->field == Ast_Comparison_Field::Tag &&
         left->is_string() && left->as_string() == "bug" &&
         right->is_string() && right->as_string() == "crit");
}

static void test_parser_allof_string() {
    Lexer lexer("tag = allof(a, b, c)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *inner = static_cast<Ast_Binary_Operator_Node *>(root->left.get());

    test(root->op == Ast_Binary_Operator::And &&
         inner->op == Ast_Binary_Operator::And);
}

static void test_parser_anyof_time() {
    Lexer lexer("deadline = anyof(20260101, 20260202)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Comparison_Operator_Node *>(root->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(root->op == Ast_Binary_Operator::Or &&
         left->field == Ast_Comparison_Field::Deadline &&
         left->is_string() && left->as_string() == "20260101" &&
         right->is_string() && right->as_string() == "20260202");
}

static void test_parser_anyof_any() {
    Lexer lexer("any = anyof(1, foo, 20260101)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *inner = static_cast<Ast_Binary_Operator_Node *>(root->left.get());
    auto *one = static_cast<Ast_Comparison_Operator_Node *>(inner->left.get());
    auto *foo = static_cast<Ast_Comparison_Operator_Node *>(inner->right.get());
    auto *ts = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(root->op == Ast_Binary_Operator::Or &&
         inner->op == Ast_Binary_Operator::Or &&

         one->is_number() && one->as_number() == 1 &&
         foo->is_string() && foo->as_string() == "foo" &&
         ts->is_string() && ts->as_string() == "20260101");
}

static void test_parser_anyof_single_value() {
    Lexer lexer("priority = anyof(5)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *comp = static_cast<Ast_Comparison_Operator_Node *>(ast.get());

    test(ast->type == Ast_Node_Type::Comparison_Operator &&
         comp->field == Ast_Comparison_Field::Priority &&
         comp->op == Ast_Comparison_Operator::Eq &&
         comp->is_number() &&
         comp->as_number() == 5);
}

static void test_parser_anyof_with_operator() {
    Lexer lexer("priority > anyof(1, 2)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Comparison_Operator_Node *>(root->left.get());

    test(root->op == Ast_Binary_Operator::Or &&
         left->op == Ast_Comparison_Operator::Gt &&
         left->is_number() && left->as_number() == 1);
}

static void test_parser_anyof_empty_throws() {
    Lexer lexer("tag = anyof()");
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

static void test_parser_anyof_trailing_comma_throws() {
    Lexer lexer("tag = anyof(a, b,)");
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

static void test_parser_anyof_missing_comma_throws() {
    Lexer lexer("tag = anyof(a b)");
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

static void test_parser_anyof_unclosed_throws() {
    Lexer lexer("tag = anyof(a, b");
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

static void test_parser_anyof_wrong_type_throws() {
    Lexer lexer("priority = anyof(a, b)");
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

static void test_parser_anyof_in_expression() {
    Lexer lexer("tag = anyof(a, b) and priority > 5");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Binary_Operator_Node *>(root->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(root->op == Ast_Binary_Operator::And &&
         left->op == Ast_Binary_Operator::Or &&
         right->field == Ast_Comparison_Field::Priority &&
         right->is_number() && right->as_number() == 5);
}

static void test_parser_in() {
    Lexer lexer("status in (opened, reopened)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Comparison_Operator_Node *>(root->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(root->op == Ast_Binary_Operator::Or &&
         left->field == Ast_Comparison_Field::Status &&
         left->op == Ast_Comparison_Operator::Eq &&
         left->is_string() && left->as_string() == "opened" &&
         right->is_string() && right->as_string() == "reopened");
}

static void test_parser_has() {
    Lexer lexer("tag has (bug, crit)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());

    test(root->op == Ast_Binary_Operator::And);
}

static void test_parser_in_single_value() {
    Lexer lexer("status in (opened)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *comp = static_cast<Ast_Comparison_Operator_Node *>(ast.get());

    test(ast->type == Ast_Node_Type::Comparison_Operator &&
         comp->op == Ast_Comparison_Operator::Eq &&
         comp->is_string() && comp->as_string() == "opened");
}

static void test_parser_range_both() {
    Lexer lexer("priority in [50..100]");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Comparison_Operator_Node *>(root->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(root->op == Ast_Binary_Operator::And &&
         left->field == Ast_Comparison_Field::Priority &&
         left->op == Ast_Comparison_Operator::Ge &&
         left->is_number() && left->as_number() == 50 &&
         right->op == Ast_Comparison_Operator::Le &&
         right->is_number() && right->as_number() == 100);
}

static void test_parser_range_low_only() {
    Lexer lexer("priority in [50..]");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *comp = static_cast<Ast_Comparison_Operator_Node *>(ast.get());

    test(ast->type == Ast_Node_Type::Comparison_Operator &&
         comp->field == Ast_Comparison_Field::Priority &&
         comp->op == Ast_Comparison_Operator::Ge &&
         comp->is_number() && comp->as_number() == 50);
}

static void test_parser_range_high_only() {
    Lexer lexer("priority in [..50]");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *comp = static_cast<Ast_Comparison_Operator_Node *>(ast.get());

    test(ast->type == Ast_Node_Type::Comparison_Operator &&
         comp->field == Ast_Comparison_Field::Priority &&
         comp->op == Ast_Comparison_Operator::Le &&
         comp->is_number() && comp->as_number() == 50);
}

static void test_parser_range_string() {
    Lexer lexer("name in [a..z]");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Comparison_Operator_Node *>(root->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(root->op == Ast_Binary_Operator::And &&
         left->field == Ast_Comparison_Field::Name &&
         left->op == Ast_Comparison_Operator::Ge &&
         left->is_string() && left->as_string() == "a" &&
         right->op == Ast_Comparison_Operator::Le &&
         right->is_string() && right->as_string() == "z");
}

static void test_parser_range_time() {
    Lexer lexer("deadline in [20260101..20260202]");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Comparison_Operator_Node *>(root->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(root->op == Ast_Binary_Operator::And &&
         left->field == Ast_Comparison_Field::Deadline &&
         left->op == Ast_Comparison_Operator::Ge &&
         left->is_string() && left->as_string() == "20260101" &&
         right->op == Ast_Comparison_Operator::Le &&
         right->is_string() && right->as_string() == "20260202");
}

static void test_parser_range_any() {
    Lexer lexer("any in [1..20260101]");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Comparison_Operator_Node *>(root->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(root->op == Ast_Binary_Operator::And &&
         left->op == Ast_Comparison_Operator::Ge &&
         left->is_number() && left->as_number() == 1 &&
         right->op == Ast_Comparison_Operator::Le &&
         right->is_string() && right->as_string() == "20260101");
}

static void test_parser_range_in_expression() {
    Lexer lexer("priority in [50..100] and tag = bug");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());
    auto *left = static_cast<Ast_Binary_Operator_Node *>(root->left.get());
    auto *right = static_cast<Ast_Comparison_Operator_Node *>(root->right.get());

    test(root->op == Ast_Binary_Operator::And &&
         left->op == Ast_Binary_Operator::And &&
         right->field == Ast_Comparison_Field::Tag &&
         right->is_string() && right->as_string() == "bug");
}

static void test_parser_range_empty_throws() {
    Lexer lexer("priority in [..]");
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

static void test_parser_range_missing_dotdot_throws() {
    Lexer lexer("priority in [50]");
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

static void test_parser_range_unclosed_throws() {
    Lexer lexer("priority in [50..100");
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

static void test_parser_range_wrong_type_throws() {
    Lexer lexer("priority in [a..z]");
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

static void test_utc_conversion_failure() {
    bool rejected = false;
    if constexpr (std::numeric_limits<std::time_t>::digits > 32) {
        try {
            mado::common::format_timestamp_utc(std::numeric_limits<std::time_t>::max());
        } catch (const std::runtime_error &) {
            rejected = true;
        }
    }
    test(std::numeric_limits<std::time_t>::digits <= 32 || rejected);
}

static void test_time_macro_values() {
    using namespace std::chrono;
    const auto now = system_clock::to_time_t(sys_days{year{2024}/February/29} +
                                            minutes{34} + seconds{56});

    test(resolve_time_macro("now", 0, now) == "20240229-003456" &&
         resolve_time_macro("now", 1, now) == "20240301-003456" &&
         resolve_time_macro("today", 0, now) == "20240229" &&
         resolve_time_macro("today", -1, now) == "20240228" &&
         resolve_time_macro("yesterday", 0, now) == "20240228" &&
         resolve_time_macro("tomorrow", 0, now) == "20240301" &&
         resolve_time_macro("week", -1, now) == "20240219" &&
         resolve_time_macro("month", -2, now) == "202312" &&
         resolve_time_macro("year", 1, now) == "2025" &&
         resolve_time_macro("never", -1, now) == "99981231-000000" &&
         resolve_time_macro("never", 0, now) == "99990101-000000" &&
         resolve_time_macro("MnTh", 1, now) == "202403");
}

static void test_time_macro_range_tokens() {
    const auto tokens = Lexer("deadline in [@never - 1..@NEVER]").tokenize();
    test(tokens.size() == 8 &&
         tokens[3].type == Token_Type::Timestamp &&
         tokens[3].position == 13 && tokens[3].value == "99981231-000000" &&
         tokens[4].type == Token_Type::DotDot &&
         tokens[5].type == Token_Type::Timestamp &&
         tokens[5].value == "99990101-000000");
}

static void test_invalid_time_macro_tokens() {
    test(Lexer("@zzzz").tokenize().front().type == Token_Type::Invalid &&
         Lexer("@").tokenize().front().type == Token_Type::Invalid &&
         Lexer("@today+").tokenize().front().type == Token_Type::Invalid &&
         Lexer("@today+0000001").tokenize().front().type == Token_Type::Invalid);
}

static void test_time_macro_out_of_range() {
    test(Lexer("@never+365").tokenize().front().type == Token_Type::Invalid &&
         Lexer("@year-999999").tokenize().front().type == Token_Type::Invalid);
}


static bool same_ast(const Ast_Node *left, const Ast_Node *right) {
    if (!left || !right)
        return left == right;
    if (left->type != right->type)
        return false;

    switch (left->type) {
    case Ast_Node_Type::Comparison_Operator: {
        const auto &a = static_cast<const Ast_Comparison_Operator_Node &>(*left);
        const auto &b = static_cast<const Ast_Comparison_Operator_Node &>(*right);
        return a.field == b.field && a.op == b.op && a.value == b.value;
    }
    case Ast_Node_Type::Binary_Operator: {
        const auto &a = static_cast<const Ast_Binary_Operator_Node &>(*left);
        const auto &b = static_cast<const Ast_Binary_Operator_Node &>(*right);
        return a.op == b.op && same_ast(a.left.get(), b.left.get()) &&
               same_ast(a.right.get(), b.right.get());
    }
    case Ast_Node_Type::Unary_Operator: {
        const auto &a = static_cast<const Ast_Unary_Operator_Node &>(*left);
        const auto &b = static_cast<const Ast_Unary_Operator_Node &>(*right);
        return a.op == b.op && same_ast(a.expr.get(), b.expr.get());
    }
    default:
        return true;
    }
}

static std::unique_ptr<Ast_Node> parse_query(const std::string &query) {
    return Parser(Lexer(query).tokenize()).parse();
}

static void test_parser_time_macro_syntax() {
    const std::pair<const char *, const char *> cases[] = {
        {R"q(deadline in [@never-1..@never])q",
         R"q(deadline in [99981231-000000..99990101-000000])q"},
        {R"q(deadline in [@never - 1..])q",
         R"q(deadline in [99981231-000000..])q"},
        {R"q(deadline in [..@never])q",
         R"q(deadline in [..99990101-000000])q"},
        {R"q(deadline >= anyof(@never-1, @never))q",
         R"q(deadline >= anyof(99981231-000000, 99990101-000000))q"},
        {R"q(deadline != allof(@never-1, @never))q",
         R"q(deadline != allof(99981231-000000, 99990101-000000))q"},
        {R"q(deadline = anyof(@never))q",
         R"q(deadline = anyof(99990101-000000))q"},
        {R"q(deadline = allof(@never))q",
         R"q(deadline = allof(99990101-000000))q"},
        {R"q(time in (@never, @never+1))q",
         R"q(time in (99990101-000000, 99990102-000000))q"},
        {R"q(mtime has (@never-1, @never))q",
         R"q(mtime has (99981231-000000, 99990101-000000))q"},
        {R"q(any = anyof(7, @never))q",
         R"q(any = anyof(7, 99990101-000000))q"},
        {R"q(not (deadline = @never or time < @never-1) and mtime <= @never xor all)q",
         R"q(not (deadline = 99990101-000000 or time < 99981231-000000) and mtime <= 99990101-000000 xor all)q"},
    };
    bool equivalent = true;
    for (const auto &[query, expanded] : cases)
        equivalent &= same_ast(parse_query(query).get(), parse_query(expanded).get());
    for (const auto *op : {"=", "!=", "<", "<=", ">", ">=", "~", "!~", "^~",
                           "!^~", "$~", "!$~", "~~", "!~~", "%~", "!%~"}) {
        const std::string prefix = std::string("deadline ") + op + " ";
        equivalent &= same_ast(parse_query(prefix + "@never").get(),
                               parse_query(prefix + "99990101-000000").get());
    }
    test(equivalent);
}

static void test_parser_string_fields_coerce_values() {
    const std::pair<const char *, const char *> cases[] = {
        {R"q(name = 123)q",
         R"q(name = "123")q"},
        {R"q(status = 007)q",
         R"q(status = "007")q"},
        {R"q(path = 20261106-010203)q",
         R"q(path = "20261106-010203")q"},
        {R"q(tag = @never)q",
         R"q(tag = "99990101-000000")q"},
        {R"q(name in [1..9])q",
         R"q(name in ["1".."9"])q"},
        {R"q(status in (1, 20261106))q",
         R"q(status in ("1", "20261106"))q"},
        {R"q(tag has (1, 2))q",
         R"q(tag has ("1", "2"))q"},
        {R"q(name = anyof(1, @never))q",
         R"q(name = anyof("1", "99990101-000000"))q"},
        {R"q(path != allof(1, 20261106))q",
         R"q(path != allof("1", "20261106"))q"},
    };
    bool equivalent = true;
    for (const auto &[query, expanded] : cases)
        equivalent &= same_ast(parse_query(query).get(), parse_query(expanded).get());
    test(equivalent);
}

static void test_parser_invalid_time_macro_syntax() {
    bool rejected = true;
    for (const auto *query : {"deadline in [@unknown..@never]", "deadline in (@never+)",
                              "deadline = anyof(@never, @today+)",
                              "deadline = allof(@never+365)", "not (deadline = @)"}) {
        try {
            parse_query(query);
            rejected = false;
        } catch (const Parse_Error &error) {
            rejected &= error.token().type == Token_Type::Invalid;
        }
    }
    test(rejected);
}

static void test_parser_hyphenated_value() {
    test(same_ast(parse_query("tag = foo-bar").get(),
                  parse_query("tag = \"foo-bar\"").get()) &&
         same_ast(parse_query("name in (_a-2, foo--bar, foo-)").get(),
                  parse_query("name in (\"_a-2\", \"foo--bar\", \"foo-\")").get()));
}

static void test_parser_expected_operand_diagnostics() {
    const std::pair<const char *, const char *> cases[] = {
        {"s ~", "Expected a string, anyof(...), or allof(...) after '~' for 'status', but the query ended (recognized as End)\nField 'status' matched from 's'"},
        {"priority =", "Expected a number, anyof(...), or allof(...) after '=' for 'priority', but the query ended (recognized as End)"},
        {"priority = \"\"", "Expected a number, anyof(...), or allof(...) after '=' for 'priority', got '' (recognized as String)"},
        {"priority = \"a'b\"", "Expected a number, anyof(...), or allof(...) after '=' for 'priority', got 'a\\'b' (recognized as String)"},
        {"priority = \"a\nb\"", "Expected a number, anyof(...), or allof(...) after '=' for 'priority', got 'a\\nb' (recognized as String)"},
        {"deadline >", "Expected a timestamp, anyof(...), or allof(...) after '>' for 'deadline', but the query ended (recognized as End)"},
        {"any =", "Expected a value, anyof(...), or allof(...) after '=' for 'any', but the query ended (recognized as End)"},
        {"name = )", "Expected a string, anyof(...), or allof(...) after '=' for 'name', got ')' (recognized as Rparen)"},
        {"name = anyof(", "Expected a string after '(' in the value list for 'name', but the query ended (recognized as End)"},
        {"name = anyof()", "List cannot be empty, expected a string after '(' in the value list for 'name', got ')' (recognized as Rparen)"},
        {"priority = allof()", "List cannot be empty, expected a number after '(' in the value list for 'priority', got ')' (recognized as Rparen)"},
        {"deadline in ()", "List cannot be empty, expected a timestamp after '(' in the value list for 'deadline', got ')' (recognized as Rparen)"},
        {"tag has ()", "List cannot be empty, expected a string after '(' in the value list for 'tag', got ')' (recognized as Rparen)"},
        {"any in ()", "List cannot be empty, expected a value after '(' in the value list for 'any', got ')' (recognized as Rparen)"},
        {"name in [..]", "Range must have at least one bound. Expected a string in the range for 'name'"},
        {"priority in [..]", "Range must have at least one bound. Expected a number in the range for 'priority'"},
        {"deadline in [..]", "Range must have at least one bound. Expected a timestamp in the range for 'deadline'"},
        {"any in [..]", "Range must have at least one bound. Expected a value in the range for 'any'"},
        {"priority in [..oops]", "Expected a number after '..' in the range for 'priority', got 'oops' (recognized as String)"},
        {"deadline in [2026..oops]", "Expected a timestamp or ']' after '..' in the range for 'deadline', got 'oops' (recognized as String)"},
        {"priority = anyof(1,)", "Expected a number after ',' in the value list for 'priority', got ')' (recognized as Rparen)"},
        {"priority = allof(1,", "Expected a number after ',' in the value list for 'priority', but the query ended (recognized as End)"},
        {"not (all or deadline in [oops..])", "Expected a timestamp or '..' after '[' in the range for 'deadline', got 'oops' (recognized as String)"},
        {"priority in [1 2]", "Expected '..' after the lower bound in the range for 'priority', got '2' (recognized as Number)"},
        {"time in [2026..2027", "Expected ']' after the upper bound in the range for 'time', but the query ended (recognized as End)"},
        {"tag = anyof(foo bar)", "Expected ',' or ')' after a value in the value list for 'tag', got 'bar' (recognized as String)"},
        {"priority has", "Expected '(' after 'has' for 'priority', but the query ended (recognized as End)"},
        {"priority in", "Expected '(' or '[' after 'in' for 'priority', but the query ended (recognized as End)"},
        {"priority", "Expected a comparison operator, 'in', or 'has' for 'priority', but the query ended (recognized as End)"},
        {"name na 123", "Expected a comparison operator, 'in', or 'has' for 'name', got 'na' (recognized as Name)"},
    };
    bool correct = true;
    for (const auto &[query, expected] : cases) {
        try {
            parse_query(query);
            correct = false;
        } catch (const Parse_Error &error) {
            correct &= std::string(error.what()) == expected;
        }
    }
    test(correct);
}

static void test_parser_fuzzy_field_diagnostics() {
    const std::pair<const char *, const char *> cases[] = {
        {"prio =", "priority"},
        {"s ~", "status"},
        {"prio = anyof()", "priority"},
        {"deadl in [..]", "deadline"},
        {"not (all and prio > )", "priority"},
    };
    bool correct = true;
    for (const auto &[query, field] : cases) {
        try {
            parse_query(query);
            correct = false;
        } catch (const Parse_Error &error) {
            const std::string message = error.what();
            correct &= message.find(std::string("\nField '") + field + "' matched from '") !=
                           std::string::npos &&
                       error.token().position > 0;
        }
    }
    try {
        parse_query("PRIORITY =");
        correct = false;
    } catch (const Parse_Error &error) {
        correct &= std::string(error.what()).find("matched from") == std::string::npos;
    }
    test(correct);
}

static void test_multiline_error_format() {
    const Parse_Error short_error("first\nsecond", Token{Token_Type::End, "", 3});
    const Parse_Error long_error("first\nsecond", Token{Token_Type::End, "", 10});
    test(short_error.format("p =") ==
             "4 | p =\n  |    ^\n  | first\n  | second\n" &&
         long_error.format("priority =") ==
             "11 | priority =\n   |           ^\n   | first\n   | second\n");
}

static void test_error_fragment_highlight() {
    const std::pair<const char *, size_t> cases[] = {
        {"priority = abc", 3},
        {"priority = \"a\\\"b\"", 6},
        {"priority = @never", 6},
        {"priority = \"猫\"", 4},
        {"priority = \"é\"", 3},
        {"priority =", 1},
    };
    bool correct = true;
    for (const auto &[query, width] : cases) {
        try {
            parse_query(query);
            correct = false;
        } catch (const Parse_Error &error) {
            const auto output = error.format(query);
            const auto marker_start = output.find('^');
            const auto marker_end = output.find('\n', marker_start);
            correct &= marker_start != std::string::npos &&
                       output.substr(marker_start, marker_end - marker_start) ==
                           std::string(width, '^');
        }
    }
    test(correct);
}


static std::string query_error_output(const std::string &query) {
    try {
        parse_query(query);
    } catch (const Parse_Error &error) {
        return error.format(query);
    }
    return {};
}

static void test_unicode_error_values() {
    const std::pair<const char *, const char *> cases[] = {
        {"priority = \"русский\"",
         "12 | priority = \"русский\"\n"
         "   |            ^^^^^^^^^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', got 'русский' (recognized as String)\n"},
        {"priority = \"漢字\"",
         "12 | priority = \"漢字\"\n"
         "   |            ^^^^^^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', got '漢字' (recognized as String)\n"},
        {"priority = \"🙂\"",
         "12 | priority = \"🙂\"\n"
         "   |            ^^^^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', got '🙂' (recognized as String)\n"},
        {"priority = \"é\"",
         "12 | priority = \"é\"\n"
         "   |            ^^^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', got 'é' (recognized as String)\n"},
        {"priority = \"русский 漢字 🙂\"",
         "12 | priority = \"русский 漢字 🙂\"\n"
         "   |            ^^^^^^^^^^^^^^^^^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', got 'русский 漢字 🙂' (recognized as String)\n"},
    };
    bool correct = true;
    for (const auto &[query, output] : cases)
        correct &= query_error_output(query) == output;
    test(correct);
}

static void test_unicode_before_error() {
    const std::pair<const char *, const char *> cases[] = {
        {"name = \"русский\" or prio = abc",
         "28 | name = \"русский\" or prio = abc\n"
         "   |                            ^^^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', got 'abc' (recognized as String)\n"
         "   | Field 'priority' matched from 'prio'\n"},
        {"name = \"漢字\" or prio = abc",
         "23 | name = \"漢字\" or prio = abc\n"
         "   |                         ^^^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', got 'abc' (recognized as String)\n"
         "   | Field 'priority' matched from 'prio'\n"},
        {"name = \"🙂\" or prio = abc",
         "22 | name = \"🙂\" or prio = abc\n"
         "   |                       ^^^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', got 'abc' (recognized as String)\n"
         "   | Field 'priority' matched from 'prio'\n"},
        {"name = \"é\" or prio = abc",
         "23 | name = \"é\" or prio = abc\n"
         "   |                      ^^^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', got 'abc' (recognized as String)\n"
         "   | Field 'priority' matched from 'prio'\n"},
        {"name = \"русский 漢字 🙂\" or prio = abc",
         "33 | name = \"русский 漢字 🙂\" or prio = abc\n"
         "   |                                    ^^^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', got 'abc' (recognized as String)\n"
         "   | Field 'priority' matched from 'prio'\n"},
    };
    bool correct = true;
    for (const auto &[query, output] : cases)
        correct &= query_error_output(query) == output;
    test(correct);
}

static void test_unquoted_unicode_errors() {
    const std::pair<const char *, const char *> cases[] = {
        {"флоывлфоывофыв",
         "1 | флоывлфоывофыв\n"
         "  | ^\n"
         "  | Expected a field name or a special keyword, got an invalid token (recognized as Invalid)\n"},
        {"漢字",
         "1 | 漢字\n"
         "  | ^\n"
         "  | Expected a field name or a special keyword, got an invalid token (recognized as Invalid)\n"},
        {"🙂",
         "1 | 🙂\n"
         "  | ^\n"
         "  | Expected a field name or a special keyword, got an invalid token (recognized as Invalid)\n"},
    };
    bool correct = true;
    for (const auto &[query, output] : cases)
        correct &= query_error_output(query) == output;
    test(correct);
}

static void test_unicode_before_macro_error() {
    const std::pair<const char *, const char *> cases[] = {
        {"name = \"漢字🙂\" or prio = @never",
         "24 | name = \"漢字🙂\" or prio = @never\n"
         "   |                           ^^^^^^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', got '99990101-000000' (recognized as Timestamp)\n"
         "   | Field 'priority' matched from 'prio'\n"},
    };
    bool correct = true;
    for (const auto &[query, output] : cases)
        correct &= query_error_output(query) == output;
    test(correct);
}

static void test_unicode_before_end_error() {
    const std::pair<const char *, const char *> cases[] = {
        {"name = \"漢字🙂\" or prio =",
         "23 | name = \"漢字🙂\" or prio =\n"
         "   |                          ^\n"
         "   | Expected a number, anyof(...), or allof(...) after '=' for 'priority', but the query ended (recognized as End)\n"
         "   | Field 'priority' matched from 'prio'\n"},
    };
    bool correct = true;
    for (const auto &[query, output] : cases)
        correct &= query_error_output(query) == output;
    test(correct);
}

static void test_fuzzy_name_selection() {
    const std::string names[] = {"names", "name", "priority"};
    const auto name = [](const std::string &value) -> const std::string & { return value; };
    test(mado::common::find_fuzzy_match("NAME", names, name) == std::begin(names) + 1 &&
         mado::common::find_fuzzy_match("prio", names, name) == std::begin(names) + 2);
}

static void test_fuzzy_name_ties() {
    const std::string names[] = {"ab", "ac"};
    const auto name = [](const std::string &value) -> const std::string & { return value; };
    test(mado::common::find_fuzzy_match("a", names, name) == std::begin(names));
}

static void test_fuzzy_name_missing() {
    const std::string names[] = {"name"};
    const std::vector<std::string> empty;
    const auto name = [](const std::string &value) -> const std::string & { return value; };
    test(mado::common::find_fuzzy_match("", names, name) == std::end(names) &&
         mado::common::find_fuzzy_match("zzz", names, name) == std::end(names) &&
         mado::common::find_fuzzy_match("name", empty, name) == empty.end());
}

static void test_parser_partial_ast() {
    const std::pair<const char *, std::unique_ptr<Ast_Node>> cases[] = {
        {"nme = )", ast_make_comparison(Ast_Comparison_Field::Name,
                                        Ast_Comparison_Operator::Eq, std::monostate{})},
        {"name =", ast_make_comparison(Ast_Comparison_Field::Name,
                                      Ast_Comparison_Operator::Eq, std::monostate{})},
        {"all junk", parse_query("all")},
        {"all and", ast_make_binary(Ast_Binary_Operator::And, parse_query("all"), nullptr)},
        {"all xor", ast_make_binary(Ast_Binary_Operator::Xor, parse_query("all"), nullptr)},
        {"all or (name = foo and)",
         ast_make_binary(Ast_Binary_Operator::Or, parse_query("all"),
                         ast_make_binary(Ast_Binary_Operator::And, parse_query("name = foo"), nullptr))},
        {"not (all", ast_make_unary(Ast_Unary_Operator::Not, parse_query("all"))},
        {"priority = anyof(1,2,)", parse_query("priority = anyof(1,2)")},
        {"priority = allof(1 2)", parse_query("priority = 1")},
        {"priority in [1 oops]", parse_query("priority >= 1")},
        {"priority in [1..oops]", parse_query("priority >= 1")},
        {"priority in [1..2", parse_query("priority in [1..2]")},
        {"priority in [..2", parse_query("priority <= 2")},
    };
    bool correct = true;
    for (const auto &[query, expected] : cases) {
        Parser parser(Lexer(query).tokenize());
        try {
            parser.parse();
            correct = false;
        } catch (const Parse_Error &) {
            correct &= same_ast(parser.partial_ast(), expected.get());
        }
    }
    test(correct);
}

static void test_parser_partial_ast_empty() {
    bool correct = true;
    for (const auto *query : {"", "priority = anyof()", "priority in [..]"}) {
        Parser parser(Lexer(query).tokenize());
        try {
            parser.parse();
            correct = false;
        } catch (const Parse_Error &) {
            correct &= parser.partial_ast() == nullptr;
        }
    }
    Parser valid(Lexer("all").tokenize());
    const auto ast = valid.parse();
    test(correct && ast && valid.partial_ast() == nullptr);
}

static void test_parser_long_digit_strings() {
    bool correct = true;
    for (const auto *field : {"name", "status", "tag", "path", "any"}) {
        for (const auto *value : {"12312", "1231231", "123123123123123",
                                  "0000000000000000000000000000000000000001",
                                  "20260230", "20011212-12312312313213", "123-foo", "123abc", "12x"}) {
            const std::string prefix = std::string(field) + " ~~ ";
            correct &= same_ast(parse_query(prefix + value).get(),
                                parse_query(prefix + "\"" + value + "\"").get());
        }
    }
    test(correct &&
         same_ast(parse_query("name in [12312..1231231]").get(),
                  parse_query("name in [\"12312\"..\"1231231\"]").get()) &&
         same_ast(parse_query("status = anyof(12312, 1231231)").get(),
                  parse_query("status = anyof(\"12312\", \"1231231\")").get()));
}

static void test_parser_long_digit_rejections() {
    bool correct = true;
    for (const auto *query : {"priority = 12312", "deadline = 20260230", "deadline = 20011212-12312312313213",
                              "priority = 123abc",
                              "name = \"12312", "name = @unknown"}) {
        try {
            parse_query(query);
            correct = false;
        } catch (const Parse_Error &) {
        }
    }
    test(correct);
}


static void test_parser_field_operator_matrix() {
    const std::pair<const char *, Ast_Comparison_Field> fields[] = {
        {"priority", Ast_Comparison_Field::Priority},
        {"tag", Ast_Comparison_Field::Tag}, {"status", Ast_Comparison_Field::Status},
        {"path", Ast_Comparison_Field::Path}, {"name", Ast_Comparison_Field::Name},
        {"time", Ast_Comparison_Field::Time}, {"mtime", Ast_Comparison_Field::Mtime},
        {"deadline", Ast_Comparison_Field::Deadline}, {"any", Ast_Comparison_Field::Any},
    };
    const std::pair<const char *, Ast_Comparison_Operator> operators[] = {
        {"=", Ast_Comparison_Operator::Eq}, {"!=", Ast_Comparison_Operator::Ne},
        {">", Ast_Comparison_Operator::Gt}, {"<", Ast_Comparison_Operator::Lt},
        {">=", Ast_Comparison_Operator::Ge}, {"<=", Ast_Comparison_Operator::Le},
        {"~", Ast_Comparison_Operator::Substr}, {"!~", Ast_Comparison_Operator::Nsubstr},
        {"^~", Ast_Comparison_Operator::Starts}, {"!^~", Ast_Comparison_Operator::Nstarts},
        {"$~", Ast_Comparison_Operator::Ends}, {"!$~", Ast_Comparison_Operator::Nends},
        {"~~", Ast_Comparison_Operator::Fuzzy}, {"!~~", Ast_Comparison_Operator::Nfuzzy},
        {"%~", Ast_Comparison_Operator::Glob}, {"!%~", Ast_Comparison_Operator::Nglob},
    };
    bool correct = true;
    for (const auto &[name, field] : fields) {
        const bool numeric = field == Ast_Comparison_Field::Priority;
        const bool timestamp = field == Ast_Comparison_Field::Time ||
                               field == Ast_Comparison_Field::Mtime ||
                               field == Ast_Comparison_Field::Deadline;
        const std::string operand = numeric ? "007" : timestamp ? "20240229-235959" : "123-ab";
        const Ast_Value value = numeric ? Ast_Value{uint16_t{7}} : Ast_Value{operand};
        for (const auto &[text, op] : operators) {
            const auto expected = ast_make_comparison(field, op, value);
            correct &= same_ast(parse_query(std::string(name) + text + operand).get(),
                                expected.get());
        }
    }
    test(correct);
}

static void test_lexer_numeric_prefix_boundaries() {
    const std::pair<const char *, Token_Type> cases[] = {
        {"0", Token_Type::Number}, {"999", Token_Type::Number},
        {"000", Token_Type::Number}, {"0000", Token_Type::Timestamp},
        {"1000", Token_Type::Timestamp}, {"10000", Token_Type::String},
        {"20240229", Token_Type::Timestamp}, {"20230229", Token_Type::String},
        {"20240229-", Token_Type::Timestamp}, {"20240229-23", Token_Type::Timestamp},
        {"20240229-2359", Token_Type::Timestamp}, {"20240229-235959", Token_Type::Timestamp},
        {"20240229-24", Token_Type::String}, {"20240229-2360", Token_Type::String},
        {"20240229-235960", Token_Type::String}, {"20011212-12312312313213", Token_Type::String},
        {"1abc", Token_Type::String}, {"1-2-3", Token_Type::String},
        {"1_", Token_Type::String}, {"123--", Token_Type::String},
    };
    bool correct = true;
    for (const auto &[value, kind] : cases) {
        const std::string input = std::string(value) + "..";
        const auto tokens = Lexer(input).tokenize();
        correct &= tokens.size() == 3 && tokens[0].type == kind &&
                   tokens[0].value == value && tokens[0].position == 0 &&
                   tokens[0].length == std::string(value).size() &&
                   tokens[1].type == Token_Type::DotDot &&
                   tokens[1].position == std::string(value).size() &&
                   tokens[2].type == Token_Type::End;
    }
    const std::string huge(10000, '9');
    const auto huge_tokens = Lexer(huge).tokenize();
    test(correct && huge_tokens.size() == 2 &&
         huge_tokens[0].type == Token_Type::String &&
         huge_tokens[0].value == huge && huge_tokens[0].length == huge.size());
}

static void test_parser_list_ast_shapes() {
    const std::pair<const char *, Ast_Binary_Operator> lists[] = {
        {"name = anyof(1, 123abc, @never)", Ast_Binary_Operator::Or},
        {"name = allof(1, 123abc, @never)", Ast_Binary_Operator::And},
        {"name in (1, 123abc, @never)", Ast_Binary_Operator::Or},
        {"name has (1, 123abc, @never)", Ast_Binary_Operator::And},
    };
    bool correct = true;
    for (const auto &[query, combine] : lists) {
        auto expected = ast_make_binary(
            combine,
            ast_make_binary(combine,
                ast_make_comparison(Ast_Comparison_Field::Name, Ast_Comparison_Operator::Eq, std::string("1")),
                ast_make_comparison(Ast_Comparison_Field::Name, Ast_Comparison_Operator::Eq, std::string("123abc"))),
            ast_make_comparison(Ast_Comparison_Field::Name, Ast_Comparison_Operator::Eq,
                                std::string("99990101-000000")));
        correct &= same_ast(parse_query(query).get(), expected.get());
    }
    test(correct);
}

static void test_parser_mixed_logical_ast() {
    auto expected = ast_make_binary(Ast_Binary_Operator::Or,
        ast_make_special(Ast_Node_Type::All),
        ast_make_binary(Ast_Binary_Operator::Xor,
            ast_make_special(Ast_Node_Type::Untagged),
            ast_make_binary(Ast_Binary_Operator::And,
                ast_make_special(Ast_Node_Type::Unnamed),
                ast_make_unary(Ast_Unary_Operator::Not,
                    ast_make_unary(Ast_Unary_Operator::Not,
                                  ast_make_special(Ast_Node_Type::Unstatused))))));
    auto left_associative = ast_make_binary(Ast_Binary_Operator::Xor,
        ast_make_binary(Ast_Binary_Operator::Xor,
            ast_make_special(Ast_Node_Type::All), ast_make_special(Ast_Node_Type::Untagged)),
        ast_make_special(Ast_Node_Type::Unnamed));
    test(same_ast(parse_query("all or untagged xor unnamed and not not unstatused").get(),
                  expected.get()) &&
         same_ast(parse_query("all xor untagged xor unnamed").get(), left_associative.get()));
}

static void test_parser_grouping_and_spacing() {
    auto expected = ast_make_comparison(Ast_Comparison_Field::Status,
                                       Ast_Comparison_Operator::Fuzzy,
                                       std::string("20011212-12312312313213"));
    const std::string nested = std::string(64, '(') +
                              "status~~20011212-12312312313213" + std::string(64, ')');
    test(same_ast(parse_query(nested).get(), expected.get()) &&
         same_ast(parse_query(" \t(\nstatus \r\n~~\t20011212-12312312313213\n) ").get(),
                  expected.get()));
}

static void test_parser_quoted_syntax_is_literal() {
    bool correct = true;
    for (const auto *value : {"anyof", "allof", "@now", "a,b", "[1..2]", "a or b", ""}) {
        const auto expected = ast_make_comparison(Ast_Comparison_Field::Name,
                                                 Ast_Comparison_Operator::Eq, std::string(value));
        correct &= same_ast(parse_query(std::string("name = \"") + value + "\"").get(),
                            expected.get());
    }
    test(correct);
}

static void test_parser_error_stage_positions() {
    const std::pair<const char *, const char *> cases[] = {
        {"name = @zzzz", "@zzzz"},
        {"name = \"unterminated", "\"unterminated"},
        {"priority = 1abc", "1abc"},
        {"deadline = 20230229", "20230229"},
        {"priority = anyof(1,,2)", ",2)"},
        {"name = allof(foo,)", ")"},
        {"priority in [1...2]", ".2]"},
        {"priority in [1..2..3]", "..3]"},
        {"name has [foo]", "[foo]"},
        {"name = anyof(allof(foo))", "(foo))"},
        {"(all))", ")"},
    };
    bool correct = true;
    for (const auto &[query, suffix] : cases) {
        try {
            parse_query(query);
            correct = false;
        } catch (const Parse_Error &error) {
            const std::string input(query);
            const auto expected = input.rfind(suffix);
            correct &= expected != std::string::npos && error.token().position == expected;
        }
    }
    test(correct);
}

static void test_parser_partial_ast_nested_comparison() {
    Parser parser(Lexer("not (name = foo or priority = )").tokenize());
    auto expected = ast_make_unary(Ast_Unary_Operator::Not,
        ast_make_binary(Ast_Binary_Operator::Or,
            ast_make_comparison(Ast_Comparison_Field::Name, Ast_Comparison_Operator::Eq, std::string("foo")),
            ast_make_comparison(Ast_Comparison_Field::Priority, Ast_Comparison_Operator::Eq,
                                std::monostate{})));
    bool rejected = false;
    try {
        parser.parse();
    } catch (const Parse_Error &) {
        rejected = true;
    }
    test(rejected && same_ast(parser.partial_ast(), expected.get()));
}

int main(int argc, char **argv) {

    if (argc >= 2 && std::string(argv[1]) == "-query") {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s -query <query>\n", argv[0]);
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
            fprintf(stderr, "%s", e.format(query).c_str());
            return 1;
        }

        return 0;
    }

    std::vector<Test_Case> tests = {
        // token
        {"Token creation", "create token with values", test_token_creation},
        {"parser_field_operator_matrix", "query edge cases", test_parser_field_operator_matrix},
        {"lexer_numeric_prefix_boundaries", "query edge cases", test_lexer_numeric_prefix_boundaries},
        {"parser_list_ast_shapes", "query edge cases", test_parser_list_ast_shapes},
        {"parser_mixed_logical_ast", "query edge cases", test_parser_mixed_logical_ast},
        {"parser_grouping_and_spacing", "query edge cases", test_parser_grouping_and_spacing},
        {"parser_quoted_syntax_is_literal", "query edge cases", test_parser_quoted_syntax_is_literal},
        {"parser_error_stage_positions", "query edge cases", test_parser_error_stage_positions},
        {"parser_partial_ast_nested_comparison", "query edge cases", test_parser_partial_ast_nested_comparison},
        {"Partial AST", "preserve completed syntax on failure", test_parser_partial_ast},
        {"Empty partial AST", "expose no tree without completed nodes", test_parser_partial_ast_empty},
        {"Fuzzy name selection", "prefer exact names before fuzzy matches", test_fuzzy_name_selection},
        {"Fuzzy name ties", "keep the first equal score", test_fuzzy_name_ties},
        {"Fuzzy name missing", "reject empty and unmatched input", test_fuzzy_name_missing},
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
        {"Lexer timestamp YYYY", "2026", test_lexer_timestamp_yyyy},
        {"Lexer timestamp YYYYMM", "202609", test_lexer_timestamp_yyyymm},
        {"Lexer timestamp YYYYMMDD", "20260911", test_lexer_timestamp_yyyymmdd},
        {"Lexer timestamp full", "20260911-123045", test_lexer_timestamp_full},
        {"Lexer timestamp - only", "20260911-", test_lexer_timestamp_with_t_only},
        {"Lexer timestamp invalid date", "20260230", test_lexer_timestamp_invalid_date},
        {"Lexer timestamp invalid month", "202613", test_lexer_timestamp_invalid_month},
        {"Lexer timestamp invalid length", "12345", test_lexer_timestamp_invalid_length},
        {"Lexer timestamp in query", "deadline < 20260911-123045", test_lexer_timestamp_in_query},
        {"Lexer timestamp and numbers", "42 20260911 0", test_lexer_timestamp_numbers_still_work},
        {"Time macro syntax", "compare macro and literal ASTs", test_parser_time_macro_syntax},
        {"Invalid macro syntax", "reject invalid macros in expressions", test_parser_invalid_time_macro_syntax},
        {"String field operands", "compare typed and quoted ASTs", test_parser_string_fields_coerce_values},
        {"Long digit strings", "accept digit-only text beyond number and date limits", test_parser_long_digit_strings},
        {"Long digit rejection boundaries", "keep malformed input and typed fields invalid", test_parser_long_digit_rejections},
        {"Time macro values", "resolve all time macros at a fixed time", test_time_macro_values},
        {"UTC conversion failure", "reject unrepresentable UTC times", test_utc_conversion_failure},
        {"Time macro range", "tokenize spaced offsets and a range delimiter", test_time_macro_range_tokens},
        {"Invalid time macros", "reject unknown names and malformed offsets", test_invalid_time_macro_tokens},
        {"Time macro year bounds", "reject years outside 0000–9999", test_time_macro_out_of_range},
        // ast
        {"AST comparison", "create comparison node", test_ast_comparison_node},
        {"AST comparison string", "create comparison node with string", test_ast_comparison_node_string},
        {"AST binary", "create binary node", test_ast_binary_node},
        {"AST unary", "create unary node", test_ast_unary_node},
        {"AST special", "create special nodes", test_ast_special_nodes},
        // parser
        {"Parser number", "parse 'priority > 5'", test_parser_number_condition},
        {"Parser string", "parse 'tag = bug'", test_parser_string_condition},
        {"Parser hyphenated value", "parse bare words containing hyphens", test_parser_hyphenated_value},
        {"Parser keyword as string", "parse 'tag = all and status = and and name = or and any = not'", test_parser_keyword_as_string_value},
        {"Parser and", "parse 'priority > 5 and tag = bug'", test_parser_binary_and},
        {"Parser not", "parse 'not status = closed'", test_parser_not},
        {"Parser parentheses", "parse '(priority > 5 or priority < 2) and tag = bug'", test_parser_parentheses},
        {"Parser special", "parse 'all'", test_parser_special},
        {"Parser empty", "empty query throws", test_parser_empty_throws},
        {"Parser unexpected", "unexpected token throws", test_parser_unexpected_token_throws},
        {"Parser operand diagnostics", "list all legal comparison operands", test_parser_expected_operand_diagnostics},
        {"Multiline diagnostics", "align all message lines with the gutter", test_multiline_error_format},
        {"Error fragment", "underline the original erroneous token", test_error_fragment_highlight},
        {"unicode_error_values", "verify exact Unicode diagnostics", test_unicode_error_values},
        {"unicode_before_error", "verify exact Unicode diagnostics", test_unicode_before_error},
        {"unquoted_unicode_errors", "verify exact Unicode diagnostics", test_unquoted_unicode_errors},
        {"unicode_before_macro_error", "verify exact Unicode diagnostics", test_unicode_before_macro_error},
        {"unicode_before_end_error", "verify exact Unicode diagnostics", test_unicode_before_end_error},
        {"Parser fuzzy field diagnostics", "explain resolved field names in errors", test_parser_fuzzy_field_diagnostics},
        {"Parser priority", "parse 'not not priority > 1 or priority > 2 xor priority > 3 and not priority > 4'", test_parser_priority},
        {"Parser anyof number", "parse 'priority = anyof(1, 2, 3)'", test_parser_anyof_number},
        {"Parser allof number", "parse 'priority = allof(1, 2)'", test_parser_allof_number},
        {"Parser anyof string", "parse 'tag = anyof(bug, crit)'", test_parser_anyof_string},
        {"Parser allof string", "parse 'tag = allof(a, b, c)'", test_parser_allof_string},
        {"Parser anyof time", "parse 'deadline = anyof(20260101, 20260202)'", test_parser_anyof_time},
        {"Parser anyof any", "parse 'any = anyof(1, foo, 20260101)'", test_parser_anyof_any},
        {"Parser anyof single", "parse 'priority = anyof(5)'", test_parser_anyof_single_value},
        {"Parser anyof with op", "parse 'priority > anyof(1, 2)'", test_parser_anyof_with_operator},
        {"Parser anyof empty", "parse 'tag = anyof()' throws", test_parser_anyof_empty_throws},
        {"Parser anyof trailing comma", "parse 'tag = anyof(a, b,)' throws", test_parser_anyof_trailing_comma_throws},
        {"Parser anyof missing comma", "parse 'tag = anyof(a b)' throws", test_parser_anyof_missing_comma_throws},
        {"Parser anyof unclosed", "parse 'tag = anyof(a, b' throws", test_parser_anyof_unclosed_throws},
        {"Parser anyof wrong type", "parse 'priority = anyof(a, b)' throws", test_parser_anyof_wrong_type_throws},
        {"Parser anyof in expression", "parse 'tag = anyof(a, b) and priority > 5'", test_parser_anyof_in_expression},
        {"Parser in", "parse 'status in (opened, reopened)'", test_parser_in},
        {"Parser has", "parse 'tag has (bug, crit)'", test_parser_has},
        {"Parser in single", "parse 'status in (opened)'", test_parser_in_single_value},
        {"Parser range both", "parse 'priority in [50..100]'", test_parser_range_both},
        {"Parser range low only", "parse 'priority in [50..]'", test_parser_range_low_only},
        {"Parser range high only", "parse 'priority in [..50]'", test_parser_range_high_only},
        {"Parser range string", "parse 'name in [a..z]'", test_parser_range_string},
        {"Parser range time", "parse 'deadline in [20260101..20260202]'", test_parser_range_time},
        {"Parser range any", "parse 'any in [1..20260101]'", test_parser_range_any},
        {"Parser range in expression", "parse 'priority in [50..100] and tag = bug'", test_parser_range_in_expression},
        {"Parser range empty", "parse 'priority in [..]' throws", test_parser_range_empty_throws},
        {"Parser range missing dotdot", "parse 'priority in [50]' throws", test_parser_range_missing_dotdot_throws},
        {"Parser range unclosed", "parse 'priority in [50..100' throws", test_parser_range_unclosed_throws},
        {"Parser range wrong type", "parse 'priority in [a..z]' throws", test_parser_range_wrong_type_throws},
    };

    return run_tests(tests, "query: ");
}
