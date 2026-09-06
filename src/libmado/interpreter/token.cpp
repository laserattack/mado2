#include "token.hpp"

namespace mado::interpreter {

std::string to_string(Token_Type type) {
    switch (type) {
    // fields
    case Token_Type::Priority:
        return "Priority";
    case Token_Type::Tag:
        return "Tag";
    case Token_Type::Status:
        return "Status";
    case Token_Type::Name:
        return "Name";
    case Token_Type::Path:
        return "Path";
    case Token_Type::Time:
        return "Time";
    case Token_Type::Deadline:
        return "Deadline";
    case Token_Type::Mtime:
        return "Mtime";
    case Token_Type::Any:
        return "Any";

    // values
    case Token_Type::Number:
        return "Number";
    case Token_Type::String:
        return "String";
    case Token_Type::Timestamp:
        return "Timestamp";

    // special expressions
    case Token_Type::All:
        return "All";
    case Token_Type::Untagged:
        return "Untagged";
    case Token_Type::Unstatused:
        return "Unstatused";
    case Token_Type::Unnamed:
        return "Unnamed";
    case Token_Type::Unprioritized:
        return "Unprioritized";
    case Token_Type::Undeadlined:
        return "Undeadlined";

    // sugar
    case Token_Type::Allof:
        return "Allof";
    case Token_Type::Anyof:
        return "Anyof";
    case Token_Type::In:
        return "In";
    case Token_Type::Has:
        return "Has";

    // logical operators
    case Token_Type::And:
        return "And";
    case Token_Type::Or:
        return "Or";
    case Token_Type::Xor:
        return "Xor";
    case Token_Type::Not:
        return "Not";

    // comparison operators
    case Token_Type::Gt:
        return ">";
    case Token_Type::Lt:
        return "<";
    case Token_Type::Ge:
        return ">=";
    case Token_Type::Le:
        return "<=";
    case Token_Type::Eq:
        return "=";
    case Token_Type::Ne:
        return "!=";
    case Token_Type::Substr:
        return "~";
    case Token_Type::Nsubstr:
        return "!~";
    case Token_Type::Fuzzy:
        return "~~";
    case Token_Type::Nfuzzy:
        return "!~~";
    case Token_Type::Starts:
        return "^~";
    case Token_Type::Nstarts:
        return "!^~";
    case Token_Type::Ends:
        return "$~";
    case Token_Type::Nends:
        return "!$~";
    case Token_Type::Glob:
        return "%~";
    case Token_Type::Nglob:
        return "!%~";

    // punctuation
    case Token_Type::Lparen:
        return "(";
    case Token_Type::Rparen:
        return ")";
    case Token_Type::Comma:
        return ",";
    case Token_Type::DotDot:
        return "..";
    case Token_Type::Lbracket:
        return "[";
    case Token_Type::Rbracket:
        return "]";

    // system
    case Token_Type::End:
        return "End";
    case Token_Type::Invalid:
        return "Invalid";
    }

    return "Unknown";
}

} // namespace mado::interpreter
