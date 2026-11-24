/*
 * noc_w21_refreshsin_e3dbfd.h -- public C API for the C11 port of upstream Hermes
 *   hermes_cli/dashboard_auth/refresh_singleflight.py
 *
 * Single-flight + short replay cache for rotating refresh tokens. Every
 * exported symbol carries the w21_refreshsin_e3dbfd_ prefix so it can share a
 * link line with the rest of the tree.
 *
 * The provider stack router (refresh_session_coalesced / scan_session_providers)
 * and the lock-scoped flight bookkeeping stay upstream: they are bound to live
 * provider objects and threads. What is ported here is the digest, the replay
 * cache graph (forget), the prune policy and the cache decision of
 * _refresh_provider.
 */
#ifndef NOC_W21_REFRESHSIN_E3DBFD_H
#define NOC_W21_REFRESHSIN_E3DBFD_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module constants (refresh_singleflight.py: _SUCCESS_TTL/_FAILURE_TTL/_MAX_ENTRIES). */
#define W21_REFRESHSIN_E3DBFD_SUCCESS_TTL 30.0
#define W21_REFRESHSIN_E3DBFD_FAILURE_TTL 5.0
#define W21_REFRESHSIN_E3DBFD_MAX_ENTRIES 256

/* sha256(token) as lower-case hex; caller frees. NULL token reads as "". */
char *w21_refreshsin_e3dbfd_digest(const char *token);

/* The three module constants as {"success_ttl","failure_ttl","max_entries"}. */
json_t *w21_refreshsin_e3dbfd_constants(void);

/*
 * forget(): follow the rotation chain both ways over a cache snapshot.
 *   entries: [{"provider":int,"token":str,"issued":str|null}, ...]
 *   flights: [{"provider":int,"token":str,"generation":int}, ...]
 * Returns {"removed":[labels sorted],"remaining":[labels in insertion order],
 *          "generations":[labels with the post-forget generation, sorted]}.
 * A label is "<provider>:<token>" (generation rows append ":<generation>").
 */
json_t *w21_refreshsin_e3dbfd_forget(const json_t *entries, const json_t *flights,
                                     const char *token);

/*
 * _prune(): drop entries whose expiry is <= now, then cap the cache at
 * _MAX_ENTRIES by evicting the smallest expiry (first-seen on ties).
 *   entries: [{"token":str,"expires":number}, ...]
 * Returns {"remaining":[tokens in insertion order],"dropped":int}.
 */
json_t *w21_refreshsin_e3dbfd_prune(const json_t *entries, double now);

/*
 * The cache half of _refresh_provider() for one already-serialized flight.
 *   req: {"token":str, "reuse_detection":bool,
 *         "expired":bool            (provider raised RefreshExpiredError)
 *         "session":null|{"refresh_token":str}
 *         "cached":null|{"expires_in":number,"session":null|{...}}
 *         "now":number}
 * Returns {"session":..., "hit":bool, "cached":bool, "ttl":number|null}.
 * Nothing here performs I/O: the provider answer is injected, which is the
 * network seam the Python code wraps in provider.refresh_session(). `now` is
 * only informational -- `cached.expires_in` already carries `expiry - now`, so
 * the `cached[0] > time.monotonic()` test is a sign check here.
 */
json_t *w21_refreshsin_e3dbfd_refresh(const json_t *req);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_REFRESHSIN_E3DBFD_H */
