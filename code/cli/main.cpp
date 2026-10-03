#include "command.hpp"
#include "flag_context.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

int main(int argc, char **argv) {
    std::string program_name = (argc > 0) ? argv[0] : "mado";

    // Parse global flags (before the command name).
    cli::Flag_Context c(program_name.c_str());

    char *directory = nullptr;
    flag_c_str_var(c, &directory, "C", "", "Change to this directory before running");

    if (!flag_c_parse(c, argc - 1, argv + 1)) {
        flag_c_print_error(c, stderr);
        return 1;
    }

    argc = flag_c_rest_argc(c);
    argv = flag_c_rest_argv(c);

    // Change directory if -C was given.
    if (directory[0] != '\0') {
        std::error_code ec;
        std::filesystem::current_path(directory, ec);
        if (ec) {
            fprintf(stderr, "Cannot change to %s: %s\n", directory, ec.message().c_str());
            return 1;
        }
    }

    if (argc <= 0) {
        cli::print_available_commands();
        fprintf(stderr, "\n");
        fprintf(stderr, "No command is provided\n");
        return 1;
    }

    std::string command_name = argv[0];

    const cli::Command *cmd = cli::find_command(command_name);
    if (!cmd) {
        cli::print_available_commands();
        fprintf(stderr, "\n");
        fprintf(stderr, "Unknown command: %s\n", command_name.c_str());
        return 1;
    }

    if (!cmd->run(program_name, argc - 1, argv + 1))
        return 1;

    return 0;
}
