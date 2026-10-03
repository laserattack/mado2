#include "command.hpp"
#include "entry_info_format.hpp"

#include <mado/mado.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>
#include <mado/repository/repository.hpp>

#define FLAG_IMPLEMENTATION
#include "flag_context.hpp"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace cli {

namespace {

class Help_Command : public Command {
  public:
    Help_Command() {
        name = "help";
        signature = "[COMMAND]";
        description = "Print this help message";
    }

    bool run(const std::string &program_name,
             int argc, char **argv) const override {
        // `mado help <command>` prints usage for a specific command.
        if (argc > 0) {
            const Command *cmd = find_command(argv[0]);
            if (!cmd) {
                fprintf(stderr, "Unknown command: %s\n", argv[0]);
                print_available_commands();
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

        std::printf("mado version %s\n", mado::version);
        return true;
    }
};

class Ls_Command : public Command {
  public:
    Ls_Command() {
        name = "ls";
        signature = "[-format <default|path|jsonl>] [QUERY]";
        description = "List entries";
    }

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

        auto formatter = make_entry_info_formatter(fmt);
        for (const auto &e : entries)
            formatter->write(e, std::cout);

        return true;
    }
};

class Rm_Command : public Command {
  public:
    Rm_Command() {
        name = "rm";
        signature = "[-format <default|path|jsonl>] <QUERY>";
        description = "Remove entries";
    }

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

const std::vector<std::unique_ptr<Command>> COMMANDS = [] {
    std::vector<std::unique_ptr<Command>> cmds;
    cmds.push_back(std::make_unique<Help_Command>());
    cmds.push_back(std::make_unique<Version_Command>());
    cmds.push_back(std::make_unique<Ls_Command>());
    cmds.push_back(std::make_unique<Rm_Command>());
    return cmds;
}();

} // namespace

const std::vector<std::unique_ptr<Command>> &commands() {
    return COMMANDS;
}

const Command *find_command(const std::string &name) {
    for (const auto &cmd : COMMANDS) {
        if (cmd->name == name)
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
