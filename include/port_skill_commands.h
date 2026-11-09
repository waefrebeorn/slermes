/*
 * port_skill_commands.h -- pure helpers ported from agent/skill_commands.py.
 *
 * The scan / cache / plugin projection half stays in Python.
 */
#ifndef PORT_SKILL_COMMANDS_H
#define PORT_SKILL_COMMANDS_H

#include <stddef.h>

/* skill_commands.py:slugify_skill_name -- "Foo Bar" -> "foo-bar"; strips
 * chars that would make invalid Telegram command names, collapses hyphen
 * runs, strips leading/trailing hyphens. Caller frees. */
char *sc_slugify_skill_name(const char *name);

/* skill_commands.py:command_snapshot -- flat [key, description, ...] in, flat
 * [name, description, ...] out with every leading "/" stripped. Caller frees
 * the array and its strings. */
char **sc_command_snapshot(const char *const *cmds, int *out_count);

/* skill_commands.py:diff_command_snapshots -- added/removed/unchanged/total
 * as JSON. `before` / `after` are sc_command_snapshot-shaped flat arrays.
 * Caller frees the returned string. */
char *sc_diff_command_snapshots(const char *const *before, const char *const *after);

/* skill_commands.py:resolve_slash_key -- "/slug" when present in `table`
 * (exact first, then "_" normalized to "-"), else NULL. Caller frees. */
char *sc_resolve_slash_key(const char *command, const char *const *table);

/* skill_commands.py:_setup_note -- setup_skipped wins, then a non-empty
 * gateway_setup_hint, then setup_note when setup_needed. Caller frees. */
char *sc_setup_note(int setup_skipped, const char *gateway_setup_hint,
                    int setup_needed, const char *setup_note);

/* skill_commands.py:format_missing_skills -- ambiguous entries keep their own
 * wording, the rest collapse to one "Unknown skill(s): ..." entry. Caller
 * frees. */
char *sc_format_missing_skills(const char *const *missing);

#endif /* PORT_SKILL_COMMANDS_H */
