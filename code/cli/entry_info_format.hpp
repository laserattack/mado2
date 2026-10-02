#pragma once

#include <mado/entry/entry.hpp>

#include <memory>
#include <optional>
#include <ostream>
#include <string>

namespace cli {

enum class Entry_Info_Format {
    Default, // compatible with emacs compile buffer
    Path,    // only paths
    Jsonl,   // 1 line = 1 json
};

// Base class for all entry info formatters. Each formatter knows how
// to write one Entry to a stream in its own format.
class Entry_Info_Formatter {
  public:
    virtual ~Entry_Info_Formatter() = default;

    virtual void write(const mado::entry::Entry &e, std::ostream &os) const = 0;
};

// Parses a format name. Returns nullopt if unknown.
std::optional<Entry_Info_Format> parse_entry_info_format(const std::string &name);

// Creates a formatter for the given format.
std::unique_ptr<Entry_Info_Formatter> make_entry_info_formatter(Entry_Info_Format fmt);

} // namespace cli
