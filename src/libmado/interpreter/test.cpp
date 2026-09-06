#include "../common/test/test_utils.hpp"
#include "token.hpp"

using namespace mado::interpreter;
using namespace mado::common;

static void test_token_creation() {
    Token t{Token_Type::Number, "42", 0};

    test(t.type == Token_Type::Number, "type is Number");
    test(t.value == "42", "value is \"42\"");
    test(t.position == 0, "position is 0");
}

static void test_token_creation_default() {
    Token t;

    test(t.type == Token_Type::Invalid, "default type is Invalid");
    test(t.value.empty(), "default value is empty");
    test(t.position == 0, "default position is 0");
}

static void test_token_to_string() {
    test(to_string(Token_Type::Number) == "Number", "to_string(Number)");
    test(to_string(Token_Type::Gt) == ">", "to_string(Gt)");
    test(to_string(Token_Type::Lparen) == "(", "to_string(Lparen)");
    test(to_string(Token_Type::And) == "And", "to_string(And)");
}

static void test_token_all_types_have_string() {
    bool all_ok = true;
    for (int i = 0; i < static_cast<int>(Token_Type::Invalid); i++) {
        auto type = static_cast<Token_Type>(i);
        if (to_string(type) == "Unknown") {
            all_ok = false;
            break;
        }
    }
    test(all_ok, "all types have string representation");
}

int main() {
    std::vector<Test_Case> tests = {
        {"Token creation", "create token with values", test_token_creation},
        {"Token default", "create default token", test_token_creation_default},
        {"Token to_string", "convert type to string", test_token_to_string},
        {"Token all types", "all types have string", test_token_all_types_have_string},
    };

    return run_tests(tests);
}
