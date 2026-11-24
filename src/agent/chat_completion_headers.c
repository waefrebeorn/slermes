/*
 * chat_completion_headers.c — real C11 port of selected behavior of upstream
 * Hermes agent/chat_completion_headers.py.
 *
 * PoP: w17_chatcomple_3345ab_capture_headers @ agent/chat_completion_headers.py:_chat_completion_capturing_headers
 */
#include "noc_w17_chatcomple_3345ab.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

json_t *w17_chatcomple_3345ab_capture_headers(
    const char *provider, int has_state, json_t *is_usage_window,
    int has_raw_response, int response_is_window,
    const char *plain_result, const char *parse_result)
{
    const char *path;
    int plain_path, raw_created = 0, captures = 0;
    char *result;
    json_t *out;

    /* `is not True`: only a genuine boolean True passes. An int 1, a string,
     * null or a missing state all take the plain branch. */
    int is_true = (is_usage_window && is_usage_window->type == JSON_BOOL &&
                   is_usage_window->bool_val) ? 1 : 0;

    plain_path = (!provider || strcmp(provider, "nous") != 0 ||
                  !has_state || !is_true || !has_raw_response) ? 1 : 0;

    if (plain_path) {
        path = "plain";
        result = dupstr(plain_result);
    } else {
        raw_created = 1;
        result = dupstr(parse_result);
        if (!response_is_window) {
            path = "parse_only";
        } else {
            /* All four capture callbacks are attempted; each is wrapped in
             * its own try/except upstream, so even failures are swallowed and
             * every callback still runs. */
            path = "captured";
            captures = 4;
        }
    }

    out = json_object();
    json_set(out, "path", json_string(path));
    json_set(out, "raw_created", json_bool(raw_created ? 1 : 0));
    json_set(out, "captures", json_number((double)captures));
    json_set(out, "result", json_string(result ? result : ""));
    free(result);
    return out;
}
