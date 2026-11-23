#ifndef NOC_W5_RELAYCWD_C03229_H
#define NOC_W5_RELAYCWD_C03229_H

/*
 * Public C API for the port of agent/relay_cwd.py (gate w5_relaycwd_c03229).
 * All exported symbols are relaycwd_-prefixed to avoid collisions.
 *
 * resolve_relay_scope_cwds reads many runtime lookups; the C port takes their
 * already-resolved values in `facts` and reproduces the exact precedence
 * tree. Host-side path resolution is supplied as a resolved absolute path.
 */
#include "hermes_json.h"

/* Port of _clean_cwd(value): strip, then "" for the sentinel set
 * {"", ".", "./", "auto", "cwd"} (case-insensitive), else the stripped value.
 * Returns a json string (caller frees). */
json_t *relaycwd_clean(const json_t *value);

/*
 * Port of resolve_relay_scope_cwds(). `facts`:
 *   task_cwd      : get_session_cwd(task_id) result (str|null)
 *   scoped_cwd    : scoped_session_cwd() result (str|null)
 *   session_key   : HERMES_SESSION_KEY value (str)
 *   recorded      : {key: cwd} map used by get_session_cwd(key)
 *   agent_cwd     : agent.session_cwd (str|null)
 *   terminal_env  : TERMINAL_ENV value (str, default "local")
 *   terminal_cwd  : TERMINAL_CWD value (str)
 *   resolved_cwd  : resolved absolute host path (str|null)
 *   platform      : platform string
 * Returns json array [session_cwd, turn_cwd] (caller frees).
 */
json_t *relaycwd_resolve(const json_t *facts);

#endif /* NOC_W5_RELAYCWD_C03229_H */
