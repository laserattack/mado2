#include "command.hpp"
#include "entry_info_format.hpp"
#include "entry_sort.hpp"

#include <mado/mado.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>
#include <mado/repository/repository.hpp>

#define FLAG_IMPLEMENTATION
#include "flag_context.hpp"

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
class Help_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    Help_Command() {
        name = "help";
        signature = "[COMMAND]";
        description = "Print this help message";
    }

    // Prints usage to stderr. Returns false for an unknown topic.
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

// Prints the program version.
class Version_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    Version_Command() {
        name = "version";
        signature = "";
        description = "Print the version of the program";
    }

    // Prints the version and returns true. Ignores stdout write failures.
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
class Ls_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    Ls_Command() {
        name = "ls";
        signature = "[-format <default|path|jsonl>] [-sort <[+|-]field,...>] [-hide-<field> ...] [-only-hidden] [QUERY]";
        description = "List entries";
    }

    // Prints matching entries. Returns false for invalid options, queries, or
    // a missing repository. Loading and formatting exceptions propagate.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
        char *format_name = nullptr;
        flag_c_str_var(c, &format_name, "format", "default", "Output format");
        char *sort_spec = nullptr;
        flag_c_str_var(c, &sort_spec, "sort", nullptr, "Sort entries by [+|-]field,...");
        Hidden_Entry_Fields hidden;
        const struct {
            const char *name;
            const char *description;
            bool Hidden_Entry_Fields::*field;
        } hide_flags[] = {
            {"hide-path", "Hide path field", &Hidden_Entry_Fields::path},
            {"hide-time", "Hide time field", &Hidden_Entry_Fields::time},
            {"hide-mtime", "Hide mtime field", &Hidden_Entry_Fields::mtime},
            {"hide-name", "Hide name field", &Hidden_Entry_Fields::name},
            {"hide-priority", "Hide priority field", &Hidden_Entry_Fields::priority},
            {"hide-deadline", "Hide deadline field", &Hidden_Entry_Fields::deadline},
            {"hide-status", "Hide status field", &Hidden_Entry_Fields::status},
            {"hide-tags", "Hide tags field", &Hidden_Entry_Fields::tags},
        };
        for (const auto &flag : hide_flags)
            flag_c_bool_var(c, &(hidden.*flag.field), flag.name, false, flag.description);
        bool only_hidden = false;
        flag_c_bool_var(c, &only_hidden, "only-hidden", false, "Show only fields selected by -hide flags");
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
            fprintf(stderr, "No MADO/ directory found\n");
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

// Removes entries selected by a query.
class Rm_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    Rm_Command() {
        name = "rm";
        signature = "[-format <default|path|jsonl>] <QUERY>";
        description = "Remove entries";
    }

    // Removes matching entries. Returns false for invalid options, queries, or
    // a missing repository. Loading and formatting exceptions propagate.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
        char *format_name = nullptr;
        flag_c_str_var(c, &format_name, "format", "default", "Output format");
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
            fprintf(stderr, "No MADO/ directory found\n");
            return false;
        }

        auto removed = repo->remove(ast.get());

        auto formatter = make_entry_info_formatter(fmt);
        for (const auto &e : removed)
            formatter->write(e, std::cout);

        return true;
    }
};

// Initializes a repository and restores standard templates.
class Init_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    Init_Command() {
        name = "init";
        signature = "[-force]";
        description = "Initialize MADO/ and restore default templates";
    }

    // Initializes the repository and prints its path. Returns false for
    // invalid arguments or initialization exceptions.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
        bool force = false;
        flag_c_bool_var(c, &force, "force", false, "Create MADO/ even if one exists above");
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

// Creates an entry using the selected template or built-in header.
class New_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    New_Command() {
        name = "new";
        signature = "[-suffix <str>] [-template <name>] [-format <default|path|jsonl>]";
        description = "Create a new entry";
    }

    // Creates and prints an entry. Returns false for invalid options, a
    // missing repository, or creation exceptions.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {

        Flag_Context c(name.c_str());

        // Declare flags
        char *suffix = nullptr;
        flag_c_str_var(c, &suffix, "suffix", "", "Optional suffix for the entry directory name");
        char *template_name = nullptr;
        flag_c_str_var(c, &template_name, "template", nullptr, "Create the entry from a named template");
        char *format_name = nullptr;
        flag_c_str_var(c, &format_name, "format", "default", "Output format");
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
            fprintf(stderr, "No MADO/ directory found\n");
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
class Info_Command : public Command {
  public:
    // Sets usage metadata. May throw std::bad_alloc.
    Info_Command() {
        name = "info";
        signature = "[-format <default|jsonl>] [-only-dir]";
        description = "Show repository information";
    }

    // Prints repository counts or its path. Returns false for invalid options
    // or a missing repository. Loading and formatting exceptions propagate.
    bool run(const std::string &program_name,
             int argc, char **argv) const override {
        Flag_Context c(name.c_str());
        char *format_name = nullptr;
        bool dir_only = false;
        flag_c_str_var(c, &format_name, "format", "default", "Output format");
        flag_c_bool_var(c, &dir_only, "only-dir", false, "Print main directory path only");
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
            fprintf(stderr, "No MADO/ directory found\n");
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

// Owns the command registry in help display order.
const std::vector<std::unique_ptr<Command>> COMMANDS = [] {
    std::vector<std::unique_ptr<Command>> cmds;
    cmds.push_back(std::make_unique<Help_Command>());
    cmds.push_back(std::make_unique<Version_Command>());
    cmds.push_back(std::make_unique<Ls_Command>());
    cmds.push_back(std::make_unique<Rm_Command>());
    cmds.push_back(std::make_unique<Init_Command>());
    cmds.push_back(std::make_unique<New_Command>());
    cmds.push_back(std::make_unique<Info_Command>());
    return cmds;
}();

} // namespace

// Returns the registered command table. No error result or exceptions.
const std::vector<std::unique_ptr<Command>> &commands() {
    return COMMANDS;
}

// Finds an exact command name or returns nullptr. No exceptions.
const Command *find_command(const std::string &name) {
    for (const auto &cmd : COMMANDS) {
        if (cmd->name == name)
            return cmd.get();
    }
    return nullptr;
}

// Writes the command list and global flags to stderr. Ignores write failures.
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

// Writes a command's usage to stderr. Ignores write failures.
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
