#include "command.hpp"
#include "entry_info_format.hpp"

#include <mado/mado.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>
#include <mado/repository/repository.hpp>

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
             const std::vector<std::string> &args) const override {
        // `mado help <command>` prints usage for a specific command.
        if (!args.empty()) {
            const Command *cmd = find_command(args[0]);
            if (!cmd) {
                fprintf(stderr, "Unknown command: %s\n", args[0].c_str());
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
             const std::vector<std::string> &args) const override {
        (void)program_name;
        (void)args;

        std::printf("mado version %s\n", mado::version);
        return true;
    }
};

class Ls_Command : public Command {
  public:
    Ls_Command() {
        name = "ls";
        signature = "[QUERY]";
        description = "List entries";
    }

    bool run(const std::string &program_name,
             const std::vector<std::string> &args) const override {
        if (args.size() > 1) {
            print_command_usage(*this, program_name);
            return false;
        }

        const std::string query = args.empty() ? "" : args[0];

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

        auto formatter = make_entry_info_formatter(Entry_Info_Format::Default);
        for (const auto &e : entries)
            formatter->write(e, std::cout);

        return true;
    }
};

const std::vector<std::unique_ptr<Command>> COMMANDS = [] {
    std::vector<std::unique_ptr<Command>> cmds;
    cmds.push_back(std::make_unique<Help_Command>());
    cmds.push_back(std::make_unique<Version_Command>());
    cmds.push_back(std::make_unique<Ls_Command>());
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
