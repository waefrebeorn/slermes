/*
 * turn_response_check.c — faithful C11 port of agent/turn_response_check.py.
 * Gate w11_turnrespon_df1eeb.
 *
 * PoP: w11_turnrespon_df1eeb_codex_finish_reason @ agent/turn_response_check.py:_codex_finish_reason
 * PoP: w11_turnrespon_df1eeb_derive_finish_reason @ agent/turn_response_check.py:_derive_finish_reason
 */
#include "hermes_json.h"
#include "noc_w11_turnrespon_df1eeb.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_cstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* str(value).strip().lower() for the JSON scalar shapes a reason can take. */
static char *reason_str(const json_t *v) {
    char buf[128];
    const char *s = NULL;
    char *out;
    size_t a, b, n;
    if (!v || v->type == JSON_NULL) return NULL;
    switch (v->type) {
        case JSON_STRING: s = v->str_val ? v->str_val : ""; break;
        case JSON_BOOL:   s = v->bool_val ? "True" : "False"; break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%.0f", v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            s = buf;
            break;
        default: return NULL;
    }
    n = strlen(s);
    if (n >= sizeof(buf)) return NULL;
    {
        size_t i;
        for (i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)s[i]);
        buf[n] = '\0';
    }
    a = 0; b = n;
    while (a < b && isspace((unsigned char)buf[a])) a++;
    while (b > a && isspace((unsigned char)buf[b - 1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    if (b > a) memcpy(out, buf + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* status.strip().lower(), only when the live value is a real str. */
static char *status_str(const json_t *status) {
    char buf[128];
    size_t n, a, b;
    char *out;
    const char *s;
    if (!status || status->type != JSON_STRING || !status->str_val) return NULL;
    s = status->str_val;
    n = strlen(s);
    if (n >= sizeof(buf)) return NULL;
    memcpy(buf, s, n); buf[n] = '\0';
    {
        size_t i;
        for (i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    }
    a = 0; b = n;
    while (a < b && isspace((unsigned char)buf[a])) a++;
    while (b > a && isspace((unsigned char)buf[b - 1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    if (b > a) memcpy(out, buf + a, b - a);
    out[b - a] = '\0';
    return out;
}

char *w11_turnrespon_df1eeb_codex_finish_reason(
    const json_t *status, const json_t *incomplete_details) {
    char *st = status_str(status);
    char *reason = NULL;
    char *ret;
    const json_t *rv = NULL;
    if (incomplete_details && incomplete_details->type == JSON_OBJECT)
        rv = json_obj_get(incomplete_details, "reason");
    reason = reason_str(rv);
    if (st && !strcmp(st, "incomplete") && reason &&
            (!strcmp(reason, "max_output_tokens") || !strcmp(reason, "length")))
        ret = dup_cstr("incomplete");
    else if (st && !strcmp(st, "incomplete") && reason && !strcmp(reason, "content_filter"))
        ret = dup_cstr("content_filter");
    else
        ret = dup_cstr("stop");
    free(st); free(reason);
    return ret;
}

char *w11_turnrespon_df1eeb_derive_finish_reason(
    const char *api_mode, const json_t *status, const json_t *incomplete_details,
    int codex_tool_calls, const char *transport_finish, int treat_stop_truncated) {
    if (api_mode && !strcmp(api_mode, "codex_responses")) {
        char *fr = w11_turnrespon_df1eeb_codex_finish_reason(status, incomplete_details);
        if (!strcmp(fr, "incomplete") && codex_tool_calls) {
            free(fr);
            return dup_cstr("length");
        }
        return fr;
    }
    if (api_mode && !strcmp(api_mode, "anthropic_messages"))
        return dup_cstr(transport_finish ? transport_finish : "");
    if (api_mode && !strcmp(api_mode, "bedrock_converse"))
        return dup_cstr(transport_finish ? transport_finish : "");
    if (treat_stop_truncated) return dup_cstr("length");
    return dup_cstr(transport_finish ? transport_finish : "");
}

