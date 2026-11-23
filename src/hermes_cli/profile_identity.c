/*
 * profile_identity.c - C home for upstream Hermes hermes_cli/profile_identity.py.
 * PoP: w15_profileide_89a518_control_answer_failure @ hermes_cli/profile_identity.py:_control_answer_failure
 * PoP: w15_profileide_89a518_store_error_text @ hermes_cli/profile_identity.py:_store_error_text
 * PoP: w15_profileide_89a518_gateway_accepts_verb @ hermes_cli/profile_identity.py:_gateway_accepts_profile_identity_verb
 *
 * Pure control-socket answer / store-error helpers. The control-socket answer and
 * the exception class are injected as JSON inputs, so no gateway or DB is touched.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_profileide_89a518.h"

static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    return v->c.count > 0;
}

/* Python str() for a scalar / minimal repr for containers. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%g", v->num_val);
        return dupstr(b);
    }
    char *s = json_dumps(v, 0); /* arrays/objects: JSON-ish fallback for str() */
    return s ? s : dupstr("");
}

static char *quote_single(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 2 + 3);
    if (!out) return dupstr("''");
    size_t m = 0;
    out[m++] = '\'';
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\\' || c == '\'') out[m++] = '\\';
        out[m++] = c;
    }
    out[m++] = '\'';
    out[m] = 0;
    return out;
}

/* repr() of a JSON value the way Python prints a control-socket answer. */
static char *py_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%g", v->num_val);
        return dupstr(b);
    }
    if (v->type == JSON_STRING) return quote_single(v->str_val ? v->str_val : "");
    size_t cap = 256, m = 0;
    char *out = (char *)malloc(cap);
    if (!out) return dupstr("");
    out[m++] = v->type == JSON_ARRAY ? '[' : '{';
    size_t n = v->c.count;
    for (size_t i = 0; i < n; i++) {
        if (i) {
            if (m + 2 >= cap) break;
            out[m++] = ','; out[m++] = ' ';
        }
        char *piece = NULL;
        if (v->type == JSON_ARRAY) {
            piece = py_repr(json_get(v, i));
        } else if (v->c.keys && v->c.items) {
            char *k = quote_single(v->c.keys[i] ? v->c.keys[i] : "");
            char *val = py_repr(v->c.items[i]);
            size_t kp = strlen(k), vp = strlen(val);
            piece = (char *)malloc(kp + vp + 4);
            if (piece) {
                size_t q = 0;
                memcpy(piece + q, k, kp); q += kp;
                piece[q++] = ':'; piece[q++] = ' ';
                memcpy(piece + q, val, vp); q += vp;
                piece[q] = 0;
            }
            free(k); free(val);
        }
        if (piece) {
            size_t pl = strlen(piece);
            if (m + pl + 2 > cap) {
                size_t need = m + pl + 4;
                char *tmp = (char *)realloc(out, need);
                if (!tmp) { free(piece); break; }
                out = tmp; cap = need;
            }
            memcpy(out + m, piece, pl); m += pl;
            free(piece);
        }
    }
    if (m + 2 >= cap) { char *tmp = (char *)realloc(out, m + 4); if (tmp) { out = tmp; cap = m + 4; } }
    out[m++] = v->type == JSON_ARRAY ? ']' : '}';
    out[m] = 0;
    return out;
}

struct json_t *w15_profileide_89a518_control_answer_failure(const struct json_t *answer) {
    if (!answer || answer->type == JSON_NULL)
        return json_string("no response from gateway control socket");
    if (answer->type == JSON_OBJECT) {
        const char *keys[] = {"error", "message", "detail", NULL};
        for (int i = 0; keys[i]; i++) {
            json_t *f = json_obj_get(answer, keys[i]);
            if (py_truthy(f)) {
                char *s = py_str(f);
                json_t *r = json_string(s);
                free(s);
                return r;
            }
        }
        char *r = py_repr(answer);
        json_t *out = json_string(r);
        free(r);
        return out;
    }
    char *r = py_repr(answer);
    json_t *out = json_string(r);
    free(r);
    return out;
}

struct json_t *w15_profileide_89a518_store_error_text(const char *class_name, const char *message,
                                                      int collision) {
    const char *name = collision ? "ValueError" : (class_name ? class_name : "Exception");
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s: %s", name, message ? message : "");
    return json_string(buf);
}

int w15_profileide_89a518_gateway_accepts_verb(int identified, int raises) {
    if (raises) return 0;
    return identified ? 1 : 0;
}

