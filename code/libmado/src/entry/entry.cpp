#include <mado/common/text_utils.hpp>
#include <mado/entry/entry.hpp>

namespace mado::entry {

void Entry::set_path(const std::filesystem::path &p) {
    path_ = p;
}

void Entry::set_name(const std::string &n) {
    name_ = n;
}

void Entry::set_status(const std::string &s) {
    status_ = s;
}

void Entry::set_priority(uint16_t p) {
    if (p > 999)
        throw Entry_Error("Priority must be in [0, 999], got " + std::to_string(p));
    priority_ = p;
}

// {""} represents "no tags"; an empty vector is invalid.
void Entry::set_tags(const std::vector<std::string> &t) {
    if (t.empty())
        throw Entry_Error("Tags must not be empty");
    tags_ = t;
}

void Entry::set_time(const std::string &t) {
    if (!mado::common::is_timestamp(t))
        throw Entry_Error("Invalid timestamp: " + t);
    time_ = t;
}

void Entry::set_mtime(const std::string &t) {
    if (!mado::common::is_timestamp(t))
        throw Entry_Error("Invalid timestamp: " + t);
    mtime_ = t;
}

void Entry::set_deadline(const std::string &t) {
    if (!mado::common::is_timestamp(t))
        throw Entry_Error("Invalid timestamp: " + t);
    deadline_ = t;
}

} // namespace mado::entry
