#ifndef NOC_W6_SKILLLOCK_842861_H
#define NOC_W6_SKILLLOCK_842861_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_skilllock_842861_skill_refusal @ tools/skill_lock.py:skill_refusal */
char *w6_skilllock_842861_skill_refusal(const char *name);

/* PoP: w6_skilllock_842861_unquoted_newlines_to_semicolons
 *     @ tools/skill_lock.py:_unquoted_newlines_to_semicolons */
char *w6_skilllock_842861_unquoted_newlines_to_semicolons(const char *command);

/* PoP: w6_skilllock_842861_segments @ tools/skill_lock.py:_segments
 * Returns an array of [argv(array of strings), targets(array of strings), separator(string)]. */
struct json_t *w6_skilllock_842861_segments(const struct json_t *tokens);

/* PoP: w6_skilllock_842861_operands @ tools/skill_lock.py:_operands */
struct json_t *w6_skilllock_842861_operands(const struct json_t *args);

/* PoP: w6_skilllock_842861_target_directory @ tools/skill_lock.py:_target_directory
 * Returns [dir(string|null), rest(array of strings)]. */
struct json_t *w6_skilllock_842861_target_directory(const struct json_t *args);

/* PoP: w6_skilllock_842861_find_exec_commands @ tools/skill_lock.py:_find_exec_commands
 * Returns an array of command groups (arrays of strings). */
struct json_t *w6_skilllock_842861_find_exec_commands(const struct json_t *args);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_SKILLLOCK_842861_H */
