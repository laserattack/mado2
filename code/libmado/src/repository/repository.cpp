#include <mado/repository/repository.hpp>

#include <mado/common/text_utils.hpp>
#include <mado/common/time_utils.hpp>
#include <mado/interpreter/interpreter.hpp>

#include <chrono>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mado::repository {

namespace {

constexpr const char *ENTRY_DIR = "MADO";
constexpr const char *ENTRY_FILE = "MAIN.md";
constexpr int MAX_HEADER_LINES = 30;

// Converts a std::filesystem::file_time_type to
// std::chrono::system_clock::time_point.
//
// file_clock and system_clock have different epochs and durations,
// and C++17 provides no direct conversion. The workaround: take the
// difference between the given time point and "now" in the file clock
// (this is a duration and does not depend on the epoch), then apply
// that duration to "now" in the system clock.
std::chrono::system_clock::time_point
file_time_to_system_clock(std::filesystem::file_time_type ft) {
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ft - std::filesystem::file_time_type::clock::now() +
        std::chrono::system_clock::now());
}

std::vector<std::string> split_tags(const std::string &s) {
    std::vector<std::string> tags;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string t = mado::common::trim(item);
        if (!t.empty())
            tags.push_back(t);
    }

    if (tags.empty())
        tags.push_back("");

    return tags;
}

// Parses "0".."999" into uint16_t. Returns std::nullopt on any error.
std::optional<uint16_t> parse_priority(const std::string &s) {
    if (s.empty() || s.size() > 3)
        return std::nullopt;

    for (char c : s) {
        if (!mado::common::is_digit(c))
            return std::nullopt;
    }

    return static_cast<uint16_t>(std::stoi(s));
}

} // namespace

std::optional<Repository> Repository::open(const std::filesystem::path &from) {
    std::error_code ec;

    std::filesystem::path dir = std::filesystem::absolute(from, ec);
    if (ec)
        return std::nullopt;

    if (!std::filesystem::is_directory(dir, ec))
        dir = dir.parent_path();

    while (!dir.empty()) {
        auto candidate = dir / ENTRY_DIR;
        if (std::filesystem::is_directory(candidate, ec))
            return Repository(candidate);

        auto parent = dir.parent_path();
        if (parent == dir)
            break;
        dir = parent;
    }

    return std::nullopt;
}

std::vector<mado::entry::Entry> Repository::find(const mado::query::Ast_Node *filter) const {
    std::vector<mado::entry::Entry> result;

    std::error_code ec;
    if (!std::filesystem::is_directory(root_, ec))
        return result;

    mado::interpreter::Interpreter interp;

    for (const auto &dir_entry : std::filesystem::directory_iterator(root_, ec)) {
        if (ec)
            break;

        if (!dir_entry.is_directory())
            continue;

        const std::string name = dir_entry.path().filename().string();

        // Entry directories are named after a timestamp
        if (!mado::common::is_timestamp(name))
            continue;

        try {
            mado::entry::Entry e = load_one(dir_entry.path());
            if (interp.evaluate(filter, e))
                result.push_back(std::move(e));
        } catch (const std::exception &) {
            // Skip invalid entries
        }
    }

    return result;
}

mado::entry::Entry Repository::load_one(const std::filesystem::path &entry_dir) const {
    mado::entry::Entry e;

    const auto main_md = entry_dir / ENTRY_FILE;

    std::error_code ec;
    if (!std::filesystem::is_regular_file(main_md, ec))
        throw std::runtime_error("No " + std::string(ENTRY_FILE) +
                                 " in " + entry_dir.string());

    // Time comes from the directory name, which is a valid timestamp.
    e.set_time(entry_dir.filename().string());

    // mtime comes from the MAIN.md modification time.
    auto ftime = std::filesystem::last_write_time(main_md, ec);
    if (ec)
        throw std::runtime_error("Cannot stat " + main_md.string());

    e.set_mtime(std::format("{:%Y%m%d-%H%M%S}",
                            std::chrono::floor<std::chrono::seconds>(
                                file_time_to_system_clock(ftime))));

    e.set_path(main_md);

    std::ifstream in(main_md);
    if (!in)
        throw std::runtime_error("Cannot open " + main_md.string());

    std::string line;
    int line_number = 0;
    std::vector<std::string> tags = {""};

    while (line_number < MAX_HEADER_LINES && std::getline(in, line)) {
        ++line_number;

        const std::string t = mado::common::trim(line);
        if (t.empty())
            continue;

        // Header lines look like "- KEY: value".
        if (!t.starts_with("- "))
            continue;

        const auto colon = t.find(':');
        if (colon == std::string::npos)
            continue;

        const std::string key = mado::common::trim(t.substr(2, colon - 2));
        const std::string value = mado::common::trim(t.substr(colon + 1));

        if (key == "NAME") {
            e.set_name(value);
        } else if (key == "TAGS") {
            e.set_tags(split_tags(value));
        } else if (key == "STATUS") {
            e.set_status(value);
        } else if (key == "PRIORITY") {
            if (auto p = parse_priority(value))
                e.set_priority(*p);
        } else if (key == "DEADLINE") {
            if (mado::common::is_timestamp(value))
                e.set_deadline(value);
        }
    }

    return e;
}

} // namespace mado::repository
