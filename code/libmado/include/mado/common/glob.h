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
    uint64_t chunks[4];
} Char_Set;

// Adds byte c to the set. cs must point to an initialized Char_Set.
void char_set_add(Char_Set *cs, uint8_t c);
// Returns whether byte c is present in the set. cs must point to an initialized Char_Set.
bool char_set_contains(Char_Set *cs, uint8_t c);

typedef enum {
    CMD_MANY,
    CMD_ONE_OF,
} Glob_Cmd_Kind;

typedef struct {
    Glob_Cmd_Kind kind;
    Char_Set cs;
} Glob_Cmd;

typedef struct {
    Glob_Cmd *items;
    size_t count;
    size_t capacity;
} Glob;

// glob_compile returns these pointers in Glob_Result.error for the matching
// syntax error. Callers may compare pointer identity or display the message.
extern const char *GLOB_ERROR_UNFINISHED_ESCAPE;
extern const char *GLOB_ERROR_UNCLOSED_BLOCK;
extern const char *GLOB_ERROR_INVALID_ESCAPE;
extern const char *GLOB_ERROR_UNESCAPED_SPECIAL;

typedef struct {
    const char *error;
    size_t location;
} Glob_Result;

// Compiles a byte-oriented glob pattern and appends commands to glob. `*`
// matches any byte sequence, `?` matches one byte, and `[chars]` matches one
// byte from the set; backslash escapes special characters. pattern must be
// non-null.
// The caller must initialize glob to zero or a valid existing Glob and free
// glob->items with free(). A syntax error is returned in Glob_Result.error
// with its byte location; commands appended before the error remain in glob.
// Successful compilation returns a null error. Allocation failure is not
// reported as a result and may abort the process.
Glob_Result glob_compile(const char *pattern, Glob *glob);
// Matches text[0..text_count) against cmds[0..cmds_count) byte-for-byte.
// Returns true only when the entire text is consumed by the commands. Inputs
// must be valid pointers when their corresponding counts are nonzero.
bool glob_match(const char *text, size_t text_count, Glob_Cmd *cmds, size_t cmds_count);

#ifdef __cplusplus
}
#endif

#endif // PM_GLOB_H_
