/*
 * session_terminal_cwd.c — C port of upstream Hermes gateway/session_terminal_cwd.py.
 *
 * PoP: w16_sessionter_e62531_local_backend_ex @ gateway/session_terminal_cwd.py:_local_backend
 * PoP: w16_sessionter_e62531_should_persist @ gateway/session_terminal_cwd.py:persist_turn_cwd
 *
 * The live _local_backend reads json.loads(policy.terminal_json).get("TERMINAL_ENV")
 * for a frozen local policy, else the profile's live _tenv("TERMINAL_ENV","local").
 * The latter is an injected seam (env_value) because the Windows CRT cannot keep
 * os.environ[name]="" distinct from an unset variable.
 */
#include "noc_w16_sessionter_e62531.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

/* Python falsiness of a JSON value. */
static int py_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    switch (v->type) {
        case JSON_BOOL:   return !v->bool_val;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || !v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count == 0;
        default: return 1;
    }
}

/* Python str() for the fixture domain (strings, bools, integral numbers).
 * Non-empty containers can never str() to "local" so they map to a sentinel. */
static char *py_str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return dupstr(buf);
    }
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    return dupstr("notlocal");
}

/* Python .strip().lower() for ASCII whitespace (fixture domain). */
static char *strip_lower(const char *s) {
    const char *a = s ? s : "";
    const char *b = a + strlen(a);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b-1))) b--;
    char *r = (char *)malloc((size_t)(b - a) + 1);
    size_t i = 0;
    for (const char *p = a; p < b; p++) r[i++] = (char)tolower((unsigned char)*p);
    r[i] = '\0';
    return r;
}

int w16_sessionter_e62531_local_backend_ex(const char *policy_json, const char *env_value) {
    char *backend = NULL;
    if (policy_json && policy_json[0]) {
        char *err = NULL;
        json_t *doc = json_parse(policy_json, &err);
        if (err) free(err);
        if (doc) {
            json_t *b = json_obj_get(doc, "TERMINAL_ENV");
            if (py_falsy(b)) backend = dupstr("local");
            else backend = py_str_of(b);   /* copy out BEFORE json_free(doc) */
            json_free(doc);
        } else {
            backend = dupstr("local");
        }
    } else {
        /* None policy: _tenv("TERMINAL_ENV", "local") -> injected env_value. */
        backend = dupstr((env_value && env_value[0]) ? env_value : "local");
    }
    char *norm = strip_lower(backend);
    int result = (strcmp(norm, "local") == 0);
    free(norm);
    free(backend);
    return result;
}

int w16_sessionter_e62531_should_persist(const char *saved, const char *cwd) {
    if (!cwd || !cwd[0]) return 0;        /* `not cwd` */
    if (!saved) return 1;                 /* None != cwd */
    return strcmp(saved, cwd) != 0;
}
