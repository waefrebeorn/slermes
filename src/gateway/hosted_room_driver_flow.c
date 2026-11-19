/*
 * hosted_room_driver_flow.c — FLOW cores + leaves of
 * tui_gateway/hosted_room_driver.py (the process-local hosted-room worker).
 *
 * Ports the DECISION SEQUENCES of the session-open/probe cluster
 * (_resolve_or_create, _resume_exact, _open_session, _terminal_from_history,
 * _peer_stop_acknowledged), the lease cache/renewal cluster (_cache_lease,
 * _renew_lease_if_needed, _ensure_lease, _release_idle_leases, _drop_lease,
 * _mark_ambiguous), the fenced-transition cluster (_fenced, _requeue,
 * _resolve_indeterminate, _complete_cancel, _defer_unconfirmed_stop,
 * _complete_acknowledged_stop, _settle_failure_if_current), the retry-backoff
 * and pending-action reporters, and the pure receipt/info leaves. All live
 * I/O enters through ONE universal seam; runtime state lives in an injected
 * json_t mirror mutated in place.
 *
 * Modeling contract (shared with tests/oracle_hosted_room_driver_flow.py):
 *  - Universal seam: json_t *(*call)(void *ud, const char *name, args).
 *    name identifies the live call (table below); args is a BORROWED JSON
 *    object the stub copies into the trace; the result is a NEW json_t
 *    (NULL == None; {"__raise__":kind,"message":m} == a raised exception).
 *    The port owns args (hrd_call frees them after the call) and the result.
 *  - Seam names -> live calls:
 *      state.get_task / state.list_tasks / state.complete_task_cancel /
 *      state.settle_task / state.acquire_lease / state.renew_lease /
 *      state.release_lease / state.<op>   -> gateway.hosted_room_driver.<op>
 *        (claimed siblings, seamed; <op> is the fixture's op/requeue name:
 *         settle_stopping_task, resolve_indeterminate_task,
 *         defer_unconfirmed_stop, requeue_deferred_task, ...)
 *      transport.resolve_exact / transport.create / transport.resume /
 *      transport.history / transport.info / transport.interrupt /
 *      transport.discard                  -> InternalSessionRPC methods
 *      remote_member / publish_terminal / pending_action / task_progress /
 *      retire_task_progress / transport_resolver -> the named injected
 *        callback (presence flags in state.cb)
 *      clock                              -> self.clock()
 *  - Callable args (store, clock, op) are NOT projected into seam args.
 *    state.settle_task's second positional is an attempt -> "attempt" key;
 *    fenced ops carry identity + lease + placement + fences + extra.
 *  - State mirror (in["state"], copied by the harness before mutation):
 *      cfg: {lease_ttl_seconds, poll_interval_seconds,
 *            active_poll_interval_seconds, turn_timeout_seconds,
 *            indeterminate_defer_seconds, unavailable_retry_min_seconds,
 *            unavailable_retry_max_seconds, process_generation}
 *      placement: str; cb: {<callback>: bool presence}; rpc_id: str
 *      rooms: [binding {room_id, gateway_id, authority_epoch}]
 *      leases: {room_id: lease {room_id, gateway_id, authority_epoch,
 *               process_generation, lease_generation, expires_at, reclaimed}}
 *      blocked / reschedule / room_threads: sorted room-id arrays (sets/dict)
 *      ambiguous: {room_id: float}; last_error: str|null
 *      route_retries: [{room_id, member_id, delay, next_attempt_at}]
 *        (live dict keyed by (room_id, member_id); insertion order kept,
 *         an update replaces the record in place)
 *      recovered: [[room_id, lease_gen]]; inspected: [[room_id, task_id, gen]]
 *        (sets of tuples; sorted on emit both sides)
 *      wake_set / stop_set: bool (threading.Event flags)
 *  - Transport doubles carry an "id"; `transport is self.rpc` compares ids
 *    against state.rpc_id (the oracle keeps a per-id object registry so the
 *    live `is` agrees). Transport RETURNS project to {"id": ...}; internal
 *    readers still see the full double (served_elsewhere flag).
 *  - wakeup() is an internal state step (reschedule |= room_threads keys;
 *    wake_set = true), not a seam. Locks are no-ops (single-threaded).
 *  - int()/float() coercions transcribe CPython 3.14 semantics: int-strip
 *    whitespace set (White_Space minus \x1c-\x1f), sign, underscore-between-
 *    digits rules, exact ValueError/TypeError messages. DOMAIN: ASCII digits
 *    only (live int() also accepts Unicode decimal digits — out of domain),
 *    finite numbers, and str() of scalars only (integral floats read as ints
 *    on both sides; non-integral float str() and str(list/dict) are out of
 *    domain and fail closed).
 *  - Exception kinds: the DriverStateError family (DriverStateError,
 *    DriverValidationError, RoomUnavailableError, LeaseHeldError,
 *    StaleLeaseError, StaleTaskError, InvalidTaskTransitionError,
 *    TaskConflictError) is swallowed exactly where live suppress()es it;
 *    BaseException kinds (SystemExit, KeyboardInterrupt, GeneratorExit,
 *    CancelledError) pass through every `except Exception` site.
 *  - Scripted seam results: a missing/exhausted script is fail-closed — the
 *    harness dispatcher RETURNS a {"__raise__":"RuntimeError",...} spec and
 *    records it, so the port propagates it like any scripted raise.
 *  - Self-contained: the harness #includes this .c; every symbol is static
 *    and hrd_*-prefixed; only libjson is linked.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "libjson/json.h"

/* ── live module constants (byte-exact) ───────────────────────────────────── */
static const char HRD_ROOM_SOURCE[] = "bot_room";
static const char HRD_TRUNC_NOTICE[] =
    "\n\n[Reply truncated. Ask the Bot to share the full result as a file.]";
#define HRD_TRUNC_NOTICE_LEN 68
#define HRD_MAX_TERMINAL_TEXT 65536
#define HRD_MAX_ERROR_TEXT 4096
static const char HRD_DEADLINE_ERROR[] =
    "This Group Chat turn exceeded its configured time limit and was stopped.";

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct hrd_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
} hrd_env;

/* ── JSON type / truthiness helpers ───────────────────────────────────────── */
static bool hrd_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool hrd_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool hrd_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool hrd_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool hrd_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *hrd_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *hrd_sval(const json_t *v) {
    const char *s = hrd_str(v);
    return s ? s : "";
}
static bool hrd_truth(const json_t *v) {
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
static const char *hrd_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* ── raise specs ──────────────────────────────────────────────────────────── */
static bool hrd_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && hrd_is_str(json_obj_get(r, "__raise__"));
}
static const char *hrd_kind(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static const char *hrd_msg(const json_t *r) { return json_get_str(r, "message", ""); }
static bool hrd_is_base(const char *k) {
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0 || strcmp(k, "CancelledError") == 0;
}
static bool hrd_is_driver_state(const char *k) {
    return strcmp(k, "DriverStateError") == 0 || strcmp(k, "DriverValidationError") == 0
        || strcmp(k, "RoomUnavailableError") == 0 || strcmp(k, "LeaseHeldError") == 0
        || strcmp(k, "StaleLeaseError") == 0 || strcmp(k, "StaleTaskError") == 0
        || strcmp(k, "InvalidTaskTransitionError") == 0 || strcmp(k, "TaskConflictError") == 0;
}
static json_t *hrd_raise_spec(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}
static json_t *hrd_attr_raise(const json_t *v, const char *attr) {
    char buf[192];
    snprintf(buf, sizeof(buf), "'%s' object has no attribute '%s'", hrd_type_name(v), attr);
    return hrd_raise_spec("AttributeError", buf);
}
static json_t *hrd_keyerror_raise(const char *key) {
    char buf[192];
    snprintf(buf, sizeof(buf), "'%s'", key);
    return hrd_raise_spec("KeyError", buf);
}
static json_t *hrd_subscript_raise(const json_t *v) {
    char buf[192];
    const char *t = hrd_type_name(v);
    if (strcmp(t, "str") == 0)
        snprintf(buf, sizeof(buf), "string indices must be integers, not 'str'");
    else
        snprintf(buf, sizeof(buf), "'%s' object is not subscriptable", t);
    return hrd_raise_spec("TypeError", buf);
}
/* `except Exception`: swallow plain Exception (free r), propagate BaseException.
 * Returns true when the caller must propagate (r stays owned). */
static bool hrd_swallow_exc(json_t *r) {
    if (hrd_is_base(hrd_kind(r))) return true;
    json_free(r);
    return false;
}

/* ── universal seam call (args owned by port, freed after the call) ───────── */
static json_t *hrd_call(hrd_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}
static json_t *hrd_seam_obj(hrd_env *e, const char *name) {
    return hrd_call(e, name, json_object());
}

/* ── deep JSON equality (order-independent objects) ───────────────────────── */
static bool hrd_json_eq(const json_t *a, const json_t *b) {
    if (hrd_is_null(a) && hrd_is_null(b)) return true;
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL:   return true;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!hrd_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !hrd_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        }
    }
    return false;
}

/* ── state mirror access (borrowed) ───────────────────────────────────────── */
static json_t *hrd_cfg(hrd_env *e) { return json_obj_get(e->state, "cfg"); }
static json_t *hrd_cb(hrd_env *e) { return json_obj_get(e->state, "cb"); }
static bool hrd_has_cb(hrd_env *e, const char *name) {
    return json_get_bool(hrd_cb(e), name, false);
}
static json_t *hrd_leases(hrd_env *e) { return json_obj_get(e->state, "leases"); }
static const char *hrd_placement(hrd_env *e) {
    return json_get_str(e->state, "placement", "shared");
}
static const char *hrd_rpc_id(hrd_env *e) {
    return json_get_str(e->state, "rpc_id", "rpc");
}
static double hrd_cfg_num(hrd_env *e, const char *key, double def) {
    return json_get_num(hrd_cfg(e), key, def);
}

/* ── sorted string-set helpers (blocked / reschedule / room_threads) ──────── */
static void hrd_set_add(json_t *arr, const char *s) {
    size_t n = json_len(arr);
    size_t pos = n;
    for (size_t i = 0; i < n; i++) {
        int cmp = strcmp(hrd_sval(json_get(arr, i)), s);
        if (cmp == 0) return;
        if (cmp > 0) { pos = i; break; }
    }
    json_t *fresh = json_array();
    for (size_t i = 0; i < pos; i++) json_append(fresh, json_copy(json_get(arr, i)));
    json_append(fresh, json_string(s));
    for (size_t i = pos; i < n; i++) json_append(fresh, json_copy(json_get(arr, i)));
    /* swap child buffers (capacity stays owned by the json_append-managed side) */
    json_t **oi = arr->c.items; size_t on = arr->c.count;
    arr->c.items = fresh->c.items; arr->c.count = fresh->c.count;
    fresh->c.items = oi; fresh->c.count = on;
    json_free(fresh);
}
static void hrd_set_del(json_t *arr, const char *s) {
    for (size_t i = 0; i < json_len(arr); i++) {
        if (strcmp(hrd_sval(json_get(arr, i)), s) == 0) {
            json_free(arr->c.items[i]);
            for (size_t j = i + 1; j < arr->c.count; j++) arr->c.items[j - 1] = arr->c.items[j];
            arr->c.count--;
            return;
        }
    }
}
static bool hrd_set_has(const json_t *arr, const char *s) {
    for (size_t i = 0; i < json_len(arr); i++)
        if (strcmp(hrd_sval(json_get(arr, i)), s) == 0) return true;
    return false;
}
/* ── UTF-8 helpers ────────────────────────────────────────────────────────── */
static unsigned long hrd_cp(const char *s, size_t *adv) {
    const unsigned char *u = (const unsigned char *)s;
    unsigned long cp; size_t n;
    if (u[0] < 0x80) { cp = u[0]; n = 1; }
    else if ((u[0] & 0xE0) == 0xC0) { cp = u[0] & 0x1F; n = 2; }
    else if ((u[0] & 0xF0) == 0xE0) { cp = u[0] & 0x0F; n = 3; }
    else if ((u[0] & 0xF8) == 0xF0) { cp = u[0] & 0x07; n = 4; }
    else { cp = u[0]; n = 1; }
    for (size_t i = 1; i < n && u[i]; i++) cp = (cp << 6) | (u[i] & 0x3F);
    *adv = n;
    return cp;
}
/* int()/float() strip set: White_Space MINUS \x1c-\x1f (probed on 3.14.9). */
static bool hrd_num_ws(unsigned long cp) {
    return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20
        || cp == 0x85 || cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)
        || cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
/* strip num-whitespace from both ends; returns malloc'd copy */
static char *hrd_num_strip(const char *s) {
    size_t start = 0, end = 0, i = 0;
    bool any = false;
    while (s[i]) {
        size_t adv; unsigned long cp = hrd_cp(s + i, &adv);
        if (!hrd_num_ws(cp)) { if (!any) { start = i; any = true; } end = i + adv; }
        i += adv;
    }
    if (!any) return strdup("");
    char *r = (char *)malloc(end - start + 1);
    memcpy(r, s + start, end - start);
    r[end - start] = '\0';
    return r;
}
/* strict UTF-8 validation of the first len bytes (Python decode('utf-8')) */
static bool hrd_utf8_valid(const char *s, size_t len) {
    const unsigned char *u = (const unsigned char *)s;
    size_t i = 0;
    while (i < len) {
        unsigned char b = u[i];
        if (b < 0x80) { i++; continue; }
        size_t need; unsigned char lo = 0x80, hi = 0xBF;
        if (b >= 0xC2 && b <= 0xDF) need = 1;
        else if (b >= 0xE0 && b <= 0xEF) {
            need = 2;
            if (b == 0xE0) lo = 0xA0;
            if (b == 0xED) hi = 0x9F;
        } else if (b >= 0xF0 && b <= 0xF4) {
            need = 3;
            if (b == 0xF0) lo = 0x90;
            if (b == 0xF4) hi = 0x8F;
        } else return false;
        if (i + need >= len) return false;
        for (size_t k = 1; k <= need; k++) {
            unsigned char c = u[i + k];
            if ((c & 0xC0) != 0x80) return false;
            if (k == 1 && (c < lo || c > hi)) return false;
        }
        i += need + 1;
    }
    return true;
}

/* ── Python str() of scalars (integral floats read as ints) ───────────────── */
/* Returns malloc'd string, or NULL when out of the documented domain. */
static char *hrd_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
                return strdup(buf);
            }
            return NULL;   /* non-integral float str(): out of domain */
        }
        default: return NULL;
    }
}
/* Python repr() of a str for error messages (ASCII + Latin-1 domain). */
static char *hrd_pyrepr(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL, has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *r = (char *)malloc(cap);
    size_t o = 0;
    r[o++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { r[o++] = '\\'; r[o++] = (char)c; }
        else if (c == '\n') { r[o++] = '\\'; r[o++] = 'n'; }
        else if (c == '\r') { r[o++] = '\\'; r[o++] = 'r'; }
        else if (c == '\t') { r[o++] = '\\'; r[o++] = 't'; }
        else if (c < 0x20 || c == 0x7F) { o += (size_t)snprintf(r + o, cap - o, "\\x%02x", c); }
        else if (c < 0x80) { r[o++] = (char)c; }
        else {
            /* Latin-1: printable range stays literal, else \xNN */
            bool printable = (c >= 0xA1 && c <= 0xAC) || (c >= 0xAE);
            if (printable) r[o++] = (char)c;
            else o += (size_t)snprintf(r + o, cap - o, "\\x%02x", c);
        }
    }
    r[o++] = q;
    r[o] = '\0';
    return r;
}

/* ── Python int() coercion (base 10, ASCII-digit domain) ──────────────────── */
/* Returns NULL on success (*out set), else an owned raise spec. */
static json_t *hrd_pyint(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL)
        return hrd_raise_spec("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return NULL; }
    if (v->type == JSON_NUMBER) { *out = trunc(v->num_val); return NULL; }
    if (v->type != JSON_STRING) {
        char buf[192];
        snprintf(buf, sizeof(buf),
            "int() argument must be a string, a bytes-like object or a real number, not '%s'",
            hrd_type_name(v));
        return hrd_raise_spec("TypeError", buf);
    }
    const char *orig = v->str_val ? v->str_val : "";
    char *clean = hrd_num_strip(orig);
    char *p = clean;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    /* digits with underscores only BETWEEN digits */
    char digits[512];
    size_t nd = 0;
    bool ok = *p != '\0', prev_digit = false;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            if (nd < sizeof(digits) - 1) digits[nd++] = *p;
            prev_digit = true;
        } else if (*p == '_') {
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) { ok = false; break; }
            prev_digit = false;
        } else { ok = false; break; }
    }
    if (ok && nd > 0) {
        digits[nd] = '\0';
        long long ll = strtoll(digits, NULL, 10);
        *out = neg ? -(double)ll : (double)ll;
        free(clean);
        return NULL;
    }
    free(clean);
    char *rep = hrd_pyrepr(orig);
    char buf[1024];
    snprintf(buf, sizeof(buf), "invalid literal for int() with base 10: %s", rep);
    free(rep);
    return hrd_raise_spec("ValueError", buf);
}

/* ── Python float() coercion ──────────────────────────────────────────────── */
static bool hrd_ieq(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}
/* validate a digit run with between-digits underscores; append clean digits.
 * Returns whether any digit was consumed; *invalid flags a bad underscore. */
static bool hrd_float_digits(char **pp, char *buf, size_t *nd, size_t cap, bool *invalid) {
    char *p = *pp;
    bool any = false, prev_digit = false;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            if (*nd < cap - 1) buf[(*nd)++] = *p;
            any = true; prev_digit = true; p++;
        } else if (*p == '_') {
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) { *invalid = true; *pp = p; return any; }
            prev_digit = false; p++;
        } else break;
    }
    *pp = p;
    return any;
}
static json_t *hrd_pyfloat(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL)
        return hrd_raise_spec("TypeError",
            "float() argument must be a string or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return NULL; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return NULL; }
    if (v->type != JSON_STRING) {
        char buf[192];
        snprintf(buf, sizeof(buf), "float() argument must be a string or a real number, not '%s'",
            hrd_type_name(v));
        return hrd_raise_spec("TypeError", buf);
    }
    const char *orig = v->str_val ? v->str_val : "";
    char *clean = hrd_num_strip(orig);
    char *p = clean;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (hrd_ieq(p, "inf") || hrd_ieq(p, "infinity")) {
        *out = neg ? -INFINITY : INFINITY;
        free(clean);
        return NULL;
    }
    if (hrd_ieq(p, "nan")) { *out = NAN; free(clean); return NULL; }
    char buf[512];
    size_t nd = 0;
    bool ok = true;
    bool inv = false;
    bool int_part = hrd_float_digits(&p, buf, &nd, sizeof(buf), &inv);
    bool frac_part = false;
    if (*p == '.') {
        p++;
        if (nd < sizeof(buf) - 1) buf[nd++] = '.';
        frac_part = hrd_float_digits(&p, buf, &nd, sizeof(buf), &inv);
        /* mantissa needs at least one digit overall */
    }
    if (inv || (!int_part && !frac_part)) ok = false;
    if (ok && (*p == 'e' || *p == 'E')) {
        p++;
        char esign = 0;
        if (*p == '+' || *p == '-') { esign = *p; p++; }
        char exp[128];
        size_t ne = 0;
        bool einf = false;
        if (!hrd_float_digits(&p, exp, &ne, sizeof(exp), &einf) || einf) ok = false;
        else {
            exp[ne] = '\0';
            if (nd < sizeof(buf) - 1) buf[nd++] = 'e';
            if (esign && nd < sizeof(buf) - 1) buf[nd++] = esign;
            for (size_t i = 0; i < ne && nd < sizeof(buf) - 1; i++) buf[nd++] = exp[i];
        }
    }
    if (ok && *p != '\0') ok = false;
    if (ok) {
        buf[nd] = '\0';
        char full[520];
        snprintf(full, sizeof(full), "%s%s", neg ? "-" : "", buf);
        *out = strtod(full, NULL);
        free(clean);
        return NULL;
    }
    free(clean);
    char *rep = hrd_pyrepr(orig);
    char msg[1024];
    snprintf(msg, sizeof(msg), "could not convert string to float: %s", rep);
    free(rep);
    return hrd_raise_spec("ValueError", msg);
}

/* ── subscript / attribute access with live raise semantics ───────────────── */
/* task["key"]: borrowed value, or NULL with *err set (owned spec). */
static const json_t *hrd_sub(const json_t *obj, const char *key, json_t **err) {
    *err = NULL;
    if (!hrd_is_obj(obj)) { *err = hrd_subscript_raise(obj); return NULL; }
    if (!json_has(obj, key)) { *err = hrd_keyerror_raise(key); return NULL; }
    return json_obj_get(obj, key);
}
/* obj.attr on an attribute double: borrowed value or NULL with *err set.
 * Domain: doubles always carry their fields; only null receivers raise. */
static const json_t *hrd_attr(const json_t *obj, const char *attr, json_t **err) {
    *err = NULL;
    if (!hrd_is_obj(obj)) { *err = hrd_attr_raise(obj, attr); return NULL; }
    const json_t *v = json_obj_get(obj, attr);
    if (!v) { *err = hrd_attr_raise(obj, attr); return NULL; }
    return v;
}
/* .get on a mapping receiver: borrowed value or NULL (absent); *err on non-obj */
static const json_t *hrd_map_get(const json_t *obj, const char *key, json_t **err) {
    *err = NULL;
    if (!hrd_is_obj(obj)) { *err = hrd_attr_raise(obj, "get"); return NULL; }
    return json_obj_get(obj, key);
}
/* ── pure leaves ──────────────────────────────────────────────────────────── */
/* PoP: hrd_session_kw @ tui_gateway/hosted_room_driver.py:_session_kw */
static json_t *hrd_session_kw(hrd_env *e, const json_t *in) {
    (void)e;
    json_t *r = json_object();
    const json_t *pf = json_obj_get(in, "profile");
    const json_t *sid = json_obj_get(in, "session_id");
    json_set(r, "profile", pf ? json_copy(pf) : json_null());
    json_set(r, "session_id", sid ? json_copy(sid) : json_null());
    json_set(r, "source", json_string(HRD_ROOM_SOURCE));
    return r;
}

/* _fences(task): int() coercions, execution_generation first */
static json_t *hrd_fences_of(const json_t *task) {
    json_t *err = NULL;
    const json_t *eg = hrd_sub(task, "execution_generation", &err);
    if (err) return err;
    const json_t *cg = hrd_sub(task, "cancel_generation", &err);
    if (err) return err;
    double ieg, icg;
    err = hrd_pyint(eg, &ieg);
    if (err) return err;
    err = hrd_pyint(cg, &icg);
    if (err) return err;
    json_t *r = json_object();
    json_set(r, "expected_execution_generation", json_number(ieg));
    json_set(r, "expected_cancel_generation", json_number(icg));
    return r;
}
/* PoP: hrd_fences @ tui_gateway/hosted_room_driver.py:_fences */
static json_t *hrd_fences(hrd_env *e, const json_t *in) {
    (void)e;
    return hrd_fences_of(json_obj_get(in, "task"));
}

/* PoP: hrd_room_session_title @ tui_gateway/hosted_room_driver.py:room_session_title */
static json_t *hrd_room_session_title(hrd_env *e, const json_t *in) {
    (void)e;
    char *s = hrd_pystr(json_obj_get(in, "room_id"));
    if (!s) return hrd_raise_spec("TypeError", "str() domain: scalar expected");
    char buf[512];
    snprintf(buf, sizeof(buf), "Group: %s", s);
    free(s);
    return json_string(buf);
}

/* _member_id(task): str(p.get("target_member_id") or p.get("target_profile") or "") */
static json_t *hrd_member_id_of(const json_t *task) {
    json_t *err = NULL;
    const json_t *payload = hrd_map_get(task, "payload", &err);
    if (err) return err;
    const json_t *p = payload;
    json_t *empty = NULL;
    if (!hrd_truth(p)) { empty = json_object(); p = empty; }
    if (!hrd_is_obj(p)) {
        json_free(empty);
        return hrd_attr_raise(p, "get");
    }
    const json_t *v = json_obj_get(p, "target_member_id");
    if (!hrd_truth(v)) v = json_obj_get(p, "target_profile");
    if (!hrd_truth(v)) v = NULL;   /* or "" */
    json_free(empty);
    char *s;
    if (!v) {
        s = strdup("");
    } else {
        s = hrd_pystr(v);
        if (!s) return hrd_raise_spec("TypeError", "str() domain: scalar expected");
    }
    json_t *r = json_string(s);
    free(s);
    return r;
}
/* PoP: hrd_member_id @ tui_gateway/hosted_room_driver.py:_member_id */
static json_t *hrd_member_id(hrd_env *e, const json_t *in) {
    (void)e;
    return hrd_member_id_of(json_obj_get(in, "task"));
}

/* _session_id(session): chained get, isinstance str + non-empty gate */
static json_t *hrd_session_id_of(const json_t *session) {
    json_t *err = NULL;
    const json_t *v = hrd_map_get(session, "session_id", &err);
    if (err) return err;
    if (!json_has(session, "session_id")) {
        v = hrd_map_get(session, "id", &err);
        if (err) return err;
    }
    const char *s = hrd_str(v);
    if (!s || !s[0])
        return hrd_raise_spec("ValueError", "session adapter returned no session_id");
    return json_string(s);
}
/* PoP: hrd_session_id @ tui_gateway/hosted_room_driver.py:_session_id */
static json_t *hrd_session_id(hrd_env *e, const json_t *in) {
    (void)e;
    return hrd_session_id_of(json_obj_get(in, "session"));
}

/* _truncate_utf8(value, max_bytes=): byte-cut with UTF-8 backoff + notice */
static json_t *hrd_truncate(const json_t *value, long max_bytes) {
    const json_t *src = hrd_truth(value) ? value : NULL;
    char *text = hrd_pystr(src);   /* NULL -> "None"? no: value or "" -> "" */
    if (!src) { free(text); text = strdup(""); }
    if (!text) return hrd_raise_spec("TypeError", "str() domain: scalar expected");
    size_t n = strlen(text);
    json_t *r = json_array();
    if ((long)n <= max_bytes) {
        json_append(r, json_string(text));
        json_append(r, json_bool(false));
        free(text);
        return r;
    }
    long cut = max_bytes - HRD_TRUNC_NOTICE_LEN;
    if (cut < 0) cut = 0;
    while (cut > 0) {
        if (hrd_utf8_valid(text, (size_t)cut)) {
            char *piece = (char *)malloc((size_t)cut + HRD_TRUNC_NOTICE_LEN + 1);
            memcpy(piece, text, (size_t)cut);
            memcpy(piece + cut, HRD_TRUNC_NOTICE, HRD_TRUNC_NOTICE_LEN + 1);
            json_append(r, json_string(piece));
            json_append(r, json_bool(true));
            free(piece);
            free(text);
            return r;
        }
        cut--;
    }
    json_append(r, json_string(HRD_TRUNC_NOTICE + 2));   /* .strip() */
    json_append(r, json_bool(true));
    free(text);
    return r;
}
/* PoP: hrd_truncate_utf8 @ tui_gateway/hosted_room_driver.py:_truncate_utf8 */
static json_t *hrd_truncate_utf8(hrd_env *e, const json_t *in) {
    (void)e;
    return hrd_truncate(json_obj_get(in, "value"), (long)json_get_num(in, "max_bytes", 0));
}

/* _bounded_terminal_result(receipt) */
static json_t *hrd_bounded_result(const json_t *receipt) {
    json_t *err = NULL;
    const json_t *text_v = hrd_map_get(receipt, "text", &err);
    if (err) return err;
    if (!text_v) text_v = json_null();   /* .get default "" -> falsy */
    const json_t *error_v = hrd_map_get(receipt, "error", &err);
    if (err) return err;
    if (!error_v) error_v = json_null();
    json_t *rt = hrd_truncate(text_v, HRD_MAX_TERMINAL_TEXT);
    if (hrd_is_raise(rt)) return rt;
    json_t *re = hrd_truncate(error_v, HRD_MAX_ERROR_TEXT);
    if (hrd_is_raise(re)) { json_free(rt); return re; }
    json_t *r = json_object();
    const json_t *mid = hrd_is_obj(receipt) ? json_obj_get(receipt, "message_id") : NULL;
    json_set(r, "message_id", mid ? json_copy(mid) : json_null());
    json_set(r, "text", json_copy(json_get(rt, 0)));
    if (hrd_truth(json_get(re, 0))) json_set(r, "error", json_copy(json_get(re, 0)));
    if (json_get(rt, 1)->bool_val || json_get(re, 1)->bool_val)
        json_set(r, "truncated", json_bool(true));
    json_free(rt);
    json_free(re);
    return r;
}
/* PoP: hrd_bounded_terminal_result @ tui_gateway/hosted_room_driver.py:_bounded_terminal_result */
static json_t *hrd_bounded_terminal_result(hrd_env *e, const json_t *in) {
    (void)e;
    return hrd_bounded_result(json_obj_get(in, "receipt"));
}

/* _receipt_from(receipt, identity, execution_generation) */
static json_t *hrd_receipt_from(const json_t *receipt, const json_t *identity, double gen) {
    json_t *err = NULL;
    const json_t *st = hrd_map_get(receipt, "status", &err);
    if (err) return err;
    const char *status = (hrd_str(st) && strcmp(hrd_str(st), "settled") == 0) ? "settled" : "failed";
    const json_t *sid = hrd_map_get(receipt, "settlement_id", &err);
    if (err) return err;
    json_t *r = json_object();
    json_set(r, "status", json_string(status));
    if (hrd_truth(sid)) {
        json_set(r, "settlement_id", json_copy(sid));
    } else {
        const json_t *tid = hrd_attr(identity, "task_id", &err);
        if (err) { json_free(r); return err; }
        char buf[512];
        snprintf(buf, sizeof(buf), "reply:%s:%.0f", (hrd_str(tid) ? hrd_str(tid) : ""), gen);
        json_set(r, "settlement_id", json_string(buf));
    }
    json_t *res = hrd_bounded_result(receipt);
    if (hrd_is_raise(res)) { json_free(r); return res; }
    json_set(r, "result", res);
    return r;
}
/* PoP: hrd_receipt_from @ tui_gateway/hosted_room_driver.py:_receipt_from */
static json_t *hrd_receipt_from_entry(hrd_env *e, const json_t *in) {
    (void)e;
    return hrd_receipt_from(json_obj_get(in, "receipt"), json_obj_get(in, "identity"),
                            json_get_num(in, "execution_generation", 0));
}

/* PoP: hrd_discarded_receipt @ tui_gateway/hosted_room_driver.py:discarded_receipt */
static json_t *hrd_discarded_receipt(hrd_env *e, const json_t *in) {
    (void)e;
    const json_t *result = json_obj_get(in, "result");
    const json_t *task = json_obj_get(in, "task");
    json_t *err = NULL;
    const json_t *st = hrd_map_get(result, "status", &err);
    if (err) return err;
    const char *s = hrd_str(st);
    if (!s || (strcmp(s, "settled") != 0 && strcmp(s, "failed") != 0)) return NULL;
    const json_t *ident = hrd_sub(task, "identity", &err);
    if (err) return err;
    const json_t *eg = hrd_sub(task, "execution_generation", &err);
    if (err) return err;
    double gen;
    err = hrd_pyint(eg, &gen);
    if (err) return err;
    return hrd_receipt_from(result, ident, gen);
}
/* _find_terminal_receipt(history, identity, execution_generation) */
static json_t *hrd_find_receipt(const json_t *history, const json_t *identity, double gen) {
    /* reversed(history): arrays reverse; dicts reverse keys; strs reverse chars;
     * scalars raise TypeError. Domain: array / null / number / bool / str / dict. */
    json_t *items = json_array();   /* borrowed refs in reverse order */
    if (hrd_is_arr(history)) {
        for (size_t i = json_len(history); i > 0; i--) json_append(items, json_copy(json_get(history, i - 1)));
    } else if (hrd_is_obj(history)) {
        for (size_t i = history->c.count; i > 0; i--) json_append(items, json_string(history->c.keys[i - 1]));
    } else if (hrd_is_str(history)) {
        const char *s = history->str_val ? history->str_val : "";
        size_t n = strlen(s);
        for (size_t i = n; i > 0; i--) {
            char ch[2] = { s[i - 1], '\0' };
            json_append(items, json_string(ch));
        }
    } else {
        char buf[160];
        snprintf(buf, sizeof(buf), "'%s' object is not reversible", hrd_type_name(history));
        json_free(items);
        return hrd_raise_spec("TypeError", buf);
    }
    json_t *ret = NULL;
    for (size_t i = 0; i < json_len(items); i++) {
        const json_t *message = json_get(items, i);
        json_t *err = NULL;
        const json_t *mtid = hrd_map_get(message, "task_id", &err);
        if (err) { ret = err; break; }
        const json_t *itid = hrd_attr(identity, "task_id", &err);
        if (err) { ret = err; break; }
        if (!hrd_json_eq(mtid, itid)) continue;
        const json_t *mgen = hrd_map_get(message, "execution_generation", &err);
        if (err) { ret = err; break; }
        /* numeric equality (int/float cross-equal; domain excludes bool) */
        bool gen_eq = hrd_is_num(mgen) && mgen->num_val == gen;
        if (!gen_eq) continue;
        const json_t *role = hrd_map_get(message, "role", &err);
        if (err) { ret = err; break; }
        if (!hrd_str(role) || strcmp(hrd_str(role), "assistant") != 0) continue;
        const json_t *status = hrd_map_get(message, "status", &err);
        if (err) { ret = err; break; }
        const char *st = hrd_str(status);
        if (!st || (strcmp(st, "settled") != 0 && strcmp(st, "failed") != 0)) continue;
        const json_t *mid = hrd_map_get(message, "message_id", &err);
        if (err) { ret = err; break; }
        const char *receipt_id = hrd_str(mid);
        char ridbuf[512];
        if (!receipt_id || !receipt_id[0]) {
            snprintf(ridbuf, sizeof(ridbuf), "reply:%s:%.0f", (hrd_str(itid) ? hrd_str(itid) : ""), gen);
            receipt_id = ridbuf;
        }
        json_t *src = json_object();
        json_set(src, "message_id", json_string(receipt_id));
        const json_t *content = hrd_map_get(message, "content", &err);
        if (err) { json_free(src); ret = err; break; }
        json_set(src, "text", content ? json_copy(content) : json_string(""));
        json_t *result = hrd_bounded_result(src);
        json_free(src);
        if (hrd_is_raise(result)) { ret = result; break; }
        ret = json_object();
        json_set(ret, "status", json_string(st));
        json_set(ret, "settlement_id", json_string(receipt_id));
        json_set(ret, "result", result);
        break;
    }
    json_free(items);
    return ret;
}
/* PoP: hrd_find_terminal_receipt @ tui_gateway/hosted_room_driver.py:_find_terminal_receipt */
static json_t *hrd_find_terminal_receipt(hrd_env *e, const json_t *in) {
    (void)e;
    return hrd_find_receipt(json_obj_get(in, "history"), json_obj_get(in, "identity"),
                            json_get_num(in, "execution_generation", 0));
}

/* _info_active(info): bool(info.get("active", info.get("running", False))) */
static json_t *hrd_info_active_of(const json_t *info) {
    json_t *err = NULL;
    const json_t *v = hrd_map_get(info, "active", &err);
    if (err) return err;
    if (!json_has(info, "active")) {
        v = hrd_map_get(info, "running", &err);
        if (err) return err;
    }
    return json_bool(hrd_truth(v));
}
/* PoP: hrd_info_active @ tui_gateway/hosted_room_driver.py:_info_active */
static json_t *hrd_info_active(hrd_env *e, const json_t *in) {
    (void)e;
    return hrd_info_active_of(json_obj_get(in, "info"));
}

/* _info_is_active_for(info, identity, require_exact=False) */
static json_t *hrd_info_active_for(const json_t *info, const json_t *identity, bool require_exact) {
    json_t *err = NULL;
    /* accepted tuple is built FIRST (identity.task_id read before the probe) */
    const json_t *itid = hrd_attr(identity, "task_id", &err);
    if (err) return err;
    json_t *act = hrd_info_active_of(info);
    if (hrd_is_raise(act)) return act;
    bool active = act->bool_val;
    json_free(act);
    if (!active) return json_bool(false);
    const json_t *tid = hrd_map_get(info, "task_id", &err);
    if (err) return err;
    bool member = hrd_json_eq(tid, itid) || (!require_exact && hrd_is_null(tid));
    return json_bool(member);
}
/* PoP: hrd_info_is_active_for @ tui_gateway/hosted_room_driver.py:_info_is_active_for */
static json_t *hrd_info_is_active_for(hrd_env *e, const json_t *in) {
    (void)e;
    return hrd_info_active_for(json_obj_get(in, "info"), json_obj_get(in, "identity"),
                               json_get_bool(in, "require_exact", false));
}

/* ── small runtime methods ────────────────────────────────────────────────── */
/* wakeup(): internal state step (no seam) */
static void hrd_wakeup_of(hrd_env *e) {
    json_t *resched = json_obj_get(e->state, "reschedule");
    json_t *threads = json_obj_get(e->state, "room_threads");
    for (size_t i = 0; i < json_len(threads); i++)
        hrd_set_add(resched, hrd_sval(json_get(threads, i)));
    json_set(e->state, "wake_set", json_bool(true));
}

/* _remote(task) */
static json_t *hrd_remote_of(hrd_env *e, const json_t *task) {
    if (!hrd_has_cb(e, "remote_member")) return json_bool(false);
    json_t *err = NULL;
    const json_t *ident = hrd_sub(task, "identity", &err);
    if (err) return err;
    const json_t *rid = hrd_attr(ident, "room_id", &err);
    if (err) return err;
    json_t *member = hrd_member_id_of(task);
    if (hrd_is_raise(member)) return member;
    json_t *args = json_object();
    json_set(args, "room_id", json_copy(rid));
    json_set(args, "member_id", member);
    json_t *r = hrd_call(e, "remote_member", args);
    if (hrd_is_raise(r)) return r;
    bool b = hrd_truth(r);
    json_free(r);
    return json_bool(b);
}
/* PoP: hrd_remote @ tui_gateway/hosted_room_driver.py:_remote */
static json_t *hrd_remote(hrd_env *e, const json_t *in) {
    return hrd_remote_of(e, json_obj_get(in, "task"));
}

/* _publish(binding, task) */
static json_t *hrd_publish_of(hrd_env *e, const json_t *binding, const json_t *task) {
    if (hrd_has_cb(e, "publish_terminal")) {
        json_t *args = json_object();
        json_set(args, "binding", binding ? json_copy(binding) : json_null());
        json_set(args, "task", task ? json_copy(task) : json_null());
        json_t *r = hrd_call(e, "publish_terminal", args);
        if (hrd_is_raise(r)) return r;
        json_free(r);
    }
    return task ? json_copy(task) : NULL;
}
/* PoP: hrd_publish @ tui_gateway/hosted_room_driver.py:_publish */
static json_t *hrd_publish(hrd_env *e, const json_t *in) {
    return hrd_publish_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"));
}

/* _set_blocked(room_id, blocked) */
static void hrd_set_blocked_of(hrd_env *e, const char *room_id, bool blocked) {
    json_t *arr = json_obj_get(e->state, "blocked");
    if (blocked) hrd_set_add(arr, room_id);
    else hrd_set_del(arr, room_id);
}
/* PoP: hrd_set_blocked @ tui_gateway/hosted_room_driver.py:_set_blocked */
static json_t *hrd_set_blocked(hrd_env *e, const json_t *in) {
    hrd_set_blocked_of(e, json_get_str(in, "room_id", ""), json_get_bool(in, "blocked", false));
    return NULL;
}

/* _record_error(message) */
static void hrd_record_error_of(hrd_env *e, const char *msg) {
    json_set(e->state, "last_error", json_string(msg ? msg : ""));
}
/* PoP: hrd_record_error @ tui_gateway/hosted_room_driver.py:_record_error */
static json_t *hrd_record_error(hrd_env *e, const json_t *in) {
    const json_t *m = json_obj_get(in, "message");
    json_set(e->state, "last_error", m ? json_copy(m) : json_null());
    return NULL;
}

/* _record_task_error(attempt, message) */
static json_t *hrd_record_task_error_of(hrd_env *e, const json_t *attempt, const char *message) {
    json_t *err = NULL;
    const json_t *ident = hrd_attr(attempt, "identity", &err);
    if (err) return err;
    const json_t *tid = hrd_attr(ident, "task_id", &err);
    if (err) return err;
    char buf[1024];
    snprintf(buf, sizeof(buf), "task %s %s", (hrd_str(tid) ? hrd_str(tid) : ""), message ? message : "");
    hrd_record_error_of(e, buf);
    return NULL;
}
/* PoP: hrd_record_task_error @ tui_gateway/hosted_room_driver.py:_record_task_error */
static json_t *hrd_record_task_error(hrd_env *e, const json_t *in) {
    return hrd_record_task_error_of(e, json_obj_get(in, "attempt"), json_get_str(in, "message", ""));
}

/* _binding_for_room(room_id) */
static json_t *hrd_binding_for_room_of(hrd_env *e, const char *room_id) {
    json_t *rooms = json_obj_get(e->state, "rooms");
    for (size_t i = 0; i < json_len(rooms); i++) {
        const json_t *b = json_get(rooms, i);
        json_t *err = NULL;
        const json_t *rid = hrd_attr(b, "room_id", &err);
        if (err) return err;
        if (hrd_str(rid) && strcmp(hrd_str(rid), room_id) == 0) return json_copy(b);
    }
    return NULL;
}
/* PoP: hrd_binding_for_room @ tui_gateway/hosted_room_driver.py:_binding_for_room */
static json_t *hrd_binding_for_room(hrd_env *e, const json_t *in) {
    return hrd_binding_for_room_of(e, json_get_str(in, "room_id", ""));
}

/* _transport_for(binding, task): full double internally; entry projects {"id"} */
static json_t *hrd_transport_for_of(hrd_env *e, const json_t *binding, const json_t *task) {
    if (hrd_has_cb(e, "transport_resolver")) {
        json_t *args = json_object();
        json_set(args, "binding", binding ? json_copy(binding) : json_null());
        json_set(args, "task", task ? json_copy(task) : json_null());
        return hrd_call(e, "transport_resolver", args);
    }
    if (!hrd_has_cb(e, "rpc"))
        return hrd_raise_spec("RuntimeError", "hosted room transport is unavailable");
    json_t *r = json_object();
    json_set(r, "id", json_string(hrd_rpc_id(e)));
    return r;
}
static json_t *hrd_transport_proj(const json_t *t) {
    if (hrd_is_null(t)) return NULL;
    json_t *r = json_object();
    json_set(r, "id", json_copy(json_obj_get(t, "id")));
    return r;
}
/* PoP: hrd_transport_for @ tui_gateway/hosted_room_driver.py:_transport_for */
static json_t *hrd_transport_for(hrd_env *e, const json_t *in) {
    json_t *r = hrd_transport_for_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"));
    if (hrd_is_raise(r) || hrd_is_null(r)) return r;
    json_t *p = hrd_transport_proj(r);
    json_free(r);
    return p;
}

/* _tasks(binding, status) */
/* PoP: hrd_tasks @ tui_gateway/hosted_room_driver.py:_tasks */
static json_t *hrd_tasks(hrd_env *e, const json_t *in) {
    const json_t *binding = json_obj_get(in, "binding");
    json_t *err = NULL;
    const json_t *rid = hrd_attr(binding, "room_id", &err);
    if (err) return err;
    json_t *args = json_object();
    json_set(args, "placement", json_string(hrd_placement(e)));
    json_set(args, "room_id", json_copy(rid));
    const json_t *st = json_obj_get(in, "status");
    json_set(args, "status", st ? json_copy(st) : json_null());
    return hrd_call(e, "state.list_tasks", args);
}
/* ── retry backoff + deadline leaves ──────────────────────────────────────── */
/* route_retries record lookup by (room_id, member_id); borrowed or NULL */
static json_t *hrd_route_rec(hrd_env *e, const char *room_id, const char *member_id) {
    json_t *arr = json_obj_get(e->state, "route_retries");
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *rec = json_get(arr, i);
        if (strcmp(json_get_str(rec, "room_id", ""), room_id) == 0
            && strcmp(json_get_str(rec, "member_id", ""), member_id) == 0)
            return rec;
    }
    return NULL;
}
/* PoP: hrd_defer_unavailable_route @ tui_gateway/hosted_room_driver.py:_defer_unavailable_route */
static json_t *hrd_defer_unavailable_route(hrd_env *e, const json_t *in) {
    const json_t *task = json_obj_get(in, "task");
    json_t *err = NULL;
    const json_t *ident = hrd_sub(task, "identity", &err);
    if (err) return err;
    const json_t *rid = hrd_attr(ident, "room_id", &err);
    if (err) return err;
    json_t *member = hrd_member_id_of(task);
    if (hrd_is_raise(member)) return member;
    const char *room_id = (hrd_str(rid) ? hrd_str(rid) : "");
    const char *member_id = hrd_str(member) ? hrd_str(member) : "";
    json_t *prev = hrd_route_rec(e, room_id, member_id);
    double lo = hrd_cfg_num(e, "unavailable_retry_min_seconds", 1.0);
    double hi = hrd_cfg_num(e, "unavailable_retry_max_seconds", 30.0);
    double delay = lo;
    if (prev) {
        double d = json_get_num(prev, "delay", 0.0) * 2.0;
        if (d < lo) d = lo;
        if (d > hi) d = hi;
        delay = d;
    }
    json_t *clk = hrd_seam_obj(e, "clock");
    if (hrd_is_raise(clk)) { json_free(member); return clk; }
    double now = hrd_is_num(clk) ? clk->num_val : 0.0;
    json_free(clk);
    json_t *rec = json_object();
    json_set(rec, "room_id", json_string(room_id));
    json_set(rec, "member_id", json_string(member_id));
    json_set(rec, "delay", json_number(delay));
    json_set(rec, "next_attempt_at", json_number(now + delay));
    json_free(member);
    if (prev) {
        /* dict update keeps the key's insertion position: replace fields in place */
        json_set(prev, "delay", json_number(delay));
        json_set(prev, "next_attempt_at", json_number(now + delay));
        json_free(rec);
    } else {
        json_append(json_obj_get(e->state, "route_retries"), rec);
    }
    return json_number(delay);
}

/* PoP: hrd_past_execution_deadline @ tui_gateway/hosted_room_driver.py:_past_execution_deadline */
static json_t *hrd_past_execution_deadline(hrd_env *e, const json_t *in) {
    const json_t *task = json_obj_get(in, "task");
    json_t *err = NULL;
    const json_t *started = hrd_map_get(task, "started_at", &err);
    if (err) return err;
    if (hrd_is_null(started)) return json_bool(false);
    /* live evaluates self.clock() BEFORE float(started) (left-to-right) */
    json_t *clk = hrd_seam_obj(e, "clock");
    if (hrd_is_raise(clk)) return clk;
    double now = hrd_is_num(clk) ? clk->num_val : 0.0;
    json_free(clk);
    double f;
    err = hrd_pyfloat(started, &f);
    if (err) return err;
    double timeout = hrd_cfg_num(e, "turn_timeout_seconds", 1830.0);
    return json_bool(now >= f + timeout);
}

/* ── lease cluster ────────────────────────────────────────────────────────── */
/* normalize a lease double: reclaimed defaults to false */
static json_t *hrd_lease_norm(const json_t *lease) {
    if (hrd_is_null(lease)) return NULL;
    json_t *r = json_copy(lease);
    if (!json_has(r, "reclaimed")) json_set(r, "reclaimed", json_bool(false));
    return r;
}
/* _cache_lease(lease) */
static json_t *hrd_cache_lease_of(hrd_env *e, const json_t *lease_in) {
    json_t *lease = hrd_lease_norm(lease_in);
    json_t *err = NULL;
    const json_t *rid = hrd_attr(lease, "room_id", &err);
    if (err) { json_free(lease); return err; }
    const char *room_id = (hrd_str(rid) ? hrd_str(rid) : "");
    json_t *leases = hrd_leases(e);
    const json_t *known = json_obj_get(leases, room_id);
    if (known) {
        const json_t *kpg = json_obj_get(known, "process_generation");
        const json_t *lpg = json_obj_get(lease, "process_generation");
        double kgen = json_get_num(known, "lease_generation", -1);
        double lgen = json_get_num(lease, "lease_generation", -2);
        double kexp = json_get_num(known, "expires_at", 0);
        double lexp = json_get_num(lease, "expires_at", 0);
        if (hrd_json_eq(kpg, lpg) && kgen == lgen && kexp >= lexp) {
            json_t *r = json_copy(known);
            json_free(lease);
            return r;
        }
    }
    json_set(leases, room_id, json_copy(lease));
    return lease;
}
/* PoP: hrd_cache_lease @ tui_gateway/hosted_room_driver.py:_cache_lease */
static json_t *hrd_cache_lease(hrd_env *e, const json_t *in) {
    return hrd_cache_lease_of(e, json_obj_get(in, "lease"));
}

/* _drop_lease(room_id) */
static void hrd_drop_lease_of(hrd_env *e, const char *room_id) {
    json_obj_del(hrd_leases(e), room_id);
}
/* PoP: hrd_drop_lease @ tui_gateway/hosted_room_driver.py:_drop_lease */
static json_t *hrd_drop_lease(hrd_env *e, const json_t *in) {
    hrd_drop_lease_of(e, json_get_str(in, "room_id", ""));
    return NULL;
}

/* PoP: hrd_mark_ambiguous @ tui_gateway/hosted_room_driver.py:_mark_ambiguous */
static json_t *hrd_mark_ambiguous(hrd_env *e, const json_t *in) {
    const json_t *binding = json_obj_get(in, "binding");
    const json_t *attempt = json_obj_get(in, "attempt");
    json_t *err = NULL;
    const json_t *rid = hrd_attr(binding, "room_id", &err);
    if (err) return err;
    const char *room_id = (hrd_str(rid) ? hrd_str(rid) : "");
    json_t *leases = hrd_leases(e);
    json_t *known = json_obj_get(leases, room_id);
    json_t *known_cp = known ? json_copy(known) : NULL;
    json_obj_del(leases, room_id);
    const json_t *alease = hrd_attr(attempt, "lease", &err);
    if (err) { json_free(known_cp); return err; }
    const json_t *exp = hrd_attr(alease, "expires_at", &err);
    if (err) { json_free(known_cp); return err; }
    double expires_at = hrd_is_num(exp) ? exp->num_val : 0.0;
    if (known_cp) {
        double kgen = json_get_num(known_cp, "lease_generation", -1);
        double agen = json_get_num(alease, "lease_generation", -2);
        if (kgen == agen) {
            double kexp = json_get_num(known_cp, "expires_at", 0);
            if (kexp > expires_at) expires_at = kexp;
        }
    }
    json_free(known_cp);
    json_set(json_obj_get(e->state, "ambiguous"), room_id, json_number(expires_at));
    return NULL;
}

/* _renew_lease_if_needed(lease, force=False) */
static json_t *hrd_renew_of(hrd_env *e, const json_t *lease_in, bool force) {
    json_t *lease = hrd_lease_norm(lease_in);
    json_t *err = NULL;
    const json_t *rid = hrd_attr(lease, "room_id", &err);
    if (err) { json_free(lease); return err; }
    const char *room_id = (hrd_str(rid) ? hrd_str(rid) : "");
    const json_t *known = json_obj_get(hrd_leases(e), room_id);
    if (known) {
        double kgen = json_get_num(known, "lease_generation", -1);
        double lgen = json_get_num(lease, "lease_generation", -2);
        double kexp = json_get_num(known, "expires_at", 0);
        double lexp = json_get_num(lease, "expires_at", 0);
        if (kgen == lgen && kexp > lexp) {
            json_free(lease);
            lease = json_copy(known);
        }
    }
    double ttl = hrd_cfg_num(e, "lease_ttl_seconds", 30.0);
    if (!force) {
        json_t *clk = hrd_seam_obj(e, "clock");
        if (hrd_is_raise(clk)) { json_free(lease); return clk; }
        double now = hrd_is_num(clk) ? clk->num_val : 0.0;
        json_free(clk);
        double lexp = json_get_num(lease, "expires_at", 0);
        if (now < lexp - ttl / 2.0) return lease;
    }
    json_t *args = json_object();
    json_set(args, "lease", json_copy(lease));
    json_set(args, "placement", json_string(hrd_placement(e)));
    json_set(args, "ttl_seconds", json_number(ttl));
    json_free(lease);
    json_t *r = hrd_call(e, "state.renew_lease", args);
    if (hrd_is_raise(r)) return r;
    json_t *cached = hrd_cache_lease_of(e, r);
    json_free(r);
    return cached;
}
/* PoP: hrd_renew_lease_if_needed @ tui_gateway/hosted_room_driver.py:_renew_lease_if_needed */
static json_t *hrd_renew_lease_if_needed(hrd_env *e, const json_t *in) {
    return hrd_renew_of(e, json_obj_get(in, "lease"), json_get_bool(in, "force", false));
}

/* _ensure_lease(binding) */
static json_t *hrd_ensure_lease_of(hrd_env *e, const json_t *binding) {
    json_t *err = NULL;
    const json_t *rid = hrd_attr(binding, "room_id", &err);
    if (err) return err;
    const char *room_id = (hrd_str(rid) ? hrd_str(rid) : "");
    const json_t *current = json_obj_get(hrd_leases(e), room_id);
    if (current) {
        json_t *r = hrd_renew_of(e, current, false);
        if (hrd_is_raise(r)) {
            if (strcmp(hrd_kind(r), "StaleLeaseError") == 0) {
                json_free(r);
                hrd_drop_lease_of(e, room_id);
            } else {
                return r;
            }
        } else {
            return r;
        }
    }
    const json_t *gid = hrd_attr(binding, "gateway_id", &err);
    if (err) return err;
    const json_t *epoch = hrd_attr(binding, "authority_epoch", &err);
    if (err) return err;
    json_t *args = json_object();
    json_set(args, "placement", json_string(hrd_placement(e)));
    json_set(args, "room_id", json_string(room_id));
    json_set(args, "gateway_id", json_copy(gid));
    json_set(args, "authority_epoch", json_copy(epoch));
    json_set(args, "process_generation", json_string(json_get_str(hrd_cfg(e), "process_generation", "pg-test")));
    json_set(args, "ttl_seconds", json_number(hrd_cfg_num(e, "lease_ttl_seconds", 30.0)));
    json_t *lease = hrd_call(e, "state.acquire_lease", args);
    if (hrd_is_raise(lease)) return lease;
    json_t *cached = hrd_cache_lease_of(e, lease);
    json_free(lease);
    if (hrd_is_raise(cached)) return cached;
    /* recovered = {k for k in recovered if k[0] != room_id} */
    json_t *rec = json_obj_get(e->state, "recovered");
    json_t *keep = json_array();
    for (size_t i = 0; i < json_len(rec); i++) {
        const json_t *k = json_get(rec, i);
        const char *kr = hrd_sval(json_get(k, 0));
        if (strcmp(kr, room_id) != 0) json_append(keep, json_copy(k));
    }
    json_t **oi = rec->c.items; size_t on = rec->c.count;
    rec->c.items = keep->c.items; rec->c.count = keep->c.count;
    keep->c.items = oi; keep->c.count = on;
    json_free(keep);
    return cached;
}
/* PoP: hrd_ensure_lease @ tui_gateway/hosted_room_driver.py:_ensure_lease */
static json_t *hrd_ensure_lease(hrd_env *e, const json_t *in) {
    return hrd_ensure_lease_of(e, json_obj_get(in, "binding"));
}

/* PoP: hrd_release_idle_leases @ tui_gateway/hosted_room_driver.py:_release_idle_leases */
static json_t *hrd_release_idle_leases(hrd_env *e, const json_t *in) {
    (void)in;
    json_t *leases = hrd_leases(e);
    /* tuple(self._leases.items()) snapshot */
    json_t *snap = json_array();
    for (size_t i = 0; i < leases->c.count; i++) {
        json_t *item = json_object();
        json_set(item, "room_id", json_string(leases->c.keys[i]));
        json_set(item, "lease", json_copy(leases->c.items[i]));
        json_append(snap, item);
    }
    json_t *ret = NULL;
    for (size_t i = 0; i < json_len(snap); i++) {
        const json_t *item = json_get(snap, i);
        const char *room_id = json_get_str(item, "room_id", "");
        json_t *args = json_object();
        json_set(args, "lease", json_copy(json_obj_get(item, "lease")));
        json_set(args, "placement", json_string(hrd_placement(e)));
        json_t *r = hrd_call(e, "state.release_lease", args);
        if (hrd_is_raise(r)) {
            if (hrd_is_driver_state(hrd_kind(r))) { json_free(r); continue; }
            ret = r;
            break;
        }
        json_free(r);
        hrd_drop_lease_of(e, room_id);
    }
    json_free(snap);
    return ret;
}
/* ── fenced transitions ───────────────────────────────────────────────────── */
/* _fenced(op, binding, task, lease, publish=True, **extra) */
static json_t *hrd_fenced_of(hrd_env *e, const char *op_name, const json_t *binding,
                             const json_t *task, const json_t *lease, bool publish,
                             const json_t *extra) {
    json_t *fences = hrd_fences_of(task);
    if (hrd_is_raise(fences)) return fences;
    json_t *err = NULL;
    const json_t *ident = hrd_sub(task, "identity", &err);
    if (err) { json_free(fences); return err; }
    json_t *args = json_object();
    json_set(args, "identity", json_copy(ident));
    json_t *lease_n = hrd_lease_norm(lease);
    json_set(args, "lease", lease_n ? lease_n : json_null());
    json_set(args, "placement", json_string(hrd_placement(e)));
    json_set(args, "expected_execution_generation",
             json_copy(json_obj_get(fences, "expected_execution_generation")));
    json_set(args, "expected_cancel_generation",
             json_copy(json_obj_get(fences, "expected_cancel_generation")));
    json_free(fences);
    if (hrd_is_obj(extra)) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(args, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    char name[160];
    snprintf(name, sizeof(name), "state.%s", op_name);
    json_t *result = hrd_call(e, name, args);
    if (hrd_is_raise(result)) return result;
    if (publish && !hrd_is_null(binding)) {
        json_t *pub = hrd_publish_of(e, binding, result);
        json_free(result);
        return pub;
    }
    return result;
}
/* PoP: hrd_fenced @ tui_gateway/hosted_room_driver.py:_fenced */
static json_t *hrd_fenced(hrd_env *e, const json_t *in) {
    return hrd_fenced_of(e, json_get_str(in, "op", ""), json_obj_get(in, "binding"),
                         json_obj_get(in, "task"), json_obj_get(in, "lease"),
                         json_get_bool(in, "publish", true), json_obj_get(in, "extra"));
}

/* _complete_cancel(task, cancel_id=None) */
static json_t *hrd_complete_cancel_of(hrd_env *e, const json_t *task, const json_t *cancel_id) {
    json_t *err = NULL;
    const json_t *ident = hrd_sub(task, "identity", &err);
    if (err) return err;
    const json_t *cid;
    if (hrd_is_null(cancel_id)) {
        cid = hrd_sub(task, "cancel_id", &err);
        if (err) return err;
    } else {
        cid = cancel_id;
    }
    const json_t *ecg = hrd_sub(task, "cancel_generation", &err);
    if (err) return err;
    json_t *args = json_object();
    json_set(args, "identity", json_copy(ident));
    json_set(args, "placement", json_string(hrd_placement(e)));
    json_set(args, "cancel_id", json_copy(cid));
    json_set(args, "expected_cancel_generation", json_copy(ecg));
    return hrd_call(e, "state.complete_task_cancel", args);
}
/* PoP: hrd_complete_cancel @ tui_gateway/hosted_room_driver.py:_complete_cancel */
static json_t *hrd_complete_cancel(hrd_env *e, const json_t *in) {
    return hrd_complete_cancel_of(e, json_obj_get(in, "task"), json_obj_get(in, "cancel_id"));
}

/* PoP: hrd_requeue @ tui_gateway/hosted_room_driver.py:_requeue */
static json_t *hrd_requeue(hrd_env *e, const json_t *in) {
    const json_t *task = json_obj_get(in, "task");
    const json_t *lease = json_obj_get(in, "lease");
    const char *room_id = json_get_str(in, "room_id", "");
    json_t *retried = hrd_fenced_of(e, json_get_str(in, "requeue", ""), NULL, task, lease, true, NULL);
    if (hrd_is_raise(retried)) return retried;
    if (hrd_has_cb(e, "retire_task_progress")) {
        json_t *err = NULL;
        const json_t *ident = hrd_sub(task, "identity", &err);
        if (err) { json_free(retried); return err; }
        const json_t *tid = hrd_attr(ident, "task_id", &err);
        if (err) { json_free(retried); return err; }
        const json_t *eg = hrd_sub(task, "execution_generation", &err);
        if (err) { json_free(retried); return err; }
        double gen;
        err = hrd_pyint(eg, &gen);
        if (err) { json_free(retried); return err; }
        json_t *args = json_object();
        json_set(args, "room_id", json_string(room_id));
        json_set(args, "task_id", json_copy(tid));
        json_set(args, "execution_generation", json_number(gen));
        json_t *r = hrd_call(e, "retire_task_progress", args);
        if (hrd_is_raise(r)) { json_free(retried); return r; }
        json_free(r);
    }
    hrd_set_blocked_of(e, room_id, false);
    hrd_wakeup_of(e);
    return retried;
}

/* PoP: hrd_resolve_indeterminate @ tui_gateway/hosted_room_driver.py:_resolve_indeterminate */
static json_t *hrd_resolve_indeterminate(hrd_env *e, const json_t *in) {
    const json_t *terminal = json_obj_get(in, "terminal");
    json_t *extra = terminal ? json_copy(terminal) : json_object();
    json_t *r = hrd_fenced_of(e, "resolve_indeterminate_task", json_obj_get(in, "binding"),
                            json_obj_get(in, "task"), json_obj_get(in, "lease"),
                            json_get_bool(in, "publish", true), extra);
    json_free(extra);
    return r;
}

/* ── session open / probe cluster ─────────────────────────────────────────── */
static json_t *hrd_transport_args(const json_t *transport, const char *profile,
                                  const char *title_or_sid, bool is_session) {
    json_t *args = json_object();
    json_set(args, "id", json_copy(json_obj_get(transport, "id")));
    json_set(args, "profile", profile ? json_string(profile) : json_null());
    json_set(args, is_session ? "session_id" : "title",
             title_or_sid ? json_string(title_or_sid) : json_null());
    json_set(args, "source", json_string(HRD_ROOM_SOURCE));
    return args;
}
/* _resolve_or_create(transport, profile, room_id, create=True) */
static json_t *hrd_resolve_or_create_of(hrd_env *e, const json_t *transport,
                                        const char *profile, const char *room_id, bool create) {
    char title[512];
    snprintf(title, sizeof(title), "Group: %s", room_id ? room_id : "");
    json_t *args = hrd_transport_args(transport, profile, title, false);
    json_t *session = hrd_call(e, "transport.resolve_exact", args);
    if (hrd_is_raise(session)) return session;
    if (hrd_is_null(session)) {
        if (!create) return NULL;
        json_t *args2 = hrd_transport_args(transport, profile, title, false);
        return hrd_call(e, "transport.create", args2);
    }
    json_t *sid = hrd_session_id_of(session);
    json_free(session);
    if (hrd_is_raise(sid)) return sid;
    json_t *args2 = hrd_transport_args(transport, profile, hrd_str(sid), true);
    json_free(sid);
    return hrd_call(e, "transport.resume", args2);
}
/* PoP: hrd_resolve_or_create @ tui_gateway/hosted_room_driver.py:_resolve_or_create */
static json_t *hrd_resolve_or_create(hrd_env *e, const json_t *in) {
    return hrd_resolve_or_create_of(e, json_obj_get(in, "transport"),
                                    json_get_str(in, "profile", ""),
                                    json_get_str(in, "room_id", ""),
                                    json_get_bool(in, "create", true));
}

/* _resume_exact(transport, room_id, profile) */
static json_t *hrd_resume_exact_of(hrd_env *e, const json_t *transport,
                                   const char *room_id, const char *profile) {
    json_t *session = hrd_resolve_or_create_of(e, transport, profile, room_id, false);
    if (hrd_is_raise(session)) return session;
    if (hrd_is_null(session)) return NULL;
    json_t *sid = hrd_session_id_of(session);
    json_free(session);
    return sid;
}
/* PoP: hrd_resume_exact @ tui_gateway/hosted_room_driver.py:_resume_exact */
static json_t *hrd_resume_exact(hrd_env *e, const json_t *in) {
    return hrd_resume_exact_of(e, json_obj_get(in, "transport"),
                               json_get_str(in, "room_id", ""), json_get_str(in, "profile", ""));
}

/* _open_session(binding, task, peer_only=False) -> [transport_proj, profile, sid] */
static json_t *hrd_open_session_of(hrd_env *e, const json_t *binding, const json_t *task,
                                   bool peer_only) {
    json_t *transport = hrd_transport_for_of(e, binding, task);
    if (hrd_is_raise(transport)) return transport;
    bool is_rpc = !hrd_is_null(transport)
        && strcmp(json_get_str(transport, "id", ""), hrd_rpc_id(e)) == 0;
    json_t *r = json_array();
    if (hrd_is_null(transport) || (peer_only && is_rpc)) {
        json_t *tp = hrd_transport_proj(transport);
        json_append(r, tp ? tp : json_null());
        json_append(r, json_null());
        json_append(r, json_null());
        json_free(transport);
        return r;
    }
    json_t *err = NULL;
    const json_t *payload = hrd_sub(task, "payload", &err);
    if (err) { json_free(transport); json_free(r); return err; }
    const json_t *profile = hrd_sub(payload, "target_profile", &err);
    if (err) { json_free(transport); json_free(r); return err; }
    const json_t *brid = hrd_attr(binding, "room_id", &err);
    if (err) { json_free(transport); json_free(r); return err; }
    json_t *sid = hrd_resume_exact_of(e, transport, (hrd_str(brid) ? hrd_str(brid) : ""),
                                      hrd_str(profile) ? hrd_str(profile) : "");
    if (hrd_is_raise(sid)) { json_free(transport); json_free(r); return sid; }
    json_append(r, hrd_transport_proj(transport));
    json_append(r, profile ? json_copy(profile) : json_null());
    json_append(r, sid ? sid : json_null());
    json_free(transport);
    return r;
}
/* PoP: hrd_open_session @ tui_gateway/hosted_room_driver.py:_open_session */
static json_t *hrd_open_session(hrd_env *e, const json_t *in) {
    return hrd_open_session_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"),
                               json_get_bool(in, "peer_only", false));
}

/* _terminal_from_history(transport, profile, session_id, task) */
static json_t *hrd_terminal_from_history_of(hrd_env *e, const json_t *transport,
                                            const char *profile, const char *session_id,
                                            const json_t *task) {
    json_t *args = hrd_transport_args(transport, profile, session_id, true);
    json_t *history = hrd_call(e, "transport.history", args);
    if (hrd_is_raise(history)) return history;
    json_t *err = NULL;
    const json_t *ident = hrd_sub(task, "identity", &err);
    if (err) { json_free(history); return err; }
    const json_t *eg = hrd_sub(task, "execution_generation", &err);
    if (err) { json_free(history); return err; }
    double gen;
    err = hrd_pyint(eg, &gen);
    if (err) { json_free(history); return err; }
    json_t *r = hrd_find_receipt(history, ident, gen);
    json_free(history);
    return r;
}
/* PoP: hrd_terminal_from_history @ tui_gateway/hosted_room_driver.py:_terminal_from_history */
static json_t *hrd_terminal_from_history(hrd_env *e, const json_t *in) {
    return hrd_terminal_from_history_of(e, json_obj_get(in, "transport"),
                                        json_get_str(in, "profile", ""),
                                        json_get_str(in, "session_id", ""),
                                        json_obj_get(in, "task"));
}
/* ── Stop-acknowledgement / reporting flows ───────────────────────────────── */
/* str(v or "") for scalar v; NULL return = out of str() domain */
static char *hrd_pystr_or_empty(const json_t *v) {
    if (!hrd_truth(v)) return strdup("");
    return hrd_pystr(v);
}
/* PoP: hrd_peer_stop_acknowledged @ tui_gateway/hosted_room_driver.py:_peer_stop_acknowledged */
static json_t *hrd_peer_stop_acknowledged(hrd_env *e, const json_t *in) {
    const json_t *binding = json_obj_get(in, "binding");
    const json_t *task = json_obj_get(in, "task");
    json_t *opened = hrd_open_session_of(e, binding, task, true);
    if (hrd_is_raise(opened)) return opened;
    const json_t *sid = json_get(opened, 2);
    if (hrd_is_null(sid)) { json_free(opened); return json_bool(false); }
    const json_t *tproj = json_get(opened, 0);
    const json_t *profile = json_get(opened, 1);
    json_t *args = hrd_transport_args(tproj, hrd_str(profile), hrd_str(sid), true);
    json_t *info = hrd_call(e, "transport.info", args);
    json_free(opened);
    if (hrd_is_raise(info)) return info;
    json_t *act = hrd_info_active_of(info);
    if (hrd_is_raise(act)) { json_free(info); return act; }
    bool active = act->bool_val;
    json_free(act);
    if (active) { json_free(info); return json_bool(false); }
    /* str(info.get("status") or "") in _STOP_ACK_STATUSES */
    json_t *err = NULL;
    const json_t *st = hrd_map_get(info, "status", &err);
    if (err) { json_free(info); return err; }
    char *st_s = hrd_pystr_or_empty(st);
    if (!st_s) { json_free(info); return hrd_raise_spec("TypeError", "str() domain: scalar expected"); }
    bool ack = strcmp(st_s, "cancelled") == 0 || strcmp(st_s, "interrupted") == 0;
    free(st_s);
    if (!ack) { json_free(info); return json_bool(false); }
    /* str(info.get("task_id") or "") == task["identity"].task_id */
    const json_t *itid = hrd_map_get(info, "task_id", &err);
    if (err) { json_free(info); return err; }
    char *tid_s = hrd_pystr_or_empty(itid);
    if (!tid_s) { json_free(info); return hrd_raise_spec("TypeError", "str() domain: scalar expected"); }
    const json_t *ident = hrd_sub(task, "identity", &err);
    if (err) { free(tid_s); json_free(info); return err; }
    const json_t *want_tid = hrd_attr(ident, "task_id", &err);
    if (err) { free(tid_s); json_free(info); return err; }
    bool tid_ok = strcmp(tid_s, hrd_str(want_tid) ? hrd_str(want_tid) : "") == 0;
    free(tid_s);
    if (!tid_ok) { json_free(info); return json_bool(false); }
    /* int(info.get("execution_generation") or 0) == int(task["execution_generation"]) */
    const json_t *igen = hrd_map_get(info, "execution_generation", &err);
    if (err) { json_free(info); return err; }
    double ig;
    json_t *zero = json_number(0);
    err = hrd_pyint(hrd_truth(igen) ? igen : zero, &ig);
    json_free(zero);
    if (err) { json_free(info); return err; }
    const json_t *tgen = hrd_sub(task, "execution_generation", &err);
    if (err) { json_free(info); return err; }
    double tg;
    err = hrd_pyint(tgen, &tg);
    if (err) { json_free(info); return err; }
    json_free(info);
    return json_bool(ig == tg);
}

/* PoP: hrd_report_pending_action @ tui_gateway/hosted_room_driver.py:_report_pending_action */
static json_t *hrd_report_pending_action(hrd_env *e, const json_t *in) {
    const json_t *task = json_obj_get(in, "task");
    const json_t *info = json_obj_get(in, "info");
    const json_t *session_id = json_obj_get(in, "session_id");
    json_t *err = NULL;
    if (hrd_has_cb(e, "task_progress")) {
        const json_t *ident = hrd_sub(task, "identity", &err);
        if (err) return err;
        const json_t *rid = hrd_attr(ident, "room_id", &err);
        if (err) return err;
        const json_t *tid = hrd_attr(ident, "task_id", &err);
        if (err) return err;
        const json_t *eg = hrd_sub(task, "execution_generation", &err);
        if (err) return err;
        double gen;
        err = hrd_pyint(eg, &gen);
        if (err) return err;
        json_t *args = json_object();
        json_set(args, "room_id", json_copy(rid));
        json_set(args, "task_id", json_copy(tid));
        json_set(args, "execution_generation", json_number(gen));
        json_set(args, "info", info ? json_copy(info) : json_null());
        json_t *r = hrd_call(e, "task_progress", args);
        if (hrd_is_raise(r)) return r;
        json_free(r);
    }
    if (!hrd_has_cb(e, "pending_action")) return NULL;
    const json_t *approval = hrd_map_get(info, "pending_approval", &err);
    if (err) return err;
    if (!hrd_truth(approval)) {
        approval = hrd_map_get(info, "approval", &err);
        if (err) return err;
    }
    json_t *action = NULL;
    if (hrd_is_obj(approval)) {
        const json_t *choices_v = json_obj_get(approval, "choices");
        json_t *choices = json_array();
        if (hrd_truth(choices_v)) {
            /* iterate list items / str chars / dict keys (live comprehension) */
            json_t *iter = json_array();
            if (hrd_is_arr(choices_v)) {
                for (size_t i = 0; i < json_len(choices_v); i++)
                    json_append(iter, json_copy(json_get(choices_v, i)));
            } else if (hrd_is_str(choices_v)) {
                const char *s = choices_v->str_val ? choices_v->str_val : "";
                for (size_t i = 0; s[i]; i++) {
                    char ch[2] = { s[i], '\0' };
                    json_append(iter, json_string(ch));
                }
            } else if (hrd_is_obj(choices_v)) {
                for (size_t i = 0; i < choices_v->c.count; i++)
                    json_append(iter, json_string(choices_v->c.keys[i]));
            } else {
                json_free(iter);
                json_free(choices);
                char buf[160];
                snprintf(buf, sizeof(buf), "'%s' object is not iterable", hrd_type_name(choices_v));
                return hrd_raise_spec("TypeError", buf);
            }
            for (size_t i = 0; i < json_len(iter); i++) {
                const json_t *c = json_get(iter, i);
                if (hrd_is_obj(c) || hrd_is_arr(c)) {
                    json_free(iter);
                    json_free(choices);
                    char buf[192];
                    snprintf(buf, sizeof(buf), "cannot use '%s' as a set element (unhashable type: '%s')",
                             hrd_type_name(c), hrd_type_name(c));
                    return hrd_raise_spec("TypeError", buf);
                }
                const char *cs = hrd_str(c);
                if (cs && (strcmp(cs, "once") == 0 || strcmp(cs, "deny") == 0))
                    json_append(choices, json_copy(c));
            }
            json_free(iter);
        }
        if (json_len(choices) == 0) {
            json_free(choices);
            choices = json_array();
            json_append(choices, json_string("once"));
            json_append(choices, json_string("deny"));
        }
        json_t *safe = json_copy(approval);
        json_set(safe, "choices", choices);
        const json_t *ident = hrd_sub(task, "identity", &err);
        if (err) { json_free(safe); return err; }
        const json_t *tid = hrd_attr(ident, "task_id", &err);
        if (err) { json_free(safe); return err; }
        const json_t *eg = hrd_sub(task, "execution_generation", &err);
        if (err) { json_free(safe); return err; }
        double gen;
        err = hrd_pyint(eg, &gen);
        if (err) { json_free(safe); return err; }
        action = json_object();
        json_set(action, "kind", json_string("approval"));
        json_set(action, "task_id", json_copy(tid));
        json_set(action, "execution_generation", json_number(gen));
        const json_t *run_id = hrd_map_get(info, "run_id", &err);
        if (err) { json_free(safe); json_free(action); return err; }
        json_set(action, "run_id", run_id ? json_copy(run_id) : json_null());
        json_set(action, "session_id", session_id ? json_copy(session_id) : json_null());
        const json_t *req = json_obj_get(safe, "request_id");
        json_set(action, "request_id", req ? json_copy(req) : json_null());
        json_set(action, "approval", safe);
    }
    const json_t *ident = hrd_sub(task, "identity", &err);
    if (err) { json_free(action); return err; }
    const json_t *rid = hrd_attr(ident, "room_id", &err);
    if (err) { json_free(action); return err; }
    json_t *member = hrd_member_id_of(task);
    if (hrd_is_raise(member)) { json_free(action); return member; }
    json_t *args = json_object();
    json_set(args, "room_id", json_copy(rid));
    json_set(args, "member_id", member);
    json_set(args, "action", action ? action : json_null());
    json_t *r = hrd_call(e, "pending_action", args);
    if (hrd_is_raise(r)) return r;
    json_free(r);   /* live discards the callback's return; the method returns None */
    return NULL;
}

/* _served_elsewhere(binding, task) */
static json_t *hrd_served_elsewhere_of(hrd_env *e, const json_t *binding, const json_t *task) {
    json_t *rem = hrd_remote_of(e, task);
    if (hrd_is_raise(rem)) return rem;   /* NOT inside the try */
    bool remote = rem->bool_val;
    json_free(rem);
    if (remote) return json_bool(true);
    json_t *t = hrd_transport_for_of(e, binding, task);
    if (hrd_is_raise(t)) {
        if (hrd_swallow_exc(t)) return t;
        return json_bool(true);
    }
    bool flag = hrd_truth(json_obj_get(t, "served_elsewhere"));
    json_free(t);
    return json_bool(flag);
}
/* PoP: hrd_served_elsewhere @ tui_gateway/hosted_room_driver.py:_served_elsewhere */
static json_t *hrd_served_elsewhere(hrd_env *e, const json_t *in) {
    return hrd_served_elsewhere_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"));
}

/* PoP: hrd_defer_unconfirmed_stop @ tui_gateway/hosted_room_driver.py:_defer_unconfirmed_stop */
static json_t *hrd_defer_unconfirmed_stop(hrd_env *e, const json_t *in) {
    const json_t *binding = json_obj_get(in, "binding");
    const json_t *task = json_obj_get(in, "task");
    /* live shadows the lease param with _ensure_lease's result before any use */
    json_t *served = hrd_served_elsewhere_of(e, binding, task);
    if (hrd_is_raise(served)) return served;
    bool is_served = served->bool_val;
    json_free(served);
    if (!is_served) return json_bool(false);
    /* clock() first, then float(task.get("updated_at") or 0) */
    json_t *clk = hrd_seam_obj(e, "clock");
    if (hrd_is_raise(clk)) return clk;
    double now = hrd_is_num(clk) ? clk->num_val : 0.0;
    json_free(clk);
    json_t *err = NULL;
    const json_t *upd = hrd_map_get(task, "updated_at", &err);
    if (err) return err;
    double f;
    json_t *zero = json_number(0);
    err = hrd_pyfloat(hrd_truth(upd) ? upd : zero, &f);
    json_free(zero);
    if (err) return err;
    double defer_s = hrd_cfg_num(e, "indeterminate_defer_seconds", 60.0);
    if (now < f + defer_s) return json_bool(false);
    json_t *lease2 = hrd_ensure_lease_of(e, binding);
    if (hrd_is_raise(lease2)) return lease2;
    json_t *extra = json_object();
    json_set(extra, "reason", json_string("member_unavailable"));
    json_t *res = hrd_fenced_of(e, "defer_unconfirmed_stop", NULL, task, lease2, true, extra);
    json_free(extra);
    json_free(lease2);
    if (hrd_is_raise(res)) return res;
    json_t *pub = hrd_publish_of(e, binding, res);
    json_free(res);
    if (hrd_is_raise(pub)) return pub;
    json_free(pub);
    return json_bool(true);
}

/* PoP: hrd_complete_acknowledged_stop @ tui_gateway/hosted_room_driver.py:_complete_acknowledged_stop */
static json_t *hrd_complete_acknowledged_stop(hrd_env *e, const json_t *in) {
    const json_t *binding = json_obj_get(in, "binding");
    const json_t *task = json_obj_get(in, "task");
    const json_t *lease = json_obj_get(in, "lease");
    json_t *err = NULL;
    const json_t *cid = hrd_map_get(task, "cancel_id", &err);
    if (err) return err;
    char *cid_s = hrd_pystr_or_empty(cid);
    if (!cid_s) return hrd_raise_spec("TypeError", "str() domain: scalar expected");
    bool deadline = strncmp(cid_s, "deadline:", 9) == 0;
    free(cid_s);
    if (!deadline) return hrd_complete_cancel_of(e, task, NULL);
    const json_t *eg = hrd_sub(task, "execution_generation", &err);
    if (err) return err;
    double gen;
    err = hrd_pyint(eg, &gen);
    if (err) return err;
    char sidbuf[160];
    snprintf(sidbuf, sizeof(sidbuf), "deadline:%.0f", gen);
    json_t *result = json_object();
    json_set(result, "error", json_string(HRD_DEADLINE_ERROR));
    json_set(result, "reason_code", json_string("turn_deadline_exceeded"));
    json_set(result, "timeout_seconds", json_number(hrd_cfg_num(e, "turn_timeout_seconds", 1830.0)));
    json_t *extra = json_object();
    json_set(extra, "settlement_id", json_string(sidbuf));
    json_set(extra, "status", json_string("failed"));
    json_set(extra, "result", result);
    json_t *r = hrd_fenced_of(e, "settle_stopping_task", binding, task, lease, true, extra);
    json_free(extra);
    return r;
}

/* PoP: hrd_settle_failure_if_current @ tui_gateway/hosted_room_driver.py:_settle_failure_if_current */
static json_t *hrd_settle_failure_if_current(hrd_env *e, const json_t *in) {
    const json_t *attempt = json_obj_get(in, "attempt");
    const json_t *exc = json_obj_get(in, "exc");
    const char *exc_msg = hrd_msg(exc);
    json_t *err = NULL;
    const json_t *ident = hrd_attr(attempt, "identity", &err);
    if (err) return err;
    const json_t *tid = hrd_attr(ident, "task_id", &err);
    if (err) return err;
    const json_t *gen = hrd_attr(attempt, "execution_generation", &err);
    if (err) return err;
    char sidbuf[512];
    snprintf(sidbuf, sizeof(sidbuf), "failure:%s:%.0f",
             hrd_str(tid) ? hrd_str(tid) : "", hrd_is_num(gen) ? gen->num_val : 0.0);
    json_t *result = json_object();
    json_set(result, "error", json_string(exc_msg));
    /* attempt projection: identity + lease + generations */
    json_t *aproj = json_object();
    json_set(aproj, "identity", json_copy(ident));
    const json_t *alease = hrd_attr(attempt, "lease", &err);
    if (err) { json_free(result); json_free(aproj); return err; }
    json_t *alease_n = hrd_lease_norm(alease);
    json_set(aproj, "lease", alease_n ? alease_n : json_null());
    json_set(aproj, "execution_generation", json_copy(gen));
    const json_t *cg = hrd_attr(attempt, "cancel_generation", &err);
    if (err) { json_free(result); json_free(aproj); return err; }
    json_set(aproj, "cancel_generation", json_copy(cg));
    json_t *args = json_object();
    json_set(args, "attempt", aproj);
    json_set(args, "placement", json_string(hrd_placement(e)));
    json_set(args, "settlement_id", json_string(sidbuf));
    json_set(args, "status", json_string("failed"));
    json_set(args, "result", result);
    json_t *r = hrd_call(e, "state.settle_task", args);
    if (hrd_is_raise(r)) {
        if (hrd_is_driver_state(hrd_kind(r))) {
            json_free(r);   /* suppress(DriverStateError, RoomUnavailableError) */
        } else {
            return r;
        }
    } else {
        json_free(r);
    }
    char msgbuf[1024];
    snprintf(msgbuf, sizeof(msgbuf), "failed: %s", exc_msg);
    return hrd_record_task_error_of(e, attempt, msgbuf);
}

/* ── dispatch ─────────────────────────────────────────────────────────────── */
typedef json_t *(*hrd_entry_fn)(hrd_env *, const json_t *);
typedef struct { const char *name; hrd_entry_fn fn; } hrd_dispatch;
static const hrd_dispatch hrd_table[] = {
    {"_session_kw", hrd_session_kw},
    {"_fences", hrd_fences},
    {"room_session_title", hrd_room_session_title},
    {"_member_id", hrd_member_id},
    {"_session_id", hrd_session_id},
    {"_truncate_utf8", hrd_truncate_utf8},
    {"_bounded_terminal_result", hrd_bounded_terminal_result},
    {"_receipt_from", hrd_receipt_from_entry},
    {"discarded_receipt", hrd_discarded_receipt},
    {"_find_terminal_receipt", hrd_find_terminal_receipt},
    {"_info_active", hrd_info_active},
    {"_info_is_active_for", hrd_info_is_active_for},
    {"_remote", hrd_remote},
    {"_publish", hrd_publish},
    {"_set_blocked", hrd_set_blocked},
    {"_record_error", hrd_record_error},
    {"_record_task_error", hrd_record_task_error},
    {"_binding_for_room", hrd_binding_for_room},
    {"_transport_for", hrd_transport_for},
    {"_tasks", hrd_tasks},
    {"_defer_unavailable_route", hrd_defer_unavailable_route},
    {"_past_execution_deadline", hrd_past_execution_deadline},
    {"_cache_lease", hrd_cache_lease},
    {"_drop_lease", hrd_drop_lease},
    {"_mark_ambiguous", hrd_mark_ambiguous},
    {"_renew_lease_if_needed", hrd_renew_lease_if_needed},
    {"_ensure_lease", hrd_ensure_lease},
    {"_release_idle_leases", hrd_release_idle_leases},
    {"_fenced", hrd_fenced},
    {"_complete_cancel", hrd_complete_cancel},
    {"_requeue", hrd_requeue},
    {"_resolve_indeterminate", hrd_resolve_indeterminate},
    {"_resolve_or_create", hrd_resolve_or_create},
    {"_resume_exact", hrd_resume_exact},
    {"_open_session", hrd_open_session},
    {"_terminal_from_history", hrd_terminal_from_history},
    {"_peer_stop_acknowledged", hrd_peer_stop_acknowledged},
    {"_report_pending_action", hrd_report_pending_action},
    {"_served_elsewhere", hrd_served_elsewhere},
    {"_defer_unconfirmed_stop", hrd_defer_unconfirmed_stop},
    {"_complete_acknowledged_stop", hrd_complete_acknowledged_stop},
    {"_settle_failure_if_current", hrd_settle_failure_if_current},
};
