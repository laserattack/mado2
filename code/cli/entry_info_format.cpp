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
    void write(const mado::entry::Entry &e, std::ostream &os,
               const Hidden_Entry_Fields &hidden) const override {
        os << e.path().string() << ":1:";

        // fields
        if (!hidden.path)
            os << " PATH:[" << e.path().string() << "]";
        if (!hidden.time)
            os << " TIME:[" << e.time() << "]";
        if (!hidden.mtime)
            os << " MTIME:[" << e.mtime() << "]";
        if (!hidden.name)
            os << " NAME:[" << e.name() << "]";
        if (!hidden.priority)
            os << " PRIORITY:[" << e.priority() << "]";
        if (!hidden.deadline)
            os << " DEADLINE:[" << e.deadline() << "]";
        if (!hidden.status)
            os << " STATUS:[" << e.status() << "]";
        if (!hidden.tags) {
            os << " TAGS:[";
            bool first = true;
            for (const auto &tag : e.tags()) {
                if (!first)
                    os << ",";
                os << tag;
                first = false;
            }
            os << "]";
        }
        //

        os << "\n";
    }
};

class Path_Entry_Info_Formatter : public Entry_Info_Formatter {
  public:
    void write(const mado::entry::Entry &e, std::ostream &os,
               const Hidden_Entry_Fields &) const override {
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
    void write(const mado::entry::Entry &e, std::ostream &os,
               const Hidden_Entry_Fields &hidden) const override {
        os << "{";

        bool has_any = false;
        auto sep = [&]() {
            if (has_any)
                os << ",";
            has_any = true;
        };

        if (!hidden.path) {
            sep();
            os << "\"path\":";
            print_json_string(e.path().string(), os);
        }

        if (!hidden.time) {
            sep();
            os << "\"time\":";
            print_json_string(e.time(), os);
        }

        if (!hidden.mtime) {
            sep();
            os << "\"mtime\":";
            print_json_string(e.mtime(), os);
        }

        if (!hidden.name) {
            sep();
            os << "\"name\":";
            print_json_string(e.name(), os);
        }

        if (!hidden.priority) {
            sep();
            os << "\"priority\":" << e.priority();
        }

        if (!hidden.deadline) {
            sep();
            os << "\"deadline\":";
            print_json_string(e.deadline(), os);
        }

        if (!hidden.status) {
            sep();
            os << "\"status\":";
            print_json_string(e.status(), os);
        }

        if (!hidden.tags) {
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
        }

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
