#include "command.hpp"

#include <mado/mado.hpp>

#include <cstdio>
#include <string>
#include <vector>

namespace mado::cli {

namespace {

bool help_run(const std::string &program_name,
              const std::vector<std::string> &args);
bool version_run(const std::string &program_name,
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
    std::fprintf(stderr, "Available commands:\n");

    std::size_t max_width = 0;
    for (const auto &cmd : COMMANDS) {
        if (cmd.name.size() > max_width)
            max_width = cmd.name.size();
    }

    for (const auto &cmd : COMMANDS) {
        std::fprintf(stderr, "  %-*s - %s\n",
                     static_cast<int>(max_width), cmd.name.c_str(),
                     cmd.description.c_str());
    }
}

void print_command_usage(const Command &command, const std::string &program_name) {
    if (command.signature.empty()) {
        std::fprintf(stderr, "Usage: %s %s\n",
                     program_name.c_str(), command.name.c_str());
    } else {
        std::fprintf(stderr, "Usage: %s %s %s\n",
                     program_name.c_str(), command.name.c_str(),
                     command.signature.c_str());
    }
}

namespace {

// command implementations

bool help_run(const std::string &program_name,
              const std::vector<std::string> &args) {

    // `mado help <command>` prints usage for a specific command.
    if (!args.empty()) {
        const Command *cmd = find_command(args[0]);
        if (!cmd) {
            std::fprintf(stderr, "Unknown command: %s\n", args[0].c_str());
            print_available_commands();
            return false;
        }
        print_command_usage(*cmd, program_name);
        return true;
    }

    std::fprintf(stderr, "mado - markdown organizer\n");
    std::fprintf(stderr, "Usage: %s <command> [OPTIONS]\n", program_name.c_str());
    print_available_commands();
    return true;
}

bool version_run(const std::string &program_name,
                 const std::vector<std::string> &args) {
    (void)program_name;
    (void)args;

    std::printf("mado version %s\n", mado::version);
    return true;
}

} // namespace

} // namespace mado::cli
