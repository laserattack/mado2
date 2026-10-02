#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cli {

// Base class for all CLI subcommands. Each command is a self-contained
// unit that parses its own arguments and performs its own work.
//
// `name` is what the user types after the program name (e.g. "ls" in
// `mado ls`). `signature` is the part that appears in the Usage line,
// e.g. "[QUERY]". `description` is a one-line summary shown in the
// command list.
class Command {
  public:
    virtual ~Command() = default;

    // Runs the command. `program_name` is argv[0] as invoked (for usage
    // messages), `args` are the remaining arguments after the command
    // name. Returns true on success, false on failure.
    virtual bool run(const std::string &program_name,
                     const std::vector<std::string> &args) const = 0;

    std::string name;
    std::string signature;
    std::string description;
};

// All registered commands, in the order they should be listed.
const std::vector<std::unique_ptr<Command>> &commands();

// Looks up a command by name. Returns nullptr if not found.
const Command *find_command(const std::string &name);

// Prints the list of available commands to stderr.
void print_available_commands();

// Prints the usage line for a single command.
void print_command_usage(const Command &command, const std::string &program_name);

} // namespace cli
