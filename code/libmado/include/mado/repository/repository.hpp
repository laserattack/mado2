#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <mado/entry/entry.hpp>
#include <mado/query/ast.hpp>

namespace mado::repository {

class Repository {
  public:
    // Find MADO/ starting from `from`, going up the tree.
    static std::optional<Repository> open(const std::filesystem::path &from);

    // Creates MADO/ in `where`. Returns the opened Repository.
    //
    // Throws:
    //   - Repository_Error: MADO/ already exists in `where`, or it is
    //     found above `where` and `force` is false.
    //   - std::runtime_error: the directory cannot be resolved or
    //     created (I/O problem).
    static Repository init(const std::filesystem::path &where, bool force = false);

    // Load entries matching the filter.
    // Invalid entries are skipped. Only matching entries are kept in memory.
    std::vector<mado::entry::Entry> find(const mado::query::Ast_Node *filter) const;

    // Finds entries matching the filter, removes their directories, and
    // returns the removed entries.
    std::vector<mado::entry::Entry> remove(const mado::query::Ast_Node *filter) const;

    // Creates a new entry in MADO/ with an empty header:
    //
    //   - NAME:
    //   - PRIORITY:
    //   - TAGS:
    //   - STATUS:
    //   - DEADLINE:
    //
    // The directory name is the current UTC time in YYYYMMDD-HHMMSS,
    // optionally followed by <suffix>.
    // Returns the created Entry.
    //
    // Throws:
    //   - std::runtime_error: cannot create the directory or file, or
    //     the generated name collides with an existing one.
    mado::entry::Entry create(const std::string &suffix = "") const;

    // Iterates over entries matching the filter and calls `action` for
    // each. The action receives the entry by value, so it can move it
    // out. If the action returns false, iteration stops early.
    template <class Action>
    void for_each_matching(const mado::query::Ast_Node *filter, Action action) const;

    const std::filesystem::path &root() const { return root_; }

  private:
    explicit Repository(std::filesystem::path root) : root_(std::move(root)) {}

    // Load one entry from MADO/<timestamp>/MAIN.md.
    mado::entry::Entry load_one(const std::filesystem::path &entry_dir) const;

    std::filesystem::path root_;
};

} // namespace mado::repository
