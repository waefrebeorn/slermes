/*
 * methods_session.c - C home for upstream Hermes tui_gateway/methods_session.py.
 * PoP: w15_methodsses_0a2164_str_param @ tui_gateway/methods_session.py:_str_param
 * PoP: w15_methodsses_0a2164_int_param @ tui_gateway/methods_session.py:_int_param
 * PoP: w15_methodsses_0a2164_denied_source @ tui_gateway/methods_session.py:_denied_source
 * PoP: w15_methodsses_0a2164_auto_resume_denied_source @ tui_gateway/methods_session.py:_auto_resume_denied_source
 *
 * Faithful ports of the module's small, stateless request-parameter helpers. They
 * are the shared plumbing every session RPC uses; the handlers themselves own live
 * DB/agent/transport state and stay with the live Python owner. 64-bit values cross
 * the JSON boundary as decimal STRINGS (numbers are double in this JSON lib).
 */
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_methodsses_0a2164.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static int is_falsy(const json_t *v) {
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

/* Python str() for JSON scalars (the helpers only feed it scalars in practice). */
static char *py_str(const json_t *v) {
    if (is_falsy(v)) return dupstr("");
    char buf[64];
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val);
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupstr(buf);
        default: return dupstr("");
    }
}

/* ASCII whitespace strip in place; returns the ORIGINAL allocation start (safe
 * to free). NUL is written at the new length. */
static char *strip(char *s) {
    if (!s) return s;
    size_t n = strlen(s), lead = 0;
    while (lead < n && isspace((unsigned char)s[lead])) lead++;
    if (lead) memmove(s, s + lead, n - lead + 1);
    n -= lead;
    while (n && isspace((unsigned char)s[n - 1])) n--;
    s[n] = '\0';
    return s;
}

static void lower_ascii(char *s) {
    for (; s && *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

struct json_t *w15_methodsses_0a2164_str_param(const struct json_t *params,
                                                const char *key, const char *def) {
    char *s = strip(py_str(json_obj_get(params, key)));
    json_t *out = json_string(s && *s ? s : (def ? def : ""));
    free(s);
    return out;
}

struct json_t *w15_methodsses_0a2164_int_param(const struct json_t *params,
                                                const char *key, long long def) {
    json_t *v = json_obj_get(params, key);
    if (!v) return json_number((double)def);            /* int(params[key, default]) */
    long long r = def;
    if (v->type == JSON_NUMBER) {
        r = (long long)v->num_val;                      /* int() truncates toward zero */
    } else if (v->type == JSON_BOOL) {
        r = v->bool_val ? 1 : 0;
    } else if (v->type == JSON_STRING) {
        char *s = strip(dupstr(v->str_val));
        if (s && *s) {
            errno = 0;
            char *end = NULL;
            long long parsed = strtoll(s, &end, 10);
            if (errno == 0 && end && *end == '\0') r = parsed;
        }
        free(s);
    }
    return json_number((double)r);                      /* TypeError/ValueError -> default */
}

struct json_t *w15_methodsses_0a2164_denied_source(const struct json_t *row) {
    char *s = strip(dupstr(json_get_str(row, "source", "")));
    lower_ascii(s);
    int denied = s && (!strcmp(s, "kanban") || !strcmp(s, "tool") || !strcmp(s, "oneshot"));
    free(s);
    return json_bool(denied ? 1 : 0);
}

struct json_t *w15_methodsses_0a2164_auto_resume_denied_source(const struct json_t *row) {
    char *s = strip(dupstr(json_get_str(row, "source", "")));
    lower_ascii(s);
    int denied = s && (!strcmp(s, "kanban") || !strcmp(s, "tool") ||
                       !strcmp(s, "oneshot") || !strcmp(s, "unknown"));
    free(s);
    return json_bool(denied ? 1 : 0);
}

