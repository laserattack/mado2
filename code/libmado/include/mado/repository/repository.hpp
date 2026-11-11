#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <mado/entry/entry.hpp>
#include <mado/query/ast.hpp>

namespace mado::repository {

inline constexpr const char *TEMPLATE_DIR_NAMES[] = {"templates", ".templates", "_templates"};

inline constexpr char ENTRY_DIR[] = "MADO";

inline constexpr const char *MTIMES_FILE_NAMES[] = {"mtimes", ".mtimes", "_mtimes"};

inline constexpr char MARKDOWN_EXTENSION[] = ".md";

inline const std::string ENTRY_FILE = std::string("MAIN") + MARKDOWN_EXTENSION;

class Repository_Error : public std::runtime_error {
  public:
    /// Creates a repository error with the supplied diagnostic message.
    explicit Repository_Error(const std::string &message)
        : std::runtime_error(message) {}
};

/// Describes an entry whose removal failed and preserves the OS error.
struct Remove_Failure {
    mado::entry::Entry entry;
    std::error_code error;
};

/// Contains entries removed successfully and entries whose removal failed.
struct Remove_Result {
    std::vector<mado::entry::Entry> removed;
    std::vector<Remove_Failure> failed;
};

class Repository {
  public:
    /// Finds a directory named exactly MADO/ starting from `from` and going up the tree.
    /// Returns std::nullopt if the starting path cannot be resolved
    /// or the search finds no repository.
    static std::optional<Repository> open(const std::filesystem::path &from);

    /// Opens a directory named exactly MADO/ directly inside `parent`.
    /// Returns std::nullopt if the path cannot be resolved or inspected,
    /// or if `parent` has no repository.
    static std::optional<Repository> open_at(const std::filesystem::path &parent);

    /// Initializes the nearest repository and restores its default templates.
    /// With `force`, creates a repository directly in `where` even if one
    /// exists above. Existing default template files are replaced when their
    /// contents differ from the built-in defaults.
    /// @throws Repository_Error If a directory with different casing prevents
    /// creating a directory named exactly MADO/.
    /// @throws std::runtime_error If a filesystem operation fails while
    /// resolving or inspecting the path, creating directories, or restoring templates.
    static Repository init(const std::filesystem::path &where, bool force = false);

    /// Loads entries matching the filter. Invalid entries are skipped.
    /// Returns an empty result if the repository cannot be opened for iteration.
    /// @throws std::bad_variant_access If an Any comparison has neither
    /// a numeric nor a string value.
    /// @throws std::runtime_error If advancing directory iteration, inspecting
    /// a directory entry, or reading an entry fails, or if an entry's path
    /// or modification time cannot be converted during loading or matching.
    std::vector<mado::entry::Entry> find(const mado::query::Ast_Node *filter) const;

    /// Removes each matching entry directory recursively. Successful removals
    /// are listed in `removed`; per-entry deletion failures are listed in
    /// `failed` with their OS error. Matching entries are loaded before any
    /// deletion occurs.
    /// @throws std::bad_variant_access If an Any comparison has neither
    /// a numeric nor a string value.
    /// @throws std::runtime_error If advancing directory iteration, inspecting
    /// a directory entry, or reading an entry fails, or if an entry's path
    /// or modification time cannot be converted during loading or matching.
    Remove_Result remove(const mado::query::Ast_Node *filter) const;

    /// Removes each matching entry directory recursively when `should_remove`
    /// returns true. The callback is called once for each matching entry before
    /// deletion; entries declined by the callback are omitted from the result.
    /// Successful removals are listed in `removed`; per-entry deletion failures
    /// are listed in `failed` with their OS error. Matching entries are loaded
    /// before any deletion occurs.
    /// @throws Any exception thrown by `should_remove`.
    /// @throws std::bad_variant_access If an Any comparison has neither
    /// a numeric nor a string value.
    /// @throws std::runtime_error If advancing directory iteration, inspecting
    /// a directory entry, or reading an entry fails, or if an entry's path
    /// or modification time cannot be converted during loading or matching.
    Remove_Result remove(
        const mado::query::Ast_Node *filter,
        const std::function<bool(const mado::entry::Entry &)> &should_remove) const;

    /// Creates and returns an entry with a directory name based on the current
    /// UTC time in YYYYMMDD-HHMMSS, optionally followed by `suffix`. Uses the
    /// built-in task template unless `template_name` selects a file from the
    /// repository's templates directory.
    /// @throws Repository_Error If the suffix or template name is invalid,
    /// the requested template is unavailable, the entry path already exists,
    /// or the entry file is missing or non-regular when loaded.
    /// @throws mado::entry::Entry_Error If the entry's creation or modification
    /// timestamp is invalid when loaded.
    /// @throws std::runtime_error If reading or converting the current time fails,
    /// or if a filesystem, I/O, path conversion, or UTC conversion error occurs
    /// while reading the template or creating or loading the entry.
    mado::entry::Entry create(
        const std::string &suffix = "",
        const std::optional<std::string> &template_name = std::nullopt) const;

    /// Calls `action` with each matching entry by value, stopping if it returns
    /// false. Invalid entries are skipped. If `action` throws Repository_Error
    /// or mado::entry::Entry_Error, iteration continues with the next entry.
    /// Returns without calling `action` if the repository cannot be opened
    /// for iteration.
    /// @throws std::bad_function_call If `action` is empty and an entry matches.
    /// @throws std::bad_variant_access If an Any comparison has neither
    /// a numeric nor a string value.
    /// @throws std::runtime_error If advancing directory iteration, inspecting
    /// a directory entry, or reading an entry fails, or if an entry's path
    /// or modification time cannot be converted during loading or matching.
    /// @throws ... Any other exception thrown by `action`.
    void for_each_matching(const mado::query::Ast_Node *filter,
                           std::function<bool(mado::entry::Entry)> action) const;

    /// Writes a sorted modification-time index for regular, non-symlink entry
    /// files and returns the destination path. An empty `file` selects the
    /// default index path in the repository root.
    /// @throws Repository_Error If the destination would overwrite an entry file,
    /// an entry identifier is invalid, or a modification time is outside the
    /// index format's range.
    /// @throws std::runtime_error If resolving the destination, inspecting
    /// entries, converting an entry path, or writing the index fails.
    std::filesystem::path dump_mtimes(const std::filesystem::path &file = {}) const;

    /// Restores entry file modification times from the index and returns its
    /// path. If `file` is empty, searches the supported index filenames in
    /// order. Missing entry files and symlinked entries or files are skipped
    /// unless `strict` is true. With `keep_newer`, existing mtimes later than
    /// the indexed time are preserved.
    /// @throws Repository_Error If the index header is unreadable or invalid,
    /// a record is invalid, a timestamp is outside the filesystem clock's range,
    /// or an entry file is non-regular. In strict mode, also throws for entries
    /// that would otherwise be skipped.
    /// @throws std::runtime_error If resolving or opening the index, reading
    /// its records, inspecting entry paths, or accessing modification times fails.
    std::filesystem::path restore_mtimes(const std::filesystem::path &file = {}, bool strict = false,
                        bool keep_newer = false) const;

    /// Returns the absolute path of this repository's MADO directory.
    const std::filesystem::path &root() const { return root_; }

  private:
    explicit Repository(std::filesystem::path root) : root_(std::move(root)) {}

    // Load one entry from MADO/<timestamp>/MAIN.md.
    mado::entry::Entry load_one(const std::filesystem::path &entry_dir) const;

    std::filesystem::path root_;
};

} // namespace mado::repository
