#include "command.hpp"
#include "entry_info_format.hpp"

#include <mado/mado.hpp>
#include <mado/query/lexer.hpp>
#include <mado/query/parser.hpp>
#include <mado/repository/repository.hpp>

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace cli {

namespace {

bool help_run(const Command &self,
              const std::string &program_name,
              const std::vector<std::string> &args);
bool version_run(const Command &self,
                 const std::string &program_name,
                 const std::vector<std::string> &args);
bool ls_run(const Command &self,
            const std::string &program_name,
            const std::vector<std::string> &args);

const std::vector<Command> COMMANDS = {
    {
        .name = "help",
        .signature = "[COMMAND]",
        .description = "Print this help message",
        .run = help_run,
    },
    {
        .name = "version",
        .signature = "",
        .description = "Print the version of the program",
        .run = version_run,
    },
    {
        .name = "ls",
        .signature = "[QUERY]",
        .description = "List entries",
        .run = ls_run,
    },
};

} // namespace

const std::vector<Command> &commands() {
    return COMMANDS;
}

const Command *find_command(const std::string &name) {
    for (const auto &cmd : COMMANDS) {
        if (cmd.name == name)
            return &cmd;
    }
    return nullptr;
}

void print_available_commands() {
    fprintf(stderr, "Available commands:\n");

    std::size_t max_width = 0;
    for (const auto &cmd : COMMANDS) {
        if (cmd.name.size() > max_width)
            max_width = cmd.name.size();
    }

    for (const auto &cmd : COMMANDS) {
        fprintf(stderr, "  %-*s - %s\n",
                static_cast<int>(max_width), cmd.name.c_str(),
                cmd.description.c_str());
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

namespace {

// command implementations

bool help_run(const Command &self,
              const std::string &program_name,
              const std::vector<std::string> &args) {

    (void)self;

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

bool version_run(const Command &self,
                 const std::string &program_name,
                 const std::vector<std::string> &args) {
    (void)self;
    (void)program_name;
    (void)args;

    std::printf("mado version %s\n", mado::version);
    return true;
}

bool ls_run(const Command &self,
            const std::string &program_name,
            const std::vector<std::string> &args) {

    if (args.size() > 1) {
        print_command_usage(self, program_name);
        return false;
    }

    const std::string query = args.empty() ? "" : args[0];

    // Open the repository from the current working directory.
    auto repo = mado::repository::Repository::open(std::filesystem::current_path());
    if (!repo) {
        std::fprintf(stderr, "No MADO/ directory found\n");
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
            std::fprintf(stderr, "%s", e.format(query).c_str());
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

} // namespace

} // namespace cli
