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
    // Writes a JSON string literal to os.
    static void print_json_string(const std::string &str, std::ostream &os) {
        os << '"';
        for (char c : str) {
            switch (c) {
            case '"':
                os << "\\\"";
                break;
            case '\\':
                os << "\\\\";
                break;
            default:
                os << c;
            }
        }
        os << '"';
    }

  public:
    void write(const mado::entry::Entry &e, std::ostream &os) const override {
        os << "{";

        bool has_any = false;
        auto sep = [&]() {
            if (has_any)
                os << ",";
            has_any = true;
        };

        sep();
        os << "\"path\":";
        print_json_string(e.path().string(), os);

        sep();
        os << "\"time\":";
        print_json_string(e.time(), os);

        sep();
        os << "\"mtime\":";
        print_json_string(e.mtime(), os);

        sep();
        os << "\"name\":";
        print_json_string(e.name(), os);

        sep();
        os << "\"priority\":" << e.priority();

        sep();
        os << "\"deadline\":";
        print_json_string(e.deadline(), os);

        sep();
        os << "\"status\":";
        print_json_string(e.status(), os);

        sep();
        os << "\"tags\":[";
        bool first_tag = true;
        for (const auto &tag : e.tags()) {
            if (!first_tag)
                os << ",";
            print_json_string(tag, os);
            first_tag = false;
        }
        os << "]";

        os << "}\n";
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
