/*
 * noc_w24_wstickets_fa7658.h - C surface for upstream
 * hermes_cli/dashboard_auth/ws_tickets.py (gate w24_wstickets_fa7658).
 *
 * The credential algebra of gated-mode WebSocket auth: the ``info`` dict a
 * minted ticket carries, the accept/expire/unknown decision ``consume_ticket``
 * makes, which entries the expiry sweep drops, and the constant-time internal
 * credential check.  The secret source (``secrets.token_urlsafe``), the clock,
 * the lock and the ``_tickets`` dict are seams -- the caller owns them; this
 * file owns only what the stored values decide.
 */
#ifndef NOC_W24_WSTICKETS_FA7658_H
#define NOC_W24_WSTICKETS_FA7658_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {user_id, provider, issuer, extra, now} -> the ticket's info object:
 *   {"user_id", "provider", "minted_at": int(now), **extra}, plus "issuer"
 *   only when issuer is truthy (after the merge, so it wins).  A truthy
 *   non-mapping ``extra`` is the ``**`` TypeError seam -> null. */
json_t *w24_wstickets_fa7658_mint_info(const json_t *req);

/* {ticket, entry, now} -> {"ok": true, "info": {...}} or
 * {"ok": false, "error": "..."}.  entry is null for a ticket that is not in
 * the store; otherwise it is the stored pair [expires_at, info]. */
json_t *w24_wstickets_fa7658_consume(const json_t *req);

/* {tickets: [{ticket, expires_at}, ...], now} -> the ticket values the sweep
 * keeps (expires_at >= now), in store order. */
json_t *w24_wstickets_fa7658_gc_keep(const json_t *req);

/* {value, expected} -> {"ok": true, "info": {user_id, provider}} or
 * {"ok": false, "error": "..."}.  expected is null until a credential has been
 * minted; a non-string value is the ``.encode()`` AttributeError seam -> null. */
json_t *w24_wstickets_fa7658_consume_internal(const json_t *req);

/* TTL_SECONDS / INTERNAL_USER_ID / INTERNAL_PROVIDER. */
json_t *w24_wstickets_fa7658_constants(void);

#ifdef __cplusplus
}
#endif
#endif
