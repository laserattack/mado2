#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cli {

// Defines a CLI command, its usage metadata, and its execution contract.
// Описывает команду CLI, её справку и контракт выполнения.
class Command {
  public:
    // Defines a CLI command, its usage metadata, and its execution contract.
// Описывает команду CLI, её справку и контракт выполнения.
    // Destroys a command through its base pointer. No exceptions.
    virtual ~Command() = default;

    // Runs the command. `program_name` is argv[0] as invoked (for
    // usage messages). `argc` and `argv` are the remaining arguments
    // after the command name. flag.h may modify argv. Returns false on
    // handled errors. Unhandled exceptions propagate to the CLI boundary.
    // Выполняет команду. program_name — исходный argv[0], argc и argv — аргументы после имени
    // команды. flag.h может менять argv. При обработанной ошибке возвращает false, остальные
    // исключения передаёт на уровень CLI.
    virtual bool run(const std::string &program_name,
                     int argc, char **argv) const = 0;

    std::string name;
    std::string signature;
    std::string description;
    // Alternative invocation names. Help displays the primary name.
    // Альтернативные имена вызова. Справка выводит основное имя.
    std::vector<std::string> aliases;
};

// Returns the registered command table. No error result or exceptions.
// Возвращает таблицу команд. Ошибок и исключений нет.
const std::vector<std::unique_ptr<Command>> &commands();

// Finds an exact primary name or alias, or returns nullptr. No exceptions.
// Ищет точное основное имя или алиас. Если совпадения нет, возвращает nullptr.
// Исключений нет.
const Command *find_command(const std::string &name);

// Writes the command list and global flags to stderr. Ignores write failures.
// Пишет список команд и глобальных флагов в stderr. Ошибки записи игнорируются.
void print_available_commands();

// Writes a command's usage to stderr. Ignores write failures.
// Пишет справку команды в stderr. Ошибки записи игнорируются.
void print_command_usage(const Command &command, const std::string &program_name);

} // namespace cli
