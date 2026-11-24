#ifndef NOC_W20_HOSTEDROOM_DFC5CA_H
#define NOC_W20_HOSTEDROOM_DFC5CA_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tui_gateway/hosted_room_peer_deadline.py (gate w20_hostedroom_dfc5ca):
 * whole-request HTTP deadlines for the room supervisor's bounded maintenance.
 *
 * The opener, TLS and the bounded response reader stay upstream. What is ported
 * is the budget itself -- one deadline for the whole request, every socket
 * tracked so close() can interrupt a buffered read -- plus the admission rule
 * that bounds outstanding attempts per target. Time is injected so the state
 * machine is exercised deterministically.
 */

/* _MAX_OUTSTANDING_ATTEMPTS */
int w20_hostedroom_dfc5ca_max_outstanding_attempts(void);

/* PoP: w20_hostedroom_dfc5ca_budget_remaining @ tui_gateway/hosted_room_peer_deadline.py:_Budget.remaining
 * Seconds left in the budget, or {"error":"peer request deadline exceeded"} once
 * the budget is closed or the deadline has passed (remaining <= 0). */
json_t *w20_hostedroom_dfc5ca_budget_remaining(double timeout, double elapsed, int closed);

/* PoP: w20_hostedroom_dfc5ca_budget_track @ tui_gateway/hosted_room_peer_deadline.py:_Budget.track
 * Register a socket for close(); a socket handed to an already-closed budget is
 * closed on the spot and refused. */
json_t *w20_hostedroom_dfc5ca_budget_track(int closed);

/* PoP: w20_hostedroom_dfc5ca_budget_close @ tui_gateway/hosted_room_peer_deadline.py:_Budget.close
 * Close the budget: mark it closed, shut down (interrupting a buffered read) and
 * close every tracked socket, then forget them. Shutdown errors are suppressed
 * exactly like `with suppress(OSError)`, so the socket is still closed.
 *   {"closed_sockets":n, "shutdown_calls":k, "sockets":0, "closed":true,
 *    "remaining_error":"peer request deadline exceeded"} */
json_t *w20_hostedroom_dfc5ca_budget_close(double timeout, int sockets, int shutdown_fails);

/* PoP: w20_hostedroom_dfc5ca_open_plan @ tui_gateway/hosted_room_peer_deadline.py:open_peer_url
 * The prologue of one bounded request: the target tuple (host, port -- defaulting
 * to 443/https or 80/http), the outstanding-attempt admission (a target already
 * in flight, or _MAX_OUTSTANDING_ATTEMPTS of them, is refused), then the budget's
 * remaining time (an expired budget refuses before anything is sent).
 *   {"target":[host,port],"status":"ok"} | {"status":"error","message":...}
 * `targets` is the set of (host, port) pairs currently outstanding. */
json_t *w20_hostedroom_dfc5ca_open_plan(const json_t *targets, const char *host,
                                        const json_t *port, const char *scheme,
                                        double timeout, double elapsed);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_HOSTEDROOM_DFC5CA_H */
