#include "command.hpp"

#include <cstdio>
#include <string>

int main(int argc, char **argv) {
    std::string program_name = (argc > 0) ? argv[0] : "mado";

    if (argc <= 1) {
        cli::print_available_commands();
        fprintf(stderr, "No command is provided\n");
        return 1;
    }

    std::string command_name = argv[1];

    const cli::Command *cmd = cli::find_command(command_name);
    if (!cmd) {
        cli::print_available_commands();
        fprintf(stderr, "Unknown command: %s\n", command_name.c_str());
        return 1;
    }

    if (!cmd->run(program_name, argc - 2, argv + 2))
        return 1;

    return 0;
}
