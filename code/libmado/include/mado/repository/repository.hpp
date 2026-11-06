#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <mado/entry/entry.hpp>
#include <mado/query/ast.hpp>

namespace mado::repository {

// Represents an opened MADO directory and manages its entries and templates.
class Repository {
  public:
    // Finds the nearest MADO directory from `from`. Returns nullopt if
    // none is found or the path cannot be resolved. May throw std::bad_alloc.
    static std::optional<Repository> open(const std::filesystem::path &from);

    // Initializes the nearest repository and restores its default templates.
    // With force, initializes in `where` even if one exists above.
    // I/O failures throw std::runtime_error or filesystem_error.
    static Repository init(const std::filesystem::path &where, bool force = false);

    // Load entries matching the filter.
    // Skips invalid entries. I/O and allocation exceptions propagate.
    std::vector<mado::entry::Entry> find(const mado::query::Ast_Node *filter) const;

    // Finds entries matching the filter, removes their directories, and
    // returns successful removals. Skips failed deletions. I/O failures
    // while loading and allocation exceptions can propagate.
    std::vector<mado::entry::Entry> remove(const mado::query::Ast_Node *filter) const;

    // Creates a UTC timestamped entry with an optional suffix. Uses a named
    // template when supplied and the built-in task header otherwise.
    // Invalid names, missing templates, and collisions throw Repository_Error.
    // I/O failures throw std::runtime_error or filesystem_error.
    mado::entry::Entry create(
        const std::string &suffix = "",
        const std::optional<std::string> &template_name = std::nullopt) const;

    // Iterates over entries matching the filter and calls `action` for
    // each. The action receives the entry by value, so it can move it
    // out. False stops iteration. I/O and callback exceptions propagate.
    template <class Action>
    // Visits matching entries until the action returns false. Skips malformed
    // entries. I/O and callback exceptions can propagate.
    void for_each_matching(const mado::query::Ast_Node *filter, Action action) const;

    // Returns the MADO directory. No exceptions.
    const std::filesystem::path &root() const { return root_; }

  private:
    // Represents an opened MADO directory and manages its entries and
    // templates.
    // Takes ownership of the resolved repository path. No exceptions.
    explicit Repository(std::filesystem::path root) : root_(std::move(root)) {}

    // Loads metadata from MAIN.md. Missing files throw Repository_Error,
    // invalid timestamps throw Entry_Error, and I/O failures throw runtime_error.
    mado::entry::Entry load_one(const std::filesystem::path &entry_dir) const;

    std::filesystem::path root_;
};

} // namespace mado::repository
