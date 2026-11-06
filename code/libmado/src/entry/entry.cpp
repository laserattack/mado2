#include <mado/common/text_utils.hpp>
#include <mado/entry/entry.hpp>

namespace mado::entry {

// Stores the supplied entry path. No error result or exceptions.
void Entry::set_path(std::filesystem::path p) {
    path_ = std::move(p);
}

// Stores the supplied name, including an empty name. No error result or
// exceptions.
void Entry::set_name(std::string n) {
    name_ = std::move(n);
}

// Stores the supplied status, including an empty status. No error result or
// exceptions.
void Entry::set_status(std::string s) {
    status_ = std::move(s);
}

// Stores a priority from 0 to 999. Throws Entry_Error outside that range.
void Entry::set_priority(uint16_t p) {
    if (p > 999)
        throw Entry_Error("Priority must be in [0, 999], got " + std::to_string(p));
    priority_ = p;
}

// {""} represents "no tags"; an empty vector is invalid.
void Entry::set_tags(std::vector<std::string> t) {
    if (t.empty())
        throw Entry_Error("Tags must not be empty");
    tags_ = std::move(t);
}

// Stores a timestamp. Throws Entry_Error for invalid input.
void Entry::set_time(std::string t) {
    if (!mado::common::is_timestamp(t))
        throw Entry_Error("Invalid timestamp: " + t);
    time_ = std::move(t);
}

// Stores a modification timestamp. Throws Entry_Error for invalid input.
void Entry::set_mtime(std::string t) {
    if (!mado::common::is_timestamp(t))
        throw Entry_Error("Invalid timestamp: " + t);
    mtime_ = std::move(t);
}

// Stores a deadline timestamp. Throws Entry_Error for invalid input.
void Entry::set_deadline(std::string t) {
    if (!mado::common::is_timestamp(t))
        throw Entry_Error("Invalid timestamp: " + t);
    deadline_ = std::move(t);
}

} // namespace mado::entry
