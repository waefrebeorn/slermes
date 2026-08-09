/*
 * terminal_env_registry.h — C11 port of tools/terminal_tool.py env registry
 *
 * Stateful, faithful port of the terminal environment registry layer:
 * session-cwd map, task-override map, active-env map, and the decision
 * functions that operate on them (resolve keys, parse config, decide
 * persistence, drive cleanup). Thread-safe via an internal mutex.
 *
 * Env *execution* backends (docker/ssh/modal/daytona/vercel) live in
 * src/tools/environments*.c — this module is purely bookkeeping.
 */
#ifndef TERMINAL_ENV_REGISTRY_H
#define TERMINAL_ENV_REGISTRY_H

#include <stdbool.h>

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- session cwd (record_session_cwd / get_session_cwd / clear_session_cwd) */
void    term_rec_record_session_cwd(const char *session_key, const char *cwd);
const char *term_rec_get_session_cwd(const char *session_key); /* caller frees */
void    term_rec_clear_session_cwd(const char *session_key);

/* --- task env overrides (register_task_env_overrides / clear_task_env_overrides) */
void    term_reg_register_task_env_overrides(const char *task_id, json_t *overrides);
void    term_reg_clear_task_env_overrides(const char *task_id);

/* --- container-task id resolution */
void    term_resolve_container_task_id(const char *task_id, char *out, size_t outsz);
json_t *term_resolve_task_overrides(const char *task_id); /* returns json copy; caller frees */

/* --- cwd usability */
bool    term_is_unusable_container_cwd(const char *cwd);

/* --- env config */
json_t *term_get_env_config(void); /* returns json object; caller frees */
bool    env_truthy_raw(const char *v);

/* --- active env registry */
json_t *term_get_active_env(const char *task_id);   /* json copy or NULL; caller frees */
bool    term_is_persistent_env(const char *task_id);
void    term_env_set_active(const char *task_id, json_t *env_entry);
void    term_cleanup_vm(const char *task_id, bool force_remove);
int     term_cleanup_all_environments(void);
int     term_cleanup_inactive_envs(long lifetime_seconds);

/* --- requirements + lifecycle */
bool    term_check_terminal_requirements(void);
void    term_env_registry_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* TERMINAL_ENV_REGISTRY_H */
