#include "command.hpp"
#include "entry_info_format.hpp"
#include "entry_sort.hpp"

#include <mado/mado.hpp>
#include <mado/query/ast.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>
#include <mado/repository/repository.hpp>

#define FLAG_IMPLEMENTATION
#include "flag_context.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cli {

namespace {

// Shows general or command-specific usage.
// Показывает общую справку или справку по команде.
class Help_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    // Задаёт метаданные справки. Возможен std::bad_alloc.
    Help_Command() {
        name = "help";
        aliases = {"usage"};
        signature = "[COMMAND]";
        description = "Print this help message";
    }

    // Prints usage to stderr. Returns false for an unknown topic.
    // Печатает справку в stderr. Для неизвестной команды возвращает false.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {
        // `mado help <command>` prints usage for a specific command.
        // Команда mado help <command> печатает справку по указанной команде.
        if (argc > 0) {
            const Command *cmd = find_command(argv[0]);
            if (!cmd) {
                print_available_commands();
                fprintf(stderr, "\n");
                fprintf(stderr, "Unknown command: %s\n", argv[0]);
                return false;
            }
            print_command_usage(*cmd, program_name);
            return true;
        }

        fprintf(stderr, "mado - markdown organizer\n");
        fprintf(stderr, "Usage: %s <command> [OPTIONS]\n", program_name.c_str());
        print_available_commands();
        return true;
    }
};

// Prints the program version.
// Печатает версию программы.
class Version_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    // Задаёт метаданные справки. Возможен std::bad_alloc.
    Version_Command() {
        name = "version";
        signature = "";
        description = "Print the version of the program";
    }

    // Prints the version and returns true. Ignores stdout write failures.
    // Печатает версию и возвращает true. Ошибки записи в stdout игнорируются.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {
        (void)program_name;
        (void)argc;
        (void)argv;

        std::printf("mado version %s\n", mado::VERSION);
        return true;
    }
};

// Lists filtered and optionally sorted entries.
// Выводит отобранные и при необходимости отсортированные записи.
class Ls_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    // Задаёт метаданные справки. Возможен std::bad_alloc.
    Ls_Command() {
        name = "ls";
        aliases = {"list", "show", "search", "find", "view"};
        signature = "[-format <default|path|jsonl>] [-sort <[+|-]field,...>] [-hide-<field> ...] [-only-hidden] [QUERY]";
        description = "List entries";
    }

    // Prints matching entries. Returns false for invalid options, queries, or
    // a missing repository. Loading and formatting exceptions propagate.
    // Печатает подходящие записи. Возвращает false при неверных параметрах, запросе или
    // отсутствии репозитория. Исключения загрузки и форматирования передаются вызывающему
    // коду.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
        // Объявляем флаги.
        char *format_name = nullptr;
        c.add_string(&format_name, {"format", "f", "-format"}, "default", "Output format");
        char *sort_spec = nullptr;
        c.add_string(&sort_spec, {"sort", "s", "-sort"}, nullptr, "Sort entries by [+|-]field,...");
        Hidden_Entry_Fields hidden;
        const struct {
            const char *long_name;
            const char *short_name;
            const char *description;
            bool Hidden_Entry_Fields::*field;
        } hide_flags[] = {
            {"-hide-path", "H", "Hide path field", &Hidden_Entry_Fields::path},
            {"-hide-time", "T", "Hide time field", &Hidden_Entry_Fields::time},
            {"-hide-mtime", "M", "Hide mtime field", &Hidden_Entry_Fields::mtime},
            {"-hide-name", "N", "Hide name field", &Hidden_Entry_Fields::name},
            {"-hide-priority", "P", "Hide priority field", &Hidden_Entry_Fields::priority},
            {"-hide-deadline", "D", "Hide deadline field", &Hidden_Entry_Fields::deadline},
            {"-hide-status", "S", "Hide status field", &Hidden_Entry_Fields::status},
            {"-hide-tags", "G", "Hide tags field", &Hidden_Entry_Fields::tags},
        };
        // Register visibility flags using the same field mapping as inversion.
        // Регистрируем флаги видимости по той же схеме, что и инверсию.
        for (const auto &flag : hide_flags)
            c.add_bool(&(hidden.*flag.field),
                       {flag.long_name + 1, flag.short_name, flag.long_name}, flag.description);
        bool only_hidden = false;
        c.add_bool(&only_hidden, {"only-hidden", "O", "-only-hidden"},
                   "Show only fields selected by -hide flags");
        //

        // Parse flags
        // Разбираем флаги.
        if (!flag_c_parse(c, argc, argv)) {
            print_command_usage(*this, program_name);
            return false;
        }
        argc = flag_c_rest_argc(c);
        argv = flag_c_rest_argv(c);
        //

        if (argc > 1) {
            print_command_usage(*this, program_name);
            fprintf(stderr, "QUERY must be a single argument\n");
            return false;
        }

        const std::string query = (argc == 0) ? "" : argv[0];

        auto fmt_opt = parse_entry_info_format(format_name);
        if (!fmt_opt) {
            fprintf(stderr, "Unknown format: %s\n", format_name);
            return false;
        }
        auto fmt = *fmt_opt;

        std::vector<Sort_Criterion> sort_criteria;
        // Validate the sort keys before accessing the repository.
        // Проверяем ключи сортировки до обращения к репозиторию.
        if (sort_spec) {
            auto criteria = parse_sort(sort_spec);
            if (!criteria) {
                fprintf(stderr, "Invalid sort specification: %s\n", sort_spec);
                return false;
            }
            sort_criteria = std::move(*criteria);
        }

        // Open the repository from the current working directory.
        // Открываем репозиторий относительно текущего каталога.
        auto repo = mado::repository::Repository::open(std::filesystem::current_path());
        if (!repo) {
            fprintf(stderr, "No %s/ directory found\n", mado::repository::ENTRY_DIR);
            return false;
        }

        // Parse the query.
        // Разбираем запрос.
        std::unique_ptr<mado::query::Ast_Node> ast;
        if (!query.empty()) {
            mado::query::Lexer lexer(query);
            auto tokens = lexer.tokenize();
            mado::query::Parser parser(std::move(tokens));
            try {
                ast = parser.parse();
            } catch (const mado::query::Parse_Error &e) {
                fprintf(stderr, "%s", e.format(query).c_str());
                return false;
            }
        }

        // Empty query = all entries
        // Пустой запрос выбирает все записи.
        auto entries = repo->find(ast.get());

        sort_entries(entries, sort_criteria);

        // Invert visibility so only the selected fields remain.
        // Инвертируем видимость, чтобы оставить только выбранные поля.
        if (only_hidden) {
            for (const auto &flag : hide_flags)
                hidden.*flag.field = !(hidden.*flag.field);
        }

        auto formatter = make_entry_info_formatter(fmt);
        // Write the filtered entries in their final order.
        // Выводим отобранные записи в итоговом порядке.
        for (const auto &e : entries)
            formatter->write(e, std::cout, hidden);

        return true;
    }
};

// Removes entries selected by a query.
// Удаляет записи, выбранные запросом.
class Rm_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    // Задаёт метаданные справки. Возможен std::bad_alloc.
    Rm_Command() {
        name = "rm";
        aliases = {"remove", "delete", "clear", "trash"};
        signature = "[-format <default|path|jsonl>] <QUERY>";
        description = "Remove entries";
    }

    // Removes matching entries. Returns false for invalid options, queries, or
    // a missing repository. Loading and formatting exceptions propagate.
    // Удаляет подходящие записи. Возвращает false при неверных параметрах, запросе или
    // отсутствии репозитория. Исключения загрузки и форматирования передаются вызывающему
    // коду.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
        // Объявляем флаги.
        char *format_name = nullptr;
        c.add_string(&format_name, {"format", "f", "-format"}, "default", "Output format");
        //

        // Parse flags
        // Разбираем флаги.
        if (!flag_c_parse(c, argc, argv)) {
            print_command_usage(*this, program_name);
            return false;
        }
        argc = flag_c_rest_argc(c);
        argv = flag_c_rest_argv(c);
        //

        if (argc == 0) {
            print_command_usage(*this, program_name);
            fprintf(stderr, "QUERY is required\n");
            return false;
        }

        if (argc > 1) {
            print_command_usage(*this, program_name);
            fprintf(stderr, "QUERY must be a single argument\n");
            return false;
        }

        const std::string query = argv[0];

        auto fmt_opt = parse_entry_info_format(format_name);
        if (!fmt_opt) {
            fprintf(stderr, "Unknown format: %s\n", format_name);
            return false;
        }
        auto fmt = *fmt_opt;

        // Parse the query.
        // Разбираем запрос.
        std::unique_ptr<mado::query::Ast_Node> ast;
        {
            mado::query::Lexer lexer(query);
            auto tokens = lexer.tokenize();
            mado::query::Parser parser(std::move(tokens));
            try {
                ast = parser.parse();
            } catch (const mado::query::Parse_Error &e) {
                fprintf(stderr, "%s", e.format(query).c_str());
                return false;
            }
        }

        // Open the repository from the current working directory.
        // Открываем репозиторий относительно текущего каталога.
        auto repo = mado::repository::Repository::open(std::filesystem::current_path());
        if (!repo) {
            fprintf(stderr, "No %s/ directory found\n", mado::repository::ENTRY_DIR);
            return false;
        }

        auto removed = repo->remove(ast.get());

        auto formatter = make_entry_info_formatter(fmt);
        // Print only entries whose directories were removed.
        // Выводим только записи, каталоги которых удалены.
        for (const auto &e : removed)
            formatter->write(e, std::cout);

        return true;
    }
};

// Initializes a repository and restores standard templates.
// Инициализирует репозиторий и восстанавливает стандартные шаблоны.
class Init_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    // Задаёт метаданные справки. Возможен std::bad_alloc.
    Init_Command() {
        name = "init";
        aliases = {"setup", "start", "install"};
        signature = "[-force]";
        description = std::string("Initialize ") + mado::repository::ENTRY_DIR +
                      "/ and restore default templates";
    }

    // Initializes the repository and prints its path. Returns false for
    // invalid arguments or initialization exceptions.
    // Инициализирует репозиторий и печатает путь. Возвращает false при неверных аргументах или
    // исключении инициализации.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
        // Объявляем флаги.
        bool force = false;
        const auto force_description = std::string("Create ") + mado::repository::ENTRY_DIR +
                                       "/ even if one exists above";
        c.add_bool(&force, {"force", "F", "-force"}, force_description.c_str());
        //

        // Parse flags
        // Разбираем флаги.
        if (!flag_c_parse(c, argc, argv)) {
            print_command_usage(*this, program_name);
            return false;
        }
        argc = flag_c_rest_argc(c);
        argv = flag_c_rest_argv(c);
        //

        if (argc > 0) {
            print_command_usage(*this, program_name);
            fprintf(stderr, "init takes no positional arguments\n");
            return false;
        }

        try {
            auto repo = mado::repository::Repository::init(
                std::filesystem::current_path(), force);
            std::cout << repo.root().string() << '\n';
        } catch (const std::exception &e) {
            fprintf(stderr, "%s\n", e.what());
            return false;
        }

        return true;
    }
};

// Creates an entry using the selected template or built-in header.
// Создаёт запись из выбранного шаблона или встроенного заголовка.
class New_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    // Задаёт метаданные справки. Возможен std::bad_alloc.
    New_Command() {
        name = "new";
        aliases = {"add", "create", "note", "task"};
        signature = "[-suffix <str>] [-template <name>] [-format <default|path|jsonl>]";
        description = "Create a new entry";
    }

    // Creates and prints an entry. Returns false for invalid options, a
    // missing repository, or creation exceptions.
    // Создаёт и печатает запись. Возвращает false при неверных параметрах, отсутствии
    // репозитория или исключении создания.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
        // Объявляем флаги.
        char *suffix = nullptr;
        flag_c_str_var(c, &suffix, "suffix", "", "Optional suffix for the entry directory name");
        char *template_name = nullptr;
        c.add_string(&template_name, {"template", "t", "-template"}, nullptr,
                     "Create the entry from a named template");
        char *format_name = nullptr;
        c.add_string(&format_name, {"format", "f", "-format"}, "default", "Output format");
        //

        // Parse flags
        // Разбираем флаги.
        if (!flag_c_parse(c, argc, argv)) {
            print_command_usage(*this, program_name);
            return false;
        }
        argc = flag_c_rest_argc(c);
        argv = flag_c_rest_argv(c);
        //

        if (argc > 0) {
            print_command_usage(*this, program_name);
            fprintf(stderr, "new takes no positional arguments\n");
            return false;
        }

        auto fmt_opt = parse_entry_info_format(format_name);
        if (!fmt_opt) {
            fprintf(stderr, "Unknown format: %s\n", format_name);
            return false;
        }
        auto fmt = *fmt_opt;

        auto repo = mado::repository::Repository::open(std::filesystem::current_path());
        if (!repo) {
            fprintf(stderr, "No %s/ directory found\n", mado::repository::ENTRY_DIR);
            return false;
        }

        try {
            std::optional<std::string> selected_template;
            if (template_name)
                selected_template = template_name;
            auto entry = repo->create(suffix, selected_template);
            auto formatter = make_entry_info_formatter(fmt);
            formatter->write(entry, std::cout);
        } catch (const std::exception &e) {
            fprintf(stderr, "%s\n", e.what());
            return false;
        }

        return true;
    }
};

// Reports repository statistics or its directory.
// Выводит статистику репозитория или путь к нему.
class Info_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    // Задаёт метаданные справки. Возможен std::bad_alloc.
    Info_Command() {
        name = "info";
        aliases = {"repo", "about", "summary", "stat"};
        signature = "[-format <default|jsonl>] [-only-dir]";
        description = "Show repository information";
    }

    // Prints repository counts or its path. Returns false for invalid options
    // or a missing repository. Loading and formatting exceptions propagate.
    // Печатает счётчики или путь. Возвращает false при неверных параметрах или отсутствии
    // репозитория. Исключения загрузки и форматирования передаются вызывающему коду.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {
        Flag_Context c(name.c_str());
        char *format_name = nullptr;
        bool dir_only = false;
        c.add_string(&format_name, {"format", "f", "-format"}, "default", "Output format");
        c.add_bool(&dir_only, {"only-dir", "d", "-dir"}, "Print main directory path only");
        if (!flag_c_parse(c, argc, argv) || flag_c_rest_argc(c) != 0) {
            print_command_usage(*this, program_name);
            return false;
        }

        const auto format = parse_entry_info_format(format_name);
        if (!format || *format == Entry_Info_Format::Path) {
            fprintf(stderr, "Invalid format for info: %s\n", format_name);
            return false;
        }

        const auto repo = mado::repository::Repository::open(std::filesystem::current_path());
        if (!repo) {
            fprintf(stderr, "No %s/ directory found\n", mado::repository::ENTRY_DIR);
            return false;
        }

        auto &out = std::cout;
        const auto main_dir = repo->root().string();
        // Return the repository path without scanning entries.
        // Возвращаем путь к репозиторию без обхода записей.
        if (dir_only) {
            if (*format == Entry_Info_Format::Jsonl)
                print_json_string(main_dir, out);
            else
                out << main_dir;
            out << '\n';
            return true;
        }

        const auto entries = repo->find(nullptr);
        std::map<std::string, size_t> statuses;
        std::map<std::string, size_t> tags;
        std::map<uint16_t, size_t> priorities;
        // Count each status, priority, and tag occurrence.
        // Подсчитываем вхождения статусов, приоритетов и тегов.
        for (const auto &entry : entries) {
            ++statuses[entry.status()];
            ++priorities[entry.priority()];
            for (const auto &tag : entry.tags())
                ++tags[tag];
        }

        if (*format == Entry_Info_Format::Jsonl) {
            // Share JSON object formatting across the three counters.
            // Используем общее форматирование JSON для трёх счётчиков.
            auto write_counts = [&](const auto &counts, auto write_key) {
                out << '{';
                const char *separator = "";
                for (const auto &[key, count] : counts) {
                    out << separator;
                    write_key(key);
                    out << ':' << count;
                    separator = ",";
                }
                out << '}';
            };
            auto write_string_key = [&](const std::string &key) { print_json_string(key, out); };

            out << "{\"main_dir\":";
            print_json_string(main_dir, out);
            out << ",\"entries_count\":" << entries.size()
                << ",\"statuses_count\":" << statuses.size()
                << ",\"tags_count\":" << tags.size()
                << ",\"priorities_count\":" << priorities.size()
                << ",\"statuses\":";
            write_counts(statuses, write_string_key);
            out << ",\"tags\":";
            write_counts(tags, write_string_key);
            out << ",\"priorities\":";
            write_counts(priorities, [&](uint16_t key) { out << '"' << key << '"'; });
            out << "}\n";
        } else {
            auto write_counts = [&](const char *label, const auto &counts) {
                out << label << " (" << counts.size() << "):\n";
                for (const auto &[key, count] : counts)
                    out << "  " << key << ": " << count << '\n';
            };

            out << "Main directory: " << main_dir << '\n'
                << "Entries count: " << entries.size() << '\n';
            write_counts("Statuses", statuses);
            write_counts("Tags", tags);
            write_counts("Priorities", priorities);
        }
        return true;
    }
};

// Parses and prints a query AST without opening a repository.
// Разбирает запрос и печатает AST без открытия репозитория.
class Debug_Ast_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    // Задаёт метаданные справки. Возможен std::bad_alloc.
    Debug_Ast_Command() {
        name = "debug-ast";
        aliases = {"debug", "check"};
        signature = "<QUERY>";
        description = "Print the query AST";
    }

    // Prints the AST or its completed portion on a parse error. Returns false for
    // invalid arguments or syntax. Clock and allocation exceptions propagate.
    // Печатает AST или готовую часть при ошибке. Неверные аргументы и синтаксис дают false.
    // Исключения часов и выделения памяти передаются вызывающему коду.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {
        Flag_Context c(name.c_str());
        if (!flag_c_parse(c, argc, argv)) {
            print_command_usage(*this, program_name);
            return false;
        }
        argc = flag_c_rest_argc(c);
        argv = flag_c_rest_argv(c);

        // A query must be passed as one shell argument.
        // Запрос нужно передать одним аргументом командной строки.
        if (argc != 1) {
            print_command_usage(*this, program_name);
            fprintf(stderr, "%s\n", argc == 0 ? "QUERY is required" : "QUERY must be a single argument");
            return false;
        }

        const std::string query = argv[0];
        mado::query::Lexer lexer(query);
        mado::query::Parser parser(lexer.tokenize());
        try {
            const auto ast = parser.parse();
            mado::query::ast_print(ast.get());
        } catch (const mado::query::Parse_Error &e) {
            // Show completed nodes before the diagnostic, retaining failure status.
            // Выводим построенные узлы перед диагностикой, сохраняя код ошибки.
            if (parser.partial_ast()) {
                mado::query::ast_print(parser.partial_ast());
                std::fflush(stdout);
            }
            fprintf(stderr, "%s", e.format(query).c_str());
            return false;
        }
        return true;
    }
};

// Dispatches portable mtime snapshots. Argument errors return false; OS exceptions propagate.
// Управляет переносимыми снимками mtime. Неверные аргументы дают false; сбои ОС передаются выше.
class Mtimes_Command : public Command {
  public:
    // Selects dump or restore and sets usage metadata. Allocation failures propagate.
    // Выбирает сохранение или восстановление и задаёт справку. Сбои памяти передаются выше.
    explicit Mtimes_Command(bool restore) : restore_(restore) {
        name = restore ? "restore-mtimes" : "dump-mtimes";
        signature = restore ? "[-strict] [FILE]" : "[FILE]";
        const auto index_path = std::string(mado::repository::ENTRY_DIR) + "/" +
                                mado::repository::MTIMES_FILE;
        description = std::string(restore ? "Restore " : "Save ") + mado::repository::ENTRY_FILE +
                      (restore ? " times from " : " times to ") + index_path + " or FILE";
    }

    // Uses the nearest repository and an optional index path. Returns false on argument errors.
    // Repository, I/O, and allocation exceptions propagate to the CLI boundary.
    // Использует ближайший репозиторий и необязательный путь. Неверные аргументы дают false.
    // Исключения репозитория, ОС и памяти передаются на границу CLI.
    bool run(const std::string &program_name, int argc, char **argv) const override {
        Flag_Context c(name.c_str());
        bool strict = false;

        // Strict missing-file checks are meaningful only for restoration.
        // Строгая проверка отсутствующих файлов нужна только при восстановлении.
        const auto strict_description = std::string("Fail before restoring if any indexed ") +
                                        mado::repository::ENTRY_FILE + " is missing";
        if (restore_)
            c.add_bool(&strict, {"strict", "-strict"}, strict_description.c_str());

        // Parse flags consistently with the other commands.
        // Разбираем флаги так же, как остальные команды.
        if (!flag_c_parse(c, argc, argv)) {
            print_command_usage(*this, program_name);
            return false;
        }

        argc = flag_c_rest_argc(c);
        argv = flag_c_rest_argv(c);

        if (argc > 1) {
            print_command_usage(*this, program_name);
            return false;
        }

        const auto repo = mado::repository::Repository::open(std::filesystem::current_path());
        if (!repo) {
            fprintf(stderr, "No %s/ directory found\n", mado::repository::ENTRY_DIR);
            return false;
        }

        const std::filesystem::path file = argc ? argv[0] : "";

        // Keep the two actions explicit; successful commands produce no output.
        // Явно разделяем действия; успешные команды ничего не печатают.
        if (restore_)
            repo->restore_mtimes(file, strict);
        else
            repo->dump_mtimes(file);

        return true;
    }

  private:
    // Selects restoration rather than snapshot creation. No owned resources or errors.
    // Выбирает восстановление вместо сохранения. Собственных ресурсов и ошибок нет.
    bool restore_;
};

// Owns the command registry in help display order.
// Хранит команды в порядке вывода справки.
const std::vector<std::unique_ptr<Command>> COMMANDS = [] {
    std::vector<std::unique_ptr<Command>> cmds;
    cmds.push_back(std::make_unique<Help_Command>());
    cmds.push_back(std::make_unique<Version_Command>());
    cmds.push_back(std::make_unique<Ls_Command>());
    cmds.push_back(std::make_unique<Rm_Command>());
    cmds.push_back(std::make_unique<Init_Command>());
    cmds.push_back(std::make_unique<New_Command>());
    cmds.push_back(std::make_unique<Info_Command>());
    cmds.push_back(std::make_unique<Mtimes_Command>(false));
    cmds.push_back(std::make_unique<Mtimes_Command>(true));
    cmds.push_back(std::make_unique<Debug_Ast_Command>());
    return cmds;
}();

} // namespace

// Returns the registered command table. No error result or exceptions.
// Возвращает таблицу команд. Ошибок и исключений нет.
const std::vector<std::unique_ptr<Command>> &commands() {
    return COMMANDS;
}

// Resolves primary names and aliases through the same command registry.
// Разрешает основные имена и алиасы через единый реестр команд.
const Command *find_command(const std::string &name) {
    for (const auto &cmd : COMMANDS) {
        if (cmd->name == name || std::ranges::find(cmd->aliases, name) != cmd->aliases.end())
            return cmd.get();
    }
    return nullptr;
}

// Writes the command list and global flags to stderr. Ignores write failures.
// Пишет список команд и глобальных флагов в stderr. Ошибки записи игнорируются.
void print_available_commands() {
    fprintf(stderr, "Available commands:\n");

    size_t max_width = 0;
    for (const auto &cmd : COMMANDS) {
        if (cmd->name.size() > max_width)
            max_width = cmd->name.size();
    }

    for (const auto &cmd : COMMANDS) {
        fprintf(stderr, "  %-*s - %s\n",
                static_cast<int>(max_width), cmd->name.c_str(),
                cmd->description.c_str());
    }

    // handling global flags in main.cpp
    // Глобальные флаги обрабатываются в main.cpp.
    fprintf(stderr, "\n");
    fprintf(stderr, "Global flags (before the command):\n");
    fprintf(stderr, "  -C <path> - Change to <path> before running\n");
}

// Writes a command's usage to stderr. Ignores write failures.
// Пишет справку команды в stderr. Ошибки записи игнорируются.
void print_command_usage(const Command &command, const std::string &program_name) {
    if (command.signature.empty()) {
        fprintf(stderr, "Usage: %s %s\n",
                program_name.c_str(), command.name.c_str());
    } else {
        fprintf(stderr, "Usage: %s %s %s\n",
                program_name.c_str(), command.name.c_str(),
                command.signature.c_str());
    }
}

} // namespace cli
