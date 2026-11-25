/*
 * noc_w34_webhostter_a3f534_p1.h - C surface for hermes_cli/
 * web_host_terminal_sessions.py, part 1 (gate w34_webhostter_a3f534_p1):
 * registry shaping and admission decisions.
 *
 * The module owns host admission for process-lifetime Webapp shells; the PTY
 * registry it extends owns processes and drains.  Everything ported here is a
 * pure function of values: which identity a request carries, whether a profile
 * home is still live, which home a ``profile`` query parameter selects, the
 * verdict of ``HostTerminalRegistry.resolve`` (attach/deny/expire), the
 * registry lookup ``get_host_terminals`` performs, the lifespan registry stack
 * teardown, and one reaper tick's retirements plus its bounded exit memory.
 *
 * Seams (in the caller, never here): the event loop and its threads, the PTY
 * spawn/drain/write path, WebSocket objects, the reaper task, path resolution
 * (``Path.resolve``), and every filesystem read behind ``_home_live``.
 */
#ifndef NOC_W34_WEBHOSTTER_A3F534_P1_H
#define NOC_W34_WEBHOSTTER_A3F534_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Budgets the admission decisions are made against. */
#define W34_WEBHOSTTER_A3F534_RETENTION_SECONDS 900.0  /* 15 * 60 */
#define W34_WEBHOSTTER_A3F534_MAX_SHELLS 16
#define W34_WEBHOSTTER_A3F534_EXITS_KEPT 64            /* 4 * _MAX_SHELLS */
#define W34_WEBHOSTTER_A3F534_TOKEN_LEN 43

/* The module's constants, as an object. */
json_t *w34_webhostter_a3f534_p1_constants(void);

/* identity: {"user_id":..,"provider":..} | null -> {"provider","user"} or
 * {"denied": true} (a missing, empty or non-string member denies). */
json_t *w34_webhostter_a3f534_p1_request_identity(const json_t *state);

/* state: {named, exists, tombstoned, is_dir} -> whether the home is live:
 * a named home must exist with no tombstone; any home must be a directory. */
json_t *w34_webhostter_a3f534_p1_home_live(const json_t *state);

/* state: {profile, current_home, profiles{name:dir}} -> {"home": dir} or
 * {"expired": true} (unknown profile == the HTTPException the route maps to
 * the 4410 close; empty/"current" selects current_home). */
json_t *w34_webhostter_a3f534_p1_request_home(const json_t *state);

/* state: {session_present, owner_present, owner_identity[2], identity[2],
 *          exits{token:[p,u]}, ssh_terminal, request_profile, owner_home,
 *          current_home, profiles{}, home_live, reap_during_validate,
 *          closing, alive, bridge_alive, expired}
 * -> {"verdict": "ok"|"denied"|"expired"|"exited"} in the order
 *    HostTerminalRegistry.resolve applies its checks. */
json_t *w34_webhostter_a3f534_p1_resolve_verdict(const json_t *state);

/* state: {present, closed} -> {"ok": true} or {"error": <message>}. */
json_t *w34_webhostter_a3f534_p1_get_registry_state(const json_t *state);

/* state: {seeded: [registry ids]|null, host_now: "ME"|<id>|null} ->
 * {registries: [ids]|null, host_terminals: <id>|null,
 *  has_registries, has_host_terminals} after one lifespan teardown. */
json_t *w34_webhostter_a3f534_p1_lifespan_exit(const json_t *state);

/* state: {now, owners[{token,identity[2],live}],
 *         sessions[{token,alive,bridge_alive,attached,last_detached_at}],
 *         exits[[token,[p,u],at], ...]}
 * -> {sessions: [token], owners: [token], exits: [[token,[p,u],at]]} after one
 *    reap_idle(now) tick. */
json_t *w34_webhostter_a3f534_p1_reap_plan(const json_t *state);

#ifdef __cplusplus
}
#endif
#endif
