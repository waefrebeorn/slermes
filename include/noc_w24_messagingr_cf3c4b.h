/*
 * noc_w24_messagingr_cf3c4b.h - C surface for upstream
 * hermes_cli/web_routers/messaging_relay.py (gate w24_messagingr_cf3c4b).
 *
 * The Channel-linking routes of the Team Gateway relay, minus the gateway: how
 * a connector answer becomes an HTTP outcome (200 + payload, 503 for the
 * not-ready codes, 409 for a refusal, 503 when nothing answered), the control
 * verb parameters each route sends, and the route/not-ready literals.  The
 * control socket, the relay secret and the timeout stay in the caller: an
 * answer arrives here as the value the gateway would have returned.
 */
#ifndef W24_MESSAGINGR_CF3C4B_H
#define W24_MESSAGINGR_CF3C4B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* answer: null | {"ok": <any>, "error": <any>, "message": <any>, ...} ->
 * {"status": <int>, "body": <what the route returns or the error detail>}.
 * A non-mapping answer is the AttributeError it raises, as {"error": "..."}. */
json_t *w24_messagingr_cf3c4b_ask(const json_t *answer);

/* req: {"action": "status"|"request"|"state"|"unlink", "platform": <any>,
 * "code": <any>, "platform_user_id": <any>} -> the params dict that route
 * sends to the gateway's relay-link verb (null for an unknown action). */
json_t *w24_messagingr_cf3c4b_params(const json_t *req);

/* req: {"code": <any>} -> whether that code means "not ready yet". */
json_t *w24_messagingr_cf3c4b_not_ready(const json_t *req);

/* The not-ready codes and the route table. */
json_t *w24_messagingr_cf3c4b_constants(void);

#ifdef __cplusplus
}
#endif
#endif
