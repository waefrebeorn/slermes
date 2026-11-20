/*
 * memory_manager_flow3.c — agent/memory_manager.py remainder beyond flow2 (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/agent/memory_manager.py
 *
 * CRLF-aware PoP grep (all src .c files) before writing found 54 claimed names
 * (40 in memory_manager.c/_gaps/_helpers/_ports/_wrappers + 2 loose cross-file
 * claims + 14 in memory_manager_flow2.c). AST enumeration of memory_manager.py
 * yields 53 class/module defs (nested closures travel with their parent); 9
 * defs remain UNCLAIMED. This file ports exactly those 9:
 *
 *   PoP: mm3_redact_for_provider      @ agent/memory_manager.py:_redact_for_provider
 *   PoP: mm3_redact_messages          @ agent/memory_manager.py:_redact_messages_for_egress
 *   PoP: mm3_gate_init                @ agent/memory_manager.py:_StuckCallGate.__init__
 *   PoP: mm3_gate_enter               @ agent/memory_manager.py:_StuckCallGate.enter
 *   PoP: mm3_gate_leave               @ agent/memory_manager.py:_StuckCallGate.leave
 *   PoP: mm3_manager_init             @ agent/memory_manager.py:MemoryManager.__init__
 *   PoP: mm3_tool_call_keywords       @ agent/memory_manager.py:MemoryManager._tool_call_keywords
 *   PoP: mm3_drop_provider            @ agent/memory_manager.py:MemoryManager._drop_provider
 *   PoP: mm3_initialize_for_agent     @ agent/memory_manager.py:MemoryManager.initialize_for_agent
 *
 * Nested closures `walk` and `scrub` of _redact_for_provider are driven inside
 * mm3_redact_for_provider (AST extraction in the oracle); they are not claims.
 * Every sibling another PoP set owns (_scrub, _signature_params, _has_var_kwargs,
 * initialize_all, ...) enters through the universal seam.
 *
 * Self-contained, libjson-only. Every live effect enters through ONE seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * `args` is BORROWED (the seam copies it); mm3_call frees it after the call.
 * The result is a NEW json_t* the caller frees. NULL == Python None;
 * {"__raise__":cls,"msg":msg} == a raised exception. Fail-closed: a missing
 * seam -> RuntimeError raise spec.
 *
 * FULL BUILTIN EXCEPTION HIERARCHY: mm3_cls_is() walks child -> parent over
 * every builtin exception class (SystemExit/KeyboardInterrupt/GeneratorExit
 * stay above Exception, so `except Exception` never swallows them).
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__":...} object == a propagated exception.
 * Seams used: redact_for_egress {text,urls}; tokenize {text} -> {tokens:[..]};
 * redact_match {text,tokens}; redact_sub {text,tokens}; without_withheld_prompts
 * {messages}; hermes_home_key {}; signature {fn}; initialize_all {kwargs};
 * say {message}; logger.warning {message}.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include "json.h"

/* ── Python-value helpers ── */
static bool mm3_jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool mm3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool mm3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool mm3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static const char *mm3_sval(const json_t *v) {
    return (mm3_is_str(v)) ? v->str_val : "";
}
static double mm3_nval(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static char *mm3_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}

/* ── universal seam ── */
typedef struct mm3_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} mm3_seams_t;

static json_t *mm3_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool mm3_is_raise(const json_t *r) {
    return mm3_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *mm3_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static const char *mm3_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}
/* mm3_call OWNS args (frees after the call); the seam BORROWS args. */
static json_t *mm3_call(const mm3_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return mm3_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static void mm3_set_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
/* set k to a copy of v, or an explicit JSON null when v is NULL/JSON_NULL. */
static void mm3_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}

/* ── FULL builtin exception hierarchy (child -> parent) ── */
typedef struct mm3_exc_pair { const char *child; const char *parent; } mm3_exc_pair_t;
static const mm3_exc_pair_t MM3_EXC_TREE[] = {
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"Exception", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
};
static const char *mm3_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(MM3_EXC_TREE) / sizeof(MM3_EXC_TREE[0]); i++)
        if (!strcmp(MM3_EXC_TREE[i].child, c)) return MM3_EXC_TREE[i].parent;
    return NULL;
}
/* isinstance(c, ancestor): walk child -> parent up to BaseException. */
static bool mm3_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = mm3_exc_parent(c);
    }
    return false;
}

/* ── _signature_params / _has_var_kwargs (claimed elsewhere; re-derived) ── */
static json_t *mm3_signature_params(mm3_seams_t *sp, const json_t *fn) {
    json_t *a = json_object();
    mm3_set_node(a, "fn", fn);
    json_t *r = mm3_call(sp, "signature", a);
    if (mm3_is_raise(r)) {
        const char *c = mm3_raise_class(r);
        if (mm3_cls_is(c, "TypeError") || mm3_cls_is(c, "ValueError")) {
            json_free(r);
            return NULL;  /* uninspectable -> None */
        }
    }
    return r;
}
static bool mm3_has_var_kwargs(const json_t *params) {
    if (!mm3_is_obj(params)) return false;
    for (size_t i = 0; i < params->c.count; i++) {
        const json_t *p = params->c.items[i];
        if (mm3_is_obj(p) &&
            !strcmp(mm3_sval(json_obj_get(p, "kind")), "VAR_KEYWORD"))
            return true;
    }
    return false;
}

/* ══ PoP: mm3_redact_for_provider @ agent/memory_manager.py:_redact_for_provider ══ */
typedef struct {
    mm3_seams_t *sp;
    bool urls;
    json_t *learned;        /* array of tokens (deduped) */
    json_t *tokens_sorted;  /* learned sorted by len desc, tie asc (owned) */
    json_t *err;            /* propagated raise spec (owned), else NULL */
} mm3_redact_ctx_t;

static bool mm3_arr_has_str(const json_t *a, const char *s) {
    if (!mm3_is_arr(a)) return false;
    for (size_t i = 0; i < a->c.count; i++)
        if (mm3_is_str(a->c.items[i]) && !strcmp(a->c.items[i]->str_val, s))
            return true;
    return false;
}
static void mm3_learned_add(mm3_redact_ctx_t *c, const char *tok) {
    if (!mm3_arr_has_str(c->learned, tok))
        json_append(c->learned, json_string(tok));
}
/* seam tokenize -> {"tokens":[...]}; add tokens of A not in B. */
static void mm3_learn_diff(mm3_redact_ctx_t *c, const char *a, const char *b) {
    json_t *aa = json_object(); mm3_set_str(aa, "text", a);
    json_t *ra = mm3_call(c->sp, "tokenize", aa);
    if (mm3_is_raise(ra)) { if (!c->err) c->err = ra; else json_free(ra); return; }
    json_t *ba = json_object(); mm3_set_str(ba, "text", b);
    json_t *rb = mm3_call(c->sp, "tokenize", ba);
    if (mm3_is_raise(rb)) { if (!c->err) c->err = rb; else json_free(rb); json_free(ra); return; }
    const json_t *ta = mm3_is_obj(ra) ? json_obj_get(ra, "tokens") : NULL;
    const json_t *tb = mm3_is_obj(rb) ? json_obj_get(rb, "tokens") : NULL;
    if (mm3_is_arr(ta))
        for (size_t i = 0; i < ta->c.count; i++)
            if (mm3_is_str(ta->c.items[i]) && !mm3_arr_has_str(tb, ta->c.items[i]->str_val))
                mm3_learned_add(c, ta->c.items[i]->str_val);
    json_free(ra); json_free(rb);
}
/* first-pass fix: _scrub(text, urls=urls) then learn tokens. */
static json_t *mm3_fix_scrub(mm3_redact_ctx_t *c, const char *text) {
    json_t *a = json_object();
    mm3_set_str(a, "text", text);
    json_set(a, "urls", json_bool(c->urls));
    json_t *out = mm3_call(c->sp, "redact_for_egress", a);
    if (mm3_is_raise(out)) { c->err = out; return NULL; }
    const char *oc = out ? (mm3_is_str(out) ? out->str_val : "") : "";
    if (strcmp(oc, text) != 0)
        mm3_learn_diff(c, text, oc);
    return out;  /* may be NULL (scripted None) */
}
/* second-pass fix: pattern.search(t) then pattern.sub("[redacted]", t). */
static json_t *mm3_fix_redact(mm3_redact_ctx_t *c, const char *text) {
    json_t *ma = json_object();
    mm3_set_str(ma, "text", text);
    mm3_set_node(ma, "tokens", c->tokens_sorted);
    json_t *m = mm3_call(c->sp, "redact_match", ma);
    if (mm3_is_raise(m)) { c->err = m; return NULL; }
    bool hit = mm3_jtruth(m);
    json_free(m);
    if (!hit) return json_string(text);
    json_t *sa = json_object();
    mm3_set_str(sa, "text", text);
    mm3_set_node(sa, "tokens", c->tokens_sorted);
    json_t *s = mm3_call(c->sp, "redact_sub", sa);
    if (mm3_is_raise(s)) { c->err = s; return NULL; }
    return s ? s : json_null();
}
/* walk(value, fix): strings get the pass fix; containers recurse. */
static json_t *mm3_walk(mm3_redact_ctx_t *c, const json_t *v, int pass) {
    if (c->err) return NULL;
    if (mm3_is_str(v)) {
        const char *s = v->str_val;
        if (!s || !s[0] || !strncmp(s, "data:", 5)) return json_copy(v);
        return pass == 0 ? mm3_fix_scrub(c, s) : mm3_fix_redact(c, s);
    }
    if (mm3_is_obj(v)) {
        json_t *out = json_object();
        for (size_t i = 0; i < v->c.count; i++) {
            json_t *child = mm3_walk(c, v->c.items[i], pass);
            if (c->err) { json_free(out); return NULL; }
            json_set(out, v->c.keys[i], child ? child : json_null());
        }
        return out;
    }
    if (mm3_is_arr(v)) {
        json_t *out = json_array();
        for (size_t i = 0; i < v->c.count; i++) {
            json_t *child = mm3_walk(c, v->c.items[i], pass);
            if (c->err) { json_free(out); return NULL; }
            json_append(out, child ? child : json_null());
        }
        return out;
    }
    return json_copy(v);  /* number / bool / null: unchanged */
}
/* sorted(learned, key=len, reverse=True); deterministic tie (len differs in domain). */
static void mm3_sort_learned(mm3_redact_ctx_t *c) {
    size_t n = mm3_is_arr(c->learned) ? c->learned->c.count : 0;
    json_t *out = json_array();
    bool *used = n ? calloc(n, sizeof(bool)) : NULL;
    for (size_t k = 0; k < n; k++) {
        size_t best = n;
        for (size_t i = 0; i < n; i++) {
            if (used[i] || !mm3_is_str(c->learned->c.items[i])) continue;
            if (best == n) { best = i; continue; }
            const char *a = c->learned->c.items[i]->str_val;
            const char *b = c->learned->c.items[best]->str_val;
            size_t la = strlen(a), lb = strlen(b);
            if (la > lb || (la == lb && strcmp(a, b) < 0)) best = i;
        }
        if (best == n) break;
        used[best] = true;
        json_append(out, json_copy(c->learned->c.items[best]));
    }
    free(used);
    c->tokens_sorted = out;
}
static json_t *mm3_redact_for_provider(mm3_seams_t *sp, const json_t *values, bool urls) {
    mm3_redact_ctx_t c = {sp, urls, json_array(), NULL, NULL};
    size_t n = mm3_is_arr(values) ? values->c.count : 0;
    json_t *scrubbed = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *v = mm3_walk(&c, mm3_is_arr(values) ? values->c.items[i] : NULL, 0);
        if (c.err) { json_free(v); json_free(scrubbed); json_free(c.learned);
                     json_free(c.tokens_sorted); return c.err; }
        json_append(scrubbed, v ? v : json_null());
    }
    if (mm3_is_arr(c.learned) && c.learned->c.count > 0) {
        mm3_sort_learned(&c);
        json_t *final = json_array();
        for (size_t i = 0; i < scrubbed->c.count; i++) {
            json_t *v = mm3_walk(&c, scrubbed->c.items[i], 1);
            if (c.err) { json_free(v); json_free(final); json_free(scrubbed);
                         json_free(c.learned); json_free(c.tokens_sorted); return c.err; }
            json_append(final, v ? v : json_null());
        }
        json_free(scrubbed); scrubbed = final;
    }
    json_free(c.learned);
    json_free(c.tokens_sorted);
    if (n == 1) {
        json_t *only = scrubbed->c.count ? scrubbed->c.items[0] : NULL;
        json_t *ret = only ? json_copy(only) : NULL;
        json_free(scrubbed);
        return ret;
    }
    return scrubbed;  /* tuple -> JSON array */
}

/* PoP: mm3_redact_messages @ agent/memory_manager.py:_redact_messages_for_egress */
static json_t *mm3_redact_messages(mm3_seams_t *sp, const json_t *messages) {
    if (!messages || messages->type == JSON_NULL) return NULL;
    json_t *a = json_object();
    mm3_set_node(a, "messages", messages);
    json_t *r = mm3_call(sp, "without_withheld_prompts", a);
    if (mm3_is_raise(r)) return r;
    json_t *one = json_array();
    json_append(one, r ? r : json_null());
    json_t *out = mm3_redact_for_provider(sp, one, true);
    json_free(one);
    return out;
}

/* ── _StuckCallGate state helpers ── */
static bool mm3_key_eq(const json_t *a, const json_t *b) {
    if (!mm3_is_arr(a) || !mm3_is_arr(b) || a->c.count != 2 || b->c.count != 2)
        return false;
    return !strcmp(mm3_sval(a->c.items[0]), mm3_sval(b->c.items[0])) &&
           !strcmp(mm3_sval(a->c.items[1]), mm3_sval(b->c.items[1]));
}
static json_t *mm3_gate_find(json_t *gate, const json_t *key) {
    if (!mm3_is_arr(gate)) return NULL;
    for (size_t i = 0; i < gate->c.count; i++)
        if (mm3_key_eq(json_obj_get(gate->c.items[i], "key"), key))
            return gate->c.items[i];
    return NULL;
}
static json_t *mm3_gate_ensure(json_t *gate, const json_t *key) {
    json_t *rec = mm3_gate_find(gate, key);
    if (rec) return rec;
    rec = json_object();
    json_set(rec, "key", json_copy(key));
    json_set(rec, "calls", json_array());
    json_append(gate, rec);
    return rec;
}

/* PoP: mm3_gate_init @ agent/memory_manager.py:_StuckCallGate.__init__ */
static json_t *mm3_gate_init(json_t *st) {
    json_set(st, "gate", json_array());
    return NULL;
}

/* PoP: mm3_gate_enter @ agent/memory_manager.py:_StuckCallGate.enter */
static json_t *mm3_gate_enter(json_t *st, const json_t *key, const char *token,
                              double timeout, double now) {
    json_t *gate = json_obj_get(st, "gate");
    if (!mm3_is_arr(gate)) { json_set(st, "gate", json_array()); gate = json_obj_get(st, "gate"); }
    json_t *rec = mm3_gate_ensure(gate, key);
    json_t *calls = json_obj_get(rec, "calls");
    double last_ok = json_has(rec, "last_ok") ? json_get_num(rec, "last_ok", 0) : -1e300;
    double stuck = 0;
    if (mm3_is_arr(calls))
        for (size_t i = 0; i < calls->c.count; i++) {
            const json_t *call = calls->c.items[i];
            if (!mm3_is_arr(call) || call->c.count < 3) continue;
            if (mm3_nval(call->c.items[1]) > last_ok && now > mm3_nval(call->c.items[2]))
                stuck += 1;
        }
    double inflight = mm3_is_arr(calls) ? (double)calls->c.count : 0.0;
    bool admit;
    if (inflight >= 16.0) {
        admit = false;
    } else if (stuck < 4.0) {
        admit = true;
        json_obj_del(rec, "probe_at");
        json_obj_del(rec, "skips");
    } else {
        double probe = json_has(rec, "probe_at") ? json_get_num(rec, "probe_at", now) : now;
        if (!json_has(rec, "probe_at")) json_set(rec, "probe_at", json_number(probe));
        admit = (now - probe) >= 60.0;
        if (admit) json_set(rec, "probe_at", json_number(now));
    }
    if (!admit) {
        double skips = (json_has(rec, "skips") ? json_get_num(rec, "skips", 0) : 0.0) + 1.0;
        json_set(rec, "skips", json_number(skips));
        json_t *out = json_array();
        json_append(out, json_number(stuck));
        json_append(out, json_number(inflight));
        json_append(out, json_number(skips));
        return out;
    }
    json_t *entry = json_array();
    json_append(entry, json_string(token ? token : ""));
    json_append(entry, json_number(now));
    json_append(entry, json_number(now + timeout));
    json_append(calls, entry);
    return NULL;
}

/* PoP: mm3_gate_leave @ agent/memory_manager.py:_StuckCallGate.leave */
static json_t *mm3_gate_leave(json_t *st, const json_t *key, const char *token,
                              bool ok, double now) {
    json_t *gate = json_obj_get(st, "gate");
    json_t *rec = mm3_gate_find(gate, key);
    if (!rec) return NULL;
    json_t *calls = json_obj_get(rec, "calls");
    if (!mm3_is_arr(calls)) return NULL;
    size_t found = calls->c.count;
    for (size_t i = 0; i < calls->c.count; i++) {
        const json_t *c = calls->c.items[i];
        if (mm3_is_arr(c) && c->c.count >= 1 && mm3_is_str(c->c.items[0]) &&
            !strcmp(c->c.items[0]->str_val, token ? token : "")) { found = i; break; }
    }
    if (found == calls->c.count) return NULL;
    json_t *nc = json_array();
    for (size_t i = 0; i < calls->c.count; i++)
        if (i != found) json_append(nc, json_copy(calls->c.items[i]));
    json_set(rec, "calls", nc);
    if (nc->c.count > 0) {
        if (ok) json_set(rec, "last_ok", json_number(now));
        return NULL;
    }
    /* empty calls -> pop key from all four maps = drop the record */
    if (mm3_is_arr(gate)) {
        json_t *ng = json_array();
        for (size_t i = 0; i < gate->c.count; i++)
            if (gate->c.items[i] != rec) json_append(ng, json_copy(gate->c.items[i]));
        json_set(st, "gate", ng);
    }
    return NULL;
}

/* PoP: mm3_manager_init @ agent/memory_manager.py:MemoryManager.__init__ */
static json_t *mm3_manager_init(mm3_seams_t *sp, json_t *st, const json_t *timeout_in) {
    /* Lines 215-219 precede validation: a float()/<=0 ValueError leaves these
     * four fields set. Mirror that order so state history matches on raise. */
    json_set(st, "providers", json_array());
    json_set(st, "tool_to_provider", json_object());
    json_set(st, "external_prefetch_spill_config", json_null());
    json_set(st, "has_external", json_bool(false));
    double timeout;
    if (!timeout_in || timeout_in->type == JSON_NULL) {
        timeout = 8.0;
    } else if (timeout_in->type == JSON_NUMBER) {
        timeout = timeout_in->num_val;
    } else if (timeout_in->type == JSON_STRING) {
        char *end = NULL;
        timeout = strtod(timeout_in->str_val, &end);
        if (!end || *end != '\0') {
            char *m = mm3_xsf("could not convert string to float: '%s'",
                              timeout_in->str_val);
            json_t *r = mm3_raise("ValueError", m); free(m); return r;
        }
    } else {
        return mm3_raise("ValueError", "external_prefetch_timeout must be positive");
    }
    if (timeout <= 0)
        return mm3_raise("ValueError", "external_prefetch_timeout must be positive");
    json_set(st, "external_prefetch_timeout", json_number(timeout));  /* line 223 */
    json_t *h = mm3_call(sp, "hermes_home_key", json_object());        /* line 225 */
    if (mm3_is_raise(h)) return h;

    json_set(st, "home_key", h ? h : json_null());
    json_set(st, "sync_executor", json_null());
    json_set(st, "background_futures", json_object());
    json_set(st, "shutting_down", json_bool(false));
    json_set(st, "teardown_claim", json_null());
    json_t *ds = json_object();
    json_set(ds, "status", json_string("not_started"));
    json_set(ds, "abandoned_writes", json_number(0));
    json_set(ds, "abandoned_prefetches", json_number(0));
    json_set(ds, "active_tasks", json_number(0));
    json_set(st, "shutdown_drain_state", ds);
    json_set(st, "bound_scope", json_null());
    json_set(st, "agent_context", json_string("primary"));
    json_set(st, "tool_call_keywords_cache", json_object());
    return NULL;
}

/* PoP: mm3_tool_call_keywords @ agent/memory_manager.py:MemoryManager._tool_call_keywords */
static json_t *mm3_tool_call_keywords(mm3_seams_t *sp, json_t *cache, const json_t *provider) {
    const char *pid = mm3_sval(json_obj_get(provider, "id"));
    if (json_obj_get(cache, pid)) {
        json_t *hit = json_obj_get(cache, pid);
        return hit && hit->type == JSON_NULL ? NULL : json_copy(hit);
    }
    json_t *params = mm3_signature_params(sp, json_obj_get(provider, "handle_tool_call"));
    if (mm3_is_raise(params)) return params;
    json_t *val;  /* NULL == cached None */
    if (!params || mm3_has_var_kwargs(params)) {
        val = json_null();
    } else {
        json_t *fs = json_array();
        bool *used = params->c.count ? calloc(params->c.count, sizeof(bool)) : NULL;
        for (size_t k = 0; k < params->c.count; k++) {
            size_t best = params->c.count;
            for (size_t i = 0; i < params->c.count; i++) {
                if (used[i]) continue;
                if (best == params->c.count ||
                    strcmp(params->c.keys[i], params->c.keys[best]) < 0) best = i;
            }
            if (best == params->c.count) break;
            used[best] = true;
            json_append(fs, json_string(params->c.keys[best]));
        }
        free(used);
        val = fs;
    }
    json_free(params);
    json_set(cache, pid, val ? json_copy(val) : json_null());
    return val;
}

/* PoP: mm3_drop_provider @ agent/memory_manager.py:MemoryManager._drop_provider */
static json_t *mm3_drop_provider(json_t *st, const json_t *provider) {
    const char *pid = mm3_sval(json_obj_get(provider, "id"));
    json_t *cache = json_obj_get(st, "tool_call_keywords_cache");
    if (mm3_is_obj(cache)) json_obj_del(cache, pid);
    json_t *provs = json_obj_get(st, "providers");
    json_t *np = json_array();
    if (mm3_is_arr(provs))
        for (size_t i = 0; i < provs->c.count; i++)
            if (strcmp(mm3_sval(json_obj_get(provs->c.items[i], "id")), pid))
                json_append(np, json_copy(provs->c.items[i]));
    json_set(st, "providers", np);
    json_t *ttm = json_obj_get(st, "tool_to_provider");
    json_t *nm = json_object();
    if (mm3_is_obj(ttm))
        for (size_t i = 0; i < ttm->c.count; i++) {
            const json_t *pv = ttm->c.items[i];
            const char *vid = mm3_sval(json_obj_get(pv, "id"));
            if (!vid[0]) vid = mm3_sval(pv);  /* legacy scalar provider id */
            if (strcmp(vid, pid))
                json_set(nm, ttm->c.keys[i], json_copy(pv));
        }
    json_set(st, "tool_to_provider", nm);
    bool ext = false;
    for (size_t i = 0; i < np->c.count; i++)
        if (strcmp(mm3_sval(json_obj_get(np->c.items[i], "name")), "builtin")) { ext = true; break; }
    json_set(st, "has_external", json_bool(ext));
    return NULL;
}

/* PoP: mm3_initialize_for_agent @ agent/memory_manager.py:MemoryManager.initialize_for_agent */
static json_t *mm3_initialize_for_agent(mm3_seams_t *sp, json_t *st,
                                        const json_t *before, const json_t *kwargs, bool say) {
    json_t *ia = json_object();
    json_set(ia, "kwargs", kwargs ? json_copy(kwargs) : json_object());
    json_t *r = mm3_call(sp, "initialize_all", ia);
    if (mm3_is_raise(r)) return r;
    json_t *after = mm3_is_obj(r) ? json_obj_get(r, "providers") : NULL;
    bool has_ext = mm3_is_obj(r) ? mm3_jtruth(json_obj_get(r, "has_external")) : false;
    /* 'after' borrows from r: snapshot it BEFORE json_free(r) or we copy freed memory. */
    json_t *providers = after ? json_copy(after) : NULL;
    json_free(r);
    json_set(st, "providers", providers ? providers : json_array());
    json_set(st, "has_external", json_bool(has_ext));

    if (mm3_is_arr(before))
        for (size_t i = 0; i < before->c.count; i++) {
            const json_t *bp = before->c.items[i];
            const char *bpid = mm3_sval(json_obj_get(bp, "id"));
            if (!strcmp(mm3_sval(json_obj_get(bp, "name")), "builtin")) continue;
            bool kept = false;
            json_t *kept_arr = json_obj_get(st, "providers");
            if (mm3_is_arr(kept_arr))
                for (size_t j = 0; j < kept_arr->c.count; j++)
                    if (!strcmp(mm3_sval(json_obj_get(kept_arr->c.items[j], "id")), bpid)) {
                        kept = true; break;
                    }
            if (kept) continue;
            char *msg = mm3_xsf(
                "\xE2\x9A\xA0 Memory provider '%s' is disabled for this profile: it handed out the "
                "provider instance another profile already uses. Built-in memory still works; the "
                "operator can see why with `hermes logs errors`.",
                mm3_sval(json_obj_get(bp, "name")));
            json_t *a = json_object(); mm3_set_str(a, "message", msg); free(msg);
            json_t *w = mm3_call(sp, say ? "say" : "logger.warning", a);
            if (mm3_is_raise(w)) return w;
            json_free(w);
        }
    return json_bool(has_ext);
}
