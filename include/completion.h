/*
 * completion.h — opaque API for shell-completion script generation.
 *
 * Faithful C port of the pure, deterministic string-generation slice of
 * hermes_cli/completion.py:
 *   - _clean(text)                 (strip shell-unsafe chars, truncate 60)
 *   - generate_bash(tree)         (bash completion script)
 *   - generate_zsh(tree)          (zsh completion script)
 *   - generate_fish(tree)         (fish completion script)
 *
 * The Python module walks a live argparse parser. The C port accepts a
 * caller-built tree (completion_node tree) instead — same shape, no argparse
 * dependency. The walk/templating logic and the exact generated script text
 * are preserved.
 *
 * Opaque structs + minimal includes. Generators return heap strings the
 * caller must free.
 *
 * PoP: _clean         @ hermes_cli/completion.py:_clean
 * PoP: generate_bash  @ hermes_cli/completion.py:generate_bash
 * PoP: generate_zsh   @ hermes_cli/completion.py:generate_zsh
 * PoP: generate_fish  @ hermes_cli/completion.py:generate_fish
 */

#ifndef COMPLETION_H
#define COMPLETION_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct completion_node_t completion_node_t;

/* A node in the command tree.
 *   name        — canonical subcommand name (e.g. "profile")
 *   help        — help text (may contain shell-unsafe chars; cleaned on render)
 *   flags       — NULL-terminated array of flag strings (e.g. "-p", "--profile")
 *   subcommands — NULL-terminated array of child nodes (sorted by caller or
 *                 by the generators; generators sort themselves)
 */
struct completion_node_t {
    char *name;
    char *help;
    char **flags;          /* NULL-terminated */
    completion_node_t **subcommands;  /* NULL-terminated */
};

/* Build a node. name/help are copied; flags/subcommands arrays are adopted
 * (caller transfers ownership of the arrays and their strings). Pass NULL
 * for flags/subcommands when absent. */
completion_node_t *completion_node_new(const char *name, const char *help,
                                        char **flags, completion_node_t **subcommands);

/* Free a node tree recursively. Safe with NULL. */
void completion_node_free(completion_node_t *node);

/* Strip shell-unsafe characters (' " \) and truncate to maxlen (default 60).
 * Returns a heap string (caller frees). */
char *completion_clean(const char *text, size_t maxlen);

/* Generate a completion script for the given top-level command tree.
 * `tree` is a NULL-terminated array of top-level command nodes. Returns a
 * heap string (caller frees) or NULL on alloc failure. */
char *completion_generate_bash(completion_node_t *const *tree);
char *completion_generate_zsh(completion_node_t *const *tree);
char *completion_generate_fish(completion_node_t *const *tree);

#ifdef __cplusplus
}
#endif

#endif /* COMPLETION_H */
