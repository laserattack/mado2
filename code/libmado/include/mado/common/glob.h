#ifndef PM_GLOB_H_
#define PM_GLOB_H_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // Represents membership for all 256 byte values.
    // Хранит принадлежность всех 256 байтов набору.
    uint64_t chunks[4];
} Char_Set;

// Adds one byte to the set. Requires a valid set pointer. No error result.
// Добавляет байт в набор. Требуется корректный указатель. Ошибок нет.
void char_set_add(Char_Set *cs, uint8_t c);
// Tests one byte for membership. Requires a valid set pointer. No error
// result.
// Проверяет наличие байта в наборе. Требуется корректный указатель. Ошибок нет.
bool char_set_contains(Char_Set *cs, uint8_t c);

// Selects the operation stored in a compiled glob command.
// Задаёт операцию скомпилированной команды glob.
typedef enum {
    CMD_MANY,
    CMD_ONE_OF,
} Glob_Cmd_Kind;

typedef struct {
    // Selects a wildcard or one byte from the stored set.
    // Выбирает звёздочку или один байт из набора.
    Glob_Cmd_Kind kind;
    Char_Set cs;
} Glob_Cmd;

typedef struct {
    // Owns the compiled command buffer, including partial compilations.
    // Владеет буфером команд, включая частично скомпилированный шаблон.
    Glob_Cmd *items;
    size_t count;
    size_t capacity;
} Glob;

// You can compare Glob_Result.error pointer to any of these to check
// for a specific error. Usually you just want to print Glob_Result.error
// to the user.
// Reports a trailing escape with no following character.
// Сравнение указателя Glob_Result.error с этими константами определяет вид ошибки. Сообщение
// можно вывести пользователю.
extern const char *GLOB_ERROR_UNFINISHED_ESCAPE;
// Reports a character set without a closing bracket.
// Сообщает об отсутствии закрывающей скобки набора.
extern const char *GLOB_ERROR_UNCLOSED_BLOCK;
// Reports an escape applied to an ordinary byte.
// Сообщает о попытке экранировать обычный байт.
extern const char *GLOB_ERROR_INVALID_ESCAPE;
// Reports an unescaped metacharacter where a literal is required.
// Сообщает о неэкранированном метасимволе там, где нужен литерал.
extern const char *GLOB_ERROR_UNESCAPED_SPECIAL;

typedef struct {
    // Carries a syntax error and its byte offset. Null error means success.
    // Хранит ошибку синтаксиса и её смещение в байтах. Нулевой указатель означает успех.
    const char *error;
    size_t location;
} Glob_Result;

// Appends commands to a caller-owned glob. Returns the syntax error and byte
// offset on failure, possibly after appending commands. Allocation failure
// asserts.
// Добавляет команды в glob вызывающей стороны. При ошибке синтаксиса возвращает её вид и
// смещение, сохраняя уже добавленные команды. При сбое выделения памяти срабатывает assert.
Glob_Result glob_compile(const char *pattern, Glob *glob);
// Tests whether the commands match all input bytes. Returns false for a
// mismatch and asserts on an invalid command kind.
// Проверяет совпадение со всеми байтами входа. При несовпадении возвращает false, при неверной
// команде срабатывает assert.
bool glob_match(const char *text, size_t text_count, Glob_Cmd *cmds, size_t cmds_count);

#ifdef __cplusplus
}
#endif

#endif // PM_GLOB_H_
