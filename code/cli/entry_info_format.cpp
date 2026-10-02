#include "entry_info_format.hpp"

#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace cli {

namespace {

class Default_Entry_Info_Formatter : public Entry_Info_Formatter {
  public:
    void write(const mado::entry::Entry &e, std::ostream &os) const override {
        os << e.path().string() << ":1:";

        // fields
        os << " PATH:[" << e.path().string() << "]";
        os << " TIME:[" << e.time() << "]";
        os << " MTIME:[" << e.mtime() << "]";
        os << " NAME:[" << e.name() << "]";
        os << " PRIORITY:[" << e.priority() << "]";
        os << " DEADLINE:[" << e.deadline() << "]";
        os << " STATUS:[" << e.status() << "]";
        os << " TAGS:[";
        bool first = true;
        for (const auto &tag : e.tags()) {
            if (!first)
                os << ",";
            os << tag;
            first = false;
        }
        os << "]";
        //

        os << "\n";
    }
};

class Path_Entry_Info_Formatter : public Entry_Info_Formatter {
  public:
    void write(const mado::entry::Entry &e, std::ostream &os) const override {
        os << e.path().string() << "\n";
    }
};

class Jsonl_Entry_Info_Formatter : public Entry_Info_Formatter {
  public:
    void write(const mado::entry::Entry &e, std::ostream &os) const override {
        (void)e;
        (void)os;
        // TODO: impl
    }
};

//

struct Format_Info {
    Entry_Info_Format format;
    const char *name;
    std::unique_ptr<Entry_Info_Formatter> (*make)();
};

std::unique_ptr<Entry_Info_Formatter> make_default() {
    return std::make_unique<Default_Entry_Info_Formatter>();
}

std::unique_ptr<Entry_Info_Formatter> make_path() {
    return std::make_unique<Path_Entry_Info_Formatter>();
}

std::unique_ptr<Entry_Info_Formatter> make_jsonl() {
    return std::make_unique<Jsonl_Entry_Info_Formatter>();
}

const std::vector<Format_Info> FORMATS = {
    {Entry_Info_Format::Default, "default", make_default},
    {Entry_Info_Format::Path, "path", make_path},
    {Entry_Info_Format::Jsonl, "jsonl", make_jsonl},
};

} // namespace

std::optional<Entry_Info_Format> parse_entry_info_format(const std::string &name) {
    for (const auto &f : FORMATS) {
        if (name == f.name)
            return f.format;
    }
    return std::nullopt;
}

std::unique_ptr<Entry_Info_Formatter> make_entry_info_formatter(Entry_Info_Format fmt) {
    for (const auto &f : FORMATS) {
        if (fmt == f.format)
            return f.make();
    }
    return nullptr;
}

} // namespace cli
