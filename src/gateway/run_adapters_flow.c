/*
 * run_adapters_flow.c — gateway/run_adapters.py adapter-lifecycle FLOW cores
 * and leaves (universal-seam convention, twin of run_gaps.c / gateway_revive).
 *
 * Ports the DECISION SEQUENCES of the adapter connect/teardown lifecycle, the
 * config-driven branch ordering, the reconnect-queue bookkeeping and the
 * watcher pass: call order, short-circuit branches, early returns,
 * exception->fallback paths and mutation order of the live Python. Every live
 * I/O (logging, runtime-status writes, env reads, clocks, asyncio scheduling,
 * platform-registry probes, adapter connect/disconnect, metric recording)
 * enters through raf_seams_t.call(ctx, name, args); args is owned by raf_call
 * (freed after the borrowed call); results are NEW json_t with NULL == Python
 * None and {"__raise__":K,"msg":M} == a raised exception. BaseException kinds
 * (SystemExit/KeyboardInterrupt/CancelledError/GeneratorExit) pass through
 * every `except Exception` site.
 *
 * Modeling notes (differential oracle tests/oracle_run_adapters_flow.py):
 *  - Platform doubles are {"value": "<pv>"} objects; adapter doubles carry an
 *    "id" plus their live fields (platform value string, fatal_error_*,
 *    has_fatal_error, _sidecar_bind/_sidecar_port, ...). Seam args reference
 *    an adapter by its id string; claims are JSON arrays (Python tuples).
 *  - asyncio is modeled by ids: adapter.connect returns the awaited value V;
 *    asyncio.ensure_future returns a task id; asyncio.wait returns
 *    {done:[ids],pending:[ids]}; `await task` reuses V silently (no extra
 *    event). task.cancel / task.add_done_callback are seams keyed by id.
 *  - Runner state is injected json_t mutated in place; sets (_background_tasks,
 *    the track_task_in set) are tagged {"set":[ids]} and normalized to sorted
 *    id lists on both sides. _shutdown_event / hooks presence is a {} flag.
 *  - _log_suppressed semantics are inlined: Exception-kind raises from the
 *    wrapped block are swallowed and logged via logger.log(level, fmt, *args
 *    [, exc]); BaseException kinds propagate. exc_info=True logs no extra arg.
 *  - Real internal calls (ported siblings) run inline: with_timeout -> bounded
 *    -> wait_or_detach; reconnect_queue_entry -> credential/listener claim;
 *    reconnect_failed_platform -> flag_attention / bump_backoff / may_heal.
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
#include <errno.h>
#include "gateway_lifecycle_flow.h"

typedef struct raf_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} raf_seams_t;

/* ── universal-seam helpers ── */
static json_t *raf_call(const raf_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool raf_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *raf_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static json_t *raf_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool raf_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static json_t *raf_args(void) { return json_object(); }
static void raf_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void raf_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void raf_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void raf_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *raf_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
/* mutable object field access (state mutation paths). */
static json_t *raf_mget(json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *raf_pv(const json_t *platform) {
    const json_t *v = raf_oget(platform, "value");
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *raf_aid(const json_t *adapter) {
    const json_t *v = raf_oget(adapter, "id");
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
/* drop-None arg setter: only sets when v is a non-null node. */
static void raf_jn(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}

/* ── logging seams (RecLogger twin) ──
 * logger.<level>(fmt, *args): args object {fmt, args?}; no level field.
 * logger.log(level, fmt, *args, exc_info=?): {fmt, level, args?, exc_info?}.
 * Empty positional args omit the "args" key. Returns the seam result. */
static json_t *raf_log_named(const raf_seams_t *s, const char *name,
                             const char *fmt, json_t *args_arr) {
    json_t *a = raf_args();
    raf_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    return raf_call(s, name, a);
}
static json_t *raf_log(const raf_seams_t *s, const char *level,
                       const char *fmt, json_t *args_arr) {
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    return raf_log_named(s, name, fmt, args_arr);
}
static json_t *raf_log_log(const raf_seams_t *s, int level, const char *fmt,
                           json_t *args_arr, bool exc_info) {
    json_t *a = raf_args();
    raf_s(a, "fmt", fmt);
    raf_n(a, "level", level);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    if (exc_info) raf_b(a, "exc_info", true);
    return raf_call(s, "logger.log", a);
}
static json_t *raf_args1s(const char *a) {
    json_t *x = json_array();
    json_append(x, json_string(a ? a : ""));
    return x;
}
static json_t *raf_args2s(const char *a, const char *b) {
    json_t *x = json_array();
    json_append(x, json_string(a ? a : ""));
    json_append(x, json_string(b ? b : ""));
    return x;
}

/* ── Python value helpers ── */
/* Python numeric-literal underscores: every '_' must sit BETWEEN two digits. */
static bool raf_underscores_ok(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '_') {
            if (i == 0 || i + 1 >= n) return false;
            if (!(s[i - 1] >= '0' && s[i - 1] <= '9')) return false;
            if (!(s[i + 1] >= '0' && s[i + 1] <= '9')) return false;
        }
    }
    return true;
}
/* float(raw) with Python acceptance (underscores, inf/nan, no hex). ok=false
 * on ValueError. Caller has already stripped whitespace. */
static double raf_py_float(const char *raw, bool *ok) {
    *ok = false;
    if (!raw || !raw[0]) return 0.0;
    size_t n = strlen(raw);
    if (!raf_underscores_ok(raw, n)) return 0.0;
    /* strip underscores (Python numeric literal separators) */
    char *buf = malloc(n + 1);
    if (!buf) return 0.0;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) if (raw[i] != '_') buf[k++] = raw[i];
    buf[k] = '\0';
    if (k == 0) { free(buf); return 0.0; }
    /* reject hex / binary / octal prefixes (C strtod accepts hex floats) */
    const char *p = buf;
    if (*p == '+' || *p == '-') p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X' || p[1] == 'b' || p[1] == 'B'
                        || p[1] == 'o' || p[1] == 'O')) { free(buf); return 0.0; }
    char *end = NULL;
    errno = 0;
    double d = strtod(buf, &end);
    if (end == buf || !end || *end != '\0') { free(buf); return 0.0; }
    free(buf);
    *ok = true;
    return d;
}
/* int(v) for JSON number/string/bool. *ok=false sets *kind to the exception
 * class name (TypeError/ValueError/OverflowError). Truncates toward zero. */
static long long raf_py_int(const json_t *v, bool *ok, const char **kind) {
    *ok = true; *kind = NULL;
    if (!v || v->type == JSON_NULL) { *ok = false; *kind = "TypeError"; return 0; }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isnan(d) || isinf(d)) { *ok = false; *kind = "OverflowError"; return 0; }
        return (long long)d;  /* trunc toward zero */
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        /* Python int(str): surrounding whitespace, optional sign, digits with
         * underscores between digits; empty -> ValueError. */
        while (*s && isspace((unsigned char)*s)) s++;
        const char *end = s + strlen(s);
        while (end > s && isspace((unsigned char)end[-1])) end--;
        size_t n = (size_t)(end - s);
        if (!raf_underscores_ok(s, n)) { *ok = false; *kind = "ValueError"; return 0; }
        const char *p = s;
        bool neg = false;
        if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
        if (p == end) { *ok = false; *kind = "ValueError"; return 0; }
        long long acc = 0;
        bool any = false;
        for (; p < end; p++) {
            char c = *p;
            if (c == '_') continue;   /* validated between digits above */
            if (c < '0' || c > '9') { *ok = false; *kind = "ValueError"; return 0; }
            acc = acc * 10 + (c - '0');
            any = true;
        }
        if (!any) { *ok = false; *kind = "ValueError"; return 0; }
        return neg ? -acc : acc;
    }
    *ok = false; *kind = "TypeError";
    return 0;
}
/* str() of a scalar for log args (gflow_pystr handles containers via JSON). */
static char *raf_str(const json_t *v) { return gflow_pystr(v); }

/* Python type name of a JSON value (for binary-op TypeError messages). */
static const char *raf_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isinf(d) && d == (double)(long long)d) return "int";
        return "float";
    }
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    return "dict";
}
static bool raf_isnum(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}
static double raf_numval(const json_t *v) {
    return (v->type == JSON_BOOL) ? (v->bool_val ? 1.0 : 0.0) : v->num_val;
}
/* Python a + b for numeric operands; a non-numeric operand raises TypeError
 * with the live operand-type message (e.g. int + None). */
static json_t *raf_add(const json_t *a, const json_t *b) {
    if (!raf_isnum(a) || !raf_isnum(b)) {
        char msg[160];
        snprintf(msg, sizeof(msg), "unsupported operand type(s) for +: '%s' and '%s'",
                 raf_typename(a), raf_typename(b));
        return raf_raise("TypeError", msg);
    }
    return json_number(raf_numval(a) + raf_numval(b));
}

/* ── ISO-8601 datetime arithmetic (datetime.now(UTC) - timedelta). ──
 * Parses "YYYY-MM-DDTHH:MM:SS[.ffffff]<tz>" (tz = Z or +HH:MM / -HH:MM),
 * subtracts seconds, re-emits datetime.isoformat() preserving the original
 * offset (Z normalizes to +00:00; microseconds only when nonzero). */
static bool raf_iso_minus(const char *iso, double seconds, char *out, size_t outn) {
    if (!iso) return false;
    int Y, Mo, D, H, Mi, S;
    long long frac = 0;
    char tzsign = '+';
    int tzh = 0, tzm = 0;
    bool has_tz = false, is_z = false;
    int consumed = 0;
    if (sscanf(iso, "%4d-%2d-%2dT%2d:%2d:%2d%n", &Y, &Mo, &D, &H, &Mi, &S, &consumed) < 6)
        return false;
    const char *p = iso + consumed;
    if (*p == '.') {
        p++;
        char fb[7] = "000000";
        int fi = 0;
        while (*p >= '0' && *p <= '9' && fi < 6) fb[fi++] = *p++;
        while (*p >= '0' && *p <= '9') p++;
        frac = atoll(fb);
    }
    if (*p == 'Z' || *p == 'z') { p++; has_tz = true; is_z = true; }
    else if (*p == '+' || *p == '-') {
        has_tz = true;
        tzsign = *p++;
        if (sscanf(p, "%2d:%2d", &tzh, &tzm) < 2) {
            if (sscanf(p, "%2d%2d", &tzh, &tzm) < 2) tzh = tzm = 0;
        }
    }
    /* days from civil (Howard Hinnant algorithm) */
    long long y = Y;
    int m = Mo;
    if (m <= 2) y -= 1;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (unsigned)(m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)D - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = era * 146097 + (long long)doe - 719468;
    long long tzoff = (long long)tzh * 3600 + (long long)tzm * 60;
    if (tzsign == '-') tzoff = -tzoff;
    long long epoch_us = ((days * 86400LL + H * 3600LL + Mi * 60LL + S) - tzoff) * 1000000LL + frac;
    /* subtract seconds (round to nearest microsecond like timedelta) */
    long long sub_us = (long long)llround(seconds * 1000000.0);
    epoch_us -= sub_us;
    /* render back in the ORIGINAL offset (isoformat preserves it) */
    epoch_us += tzoff * 1000000LL;
    long long total_s = epoch_us / 1000000LL;
    long long us = epoch_us % 1000000LL;
    if (us < 0) { us += 1000000LL; total_s -= 1; }
    long long z = total_s + 719468LL * 86400LL;
    long long days2 = z / 86400LL;
    long long rem = z % 86400LL;
    if (rem < 0) { rem += 86400LL; days2 -= 1; }
    long long era2 = (days2 >= 0 ? days2 : days2 - 146096) / 146097;
    unsigned doe2 = (unsigned)(days2 - era2 * 146097);
    unsigned yoe2 = (doe2 - doe2 / 1460 + doe2 / 36524 - doe2 / 146096) / 365;
    long long y2 = (long long)yoe2 + era2 * 400;
    unsigned doy2 = doe2 - (365 * yoe2 + yoe2 / 4 - yoe2 / 100);
    unsigned mp = (5 * doy2 + 2) / 153;
    unsigned d2 = doy2 - (153 * mp + 2) / 5 + 1;
    unsigned m2 = mp + (mp < 10 ? 3 : -9);
    if (m2 <= 2) y2 += 1;
    int hh = (int)(rem / 3600), mi = (int)((rem % 3600) / 60), ss = (int)(rem % 60);
    char off[8] = "";
    if (has_tz) {
        if (is_z) snprintf(off, sizeof(off), "+00:00");
        else snprintf(off, sizeof(off), "%c%02d:%02d", tzsign, tzh, tzm);
    }
    if (us)
        snprintf(out, outn, "%04lld-%02u-%02uT%02d:%02d:%02d.%06lld%s",
                 y2, m2, d2, hh, mi, ss, us, off);
    else
        snprintf(out, outn, "%04lld-%02u-%02uT%02d:%02d:%02d%s",
                 y2, m2, d2, hh, mi, ss, off);
    return true;
}
/* ── Python str.strip() (both ends, all whitespace) ── */
static char *raf_strip(const char *s) {
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* PoP: raf_env_timeout_override @ gateway/run_adapters.py:_env_timeout_override */
json_t *raf_env_timeout_override(const char *name, const raf_seams_t *s) {
    json_t *a = raf_args();
    raf_s(a, "name", name);
    raf_s(a, "default", "");
    json_t *r = raf_call(s, "os.getenv", a);
    if (raf_raised(r)) return r;
    const char *raw = (r && r->type == JSON_STRING && r->str_val) ? r->str_val : "";
    char *stripped = raf_strip(raw);
    json_free(r);
    if (!stripped || !stripped[0]) { free(stripped); return NULL; }
    bool ok = false;
    double d = raf_py_float(stripped, &ok);
    if (!ok) {
        json_t *lr = raf_log(s, "warning", "Ignoring invalid %s=%r",
                             raf_args2s(name, stripped));
        free(stripped);
        if (raf_raised(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    free(stripped);
    double res = (d > 0.0) ? d : 0.0;   /* max(0.0, d): nan/-0/neg -> 0.0 */
    return json_number(res);
}

/* PoP: raf_wait_or_detach @ gateway/run_adapters.py:_wait_or_detach */
json_t *raf_wait_or_detach(const char *task_id, double timeout, const raf_seams_t *s) {
    json_t *a = raf_args();
    json_t *tasks = json_array();
    json_append(tasks, json_string(task_id ? task_id : ""));
    json_set(a, "tasks", tasks);
    raf_n(a, "timeout", timeout);
    json_t *rw = raf_call(s, "asyncio.wait", a);
    bool wait_raised = raf_raised(rw);
    json_t *wait_err = NULL;
    if (!wait_raised && !rw)
        wait_err = raf_raise("TypeError", "cannot unpack non-iterable NoneType object");
    const json_t *done = (!wait_raised && rw && rw->type == JSON_OBJECT)
                         ? json_obj_get(rw, "done") : NULL;
    bool in_done = false;
    if (done && done->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(done); i++) {
            const json_t *d = json_get(done, i);
            if (d && d->type == JSON_STRING && d->str_val
                && strcmp(d->str_val, task_id ? task_id : "") == 0) { in_done = true; break; }
        }
    }
    /* finally: if task not in done -> cancel + add_done_callback (a raise here
     * replaces any in-flight wait exception and skips the remaining step). */
    if (!in_done) {
        json_t *ca = raf_args();
        raf_s(ca, "id", task_id);
        json_t *rc = raf_call(s, "task.cancel", ca);
        if (raf_raised(rc)) { json_free(rw); json_free(wait_err); return rc; }
        json_free(rc);
        json_t *cba = raf_args();
        raf_s(cba, "id", task_id);
        raf_s(cba, "cb", "consume_detached_task_result");
        json_t *rcb = raf_call(s, "task.add_done_callback", cba);
        if (raf_raised(rcb)) { json_free(rw); json_free(wait_err); return rcb; }
        json_free(rcb);
    }
    if (wait_raised) { json_free(wait_err); return rw; }
    if (wait_err) { json_free(rw); return wait_err; }
    json_free(rw);
    return json_bool(in_done);
}

/* PoP: raf_connect_adapter_bounded @ gateway/run_adapters.py:_connect_adapter_bounded */
json_t *raf_connect_adapter_bounded(const json_t *self, const json_t *adapter,
                                    const json_t *platform, bool is_reconnect,
                                    bool initial, const raf_seams_t *s) {
    (void)self; (void)adapter;
    const char *pv = raf_pv(platform);
    json_t *a = raf_args();
    raf_s(a, "platform", pv);
    raf_b(a, "initial", initial);
    json_t *rt = raf_call(s, "platform_connect_timeout_secs", a);
    if (raf_raised(rt)) return rt;
    double timeout = (rt && rt->type == JSON_NUMBER) ? rt->num_val : 0.0;
    json_free(rt);
    json_t *ca = raf_args();
    raf_b(ca, "is_reconnect", is_reconnect);
    json_t *vc = raf_call(s, "adapter.connect", ca);
    if (raf_raised(vc)) return vc;
    if (timeout <= 0) return vc;   /* return await adapter.connect(...) */
    json_t *re = raf_call(s, "asyncio.ensure_future", raf_args());
    if (raf_raised(re)) { json_free(vc); return re; }
    const char *tid = (re && re->type == JSON_STRING) ? re->str_val : "";
    char *tid_copy = strdup(tid);
    json_free(re);
    json_t *wd = raf_wait_or_detach(tid_copy, timeout, s);
    free(tid_copy);
    if (raf_raised(wd)) { json_free(vc); return wd; }
    bool done = gflow_truth(wd);
    json_free(wd);
    if (done) {
        bool b = gflow_truth(vc);   /* bool(await task) */
        json_free(vc);
        return json_bool(b);
    }
    json_free(vc);
    char msg[160];
    snprintf(msg, sizeof(msg), "%s connect timed out after %gs", pv ? pv : "", timeout);
    return raf_raise("TimeoutError", msg);
}

/* PoP: raf_connect_adapter_with_timeout @ gateway/run_adapters.py:_connect_adapter_with_timeout */
json_t *raf_connect_adapter_with_timeout(const json_t *self, const json_t *adapter,
                                         const json_t *platform, bool is_reconnect,
                                         bool initial, const raf_seams_t *s) {
    json_t *ok = raf_connect_adapter_bounded(self, adapter, platform, is_reconnect, initial, s);
    if (raf_raised(ok)) {
        if (raf_is_base_exc(raf_kind(ok))) return ok;   /* except Exception passes BaseException */
        json_t *a = raf_args();
        raf_s(a, "adapter", raf_aid(adapter));
        raf_s(a, "platform", raf_pv(platform));
        raf_b(a, "is_reconnect", is_reconnect);
        raf_b(a, "ok", false);
        json_t *exc = json_object();
        raf_s(exc, "__raise__", raf_kind(ok));
        raf_s(exc, "msg", json_get_str(ok, "msg", ""));
        json_set(a, "exc", exc);
        json_t *rr = raf_call(s, "record_platform_connect", a);
        if (raf_raised(rr)) { json_free(ok); return rr; }
        json_free(rr);
        return ok;   /* re-raise */
    }
    json_t *a = raf_args();
    raf_s(a, "adapter", raf_aid(adapter));
    raf_s(a, "platform", raf_pv(platform));
    raf_b(a, "is_reconnect", is_reconnect);
    raf_b(a, "ok", gflow_truth(ok));
    json_t *rr = raf_call(s, "record_platform_connect", a);
    if (raf_raised(rr)) { json_free(ok); return rr; }
    json_free(rr);
    return ok;
}

/* PoP: raf_extend_systemd_boot_for @ gateway/run_adapters.py:_extend_systemd_boot_for */
json_t *raf_extend_systemd_boot_for(const json_t *self, const json_t *platform,
                                    const raf_seams_t *s) {
    json_t *a = raf_args();
    raf_s(a, "platform", raf_pv(platform));
    raf_b(a, "initial", true);
    json_t *rt = raf_call(s, "platform_connect_timeout_secs", a);
    if (raf_raised(rt)) return rt;
    double timeout = (rt && rt->type == JSON_NUMBER) ? rt->num_val : 0.0;
    json_free(rt);
    if (gflow_truth(raf_oget(self, "_running"))) return NULL;
    const json_t *config = raf_oget(self, "config");
    if (!config) return raf_raise("AttributeError", "missing attribute 'config'");
    const json_t *wd = raf_oget(config, "systemd_watchdog_seconds");
    if (!wd) return raf_raise("AttributeError", "missing attribute 'systemd_watchdog_seconds'");
    double wdn;
    if (wd->type == JSON_NUMBER) wdn = wd->num_val;
    else if (wd->type == JSON_BOOL) wdn = wd->bool_val ? 1.0 : 0.0;
    else return raf_raise("TypeError", "'<=' not supported between instances of 'str' and 'int'");
    if (wdn <= 0) return NULL;
    if (timeout <= 0) return NULL;
    json_t *ea = raf_args();
    raf_n(ea, "seconds", timeout + 30.0);   /* _SYSTEMD_BOOT_STEP_MARGIN_SECS */
    json_t *re = raf_call(s, "extend_start_timeout", ea);
    if (raf_raised(re)) return re;
    json_free(re);
    return NULL;
}

/* PoP: raf_handle_reaction_event @ gateway/run_adapters.py:_handle_reaction_event */
json_t *raf_handle_reaction_event(const json_t *self, const json_t *ctx,
                                  const raf_seams_t *s) {
    const json_t *en = raf_oget(ctx, "event_name");
    char *evname = gflow_truth(en) ? raf_str(en) : strdup("reaction:added");
    /* with _log_suppressed(DEBUG, fmt, exc_info=True): await self.hooks.emit(...) */
    json_t *r;
    if (!raf_oget(self, "hooks")) {
        r = raf_raise("AttributeError", "missing attribute 'hooks'");
    } else {
        json_t *a = raf_args();
        raf_s(a, "event_name", evname);
        raf_j(a, "ctx", ctx);
        r = raf_call(s, "hooks.emit", a);
    }
    free(evname);
    if (raf_raised(r)) {
        if (raf_is_base_exc(raf_kind(r))) return r;
        json_t *lr = raf_log_log(s, 10, "[Gateway] reaction hook emit failed", NULL, true);
        json_free(r);
        if (raf_raised(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    json_free(r);
    return NULL;
}

/* PoP: raf_on_all_adapters_down @ gateway/run_adapters.py:_on_all_adapters_down */
json_t *raf_on_all_adapters_down(const json_t *self) {
    const json_t *config = raf_oget(self, "config");
    const json_t *val = config ? raf_oget(config, "on_all_adapters_down") : NULL;
    if (val && val->type == JSON_STRING && val->str_val
        && (strcmp(val->str_val, "exit") == 0 || strcmp(val->str_val, "stay_alive") == 0))
        return json_string(val->str_val);
    return json_string("exit");
}
/* PoP: raf_adapter_listener_claim @ gateway/run_adapters.py:_adapter_listener_claim */
json_t *raf_adapter_listener_claim(const json_t *platform, const json_t *adapter) {
    const char *pv = raf_pv(platform);
    if (!pv || strcmp(pv, "photon") != 0) return NULL;
    const json_t *bind = raf_oget(adapter, "_sidecar_bind");
    if (!bind || bind->type != JSON_STRING) return NULL;
    char *bs = raf_strip(bind->str_val);
    if (!bs || !bs[0]) { free(bs); return NULL; }
    const json_t *port = raf_oget(adapter, "_sidecar_port");
    bool ok = false;
    const char *kind = NULL;
    long long p = raf_py_int(port, &ok, &kind);
    if (!ok) { free(bs); return NULL; }   /* (TypeError, ValueError, OverflowError) */
    for (char *q = bs; *q; q++) *q = (char)tolower((unsigned char)*q);
    json_t *r = json_array();
    json_append(r, json_string("listener"));
    json_append(r, json_string("photon"));
    json_append(r, json_string(bs));
    json_append(r, json_number((double)p));
    free(bs);
    return r;
}

/* PoP: raf_adapter_credential_claim @ gateway/run_adapters.py:_adapter_credential_claim */
json_t *raf_adapter_credential_claim(const json_t *platform, const json_t *adapter,
                                     const raf_seams_t *s) {
    json_t *a = raf_args();
    raf_s(a, "adapter", raf_aid(adapter));
    json_t *fp = raf_call(s, "adapter_credential_fingerprint", a);
    if (raf_raised(fp)) return fp;
    if (!fp || fp->type == JSON_NULL) { json_free(fp); return NULL; }
    json_t *r = json_array();
    const char *pv = raf_pv(platform);
    json_append(r, json_string(pv ? pv : ""));
    json_append(r, fp);   /* adopts the fingerprint node */
    return r;
}

/* PoP: raf_reconnect_queue_entry @ gateway/run_adapters.py:_reconnect_queue_entry */
json_t *raf_reconnect_queue_entry(const json_t *self, const json_t *platform,
                                  const json_t *adapter, const json_t *platform_config,
                                  double attempts, double delay, bool queued,
                                  const raf_seams_t *s) {
    (void)self;
    json_t *now_r = raf_call(s, "time.monotonic", raf_args());
    if (raf_raised(now_r)) return now_r;
    double now = (now_r && now_r->type == JSON_NUMBER) ? now_r->num_val : 0.0;
    json_free(now_r);
    json_t *e = json_object();
    raf_j(e, "config", platform_config);
    raf_n(e, "attempts", attempts);
    raf_n(e, "next_retry", now + delay);
    if (queued) raf_n(e, "queued_at", now);
    json_t *cc = raf_adapter_credential_claim(platform, adapter, s);
    if (raf_raised(cc)) { json_free(e); return cc; }
    json_set(e, "credential_claim", cc ? cc : json_null());
    json_t *lc = raf_adapter_listener_claim(platform, adapter);
    json_set(e, "listener_claim", lc ? lc : json_null());
    raf_j(e, "predecessor", adapter);
    return e;
}

/* PoP: raf_queue_retryable_best_effort @ gateway/run_adapters.py:_queue_retryable_best_effort */
json_t *raf_queue_retryable_best_effort(const json_t *self, const json_t *adapter,
                                        const char *why, const raf_seams_t *s) {
    (void)self;
    const json_t *pf = raf_oget(adapter, "platform");
    if (!pf) return raf_raise("AttributeError", "missing attribute 'platform'");
    const char *pv = (pf->type == JSON_STRING) ? pf->str_val : NULL;
    json_t *a = raf_args();
    raf_s(a, "adapter", raf_aid(adapter));
    json_t *r = raf_call(s, "queue_retryable_fatal_platform", a);
    if (raf_raised(r)) {
        if (raf_is_base_exc(raf_kind(r))) return r;
        json_t *lr = raf_log_log(s, 10, "Failed to queue %s after fatal-handler %s",
                                 raf_args2s(pv, why), true);
        json_free(r);
        if (raf_raised(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    json_free(r);
    return NULL;
}

/* PoP: raf_retain_background_task @ gateway/run_adapters.py:_retain_background_task */
json_t *raf_retain_background_task(json_t *self, const char *task_id,
                                   const raf_seams_t *s) {
    json_t *bt = raf_mget(self, "_background_tasks");
    const json_t *items0 = bt ? raf_oget(bt, "set") : NULL;
    bool is_set = items0 && items0->type == JSON_ARRAY;
    json_t *setnode;
    if (!is_set) {
        setnode = json_object();
        json_set(setnode, "set", json_array());
        json_set(self, "_background_tasks", setnode);
    } else {
        setnode = bt;
    }
    json_t *items = raf_mget(setnode, "set");
    bool found = false;
    for (size_t i = 0; i < json_len(items); i++) {
        const json_t *d = json_get(items, i);
        if (d && d->type == JSON_STRING && d->str_val && strcmp(d->str_val, task_id) == 0) {
            found = true; break;
        }
    }
    if (!found) json_append(items, json_string(task_id));
    json_t *a = raf_args();
    raf_s(a, "id", task_id);
    raf_s(a, "cb", "set.discard");
    json_t *r = raf_call(s, "task.add_done_callback", a);
    if (raf_raised(r)) return r;
    json_free(r);
    return json_string(task_id);
}

/* PoP: raf_track_task_in @ gateway/run_adapters.py:_track_task_in */
json_t *raf_track_task_in(json_t *tasks_arr, const char *task_id, const raf_seams_t *s) {
    bool found = false;
    for (size_t i = 0; i < json_len(tasks_arr); i++) {
        const json_t *d = json_get(tasks_arr, i);
        if (d && d->type == JSON_STRING && d->str_val && strcmp(d->str_val, task_id) == 0) {
            found = true; break;
        }
    }
    if (!found) json_append(tasks_arr, json_string(task_id));
    json_t *a = raf_args();
    raf_s(a, "id", task_id);
    raf_s(a, "cb", "set.discard");
    json_t *r = raf_call(s, "task.add_done_callback", a);
    if (raf_raised(r)) return r;
    json_free(r);
    return json_string(task_id);
}

/* PoP: raf_request_clean_exit @ gateway/run_adapters.py:_request_clean_exit */
json_t *raf_request_clean_exit(json_t *self, const char *reason, const raf_seams_t *s) {
    json_set(self, "_exit_cleanly", json_bool(true));
    json_set(self, "_exit_reason", json_string(reason ? reason : ""));
    if (!raf_oget(self, "_shutdown_event"))
        return raf_raise("AttributeError", "missing attribute '_shutdown_event'");
    json_t *r = raf_call(s, "shutdown_event.set", raf_args());
    if (raf_raised(r)) return r;
    json_free(r);
    return NULL;
}
#define RAF_FLAG_FMT \
    "%s has been failing/reconnecting continuously for %.1f hours (%d attempts) — flagging " \
    "NEEDS_ATTENTION. Retries continue, but this usually means a permanent problem (revoked " \
    "credentials, missing intents, broken sidecar). Check `hermes status` / `/platform list`."

/* PoP: raf_flag_reconnect_needs_attention @ gateway/run_adapters.py:_flag_reconnect_needs_attention */
json_t *raf_flag_reconnect_needs_attention(const json_t *self, const json_t *platform,
                                           json_t *info, double now, const char *status_key,
                                           const raf_seams_t *s) {
    (void)self;
    if (gflow_truth(raf_oget(info, "attention_flagged"))) return json_bool(false);
    const json_t *att = raf_oget(info, "attempts");
    double attempts = (att && att->type == JSON_NUMBER) ? att->num_val : 0.0;
    if (attempts < 1) return json_bool(false);
    json_t *a = raf_args();
    raf_j(a, "info", info);
    raf_n(a, "now", now);
    json_t *na = raf_call(s, "_reconnect_needs_attention", a);
    if (raf_raised(na)) return na;
    bool needs = gflow_truth(na);
    json_free(na);
    if (!needs) return json_bool(false);
    json_set(info, "attention_flagged", json_bool(true));
    const json_t *qa = raf_oget(info, "queued_at");
    double queued_at = (qa && qa->type == JSON_NUMBER) ? qa->num_val : now;
    double queued_for = now - queued_at;
    const char *pv = raf_pv(platform);
    const char *sk = (status_key && status_key[0]) ? status_key : pv;
    json_t *la = json_array();
    json_append(la, json_string(sk ? sk : ""));
    json_append(la, json_number(queued_for / 3600.0));
    json_append(la, json_number(attempts));
    json_t *lr = raf_log(s, "warning", RAF_FLAG_FMT, la);
    if (raf_raised(lr)) return lr;
    json_free(lr);
    json_t *da = raf_args();
    raf_s(da, "tz", "UTC");
    json_t *dn = raf_call(s, "datetime.now", da);
    if (raf_raised(dn)) return dn;
    const char *iso = (dn && dn->type == JSON_STRING) ? dn->str_val : NULL;
    char *iso_copy = iso ? strdup(iso) : NULL;
    json_free(dn);
    char since[64];
    bool okiso = raf_iso_minus(iso_copy, queued_for, since, sizeof(since));
    free(iso_copy);
    if (!okiso) return raf_raise("ValueError", "invalid datetime.now script");
    json_t *ua = raf_args();
    raf_s(ua, "status_key", sk);
    raf_s(ua, "platform_state", "retrying");
    raf_b(ua, "needs_attention", true);
    raf_s(ua, "retrying_since", since);
    json_t *ur = raf_call(s, "update_platform_runtime_status", ua);
    if (raf_raised(ur)) return ur;
    json_free(ur);
    return json_bool(true);
}

/* PoP: raf_bump_reconnect_backoff @ gateway/run_adapters.py:_bump_reconnect_backoff */
json_t *raf_bump_reconnect_backoff(const json_t *self, const json_t *platform,
                                   json_t *info, double attempt, const json_t *error_code,
                                   const json_t *error_message, const raf_seams_t *s) {
    (void)self;
    const char *pv = raf_pv(platform);
    json_t *ua = raf_args();
    raf_s(ua, "status_key", pv);
    raf_s(ua, "platform_state", "retrying");
    raf_jn(ua, "error_code", error_code);
    raf_jn(ua, "error_message", error_message);
    json_t *ur = raf_call(s, "update_platform_runtime_status", ua);
    if (raf_raised(ur)) return ur;
    json_free(ur);
    bool retry_now = gflow_truth(raf_oget(info, "retry_now"));
    json_obj_del(info, "retry_now");   /* info.pop("retry_now", False) */
    json_t *backoff_node;
    if (retry_now) {
        backoff_node = json_number(0);
    } else {
        json_t *ba = raf_args();
        raf_n(ba, "attempt", attempt);
        backoff_node = raf_call(s, "_reconnect_backoff", ba);
        if (raf_raised(backoff_node)) return backoff_node;
    }
    json_set(info, "attempts", json_number(attempt));
    json_t *mr = raf_call(s, "time.monotonic", raf_args());
    if (raf_raised(mr)) { json_free(backoff_node); return mr; }
    json_t *sum = raf_add(mr, backoff_node);   /* time.monotonic() + backoff */
    json_free(mr);
    if (raf_raised(sum)) { json_free(backoff_node); return sum; }
    json_set(info, "next_retry", sum);
    json_t *ret = backoff_node ? json_copy(backoff_node) : json_null();
    json_free(backoff_node);
    return ret;
}

/* PoP: raf_adapter_may_heal @ gateway/run_adapters.py:_adapter_may_heal */
json_t *raf_adapter_may_heal(const json_t *self, const json_t *platform,
                             const json_t *platform_config, const json_t *builtin_list,
                             const raf_seams_t *s) {
    (void)self;
    const char *pv = raf_pv(platform);
    if (builtin_list && builtin_list->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(builtin_list); i++) {
            const json_t *b = json_get(builtin_list, i);
            if (b && b->type == JSON_STRING && b->str_val && pv
                && strcmp(b->str_val, pv) == 0)
                return json_bool(false);   /* platform in _BUILTIN_ADAPTERS */
        }
    }
    json_t *a = raf_args();
    raf_s(a, "platform", pv);
    json_t *rr = raf_call(s, "platform_registry.is_registered", a);
    if (raf_raised(rr)) return rr;
    bool reg = gflow_truth(rr);
    json_free(rr);
    if (reg) return json_bool(false);
    json_t *ca = raf_args();
    raf_s(ca, "platform", pv);
    raf_j(ca, "platform_config", platform_config);
    json_t *cr = raf_call(s, "_platform_has_bot_credential", ca);
    if (raf_raised(cr)) return cr;
    bool cred = gflow_truth(cr);
    json_free(cr);
    return json_bool(cred);
}
/* _drop_from_reconnect_queue: log seam + state-effect del (the harness/oracle
 * stub applies the identical del; the entry is present on every scripted path). */
static json_t *raf_drop(json_t *self, const char *pv, const char *reason,
                        const raf_seams_t *s) {
    json_t *a = raf_args();
    raf_s(a, "platform", pv);
    raf_s(a, "reason", reason);
    json_t *r = raf_call(s, "drop_from_reconnect_queue", a);
    if (raf_raised(r)) return r;
    json_free(r);
    json_t *fp = raf_mget(self, "_failed_platforms");
    if (fp) json_obj_del(fp, pv);
    return NULL;
}

/* Inside the watcher try: a BaseException propagates at once; an Exception
 * becomes the caught `e` and runs the except body. */
#define RAF_TRY_RAISE(r) do { \
    if (raf_raised(r)) { \
        if (raf_is_base_exc(raf_kind(r))) { json_free(adapter); return (r); } \
        e = (r); goto except; \
    } \
} while (0)

/* PoP: raf_reconnect_failed_platform @ gateway/run_adapters.py:_reconnect_failed_platform */
json_t *raf_reconnect_failed_platform(json_t *self, const json_t *platform, double now,
                                      const json_t *builtin_list, const raf_seams_t *s) {
    const char *pv = raf_pv(platform);
    json_t *fp = raf_mget(self, "_failed_platforms");
    json_t *info = fp ? raf_mget(fp, pv) : NULL;
    if (!info || info->type == JSON_NULL) return NULL;
    if (gflow_truth(raf_oget(info, "paused"))) return NULL;
    json_t *fr = raf_flag_reconnect_needs_attention(self, platform, info, now, NULL, s);
    if (raf_raised(fr)) return fr;
    json_free(fr);
    const json_t *nr = raf_oget(info, "next_retry");
    if (!nr) return raf_raise("KeyError", "'next_retry'");
    double next_retry = (nr->type == JSON_NUMBER) ? nr->num_val : 0.0;
    if (now < next_retry) return NULL;
    const json_t *pcfg = raf_oget(info, "config");
    if (!pcfg) return raf_raise("KeyError", "'config'");
    const json_t *at = raf_oget(info, "attempts");
    if (!at) return raf_raise("KeyError", "'attempts'");
    double attempt = ((at->type == JSON_NUMBER) ? at->num_val : 0.0) + 1;
    json_obj_del(info, "retry_now");
    json_t *ca = raf_args();
    raf_s(ca, "platform", pv);
    raf_j(ca, "platform_config", pcfg);
    json_t *cr = raf_call(s, "_platform_has_bot_credential", ca);
    if (raf_raised(cr)) return cr;
    bool has_cred = gflow_truth(cr);
    json_free(cr);
    if (!has_cred) {
        json_t *dr = raf_drop(self, pv, "no bot credential on queued config", s);
        if (raf_raised(dr)) return dr;
        json_free(dr);
        return NULL;
    }
    json_t *la = json_array();
    json_append(la, json_string(pv ? pv : ""));
    json_append(la, json_number(attempt));
    json_t *lr = raf_log(s, "info", "Reconnecting %s (attempt %d)...", la);
    if (raf_raised(lr)) return lr;
    json_free(lr);

    json_t *adapter = NULL;
    json_t *e = NULL;
    json_t *r = NULL;
    double backoff = 0;

    r = raf_call(s, "_run_in_executor_with_context",
                 ({ json_t *a = raf_args(); raf_s(a, "fn", "platform_registry.get"); raf_s(a, "arg", pv); a; }));
    RAF_TRY_RAISE(r);
    json_free(r);

    r = raf_call(s, "create_adapter_off_loop_deps",
                 ({ json_t *a = raf_args(); raf_s(a, "platform", pv); raf_j(a, "config", pcfg); a; }));
    RAF_TRY_RAISE(r);
    adapter = r;   /* NULL == None; an adapter object is truthy by identity */

    if (!adapter || adapter->type == JSON_NULL) {
        json_t *mh = raf_adapter_may_heal(self, platform, pcfg, builtin_list, s);
        RAF_TRY_RAISE(mh);
        bool heals = gflow_truth(mh);
        json_free(mh);
        if (!heals) {
            r = raf_call(s, "mark_adapter_unavailable",
                         ({ json_t *a = raf_args(); raf_s(a, "status_key", pv); raf_s(a, "platform", pv);
                            raf_b(a, "retrying", false); raf_b(a, "needs_attention", true); a; }));
            RAF_TRY_RAISE(r);
            json_free(r);
            json_t *dr = raf_drop(self, pv, "adapter creation returned None", s);
            RAF_TRY_RAISE(dr);
            json_free(dr);
            json_free(adapter);
            return NULL;
        }
        r = raf_call(s, "rearm_failed_plugin_load",
                     ({ json_t *a = raf_args(); raf_s(a, "platform", pv); raf_j(a, "info", info); a; }));
        RAF_TRY_RAISE(r);
        json_free(r);
        json_t *um = raf_call(s, "_adapter_unavailable_message",
                              ({ json_t *a = raf_args(); raf_s(a, "platform", pv); a; }));
        RAF_TRY_RAISE(um);
        json_t *ec_node = json_string("adapter_unavailable");
        json_t *br = raf_bump_reconnect_backoff(self, platform, info, attempt, ec_node, um, s);
        json_free(ec_node);
        json_free(um);
        RAF_TRY_RAISE(br);
        backoff = (br->type == JSON_NUMBER) ? br->num_val : 0.0;
        json_free(br);
        la = json_array();
        json_append(la, json_string(pv ? pv : ""));
        json_append(la, json_number(backoff));
        lr = raf_log(s, "info", "Reconnect %s: no adapter yet, next retry in %ds", la);
        RAF_TRY_RAISE(lr);
        json_free(lr);
        json_free(adapter);
        return NULL;
    }

    const json_t *pred = raf_oget(info, "predecessor");
    r = raf_call(s, "carry_inbound_dedup",
                 ({ json_t *a = raf_args(); raf_s(a, "predecessor", pred ? raf_aid(pred) : NULL);
                    raf_s(a, "adapter", raf_aid(adapter)); a; }));
    RAF_TRY_RAISE(r);
    json_free(r);
    r = raf_call(s, "wire_adapter_handlers",
                 ({ json_t *a = raf_args(); raf_s(a, "adapter", raf_aid(adapter)); a; }));
    RAF_TRY_RAISE(r);
    json_free(r);
    r = raf_call(s, "connect_adapter_with_timeout",
                 ({ json_t *a = raf_args(); raf_s(a, "adapter", raf_aid(adapter)); raf_s(a, "platform", pv);
                    raf_b(a, "is_reconnect", true); a; }));
    RAF_TRY_RAISE(r);
    bool success = gflow_truth(r);
    json_free(r);

    if (success) {
        r = raf_call(s, "install_reconnected_adapter",
                     ({ json_t *a = raf_args(); raf_s(a, "platform", pv); raf_s(a, "adapter", raf_aid(adapter)); a; }));
        RAF_TRY_RAISE(r);
        json_free(r);
    } else {
        const json_t *hfe = raf_oget(adapter, "has_fatal_error");
        if (!hfe) { e = raf_raise("AttributeError", "missing attribute 'has_fatal_error'"); goto except; }
        bool has_fatal = gflow_truth(hfe);
        bool nonretry = false;
        if (has_fatal) {
            const json_t *fer = raf_oget(adapter, "fatal_error_retryable");
            if (!fer) { e = raf_raise("AttributeError", "missing attribute 'fatal_error_retryable'"); goto except; }
            nonretry = !gflow_truth(fer);
        }
        if (has_fatal && nonretry) {
            r = raf_call(s, "mark_platform_fatal",
                         ({ json_t *a = raf_args(); raf_s(a, "status_key", pv); raf_s(a, "adapter", raf_aid(adapter)); a; }));
            RAF_TRY_RAISE(r);
            json_free(r);
            const json_t *fem = raf_oget(adapter, "fatal_error_message");
            la = json_array();
            json_append(la, json_string(pv ? pv : ""));
            json_append(la, fem ? json_copy(fem) : json_null());
            lr = raf_log(s, "warning", "Reconnect %s: non-retryable error (%s), removing from retry queue", la);
            RAF_TRY_RAISE(lr);
            json_free(lr);
            r = raf_call(s, "_dispose_unused_adapter",
                         ({ json_t *a = raf_args(); raf_s(a, "adapter", raf_aid(adapter)); a; }));
            RAF_TRY_RAISE(r);
            json_free(r);
            json_obj_del(fp, pv);   /* del self._failed_platforms[platform] */
        } else {
            const json_t *fec = raf_oget(adapter, "fatal_error_code");
            const json_t *fem = raf_oget(adapter, "fatal_error_message");
            char *fem_or = gflow_truth(fem) ? raf_str(fem) : strdup("failed to reconnect");
            json_t *fem_node = json_string(fem_or);
            free(fem_or);
            json_t *br = raf_bump_reconnect_backoff(self, platform, info, attempt, fec, fem_node, s);
            json_free(fem_node);
            RAF_TRY_RAISE(br);
            backoff = (br->type == JSON_NUMBER) ? br->num_val : 0.0;
            json_free(br);
            la = json_array();
            json_append(la, json_string(pv ? pv : ""));
            json_append(la, json_number(backoff));
            lr = raf_log(s, "info", "Reconnect %s failed, next retry in %ds", la);
            RAF_TRY_RAISE(lr);
            json_free(lr);
            r = raf_call(s, "_dispose_unused_adapter",
                         ({ json_t *a = raf_args(); raf_s(a, "adapter", raf_aid(adapter)); a; }));
            RAF_TRY_RAISE(r);
            json_free(r);
            r = raf_call(s, "hand_over_held_inbound",
                         ({ json_t *a = raf_args(); raf_s(a, "adapter", raf_aid(adapter));
                            raf_s(a, "predecessor", pred ? raf_aid(pred) : NULL); a; }));
            RAF_TRY_RAISE(r);
            json_free(r);
        }
    }
    json_free(adapter);
    return NULL;

except:
    /* except Exception as e: BaseException kinds never reach here. */
    if (adapter) {
        r = raf_call(s, "_dispose_unused_adapter",
                     ({ json_t *a = raf_args(); raf_s(a, "adapter", raf_aid(adapter)); a; }));
        if (raf_raised(r)) { json_free(adapter); json_free(e); return r; }
        json_free(r);
        /* self.adapters.get(platform) is not adapter: the freshly created
         * adapter is never the installed one (object identity) -> always True. */
        const json_t *pred2 = raf_oget(info, "predecessor");
        r = raf_call(s, "hand_over_held_inbound",
                     ({ json_t *a = raf_args(); raf_s(a, "adapter", raf_aid(adapter));
                        raf_s(a, "predecessor", pred2 ? raf_aid(pred2) : NULL); a; }));
        if (raf_raised(r)) { json_free(adapter); json_free(e); return r; }
        json_free(r);
    }
    {
        char *e_str = strdup(json_get_str(e, "msg", ""));
        json_t *em_node = json_string(e_str);
        free(e_str);
        json_t *br = raf_bump_reconnect_backoff(self, platform, info, attempt, NULL, em_node, s);
        json_free(em_node);
        if (raf_raised(br)) { json_free(adapter); json_free(e); return br; }
        backoff = (br->type == JSON_NUMBER) ? br->num_val : 0.0;
        json_free(br);
    }
    {
        json_t *e_spec = json_object();
        raf_s(e_spec, "__raise__", raf_kind(e));
        raf_s(e_spec, "msg", json_get_str(e, "msg", ""));
        la = json_array();
        json_append(la, json_string(pv ? pv : ""));
        json_append(la, e_spec);
        json_append(la, json_number(backoff));
        lr = raf_log(s, "warning", "Reconnect %s error: %s, next retry in %ds", la);
        if (raf_raised(lr)) { json_free(adapter); json_free(e); return lr; }
        json_free(lr);
    }
    json_free(adapter);
    json_free(e);
    return NULL;
}
#undef RAF_TRY_RAISE
