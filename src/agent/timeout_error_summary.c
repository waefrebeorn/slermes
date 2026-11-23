/*
 * timeout_error_summary.c — C port of upstream Hermes agent/timeout_error_summary.py.
 *
 * PoP: w14_timeouterr_c3731b_timeout_phase @ agent/timeout_error_summary.py:_timeout_phase
 * PoP: w14_timeouterr_c3731b_summarize @ agent/timeout_error_summary.py:summarize_timeout_error
 *
 * The exception chain is represented as a JSON array of {type, cause} nodes (the
 * cause is an index into the same array, or null). That mirrors the Python
 * __cause__ walk, including its id()-based cycle guard, without needing live
 * exception objects in C.
 */
#include "noc_w14_timeouterr_c3731b.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONNECT_HINT \
    "Connection to the provider timed out before it was established " \
    "(check the base_url and network path)"
#define READ_HINT_PREFIX \
    "Request timed out waiting for the provider to respond (if the model needs " \
    "longer, raise providers."
#define READ_HINT_SUFFIX ".request_timeout_seconds in config.yaml)"

static int name_in(const char *name, const char *const *set, size_t n) {
    if (!name) return 0;
    for (size_t i = 0; i < n; i++) if (!strcmp(name, set[i])) return 1;
    return 0;
}

const char *w14_timeouterr_c3731b_timeout_phase(const struct json_t *nodes) {
    static const char *const connect[] = {"ConnectTimeout", "ConnectTimeoutError"};
    static const char *const other[] = {"PoolTimeout", "WriteTimeout"};
    static const char *const read[] = {
        "ReadTimeout", "TimeoutException", "ReadTimeoutError", "APITimeoutError"
    };
    const char *phase = NULL;
    size_t n, i = 0;
    unsigned char *seen;

    if (!nodes || nodes->type != JSON_ARRAY || nodes->c.count == 0) return NULL;
    n = nodes->c.count;
    seen = (unsigned char *)calloc(n ? n : 1, 1);
    if (!seen) return NULL;

    while (i < n && !seen[i]) {
        const struct json_t *node = nodes->c.items[i];
        const struct json_t *cause;
        const char *name;
        size_t next;
        seen[i] = 1;
        name = (node && node->type == JSON_OBJECT) ? json_get_str(node, "type", "") : "";
        if (name_in(name, connect, sizeof(connect) / sizeof(connect[0]))) {
            free(seen);
            return "connect";
        }
        if (name_in(name, other, sizeof(other) / sizeof(other[0]))) {
            free(seen);
            return NULL;
        }
        if (!name_in(name, read, sizeof(read) / sizeof(read[0]))) break;
        phase = "read";
        cause = (node && node->type == JSON_OBJECT) ? json_obj_get(node, "cause") : NULL;
        if (!cause || cause->type != JSON_NUMBER) break;
        next = (size_t)cause->num_val;
        if (next >= n) break;
        i = next;
    }
    free(seen);
    return phase;
}

char *w14_timeouterr_c3731b_summarize(const struct json_t *nodes, const struct json_t *provider) {
    const char *phase = w14_timeouterr_c3731b_timeout_phase(nodes);
    char keybuf[4096];
    const char *key = "<provider>";
    size_t cap, len;
    char *out;

    if (!phase) return NULL;
    if (!strcmp(phase, "connect")) {
        out = (char *)malloc(strlen(CONNECT_HINT) + 1);
        if (out) memcpy(out, CONNECT_HINT, strlen(CONNECT_HINT) + 1);
        return out;
    }
    if (provider && provider->type == JSON_STRING && provider->str_val) {
        const char *s = provider->str_val;
        size_t a = 0, b = strlen(s);
        while (a < b && isspace((unsigned char)s[a])) a++;
        while (b > a && isspace((unsigned char)s[b - 1])) b--;
        if (a < b && b - a < sizeof(keybuf)) {
            memcpy(keybuf, s + a, b - a);
            keybuf[b - a] = '\0';
            key = keybuf;
        }
    }
    cap = strlen(READ_HINT_PREFIX) + strlen(key) + strlen(READ_HINT_SUFFIX) + 1;
    out = (char *)malloc(cap);
    if (!out) return NULL;
    len = 0;
    memcpy(out + len, READ_HINT_PREFIX, strlen(READ_HINT_PREFIX));
    len += strlen(READ_HINT_PREFIX);
    memcpy(out + len, key, strlen(key));
    len += strlen(key);
    memcpy(out + len, READ_HINT_SUFFIX, strlen(READ_HINT_SUFFIX) + 1);
    return out;
}
