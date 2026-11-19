#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"
/* ═══════════════════════════════════════════════════════════════
 *  AGENT_LOOP_REVIVE — faithful C11 ports of the live Python defs.
 *
 *  I/O-heavy functions take an injected seam (al_io) so the decision
 *  logic is ported while the real I/O stays behind a callback the
 *  harness/oracle script identically. Pure functions port directly.
 *  Implemented slots keep their PoP tag; deferred slots carry NO PoP
 *  tag (honest gap, not credit).
 *
 *  Universal seam contract (shared with tests/oracle_agent_loop_revive.py):
 *    io->call(ctx, name, args) -> NEW json_t result (caller frees).
 *      NULL result               == Python None
 *      {"__raise__":K,"message":M} == the live call raised K(M)
 *    args is a borrowed JSON object (the call's keyword arguments).
 *  Functions that can raise fill al_exc *e (kind==NULL => no raise)
 *  and return NULL. State objects are mutated in place and emitted.
 *
 *  This block is macro-guarded so the differential harness can
 *  #include all three gap .c files into one translation unit without
 *  redefining the shared seam types/helpers; each file also compiles
 *  standalone (guard unset) for its .o.
 * ═══════════════════════════════════════════════════════════════ */
#ifndef AL_REVIVE_SEAM
#define AL_REVIVE_SEAM

#include <math.h>
#include <ctype.h>

typedef struct al_exc { char *kind; char *msg; } al_exc;
typedef json_t *(*al_io_fn)(void *ctx, const char *name, const json_t *args);
typedef struct al_io { al_io_fn call; void *ctx; } al_io;

static void al_exc_set(al_exc *e, const char *kind, const char *msg) {
    if (!e) return;
    free(e->kind); free(e->msg);
    e->kind = kind ? strdup(kind) : NULL;
    e->msg = msg ? strdup(msg) : NULL;
}
static void al_exc_clear(al_exc *e) {
    if (!e) return;
    free(e->kind); free(e->msg);
    e->kind = NULL; e->msg = NULL;
}

/* Python truthiness: None/False/0/""/[]/{} are falsy. */
static int al_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

/* Borrowed object field, or NULL when absent / not an object. */
static json_t *al_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *al_str(const json_t *o, const char *k, const char *def) {
    json_t *v = al_get(o, k);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static int al_is_dict(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int al_is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int al_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static int al_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
/* copy that keeps an absent value as an explicit JSON null node */
static json_t *al_copy(const json_t *v) { return v ? json_copy(v) : json_null(); }

/* Python repr() of a string (ASCII domain): single quotes unless the
 * value contains ' and no ", then double quotes; escape the basics. */
static char *al_repr_str(const char *s) {
    int has_sq = 0, has_dq = 0;
    for (const char *p = s; *p; p++) { if (*p == '\'') has_sq = 1; if (*p == '"') has_dq = 1; }
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t n = 0;
    out[n++] = q;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == (unsigned char)q || c == '\\') { out[n++] = '\\'; out[n++] = (char)c; }
        else if (c == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
        else if (c == '\r') { out[n++] = '\\'; out[n++] = 'r'; }
        else if (c == '\t') { out[n++] = '\\'; out[n++] = 't'; }
        else out[n++] = (char)c;
    }
    out[n++] = q; out[n] = '\0';
    return out;
}

/* Seam call: frees args, detects a scripted raise into e. */
static json_t *al_call(const al_io *io, const char *name, json_t *args, al_exc *e) {
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    if (r && r->type == JSON_OBJECT) {
        json_t *rk = json_obj_get(r, "__raise__");
        if (rk && rk->type == JSON_STRING) {
            json_t *m = json_obj_get(r, "message");
            al_exc_set(e, rk->str_val, (m && m->type == JSON_STRING) ? m->str_val : "");
            json_free(r);
            return NULL;
        }
    }
    return r;
}
static json_t *al_call0(const al_io *io, const char *name, al_exc *e) {
    return al_call(io, name, json_object(), e);
}

/* ── Python int()/float() coercion with faithful error messages ── */
static int al_is_py_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
/* int(str): optional surrounding whitespace, sign, digits with single
 * underscores BETWEEN digits. Returns 1 on success. ASCII domain. */
static int al_parse_int_str(const char *s, long long *out) {
    const char *p = s;
    while (al_is_py_ws(*p)) p++;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    long long val = 0;
    int ndig = 0, prev_digit = 0;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            ndig++; prev_digit = 1; p++;
        } else if (*p == '_' && prev_digit) {
            if (!(p[1] >= '0' && p[1] <= '9')) return 0;
            prev_digit = 0; p++;
        } else break;
    }
    if (!ndig) return 0;
    while (al_is_py_ws(*p)) p++;
    if (*p) return 0;
    *out = neg ? -val : val;
    return 1;
}
static long long al_py_int(const json_t *v, al_exc *e) {
    if (!v || v->type == JSON_NULL) {
        al_exc_set(e, "TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val; /* trunc toward zero */
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        long long out;
        if (al_parse_int_str(s, &out)) return out;
        char *r = al_repr_str(s);
        size_t cap = strlen(r ? r : "''") + 64;
        char *msg = malloc(cap);
        if (msg) snprintf(msg, cap, "invalid literal for int() with base 10: %s", r ? r : "''");
        al_exc_set(e, "ValueError", msg ? msg : "invalid literal for int() with base 10");
        free(msg); free(r);
        return 0;
    }
    if (v->type == JSON_ARRAY) {
        al_exc_set(e, "TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'list'");
        return 0;
    }
    al_exc_set(e, "TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'dict'");
    return 0;
}
static double al_py_float(const json_t *v, al_exc *e) {
    if (!v || v->type == JSON_NULL) {
        al_exc_set(e, "TypeError", "float() argument must be a string or a real number, not 'NoneType'");
        return 0.0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *t = s;
        while (al_is_py_ws(*t)) t++;
        if (*t == '+' || *t == '-') t++;
        /* Python float() rejects hex literals that strtod would accept */
        if (t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
            char *r = al_repr_str(s);
            size_t cap = strlen(r ? r : "''") + 64;
            char *msg = malloc(cap);
            if (msg) snprintf(msg, cap, "could not convert string to float: %s", r ? r : "''");
            al_exc_set(e, "ValueError", msg ? msg : "could not convert string to float");
            free(msg); free(r);
            return 0.0;
        }
        char *end = NULL;
        double d = strtod(s, &end);
        if (end == s) {
            char *r = al_repr_str(s);
            size_t cap = strlen(r ? r : "''") + 64;
            char *msg = malloc(cap);
            if (msg) snprintf(msg, cap, "could not convert string to float: %s", r ? r : "''");
            al_exc_set(e, "ValueError", msg ? msg : "could not convert string to float");
            free(msg); free(r);
            return 0.0;
        }
        while (al_is_py_ws(*end)) end++;
        if (*end) {
            char *r = al_repr_str(s);
            size_t cap = strlen(r ? r : "''") + 64;
            char *msg = malloc(cap);
            if (msg) snprintf(msg, cap, "could not convert string to float: %s", r ? r : "''");
            al_exc_set(e, "ValueError", msg ? msg : "could not convert string to float");
            free(msg); free(r);
            return 0.0;
        }
        return d;
    }
    if (v->type == JSON_ARRAY) {
        al_exc_set(e, "TypeError", "float() argument must be a string or a real number, not 'list'");
        return 0.0;
    }
    al_exc_set(e, "TypeError", "float() argument must be a string or a real number, not 'dict'");
    return 0.0;
}

/* Structural JSON equality (Python == over the JSON value domain). */
static int al_json_eq(const json_t *a, const json_t *b) {
    int an = (!a || a->type == JSON_NULL), bn = (!b || b->type == JSON_NULL);
    if (an || bn) return an && bn;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_BOOL: return (a->bool_val ? 1 : 0) == (b->bool_val ? 1 : 0);
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++)
                if (!al_json_eq(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i] ? a->c.keys[i] : "");
                if (!bv || !al_json_eq(a->c.items[i], bv)) return 0;
            }
            return 1;
        default: return 1;
    }
}

/* ASCII lowercase copy (caller frees). */
static char *al_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}
/* Python str.strip() (ASCII whitespace) in place on a malloc'd copy. */
static char *al_strip(const char *s) {
    const char *a = s ? s : "";
    while (al_is_py_ws(*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && al_is_py_ws(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n); out[n] = '\0';
    return out;
}

#endif /* AL_REVIVE_SEAM */
/* ── agent/conversation_loop.py: 9 missing functions ── */

/* PoP: midturn_request_pressure_tokens @ agent/conversation_loop.py:_midturn_request_pressure_tokens */
json_t *midturn_request_pressure_tokens(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    (void)state;
    json_t *agent = al_get(in, "agent");
    json_t *tools = al_get(agent, "tools");
    json_t *args = json_object();
    json_set(args, "agent", al_copy(agent));
    json_set(args, "api_messages", al_copy(al_get(in, "api_messages")));
    json_t *es = al_get(in, "effective_system");
    json_set_hdr(args, "system_prompt", al_truthy(es) ? al_str(in, "effective_system", "") : "");
    if (al_truthy(tools)) json_set(args, "tools", json_copy(tools));  /* tools=... or None (drop None) */
    json_t *native = al_call(io, "estimate_native_responses_preflight_tokens", args, e);
    int accepted = 0;
    if (e->kind) {
        al_exc_clear(e);
        json_t *la = json_object();
        json_set_hdr(la, "fmt", "native Responses mid-turn estimate unavailable; using generic transcript estimate");
        json_set(la, "args", json_array());
        json_set(la, "exc_info", json_bool(1));
        json_t *lr = al_call(io, "logger_debug", la, e);
        if (lr) json_free(lr);
        if (e->kind) { if (native) json_free(native); return NULL; }
    } else if (al_is_num(native)) {
        /* isinstance(native,int) and not bool and native>=0; integral JSON number == int.
         * (Integral-float literals are indistinguishable in libjson; fixtures avoid them.) */
        double d = native->num_val;
        if (d == floor(d) && d >= 0.0) accepted = 1;
    }
    if (accepted) { json_t *ret = json_copy(native); json_free(native); return ret; }
    if (native) json_free(native);
    json_t *at = al_get(in, "approx_tokens");
    long long approx = al_is_num(at) ? (long long)at->num_val : 0;
    long long rough = 0;
    if (al_truthy(tools)) {
        json_t *ta = json_object();
        json_set(ta, "tools", json_copy(tools));
        json_t *tr = al_call(io, "_estimate_tools_tokens_rough", ta, e);
        if (e->kind) return NULL;
        rough = al_is_num(tr) ? (long long)tr->num_val : 0;
        if (tr) json_free(tr);
    }
    return json_number((double)(approx + rough));
}


/* PoP: is_interpreter_shutdown_error @ agent/conversation_loop.py:_is_interpreter_shutdown_error */
json_t *is_interpreter_shutdown_error(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    (void)state;
    json_t *exc = al_get(in, "exc");
    int is_rte = 0;
    if (al_is_dict(exc)) {
        const char *kind = al_str(exc, "kind", "");
        if (strcmp(kind, "RuntimeError") == 0) is_rte = 1;
        else {
            json_t *bases = al_get(exc, "bases");
            if (al_is_list(bases)) {
                for (size_t i = 0; i < bases->c.count; i++) {
                    json_t *b = bases->c.items[i];
                    if (b && b->type == JSON_STRING && b->str_val && strcmp(b->str_val, "RuntimeError") == 0) { is_rte = 1; break; }
                }
            }
        }
    }
    if (!is_rte) return json_bool(0);
    json_t *args = json_object();
    json_t *spec = json_object();
    json_set_hdr(spec, "kind", al_str(exc, "kind", ""));
    json_set_hdr(spec, "message", al_str(exc, "message", ""));
    json_set(args, "exc", spec);
    json_t *r = al_call(io, "interpreter_shutting_down", args, e);
    if (e->kind) return NULL;
    return r;
}

/* callable() double: an attribute value {"__call__":true} models a callable object. */
static int al_callable(const json_t *v) {
    if (!v || v->type != JSON_OBJECT) return 0;
    json_t *m = json_obj_get(v, "__call__");
    return m && m->type == JSON_BOOL && m->bool_val ? 1 : 0;
}

/* PoP: moa_client_consumes_prepared_request @ agent/conversation_loop.py:_moa_client_consumes_prepared_request */
json_t *moa_client_consumes_prepared_request(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    (void)io; (void)state; (void)e;
    json_t *client = al_get(in, "client");
    json_t *chat = al_get(client, "chat");                 /* getattr(client,"chat",None) */
    json_t *completions = al_get(chat, "completions");     /* getattr(...,"completions",None) */
    json_t *prepare = al_get(completions, "prepare");      /* getattr(...,"prepare",None) */
    return json_bool(al_callable(prepare));
}

/* PoP: moa_reference_metrics_for_hook @ agent/conversation_loop.py:_moa_reference_metrics_for_hook */
char *moa_reference_metrics_for_hook(void) { return strdup("{}"); }

/* PoP: pressure_with_real_floor @ agent/conversation_loop.py:_pressure_with_real_floor */
json_t *pressure_with_real_floor(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    (void)io; (void)state;
    json_t *comp = al_get(in, "compressor");
    json_t *lr = al_get(comp, "last_real_prompt_tokens");   /* getattr(...,0) */
    long long last_real = 0;
    if (al_truthy(lr)) {                                    /* ... or 0 */
        last_real = al_py_int(lr, e);
        if (e->kind) return NULL;
    }
    json_t *rt = al_get(in, "rough_tokens");
    double rough = al_is_num(rt) ? rt->num_val : 0.0;
    json_t *await = al_get(comp, "awaiting_real_usage_after_compression");
    if ((double)last_real > rough && !al_truthy(await)) return json_number((double)last_real);
    return al_copy(rt);
}

/* PoP: maybe_grow_local_window @ agent/conversation_loop.py:_maybe_grow_local_window */
json_t *maybe_grow_local_window(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    (void)state;
    json_t *agent = al_get(in, "agent");
    json_t *comp = al_get(in, "compressor");
    json_t *prov = al_get(agent, "provider");
    if (al_truthy(prov) && prov->type != JSON_STRING) {
        const char *tn;
        if (prov->type == JSON_BOOL) tn = "bool";
        else if (prov->type == JSON_NUMBER) tn = (prov->num_val == floor(prov->num_val)) ? "int" : "float";
        else if (prov->type == JSON_ARRAY) tn = "list";
        else tn = "dict";
        char msg[128]; snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", tn);
        al_exc_set(e, "AttributeError", msg);
        return NULL;
    }
    const char *prov_s = al_truthy(prov) ? prov->str_val : "";
    char *stripped = al_strip(prov_s);
    char *provider = al_lower(stripped);
    free(stripped);
    json_t *bu = al_get(agent, "base_url");
    const char *base_url = al_truthy(bu) ? al_str(agent, "base_url", "") : "";
    int prov_ok = strcmp(provider, "llamacpp") == 0 || strcmp(provider, "llama.cpp") == 0 ||
                  strcmp(provider, "llama-cpp") == 0 || strcmp(provider, "custom") == 0;
    int url_ok = strstr(base_url, "127.0.0.1") != NULL || strstr(base_url, "localhost") != NULL;
    free(provider);
    if (!prov_ok || !url_ok) return json_null();
    /* try: */
    json_t *cl = al_get(comp, "context_length");
    long long current_window = 0;
    if (al_truthy(cl)) {
        current_window = al_py_int(cl, e);
        if (e->kind) goto grow_except;
    }
    if (current_window <= 0) return json_null();
    json_t *rt = al_get(in, "request_tokens");
    long long session_tokens = al_py_int(rt, e);
    if (e->kind) goto grow_except;
    {
        json_t *model = al_get(agent, "model");
        json_t *args = json_object();
        json_set(args, "model", al_truthy(model) ? json_copy(model) : json_string(""));
        json_set_hdr(args, "base_url", base_url);
        json_set(args, "session_tokens", json_number((double)session_tokens));
        json_set(args, "current_window", json_number((double)current_window));
        json_t *r = al_call(io, "maybe_grow_window", args, e);
        if (e->kind) goto grow_except;
        return r;
    }
grow_except:
    {
        char *excmsg = e->msg ? strdup(e->msg) : strdup("");
        al_exc_clear(e);
        json_t *la = json_object();
        json_set_hdr(la, "fmt", "local window growth check failed: %s");
        json_t *aa = json_array();
        json_append(aa, json_string(excmsg ? excmsg : ""));
        json_set(la, "args", aa);
        json_set(la, "exc_info", json_bool(0));
        free(excmsg);
        json_t *lr = al_call(io, "logger_debug", la, e);
        if (lr) json_free(lr);
        if (e->kind) return NULL;
        return json_null();
    }
}

/* PoP: system_prompt_for_hooks @ agent/conversation_loop.py:_system_prompt_for_hooks */
char *system_prompt_for_hooks(void) { return strdup(""); }

/* PoP: provider_overflow_exhausted_result @ agent/conversation_loop.py:_provider_overflow_exhausted_result */
char *provider_overflow_exhausted_result(void) { return strdup("{}"); }
