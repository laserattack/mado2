#include "entry_info_format.hpp"

#include <mado/common/fuzzy_match.hpp>
#include <mado/common/text_utils.hpp>

#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace cli {

// Writes a JSON string literal to os.
// Пишет строковый литерал JSON в os.
void print_json_string(const std::string &str, std::ostream &os) {
    os << '"';
    // Escape characters that would end or alter the JSON string.
    // Экранируем символы, которые могли бы оборвать или изменить JSON-строку.
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

namespace {

// Prints an Emacs-compatible location followed by visible fields.
// Печатает адрес для Emacs и видимые поля.
class Default_Entry_Info_Formatter : public Entry_Info_Formatter {
  public:
    // Writes the location and visible fields. Allocation and
    // stream exceptions propagate.
    // Пишет адрес и видимые поля. Исключения выделения памяти и потока передаются вызывающему
    // коду.
    void write(const mado::entry::Entry &e, std::ostream &os,
               const Hidden_Entry_Fields &hidden) const override {
        os << e.path().string() << ":1:";

        // fields
        // Поля записи.
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

// Prints the entry path regardless of field visibility.
// Печатает путь независимо от видимости полей.
class Path_Entry_Info_Formatter : public Entry_Info_Formatter {
  public:
    // Writes the path and ignores visibility. Allocation and
    // stream exceptions propagate.
    // Пишет путь независимо от видимости. Исключения выделения памяти и потока передаются
    // вызывающему коду.
    void write(const mado::entry::Entry &e, std::ostream &os,
               const Hidden_Entry_Fields &) const override {
        os << e.path().string() << "\n";
    }
};

// Prints visible fields as one JSON object per entry.
// Печатает видимые поля одним объектом JSON на запись.
class Jsonl_Entry_Info_Formatter : public Entry_Info_Formatter {
  public:
    // Writes a JSON object containing the visible fields. Allocation and
    // stream exceptions propagate.
    // Пишет объект JSON с видимыми полями. Исключения выделения памяти и потока передаются
    // вызывающему коду.
    void write(const mado::entry::Entry &e, std::ostream &os,
               const Hidden_Entry_Fields &hidden) const override {
        os << "{";

        bool has_any = false;
        auto sep = [&]() {
            if (has_any)
                os << ",";
            has_any = true;
        };

        // Emit a JSON key only when the field is visible.
        // Выводим JSON-ключ только для видимого поля.
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

// Associates a format name and identifier with its factory.
// Связывает имя и идентификатор формата с фабрикой.
struct Format_Info {
    Entry_Info_Format format;
    const char *name;
    std::unique_ptr<Entry_Info_Formatter> (*make)();
};

// Allocates the Emacs-compatible formatter. May throw std::bad_alloc.
// Создаёт форматтер для Emacs. Возможен std::bad_alloc.
std::unique_ptr<Entry_Info_Formatter> make_default() {
    return std::make_unique<Default_Entry_Info_Formatter>();
}

// Allocates the path-only formatter. May throw std::bad_alloc.
// Создаёт форматтер путей. Возможен std::bad_alloc.
std::unique_ptr<Entry_Info_Formatter> make_path() {
    return std::make_unique<Path_Entry_Info_Formatter>();
}

// Allocates the JSON Lines formatter. May throw std::bad_alloc.
// Создаёт форматтер JSON Lines. Возможен std::bad_alloc.
std::unique_ptr<Entry_Info_Formatter> make_jsonl() {
    return std::make_unique<Jsonl_Entry_Info_Formatter>();
}

// Supported format names in fuzzy tie-break order.
// Поддерживаемые форматы в порядке разрешения равных нечётких оценок.
const std::vector<Format_Info> FORMATS = {
    {Entry_Info_Format::Default, "default", make_default},
    {Entry_Info_Format::Path, "path", make_path},
    {Entry_Info_Format::Jsonl, "jsonl", make_jsonl},
};

} // namespace

// Prefers an exact name, then a fuzzy match. Returns nullopt for empty or
// unknown names. May throw std::bad_alloc.
// Сначала ищет точное имя, затем нечёткое совпадение. Если совпадения нет, возвращает nullopt.
// Возможен std::bad_alloc.
std::optional<Entry_Info_Format> parse_entry_info_format(const std::string &name) {
    if (name.empty())
        return std::nullopt;

    std::optional<Entry_Info_Format> best_format;
    std::optional<int32_t> best_score;
    // Prefer an exact format name over every fuzzy score.
    // Точное имя формата имеет приоритет над нечёткими совпадениями.
    for (const auto &f : FORMATS) {
        if (mado::common::equals_ignore_case(name, f.name))
            return f.format;

        auto score = mado::common::fuzzy_match(name, f.name, true);
        if (score && (!best_score || *score > *best_score)) {
            best_score = score;
            best_format = f.format;
        }
    }
    return best_format;
}

// Creates the requested formatter or returns nullptr for an unknown value. May
// throw std::bad_alloc.
// Создаёт форматтер или возвращает nullptr для неизвестного значения. Возможен std::bad_alloc.
std::unique_ptr<Entry_Info_Formatter> make_entry_info_formatter(Entry_Info_Format fmt) {
    for (const auto &f : FORMATS) {
        if (fmt == f.format)
            return f.make();
    }
    return nullptr;
}

} // namespace cli
