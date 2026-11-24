/*
 * run_turn_persistence.c - C home for upstream Hermes gateway/run_turn_persistence.py.
 * PoP: w16_runturnpe_0c61cf_add_failed_turn_notice @ gateway/run_turn_persistence.py:GatewayTurnPersistenceMixin._hmwa_add_failed_turn_notice
 *
 * Faithful port of the pure decision helper (str coercion + str.strip + the
 * two-way notice join). Async turn/IO machinery stays with the live Python owner.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_runturnpe_0c61cf.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

/* Python truthiness for the JSON values crossing this boundary. */
static int falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    switch (v->type) {
        case JSON_BOOL:   return !v->bool_val;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || !v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count == 0;
        default:          return 1;
    }
}

/* Python str(v) for the scalar types the fixtures exercise. */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dupstr("None");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupstr(buf);
        default:          return dupstr("");
    }
}

/* str.strip() for ASCII whitespace; returns the ORIGINAL allocation start. */
static char *strip_ws(char *s) {
    if (!s) return s;
    size_t n = strlen(s), lead = 0;
    while (lead < n && isspace((unsigned char)s[lead])) lead++;
    if (lead) memmove(s, s + lead, n - lead + 1);
    n -= lead;
    while (n && isspace((unsigned char)s[n - 1])) n--;
    s[n] = '\0';
    return s;
}

struct json_t *w16_runturnpe_0c61cf_add_failed_turn_notice(const struct json_t *response,
                                                           const struct json_t *notice) {
    /* response = str(response or "").strip() */
    char *r = falsy(response) ? dupstr("") : py_str(response);
    char *rs = strip_ws(r);
    json_t *out;
    if (rs && rs[0]) {
        char *n = py_str(notice);
        size_t need = strlen(rs) + 2 + strlen(n) + 1;
        char *buf = (char *)malloc(need);
        if (buf) snprintf(buf, need, "%s\n\n%s", rs, n);
        out = json_string(buf ? buf : "");
        free(buf);
        free(n);
    } else {
        /* Python returns `notice` unchanged; preserve its JSON type. */
        out = notice ? json_copy(notice) : json_null();
    }
    free(r);
    return out;
}
