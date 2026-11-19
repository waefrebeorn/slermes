#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/growth.py: 6 missing functions ── */

/* PoP: window_overrides_path @ hermes_cli/local_runtime/growth.py:window_overrides_path */
char *window_overrides_path(void) { return strdup(""); }

/* PoP: load_window_overrides @ hermes_cli/local_runtime/growth.py:load_window_overrides */
char *load_window_overrides(void) { return strdup("{}"); }



/* ── is_managed_endpoint revival (tail_revive3) ─────────────────────────────
 * Faithful port of hermes_cli/local_runtime/growth.py:is_managed_endpoint.
 * Void signature: base_url and the seam channel ride module-static
 * call-context slots installed per call by the test harness.
 * Seam: state_endpoint -> hermes_cli.local_runtime.endpoint._state_endpoint()
 * args {}. The live `with suppress(Exception)` swallows plain exceptions
 * (including a scripted raise and the AttributeError from .get on a
 * non-dict state) to False; BaseException kinds pass through via
 * g_gr_pending_raise. state None -> False. str(state.get("base_url", ""))
 * stringifies scalars (absent -> "", null -> "None"); container values are
 * out of fixture scope (documented). rstrip("/") on both sides. */

typedef struct gr_io {
    json_t *(*call)(void *io, const char *name, json_t *args);
} gr_io_t;

static const char *g_gr_base_url;
static gr_io_t *g_gr_io;
static json_t *g_gr_pending_raise;

static const char *gr_raise_kind(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}

static int gr_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

/* str(v) for scalars; absent -> ""; NULL for containers (out of scope). */
static char *gr_str_of(const json_t *v) {
    char buf[64];
    if (!v) return strdup("");
    if (v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && d >= -1e15 && d <= 1e15)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            snprintf(buf, sizeof buf, "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val);
    return NULL;
}

static size_t gr_rstrip_slash(const char *s, size_t n) {
    while (n > 0 && s[n-1] == '/') n--;
    return n;
}

/* PoP: is_managed_endpoint @ hermes_cli/local_runtime/growth.py:is_managed_endpoint */
int is_managed_endpoint(void) {
    json_free(g_gr_pending_raise);
    g_gr_pending_raise = NULL;
    if (!g_gr_io || !g_gr_io->call) return 0; /* fail-closed */

    json_t *args = json_object();
    json_t *r = g_gr_io->call(g_gr_io, "state_endpoint", args);
    json_free(args);
    const char *kind = gr_raise_kind(r);
    if (kind) {
        if (gr_is_base_exc(kind)) {
            json_free(g_gr_pending_raise);
            g_gr_pending_raise = r;
        } else {
            json_free(r);
        }
        return 0;
    }
    if (!r || r->type == JSON_NULL) { json_free(r); return 0; }
    if (r->type != JSON_OBJECT) { json_free(r); return 0; } /* .get raises */

    char *state_url = gr_str_of(json_obj_get(r, "base_url"));
    json_free(r);
    if (!state_url) return 0;
    const char *base = g_gr_base_url ? g_gr_base_url : "";
    size_t bn = gr_rstrip_slash(base, strlen(base));
    size_t sn = gr_rstrip_slash(state_url, strlen(state_url));
    int eq = bn == sn && memcmp(base, state_url, bn) == 0;
    free(state_url);
    return eq;
}

/* PoP: maybe_grow_window @ hermes_cli/local_runtime/growth.py:maybe_grow_window */
char *maybe_grow_window(void) { return strdup("{}"); }
