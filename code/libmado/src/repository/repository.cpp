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
#include <unordered_set>
#include <vector>

namespace mado::repository {

namespace {

constexpr const char *ENTRY_DIR = "MADO";
constexpr const char *ENTRY_FILE = "MAIN.md";
constexpr int MAX_HEADER_LINES = 30;

class Repository_Error : public std::runtime_error {
  public:
    explicit Repository_Error(const std::string &message)
        : std::runtime_error(message) {}
};

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
    std::unordered_set<std::string> seen;

    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string t = mado::common::trim(item);
        if (seen.insert(t).second)
            tags.push_back(t);
    }

    // getline does not produce a trailing empty token for "a,"
    if (!s.empty() && s.back() == ',') {
        if (seen.insert("").second)
            tags.push_back("");
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

Repository Repository::init(const std::filesystem::path &where, bool force) {
    std::error_code ec;

    const auto abs_where = std::filesystem::absolute(where, ec);
    if (ec)
        throw std::runtime_error("Cannot resolve path " + where.string() + ": " + ec.message());

    const auto mado_dir = abs_where / ENTRY_DIR;

    // Refuse if MADO/ already exists in `where` itself.
    const bool exists_here = std::filesystem::exists(mado_dir, ec);
    if (ec)
        throw std::runtime_error("Cannot check " + mado_dir.string() + ": " + ec.message());
    if (exists_here)
        throw Repository_Error("MADO/ already exists in " + abs_where.string());

    // Refuse if MADO/ is found above, unless force.
    if (!force) {
        if (open(abs_where.parent_path()))
            throw Repository_Error("MADO/ already exists above " + abs_where.string());
    }

    // Create MADO/.
    if (!std::filesystem::create_directory(mado_dir, ec)) {
        if (ec)
            throw std::runtime_error("Cannot create " + mado_dir.string() + ": " + ec.message());
    }

    return Repository(mado_dir);
}

std::vector<mado::entry::Entry> Repository::find(
    const mado::query::Ast_Node *filter) const {

    std::vector<mado::entry::Entry> result;
    for_each_matching(filter, [&](mado::entry::Entry e) {
        result.push_back(std::move(e));
        return true;
    });
    return result;
}

std::vector<mado::entry::Entry> Repository::remove(
    const mado::query::Ast_Node *filter) const {

    std::vector<mado::entry::Entry> result;
    for_each_matching(filter, [&](mado::entry::Entry e) {
        std::error_code ec;
        std::filesystem::remove_all(e.path().parent_path(), ec);
        // TODO: decide what to do when remove_all fails. Options:
        //   - skip silently (current)
        //   - throw std::filesystem::filesystem_error
        //   - return something like Remove_Result with both removed and failed entries
        if (!ec)
            result.push_back(std::move(e));
        return true;
    });
    return result;
}

template <class Action>
void Repository::for_each_matching(const mado::query::Ast_Node *filter,
                                   Action action) const {

    std::error_code ec;
    if (!std::filesystem::is_directory(root_, ec))
        return;

    mado::interpreter::Interpreter interp;

    for (const auto &dir_entry : std::filesystem::directory_iterator(root_, ec)) {
        if (ec)
            break;

        if (!dir_entry.is_directory())
            continue;

        try {
            mado::entry::Entry e = load_one(dir_entry.path());
            if (interp.evaluate(filter, e)) {
                if (!action(std::move(e)))
                    return;
            }
        } catch (const Repository_Error &) {
            // MAIN.md is missing or cannot be opened
        } catch (const mado::entry::Entry_Error &) {
            // The directory name is not a valid timestamp
        }
    }
}

// TODO: validate suffix?? what if the suffix contains / ?
mado::entry::Entry Repository::create(const std::string &suffix) const {
    std::string dir_name = mado::common::current_timestamp_utc();
    if (!suffix.empty())
        dir_name += suffix;

    const auto entry_dir = root_ / dir_name;
    const auto main_md = entry_dir / ENTRY_FILE;

    std::error_code ec;

    // Refuse if the directory already exists. This can happen if two
    // entries are created within the same second.
    const bool exists = std::filesystem::exists(entry_dir, ec);
    if (ec)
        throw std::runtime_error("Cannot check " + entry_dir.string() + ": " + ec.message());
    if (exists)
        throw std::runtime_error("Entry " + dir_name + " already exists; try again in a second");

    // Create MADO/<timestamp>/.
    std::filesystem::create_directory(entry_dir, ec);
    if (ec)
        throw std::runtime_error("Cannot create " + entry_dir.string() + ": " + ec.message());

    // Write the empty header to MAIN.md.
    std::ofstream out(main_md);
    if (!out)
        throw std::runtime_error("Cannot create " + main_md.string());

    out << "- NAME:\n"
        << "- PRIORITY:\n"
        << "- TAGS:\n"
        << "- STATUS:\n"
        << "- DEADLINE:\n";

    return load_one(entry_dir);
}

// Loads one entry from MADO/<timestamp>/.
//
// The entry directory name must be a valid timestamp; otherwise
// set_time throws Entry_Error. The directory must contain a regular
// file named MAIN.md, otherwise Repository_Error is thrown.
//
// The entry's TIME is taken from the directory name and MTIME from
// the modification time of MAIN.md.
//
// MAIN.md is parsed as a header of "- KEY: value" lines, at most
// MAX_HEADER_LINES of them. Only the first occurrence of each known
// field (NAME, TAGS, STATUS, PRIORITY, DEADLINE) is used; later
// duplicates are ignored. Unknown keys and lines that do not start
// with "- " are skipped. Invalid values (e.g. non-numeric PRIORITY,
// malformed DEADLINE) are ignored, and the field keeps its default.
//
// Errors:
//   - Repository_Error: MAIN.md is missing or cannot be opened.
//   - std::runtime_error: MAIN.md cannot be stat'ed;
//     this indicates an I/O problem, not a malformed entry.
//   - Entry_Error: the directory name is not a valid timestamp.
mado::entry::Entry Repository::load_one(const std::filesystem::path &entry_dir) const {
    mado::entry::Entry e;

    const auto main_md = entry_dir / ENTRY_FILE;

    std::ifstream in(main_md);
    if (!in)
        throw Repository_Error("Cannot open " + main_md.string());

    // Time comes from the directory name.
    std::string dir_name = entry_dir.filename().string();

    // Allow suffixes in dir names after timestamp part.
    if (dir_name.size() > 15)
        dir_name.resize(15);

    // Old format uses 'T' as the date/time separator - accept it too.
    if (dir_name.size() > 8 && dir_name[8] == 'T')
        dir_name[8] = '-';

    e.set_time(dir_name); // exception if not valid timestamp

    // mtime comes from the MAIN.md modification time.
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(main_md, ec);
    if (ec)
        throw std::runtime_error("Cannot stat " + main_md.string());

    e.set_mtime(std::format("{:%Y%m%d-%H%M%S}",
                            std::chrono::floor<std::chrono::seconds>(
                                file_time_to_system_clock(ftime))));

    e.set_path(main_md);

    std::string line;
    int line_number = 0;
    std::vector<std::string> tags;

    bool has_name = false;
    bool has_tags = false;
    bool has_status = false;
    bool has_priority = false;
    bool has_deadline = false;

    while (line_number < MAX_HEADER_LINES && std::getline(in, line)) {
        ++line_number;

        const std::string t = mado::common::trim(line);
        if (t.empty())
            continue;

        if (!t.starts_with("- "))
            continue;

        const auto colon = t.find(':');
        if (colon == std::string::npos)
            continue;

        const std::string key = mado::common::trim(t.substr(2, colon - 2));
        const std::string value = mado::common::trim(t.substr(colon + 1));

        if (key == "NAME" && !has_name) {
            e.set_name(value);
            has_name = true;
        } else if (key == "TAGS" && !has_tags) {
            e.set_tags(split_tags(value));
            has_tags = true;
        } else if (key == "STATUS" && !has_status) {
            e.set_status(value);
            has_status = true;
        } else if (key == "PRIORITY" && !has_priority) {
            if (auto p = parse_priority(value))
                e.set_priority(*p);
            has_priority = true;
        } else if (key == "DEADLINE" && !has_deadline) {
            if (mado::common::is_timestamp(value))
                e.set_deadline(value);
            has_deadline = true;
        }
    }

    return e;
}

} // namespace mado::repository
