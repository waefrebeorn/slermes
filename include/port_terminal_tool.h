/*
 * port_terminal_tool.h -- pure helpers ported from tools/terminal_tool.py.
 *
 * The threading / callback / registry half stays in Python.
 */
#ifndef PORT_TERMINAL_TOOL_H
#define PORT_TERMINAL_TOOL_H

#include <stddef.h>

/* _docker_volume_uses_host_path -- True when a volume spec bind-mounts a host
 * path (POSIX/relative prefix, or a Windows drive specifier). */
int tt_volume_uses_host_path(const char *volume_spec);

/* _docker_has_host_access -- True when a Docker sandbox exposes host paths.
 * `config` is a flat [key, value, ...] array; "docker_volumes" is supplied as
 * a NUL-terminated array of volume specs (cast to const char *). */
int tt_has_host_access(const char *const *config);

/* _is_isolation_override_key -- "env_type" or a *_image key. */
int tt_is_isolation_key(const char *key);

/* _qualify_task_key -- prefix a session key with the routed profile/home
 * scope, or return it unchanged when the scope is empty. Caller frees. */
char *tt_qualify_task_key(const char *key, const char *scope);

/* _resolve_container_alias -- follow the child->parent alias chain, cycle-safe.
 * `aliases` is a flat [child, parent, ...] array. Caller frees. */
char *tt_resolve_container_alias(const char *task_id, const char *const *aliases);

#endif /* PORT_TERMINAL_TOOL_H */
