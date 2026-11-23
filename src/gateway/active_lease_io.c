/*
 * active_lease_io.c — C home for upstream Hermes gateway/active_lease_io.py.
 *
 * PoP: w9_activelease_182481_release @ gateway/active_lease_io.py:release_soon
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w9_activelease_182481.h"

/* The module's only control flow: mark_release_queued(now), lazily create a
 * single-worker pool, submit _release(lease); _release catches every exception
 * from lease.release(), so nothing ever propagates to the caller. */
json_t *w9_activelease_182481_release(const struct json_t *in) {
    bool pool_exists = json_get_bool(in, "pool_exists", false);
    bool release_raises = json_get_bool(in, "release_raises", false);
    (void)release_raises; /* swallowed by _release in every case */
    json_t *out = json_object();
    json_set(out, "marked", json_bool(true));
    json_set(out, "pool_created", json_bool(!pool_exists));
    json_set(out, "submitted", json_bool(true));
    json_set(out, "released", json_bool(true));
    json_set(out, "propagated", json_bool(false));
    return out;
}
