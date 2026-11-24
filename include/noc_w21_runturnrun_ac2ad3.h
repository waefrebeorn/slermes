#ifndef NOC_W21_RUNTURNRUN_AC2AD3_H
#define NOC_W21_RUNTURNRUN_AC2AD3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/run_turn_runner_approval_transport.py
 * (gate w21_runturnrun_ac2ad3).
 *
 * `approval_transport(turn)` clones the turn's context, builds a detached
 * `TurnRunner` presenter and returns a `notify` closure; the one decision it
 * makes is the last line -- whether a chat platform can show a prompt once the
 * turn is gone:
 *
 *   platform = getattr(getattr(ctx.source, "platform", None), "value", "")
 *   if str(platform or "") not in NON_MESSAGING_SESSION_SURFACES and not local_viewer_turn(turn):
 *       notify.background_notify = background_notify
 *
 * Everything else (the closure, the presenter, the resolver) is live turn
 * plumbing, so this port owns the surface table, the attribute walk and the
 * predicate.  The table is `gateway.session_context.NON_MESSAGING_SESSION_SURFACES`,
 * matched verbatim -- it is NOT stripped or lower-cased here, unlike
 * `session_is_messaging_surface`, so `" Telegram "` is a messaging surface.
 */

/* PoP: w21_runturnrun_ac2ad3_surfaces @ gateway/run_turn_runner_approval_transport.py:NON_MESSAGING_SESSION_SURFACES
 * The frozenset as a sorted JSON array (set order is not stable in Python). */
json_t *w21_runturnrun_ac2ad3_surfaces(void);

/* PoP: w21_runturnrun_ac2ad3_platform_value @ gateway/run_turn_runner_approval_transport.py:approval_transport
 * `str(getattr(getattr(source, "platform", None), "value", "") or "")`.  A JSON
 * object is the only shape that can carry `.platform`, and only an object
 * platform can carry `.value`: a string or number platform answers "".  Returns
 * a malloc'd string the caller frees. */
char *w21_runturnrun_ac2ad3_platform_value(const json_t *source);

/* PoP: w21_runturnrun_ac2ad3_transport @ gateway/run_turn_runner_approval_transport.py:approval_transport
 * `{"platform": <str>, "attached": <bool>}`, where `attached` is true exactly
 * when the background transport is published on the notifier: the platform is a
 * messaging surface (default-deny: anything unknown counts as messaging) and
 * the run is not a local viewer turn. */
json_t *w21_runturnrun_ac2ad3_transport(const json_t *source, int local_viewer);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_RUNTURNRUN_AC2AD3_H */
