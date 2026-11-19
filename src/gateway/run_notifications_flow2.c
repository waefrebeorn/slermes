/*
 * run_notifications_flow2.c — gateway/run_notifications.py UNCLAIMED-remainder
 * FLOW cores (universal-seam convention, sibling of run_notifications_flow.c /
 * run_shutdown_flow2.c).
 *
 * Ports the DECISION SEQUENCES of the process-watcher output/message builders,
 * the secondary-ledger replay cluster, the completion classification/preflight
 * chain, the injection-adapter resolver, the api_server wake-profile probe and
 * the home-channel fan-out generators: call order, short-circuit branches,
 * early returns, exception->fallback paths and mutation order of the live
 * Python. Every live I/O (redactors, transport/identity probes, durable-claim
 * ops, scope binding, adapter sends, clocks, logging) enters through one
 * universal seam rn2_seams_t.call(ctx, name, args); args is OWNED by rn2_call
 * (freed after the borrowed call); results are NEW json_t with NULL == Python
 * None and {"__raise__":K,"msg":M} == a raised exception. BaseException kinds
 * (SystemExit/KeyboardInterrupt/CancelledError/GeneratorExit) pass through every
 * `except Exception` / `_log_suppressed` site.
 *
 * Self-contained: libjson only (no cross-object symbols). All helpers static rn2_*.
 *
 * Seam names (the oracle installs recording stubs under the SAME names):
 *   self.<name>            — a claimed/foreign SELF method (instance attribute;
 *                            no self argument is recorded). Claimed siblings
 *                            (_build_process_event_source, _settle_durable_claim)
 *                            and foreign collaborators (_transport_owner,
 *                            _delivery_adapter_for, _restored_transport_trusted,
 *                            _adapters_for_profile, _routed_primary_adapter,
 *                            _committed_relay_adapter, _lookup_completion_owner,
 *                            _closed_local_target,
 *                            _resolve_compression_lineage_target) are SEAMED.
 *   <live module fn name>  — module-level claimed/foreign functions, verbatim
 *                            (_raw_process_event_session_id, _held_reason,
 *                            _safe_delivery_transport, _redact_gateway_user_facing_secrets,
 *                            _format_concise_process_notification,
 *                            _shorten_command_for_display, _non_conversational_metadata,
 *                            t, strip_ansi, transform_process_output,
 *                            redact_terminal_output, resolve_delivery_transport,
 *                            detached_route_of, identity_of, session_owned_by_profile,
 *                            _multiplex_profile_homes, claim_completion_delivery,
 *                            restore_undelivered_completions, sweep_orphaned_completions,
 *                            adapter_supports_push).
 *   adapter.send / adapter.ensure_store — adapter hook calls.
 *   time.time / uuid4_hex / self_id     — clock, uuid4().hex, id(self).
 *   profile_scope_enter / profile_scope_exit — _profile_runtime_scope CM.
 *   ledger.fn              — the generic fn() callback of _each_secondary_ledger.
 *   session_store.ensure_loaded — asyncio.to_thread(session_store._ensure_loaded).
 *   logger.debug/info/warning/error/log — recording logger (raw fmt + args).
 * asyncio.to_thread(fn, ...) is modeled as the fn seam itself (the thread
 * dispatch is transparent); the awaited result is the seam result.
 *
 * Modeling notes (differential oracle tests/oracle_run_notifications_flow2.py):
 *  - Runner state (self) is injected json_t mutated in place. Platform keys are
 *    the enum .value strings. Adapter doubles are objects whose PRESENT hook
 *    keys model callable methods (send / _ensure_store); data fields
 *    (_active_sessions) are read directly. Source/identity/config doubles are
 *    objects read via getattr semantics (absent key -> the getattr default).
 *  - Ported siblings run inline (real) on both sides: _redacted_output_tail ->
 *    _format_process_final_message / _format_process_running_message;
 *    _resolve_injection_adapter -> _launching_turn_active / _send_watcher_message /
 *    _completion_delivery_ready; _classify_completion_owner ->
 *    _classify_completion_target / _preflight_completion_delivery;
 *    _completion_delivery_ready -> _preflight_completion_delivery;
 *    _each_secondary_ledger -> _restore_secondary_completion_ledgers /
 *    _sweep_orphaned_completion_ledgers; _home_channel_transports ->
 *    _served_home_channel_transports.
 *  - Platform(platform_name) is modeled over the 22 BUILTIN value strings; an
 *    unknown name raises ValueError (caught -> transport None). Dynamic plugin
 *    pseudo-members (_missing_ filesystem/registry scan) are OUTSIDE the fixture
 *    domain.
 *  - Integral floats == ints (libjson has no int/float identity): fixtures keep
 *    str()-computed positions integral-int or non-integral. isinstance(x,(int,
 *    float)) admits bool (a subclass of int). `is True` is JSON_BOOL-true only.
 *  - Ordered collections (config.platforms / adapters / _profile_configs /
 *    _entries / _multiplex_profile_homes) iterate in JSON insertion order ==
 *    dict order. The _completion_no_transport_warned SET echoes as a sorted
 *    pair list (null profile sorts first); membership is order-insensitive.
 *  - str()/repr() fixture domain: strings, integral numbers, bools, None.
 *    LookupError repr(hinted) picks the quote to minimise escapes. Invalid
 *    int()/repr tokens are printable ASCII (no quotes/backslashes).
 *  - Non-ASCII message constants (em dash, ellipsis) are hex-escaped literals;
 *    fixture INPUTS are ASCII.
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
#include "libjson/json.h"

typedef struct rn2_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} rn2_seams_t;

/* ── universal-seam helpers ── */
static json_t *rn2_call(const rn2_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    if (!r && !(s && s->call)) {
        /* no seam installed: fail closed */
        json_t *e = json_object();
        json_set(e, "__raise__", json_string("RuntimeError"));
        json_set(e, "msg", json_string("seam not installed"));
        return e;
    }
    return r;
}
static bool rn2_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *rn2_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static const char *rn2_msg(const json_t *r) {
    const json_t *k = json_obj_get(r, "msg");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static json_t *rn2_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool rn2_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static json_t *rn2_args(void) { return json_object(); }
static void rn2_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void rn2_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void rn2_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void rn2_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *rn2_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* ── Python value helpers ── */
static bool rn2_truth(const json_t *v) {
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
static bool rn2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool rn2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool rn2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static const char *rn2_strval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *rn2_typename(const json_t *v) {
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
static bool rn2_isnum(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}
static double rn2_numval(const json_t *v) {
    return (v->type == JSON_BOOL) ? (v->bool_val ? 1.0 : 0.0) : v->num_val;
}
/* str() of a scalar; containers serialize (outside the str()-position domain). */
static char *rn2_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isinf(d) && !isnan(d) && d == (double)(long long)d && d < 9.2e18 && d > -9.2e18) {
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
            return strdup(buf);
        }
        snprintf(buf, sizeof(buf), "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    char *s = json_serialize(v);
    return s ? s : strdup("");
}
/* str(x or "") — falsy collapses to the empty string, NOT str(None). */
static char *rn2_str_or_empty(const json_t *v) {
    if (!rn2_truth(v)) return strdup("");
    return rn2_pystr(v);
}
/* Python repr for the fixture string domain (quote pick + escapes). */
static char *rn2_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return rn2_pystr(v);
    if (v->type != JSON_STRING) { char *s = json_serialize(v); return s ? s : strdup(""); }
    const char *s = v->str_val ? v->str_val : "";
    bool has_sq = strchr(s, '\'') != NULL, has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3, len = 0;
    char *r = malloc(cap);
    r[len++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\') { r[len++] = '\\'; r[len++] = '\\'; }
        else if (c == (unsigned char)q) { r[len++] = '\\'; r[len++] = (char)c; }
        else if (c == '\n') { r[len++] = '\\'; r[len++] = 'n'; }
        else if (c == '\r') { r[len++] = '\\'; r[len++] = 'r'; }
        else if (c == '\t') { r[len++] = '\\'; r[len++] = 't'; }
        else if (c < 0x20 || c == 0x7F) len += (size_t)snprintf(r + len, cap - len, "\\x%02x", c);
        else r[len++] = (char)c;
    }
    r[len++] = q;
    r[len] = '\0';
    return r;
}
/* UTF-8 White_Space set used by str.strip()/rstrip() (no argument). */
static bool rn2_is_ws(unsigned int cp) {
    return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20 || cp == 0x85 || cp == 0xA0
           || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)
           || cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F
           || cp == 0x3000;
}
static size_t rn2_cp_len(const char *s, size_t len, size_t i, unsigned int *cp) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { *cp = c; return 1; }
    size_t n = 0; unsigned int v = 0;
    if ((c & 0xE0) == 0xC0) { n = 2; v = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { n = 3; v = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { n = 4; v = c & 0x07; }
    else { *cp = c; return 1; }
    if (i + n > len) { *cp = c; return 1; }
    for (size_t k = 1; k < n; k++) {
        unsigned char cc = (unsigned char)s[i + k];
        if ((cc & 0xC0) != 0x80) { *cp = c; return 1; }
        v = (v << 6) | (cc & 0x3F);
    }
    *cp = v;
    return n;
}
static char *rn2_strip(const char *s) {
    if (!s) s = "";
    size_t len = strlen(s), a = 0, b = len;
    while (a < b) {
        unsigned int cp; size_t n = rn2_cp_len(s, b, a, &cp);
        if (!rn2_is_ws(cp)) break;
        a += n;
    }
    while (b > a) {
        size_t i = b - 1;
        while (i > a && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
        unsigned int cp; size_t n = rn2_cp_len(s, b, i, &cp);
        if (!rn2_is_ws(cp) || i + n != b) break;
        b = i;
    }
    char *r = malloc(b - a + 1);
    memcpy(r, s + a, b - a);
    r[b - a] = '\0';
    return r;
}
static char *rn2_lower(const char *s) {
    char *r = strdup(s ? s : "");
    for (char *p = r; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    return r;
}
/* codepoint count of a UTF-8 string. */
static size_t rn2_cplen(const char *s) {
    size_t n = 0;
    for (; *s; s++) if ((*s & 0xC0) != 0x80) n++;
    return n;
}
/* s[-n:] by CODEPOINTS; n == 0 keeps the WHOLE string (Python -0 == 0). */
static char *rn2_tail(const char *s, size_t n) {
    size_t len = rn2_cplen(s);
    if (n == 0 || n >= len) return strdup(s);
    size_t skip = len - n, i = 0, seen = 0;
    while (s[i] && seen < skip) {
        unsigned int cp; size_t k = rn2_cp_len(s, strlen(s), i, &cp);
        i += k; seen++;
    }
    return strdup(s + i);
}
/* codepoint index of the first '\n' (ASCII boundary), or -1. */
static long long rn2_find_nl(const char *s) {
    size_t i = 0, idx = 0;
    while (s[i]) {
        if (s[i] == '\n') return (long long)idx;
        unsigned int cp; size_t k = rn2_cp_len(s, strlen(s), i, &cp);
        i += k; idx++;
    }
    return -1;
}
/* s[start:] by CODEPOINT index (start may exceed len -> ""). */
static char *rn2_slice_from(const char *s, size_t start) {
    size_t i = 0, seen = 0;
    while (s[i] && seen < start) {
        unsigned int cp; size_t k = rn2_cp_len(s, strlen(s), i, &cp);
        i += k; seen++;
    }
    return strdup(s + i);
}
/* a - b for numeric operands; non-numeric -> the live operand-type TypeError. */
static json_t *rn2_sub(const json_t *a, const json_t *b) {
    if (!rn2_isnum(a) || !rn2_isnum(b)) {
        char msg[160];
        snprintf(msg, sizeof(msg), "unsupported operand type(s) for -: '%s' and '%s'",
                 rn2_typename(a), rn2_typename(b));
        return rn2_raise("TypeError", msg);
    }
    return json_number(rn2_numval(a) - rn2_numval(b));
}
/* max(0.0, x) over the numeric domain. */
static json_t *rn2_max0(const json_t *x) {
    if (!rn2_isnum(x)) {
        char msg[160];
        snprintf(msg, sizeof(msg), "'>' not supported between instances of '%s' and 'float'",
                 rn2_typename(x));
        return rn2_raise("TypeError", msg);
    }
    double d = rn2_numval(x);
    return json_number(d >= 0.0 ? d : 0.0);
}
/* owner[0] where owner is materialized as a TUPLE. */
static json_t *rn2_index0(const json_t *arr) {
    if (arr && arr->type == JSON_ARRAY) {
        if (json_len(arr) == 0) return rn2_raise("IndexError", "tuple index out of range");
        return json_copy(json_get(arr, 0));
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "'%s' object is not subscriptable", rn2_typename(arr));
    return rn2_raise("TypeError", buf);
}
/* Deep JSON equality with Python numeric cross-type semantics (True == 1). */
static bool rn2_eq(const json_t *a, const json_t *b) {
    bool an = rn2_is_null(a), bn = rn2_is_null(b);
    if (an || bn) return an && bn;
    if (rn2_isnum(a) && rn2_isnum(b)) return rn2_numval(a) == rn2_numval(b);
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_STRING:
            return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!rn2_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !rn2_eq(a->c.items[i], bv)) return false;
            }
            return true;
        }
        default: return false;
    }
}
/* ── logging seams (RecLogger twin) ──
 * logger.<level>(fmt, *args, exc_info=?): {fmt, args?, exc_info?};
 * logger.log(level, fmt, *args, exc_info=?): {fmt, level, args?, exc_info?}.
 * Empty positional args omit "args"; exc_info only when true. A BaseException
 * positional arg projects to its {"__raise__","msg"} spec (the caught raise). */
static json_t *rn2_log_named(const rn2_seams_t *s, const char *name, const char *fmt,
                             json_t *args_arr, bool exc_info) {
    json_t *a = rn2_args();
    rn2_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    if (exc_info) rn2_b(a, "exc_info", true);
    return rn2_call(s, name, a);
}
static json_t *rn2_log(const rn2_seams_t *s, const char *level, const char *fmt,
                       json_t *args_arr, bool exc_info) {
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    return rn2_log_named(s, name, fmt, args_arr, exc_info);
}
static json_t *rn2_log_log(const rn2_seams_t *s, int level, const char *fmt,
                           json_t *args_arr, bool exc_info) {
    json_t *a = rn2_args();
    rn2_s(a, "fmt", fmt);
    rn2_n(a, "level", (double)level);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    if (exc_info) rn2_b(a, "exc_info", true);
    return rn2_call(s, "logger.log", a);
}
/* getattr(node, key, "") or "" -> malloc'd string ("" when absent/falsy). */
static char *rn2_getattr_str_or_empty(const json_t *node, const char *key) {
    return rn2_str_or_empty(rn2_oget(node, key));
}
/* malloc'd concatenation of two C strings. */
static char *rn2_concat(const char *a, const char *b) {
    size_t n = strlen(a) + strlen(b) + 1;
    char *r = malloc(n);
    snprintf(r, n, "%s%s", a, b);
    return r;
}

/* ── Platform builtin value table (dynamic pseudo-members out of domain) ── */
static const char *rn2_platform_values[] = {
    "local", "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
    "signal", "mattermost", "matrix", "email", "sms", "dingtalk", "api_server",
    "webhook", "msgraph_webhook", "feishu", "wecom", "wecom_callback", "weixin",
    "bluebubbles", "qqbot", "relay",
};
static bool rn2_platform_valid(const char *v) {
    if (!v) return false;
    for (size_t i = 0; i < sizeof(rn2_platform_values) / sizeof(rn2_platform_values[0]); i++)
        if (strcmp(v, rn2_platform_values[i]) == 0) return true;
    return false;
}
/* source.platform projection: object with "value" -> value; else str(platform). */
static char *rn2_platform_of(const json_t *source) {
    const json_t *p = rn2_oget(source, "platform");
    const json_t *pv = rn2_oget(p, "value");
    if (pv) return rn2_pystr(pv);
    return rn2_pystr(p);
}

/* ── exported flow cores (forward declarations) ── */
static json_t *rn2_redacted_output_tail(const json_t *self, const json_t *session,
                                        long long limit, const rn2_seams_t *s);
static json_t *rn2_build_process_completion_event(const json_t *watcher, const json_t *session,
                                                  const char *session_id, const rn2_seams_t *s);
static json_t *rn2_format_process_final_message(const json_t *self, const char *session_id,
                                                const json_t *session, const char *notify_mode,
                                                const rn2_seams_t *s);
static json_t *rn2_format_process_running_message(const json_t *self, const json_t *session,
                                                  const rn2_seams_t *s);
static json_t *rn2_format_coalesced_process_completions(const json_t *entries, const rn2_seams_t *s);
static json_t *rn2_launching_turn_active(const json_t *self, const char *platform_name,
                                         const json_t *watcher, const rn2_seams_t *s);
static json_t *rn2_send_watcher_message(const json_t *self, const char *platform_name,
                                        const json_t *chat_id, const json_t *thread_id,
                                        const char *message_text, const json_t *watcher,
                                        const rn2_seams_t *s);
static json_t *rn2_each_secondary_ledger(const json_t *self, const json_t *profile_homes,
                                         const char *verb, const char *fn_seam,
                                         const json_t *fn_args, const rn2_seams_t *s);
static json_t *rn2_restore_secondary_completion_ledgers(const json_t *self,
                                                        const json_t *profile_homes,
                                                        const rn2_seams_t *s);
static json_t *rn2_sweep_orphaned_completion_ledgers(const json_t *self, const rn2_seams_t *s);
static json_t *rn2_classify_completion_target(const json_t *self, const char *parent_session_id,
                                              const char *session_key, const rn2_seams_t *s);
static json_t *rn2_classify_completion_owner(const json_t *self, const char *parent_session_id,
                                             const char *session_key, const rn2_seams_t *s);
static json_t *rn2_completion_delivery_ready(const json_t *self, json_t *evt, const rn2_seams_t *s);
static json_t *rn2_preflight_completion_delivery(const json_t *self, json_t *evt, const rn2_seams_t *s);
static json_t *rn2_resolve_injection_adapter(const json_t *self, const char *platform_name,
                                             const json_t *source, const rn2_seams_t *s);
static json_t *rn2_served_api_server_wake_profile(const json_t *self, const json_t *evt,
                                                  const char *raw_sid, const rn2_seams_t *s);
static json_t *rn2_adapter_by_platform_value(const json_t *self, const char *platform_name);
static json_t *rn2_home_channel_transports(const json_t *self, const rn2_seams_t *s);
static json_t *rn2_served_home_channel_configs(const json_t *self);
static json_t *rn2_served_home_channel_transports(const json_t *self, const rn2_seams_t *s);

/* ── 1. _redacted_output_tail: secret-redacted process output tail ── */
/* PoP: rn2_redacted_output_tail @ gateway/run_notifications.py:_redacted_output_tail */
static json_t *rn2_redacted_output_tail(const json_t *self, const json_t *session,
                                        long long limit, const rn2_seams_t *s) {
    (void)self;
    const json_t *obuf = rn2_oget(session, "output_buffer");
    char *new_output;
    if (rn2_truth(obuf)) {
        char *tail = rn2_tail(obuf->str_val ? obuf->str_val : "", (size_t)(limit < 0 ? 0 : limit));
        json_t *a = rn2_args(); rn2_s(a, "text", tail); free(tail);
        json_t *r = rn2_call(s, "strip_ansi", a);
        if (rn2_raised(r)) return r;
        new_output = rn2_pystr(r);
        json_free(r);
    } else {
        new_output = strdup("");
    }
    if (new_output[0]) {
        char *cmd = rn2_getattr_str_or_empty(session, "command");
        const json_t *exit_code = rn2_oget(session, "exit_code");
        char *task_id = rn2_getattr_str_or_empty(session, "task_id");
        json_t *a = rn2_args();
        rn2_s(a, "text", new_output);
        rn2_s(a, "command", cmd);
        rn2_j(a, "returncode", exit_code);
        rn2_s(a, "task_id", task_id);
        free(task_id);
        json_t *r = rn2_call(s, "transform_process_output", a);
        if (rn2_raised(r)) { free(new_output); free(cmd); return r; }
        free(new_output);
        new_output = rn2_pystr(r);
        json_free(r);
        a = rn2_args();
        rn2_s(a, "text", new_output);
        rn2_s(a, "command", cmd);
        r = rn2_call(s, "redact_terminal_output", a);
        if (rn2_raised(r)) { free(new_output); free(cmd); return r; }
        free(new_output);
        new_output = rn2_pystr(r);
        json_free(r);
        a = rn2_args();
        rn2_s(a, "text", new_output);
        r = rn2_call(s, "_redact_gateway_user_facing_secrets", a);
        free(cmd);
        if (rn2_raised(r)) { free(new_output); return r; }
        free(new_output);
        new_output = rn2_pystr(r);
        json_free(r);
    }
    json_t *ret = json_string(new_output);
    free(new_output);
    return ret;
}

/* ── 2. _build_process_completion_event: synthetic completion event dict ── */
/* PoP: rn2_build_process_completion_event @ gateway/run_notifications.py:_build_process_completion_event */
static json_t *rn2_build_process_completion_event(const json_t *watcher, const json_t *session,
                                                  const char *session_id, const rn2_seams_t *s) {
    static const char *route_fields[] = {
        "session_key", "platform", "chat_type", "chat_id", "thread_id", "user_id", "user_name"
    };
    char *cmd = rn2_getattr_str_or_empty(session, "command");
    const json_t *obuf = rn2_oget(session, "output_buffer");
    char *raw;
    if (rn2_truth(obuf)) {
        json_t *a = rn2_args(); rn2_s(a, "text", obuf->str_val ? obuf->str_val : "");
        json_t *r = rn2_call(s, "strip_ansi", a);
        if (rn2_raised(r)) { free(cmd); return r; }
        raw = rn2_pystr(r);
        json_free(r);
    } else {
        raw = strdup("");
    }
    if (raw[0]) {
        const json_t *exit_code = rn2_oget(session, "exit_code");
        char *task_id = rn2_getattr_str_or_empty(session, "task_id");
        json_t *a = rn2_args();
        rn2_s(a, "text", raw);
        rn2_s(a, "command", cmd);
        rn2_j(a, "returncode", exit_code);
        rn2_s(a, "task_id", task_id);
        free(task_id);
        json_t *r = rn2_call(s, "transform_process_output", a);
        if (rn2_raised(r)) { free(raw); free(cmd); return r; }
        free(raw);
        raw = rn2_pystr(r);
        json_free(r);
    }
    {
        json_t *a = rn2_args();
        rn2_s(a, "text", raw);
        rn2_s(a, "command", cmd);
        json_t *r = rn2_call(s, "redact_terminal_output", a);
        if (rn2_raised(r)) { free(raw); free(cmd); return r; }
        free(raw);
        raw = rn2_pystr(r);
        json_free(r);
    }
    /* keep the last ~2000 chars snapped to a line boundary, with a marker */
    char *out;
    if (rn2_cplen(raw) > 2000) {
        char *tail = rn2_tail(raw, 2000);
        long long nl = rn2_find_nl(tail);
        char *snapped = (nl != -1) ? rn2_slice_from(tail, (size_t)(nl + 1)) : strdup(tail);
        free(tail);
        size_t tl = rn2_cplen(snapped);
        size_t n = tl + strlen(snapped) + 96;
        out = malloc(n);
        snprintf(out, n, "[\xe2\x80\xa6 output truncated \xe2\x80\x94 showing last %zu chars]\n%s",
                 tl, snapped);
        free(snapped);
    } else {
        out = strdup(raw);
    }
    free(raw);
    /* command redaction fires during dict construction (before output) */
    json_t *ca = rn2_args(); rn2_s(ca, "text", cmd);
    json_t *command = rn2_call(s, "_redact_gateway_user_facing_secrets", ca);
    if (rn2_raised(command)) { free(out); free(cmd); return command; }
    json_t *oa = rn2_args(); rn2_s(oa, "text", out);
    json_t *output = rn2_call(s, "_redact_gateway_user_facing_secrets", oa);
    if (rn2_raised(output)) { free(out); free(cmd); json_free(command); return output; }
    free(out);

    json_t *ret = json_object();
    json_set(ret, "type", json_string("completion"));
    json_set(ret, "session_id", json_string(session_id ? session_id : ""));
    for (size_t i = 0; i < sizeof(route_fields) / sizeof(route_fields[0]); i++) {
        const json_t *v = rn2_oget(watcher, route_fields[i]);
        json_set(ret, route_fields[i], v ? json_copy(v) : json_string(""));
    }
    { char *ot = rn2_getattr_str_or_empty(session, "owner_task_id");
      json_set(ret, "owner_task_id", json_string(ot)); free(ot); }
    { char *tid = rn2_getattr_str_or_empty(session, "task_id");
      json_set(ret, "task_id", json_string(tid)); free(tid); }
    {
        char *mid_s = rn2_str_or_empty(rn2_oget(watcher, "message_id"));
        char *mid = rn2_strip(mid_s);
        free(mid_s);
        json_set(ret, "message_id", mid[0] ? json_string(mid) : json_null());
        free(mid);
    }
    {
        const json_t *st = rn2_oget(session, "started_at");
        json_set(ret, "started_at", st ? json_copy(st) : json_null());
    }
    json_set(ret, "command", command);
    {
        const json_t *ec = rn2_oget(session, "exit_code");
        json_set(ret, "exit_code", ec ? json_copy(ec) : json_null());
    }
    {
        const json_t *cr = rn2_oget(session, "completion_reason");
        json_set(ret, "completion_reason", cr ? json_copy(cr) : json_string("exited"));
    }
    {
        const json_t *ts = rn2_oget(session, "termination_source");
        json_set(ret, "termination_source", ts ? json_copy(ts) : json_string(""));
    }
    json_set(ret, "output", output);
    {
        const json_t *wp = rn2_oget(watcher, "parent_session_id");
        const json_t *sp = rn2_oget(session, "parent_session_id");
        if (rn2_truth(wp)) json_set(ret, "parent_session_id", json_copy(wp));
        else if (rn2_truth(sp)) json_set(ret, "parent_session_id", json_copy(sp));
        else json_set(ret, "parent_session_id", json_string(""));
    }
    free(cmd);
    return ret;
}

/* ── 3. _format_process_final_message: human-facing completion message ── */
/* PoP: rn2_format_process_final_message @ gateway/run_notifications.py:_format_process_final_message */
static json_t *rn2_format_process_final_message(const json_t *self, const char *session_id,
                                                const json_t *session, const char *notify_mode,
                                                const rn2_seams_t *s) {
    json_t *new_output = rn2_redacted_output_tail(self, session, 1000, s);
    if (rn2_raised(new_output)) return new_output;
    const json_t *started = rn2_oget(session, "started_at");
    json_t *dur = NULL;   /* None unless isinstance(started, (int, float)) */
    if (rn2_isnum(started)) {
        json_t *tt = rn2_call(s, "time.time", rn2_args());
        if (rn2_raised(tt)) { json_free(new_output); return tt; }
        json_t *diff = rn2_sub(tt, started);
        json_free(tt);
        if (rn2_raised(diff)) { json_free(new_output); return diff; }
        dur = rn2_max0(diff);
        json_free(diff);
        if (rn2_raised(dur)) { json_free(new_output); return dur; }
    }
    char *cmd_raw = rn2_getattr_str_or_empty(session, "command");
    json_t *ca = rn2_args(); rn2_s(ca, "text", cmd_raw); free(cmd_raw);
    json_t *command = rn2_call(s, "_redact_gateway_user_facing_secrets", ca);
    if (rn2_raised(command)) { json_free(new_output); json_free(dur); return command; }
    const json_t *exit_code = rn2_oget(session, "exit_code");
    const char *mode = notify_mode ? notify_mode : "";
    json_t *ret = NULL;
    if (strcmp(mode, "concise") == 0) {
        json_t *a = rn2_args();
        rn2_s(a, "session_id", session_id);
        rn2_j(a, "command", command);
        rn2_j(a, "exit_code", exit_code);
        rn2_j(a, "output", new_output);
        rn2_j(a, "duration_seconds", dur);
        ret = rn2_call(s, "_format_concise_process_notification", a);
    } else {
        json_t *a = rn2_args();
        rn2_s(a, "session_id", session_id);
        rn2_j(a, "command", command);
        rn2_j(a, "exit_code", exit_code);
        rn2_s(a, "output", "");
        rn2_j(a, "duration_seconds", dur);
        json_t *header = rn2_call(s, "_format_concise_process_notification", a);
        if (rn2_raised(header)) { json_free(new_output); json_free(dur); json_free(command); return header; }
        char *no_s = rn2_pystr(new_output);
        char *stripped = rn2_strip(no_s);
        free(no_s);
        if (stripped[0]) {
            json_t *ta = rn2_args();
            rn2_s(ta, "key", "gateway.background.final_output");
            rn2_j(ta, "header", header);
            rn2_s(ta, "output", stripped);
            ret = rn2_call(s, "t", ta);
        } else {
            ret = header;
            header = NULL;
        }
        free(stripped);
        json_free(header);
    }
    json_free(new_output);
    json_free(dur);
    json_free(command);
    return ret;
}

/* ── 4. _format_process_running_message: still-running status message ── */
/* PoP: rn2_format_process_running_message @ gateway/run_notifications.py:_format_process_running_message */
static json_t *rn2_format_process_running_message(const json_t *self, const json_t *session,
                                                  const rn2_seams_t *s) {
    json_t *new_output = rn2_redacted_output_tail(self, session, 500, s);
    if (rn2_raised(new_output)) return new_output;
    char *cmd_raw = rn2_getattr_str_or_empty(session, "command");
    json_t *ca = rn2_args(); rn2_s(ca, "text", cmd_raw); free(cmd_raw);
    json_t *redacted = rn2_call(s, "_redact_gateway_user_facing_secrets", ca);
    if (rn2_raised(redacted)) { json_free(new_output); return redacted; }
    json_t *sa = rn2_args(); rn2_j(sa, "command", redacted);
    json_t *short_cmd = rn2_call(s, "_shorten_command_for_display", sa);
    json_free(redacted);
    if (rn2_raised(short_cmd)) { json_free(new_output); return short_cmd; }
    json_t *ta = rn2_args(); rn2_s(ta, "key", "gateway.background.still_running");
    json_t *t_head = rn2_call(s, "t", ta);
    if (rn2_raised(t_head)) { json_free(new_output); json_free(short_cmd); return t_head; }
    json_t *header;
    if (rn2_truth(short_cmd)) {
        char *sc = rn2_pystr(short_cmd);
        char *suffix = rn2_concat(" \xe2\x80\x94 `", sc);
        char *suffix2 = rn2_concat(suffix, "`");
        free(suffix); free(sc);
        char *th = rn2_pystr(t_head);
        char *hs = rn2_concat(th, suffix2);
        free(th); free(suffix2);
        header = json_string(hs);
        free(hs);
    } else {
        header = json_copy(t_head);
    }
    json_free(t_head);
    json_free(short_cmd);
    char *no_s = rn2_pystr(new_output);
    char *stripped = rn2_strip(no_s);
    free(no_s);
    json_t *ret;
    if (stripped[0]) {
        json_t *a = rn2_args();
        rn2_s(a, "key", "gateway.background.recent_output");
        rn2_j(a, "header", header);
        rn2_s(a, "output", stripped);
        ret = rn2_call(s, "t", a);
    } else {
        ret = header;
        header = NULL;
    }
    free(stripped);
    json_free(header);
    json_free(new_output);
    return ret;
}

/* ── 5. _format_coalesced_process_completions: bounded batch synthetic text ── */
/* PoP: rn2_format_coalesced_process_completions @ gateway/run_notifications.py:_format_coalesced_process_completions */
static json_t *rn2_format_coalesced_process_completions(const json_t *entries, const rn2_seams_t *s) {
    size_t n = rn2_is_arr(entries) ? json_len(entries) : 0;
    json_t *lines = json_array();
    char buf[160];
    snprintf(buf, sizeof(buf), "[IMPORTANT: %zu background processes completed for this session.", n);
    json_append(lines, json_string(buf));
    json_append(lines, json_string("Treat these results as one completion batch and send at most one "
                                   "consolidated user-facing response."));
    size_t shown = n < 10 ? n : 10;
    for (size_t i = 0; i < shown; i++) {
        const json_t *triple = json_get(entries, i);
        const json_t *evt = rn2_is_arr(triple) ? json_get(triple, 1) : NULL;
        char *session_id = rn2_truth(rn2_oget(evt, "session_id"))
                           ? rn2_pystr(rn2_oget(evt, "session_id")) : strdup("unknown");
        const json_t *exit_code = rn2_oget(evt, "exit_code");
        char *reason = rn2_truth(rn2_oget(evt, "completion_reason"))
                       ? rn2_pystr(rn2_oget(evt, "completion_reason")) : strdup("exited");
        char *out_raw = rn2_str_or_empty(rn2_oget(evt, "output"));
        json_t *a = rn2_args(); rn2_s(a, "text", out_raw); free(out_raw);
        json_t *r = rn2_call(s, "_redact_gateway_user_facing_secrets", a);
        if (rn2_raised(r)) { free(session_id); free(reason); json_free(lines); return r; }
        char *red = rn2_pystr(r);
        json_free(r);
        char *output = rn2_strip(red);
        free(red);
        char *output_owned = output;
        if (rn2_cplen(output) > 800) {
            char *tail = rn2_tail(output, 800);
            size_t nn = strlen(tail) + 48;
            char *marked = malloc(nn);
            snprintf(marked, nn, "[\xe2\x80\xa6 truncated \xe2\x80\xa6]\n%s", tail);
            free(tail); free(output);
            output_owned = marked;
        }
        char *ec = rn2_pystr(exit_code);
        size_t ln = strlen(session_id) + strlen(ec) + strlen(reason) + 64;
        char *line = malloc(ln);
        snprintf(line, ln, "\n- %s: exit_code=%s, reason=%s", session_id, ec, reason);
        json_append(lines, json_string(line));
        free(line); free(ec);
        if (output_owned[0]) json_append(lines, json_string(output_owned));
        free(output_owned);
        free(session_id); free(reason);
    }
    size_t omitted = n - shown;
    if (omitted) {
        snprintf(buf, sizeof(buf), "\n- \xe2\x80\xa6 and %zu more completion(s); inspect them with "
                 "the process tool if they affect the conclusion.", omitted);
        json_append(lines, json_string(buf));
    }
    json_append(lines, json_string("If a result does not change the current conclusion, absorb it silently.]"));
    /* "\n".join(lines) */
    size_t total = 1;
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *ls = rn2_strval(json_get(lines, i));
        total += (ls ? strlen(ls) : 0) + 1;
    }
    char *joined = malloc(total);
    size_t pos = 0;
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *ls = rn2_strval(json_get(lines, i));
        if (i) joined[pos++] = '\n';
        size_t l = ls ? strlen(ls) : 0;
        if (l) { memcpy(joined + pos, ls, l); pos += l; }
    }
    joined[pos] = '\0';
    json_t *ret = json_string(joined);
    free(joined);
    json_free(lines);
    return ret;
}
/* ── 6. _resolve_injection_adapter: alias-aware adapter resolution chain ── */
/* PoP: rn2_resolve_injection_adapter @ gateway/run_notifications.py:_resolve_injection_adapter */
static json_t *rn2_resolve_injection_adapter(const json_t *self, const char *platform_name,
                                             const json_t *source, const rn2_seams_t *s) {
    json_t *a = rn2_args(); rn2_j(a, "source", source);
    json_t *dr = rn2_call(s, "detached_route_of", a);
    if (rn2_raised(dr)) return dr;
    bool detached = rn2_truth(dr);
    json_free(dr);
    if (detached) {
        a = rn2_args(); rn2_j(a, "source", source);
        return rn2_call(s, "self._delivery_adapter_for", a);
    }
    if (!rn2_is_null(source)) {
        a = rn2_args(); rn2_j(a, "source", source);
        json_t *owner = rn2_call(s, "self._transport_owner", a);
        if (rn2_raised(owner)) return owner;
        if (!rn2_is_null(owner)) {
            json_t *r = rn2_index0(owner);
            json_free(owner);
            return r;
        }
        json_free(owner);
        const json_t *dv = rn2_oget(source, "delivered_via_upstream_relay");
        if (dv && dv->type == JSON_BOOL && dv->bool_val) {   /* `is True` identity */
            const json_t *adapters = rn2_oget(self, "adapters");
            if (!adapters) return rn2_raise("AttributeError", "missing attribute 'adapters'");
            if (adapters->type == JSON_NULL)
                return rn2_raise("AttributeError", "'NoneType' object has no attribute 'get'");
            if (adapters->type != JSON_OBJECT) {
                char buf[128];
                snprintf(buf, sizeof(buf), "'%s' object has no attribute 'get'", rn2_typename(adapters));
                return rn2_raise("AttributeError", buf);
            }
            const json_t *relay = json_obj_get(adapters, "relay");
            return relay ? json_copy(relay) : NULL;
        }
    }
    a = rn2_args(); rn2_j(a, "source", source);
    json_t *identity = rn2_call(s, "identity_of", a);
    if (rn2_raised(identity)) return identity;
    bool pinned = false;
    if (!rn2_is_null(identity)) {
        const json_t *mx = rn2_oget(identity, "multiplexed");
        const json_t *ti = rn2_oget(identity, "transport_inferred");
        if (rn2_truth(mx) && !rn2_truth(ti)) {
            a = rn2_args(); rn2_j(a, "source", source); rn2_j(a, "identity", identity);
            json_t *tt = rn2_call(s, "self._restored_transport_trusted", a);
            if (rn2_raised(tt)) { json_free(identity); return tt; }
            pinned = rn2_truth(tt);
            json_free(tt);
        }
    }
    const json_t *prof_arg = pinned ? rn2_oget(identity, "transport_profile")
                                    : rn2_oget(source, "profile");
    a = rn2_args(); rn2_j(a, "profile", prof_arg);
    json_t *adapters = rn2_call(s, "self._adapters_for_profile", a);
    if (rn2_raised(adapters)) { json_free(identity); return adapters; }
    /* try: resolve_delivery_transport(Platform(platform_name), self.config, adapters) */
    json_t *transport = NULL;
    if (rn2_platform_valid(platform_name)) {
        const json_t *config = rn2_oget(self, "config");
        if (config) {   /* absent config -> AttributeError caught -> transport None */
            a = rn2_args();
            rn2_s(a, "platform", platform_name);
            rn2_j(a, "config", config);
            rn2_j(a, "adapters", adapters);
            json_t *tr = rn2_call(s, "resolve_delivery_transport", a);
            if (rn2_raised(tr)) {
                if (rn2_is_base_exc(rn2_kind(tr))) { json_free(identity); json_free(adapters); return tr; }
                json_free(tr);
            } else {
                transport = tr;
            }
        }
    }
    if (!rn2_is_null(transport)) {
        const json_t *ad = rn2_oget(transport, "adapter");
        json_t *ret = ad ? json_copy(ad) : NULL;
        json_free(transport); json_free(identity); json_free(adapters);
        return ret;
    }
    json_free(transport);
    if (!pinned && !rn2_is_null(source)) {
        a = rn2_args(); rn2_j(a, "source", source);
        json_t *borrowed = rn2_call(s, "self._routed_primary_adapter", a);
        if (rn2_raised(borrowed)) { json_free(identity); json_free(adapters); return borrowed; }
        if (!rn2_is_null(borrowed)) { json_free(identity); json_free(adapters); return borrowed; }
        json_free(borrowed);
    }
    if (!rn2_is_null(source)) {
        a = rn2_args(); rn2_j(a, "source", source);
        json_t *relay = rn2_call(s, "self._committed_relay_adapter", a);
        if (rn2_raised(relay)) { json_free(identity); json_free(adapters); return relay; }
        if (!rn2_is_null(relay)) { json_free(identity); json_free(adapters); return relay; }
        json_free(relay);
    }
    json_t *ret = NULL;
    if (rn2_is_obj(adapters)) {
        const json_t *v = json_obj_get(adapters, platform_name ? platform_name : "");
        if (v) ret = json_copy(v);
    }
    json_free(identity); json_free(adapters);
    return ret;
}

/* ── 7. _launching_turn_active: is the launching session still in a turn ── */
/* PoP: rn2_launching_turn_active @ gateway/run_notifications.py:_launching_turn_active */
static json_t *rn2_launching_turn_active(const json_t *self, const char *platform_name,
                                         const json_t *watcher, const rn2_seams_t *s) {
    char *sk_s = rn2_str_or_empty(rn2_oget(watcher, "session_key"));
    char *session_key = rn2_strip(sk_s);
    free(sk_s);
    if (!session_key[0]) { free(session_key); return json_bool(false); }
    json_t *a = rn2_args(); rn2_j(a, "evt", watcher);
    json_t *source = rn2_call(s, "self._build_process_event_source", a);
    if (rn2_raised(source)) { free(session_key); return source; }
    json_t *adapter = rn2_resolve_injection_adapter(self, platform_name, source, s);
    json_free(source);
    if (rn2_raised(adapter)) { free(session_key); return adapter; }
    const json_t *acts = rn2_oget(adapter, "_active_sessions");
    bool member = false;
    if (rn2_truth(acts) && rn2_is_obj(acts)) member = json_has(acts, session_key);
    json_free(adapter);
    free(session_key);
    return json_bool(member);
}

/* ── 8. _send_watcher_message: best-effort watcher status send ── */
/* PoP: rn2_send_watcher_message @ gateway/run_notifications.py:_send_watcher_message */
static json_t *rn2_send_watcher_message(const json_t *self, const char *platform_name,
                                        const json_t *chat_id, const json_t *thread_id,
                                        const char *message_text, const json_t *watcher,
                                        const rn2_seams_t *s) {
    json_t *a = rn2_args(); rn2_j(a, "evt", watcher);
    json_t *source = rn2_call(s, "self._build_process_event_source", a);
    if (rn2_raised(source)) return source;
    json_t *adapter = rn2_resolve_injection_adapter(self, platform_name, source, s);
    json_free(source);
    if (rn2_raised(adapter)) return adapter;
    if (rn2_truth(adapter) && rn2_truth(chat_id)) {
        /* with _log_suppressed(logging.ERROR, "Watcher delivery error: %s"): */
        json_t *send_meta = NULL;
        if (rn2_truth(thread_id)) {
            send_meta = json_object();
            rn2_j(send_meta, "thread_id", thread_id);
        }
        a = rn2_args();
        rn2_j(a, "metadata", send_meta);
        rn2_s(a, "platform", platform_name);
        json_free(send_meta);
        json_t *meta = rn2_call(s, "_non_conversational_metadata", a);
        json_t *body_raise = NULL;
        if (rn2_raised(meta)) {
            body_raise = meta;
        } else {
            a = rn2_args();
            rn2_j(a, "chat_id", chat_id);
            rn2_s(a, "message", message_text);
            rn2_j(a, "metadata", meta);
            json_t *sr = rn2_call(s, "adapter.send", a);
            json_free(meta);
            if (rn2_raised(sr)) body_raise = sr;
            else json_free(sr);
        }
        if (body_raise) {
            if (rn2_is_base_exc(rn2_kind(body_raise))) { json_free(adapter); return body_raise; }
            json_t *la = json_array();
            json_append(la, json_copy(body_raise));
            json_t *lr = rn2_log_log(s, 40, "Watcher delivery error: %s", la, false);
            json_free(body_raise);
            if (rn2_raised(lr)) { json_free(adapter); return lr; }
            json_free(lr);
        }
    }
    json_free(adapter);
    return NULL;
}

/* ── 9. _each_secondary_ledger: run fn() once per SECONDARY profile ── */
/* PoP: rn2_each_secondary_ledger @ gateway/run_notifications.py:_each_secondary_ledger */
static json_t *rn2_each_secondary_ledger(const json_t *self, const json_t *profile_homes,
                                         const char *verb, const char *fn_seam,
                                         const json_t *fn_args, const rn2_seams_t *s) {
    const json_t *primary = rn2_oget(self, "_primary_profile_name");
    size_t n = rn2_is_arr(profile_homes) ? json_len(profile_homes) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *pair = json_get(profile_homes, i);
        const json_t *pn = rn2_is_arr(pair) ? json_get(pair, 0) : NULL;
        const json_t *ph = rn2_is_arr(pair) ? json_get(pair, 1) : NULL;
        if (rn2_eq(pn, primary)) continue;   /* profile_name == primary (None == None) */
        char *home = rn2_pystr(ph);          /* Path(profile_home).as_posix() */
        json_t *caught = NULL;
        json_t *count_val = NULL;
        json_t *a = rn2_args(); rn2_s(a, "home", home);
        json_t *er = rn2_call(s, "profile_scope_enter", a);
        if (rn2_raised(er)) {
            caught = er;                     /* __enter__ raise: no __exit__ */
        } else {
            json_free(er);
            json_t *fa = fn_args ? json_copy(fn_args) : json_object();
            json_t *count = rn2_call(s, fn_seam, fa);
            json_t *body_raise = rn2_raised(count) ? count : NULL;
            if (!body_raise) count_val = count;
            /* __exit__ runs even when the body raised; an exit raise overrides */
            a = rn2_args(); rn2_s(a, "home", home);
            json_t *ex = rn2_call(s, "profile_scope_exit", a);
            if (rn2_raised(ex)) {
                if (body_raise) json_free(body_raise);
                if (count_val) { json_free(count_val); count_val = NULL; }
                caught = ex;
            } else {
                json_free(ex);
                if (body_raise) {
                    if (count_val) { json_free(count_val); count_val = NULL; }
                    caught = body_raise;
                }
            }
        }
        if (caught) {
            if (rn2_is_base_exc(rn2_kind(caught))) { free(home); json_free(count_val); return caught; }
            json_t *la = json_array();
            json_append(la, pn ? json_copy(pn) : json_null());
            json_t *lr = rn2_log(s, "warning",
                                 "Could not replay async completions for profile %r", la, true);
            json_free(caught);
            if (rn2_raised(lr)) { free(home); json_free(count_val); return lr; }
            json_free(lr);
            free(home);
            json_free(count_val);
            continue;
        }
        /* success: `if count:` is OUTSIDE the try (a logger raise propagates) */
        if (rn2_truth(count_val)) {
            json_t *la = json_array();
            json_append(la, json_string(verb ? verb : ""));
            json_append(la, json_copy(count_val));
            json_append(la, pn ? json_copy(pn) : json_null());
            json_t *lr = rn2_log(s, "info",
                                 "%s %d undelivered async completion(s) for profile %r", la, false);
            if (rn2_raised(lr)) { free(home); json_free(count_val); return lr; }
            json_free(lr);
        }
        json_free(count_val);
        free(home);
    }
    return NULL;
}

/* ── 10. _restore_secondary_completion_ledgers: re-queue undelivered rows ── */
/* PoP: rn2_restore_secondary_completion_ledgers @ gateway/run_notifications.py:_restore_secondary_completion_ledgers */
static json_t *rn2_restore_secondary_completion_ledgers(const json_t *self,
                                                        const json_t *profile_homes,
                                                        const rn2_seams_t *s) {
    json_t *fa = rn2_args(); rn2_s(fa, "queue", "completion_queue");
    json_t *r = rn2_each_secondary_ledger(self, profile_homes, "Restored",
                                          "restore_undelivered_completions", fa, s);
    json_free(fa);
    return r;
}

/* ── 11. _sweep_orphaned_completion_ledgers: re-offer dead-owner completions ── */
/* PoP: rn2_sweep_orphaned_completion_ledgers @ gateway/run_notifications.py:_sweep_orphaned_completion_ledgers */
static json_t *rn2_sweep_orphaned_completion_ledgers(const json_t *self, const rn2_seams_t *s) {
    /* with _log_suppressed(logging.DEBUG, "Orphaned async completion sweep failed: %s"):
     *     if count := sweep(): logger.info("Re-offered %d orphaned async completion(s)", count) */
    json_t *fa = rn2_args(); rn2_s(fa, "queue", "completion_queue");
    json_t *count = rn2_call(s, "sweep_orphaned_completions", fa);
    json_t *body_raise = NULL;
    if (rn2_raised(count)) {
        body_raise = count;
    } else {
        if (rn2_truth(count)) {
            json_t *la = json_array();
            json_append(la, json_copy(count));
            json_t *lr = rn2_log(s, "info", "Re-offered %d orphaned async completion(s)", la, false);
            if (rn2_raised(lr)) body_raise = lr;
            else json_free(lr);
        }
        json_free(count);
    }
    if (body_raise) {
        if (rn2_is_base_exc(rn2_kind(body_raise))) return body_raise;
        json_t *la = json_array();
        json_append(la, json_copy(body_raise));
        json_t *lr = rn2_log_log(s, 10, "Orphaned async completion sweep failed: %s", la, false);
        json_free(body_raise);
        if (rn2_raised(lr)) return lr;
        json_free(lr);
    }
    /* self._each_secondary_ledger((getattr(self,"_served_profile_homes",None) or {}).items(), ...) */
    const json_t *sph = rn2_oget(self, "_served_profile_homes");
    json_t *homes = json_array();
    if (rn2_truth(sph) && rn2_is_obj(sph)) {
        for (size_t i = 0; i < sph->c.count; i++) {
            json_t *pair = json_array();
            json_append(pair, json_string(sph->c.keys[i]));
            json_append(pair, json_copy(sph->c.items[i]));
            json_append(homes, pair);
        }
    }
    json_t *fa2 = rn2_args(); rn2_s(fa2, "queue", "completion_queue");
    json_t *r = rn2_each_secondary_ledger(self, homes, "Re-offered orphaned",
                                          "sweep_orphaned_completions", fa2, s);
    json_free(fa2);
    json_free(homes);
    return r;
}
/* ── classification / preflight helpers ── */
static json_t *rn2_ret3(const char *a, const char *b, const char *c) {
    json_t *r = json_array();
    json_append(r, json_string(a ? a : ""));
    json_append(r, json_string(b ? b : ""));
    json_append(r, json_string(c ? c : ""));
    return r;
}
static bool rn2_user_boundary(const char *reason) {
    return strcmp(reason, "session_reset") == 0 || strcmp(reason, "user_exit") == 0
        || strcmp(reason, "session_switch") == 0 || strcmp(reason, "new_session") == 0;
}
/* unpack length guard (py3.14 messages); NULL when the array has exactly n items. */
static json_t *rn2_unpack_check(const json_t *arr, size_t n) {
    if (rn2_is_arr(arr) && json_len(arr) == n) return NULL;
    size_t got = rn2_is_arr(arr) ? json_len(arr) : 0;
    char buf[96];
    if (!rn2_is_arr(arr))
        snprintf(buf, sizeof(buf), "cannot unpack non-iterable %s object", rn2_typename(arr));
    else if (got < n)
        snprintf(buf, sizeof(buf), "not enough values to unpack (expected %zu, got %zu)", n, got);
    else
        snprintf(buf, sizeof(buf), "too many values to unpack (expected %zu, got %zu)", n, got);
    return rn2_raise("ValueError", buf);
}
static char *rn2_repr_str(const char *s) {
    json_t *n = json_string(s ? s : "");
    char *r = rn2_repr(n);
    json_free(n);
    return r;
}
/* warned set = array of [profile, platform] pairs; membership by deep equality. */
static bool rn2_warned_has(const json_t *warned, const json_t *key) {
    if (!rn2_is_arr(warned)) return false;
    for (size_t i = 0; i < json_len(warned); i++)
        if (rn2_eq(json_get(warned, i), key)) return true;
    return false;
}
static void rn2_warned_add(json_t *warned, const json_t *key) {
    if (!rn2_warned_has(warned, key)) json_append(warned, json_copy(key));
}
static void rn2_warned_discard(json_t *warned, const json_t *key) {
    if (!rn2_is_arr(warned) || !key) return;
    for (size_t i = 0; i < json_len(warned); i++) {
        if (rn2_eq(json_get(warned, i), key)) {
            json_free(warned->c.items[i]);
            for (size_t k = i; k + 1 < warned->c.count; k++) warned->c.items[k] = warned->c.items[k + 1];
            warned->c.count--;
            return;
        }
    }
}
/* sort key: null profile first, then profile string, then platform. */
static int rn2_warned_cmp(const void *pa, const void *pb) {
    const json_t *a = *(json_t *const *)pa, *b = *(json_t *const *)pb;
    const json_t *ap = rn2_is_arr(a) ? json_get(a, 0) : NULL;
    const json_t *bp = rn2_is_arr(b) ? json_get(b, 0) : NULL;
    bool an = rn2_is_null(ap), bn = rn2_is_null(bp);
    if (an != bn) return an ? -1 : 1;
    if (!an) {
        int c = strcmp(rn2_strval(ap) ? rn2_strval(ap) : "", rn2_strval(bp) ? rn2_strval(bp) : "");
        if (c) return c;
    }
    const json_t *apl = rn2_is_arr(a) ? json_get(a, 1) : NULL;
    const json_t *bpl = rn2_is_arr(b) ? json_get(b, 1) : NULL;
    return strcmp(rn2_strval(apl) ? rn2_strval(apl) : "", rn2_strval(bpl) ? rn2_strval(bpl) : "");
}

/* ── 12. _classify_completion_owner: verdict + owning/proven session ── */
/* PoP: rn2_classify_completion_owner @ gateway/run_notifications.py:_classify_completion_owner */
static json_t *rn2_classify_completion_owner(const json_t *self, const char *parent_session_id,
                                             const char *session_key, const rn2_seams_t *s) {
    const json_t *store = rn2_oget(self, "persistence");
    if (rn2_is_null(store)) return rn2_ret3("retry", "", "");
    const json_t *entry = NULL;
    char *entry_sid = strdup("");
    const char *psid_in = parent_session_id ? parent_session_id : "";
    if (session_key && session_key[0]) {
        const json_t *ss = rn2_oget(self, "session_store");
        json_t *body_raise = NULL;
        if (!ss) {
            body_raise = rn2_raise("AttributeError", "missing attribute 'session_store'");
        } else {
            json_t *er = rn2_call(s, "session_store.ensure_loaded", rn2_args());
            if (rn2_raised(er)) {
                body_raise = er;
            } else {
                json_free(er);
                const json_t *entries = rn2_oget(ss, "_entries");
                if (!entries) body_raise = rn2_raise("AttributeError", "missing attribute '_entries'");
                else if (entries->type != JSON_OBJECT) {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "'%s' object has no attribute 'get'", rn2_typename(entries));
                    body_raise = rn2_raise("AttributeError", buf);
                } else {
                    entry = json_obj_get(entries, session_key);
                }
            }
        }
        if (body_raise) {
            if (rn2_is_base_exc(rn2_kind(body_raise))) { free(entry_sid); return body_raise; }
            json_t *la = json_array();
            json_append(la, json_string(session_key));
            json_t *lr = rn2_log(s, "debug", "Completion route lookup failed for %s", la, true);
            json_free(body_raise);
            if (rn2_raised(lr)) { free(entry_sid); return lr; }
            json_free(lr);
            free(entry_sid);
            return rn2_ret3("retry", "", "");
        }
        if (rn2_truth(entry)) {
            free(entry_sid);
            entry_sid = rn2_pystr(rn2_oget(entry, "session_id"));
        }
    }
    json_t *a = rn2_args();
    rn2_s(a, "parent_session_id", psid_in);
    rn2_s(a, "session_id", entry_sid);
    free(entry_sid);
    json_t *lk = rn2_call(s, "self._lookup_completion_owner", a);
    if (rn2_raised(lk)) return lk;
    json_t *uc = rn2_unpack_check(lk, 3);
    if (uc) { json_free(lk); return uc; }
    char *verdict = rn2_pystr(json_get(lk, 0));
    char *psid = rn2_pystr(json_get(lk, 1));
    const json_t *parent = json_get(lk, 2);
    char *target = strdup(psid);
    const json_t *aio = rn2_oget(store, "aio");
    a = rn2_args();
    rn2_j(a, "aio", aio);
    rn2_j(a, "entry", entry);
    rn2_s(a, "parent_session_id", psid);
    rn2_j(a, "parent", parent);
    json_t *ct = rn2_call(s, "self._closed_local_target", a);
    if (rn2_raised(ct)) { free(verdict); free(psid); free(target); json_free(lk); return ct; }
    if (rn2_truth(ct)) {
        free(verdict); verdict = strdup("deliver");
        free(target); target = rn2_pystr(ct);
    } else if (strcmp(verdict, "deliver") == 0 && !rn2_is_null(parent)
               && rn2_truth(rn2_oget(parent, "ended_at"))) {
        char *end_reason = rn2_str_or_empty(rn2_oget(parent, "end_reason"));
        if (strcmp(end_reason, "compression") != 0) {
            free(verdict);
            verdict = strdup(rn2_user_boundary(end_reason) ? "terminal" : "deliver");
        } else {
            a = rn2_args();
            rn2_j(a, "aio", aio);
            rn2_j(a, "entry", entry);
            rn2_s(a, "parent_session_id", psid);
            json_t *cl = rn2_call(s, "self._resolve_compression_lineage_target", a);
            if (rn2_raised(cl)) { free(end_reason); free(verdict); free(psid); free(target); json_free(lk); json_free(ct); return cl; }
            json_t *uc2 = rn2_unpack_check(cl, 2);
            if (uc2) { json_free(cl); free(end_reason); free(verdict); free(psid); free(target); json_free(lk); json_free(ct); return uc2; }
            free(verdict); verdict = rn2_pystr(json_get(cl, 0));
            free(target); target = rn2_pystr(json_get(cl, 1));
            json_free(cl);
        }
        free(end_reason);
    }
    json_free(ct);
    json_t *ret;
    if (strcmp(verdict, "deliver") != 0) {
        ret = rn2_ret3(verdict, "", "");
    } else {
        ret = rn2_ret3(verdict, psid, target[0] ? target : psid);
    }
    free(verdict); free(psid); free(target);
    json_free(lk);
    return ret;
}

/* ── 13. _classify_completion_target: verdict-only wrapper ── */
/* PoP: rn2_classify_completion_target @ gateway/run_notifications.py:_classify_completion_target */
static json_t *rn2_classify_completion_target(const json_t *self, const char *parent_session_id,
                                              const char *session_key, const rn2_seams_t *s) {
    json_t *r = rn2_classify_completion_owner(self, parent_session_id, session_key, s);
    if (rn2_raised(r)) return r;
    json_t *v = rn2_index0(r);
    json_free(r);
    return v;
}

/* ── 14. _completion_delivery_ready: owner/transport availability gate ── */
/* PoP: rn2_completion_delivery_ready @ gateway/run_notifications.py:_completion_delivery_ready */
static json_t *rn2_completion_delivery_ready(const json_t *self, json_t *evt, const rn2_seams_t *s) {
    char *ps_s = rn2_str_or_empty(rn2_oget(evt, "parent_session_id"));
    char *parent = rn2_strip(ps_s);
    free(ps_s);
    if (parent[0]) {
        char *sk = rn2_str_or_empty(rn2_oget(evt, "session_key"));
        json_t *vt = rn2_classify_completion_target(self, parent, sk, s);
        free(sk);
        if (rn2_raised(vt)) { free(parent); return vt; }
        const char *verdict = rn2_strval(vt);
        bool is_deliver = verdict && strcmp(verdict, "deliver") == 0;
        bool is_terminal = verdict && strcmp(verdict, "terminal") == 0;
        json_free(vt);
        if (!is_deliver) { free(parent); return json_bool(is_terminal); }
    }
    free(parent);
    json_t *a = rn2_args(); rn2_j(a, "evt", evt);
    json_t *source = rn2_call(s, "self._build_process_event_source", a);
    if (rn2_raised(source)) return source;
    json_t *adapter = NULL;
    char *platform = NULL;
    if (!rn2_is_null(source)) {
        platform = rn2_platform_of(source);
        adapter = rn2_resolve_injection_adapter(self, platform, source, s);
        if (rn2_raised(adapter)) { free(platform); json_free(source); return adapter; }
    } else {
        a = rn2_args(); rn2_j(a, "evt", evt);
        json_t *rsid = rn2_call(s, "_raw_process_event_session_id", a);
        if (rn2_raised(rsid)) { json_free(source); return rsid; }
        bool has_raw = rn2_truth(rsid);
        json_free(rsid);
        if (has_raw) {
            const json_t *adapters = rn2_oget(self, "adapters");
            const json_t *api = (adapters && adapters->type == JSON_OBJECT) ? json_obj_get(adapters, "api_server") : NULL;
            adapter = api ? json_copy(api) : NULL;
        }
        if (!rn2_is_null(adapter)) {
            a = rn2_args(); rn2_j(a, "adapter", adapter);
            json_t *sp = rn2_call(s, "adapter_supports_push", a);
            if (rn2_raised(sp)) { json_free(adapter); json_free(source); return sp; }
            bool push = rn2_truth(sp);
            json_free(sp);
            if (push) { json_free(adapter); json_free(source); return json_bool(false); }
        }
    }
    /* warned = self.__dict__.setdefault("_completion_no_transport_warned", set()) */
    json_t *warned = (self && self->type == JSON_OBJECT) ? json_obj_get(self, "_completion_no_transport_warned") : NULL;
    if (!warned) {
        warned = json_array();
        json_set((json_t *)self, "_completion_no_transport_warned", warned);
    }
    json_t *key = NULL;
    if (!rn2_is_null(source)) {
        key = json_array();
        const json_t *prof = rn2_oget(source, "profile");
        json_append(key, prof ? json_copy(prof) : json_null());
        json_append(key, json_string(platform ? platform : ""));
    }
    if (rn2_is_null(adapter)) {
        json_set(evt, "_no_transport", json_bool(true));
        if (key && !rn2_warned_has(warned, key)) {
            rn2_warned_add(warned, key);
            const json_t *config = rn2_oget(self, "config");
            bool multiplexed = rn2_truth(rn2_oget(config, "multiplex_profiles"));
            a = rn2_args();
            rn2_j(a, "source", source);
            rn2_s(a, "platform", platform);
            rn2_b(a, "multiplexed", multiplexed);
            json_t *hr = rn2_call(s, "_held_reason", a);
            if (rn2_raised(hr)) { json_free(key); json_free(adapter); json_free(source); free(platform); return hr; }
            json_t *la = json_array();
            json_append(la, hr);
            json_t *lr = rn2_log(s, "warning", "Async completion held: %s", la, false);
            if (rn2_raised(lr)) { json_free(key); json_free(adapter); json_free(source); free(platform); return lr; }
            json_free(lr);
        }
        json_free(key); json_free(adapter); json_free(source); free(platform);
        return json_bool(false);
    }
    if (key) rn2_warned_discard(warned, key);
    json_free(key);
    json_obj_del(evt, "_no_transport");
    a = rn2_args(); rn2_j(a, "adapter", adapter);
    json_t *sp = rn2_call(s, "adapter_supports_push", a);
    if (rn2_raised(sp)) { json_free(adapter); json_free(source); free(platform); return sp; }
    bool push = rn2_truth(sp);
    json_free(sp);
    if (!push) {
        const json_t *ensure = rn2_oget(adapter, "_ensure_store");
        bool callable = ensure != NULL;   /* present hook key models a callable method */
        json_t *body_raise = NULL;
        bool ret_false = false;
        if (!callable) {
            ret_false = true;
        } else {
            json_t *er = rn2_call(s, "adapter.ensure_store", rn2_args());
            if (rn2_raised(er)) body_raise = er;
            else { if (rn2_is_null(er)) ret_false = true; json_free(er); }
        }
        if (body_raise) {
            if (rn2_is_base_exc(rn2_kind(body_raise))) { json_free(adapter); json_free(source); free(platform); return body_raise; }
            json_t *lr = rn2_log(s, "debug", "Async-completion delivery DB unavailable", json_array(), true);
            json_free(body_raise);
            if (rn2_raised(lr)) { json_free(adapter); json_free(source); free(platform); return lr; }
            json_free(lr);
            ret_false = true;
        }
        if (ret_false) { json_free(adapter); json_free(source); free(platform); return json_bool(false); }
    }
    json_free(adapter); json_free(source); free(platform);
    return json_bool(true);
}

/* ── 15. _preflight_completion_delivery: claim + verify before acceptance ── */
/* PoP: rn2_preflight_completion_delivery @ gateway/run_notifications.py:_preflight_completion_delivery */
static json_t *rn2_preflight_completion_delivery(const json_t *self, json_t *evt, const rn2_seams_t *s) {
    json_t *claim = json_object();
    json_set(claim, "delegation_id", json_string(""));
    json_set(claim, "claim_id", json_string(""));
    json_set(claim, "proceed", json_bool(true));
    json_set(claim, "early_result", json_null());
    json_set(claim, "owner_session_id", json_string(""));
    json_set(claim, "proven_session_id", json_string(""));
    json_set(claim, "defer", json_bool(false));
    const char *tys = rn2_strval(rn2_oget(evt, "type"));
    bool is_ad = tys && strcmp(tys, "async_delegation") == 0;
    bool is_completion = tys && strcmp(tys, "completion") == 0;
    if (is_ad) {
        json_t *ready = rn2_completion_delivery_ready(self, evt, s);
        if (rn2_raised(ready)) { json_free(claim); return ready; }
        bool r = rn2_truth(ready);
        json_free(ready);
        if (!r) {
            json_set(claim, "proceed", json_bool(false));
            json_set(claim, "early_result", json_bool(false));
            return claim;
        }
    }
    if (is_ad && !rn2_truth(rn2_oget(evt, "task_failure_notice"))) {
        char *did = rn2_str_or_empty(rn2_oget(evt, "delegation_id"));
        json_set(claim, "delegation_id", json_string(did));
        if (did[0]) {
            json_t *body_raise = NULL;
            char *cid = NULL;
            json_t *sid = rn2_call(s, "self_id", rn2_args());
            if (rn2_raised(sid)) {
                body_raise = sid;
            } else {
                json_t *hx = rn2_call(s, "uuid4_hex", rn2_args());
                if (rn2_raised(hx)) { json_free(sid); body_raise = hx; }
                else {
                    char *sids = rn2_pystr(sid);
                    char *hxs = rn2_pystr(hx);
                    size_t n = strlen(sids) + strlen(hxs) + 32;
                    cid = malloc(n);
                    snprintf(cid, n, "gateway:%s:%s", sids, hxs);
                    free(sids); free(hxs);
                    json_free(sid); json_free(hx);
                    json_set(claim, "claim_id", json_string(cid));
                    json_t *a = rn2_args();
                    rn2_s(a, "delegation_id", did);
                    rn2_s(a, "claim_id", cid);
                    json_t *cr = rn2_call(s, "claim_completion_delivery", a);
                    if (rn2_raised(cr)) {
                        body_raise = cr;
                    } else {
                        bool ok = rn2_truth(cr);
                        json_free(cr);
                        if (!ok) {
                            json_set(claim, "proceed", json_bool(false));
                            free(cid); free(did);
                            return claim;
                        }
                    }
                }
            }
            if (body_raise) {
                if (rn2_is_base_exc(rn2_kind(body_raise))) { free(cid); free(did); json_free(claim); return body_raise; }
                json_t *la = json_array();
                json_append(la, json_string(did));
                json_append(la, json_copy(body_raise));
                json_t *lr = rn2_log(s, "warning", "Could not claim durable async completion %s: %s", la, false);
                json_free(body_raise);
                if (rn2_raised(lr)) { free(cid); free(did); json_free(claim); return lr; }
                json_free(lr);
                json_set(claim, "proceed", json_bool(false));
                json_set(claim, "early_result", json_bool(false));
                free(cid); free(did);
                return claim;
            }
            free(cid);
        }
        free(did);
    } else if (!is_completion) {
        return claim;
    }
    char *ps_s = rn2_str_or_empty(rn2_oget(evt, "parent_session_id"));
    char *parent = rn2_strip(ps_s);
    free(ps_s);
    if (!parent[0]) { free(parent); return claim; }
    char *sk = rn2_str_or_empty(rn2_oget(evt, "session_key"));
    json_t *co = rn2_classify_completion_owner(self, parent, sk, s);
    free(sk);
    if (rn2_raised(co)) { free(parent); json_free(claim); return co; }
    char *verdict = rn2_pystr(json_get(co, 0));
    json_set(claim, "owner_session_id", json_copy(json_get(co, 1)));
    json_set(claim, "proven_session_id", json_copy(json_get(co, 2)));
    json_free(co);
    if (strcmp(verdict, "terminal") == 0) {
        if (is_ad) {
            const char *didv = rn2_strval(json_obj_get(claim, "delegation_id"));
            const char *did_log = (didv && didv[0]) ? didv : "<legacy>";
            json_t *la = json_array();
            json_append(la, json_string(did_log));
            json_append(la, json_string(parent));
            json_t *lr = rn2_log(s, "warning",
                "Async delegation %s targets permanently-gone session %s; "
                "terminally dropping delivery (result remains in the delegation records).", la, false);
            if (rn2_raised(lr)) { free(verdict); free(parent); json_free(claim); return lr; }
            json_free(lr);
            const char *cid = rn2_strval(json_obj_get(claim, "claim_id"));
            if (cid && cid[0]) {
                json_t *a = rn2_args();
                rn2_s(a, "kind", "drop");
                rn2_s(a, "delegation_id", didv);
                rn2_s(a, "claim_id", cid);
                json_t *sr = rn2_call(s, "self._settle_durable_claim", a);
                if (rn2_raised(sr)) { free(verdict); free(parent); json_free(claim); return sr; }
                json_free(sr);
            }
        } else {
            const json_t *sidn = rn2_oget(evt, "session_id");
            char *sid_log = rn2_truth(sidn) ? rn2_pystr(sidn) : strdup("<unknown>");
            json_t *la = json_array();
            json_append(la, json_string(sid_log));
            json_append(la, json_string(parent));
            free(sid_log);
            json_t *lr = rn2_log(s, "warning",
                "Background process %s completion targets "
                "permanently-gone session %s (user boundary such as "
                "/new); dropping notification (output remains available via process(action='log')).", la, false);
            if (rn2_raised(lr)) { free(verdict); free(parent); json_free(claim); return lr; }
            json_free(lr);
        }
        json_set(claim, "proceed", json_bool(false));
    } else if (strcmp(verdict, "retry") == 0) {
        json_set(claim, "proceed", json_bool(false));
        json_set(claim, "early_result", json_bool(false));
        json_set(claim, "defer", json_bool(true));
    }
    free(verdict); free(parent);
    return claim;
}

/* ── 16. _served_api_server_wake_profile: served-profile ownership probe ── */
/* PoP: rn2_served_api_server_wake_profile @ gateway/run_notifications.py:_served_api_server_wake_profile */
static json_t *rn2_served_api_server_wake_profile(const json_t *self, const json_t *evt,
                                                  const char *raw_sid, const rn2_seams_t *s) {
    const json_t *config = rn2_oget(self, "config");
    bool multiplex = rn2_truth(rn2_oget(config, "multiplex_profiles"));
    if (!multiplex) return NULL;
    const json_t *prim = rn2_oget(self, "_primary_profile_name");
    char *primary = rn2_truth(prim) ? rn2_pystr(prim) : strdup("default");
    json_t *a = rn2_args(); rn2_j(a, "evt", evt);
    json_t *src = rn2_call(s, "self._build_process_event_source", a);
    if (rn2_raised(src)) { free(primary); return src; }
    char *hint_s = rn2_str_or_empty(rn2_oget(src, "profile"));
    char *hinted = rn2_strip(hint_s);
    free(hint_s);
    json_free(src);
    const char *rsid = raw_sid ? raw_sid : "";
    if (hinted[0] && strcmp(hinted, primary) != 0) {
        a = rn2_args();
        rn2_j(a, "config", config);
        rn2_s(a, "profile", hinted);
        rn2_s(a, "raw_sid", rsid);
        json_t *own = rn2_call(s, "session_owned_by_profile", a);
        if (rn2_raised(own)) { free(hinted); free(primary); return own; }
        bool owned = rn2_truth(own);
        json_free(own);
        if (owned) { json_t *r = json_string(hinted); free(hinted); free(primary); return r; }
        char *rp = rn2_repr_str(hinted);
        size_t n = strlen(rp) + 64;
        char *msg = malloc(n);
        snprintf(msg, n, "session is not in served profile %s's own store", rp);
        free(rp);
        json_t *e = rn2_raise("LookupError", msg);
        free(msg); free(hinted); free(primary);
        return e;
    }
    a = rn2_args(); rn2_j(a, "config", config);
    json_t *homes = rn2_call(s, "_multiplex_profile_homes", a);
    if (rn2_raised(homes)) { free(hinted); free(primary); return homes; }
    json_t *ret = NULL;
    if (rn2_is_arr(homes)) {
        for (size_t i = 0; i < json_len(homes); i++) {
            const json_t *pair = json_get(homes, i);
            const json_t *nm = rn2_is_arr(pair) ? json_get(pair, 0) : NULL;
            char *name = rn2_pystr(nm);
            if (strcmp(name, primary) != 0) {
                a = rn2_args();
                rn2_j(a, "config", config);
                rn2_s(a, "profile", name);
                rn2_s(a, "raw_sid", rsid);
                json_t *own = rn2_call(s, "session_owned_by_profile", a);
                if (rn2_raised(own)) { free(name); json_free(homes); free(hinted); free(primary); return own; }
                bool owned = rn2_truth(own);
                json_free(own);
                if (owned) { ret = json_string(name); free(name); break; }
            }
            free(name);
        }
    }
    json_free(homes); free(hinted); free(primary);
    return ret;
}
/* ── 17. _adapter_by_platform_value: literal p.value scan over adapters ── */
/* PoP: rn2_adapter_by_platform_value @ gateway/run_notifications.py:_adapter_by_platform_value */
static json_t *rn2_adapter_by_platform_value(const json_t *self, const char *platform_name) {
    const json_t *adapters = rn2_oget(self, "adapters");
    if (rn2_is_obj(adapters)) {
        for (size_t i = 0; i < adapters->c.count; i++) {
            if (strcmp(adapters->c.keys[i], platform_name ? platform_name : "") == 0)
                return json_copy(adapters->c.items[i]);
        }
    }
    return NULL;
}

/* ── 18. _home_channel_transports: launch-profile home channels w/ transport ── */
/* PoP: rn2_home_channel_transports @ gateway/run_notifications.py:_home_channel_transports */
static json_t *rn2_home_channel_transports(const json_t *self, const rn2_seams_t *s) {
    json_t *out = json_array();
    const json_t *config = rn2_oget(self, "config");
    const json_t *platforms = rn2_oget(config, "platforms");
    if (rn2_is_obj(platforms)) {
        for (size_t i = 0; i < platforms->c.count; i++) {
            const char *pv = platforms->c.keys[i];
            const json_t *pcfg = platforms->c.items[i];
            const json_t *home = rn2_oget(pcfg, "home_channel");
            if (!rn2_truth(home) || !rn2_truth(rn2_oget(home, "chat_id"))) continue;
            json_t *a = rn2_args();
            rn2_s(a, "platform", pv);
            rn2_j(a, "config", config);
            rn2_j(a, "adapters", rn2_oget(self, "adapters"));
            rn2_j(a, "profile", NULL);
            json_t *transport = rn2_call(s, "_safe_delivery_transport", a);
            if (rn2_raised(transport)) { json_free(out); return transport; }
            if (rn2_is_null(transport)) { json_free(transport); continue; }
            json_t *tup = json_array();
            json_append(tup, json_string(pv));
            json_append(tup, json_copy(pcfg));
            json_append(tup, json_copy(home));
            json_append(tup, transport);
            json_append(out, tup);
        }
    }
    return out;
}

/* ── 19. _served_home_channel_configs: every served profile's home config ── */
/* PoP: rn2_served_home_channel_configs @ gateway/run_notifications.py:_served_home_channel_configs */
static json_t *rn2_served_home_channel_configs(const json_t *self) {
    json_t *out = json_array();
    const json_t *config = rn2_oget(self, "config");
    const json_t *platforms = rn2_oget(config, "platforms");
    if (rn2_is_obj(platforms)) {
        for (size_t i = 0; i < platforms->c.count; i++) {
            json_t *tup = json_array();
            json_append(tup, json_null());
            json_append(tup, json_string(platforms->c.keys[i]));
            json_append(tup, json_copy(platforms->c.items[i]));
            json_append(out, tup);
        }
    }
    const json_t *pc = rn2_oget(self, "_profile_configs");
    if (rn2_truth(pc) && rn2_is_obj(pc)) {
        for (size_t i = 0; i < pc->c.count; i++) {
            const char *profile = pc->c.keys[i];
            const json_t *profile_cfg = pc->c.items[i];
            const json_t *pp = rn2_oget(profile_cfg, "platforms");
            if (rn2_is_obj(pp)) {
                for (size_t j = 0; j < pp->c.count; j++) {
                    json_t *tup = json_array();
                    json_append(tup, json_string(profile));
                    json_append(tup, json_string(pp->c.keys[j]));
                    json_append(tup, json_copy(pp->c.items[j]));
                    json_append(out, tup);
                }
            }
        }
    }
    return out;
}

/* ── 20. _served_home_channel_transports: served home channels w/ transport ── */
/* PoP: rn2_served_home_channel_transports @ gateway/run_notifications.py:_served_home_channel_transports */
static json_t *rn2_served_home_channel_transports(const json_t *self, const rn2_seams_t *s) {
    json_t *out = json_array();
    json_t *hct = rn2_home_channel_transports(self, s);
    if (rn2_raised(hct)) return hct;
    if (rn2_is_arr(hct)) {
        for (size_t i = 0; i < json_len(hct); i++) {
            const json_t *t = json_get(hct, i);
            json_t *tup = json_array();
            json_append(tup, json_null());
            for (size_t k = 0; k < 4; k++) json_append(tup, json_copy(json_get(t, k)));
            json_append(out, tup);
        }
    }
    json_free(hct);
    const json_t *pc = rn2_oget(self, "_profile_configs");
    if (rn2_truth(pc) && rn2_is_obj(pc)) {
        for (size_t i = 0; i < pc->c.count; i++) {
            const char *profile = pc->c.keys[i];
            const json_t *profile_cfg = pc->c.items[i];
            const json_t *pa = rn2_oget(self, "_profile_adapters");
            const json_t *adapters = (rn2_truth(pa) && rn2_is_obj(pa)) ? json_obj_get(pa, profile) : NULL;
            json_t *adapters_arg = rn2_truth(adapters) ? json_copy(adapters) : json_object();
            const json_t *pp = rn2_oget(profile_cfg, "platforms");
            if (rn2_is_obj(pp)) {
                for (size_t j = 0; j < pp->c.count; j++) {
                    const char *pv = pp->c.keys[j];
                    const json_t *pcfg = pp->c.items[j];
                    const json_t *home = rn2_oget(pcfg, "home_channel");
                    if (!rn2_truth(home) || !rn2_truth(rn2_oget(home, "chat_id"))) continue;
                    json_t *a = rn2_args();
                    rn2_s(a, "platform", pv);
                    rn2_j(a, "config", profile_cfg);
                    json_set(a, "adapters", json_copy(adapters_arg));
                    rn2_s(a, "profile", profile);
                    json_t *transport = rn2_call(s, "_safe_delivery_transport", a);
                    if (rn2_raised(transport)) { json_free(adapters_arg); json_free(out); return transport; }
                    if (rn2_is_null(transport)) { json_free(transport); continue; }
                    json_t *tup = json_array();
                    json_append(tup, json_string(profile));
                    json_append(tup, json_string(pv));
                    json_append(tup, json_copy(pcfg));
                    json_append(tup, json_copy(home));
                    json_append(tup, transport);
                    json_append(out, tup);
                }
            }
            json_free(adapters_arg);
        }
    }
    return out;
}