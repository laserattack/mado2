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

class Help_Command : public Command {
  public:
    Help_Command() {
        name = "help";
        aliases = {"usage"};
        signature = "[COMMAND]";
        description = "Print this help message";
    }

    bool run(const std::string &program_name,
             int argc, char **argv) const override {
        // `mado help <command>` prints usage for a specific command.
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

class Version_Command : public Command {
  public:
    Version_Command() {
        name = "version";
        signature = "";
        description = "Print the version of the program";
    }

    bool run(const std::string &program_name,
             int argc, char **argv) const override {
        (void)program_name;
        (void)argc;
        (void)argv;

        std::printf("mado version %s\n", mado::VERSION);
        return true;
    }
};

class Ls_Command : public Command {
  public:
    Ls_Command() {
        name = "ls";
        aliases = {"list", "show", "search", "find", "view"};
        signature = "[-format <default|path|jsonl>] [-sort <[+|-]field,...>] [-hide-<field> ...] [-only-hidden] [QUERY]";
        description = "List entries";
    }

    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
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
        for (const auto &flag : hide_flags)
            c.add_bool(&(hidden.*flag.field),
                       {flag.long_name + 1, flag.short_name, flag.long_name}, flag.description);
        bool only_hidden = false;
        c.add_bool(&only_hidden, {"only-hidden", "O", "-only-hidden"},
                   "Show only fields selected by -hide flags");
        //

        // Parse flags
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
        if (sort_spec) {
            auto criteria = parse_sort(sort_spec);
            if (!criteria) {
                fprintf(stderr, "Invalid sort specification: %s\n", sort_spec);
                return false;
            }
            sort_criteria = std::move(*criteria);
        }

        // Open the repository from the current working directory.
        auto repo = mado::repository::Repository::open(std::filesystem::current_path());
        if (!repo) {
            fprintf(stderr, "No %s/ directory found\n", mado::repository::ENTRY_DIR);
            return false;
        }

        // Parse the query.
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
        auto entries = repo->find(ast.get());

        sort_entries(entries, sort_criteria);

        if (only_hidden) {
            for (const auto &flag : hide_flags)
                hidden.*flag.field = !(hidden.*flag.field);
        }

        auto formatter = make_entry_info_formatter(fmt);
        for (const auto &e : entries)
            formatter->write(e, std::cout, hidden);

        return true;
    }
};

class Rm_Command : public Command {
  public:
    Rm_Command() {
        name = "rm";
        aliases = {"remove", "delete", "clear", "trash"};
        signature = "[-interactive] [-format <default|path|jsonl>] <QUERY>";
        description = "Remove entries";
    }

    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
        bool interactive = false;
        c.add_bool(&interactive, {"interactive", "-interactive"},
                   "Ask before removing each entry");

        constexpr char confirmation_yes[] = "y";
        constexpr char confirmation_yes_upper[] = "Y";
        constexpr char confirmation_no_default[] = "N";

        char *format_name = nullptr;
        c.add_string(&format_name, {"format", "f", "-format"}, "default", "Output format");
        //

        // Parse flags
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
        auto repo = mado::repository::Repository::open(std::filesystem::current_path());
        if (!repo) {
            fprintf(stderr, "No %s/ directory found\n", mado::repository::ENTRY_DIR);
            return false;
        }

        auto formatter = make_entry_info_formatter(fmt);
        const auto result = repo->remove(ast.get(), [&](const mado::entry::Entry &entry) {
            if (!interactive)
                return true;

            std::cerr << "Remove entry:\n";
            formatter->write(entry, std::cerr);
            std::cerr << "[" << confirmation_yes << "/"
                      << confirmation_no_default << "] ";
            std::cerr.flush();

            std::string response;
            if (!std::getline(std::cin, response))
                return false;

            return response == confirmation_yes ||
                   response == confirmation_yes_upper;
        });

        if (interactive) {
            std::cerr << "Removed entries:\n";
        }

        for (const auto &entry : result.removed)
            formatter->write(entry, std::cout);

        for (const auto &failure : result.failed)
            std::cerr << "Cannot remove " << failure.entry.path().parent_path().string()
                      << ": " << failure.error.message() << '\n';

        return result.failed.empty();
    }
};

class Init_Command : public Command {
  public:
    Init_Command() {
        name = "init";
        aliases = {"setup", "start", "install"};
        signature = "[-force]";
        description = std::string("Initialize ") + mado::repository::ENTRY_DIR +
                      "/ and restore default templates";
    }

    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
        bool force = false;
        const auto force_description = std::string("Create ") + mado::repository::ENTRY_DIR +
                                       "/ even if one exists above";
        c.add_bool(&force, {"force", "F", "-force"}, force_description.c_str());
        //

        // Parse flags
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

class New_Command : public Command {
  public:
    New_Command() {
        name = "new";
        aliases = {"add", "create", "note", "task"};
        signature = "[-suffix <str>] [-template <name>] [-format <default|path|jsonl>]";
        description = "Create a new entry";
    }

    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
        char *suffix = nullptr;
        flag_c_str_var(c, &suffix, "suffix", "", "Optional suffix for the entry directory name");
        char *template_name = nullptr;
        c.add_string(&template_name, {"template", "t", "-template"}, nullptr,
                     "Create the entry from a named template");
        char *format_name = nullptr;
        c.add_string(&format_name, {"format", "f", "-format"}, "default", "Output format");
        //

        // Parse flags
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

class Info_Command : public Command {
  public:
    Info_Command() {
        name = "info";
        aliases = {"repo", "about", "summary", "stat"};
        signature = "[-format <default|jsonl>] [-only-dir]";
        description = "Show repository information";
    }

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
        for (const auto &entry : entries) {
            ++statuses[entry.status()];
            ++priorities[entry.priority()];
            for (const auto &tag : entry.tags())
                ++tags[tag];
        }

        if (*format == Entry_Info_Format::Jsonl) {
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

class Debug_Ast_Command : public Command {
  public:
    Debug_Ast_Command() {
        name = "debug-ast";
        aliases = {"debug", "check"};
        signature = "<QUERY>";
        description = "Print the query AST";
    }

    bool run(const std::string &program_name,
             int argc, char **argv) const override {
        Flag_Context c(name.c_str());
        if (!flag_c_parse(c, argc, argv)) {
            print_command_usage(*this, program_name);
            return false;
        }
        argc = flag_c_rest_argc(c);
        argv = flag_c_rest_argv(c);

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

class Mtimes_Command : public Command {
  public:
    explicit Mtimes_Command(bool restore) : restore_(restore) {
        name = restore ? "restore-mtimes" : "dump-mtimes";
        signature = restore ? "[-strict] [-keep-newer] [FILE]" : "[FILE]";
        const auto index_path = std::string(mado::repository::ENTRY_DIR) + "/" +
                                mado::repository::MTIMES_FILE_NAMES[0];
        description = restore ? "Restore entry modification times from " + index_path + " or FILE"
                              : "Save entry modification times to " + index_path + " or FILE";
    }

    bool run(const std::string &program_name, int argc, char **argv) const override {
        Flag_Context c(name.c_str());
        bool strict = false;
        bool keep_newer = false;

        if (restore_) {
            c.add_bool(&strict, {"strict", "-strict"},
                       "Fail before restoring if any indexed entry is missing or uses a symlink");
            c.add_bool(&keep_newer, {"keep-newer", "-keep-newer"},
                       "Preserve modification times newer than the index");
        }

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

        const auto index = restore_ ? repo->restore_mtimes(file, strict, keep_newer)
                                    : repo->dump_mtimes(file);
        std::cout << index.string() << '\n';

        return true;
    }

  private:
    bool restore_;
};

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

const std::vector<std::unique_ptr<Command>> &commands() {
    return COMMANDS;
}

const Command *find_command(const std::string &name) {
    for (const auto &cmd : COMMANDS) {
        if (cmd->name == name || std::ranges::find(cmd->aliases, name) != cmd->aliases.end())
            return cmd.get();
    }
    return nullptr;
}

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
    fprintf(stderr, "\n");
    fprintf(stderr, "Global flags (before the command):\n");
    fprintf(stderr, "  -C <path> - Change to <path> before running\n");
}

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
