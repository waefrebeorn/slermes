/*
 * auxiliary_client_registry.c -- real C11 port of the auth_type dispatch table
 * and the minimax-oauth arm of agent/auxiliary_client_registry.py.
 *
 * PoP: w21_auxiliaryc_fb830d_registry_arms @ agent/auxiliary_client_registry.py:REGISTRY_AUTHTYPE_ARMS
 * PoP: w21_auxiliaryc_fb830d_arm_for @ agent/auxiliary_client_registry.py:REGISTRY_AUTHTYPE_ARMS
 * PoP: w21_auxiliaryc_fb830d_log_once @ agent/auxiliary_client_registry.py:_log_once_debug
 * PoP: w21_auxiliaryc_fb830d_minimax_resolve @ agent/auxiliary_client_registry.py:_resolve_minimax_oauth_arm
 *
 * Faithful details:
 *  - the table is exactly the five registry auth types; api_key and
 *    external_process stay on the facade and are NOT arms here;
 *  - prologue of the arm: `str(base_url or "").strip().rstrip("/")`, so a
 *    whitespace- or slash-padded endpoint resolves to the same client;
 *  - probe mode never refreshes: a missing access_token OR an empty base_url
 *    ends it with a debug-once "not logged in";
 *  - non-probe failures (AuthError, anything else, incomplete credentials, the
 *    SDK-absent ImportError) all end as (None, None) -- never an exception;
 *  - the model ladder is `req.model or aux model or "MiniMax-M3"`.
 */
#include "noc_w21_auxiliaryc_fb830d.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AC_PLACEHOLDER_MODEL "MiniMax-M3"

static char *ac_dup_n(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n && s) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *ac_dup(const char *s) {
    if (!s) s = "";
    return ac_dup_n(s, strlen(s));
}

static int ac_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} ac_sb;

static int ac_sb_grow(ac_sb *b, size_t extra) {
    size_t want = b->n + extra + 1;
    char *np;
    if (want <= b->cap) return 1;
    if (want < 32) want = 32;
    np = (char *)realloc(b->p, want);
    if (!np) return 0;
    b->p = np;
    b->cap = want;
    return 1;
}

static int ac_sb_raw(ac_sb *b, const char *s, size_t len) {
    if (!ac_sb_grow(b, len)) return 0;
    if (len) memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
    return 1;
}

static int ac_sb_puts(ac_sb *b, const char *s) {
    return ac_sb_raw(b, s ? s : "", s ? strlen(s) : 0);
}

static int ac_sb_putc(ac_sb *b, char c) {
    return ac_sb_raw(b, &c, 1);
}

static char *ac_sb_take(ac_sb *b) {
    char *p;
    if (!ac_sb_grow(b, 0)) {
        free(b->p);
        b->p = NULL;
        b->n = b->cap = 0;
        return NULL;
    }
    p = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return p;
}

static int ac_py_int(const json_t *v, long long *out) {
    double d;
    if (!v) return 0;
    if (v->type == JSON_BOOL) {
        *out = v->bool_val ? 1 : 0;
        return 1;
    }
    if (v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    *out = (long long)d;
    return 1;
}

static void ac_fmt_float(double d, char *buf, size_t n) {
    int prec;
    for (prec = 15; prec <= 17; prec++) {
        snprintf(buf, n, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) return;
    }
}

static int ac_repr_str(ac_sb *b, const char *s) {
    const char *p;
    if (!ac_sb_putc(b, '\'')) return 0;
    for (p = s; *p; p++) {
        if (*p == '\\' || *p == '\'') {
            if (!ac_sb_putc(b, '\\') || !ac_sb_putc(b, *p)) return 0;
        } else if (!ac_sb_putc(b, *p)) {
            return 0;
        }
    }
    return ac_sb_putc(b, '\'');
}

static int ac_repr(ac_sb *b, const json_t *v);

static int ac_emit_str(ac_sb *b, const json_t *v) {
    char num[48];
    if (!v) return ac_sb_puts(b, "None");
    switch (v->type) {
    case JSON_NULL:
        return ac_sb_puts(b, "None");
    case JSON_BOOL:
        return ac_sb_puts(b, v->bool_val ? "True" : "False");
    case JSON_STRING:
        return ac_sb_puts(b, v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        long long i;
        if (ac_py_int(v, &i)) snprintf(num, sizeof(num), "%lld", i);
        else ac_fmt_float(v->num_val, num, sizeof(num));
        return ac_sb_puts(b, num);
    }
    case JSON_ARRAY:
    case JSON_OBJECT:
        return ac_repr(b, v);
    }
    return ac_sb_puts(b, "None");
}

static int ac_repr(ac_sb *b, const json_t *v) {
    size_t i;
    if (!v) return ac_sb_puts(b, "None");
    switch (v->type) {
    case JSON_STRING:
        return ac_repr_str(b, v->str_val ? v->str_val : "");
    case JSON_ARRAY:
        if (!ac_sb_putc(b, '[')) return 0;
        for (i = 0; i < v->c.count; i++) {
            if (i && !ac_sb_puts(b, ", ")) return 0;
            if (!ac_repr(b, v->c.items[i])) return 0;
        }
        return ac_sb_putc(b, ']');
    case JSON_OBJECT:
        if (!ac_sb_putc(b, '{')) return 0;
        for (i = 0; i < v->c.count; i++) {
            const char *k = v->c.keys ? v->c.keys[i] : "";
            if (i && !ac_sb_puts(b, ", ")) return 0;
            if (!ac_repr_str(b, k ? k : "")) return 0;
            if (!ac_sb_puts(b, ": ")) return 0;
            if (!ac_repr(b, v->c.items[i])) return 0;
        }
        return ac_sb_putc(b, '}');
    default:
        return ac_emit_str(b, v);
    }
}

static char *ac_py_str(const json_t *v) {
    ac_sb b = {NULL, 0, 0};
    char *out;
    if (!ac_emit_str(&b, v)) {
        free(b.p);
        return NULL;
    }
    out = ac_sb_take(&b);
    return out ? out : ac_dup("");
}

/* str(value or "").strip().rstrip("/") */
static char *ac_base_url(const json_t *v) {
    char *s = ac_truthy(v) ? ac_py_str(v) : ac_dup("");
    size_t i, j;
    if (!s) return NULL;
    i = 0;
    j = strlen(s);
    while (i < j && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' ||
                     s[i] == '\f' || s[i] == '\v')) i++;
    while (j > i && (s[j - 1] == ' ' || s[j - 1] == '\t' || s[j - 1] == '\n' ||
                     s[j - 1] == '\r' || s[j - 1] == '\f' || s[j - 1] == '\v')) j--;
    while (j > i && s[j - 1] == '/') j--;
    memmove(s, s + i, j - i);
    s[j - i] = '\0';
    return s;
}

/* --- REGISTRY_AUTHTYPE_ARMS ---------------------------------------------- */

typedef struct { const char *auth_type; const char *arm; } ac_arm_row;

static const ac_arm_row ARM_TABLE[] = {
    {"vertex",            "_resolve_vertex_arm"},
    {"aws_sdk",           "_resolve_bedrock_arm"},
    {"oauth_minimax",     "_resolve_minimax_oauth_arm"},
    {"oauth_device_code", "_resolve_plugin_oauth_arm"},
    {"oauth_external",    "_resolve_plugin_oauth_arm"},
    {NULL, NULL}
};

json_t *w21_auxiliaryc_fb830d_registry_arms(void) {
    json_t *out = json_object();
    int i;
    for (i = 0; ARM_TABLE[i].auth_type; i++) {
        json_set(out, ARM_TABLE[i].auth_type, json_string(ARM_TABLE[i].arm));
    }
    return out;
}

json_t *w21_auxiliaryc_fb830d_arm_for(const char *auth_type) {
    int i;
    if (auth_type) {
        for (i = 0; ARM_TABLE[i].auth_type; i++) {
            if (!strcmp(auth_type, ARM_TABLE[i].auth_type)) return json_string(ARM_TABLE[i].arm);
        }
    }
    return json_null();
}

/* --- _log_once_debug ------------------------------------------------------ */

typedef struct {
    char **v;
    size_t n;
    size_t cap;
} ac_set;

static int ac_set_has(const ac_set *s, const char *k) {
    size_t i;
    for (i = 0; i < s->n; i++) {
        if (!strcmp(s->v[i], k)) return 1;
    }
    return 0;
}

static void ac_set_add(ac_set *s, const char *k) {
    char *copy;
    if (!k || ac_set_has(s, k)) return;
    if (s->n == s->cap) {
        size_t ncap = s->cap ? s->cap * 2 : 8;
        char **nv = (char **)realloc(s->v, ncap * sizeof(*nv));
        if (!nv) return;
        s->v = nv;
        s->cap = ncap;
    }
    copy = ac_dup(k);
    if (copy) s->v[s->n++] = copy;
}

static int ac_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w21_auxiliaryc_fb830d_log_once(const json_t *keys, const json_t *seen) {
    ac_set state;
    json_t *out = json_object();
    json_t *logged = json_array();
    json_t *seen_out = json_array();
    size_t i;

    state.v = NULL;
    state.n = state.cap = 0;
    if (seen && seen->type == JSON_ARRAY) {
        for (i = 0; i < seen->c.count; i++) {
            const json_t *v = seen->c.items[i];
            if (v && v->type == JSON_STRING) ac_set_add(&state, v->str_val);
        }
    }
    if (keys && keys->type == JSON_ARRAY) {
        for (i = 0; i < keys->c.count; i++) {
            const json_t *v = keys->c.items[i];
            if (v && v->type == JSON_STRING && v->str_val) {
                int already = ac_set_has(&state, v->str_val);
                if (!already) ac_set_add(&state, v->str_val);
                json_append(logged, json_bool(already ? 0 : 1));
            } else {
                json_append(logged, json_bool(0));
            }
        }
    }
    if (state.n) qsort(state.v, state.n, sizeof(state.v[0]), ac_cmp_str);
    for (i = 0; i < state.n; i++) json_append(seen_out, json_string(state.v[i]));
    json_set(out, "logged", logged);
    json_set(out, "seen", seen_out);
    for (i = 0; i < state.n; i++) free(state.v[i]);
    free(state.v);
    return out;
}

/* --- _resolve_minimax_oauth_arm ------------------------------------------ */

static json_t *ac_absent(const char *reason, const char *debug, int logged) {
    json_t *out = json_object();
    json_set(out, "status", json_string("absent"));
    json_set(out, "reason", json_string(reason));
    json_set(out, "base_url", json_null());
    json_set(out, "model", json_null());
    json_set(out, "kind", json_null());
    /* `debug` is the message the caller actually saw: the debug-once guard
     * suppresses a repeat, so nothing is emitted then. */
    json_set(out, "debug", (debug && logged) ? json_string(debug) : json_null());
    json_set(out, "logged", json_bool(logged ? 1 : 0));
    return out;
}

static json_t *ac_ok(const char *kind, json_t *base_url, json_t *model) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "reason", json_null());
    json_set(out, "base_url", base_url ? base_url : json_null());
    json_set(out, "model", model ? model : json_null());
    json_set(out, "kind", json_string(kind));
    json_set(out, "debug", json_null());
    json_set(out, "logged", json_bool(0));
    return out;
}

/* req.model or aux_model or "MiniMax-M3" */
static char *ac_pick_model(const json_t *in) {
    const json_t *req = json_obj_get(in, "req_model");
    const json_t *aux = json_obj_get(in, "aux_model");
    if (ac_truthy(req)) return ac_py_str(req);
    if (ac_truthy(aux)) return ac_py_str(aux);
    return ac_dup(AC_PLACEHOLDER_MODEL);
}

static void ac_set_free(ac_set *s) {
    size_t i;
    for (i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL;
    s->n = s->cap = 0;
}

json_t *w21_auxiliaryc_fb830d_minimax_resolve(const json_t *in) {
    int probe, logged = 0;
    ac_set seen;
    const json_t *seen_in, *state, *creds;
    char *base = NULL;

    if (!in || in->type != JSON_OBJECT) in = json_object();
    seen.v = NULL;
    seen.n = seen.cap = 0;
    seen_in = json_obj_get(in, "seen");
    if (seen_in && seen_in->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < seen_in->c.count; i++) {
            const json_t *v = seen_in->c.items[i];
            if (v && v->type == JSON_STRING) ac_set_add(&seen, v->str_val);
        }
    }
    probe = ac_truthy(json_obj_get(in, "probe"));
    state = json_obj_get(in, "state");
    creds = json_obj_get(in, "creds");

    if (probe) {
        const json_t *token = (state && state->type == JSON_OBJECT)
                              ? json_obj_get(state, "access_token") : NULL;
        const json_t *raw_base = (state && state->type == JSON_OBJECT)
                                 ? json_obj_get(state, "inference_base_url") : NULL;
        base = ac_base_url(raw_base);
        if (!ac_truthy(token) || !base || !base[0]) {
            json_t *out;
            logged = !ac_set_has(&seen, "minimax-oauth");
            out = ac_absent("probe_absent",
                            "resolve_provider_client: minimax-oauth not logged in", logged);
            free(base);
            ac_set_free(&seen);
            return out;
        }
        {
            char *model_s = ac_pick_model(in);
            json_t *out = ac_ok("probe", json_string(base),
                                json_string(model_s ? model_s : AC_PLACEHOLDER_MODEL));
            free(model_s);
            free(base);
            ac_set_free(&seen);
            return out;
        }
    }

    /* non-probe: the credential store is the seam */
    {
        const json_t *err = json_obj_get(in, "creds_error");
        if (err && err->type == JSON_OBJECT) {
            const char *kind = json_get_str(err, "type", "Exception");
            const char *text = json_get_str(err, "message", "");
            char msg[512];
            json_t *out;
            if (!strcmp(kind, "AuthError")) {
                snprintf(msg, sizeof(msg),
                         "resolve_provider_client: minimax-oauth unavailable: %s", text);
                logged = !ac_set_has(&seen, "minimax-oauth");
                out = ac_absent("auth_error", msg, logged);
            } else {
                snprintf(msg, sizeof(msg),
                         "resolve_provider_client: unexpected minimax-oauth resolution failure: %s",
                         text);
                logged = !ac_set_has(&seen, "minimax-oauth");
                out = ac_absent("unexpected", msg, logged);
            }
            ac_set_free(&seen);
            return out;
        }
    }

    if (!creds || creds->type != JSON_OBJECT) {
        json_t *out;
        logged = !ac_set_has(&seen, "minimax-oauth");
        out = ac_absent("incomplete",
                        "resolve_provider_client: minimax-oauth credentials incomplete", logged);
        ac_set_free(&seen);
        return out;
    }

    base = ac_base_url(json_obj_get(creds, "base_url"));
    if (!ac_truthy(json_obj_get(creds, "api_key_callable")) || !base || !base[0]) {
        json_t *out;
        logged = !ac_set_has(&seen, "minimax-oauth");
        out = ac_absent("incomplete",
                        "resolve_provider_client: minimax-oauth credentials incomplete", logged);
        free(base);
        ac_set_free(&seen);
        return out;
    }
    if (ac_truthy(json_obj_get(in, "build_import_error"))) {
        /* `except ImportError: return None, None` -- silently, no debug */
        json_t *out = ac_absent("sdk_absent", NULL, 0);
        free(base);
        ac_set_free(&seen);
        return out;
    }

    {
        char *model_s = ac_pick_model(in);
        json_t *out = ac_ok("anthropic", json_string(base), json_string(model_s ? model_s : ""));
        free(model_s);
        free(base);
        ac_set_free(&seen);
        return out;
    }
}
