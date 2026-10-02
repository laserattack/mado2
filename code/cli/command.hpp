#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cli {

class Command {
  public:
    virtual ~Command() = default;

    // Runs the command. `program_name` is argv[0] as invoked (for
    // usage messages). `argc` and `argv` are the remaining arguments
    // after the command name. argv is mutable: flag.h may write into
    // it (e.g. for --flag=value).
    virtual bool run(const std::string &program_name,
                     int argc, char **argv) const = 0;

    std::string name;
    std::string signature;
    std::string description;
};

const std::vector<std::unique_ptr<Command>> &commands();

const Command *find_command(const std::string &name);

void print_available_commands();

void print_command_usage(const Command &command, const std::string &program_name);

} // namespace cli
