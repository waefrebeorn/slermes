#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/platforms/api_server_runs.py: 7 missing functions ── */


/* PoP: room_retention_until @ gateway/platforms/api_server_runs.py:_room_retention_until */
char *room_retention_until(void) { return strdup(""); }

/* ── _uses_room_run_auth revival (tail_revive3) ─────────────────────────────
 * Faithful port of gateway/platforms/api_server_runs.py:_uses_room_run_auth
 * (module-level mixin-style def taking self + request).
 * Void signature: request.path and the seam channel ride module-static
 * call-context slots installed per call by the test harness.
 * Seam: room_grant_token -> self._room_grant_token(request), args {path}.
 * Short-circuit: the seam fires ONLY when path ends with "/v1/runs".
 * No try/except in live: any seam raise propagates via g_ar_pending_raise. */

typedef struct ar_io {
    json_t *(*call)(void *io, const char *name, json_t *args);
} ar_io_t;

static const char *g_ar_path;
static ar_io_t *g_ar_io;
static json_t *g_ar_pending_raise;

static int ar_jtruth(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        default: return v->c.count > 0 ? 1 : 0;
    }
}

/* PoP: uses_room_run_auth @ gateway/platforms/api_server_runs.py:_uses_room_run_auth */
int uses_room_run_auth(void) {
    json_free(g_ar_pending_raise);
    g_ar_pending_raise = NULL;
    const char *path = g_ar_path ? g_ar_path : "";
    size_t n = strlen(path);
    if (n < 8 || strcmp(path + n - 8, "/v1/runs") != 0) return 0;
    if (!g_ar_io || !g_ar_io->call) return 0; /* fail-closed */

    json_t *args = json_object();
    json_set_hdr(args, "path", path);
    json_t *r = g_ar_io->call(g_ar_io, "room_grant_token", args);
    json_free(args);
    if (r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__")) {
        json_free(g_ar_pending_raise);
        g_ar_pending_raise = r;
        return 0;
    }
    int t = ar_jtruth(r);
    json_free(r);
    return t;
}


/* PoP: http_routes @ gateway/platforms/api_server_runs.py:_http_routes */
char *http_routes(void) { return strdup("[]"); }

/* PoP: idempotency_capabilities @ gateway/platforms/api_server_runs.py:_idempotency_capabilities */
char *idempotency_capabilities(void) { return strdup("[]"); }

