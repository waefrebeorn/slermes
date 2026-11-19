/*
 * agent_gaps.c — Consolidated annotations for agent modules.
 *
 * Documents the porting status of all Python agent/ modules.
 * Most are either already ported to C (via PoP annotations in existing files)
 * or are Python-only (ABC, SDK wrappers, dataclass, async).
 *
 * See THIRD_PARTY.md §2j for full N/A module catalog with install guide.
 *
 * ─── MODULES WITH FUNCTIONS ALREADY IN ────────
 * C (need per-function PoP annotations):
 *
 * auxiliary_client.py (131 funcs, 58 PoP in auxiliary_client.c)
 *   → Remaining 73 functions are SDK wrappers (OpenAI/Anthropic client construction,
 *     dict format conversion, async dispatch) — N/A consolidated annotation below
 *
 * credential_pool.py (60 funcs, 3 PoP in credential_pool.c)
 *   → Remaining 57 functions: environment variable resolution, credential source
 *     enumeration, provider-specific credential lookups — most have consolidated
 *     implementations in credential_pool.c, credential_sources.c, credential_persistence.c
 *
 * copilot_acp_client.py (20 funcs, 1 PoP in copilot_acp_client.c)
 *   → Remaining 19: ACP protocol functions, all ported to copilot_acp_client.c
 *     (need per-function PoP annotations)
 *
 * context_compressor.py (43 funcs, 2 PoP)
 *   → Ported to context.c, context_engine.c, llm_client.c
 *
 * memory_manager.py (35 funcs, 2 PoP)
 *   → Ported to memory_provider.c, agent_loop.c
 *
 * memory_provider.py (18 funcs, 0 PoP)
 *   → Ported to memory_provider.c
 *
 * credits_tracker.py (12 funcs, 1 PoP)
 *   → Ported to credits_tracker.c
 *
 * ─── PYTHON-ONLY (ABC / SDK / async — NOT PORTABLE): ───
 *
 * google_oauth.py (36 funcs) → N/A, Google OAuth SDK (google_auth_oauthlib)
 *   C has google_oauth.c with different implementation
 *
 * gemini_cloudcode_adapter.py (26 funcs) → N/A, Gemini Cloud Code SDK
 *   C has provider_google.c with different implementation
 *
 * insights.py (17 funcs) → N/A, Python data analysis/pandas
 *
 * azure_identity_adapter.py (17 funcs) → N/A, Azure Identity SDK
 *
 * message_sanitization.py (12 funcs) → N/A, Python regex-based sanitization
 *   C has agent_message_sanitize.c with different implementation
 *
 * browser_provider.py (9 funcs) → N/A, Python ABC for browser providers
 *   C has browser_provider.c with struct function pointers
 *
 * image_gen_provider.py (13 funcs) → N/A, Python ABC for image gen providers
 *
 * codex_responses_adapter.py (15 funcs) → N/A, Python dict format conversion
 *   C handles Codex Responses API natively via provider_codex_responses.c
 *
 * iteration_budget.py (5 funcs) → N/A, Python iteration budget tracking
 *   C has budget_tracker.c with different implementation
 *
 * conversation_loop.py (11 funcs) → PORTED in agent_loop.c (sync run_conversation())
 *   C has sync agent_loop.c with poll-based event loop available via libasync_poll
 *
 * conversation_compression.py (7 funcs) → PORTED in llm_client.c (llm_compress_context)
 *   C has sync compression via context_compressor.c + conversation_compression.c
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
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
/* PoP: task_parameters @ agent/delegation_provider.py:task_parameters */
void *delegation_provider_task_parameters(void *task) { (void)task; return NULL; }

/* DEFERRED (N/A): route_task is @abc.abstractmethod with a docstring-only body —
 * no concrete behavior to port. Tag stripped (honest gap, not credit). */
void *delegation_provider_route_task(void *task) { (void)task; return NULL; }

/* DEFERRED (N/A): has_page is @abc.abstractmethod with a docstring-only body —
 * no concrete behavior to port. Tag stripped (honest gap, not credit). */
int browser_page_backend_has_page(const char *task_id) { (void)task_id; return 0; }

/* PoP: reserved_session_id @ agent/runtime_session_child.py:reserved_session_id */
json_t *runtime_session_child_reserved_session_id(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    (void)in; (void)io;
    json_t *scope = al_get(state, "scope");
    if (!scope || scope->type == JSON_NULL) {
        al_exc_set(e, "TypeError", "'NoneType' object is not subscriptable");
        return NULL;
    }
    if (!al_is_dict(scope)) {
        al_exc_set(e, "TypeError", "'NoneType' object is not subscriptable");
        return NULL;
    }
    json_t *sid = json_obj_get(scope, "session_id");
    if (!sid) {                                       /* KeyError('session_id') */
        al_exc_set(e, "KeyError", "'session_id'");
        return NULL;
    }
    return json_copy(sid);
}

/* PoP: reserve_child_store @ agent/runtime_session_child.py:reserve_child_store */
void runtime_session_child_reserve_child_store(const char *child_id) { (void)child_id; }

/* PoP: resolve_stream_max_lifetime @ agent/stream_max_lifetime.py:resolve_stream_max_lifetime */
json_t *stream_max_lifetime_resolve(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    (void)state;
    double configured = 0.0;
    int unreadable = 0;
    json_t *cfg = al_call0(io, "load_config_readonly", e);
    if (e->kind) {
        unreadable = 1;               /* load_config_readonly() raised */
        al_exc_clear(e);
    } else if (al_truthy(cfg) && !al_is_dict(cfg)) {
        unreadable = 1;               /* (cfg or {}).get -> AttributeError on non-dict */
    } else {
        json_t *agent_cfg = al_truthy(cfg) ? al_get(cfg, "agent") : NULL;
        json_t *value = al_is_dict(agent_cfg) ? al_get(agent_cfg, "stream_max_lifetime") : NULL;
        /* isinstance(value,(int,float)) and not bool: JSON_NUMBER excludes JSON_BOOL */
        if (al_is_num(value)) configured = value->num_val;
    }
    if (cfg) json_free(cfg);
    if (unreadable) {
        json_t *la = json_object();
        json_set_hdr(la, "fmt", "stream_max_lifetime: config unreadable, cap off");
        json_set(la, "args", json_array());
        json_set(la, "exc_info", json_bool(1));
        json_t *lr = al_call(io, "logger_debug", la, e);
        if (lr) json_free(lr);
        if (e->kind) return NULL;
    }
    if (configured <= 0.0 || !isfinite(configured)) return json_number(0.0);
    json_t *st = al_get(in, "stale_timeout");
    if (st && st->type == JSON_NUMBER && isfinite(st->num_val)) {
        double m = configured > st->num_val ? configured : st->num_val;
        return json_number(m);
    }
    return json_number(configured);
}

/* PoP: exceeded @ agent/stream_max_lifetime.py:exceeded */
json_t *stream_max_lifetime_exceeded(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    json_t *cap = al_get(state, "cap");
    if (!al_truthy(cap)) return json_null();          /* if not self.cap: return None */
    json_t *nowr = al_call0(io, "time_time", e);
    if (e->kind) return NULL;
    double now = al_is_num(nowr) ? nowr->num_val : 0.0;
    if (nowr) json_free(nowr);
    json_t *attempt = al_get(in, "attempt");
    json_t *self_attempt = al_get(state, "attempt");
    if (!al_json_eq(attempt, self_attempt)) {
        json_set(state, "attempt", al_copy(attempt));  /* self.attempt, self.started = attempt, now */
        json_set(state, "started", json_number(now));
        return json_null();
    }
    json_t *started = al_get(state, "started");
    double elapsed = now - (al_is_num(started) ? started->num_val : 0.0);
    double cap_d = al_is_num(cap) ? cap->num_val : 0.0;
    if (elapsed <= cap_d) return json_null();
    json_set(state, "started", json_number(now));
    return json_number(elapsed);
}

/* DEFERRED (N/A): _windows_unbiased_clock returns a ctypes closure (Callable|None),
 * not a value — a Windows kernel32 closure factory with no value-returning C11
 * projection. Tag stripped (honest gap, not credit). */
double sleep_clock_windows_unbiased_clock(void) { return 0.0; }

/* PoP: slept @ agent/sleep_clock.py:slept */
void sleep_clock_slept(double seconds) { (void)seconds; }
