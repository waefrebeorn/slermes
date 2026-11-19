#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/cron.py: 10 missing functions ── */

/* Deferred (deep live-probe flow — untagged stub, rest_revive2). */
int builtin_gateway_liveness(void) { return 1; }

/* ── rest_revive2 shared revival infrastructure ─────────────────────────────
 * Faithful C11 ports of the cronjob_tools / job_args / hermes_cli.cron gap
 * stubs. Each port keeps its PoP tag and takes (io, in): io is the seam
 * channel (installed per call by tests/t_port_rest_revive2.c), in is the
 * borrowed argument object. Seam results are NEW json_t (caller frees);
 * NULL == None; {"__raise__": cls, "message": msg} == a raised exception.
 * BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 * CancelledError) pass through every `except Exception` site.
 * The shared helpers below are duplicated verbatim in both gap files under
 * ONE guard so the merged harness TU compiles and each standalone .o keeps
 * its own copy. Prefix r2_ = rest_revive2 shared; cj_ = cronjob_tools_gaps;
 * hc_ = cron_gaps.
 *
 * Documented fixture scope (both sides):
 * - str()/repr() of containers uses Python repr syntax; exotic non-printable
 *   Unicode (Cf/Zs beyond NBSP/NEL) inside repr'd strings is out of scope.
 * - .lower() is ASCII (fixtures ASCII); .strip() is full Unicode White_Space.
 * - Path strings are POSIX-form; the oracle's Path doubles normalize str()
 *   to as_posix() so Windows backslash flips never reach the comparison.
 *   is_absolute()/sort/relative_to follow the HOST pathlib semantics
 *   (#ifdef _WIN32), transcribed from CPython 3.14 on the gate host.
 * - In-function imports are always resolvable (stub modules); a scripted
 *   raise on the imported callable models a CALL failure. Import-failure
 *   paths are not exercised (documented, fail-closed scripts).
 * - int() string coercion covers base-10 sign/underscore/whitespace forms;
 *   magnitudes beyond int64 are out of fixture scope.
 */
#ifndef R2_REVIVE2_SHARED
#define R2_REVIVE2_SHARED

#include <math.h>
#include <inttypes.h>

/* Byte-exact string sites captured from the live quarry defs
 * (probe_r2_literals.py verified every template against live output). */
#define R2_L_ALL_NOTE "deliver='all' resolves at fire time and never includes bot-chat targets \xe2\x80\x94 channels connected later are picked up automatically."
#define R2_L_ALREADY_RUNNING "Job is already running (a scheduler tick or another manual run is executing it); not started again."
#define R2_L_BOT_NOTE "Targeting another profile's Bot Chat costs that bot an agent turn per run."
#define R2_L_CLAIM_MISSING "Job no longer exists; nothing to run."
#define R2_L_CLAIM_PAUSED "Job is paused/disabled; resume it before running."
#define R2_L_CLAIM_SCHED "Job is already being fired by the scheduler; not run again."
#define R2_L_DD_LATE_DEFAULT "late"
#define R2_L_DD_LATE_PRE "("
#define R2_L_DD_LATE_SUF " late)"
#define R2_L_DD_ONTIME_PRE "on time (scheduled "
#define R2_L_DD_ONTIME_SUF ")"
#define R2_L_DD_RAN_MID ", ran "
#define R2_L_DD_RAN_SUF " "
#define R2_L_DD_SCHED_PRE "scheduled "
#define R2_L_DD_WARN_PRE "\xe2\x9a\xa0 "
#define R2_L_DD_WARN_SUF ": "
#define R2_L_DISPATCH_CONTEXT "Triggered via cronjob(action='run'). The job executed in its own fresh cron session; this block reports its outcome."
#define R2_L_DISPATCH_GOAL_MID "' ("
#define R2_L_DISPATCH_GOAL_PRE "Manual run of cron job '"
#define R2_L_DISPATCH_GOAL_SUF ")"
#define R2_L_DOC_DELIV_MID "). "
#define R2_L_DOC_DELIV_PRE "last run finished but the result was not delivered ("
#define R2_L_DOC_FIRE_MID1 " (scheduled "
#define R2_L_DOC_FIRE_MID2 ", "
#define R2_L_DOC_FIRE_PRE "last fire was "
#define R2_L_DOC_FIRE_SUF " late). This warning clears at the next on-time fire."
#define R2_L_DOC_GRACE_NONCRON "grace_minutes has no effect on a non-cron schedule"
#define R2_L_DOC_GRACE_PRE "grace_minutes is unusable ("
#define R2_L_DOC_GRACE_SUF ")"
#define R2_L_DOC_LASTRUN_PRE "last run failed: "
#define R2_L_DOC_NO_NEXT "active job has no next_run_at"
#define R2_L_DOC_NO_SCRIPT "no-agent job has no script"
#define R2_L_DOC_TZ_PRE "timezone "
#define R2_L_DOC_TZ_SUF " is not a valid IANA zone; the job falls back to the profile timezone"
#define R2_L_DOC_UNKNOWN_ERR "unknown error"
#define R2_L_DOC_UNVER_PRE "last delivery unverified (adapter acked without evidence): "
#define R2_L_DOC_WORKDIR_PRE "workdir not found: "
#define R2_L_ELLIPSIS "\xe2\x80\xa6"
#define R2_L_EXEC_ENDED_PRE "execution ended in "
#define R2_L_EXEC_ENDED_SUF " state"
#define R2_L_EXEC_UNKNOWN "unknown"
#define R2_L_FL_QMARK "?"
#define R2_L_FL_UNITS_D "d"
#define R2_L_FL_UNITS_H "h"
#define R2_L_FL_UNITS_M "m"
#define R2_L_FL_UNITS_S "s"
#define R2_L_FL_UNITS_ZERO "0m"
#define R2_L_FL_UNITS_Q "?"
#define R2_L_FL_ZERO_M "0m"
#define R2_L_FR_ERROR "This job targets a relay-fronted platform, which has no standalone sender. Start the gateway \xe2\x80\x94 its ticker will deliver the job on schedule via the live relay adapter."
#define R2_L_FR_NOTE "This job targets a relay-fronted platform; it was dispatched to the running gateway, whose live relay adapter owns that delivery."
#define R2_L_GW_SUBJ_PLUR "these jobs are saved"
#define R2_L_GW_SUBJ_SING "this job is saved"
#define R2_L_GW_WARN_PRE "The Hermes gateway is not running \xe2\x80\x94 "
#define R2_L_GW_WARN_SUF " but will NOT fire until the gateway is started (hermes gateway install / hermes gateway start). Tell the user the task is scheduled but not active yet."
#define R2_L_HINT_ID_DEFAULT "<id>"
#define R2_L_HINT_PRE "Check the target with `hermes cron status` or change it with `hermes cron edit "
#define R2_L_HINT_SUF " --deliver <target>`."
#define R2_L_KIND_CATCHUP "catch-up after missed fire"
#define R2_L_KIND_LATE "late"
#define R2_L_LOG_ASYNC_UNAVAIL_FMT "cronjob run: async delegation registry unavailable (%s); running job '%s' inline."
#define R2_L_LOG_CLAIM_FMT "Failed to claim cron job %s for %s: %s"
#define R2_L_LOG_POOL_UNAVAIL_FMT "cronjob run: background pool unavailable (%s); running job '%s' inline."
#define R2_L_LOG_RUN_FMT "Failed to execute cron job %s immediately: %s"
#define R2_L_MF_AT_DEFAULT "?"
#define R2_L_MF_MID1 ": "
#define R2_L_MF_MID2 ". The messaging gateway was unreachable. Run `hermes gateway restart`, then `hermes cron run "
#define R2_L_MF_PRE "missed scheduled fire at "
#define R2_L_MF_SUF "` to run it now."
#define R2_L_MN_DELIVERED " (output was delivered there by the job itself)"
#define R2_L_MN_FAILED_PRE " (\xe2\x9a\xa0 delivery FAILED: "
#define R2_L_MN_FAILED_SUF ")"
#define R2_L_MN_LOCAL " (output saved locally only)"
#define R2_L_MN_QUEUED " (output queued for Bot Chat; completion unverified, do not resend)"
#define R2_L_MON_NOTE "Monitor mode: the source runs first each tick and its output is hashed as exact bytes \xe2\x80\x94 unchanged output suppresses the agent run (silent no_change tick), changed output injects a MONITOR CHANGE DETECTED diff into the prompt. The first tick always runs as baseline. The source must emit STABLE output (no timestamps, no random ordering) or every tick will look changed."
#define R2_L_NOAG_NOTE "no_agent mode: stdout is delivered verbatim; EMPTY stdout sends nothing at all (watchdog pattern \xe2\x80\x94 script should stay quiet when there is nothing to report). Non-zero exit or timeout sends an error alert. prompt/skills are ignored."
#define R2_L_NO_DETAILS "no details"
#define R2_L_NR_H_SUFFIX "h"
#define R2_L_NR_INVALID_PRE "next_run_at is not a valid timestamp: "
#define R2_L_NR_M_SUFFIX "m"
#define R2_L_NR_OVERDUE_PRE "next_run_at is "
#define R2_L_NR_OVERDUE_SUF " overdue \xe2\x80\x94 job is not firing (is the scheduler running?)"
#define R2_L_REJECTED_DEFAULT "rejected"
#define R2_L_SH_NOTFILE_PRE "script path is not a file: "
#define R2_L_SH_NOTFOUND_PRE "script not found: "
#define R2_L_SH_OUTSIDE_MID ": "
#define R2_L_SH_OUTSIDE_PRE "script resolves outside "
#define R2_L_THREAD_NOTE_PRE "deliver target '"
#define R2_L_THREAD_NOTE_SUF "' has no :thread_id segment \xe2\x80\x94 on thread/topic platforms the delivery lands in the main chat, not a topic."
#define R2_L_TRUNC_PRE "\n\xe2\x80\xa6 (truncated; full output: "
#define R2_L_TRUNC_SUF ")"


typedef struct { char *p; size_t n, cap; } r2_buf;

static void r2_binit(r2_buf *b) {
    b->cap = 64; b->n = 0;
    b->p = (char *)malloc(b->cap);
    b->p[0] = '\0';
}
static void r2_bput(r2_buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        while (b->n + n + 1 > b->cap) b->cap *= 2;
        b->p = (char *)realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}
static void r2_bputs(r2_buf *b, const char *s) { r2_bput(b, s, strlen(s)); }
static void r2_bputc(r2_buf *b, char c) { r2_bput(b, &c, 1); }

static int r2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        default:          return v->c.count > 0 ? 1 : 0;
    }
}

static const char *r2_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER:
            return (isfinite(v->num_val) && v->num_val == floor(v->num_val))
                   ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

static json_t *r2_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
}
static const char *r2_raise_kind(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}
static const char *r2_raise_msg(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return "";
    return json_get_str(r, "message", "");
}
static int r2_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

/* ── UTF-8 / Python str semantics ── */

static unsigned r2_cp(const char *s, size_t *i) {
    const unsigned char *u = (const unsigned char *)s;
    unsigned c = u[*i];
    size_t adv = 1;
    if (c >= 0xC0 && c < 0xE0) {
        adv = 2; c = ((c & 0x1F) << 6) | (u[*i + 1] & 0x3F);
    } else if (c >= 0xE0 && c < 0xF0) {
        adv = 3; c = ((c & 0x0F) << 12) | ((u[*i + 1] & 0x3F) << 6) | (u[*i + 2] & 0x3F);
    } else if (c >= 0xF0) {
        adv = 4; c = ((c & 0x07) << 18) | ((u[*i + 1] & 0x3F) << 12) |
                   ((u[*i + 2] & 0x3F) << 6) | (u[*i + 3] & 0x3F);
    }
    *i += adv;
    return c;
}

/* CPython str.strip() White_Space set (incl. 0x1C-0x1F separators). */
static int r2_is_space(unsigned c) {
    return (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F) ||
           c == 0x20 || c == 0x85 || c == 0xA0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 ||
           c == 0x202F || c == 0x205F || c == 0x3000;
}

static char *r2_strip(const char *s) {
    size_t n = strlen(s), a = 0, p = 0, endns = 0;
    while (a < n) {
        size_t st = a;
        unsigned c = r2_cp(s, &st);
        if (!r2_is_space(c)) break;
        a = st;
    }
    p = a;
    endns = a;   /* all-whitespace input: empty result (no size_t underflow) */
    while (p < n) {
        size_t st = p;
        unsigned c = r2_cp(s, &st);
        if (!r2_is_space(c)) endns = st;
        p = st;
    }
    char *out = (char *)malloc(endns - a + 1);
    memcpy(out, s + a, endns - a);
    out[endns - a] = '\0';
    return out;
}

static char *r2_lower(const char *s) {
    char *out = strdup(s);
    for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return out;
}

static size_t r2_utf8_len(const char *s) {
    size_t n = 0, i = 0;
    while (s[i]) { r2_cp(s, &i); n++; }
    return n;
}

/* Python s[:n] on codepoints (negative n counts from the end). */
static char *r2_utf8_prefix(const char *s, long long n) {
    size_t len = r2_utf8_len(s);
    long long k = n < 0 ? (long long)len + n : n;
    if (k < 0) k = 0;
    if ((size_t)k > len) k = (long long)len;
    size_t p = 0;
    for (long long j = 0; j < k; j++) r2_cp(s, &p);
    char *out = (char *)malloc(p + 1);
    memcpy(out, s, p);
    out[p] = '\0';
    return out;
}

/* ── Python float repr (shortest round-trip + CPython exponent rules) ── */

static void r2_float_repr(double d, char *buf, size_t cap) {
    if (isnan(d)) { snprintf(buf, cap, "nan"); return; }
    if (isinf(d)) { snprintf(buf, cap, d < 0 ? "-inf" : "inf"); return; }
    if (d == 0.0) { snprintf(buf, cap, signbit(d) ? "-0.0" : "0.0"); return; }
    int neg = d < 0;
    double ad = neg ? -d : d;
    char tmp[64];
    int prec;
    for (prec = 0; prec <= 16; prec++) {
        snprintf(tmp, sizeof tmp, "%.*e", prec, ad);
        if (strtod(tmp, NULL) == ad) break;
    }
    if (prec > 16) prec = 16;
    /* tmp = d[.ddd]e±XX — extract digits and decimal exponent */
    char digits[32];
    size_t dn = 0;
    const char *p = tmp;
    while (*p && *p != 'e' && *p != 'E') {
        if (*p >= '0' && *p <= '9') digits[dn++] = *p;
        p++;
    }
    digits[dn] = '\0';
    int ex = (int)strtol(p + 1, NULL, 10);
    /* strip trailing zeros of the significand (shortest form) */
    while (dn > 1 && digits[dn - 1] == '0') digits[--dn] = '\0';
    r2_buf b; r2_binit(&b);
    if (neg) r2_bputc(&b, '-');
    if (ex >= -4 && ex < 16) {
        if (ex >= 0) {
            size_t ip = (size_t)ex + 1;
            if (dn >= ip) {
                r2_bput(&b, digits, ip);
                r2_bputc(&b, '.');
                r2_bputs(&b, digits + ip);
                if (dn == ip) r2_bputc(&b, '0');
            } else {
                r2_bputs(&b, digits);
                for (size_t z = dn; z < ip; z++) r2_bputc(&b, '0');
                r2_bputs(&b, ".0");
            }
        } else {
            r2_bputs(&b, "0.");
            for (int z = 0; z < -ex - 1; z++) r2_bputc(&b, '0');
            r2_bputs(&b, digits);
        }
    } else {
        r2_bputc(&b, digits[0]);
        if (dn > 1) { r2_bputc(&b, '.'); r2_bputs(&b, digits + 1); }
        char ebuf[16];
        snprintf(ebuf, sizeof ebuf, "e%c%02d", ex < 0 ? '-' : '+', ex < 0 ? -ex : ex);
        r2_bputs(&b, ebuf);
    }
    snprintf(buf, cap, "%s", b.p);
    free(b.p);
}

/* ── Python repr / str ── */

static void r2_repr_str_buf(const char *s, r2_buf *b) {
    int has_sq = strchr(s, '\'') != NULL;
    int has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    r2_bputc(b, q);
    size_t i = 0;
    while (s[i]) {
        size_t st = i;
        unsigned c = r2_cp(s, &i);
        if (c == (unsigned)q || c == '\\') {
            r2_bputc(b, '\\');
            r2_bputc(b, (char)c);
        } else if (c == '\n') r2_bputs(b, "\\n");
        else if (c == '\r') r2_bputs(b, "\\r");
        else if (c == '\t') r2_bputs(b, "\\t");
        else if (c < 0x20 || c == 0x7F || c == 0x85 || c == 0xA0) {
            char eb[8];
            snprintf(eb, sizeof eb, "\\x%02x", c);
            r2_bputs(b, eb);
        } else if (c < 0x80) {
            r2_bputc(b, (char)c);
        } else {
            r2_bput(b, s + st, i - st);   /* printable non-ASCII kept raw */
        }
    }
    r2_bputc(b, q);
}

static void r2_repr_buf(const json_t *v, r2_buf *b) {
    char num[64];
    if (!v || v->type == JSON_NULL) { r2_bputs(b, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: r2_bputs(b, v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER:
            if (isfinite(v->num_val) && v->num_val == floor(v->num_val) &&
                fabs(v->num_val) < 9.2e18) {
                snprintf(num, sizeof num, "%lld", (long long)v->num_val);
                r2_bputs(b, num);
            } else {
                r2_float_repr(v->num_val, num, sizeof num);
                r2_bputs(b, num);
            }
            return;
        case JSON_STRING: r2_repr_str_buf(v->str_val ? v->str_val : "", b); return;
        case JSON_ARRAY:
            r2_bputc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) r2_bputs(b, ", ");
                r2_repr_buf(v->c.items[i], b);
            }
            r2_bputc(b, ']');
            return;
        case JSON_OBJECT:
            r2_bputc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) r2_bputs(b, ", ");
                r2_repr_str_buf(v->c.keys[i], b);
                r2_bputs(b, ": ");
                r2_repr_buf(v->c.items[i], b);
            }
            r2_bputc(b, '}');
            return;
        default: r2_bputs(b, "object"); return;
    }
}

static char *r2_repr(const json_t *v) {
    r2_buf b; r2_binit(&b);
    r2_repr_buf(v, &b);
    return b.p;
}

/* Python str(): strings verbatim, everything else repr(). */
static char *r2_str_of(const json_t *v) {
    if (v && v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return r2_repr(v);
}

/* ── Python int() coercion ──
 * status: 0 ok, 1 TypeError, 2 ValueError, 3 OverflowError. */
static int r2_py_int(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 0; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isnan(d)) return 2;
        if (isinf(d)) return 3;
        if (d < -9.2233720368547758e18 || d > 9.2233720368547758e18) return 3;
        *out = (long long)d;   /* truncates toward zero */
        return 0;
    }
    if (v->type != JSON_STRING) return 1;
    char *s = r2_strip(v->str_val ? v->str_val : "");
    const char *p = s;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    long long acc = 0;
    int digits = 0, prev_digit = 0, bad = 0;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            acc = acc * 10 + (*p - '0');
            digits++; prev_digit = 1;
        } else if (*p == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') {
            prev_digit = 0;
        } else { bad = 1; break; }
    }
    free(s);
    if (bad || !digits) return 2;
    *out = neg ? -acc : acc;
    return 0;
}

/* ── json.dumps(payload, indent=2) — byte-exact, ensure_ascii=True ── */

static void r2_json_str_ascii(const char *s, r2_buf *b) {
    r2_bputc(b, '"');
    size_t i = 0;
    while (s[i]) {
        size_t st = i;
        unsigned c = r2_cp(s, &i);
        if (c == '"') r2_bputs(b, "\\\"");
        else if (c == '\\') r2_bputs(b, "\\\\");
        else if (c == '\b') r2_bputs(b, "\\b");
        else if (c == '\f') r2_bputs(b, "\\f");
        else if (c == '\n') r2_bputs(b, "\\n");
        else if (c == '\r') r2_bputs(b, "\\r");
        else if (c == '\t') r2_bputs(b, "\\t");
        else if (c < 0x20) {
            char eb[8]; snprintf(eb, sizeof eb, "\\u%04x", c); r2_bputs(b, eb);
        } else if (c < 0x80) {
            r2_bputc(b, (char)c);
        } else if (c <= 0xFFFF) {
            char eb[8]; snprintf(eb, sizeof eb, "\\u%04x", c); r2_bputs(b, eb);
        } else {
            unsigned u = c - 0x10000;
            char eb[16];
            snprintf(eb, sizeof eb, "\\u%04x\\u%04x", 0xD800 + (u >> 10), 0xDC00 + (u & 0x3FF));
            r2_bputs(b, eb);
        }
        (void)st;
    }
    r2_bputc(b, '"');
}

static void r2_indent(r2_buf *b, int depth) {
    for (int i = 0; i < depth * 2; i++) r2_bputc(b, ' ');
}

static void r2_dumps2_buf(const json_t *v, r2_buf *b, int depth) {
    char num[64];
    if (!v || v->type == JSON_NULL) { r2_bputs(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: r2_bputs(b, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER:
            if (isfinite(v->num_val) && v->num_val == floor(v->num_val) &&
                fabs(v->num_val) < 9.2e18) {
                snprintf(num, sizeof num, "%lld", (long long)v->num_val);
                r2_bputs(b, num);
            } else {
                r2_float_repr(v->num_val, num, sizeof num);
                r2_bputs(b, num);
            }
            return;
        case JSON_STRING: r2_json_str_ascii(v->str_val ? v->str_val : "", b); return;
        case JSON_ARRAY:
            if (v->c.count == 0) { r2_bputs(b, "[]"); return; }
            r2_bputs(b, "[\n");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) r2_bputs(b, ",\n");
                r2_indent(b, depth + 1);
                r2_dumps2_buf(v->c.items[i], b, depth + 1);
            }
            r2_bputc(b, '\n'); r2_indent(b, depth); r2_bputc(b, ']');
            return;
        case JSON_OBJECT:
            if (v->c.count == 0) { r2_bputs(b, "{}"); return; }
            r2_bputs(b, "{\n");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) r2_bputs(b, ",\n");
                r2_indent(b, depth + 1);
                r2_json_str_ascii(v->c.keys[i], b);
                r2_bputs(b, ": ");
                r2_dumps2_buf(v->c.items[i], b, depth + 1);
            }
            r2_bputc(b, '\n'); r2_indent(b, depth); r2_bputc(b, '}');
            return;
        default: r2_bputs(b, "null"); return;
    }
}

static char *r2_dumps2(const json_t *v) {
    r2_buf b; r2_binit(&b);
    r2_dumps2_buf(v, &b, 0);
    return b.p;
}

/* Seam-args convention: drop None-valued keys on BOTH sides. */
static json_t *r2_null_filter(const json_t *args) {
    json_t *o = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            json_t *val = args->c.items[i];
            if (val && val->type == JSON_NULL) continue;
            json_set(o, args->c.keys[i], json_copy(val));
        }
    }
    return o;
}

/* ── lexical Path model (POSIX-form strings; host semantics) ── */

static char *r2_path_norm(const char *s) {
    int rooted = s[0] == '/';
    int unc = rooted && s[1] == '/';
    r2_buf b; r2_binit(&b);
    if (unc) r2_bputs(&b, "//");
    else if (rooted) r2_bputc(&b, '/');
    const char *p = s;
    int first = 1;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *st = p;
        while (*p && *p != '/') p++;
        size_t n = (size_t)(p - st);
        if (n == 1 && st[0] == '.') continue;
        if (!first) r2_bputc(&b, '/');
        r2_bput(&b, st, n);
        first = 0;
    }
    if (first && !rooted) { free(b.p); return strdup("."); }
    return b.p;
}

static int r2_path_is_abs(const char *norm) {
#ifdef _WIN32
    if (norm[0] == '/' && norm[1] == '/') return 1;
    if (((norm[0] >= 'A' && norm[0] <= 'Z') || (norm[0] >= 'a' && norm[0] <= 'z')) &&
        norm[1] == ':' && norm[2] == '/') return 1;
    return 0;
#else
    return norm[0] == '/';
#endif
}

static char *r2_path_parent(const char *norm) {
    if (strcmp(norm, "/") == 0) return strdup("/");
    if (norm[0] == '/' && norm[1] == '/') return strdup(norm);
    const char *last = strrchr(norm, '/');
    if (!last) return strdup(".");
    if (last == norm) return strdup("/");
    size_t n = (size_t)(last - norm);
    char *out = (char *)malloc(n + 2);
    memcpy(out, norm, n);
    out[n] = '\0';
    /* drive-root parent: 'C:' -> 'C:/' */
    if (n == 2 && out[1] == ':' &&
        ((out[0] >= 'A' && out[0] <= 'Z') || (out[0] >= 'a' && out[0] <= 'z'))) {
        out[2] = '/'; out[3] = '\0';
    }
    return out;
}

static char *r2_path_join(const char *a, const char *b) {
    if (!b[0]) return r2_path_norm(a);
    char *bn = r2_path_norm(b);
    if (r2_path_is_abs(bn) || bn[0] == '/') return bn;
    char *an = r2_path_norm(a);
    if (strcmp(an, ".") == 0) {   /* Path(".") / "x" == Path("x") */
        free(an);
        return bn;
    }
    r2_buf t; r2_binit(&t);
    r2_bputs(&t, an);
    if (t.n == 0 || t.p[t.n - 1] != '/') r2_bputc(&t, '/');
    r2_bputs(&t, bn);
    free(an); free(bn);
    return t.p;
}

/* parts: leading "/" is its own part; components follow. "." -> 0 parts. */
static size_t r2_path_parts(const char *norm, char **out, size_t max) {
    size_t n = 0;
    const char *p = norm;
    if (p[0] == '/' && p[1] != '/') {
        if (n < max) out[n] = strdup("/");
        n++;
        p++;
    } else if (p[0] == '/' && p[1] == '/') {
        /* UNC: first two components form the root part (fixture scope) */
        const char *st = p;
        p += 2;
        while (*p && *p != '/') p++;
        if (*p == '/') { p++; while (*p && *p != '/') p++; }
        if (n < max) {
            size_t len = (size_t)(p - st);
            out[n] = (char *)malloc(len + 1);
            memcpy(out[n], st, len);
            out[n][len] = '\0';
        }
        n++;
    }
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *st = p;
        while (*p && *p != '/') p++;
        if (n < max) {
            size_t len = (size_t)(p - st);
            out[n] = (char *)malloc(len + 1);
            memcpy(out[n], st, len);
            out[n][len] = '\0';
        }
        n++;
    }
    return n;
}

static void r2_parts_free(char **parts, size_t n) {
    for (size_t i = 0; i < n; i++) free(parts[i]);
}

static int r2_part_cmp(const char *a, const char *b) {
#ifdef _WIN32
    char *la = r2_lower(a), *lb = r2_lower(b);
    int r = strcmp(la, lb);
    free(la); free(lb);
    return r;
#else
    return strcmp(a, b);
#endif
}

/* sorted() on Paths: tuple-of-parts comparison (host normcase). */
static int r2_path_lt(const char *a, const char *b) {
    char *pa[64], *pb[64];
    size_t na = r2_path_parts(a, pa, 64), nb = r2_path_parts(b, pb, 64);
    int res = 0;
    size_t m = na < nb ? na : nb;
    for (size_t i = 0; i < m; i++) {
        int c = r2_part_cmp(pa[i], pb[i]);
        if (c != 0) { res = c < 0; goto done; }
    }
    res = na < nb;
done:
    r2_parts_free(pa, na); r2_parts_free(pb, nb);
    return res;
}

static int r2_path_relative_ok(const char *p, const char *base) {
    char *pa[64], *pb[64];
    size_t na = r2_path_parts(p, pa, 64), nb = r2_path_parts(base, pb, 64);
    int ok = nb <= na;
    if (ok) {
        for (size_t i = 0; i < nb; i++) {
            if (r2_part_cmp(pa[i], pb[i]) != 0) { ok = 0; break; }
        }
    }
    r2_parts_free(pa, na); r2_parts_free(pb, nb);
    return ok;
}

/* ── ISO-8601 aware timestamp -> epoch microseconds (exact) ── */

static int64_t r2_days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static int r2_iso_us(const char *s, int64_t *out) {
    int y, mo, d, h, mi, sec;
    char sep;
    int consumed = 0;
    if (sscanf(s, "%4d-%2d-%2d%c%2d:%2d:%2d%n", &y, &mo, &d, &sep, &h, &mi, &sec, &consumed) < 7)
        return 0;
    if (sep != 'T' && sep != ' ') return 0;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec > 59) return 0;
    const char *p = s + consumed;
    int64_t frac = 0;
    if (*p == '.') {
        p++;
        int64_t scale = 100000;
        int nd = 0;
        while (*p >= '0' && *p <= '9' && nd < 6) {
            frac += (*p - '0') * scale;
            scale /= 10; p++; nd++;
        }
        while (*p >= '0' && *p <= '9') p++;
    }
    int off_min = 0;
    if (*p == 'Z') { p++; }
    else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1;
        p++;
        int oh = 0, om = 0;
        if (sscanf(p, "%2d:%2d", &oh, &om) == 2) { }
        else if (sscanf(p, "%2d%2d", &oh, &om) == 2) { }
        else if (sscanf(p, "%2d", &oh) == 1) { om = 0; }
        else return 0;
        off_min = sign * (oh * 60 + om);
        while (*p && *p != 'Z') p++;
    } else return 0;   /* aware stamps only (fixture scope) */
    if (*p) return 0;
    int64_t days = r2_days_from_civil(y, mo, d);
    int64_t us = ((days * 24 + h) * 3600 + (int64_t)mi * 60 + sec) * 1000000 + frac;
    us -= (int64_t)off_min * 60 * 1000000;
    *out = us;
    return 1;
}

#ifndef R2_REVIVE2_STR
#define R2_REVIVE2_STR
/* Python str.split(sep) — empties kept ("" -> [""]). */
static char **r2_split(const char *s, char sep, size_t *count) {
    size_t n = 1;
    for (const char *p = s; *p; p++) if (*p == sep) n++;
    char **out = (char **)malloc(n * sizeof(char *));
    size_t idx = 0;
    const char *st = s;
    for (const char *p = s; ; p++) {
        if (*p == sep || !*p) {
            size_t len = (size_t)(p - st);
            out[idx] = (char *)malloc(len + 1);
            memcpy(out[idx], st, len);
            out[idx][len] = '\0';
            idx++;
            if (!*p) break;
            st = p + 1;
        }
    }
    *count = n;
    return out;
}
static void r2_split_free(char **parts, size_t n) {
    for (size_t i = 0; i < n; i++) free(parts[i]);
    free(parts);
}
static int r2_starts_with(const char *s, const char *pfx) {
    return strncmp(s, pfx, strlen(pfx)) == 0;
}
/* str.splitlines() boundaries: \n \r \r\n \v \f \x1c \x1d \x1e \x85 \u2028 \u2029 */
static int r2_is_line_boundary(unsigned c) {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
           (c >= 0x1C && c <= 0x1E) || c == 0x85 || c == 0x2028 || c == 0x2029;
}
static char *r2_first_line(const char *s) {
    size_t i = 0;
    while (s[i]) {
        size_t st = i;
        unsigned c = r2_cp(s, &i);
        if (r2_is_line_boundary(c)) {
            char *out = (char *)malloc(st + 1);
            memcpy(out, s, st);
            out[st] = '\0';
            return out;
        }
    }
    return strdup(s);
}
/* " ".join(s.split()) — Unicode-whitespace runs collapsed, empties dropped. */
static char *r2_collapse_ws(const char *s) {
    r2_buf b; r2_binit(&b);
    size_t i = 0;
    int first = 1;
    while (s[i]) {
        size_t st = i;
        unsigned c = r2_cp(s, &i);
        if (r2_is_space(c)) continue;
        size_t ws = st;
        while (s[i]) {
            size_t st2 = i;
            unsigned c2 = r2_cp(s, &i);
            if (r2_is_space(c2)) { i = st2; break; }
        }
        if (!first) r2_bputc(&b, ' ');
        r2_bput(&b, s + ws, i - ws);
        first = 0;
    }
    return b.p;
}
#endif /* R2_REVIVE2_STR */

#endif /* R2_REVIVE2_SHARED */

/* ── hc seam channel (cron_gaps) ── */

typedef struct hc_io {
    json_t *(*call)(struct hc_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} hc_io_t;

/* args ADOPTED (freed here); ret is NEW (caller frees). */
static json_t *hc_seam(hc_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        json_free(args);
        return r2_raise("RuntimeError", "no seam channel installed");
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", r2_null_filter(args));
    json_free(args);
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    if (io->trace) json_append(io->trace, ev);
    else json_free(ev);
    return ret;
}

/* _short_reason(text, limit=120) — real on both sides. */
static char *hc_short_reason(const json_t *text) {
    char *raw = r2_truthy(text) ? r2_str_of(text) : strdup("");
    char *stripped = r2_strip(raw);
    free(raw);
    char *first = r2_first_line(stripped);
    free(stripped);
    char *reason = r2_collapse_ws(first);
    free(first);
    char *out;
    if (r2_utf8_len(reason) > 120) {
        char *cut = r2_utf8_prefix(reason, 119);
        r2_buf b; r2_binit(&b);
        r2_bputs(&b, cut);
        r2_bputs(&b, R2_L_ELLIPSIS);
        out = b.p;
        free(cut);
    } else {
        out = reason[0] ? strdup(reason) : strdup(R2_L_NO_DETAILS);
    }
    free(reason);
    return out;
}

/* _delivery_fix_hint(job) */
static char *hc_delivery_fix_hint(const json_t *job) {
    json_t *idv = json_obj_get(job, "id");
    char *ids = idv ? r2_str_of(idv) : strdup(R2_L_HINT_ID_DEFAULT);
    r2_buf b; r2_binit(&b);
    r2_bputs(&b, R2_L_HINT_PRE);
    r2_bputs(&b, ids);
    r2_bputs(&b, R2_L_HINT_SUF);
    free(ids);
    return b.p;
}

/* _unverified_targets(unverified) */
static char *hc_unverified_targets(const json_t *uv) {
    if (uv && uv->type == JSON_ARRAY) {
        r2_buf b; r2_binit(&b);
        for (size_t i = 0; i < uv->c.count; i++) {
            if (i) r2_bputs(&b, ", ");
            char *s = r2_str_of(uv->c.items[i]);
            r2_bputs(&b, s);
            free(s);
        }
        return b.p;
    }
    return r2_str_of(uv);
}

/* _dispatch_kind_label(kind) — NULL for unknown; raise spec for unhashable. */
static char *hc_dispatch_kind_label(const json_t *kind, json_t **err) {
    *err = NULL;
    if (kind && (kind->type == JSON_ARRAY || kind->type == JSON_OBJECT)) {
        char msg[160];
        snprintf(msg, sizeof msg,
                 "cannot use '%s' as a dict key (unhashable type: '%s')",
                 r2_typename(kind), r2_typename(kind));
        *err = r2_raise("TypeError", msg);
        return NULL;
    }
    if (kind && kind->type == JSON_STRING) {
        if (strcmp(kind->str_val, "catch_up") == 0) return strdup(R2_L_KIND_CATCHUP);
        if (strcmp(kind->str_val, "late") == 0) return strdup(R2_L_KIND_LATE);
    }
    return NULL;
}

/* _missed_fire_issue(job, fire_err) */
static char *hc_missed_fire_issue(const json_t *job, const json_t *fe) {
    json_t *at = json_obj_get(fe, "at");
    char *ats = at ? r2_str_of(at) : strdup(R2_L_MF_AT_DEFAULT);
    char *sr = hc_short_reason(json_obj_get(fe, "detail"));
    json_t *idv = json_obj_get(job, "id");
    char *ids = idv ? r2_str_of(idv) : strdup(R2_L_HINT_ID_DEFAULT);
    r2_buf b; r2_binit(&b);
    r2_bputs(&b, R2_L_MF_PRE);
    r2_bputs(&b, ats);
    r2_bputs(&b, R2_L_MF_MID1);
    r2_bputs(&b, sr);
    r2_bputs(&b, R2_L_MF_MID2);
    r2_bputs(&b, ids);
    r2_bputs(&b, R2_L_MF_SUF);
    free(ats); free(sr); free(ids);
    return b.p;
}

/* ── ports ── */

/* PoP: format_lateness @ hermes_cli/cron.py:_format_lateness */
json_t *format_lateness(void *io, json_t *in) {
    (void)io;
    json_t *sv = json_obj_get(in, "seconds");
    long long secs = 0;
    int st = r2_py_int(sv, &secs);
    if (st == 1 || st == 2) return json_string(R2_L_FL_QMARK);
    if (st == 3)
        return r2_raise("OverflowError", "cannot convert float infinity to integer");
    if (secs < 0) secs = 0;   /* max(0, int(seconds)) */
    char buf[64];
    if (secs < 60) {
        snprintf(buf, sizeof buf, "%llds", secs);
        return json_string(buf);
    }
    long long minutes = secs / 60;
    long long hours = minutes / 60;
    minutes %= 60;
    long long days = hours / 24;
    hours %= 24;
    long long mpart = days ? 0 : minutes;   /* minutes dropped when days */
    r2_buf b; r2_binit(&b);
    if (days) { snprintf(buf, sizeof buf, "%lldd", days); r2_bputs(&b, buf); }
    if (hours) {
        if (b.n) r2_bputc(&b, ' ');
        snprintf(buf, sizeof buf, "%lldh", hours);
        r2_bputs(&b, buf);
    }
    if (mpart) {
        if (b.n) r2_bputc(&b, ' ');
        snprintf(buf, sizeof buf, "%lldm", mpart);
        r2_bputs(&b, buf);
    }
    if (!b.n) r2_bputs(&b, R2_L_FL_ZERO_M);
    json_t *ret = json_string(b.p);
    free(b.p);
    return ret;
}

/* PoP: dispatch_display @ hermes_cli/cron.py:_dispatch_display
 * color() is the seam (codes ride as an array); _format_lateness and
 * _dispatch_kind_label run real. */
json_t *dispatch_display(hc_io_t *io, json_t *in) {
    json_t *d = json_obj_get(in, "dispatch");
    if (!d || d->type != JSON_OBJECT) return json_null();
    json_t *sched = json_obj_get(d, "scheduled_at");
    json_t *actual = json_obj_get(d, "dispatched_at");
    json_t *kind = json_obj_get(d, "kind");
    if (!r2_truthy(sched) || !r2_truthy(actual) || !r2_truthy(kind))
        return json_null();
    json_t *fin = json_object();
    json_t *ls = json_obj_get(d, "lateness_seconds");
    json_set(fin, "seconds", ls ? json_copy(ls) : json_number(0));
    json_t *lat = format_lateness(NULL, fin);
    json_free(fin);
    if (r2_raise_kind(lat)) return lat;
    char *lateness = strdup(lat->str_val);
    json_free(lat);

    if (kind->type == JSON_STRING && strcmp(kind->str_val, "on_time") == 0) {
        char *ss = r2_str_of(sched);
        r2_buf t; r2_binit(&t);
        r2_bputs(&t, R2_L_DD_ONTIME_PRE);
        r2_bputs(&t, ss);
        r2_bputs(&t, R2_L_DD_ONTIME_SUF);
        free(ss);
        json_t *a = json_object();
        json_set_hdr(a, "text", t.p);
        free(t.p);
        json_t *codes = json_array();
        json_append(codes, json_string("\x1b[2m"));   /* Colors.DIM */
        json_set(a, "codes", codes);
        json_t *r = hc_seam(io, "color", a);
        free(lateness);
        return r ? r : json_null();
    }
    json_t *lerr = NULL;
    char *label = hc_dispatch_kind_label(kind, &lerr);
    if (lerr) { free(lateness); return lerr; }
    if (!label) label = strdup(R2_L_DD_LATE_DEFAULT);

    r2_buf t1; r2_binit(&t1);
    r2_bputs(&t1, R2_L_DD_WARN_PRE);
    r2_bputs(&t1, label);
    r2_bputs(&t1, R2_L_DD_WARN_SUF);
    free(label);
    json_t *a = json_object();
    json_set_hdr(a, "text", t1.p);
    free(t1.p);
    json_t *codes = json_array();
    json_append(codes, json_string("\x1b[33m"));   /* Colors.YELLOW */
    json_set(a, "codes", codes);
    json_t *r1 = hc_seam(io, "color", a);
    if (r2_raise_kind(r1)) { free(lateness); return r1; }
    if (!r1 || r1->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "can only concatenate str (not \"%s\") to str",
                 r2_typename(r1));
        json_free(r1); free(lateness);
        return r2_raise("TypeError", msg);
    }
    char *ss = r2_str_of(sched);
    char *as = r2_str_of(actual);
    r2_buf mid; r2_binit(&mid);
    r2_bputs(&mid, R2_L_DD_SCHED_PRE);
    r2_bputs(&mid, ss);
    r2_bputs(&mid, R2_L_DD_RAN_MID);
    r2_bputs(&mid, as);
    r2_bputs(&mid, R2_L_DD_RAN_SUF);
    free(ss); free(as);
    r2_buf t2; r2_binit(&t2);
    r2_bputs(&t2, R2_L_DD_LATE_PRE);
    r2_bputs(&t2, lateness);
    r2_bputs(&t2, R2_L_DD_LATE_SUF);
    free(lateness);
    a = json_object();
    json_set_hdr(a, "text", t2.p);
    free(t2.p);
    codes = json_array();
    json_append(codes, json_string("\x1b[33m"));
    json_set(a, "codes", codes);
    json_t *r2 = hc_seam(io, "color", a);
    if (r2_raise_kind(r2)) { json_free(r1); free(mid.p); return r2; }
    if (!r2 || r2->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "can only concatenate str (not \"%s\") to str",
                 r2_typename(r2));
        json_free(r1); json_free(r2); free(mid.p);
        return r2_raise("TypeError", msg);
    }
    r2_buf out; r2_binit(&out);
    r2_bputs(&out, r1->str_val);
    r2_bputs(&out, mid.p);
    r2_bputs(&out, r2->str_val);
    json_free(r1); json_free(r2); free(mid.p);
    json_t *ret = json_string(out.p);
    free(out.p);
    return ret;
}

/* PoP: scripts_dir_for_cron @ hermes_cli/cron.py:_scripts_dir_for_cron
 * CRON_DIR rides the "cron_dir" seam (the oracle's cron.jobs module double
 * records the attribute read via __getattr__). */
json_t *scripts_dir_for_cron(hc_io_t *io, json_t *in) {
    (void)in;
    json_t *cd = hc_seam(io, "cron_dir", json_object());
    if (r2_raise_kind(cd)) return cd;
    if (!cd || cd->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'parent'",
                 r2_typename(cd));
        json_free(cd);
        return r2_raise("AttributeError", msg);
    }
    char *norm = r2_path_norm(cd->str_val);
    json_free(cd);
    char *par = r2_path_parent(norm);
    free(norm);
    char *joined = r2_path_join(par, "scripts");
    free(par);
    json_t *ret = json_string(joined);
    free(joined);
    return ret;
}

/* PoP: script_health_issue @ hermes_cli/cron.py:_script_health_issue
 * Path I/O (resolve/expanduser/exists/is_file) rides seams; the lexical
 * ops (is_absolute/join/relative_to) use the host pathlib model. */
json_t *script_health_issue(hc_io_t *io, json_t *in) {
    json_t *sv = json_obj_get(in, "script");
    /* Live statement order: scripts_dir = _scripts_dir_for_cron().resolve()
     * runs BEFORE Path(script) — its seams fire even for a non-str script. */
    json_t *sd = scripts_dir_for_cron(io, NULL);
    if (r2_raise_kind(sd)) return sd;
    char *sdn = r2_path_norm(sd->str_val);
    json_free(sd);
    json_t *a = json_object();
    json_set_hdr(a, "path", sdn);
    json_t *rs = hc_seam(io, "path_resolve", a);
    free(sdn);
    if (r2_raise_kind(rs)) return rs;
    if (!rs || rs->type != JSON_STRING) {
        json_free(rs);
        return r2_raise("TypeError", "path seam returned non-str");
    }
    char *scripts_dir = strdup(rs->str_val);
    json_free(rs);

    /* Path(script) — evaluated after the scripts_dir resolve (live order). */
    if (!sv || sv->type != JSON_STRING) {
        char msg[192];
        snprintf(msg, sizeof msg,
                 "argument should be a str or an os.PathLike object where "
                 "__fspath__ returns a str, not '%s'",
                 r2_typename(sv));
        free(scripts_dir);
        return r2_raise("TypeError", msg);
    }
    const char *script = sv->str_val;
    a = json_object();
    json_set_hdr(a, "path", script);
    json_t *eu = hc_seam(io, "path_expanduser", a);
    if (r2_raise_kind(eu)) { free(scripts_dir); return eu; }
    if (!eu || eu->type != JSON_STRING) {
        json_free(eu); free(scripts_dir);
        return r2_raise("TypeError", "path seam returned non-str");
    }
    char *rawn = r2_path_norm(eu->str_val);
    json_free(eu);
    char *joined = NULL;
    const char *resolve_arg;
    if (r2_path_is_abs(rawn)) {
        resolve_arg = rawn;
    } else {
        joined = r2_path_join(scripts_dir, rawn);
        resolve_arg = joined;
    }
    a = json_object();
    json_set_hdr(a, "path", resolve_arg);
    rs = hc_seam(io, "path_resolve", a);
    if (r2_raise_kind(rs)) { free(rawn); free(joined); free(scripts_dir); return rs; }
    if (!rs || rs->type != JSON_STRING) {
        json_free(rs); free(rawn); free(joined); free(scripts_dir);
        return r2_raise("TypeError", "path seam returned non-str");
    }
    char *path = strdup(rs->str_val);
    json_free(rs);
    free(rawn); free(joined);

    json_t *ret = NULL;
    if (!r2_path_relative_ok(path, scripts_dir)) {
        char *rs_repr = r2_repr(sv);
        r2_buf b; r2_binit(&b);
        r2_bputs(&b, R2_L_SH_OUTSIDE_PRE);
        r2_bputs(&b, scripts_dir);
        r2_bputs(&b, R2_L_SH_OUTSIDE_MID);
        r2_bputs(&b, rs_repr);
        free(rs_repr);
        ret = json_string(b.p);
        free(b.p);
    } else {
        a = json_object();
        json_set_hdr(a, "path", path);
        json_t *ex = hc_seam(io, "path_exists", a);
        if (r2_raise_kind(ex)) { free(path); free(scripts_dir); return ex; }
        int exists = r2_truthy(ex);
        json_free(ex);
        if (!exists) {
            r2_buf b; r2_binit(&b);
            r2_bputs(&b, R2_L_SH_NOTFOUND_PRE);
            r2_bputs(&b, path);
            ret = json_string(b.p);
            free(b.p);
        } else {
            a = json_object();
            json_set_hdr(a, "path", path);
            json_t *fi = hc_seam(io, "path_is_file", a);
            if (r2_raise_kind(fi)) { free(path); free(scripts_dir); return fi; }
            int is_file = r2_truthy(fi);
            json_free(fi);
            if (!is_file) {
                r2_buf b; r2_binit(&b);
                r2_bputs(&b, R2_L_SH_NOTFILE_PRE);
                r2_bputs(&b, path);
                ret = json_string(b.p);
                free(b.p);
            } else {
                ret = json_null();
            }
        }
    }
    free(path);
    free(scripts_dir);
    return ret;
}

/* PoP: next_run_overdue_issue @ hermes_cli/cron.py:_next_run_overdue_issue
 * _parse_aware/hermes_time.now ride seams returning aware ISO strings (or
 * null for unparseable); the instant math is exact epoch-microseconds. */
json_t *next_run_overdue_issue(hc_io_t *io, json_t *in) {
    json_t *nr = json_obj_get(in, "next_run");
    json_t *a = json_object();
    json_set(a, "value", nr ? json_copy(nr) : json_null());
    json_t *pa = hc_seam(io, "parse_aware", a);
    if (r2_raise_kind(pa)) return pa;
    if (!pa || pa->type == JSON_NULL) {
        json_free(pa);
        char *rp = r2_repr(nr);
        r2_buf b; r2_binit(&b);
        r2_bputs(&b, R2_L_NR_INVALID_PRE);
        r2_bputs(&b, rp);
        free(rp);
        json_t *ret = json_string(b.p);
        free(b.p);
        return ret;
    }
    if (pa->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'astimezone'",
                 r2_typename(pa));
        json_free(pa);
        return r2_raise("AttributeError", msg);
    }
    int64_t dt_us = 0;
    if (!r2_iso_us(pa->str_val, &dt_us)) {
        char msg[192];
        snprintf(msg, sizeof msg, "Invalid isoformat string: '%s'", pa->str_val);
        json_free(pa);
        return r2_raise("ValueError", msg);
    }
    json_free(pa);
    json_t *nw = hc_seam(io, "now", json_object());
    if (r2_raise_kind(nw)) return nw;
    if (!nw || nw->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'astimezone'",
                 r2_typename(nw));
        json_free(nw);
        return r2_raise("AttributeError", msg);
    }
    int64_t now_us = 0;
    if (!r2_iso_us(nw->str_val, &now_us)) {
        char msg[192];
        snprintf(msg, sizeof msg, "Invalid isoformat string: '%s'", nw->str_val);
        json_free(nw);
        return r2_raise("ValueError", msg);
    }
    json_free(nw);
    double overdue = (double)(now_us - dt_us) / 1000000.0;
    if (overdue <= 900.0) return json_null();   /* _OVERDUE_GRACE_SECONDS */
    char amount[64];
    if (overdue >= 3600.0)
        snprintf(amount, sizeof amount, "%.1f%s", overdue / 3600.0, R2_L_NR_H_SUFFIX);
    else
        snprintf(amount, sizeof amount, "%.0f%s", overdue / 60.0, R2_L_NR_M_SUFFIX);
    r2_buf b; r2_binit(&b);
    r2_bputs(&b, R2_L_NR_OVERDUE_PRE);
    r2_bputs(&b, amount);
    r2_bputs(&b, R2_L_NR_OVERDUE_SUF);
    json_t *ret = json_string(b.p);
    free(b.p);
    return ret;
}

/* PoP: cron_doctor_issues_for_job @ hermes_cli/cron.py:_cron_doctor_issues_for_job
 * Pure helpers (_short_reason/_delivery_fix_hint/_unverified_targets/
 * _dispatch_kind_label/_missed_fire_issue) run real; _format_lateness,
 * _next_run_overdue_issue and _script_health_issue are the sibling ports
 * above (real on both sides). zone_for_job/normalize_grace_minutes and the
 * workdir Path I/O ride seams. Only ValueError is caught around
 * normalize_grace_minutes — every other kind propagates. */
json_t *cron_doctor_issues_for_job(hc_io_t *io, json_t *in) {
    json_t *job = json_obj_get(in, "job");
    if (!job || job->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                 r2_typename(job));
        return r2_raise("AttributeError", msg);
    }
    json_t *issues = json_array();
    json_t *a;
    const char *kind;

    {
        json_t *lsv = json_obj_get(job, "last_status");
        char *raw = r2_truthy(lsv) ? r2_str_of(lsv) : strdup("");
        char *stripped = r2_strip(raw);
        free(raw);
        char *last_status = r2_lower(stripped);
        free(stripped);
        if (last_status[0] &&
            strcmp(last_status, "ok") != 0 &&
            strcmp(last_status, "delivery_failed") != 0 &&
            strcmp(last_status, "delivery_queued") != 0) {
            json_t *lev = json_obj_get(job, "last_error");
            char *es = r2_truthy(lev) ? r2_str_of(lev) : strdup(R2_L_DOC_UNKNOWN_ERR);
            char *est = r2_strip(es);
            free(es);
            r2_buf b; r2_binit(&b);
            r2_bputs(&b, R2_L_DOC_LASTRUN_PRE);
            r2_bputs(&b, est);
            free(est);
            json_append(issues, json_string(b.p));
            free(b.p);
        }
        free(last_status);
    }
    {
        json_t *dev = json_obj_get(job, "last_delivery_error");
        char *raw = r2_truthy(dev) ? r2_str_of(dev) : strdup("");
        char *delivery_err = r2_strip(raw);
        free(raw);
        if (delivery_err[0]) {
            json_t *dev_node = json_string(delivery_err);
            char *sr = hc_short_reason(dev_node);
            json_free(dev_node);
            char *hint = hc_delivery_fix_hint(job);
            r2_buf b; r2_binit(&b);
            r2_bputs(&b, R2_L_DOC_DELIV_PRE);
            r2_bputs(&b, sr);
            r2_bputs(&b, R2_L_DOC_DELIV_MID);
            r2_bputs(&b, hint);
            free(sr); free(hint);
            json_append(issues, json_string(b.p));
            free(b.p);
        }
        free(delivery_err);
    }
    {
        json_t *uv = json_obj_get(job, "last_delivery_unverified");
        if (r2_truthy(uv)) {
            char *targets = hc_unverified_targets(uv);
            r2_buf b; r2_binit(&b);
            r2_bputs(&b, R2_L_DOC_UNVER_PRE);
            r2_bputs(&b, targets);
            free(targets);
            json_append(issues, json_string(b.p));
            free(b.p);
        }
    }
    {
        json_t *disp = json_obj_get(job, "last_dispatch");
        if (disp && disp->type == JSON_OBJECT) {
            json_t *lerr = NULL;
            char *label = hc_dispatch_kind_label(json_obj_get(disp, "kind"), &lerr);
            if (lerr) { json_free(issues); return lerr; }
            if (label) {
                json_t *sav = json_obj_get(disp, "scheduled_at");
                char *sched = sav ? r2_str_of(sav) : strdup("?");
                json_t *fin = json_object();
                json_t *lsv = json_obj_get(disp, "lateness_seconds");
                json_set(fin, "seconds", lsv ? json_copy(lsv) : json_number(0));
                json_t *lat = format_lateness(NULL, fin);
                json_free(fin);
                if (r2_raise_kind(lat)) {
                    free(label); free(sched); json_free(issues);
                    return lat;
                }
                r2_buf b; r2_binit(&b);
                r2_bputs(&b, R2_L_DOC_FIRE_PRE);
                r2_bputs(&b, label);
                r2_bputs(&b, R2_L_DOC_FIRE_MID1);
                r2_bputs(&b, sched);
                r2_bputs(&b, R2_L_DOC_FIRE_MID2);
                r2_bputs(&b, lat->str_val);
                r2_bputs(&b, R2_L_DOC_FIRE_SUF);
                json_append(issues, json_string(b.p));
                free(b.p);
                json_free(lat);
                free(sched);
            }
            free(label);
        }
    }
    {
        json_t *fe = json_obj_get(job, "last_fire_error");
        if (fe && fe->type == JSON_OBJECT && r2_truthy(json_obj_get(fe, "detail"))) {
            char *mf = hc_missed_fire_issue(job, fe);
            json_append(issues, json_string(mf));
            free(mf);
        }
    }
    {
        json_t *en = json_obj_get(job, "enabled");
        int enabled = en ? r2_truthy(en) : 1;
        json_t *state = json_obj_get(job, "state");
        if (state && (state->type == JSON_ARRAY || state->type == JSON_OBJECT)) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "cannot use '%s' as a set element (unhashable type: '%s')",
                     r2_typename(state), r2_typename(state));
            json_free(issues);
            return r2_raise("TypeError", msg);
        }
        int parked = state && state->type == JSON_STRING &&
                     (strcmp(state->str_val, "paused") == 0 ||
                      strcmp(state->str_val, "completed") == 0);
        if (enabled && !parked) {
            json_t *nrv = json_obj_get(job, "next_run_at");
            char *raw = r2_truthy(nrv) ? r2_str_of(nrv) : strdup("");
            char *next_run = r2_strip(raw);
            free(raw);
            json_t *issue;
            if (next_run[0]) {
                json_t *nin = json_object();
                json_set_hdr(nin, "next_run", next_run);
                issue = next_run_overdue_issue(io, nin);
                json_free(nin);
                if (r2_raise_kind(issue)) {
                    free(next_run); json_free(issues);
                    return issue;
                }
            } else {
                issue = json_string(R2_L_DOC_NO_NEXT);
            }
            if (issue && issue->type != JSON_NULL) json_append(issues, issue);
            else json_free(issue);
            free(next_run);
        }
    }
    {
        json_t *scv = json_obj_get(job, "script");
        char *raw = r2_truthy(scv) ? r2_str_of(scv) : strdup("");
        char *script = r2_strip(raw);
        free(raw);
        if (r2_truthy(json_obj_get(job, "no_agent")) && !script[0])
            json_append(issues, json_string(R2_L_DOC_NO_SCRIPT));
        if (script[0]) {
            json_t *sin = json_object();
            json_set_hdr(sin, "script", script);
            json_t *si = script_health_issue(io, sin);
            json_free(sin);
            if (r2_raise_kind(si)) {
                free(script); json_free(issues);
                return si;
            }
            if (si && si->type != JSON_NULL) json_append(issues, si);
            else json_free(si);
        }
        free(script);
    }
    {
        json_t *wv = json_obj_get(job, "workdir");
        char *raw = r2_truthy(wv) ? r2_str_of(wv) : strdup("");
        char *workdir = r2_strip(raw);
        free(raw);
        if (workdir[0]) {
            a = json_object();
            json_set_hdr(a, "path", workdir);
            json_t *eu = hc_seam(io, "path_expanduser", a);
            if (r2_raise_kind(eu)) { free(workdir); json_free(issues); return eu; }
            if (!eu || eu->type != JSON_STRING) {
                json_free(eu); free(workdir); json_free(issues);
                return r2_raise("TypeError", "path seam returned non-str");
            }
            a = json_object();
            json_set_hdr(a, "path", eu->str_val);
            json_t *ex = hc_seam(io, "path_exists", a);
            json_free(eu);
            if (r2_raise_kind(ex)) { free(workdir); json_free(issues); return ex; }
            int exists = r2_truthy(ex);
            json_free(ex);
            if (!exists) {
                r2_buf b; r2_binit(&b);
                r2_bputs(&b, R2_L_DOC_WORKDIR_PRE);
                r2_bputs(&b, workdir);
                json_append(issues, json_string(b.p));
                free(b.p);
            }
        }
        free(workdir);
    }
    {
        json_t *tz = json_obj_get(job, "timezone");
        if (r2_truthy(tz)) {
            a = json_object();
            json_set(a, "job", json_copy(job));
            json_t *zf = hc_seam(io, "zone_for_job", a);
            if (r2_raise_kind(zf)) { json_free(issues); return zf; }
            if (!zf || zf->type == JSON_NULL) {
                char *rp = r2_repr(tz);
                r2_buf b; r2_binit(&b);
                r2_bputs(&b, R2_L_DOC_TZ_PRE);
                r2_bputs(&b, rp);
                r2_bputs(&b, R2_L_DOC_TZ_SUF);
                free(rp);
                json_append(issues, json_string(b.p));
                free(b.p);
            }
            json_free(zf);
        }
    }
    {
        json_t *gm = json_obj_get(job, "grace_minutes");
        if (gm && gm->type != JSON_NULL) {   /* `is not None` identity */
            a = json_object();
            json_set(a, "value", json_copy(gm));
            json_t *ng = hc_seam(io, "normalize_grace_minutes", a);
            kind = r2_raise_kind(ng);
            if (kind) {
                if (strcmp(kind, "ValueError") == 0) {
                    r2_buf b; r2_binit(&b);
                    r2_bputs(&b, R2_L_DOC_GRACE_PRE);
                    r2_bputs(&b, r2_raise_msg(ng));
                    r2_bputs(&b, R2_L_DOC_GRACE_SUF);
                    json_append(issues, json_string(b.p));
                    free(b.p);
                    json_free(ng);
                } else {
                    json_free(issues);
                    return ng;   /* only ValueError is caught */
                }
            } else {
                json_free(ng);
                json_t *sch = json_obj_get(job, "schedule");
                if (r2_truthy(sch) && sch->type != JSON_OBJECT) {
                    char msg[128];
                    snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                             r2_typename(sch));
                    json_free(issues);
                    return r2_raise("AttributeError", msg);
                }
                json_t *sk = r2_truthy(sch) ? json_obj_get(sch, "kind") : NULL;
                int is_cron = sk && sk->type == JSON_STRING &&
                              strcmp(sk->str_val, "cron") == 0;
                if (!is_cron)
                    json_append(issues, json_string(R2_L_DOC_GRACE_NONCRON));
            }
        }
    }
    return issues;
}

/* Deferred (deep CLI print flows — untagged stubs, rest_revive2):
 * cron_incidents / cron_doctor / cron_command.
 * cron_command is PoP-claimed by src/hermes_cli/commands.c:cmd_cron. */

char *cron_incidents(void) { return strdup("[]"); }

char *cron_doctor(void) { return strdup("[]"); }

char *cron_command(void) { return strdup("{}"); }
