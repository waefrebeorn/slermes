/*
 * noc_w26_methodsses_f994df.h - C surface for upstream
 * tui_gateway/methods_session_interrupt.py (gate w26_methodsses_f994df).
 *
 * The three JSON-RPC handlers in that module (``session.interrupt``,
 * ``session.steer``, ``session.redirect``) are hand-written branch trees over a
 * params dict and a live session dict; the session's transport, agent object,
 * queues and locks are the seams.  Here the session is a plain value:
 *
 *   session = {
 *     "running":               bool,   # session.get("running")
 *     "agent":                 null | object,   # None == mid-turn-build window
 *     "compression_in_flight": bool,   # _session_compression_in_flight(session)
 *     "supports_steer":        bool,   # hasattr(agent, "steer")
 *     "supports_redirect":     bool,   # _supports_active_turn_redirect is True and hasattr(agent,"redirect")
 *     "accepted":              bool,   # what agent.steer()/agent.redirect() returns (default true)
 *     "agent_error":           string|null,     # non-null: the agent call raises that message
 *     "compute_host":          bool,   # _session_uses_compute_host(session)
 *     "_hosted_room_task":     object|null      # the hosted-task record, {"task_id": ...}
 *   }
 *
 * Both functions answer with the RPC envelope the handler would return:
 *   {"ok": true,  "result": {...}}              # _ok(rid, result)
 *   {"ok": false, "code": 4002, "message": ...} # _err(rid, code, msg)
 */
#ifndef NOC_W26_METHODSSES_F994DF_H
#define NOC_W26_METHODSSES_F994DF_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* session.steer / session.redirect.  verb is "steer" or "redirect"; the text
 * parameter, the 4002/4010 checks, the queue demotions and the accepted-status
 * mapping are the upstream ones. */
json_t *w26_methodsses_f994df_correction(const char *verb, const json_t *params,
                                         const json_t *session);

/* session.interrupt: the expected_hosted_task_id guard and the compute-host
 * answer shape. */
json_t *w26_methodsses_f994df_interrupt(const json_t *params, const json_t *session);

/* The method names this module registers, as an object. */
json_t *w26_methodsses_f994df_constants(void);

#ifdef __cplusplus
}
#endif
#endif
