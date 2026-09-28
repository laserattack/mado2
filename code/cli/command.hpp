#pragma once

#include <string>
#include <vector>

namespace mado::cli {

// A single CLI subcommand. Each command is a self-contained unit that
// parses its own arguments and performs its own work.
//
// The command name is what the user types after the program name
// (e.g. "ls" in `mado ls`). `signature` is the part that appears in
// the Usage line, e.g. "[OPTIONS] [QUERY...]". `description` is a
// one-line summary shown in the command list.
//
// `run` receives:
//   - program_name: argv[0] as invoked (for usage messages)
//   - args: the remaining arguments after the command name
// It returns true on success, false on failure.
struct Command {
    std::string name;
    std::string signature;
    std::string description;
    bool (*run)(const std::string &program_name,
                const std::vector<std::string> &args);
};

// All registered commands, in the order they should be listed.
const std::vector<Command> &commands();

// Looks up a command by name. Returns nullptr if not found.
const Command *find_command(const std::string &name);

// Prints the list of available commands to stderr.
void print_available_commands();

// Prints the usage line for a single command.
void print_command_usage(const Command &command, const std::string &program_name);

} // namespace mado::cli
