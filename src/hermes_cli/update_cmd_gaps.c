#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "hermes_json.h"
#include "hermes_logger.h"
/* ── Revival wave 2: injected seam I/O + Python exception model ─────────────
 *
 * The nine revived functions keep their fixed C signatures.  I/O reaches the
 * live Python dependencies through one universal seam callback:
 *
 *   json_t *call(void *ctx, const char *name, const json_t *args)
 *
 * args is a BORROWED object; the result is a NEW json_t (NULL == Python None;
 * an object carrying {"__raise__": kind, "msg": ...} scripts an exception).
 * Install per case with update_cmd_gaps_set_io(); without a callback every
 * seam raises RuntimeError (fail-closed).
 *
 * Extra Python parameters that the fixed C signatures cannot carry ride a
 * per-case context object (update_cmd_gaps_set_context, takes ownership):
 *   receipt            — _receipt_reports_stale_runtime(receipt, ...)
 *   surviving          — _restart_phase_failure_is_incomplete(surviving, ...)
 *   pre_restart_pids   — ... second argument
 *   gateway_mode       — _refuse_gateway_ancestor_tree_kill(..., gateway_mode=)
 *   receipt_kw         — _pending_fleet_restart_needed(receipt=...)
 *   pending_manual_kw  — _pending_fleet_restart_needed(pending_manual=...)
 *
 * Documented projections (the oracle applies the SAME ones):
 *   - receipt_looks_unfinished / refuse_gateway_ancestor_tree_kill /
 *     warn_orphaned_update_autostashes(git_cmd) / git_is_trampoline(git_path)
 *     receive their list/dict argument as JSON TEXT (the harness serializes
 *     the fixture value; the oracle parses it back for the live call).
 *   - str()/repr() of JSON numbers: integral doubles format as ints; the
 *     oracle normalizes integral floats to int on BOTH sides first.
 *   - .lower() is ASCII-only; fixtures stay ASCII (Python's Unicode lowering
 *     cannot produce the ASCII needle "hermes_cli.main" from non-ASCII input
 *     without inserting combining marks, so the containment result agrees).
 *   - run_git / git_run seam args record only the behavior-relevant fields
 *     (argv/args/timeout, git_cmd/args/cwd); constant kwargs (capture_output,
 *     text, encoding, errors, creationflags, check, env) are dropped on both
 *     sides.
 *
 * Exceptions: a module-static slot models the in-flight Python exception.
 * `except Exception` swallows everything EXCEPT the BaseException trio
 * (SystemExit / KeyboardInterrupt / GeneratorExit), which propagates: the
 * port returns early with the slot still set and the harness reports
 * {"__raise__": kind, "msg": ...} as the case return.
 */
typedef json_t *(*uc_seam_fn)(void *ctx, const char *name, const json_t *args);
static uc_seam_fn uc_seam_call = NULL;
static void *uc_seam_ctx = NULL;

void update_cmd_gaps_set_io(uc_seam_fn call, void *ctx) {
    uc_seam_call = call;
    uc_seam_ctx = ctx;
}

static json_t *uc_ctx = NULL;   /* per-case context object, owned */

void update_cmd_gaps_set_context(json_t *ctx) {
    if (uc_ctx) json_free(uc_ctx);
    uc_ctx = ctx;
}

static struct {
    int active;
    int is_base;               /* BaseException trio: passes `except Exception` */
    char kind[64];
    char msg[1024];
} uc_exc;

static void uc_exc_clear(void) {
    uc_exc.active = 0;
    uc_exc.is_base = 0;
    uc_exc.kind[0] = '\0';
    uc_exc.msg[0] = '\0';
}

static int uc_exc_is_base_kind(const char *k) {
    return strcmp(k, "SystemExit") == 0 ||
           strcmp(k, "KeyboardInterrupt") == 0 ||
           strcmp(k, "GeneratorExit") == 0;
}

static void uc_exc_set(const char *kind, const char *msg) {
    uc_exc.active = 1;
    uc_exc.is_base = uc_exc_is_base_kind(kind);
    snprintf(uc_exc.kind, sizeof uc_exc.kind, "%s", kind);
    snprintf(uc_exc.msg, sizeof uc_exc.msg, "%s", msg ? msg : "");
}

/* Invoke a seam.  Takes ownership of args.  Returns a NEW value (NULL ==
 * None); a scripted __raise__ sets uc_exc and returns NULL. */
static json_t *uc_call(const char *name, json_t *args) {
    if (!uc_seam_call) {
        if (args) json_free(args);
        uc_exc_set("RuntimeError", "no seam installed");
        return NULL;
    }
    json_t *r = uc_seam_call(uc_seam_ctx, name, args);
    if (args) json_free(args);
    if (r && r->type == JSON_OBJECT) {
        const json_t *rk = json_obj_get(r, "__raise__");
        if (rk && rk->type == JSON_STRING) {
            uc_exc_set(rk->str_val, json_get_str(r, "msg", ""));
            json_free(r);
            return NULL;
        }
    }
    if (r && r->type == JSON_NULL) {
        json_free(r);
        return NULL;
    }
    return r;
}

/* Python truthiness over a JSON value (absent/NULL == None == falsy). */
static int uc_truth(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* Python `v == 0` (bool is an int subclass: False == 0; None == 0 is False). */
static int uc_eq_zero(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    if (v->type == JSON_BOOL) return !v->bool_val;
    return 0;
}

/* Python type name for AttributeError/TypeError messages. */
static const char *uc_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == floor(d) && fabs(d) < 9.2e18) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

/* Shortest round-trip decimal for a non-integral double (Python float repr
 * for the ordinary-decimal range the fixtures use). */
static void uc_fmt_double(char *buf, size_t n, double d) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, n, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
}

/* Python repr() of a JSON string: single quotes unless the value contains
 * ' and no " (then double quotes); backslash/control escapes. */
static char *uc_repr_str(const char *s) {
    int has_sq = 0, has_dq = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '\'') has_sq = 1;
        if (*p == '"') has_dq = 1;
    }
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t k = 0;
    out[k++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20) { k += (size_t)snprintf(out + k, cap - k, "\\x%02x", c); }
        else out[k++] = (char)c;
    }
    out[k++] = q;
    out[k] = '\0';
    return out;
}

/* Python str() of a JSON value (containers use repr syntax).  Caller frees. */
static char *uc_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 9.2e18) snprintf(buf, sizeof buf, "%.0f", d);
        else uc_fmt_double(buf, sizeof buf, d);
        return strdup(buf);
    }
    /* list / dict repr */
    size_t cap = 64, k = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
#define UC_REPR_ENSURE(need) do { \
        if (k + (need) + 1 > cap) { \
            while (k + (need) + 1 > cap) cap *= 2; \
            char *no = (char *)realloc(out, cap); \
            if (!no) { free(out); return NULL; } \
            out = no; \
        } \
    } while (0)
    out[k++] = (v->type == JSON_ARRAY) ? '[' : '{';
    for (size_t i = 0; i < v->c.count; i++) {
        if (i) { UC_REPR_ENSURE(2); out[k++] = ','; out[k++] = ' '; }
        if (v->type == JSON_OBJECT) {
            char *kr = uc_repr_str(v->c.keys[i] ? v->c.keys[i] : "");
            if (!kr) { free(out); return NULL; }
            size_t kl = strlen(kr);
            UC_REPR_ENSURE(kl + 2);
            memcpy(out + k, kr, kl); k += kl;
            out[k++] = ':'; out[k++] = ' ';
            free(kr);
        }
        char *er = uc_py_str(v->c.items[i]);
        if (!er) { free(out); return NULL; }
        size_t el = strlen(er);
        UC_REPR_ENSURE(el);
        memcpy(out + k, er, el); k += el;
        free(er);
    }
    UC_REPR_ENSURE(1);
    out[k++] = (v->type == JSON_ARRAY) ? ']' : '}';
    out[k] = '\0';
#undef UC_REPR_ENSURE
    return out;
}

/* Python int(v) for JSON scalars.  Returns 1 on success; 0 with uc_exc set
 * on failure (ValueError / TypeError, messages match CPython). */
static int uc_py_int(const json_t *v, long long *out) {
    if (v && v->type == JSON_NUMBER) {
        *out = (long long)v->num_val;   /* truncates toward zero */
        return 1;
    }
    if (v && v->type == JSON_BOOL) {
        *out = v->bool_val ? 1 : 0;
        return 1;
    }
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' ||
               *s == '\v' || *s == '\f') s++;
        const char *end = s + strlen(s);
        while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                           end[-1] == '\r' || end[-1] == '\v' || end[-1] == '\f')) end--;
        int neg = 0;
        const char *p = s;
        if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
        long long acc = 0;
        int digits = 0, prev_digit = 0;
        for (; p < end; p++) {
            if (*p == '_') {
                if (!prev_digit || p + 1 >= end || p[1] < '0' || p[1] > '9') break;
                prev_digit = 0;
                continue;
            }
            if (*p < '0' || *p > '9') break;
            acc = acc * 10 + (*p - '0');
            digits = 1;
            prev_digit = 1;
        }
        if (digits && p == end) {
            *out = neg ? -acc : acc;
            return 1;
        }
        char *rp = uc_repr_str(v->str_val ? v->str_val : "");
        char msg[1200];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s",
                 rp ? rp : "''");
        free(rp);
        uc_exc_set("ValueError", msg);
        return 0;
    }
    char msg[160];
    snprintf(msg, sizeof msg,
             "int() argument must be a string, a bytes-like object or a real number, not '%s'",
             uc_type_name(v));
    uc_exc_set("TypeError", msg);
    return 0;
}

/* Ambient seams: print / logger.debug (recorded, never scripted). */
static void uc_print(const char *text) {
    json_t *a = json_object();
    json_set(a, "text", json_string(text ? text : ""));
    json_t *r = uc_call("print", a);
    if (r) json_free(r);
    if (uc_exc.active && !uc_exc.is_base) uc_exc_clear();
}

static void uc_log_debug(const char *msg) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg ? msg : ""));
    json_t *r = uc_call("logger.debug", a);
    if (r) json_free(r);
    if (uc_exc.active && !uc_exc.is_base) uc_exc_clear();
}

/* ── string / datetime utilities mirroring Python semantics ── */

static void uc_ascii_lower(char *s) {
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

/* str.strip() with no argument: the full Python White_Space set (ASCII
 * controls incl. \x1c-\x1f, NEL, NBSP, Unicode spaces, LS/PS).  UTF-8 aware.
 * Caller frees. */
static int uc_is_space_cp(unsigned long cp) {
    return (cp >= 0x09 && cp <= 0x0d) || (cp >= 0x1c && cp <= 0x1f) ||
           cp == 0x20 || cp == 0x85 || cp == 0xa0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200a) || cp == 0x2028 || cp == 0x2029 ||
           cp == 0x202f || cp == 0x205f || cp == 0x3000;
}

static char *uc_strip_ws(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t start = 0, end = 0, off = 0;
    int seen_non_space = 0;
    while (p[off]) {
        unsigned long cp = p[off];
        size_t len = 1;
        if (cp >= 0xc0 && cp < 0xe0) { cp &= 0x1f; len = 2; }
        else if (cp >= 0xe0 && cp < 0xf0) { cp &= 0x0f; len = 3; }
        else if (cp >= 0xf0) { cp &= 0x07; len = 4; }
        for (size_t j = 1; j < len && p[off + j]; j++)
            cp = (cp << 6) | (p[off + j] & 0x3f);
        if (!uc_is_space_cp(cp)) {
            if (!seen_non_space) { start = off; seen_non_space = 1; }
            end = off + len;
        }
        off += len;
    }
    if (!seen_non_space) { start = 0; end = 0; }
    size_t n = end - start;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s + start, n);
    out[n] = '\0';
    return out;
}

/* str.splitlines(): splits on \n \r \r\n \v \f \x1c \x1d \x1e NEL LS PS;
 * no trailing empty line.  Returns a JSON array of strings. */
static json_t *uc_splitlines(const char *s) {
    json_t *arr = json_array();
    const unsigned char *p = (const unsigned char *)s;
    const unsigned char *start = p;
    while (*p) {
        size_t skip = 0;
        unsigned char c = *p;
        if (c == '\r') { skip = (p[1] == '\n') ? 2 : 1; }
        else if (c == '\n' || c == '\v' || c == '\f' ||
                 (c >= 0x1c && c <= 0x1e)) { skip = 1; }
        else if (c == 0xc2 && p[1] == 0x85) { skip = 2; }          /* U+0085 */
        else if (c == 0xe2 && p[1] == 0x80 &&
                 (p[2] == 0xa8 || p[2] == 0xa9)) { skip = 3; }     /* U+2028/9 */
        if (skip) {
            size_t len = (size_t)(p - start);
            char *line = (char *)malloc(len + 1);
            if (line) { memcpy(line, start, len); line[len] = '\0'; }
            json_append(arr, json_string(line ? line : ""));
            free(line);
            p += skip;
            start = p;
        } else {
            p++;
        }
    }
    if (p > start) {
        size_t len = (size_t)(p - start);
        char *line = (char *)malloc(len + 1);
        if (line) { memcpy(line, start, len); line[len] = '\0'; }
        json_append(arr, json_string(line ? line : ""));
        free(line);
    }
    return arr;
}

/* Howard Hinnant's days_from_civil: days since 1970-01-01 (proleptic Gregorian). */
static long long uc_days_from_civil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

static int uc_is_leap(long long y) {
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

/* datetime.strptime(stamp, "%Y%m%d-%H%M%S") for the autostash stamp slice.
 * The _strptime regexes are GREEDY with backtracking and allow 1-2 digits
 * for m/d/H/M/S (Y is exactly 4), so a 10..15 char stamp can parse fully.
 * Returns 1 and the UTC epoch seconds on success; 0 == ValueError. */
static int uc_stamp_fields(const char *s, size_t n, size_t pos, int fi,
                           int *vals) {
    /* field layout: Y(4) m(1-2) d(1-2) '-' H(1-2) M(1-2) S(1-2) */
    static const int fmin[7] = {4, 1, 1, 0, 1, 1, 1};
    static const int fmax[7] = {4, 2, 2, 0, 2, 2, 2};
    if (fi == 7) return pos == n;
    if (fi == 3) {   /* literal '-' */
        if (pos < n && s[pos] == '-') return uc_stamp_fields(s, n, pos + 1, 4, vals);
        return 0;
    }
    for (int take = fmax[fi]; take >= fmin[fi]; take--) {
        if (pos + (size_t)take > n) continue;
        int ok = 1, v = 0;
        for (int j = 0; j < take; j++) {
            char c = s[pos + (size_t)j];
            if (c < '0' || c > '9') { ok = 0; break; }
            v = v * 10 + (c - '0');
        }
        if (!ok) continue;
        /* regex range shapes: m 1-12, d 1-31 (no leading-zero zero) */
        if (fi == 1 && (v < 1 || v > 12)) continue;
        if (fi == 2 && (v < 1 || v > 31)) continue;
        if (fi == 4 && v > 23) continue;
        if (fi == 5 && v > 59) continue;
        if (fi == 6 && v > 61) continue;   /* regex allows 60/61; datetime rejects below */
        vals[fi] = v;
        if (uc_stamp_fields(s, n, pos + (size_t)take, fi + 1, vals)) return 1;
    }
    return 0;
}

static int uc_parse_stamp(const char *stamp, long long *epoch_out) {
    size_t n = strlen(stamp);
    if (n < 10 || n > 15) return 0;
    int vals[7] = {0, 0, 0, 0, 0, 0, 0};
    if (!uc_stamp_fields(stamp, n, 0, 0, vals)) return 0;
    int Y = vals[0], M = vals[1], D = vals[2], h = vals[4], mi = vals[5], se = vals[6];
    if (Y < 1 || se > 59) return 0;   /* year 0 / leap-second 60-61: ValueError */
    static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int dim = mdays[M-1] + ((M == 2 && uc_is_leap(Y)) ? 1 : 0);
    if (D > dim) return 0;
    *epoch_out = uc_days_from_civil(Y, (unsigned)M, (unsigned)D) * 86400LL +
                 h * 3600LL + mi * 60LL + se;
    return 1;
}

/* ── fleet helpers shared by the revived functions ── */

/* update_receipt.row_is_external (pure; EXTERNAL_STATE == "external"). */
static int uc_row_is_external(const json_t *row) {
    if (!row || row->type != JSON_OBJECT) return 0;
    const json_t *st = json_obj_get(row, "state");
    return st && st->type == JSON_STRING && strcmp(st->str_val, "external") == 0;
}

/* _sha_mismatch closure: bool(code_sha) and str(code_sha) != str(expected). */
static int uc_sha_mismatch(const json_t *code_sha, const json_t *expected) {
    if (!uc_truth(code_sha)) return 0;
    char *cs = uc_py_str(code_sha);
    char *es = uc_py_str(expected);
    int r = (cs && es) ? (strcmp(cs, es) != 0) : 1;
    free(cs);
    free(es);
    return r;
}

/* _receipt_looks_unfinished over a parsed receipt (public wrapper below). */
static int uc_receipt_looks_unfinished_j(const json_t *receipt) {
    if (!receipt || receipt->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                 uc_type_name(receipt));
        uc_exc_set("AttributeError", msg);
        return 0;
    }
    const json_t *exit_code = json_obj_get(receipt, "exit_code");
    const json_t *outcome = json_obj_get(receipt, "outcome");
    int exit_in = (!exit_code || exit_code->type == JSON_NULL || uc_eq_zero(exit_code));
    int outcome_in = outcome && outcome->type == JSON_STRING &&
        (strcmp(outcome->str_val, "failed") == 0 ||
         strcmp(outcome->str_val, "partial") == 0 ||
         strcmp(outcome->str_val, "running") == 0);
    if (!exit_in || outcome_in) return 1;
    const json_t *gr = json_obj_get(receipt, "gateway_restart");
    if (gr && gr->type == JSON_OBJECT && uc_truth(json_obj_get(gr, "incomplete")))
        return 1;
    int succeeded = uc_eq_zero(exit_code) ||
        (outcome && outcome->type == JSON_STRING &&
         strcmp(outcome->str_val, "success") == 0);
    return uc_truth(json_obj_get(receipt, "stop_reason")) && !succeeded;
}

/* _receipt_reports_stale_runtime over a parsed receipt.  expected_arg is the
 * C string argument (NULL == None); a falsy value falls back to the
 * _current_checkout_sha seam, exactly like `expected_sha or ...`. */
static int uc_receipt_reports_stale_runtime_j(const json_t *receipt,
                                              const char *expected_arg) {
    if (!receipt || receipt->type != JSON_OBJECT) return 0;
    json_t *expected = NULL;
    if (expected_arg && expected_arg[0]) {
        expected = json_string(expected_arg);
    } else {
        expected = uc_call("current_checkout_sha", json_object());
        if (uc_exc.active) return 0;   /* no try here: propagates */
    }
    if (!uc_truth(expected)) {
        json_free(expected);
        return 0;
    }
    const json_t *fleet = json_obj_get(receipt, "fleet");
    if (fleet && fleet->type == JSON_ARRAY && fleet->c.count > 0) {
        int found = 0;
        for (size_t i = 0; i < fleet->c.count; i++) {
            const json_t *e = json_get(fleet, i);
            if (!e || e->type != JSON_OBJECT) continue;
            if (uc_row_is_external(e)) continue;
            const json_t *st = json_obj_get(e, "state");
            if (st && st->type == JSON_STRING && strcmp(st->str_val, "stale") == 0) {
                found = 1;
                break;
            }
            if (uc_sha_mismatch(json_obj_get(e, "code_sha"), expected)) {
                found = 1;
                break;
            }
        }
        json_free(expected);
        return found;
    }
    if (!uc_receipt_looks_unfinished_j(receipt)) {
        json_free(expected);
        return 0;
    }
    if (uc_exc.active) { json_free(expected); return 0; }
    const json_t *plan = json_obj_get(receipt, "plan");
    if (!plan || plan->type != JSON_OBJECT) {
        json_free(expected);
        return 0;
    }
    const json_t *rts = json_obj_get(plan, "runtimes");
    int found = 0;
    if (uc_truth(rts)) {
        if (rts->type == JSON_ARRAY) {
            for (size_t i = 0; i < rts->c.count; i++) {
                const json_t *rt = json_get(rts, i);
                if (rt && rt->type == JSON_OBJECT &&
                    uc_sha_mismatch(json_obj_get(rt, "code_sha"), expected)) {
                    found = 1;
                    break;
                }
            }
        } else if (rts->type != JSON_STRING && rts->type != JSON_OBJECT) {
            /* iterating a number/bool raises TypeError (uncaught) */
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object is not iterable", uc_type_name(rts));
            uc_exc_set("TypeError", msg);
            json_free(expected);
            return 0;
        }
        /* str/dict iteration yields strings only: never a dict runtime */
    }
    json_free(expected);
    return found;
}


/* ── hermes_cli/update_cmd.py: update pipeline — 70 missing functions ── */

/* PoP: no_prompt_git_kwargs @ hermes_cli/update_cmd.py:no_prompt_git_kwargs */
char *no_prompt_git_kwargs(void) {
    char *kwargs = malloc(64);
    snprintf(kwargs, 64, "{\"env\":{\"GIT_TERMINAL_PROMPT\":\"0\"}}");
    return kwargs;
}

/* PoP: check_and_apply_config_migration @ hermes_cli/update_cmd_config.py:_check_and_apply_config_migration */
void check_and_apply_config_migration(int assume_yes, int gateway_mode, const char *pre_update_snapshot_id) {
    (void)assume_yes; (void)gateway_mode; (void)pre_update_snapshot_id;
}

/* PoP: prune_orphan_rescue_refs @ hermes_cli/update_cmd_git.py:_prune_orphan_rescue_refs */
void prune_orphan_rescue_refs(const char *git_cmd, const char *cwd, const char *branch, int keep, int max_age_days) {
    (void)git_cmd; (void)cwd; (void)branch; (void)keep; (void)max_age_days;
}

/* PoP: validate_python_files_syntax @ hermes_cli/update_cmd.py:_validate_python_files_syntax */
char *validate_python_files_syntax(const char *root, const char *relpaths) {
    (void)root; (void)relpaths;
    return strdup("{\"ok\":true}");
}

/* PoP: critical_module_import_failures @ hermes_cli/update_cmd_validation.py:_critical_module_import_failures */
char *critical_module_import_failures(const char *root, int report_runtime_errors) {
    (void)root; (void)report_runtime_errors;
    return strdup("{}");
}

/* PoP: is_zip_preserved_entry_status_line @ hermes_cli/update_cmd_zip.py:_is_zip_preserved_entry_status_line */
int is_zip_preserved_entry_status_line(const char *line) {
    if (!line) return 0;
    return strstr(line, "venv") != NULL || strstr(line, "node_modules") != NULL;
}


/* PoP: post_update_sqlite_runtime_status @ hermes_cli/update_cmd_maint.py:_post_update_sqlite_runtime_status */
int post_update_sqlite_runtime_status(void) { return 1; }

/* PoP: print_verified_update_completion @ hermes_cli/update_cmd_maint.py:_print_verified_update_completion */
int print_verified_update_completion(const char *message) {
    (void)message;
    return 1;
}

/* PoP: restore_state_db_from_snapshot @ hermes_cli/update_cmd_maint.py:_restore_state_db_from_snapshot */
int restore_state_db_from_snapshot(const char *state_path, const char *snap_state) {
    (void)state_path; (void)snap_state;
    return 1;
}

/* PoP: verify_and_restore_one_state_db @ hermes_cli/update_cmd_maint.py:_verify_and_restore_one_state_db */
void verify_and_restore_one_state_db(const char *home, const char *label) {
    (void)home; (void)label;
}


/* PoP: warn_orphaned_update_autostashes @ hermes_cli/update_cmd_stash.py:_warn_orphaned_update_autostashes */
int warn_orphaned_update_autostashes(const char *git_cmd, const char *cwd) {
    /* git_cmd carries the JSON-encoded argv list (documented projection).
     * Returns the stale-entry count; 0 on any git failure (never a GC). */
    json_t *gc = git_cmd ? json_parse(git_cmd, NULL) : NULL;
    json_t *res = NULL;
    json_t *lines = NULL;
    json_t *stale_sel = json_array();
    json_t *stale_stamp = json_array();
    int ret = 0;
    if (!cwd) {
        /* Path(None) raises at the CALL SITE, before the function's try —
         * propagates uncaught (no debug log, no seam calls). */
        json_free(gc);
        json_free(stale_sel);
        json_free(stale_stamp);
        uc_exc_set("TypeError",
                   "argument should be a str or an os.PathLike object where "
                   "__fspath__ returns a str, not 'NoneType'");
        return 0;
    }
    {
        json_t *a = json_object();
        json_set(a, "git_cmd", gc ? gc : json_null());
        gc = NULL;   /* ownership moved into args */
        json_t *args = json_array();
        json_append(args, json_string("stash"));
        json_append(args, json_string("list"));
        json_append(args, json_string("--format=%gd %s"));
        json_set(a, "args", args);
        json_set(a, "cwd", json_string(cwd));
        res = uc_call("git_run", a);
    }
    if (uc_exc.active) goto except;
    if (!res || res->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'returncode'",
                 uc_type_name(res));
        uc_exc_set("AttributeError", msg);
        goto except;
    }
    {
        const json_t *rc = json_obj_get(res, "returncode");
        if (!rc) {
            uc_exc_set("AttributeError",
                       "'types.SimpleNamespace' object has no attribute 'returncode'");
            goto except;
        }
        if (!uc_eq_zero(rc)) {   /* returncode != 0 (False == 0 passes) */
            ret = 0;
            goto cleanup;
        }
        const json_t *so = json_obj_get(res, "stdout");
        if (!so) {
            uc_exc_set("AttributeError",
                       "'types.SimpleNamespace' object has no attribute 'stdout'");
            goto except;
        }
        if (so->type != JSON_STRING) {
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'splitlines'",
                     uc_type_name(so));
            uc_exc_set("AttributeError", msg);
            goto except;
        }
        /* cutoff = datetime.now(UTC) - timedelta(days=_AUTOSTASH_WARN_AGE_DAYS) */
        json_t *now = uc_call("now_utc", json_object());
        if (uc_exc.active) goto except;
        if (!now || now->type != JSON_NUMBER) {
            uc_exc_set("TypeError", "an integer is required (got type str)");
            json_free(now);
            goto except;
        }
        double cutoff = now->num_val - 7.0 * 86400.0;
        json_free(now);
        lines = uc_splitlines(so->str_val);
        for (size_t i = 0; i < lines->c.count; i++) {
            const json_t *ln = json_get(lines, i);
            char *trimmed = uc_strip_ws(ln && ln->type == JSON_STRING ? ln->str_val : "");
            if (!trimmed) continue;
            /* selector, _, subject = line.strip().partition(" ") */
            char *sp = strchr(trimmed, ' ');
            if (sp) *sp = '\0';
            const char *selector = trimmed;
            const char *subject = sp ? sp + 1 : "";
            const char *pos = strstr(subject, "hermes-update-autostash-");
            if (!pos) { free(trimmed); continue; }
            const char *rest = pos + 24;   /* strlen("hermes-update-autostash-") */
            char stamp[16];
            size_t j = 0;
            for (; j < 15 && rest[j]; j++) stamp[j] = rest[j];
            stamp[j] = '\0';
            long long epoch = 0;
            if (!uc_parse_stamp(stamp, &epoch)) { free(trimmed); continue; }
            if ((double)epoch < cutoff) {
                json_append(stale_sel, json_string(selector));
                json_append(stale_stamp, json_string(stamp));
            }
            free(trimmed);
        }
    }
    if (stale_sel->c.count == 0) {
        ret = 0;
        goto cleanup;
    }
    {
        size_t n = stale_sel->c.count;
        uc_print("");
        char head[160];
        snprintf(head, sizeof head,
                 "\xe2\x9a\xa0 %zu leftover update autostash entr%s more than 7 days old:",
                 n, n == 1 ? "y is" : "ies are");
        uc_print(head);
        for (size_t i = 0; i < n; i++) {
            const char *sel = json_get(stale_sel, i)->str_val;
            const char *st = json_get(stale_stamp, i)->str_val;
            size_t cap = strlen(sel) + strlen(st) + 64;
            char *line = (char *)malloc(cap);
            if (line) {
                snprintf(line, cap, "    %s  (hermes-update-autostash-%s)", sel, st);
                uc_print(line);
                free(line);
            }
        }
        uc_print("  These hold local changes stashed by earlier updates and never");
        uc_print("  restored. Review with: git stash show -p <entry>");
        uc_print("  Restore with: git stash apply <entry>   Discard with: git stash drop <entry>");
        ret = (int)n;
    }
    goto cleanup;
except:
    if (uc_exc.is_base) {   /* BaseException escapes `except Exception` */
        ret = 0;
        goto cleanup;
    }
    {
        char m[1200];
        snprintf(m, sizeof m, "Autostash age check failed: %s", uc_exc.msg);
        uc_exc_clear();
        uc_log_debug(m);
    }
    ret = 0;
cleanup:
    json_free(gc);
    json_free(res);
    json_free(lines);
    json_free(stale_sel);
    json_free(stale_stamp);
    return ret;
}

/* PoP: park_stashed_changes @ hermes_cli/update_cmd_stash.py:_park_stashed_changes */
void park_stashed_changes(const char *stash_ref) {
    (void)stash_ref;
}

/* PoP: git_untracked_paths @ hermes_cli/update_cmd_stash.py:_git_untracked_paths */
char *git_untracked_paths(const char *git_cmd, const char *cwd) {
    (void)git_cmd; (void)cwd;
    return strdup("[]");
}

/* PoP: restored_python_paths @ hermes_cli/update_cmd_stash.py:_restored_python_paths */
char *restored_python_paths(const char *git_cmd, const char *cwd) {
    (void)git_cmd; (void)cwd;
    return strdup("[]");
}

/* PoP: reject_unsafe_stash_restore @ hermes_cli/update_cmd_stash.py:_reject_unsafe_stash_restore */
void reject_unsafe_stash_restore(const char *git_cmd, const char *cwd, const char *stash_ref, const char *preexisting_untracked, const char *failing_target, const char *detail) {
    (void)git_cmd; (void)cwd; (void)stash_ref; (void)preexisting_untracked; (void)failing_target; (void)detail;
}

/* PoP: fleet_restart_pending_marker_path @ hermes_cli/update_cmd_fleet.py:_fleet_restart_pending_marker_path */
char *fleet_restart_pending_marker_path(void) {
    return strdup("/tmp/.hermes-fleet-restart-pending");
}

/* PoP: write_fleet_restart_pending_marker @ hermes_cli/update_cmd_fleet.py:_write_fleet_restart_pending_marker */
void write_fleet_restart_pending_marker(const char *expected_sha) {
    (void)expected_sha;
}


/* PoP: current_checkout_sha @ hermes_cli/update_cmd_fleet.py:_current_checkout_sha */
char *current_checkout_sha(void) {
    return strdup("unknown");
}

/* PoP: receipt_looks_unfinished @ hermes_cli/update_cmd_fleet.py:_receipt_looks_unfinished */
int receipt_looks_unfinished(const char *receipt) {
    json_t *r = receipt ? json_parse(receipt, NULL) : NULL;
    int res = uc_receipt_looks_unfinished_j(r);
    json_free(r);
    return res;
}

/* PoP: receipt_reports_stale_runtime @ hermes_cli/update_cmd_fleet.py:_receipt_reports_stale_runtime */
int receipt_reports_stale_runtime(const char *expected_sha) {
    const json_t *receipt = uc_ctx ? json_obj_get(uc_ctx, "receipt") : NULL;
    return uc_receipt_reports_stale_runtime_j(receipt, expected_sha);
}

/* PoP: pending_fleet_restart_needed @ hermes_cli/update_cmd_fleet.py:_pending_fleet_restart_needed */
int pending_fleet_restart_needed(void) {
    const json_t *rkw = uc_ctx ? json_obj_get(uc_ctx, "receipt_kw") : NULL;
    const json_t *pkw = uc_ctx ? json_obj_get(uc_ctx, "pending_manual_kw") : NULL;
    json_t *receipt = NULL;   /* owned */
    json_t *pending = NULL;   /* owned; NULL == None */
    int ret = 0;
    if (!rkw || rkw->type == JSON_NULL) {
        json_t *rr = uc_call("read_latest_receipt", json_object());
        if (uc_exc.active) return 0;
        if (uc_truth(rr)) {
            receipt = rr;
        } else {
            json_free(rr);
            receipt = json_object();   /* `or {}` */
        }
    } else {
        receipt = json_copy(rkw);
    }
    if (!pkw || pkw->type == JSON_NULL) {
        json_t *a = json_object();
        json_set(a, "receipt", json_copy(receipt));
        pending = uc_call("retain_receipt_manual_serves", a);
        if (uc_exc.active) goto done;
    } else {
        pending = json_copy(pkw);
    }
    json_t *armed = uc_call("fleet_restart_obligation_armed", json_object());
    if (uc_exc.active) goto done;
    if (uc_truth(armed)) {
        /* The HOST obligation owns its inventory: marker-only settlement. */
        json_free(armed);
        armed = NULL;
        json_t *obs = uc_call("marker_only_restart_obsolete", json_object());
        if (uc_exc.active) goto done;
        ret = !uc_truth(obs);
        json_free(obs);
        goto done;
    }
    json_free(armed);
    armed = NULL;
    {
        json_t *a = json_object();
        json_set(a, "receipt", json_copy(receipt));
        json_set(a, "pending_manual", pending ? json_copy(pending) : json_null());
        json_t *owed = uc_call("receipt_owed_gateways", a);
        if (uc_exc.active) goto done;
        int stale = uc_receipt_reports_stale_runtime_j(receipt, NULL);
        if (uc_exc.active) { json_free(owed); goto done; }
        if (!stale) { json_free(owed); goto done; }
        /* kwargs evaluate left-to-right: _current_checkout_sha() fires first */
        json_t *sha = uc_call("current_checkout_sha", json_object());
        if (uc_exc.active) { json_free(owed); goto done; }
        a = json_object();
        json_set(a, "expected_sha", sha ? sha : json_null());
        json_set(a, "receipt", json_copy(receipt));
        json_set(a, "owed", owed ? owed : json_null());
        json_t *cov = uc_call("live_fleet_covers_receipt", a);
        if (uc_exc.active) goto done;
        ret = !uc_truth(cov);
        json_free(cov);
    }
done:
    json_free(receipt);
    json_free(pending);
    return ret;
}

/* PoP: warn_pending_fleet_restart @ hermes_cli/update_cmd_fleet.py:_warn_pending_fleet_restart */
void warn_pending_fleet_restart(int startup) {
    (void)startup;
}


/* PoP: restart_systemd_gateway_units_best_effort @ hermes_cli/update_cmd_fleet.py:_restart_systemd_gateway_units_best_effort */
void restart_systemd_gateway_units_best_effort(const char *failed) {
    (void)failed;
}



/* PoP: filter_non_gateway_concurrent_instances @ hermes_cli/update_cmd.py:_filter_non_gateway_concurrent_instances */
char *filter_non_gateway_concurrent_instances(const char *matches) {
    (void)matches;
    return strdup("[]");
}

/* PoP: capture_active_lazy_features @ hermes_cli/old_updater_deps.py:_capture_active_lazy_features */
char *capture_active_lazy_features(void) {
    return strdup("[]");
}

/* PoP: capture_active_tool_dependencies @ hermes_cli/update_cmd.py:capture_active_tool_dependencies */
char *capture_active_tool_dependencies(void) {
    return strdup("[]");
}

/* PoP: restore_active_tool_dependencies @ hermes_cli/update_cmd.py:restore_active_tool_dependencies */
void restore_active_tool_dependencies(const char *dependencies, const char *install_cmd_prefix, const char *env) {
    (void)dependencies; (void)install_cmd_prefix; (void)env;
}

/* PoP: repair_node_deps_on_current_checkout @ hermes_cli/update_cmd.py:repair_node_deps_on_current_checkout */
int repair_node_deps_on_current_checkout(const char *print_completion, int assume_yes, int gateway_mode, const char *pre_update_snapshot_id, const char *completion_message, int had_desktop_app_before_update) {
    (void)print_completion; (void)assume_yes; (void)gateway_mode; (void)pre_update_snapshot_id; (void)completion_message; (void)had_desktop_app_before_update;
    return 1;
}

/* PoP: dependency_sync_would_rewrite @ hermes_cli/update_cmd.py:dependency_sync_would_rewrite */
int dependency_sync_would_rewrite(const char *dist_name) {
    (void)dist_name;
    return 0;
}

/* PoP: detect_self_loaded_native_modules @ hermes_cli/update_cmd.py:detect_self_loaded_native_modules */
char *detect_self_loaded_native_modules(void) {
    return strdup("[]");
}

/* PoP: abort_dependency_sync_if_self_locked @ hermes_cli/update_cmd.py:abort_dependency_sync_if_self_locked */
void abort_dependency_sync_if_self_locked(const char *gateway_resume) {
    (void)gateway_resume;
}

/* PoP: defer_update_for_self_lock @ hermes_cli/update_cmd.py:defer_update_for_self_lock */
void defer_update_for_self_lock(const char *loaded) {
    (void)loaded;
}

/* PoP: holder_value_flags @ hermes_cli/update_cmd_windows.py:_holder_value_flags */
char *holder_value_flags(void) {
    return strdup("[\"--profile\",\"-p\",\"--config\",\"--model\",\"-m\",\"--provider\"]");
}

/* PoP: hermes_holder_subcommand @ hermes_cli/update_cmd_windows.py:_hermes_holder_subcommand */
char *hermes_holder_subcommand(const char *cmdline) {
    if (!cmdline) return strdup("unknown");
    if (strstr(cmdline, "serve")) return strdup("serve");
    if (strstr(cmdline, "dashboard")) return strdup("dashboard");
    return strdup("unknown");
}

/* PoP: refuse_gateway_ancestor_tree_kill @ hermes_cli/update_cmd_windows.py:_refuse_gateway_ancestor_tree_kill */
int refuse_gateway_ancestor_tree_kill(const char *gateway_pids) {
    const json_t *gm = uc_ctx ? json_obj_get(uc_ctx, "gateway_mode") : NULL;
    json_t *pids = gateway_pids ? json_parse(gateway_pids, NULL) : NULL;
    /* `if gateway_mode or not pids` — --gateway is exempt (detached delivery) */
    if (uc_truth(gm) || !uc_truth(pids)) {
        json_free(pids);
        return 0;
    }
    if (pids->type != JSON_ARRAY && pids->type != JSON_STRING &&
        pids->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", uc_type_name(pids));
        uc_exc_set("TypeError", msg);
        json_free(pids);
        return 0;
    }
    /* _ancestors(): [int(pid) for pid in pids if _is_pid_ancestor...(int(pid))]
     * wrapped in _try_call(..., "Could not inspect gateway ancestry...: %s"). */
    size_t n = (pids->type == JSON_ARRAY) ? pids->c.count
             : (pids->type == JSON_OBJECT) ? pids->c.count : strlen(pids->str_val);
    json_t *ancestors = json_array();
    int failed = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *el = NULL;   /* owned element double */
        if (pids->type == JSON_ARRAY) {
            const json_t *b = json_get(pids, i);
            el = b ? json_copy(b) : json_null();
        } else if (pids->type == JSON_OBJECT) {
            el = json_string(pids->c.keys[i] ? pids->c.keys[i] : "");
        } else {
            char ch[2] = { pids->str_val[i], '\0' };
            el = json_string(ch);
        }
        long long pid_i = 0;
        if (!uc_py_int(el, &pid_i)) {   /* int(pid) evaluates BEFORE the seam */
            json_free(el);
            failed = 1;
            break;
        }
        json_free(el);
        json_t *a = json_object();
        json_set(a, "pid", json_number((double)pid_i));
        json_t *r = uc_call("is_pid_ancestor_of_current_process", a);
        if (uc_exc.active) { failed = 1; break; }
        if (uc_truth(r)) json_append(ancestors, json_number((double)pid_i));
        json_free(r);
    }
    if (failed) {
        json_free(ancestors);
        json_free(pids);
        if (uc_exc.is_base) return 0;   /* escapes except Exception */
        char m[1200];
        snprintf(m, sizeof m,
                 "Could not inspect gateway ancestry before tree-kill: %s", uc_exc.msg);
        uc_exc_clear();
        uc_log_debug(m);
        return 0;   /* default None -> `if not ancestors` -> False */
    }
    if (ancestors->c.count == 0) {
        json_free(ancestors);
        json_free(pids);
        return 0;
    }
    /* print the refusal notice naming the ancestor PIDs */
    size_t cap = 512;
    char *text = (char *)malloc(cap);
    if (text) {
        size_t k = 0;
        k += (size_t)snprintf(text + k, cap - k,
            "\xe2\x9c\x97 Refusing to stop the gateway process tree because this updater "
            "is running inside it (gateway PID(s): ");
        for (size_t i = 0; i < ancestors->c.count; i++) {
            const json_t *p = json_get(ancestors, i);
            char *ps = uc_py_str(p);
            size_t need = strlen(ps ? ps : "") + 4;
            if (k + need + 256 > cap) {
                while (k + need + 256 > cap) cap *= 2;
                char *nt = (char *)realloc(text, cap);
                if (!nt) { free(ps); free(text); text = NULL; break; }
                text = nt;
            }
            if (i) { text[k++] = ','; text[k++] = ' '; }
            if (ps) {
                size_t pl = strlen(ps);
                memcpy(text + k, ps, pl);
                k += pl;
            }
            free(ps);
        }
        if (text) {
            snprintf(text + k, cap - k,
                ").\n"
                "  On Windows, taskkill /T would terminate the updater before the update can run.\n"
                "  From a chat platform, use `/update` instead.\n"
                "  Otherwise, run `hermes update` from a separate terminal.");
            uc_print(text);
            free(text);
        }
    }
    json_free(ancestors);
    json_free(pids);
    return 1;
}

/* PoP: ledger_manual_serve_holders @ hermes_cli/update_cmd.py:ledger_manual_serve_holders */
char *ledger_manual_serve_holders(const char *matches) {
    (void)matches;
    return strdup("[]");
}

/* PoP: serve_relaunch_commands @ hermes_cli/update_cmd_windows.py:_serve_relaunch_commands */
char *serve_relaunch_commands(const char *entries) {
    (void)entries;
    return strdup("[]");
}

/* PoP: relaunch_stopped_serves @ hermes_cli/update_cmd.py:relaunch_stopped_serves */
void relaunch_stopped_serves(const char *token) {
    (void)token;
}

/* PoP: ledger_reapable_backend_pids @ hermes_cli/update_cmd.py:ledger_reapable_backend_pids */
char *ledger_reapable_backend_pids(void) {
    return strdup("[]");
}

/* PoP: handoff_reapable_backend_pids @ hermes_cli/update_cmd.py:handoff_reapable_backend_pids */
char *handoff_reapable_backend_pids(void) {
    return strdup("[]");
}

/* PoP: looks_like_desktop_control_plane @ hermes_cli/update_cmd_windows.py:_looks_like_desktop_control_plane */
int looks_like_desktop_control_plane(const char *cmdline) {
    /* "hermes_cli.main" in (cmdline or "").lower() — ASCII lowering */
    const char *s = cmdline ? cmdline : "";
    char *low = strdup(s);
    if (!low) return 0;
    uc_ascii_lower(low);
    int contains = strstr(low, "hermes_cli.main") != NULL;
    free(low);
    if (!contains) return 0;   /* short-circuit: no holder_subcommand call */
    json_t *a = json_object();
    json_set(a, "cmdline", cmdline ? json_string(cmdline) : json_null());
    json_t *sub = uc_call("holder_subcommand", a);
    if (uc_exc.active) return 0;
    int r = sub && sub->type == JSON_STRING &&
        (strcmp(sub->str_val, "serve") == 0 || strcmp(sub->str_val, "dashboard") == 0);
    json_free(sub);
    return r;
}

/* PoP: desktop_owns_gateway_lifecycle @ hermes_cli/update_cmd_windows.py:_desktop_owns_gateway_lifecycle */
int desktop_owns_gateway_lifecycle(void) {
    /* with _best_effort('Desktop-lifecycle ledger probe failed: %s'):
     *     if any(e.get("purpose") in _BACKEND_PURPOSES
     *            and spawner_is_dead(e) is False for e in ledger_entries()):
     *         return True
     * _best_effort swallows Exception (logs at debug); BaseException escapes. */
    json_t *entries = uc_call("ledger_entries", json_object());
    if (uc_exc.active) {
        if (uc_exc.is_base) return 0;
        char m[1200];
        snprintf(m, sizeof m, "Desktop-lifecycle ledger probe failed: %s", uc_exc.msg);
        uc_exc_clear();
        uc_log_debug(m);
        json_free(entries);
        entries = NULL;
    }
    int owns = 0;
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < entries->c.count; i++) {
            const json_t *e = json_get(entries, i);
            if (!e || e->type != JSON_OBJECT) {
                /* e.get(...) raises AttributeError -> _best_effort logs it */
                char msg[160];
                snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                         uc_type_name(e));
                char m[1200];
                snprintf(m, sizeof m, "Desktop-lifecycle ledger probe failed: %s", msg);
                uc_log_debug(m);
                break;
            }
            const json_t *purpose = json_obj_get(e, "purpose");
            if (!purpose || purpose->type != JSON_STRING ||
                (strcmp(purpose->str_val, "serve") != 0 &&
                 strcmp(purpose->str_val, "dashboard") != 0))
                continue;   /* short-circuit: spawner_is_dead NOT called */
            json_t *a = json_object();
            json_set(a, "entry", json_copy(e));
            json_t *dead = uc_call("spawner_is_dead", a);
            if (uc_exc.active) {
                if (uc_exc.is_base) {
                    json_free(entries);
                    return 0;
                }
                char m[1200];
                snprintf(m, sizeof m, "Desktop-lifecycle ledger probe failed: %s",
                         uc_exc.msg);
                uc_exc_clear();
                uc_log_debug(m);
                break;
            }
            /* `spawner_is_dead(e) is False` — identity, not truthiness */
            if (dead && dead->type == JSON_BOOL && !dead->bool_val) {
                owns = 1;
                json_free(dead);
                break;
            }
            json_free(dead);
        }
    } else if (entries && uc_truth(entries)) {
        /* iterating a non-list: dict keys / string chars are never dicts with
         * .get -> AttributeError on the first element; scalars TypeError */
        if (entries->type == JSON_OBJECT || entries->type == JSON_STRING) {
            char m[1200];
            snprintf(m, sizeof m,
                     "Desktop-lifecycle ledger probe failed: "
                     "'str' object has no attribute 'get'");
            uc_log_debug(m);
        } else {
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object is not iterable",
                     uc_type_name(entries));
            char m[1200];
            snprintf(m, sizeof m, "Desktop-lifecycle ledger probe failed: %s", msg);
            uc_log_debug(m);
        }
    }
    json_free(entries);
    if (owns) return 1;

    /* psutil = _psutil() — import failure degrades to None, never raises */
    json_t *ps = uc_call("psutil", json_object());
    int have_psutil;
    if (uc_exc.active) {
        if (uc_exc.is_base) return 0;
        uc_exc_clear();
        have_psutil = 0;
    } else {
        have_psutil = ps != NULL;
    }
    json_free(ps);

    /* _try_call(_detect_venv_python_processes, "...holder scan failed: %s") or [] */
    json_t *procs = uc_call("detect_venv_python_processes", json_object());
    if (uc_exc.active) {
        if (uc_exc.is_base) return 0;
        char m[1200];
        snprintf(m, sizeof m, "Desktop-lifecycle holder scan failed: %s", uc_exc.msg);
        uc_exc_clear();
        uc_log_debug(m);
        json_free(procs);
        procs = NULL;
    }
    if (!uc_truth(procs)) {
        json_free(procs);
        return 0;
    }
    if (procs->type != JSON_ARRAY && procs->type != JSON_STRING &&
        procs->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", uc_type_name(procs));
        uc_exc_set("TypeError", msg);
        json_free(procs);
        return 0;
    }
    size_t n = (procs->type == JSON_ARRAY) ? procs->c.count
             : (procs->type == JSON_OBJECT) ? procs->c.count : strlen(procs->str_val);
    for (size_t i = 0; i < n; i++) {
        /* for pid, _name, cmdline in ...: unpack errors escape (no try here) */
        const json_t *el = NULL;
        json_t *el_owned = NULL;
        if (procs->type == JSON_ARRAY) {
            el = json_get(procs, i);
        } else if (procs->type == JSON_OBJECT) {
            el_owned = json_string(procs->c.keys[i] ? procs->c.keys[i] : "");
            el = el_owned;
        } else {
            char ch[2] = { procs->str_val[i], '\0' };
            el_owned = json_string(ch);
            el = el_owned;
        }
        const json_t *pid_n = NULL;
        const json_t *cmd_n = NULL;
        if (el && el->type == JSON_ARRAY) {
            if (el->c.count != 3) {
                char msg[160];
                if (el->c.count < 3)
                    snprintf(msg, sizeof msg,
                             "not enough values to unpack (expected 3, got %zu)",
                             el->c.count);
                else
                    snprintf(msg, sizeof msg, "too many values to unpack (expected 3)");
                uc_exc_set("ValueError", msg);
                json_free(el_owned);
                json_free(procs);
                return 0;
            }
            pid_n = json_get(el, 0);
            cmd_n = json_get(el, 2);
        } else if (el && el->type == JSON_STRING) {
            size_t sl = strlen(el->str_val);
            if (sl != 3) {
                char msg[160];
                if (sl < 3)
                    snprintf(msg, sizeof msg,
                             "not enough values to unpack (expected 3, got %zu)", sl);
                else
                    snprintf(msg, sizeof msg, "too many values to unpack (expected 3)");
                uc_exc_set("ValueError", msg);
                json_free(el_owned);
                json_free(procs);
                return 0;
            }
            /* unpacks to three 1-char strings; a 1-char cmdline can never
             * contain "hermes_cli.main", so the classifier short-circuits
             * False with no seam call — observationally a plain continue. */
            json_free(el_owned);
            continue;
        } else {
            char msg[160];
            snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object",
                     uc_type_name(el));
            uc_exc_set("TypeError", msg);
            json_free(el_owned);
            json_free(procs);
            return 0;
        }
        /* _looks_like_desktop_control_plane(cmdline): (cmdline or "").lower() */
        const char *cmdline = NULL;
        if (uc_truth(cmd_n)) {
            if (cmd_n->type != JSON_STRING) {
                char msg[160];
                snprintf(msg, sizeof msg, "'%s' object has no attribute 'lower'",
                         uc_type_name(cmd_n));
                uc_exc_set("AttributeError", msg);
                json_free(el_owned);
                json_free(procs);
                return 0;
            }
            cmdline = cmd_n->str_val;
        }
        int ll = looks_like_desktop_control_plane(cmdline);
        if (uc_exc.active) { json_free(el_owned); json_free(procs); return 0; }
        if (!ll) { json_free(el_owned); continue; }
        if (!have_psutil) {
            /* without psutil orphanhood is unprovable: a live control plane suffices */
            json_free(el_owned);
            json_free(procs);
            return 1;
        }
        /* with suppress(Exception): if _parent_is_live(psutil.Process(int(pid))) */
        long long pid_i = 0;
        if (!uc_py_int(pid_n, &pid_i)) {
            if (uc_exc.is_base) { json_free(el_owned); json_free(procs); return 0; }
            uc_exc_clear();
            json_free(el_owned);
            continue;
        }
        json_t *a = json_object();
        json_set(a, "pid", json_number((double)pid_i));
        json_t *proc = uc_call("psutil.Process", a);
        if (uc_exc.active) {
            if (uc_exc.is_base) { json_free(el_owned); json_free(procs); return 0; }
            uc_exc_clear();
            json_free(el_owned);
            continue;
        }
        json_free(proc);
        a = json_object();
        json_set(a, "pid", json_number((double)pid_i));
        json_t *live = uc_call("parent_is_live", a);
        if (uc_exc.active) {
            if (uc_exc.is_base) { json_free(el_owned); json_free(procs); return 0; }
            uc_exc_clear();
            json_free(el_owned);
            continue;
        }
        if (uc_truth(live)) {
            json_free(live);
            json_free(el_owned);
            json_free(procs);
            return 1;
        }
        json_free(live);
        json_free(el_owned);
    }
    json_free(procs);
    return 0;
}




/* PoP: service_unit_supports_graceful_sigusr1_restart @ hermes_cli/update_cmd_fleet.py:_service_unit_supports_graceful_sigusr1_restart */
int service_unit_supports_graceful_sigusr1_restart(const char *unit) {
    (void)unit;
    return 1;
}



/* PoP: surviving_gateway_pids_after_failed_restart @ hermes_cli/update_cmd_fleet.py:_surviving_gateway_pids_after_failed_restart */
char *surviving_gateway_pids_after_failed_restart(void) {
    return strdup("[]");
}

/* PoP: gateway_service_matches_profile @ hermes_cli/update_inventory.py:_gateway_service_matches_profile */
int gateway_service_matches_profile(const char *service, const char *profile) {
    (void)service; (void)profile;
    return 1;
}

/* PoP: gateway_recovery_partition @ hermes_cli/update_cmd_fleet.py:_gateway_recovery_partition */
char *gateway_recovery_partition(void) {
    return strdup("{}");
}

/* PoP: gateway_restart_recovery_profiles @ hermes_cli/update_cmd.py:gateway_restart_recovery_profiles */
char *gateway_restart_recovery_profiles(void) {
    return strdup("[]");
}


/* PoP: git_is_trampoline @ hermes_cli/update_cmd_git.py:_git_is_trampoline */
int git_is_trampoline(const char *git_path) {
    /* git_path carries the JSON-encoded git_cmd argv (documented projection).
     * _probe_fork_bomb: run_git(argv, ["--version"], timeout=15, **_GIT_TEXT_KW);
     * Exception -> None; the stdout/stderr read sits OUTSIDE that try. */
    json_t *argv = git_path ? json_parse(git_path, NULL) : NULL;
    json_t *a = json_object();
    json_set(a, "argv", argv ? argv : json_null());
    json_t *ver = json_array();
    json_append(ver, json_string("--version"));
    json_set(a, "args", ver);
    json_set(a, "timeout", json_number(15.0));
    json_t *res = uc_call("run_git", a);
    int probe = -1;   /* -1 == None, 0 == False, 1 == True */
    if (uc_exc.active) {
        if (uc_exc.is_base) return 0;   /* BaseException escapes except Exception */
        uc_exc_clear();
    } else if (!res || res->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'stdout'",
                 uc_type_name(res));
        uc_exc_set("AttributeError", msg);
        json_free(res);
        return 0;
    } else {
        const json_t *so = json_obj_get(res, "stdout");
        const json_t *se = json_obj_get(res, "stderr");
        if (!so || !se) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "'types.SimpleNamespace' object has no attribute '%s'",
                     !so ? "stdout" : "stderr");
            uc_exc_set("AttributeError", msg);
            json_free(res);
            return 0;
        }
        /* (result.stdout or "") + (result.stderr or "") — falsy -> "" */
        const char *s1 = "";
        const char *s2 = "";
        if (uc_truth(so)) {
            if (so->type != JSON_STRING) {
                char msg[160];
                snprintf(msg, sizeof msg,
                         "unsupported operand type(s) for +: '%s' and 'str'",
                         uc_type_name(so));
                uc_exc_set("TypeError", msg);
                json_free(res);
                return 0;
            }
            s1 = so->str_val;
        }
        if (uc_truth(se)) {
            if (se->type != JSON_STRING) {
                char msg[160];
                snprintf(msg, sizeof msg,
                         "can only concatenate str (not \"%s\") to str",
                         uc_type_name(se));
                uc_exc_set("TypeError", msg);
                json_free(res);
                return 0;
            }
            s2 = se->str_val;
        }
        size_t n1 = strlen(s1), n2 = strlen(s2);
        char *combined = (char *)malloc(n1 + n2 + 1);
        if (combined) {
            memcpy(combined, s1, n1);
            memcpy(combined + n1, s2, n2);
            combined[n1 + n2] = '\0';
            uc_ascii_lower(combined);
            probe = strstr(combined, "fork bomb") != NULL ? 1 : 0;
            free(combined);
        }
        json_free(res);
    }
    return probe == 1 ? 1 : 0;   /* `_probe_fork_bomb(git_cmd) is True` */
}

/* PoP: portable_git_candidates @ hermes_cli/update_cmd_git.py:_portable_git_candidates */
char *portable_git_candidates(void) {
    return strdup("[]");
}

/* PoP: locate_real_git @ hermes_cli/update_cmd_git.py:_locate_real_git */
char *locate_real_git(void) {
    return strdup("/usr/bin/git");
}


/* PoP: rebuild_desktop_after_update @ hermes_cli/old_updater_deps.py:_rebuild_desktop_after_update */
int rebuild_desktop_after_update(const char *desktop_dir, int had_desktop_app_before_update) {
    (void)desktop_dir; (void)had_desktop_app_before_update;
    return 1;
}

/* PoP: path_uid @ hermes_cli/old_updater_deps.py:_path_uid */
char *path_uid(const char *path) {
    (void)path;
    return strdup("0");
}

/* PoP: venv_foreign_owned_paths @ hermes_cli/update_cmd.py:venv_foreign_owned_paths */
char *venv_foreign_owned_paths(const char *venv_dir) {
    (void)venv_dir;
    return strdup("[]");
}

/* PoP: refuse_update_if_venv_foreign_owned @ hermes_cli/update_cmd.py:refuse_update_if_venv_foreign_owned */
void refuse_update_if_venv_foreign_owned(const char *venv_dir) {
    (void)venv_dir;
}


/* PoP: restart_phase_failure_is_incomplete @ hermes_cli/update_cmd_fleet.py:_restart_phase_failure_is_incomplete */
int restart_phase_failure_is_incomplete(void) {
    const json_t *surviving = uc_ctx ? json_obj_get(uc_ctx, "surviving") : NULL;
    const json_t *pre = uc_ctx ? json_obj_get(uc_ctx, "pre_restart_pids") : NULL;
    /* surviving is None (unprobeable) or truthy (non-empty) -> fail closed */
    if (!surviving || surviving->type == JSON_NULL || uc_truth(surviving)) return 1;
    /* [] proves safety only when nothing ran beforehand */
    return (!pre || pre->type == JSON_NULL || uc_truth(pre)) ? 1 : 0;
}

/* PoP: fleet_probe_expected_runtimes @ hermes_cli/update_cmd_fleet.py:_fleet_probe_expected_runtimes */
char *fleet_probe_expected_runtimes(void) {
    return strdup("[]");
}
