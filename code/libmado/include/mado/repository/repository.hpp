#pragma once

#include <filesystem>
#include <functional>
#include <optional>
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

struct Remove_Failure {
    mado::entry::Entry entry;
    std::error_code error;
};

struct Remove_Result {
    std::vector<mado::entry::Entry> removed;
    std::vector<Remove_Failure> failed;
};

class Repository {
  public:
    // Find MADO/ starting from `from`, going up the tree.
    static std::optional<Repository> open(const std::filesystem::path &from);

    // Initializes the nearest repository and restores its default templates.
    // With force, initializes a repository in `where` even if one exists above.
    static Repository init(const std::filesystem::path &where, bool force = false);

    // Load entries matching the filter.
    // Invalid entries are skipped. Only matching entries are kept in memory.
    std::vector<mado::entry::Entry> find(const mado::query::Ast_Node *filter) const;

    Remove_Result remove(const mado::query::Ast_Node *filter) const;

    // The directory name is the current UTC time in YYYYMMDD-HHMMSS,
    // optionally followed by <suffix>.
    // Returns the created Entry.
    mado::entry::Entry create(
        const std::string &suffix = "",
        const std::optional<std::string> &template_name = std::nullopt) const;

    // Iterates over entries matching the filter and calls `action` for
    // each. The action receives the entry by value, so it can move it
    // out. If the action returns false, iteration stops early.
    void for_each_matching(const mado::query::Ast_Node *filter,
                           std::function<bool(mado::entry::Entry)> action) const;

    std::filesystem::path dump_mtimes(const std::filesystem::path &file = {}) const;

    std::filesystem::path restore_mtimes(const std::filesystem::path &file = {}, bool strict = false,
                        bool keep_newer = false) const;

    const std::filesystem::path &root() const { return root_; }

  private:
    explicit Repository(std::filesystem::path root) : root_(std::move(root)) {}

    // Load one entry from MADO/<timestamp>/MAIN.md.
    mado::entry::Entry load_one(const std::filesystem::path &entry_dir) const;

    std::filesystem::path root_;
};

} // namespace mado::repository
