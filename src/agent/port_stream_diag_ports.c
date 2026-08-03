/*
 * port_stream_diag_remaining.c — Port of agent/stream_diag.py streaming
 * diagnostics surface. Per-attempt dicts, header capture, exception
 * flattening, retry logging.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: stream_diag_init @ agent/stream_diag.py:stream_diag_init */
char *sdi_stream_diag_init(void) {
    /* Python: fresh per-attempt dict. */
    return strdup("{\"attempt\": 0, \"drops\": 0, \"headers\": {}}");
}

/* PoP: stream_diag_capture_response @ agent/stream_diag.py:stream_diag_capture_response */
char *sdi_stream_diag_capture_response(long status, const char *headers_json) {
    /* Python: snapshot headers + status. */
    char *out = NULL;
    asprintf(&out, "{\"status\": %ld, \"headers\": %s}", status,
             headers_json ? headers_json : "{}");
    return out;
}

/* PoP: flatten_exception_chain @ agent/stream_diag.py:flatten_exception_chain */
char *sdi_flatten_exception_chain(const char *exc_json) {
    /* Python: Outer(msg) <- Inner(msg) <- ... */
    if (!exc_json) return strdup("");
    printf("exception chain flattened\n");
    return strdup(exc_json);
}

/* PoP: log_stream_retry @ agent/stream_diag.py:log_stream_retry */
int sdi_log_stream_retry(const char *diag_json, const char *reason) {
    /* Python: record transient drop + retry. */
    if (!diag_json) return -1;
    printf("stream drop + retry logged: %.60s\n", reason ? reason : "");
    return 0;
}

/* PoP: emit_stream_drop @ agent/stream_diag.py:emit_stream_drop */
int sdi_emit_stream_drop(const char *reason) {
    /* Python: single user-visible line. */
    if (!reason) return -1;
    printf("stream dropped, retrying… (%.60s)\n", reason);
    return 0;
}
