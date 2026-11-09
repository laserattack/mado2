#include <mado/repository/repository.hpp>

#include <mado/common/text_utils.hpp>
#include <mado/common/time_utils.hpp>
#include <mado/interpreter/interpreter.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace mado::repository {

namespace {

constexpr const char *MTIMES_HEADER = "mado-mtimes-v1 unix-nanoseconds";

constexpr const char *INVALID_PATH_CHARS = "/\\:\r\n";

constexpr std::string_view NAME_PREFIX = "- NAME:";

constexpr std::string_view PRIORITY_PREFIX = "- PRIORITY:";

constexpr std::string_view TAGS_PREFIX = "- TAGS:";

constexpr std::string_view STATUS_PREFIX = "- STATUS:";

constexpr std::string_view DEADLINE_PREFIX = "- DEADLINE:";

constexpr int MAX_HEADER_LINES = 30;

const std::string INVALID_SUFFIX_CHARS = std::string(INVALID_PATH_CHARS) + '.';

const std::string NOTE_TEMPLATE_FILE = std::string("note") + MARKDOWN_EXTENSION;
const std::string TASK_TEMPLATE_FILE = std::string("task") + MARKDOWN_EXTENSION;

const std::string TASK_TEMPLATE = std::string(NAME_PREFIX) + "\n" +
                                  std::string(PRIORITY_PREFIX) + "\n" + std::string(TAGS_PREFIX) + "\n" +
                                  std::string(STATUS_PREFIX) + "\n" + std::string(DEADLINE_PREFIX) + "\n";
const std::string NOTE_TEMPLATE = std::string(NAME_PREFIX) + "\n" + std::string(TAGS_PREFIX) + "\n";

class Repository_Error : public std::runtime_error {
  public:
    explicit Repository_Error(const std::string &message)
        : std::runtime_error(message) {}
};

std::string describe_invalid_chars(std::string_view chars) {
    std::string result;

    for (char c : chars) {
        if (!result.empty())
            result += ", ";

        result += '\'';
        switch (c) {
        case '\r':
            result += "\\r";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\'':
            result += "\\'";
            break;
        default:
            result += c;
            break;
        }
        result += '\'';
    }

    return result;
}

std::optional<std::filesystem::path> find_templates(const std::filesystem::path &root) {
    for (const auto *name : TEMPLATE_DIR_NAMES) {
        auto dir = root / name;
        if (std::filesystem::is_directory(dir))
            return dir;
    }
    return std::nullopt;
}

void write_default_template(const std::filesystem::path &path, const std::string &content) {
    {
        std::ifstream in(path, std::ios::binary);
        if (in && std::string(std::istreambuf_iterator<char>(in), {}) == content)
            return;
    }
    std::ofstream out(path, std::ios::binary);
    out << content;
    out.close();
    if (!out)
        throw std::runtime_error("Cannot write template " + path.string());
}

std::chrono::system_clock::time_point
file_time_to_system_clock(std::filesystem::file_time_type ft) {
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        std::filesystem::file_time_type::clock::to_sys(ft));
}

std::vector<std::string> split_tags(const std::string &s) {
    std::vector<std::string> tags;
    std::unordered_set<std::string> seen;

    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        mado::common::trim_inplace(item);
        if (seen.insert(item).second)
            tags.push_back(item);
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

    auto mado_dir = abs_where / ENTRY_DIR;
    if (!force && !std::filesystem::exists(mado_dir)) {
        if (auto repo = open(abs_where))
            mado_dir = repo->root();
    }
    std::filesystem::create_directory(mado_dir);

    const auto templates = find_templates(mado_dir).value_or(mado_dir / TEMPLATE_DIR_NAMES[0]);
    std::filesystem::create_directory(templates);
    write_default_template(templates / NOTE_TEMPLATE_FILE, NOTE_TEMPLATE);
    write_default_template(templates / TASK_TEMPLATE_FILE, TASK_TEMPLATE);
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

Remove_Result Repository::remove(const mado::query::Ast_Node *filter) const {
    auto entries = find(filter);
    Remove_Result result;
    result.removed.reserve(entries.size());
    result.failed.reserve(entries.size());

    for (auto &entry : entries) {
        std::error_code error;
        std::filesystem::remove_all(entry.path().parent_path(), error);

        if (error)
            result.failed.push_back({std::move(entry), error});
        else
            result.removed.push_back(std::move(entry));
    }

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
            // MAIN.md is missing or is not a regular file.
        } catch (const mado::entry::Entry_Error &) {
            // The directory name is not a valid timestamp
        }
    }
}

mado::entry::Entry Repository::create(
    const std::string &suffix, const std::optional<std::string> &template_name) const {
    if (suffix.find_first_of(INVALID_SUFFIX_CHARS) != std::string::npos)
        throw Repository_Error("Invalid suffix '" + suffix + "': must not contain " +
                               describe_invalid_chars(INVALID_SUFFIX_CHARS));

    std::string content = TASK_TEMPLATE;
    if (template_name) {
        if (template_name->empty() || template_name->find_first_of(INVALID_PATH_CHARS) != std::string::npos ||
            template_name->find('\0') != std::string::npos)
            throw Repository_Error("Invalid template name: " + *template_name);

        const auto templates = find_templates(root_);
        if (!templates)
            throw Repository_Error("No templates directory found. Run 'mado init' to create it.");

        const auto path = *templates / (*template_name + MARKDOWN_EXTENSION);
        if (!std::filesystem::is_regular_file(path))
            throw Repository_Error("Template '" + *template_name +
                                   "' not found. Run 'mado init' to restore default templates.");
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("Cannot read template " + path.string());
        std::ostringstream buffer;
        buffer << in.rdbuf();
        if (buffer.bad() || in.bad())
            throw std::runtime_error("Cannot read template " + path.string());
        content = std::move(buffer).str();
    }

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
        throw Repository_Error("Entry " + dir_name + " already exists; try again in a second");

    // Create MADO/<timestamp>/.
    const bool created = std::filesystem::create_directory(entry_dir, ec);
    if (ec)
        throw std::runtime_error("Cannot create " + entry_dir.string() + ": " + ec.message());
    if (!created)
        throw Repository_Error("Entry " + dir_name + " already exists; try again in a second");

    std::ofstream out(main_md, std::ios::binary);
    out << content;
    out.close();
    if (!out) {
        std::filesystem::remove(main_md, ec);
        std::filesystem::remove(entry_dir, ec);
        throw std::runtime_error("Cannot write " + main_md.string());
    }

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
//   - Repository_Error: MAIN.md is missing or is not a regular file.
//   - std::runtime_error: an I/O operation on MAIN.md failed.
//   - Entry_Error: the directory name is not a valid timestamp.
mado::entry::Entry Repository::load_one(const std::filesystem::path &entry_dir) const {
    mado::entry::Entry e;

    auto main_md = entry_dir / ENTRY_FILE;

    std::ifstream in(main_md);
    if (!in) {
        if (!std::filesystem::is_regular_file(main_md))
            throw Repository_Error("Missing regular file " + main_md.string());
        throw std::runtime_error("Cannot open " + main_md.string());
    }

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
        throw std::runtime_error("Cannot stat " + main_md.string() + ": " + ec.message());

    e.set_mtime(mado::common::format_timestamp_utc(
        std::chrono::system_clock::to_time_t(
            std::chrono::floor<std::chrono::seconds>(
                file_time_to_system_clock(ftime)))));

    e.set_path(std::move(main_md));

    std::string line;
    int line_number = 0;

    bool has_name = false;
    bool has_tags = false;
    bool has_status = false;
    bool has_priority = false;
    bool has_deadline = false;

    while (line_number < MAX_HEADER_LINES && std::getline(in, line)) {
        ++line_number;

        if (has_name && has_tags && has_status && has_priority && has_deadline)
            break;

        mado::common::trim_inplace(line);
        if (line.empty())
            continue;

        if (!has_name && line.starts_with(NAME_PREFIX)) {
            line.erase(0, NAME_PREFIX.size());
            mado::common::trim_inplace(line);
            e.set_name(std::move(line));
            has_name = true;
        } else if (!has_tags && line.starts_with(TAGS_PREFIX)) {
            line.erase(0, TAGS_PREFIX.size());
            mado::common::trim_inplace(line);
            e.set_tags(split_tags(line));
            has_tags = true;
        } else if (!has_status && line.starts_with(STATUS_PREFIX)) {
            line.erase(0, STATUS_PREFIX.size());
            mado::common::trim_inplace(line);
            e.set_status(std::move(line));
            has_status = true;
        } else if (!has_priority && line.starts_with(PRIORITY_PREFIX)) {
            line.erase(0, PRIORITY_PREFIX.size());
            mado::common::trim_inplace(line);
            if (auto p = parse_priority(line))
                e.set_priority(*p);
            has_priority = true;
        } else if (!has_deadline && line.starts_with(DEADLINE_PREFIX)) {
            line.erase(0, DEADLINE_PREFIX.size());
            mado::common::trim_inplace(line);
            if (mado::common::is_timestamp(line))
                e.set_deadline(std::move(line));
            has_deadline = true;
        }
    }

    if (in.bad()) {
        if (!std::filesystem::is_regular_file(e.path()))
            throw Repository_Error("Missing regular file " + e.path().string());
        throw std::runtime_error("Cannot read " + e.path().string());
    }

    return e;
}

std::filesystem::path Repository::dump_mtimes(const std::filesystem::path &file) const {
    const auto destination = std::filesystem::absolute(
        file.empty() ? root_ / MTIMES_FILE_NAMES[0] : file);
    std::vector<std::pair<std::string, int64_t>> records;

    for (const auto &dir : std::filesystem::directory_iterator(root_)) {
        const auto main = dir.path() / ENTRY_FILE;

        if (dir.is_symlink() || !dir.is_directory() ||
            std::filesystem::is_symlink(main) || !std::filesystem::is_regular_file(main))
            continue;

        // Check that the dump destination does not refer to any entry's MAIN.md.
        if (std::filesystem::exists(destination) && std::filesystem::equivalent(destination, main))
            throw Repository_Error("Mtime index cannot overwrite entry " + main.string());

        // Check whether mtime fits the index format. The index stores time as
        // a signed 64-bit count of nanoseconds since the Unix epoch (int64_t).
        const auto file_time = std::filesystem::last_write_time(main);
        const auto wide_time = std::filesystem::file_time_type::clock::to_sys(
            std::chrono::time_point<std::filesystem::file_time_type::clock,
                                    std::chrono::duration<long double>>{file_time});
        const auto unix_ticks = std::chrono::duration<long double, std::nano>(
                                    wide_time.time_since_epoch())
                                    .count();
        if (unix_ticks <= static_cast<long double>(std::numeric_limits<int64_t>::lowest()) ||
            unix_ticks >= static_cast<long double>(std::numeric_limits<int64_t>::max()))
            throw Repository_Error("Mtime outside index range for " + main.string());

        const auto time = std::filesystem::file_time_type::clock::to_sys(file_time);

        const auto id = dir.path().filename().string();
        if (id.find_first_of(INVALID_PATH_CHARS) != std::string::npos)
            throw Repository_Error("Invalid entry identifier " + dir.path().string());

        records.emplace_back(id,
                             std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count());
    }

    std::sort(records.begin(), records.end());

    std::ofstream out(destination, std::ios::binary);
    out << MTIMES_HEADER << '\n';

    for (const auto &[id, time] : records)
        out << std::quoted(id) << '\t' << time << '\n';

    out.close();
    if (!out)
        throw std::runtime_error("Cannot write mtime index " + destination.string());

    return destination;
}

std::filesystem::path Repository::restore_mtimes(const std::filesystem::path &file, bool strict, bool keep_newer) const {
    auto source = std::filesystem::absolute(
        file.empty() ? root_ / MTIMES_FILE_NAMES[0] : file);

    if (file.empty()) {
        for (const auto *name : MTIMES_FILE_NAMES) {
            const auto candidate = root_ / name;
            if (std::filesystem::is_regular_file(candidate)) {
                source = candidate;
                break;
            }
        }
    }

    std::ifstream in(source, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot read mtime index " + source.string());

    std::string line;
    if (!std::getline(in, line) || line != MTIMES_HEADER)
        throw Repository_Error("Invalid mtime index header in " + source.string());

    std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> records;
    std::unordered_set<std::string> seen;

    while (std::getline(in, line)) {
        std::istringstream row(line);
        std::string id;
        int64_t ticks;
        if (
            // Read the entry ID into id and its modification time into ticks.
            !(row >> std::quoted(id) >> ticks) || !(row >> std::ws).eof() ||
            // Validate the entry ID.
            id.empty() || id == "." || id == ".." ||
            id.find_first_of(INVALID_PATH_CHARS) != std::string::npos ||
            // Has this ID already appeared?
            !seen.insert(id).second)
            throw Repository_Error("Invalid mtime index record in " + source.string());

        const auto dir = root_ / id;
        const auto main = dir / ENTRY_FILE;

        // last_write_time follows symlinks and would modify the target file.
        // Reject symlinks to prevent that.
        if (std::filesystem::is_symlink(dir) || std::filesystem::is_symlink(main))
            throw Repository_Error("Invalid symbolic link entry " + main.string());

        if (!std::filesystem::exists(main) && !strict)
            continue;
        if (!std::filesystem::is_regular_file(main))
            throw Repository_Error("Missing regular entry file " + main.string());

        // Check whether the entry's modification time fits the current system's
        // file clock.
        using File_Duration = std::filesystem::file_time_type::duration;
        const auto wide_time = std::filesystem::file_time_type::clock::from_sys(
            std::chrono::sys_time<std::chrono::duration<long double>>{
                std::chrono::duration<long double>{std::chrono::nanoseconds{ticks}}});
        const auto native_ticks = std::chrono::duration<long double, File_Duration::period>(
                                      wide_time.time_since_epoch())
                                      .count();
        if (native_ticks <= static_cast<long double>(std::numeric_limits<File_Duration::rep>::lowest()) ||
            native_ticks >= static_cast<long double>(std::numeric_limits<File_Duration::rep>::max()))
            throw Repository_Error("Mtime outside filesystem clock range in " + source.string());

        const auto time = std::filesystem::file_time_type::clock::from_sys(
            std::chrono::sys_time<std::chrono::nanoseconds>{std::chrono::nanoseconds{ticks}});

        // Save the entry's file path and its new modification time.
        records.emplace_back(main, time);
    }

    if (in.bad())
        throw std::runtime_error("Cannot read mtime index " + source.string());

    // Update modification times.
    for (const auto &[main, time] : records) {
        if (keep_newer && std::filesystem::last_write_time(main) > time)
            continue;

        std::filesystem::last_write_time(main, time);
    }

    return source;
}

} // namespace mado::repository
