#include "command.hpp"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    std::string program_name = (argc > 0) ? argv[0] : "mado";

    if (argc <= 1) {
        mado::cli::print_available_commands();
        std::fprintf(stderr, "No command is provided\n");
        return 1;
    }

    std::string command_name = argv[1];

    const mado::cli::Command *cmd = mado::cli::find_command(command_name);
    if (!cmd) {
        mado::cli::print_available_commands();
        std::fprintf(stderr, "Unknown command: %s\n", command_name.c_str());
        return 1;
    }

    // Collect the remaining arguments.
    std::vector<std::string> args;
    args.reserve(argc - 2);
    for (int i = 2; i < argc; ++i)
        args.emplace_back(argv[i]);

    if (!cmd->run(program_name, args))
        return 1;

    return 0;
}
