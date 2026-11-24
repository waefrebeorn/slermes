/*
 * noc_w30_updatecmdp_fa52e9_p1.h - C surface for upstream
 * hermes_cli/update_cmd_posix_pause.py (gate w30_updatecmdp_fa52e9_p1).
 *
 * The pause token's shape, the pause predicate, cgroup containment, the
 * scope/privilege command prefixes, the unit-stop settling predicate and the
 * pid identity rules (native start time).  Every process/systemd/psutil seam
 * arrives as a fixture value in the request object; nothing here reads /proc,
 * spawns systemctl or touches the filesystem.
 *
 * Each function takes the whole JSON request object (a fixture line).  The
 * field names are listed per function; unknown fields are ignored.
 */
#ifndef NOC_W30_UPDATE_CMDP_FA52E9_P1_H
#define NOC_W30_UPDATE_CMDP_FA52E9_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _empty_token(): {"platform":"posix","resume_needed":false,"profiles":{},
 * "unmapped_pids":[],"unmapped":[],"posix_units":[]}. */
json_t *w30_updatecmdp_fa52e9_p1_empty_token(void);

/* gateways_paused(): req.token (object|null) -> bool. */
json_t *w30_updatecmdp_fa52e9_p1_gateways_paused(const json_t *req);

/* _inside(): req.cgroup, req.unit_cgroup (strings|null) -> bool. */
json_t *w30_updatecmdp_fa52e9_p1_inside(const json_t *req);

/* _scope_cmd(): req.scope (string|null) -> ["systemctl", "--user"] |
 * ["systemctl"]. */
json_t *w30_updatecmdp_fa52e9_p1_scope_cmd(const json_t *req);

/* _manage_cmd(): req.scope, req.needs_sudo, req.sudo_ok -> array|null.
 * needs_sudo/sudo_ok are the seam answers for _needs_sudo(scope) and
 * _sudo_noninteractive_ok(probe). */
json_t *w30_updatecmdp_fa52e9_p1_manage_cmd(const json_t *req);

/* unit_stop_pending(): req.kind, req.state (systemd show dict|null) -> bool. */
json_t *w30_updatecmdp_fa52e9_p1_unit_stop_pending(const json_t *req);

/* _alive(): req.pid, req.pid_exists ("yes"|"no"|"error"), req.start_time,
 * req.ct -> bool. */
json_t *w30_updatecmdp_fa52e9_p1_alive(const json_t *req);

/* is_left_pending(): req.left_pending ([[pid, ct], ...]), req.pid, req.ct
 * (the live start time) -> bool. */
json_t *w30_updatecmdp_fa52e9_p1_is_left_pending(const json_t *req);

/* adopt_left_pending(): req.pairs ([[pid, ct], ...]), req.start_times
 * ({"<pid>": <start time|null>}) -> [[pid, ct], ...] sorted by pid. */
json_t *w30_updatecmdp_fa52e9_p1_adopt_left_pending(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W30_UPDATE_CMDP_FA52E9_P1_H */
