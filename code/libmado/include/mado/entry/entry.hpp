#pragma once

#include <mado/common/time_utils.hpp>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace mado::entry {

inline constexpr uint16_t MAX_PRIORITY = 999;


/// Reports invalid entry values or entry data.
class Entry_Error : public std::runtime_error {
  public:
    /// Creates an error with the supplied diagnostic message.
    explicit Entry_Error(const std::string &message)
        : std::runtime_error(message) {}
};

/// Holds the editable metadata for one repository entry. A default instance
/// has empty path/name/status, priority 0, no tags (`{ "" }`), and all three
/// timestamps set to `NEVER_TIMESTAMP`.
class Entry {
  public:
    /// Returns the entry's filesystem path.
    // Getters
    const std::filesystem::path &path() const { return path_; }
    /// Returns the entry's display name.
    const std::string &name() const { return name_; }
    /// Returns the entry's status text.
    const std::string &status() const { return status_; }
    /// Returns the entry's priority in the range [0, MAX_PRIORITY].
    uint16_t priority() const { return priority_; }
    /// Returns the entry's tags; `{ "" }` represents no tags.
    const std::vector<std::string> &tags() const { return tags_; }
    /// Returns the entry's creation timestamp.
    const std::string &time() const { return time_; }
    /// Returns the entry's modification timestamp.
    const std::string &mtime() const { return mtime_; }
    /// Returns the entry's deadline timestamp.
    const std::string &deadline() const { return deadline_; }

    // Setters
    /// Replaces the entry's path without validation.
    void set_path(std::filesystem::path p);
    /// Replaces the entry's display name without validation.
    void set_name(std::string n);
    /// Replaces the entry's status text without validation.
    void set_status(std::string s);

    /// Sets the priority.
    /// @throws Entry_Error If `p` exceeds MAX_PRIORITY.
    void set_priority(uint16_t p);

    /// Replaces the tags. `{ "" }` represents no tags.
    /// @throws Entry_Error If the tag list is empty.
    void set_tags(std::vector<std::string> t);

    /// Sets the creation timestamp.
    /// @throws Entry_Error If `t` is not a valid timestamp.
    void set_time(std::string t);

    /// Sets the modification timestamp.
    /// @throws Entry_Error If `t` is not a valid timestamp.
    void set_mtime(std::string t);

    /// Sets the deadline timestamp.
    /// @throws Entry_Error If `t` is not a valid timestamp.
    void set_deadline(std::string t);

  private:
    std::filesystem::path path_ = "";
    std::string name_ = "";
    std::string status_ = "";
    uint16_t priority_ = 0;
    std::vector<std::string> tags_ = {""};
    std::string time_ = mado::common::NEVER_TIMESTAMP;
    std::string mtime_ = mado::common::NEVER_TIMESTAMP;
    std::string deadline_ = mado::common::NEVER_TIMESTAMP;
};

} // namespace mado::entry
