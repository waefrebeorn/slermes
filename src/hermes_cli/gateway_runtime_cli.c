/*
 * gateway_runtime_cli.c — faithful C11 port of hermes_cli/gateway_runtime_cli.py
 * (gate w18_gatewayrun_844e6c).
 *
 * PoP: w18_gatewayrun_844e6c_ensure_exit_code @ hermes_cli/gateway_runtime_cli.py:ensure_exit_code
 * PoP: w18_gatewayrun_844e6c_verdict_payload @ hermes_cli/gateway_runtime_cli.py:_verdict_payload
 * PoP: w18_gatewayrun_844e6c_invalid_invocation @ hermes_cli/gateway_runtime_cli.py:INVALID_INVOCATION
 */
#include "noc_w18_gatewayrun_844e6c.h"

#include <stdlib.h>
#include <string.h>

static int is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }

static int in_auth_set(const char *rc) {
    static const char *const codes[] = {
        "authorization", "profile_mismatch", "gateway_elevated", "gateway_other_session"
    };
    size_t i;
    if (!rc) return 0;
    for (i = 0; i < 4; i++) if (!strcmp(rc, codes[i])) return 1;
    return 0;
}

json_t *w18_gatewayrun_844e6c_ensure_exit_code(const char *reason_code, const char *state) {
    int code;
    if (reason_code && !strcmp(reason_code, "deadline")) return json_number(5);
    if (in_auth_set(reason_code)) return json_number(4);
    if (!state) return json_number(0);
    if (!strcmp(state, "ready")) code = 0;
    else if (!strcmp(state, "incompatible")) code = 3;
    else if (!strcmp(state, "draining")) code = 6;
    else if (!strcmp(state, "inaccessible")) code = 7;
    else if (!strcmp(state, "conflict")) code = 7;
    else if (!strcmp(state, "starting")) code = 5;
    else if (!strcmp(state, "absent")) code = 5;
    else code = 0; /* live dict has no other key; fixtures stay in-domain */
    return json_number((double)code);
}

static int cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    return strcmp(x, y);
}

/* sorted(list(endpoint.capabilities)) without holding an interior pointer
 * across the json_set that frees the old array. */
static void sort_capabilities(json_t *endpoint) {
    json_t *caps = json_obj_get(endpoint, "capabilities");
    size_t n, i;
    char **buf;
    json_t *fresh;
    if (!caps || caps->type != JSON_ARRAY) return;
    n = json_len(caps);
    if (n < 2) return;
    buf = (char **)calloc(n, sizeof(char *));
    if (!buf) return;
    for (i = 0; i < n; i++) {
        const json_t *item = json_get(caps, i);
        const char *s = item && item->type == JSON_STRING ? item->str_val : "";
        buf[i] = (char *)malloc(strlen(s) + 1);
        if (buf[i]) strcpy(buf[i], s);
    }
    qsort(buf, n, sizeof(char *), cmp_str);
    fresh = json_array();
    for (i = 0; i < n; i++) {
        if (buf[i]) { json_append(fresh, json_string(buf[i])); free(buf[i]); }
    }
    free(buf);
    json_set(endpoint, "capabilities", fresh);
}

json_t *w18_gatewayrun_844e6c_verdict_payload(const json_t *result, const char *code_sha) {
    json_t *out = is_obj(result) ? json_copy(result) : json_object();
    json_t *endpoint = json_obj_get(out, "endpoint");
    if (is_obj(endpoint) && endpoint->c.count != 0) sort_capabilities(endpoint);
    json_set(out, "client_code_sha", code_sha ? json_string(code_sha) : json_null());
    return out;
}

json_t *w18_gatewayrun_844e6c_invalid_invocation(void) {
    json_t *out = json_object();
    json_set(out, "state", json_string("inaccessible"));
    json_set(out, "reason_code", json_string("invalid_invocation"));
    json_set(out, "endpoint", json_null());
    return out;
}

