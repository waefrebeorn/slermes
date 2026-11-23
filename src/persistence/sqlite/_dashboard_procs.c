/*
 * _dashboard_procs.c — faithful C11 port of upstream Hermes
 * persistence/sqlite/_dashboard_procs.py (gate dashproc).
 * PoP: dashproc_dashboard_subcommand_index @ persistence/sqlite/_dashboard_procs.py:_dashboard_subcommand_index
 * PoP: dashproc_is_ephemeral_port_zero_backend @ persistence/sqlite/_dashboard_procs.py:_is_ephemeral_port_zero_backend
 */
#include "noc_dashproc.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

static int dp_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static int dp_tok_is(const json_t *t, const char *s) {
    return t && t->type == JSON_STRING && t->str_val && !strcmp(t->str_val, s);
}

/* Python str(token) for the token shapes that can equal "0" (and a few others). */
static const char *dp_tok_str(const json_t *t, char *buf, size_t n) {
    if (!t) return NULL;
    if (t->type == JSON_STRING) return t->str_val;
    if (t->type == JSON_NUMBER) {
        if (t->num_val == 0.0) { snprintf(buf, n, "0"); return buf; } /* Python int 0 */
        snprintf(buf, n, "%.17g", t->num_val);
        return buf;
    }
    if (t->type == JSON_BOOL) return t->bool_val ? "True" : "False";
    return NULL;
}

/* Python str.strip() -> ASCII whitespace (fixtures are ASCII). */
static const char *dp_strip(const char *s, char *buf, size_t n) {
    const char *e;
    size_t len;
    if (!s) return NULL;
    while (*s && dp_is_ws((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && dp_is_ws((unsigned char)*(e - 1))) e--;
    len = (size_t)(e - s);
    if (len >= n) len = n - 1;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

int dashproc_dashboard_subcommand_index(const struct json_t *argv) {
    size_t i, n;
    if (!argv || argv->type != JSON_ARRAY) return -1;
    n = json_len(argv);
    for (i = 0; i < n; i++) {
        json_t *t = json_get(argv, i);
        if (dp_tok_is(t, "serve") || dp_tok_is(t, "dashboard")) return (int)i;
    }
    return -1;
}

int dashproc_is_ephemeral_port_zero_backend(const struct json_t *argv) {
    size_t i, n;
    char vbuf[64], sbuf[4096];
    if (dashproc_dashboard_subcommand_index(argv) < 0) return 0;
    if (!argv || argv->type != JSON_ARRAY) return 0;
    n = json_len(argv);
    for (i = 0; i < n; i++) {
        json_t *t = json_get(argv, i);
        if (dp_tok_is(t, "--port")) {
            if (i + 1 < n) {
                const char *v = dp_tok_str(json_get(argv, i + 1), vbuf, sizeof(vbuf));
                if (v && !strcmp(v, "0")) return 1;
            }
        }
        if (t && t->type == JSON_STRING && t->str_val &&
            !strncmp(t->str_val, "--port=", 7)) {
            const char *tail = dp_strip(t->str_val + 7, sbuf, sizeof(sbuf));
            if (tail && !strcmp(tail, "0")) return 1;
        }
    }
    return 0;
}
