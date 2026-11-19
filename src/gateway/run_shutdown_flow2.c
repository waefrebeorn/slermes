/*
 * run_shutdown_flow2.c — gateway/run_shutdown.py UNCLAIMED-remainder FLOW cores
 * (universal-seam convention, sibling of run_shutdown_flow.c / run_adapters_flow.c).
 *
 * Ports the DECISION SEQUENCES of the shutdown active-work accounting, the
 * notice/result leaves, the watchdog-leash composition, the scale-to-zero
 * config/helpers and the cron restart-wait counters: call order, short-circuit
 * branches, early returns, exception->fallback paths and mutation order of the
 * live Python. Every live I/O (logging, config loads, cron/api/delegation probes,
 * relay adapter hooks, clocks) enters through rs2_seams_t.call(ctx, name, args);
 * args is OWNED by rs2_call (freed after the borrowed call); results are NEW
 * json_t with NULL == Python None and {"__raise__":K,"msg":M} == a raised
 * exception. BaseException kinds (SystemExit/KeyboardInterrupt/CancelledError/
 * GeneratorExit) pass through every `except Exception` site.
 *
 * Self-contained: libjson only (no cross-object symbols). All helpers static rs2_*.
 *
 * Modeling notes (differential oracle tests/oracle_run_shutdown_flow2.py):
 *  - Runner state is injected json_t mutated in place. Platform keys are the
 *    enum .value strings ("api_server", "telegram", ...). Adapter doubles are
 *    objects whose PRESENT hook keys model callable methods (api_server hooks
 *    record seam "api_server_hook" {name,args}; relay redial hooks record
 *    "adapter.hold_redial"/"adapter.release_redial" {}).
 *  - Result doubles (_send_failed/_send_error) are objects read via getattr
 *    semantics: absent key -> the getattr default; `is False`/`is True` are
 *    JSON_BOOL-true/false identity (0 / 1 do NOT match).
 *  - Ported siblings run inline (real) on both sides: _active_cron_job_count ->
 *    _running_cron_job_count; _active_api_run_count/_mark_api_runs_shutdown_requested
 *    -> _api_server_hook; _active_work_count/_drain_work_counts/_awaitable_work_count
 *    compose the counters; _wedged_cron_job_count/_restart_safe_cron_count ->
 *    _restart_wait_cron_counts; _scale_to_zero_idle_timeout_seconds ->
 *    _gateway_cfg_section; _scale_to_zero_abandon_suspend -> _scale_to_zero_hold_redial.
 *  - Claimed/foreign siblings are seams: running_agent_count, managed_turn_count,
 *    active_deferred_agent_worker_count, wedged_chat_agent_count,
 *    relay_adapter_for_dormancy, serving_state, scale_to_zero_status, and the
 *    gateway.restart / cron / tools / scale_to_zero leaves.
 *  - Integral floats == ints (libjson has no int/float identity): fixtures keep
 *    str()-computed positions (chat_id/thread_id) integral-int or non-integral,
 *    and numeric counters integral. int()/len()/subscript raise messages are the
 *    CPython 3.14 strings; invalid int() tokens are printable ASCII (no quotes /
 *    backslashes) so repr() quoting is unambiguous.
 *  - Ordered collections: _running_agents / adapters / config.platforms iterate
 *    in JSON insertion order (== dict order). Sets are not modeled here.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include "libjson/json.h"

typedef struct rs2_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} rs2_seams_t;

/* ── universal-seam helpers ── */
static json_t *rs2_call(const rs2_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool rs2_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *rs2_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static json_t *rs2_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool rs2_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static json_t *rs2_args(void) { return json_object(); }
static void rs2_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void rs2_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void rs2_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void rs2_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *rs2_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static json_t *rs2_mget(json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* ── Python value helpers ── */
static bool rs2_truth(const json_t *v) {
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
static bool rs2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool rs2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static const char *rs2_strval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *rs2_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isinf(d) && !isnan(d) && d == (double)(long long)d) return "int";
        return "float";
    }
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    return "dict";
}
static bool rs2_isnum(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}
static double rs2_numval(const json_t *v) {
    return (v->type == JSON_BOOL) ? (v->bool_val ? 1.0 : 0.0) : v->num_val;
}
/* str() of a scalar (int/bool/str/None). Containers are outside the fixture
 * domain for the str()-computed positions; returns a malloc'd string. */
static char *rs2_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isinf(d) && !isnan(d) && d == (double)(long long)d) {
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
            return strdup(buf);
        }
        snprintf(buf, sizeof(buf), "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    /* containers: not in domain; emit empty to stay safe */
    return strdup("");
}
/* Python numeric-literal underscores: every '_' must sit BETWEEN two digits. */
static bool rs2_underscores_ok(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '_') {
            if (i == 0 || i + 1 >= n) return false;
            if (!(s[i - 1] >= '0' && s[i - 1] <= '9')) return false;
            if (!(s[i + 1] >= '0' && s[i + 1] <= '9')) return false;
        }
    }
    return true;
}
/* int(v) for a JSON value. On failure sets *kind and builds *msg (caller frees
 * *msg when non-NULL). Truncates toward zero; bool -> 0/1. */
static long long rs2_py_int(const json_t *v, const char **kind, char **msg) {
    *kind = NULL; *msg = NULL;
    if (!v || v->type == JSON_NULL) {
        *kind = "TypeError";
        *msg = strdup("int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isinf(d)) { *kind = "OverflowError"; *msg = strdup("cannot convert float infinity to integer"); return 0; }
        if (isnan(d)) { *kind = "ValueError"; *msg = strdup("cannot convert float NaN to integer"); return 0; }
        return (long long)d;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *raw = s;
        while (*s && isspace((unsigned char)*s)) s++;
        const char *end = s + strlen(s);
        while (end > s && isspace((unsigned char)end[-1])) end--;
        size_t n = (size_t)(end - s);
        bool ok = rs2_underscores_ok(s, n);
        const char *p = s;
        bool neg = false;
        if (ok && p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
        long long acc = 0; bool any = false;
        if (ok) {
            for (; p < end; p++) {
                char c = *p;
                if (c == '_') continue;
                if (c < '0' || c > '9') { ok = false; break; }
                acc = acc * 10 + (c - '0');
                any = true;
            }
        }
        if (!ok || !any) {
            *kind = "ValueError";
            /* repr() of the ORIGINAL token: printable-ASCII domain, single quotes */
            size_t rl = strlen(raw);
            char *q = malloc(rl + 64);
            if (q) {
                strcpy(q, "invalid literal for int() with base 10: '");
                size_t k = strlen(q);
                for (size_t i = 0; i < rl; i++) {
                    char c = raw[i];
                    if (c == '\n') { q[k++] = '\\'; q[k++] = 'n'; }
                    else if (c == '\r') { q[k++] = '\\'; q[k++] = 'r'; }
                    else if (c == '\t') { q[k++] = '\\'; q[k++] = 't'; }
                    else q[k++] = c;
                }
                q[k++] = '\''; q[k] = '\0';
                *msg = q;
            }
            return 0;
        }
        return neg ? -acc : acc;
    }
    *kind = "TypeError";
    {
        char buf[160];
        snprintf(buf, sizeof(buf),
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 rs2_typename(v));
        *msg = strdup(buf);
    }
    return 0;
}
/* len(v): list/dict/str sized; others raise TypeError. */
static size_t rs2_len(const json_t *v, const char **kind, char **msg) {
    *kind = NULL; *msg = NULL;
    if (v && (v->type == JSON_ARRAY || v->type == JSON_OBJECT)) return v->c.count;
    if (v && v->type == JSON_STRING) {
        /* codepoint count (ASCII domain fixtures; count UTF-8 lead bytes) */
        const char *s = v->str_val ? v->str_val : "";
        size_t n = 0;
        for (; *s; s++) if ((*s & 0xC0) != 0x80) n++;
        return n;
    }
    *kind = "TypeError";
    {
        char buf[96];
        snprintf(buf, sizeof(buf), "object of type '%s' has no len()", rs2_typename(v));
        *msg = strdup(buf);
    }
    return 0;
}
/* obj[key] for a string key: dict lookup, else the live TypeError / KeyError. */
static json_t *rs2_subscript(const json_t *obj, const char *key) {
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return json_copy(v);
        char buf[96];
        snprintf(buf, sizeof(buf), "'%s'", key);
        return rs2_raise("KeyError", buf);
    }
    const char *tn = rs2_typename(obj);
    char buf[160];
    if (obj && obj->type == JSON_ARRAY)
        snprintf(buf, sizeof(buf), "list indices must be integers or slices, not str");
    else if (obj && obj->type == JSON_STRING)
        snprintf(buf, sizeof(buf), "string indices must be integers, not 'str'");
    else
        snprintf(buf, sizeof(buf), "'%s' object is not subscriptable", tn);
    return rs2_raise("TypeError", buf);
}
/* a + b for numeric operands; non-numeric -> the live operand-type TypeError. */
static json_t *rs2_add(const json_t *a, const json_t *b) {
    if (!rs2_isnum(a) || !rs2_isnum(b)) {
        char msg[160];
        snprintf(msg, sizeof(msg), "unsupported operand type(s) for +: '%s' and '%s'",
                 rs2_typename(a), rs2_typename(b));
        return rs2_raise("TypeError", msg);
    }
    return json_number(rs2_numval(a) + rs2_numval(b));
}
static json_t *rs2_sub(const json_t *a, const json_t *b) {
    if (!rs2_isnum(a) || !rs2_isnum(b)) {
        char msg[160];
        snprintf(msg, sizeof(msg), "unsupported operand type(s) for -: '%s' and '%s'",
                 rs2_typename(a), rs2_typename(b));
        return rs2_raise("TypeError", msg);
    }
    return json_number(rs2_numval(a) - rs2_numval(b));
}
/* max(a, b) over the numeric domain (fixtures keep both operands numeric). */
static json_t *rs2_max_num(const json_t *a, const json_t *b) {
    if (!rs2_isnum(a) || !rs2_isnum(b)) {
        char msg[160];
        snprintf(msg, sizeof(msg), "'>' not supported between instances of '%s' and '%s'",
                 rs2_typename(b), rs2_typename(a));
        return rs2_raise("TypeError", msg);
    }
    return json_number(rs2_numval(a) >= rs2_numval(b) ? rs2_numval(a) : rs2_numval(b));
}

/* ── logging seams (RecLogger twin) ──
 * logger.<level>(fmt, *args): {fmt, args?}; logger.log(level, fmt, *args,
 * exc_info=?): {fmt, level, args?, exc_info?}. Empty positional args omit the
 * "args" key. Returns the seam result (caller frees / propagates). */
static json_t *rs2_log_named(const rs2_seams_t *s, const char *name,
                             const char *fmt, json_t *args_arr) {
    json_t *a = rs2_args();
    rs2_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    return rs2_call(s, name, a);
}
static json_t *rs2_log(const rs2_seams_t *s, const char *level,
                       const char *fmt, json_t *args_arr) {
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    return rs2_log_named(s, name, fmt, args_arr);
}
static json_t *rs2_log_log(const rs2_seams_t *s, int level, const char *fmt,
                           json_t *args_arr, bool exc_info) {
    json_t *a = rs2_args();
    rs2_s(a, "fmt", fmt);
    rs2_n(a, "level", level);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    if (exc_info) rs2_b(a, "exc_info", true);
    return rs2_call(s, "logger.log", a);
}
static json_t *rs2_args1s(const char *a) {
    json_t *x = json_array();
    json_append(x, json_string(a ? a : ""));
    return x;
}
/* logger.<level>(fmt, exc_info=True) with no positional args: {fmt, exc_info?}. */
static json_t *rs2_log_exc(const rs2_seams_t *s, const char *level, const char *fmt, bool exc_info) {
    json_t *a = rs2_args();
    rs2_s(a, "fmt", fmt);
    if (exc_info) rs2_b(a, "exc_info", true);
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    return rs2_call(s, name, a);
}
/* ── exported flow cores (forward declarations) ── */
static json_t *rs2_send_failed(const json_t *result);
static json_t *rs2_send_error(const json_t *result);
static json_t *rs2_notice_target_key(const char *pv, const json_t *chat_id, const json_t *thread_id);
static json_t *rs2_delivery_target_key(const char *pv, const json_t *chat_id, const json_t *thread_id,
                                       const char *profile, const rs2_seams_t *s);
static json_t *rs2_restart_notification_allowed(const json_t *self, const char *pv);
static json_t *rs2_log_suppressed(int level, const char *msg, const json_t *args_arr,
                                  bool exc_info, const rs2_seams_t *s);
static json_t *rs2_effective_watchdog_leash(const json_t *runner, const rs2_seams_t *s);
static json_t *rs2_active_async_delegation_count(const rs2_seams_t *s);
static json_t *rs2_running_cron_job_count(const rs2_seams_t *s);
static json_t *rs2_active_cron_job_count(const json_t *self, const rs2_seams_t *s);
static json_t *rs2_api_server_hook(const json_t *self, const char *name, const json_t *args,
                                   const rs2_seams_t *s);
static json_t *rs2_active_api_run_count(const json_t *self, const rs2_seams_t *s);
static json_t *rs2_active_api_worker_count(const json_t *self, const rs2_seams_t *s);
static json_t *rs2_mark_api_runs_shutdown_requested(const json_t *self, const rs2_seams_t *s);
static json_t *rs2_active_work_count(const json_t *self, const rs2_seams_t *s);
static json_t *rs2_drain_work_counts(const json_t *self, const rs2_seams_t *s);
static json_t *rs2_gateway_cfg_section(const char *name, const rs2_seams_t *s);
static json_t *rs2_scale_to_zero_idle_timeout_seconds(const json_t *self, const rs2_seams_t *s);
static json_t *rs2_relay_wake_url_or_none(const rs2_seams_t *s);
static json_t *rs2_scale_to_zero_hold_redial(const json_t *self, bool held, const rs2_seams_t *s);
static json_t *rs2_scale_to_zero_abandon_suspend(json_t *self, const rs2_seams_t *s);
static json_t *rs2_restart_wait_cron_counts(const json_t *self, const rs2_seams_t *s);
static json_t *rs2_wedged_cron_job_count(const json_t *self, const rs2_seams_t *s);
static json_t *rs2_restart_safe_cron_count(const json_t *self, const rs2_seams_t *s);
static json_t *rs2_wedged_agent_count(const json_t *self, const rs2_seams_t *s);
static json_t *rs2_awaitable_work_count(const json_t *self, const rs2_seams_t *s);

/* PoP: rs2_send_failed @ gateway/run_shutdown.py:_send_failed */
static json_t *rs2_send_failed(const json_t *result) {
    bool f = false;
    if (result && result->type == JSON_OBJECT) {
        const json_t *s = json_obj_get(result, "success");
        if (s && s->type == JSON_BOOL && !s->bool_val) f = true;   /* `is False` identity */
    }
    return json_bool(f);
}

/* PoP: rs2_send_error @ gateway/run_shutdown.py:_send_error */
static json_t *rs2_send_error(const json_t *result) {
    if (result && result->type == JSON_OBJECT) {
        const json_t *e = json_obj_get(result, "error");
        if (e) return json_copy(e);
    }
    return json_string("send returned success=False");
}

/* PoP: rs2_notice_target_key @ gateway/run_shutdown.py:_notice_target_key */
static json_t *rs2_notice_target_key(const char *pv, const json_t *chat_id, const json_t *thread_id) {
    json_t *arr = json_array();
    json_append(arr, json_string(pv ? pv : ""));
    char *cs = rs2_pystr(chat_id);
    json_append(arr, json_string(cs));
    free(cs);
    if (rs2_truth(thread_id)) {
        char *ts = rs2_pystr(thread_id);
        json_append(arr, json_string(ts));
        free(ts);
    } else {
        json_append(arr, json_null());
    }
    return arr;
}

/* PoP: rs2_delivery_target_key @ gateway/run_shutdown.py:_delivery_target_key */
static json_t *rs2_delivery_target_key(const char *pv, const json_t *chat_id, const json_t *thread_id,
                                       const char *profile, const rs2_seams_t *s) {
    const char *use_pv = pv ? pv : "";
    char *pv2 = NULL;
    bool gate = profile && profile[0] && strcmp(profile, "default") != 0
                && strcmp(use_pv, "telegram") == 0;
    if (gate) {
        json_t *a = rs2_args();
        rs2_j(a, "chat_id", chat_id);
        json_t *r = rs2_call(s, "looks_like_telegram_private_chat_id", a);
        if (rs2_raised(r)) return r;
        bool like = rs2_truth(r);
        json_free(r);
        if (like) {
            size_t n = strlen(profile) + 1 + strlen(use_pv) + 1;
            pv2 = malloc(n);
            if (pv2) { snprintf(pv2, n, "%s:%s", profile, use_pv); use_pv = pv2; }
        }
    }
    json_t *res = rs2_notice_target_key(use_pv, chat_id, thread_id);
    free(pv2);
    return res;
}

/* PoP: rs2_restart_notification_allowed @ gateway/run_shutdown.py:_restart_notification_allowed */
static json_t *rs2_restart_notification_allowed(const json_t *self, const char *pv) {
    const json_t *config = rs2_oget(self, "config");
    const json_t *platforms = config ? rs2_oget(config, "platforms") : NULL;
    const json_t *pcfg = platforms ? rs2_oget(platforms, pv ? pv : "") : NULL;
    if (rs2_is_null(pcfg)) return json_bool(true);   /* platform_cfg is None */
    if (!json_has(pcfg, "gateway_restart_notification"))
        return rs2_raise("AttributeError", "missing attribute 'gateway_restart_notification'");
    const json_t *g = rs2_oget(pcfg, "gateway_restart_notification");
    return json_bool(rs2_truth(g));
}

/* PoP: rs2_log_suppressed @ gateway/run_shutdown.py:_log_suppressed */
static json_t *rs2_log_suppressed(int level, const char *msg, const json_t *args_arr,
                                  bool exc_info, const rs2_seams_t *s) {
    json_t *r = rs2_call(s, "step.fn", rs2_args());
    if (rs2_raised(r)) {
        if (rs2_is_base_exc(rs2_kind(r))) return r;   /* except Exception passes BaseException */
        json_t *la = args_arr ? json_copy(args_arr) : json_array();
        if (!exc_info) json_append(la, json_copy(r));   /* exc appended as last %s arg */
        json_t *lr = rs2_log_log(s, level, msg, la, exc_info);
        json_free(r);
        if (rs2_raised(lr)) return lr;
        json_free(lr);
        return NULL;   /* swallowed -> None */
    }
    json_free(r);
    return NULL;
}

/* PoP: rs2_effective_watchdog_leash @ gateway/run_shutdown.py:_effective_watchdog_leash */
static json_t *rs2_effective_watchdog_leash(const json_t *runner, const rs2_seams_t *s) {
    (void)runner;
    json_t *d = rs2_call(s, "effective_stop_drain_timeout", rs2_args());
    if (rs2_raised(d)) return d;
    json_t *a1 = rs2_args();
    rs2_j(a1, "delay", d);
    json_free(d);
    json_t *w = rs2_call(s, "resolve_shutdown_watchdog_delay", a1);
    if (rs2_raised(w)) return w;
    json_t *a2 = rs2_args();
    rs2_j(a2, "delay", w);
    json_free(w);
    return rs2_call(s, "effective_stop_watchdog_delay", a2);
}

/* PoP: rs2_active_async_delegation_count @ gateway/run_shutdown.py:_active_async_delegation_count */
static json_t *rs2_active_async_delegation_count(const rs2_seams_t *s) {
    json_t *r = rs2_call(s, "async_delegation.active_count", rs2_args());
    if (rs2_raised(r)) {
        if (rs2_is_base_exc(rs2_kind(r))) return r;
        json_free(r);
        return json_number(0);
    }
    return r;
}

/* PoP: rs2_running_cron_job_count @ gateway/run_shutdown.py:_running_cron_job_count */
static json_t *rs2_running_cron_job_count(const rs2_seams_t *s) {
    json_t *r = rs2_call(s, "cron.get_running_job_ids", rs2_args());
    if (rs2_raised(r)) return r;
    const char *kind = NULL; char *msg = NULL;
    size_t n = rs2_len(r, &kind, &msg);
    if (kind) {
        json_free(r);
        json_t *e = rs2_raise(kind, msg);
        free(msg);
        return e;
    }
    json_free(r);
    return json_number((double)n);
}

/* PoP: rs2_active_cron_job_count @ gateway/run_shutdown.py:_active_cron_job_count */
static json_t *rs2_active_cron_job_count(const json_t *self, const rs2_seams_t *s) {
    (void)self;
    json_t *r = rs2_running_cron_job_count(s);
    if (rs2_raised(r)) {
        if (rs2_is_base_exc(rs2_kind(r))) return r;
        json_free(r);
        return json_number(0);
    }
    return r;
}

/* PoP: rs2_api_server_hook @ gateway/run_shutdown.py:_api_server_hook */
static json_t *rs2_api_server_hook(const json_t *self, const char *name, const json_t *args,
                                   const rs2_seams_t *s) {
    const json_t *adapters = (self && self->type == JSON_OBJECT) ? json_obj_get(self, "adapters") : NULL;
    const json_t *adapter = NULL;
    if (!adapters) {
        adapter = NULL;   /* getattr(self,"adapters",{}).get(...) -> None */
    } else if (adapters->type == JSON_NULL) {
        return rs2_raise("AttributeError", "'NoneType' object has no attribute 'get'");
    } else if (adapters->type == JSON_OBJECT) {
        adapter = json_obj_get(adapters, "api_server");
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "'%s' object has no attribute 'get'", rs2_typename(adapters));
        return rs2_raise("AttributeError", buf);
    }
    bool callable = adapter && adapter->type == JSON_OBJECT && json_obj_get(adapter, name) != NULL;
    if (!callable) return json_number(0);
    json_t *a = rs2_args();
    rs2_s(a, "name", name);
    json_set(a, "args", args ? json_copy(args) : json_array());
    json_t *r = rs2_call(s, "api_server_hook", a);
    if (rs2_raised(r)) return r;
    const char *kind = NULL; char *msg = NULL;
    long long iv = rs2_py_int(r, &kind, &msg);
    if (kind) {
        json_free(r);
        json_t *e = rs2_raise(kind, msg);
        free(msg);
        return e;
    }
    json_free(r);
    return json_number((double)(iv > 0 ? iv : 0));
}

/* PoP: rs2_active_api_run_count @ gateway/run_shutdown.py:_active_api_run_count */
static json_t *rs2_active_api_run_count(const json_t *self, const rs2_seams_t *s) {
    json_t *r = rs2_api_server_hook(self, "active_agent_work_count", NULL, s);
    if (rs2_raised(r)) {
        if (rs2_is_base_exc(rs2_kind(r))) return r;
        json_free(r);
        return json_number(0);
    }
    return r;
}

/* PoP: rs2_active_api_worker_count @ gateway/run_shutdown.py:_active_api_worker_count */
static json_t *rs2_active_api_worker_count(const json_t *self, const rs2_seams_t *s) {
    (void)self;
    json_t *r = rs2_call(s, "api_worker_live_count", rs2_args());
    if (rs2_raised(r)) {
        if (rs2_is_base_exc(rs2_kind(r))) return r;
        json_free(r);
        return json_number(0);
    }
    const char *kind = NULL; char *msg = NULL;
    long long iv = rs2_py_int(r, &kind, &msg);
    if (kind) {   /* int() failure is an Exception -> swallowed to 0 */
        json_free(r);
        free(msg);
        return json_number(0);
    }
    json_free(r);
    return json_number((double)(iv > 0 ? iv : 0));
}

/* PoP: rs2_mark_api_runs_shutdown_requested @ gateway/run_shutdown.py:_mark_api_runs_shutdown_requested */
static json_t *rs2_mark_api_runs_shutdown_requested(const json_t *self, const rs2_seams_t *s) {
    json_t *r = rs2_api_server_hook(self, "mark_shutdown_requested", NULL, s);
    if (rs2_raised(r)) {
        if (rs2_is_base_exc(rs2_kind(r))) return r;
        json_t *la = json_array();
        json_append(la, json_copy(r));   /* exc as last %s arg */
        json_t *lr = rs2_log(s, "debug",
                             "Failed marking api_server runs as shutdown-requested: %s", la);
        json_free(r);
        if (rs2_raised(lr)) return lr;
        json_free(lr);
        return json_number(0);
    }
    return r;
}
/* acc = acc + op (frees acc and op). On a raise (op or the add) returns the spec
 * and sets *acc=NULL; otherwise stores the sum and returns NULL. The op argument
 * is evaluated by the caller BEFORE this runs, preserving left-to-right order. */
static json_t *rs2_acc_add(json_t **acc, json_t *op) {
    if (rs2_raised(op)) { json_free(*acc); *acc = NULL; return op; }
    json_t *sum = rs2_add(*acc, op);
    json_free(*acc); json_free(op);
    if (rs2_raised(sum)) { *acc = NULL; return sum; }
    *acc = sum;
    return NULL;
}

/* PoP: rs2_active_work_count @ gateway/run_shutdown.py:_active_work_count */
static json_t *rs2_active_work_count(const json_t *self, const rs2_seams_t *s) {
    json_t *acc = rs2_call(s, "running_agent_count", rs2_args());
    if (rs2_raised(acc)) return acc;
    json_t *e;
    e = rs2_acc_add(&acc, rs2_call(s, "managed_turn_count", rs2_args())); if (e) return e;
    e = rs2_acc_add(&acc, rs2_active_cron_job_count(self, s)); if (e) return e;
    e = rs2_acc_add(&acc, rs2_active_api_run_count(self, s)); if (e) return e;
    e = rs2_acc_add(&acc, rs2_call(s, "active_deferred_agent_worker_count", rs2_args())); if (e) return e;
    e = rs2_acc_add(&acc, rs2_active_async_delegation_count(s)); if (e) return e;
    return acc;
}

/* PoP: rs2_drain_work_counts @ gateway/run_shutdown.py:_drain_work_counts */
static json_t *rs2_drain_work_counts(const json_t *self, const rs2_seams_t *s) {
    json_t *arr = json_array();
    json_t *acc = rs2_call(s, "running_agent_count", rs2_args());
    if (rs2_raised(acc)) return acc;
    json_t *e = rs2_acc_add(&acc, rs2_call(s, "managed_turn_count", rs2_args()));
    if (e) { json_free(arr); return e; }
    json_append(arr, acc);
    json_t *op = rs2_active_cron_job_count(self, s);
    if (rs2_raised(op)) { json_free(arr); return op; }
    json_append(arr, op);
    op = rs2_active_api_run_count(self, s);
    if (rs2_raised(op)) { json_free(arr); return op; }
    json_append(arr, op);
    op = rs2_call(s, "active_deferred_agent_worker_count", rs2_args());
    if (rs2_raised(op)) { json_free(arr); return op; }
    json_append(arr, op);
    op = rs2_active_async_delegation_count(s);
    if (rs2_raised(op)) { json_free(arr); return op; }
    json_append(arr, op);
    return arr;
}

/* PoP: rs2_restart_wait_cron_counts @ gateway/run_shutdown.py:_restart_wait_cron_counts */
static json_t *rs2_restart_wait_cron_counts(const json_t *self, const rs2_seams_t *s) {
    json_t *r = rs2_call(s, "cron.get_restart_wait_cron_counts", rs2_args());
    if (rs2_raised(r)) {
        if (rs2_is_base_exc(rs2_kind(r))) return r;
        json_free(r);
        json_t *aw = rs2_active_cron_job_count(self, s);
        if (rs2_raised(aw)) return aw;
        json_t *fb = json_object();
        json_set(fb, "awaitable", aw);
        json_set(fb, "wedged", json_number(0));
        json_set(fb, "restart_safe", json_number(0));
        return fb;
    }
    return r;
}

/* PoP: rs2_wedged_cron_job_count @ gateway/run_shutdown.py:_wedged_cron_job_count */
static json_t *rs2_wedged_cron_job_count(const json_t *self, const rs2_seams_t *s) {
    json_t *c = rs2_restart_wait_cron_counts(self, s);
    if (rs2_raised(c)) return c;
    json_t *v = rs2_subscript(c, "wedged");
    json_free(c);
    return v;
}

/* PoP: rs2_restart_safe_cron_count @ gateway/run_shutdown.py:_restart_safe_cron_count */
static json_t *rs2_restart_safe_cron_count(const json_t *self, const rs2_seams_t *s) {
    json_t *c = rs2_restart_wait_cron_counts(self, s);
    if (rs2_raised(c)) return c;
    json_t *v = rs2_subscript(c, "restart_safe");
    json_free(c);
    return v;
}

/* PoP: rs2_wedged_agent_count @ gateway/run_shutdown.py:_wedged_agent_count */
static json_t *rs2_wedged_agent_count(const json_t *self, const rs2_seams_t *s) {
    json_t *a = rs2_call(s, "wedged_chat_agent_count", rs2_args());
    if (rs2_raised(a)) return a;
    json_t *b = rs2_wedged_cron_job_count(self, s);
    if (rs2_raised(b)) { json_free(a); return b; }
    json_t *sum = rs2_add(a, b);
    json_free(a); json_free(b);
    return sum;
}

/* PoP: rs2_awaitable_work_count @ gateway/run_shutdown.py:_awaitable_work_count */
static json_t *rs2_awaitable_work_count(const json_t *self, const rs2_seams_t *s) {
    json_t *acc = rs2_call(s, "running_agent_count", rs2_args());
    if (rs2_raised(acc)) return acc;
    json_t *e;
    e = rs2_acc_add(&acc, rs2_call(s, "managed_turn_count", rs2_args())); if (e) return e;
    e = rs2_acc_add(&acc, rs2_active_api_run_count(self, s)); if (e) return e;
    e = rs2_acc_add(&acc, rs2_call(s, "active_deferred_agent_worker_count", rs2_args())); if (e) return e;
    json_t *w = rs2_call(s, "wedged_chat_agent_count", rs2_args());
    if (rs2_raised(w)) { json_free(acc); return w; }
    json_t *diff = rs2_sub(acc, w);
    json_free(acc); json_free(w);
    if (rs2_raised(diff)) return diff;
    json_t *zero = json_number(0);
    json_t *mx = rs2_max_num(zero, diff);
    json_free(zero); json_free(diff);
    if (rs2_raised(mx)) return mx;
    acc = mx;
    e = rs2_acc_add(&acc, rs2_active_async_delegation_count(s)); if (e) return e;
    json_t *c = rs2_restart_wait_cron_counts(self, s);
    if (rs2_raised(c)) { json_free(acc); return c; }
    json_t *aw = rs2_subscript(c, "awaitable");
    json_free(c);
    if (rs2_raised(aw)) { json_free(acc); return aw; }
    e = rs2_acc_add(&acc, aw); if (e) return e;
    return acc;
}

/* PoP: rs2_gateway_cfg_section @ gateway/run_shutdown.py:_gateway_cfg_section */
static json_t *rs2_gateway_cfg_section(const char *name, const rs2_seams_t *s) {
    json_t *uc = rs2_call(s, "_load_gateway_config", rs2_args());
    if (rs2_raised(uc)) {
        if (rs2_is_base_exc(rs2_kind(uc))) return uc;
        json_free(uc);
        return NULL;
    }
    const json_t *gw = (uc && uc->type == JSON_OBJECT) ? json_obj_get(uc, "gateway") : NULL;
    const json_t *section = (gw && gw->type == JSON_OBJECT) ? json_obj_get(gw, name) : NULL;
    json_t *res = (section && section->type == JSON_OBJECT) ? json_copy(section) : NULL;
    json_free(uc);
    return res;
}

/* PoP: rs2_scale_to_zero_idle_timeout_seconds @ gateway/run_shutdown.py:_scale_to_zero_idle_timeout_seconds */
static json_t *rs2_scale_to_zero_idle_timeout_seconds(const json_t *self, const rs2_seams_t *s) {
    (void)self;
    json_t *stz = rs2_gateway_cfg_section("scale_to_zero", s);
    if (rs2_raised(stz)) return stz;
    const json_t *value = rs2_truth(stz) ? rs2_oget(stz, "idle_timeout_minutes") : NULL;
    json_t *a = rs2_args();
    rs2_j(a, "value", value);
    json_free(stz);
    return rs2_call(s, "parse_idle_timeout_seconds", a);
}

/* PoP: rs2_relay_wake_url_or_none @ gateway/run_shutdown.py:_relay_wake_url_or_none */
static json_t *rs2_relay_wake_url_or_none(const rs2_seams_t *s) {
    json_t *r = rs2_call(s, "relay_wake_url", rs2_args());
    if (rs2_raised(r)) {
        if (rs2_is_base_exc(rs2_kind(r))) return r;
        json_free(r);
        return NULL;
    }
    return r;
}

/* PoP: rs2_scale_to_zero_hold_redial @ gateway/run_shutdown.py:_scale_to_zero_hold_redial */
static json_t *rs2_scale_to_zero_hold_redial(const json_t *self, bool held, const rs2_seams_t *s) {
    (void)self;
    json_t *adapter = rs2_call(s, "relay_adapter_for_dormancy", rs2_args());
    if (rs2_raised(adapter)) {
        if (rs2_is_base_exc(rs2_kind(adapter))) return adapter;
        json_t *lr = rs2_log_exc(s, "debug", "scale-to-zero: redial hold toggle failed", true);
        json_free(adapter);
        if (rs2_raised(lr)) return lr;
        json_free(lr);
        return json_bool(false);
    }
    if (rs2_is_null(adapter)) { json_free(adapter); return json_bool(false); }
    const char *mname = held ? "hold_redial" : "release_redial";
    bool callable = adapter->type == JSON_OBJECT && json_obj_get(adapter, mname) != NULL;
    json_free(adapter);
    if (!callable) return json_bool(false);
    char seamname[40];
    snprintf(seamname, sizeof(seamname), "adapter.%s", mname);
    json_t *r = rs2_call(s, seamname, rs2_args());
    if (rs2_raised(r)) {
        if (rs2_is_base_exc(rs2_kind(r))) return r;
        json_t *lr = rs2_log_exc(s, "debug", "scale-to-zero: redial hold toggle failed", true);
        json_free(r);
        if (rs2_raised(lr)) return lr;
        json_free(lr);
        return json_bool(false);
    }
    bool took = r && r->type == JSON_BOOL && r->bool_val;   /* method() is True */
    json_free(r);
    return json_bool(took);
}

/* PoP: rs2_scale_to_zero_abandon_suspend @ gateway/run_shutdown.py:_scale_to_zero_abandon_suspend */
static json_t *rs2_scale_to_zero_abandon_suspend(json_t *self, const rs2_seams_t *s) {
    json_t *hr = rs2_scale_to_zero_hold_redial(self, false, s);
    if (rs2_raised(hr)) return hr;
    json_free(hr);
    const json_t *draining = rs2_oget(self, "_draining");
    bool enter = false;
    if (!rs2_truth(draining)) {
        const json_t *running = rs2_oget(self, "_running");
        if (!running) return rs2_raise("AttributeError", "missing attribute '_running'");
        enter = rs2_truth(running);
    }
    if (enter) {
        json_t *ss = rs2_call(s, "serving_state", rs2_args());
        if (rs2_raised(ss)) return ss;
        json_t *a = rs2_args();
        rs2_j(a, "state", ss);
        rs2_s(a, "fail_msg", "scale-to-zero: status restore failed");
        json_free(ss);
        json_t *st = rs2_call(s, "scale_to_zero_status", a);
        if (rs2_raised(st)) return st;
        json_free(st);
    }
    const json_t *cur = rs2_oget(self, "_scale_to_zero_cooldown_until");
    if (!cur) return rs2_raise("AttributeError", "missing attribute '_scale_to_zero_cooldown_until'");
    json_t *tt = rs2_call(s, "time.time", rs2_args());
    if (rs2_raised(tt)) return tt;
    json_t *sixty = json_number(60.0);
    json_t *sum = rs2_add(tt, sixty);
    json_free(tt); json_free(sixty);
    if (rs2_raised(sum)) return sum;
    json_t *mx = rs2_max_num(cur, sum);
    json_free(sum);
    if (rs2_raised(mx)) return mx;
    json_set(self, "_scale_to_zero_cooldown_until", mx);
    return NULL;
}