/*
 * cli_command_registry.h — pure data-model + walk port of hermes_cli/commands.py
 *
 * Faithful C port of the command-registry slice that backs both the gateway's
 * COMMANDS dict and the shell-completion _walk():
 *
 *   - command_def_t                 (mirrors CommandDef: name, description,
 *                                     category, aliases, args_hint, subcommands,
 *                                     gateway_only)
 *   - COMMAND_REGISTRY              (the canonical list of commands)
 *   - resolve_command(name)         (mirrors resolve_command: name/alias,
 *                                     strips leading '/', case-insensitive)
 *   - cli_build_commands_index()    (mirrors COMMANDS flat dict)
 *   - cli_build_subcommands_index() (mirrors SUBCOMMANDS + pipe-hint fallback)
 *   - completion_walk()             (the C equivalent of completion.py:_walk:
 *                                     turns COMMAND_REGISTRY into the
 *                                     completion_node tree that generate_bash/
 *                                     zsh/fish already consume)
 *
 * No argparse, no TTY. The registry IS the canonical source the Python code
 * derives its structures from, so walking it is a faithful port of _walk().
 *
 * Opaque-friendly, minimal includes, C11.
 */

#ifndef CLICOMMANDREGISTRY_H
#define CLICOMMANDREGISTRY_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A command definition (mirrors CommandDef). Distinct name from the live
 * dispatch's command_def_t (hermes_cli.h) so both can coexist in one TU:
 * this is the faithful data-model port (aliases array, no handler). */
typedef struct cli_command_def_t {
    const char *name;          /* canonical, no slash: "background" */
    const char *description;   /* human-readable */
    const char *category;      /* "Session", "Configuration", ... */
    const char *const *aliases;     /* NULL-terminated */
    const char *args_hint;     /* "<prompt>", "[name]", "[on|off|tts|status]" */
    const char *const *subcommands; /* NULL-terminated */
    bool gateway_only;         /* only in gateway/messaging */
    const char *busy_policy;   /* "dispatch" | "reject" | "interrupt_then_dispatch" */
    const char *busy_handler;  /* handler key when busy_policy != "dispatch" */
} cli_command_def_t;

/* PoP: is_interrupt_then_dispatch @ hermes_cli/commands.py:is_interrupt_then_dispatch */
/* Return true when command_name must interrupt a running agent first.
 * True for commands whose busy_policy is "interrupt_then_dispatch"
 * (the /stop, /new, /reset class). Accepts aliases. */
bool cli_is_interrupt_then_dispatch(const char *command_name);

/* The canonical registry (mirrors COMMAND_REGISTRY). NULL-terminated.
 * Populated in cli_command_registry.c. */
extern const cli_command_def_t *const CLI_COMMAND_REGISTRY[];
extern size_t CLI_COMMAND_REGISTRY_COUNT;

/* Resolve a command name or alias to its CommandDef. Accepts names with or
 * without a leading slash, case-insensitive. Returns NULL if not found. */
const cli_command_def_t *cli_resolve_command(const char *name);

/* Build a flat "/name" -> description index over non-gateway commands.
 * Returns a heap array of (key, value) pairs, NULL-terminated. Caller frees
 * with cli_free_index(). */
typedef struct { char *key; char *value; } cli_index_entry_t;
cli_index_entry_t *cli_build_commands_index(void);
cli_index_entry_t *cli_build_subcommands_index(void);
void cli_free_index(cli_index_entry_t *idx);

/* completion_walk: the C equivalent of completion.py:_walk.
 * Walks COMMAND_REGISTRY (non-gateway commands) and builds the
 * completion_node tree consumed by generate_bash/zsh/fish. The returned
 * array is NULL-terminated; caller frees with completion_node_free on each
 * element then frees the array. `help_cb` (optional) supplies help text for a
 * command name; if NULL, the command's description is used. */
struct completion_node_t;
struct completion_node_t **completion_walk(
    const char *(*help_cb)(const char *name, void *ctx), void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* CLICOMMANDREGISTRY_H */
