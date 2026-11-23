/*
 * session_workdir.c - C home for upstream Hermes tui_gateway/session_workdir.py.
 * PoP: w15_sessionwor_7524ea_is_container_path @ tui_gateway/session_workdir.py:_is_container_path
 * PoP: w15_sessionwor_7524ea_workdir_valid_generation @ tui_gateway/session_workdir.py:_workdir_valid_generation
 * PoP: w15_sessionwor_7524ea_policy_backend @ tui_gateway/session_workdir.py:_policy_backend
 * PoP: w15_sessionwor_7524ea_session_source @ tui_gateway/session_workdir.py:_session_source
 *
 * Faithful ports of the pure path/backend predicates. Filesystem, config and DB
 * state stay with the live Python owner; 64-bit values cross the boundary as
 * decimal strings (this JSON library stores numbers as double).
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_sessionwor_7524ea.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

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

static char *py_str(const json_t *v) {
    char buf[64];
    if (falsy(v)) return dupstr("");
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

/* In-place ASCII strip + lowercase; returns the original allocation start. */
static char *strip_lower(char *s) {
    if (!s) return s;
    size_t n = strlen(s), lead = 0;
    while (lead < n && isspace((unsigned char)s[lead])) lead++;
    if (lead) memmove(s, s + lead, n - lead + 1);
    n -= lead;
    while (n && isspace((unsigned char)s[n - 1])) n--;
    s[n] = '\0';
    for (size_t i = 0; i < n; i++)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
    return s;
}

static char *strip_only(char *s) {
    if (!s) return s;
    size_t n = strlen(s), lead = 0;
    while (lead < n && isspace((unsigned char)s[lead])) lead++;
    if (lead) memmove(s, s + lead, n - lead + 1);
    n -= lead;
    while (n && isspace((unsigned char)s[n - 1])) n--;
    s[n] = '\0';
    return s;
}

struct json_t *w15_sessionwor_7524ea_is_container_path(const char *raw) {
    int ok = raw && raw[0] == '/' && raw[1] != '/';
    return json_bool(ok ? 1 : 0);
}

struct json_t *w15_sessionwor_7524ea_workdir_valid_generation(const struct json_t *generation) {
    int ok = generation && generation->type == JSON_NUMBER &&
             generation->num_val >= 1.0 &&
             generation->num_val == (double)(long long)generation->num_val;
    return json_bool(ok ? 1 : 0);
}

struct json_t *w15_sessionwor_7524ea_policy_backend(const struct json_t *policy) {
    char *s = strip_lower(py_str(json_obj_get(policy, "TERMINAL_ENV")));
    json_t *out = json_string(s && *s ? s : "local");
    free(s);
    return out;
}

struct json_t *w15_sessionwor_7524ea_session_source(const struct json_t *session,
                                                     const char *platform_fallback) {
    char *s = session ? strip_only(py_str(json_obj_get(session, "source"))) : dupstr("");
    json_t *out = json_string(s && *s ? s : (platform_fallback ? platform_fallback : ""));
    free(s);
    return out;
}

