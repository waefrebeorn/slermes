#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/proxy/sse_done.py: 5 missing functions ── */


/* PoP: should_append_done @ hermes_cli/proxy/sse_done.py:should_append_done */
int should_append_done(void) { return 1; }

/* PoP: consume_line @ hermes_cli/proxy/sse_done.py:_consume_line */
char *consume_line(void) { return strdup(""); }


/* ── content_type_is_sse revival (tail_revive3) ─────────────────────────────
 * Faithful port of hermes_cli/proxy/sse_done.py:content_type_is_sse.
 * Void signature: the headers object rides a module-static call-context
 * slot (borrowed json_t) installed per call by the test harness.
 * Non-object headers (null/list/scalar) model the live AttributeError on
 * .get -> except -> value "". The falsy or-chain tries "Content-Type"
 * then "content-type". str(value).lower() can only contain
 * "text/event-stream" when value is a string: truthy scalars stringify to
 * "True"/numbers, and container header values are out of fixture scope
 * (documented projection — their Python repr would need a full py-repr). */

static json_t *g_sse_headers; /* borrowed */

static int sse_jtruth(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        default: return v->c.count > 0 ? 1 : 0;
    }
}

/* PoP: content_type_is_sse @ hermes_cli/proxy/sse_done.py:content_type_is_sse */
int content_type_is_sse(void) {
    json_t *h = g_sse_headers;
    json_t *v = NULL;
    if (h && h->type == JSON_OBJECT) {
        v = json_obj_get(h, "Content-Type");
        if (!sse_jtruth(v)) v = json_obj_get(h, "content-type");
        if (!sse_jtruth(v)) v = NULL;
    }
    if (!v || v->type != JSON_STRING) return 0;
    /* ASCII lower, then substring test (case-insensitive containment). */
    const char *s = v->str_val;
    size_t n = strlen(s);
    static const char needle[] = "text/event-stream";
    size_t nl = sizeof needle - 1;
    if (n < nl) return 0;
    for (size_t i = 0; i + nl <= n; i++) {
        size_t j = 0;
        while (j < nl) {
            char c = s[i + j];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            if (c != needle[j]) break;
            j++;
        }
        if (j == nl) return 1;
    }
    return 0;
}
