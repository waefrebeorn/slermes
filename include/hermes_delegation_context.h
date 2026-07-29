/* hermes_delegation_context.h
 *
 * Minimal public API for port_agent_delegation_context.c
 */

#ifndef HERMES_DELEGATION_CONTEXT_H
#define HERMES_DELEGATION_CONTEXT_H

#include <stdbool.h>

/* Enter a delegated-child execution context. Returns true on success. */
extern bool delegated_child_context_enter(void);

/* Exit a delegated-child execution context. */
extern void delegated_child_context_exit(void);

/* True while running for a delegate_task child. */
extern bool is_delegated_child_context(void);

/* True in this process or any subprocess spawned by a delegated child. */
extern bool is_delegated_child_process_context(void);

/* Scrubbed env: removes HERMES_KANBAN_* keys, adds lineage marker.
 * Caller frees the returned NULL-terminated array with scrub_kanban_env_free(). */
extern char **scrub_kanban_env(char **env);

/* Subprocess env helper. If not delegated, returns a shallow copy of env
 * (or NULL if env is NULL). If delegated, returns scrub_kanban_env(env). */
extern char **delegated_child_subprocess_env(char **env);

#endif /* HERMES_DELEGATION_CONTEXT_H */
