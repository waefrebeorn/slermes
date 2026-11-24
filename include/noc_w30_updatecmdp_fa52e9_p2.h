/*
 * noc_w30_updatecmdp_fa52e9_p2.h - C surface for upstream
 * hermes_cli/update_cmd_posix_pause.py (gate w30_updatecmdp_fa52e9_p2).
 *
 * The naming, drain-ACK, supervisor-attribution, left-running and refusal
 * surfaces of the POSIX pause: what a paused set is called, whether a gateway
 * answered the drain, why a bare gateway is supervised, which runtimes stay up
 * because stopping them would take the updater down, the replay environment
 * cleanup and the adopted-pid rule.
 *
 * Every function takes the whole JSON request object (a fixture line); the
 * process/psutil/launchd answers arrive as fixture values.
 */
#ifndef NOC_W30_UPDATE_CMDP_FA52E9_P2_H
#define NOC_W30_UPDATE_CMDP_FA52E9_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _names(): req.entries ([{unit?, label?, pid}]) -> the comma-joined names. */
json_t *w30_updatecmdp_fa52e9_p2_names(const json_t *req);

/* _describe(): req.token {posix_units: [{unit?, label?}], unmapped: [{pid, argv?}]}
 * -> the comma-joined description. */
json_t *w30_updatecmdp_fa52e9_p2_describe(const json_t *req);

/* _ask_to_drain(): req.home, req.ack (control-socket answer|null),
 * req.raises (the socket call failed) -> bool. */
json_t *w30_updatecmdp_fa52e9_p2_drain_ack(const json_t *req);

/* _supervisor_markers(): req.argv_external, req.env, req.respawning_unit,
 * req.env_error -> string|null. */
json_t *w30_updatecmdp_fa52e9_p2_supervisor_marker(const json_t *req);

/* _left_running(): req.units [{scope, unit, escape_ok}], req.jobs
 * [{label, pid, ancestor}], req.needs_sudo, req.sudo_ok, req.xpc ->
 * {"keep_units": [...], "keep_jobs": [...], "notices": [...]}. */
json_t *w30_updatecmdp_fa52e9_p2_left_running(const json_t *req);

/* _abort_unsettled(): req.why, req.token -> {"message": ..., "token": ...}. */
json_t *w30_updatecmdp_fa52e9_p2_abort_unsettled(const json_t *req);

/* _bare_replay_env(): req.env (the replay environment) -> the stripped env. */
json_t *w30_updatecmdp_fa52e9_p2_bare_replay_env(const json_t *req);

/* _gateway_on_home(): req.home_pid (identity reader answer), req.error,
 * req.exclude ([pid, ...]) -> int|null. */
json_t *w30_updatecmdp_fa52e9_p2_gateway_on_home(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W30_UPDATE_CMDP_FA52E9_P2_H */
