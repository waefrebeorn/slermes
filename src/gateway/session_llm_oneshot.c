/*
 * session_llm_oneshot.c — real C11 port of upstream Hermes gateway/session_llm_oneshot.py.
 * PoP: w14_sessionllm_2068e9_validate @ gateway/session_llm_oneshot.py:_validate
 * Seam-scoped: only the deterministic parameter validation is ported. The
 * thread pool, authority, transport and model-call surfaces are excluded; the
 * PROMPT_TEMPLATES registry is injected by the caller as a fixture seam.
 */
#include "hermes_json.h"
#include "noc_w14_sessionllm_2068e9.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n; char *p;
    if (!s) return NULL;
    n = strlen(s) + 1; p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return v->c.count ? 1 : 0;
        case JSON_OBJECT: return v->c.count ? 1 : 0;
        default: return 0;
    }
}

/* Byte length of a Python str.strip whitespace code point at s, else 0. */
static size_t ws_len_at(const unsigned char *s) {
    unsigned char a = s[0];
    if (!a) return 0;
    if (a == ' ' || a == '\t' || a == '\n' || a == '\r' || a == '\v' || a == '\f') return 1;
    if (a == 0xC2 && s[1] == 0xA0) return 2;                 /* U+00A0 */
    if (!s[1] || !s[2]) return 0;
    if (a == 0xE1 && s[1] == 0x9A && s[2] == 0x80) return 3; /* U+1680 */
    if (a == 0xE2 && s[1] == 0x80 && ((s[2] >= 0x80 && s[2] <= 0x8A) ||
        s[2] == 0xA8 || s[2] == 0xA9 || s[2] == 0xAF)) return 3;
    if (a == 0xE2 && s[1] == 0x81 && s[2] == 0x9F) return 3; /* U+205F */
    if (a == 0xE3 && s[1] == 0x80 && s[2] == 0x80) return 3; /* U+3000 */
    if (a == 0xEF && s[1] == 0xBB && s[2] == 0xBF) return 3; /* U+FEFF */
    return 0;
}

static char *py_strip(const char *src) {
    const unsigned char *s = (const unsigned char *)(src ? src : "");
    size_t n = strlen((const char *)s), b = 0, e = n, p, last_non = 0;
    char *out;
    while (b < e) {
        size_t w = ws_len_at(s + b);
        if (!w || b + w > e) break;
        b += w;
    }
    last_non = b;
    p = b;
    while (p < e) {
        size_t w = ws_len_at(s + p);
        if (w && p + w <= e) { p += w; }
        else { last_non = p + 1; p += 1; }
    }
    e = last_non;
    out = (char *)malloc(e - b + 1);
    if (!out) return NULL;
    if (e > b) memcpy(out, s + b, e - b);
    out[e - b] = 0;
    return out;
}

static char *get_str_or_empty(const json_t *o, const char *key) {
    const json_t *v = json_obj_get(o, key);
    if (v && v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    return dupstr("");
}

static struct json_t *refusal(struct json_t *out, const char *field, const char *detail) {
    json_set(out, "ok", json_bool(0));
    json_set(out, "reason", json_string("invalid_params"));
    json_set(out, "field", field ? json_string(field) : json_null());
    json_set(out, "detail", detail ? json_string(detail) : json_null());
    return out;
}

struct json_t *w14_sessionllm_2068e9_validate(const struct json_t *params,
                                              const struct json_t *templates) {
    static const char *FIELDS[] = {"session_id", "profile", "template", "instructions",
                                   "input", "variables", "task", "temperature", "max_tokens", NULL};
    static const char *TEXT[] = {"template", "instructions", "input", "task", NULL};
    struct json_t *out = json_object();
    int bad = 0;
    size_t i;
    int j;

    if (!params || params->type != JSON_OBJECT) return refusal(out, NULL, NULL);

    /* set(params) - _FIELDS */
    for (i = 0; i < json_object_size(params); i++) {
        const char *k = json_object_get_key_at(params, i);
        int known = 0;
        for (j = 0; FIELDS[j]; j++) if (k && !strcmp(k, FIELDS[j])) { known = 1; break; }
        if (!known) { bad = 1; break; }
    }
    /* text fields + session_id: non-null must be a string */
    if (!bad) for (j = 0; TEXT[j]; j++) {
        const json_t *v = json_obj_get(params, TEXT[j]);
        if (v && v->type != JSON_NULL && v->type != JSON_STRING) { bad = 1; break; }
    }
    if (!bad) {
        const json_t *v = json_obj_get(params, "session_id");
        if (v && v->type != JSON_NULL && v->type != JSON_STRING) bad = 1;
    }
    /* variables: `params.get('variables') or {}` must be a dict */
    if (!bad) {
        const json_t *v = json_obj_get(params, "variables");
        if (v && py_truthy(v) && v->type != JSON_OBJECT) bad = 1;
    }
    /* temperature / max_tokens: non-null must be a number and not a bool */
    if (!bad) {
        static const char *NUM[] = {"temperature", "max_tokens"};
        for (j = 0; j < 2; j++) {
            const json_t *v = json_obj_get(params, NUM[j]);
            if (v && v->type != JSON_NULL && (v->type == JSON_BOOL || v->type != JSON_NUMBER)) {
                bad = 1; break;
            }
        }
    }
    if (bad) return refusal(out, NULL, NULL);

    {
        char *tmpl = get_str_or_empty(params, "template");
        char *tmpl_s = py_strip(tmpl);
        free(tmpl);
        if (tmpl_s && tmpl_s[0]) {
            int found = 0;
            if (templates && templates->type == JSON_ARRAY) {
                for (i = 0; i < json_len(templates); i++) {
                    const json_t *t = json_get(templates, i);
                    if (t && t->type == JSON_STRING && !strcmp(t->str_val, tmpl_s)) { found = 1; break; }
                }
            }
            free(tmpl_s);
            if (!found) return refusal(out, "template", "unknown one-shot template");
        } else {
            char *ins, *inp, *ins_s, *inp_s;
            int have;
            free(tmpl_s);
            ins = get_str_or_empty(params, "instructions");
            ins_s = py_strip(ins); free(ins);
            inp = get_str_or_empty(params, "input");
            inp_s = py_strip(inp); free(inp);
            have = (ins_s && ins_s[0]) || (inp_s && inp_s[0]);
            free(ins_s); free(inp_s);
            if (!have) return refusal(out, "input", "needs a template or instructions/input");
        }
    }
    json_set(out, "ok", json_bool(1));
    return out;
}
