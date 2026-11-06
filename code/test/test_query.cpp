#include "test_utils.hpp"

#include <mado/query/ast.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>
#include <mado/query/token.hpp>

#include <cstdio>
#include <iostream>
#include <string>

using namespace mado::query;

// token
// Токены.

// Checks creation of token with values. Unexpected exceptions fail the test.
// Проверяет создание токена с заданным значением. Неожиданное исключение проваливает тест.
static void test_token_creation() {
    Token t{Token_Type::Number, "42", 0};

    test(t.type == Token_Type::Number &&
         t.value == "42" &&
         t.position == 0);
}

// Checks creation of default token. Unexpected exceptions fail the test.
// Проверяет создание токена по умолчанию. Неожиданное исключение проваливает тест.
static void test_token_creation_default() {
    Token t;

    test(t.type == Token_Type::Invalid &&
         t.value.empty() &&
         t.position == 0);
}

// Checks conversion of type to string. Unexpected exceptions fail the test.
// Проверяет строковое имя типа токена. Неожиданное исключение проваливает тест.
static void test_token_type_to_string() {
    test(token_type_to_string(Token_Type::Number) == "Number" &&
         token_type_to_string(Token_Type::Gt) == "Gt" &&
         token_type_to_string(Token_Type::Lparen) == "Lparen" &&
         token_type_to_string(Token_Type::And) == "And");
}

// lexer
// Лексер.

// Checks tokenization of empty query. Unexpected exceptions fail the test.
// Проверяет разбор пустого запроса. Неожиданное исключение проваливает тест.
static void test_lexer_empty_query() {
    Lexer lexer("");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 1 &&
         tokens[0].type == Token_Type::End);
}

// Checks tokenization of numbers. Unexpected exceptions fail the test.
// Проверяет разбор чисел. Неожиданное исключение проваливает тест.
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

// Checks tokenization of keywords. Unexpected exceptions fail the test.
// Проверяет распознавание ключевых слов. Неожиданное исключение проваливает тест.
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

// Checks tokenization of quoted strings. Unexpected exceptions fail the test.
// Проверяет распознавание строк в кавычках. Неожиданное исключение проваливает тест.
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

// Checks tokenization of operators. Unexpected exceptions fail the test.
// Проверяет распознавание операторов. Неожиданное исключение проваливает тест.
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

// Checks tokenization of punctuation. Unexpected exceptions fail the test.
// Проверяет распознавание пунктуации. Неожиданное исключение проваливает тест.
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

// Checks tokenization of query. Unexpected exceptions fail the test.
// Проверяет разбиение запроса на токены. Неожиданное исключение проваливает тест.
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

// Checks tokenization of with whitespace. Unexpected exceptions fail the test.
// Проверяет пропуск пробельных символов. Неожиданное исключение проваливает тест.
static void test_lexer_whitespace() {
    Lexer lexer("    priority\t   >\n  5  ");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 4 &&
         tokens[0].type == Token_Type::Priority &&
         tokens[1].type == Token_Type::Gt &&
         tokens[2].type == Token_Type::Number &&
         tokens[3].type == Token_Type::End);
}

// Checks token positions. Unexpected exceptions fail the test.
// Проверяет позиции токенов. Неожиданное исключение проваливает тест.
static void test_lexer_position() {
    Lexer lexer("priority > 5");
    auto tokens = lexer.tokenize();

    test(tokens[0].position == 0 &&
         tokens[1].position == 9 &&
         tokens[2].position == 11);
}

// Checks tokenization of invalid operators. Unexpected exceptions fail the test.
// Проверяет некорректные операторы. Неожиданное исключение проваливает тест.
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
         tokens[3].type == Token_Type::Allof && // fuzzy / Нечёткое сравнение.
         tokens[3].value == "f" &&
         tokens[4].type == Token_Type::Tag && // fuzzy / Нечёткое сравнение.
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

// Checks tokenization of unterminated string. Unexpected exceptions fail the test.
// Проверяет незакрытую строку. Неожиданное исключение проваливает тест.
static void test_lexer_unterminated_string() {
    Lexer lexer("\"hello");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Invalid &&
         tokens[0].value == "hello" &&
         tokens[1].type == Token_Type::End);
}

// Checks how the lexer classifies `2026`. Unexpected exceptions fail the test.
// Проверяет метку с одним годом. Неожиданное исключение проваливает тест.
static void test_lexer_timestamp_yyyy() {
    Lexer lexer("2026");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Timestamp &&
         tokens[0].value == "2026" &&
         tokens[1].type == Token_Type::End);
}

// Checks how the lexer classifies `202609`. Unexpected exceptions fail the test.
// Проверяет метку с годом и месяцем. Неожиданное исключение проваливает тест.
static void test_lexer_timestamp_yyyymm() {
    Lexer lexer("202609");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Timestamp &&
         tokens[0].value == "202609" &&
         tokens[1].type == Token_Type::End);
}

// Checks how the lexer classifies `20260911`. Unexpected exceptions fail the test.
// Проверяет метку с полной датой. Неожиданное исключение проваливает тест.
static void test_lexer_timestamp_yyyymmdd() {
    Lexer lexer("20260911");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Timestamp &&
         tokens[0].value == "20260911" &&
         tokens[1].type == Token_Type::End);
}

// Checks how the lexer classifies `20260911-123045`. Unexpected exceptions fail the test.
// Проверяет полную метку с датой и временем. Неожиданное исключение проваливает тест.
static void test_lexer_timestamp_full() {
    Lexer lexer("20260911-123045");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Timestamp &&
         tokens[0].value == "20260911-123045" &&
         tokens[1].type == Token_Type::End);
}

// Checks how the lexer classifies `20260911-`. Unexpected exceptions fail the test.
// Проверяет метку с датой и завершающим разделителем. Неожиданное исключение проваливает тест.
static void test_lexer_timestamp_with_t_only() {
    Lexer lexer("20260911-");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Timestamp &&
         tokens[0].value == "20260911-" &&
         tokens[1].type == Token_Type::End);
}

// Checks how the lexer classifies `20260230`. Unexpected exceptions fail the test.
// Проверяет отклонение невозможной даты. Неожиданное исключение проваливает тест.
static void test_lexer_timestamp_invalid_date() {
    Lexer lexer("20260230");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Invalid &&
         tokens[1].type == Token_Type::End);
}

// Checks how the lexer classifies `202613`. Unexpected exceptions fail the test.
// Проверяет отклонение неверного месяца. Неожиданное исключение проваливает тест.
static void test_lexer_timestamp_invalid_month() {
    Lexer lexer("202613");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Invalid &&
         tokens[1].type == Token_Type::End);
}

// Checks how the lexer classifies `12345`. Unexpected exceptions fail the test.
// Проверяет отклонение неверной длины даты. Неожиданное исключение проваливает тест.
static void test_lexer_timestamp_invalid_length() {
    Lexer lexer("12345");
    auto tokens = lexer.tokenize();

    test(tokens.size() == 2 &&
         tokens[0].type == Token_Type::Invalid &&
         tokens[1].type == Token_Type::End);
}

// Checks deadline < 20260911-123045. Unexpected exceptions fail the test.
// Проверяет метку времени внутри запроса. Неожиданное исключение проваливает тест.
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

// Checks 42 20260911 0. Unexpected exceptions fail the test.
// Проверяет различение чисел и меток времени. Неожиданное исключение проваливает тест.
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
// AST.

// Checks creation of comparison node. Unexpected exceptions fail the test.
// Проверяет узел числового сравнения. Неожиданное исключение проваливает тест.
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

// Checks creation of comparison node with string. Unexpected exceptions fail the
// test.
// Проверяет узел строкового сравнения. Неожиданное исключение проваливает тест.
static void test_ast_comparison_node_string() {
    auto node = ast_make_comparison(
        Ast_Comparison_Field::Tag,
        Ast_Comparison_Operator::Eq,
        std::string("bug"));

    auto *comp = static_cast<Ast_Comparison_Operator_Node *>(node.get());

    test(comp->is_string() &&
         comp->as_string() == "bug");
}

// Checks creation of binary node. Unexpected exceptions fail the test.
// Проверяет бинарный узел AST. Неожиданное исключение проваливает тест.
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

// Checks creation of unary node. Unexpected exceptions fail the test.
// Проверяет унарный узел AST. Неожиданное исключение проваливает тест.
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

// Checks creation of special nodes. Unexpected exceptions fail the test.
// Проверяет узлы специальных выражений. Неожиданное исключение проваливает тест.
static void test_ast_special_nodes() {
    auto all_node = ast_make_special(Ast_Node_Type::All);
    auto untagged_node = ast_make_special(Ast_Node_Type::Untagged);
    auto unnamed_node = ast_make_special(Ast_Node_Type::Unnamed);

    test(all_node->type == Ast_Node_Type::All &&
         untagged_node->type == Ast_Node_Type::Untagged &&
         unnamed_node->type == Ast_Node_Type::Unnamed);
}

// parser
// Парсер.

// Checks parse 'priority > 5'. Unexpected exceptions fail the test.
// Проверяет числовое условие. Неожиданное исключение проваливает тест.
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

// Checks parse 'tag = bug'. Unexpected exceptions fail the test.
// Проверяет строковое условие. Неожиданное исключение проваливает тест.
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

// Checks parse 'tag = all and status = and and name = or and any = not'.
// Unexpected exceptions fail the test.
// Проверяет ключевое слово в роли строкового значения. Неожиданное исключение проваливает
// тест.
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

// Checks parse 'priority > 5 and tag = bug'. Unexpected exceptions fail the
// test.
// Проверяет бинарное выражение AND. Неожиданное исключение проваливает тест.
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

// Checks parse 'not status = closed'. Unexpected exceptions fail the test.
// Проверяет выражение NOT. Неожиданное исключение проваливает тест.
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

// Checks parse '(priority > 5 or priority < 2) and tag = bug'. Unexpected
// exceptions fail the test.
// Проверяет группировку скобками. Неожиданное исключение проваливает тест.
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

// Checks parse 'all'. Unexpected exceptions fail the test.
// Проверяет специальное выражение. Неожиданное исключение проваливает тест.
static void test_parser_special() {
    Lexer lexer("all");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    test(ast->type == Ast_Node_Type::All);
}

// Checks parse 'not not priority > 1 or priority > 2 xor priority > 3 and not
// priority > 4'. Unexpected exceptions fail the test.
// Проверяет исключение для пустого запроса. Неожиданное исключение проваливает тест.
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
 * Дерево проверяет приоритеты OR, XOR, AND и NOT.
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

// Checks empty query throws. Unexpected exceptions fail the test.
// Проверяет исключение для неожиданного токена. Неожиданное исключение проваливает тест.
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

// Checks unexpected token throws. Unexpected exceptions fail the test.
// Проверяет приоритет логических операций. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority = anyof(1, 2, 3)'. Unexpected exceptions fail the
// test.
// Проверяет числовые значения anyof. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority = allof(1, 2)'. Unexpected exceptions fail the test.
// Проверяет числовые значения allof. Неожиданное исключение проваливает тест.
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

// Checks parse 'tag = anyof(bug, crit)'. Unexpected exceptions fail the test.
// Проверяет строковые значения anyof. Неожиданное исключение проваливает тест.
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

// Checks parse 'tag = allof(a, b, c)'. Unexpected exceptions fail the test.
// Проверяет строковые значения allof. Неожиданное исключение проваливает тест.
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

// Checks parse 'deadline = anyof(20260101, 20260202)'. Unexpected exceptions
// fail the test.
// Проверяет метки времени в anyof. Неожиданное исключение проваливает тест.
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

// Checks parse 'any = anyof(1, foo, 20260101)'. Unexpected exceptions fail the
// test.
// Проверяет разные типы в anyof для поля any. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority = anyof(5)'. Unexpected exceptions fail the test.
// Проверяет anyof с одним значением. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority > anyof(1, 2)'. Unexpected exceptions fail the test.
// Проверяет anyof с оператором сравнения. Неожиданное исключение проваливает тест.
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

// Checks parse 'tag = anyof()' throws. Unexpected exceptions fail the test.
// Проверяет исключение для пустого anyof. Неожиданное исключение проваливает тест.
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

// Checks parse 'tag = anyof(a, b,)' throws. Unexpected exceptions fail the
// test.
// Проверяет исключение для завершающей запятой в anyof. Неожиданное исключение проваливает
// тест.
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

// Checks parse 'tag = anyof(a b)' throws. Unexpected exceptions fail the test.
// Проверяет исключение для пропущенной запятой в anyof. Неожиданное исключение проваливает
// тест.
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

// Checks parse 'tag = anyof(a, b' throws. Unexpected exceptions fail the test.
// Проверяет исключение для незакрытого anyof. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority = anyof(a, b)' throws. Unexpected exceptions fail the
// test.
// Проверяет исключение для неверного типа в anyof. Неожиданное исключение проваливает тест.
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

// Checks parse 'tag = anyof(a, b) and priority > 5'. Unexpected exceptions
// fail the test.
// Проверяет anyof внутри составного выражения. Неожиданное исключение проваливает тест.
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

// Checks parse 'status in (opened, reopened)'. Unexpected exceptions fail the
// test.
// Проверяет оператор in со списком. Неожиданное исключение проваливает тест.
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

// Checks parse 'tag has (bug, crit)'. Unexpected exceptions fail the test.
// Проверяет оператор has со списком. Неожиданное исключение проваливает тест.
static void test_parser_has() {
    Lexer lexer("tag has (bug, crit)");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));

    auto ast = parser.parse();

    auto *root = static_cast<Ast_Binary_Operator_Node *>(ast.get());

    test(root->op == Ast_Binary_Operator::And);
}

// Checks parse 'status in (opened)'. Unexpected exceptions fail the test.
// Проверяет оператор in с одним значением. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority in [50..100]'. Unexpected exceptions fail the test.
// Проверяет диапазон с двумя границами. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority in [50..]'. Unexpected exceptions fail the test.
// Проверяет диапазон только с нижней границей. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority in [..50]'. Unexpected exceptions fail the test.
// Проверяет диапазон только с верхней границей. Неожиданное исключение проваливает тест.
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

// Checks parse 'name in [a..z]'. Unexpected exceptions fail the test.
// Проверяет строковый диапазон. Неожиданное исключение проваливает тест.
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

// Checks parse 'deadline in [20260101..20260202]'. Unexpected exceptions fail
// the test.
// Проверяет диапазон меток времени. Неожиданное исключение проваливает тест.
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

// Checks parse 'any in [1..20260101]'. Unexpected exceptions fail the test.
// Проверяет диапазон с разными типами для поля any. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority in [50..100] and tag = bug'. Unexpected exceptions
// fail the test.
// Проверяет диапазон внутри составного выражения. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority in [..]' throws. Unexpected exceptions fail the test.
// Проверяет исключение для пустого диапазона. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority in [50]' throws. Unexpected exceptions fail the test.
// Проверяет исключение при отсутствии разделителя диапазона. Неожиданное исключение
// проваливает тест.
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

// Checks parse 'priority in [50..100' throws. Unexpected exceptions fail the
// test.
// Проверяет исключение для незакрытого диапазона. Неожиданное исключение проваливает тест.
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

// Checks parse 'priority in [a..z]' throws. Unexpected exceptions fail the
// test.
// Проверяет исключение для неверного типа границы. Неожиданное исключение проваливает тест.
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
// Точка входа.

// Runs this test suite and returns 1 on failure or 0 on success.
// Запускает набор тестов. Возвращает 1 при провале, 0 при успехе.
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
        // Токены.
// Токены.
        {"Token creation", "create token with values", test_token_creation},
        {"Token default", "create default token", test_token_creation_default},
        {"Token to_string", "convert type to string", test_token_type_to_string},
        // lexer
        // Лексер.
// Лексер.
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
        // ast
        // AST.
// AST.
        {"AST comparison", "create comparison node", test_ast_comparison_node},
        {"AST comparison string", "create comparison node with string", test_ast_comparison_node_string},
        {"AST binary", "create binary node", test_ast_binary_node},
        {"AST unary", "create unary node", test_ast_unary_node},
        {"AST special", "create special nodes", test_ast_special_nodes},
        // parser
        // Парсер.
// Парсер.
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
