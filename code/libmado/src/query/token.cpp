#include <mado/query/token.hpp>

#include <cstdint>

namespace mado::query {

std::string token_type_to_string(Token_Type type) {
    using enum Token_Type;

    switch (type) {
    // fields
    case Priority:
        return "Priority";
    case Tag:
        return "Tag";
    case Status:
        return "Status";
    case Name:
        return "Name";
    case Path:
        return "Path";
    case Time:
        return "Time";
    case Deadline:
        return "Deadline";
    case Mtime:
        return "Mtime";
    case Any:
        return "Any";

    // values
    case Number:
        return "Number";
    case String:
        return "String";
    case Timestamp:
        return "Timestamp";

    // special expressions
    case All:
        return "All";
    case Untagged:
        return "Untagged";
    case Unstatused:
        return "Unstatused";
    case Unnamed:
        return "Unnamed";
    case Unprioritized:
        return "Unprioritized";
    case Undeadlined:
        return "Undeadlined";

    // sugar
    case Allof:
        return "Allof";
    case Anyof:
        return "Anyof";
    case In:
        return "In";
    case Has:
        return "Has";

    // logical operators
    case And:
        return "And";
    case Or:
        return "Or";
    case Xor:
        return "Xor";
    case Not:
        return "Not";

    // comparison operators
    case Gt:
        return "Gt";
    case Lt:
        return "Lt";
    case Ge:
        return "Ge";
    case Le:
        return "Le";
    case Eq:
        return "Eq";
    case Ne:
        return "Ne";
    case Substr:
        return "Substr";
    case Nsubstr:
        return "Nsubstr";
    case Fuzzy:
        return "Fuzzy";
    case Nfuzzy:
        return "Nfuzzy";
    case Starts:
        return "Starts";
    case Nstarts:
        return "Nstarts";
    case Ends:
        return "Ends";
    case Nends:
        return "Nends";
    case Glob:
        return "Glob";
    case Nglob:
        return "Nglob";

    // punctuation
    case Lparen:
        return "Lparen";
    case Rparen:
        return "Rparen";
    case Comma:
        return "Comma";
    case DotDot:
        return "DotDot";
    case Lbracket:
        return "Lbracket";
    case Rbracket:
        return "Rbracket";

    // system
    case End:
        return "End";
    case Invalid:
        return "Invalid";
    }

    return "Unknown";
}

bool token_is_keyword(Token_Type type) {
    return static_cast<uint16_t>(type) <= MAX_KEYWORD_VALUE &&
           token_type_to_string(type) != "Unknown";
}

} // namespace mado::query
