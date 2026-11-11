#pragma once

#include <mado/entry/entry.hpp>

#include <memory>
#include <optional>
#include <ostream>
#include <string>

namespace cli {

/// Writes `str` as a JSON string literal to `os`, escaping JSON special bytes.
void print_json_string(const std::string &str, std::ostream &os);

enum class Entry_Info_Format {
    Default, // compatible with emacs compile buffer
    Path,    // only paths
    Jsonl,   // 1 line = 1 json
};

struct Hidden_Entry_Fields {
    bool path = false;
    bool time = false;
    bool mtime = false;
    bool name = false;
    bool priority = false;
    bool deadline = false;
    bool status = false;
    bool tags = false;
};

// Base class for all entry info formatters. Each formatter knows how
// to write one Entry to a stream in its own format.
class Entry_Info_Formatter {
  public:
    /// Allows derived formatters to be destroyed through the base interface.
    virtual ~Entry_Info_Formatter() = default;

    /// Writes one entry in the formatter's representation, omitting fields
    /// marked true in `hidden`.
    virtual void write(const mado::entry::Entry &e, std::ostream &os,
                       const Hidden_Entry_Fields &hidden = {}) const = 0;
};

/// Parses a format name using exact or fuzzy matching; returns nullopt if none
/// of the supported formats matches.
std::optional<Entry_Info_Format> parse_entry_info_format(const std::string &name);

/// Creates a formatter for a supported format; returns null if `fmt` is invalid.
std::unique_ptr<Entry_Info_Formatter> make_entry_info_formatter(Entry_Info_Format fmt);

} // namespace cli
