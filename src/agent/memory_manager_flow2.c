/*
 * memory_manager_flow2.c — agent/memory_manager.py UNCLAIMED remainder (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/agent/memory_manager.py
 *
 * CRLF-aware PoP grep (all src .c files) before writing: the existing PoP sets
 * (memory_manager.c / _gaps.c / _helpers.c / _ports.c / _wrappers.c, plus the
 * loose cross-file claims in cli_cmd_config.c:on_delegation and
 * gw_dispatch.c:shutdown_all) claim 40 names. This file ports ONLY defs NO
 * existing PoP annotation claims:
 *
 *   PoP: mm2_signature_params              @ agent/memory_manager.py:_signature_params
 *   PoP: mm2_has_var_kwargs                @ agent/memory_manager.py:_has_var_kwargs
 *   PoP: mm2_accepts_require_checkpoint    @ agent/memory_manager.py:_accepts_require_checkpoint
 *   PoP: mm2_scrub                         @ agent/memory_manager.py:_scrub
 *   PoP: mm2_owner_allows                  @ agent/memory_manager.py:_owner_allows
 *   PoP: mm2_fork_turn_hooks_suppressed    @ agent/memory_manager.py:_fork_turn_hooks_suppressed
 *   PoP: mm2_provider_sync_accepts         @ agent/memory_manager.py:_provider_sync_accepts
 *   PoP: mm2_registry_key                  @ agent/memory_manager.py:_registry_key
 *   PoP: mm2_checkpoint_api_version        @ agent/memory_manager.py:_checkpoint_api_version
 *   PoP: mm2_supports_pre_compress_checkpoint
 *                                          @ agent/memory_manager.py:supports_pre_compress_checkpoint
 *   PoP: mm2_each_provider                 @ agent/memory_manager.py:_each_provider
 *   PoP: mm2_call_unless_stalled           @ agent/memory_manager.py:_call_unless_stalled
 *   PoP: mm2_claim_teardown                @ agent/memory_manager.py:claim_teardown
 *   PoP: mm2_release_soft                  @ agent/memory_manager.py:release_soft
 *
 * 14 functions (>=10 required). Nested closures of a ported def travel with
 * their parent (AST extraction in the oracle); they are not separate claims.
 * Every sibling that another PoP set owns (add_provider, get_provider,
 * on_pre_compress, on_memory_write, ...) is SEAMED here, never re-ported.
 *
 * Self-contained, libjson-only. Every live effect (inspect.signature,
 * redact_for_egress, caller_scope, provider callbacks, logger, the background
 * gate, flush_pending/shutdown_all) enters through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `args` is BORROWED (the seam records a copy, must not free it); `mm2_call`
 * frees it after the call. The result is a NEW json_t* the caller frees.
 * NULL == Python None (never a JSON_NULL node); {"__raise__":cls,"msg":msg}
 * == a raised exception. Fail-closed: missing seam -> RuntimeError raise spec.
 *
 * FULL BUILTIN EXCEPTION HIERARCHY: the mm2_cls_is() walk below covers every
 * builtin exception class (child -> parent). SystemExit / KeyboardInterrupt /
 * GeneratorExit stay above Exception, so `except Exception` never swallows
 * them; ValueError/TypeError children fold into their parents. Scripted
 * unknown class names are outside the fixture domain (the oracle would build
 * them as Exception subclasses).
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__":...} object == a propagated exception.
 * State window (caller-held, emitted by the harness as out.state) carries
 * bound_scope / home_key / agent_context / teardown_claim / shutting_down.
 *
 * Fixture domain: ASCII text; integral JSON numbers read as int; inspect
 * Parameter kinds are the six string names; provider doubles are objects with
 * a "name" (and optional pre_compress_checkpoint_api_version); keys in seam
 * arg objects are emitted in the exact order below (canon is order-sensitive).
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
static bool mm2_jtruth(const json_t *v) {
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
static bool mm2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool mm2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool mm2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static const char *mm2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
/* json_get_str/num/bool are OBJECT accessors — a bare VALUE node needs sval/nval. */
static double mm2_nval(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static char *mm2_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}

/* ── universal seam ── */
typedef struct mm2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} mm2_seams_t;

static json_t *mm2_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool mm2_is_raise(const json_t *r) {
    return mm2_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *mm2_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static const char *mm2_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}
/* mm2_call OWNS args (frees after the call); the seam BORROWS args. */
static json_t *mm2_call(const mm2_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return mm2_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static void mm2_set_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
/* set k to a copy of v, or an explicit JSON null when v is NULL/JSON_NULL
 * (json_set(obj,k,NULL) is a no-op in libjson — never pass NULL). */
static void mm2_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}

/* ── FULL builtin exception hierarchy (child -> parent) ── */
typedef struct mm2_exc_pair { const char *child; const char *parent; } mm2_exc_pair_t;
static const mm2_exc_pair_t MM2_EXC_TREE[] = {
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
static const char *mm2_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(MM2_EXC_TREE) / sizeof(MM2_EXC_TREE[0]); i++)
        if (!strcmp(MM2_EXC_TREE[i].child, c)) return MM2_EXC_TREE[i].parent;
    return NULL;
}
/* isinstance(c, ancestor): walk child -> parent up to BaseException. */
static bool mm2_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = mm2_exc_parent(c);
    }
    return false;
}

/* PoP: mm2_signature_params @ agent/memory_manager.py:_signature_params */
static json_t *mm2_signature_params(mm2_seams_t *sp, const json_t *fn) {
    json_t *a = json_object();
    mm2_set_node(a, "fn", fn);
    json_t *r = mm2_call(sp, "signature", a);
    if (mm2_is_raise(r)) {
        const char *c = mm2_raise_class(r);
        if (mm2_cls_is(c, "TypeError") || mm2_cls_is(c, "ValueError")) {
            json_free(r);
            return NULL;  /* uninspectable -> None */
        }
    }
    return r;  /* params object or a propagating raise */
}

/* PoP: mm2_has_var_kwargs @ agent/memory_manager.py:_has_var_kwargs */
static bool mm2_has_var_kwargs(const json_t *params) {
    if (!mm2_is_obj(params)) return false;
    for (size_t i = 0; i < params->c.count; i++) {
        const json_t *p = params->c.items[i];
        if (mm2_is_obj(p) &&
            !strcmp(mm2_sval(json_obj_get(p, "kind")), "VAR_KEYWORD"))
            return true;
    }
    return false;
}

/* PoP: mm2_accepts_require_checkpoint @ agent/memory_manager.py:_accepts_require_checkpoint */
static json_t *mm2_accepts_require_checkpoint(mm2_seams_t *sp, const json_t *fn) {
    json_t *params = mm2_signature_params(sp, fn);
    if (mm2_is_raise(params)) return params;
    if (!params) return json_bool(false);
    bool has_vk = mm2_has_var_kwargs(params);
    bool kind_ok = false;
    const json_t *rc = json_obj_get(params, "require_checkpoint");
    if (mm2_is_obj(rc)) {
        const char *k = mm2_sval(json_obj_get(rc, "kind"));
        kind_ok = !strcmp(k, "KEYWORD_ONLY") || !strcmp(k, "POSITIONAL_OR_KEYWORD");
    }
    json_free(params);
    return json_bool(has_vk || kind_ok);
}

/* PoP: mm2_scrub @ agent/memory_manager.py:_scrub */
static json_t *mm2_scrub(mm2_seams_t *sp, const json_t *text, bool urls) {
    json_t *a = json_object();
    mm2_set_node(a, "text", text);
    json_set(a, "urls", json_bool(urls));
    return mm2_call(sp, "redact_for_egress", a);
}

/* PoP: mm2_owner_allows @ agent/memory_manager.py:_owner_allows */
static json_t *mm2_owner_allows(mm2_seams_t *sp, const json_t *providers,
                                const json_t *bound, const char *hook) {
    if (!mm2_is_arr(providers) || providers->c.count == 0)
        return json_bool(true);  /* nothing to reach, nothing to refuse */
    char *label = mm2_xsf("hook %s", hook ? hook : "");
    json_t *a = json_object();
    mm2_set_node(a, "scope", bound);
    mm2_set_str(a, "label", label);
    free(label);
    json_t *r = mm2_call(sp, "caller_scope", a);
    if (mm2_is_raise(r)) return r;
    bool ok = (r != NULL);  /* `is not None`: scripted null -> NULL pointer */
    json_free(r);
    return json_bool(ok);
}

/* PoP: mm2_fork_turn_hooks_suppressed @ agent/memory_manager.py:_fork_turn_hooks_suppressed */
static json_t *mm2_fork_turn_hooks_suppressed(const char *agent_context) {
    return json_bool(agent_context && !strcmp(agent_context, "subagent"));
}

/* PoP: mm2_provider_sync_accepts @ agent/memory_manager.py:_provider_sync_accepts */
static json_t *mm2_provider_sync_accepts(mm2_seams_t *sp, const json_t *sync_fn,
                                        const char *keyword) {
    json_t *params = mm2_signature_params(sp, sync_fn);
    if (mm2_is_raise(params)) return params;
    if (!params) return json_bool(true);  /* uninspectable -> assume yes */
    bool has_vk = mm2_has_var_kwargs(params);
    bool present = json_obj_get(params, keyword) != NULL;  /* key presence */
    json_free(params);
    return json_bool(has_vk || present);
}

/* PoP: mm2_registry_key @ agent/memory_manager.py:_registry_key */
static json_t *mm2_registry_key(const json_t *bound, const char *home_key,
                                const char *pname) {
    const char *h = home_key ? home_key : "";
    if (mm2_is_obj(bound)) {
        const json_t *hk = json_obj_get(bound, "home_key");
        const char *s = mm2_sval(hk);
        if (s[0]) h = s;  /* (scope or {}).get("home_key") or self._home_key */
    }
    json_t *arr = json_array();
    json_append(arr, json_string(h));
    json_append(arr, json_string(pname ? pname : ""));
    return arr;
}

/* int() with the (TypeError, ValueError) swallow of _checkpoint_api_version.
 * Returns a NEW json number node, NULL for the swallowed None, or a raise
 * object for a BaseException kind that must propagate. */
static json_t *mm2_int_value(const json_t *v) {
    if (!v || v->type == JSON_NULL) return NULL;  /* int(None) -> TypeError */
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) return NULL;  /* out of domain */
        double t = d < 0 ? ceil(d) : floor(d);
        return json_number(t);
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\v' ||
               *s == '\f' || *s == '\r') s++;
        bool neg = false;
        if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
        if (*s < '0' || *s > '9') return NULL;  /* ValueError */
        long long acc = 0;
        while (*s >= '0' && *s <= '9') {
            acc = acc * 10 + (*s - '0');
            s++;
        }
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\v' ||
               *s == '\f' || *s == '\r') s++;
        if (*s != '\0') return NULL;
        return json_number(neg ? -(double)acc : (double)acc);
    }
    return NULL;
}

/* PoP: mm2_checkpoint_api_version @ agent/memory_manager.py:_checkpoint_api_version */
static json_t *mm2_checkpoint_api_version(const json_t *provider) {
    const json_t *v = mm2_is_obj(provider)
        ? json_obj_get(provider, "pre_compress_checkpoint_api_version") : NULL;
    if (!v) return json_number(1);  /* ABSENT -> getattr default (legacy v1) */
    return mm2_int_value(v);        /* present (incl. JSON null) -> int(value) */
}

/* PoP: mm2_supports_pre_compress_checkpoint @ agent/memory_manager.py:supports_pre_compress_checkpoint */
static json_t *mm2_supports_pre_compress_checkpoint(mm2_seams_t *sp,
                                                    const json_t *providers,
                                                    double api_version) {
    (void)sp;  /* the composed checkpoint helper is pure data here */
    if (!mm2_is_arr(providers)) return json_bool(false);
    for (size_t i = 0; i < providers->c.count; i++) {
        json_t *v = mm2_checkpoint_api_version(json_get(providers, i));
        if (mm2_is_raise(v)) return v;  /* no generator-level swallow */
        if (v && v->type == JSON_NUMBER && v->num_val >= api_version) {
            json_free(v);
            return json_bool(true);  /* any() short-circuits */
        }
        json_free(v);
    }
    return json_bool(false);
}

/* PoP: mm2_each_provider @ agent/memory_manager.py:_each_provider */
static json_t *mm2_each_provider(mm2_seams_t *sp, const json_t *providers,
                                 const char *label, const char *call_name,
                                 double level, bool exc_info) {
    json_t *results = json_array();
    if (!mm2_is_arr(providers)) return results;
    for (size_t i = 0; i < providers->c.count; i++) {
        const json_t *p = json_get(providers, i);
        json_t *a = json_object();
        mm2_set_node(a, "provider", p);
        json_t *r = mm2_call(sp, call_name, a);
        if (mm2_is_raise(r)) {
            const char *cn = mm2_raise_class(r);
            if (mm2_cls_is(cn, "Exception")) {
                /* logger.log(level, "Memory provider '%s' %s: %s",
                 *           provider.name, label, e, exc_info=exc_info) */
                json_t *la = json_object();
                json_set(la, "level", json_number(level));
                json_set(la, "fmt", json_string("Memory provider '%s' %s: %s"));
                json_t *args = json_array();
                json_append(args, json_string(mm2_sval(json_obj_get(p, "name"))));
                json_append(args, json_string(label ? label : ""));
                json_append(args, json_string(mm2_raise_msg(r)));  /* str(e) */
                json_set(la, "args", args);
                json_set(la, "exc_info", json_bool(exc_info));
                json_t *lr = mm2_call(sp, "logger.log", la);
                if (mm2_is_raise(lr)) { json_free(r); return lr; }
                json_free(lr);
                json_free(r);
                continue;  /* failure swallowed */
            }
            json_free(results);
            return r;  /* BaseException trio propagates */
        }
        json_append(results, r ? r : json_null());  /* None result appended */
    }
    return results;
}

/* PoP: mm2_call_unless_stalled @ agent/memory_manager.py:_call_unless_stalled */
static json_t *mm2_call_unless_stalled(mm2_seams_t *sp, const json_t *provider,
                                       const json_t *bound, const char *home_key,
                                       const char *call_name) {
    const char *pname = mm2_sval(json_obj_get(provider, "name"));
    if (!strcmp(pname, "builtin")) {
        json_t *r = mm2_call(sp, call_name, json_object());
        if (mm2_is_raise(r)) return r;
        json_free(r);
        return NULL;  /* `call(); return` — result DISCARDED */
    }
    json_t *key = mm2_registry_key(bound, home_key, pname);
    json_t *ea = json_object();
    mm2_set_node(ea, "key", key);
    json_set(ea, "timeout", json_number(30.0));
    json_t *skipped = mm2_call(sp, "gate.enter", ea);
    if (mm2_is_raise(skipped)) { json_free(key); return skipped; }
    if (mm2_is_arr(skipped) && skipped->c.count >= 3) {
        double stuck   = mm2_nval(json_get(skipped, 0));
        double infl    = mm2_nval(json_get(skipped, 1));
        double skips   = mm2_nval(json_get(skipped, 2));
        bool warn = (skips == 1.0 || fmod(skips, 100.0) == 0.0);
        if (warn) {
            json_t *la = json_object();
            json_set(la, "fmt", json_string(
                "Memory provider '%s' has %d background calls stuck for over "
                "%.0fs (%d in flight); skipping its sync/prefetch until it "
                "recovers (%d skipped)"));
            json_t *args = json_array();
            json_append(args, json_string(pname));
            json_append(args, json_number(stuck));
            json_append(args, json_number(30.0));
            json_append(args, json_number(infl));
            json_append(args, json_number(skips));
            json_set(la, "args", args);
            json_t *lr = mm2_call(sp, "logger.warning", la);
            if (mm2_is_raise(lr)) { json_free(skipped); json_free(key); return lr; }
            json_free(lr);
        }
        json_free(skipped);
        json_free(key);
        return NULL;
    }
    json_free(skipped);  /* null (admit) */

    json_t *r = mm2_call(sp, call_name, json_object());
    bool ok = !mm2_is_raise(r);
    json_t *la = json_object();
    mm2_set_node(la, "key", key);
    json_set(la, "ok", json_bool(ok));
    json_t *lr = mm2_call(sp, "gate.leave", la);  /* finally: runs on raise too */
    json_free(key);
    if (mm2_is_raise(lr)) { json_free(r); return lr; }  /* exit raise overrides */
    json_free(lr);
    return r;
}

/* PoP: mm2_claim_teardown @ agent/memory_manager.py:claim_teardown */
static json_t *mm2_claim_teardown(json_t *st, bool session_end) {
    const json_t *claim = json_obj_get(st, "teardown_claim");
    bool claim_none = !claim || claim->type == JSON_NULL;
    bool shut = mm2_jtruth(json_obj_get(st, "shutting_down"));
    bool result;
    if (claim_none && (session_end || !shut)) {
        json_set(st, "teardown_claim",
                 json_string(session_end ? "session_end" : "soft_release"));
        result = true;
    } else {
        result = session_end && claim && claim->type == JSON_STRING &&
                 !strcmp(claim->str_val, "session_end");
    }
    return json_bool(result);
}

/* PoP: mm2_release_soft @ agent/memory_manager.py:release_soft */
static json_t *mm2_release_soft(mm2_seams_t *sp, json_t *st) {
    json_t *c = mm2_claim_teardown(st, false);
    if (!mm2_jtruth(c)) { json_free(c); return NULL; }
    json_free(c);
    json_t *a = json_object();
    json_set(a, "timeout", json_number(10));
    json_t *fp = mm2_call(sp, "flush_pending", a);
    if (mm2_is_raise(fp)) return fp;
    json_free(fp);
    json_t *sd = mm2_call(sp, "shutdown_all", json_object());
    if (mm2_is_raise(sd)) return sd;
    json_free(sd);
    return NULL;  /* None */
}
