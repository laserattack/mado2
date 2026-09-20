#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <mado/query/ast.hpp>

namespace mado::interpreter {

class Entry_Error : public std::runtime_error {
  public:
    explicit Entry_Error(const std::string &message)
        : std::runtime_error(message) {}
};

class Entry {
  public:
    // Returns true if the entry matches the filter (or filter is nullptr)
    bool match_filter(const mado::query::Ast_Node *filter) const;

    // Getters
    const std::filesystem::path &path() const { return path_; }
    const std::string &name() const { return name_; }
    const std::string &status() const { return status_; }
    uint16_t priority() const { return priority_; }
    const std::vector<std::string> &tags() const { return tags_; }
    const std::string &time() const { return time_; }
    const std::string &mtime() const { return mtime_; }
    const std::string &deadline() const { return deadline_; }

    // Setters
    void set_path(const std::filesystem::path &p);
    void set_name(const std::string &n);
    void set_status(const std::string &s);
    void set_priority(uint16_t p);
    void set_tags(const std::vector<std::string> &t);
    void set_time(const std::string &t);
    void set_mtime(const std::string &t);
    void set_deadline(const std::string &t);

  private:
    std::filesystem::path path_ = "";
    std::string name_ = "";
    std::string status_ = "";
    uint16_t priority_ = 0;
    std::vector<std::string> tags_ = {""};
    std::string time_ = "99990101-000000";
    std::string mtime_ = "99990101-000000";
    std::string deadline_ = "99990101-000000";
};

} // namespace mado::interpreter
