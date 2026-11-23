#ifndef NOC_W9_ACTIVELEASE_182481_H
#define NOC_W9_ACTIVELEASE_182481_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_activelease_182481_release @ gateway/active_lease_io.py:release_soon
 * Mirrors release_soon + _release with an injected pool state and a fake lease.
 * Returns {"marked":bool,"pool_created":bool,"submitted":bool,
 *          "released":bool,"propagated":bool}. Caller frees. */
struct json_t *w9_activelease_182481_release(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_ACTIVELEASE_182481_H */
