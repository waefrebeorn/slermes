/*
 * hosted_room_driver_flow2.c — FLOW port of the UNCLAIMED stop/cancel/recovery
 * remainder of tui_gateway/hosted_room_driver.py (the process-local hosted-room
 * worker). The 42 claimed siblings live in hosted_room_driver_flow.c and are
 * SEAMED here ("self.<name>" universal-seam calls); the pure module leaves are
 * transcribed inline as un-annotated helpers (no PoP re-claim).
 *
 * Ports the DECISION SEQUENCES of:
 *   stop cluster   — _stop_outcome, _discard_unknown_stop,
 *                    _interrupt_stale_admission, _interrupt_stopping_task,
 *                    _stop_or_settle, _finish_stop, _settle_stopping_completion,
 *                    _retry_stopping_tasks
 *   public ops     — cancel, retry_indeterminate, _retry_deferred,
 *                    deferred_stop_confirmed
 *   recovery       — _expire_attempt_deadline, _inspect_session,
 *                    _inspect_recovery_session, _inspect_local_recovery_session,
 *                    _inspect_abandoned_attempts, _harvest_previous_attempt
 * Ported functions call each other REAL (internal composition); every claimed
 * sibling, state op, transport method, injected callback, the clock and the
 * turn_lock context manager rides ONE universal seam.
 *
 * Modeling contract (shared with tests/oracle_hosted_room_driver_flow2.py):
 *  - Universal seam: json_t *(*call)(void *ud, const char *name, args).
 *    name identifies the live call; args is a BORROWED JSON object the stub
 *    copies into the trace; the result is a NEW json_t (NULL == None;
 *    {"__raise__":kind,"message":m} == a raised exception). The port owns args
 *    (hrd2_call frees them) and the result.
 *  - Seam names -> live calls:
 *      self.<name>                  -> a CLAIMED HostedRoomRuntime sibling
 *        (seamed; args projected, result materialized — table below)
 *      state.get_task / state.cancel_task / state.begin_task_cancel /
 *      state.settle_task            -> gateway.hosted_room_driver.<op>
 *      transport.resolve_exact / transport.info / transport.interrupt /
 *      transport.discard            -> InternalSessionRPC methods
 *      confirm_deferred_stop / task_progress -> injected callbacks (state.cb)
 *      turn_lock                    -> self.turn_lock(profile) context manager
 *      clock                        -> self.clock() (callable args NOT projected)
 *  - Claimed-sibling seam args / results:
 *      self._open_session(binding,task,peer_only) -> [transport,profile,sid]
 *      self._terminal_from_history(transport,profile,session_id,task) -> receipt|null
 *      self._fenced(op,binding,task,lease,publish,**extra) -> task
 *      self._complete_cancel(task,cancel_id) -> task
 *      self._peer_stop_acknowledged(binding,task) -> bool
 *      self._binding_for_room(room_id) -> binding|null
 *      self._ensure_lease(binding) / self._renew_lease_if_needed(lease,force) -> lease
 *      self._record_error(message) / self._set_blocked(room_id,blocked) -> null
 *      self._tasks(binding,status) -> [task]
 *      self._remote(task) / self._served_elsewhere(binding,task) -> bool
 *      self._transport_for(binding,task) -> transport
 *      self._resolve_indeterminate(binding,task,lease,terminal,publish) -> task
 *      self._requeue(requeue,task,lease,room_id) -> task
 *      self._defer_unconfirmed_stop(binding,task,lease) -> bool
 *      self._complete_acknowledged_stop(binding,task,lease) -> task
 *      self._resume_exact(transport,room_id,profile) -> sid|null
 *      self._report_pending_action(task,session_id,info) -> null
 *  - State mirror (in["state"], copied by the harness before mutation): the same
 *    shape as hosted_room_driver_flow (cfg / cb / placement / rpc_id / rooms /
 *    leases / blocked / ambiguous / route_retries / recovered / inspected /
 *    last_error / room_threads / reschedule / wake_set / stop_set). wakeup() is
 *    an inline state step (reschedule |= room_threads keys; wake_set = true).
 *    Seamed siblings do NOT mutate the mirror (their effects are abstracted);
 *    only wakeup and the inline recovery inspectors touch it.
 *  - Transport doubles carry {"id", "discard"(bool presence)}; `transport is
 *    self.rpc` compares ids against state.rpc_id. `callable(getattr(transport,
 *    "discard", None))` reads the discard flag. Seam args project transport to
 *    {"id"} only. DOMAIN: the oracle registers transport doubles per-id
 *    FIRST-WINS (a later same-id result keeps the first discard flag), so
 *    fixtures must not reuse one id with conflicting discard flags — use a
 *    fresh id (e.g. "peer2") when a second transport needs discard presence.
 *  - _TerminalReceipt / _RecoveryInspection travel as JSON objects
 *    {status,settlement_id,result} / {terminal,active,status}; isinstance checks
 *    read object-vs-bool. _NO_INSPECTION identity is an inline flag.
 *  - int()/float()/str() coercions transcribe CPython 3.14 semantics (ASCII
 *    digit domain, integral floats read as ints, finite numbers). `type(x) is
 *    int` gates (info/result execution_generation) require an integral JSON
 *    number (bool excluded; non-integral floats out of domain).
 *  - Exception kinds: the DriverStateError family is swallowed exactly where
 *    live suppress()es / except-clauses catch it; BaseException kinds
 *    (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) pass through
 *    every `except Exception` site.
 *  - Scripted seam results: a missing/exhausted script is fail-closed — the
 *    harness dispatcher RETURNS a {"__raise__":"RuntimeError",...} spec and
 *    records it, so the port propagates it like any scripted raise.
 *  - turn_lock: the seam result is a CM handle; an inert handle enters/exits
 *    silently; a {"__exit__":{__raise__...}} handle raises on exit, discarding
 *    the body's return (the raise becomes the function result).
 *  - Self-contained: the harness #includes this .c; every symbol is static and
 *    hrd2_*-prefixed; only libjson is linked.
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
static const char HRD2_ROOM_SOURCE[] = "bot_room";
static const char HRD2_TRUNC_NOTICE[] =
    "\n\n[Reply truncated. Ask the Bot to share the full result as a file.]";
#define HRD2_TRUNC_NOTICE_LEN 68
#define HRD2_MAX_TERMINAL_TEXT 65536
#define HRD2_MAX_ERROR_TEXT 4096
#define HRD2_CANCEL_ROUTE_RETRIES 8
static const char HRD2_STOP_PENDING[] = "stop retry remains pending: ";

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct hrd2_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
} hrd2_env;

/* ── JSON type / truthiness helpers ───────────────────────────────────────── */
static bool hrd2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool hrd2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool hrd2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool hrd2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool hrd2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *hrd2_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *hrd2_sval(const json_t *v) {
    const char *s = hrd2_str(v);
    return s ? s : "";
}
static bool hrd2_truth(const json_t *v) {
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
static const char *hrd2_type_name(const json_t *v) {
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
/* `type(x) is int`: integral JSON number, bool excluded (float domain excluded). */
static bool hrd2_type_is_int(const json_t *v) {
    if (!v || v->type != JSON_NUMBER) return false;
    double d = v->num_val;
    return d == (double)(long long)d && d < 9.2e18 && d > -9.2e18;
}

/* ── raise specs ──────────────────────────────────────────────────────────── */
static bool hrd2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && hrd2_is_str(json_obj_get(r, "__raise__"));
}
static const char *hrd2_kind(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static const char *hrd2_msg(const json_t *r) { return json_get_str(r, "message", ""); }
static bool hrd2_is_base(const char *k) {
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0 || strcmp(k, "CancelledError") == 0;
}
static bool hrd2_is_driver_state(const char *k) {
    return strcmp(k, "DriverStateError") == 0 || strcmp(k, "DriverValidationError") == 0
        || strcmp(k, "RoomUnavailableError") == 0 || strcmp(k, "LeaseHeldError") == 0
        || strcmp(k, "StaleLeaseError") == 0 || strcmp(k, "StaleTaskError") == 0
        || strcmp(k, "InvalidTaskTransitionError") == 0 || strcmp(k, "TaskConflictError") == 0;
}
static json_t *hrd2_raise_spec(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}
static json_t *hrd2_attr_raise(const json_t *v, const char *attr) {
    char buf[192];
    snprintf(buf, sizeof(buf), "'%s' object has no attribute '%s'", hrd2_type_name(v), attr);
    return hrd2_raise_spec("AttributeError", buf);
}
static json_t *hrd2_keyerror_raise(const char *key) {
    char buf[192];
    snprintf(buf, sizeof(buf), "'%s'", key);
    return hrd2_raise_spec("KeyError", buf);
}
static json_t *hrd2_subscript_raise(const json_t *v) {
    char buf[192];
    const char *t = hrd2_type_name(v);
    if (strcmp(t, "str") == 0)
        snprintf(buf, sizeof(buf), "string indices must be integers, not 'str'");
    else
        snprintf(buf, sizeof(buf), "'%s' object is not subscriptable", t);
    return hrd2_raise_spec("TypeError", buf);
}
/* `except Exception`: swallow plain Exception (free r), propagate BaseException.
 * Returns true when the caller must propagate (r stays owned). */
static bool hrd2_swallow_exc(json_t *r) {
    if (hrd2_is_base(hrd2_kind(r))) return true;
    json_free(r);
    return false;
}

/* ── universal seam call (args owned by port, freed after the call) ───────── */
static json_t *hrd2_call(hrd2_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}
static json_t *hrd2_seam_obj(hrd2_env *e, const char *name) {
    return hrd2_call(e, name, json_object());
}

/* ── deep JSON equality (order-independent objects) ───────────────────────── */
static bool hrd2_json_eq(const json_t *a, const json_t *b) {
    if (hrd2_is_null(a) && hrd2_is_null(b)) return true;
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL:   return true;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!hrd2_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !hrd2_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        }
    }
    return false;
}

/* ── state mirror access (borrowed) ───────────────────────────────────────── */
static json_t *hrd2_cfg(hrd2_env *e) { return json_obj_get(e->state, "cfg"); }
static json_t *hrd2_cb(hrd2_env *e) { return json_obj_get(e->state, "cb"); }
static bool hrd2_has_cb(hrd2_env *e, const char *name) {
    return json_get_bool(hrd2_cb(e), name, false);
}
static const char *hrd2_placement(hrd2_env *e) {
    return json_get_str(e->state, "placement", "shared");
}
static const char *hrd2_rpc_id(hrd2_env *e) {
    return json_get_str(e->state, "rpc_id", "rpc");
}
static const char *hrd2_process_generation(hrd2_env *e) {
    return json_get_str(hrd2_cfg(e), "process_generation", "pg-test");
}
static double hrd2_cfg_num(hrd2_env *e, const char *key, double def) {
    return json_get_num(hrd2_cfg(e), key, def);
}

/* ── sorted string-set helper (reschedule, via wakeup) ────────────────────── */
static void hrd2_set_add(json_t *arr, const char *s) {
    size_t n = json_len(arr);
    size_t pos = n;
    for (size_t i = 0; i < n; i++) {
        int cmp = strcmp(hrd2_sval(json_get(arr, i)), s);
        if (cmp == 0) return;
        if (cmp > 0) { pos = i; break; }
    }
    json_t *fresh = json_array();
    for (size_t i = 0; i < pos; i++) json_append(fresh, json_copy(json_get(arr, i)));
    json_append(fresh, json_string(s));
    for (size_t i = pos; i < n; i++) json_append(fresh, json_copy(json_get(arr, i)));
    json_t **oi = arr->c.items; size_t on = arr->c.count;
    arr->c.items = fresh->c.items; arr->c.count = fresh->c.count;
    fresh->c.items = oi; fresh->c.count = on;
    json_free(fresh);
}

/* ── UTF-8 / number coercion helpers (CPython 3.14, ASCII domain) ─────────── */
static unsigned long hrd2_cp(const char *s, size_t *adv) {
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
static bool hrd2_num_ws(unsigned long cp) {
    return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20
        || cp == 0x85 || cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)
        || cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
static char *hrd2_num_strip(const char *s) {
    size_t start = 0, end = 0, i = 0;
    bool any = false;
    while (s[i]) {
        size_t adv; unsigned long cp = hrd2_cp(s + i, &adv);
        if (!hrd2_num_ws(cp)) { if (!any) { start = i; any = true; } end = i + adv; }
        i += adv;
    }
    if (!any) return strdup("");
    char *r = (char *)malloc(end - start + 1);
    memcpy(r, s + start, end - start);
    r[end - start] = '\0';
    return r;
}
static bool hrd2_utf8_valid(const char *s, size_t len) {
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
static char *hrd2_pystr(const json_t *v) {
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
            return NULL;
        }
        default: return NULL;
    }
}
static char *hrd2_pystr_or_empty(const json_t *v) {
    if (!hrd2_truth(v)) return strdup("");
    return hrd2_pystr(v);
}
static char *hrd2_pyrepr(const char *s) {
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
            bool printable = (c >= 0xA1 && c <= 0xAC) || (c >= 0xAE);
            if (printable) r[o++] = (char)c;
            else o += (size_t)snprintf(r + o, cap - o, "\\x%02x", c);
        }
    }
    r[o++] = q;
    r[o] = '\0';
    return r;
}
static json_t *hrd2_pyint(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL)
        return hrd2_raise_spec("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return NULL; }
    if (v->type == JSON_NUMBER) { *out = trunc(v->num_val); return NULL; }
    if (v->type != JSON_STRING) {
        char buf[192];
        snprintf(buf, sizeof(buf),
            "int() argument must be a string, a bytes-like object or a real number, not '%s'",
            hrd2_type_name(v));
        return hrd2_raise_spec("TypeError", buf);
    }
    const char *orig = v->str_val ? v->str_val : "";
    char *clean = hrd2_num_strip(orig);
    char *p = clean;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
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
    char *rep = hrd2_pyrepr(orig);
    char buf[1024];
    snprintf(buf, sizeof(buf), "invalid literal for int() with base 10: %s", rep);
    free(rep);
    return hrd2_raise_spec("ValueError", buf);
}
static bool hrd2_ieq(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}
static bool hrd2_float_digits(char **pp, char *buf, size_t *nd, size_t cap, bool *invalid) {
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
static json_t *hrd2_pyfloat(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL)
        return hrd2_raise_spec("TypeError",
            "float() argument must be a string or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return NULL; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return NULL; }
    if (v->type != JSON_STRING) {
        char buf[192];
        snprintf(buf, sizeof(buf), "float() argument must be a string or a real number, not '%s'",
            hrd2_type_name(v));
        return hrd2_raise_spec("TypeError", buf);
    }
    const char *orig = v->str_val ? v->str_val : "";
    char *clean = hrd2_num_strip(orig);
    char *p = clean;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (hrd2_ieq(p, "inf") || hrd2_ieq(p, "infinity")) {
        *out = neg ? -INFINITY : INFINITY; free(clean); return NULL;
    }
    if (hrd2_ieq(p, "nan")) { *out = NAN; free(clean); return NULL; }
    char buf[512];
    size_t nd = 0;
    bool ok = true, inv = false;
    bool int_part = hrd2_float_digits(&p, buf, &nd, sizeof(buf), &inv);
    bool frac_part = false;
    if (*p == '.') {
        p++;
        if (nd < sizeof(buf) - 1) buf[nd++] = '.';
        frac_part = hrd2_float_digits(&p, buf, &nd, sizeof(buf), &inv);
    }
    if (inv || (!int_part && !frac_part)) ok = false;
    if (ok && (*p == 'e' || *p == 'E')) {
        p++;
        char esign = 0;
        if (*p == '+' || *p == '-') { esign = *p; p++; }
        char exp[128];
        size_t ne = 0;
        bool einf = false;
        if (!hrd2_float_digits(&p, exp, &ne, sizeof(exp), &einf) || einf) ok = false;
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
    char *rep = hrd2_pyrepr(orig);
    char msg[1024];
    snprintf(msg, sizeof(msg), "could not convert string to float: %s", rep);
    free(rep);
    return hrd2_raise_spec("ValueError", msg);
}

/* ── subscript / attribute access with live raise semantics ───────────────── */
static const json_t *hrd2_sub(const json_t *obj, const char *key, json_t **err) {
    *err = NULL;
    if (!hrd2_is_obj(obj)) { *err = hrd2_subscript_raise(obj); return NULL; }
    if (!json_has(obj, key)) { *err = hrd2_keyerror_raise(key); return NULL; }
    return json_obj_get(obj, key);
}
static const json_t *hrd2_attr(const json_t *obj, const char *attr, json_t **err) {
    *err = NULL;
    if (!hrd2_is_obj(obj)) { *err = hrd2_attr_raise(obj, attr); return NULL; }
    const json_t *v = json_obj_get(obj, attr);
    if (!v) { *err = hrd2_attr_raise(obj, attr); return NULL; }
    return v;
}
static const json_t *hrd2_map_get(const json_t *obj, const char *key, json_t **err) {
    *err = NULL;
    if (!hrd2_is_obj(obj)) { *err = hrd2_attr_raise(obj, "get"); return NULL; }
    return json_obj_get(obj, key);
}

/* ── inline pure leaves (claimed; transcribed without PoP re-claim) ───────── */
static json_t *hrd2_session_kw(const char *profile, const char *session_id) {
    json_t *r = json_object();
    json_set(r, "profile", profile ? json_string(profile) : json_null());
    json_set(r, "session_id", session_id ? json_string(session_id) : json_null());
    json_set(r, "source", json_string(HRD2_ROOM_SOURCE));
    return r;
}
static json_t *hrd2_fences_of(const json_t *task) {
    json_t *err = NULL;
    const json_t *eg = hrd2_sub(task, "execution_generation", &err);
    if (err) return err;
    const json_t *cg = hrd2_sub(task, "cancel_generation", &err);
    if (err) return err;
    double ieg, icg;
    err = hrd2_pyint(eg, &ieg);
    if (err) return err;
    err = hrd2_pyint(cg, &icg);
    if (err) return err;
    json_t *r = json_object();
    json_set(r, "expected_execution_generation", json_number(ieg));
    json_set(r, "expected_cancel_generation", json_number(icg));
    return r;
}
static json_t *hrd2_member_id_of(const json_t *task) {
    json_t *err = NULL;
    const json_t *payload = hrd2_map_get(task, "payload", &err);
    if (err) return err;
    const json_t *p = payload;
    json_t *empty = NULL;
    if (!hrd2_truth(p)) { empty = json_object(); p = empty; }
    if (!hrd2_is_obj(p)) { json_free(empty); return hrd2_attr_raise(p, "get"); }
    const json_t *v = json_obj_get(p, "target_member_id");
    if (!hrd2_truth(v)) v = json_obj_get(p, "target_profile");
    if (!hrd2_truth(v)) v = NULL;
    json_free(empty);
    char *s;
    if (!v) s = strdup("");
    else {
        s = hrd2_pystr(v);
        if (!s) return hrd2_raise_spec("TypeError", "str() domain: scalar expected");
    }
    json_t *r = json_string(s);
    free(s);
    return r;
}
static json_t *hrd2_session_id_of(const json_t *session) {
    json_t *err = NULL;
    const json_t *v = hrd2_map_get(session, "session_id", &err);
    if (err) return err;
    if (!json_has(session, "session_id")) {
        v = hrd2_map_get(session, "id", &err);
        if (err) return err;
    }
    const char *s = hrd2_str(v);
    if (!s || !s[0])
        return hrd2_raise_spec("ValueError", "session adapter returned no session_id");
    return json_string(s);
}
static json_t *hrd2_truncate(const json_t *value, long max_bytes) {
    const json_t *src = hrd2_truth(value) ? value : NULL;
    char *text = hrd2_pystr(src);
    if (!src) { free(text); text = strdup(""); }
    if (!text) return hrd2_raise_spec("TypeError", "str() domain: scalar expected");
    size_t n = strlen(text);
    json_t *r = json_array();
    if ((long)n <= max_bytes) {
        json_append(r, json_string(text));
        json_append(r, json_bool(false));
        free(text);
        return r;
    }
    long cut = max_bytes - HRD2_TRUNC_NOTICE_LEN;
    if (cut < 0) cut = 0;
    while (cut > 0) {
        if (hrd2_utf8_valid(text, (size_t)cut)) {
            char *piece = (char *)malloc((size_t)cut + HRD2_TRUNC_NOTICE_LEN + 1);
            memcpy(piece, text, (size_t)cut);
            memcpy(piece + cut, HRD2_TRUNC_NOTICE, HRD2_TRUNC_NOTICE_LEN + 1);
            json_append(r, json_string(piece));
            json_append(r, json_bool(true));
            free(piece); free(text);
            return r;
        }
        cut--;
    }
    json_append(r, json_string(HRD2_TRUNC_NOTICE + 2));
    json_append(r, json_bool(true));
    free(text);
    return r;
}
static json_t *hrd2_bounded_result(const json_t *receipt) {
    json_t *err = NULL;
    const json_t *text_v = hrd2_map_get(receipt, "text", &err);
    if (err) return err;
    if (!text_v) text_v = json_null();
    const json_t *error_v = hrd2_map_get(receipt, "error", &err);
    if (err) return err;
    if (!error_v) error_v = json_null();
    json_t *rt = hrd2_truncate(text_v, HRD2_MAX_TERMINAL_TEXT);
    if (hrd2_is_raise(rt)) return rt;
    json_t *re = hrd2_truncate(error_v, HRD2_MAX_ERROR_TEXT);
    if (hrd2_is_raise(re)) { json_free(rt); return re; }
    json_t *r = json_object();
    const json_t *mid = hrd2_is_obj(receipt) ? json_obj_get(receipt, "message_id") : NULL;
    json_set(r, "message_id", mid ? json_copy(mid) : json_null());
    json_set(r, "text", json_copy(json_get(rt, 0)));
    if (hrd2_truth(json_get(re, 0))) json_set(r, "error", json_copy(json_get(re, 0)));
    if (json_get(rt, 1)->bool_val || json_get(re, 1)->bool_val)
        json_set(r, "truncated", json_bool(true));
    json_free(rt); json_free(re);
    return r;
}
static json_t *hrd2_receipt_from(const json_t *receipt, const json_t *identity, double gen) {
    json_t *err = NULL;
    const json_t *st = hrd2_map_get(receipt, "status", &err);
    if (err) return err;
    const char *status = (hrd2_str(st) && strcmp(hrd2_str(st), "settled") == 0) ? "settled" : "failed";
    const json_t *sid = hrd2_map_get(receipt, "settlement_id", &err);
    if (err) return err;
    json_t *r = json_object();
    json_set(r, "status", json_string(status));
    if (hrd2_truth(sid)) {
        json_set(r, "settlement_id", json_copy(sid));
    } else {
        const json_t *tid = hrd2_attr(identity, "task_id", &err);
        if (err) { json_free(r); return err; }
        char buf[512];
        snprintf(buf, sizeof(buf), "reply:%s:%.0f", hrd2_sval(tid), gen);
        json_set(r, "settlement_id", json_string(buf));
    }
    json_t *res = hrd2_bounded_result(receipt);
    if (hrd2_is_raise(res)) { json_free(r); return res; }
    json_set(r, "result", res);
    return r;
}
static json_t *hrd2_discarded_receipt(const json_t *result, const json_t *task) {
    json_t *err = NULL;
    const json_t *st = hrd2_map_get(result, "status", &err);
    if (err) return err;
    const char *s = hrd2_str(st);
    if (!s || (strcmp(s, "settled") != 0 && strcmp(s, "failed") != 0)) return NULL;
    const json_t *ident = hrd2_sub(task, "identity", &err);
    if (err) return err;
    const json_t *eg = hrd2_sub(task, "execution_generation", &err);
    if (err) return err;
    double gen;
    err = hrd2_pyint(eg, &gen);
    if (err) return err;
    return hrd2_receipt_from(result, ident, gen);
}
static json_t *hrd2_info_active_of(const json_t *info) {
    json_t *err = NULL;
    const json_t *v = hrd2_map_get(info, "active", &err);
    if (err) return err;
    if (!json_has(info, "active")) {
        v = hrd2_map_get(info, "running", &err);
        if (err) return err;
    }
    return json_bool(hrd2_truth(v));
}
static json_t *hrd2_info_active_for(const json_t *info, const json_t *identity, bool require_exact) {
    json_t *err = NULL;
    const json_t *itid = hrd2_attr(identity, "task_id", &err);
    if (err) return err;
    json_t *act = hrd2_info_active_of(info);
    if (hrd2_is_raise(act)) return act;
    bool active = act->bool_val;
    json_free(act);
    if (!active) return json_bool(false);
    const json_t *tid = hrd2_map_get(info, "task_id", &err);
    if (err) return err;
    bool member = hrd2_json_eq(tid, itid) || (!require_exact && hrd2_is_null(tid));
    return json_bool(member);
}

/* ── wakeup(): inline state step (no seam) ────────────────────────────────── */
static void hrd2_wakeup_of(hrd2_env *e) {
    json_t *resched = json_obj_get(e->state, "reschedule");
    json_t *threads = json_obj_get(e->state, "room_threads");
    for (size_t i = 0; i < json_len(threads); i++)
        hrd2_set_add(resched, hrd2_sval(json_get(threads, i)));
    json_set(e->state, "wake_set", json_bool(true));
}

/* ── transport helpers ────────────────────────────────────────────────────── */
static json_t *hrd2_transport_proj(const json_t *t) {
    if (hrd2_is_null(t)) return NULL;
    json_t *r = json_object();
    json_set(r, "id", json_copy(json_obj_get(t, "id")));
    return r;
}
static bool hrd2_transport_is_rpc(hrd2_env *e, const json_t *t) {
    return !hrd2_is_null(t) && strcmp(json_get_str(t, "id", ""), hrd2_rpc_id(e)) == 0;
}
static bool hrd2_transport_has_discard(const json_t *t) {
    return hrd2_is_obj(t) && json_get_bool(t, "discard", false);
}
static json_t *hrd2_transport_args(const json_t *transport, const char *profile,
                                   const char *sid, bool is_session) {
    json_t *args = json_object();
    json_set(args, "id", json_copy(json_obj_get(transport, "id")));
    json_set(args, "profile", profile ? json_string(profile) : json_null());
    json_set(args, is_session ? "session_id" : "title", sid ? json_string(sid) : json_null());
    json_set(args, "source", json_string(HRD2_ROOM_SOURCE));
    return args;
}
/* normalize a lease double: reclaimed defaults to false */
static json_t *hrd2_lease_norm(const json_t *lease) {
    if (hrd2_is_null(lease)) return NULL;
    json_t *r = json_copy(lease);
    if (!json_has(r, "reclaimed")) json_set(r, "reclaimed", json_bool(false));
    return r;
}

/* ── claimed-sibling seam wrappers (args owned, result owned) ─────────────── */
static json_t *hrd2_arg_copy(json_t *args, const char *key, const json_t *v) {
    json_set(args, key, v ? json_copy(v) : json_null());
    return args;
}
static json_t *hrd2_self_open_session(hrd2_env *e, const json_t *binding,
                                      const json_t *task, bool peer_only) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "binding", binding);
    hrd2_arg_copy(a, "task", task);
    json_set(a, "peer_only", json_bool(peer_only));
    return hrd2_call(e, "self._open_session", a);
}
static json_t *hrd2_self_terminal_from_history(hrd2_env *e, const json_t *transport,
                                               const char *profile, const char *session_id,
                                               const json_t *task) {
    json_t *a = json_object();
    json_t *tp = hrd2_transport_proj(transport);
    json_set(a, "transport", tp ? tp : json_null());
    json_set(a, "profile", profile ? json_string(profile) : json_null());
    json_set(a, "session_id", session_id ? json_string(session_id) : json_null());
    hrd2_arg_copy(a, "task", task);
    return hrd2_call(e, "self._terminal_from_history", a);
}
static json_t *hrd2_self_fenced(hrd2_env *e, const char *op, const json_t *binding,
                                const json_t *task, const json_t *lease, bool publish,
                                const json_t *extra) {
    json_t *a = json_object();
    json_set(a, "op", json_string(op));
    hrd2_arg_copy(a, "binding", binding);
    hrd2_arg_copy(a, "task", task);
    json_t *ln = hrd2_lease_norm(lease);
    json_set(a, "lease", ln ? ln : json_null());
    json_set(a, "publish", json_bool(publish));
    json_set(a, "extra", extra ? json_copy(extra) : json_object());
    return hrd2_call(e, "self._fenced", a);
}
static json_t *hrd2_self_complete_cancel(hrd2_env *e, const json_t *task, const json_t *cancel_id) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "task", task);
    hrd2_arg_copy(a, "cancel_id", cancel_id);
    return hrd2_call(e, "self._complete_cancel", a);
}
static json_t *hrd2_self_peer_stop_acknowledged(hrd2_env *e, const json_t *binding, const json_t *task) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "binding", binding);
    hrd2_arg_copy(a, "task", task);
    return hrd2_call(e, "self._peer_stop_acknowledged", a);
}
static json_t *hrd2_self_binding_for_room(hrd2_env *e, const char *room_id) {
    json_t *a = json_object();
    json_set(a, "room_id", json_string(room_id ? room_id : ""));
    return hrd2_call(e, "self._binding_for_room", a);
}
static json_t *hrd2_self_ensure_lease(hrd2_env *e, const json_t *binding) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "binding", binding);
    return hrd2_call(e, "self._ensure_lease", a);
}
static json_t *hrd2_self_renew(hrd2_env *e, const json_t *lease, bool force) {
    json_t *a = json_object();
    json_t *ln = hrd2_lease_norm(lease);
    json_set(a, "lease", ln ? ln : json_null());
    json_set(a, "force", json_bool(force));
    return hrd2_call(e, "self._renew_lease_if_needed", a);
}
static json_t *hrd2_self_record_error(hrd2_env *e, const char *msg) {
    json_t *a = json_object();
    json_set(a, "message", json_string(msg ? msg : ""));
    return hrd2_call(e, "self._record_error", a);
}
static json_t *hrd2_self_set_blocked(hrd2_env *e, const char *room_id, bool blocked) {
    json_t *a = json_object();
    json_set(a, "room_id", json_string(room_id ? room_id : ""));
    json_set(a, "blocked", json_bool(blocked));
    return hrd2_call(e, "self._set_blocked", a);
}
static json_t *hrd2_self_tasks(hrd2_env *e, const json_t *binding, const json_t *status) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "binding", binding);
    hrd2_arg_copy(a, "status", status);
    return hrd2_call(e, "self._tasks", a);
}
static json_t *hrd2_self_remote(hrd2_env *e, const json_t *task) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "task", task);
    return hrd2_call(e, "self._remote", a);
}
static json_t *hrd2_self_transport_for(hrd2_env *e, const json_t *binding, const json_t *task) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "binding", binding);
    hrd2_arg_copy(a, "task", task);
    return hrd2_call(e, "self._transport_for", a);
}
static json_t *hrd2_self_resolve_indeterminate(hrd2_env *e, const json_t *binding,
                                               const json_t *task, const json_t *lease,
                                               const json_t *terminal, bool publish) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "binding", binding);
    hrd2_arg_copy(a, "task", task);
    json_t *ln = hrd2_lease_norm(lease);
    json_set(a, "lease", ln ? ln : json_null());
    hrd2_arg_copy(a, "terminal", terminal);
    json_set(a, "publish", json_bool(publish));
    return hrd2_call(e, "self._resolve_indeterminate", a);
}
static json_t *hrd2_self_requeue(hrd2_env *e, const char *requeue, const json_t *task,
                                 const json_t *lease, const char *room_id) {
    json_t *a = json_object();
    json_set(a, "requeue", json_string(requeue));
    hrd2_arg_copy(a, "task", task);
    json_t *ln = hrd2_lease_norm(lease);
    json_set(a, "lease", ln ? ln : json_null());
    json_set(a, "room_id", json_string(room_id ? room_id : ""));
    return hrd2_call(e, "self._requeue", a);
}
static json_t *hrd2_self_defer_unconfirmed_stop(hrd2_env *e, const json_t *binding,
                                                const json_t *task, const json_t *lease) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "binding", binding);
    hrd2_arg_copy(a, "task", task);
    json_t *ln = hrd2_lease_norm(lease);
    json_set(a, "lease", ln ? ln : json_null());
    return hrd2_call(e, "self._defer_unconfirmed_stop", a);
}
static json_t *hrd2_self_complete_acknowledged_stop(hrd2_env *e, const json_t *binding,
                                                    const json_t *task, const json_t *lease) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "binding", binding);
    hrd2_arg_copy(a, "task", task);
    json_t *ln = hrd2_lease_norm(lease);
    json_set(a, "lease", ln ? ln : json_null());
    return hrd2_call(e, "self._complete_acknowledged_stop", a);
}
static json_t *hrd2_self_served_elsewhere(hrd2_env *e, const json_t *binding, const json_t *task) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "binding", binding);
    hrd2_arg_copy(a, "task", task);
    return hrd2_call(e, "self._served_elsewhere", a);
}
static json_t *hrd2_self_resume_exact(hrd2_env *e, const json_t *transport,
                                      const char *room_id, const char *profile) {
    json_t *a = json_object();
    json_t *tp = hrd2_transport_proj(transport);
    json_set(a, "transport", tp ? tp : json_null());
    json_set(a, "room_id", json_string(room_id ? room_id : ""));
    json_set(a, "profile", profile ? json_string(profile) : json_null());
    return hrd2_call(e, "self._resume_exact", a);
}
static json_t *hrd2_self_report_pending_action(hrd2_env *e, const json_t *task,
                                               const json_t *session_id, const json_t *info) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "task", task);
    hrd2_arg_copy(a, "session_id", session_id);
    hrd2_arg_copy(a, "info", info);
    return hrd2_call(e, "self._report_pending_action", a);
}

/* ── state-op seams ───────────────────────────────────────────────────────── */
static json_t *hrd2_state_get_task(hrd2_env *e, const json_t *identity) {
    json_t *a = json_object();
    hrd2_arg_copy(a, "identity", identity);
    json_set(a, "placement", json_string(hrd2_placement(e)));
    return hrd2_call(e, "state.get_task", a);
}

/* ── turn_lock context manager (seam; inert or __exit__ raise) ────────────── */
typedef struct hrd2_cm { json_t *handle; bool exited; } hrd2_cm;
static json_t *hrd2_turn_lock_enter(hrd2_env *e, const char *profile, hrd2_cm *cm) {
    json_t *a = json_object();
    json_set(a, "profile", profile ? json_string(profile) : json_null());
    json_t *r = hrd2_call(e, "turn_lock", a);
    cm->handle = r;
    cm->exited = false;
    if (hrd2_is_raise(r)) return r;   /* raise at call */
    return NULL;
}
/* returns an owned raise spec when __exit__ raises, else NULL */
static json_t *hrd2_turn_lock_exit(hrd2_cm *cm) {
    if (cm->exited) return NULL;
    cm->exited = true;
    json_t *h = cm->handle;
    cm->handle = NULL;
    if (hrd2_is_obj(h)) {
        json_t *ex = json_obj_get(h, "__exit__");
        if (hrd2_is_raise(ex)) { json_t *r = json_copy(ex); json_free(h); return r; }
    }
    json_free(h);
    return NULL;
}

/* ── forward declarations (internal composition) ──────────────────────────── */
static json_t *hrd2_stop_outcome_of(hrd2_env *e, const json_t *binding, const json_t *task);
static json_t *hrd2_stop_or_settle_of(hrd2_env *e, const json_t *binding, const json_t *task, const json_t *lease);
static json_t *hrd2_finish_stop_of(hrd2_env *e, const json_t *binding, const json_t *task, const json_t *lease);
static json_t *hrd2_settle_stopping_completion_of(hrd2_env *e, const json_t *binding, const json_t *task, const json_t *lease);
static json_t *hrd2_inspect_session_of(hrd2_env *e, const json_t *transport, const json_t *task, const json_t *session_id, bool read_history);
static json_t *hrd2_inspect_recovery_session_of(hrd2_env *e, const json_t *binding, const json_t *task);
static json_t *hrd2_inspect_local_recovery_session_of(hrd2_env *e, const json_t *task);
static json_t *hrd2_harvest_previous_attempt_of(hrd2_env *e, const json_t *binding, const json_t *task, const json_t *receipt);
static json_t *hrd2_expire_attempt_deadline_of(hrd2_env *e, const json_t *binding, const json_t *task, const json_t *lease);

/* ── _discard_unknown_stop (staticmethod) ─────────────────────────────────── */
static json_t *hrd2_discard_unknown_stop_of(hrd2_env *e, const json_t *transport,
                                            const char *profile, const char *session_id,
                                            const json_t *task, const json_t *info) {
    (void)e;
    json_t *err = NULL;
    const json_t *eg = hrd2_sub(task, "execution_generation", &err);
    if (err) return err;
    double gen;
    err = hrd2_pyint(eg, &gen);
    if (err) return err;
    /* discard = transport.discard (presence gate; domain: present in fixtures) */
    if (!hrd2_transport_has_discard(transport))
        return hrd2_attr_raise(transport, "discard");
    const json_t *ident = hrd2_sub(task, "identity", &err);
    if (err) return err;
    const json_t *want_tid = hrd2_attr(ident, "task_id", &err);
    if (err) return err;
    /* if (_info_active(info) or info.get("task_id") != task_id
     *     or type(info.get("execution_generation")) is not int
     *     or info["execution_generation"] != generation): return False */
    json_t *act = hrd2_info_active_of(info);
    if (hrd2_is_raise(act)) return act;
    bool active = act->bool_val;
    json_free(act);
    if (active) return json_bool(false);
    const json_t *itid = hrd2_map_get(info, "task_id", &err);
    if (err) return err;
    if (!hrd2_json_eq(itid, want_tid)) return json_bool(false);
    const json_t *igen = hrd2_map_get(info, "execution_generation", &err);
    if (err) return err;
    if (!hrd2_type_is_int(igen)) return json_bool(false);
    if (igen->num_val != gen) return json_bool(false);
    /* result = discard(**_session_kw, expected_task_id=, execution_generation=gen) */
    json_t *a = hrd2_transport_args(transport, profile, session_id, true);
    json_set(a, "expected_task_id", json_copy(want_tid));
    json_set(a, "execution_generation", json_number(gen));
    json_t *result = hrd2_call(e, "transport.discard", a);
    if (hrd2_is_raise(result)) return result;
    /* if not (result and result.get("discarded") is True
     *         and result.get("task_id") == task_id
     *         and type(result.get("execution_generation")) is int
     *         and result["execution_generation"] == generation): return False */
    bool ok = false;
    if (hrd2_truth(result)) {
        const json_t *disc = json_obj_get(result, "discarded");
        const json_t *rtid = json_obj_get(result, "task_id");
        const json_t *rgen = json_obj_get(result, "execution_generation");
        ok = disc && disc->type == JSON_BOOL && disc->bool_val
            && hrd2_json_eq(rtid, want_tid)
            && hrd2_type_is_int(rgen) && rgen->num_val == gen;
    }
    if (!ok) { json_free(result); return json_bool(false); }
    /* return discarded_receipt(result, task) or result.get("status") == "cancelled" */
    json_t *rec = hrd2_discarded_receipt(result, task);
    if (hrd2_is_raise(rec)) { json_free(result); return rec; }
    if (rec) { json_free(result); return rec; }
    const json_t *rst = json_obj_get(result, "status");
    bool cancelled = hrd2_str(rst) && strcmp(hrd2_str(rst), "cancelled") == 0;
    json_free(result);
    return json_bool(cancelled);
}
/* PoP: hrd2_discard_unknown_stop @ tui_gateway/hosted_room_driver.py:_discard_unknown_stop */
static json_t *hrd2_discard_unknown_stop(hrd2_env *e, const json_t *in) {
    return hrd2_discard_unknown_stop_of(e, json_obj_get(in, "transport"),
                                        json_get_str(in, "profile", ""),
                                        json_get_str(in, "session_id", ""),
                                        json_obj_get(in, "task"), json_obj_get(in, "info"));
}

/* ── _interrupt_stale_admission ───────────────────────────────────────────── */
static json_t *hrd2_interrupt_stale_admission_of(hrd2_env *e, const json_t *transport,
                                                 const json_t *binding, const char *profile,
                                                 const char *session_id, const json_t *info) {
    json_t *err = NULL;
    const json_t *itid = hrd2_map_get(info, "task_id", &err);
    if (err) return err;
    char *stale_s = hrd2_pystr_or_empty(itid);
    if (!stale_s) return hrd2_raise_spec("TypeError", "str() domain: scalar expected");
    if (!stale_s[0]) { free(stale_s); return json_bool(false); }
    /* live_ids = {t["identity"].task_id for status in ("running","indeterminate")
     *             for t in self._tasks(binding, status)} */
    json_t *live_ids = json_array();
    const char *statuses[2] = {"running", "indeterminate"};
    for (int s = 0; s < 2; s++) {
        json_t *st = json_string(statuses[s]);
        json_t *tasks = hrd2_self_tasks(e, binding, st);
        json_free(st);
        if (hrd2_is_raise(tasks)) { free(stale_s); json_free(live_ids); return tasks; }
        if (hrd2_is_arr(tasks)) {
            for (size_t i = 0; i < json_len(tasks); i++) {
                const json_t *t = json_get(tasks, i);
                const json_t *ident = hrd2_sub(t, "identity", &err);
                if (err) { json_free(tasks); free(stale_s); json_free(live_ids); return err; }
                const json_t *tid = hrd2_attr(ident, "task_id", &err);
                if (err) { json_free(tasks); free(stale_s); json_free(live_ids); return err; }
                json_append(live_ids, json_copy(tid));
            }
        }
        json_free(tasks);
    }
    bool in_live = false;
    for (size_t i = 0; i < json_len(live_ids); i++)
        if (strcmp(hrd2_sval(json_get(live_ids, i)), stale_s) == 0) { in_live = true; break; }
    json_free(live_ids);
    if (in_live) { free(stale_s); return json_bool(false); }
    /* stale = next((t for t in self._tasks(binding, None)
     *               if t["identity"].task_id == stale_task_id), None) */
    json_t *nullst = json_null();
    json_t *tasks = hrd2_self_tasks(e, binding, nullst);
    json_free(nullst);
    if (hrd2_is_raise(tasks)) { free(stale_s); return tasks; }
    json_t *stale = NULL;
    if (hrd2_is_arr(tasks)) {
        for (size_t i = 0; i < json_len(tasks); i++) {
            const json_t *t = json_get(tasks, i);
            const json_t *ident = hrd2_sub(t, "identity", &err);
            if (err) { json_free(tasks); free(stale_s); return err; }
            const json_t *tid = hrd2_attr(ident, "task_id", &err);
            if (err) { json_free(tasks); free(stale_s); return err; }
            if (strcmp(hrd2_sval(tid), stale_s) == 0) { stale = json_copy(t); break; }
        }
    }
    json_free(tasks);
    if (!stale) { free(stale_s); return json_bool(false); }
    json_t *rem = hrd2_self_remote(e, stale);
    if (hrd2_is_raise(rem)) { json_free(stale); free(stale_s); return rem; }
    bool remote = hrd2_truth(rem);
    json_free(rem);
    if (remote) { json_free(stale); free(stale_s); return json_bool(false); }
    /* with suppress(RuntimeError, OSError, ValueError): transport.interrupt(...) */
    json_t *a = hrd2_transport_args(transport, profile, session_id, true);
    json_set(a, "expected_task_id", json_string(stale_s));
    json_t *r = hrd2_call(e, "transport.interrupt", a);
    if (hrd2_is_raise(r)) {
        const char *k = hrd2_kind(r);
        /* suppress(RuntimeError, OSError, ValueError): DriverStateError subclasses ValueError */
        if (strcmp(k, "RuntimeError") == 0 || strcmp(k, "OSError") == 0 || strcmp(k, "ValueError") == 0
            || hrd2_is_driver_state(k))
            json_free(r);
        else { json_free(stale); free(stale_s); return r; }
    } else json_free(r);
    json_free(stale);
    free(stale_s);
    return json_bool(false);
}
/* PoP: hrd2_interrupt_stale_admission @ tui_gateway/hosted_room_driver.py:_interrupt_stale_admission */
static json_t *hrd2_interrupt_stale_admission(hrd2_env *e, const json_t *in) {
    return hrd2_interrupt_stale_admission_of(e, json_obj_get(in, "transport"),
                                             json_obj_get(in, "binding"),
                                             json_get_str(in, "profile", ""),
                                             json_get_str(in, "session_id", ""),
                                             json_obj_get(in, "info"));
}

/* ── _stop_outcome ────────────────────────────────────────────────────────── */
static json_t *hrd2_stop_outcome_of(hrd2_env *e, const json_t *binding, const json_t *task) {
    json_t *opened = hrd2_self_open_session(e, binding, task, false);
    if (hrd2_is_raise(opened)) return opened;
    const json_t *transport = json_get(opened, 0);
    const json_t *profile = json_get(opened, 1);
    const json_t *sid = json_get(opened, 2);
    if (hrd2_is_null(sid)) {
        /* return transport is not None and transport is self.rpc */
        bool ack = !hrd2_is_null(transport) && hrd2_transport_is_rpc(e, transport);
        json_free(opened);
        return json_bool(ack);
    }
    json_t *tproj = hrd2_transport_proj(transport);
    json_t *a = hrd2_transport_args(tproj, hrd2_str(profile), hrd2_str(sid), true);
    json_t *info = hrd2_call(e, "transport.info", a);
    json_free(tproj);
    if (hrd2_is_raise(info)) { json_free(opened); return info; }
    json_t *err = NULL;
    const json_t *ist = hrd2_map_get(info, "status", &err);
    if (err) { json_free(info); json_free(opened); return err; }
    const json_t *tst = hrd2_sub(task, "status", &err);
    if (err) { json_free(info); json_free(opened); return err; }
    bool unknown_stop = hrd2_str(ist) && strcmp(hrd2_str(ist), "unknown") == 0
        && hrd2_str(tst) && strcmp(hrd2_str(tst), "stopping") == 0
        && hrd2_transport_has_discard(transport);
    if (unknown_stop) {
        json_t *r = hrd2_discard_unknown_stop_of(e, transport, hrd2_str(profile), hrd2_str(sid), task, info);
        json_free(info); json_free(opened);
        return r;
    }
    json_t *act = hrd2_info_active_of(info);
    if (hrd2_is_raise(act)) { json_free(info); json_free(opened); return act; }
    bool active = act->bool_val;
    json_free(act);
    if (!active) { json_free(info); json_free(opened); return json_bool(true); }
    const json_t *ident = hrd2_sub(task, "identity", &err);
    if (err) { json_free(info); json_free(opened); return err; }
    json_t *af = hrd2_info_active_for(info, ident, true);
    if (hrd2_is_raise(af)) { json_free(info); json_free(opened); return af; }
    bool exact = af->bool_val;
    json_free(af);
    if (!exact) {
        json_t *r = hrd2_interrupt_stale_admission_of(e, transport, binding, hrd2_str(profile), hrd2_str(sid), info);
        json_free(info); json_free(opened);
        return r;
    }
    /* result = transport.interrupt(**_session_kw, expected_task_id=task_id) */
    const json_t *want_tid = hrd2_attr(ident, "task_id", &err);
    if (err) { json_free(info); json_free(opened); return err; }
    json_t *tp2 = hrd2_transport_proj(transport);
    json_t *ia = hrd2_transport_args(tp2, hrd2_str(profile), hrd2_str(sid), true);
    json_set(ia, "expected_task_id", json_copy(want_tid));
    json_t *result = hrd2_call(e, "transport.interrupt", ia);
    json_free(tp2);
    json_free(info); json_free(opened);
    if (hrd2_is_raise(result)) return result;
    /* return result is not None and (result.get("interrupted") is True
     *        or str(result.get("status") or "") in _STOP_ACK_STATUSES) */
    bool ack = false;
    if (!hrd2_is_null(result)) {
        const json_t *intr = json_obj_get(result, "interrupted");
        if (intr && intr->type == JSON_BOOL && intr->bool_val) ack = true;
        else {
            const json_t *rst = json_obj_get(result, "status");
            char *s = hrd2_pystr_or_empty(rst);
            if (!s) { json_free(result); return hrd2_raise_spec("TypeError", "str() domain: scalar expected"); }
            ack = strcmp(s, "cancelled") == 0 || strcmp(s, "interrupted") == 0;
            free(s);
        }
    }
    json_free(result);
    return json_bool(ack);
}
/* PoP: hrd2_stop_outcome @ tui_gateway/hosted_room_driver.py:_stop_outcome */
static json_t *hrd2_stop_outcome(hrd2_env *e, const json_t *in) {
    return hrd2_stop_outcome_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"));
}

/* ── _interrupt_stopping_task ─────────────────────────────────────────────── */
/* PoP: hrd2_interrupt_stopping_task @ tui_gateway/hosted_room_driver.py:_interrupt_stopping_task */
static json_t *hrd2_interrupt_stopping_task(hrd2_env *e, const json_t *in) {
    json_t *r = hrd2_stop_outcome_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"));
    if (hrd2_is_raise(r)) return r;
    bool is_true = r && r->type == JSON_BOOL && r->bool_val;
    json_free(r);
    return json_bool(is_true);
}

/* ── _stop_or_settle ──────────────────────────────────────────────────────── */
static json_t *hrd2_stop_or_settle_of(hrd2_env *e, const json_t *binding, const json_t *task, const json_t *lease) {
    json_t *outcome = hrd2_stop_outcome_of(e, binding, task);
    if (hrd2_is_raise(outcome)) return outcome;
    if (hrd2_is_obj(outcome)) {   /* isinstance(outcome, _TerminalReceipt) */
        json_t *r = hrd2_self_fenced(e, "settle_stopping_task", binding, task, lease, true, outcome);
        json_free(outcome);
        if (hrd2_is_raise(r)) return r;
        json_free(r);
        return NULL;
    }
    return outcome;   /* bool */
}
/* PoP: hrd2_stop_or_settle @ tui_gateway/hosted_room_driver.py:_stop_or_settle */
static json_t *hrd2_stop_or_settle(hrd2_env *e, const json_t *in) {
    return hrd2_stop_or_settle_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"), json_obj_get(in, "lease"));
}

/* ── _settle_stopping_completion ──────────────────────────────────────────── */
static json_t *hrd2_settle_stopping_completion_of(hrd2_env *e, const json_t *binding, const json_t *task, const json_t *lease) {
    json_t *opened = hrd2_self_open_session(e, binding, task, false);
    if (hrd2_is_raise(opened)) return opened;
    const json_t *transport = json_get(opened, 0);
    const json_t *profile = json_get(opened, 1);
    const json_t *sid = json_get(opened, 2);
    if (hrd2_is_null(sid)) { json_free(opened); return json_bool(false); }
    json_t *receipt = hrd2_self_terminal_from_history(e, transport, hrd2_str(profile), hrd2_str(sid), task);
    json_free(opened);
    if (hrd2_is_raise(receipt)) return receipt;
    if (hrd2_is_null(receipt)) return json_bool(false);
    json_t *r = hrd2_self_fenced(e, "settle_stopping_task", binding, task, lease, true, receipt);
    json_free(receipt);
    if (hrd2_is_raise(r)) return r;
    json_free(r);
    return json_bool(true);
}
/* PoP: hrd2_settle_stopping_completion @ tui_gateway/hosted_room_driver.py:_settle_stopping_completion */
static json_t *hrd2_settle_stopping_completion(hrd2_env *e, const json_t *in) {
    return hrd2_settle_stopping_completion_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"), json_obj_get(in, "lease"));
}

/* ── _finish_stop ─────────────────────────────────────────────────────────── */
static json_t *hrd2_finish_stop_of(hrd2_env *e, const json_t *binding, const json_t *task, const json_t *lease) {
    json_t *settled = hrd2_settle_stopping_completion_of(e, binding, task, lease);
    if (hrd2_is_raise(settled)) {
        /* except DriverStateError: raise — the whole family subclasses it */
        if (hrd2_is_driver_state(hrd2_kind(settled))) return settled;
        if (hrd2_is_base(hrd2_kind(settled))) return settled;
        /* except Exception: record _STOP_PENDING */
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s%s", HRD2_STOP_PENDING, hrd2_msg(settled));
        json_free(settled);
        json_t *rec = hrd2_self_record_error(e, buf);
        if (hrd2_is_raise(rec)) return rec;
        json_free(rec);
    } else {
        bool done = settled->bool_val;
        json_free(settled);
        if (done) return json_bool(true);
    }
    json_t *ack = hrd2_stop_or_settle_of(e, binding, task, lease);
    if (hrd2_is_raise(ack)) return ack;
    bool acknowledged = hrd2_truth(ack);   /* True or None (null) -> truthy? no: None falsy */
    /* live: if acknowledged: ... ; acknowledged is bool|None. `if acknowledged`
     * treats None as falsy. But _stop_or_settle returns bool|None where None means
     * "settled its own receipt" -> live `if acknowledged:` None is falsy so skips
     * complete_acknowledged_stop. Then `return acknowledged is not False`. */
    bool is_none = hrd2_is_null(ack);
    bool is_false = ack && ack->type == JSON_BOOL && !ack->bool_val;
    if (acknowledged && !is_none) {
        json_t *r = hrd2_self_complete_acknowledged_stop(e, binding, task, lease);
        if (hrd2_is_raise(r)) { json_free(ack); return r; }
        json_free(r);
    }
    json_free(ack);
    return json_bool(!is_false);
}
/* PoP: hrd2_finish_stop @ tui_gateway/hosted_room_driver.py:_finish_stop */
static json_t *hrd2_finish_stop(hrd2_env *e, const json_t *in) {
    return hrd2_finish_stop_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"), json_obj_get(in, "lease"));
}

/* ── _retry_stopping_tasks ────────────────────────────────────────────────── */
static json_t *hrd2_retry_stopping_tasks_of(hrd2_env *e, const json_t *binding, const json_t *lease_in) {
    json_t *st = json_string("stopping");
    json_t *tasks = hrd2_self_tasks(e, binding, st);
    json_free(st);
    if (hrd2_is_raise(tasks)) return tasks;
    json_t *lease = lease_in ? json_copy(lease_in) : json_null();
    json_t *ret = json_bool(false);
    if (hrd2_is_arr(tasks)) {
        for (size_t i = 0; i < json_len(tasks); i++) {
            const json_t *task = json_get(tasks, i);
            bool continued = false;
            json_t *renew = hrd2_self_renew(e, lease, false);
            if (hrd2_is_raise(renew)) {
                if (hrd2_is_base(hrd2_kind(renew))) { json_free(tasks); json_free(lease); return renew; }
                char buf[1024];
                snprintf(buf, sizeof(buf), "%s%s", HRD2_STOP_PENDING, hrd2_msg(renew));
                json_free(renew);
                json_t *rec = hrd2_self_record_error(e, buf);
                if (hrd2_is_raise(rec)) { json_free(tasks); json_free(lease); return rec; }
                json_free(rec);
            } else {
                json_free(lease);
                lease = renew;
                json_t *ack = hrd2_self_peer_stop_acknowledged(e, binding, task);
                if (hrd2_is_raise(ack)) {
                    if (hrd2_is_base(hrd2_kind(ack))) { json_free(tasks); json_free(lease); return ack; }
                    char buf[1024];
                    snprintf(buf, sizeof(buf), "%s%s", HRD2_STOP_PENDING, hrd2_msg(ack));
                    json_free(ack);
                    json_t *rec = hrd2_self_record_error(e, buf);
                    if (hrd2_is_raise(rec)) { json_free(tasks); json_free(lease); return rec; }
                    json_free(rec);
                } else {
                    bool peer_ack = hrd2_truth(ack);
                    json_free(ack);
                    if (peer_ack) {
                        json_t *cc = hrd2_self_complete_cancel(e, task, NULL);
                        if (hrd2_is_raise(cc)) {
                            if (hrd2_is_base(hrd2_kind(cc))) { json_free(tasks); json_free(lease); return cc; }
                            char buf[1024];
                            snprintf(buf, sizeof(buf), "%s%s", HRD2_STOP_PENDING, hrd2_msg(cc));
                            json_free(cc);
                            json_t *rec = hrd2_self_record_error(e, buf);
                            if (hrd2_is_raise(rec)) { json_free(tasks); json_free(lease); return rec; }
                            json_free(rec);
                        } else { json_free(cc); continued = true; }
                    } else {
                        json_t *fin = hrd2_finish_stop_of(e, binding, task, lease);
                        if (hrd2_is_raise(fin)) {
                            if (hrd2_is_base(hrd2_kind(fin))) { json_free(tasks); json_free(lease); return fin; }
                            char buf[1024];
                            snprintf(buf, sizeof(buf), "%s%s", HRD2_STOP_PENDING, hrd2_msg(fin));
                            json_free(fin);
                            json_t *rec = hrd2_self_record_error(e, buf);
                            if (hrd2_is_raise(rec)) { json_free(tasks); json_free(lease); return rec; }
                            json_free(rec);
                        } else {
                            bool done = hrd2_truth(fin);
                            json_free(fin);
                            if (done) continued = true;
                        }
                    }
                }
            }
            if (continued) continue;
            json_t *def = hrd2_self_defer_unconfirmed_stop(e, binding, task, lease);
            if (hrd2_is_raise(def)) { json_free(tasks); json_free(lease); return def; }
            bool deferred = hrd2_truth(def);
            json_free(def);
            if (!deferred) { json_free(ret); ret = json_bool(true); break; }
        }
    }
    json_free(tasks);
    json_free(lease);
    return ret;
}
/* PoP: hrd2_retry_stopping_tasks @ tui_gateway/hosted_room_driver.py:_retry_stopping_tasks */
static json_t *hrd2_retry_stopping_tasks(hrd2_env *e, const json_t *in) {
    return hrd2_retry_stopping_tasks_of(e, json_obj_get(in, "binding"), json_obj_get(in, "lease"));
}

/* ── cancel (public op) ───────────────────────────────────────────────────── */
static bool hrd2_terminal_status(const char *s) {
    return s && (strcmp(s, "settled") == 0 || strcmp(s, "failed") == 0 || strcmp(s, "cancelled") == 0);
}
/* PoP: hrd2_cancel @ tui_gateway/hosted_room_driver.py:cancel */
static json_t *hrd2_cancel(hrd2_env *e, const json_t *in) {
    const json_t *identity = json_obj_get(in, "identity");
    json_t *cancel_id = json_obj_get(in, "cancel_id") ? json_copy(json_obj_get(in, "cancel_id")) : json_null();
    bool acknowledge = json_get_bool(in, "acknowledge", true);
    json_t *err = NULL;
    const json_t *room_id = hrd2_attr(identity, "room_id", &err);
    if (err) { json_free(cancel_id); return err; }
    for (int attempt = 0; attempt < HRD2_CANCEL_ROUTE_RETRIES; attempt++) {
        json_t *before = hrd2_state_get_task(e, identity);
        if (hrd2_is_raise(before)) { json_free(cancel_id); return before; }
        const json_t *bst = hrd2_sub(before, "status", &err);
        if (err) { json_free(before); json_free(cancel_id); return err; }
        const char *bs = hrd2_str(bst);
        if (bs && strcmp(bs, "cancelled") == 0) { json_free(cancel_id); return before; }
        if (hrd2_terminal_status(bs)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "cannot cancel task in state '%s'", bs ? bs : "");
            json_free(before); json_free(cancel_id);
            return hrd2_raise_spec("InvalidTaskTransitionError", buf);
        }
        if (bs && strcmp(bs, "stopping") == 0) {
            const json_t *bcid = hrd2_sub(before, "cancel_id", &err);
            if (err) { json_free(before); json_free(cancel_id); return err; }
            json_free(cancel_id);
            cancel_id = json_copy(bcid);
        }
        bool direct = bs && (strcmp(bs, "queued") == 0 || strcmp(bs, "deferred") == 0);
        if (bs && strcmp(bs, "deferred") == 0 && hrd2_has_cb(e, "confirm_deferred_stop")) {
            bool confirm = true;
            if (!acknowledge) confirm = false;
            else {
                json_t *a = json_object();
                json_set(a, "room_id", json_copy(room_id));
                hrd2_arg_copy(a, "task", before);
                json_t *cr = hrd2_call(e, "confirm_deferred_stop", a);
                if (hrd2_is_raise(cr)) { json_free(before); json_free(cancel_id); return cr; }
                confirm = hrd2_truth(cr);
                json_free(cr);
            }
            if (!confirm) { json_free(before); json_free(cancel_id); return before; }
        }
        /* result = (cancel_task if direct else begin_task_cancel)(...) */
        const json_t *ecg = hrd2_sub(before, "cancel_generation", &err);
        if (err) { json_free(before); json_free(cancel_id); return err; }
        json_t *a = json_object();
        hrd2_arg_copy(a, "identity", identity);
        json_set(a, "placement", json_string(hrd2_placement(e)));
        hrd2_arg_copy(a, "cancel_id", cancel_id);
        json_set(a, "expected_cancel_generation", json_copy(ecg));
        json_t *result = hrd2_call(e, direct ? "state.cancel_task" : "state.begin_task_cancel", a);
        if (hrd2_is_raise(result)) {
            const char *k = hrd2_kind(result);
            if (strcmp(k, "InvalidTaskTransitionError") == 0 || strcmp(k, "StaleTaskError") == 0) {
                json_free(result); json_free(before);
                continue;   /* lost the race; re-route */
            }
            json_free(before); json_free(cancel_id);
            return result;
        }
        json_free(before);
        if (!direct && acknowledge) {
            /* binding read is OUTSIDE the try: a raise here propagates uncaught */
            json_t *binding = hrd2_self_binding_for_room(e, hrd2_str(room_id));
            if (hrd2_is_raise(binding)) { json_free(result); json_free(cancel_id); return binding; }
            if (!hrd2_is_null(binding)) {
                json_t *lease = hrd2_self_ensure_lease(e, binding);
                if (hrd2_is_raise(lease)) {
                    if (hrd2_is_base(hrd2_kind(lease))) { json_free(binding); json_free(result); json_free(cancel_id); return lease; }
                    char buf[1024];
                    snprintf(buf, sizeof(buf), "stop remains pending: %s", hrd2_msg(lease));
                    json_free(lease); json_free(binding);
                    json_t *rec = hrd2_self_record_error(e, buf);
                    if (hrd2_is_raise(rec)) { json_free(result); json_free(cancel_id); return rec; }
                    json_free(rec);
                } else {
                    /* if peer_stop_acknowledged or (not settle_stopping_completion
                     *    and stop_or_settle is True): complete_cancel */
                    bool do_complete = false;
                    json_t *ack = hrd2_self_peer_stop_acknowledged(e, binding, result);
                    if (hrd2_is_raise(ack)) {
                        if (hrd2_is_base(hrd2_kind(ack))) { json_free(lease); json_free(binding); json_free(result); json_free(cancel_id); return ack; }
                        char buf[1024];
                        snprintf(buf, sizeof(buf), "stop remains pending: %s", hrd2_msg(ack));
                        json_free(ack); json_free(lease); json_free(binding);
                        json_t *rec = hrd2_self_record_error(e, buf);
                        if (hrd2_is_raise(rec)) { json_free(result); json_free(cancel_id); return rec; }
                        json_free(rec);
                    } else {
                        bool peer_ack = hrd2_truth(ack);
                        json_free(ack);
                        if (peer_ack) do_complete = true;
                        else {
                            json_t *sc = hrd2_settle_stopping_completion_of(e, binding, result, lease);
                            if (hrd2_is_raise(sc)) {
                                if (hrd2_is_base(hrd2_kind(sc))) { json_free(lease); json_free(binding); json_free(result); json_free(cancel_id); return sc; }
                                char buf[1024];
                                snprintf(buf, sizeof(buf), "stop remains pending: %s", hrd2_msg(sc));
                                json_free(sc); json_free(lease); json_free(binding);
                                json_t *rec = hrd2_self_record_error(e, buf);
                                if (hrd2_is_raise(rec)) { json_free(result); json_free(cancel_id); return rec; }
                                json_free(rec);
                            } else {
                                bool settled = hrd2_truth(sc);
                                json_free(sc);
                                if (!settled) {
                                    json_t *sos = hrd2_stop_or_settle_of(e, binding, result, lease);
                                    if (hrd2_is_raise(sos)) {
                                        if (hrd2_is_base(hrd2_kind(sos))) { json_free(lease); json_free(binding); json_free(result); json_free(cancel_id); return sos; }
                                        char buf[1024];
                                        snprintf(buf, sizeof(buf), "stop remains pending: %s", hrd2_msg(sos));
                                        json_free(sos); json_free(lease); json_free(binding);
                                        json_t *rec = hrd2_self_record_error(e, buf);
                                        if (hrd2_is_raise(rec)) { json_free(result); json_free(cancel_id); return rec; }
                                        json_free(rec);
                                    } else {
                                        bool is_true = sos && sos->type == JSON_BOOL && sos->bool_val;
                                        json_free(sos);
                                        if (is_true) do_complete = true;
                                    }
                                }
                            }
                        }
                    }
                    if (do_complete) {
                        json_t *cc = hrd2_self_complete_cancel(e, result, cancel_id);
                        if (hrd2_is_raise(cc)) {
                            if (hrd2_is_base(hrd2_kind(cc))) { json_free(lease); json_free(binding); json_free(result); json_free(cancel_id); return cc; }
                            char buf[1024];
                            snprintf(buf, sizeof(buf), "stop remains pending: %s", hrd2_msg(cc));
                            json_free(cc);
                            json_t *rec = hrd2_self_record_error(e, buf);
                            if (hrd2_is_raise(rec)) { json_free(lease); json_free(binding); json_free(result); json_free(cancel_id); return rec; }
                            json_free(rec);
                        } else json_free(cc);
                    }
                    json_free(lease);
                    json_free(binding);
                }
            } else {
                json_free(binding);
            }
        }
        hrd2_wakeup_of(e);
        if (direct) { json_free(cancel_id); return result; }
        json_t *final = hrd2_state_get_task(e, identity);
        json_free(result);
        json_free(cancel_id);
        return final;
    }
    /* retries exhausted */
    json_t *final = hrd2_state_get_task(e, identity);
    if (hrd2_is_raise(final)) { json_free(cancel_id); return final; }
    const json_t *fst = hrd2_sub(final, "status", &err);
    if (err) { json_free(final); json_free(cancel_id); return err; }
    const char *fs = hrd2_str(fst);
    if (fs && strcmp(fs, "cancelled") == 0) { json_free(cancel_id); return final; }
    char buf[256];
    snprintf(buf, sizeof(buf),
        "cancel kept losing races with task transitions (last observed state '%s')", fs ? fs : "");
    json_free(final); json_free(cancel_id);
    return hrd2_raise_spec("InvalidTaskTransitionError", buf);
}

/* ── _retry_deferred ──────────────────────────────────────────────────────── */
static json_t *hrd2_retry_deferred_of(hrd2_env *e, const json_t *binding, const json_t *task, const json_t *lease_in) {
    json_t *err = NULL;
    const json_t *ident = hrd2_sub(task, "identity", &err);
    if (err) return err;
    const json_t *rid = hrd2_attr(ident, "room_id", &err);
    if (err) return err;
    const char *room_id = hrd2_sval(rid);
    json_t *rem = hrd2_self_remote(e, task);
    if (hrd2_is_raise(rem)) return rem;
    bool remote = hrd2_truth(rem);
    json_free(rem);
    if (!remote)
        return hrd2_self_requeue(e, "requeue_deferred_task", task, lease_in, room_id);
    json_t *t = hrd2_self_transport_for(e, binding, task);
    if (hrd2_is_raise(t)) return t;
    json_free(t);
    json_t *lease = hrd2_self_ensure_lease(e, binding);
    if (hrd2_is_raise(lease)) return lease;
    json_t *reopened = hrd2_self_fenced(e, "reopen_deferred_task", NULL, task, lease, true, NULL);
    if (hrd2_is_raise(reopened)) { json_free(lease); return reopened; }
    json_t *inspection = hrd2_inspect_recovery_session_of(e, binding, reopened);
    if (hrd2_is_raise(inspection)) {
        const char *k = hrd2_kind(inspection);
        /* except (RuntimeError, ValueError, OSError): DriverStateError subclasses ValueError */
        if (strcmp(k, "RuntimeError") == 0 || strcmp(k, "ValueError") == 0 || strcmp(k, "OSError") == 0
            || hrd2_is_driver_state(k)) {
            const json_t *rident = hrd2_sub(reopened, "identity", &err);
            if (err) { json_free(inspection); json_free(reopened); json_free(lease); return err; }
            const json_t *rtid = hrd2_attr(rident, "task_id", &err);
            if (err) { json_free(inspection); json_free(reopened); json_free(lease); return err; }
            char buf[1024];
            snprintf(buf, sizeof(buf), "task %s retry is being watched: %s", hrd2_sval(rtid), hrd2_msg(inspection));
            json_free(inspection);
            json_t *rec = hrd2_self_record_error(e, buf);
            if (hrd2_is_raise(rec)) { json_free(reopened); json_free(lease); return rec; }
            json_free(rec);
            hrd2_wakeup_of(e);
            json_free(lease);
            return reopened;
        }
        json_free(reopened); json_free(lease);
        return inspection;
    }
    const json_t *terminal = json_obj_get(inspection, "terminal");
    if (!hrd2_is_null(terminal)) {
        json_t *r = hrd2_self_resolve_indeterminate(e, binding, reopened, lease, terminal, true);
        json_free(inspection); json_free(reopened); json_free(lease);
        return r;
    }
    const json_t *ist = json_obj_get(inspection, "status");
    if (hrd2_str(ist) && strcmp(hrd2_str(ist), "cancelled") == 0) {
        const json_t *reg = hrd2_sub(reopened, "execution_generation", &err);
        if (err) { json_free(inspection); json_free(reopened); json_free(lease); return err; }
        char cid[160];
        snprintf(cid, sizeof(cid), "remote-cancel:%s", hrd2_sval(reg));
        json_t *extra = json_object();
        json_set(extra, "cancel_id", json_string(cid));
        json_t *r = hrd2_self_fenced(e, "resolve_indeterminate_cancellation", binding, reopened, lease, true, extra);
        json_free(extra); json_free(inspection); json_free(reopened); json_free(lease);
        return r;
    }
    json_free(inspection);
    hrd2_wakeup_of(e);
    json_free(lease);
    return reopened;
}
/* PoP: hrd2_retry_deferred @ tui_gateway/hosted_room_driver.py:_retry_deferred */
static json_t *hrd2_retry_deferred(hrd2_env *e, const json_t *in) {
    return hrd2_retry_deferred_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"), json_obj_get(in, "lease"));
}

/* ── retry_indeterminate (public op) ──────────────────────────────────────── */
/* PoP: hrd2_retry_indeterminate @ tui_gateway/hosted_room_driver.py:retry_indeterminate */
static json_t *hrd2_retry_indeterminate(hrd2_env *e, const json_t *in) {
    const json_t *identity = json_obj_get(in, "identity");
    const json_t *exec_gen = json_obj_get(in, "execution_generation");
    json_t *task = hrd2_state_get_task(e, identity);
    if (hrd2_is_raise(task)) return task;
    json_t *err = NULL;
    const json_t *teg = hrd2_sub(task, "execution_generation", &err);
    if (err) { json_free(task); return err; }
    /* if execution_generation not in (None, task["execution_generation"]): raise */
    if (!hrd2_is_null(exec_gen) && !hrd2_json_eq(exec_gen, teg)) {
        json_free(task);
        return hrd2_raise_spec("StaleTaskError", "task attempt changed before retry");
    }
    const json_t *tst = hrd2_sub(task, "status", &err);
    if (err) { json_free(task); return err; }
    const char *ts = hrd2_str(tst);
    if (!ts || (strcmp(ts, "indeterminate") != 0 && strcmp(ts, "deferred") != 0)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "cannot retry task in state '%s'", ts ? ts : "");
        json_free(task);
        return hrd2_raise_spec("InvalidTaskTransitionError", buf);
    }
    const json_t *rid = hrd2_attr(identity, "room_id", &err);
    if (err) { json_free(task); return err; }
    json_t *binding = hrd2_self_binding_for_room(e, hrd2_sval(rid));
    if (hrd2_is_raise(binding)) { json_free(task); return binding; }
    if (hrd2_is_null(binding)) {
        json_free(task);
        return hrd2_raise_spec("RoomUnavailableError", "hosted room is unavailable");
    }
    json_t *lease = hrd2_self_ensure_lease(e, binding);
    if (hrd2_is_raise(lease)) { json_free(binding); json_free(task); return lease; }
    if (strcmp(ts, "deferred") == 0) {
        json_t *r = hrd2_retry_deferred_of(e, binding, task, lease);
        json_free(binding); json_free(task); json_free(lease);
        return r;
    }
    json_t *inspection = hrd2_inspect_recovery_session_of(e, binding, task);
    if (hrd2_is_raise(inspection)) { json_free(binding); json_free(task); json_free(lease); return inspection; }
    const json_t *terminal = json_obj_get(inspection, "terminal");
    if (!hrd2_is_null(terminal)) {
        json_t *r = hrd2_self_resolve_indeterminate(e, binding, task, lease, terminal, true);
        json_free(inspection); json_free(binding); json_free(task); json_free(lease);
        return r;
    }
    const json_t *ist = json_obj_get(inspection, "status");
    if (hrd2_str(ist) && strcmp(hrd2_str(ist), "cancelled") == 0) {
        char cid[160];
        snprintf(cid, sizeof(cid), "remote-cancel:%s", hrd2_sval(teg));
        json_t *extra = json_object();
        json_set(extra, "cancel_id", json_string(cid));
        json_t *r = hrd2_self_fenced(e, "resolve_indeterminate_cancellation", binding, task, lease, true, extra);
        json_free(extra); json_free(inspection); json_free(binding); json_free(task); json_free(lease);
        return r;
    }
    const json_t *iact = json_obj_get(inspection, "active");
    if (hrd2_truth(iact)) {
        json_t *sb = hrd2_self_set_blocked(e, hrd2_sval(rid), true);
        if (hrd2_is_raise(sb)) { json_free(inspection); json_free(binding); json_free(task); json_free(lease); return sb; }
        json_free(sb);
        json_free(inspection); json_free(binding); json_free(task); json_free(lease);
        return hrd2_raise_spec("InvalidTaskTransitionError",
            "cannot retry while the original task attempt is still active");
    }
    json_t *r = hrd2_self_requeue(e, "requeue_indeterminate_task", task, lease, hrd2_sval(rid));
    json_free(inspection); json_free(binding); json_free(task); json_free(lease);
    return r;
}

/* ── deferred_stop_confirmed (public op) ──────────────────────────────────── */
/* PoP: hrd2_deferred_stop_confirmed @ tui_gateway/hosted_room_driver.py:deferred_stop_confirmed */
static json_t *hrd2_deferred_stop_confirmed(hrd2_env *e, const json_t *in) {
    const char *room_id = json_get_str(in, "room_id", "");
    const json_t *task = json_obj_get(in, "task");
    json_t *binding = hrd2_self_binding_for_room(e, room_id);
    if (hrd2_is_raise(binding)) return binding;
    if (hrd2_is_null(binding)) return json_bool(false);
    json_t *served = hrd2_self_served_elsewhere(e, binding, task);
    if (hrd2_is_raise(served)) { json_free(binding); return served; }
    bool elsewhere = hrd2_truth(served);
    json_free(served);
    if (!elsewhere) { json_free(binding); return json_bool(true); }
    /* try: open_session; if sid None: return transport is not None;
     *      else return _interrupt_stopping_task */
    json_t *opened = hrd2_self_open_session(e, binding, task, false);
    if (hrd2_is_raise(opened)) {
        if (hrd2_is_base(hrd2_kind(opened))) { json_free(binding); return opened; }
        json_t *err = NULL;
        const json_t *ident = hrd2_sub(task, "identity", &err);
        if (err) { json_free(opened); json_free(binding); return err; }
        const json_t *tid = hrd2_attr(ident, "task_id", &err);
        if (err) { json_free(opened); json_free(binding); return err; }
        char buf[1024];
        snprintf(buf, sizeof(buf), "stop of deferred task %s is unconfirmed: %s", hrd2_sval(tid), hrd2_msg(opened));
        json_free(opened);
        json_t *rec = hrd2_self_record_error(e, buf);
        if (hrd2_is_raise(rec)) { json_free(binding); return rec; }
        json_free(rec);
        json_free(binding);
        return json_bool(false);
    }
    const json_t *transport = json_get(opened, 0);
    const json_t *sid = json_get(opened, 2);
    json_t *ret;
    if (hrd2_is_null(sid)) {
        ret = json_bool(!hrd2_is_null(transport));
        json_free(opened);
    } else {
        json_free(opened);
        ret = hrd2_stop_outcome_of(e, binding, task);
        if (hrd2_is_raise(ret)) {
            if (hrd2_is_base(hrd2_kind(ret))) { json_free(binding); return ret; }
            json_t *err = NULL;
            const json_t *ident = hrd2_sub(task, "identity", &err);
            if (err) { json_free(ret); json_free(binding); return err; }
            const json_t *tid = hrd2_attr(ident, "task_id", &err);
            if (err) { json_free(ret); json_free(binding); return err; }
            char buf[1024];
            snprintf(buf, sizeof(buf), "stop of deferred task %s is unconfirmed: %s", hrd2_sval(tid), hrd2_msg(ret));
            json_free(ret);
            json_t *rec = hrd2_self_record_error(e, buf);
            if (hrd2_is_raise(rec)) { json_free(binding); return rec; }
            json_free(rec);
            json_free(binding);
            return json_bool(false);
        }
        bool is_true = ret && ret->type == JSON_BOOL && ret->bool_val;
        json_free(ret);
        ret = json_bool(is_true);
    }
    json_free(binding);
    return ret;
}

/* ── _expire_attempt_deadline ─────────────────────────────────────────────── */
static json_t *hrd2_expire_attempt_deadline_of(hrd2_env *e, const json_t *binding, const json_t *task_in, const json_t *lease_in) {
    json_t *err = NULL;
    const json_t *tst = hrd2_sub(task_in, "status", &err);
    if (err) return err;
    const char *ts = hrd2_str(tst);
    json_t *task = json_copy(task_in);
    if (ts && (strcmp(ts, "running") == 0 || strcmp(ts, "indeterminate") == 0)) {
        const json_t *ident = hrd2_sub(task, "identity", &err);
        if (err) { json_free(task); return err; }
        const json_t *eg = hrd2_sub(task, "execution_generation", &err);
        if (err) { json_free(task); return err; }
        double gen;
        err = hrd2_pyint(eg, &gen);
        if (err) { json_free(task); return err; }
        const json_t *cg = hrd2_sub(task, "cancel_generation", &err);
        if (err) { json_free(task); return err; }
        double cgen;
        err = hrd2_pyint(cg, &cgen);
        if (err) { json_free(task); return err; }
        char cid[160];
        snprintf(cid, sizeof(cid), "deadline:%.0f", gen);
        json_t *a = json_object();
        hrd2_arg_copy(a, "identity", ident);
        json_set(a, "placement", json_string(hrd2_placement(e)));
        json_set(a, "cancel_id", json_string(cid));
        json_set(a, "expected_cancel_generation", json_number(cgen));
        json_t *r = hrd2_call(e, "state.begin_task_cancel", a);
        if (hrd2_is_raise(r)) { json_free(task); return r; }
        json_free(task);
        task = r;
    } else if (!ts || strcmp(ts, "stopping") != 0) {
        json_free(task);
        return NULL;
    }
    const json_t *cid = hrd2_map_get(task, "cancel_id", &err);
    if (err) { json_free(task); return err; }
    char *cid_s = hrd2_pystr_or_empty(cid);
    if (!cid_s) { json_free(task); return hrd2_raise_spec("TypeError", "str() domain: scalar expected"); }
    bool deadline = strncmp(cid_s, "deadline:", 9) == 0;
    free(cid_s);
    if (!deadline) { json_free(task); return NULL; }
    json_t *lease = hrd2_self_renew(e, lease_in, true);
    if (hrd2_is_raise(lease)) { json_free(task); return lease; }
    json_t *fin = hrd2_finish_stop_of(e, binding, task, lease);
    json_free(lease);
    if (hrd2_is_raise(fin)) { json_free(task); return fin; }
    bool done = hrd2_truth(fin);
    json_free(fin);
    if (!done) {
        const json_t *ident = hrd2_sub(task, "identity", &err);
        if (err) { json_free(task); return err; }
        const json_t *tid = hrd2_attr(ident, "task_id", &err);
        if (err) { json_free(task); return err; }
        char buf[1024];
        snprintf(buf, sizeof(buf), "task %s exceeded its deadline; stop remains pending", hrd2_sval(tid));
        json_t *rec = hrd2_self_record_error(e, buf);
        if (hrd2_is_raise(rec)) { json_free(task); return rec; }
        json_free(rec);
    }
    json_free(task);
    return NULL;
}
/* PoP: hrd2_expire_attempt_deadline @ tui_gateway/hosted_room_driver.py:_expire_attempt_deadline */
static json_t *hrd2_expire_attempt_deadline(hrd2_env *e, const json_t *in) {
    return hrd2_expire_attempt_deadline_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"), json_obj_get(in, "lease"));
}

/* ── _inspect_session ─────────────────────────────────────────────────────── */
/* _NO_INSPECTION = _RecoveryInspection(terminal=None, active=False, status=None) */
static json_t *hrd2_no_inspection(void) {
    json_t *r = json_object();
    json_set(r, "terminal", json_null());
    json_set(r, "active", json_bool(false));
    json_set(r, "status", json_null());
    return r;
}
static json_t *hrd2_inspect_session_of(hrd2_env *e, const json_t *transport, const json_t *task, const json_t *session_id, bool read_history) {
    json_t *err = NULL;
    const json_t *payload = hrd2_sub(task, "payload", &err);
    if (err) return err;
    const json_t *profile = hrd2_sub(payload, "target_profile", &err);
    if (err) return err;
    json_t *receipt = NULL;
    if (read_history) {
        receipt = hrd2_self_terminal_from_history(e, transport, hrd2_str(profile), hrd2_sval(session_id), task);
        if (hrd2_is_raise(receipt)) return receipt;
    }
    json_t *tp = hrd2_transport_proj(transport);
    json_t *a = hrd2_transport_args(tp, hrd2_str(profile), hrd2_sval(session_id), true);
    json_t *info = hrd2_call(e, "transport.info", a);
    json_free(tp);
    if (hrd2_is_raise(info)) { json_free(receipt); return info; }
    json_t *rp = hrd2_self_report_pending_action(e, task, session_id, info);
    if (hrd2_is_raise(rp)) { json_free(info); json_free(receipt); return rp; }
    json_free(rp);
    const json_t *ident = hrd2_sub(task, "identity", &err);
    if (err) { json_free(info); json_free(receipt); return err; }
    json_t *act = hrd2_info_active_for(info, ident, false);
    if (hrd2_is_raise(act)) { json_free(info); json_free(receipt); return act; }
    const json_t *ist = hrd2_map_get(info, "status", &err);
    if (err) { json_free(act); json_free(info); json_free(receipt); return err; }
    char *st_s = hrd2_pystr_or_empty(ist);
    if (!st_s) { json_free(act); json_free(info); json_free(receipt); return hrd2_raise_spec("TypeError", "str() domain: scalar expected"); }
    json_t *r = json_object();
    json_set(r, "terminal", receipt ? receipt : json_null());
    json_set(r, "active", act);
    json_set(r, "status", st_s[0] ? json_string(st_s) : json_null());
    free(st_s);
    json_free(info);
    return r;
}
/* PoP: hrd2_inspect_session @ tui_gateway/hosted_room_driver.py:_inspect_session */
static json_t *hrd2_inspect_session(hrd2_env *e, const json_t *in) {
    return hrd2_inspect_session_of(e, json_obj_get(in, "transport"), json_obj_get(in, "task"),
                                   json_obj_get(in, "session_id"), json_get_bool(in, "read_history", false));
}

/* ── _inspect_recovery_session ────────────────────────────────────────────── */
static json_t *hrd2_inspect_recovery_session_of(hrd2_env *e, const json_t *binding, const json_t *task) {
    json_t *err = NULL;
    bool inspection_set = false;   /* inspection is _NO_INSPECTION until assigned */
    json_t *inspection = NULL;
    json_t *ret = NULL;
    const json_t *payload = hrd2_sub(task, "payload", &err);
    if (err) { ret = err; goto finally; }
    const json_t *profile = hrd2_sub(payload, "target_profile", &err);
    if (err) { ret = err; goto finally; }
    json_t *transport = hrd2_self_transport_for(e, binding, task);
    if (hrd2_is_raise(transport)) { ret = transport; goto finally; }
    hrd2_cm cm;
    json_t *enter = hrd2_turn_lock_enter(e, hrd2_str(profile), &cm);
    if (enter) { json_free(transport); ret = enter; goto finally; }
    const json_t *ident = hrd2_sub(task, "identity", &err);
    if (err) { json_t *ex = hrd2_turn_lock_exit(&cm); json_free(transport); ret = ex ? ex : err; goto finally; }
    const json_t *rid = hrd2_attr(ident, "room_id", &err);
    if (err) { json_t *ex = hrd2_turn_lock_exit(&cm); json_free(transport); ret = ex ? ex : err; goto finally; }
    json_t *sid = hrd2_self_resume_exact(e, transport, hrd2_sval(rid), hrd2_str(profile));
    if (hrd2_is_raise(sid)) { json_t *ex = hrd2_turn_lock_exit(&cm); json_free(transport); ret = ex ? ex : sid; goto finally; }
    if (!hrd2_is_null(sid)) {
        json_t *ins = hrd2_inspect_session_of(e, transport, task, sid, true);
        if (hrd2_is_raise(ins)) { json_free(sid); json_t *ex = hrd2_turn_lock_exit(&cm); json_free(transport); ret = ex ? ex : ins; goto finally; }
        inspection = ins;
        inspection_set = true;
    }
    json_free(sid);
    json_t *exit_err = hrd2_turn_lock_exit(&cm);
    json_free(transport);
    if (exit_err) { json_free(inspection); inspection = NULL; ret = exit_err; goto finally; }
    ret = inspection ? inspection : hrd2_no_inspection();   /* _NO_INSPECTION projection */
    inspection = NULL;   /* ownership transferred to ret */
finally:
    /* finally: if inspection is _NO_INSPECTION and task_progress is not None: ... */
    if (!inspection_set && hrd2_has_cb(e, "task_progress")) {
        const json_t *ident2 = hrd2_sub(task, "identity", &err);
        if (err) { json_free(ret); json_free(inspection); return err; }
        const json_t *rid2 = hrd2_attr(ident2, "room_id", &err);
        if (err) { json_free(ret); json_free(inspection); return err; }
        const json_t *tid2 = hrd2_attr(ident2, "task_id", &err);
        if (err) { json_free(ret); json_free(inspection); return err; }
        const json_t *eg2 = hrd2_sub(task, "execution_generation", &err);
        if (err) { json_free(ret); json_free(inspection); return err; }
        double gen;
        err = hrd2_pyint(eg2, &gen);
        if (err) { json_free(ret); json_free(inspection); return err; }
        json_t *a = json_object();
        json_set(a, "room_id", json_copy(rid2));
        json_set(a, "task_id", json_copy(tid2));
        json_set(a, "execution_generation", json_number(gen));
        json_set(a, "info", json_object());
        json_t *r = hrd2_call(e, "task_progress", a);
        if (hrd2_is_raise(r)) { json_free(ret); json_free(inspection); return r; }
        json_free(r);
    }
    json_free(inspection);
    return ret ? ret : json_null();
}
/* PoP: hrd2_inspect_recovery_session @ tui_gateway/hosted_room_driver.py:_inspect_recovery_session */
static json_t *hrd2_inspect_recovery_session(hrd2_env *e, const json_t *in) {
    return hrd2_inspect_recovery_session_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"));
}

/* ── _inspect_local_recovery_session ──────────────────────────────────────── */
static json_t *hrd2_inspect_local_recovery_session_of(hrd2_env *e, const json_t *task) {
    json_t *err = NULL;
    const json_t *payload = hrd2_sub(task, "payload", &err);
    if (err) return err;
    const json_t *profile = hrd2_sub(payload, "target_profile", &err);
    if (err) return err;
    hrd2_cm cm;
    json_t *enter = hrd2_turn_lock_enter(e, hrd2_str(profile), &cm);
    if (enter) return enter;
    /* self.rpc.resolve_exact: the attribute read happens INSIDE the with body */
    if (!hrd2_has_cb(e, "rpc")) {
        json_t *ex = hrd2_turn_lock_exit(&cm);
        return ex ? ex : hrd2_attr_raise(NULL, "resolve_exact");
    }
    const json_t *ident = hrd2_sub(task, "identity", &err);
    if (err) { json_t *ex = hrd2_turn_lock_exit(&cm); return ex ? ex : err; }
    const json_t *rid = hrd2_attr(ident, "room_id", &err);
    if (err) { json_t *ex = hrd2_turn_lock_exit(&cm); return ex ? ex : err; }
    char title[512];
    snprintf(title, sizeof(title), "Group: %s", hrd2_sval(rid));
    json_t *rpc = json_object();
    json_set(rpc, "id", json_string(hrd2_rpc_id(e)));
    json_t *a = hrd2_transport_args(rpc, hrd2_str(profile), title, false);
    json_t *session = hrd2_call(e, "transport.resolve_exact", a);
    json_free(rpc);
    if (hrd2_is_raise(session)) { json_t *ex = hrd2_turn_lock_exit(&cm); return ex ? ex : session; }
    if (hrd2_is_null(session)) {
        json_t *ex = hrd2_turn_lock_exit(&cm);
        return ex ? ex : hrd2_no_inspection();   /* _NO_INSPECTION */
    }
    json_t *sid = hrd2_session_id_of(session);
    json_free(session);
    if (hrd2_is_raise(sid)) { json_t *ex = hrd2_turn_lock_exit(&cm); return ex ? ex : sid; }
    json_t *rpc2 = json_object();
    json_set(rpc2, "id", json_string(hrd2_rpc_id(e)));
    json_t *ins = hrd2_inspect_session_of(e, rpc2, task, sid, false);
    json_free(rpc2);
    json_free(sid);
    if (hrd2_is_raise(ins)) { json_t *ex = hrd2_turn_lock_exit(&cm); return ex ? ex : ins; }
    json_t *exit_err = hrd2_turn_lock_exit(&cm);
    if (exit_err) { json_free(ins); return exit_err; }
    return ins;
}
/* PoP: hrd2_inspect_local_recovery_session @ tui_gateway/hosted_room_driver.py:_inspect_local_recovery_session */
static json_t *hrd2_inspect_local_recovery_session(hrd2_env *e, const json_t *in) {
    return hrd2_inspect_local_recovery_session_of(e, json_obj_get(in, "task"));
}

/* ── _harvest_previous_attempt ────────────────────────────────────────────── */
static json_t *hrd2_harvest_previous_attempt_of(hrd2_env *e, const json_t *binding, const json_t *task, const json_t *receipt) {
    json_t *err = NULL;
    const json_t *ident = hrd2_sub(task, "identity", &err);
    if (err) return err;
    const json_t *eg = hrd2_sub(task, "execution_generation", &err);
    if (err) return err;
    const json_t *cg = hrd2_sub(task, "cancel_generation", &err);
    if (err) return err;
    const json_t *brid = hrd2_attr(binding, "room_id", &err);
    if (err) return err;
    const json_t *rgid = hrd2_sub(task, "run_gateway_id", &err);
    if (err) return err;
    const json_t *bepoch = hrd2_attr(binding, "authority_epoch", &err);
    if (err) return err;
    const json_t *rpg = hrd2_sub(task, "run_process_generation", &err);
    if (err) return err;
    const json_t *rlg = hrd2_sub(task, "run_lease_generation", &err);
    if (err) return err;
    /* previous_attempt projection: identity + lease + generations */
    json_t *lease = json_object();
    json_set(lease, "room_id", json_copy(brid));
    json_set(lease, "gateway_id", json_copy(rgid));
    json_set(lease, "authority_epoch", json_copy(bepoch));
    json_set(lease, "process_generation", json_copy(rpg));
    json_set(lease, "lease_generation", json_copy(rlg));
    json_set(lease, "expires_at", json_number(0.0));
    json_set(lease, "reclaimed", json_bool(false));
    json_t *aproj = json_object();
    json_set(aproj, "identity", json_copy(ident));
    json_set(aproj, "lease", lease);
    json_set(aproj, "execution_generation", json_copy(eg));
    json_set(aproj, "cancel_generation", json_copy(cg));
    json_t *a = json_object();
    json_set(a, "attempt", aproj);
    json_set(a, "placement", json_string(hrd2_placement(e)));
    /* **asdict(receipt): status, settlement_id, result */
    if (hrd2_is_obj(receipt)) {
        for (size_t i = 0; i < receipt->c.count; i++)
            json_set(a, receipt->c.keys[i], json_copy(receipt->c.items[i]));
    }
    json_t *r = hrd2_call(e, "state.settle_task", a);
    if (hrd2_is_raise(r)) {
        const char *k = hrd2_kind(r);
        if (strcmp(k, "StaleLeaseError") == 0 || strcmp(k, "StaleTaskError") == 0) {
            json_free(r);   /* suppress */
        } else return r;
    } else json_free(r);
    return NULL;
}
/* PoP: hrd2_harvest_previous_attempt @ tui_gateway/hosted_room_driver.py:_harvest_previous_attempt */
static json_t *hrd2_harvest_previous_attempt(hrd2_env *e, const json_t *in) {
    return hrd2_harvest_previous_attempt_of(e, json_obj_get(in, "binding"), json_obj_get(in, "task"), json_obj_get(in, "receipt"));
}

/* ── _inspect_abandoned_attempts ──────────────────────────────────────────── */
static json_t *hrd2_inspect_abandoned_attempts_of(hrd2_env *e, const json_t *binding) {
    json_t *st = json_string("running");
    json_t *tasks = hrd2_self_tasks(e, binding, st);
    json_free(st);
    if (hrd2_is_raise(tasks)) return tasks;
    json_t *ret = NULL;
    if (hrd2_is_arr(tasks)) {
        for (size_t i = 0; i < json_len(tasks); i++) {
            const json_t *task = json_get(tasks, i);
            json_t *err = NULL;
            const json_t *rpg = hrd2_sub(task, "run_process_generation", &err);
            if (err) { ret = err; break; }
            if (hrd2_str(rpg) && strcmp(hrd2_str(rpg), hrd2_process_generation(e)) == 0)
                continue;
            json_t *transport = hrd2_self_transport_for(e, binding, task);
            if (hrd2_is_raise(transport)) { ret = transport; break; }
            bool is_local = hrd2_transport_is_rpc(e, transport);
            json_free(transport);
            json_t *inspection = is_local
                ? hrd2_inspect_local_recovery_session_of(e, task)
                : hrd2_inspect_recovery_session_of(e, binding, task);
            if (hrd2_is_raise(inspection)) { ret = inspection; break; }
            const json_t *terminal = json_obj_get(inspection, "terminal");
            if (!hrd2_is_null(terminal)) {
                json_t *h = hrd2_harvest_previous_attempt_of(e, binding, task, terminal);
                json_free(inspection);
                if (hrd2_is_raise(h)) { ret = h; break; }
                json_free(h);
            } else {
                const json_t *act = json_obj_get(inspection, "active");
                bool active = hrd2_truth(act);
                json_free(inspection);
                if (active) {
                    ret = hrd2_raise_spec("LeaseHeldError", "recovered session turn is still active");
                    break;
                }
            }
        }
    }
    json_free(tasks);
    return ret;
}
/* PoP: hrd2_inspect_abandoned_attempts @ tui_gateway/hosted_room_driver.py:_inspect_abandoned_attempts */
static json_t *hrd2_inspect_abandoned_attempts(hrd2_env *e, const json_t *in) {
    return hrd2_inspect_abandoned_attempts_of(e, json_obj_get(in, "binding"));
}

/* ── dispatch ─────────────────────────────────────────────────────────────── */
typedef json_t *(*hrd2_entry_fn)(hrd2_env *, const json_t *);
typedef struct { const char *name; hrd2_entry_fn fn; } hrd2_dispatch;
static const hrd2_dispatch hrd2_table[] = {
    {"_stop_outcome", hrd2_stop_outcome},
    {"_discard_unknown_stop", hrd2_discard_unknown_stop},
    {"_interrupt_stale_admission", hrd2_interrupt_stale_admission},
    {"_interrupt_stopping_task", hrd2_interrupt_stopping_task},
    {"_stop_or_settle", hrd2_stop_or_settle},
    {"_finish_stop", hrd2_finish_stop},
    {"_settle_stopping_completion", hrd2_settle_stopping_completion},
    {"_retry_stopping_tasks", hrd2_retry_stopping_tasks},
    {"cancel", hrd2_cancel},
    {"retry_indeterminate", hrd2_retry_indeterminate},
    {"_retry_deferred", hrd2_retry_deferred},
    {"deferred_stop_confirmed", hrd2_deferred_stop_confirmed},
    {"_expire_attempt_deadline", hrd2_expire_attempt_deadline},
    {"_inspect_session", hrd2_inspect_session},
    {"_inspect_recovery_session", hrd2_inspect_recovery_session},
    {"_inspect_local_recovery_session", hrd2_inspect_local_recovery_session},
    {"_inspect_abandoned_attempts", hrd2_inspect_abandoned_attempts},
    {"_harvest_previous_attempt", hrd2_harvest_previous_attempt},
};