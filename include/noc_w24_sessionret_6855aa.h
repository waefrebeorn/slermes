/*
 * noc_w24_sessionret_6855aa.h - C surface for upstream
 * gateway/session_retry_admission.py (gate w24_sessionret_6855aa).
 *
 * One ``/retry`` rewrite bound to the re-send's own native admission.  The
 * ContextVar that carries the binding, the admission gate, the queue and the
 * commit are seams: the bound value arrives as a value, and this file owns the
 * decision ``take`` makes on it -- once, and only for the identical event source
 * on the identical store.
 *
 * Object identity (``is``) is carried by an identity token: two sides are the
 * same object exactly when their tokens are the same string, and a null token is
 * Python's None on both sides.
 */
#ifndef NOC_W24_SESSIONRET_6855AA_H
#define NOC_W24_SESSIONRET_6855AA_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Python ``a is b`` over identity tokens (equal strings, or None on both sides). */
int w24_sessionret_6855aa_identity(const json_t *a, const json_t *b);

/* RetryRewrite(store=..., source=...): the dataclass with its defaults
 * (applied False, taken False). */
json_t *w24_sessionret_6855aa_rewrite(const json_t *store, const json_t *source);

/* take(event, store) called *calls* times on one binding:
 *   bound       null, or {"store": tok, "source": tok, "taken": bool, "applied": bool}
 *   event_source the admission's SessionSource token (null when it has none)
 *   store       the admitting store's token
 * -> one entry per call: the rewrite ({"store", "source", "taken", "applied"})
 *    where that admission took it, else null.  A drain task inheriting the
 *    context, a second admission, or another store's admission takes nothing. */
json_t *w24_sessionret_6855aa_take(const json_t *bound,
                                   const json_t *event_source,
                                   const json_t *store,
                                   int calls);

#ifdef __cplusplus
}
#endif
#endif
