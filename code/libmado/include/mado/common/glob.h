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
    uint64_t chunks[4];
} Char_Set;

// Adds one byte to the set. Requires a valid set pointer. No error result.
void char_set_add(Char_Set *cs, uint8_t c);
// Tests one byte for membership. Requires a valid set pointer. No error
// result.
bool char_set_contains(Char_Set *cs, uint8_t c);

// Selects the operation stored in a compiled glob command.
typedef enum {
    CMD_MANY,
    CMD_ONE_OF,
} Glob_Cmd_Kind;

typedef struct {
    // Selects a wildcard or one byte from the stored set.
    Glob_Cmd_Kind kind;
    Char_Set cs;
} Glob_Cmd;

typedef struct {
    // Owns the compiled command buffer, including partial compilations.
    Glob_Cmd *items;
    size_t count;
    size_t capacity;
} Glob;

// You can compare Glob_Result.error pointer to any of these to check
// for a specific error. Usually you just want to print Glob_Result.error
// to the user.
// Reports a trailing escape with no following character.
extern const char *GLOB_ERROR_UNFINISHED_ESCAPE;
// Reports a character set without a closing bracket.
extern const char *GLOB_ERROR_UNCLOSED_BLOCK;
// Reports an escape applied to an ordinary byte.
extern const char *GLOB_ERROR_INVALID_ESCAPE;
// Reports an unescaped metacharacter where a literal is required.
extern const char *GLOB_ERROR_UNESCAPED_SPECIAL;

typedef struct {
    // Carries a syntax error and its byte offset. Null error means success.
    const char *error;
    size_t location;
} Glob_Result;

// Appends commands to a caller-owned glob. Returns the syntax error and byte
// offset on failure, possibly after appending commands. Allocation failure
// asserts.
Glob_Result glob_compile(const char *pattern, Glob *glob);
// Tests whether the commands match all input bytes. Returns false for a
// mismatch and asserts on an invalid command kind.
bool glob_match(const char *text, size_t text_count, Glob_Cmd *cmds, size_t cmds_count);

#ifdef __cplusplus
}
#endif

#endif // PM_GLOB_H_
