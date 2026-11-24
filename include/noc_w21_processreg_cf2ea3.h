/*
 * noc_w21_processreg_cf2ea3.h -- public C API for the port of upstream Hermes
 *   tools/process_registry_env_scope.py (gate w21_processreg_cf2ea3).
 *
 * Ported: the environment-identity rule of _in_env (env_ref identity wins,
 * then the task_id fallback for local/host-recovered sessions),
 * has_running_in_env over the running set, and the whole settle_env_closed
 * ladder (None env, a backend whose cleanup() stops processes, the
 * env_ref+not-exited snapshot and the lost settlement).
 */
#ifndef NOC_W21_PROCESSREG_CF2EA3_H
#define NOC_W21_PROCESSREG_CF2EA3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _in_env(session, key, env). `session`:
 *   env_ref  -- "e" when session.env_ref is the env under test, any other tag
 *               for a different env object, null when there is no env_ref
 *   task_id  -- session.task_id
 * `env_tag` is "e" when the env under test is a live object, null for None. */
int w21_processreg_cf2ea3_in_env(const json_t *session, const char *key,
                                 const char *env_tag);

/* has_running_in_env(key, env): {"env_tag","key","sessions":[...]}
 * -> bool (a still-running process lives in that environment). */
json_t *w21_processreg_cf2ea3_has_running(const json_t *world);

/* settle_env_closed(env): {"env_tag","cleanup_stops_processes","sessions":[...]}
 * -> the number of sessions settled as lost. A missing cleanup flag reads as
 * True (getattr(env, "cleanup_stops_processes", True)). */
json_t *w21_processreg_cf2ea3_settle(const json_t *world);

/* Probe of the harness environment: {"sler":bool,"home":bool,"same":bool}. */
json_t *w21_processreg_cf2ea3_env_probe(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_PROCESSREG_CF2EA3_H */
