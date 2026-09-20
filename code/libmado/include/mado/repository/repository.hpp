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

    // Load entries matching the filter (nullptr = all).
    // Invalid entries are skipped. Only matching entries are kept in memory.
    std::vector<Entry> find(const mado::query::Ast_Node *filter) const;

    const std::filesystem::path &root() const { return root_; }

  private:
    explicit Repository(std::filesystem::path root) : root_(std::move(root)) {}

    // Load one entry from MADO/<timestamp>/MAIN.md.
    Entry load_one(const std::filesystem::path &entry_dir) const;

    std::filesystem::path root_;
};

} // namespace mado::repository
