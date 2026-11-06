#include <stdio.h>

#include <mado/common/glob.h>

// NOTE: MSVC C++ compiler does not support compound literals (C99 feature)
// Plain structures in C++ (without constructors) can be initialized with { }
// MSVC в режиме C++ не поддерживает составные литералы C99. Для обычных структур C++
// используется инициализация через фигурные скобки.
#if defined(__cplusplus)
#define CLITERAL(type) type
#else
#define CLITERAL(type) (type)
#endif

// Initial allocation in commands for a compiled pattern.
// Начальный объём буфера команд шаблона.
#define DA_INIT_CAP 256
// Appends one command and grows storage as needed. Asserts on allocation failure.
// Добавляет команду, при необходимости расширяя буфер. При сбое выделения памяти срабатывает
// assert.
#define da_append(da, item)                                                            \
    do {                                                                               \
        if ((da)->count >= (da)->capacity) {                                           \
            (da)->capacity = (da)->capacity == 0 ? DA_INIT_CAP : (da)->capacity * 2;   \
            (da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items)); \
            assert((da)->items != NULL && "Buy more RAM lol");                         \
        }                                                                              \
                                                                                       \
        (da)->items[(da)->count++] = (item);                                           \
    } while (0)

// Identifies an escape without a following byte.
// Обозначает экранирование без следующего байта.
const char *GLOB_ERROR_UNFINISHED_ESCAPE = "Unfinished escape sequence.";
// Identifies a character set without a closing bracket.
// Обозначает набор символов без закрывающей скобки.
const char *GLOB_ERROR_UNCLOSED_BLOCK = "Missing closing ] for a character set block.";
// Identifies an escape of an ordinary byte.
// Обозначает попытку экранировать обычный байт.
const char *GLOB_ERROR_INVALID_ESCAPE = "Invalid escape sequence. Only special characters are allowed to be escaped.";
// Identifies a metacharacter used as an unescaped literal.
// Обозначает метасимвол, использованный без экранирования.
const char *GLOB_ERROR_UNESCAPED_SPECIAL = "Unescaped special character! To avoid confusion, please, escape it!";

// Adds one byte to the set. Requires a valid set pointer. No error result.
// Добавляет байт в набор. Требуется корректный указатель. Ошибок нет.
void char_set_add(Char_Set *cs, uint8_t c) {
    cs->chunks[c / 64] |= (1ULL << (c % 64));
}

// Tests one byte for membership. Requires a valid set pointer. No error
// result.
// Проверяет наличие байта в наборе. Требуется корректный указатель. Ошибок нет.
bool char_set_contains(Char_Set *cs, uint8_t c) {
    return (cs->chunks[c / 64] >> (c % 64)) & 1;
}

// Tests whether a byte is glob syntax. No error result.
// Проверяет, является ли байт элементом синтаксиса glob. Ошибок нет.
static bool is_special(char x) {
    if (x == '*')
        return true;
    if (x == '?')
        return true;
    if (x == '\\')
        return true;
    if (x == '[')
        return true;
    if (x == ']')
        return true;
    return false;
}

// Appends commands to a caller-owned glob. Returns the syntax error and byte
// offset on failure, possibly after appending commands. Allocation failure
// asserts.
// Добавляет команды в glob вызывающей стороны. При ошибке синтаксиса возвращает её вид и
// смещение, сохраняя уже добавленные команды. При сбое выделения памяти срабатывает assert.
Glob_Result glob_compile(const char *pattern, Glob *glob) {
    size_t location = 0;
    // Compile the pattern one command at a time.
    // Компилируем шаблон по одной команде.
    while (pattern[location] != '\0') {
        switch (pattern[location]) {
        case '*': {
            da_append(glob, CLITERAL(Glob_Cmd){
                                .kind = CMD_MANY});
            location += 1;
        } break;

        case '?': {
            da_append(glob, (CLITERAL(Glob_Cmd){
                                .kind = CMD_ONE_OF,
                                .cs = {
                                    // All of them!
                                    // Подходит любой байт.
                                    .chunks = {
                                        0xFFFFFFFFFFFFFFFF,
                                        0xFFFFFFFFFFFFFFFF,
                                        0xFFFFFFFFFFFFFFFF,
                                        0xFFFFFFFFFFFFFFFF,
                                    },
                                },
                            }));
            location += 1;
        } break;

        case '\\': {
            location += 1;

            if (pattern[location] == '\0') {
                return CLITERAL(Glob_Result){
                    .error = GLOB_ERROR_UNFINISHED_ESCAPE,
                    .location = location,
                };
            }

            if (!is_special(pattern[location])) {
                return CLITERAL(Glob_Result){
                    .error = GLOB_ERROR_INVALID_ESCAPE,
                    .location = location,
                };
            }

            Glob_Cmd cmd = {0};
            cmd.kind = CMD_ONE_OF;
            char_set_add(&cmd.cs, pattern[location]);
            da_append(glob, cmd);
            location += 1;
        } break;

        case '[': {
            location += 1;

            Glob_Cmd cmd = {0};
            cmd.kind = CMD_ONE_OF;

            while (pattern[location] != '\0') {
                if (pattern[location] == ']') {
                    break;
                } else if (pattern[location] == '\\') {
                    location += 1;

                    if (pattern[location] == '\0') {
                        return CLITERAL(Glob_Result){
                            .error = GLOB_ERROR_UNFINISHED_ESCAPE,
                            .location = location,
                        };
                    }

                    if (!is_special(pattern[location])) {
                        return CLITERAL(Glob_Result){
                            .error = GLOB_ERROR_INVALID_ESCAPE,
                            .location = location,
                        };
                    }

                    char_set_add(&cmd.cs, pattern[location]);
                    location += 1;
                } else {
                    if (is_special(pattern[location])) {
                        return CLITERAL(Glob_Result){
                            .error = GLOB_ERROR_UNESCAPED_SPECIAL,
                            .location = location,
                        };
                    } else {
                        char_set_add(&cmd.cs, pattern[location]);
                        location += 1;
                    }
                }
            }

            if (pattern[location] != ']') {
                return CLITERAL(Glob_Result){
                    .error = GLOB_ERROR_UNCLOSED_BLOCK,
                    .location = location,
                };
            }
            location += 1;

            da_append(glob, cmd);

        } break;

        default:
            if (is_special(pattern[location])) {
                return CLITERAL(Glob_Result){
                    .error = GLOB_ERROR_UNESCAPED_SPECIAL,
                    .location = location,
                };
            } else {
                Glob_Cmd cmd = {0};
                cmd.kind = CMD_ONE_OF;
                char_set_add(&cmd.cs, pattern[location]);
                da_append(glob, cmd);

                location += 1;
            }
        }
    }

    return CLITERAL(Glob_Result){0};
}

// Tests whether the commands match all input bytes. Returns false for a
// mismatch and asserts on an invalid command kind.
// Проверяет совпадение со всеми байтами входа. При несовпадении возвращает false, при неверной
// команде срабатывает assert.
bool glob_match(const char *text, size_t text_count, Glob_Cmd *cmds, size_t cmds_count) {
    // Consume literal matches and backtrack over wildcard lengths.
    // Сопоставляем литералы и перебираем длины совпадения для звёздочки.
    while (text_count > 0 && cmds_count > 0) {
        switch (cmds->kind) {
        case CMD_MANY: {
            if (glob_match(text, text_count, cmds + 1, cmds_count - 1))
                return true;
            text += 1;
            text_count -= 1;
        } break;

        case CMD_ONE_OF: {
            if (char_set_contains(&cmds->cs, *text)) {
                text += 1;
                text_count -= 1;
                cmds += 1;
                cmds_count -= 1;
            } else {
                return false;
            }
        } break;

        default:
            assert(0 && "unreachable");
        }
    }

    // Trailing stars may match an empty suffix.
    // Звёздочки в конце могут совпасть с пустым суффиксом.
    while (cmds_count > 0 && cmds->kind == CMD_MANY) {
        cmds += 1;
        cmds_count -= 1;
    }
    return cmds_count == 0 && text_count == 0;
}

// TODO: implement tests for the glob
// TODO: добавить тесты для glob.
