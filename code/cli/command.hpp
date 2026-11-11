#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cli {

class Command {
  public:
    /// Allows derived commands to be destroyed through the base interface.
    virtual ~Command() = default;

    /// Runs the command and returns whether execution succeeded.
    /// `program_name` is argv[0] as invoked (for usage messages); `argc` and
    /// `argv` contain arguments after the command name. `argv` is mutable
    /// because flag.h may rewrite it.
    virtual bool run(const std::string &program_name,
                     int argc, char **argv) const = 0;

    std::string name;
    std::string signature;
    std::string description;
    std::vector<std::string> aliases;
};

/// Returns the process-wide command registry.
const std::vector<std::unique_ptr<Command>> &commands();

/// Finds a command by its canonical name or alias; returns null if absent.
const Command *find_command(const std::string &name);

/// Writes the available commands and global flags to standard error.
void print_available_commands();

/// Writes the usage line for `command` to standard error.
void print_command_usage(const Command &command, const std::string &program_name);

} // namespace cli
