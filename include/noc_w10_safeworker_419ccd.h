#ifndef NOC_W10_SAFEWORKER_419CCD_H
#define NOC_W10_SAFEWORKER_419CCD_H

#include <hermes_json.h>

/* C11 ports of agent/safe_worker_policy.py.  Every symbol is prefixed
 * w10_safeworker_419ccd_.  Each takes one borrowed json_t *args object and
 * returns a NEW json_t *.  One-shot binding / fork / runtime-import state is
 * injected through the args object ("already_bound", "runtime_imports",
 * "policy", "caller_pid") instead of mutating the host process. */
json_t *w10_safeworker_419ccd_bind_safe_worker_policy(json_t *a);
json_t *w10_safeworker_419ccd_bind_worker_config(json_t *a);
json_t *w10_safeworker_419ccd_current_policy(json_t *a);
json_t *w10_safeworker_419ccd_worker_config_snapshot(json_t *a);

#endif /* NOC_W10_SAFEWORKER_419CCD_H */
