/*
 * mcp_tool_registration.c - C home for upstream Hermes tools/mcp_tool_registration.py.
 * PoP: w15_mcptoolre_b8eaf7_normalize_trust @ tools/mcp_tool_registration.py:_normalize_server_trust
 * PoP: w15_mcptoolre_b8eaf7_auth_type @ tools/mcp_tool_registration.py:_auth_type
 *
 * Pure config-classification helpers from MCP tool registration. Trust/auth values
 * are injected as JSON; only the documented None/strip/lower/fail-closed rules live
 * here. No registry, transport or lock is touched.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_mcptoolre_b8eaf7.h"

static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }
static char *fmti(long long v) { char b[48]; snprintf(b, sizeof(b), "%lld", v); return dupstr(b); }
static char *fmtg(double v) { char b[64]; snprintf(b, sizeof(b), "%g", v); return dupstr(b); }

static int jtruthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL)   return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return v->num_val == (double)(long long)v->num_val ? fmti((long long)v->num_val) : fmtg(v->num_val);
    char *s = json_dumps(v, 0); return s ? s : dupstr("");
}
/* str(...).strip().lower() for the ASCII config values we classify. */
static char *strip_lower(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = 0;
    return out;
}

struct json_t *w15_mcptoolre_b8eaf7_normalize_trust(const struct json_t *value) {
    if (!value || value->type == JSON_NULL) return json_string("full");
    char *s = py_str(value);
    char *low = strip_lower(s);
    const char *result = (!strcmp(low, "full") || !strcmp(low, "untrusted")) ? low : "untrusted";
    json_t *out = json_string(result);
    free(low); free(s);
    return out;
}

struct json_t *w15_mcptoolre_b8eaf7_auth_type(const struct json_t *config) {
    const json_t *auth = (config && config->type == JSON_OBJECT) ? json_obj_get(config, "auth") : NULL;
    if (!jtruthy(auth)) return json_string("");
    if (auth->type != JSON_STRING) {
        /* Python calls `.lower()` on the truthy value; non-strings raise AttributeError. */
        const char *t = auth->type == JSON_BOOL ? "bool"
                      : auth->type == JSON_NUMBER ? (auth->num_val == (double)(long long)auth->num_val ? "int" : "float")
                      : auth->type == JSON_ARRAY ? "list"
                      : auth->type == JSON_OBJECT ? "dict" : "object";
        size_t n = strlen(t) + 64;
        char *msg = (char *)malloc(n);
        snprintf(msg, n, "'%s' object has no attribute 'lower'", t);
        json_t *e = json_object();
        json_set(e, "__raise__", json_string("AttributeError"));
        json_set(e, "msg", json_string(msg));
        free(msg);
        return e;
    }
    char *s = py_str(auth);
    char *low = strip_lower(s);
    json_t *out = json_string(low);
    free(low); free(s);
    return out;
}

