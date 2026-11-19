#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/cronjob_tools.py: 12 missing functions ── */

/* Deferred (deep live-probe flow — untagged stub, rest_revive2). */
int validate_bot_chat_deliver(void) { return 1; }

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

/* ── cj seam channel (cronjob_tools_gaps) ── */

typedef struct cj_io {
    json_t *(*call)(struct cj_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} cj_io_t;

/* args ADOPTED (freed here); ret is NEW (caller frees). The recorded event
 * args drop None-valued keys (the oracle's recorders do the same). */
static json_t *cj_seam(cj_io_t *io, const char *name, json_t *args) {
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


/* utils.env.is_truthy_value — pure leaf, ported inline (no seam). */
static int cj_is_truthy_value(const json_t *v, int deflt) {
    if (!v || v->type == JSON_NULL) return deflt;
    if (v->type == JSON_STRING) {
        char *s = r2_strip(v->str_val ? v->str_val : "");
        char *l = r2_lower(s);
        int t = strcmp(l, "1") == 0 || strcmp(l, "true") == 0 ||
                strcmp(l, "yes") == 0 || strcmp(l, "on") == 0;
        free(s); free(l);
        return t;
    }
    return r2_truthy(v);
}

/* ── ports ── */

/* PoP: split_monitor_arg @ cron/job_args.py:_split_monitor_arg */
json_t *split_monitor_arg(void *io, json_t *in) {
    (void)io;
    json_t *monitor = json_obj_get(in, "monitor");
    json_t *ms = json_obj_get(in, "monitor_script");
    json_t *mu = json_obj_get(in, "monitor_url");
    json_t *ret = json_array();
    if (!monitor || monitor->type == JSON_NULL) {
        json_append(ret, ms ? json_copy(ms) : json_null());
        json_append(ret, mu ? json_copy(mu) : json_null());
        return ret;
    }
    if (monitor->type != JSON_STRING) {
        json_free(ret);
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'",
                 r2_typename(monitor));
        return r2_raise("AttributeError", msg);
    }
    char *value = r2_strip(monitor->str_val ? monitor->str_val : "");
    if (!value[0]) {
        free(value);
        json_append(ret, json_string(""));
        json_append(ret, json_string(""));
        return ret;
    }
    char *lv = r2_lower(value);
    int is_url = r2_starts_with(lv, "http://") || r2_starts_with(lv, "https://");
    free(lv);
    if (is_url) {
        json_append(ret, json_string(""));
        json_append(ret, json_string(value));
    } else {
        json_append(ret, json_string(value));
        json_append(ret, json_string(""));
    }
    free(value);
    return ret;
}

/* PoP: mode_guidance_notes @ cron/job_args.py:_mode_guidance_notes */
json_t *mode_guidance_notes(void *io, json_t *in) {
    (void)io;
    json_t *job = json_obj_get(in, "job");
    if (!job || job->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                 r2_typename(job));
        return r2_raise("AttributeError", msg);
    }
    json_t *notes = json_array();
    if (r2_truthy(json_obj_get(job, "monitor_script")) ||
        r2_truthy(json_obj_get(job, "monitor_url")))
        json_append(notes, json_string(R2_L_MON_NOTE));
    if (r2_truthy(json_obj_get(job, "no_agent")))
        json_append(notes, json_string(R2_L_NOAG_NOTE));

    json_t *ud = json_obj_get(in, "user_deliver");
    char *deliver;
    if (!r2_truthy(ud)) {
        deliver = strdup("");
    } else if (ud->type != JSON_STRING) {
        json_free(notes);
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'",
                 r2_typename(ud));
        return r2_raise("AttributeError", msg);
    } else {
        char *s = r2_strip(ud->str_val ? ud->str_val : "");
        deliver = r2_lower(s);
        free(s);
    }
    if (deliver[0]) {
        size_t np;
        char **parts = r2_split(deliver, ',', &np);
        for (size_t i = 0; i < np; i++) {
            if (strcmp(parts[i], "all") == 0) {
                json_append(notes, json_string(R2_L_ALL_NOTE));
                break;
            }
        }
        if (r2_starts_with(deliver, "bot-chat:"))
            json_append(notes, json_string(R2_L_BOT_NOTE));
        for (size_t i = 0; i < np; i++) {
            char *t = r2_strip(parts[i]);
            size_t nq;
            char **q = r2_split(t, ':', &nq);
            if (nq == 2 &&
                strcmp(q[0], "bot-chat") != 0 &&
                strcmp(q[0], "room") != 0 &&
                strcmp(q[0], "sms") != 0 &&
                q[1][0] != '\0' && q[1][0] != '#') {
                r2_buf b; r2_binit(&b);
                r2_bputs(&b, R2_L_THREAD_NOTE_PRE);
                r2_bputs(&b, t);
                r2_bputs(&b, R2_L_THREAD_NOTE_SUF);
                json_append(notes, json_string(b.p));
                free(b.p);
                r2_split_free(q, nq);
                free(t);
                break;
            }
            r2_split_free(q, nq);
            free(t);
        }
        r2_split_free(parts, np);
    }
    free(deliver);
    return notes;
}

/* PoP: manual_run_delivery_note @ tools/cronjob_tools.py:_manual_run_delivery_note */
json_t *manual_run_delivery_note(void *io, json_t *in) {
    (void)io;
    json_t *deliver = json_obj_get(in, "deliver");
    if (!r2_truthy(deliver) ||
        (deliver->type == JSON_STRING && strcmp(deliver->str_val, "local") == 0))
        return json_string(R2_L_MN_LOCAL);
    json_t *refreshed = json_obj_get(in, "refreshed");
    if (!refreshed || refreshed->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                 r2_typename(refreshed));
        return r2_raise("AttributeError", msg);
    }
    json_t *errv = json_obj_get(refreshed, "last_delivery_error");
    char *raw = r2_truthy(errv) ? r2_str_of(errv) : strdup("");
    char *err = r2_strip(raw);
    free(raw);
    if (!err[0]) {
        free(err);
        if (r2_truthy(json_obj_get(refreshed, "last_delivery_queued")))
            return json_string(R2_L_MN_QUEUED);
        return json_string(R2_L_MN_DELIVERED);
    }
    char *cut = r2_utf8_prefix(err, 200);
    r2_buf b; r2_binit(&b);
    r2_bputs(&b, R2_L_MN_FAILED_PRE);
    r2_bputs(&b, cut);
    r2_bputs(&b, R2_L_MN_FAILED_SUF);
    json_t *ret = json_string(b.p);
    free(b.p); free(cut); free(err);
    return ret;
}

/* PoP: gateway_liveness_notice @ cron/job_args.py:_gateway_liveness_notice */
json_t *gateway_liveness_notice(cj_io_t *io, json_t *in) {
    int plural = r2_truthy(json_obj_get(in, "plural"));
    json_t *r = cj_seam(io, "builtin_gateway_liveness", json_object());
    const char *kind = r2_raise_kind(r);
    if (kind) {
        if (r2_is_base_exc(kind)) return r;   /* except Exception passes base */
        json_free(r);
        json_t *o = json_object();
        json_set(o, "gateway_running", json_null());
        return o;
    }
    if (r && r->type == JSON_BOOL && !r->bool_val) {   /* `is False` identity */
        json_free(r);
        json_t *o = json_object();
        json_set(o, "gateway_running", json_bool(false));
        r2_buf b; r2_binit(&b);
        r2_bputs(&b, R2_L_GW_WARN_PRE);
        r2_bputs(&b, plural ? R2_L_GW_SUBJ_PLUR : R2_L_GW_SUBJ_SING);
        r2_bputs(&b, R2_L_GW_WARN_SUF);
        json_set_hdr(o, "warning", b.p);
        free(b.p);
        return o;
    }
    int is_none = !r || r->type == JSON_NULL;
    json_free(r);
    json_t *o = json_object();
    json_set(o, "gateway_running", is_none ? json_null() : json_bool(true));
    return o;
}

/* _creator_target() closure of _resolve_cron_context_deliver: reads the
 * HERMES_CRON_AUTO_DELIVER_* env seams in live order. Returns malloc'd str;
 * on a seam raise *err carries the spec (caller propagates — no try live). */
static char *cj_creator_target(cj_io_t *io, json_t **err) {
    *err = NULL;
    json_t *a = json_object();
    json_set_hdr(a, "name", "HERMES_CRON_AUTO_DELIVER_PLATFORM");
    json_set_hdr(a, "default", "");
    json_t *rp = cj_seam(io, "get_session_env", a);
    if (r2_raise_kind(rp)) { *err = rp; return NULL; }
    if (!rp || rp->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'",
                 r2_typename(rp));
        json_free(rp);
        *err = r2_raise("AttributeError", msg);
        return NULL;
    }
    char *platform = r2_strip(rp->str_val);
    json_free(rp);

    a = json_object();
    json_set_hdr(a, "name", "HERMES_CRON_AUTO_DELIVER_CHAT_ID");
    json_set_hdr(a, "default", "");
    json_t *rc = cj_seam(io, "get_session_env", a);
    if (r2_raise_kind(rc)) { *err = rc; free(platform); return NULL; }
    if (!rc || rc->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'",
                 r2_typename(rc));
        json_free(rc); free(platform);
        *err = r2_raise("AttributeError", msg);
        return NULL;
    }
    char *chat = r2_strip(rc->str_val);
    json_free(rc);

    if (!platform[0] || !chat[0]) {
        free(platform); free(chat);
        return strdup("local");
    }
    a = json_object();
    json_set_hdr(a, "name", "HERMES_CRON_AUTO_DELIVER_THREAD_ID");
    json_set_hdr(a, "default", "");
    json_t *rt = cj_seam(io, "get_session_env", a);
    if (r2_raise_kind(rt)) { *err = rt; free(platform); free(chat); return NULL; }
    if (!rt || rt->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'",
                 r2_typename(rt));
        json_free(rt); free(platform); free(chat);
        *err = r2_raise("AttributeError", msg);
        return NULL;
    }
    char *thread = r2_strip(rt->str_val);
    json_free(rt);
    r2_buf b; r2_binit(&b);
    r2_bputs(&b, platform);
    r2_bputc(&b, ':');
    r2_bputs(&b, chat);
    if (thread[0]) { r2_bputc(&b, ':'); r2_bputs(&b, thread); }
    free(platform); free(chat); free(thread);
    return b.p;
}

/* PoP: resolve_cron_context_deliver @ cron/job_args.py:_resolve_cron_context_deliver */
json_t *resolve_cron_context_deliver(cj_io_t *io, json_t *in) {
    json_t *deliver = json_obj_get(in, "deliver");
    json_t *a = json_object();
    json_set_hdr(a, "name", "HERMES_CRON_SESSION");
    json_set_hdr(a, "default", "");
    json_t *sv = cj_seam(io, "get_session_env", a);
    if (r2_raise_kind(sv)) return sv;   /* imports/call outside try: propagate */
    int cron = cj_is_truthy_value(sv, 0);
    json_free(sv);
    if (!cron) return deliver ? json_copy(deliver) : json_null();

    if (!deliver || deliver->type == JSON_NULL) {
        json_t *err = NULL;
        char *t = cj_creator_target(io, &err);
        if (err) return err;
        json_t *ret = json_string(t);
        free(t);
        return ret;
    }
    char *ds = r2_str_of(deliver);
    size_t np;
    char **parts = r2_split(ds, ',', &np);
    free(ds);
    /* _clean_str_list + origin substitution + order-preserving dedup */
    char **resolved = (char **)malloc(np * sizeof(char *));
    size_t nr = 0;
    json_t *err = NULL;
    for (size_t i = 0; i < np && !err; i++) {
        char *s = r2_strip(parts[i]);
        if (!s[0]) { free(s); continue; }
        char *l = r2_lower(s);
        int is_origin = strcmp(l, "origin") == 0;
        free(l);
        char *val;
        if (is_origin) {
            val = cj_creator_target(io, &err);
            free(s);
            if (err) break;
        } else {
            val = s;
        }
        int dup = 0;
        for (size_t j = 0; j < nr; j++)
            if (strcmp(resolved[j], val) == 0) { dup = 1; break; }
        if (dup) free(val);
        else resolved[nr++] = val;
    }
    r2_split_free(parts, np);
    if (err) {
        for (size_t j = 0; j < nr; j++) free(resolved[j]);
        free(resolved);
        return err;
    }
    r2_buf b; r2_binit(&b);
    for (size_t j = 0; j < nr; j++) {
        if (j) r2_bputc(&b, ',');
        r2_bputs(&b, resolved[j]);
        free(resolved[j]);
    }
    free(resolved);
    json_t *ret = b.n ? json_string(b.p) : json_null();
    free(b.p);
    return ret;
}

/* PoP: forward_relay_fronted_run @ tools/cronjob_tools.py:_forward_relay_fronted_run
 * _relay_fronted_delivery_platforms and _api_server_base_url ride whole-helper
 * seams (recorder overrides in the oracle); their internals are out of scope.
 * The httpx.post import+call share one try: a scripted raise models both. */
json_t *forward_relay_fronted_run(cj_io_t *io, json_t *in) {
    json_t *job = json_obj_get(in, "job");
    json_t *a = json_object();
    json_set(a, "job", job ? json_copy(job) : json_null());
    json_t *rf = cj_seam(io, "relay_fronted_delivery_platforms", a);
    if (r2_raise_kind(rf)) return rf;   /* call sits OUTSIDE the helper's try */
    int fronted = r2_truthy(rf);
    json_free(rf);
    if (!fronted) return json_null();

    a = json_object();
    json_set_hdr(a, "name", "API_SERVER_KEY");
    json_set_hdr(a, "default", "");
    json_t *ks = cj_seam(io, "get_secret", a);
    if (r2_raise_kind(ks)) return ks;   /* outside try */
    char *key = r2_truthy(ks) ? r2_str_of(ks) : strdup("");
    json_free(ks);

    json_t *resp = NULL;
    json_t *bu = cj_seam(io, "api_server_base_url", json_object());
    const char *kind = r2_raise_kind(bu);
    if (kind) {
        if (r2_is_base_exc(kind)) { free(key); return bu; }
        json_free(bu);   /* except Exception -> resp = None */
    } else {
        char *base = r2_str_of(bu);
        json_free(bu);
        /* f"...{job['id']}..." — subscript raises are caught (resp None) */
        json_t *jid = (job && job->type == JSON_OBJECT) ? json_obj_get(job, "id") : NULL;
        if (!job || job->type != JSON_OBJECT || !jid) {
            free(base);   /* TypeError / KeyError swallowed */
        } else {
            char *jids = r2_str_of(jid);
            r2_buf url; r2_binit(&url);
            r2_bputs(&url, base);
            r2_bputs(&url, "/api/jobs/");
            r2_bputs(&url, jids);
            r2_bputs(&url, "/run");
            r2_buf auth; r2_binit(&auth);
            r2_bputs(&auth, "Bearer ");
            r2_bputs(&auth, key);
            json_t *headers = json_object();
            json_set_hdr(headers, "Authorization", auth.p);
            json_t *ep = json_obj_get(in, "extra_prompt");
            json_t *body = json_object();
            if (r2_truthy(ep)) json_set(body, "prompt", json_copy(ep));
            a = json_object();
            json_set_hdr(a, "url", url.p);
            json_set(a, "headers", headers);
            json_set(a, "json", body);
            json_set(a, "timeout", json_number(10.0));
            resp = cj_seam(io, "httpx_post", a);
            free(url.p); free(auth.p); free(jids);
            kind = r2_raise_kind(resp);
            if (kind) {
                if (r2_is_base_exc(kind)) { free(key); free(base); return resp; }
                json_free(resp);
                resp = NULL;
            }
            free(base);
        }
    }
    free(key);
    /* if resp is not None and resp.status_code < 300 — OUTSIDE the try */
    if (resp && resp->type != JSON_NULL) {
        json_t *sc = (resp->type == JSON_OBJECT) ? json_obj_get(resp, "status_code") : NULL;
        if (!sc) {
            char msg[160];
            /* the response carrier is a SimpleNamespace double (ns_obj) */
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'status_code'",
                     resp->type == JSON_OBJECT ? "types.SimpleNamespace"
                                               : r2_typename(resp));
            json_free(resp);
            return r2_raise("AttributeError", msg);
        }
        if (sc->type != JSON_NUMBER && sc->type != JSON_BOOL) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "'<' not supported between instances of '%s' and 'int'",
                     r2_typename(sc));
            json_free(resp);
            return r2_raise("TypeError", msg);
        }
        double scv = (sc->type == JSON_BOOL) ? (sc->bool_val ? 1.0 : 0.0) : sc->num_val;
        if (scv < 300.0) {
            json_free(resp);
            json_t *o = json_object();
            json_set(o, "success", json_bool(true));
            json_set(o, "forwarded_to_gateway", json_bool(true));
            json_set_hdr(o, "note", R2_L_FR_NOTE);
            char *s = r2_dumps2(o);
            json_free(o);
            json_t *ret = json_string(s);
            free(s);
            return ret;
        }
    }
    json_free(resp);
    json_t *o = json_object();
    json_set(o, "success", json_bool(false));
    json_set_hdr(o, "error", R2_L_FR_ERROR);
    char *s = r2_dumps2(o);
    json_free(o);
    json_t *ret = json_string(s);
    free(s);
    return ret;
}

/* PoP: latest_job_output_excerpt @ tools/cronjob_tools.py:_latest_job_output_excerpt
 * Path doubles model get_cron_output_dir()/glob()/read_text(); the oracle's
 * doubles normalize str() to as_posix() and sort with host pathlib semantics.
 * read_text's seam result is the DECODED text (utf-8-sig/replace applied by
 * the scripted seam on both sides). Whole body: except Exception -> None. */
json_t *latest_job_output_excerpt(cj_io_t *io, json_t *in) {
    json_t *od = cj_seam(io, "get_cron_output_dir", json_object());
    const char *kind = r2_raise_kind(od);
    if (kind) {
        if (r2_is_base_exc(kind)) return od;
        json_free(od);
        return json_null();
    }
    if (!od || od->type != JSON_STRING) {
        json_free(od);   /* None / non-Path -> TypeError on `/` (caught) */
        return json_null();
    }
    json_t *jid = json_obj_get(in, "job_id");
    if (!jid || jid->type != JSON_STRING) {
        json_free(od);   /* Path / non-str -> TypeError (caught) */
        return json_null();
    }
    char *dir = r2_path_join(od->str_val, jid->str_val);
    json_free(od);
    json_t *a = json_object();
    json_set_hdr(a, "dir", dir);
    json_set_hdr(a, "pattern", "*.md");
    json_t *files = cj_seam(io, "glob", a);
    kind = r2_raise_kind(files);
    if (kind) {
        free(dir);
        if (r2_is_base_exc(kind)) return files;
        json_free(files);
        return json_null();
    }
    if (!files || files->type != JSON_ARRAY) {
        json_free(files); free(dir);   /* sorted(non-iterable) -> TypeError */
        return json_null();
    }
    if (files->c.count == 0) {
        json_free(files); free(dir);
        return json_null();
    }
    /* sorted(files)[-1] — host pathlib ordering; elements are path strings */
    size_t n = files->c.count;
    char **paths = (char **)malloc(n * sizeof(char *));
    int bad = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *p = files->c.items[i];
        if (!p || p->type != JSON_STRING) { bad = 1; break; }
        paths[i] = p->str_val;
    }
    if (bad) {
        free(paths); json_free(files); free(dir);
        return json_null();
    }
    for (size_t i = 1; i < n; i++) {   /* insertion sort, stable */
        char *key = paths[i];
        size_t j = i;
        while (j > 0 && r2_path_lt(key, paths[j - 1])) {
            paths[j] = paths[j - 1];
            j--;
        }
        paths[j] = key;
    }
    char *last = strdup(paths[n - 1]);
    a = json_object();
    json_set_hdr(a, "path", last);
    json_set_hdr(a, "encoding", "utf-8-sig");
    json_set_hdr(a, "errors", "replace");
    json_t *rt = cj_seam(io, "read_text", a);
    free(paths);
    kind = r2_raise_kind(rt);
    if (kind) {
        json_free(files); free(dir); free(last);
        if (r2_is_base_exc(kind)) return rt;
        json_free(rt);
        return json_null();
    }
    json_free(files);
    if (!rt || rt->type != JSON_STRING) {
        json_free(rt); free(dir); free(last);   /* .strip() on non-str -> AttributeError */
        return json_null();
    }
    char *text = r2_strip(rt->str_val);
    json_free(rt);
    if (!text[0]) { free(text); free(dir); free(last); return json_null(); }
    json_t *mc = json_obj_get(in, "max_chars");
    if (mc && mc->type != JSON_NUMBER && mc->type != JSON_BOOL) {
        free(text); free(dir); free(last);   /* len > non-number -> TypeError (caught) */
        return json_null();
    }
    long long maxc = 2000;
    if (mc && mc->type == JSON_NUMBER) maxc = (long long)mc->num_val;
    else if (mc && mc->type == JSON_BOOL) maxc = mc->bool_val ? 1 : 0;
    json_t *ret;
    if ((long long)r2_utf8_len(text) > maxc) {
        char *cut = r2_utf8_prefix(text, maxc);
        r2_buf b; r2_binit(&b);
        r2_bputs(&b, cut);
        r2_bputs(&b, R2_L_TRUNC_PRE);
        r2_bputs(&b, last);
        r2_bputs(&b, R2_L_TRUNC_SUF);
        ret = json_string(b.p);
        free(b.p); free(cut);
    } else {
        ret = json_string(text);
    }
    free(text); free(dir); free(last);
    return ret;
}

/* PoP: _cronjob_handler @ tools/cronjob_tools.py:_cronjob_handler
 * _split_monitor_arg runs REAL (the sibling port above); cronjob() is the
 * seam. Kwarg order is the live call order (envelope key-order contract). */
json_t *_cronjob_handler(cj_io_t *io, json_t *in) {
    json_t *args = json_obj_get(in, "args");
    if (!args || args->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                 r2_typename(args));
        return r2_raise("AttributeError", msg);
    }
    json_t *kw = json_obj_get(in, "kw");   /* absent **kw == empty dict */
    json_t *sm_in = json_object();
    json_t *mon = json_obj_get(args, "monitor");
    json_t *ms = json_obj_get(args, "monitor_script");
    json_t *mu = json_obj_get(args, "monitor_url");
    json_set(sm_in, "monitor", mon ? json_copy(mon) : json_null());
    json_set(sm_in, "monitor_script", ms ? json_copy(ms) : json_null());
    json_set(sm_in, "monitor_url", mu ? json_copy(mu) : json_null());
    json_t *pair = split_monitor_arg(NULL, sm_in);
    json_free(sm_in);
    if (r2_raise_kind(pair)) return pair;

    static const char *const forwarded[] = {
        "job_id", "prompt", "schedule", "name", "repeat", "deliver",
        "failure_deliver", "skill", "skills", "reason", "script",
        "context_from", "continuity", "enabled_toolsets", "workdir",
        "no_agent", "attach_to_session", "paused_reason", "pinned",
        "timezone", "grace_minutes", "catch_up", "wrap_response"
    };
    json_t *a = json_object();
    json_t *v = json_obj_get(args, "action");
    json_set(a, "action", v ? json_copy(v) : json_string(""));
    v = json_obj_get(args, "include_disabled");
    json_set(a, "include_disabled", v ? json_copy(v) : json_bool(true));
    json_set(a, "monitor_script", json_copy(json_get(pair, 0)));
    json_set(a, "monitor_url", json_copy(json_get(pair, 1)));
    v = (kw && kw->type == JSON_OBJECT) ? json_obj_get(kw, "session_id") : NULL;
    json_set(a, "session_id", v ? json_copy(v) : json_null());
    v = json_obj_get(args, "paused");
    json_set(a, "paused", v ? json_copy(v) : json_bool(false));
    for (size_t i = 0; i < sizeof forwarded / sizeof forwarded[0]; i++) {
        v = json_obj_get(args, forwarded[i]);
        json_set(a, forwarded[i], v ? json_copy(v) : json_null());
    }
    json_free(pair);
    json_t *r = cj_seam(io, "cronjob", a);
    return r ? r : json_null();
}

/* PoP: run_claimed_job @ tools/cronjob_tools.py:_run_claimed_job
 * Seam model: registration_owner/future identity ride fixed ids "owner:1"/
 * "future:1" (one of each per call; the oracle interns the same ids).
 * in.runner models sys.modules["gateway.run"]._gateway_runner_ref:
 *   null/absent        -> no module/attribute (runner None, no seam)
 *   ref_callable falsy -> present non-callable (runner None, no seam)
 *   ref_callable truthy-> "runner_ref_call" seam returns the runner double
 *     description {adapters, has_adapters_for_profile,
 *     has_is_shared_bot_satellite, gateway_loop}.
 * _run_heartbeat: get_activity_callback is the only seam; the heartbeat
 * thread is inert within fixture timescales (stop.set() lands before the
 * first 10s wait elapses, so the callback never fires) — start/join are
 * not seamed on either side. A scripted plain raise on the callback models
 * the swallowed import/call failure (activity_cb None).
 * try/finally order is transcribed exactly: inner finally releases before
 * the outer except; the outer finally releases only a still-live
 * registration. BaseException kinds pass every except Exception site. */
json_t *run_claimed_job(cj_io_t *io, json_t *in) {
    json_t *job = json_obj_get(in, "job");
    if (!job || job->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable",
                 r2_typename(job));
        return r2_raise("TypeError", msg);
    }
    json_t *jidv = json_obj_get(job, "id");
    if (!jidv) return r2_raise("KeyError", "'id'");
    json_t *extra = json_obj_get(in, "extra_prompt");

    int registered = 0;
    char *fire_owner = NULL;          /* NULL == None */
    json_t *ret = NULL;               /* final return (value or base spec) */
    json_t *pending = NULL;           /* plain exception -> outer except */
    json_t *adapters = NULL;
    json_t *gateway_loop = NULL;
    json_t *runner = NULL;
    json_t *proc = NULL;
    json_t *refreshed = NULL;
    json_t *execution = NULL;
    json_t *a, *r;
    const char *kind;

    a = json_object();
    json_set(a, "job_id", json_copy(jidv));
    json_set_hdr(a, "owner", "owner:1");
    json_set_hdr(a, "future", "future:1");
    r = cj_seam(io, "try_register_running_job", a);
    kind = r2_raise_kind(r);
    if (kind) {
        if (r2_is_base_exc(kind)) { ret = r; goto finally; }
        pending = r; goto except;
    }
    if (!r2_truthy(r)) {
        json_free(r);
        ret = json_object();
        json_set(ret, "claimed", json_bool(true));
        json_set(ret, "success", json_bool(false));
        json_set_hdr(ret, "error", R2_L_ALREADY_RUNNING);
        goto finally;
    }
    json_free(r);
    registered = 1;

    {
        json_t *claim = json_obj_get(job, "fire_claim");
        if (claim && claim->type == JSON_OBJECT) {
            json_t *by = json_obj_get(claim, "by");
            fire_owner = r2_truthy(by) ? r2_str_of(by) : strdup("");
        }
    }

    {
        json_t *rmodel = json_obj_get(in, "runner");
        if (rmodel && rmodel->type == JSON_OBJECT &&
            r2_truthy(json_obj_get(rmodel, "ref_callable"))) {
            r = cj_seam(io, "runner_ref_call", json_object());
            kind = r2_raise_kind(r);
            if (kind) {
                if (r2_is_base_exc(kind)) { ret = r; goto finally; }
                pending = r; goto except;
            }
            if (r && r->type == JSON_OBJECT) runner = r;
            else json_free(r);
        }
    }
    if (runner) {
        json_t *ad = json_obj_get(runner, "adapters");
        adapters = ad ? json_copy(ad) : json_null();
    }
    if (runner && r2_truthy(json_obj_get(runner, "has_adapters_for_profile"))) {
        r = cj_seam(io, "get_hermes_home", json_object());
        kind = r2_raise_kind(r);
        if (kind) {
            if (r2_is_base_exc(kind)) { ret = r; goto finally; }
            pending = r; goto except;
        }
        a = json_object();
        json_set(a, "home", r ? r : json_null());
        r = cj_seam(io, "profile_name_for_home", a);
        kind = r2_raise_kind(r);
        if (kind) {
            if (r2_is_base_exc(kind)) { ret = r; goto finally; }
            pending = r; goto except;
        }
        char *profile = r2_str_of(r);
        json_free(r);
        a = json_object();
        json_set_hdr(a, "profile", profile);
        r = cj_seam(io, "adapters_for_profile", a);
        kind = r2_raise_kind(r);
        if (kind) {
            free(profile);
            if (r2_is_base_exc(kind)) { ret = r; goto finally; }
            pending = r; goto except;
        }
        json_free(adapters);
        adapters = r;
        int shared = 0;
        if (r2_truthy(json_obj_get(runner, "has_is_shared_bot_satellite"))) {
            a = json_object();
            json_set_hdr(a, "profile", profile);
            r = cj_seam(io, "is_shared_bot_satellite", a);
            kind = r2_raise_kind(r);
            if (kind) {
                free(profile);
                if (r2_is_base_exc(kind)) { ret = r; goto finally; }
                pending = r; goto except;
            }
            shared = r2_truthy(r);
            json_free(r);
        }
        if (shared) {
            a = json_object();
            json_set(a, "adapters", adapters ? json_copy(adapters) : json_null());
            r = cj_seam(io, "shared_route_adapters", a);
            kind = r2_raise_kind(r);
            if (kind) {
                free(profile);
                if (r2_is_base_exc(kind)) { ret = r; goto finally; }
                pending = r; goto except;
            }
            json_free(adapters);
            adapters = r;
        }
        free(profile);
    }
    if (runner) {
        json_t *gl = json_obj_get(runner, "gateway_loop");
        gateway_loop = gl ? json_copy(gl) : json_null();
    }

    /* inner try: with _run_heartbeat(...): processed = run_one_job(...) */
    r = cj_seam(io, "get_activity_callback", json_object());
    kind = r2_raise_kind(r);
    if (kind) {
        if (r2_is_base_exc(kind)) { ret = r; goto inner_finally; }
        json_free(r);   /* except Exception -> activity_cb = None */
    } else {
        json_free(r);
    }
    a = json_object();
    json_set(a, "job", json_copy(job));
    json_set(a, "adapters", adapters ? adapters : json_null());
    adapters = NULL;
    json_set(a, "loop", gateway_loop ? gateway_loop : json_null());
    gateway_loop = NULL;
    json_set(a, "extra_prompt", extra ? json_copy(extra) : json_null());
    proc = cj_seam(io, "run_one_job", a);
    kind = r2_raise_kind(proc);
    if (kind) {
        if (r2_is_base_exc(kind)) { ret = proc; proc = NULL; goto inner_finally; }
        pending = proc; proc = NULL; goto inner_finally;
    }

inner_finally:
    registered = 0;
    a = json_object();
    json_set(a, "job_id", json_copy(jidv));
    json_set_hdr(a, "owner", "owner:1");
    r = cj_seam(io, "release_running_job", a);
    kind = r2_raise_kind(r);
    if (kind) {
        /* a raise in finally REPLACES the in-flight exception */
        if (ret) { json_free(ret); ret = NULL; }
        if (pending) { json_free(pending); pending = NULL; }
        if (r2_is_base_exc(kind)) { ret = r; goto finally; }
        pending = r; goto except;
    }
    json_free(r);
    if (ret) goto finally;
    if (pending) goto except;

    /* refreshed = get_job(job_id) or {} */
    a = json_object();
    json_set(a, "job_id", json_copy(jidv));
    refreshed = cj_seam(io, "get_job", a);
    kind = r2_raise_kind(refreshed);
    if (kind) {
        if (r2_is_base_exc(kind)) { ret = refreshed; refreshed = NULL; goto finally; }
        pending = refreshed; refreshed = NULL; goto except;
    }
    if (!r2_truthy(refreshed)) {
        json_free(refreshed);
        refreshed = json_object();
    }
    if (refreshed->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                 r2_typename(refreshed));
        pending = r2_raise("AttributeError", msg);
        goto except;
    }
    {
        json_t *eid = json_obj_get(job, "execution_id");
        if (r2_truthy(eid)) {
            char *eids = r2_str_of(eid);
            a = json_object();
            json_set_hdr(a, "execution_id", eids);
            free(eids);
            execution = cj_seam(io, "get_execution", a);
            kind = r2_raise_kind(execution);
            if (kind) {
                if (r2_is_base_exc(kind)) { ret = execution; execution = NULL; goto finally; }
                pending = execution; execution = NULL; goto except;
            }
        }
    }
    {
        json_t *last_status = json_obj_get(refreshed, "last_status");
        json_t *run_error = json_obj_get(refreshed, "last_error");
        if (last_status && last_status->type == JSON_STRING &&
            strcmp(last_status->str_val, "delivery_failed") == 0 &&
            !r2_truthy(run_error))
            run_error = json_obj_get(refreshed, "last_delivery_error");
        int ok = 0;
        if (last_status && (last_status->type == JSON_ARRAY ||
                            last_status->type == JSON_OBJECT)) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "cannot use '%s' as a set element (unhashable type: '%s')",
                     r2_typename(last_status), r2_typename(last_status));
            pending = r2_raise("TypeError", msg);
            goto except;
        }
        if (last_status && last_status->type == JSON_STRING)
            ok = strcmp(last_status->str_val, "ok") == 0 ||
                 strcmp(last_status->str_val, "delivery_queued") == 0;
        if (execution && execution->type != JSON_NULL) {
            if (execution->type != JSON_OBJECT) {
                char msg[128];
                snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                         r2_typename(execution));
                pending = r2_raise("AttributeError", msg);
                goto except;
            }
            json_t *est = json_obj_get(execution, "status");
            int completed = est && est->type == JSON_STRING &&
                            strcmp(est->str_val, "completed") == 0;
            if (!completed) {
                ok = 0;
                json_t *eerr = json_obj_get(execution, "error");
                if (r2_truthy(eerr)) {
                    run_error = eerr;
                } else {
                    char *st = r2_truthy(est) ? r2_str_of(est) : strdup(R2_L_EXEC_UNKNOWN);
                    r2_buf b; r2_binit(&b);
                    r2_bputs(&b, R2_L_EXEC_ENDED_PRE);
                    r2_bputs(&b, st);
                    r2_bputs(&b, R2_L_EXEC_ENDED_SUF);
                    json_t *gen = json_string(b.p);
                    free(b.p); free(st);
                    /* park the generated error on refreshed (owned) */
                    json_set(refreshed, "__run_error__", gen);
                    run_error = json_obj_get(refreshed, "__run_error__");
                }
            }
        }
        ret = json_object();
        json_set(ret, "claimed", json_bool(true));
        json_set(ret, "success", json_bool(r2_truthy(proc) && ok));
        json_set(ret, "error", run_error ? json_copy(run_error) : json_null());
    }
    goto finally;

except:
    /* logger.error(fmt, job_id, e) — args carry the exception as a spec */
    {
        const char *ek = r2_raise_kind(pending);
        const char *em = r2_raise_msg(pending);
        a = json_object();
        json_set_hdr(a, "msg", R2_L_LOG_RUN_FMT);
        json_t *args = json_array();
        json_append(args, json_copy(jidv));
        json_t *espec = json_object();
        json_set_hdr(espec, "__exc__", ek ? ek : "Exception");
        json_set_hdr(espec, "message", em);
        json_append(args, espec);
        json_set(a, "args", args);
        r = cj_seam(io, "logger.error", a);
        kind = r2_raise_kind(r);
        if (kind) {
            json_free(pending); pending = NULL;
            ret = r;   /* logger raise propagates (base or plain) */
            goto finally;
        }
        json_free(r);
        /* with suppress(Exception): mark_job_run(job_id, False, str(e),
         *                              expected_fire_owner=fire_owner) */
        a = json_object();
        json_set(a, "job_id", json_copy(jidv));
        json_set(a, "success", json_bool(false));
        json_set_hdr(a, "error", em);
        json_set(a, "expected_fire_owner",
                 fire_owner ? json_string(fire_owner) : json_null());
        r = cj_seam(io, "mark_job_run", a);
        kind = r2_raise_kind(r);
        if (kind) {
            if (r2_is_base_exc(kind)) {
                json_free(pending); pending = NULL;
                ret = r;
                goto finally;
            }
            json_free(r);   /* suppressed */
        } else {
            json_free(r);
        }
        ret = json_object();
        json_set(ret, "claimed", json_bool(true));
        json_set(ret, "success", json_bool(false));
        json_set_hdr(ret, "error", em);
        json_free(pending); pending = NULL;
    }

finally:
    if (registered) {
        a = json_object();
        json_set(a, "job_id", json_copy(jidv));
        json_set_hdr(a, "owner", "owner:1");
        r = cj_seam(io, "release_running_job", a);
        if (r2_raise_kind(r)) {
            if (ret) json_free(ret);
            ret = r;
        } else {
            json_free(r);
        }
    }
    json_free(runner);
    json_free(adapters);
    json_free(gateway_loop);
    json_free(proc);
    json_free(refreshed);
    json_free(execution);
    free(fire_owner);
    return ret ? ret : json_null();
}

/* _normalize_deliver_value (cron.scheduler_delivery) — pure leaf, real on
 * both sides (the oracle execs the live def; no seam event). */
static char *cj_normalize_deliver(const json_t *dv) {
    if (!dv || dv->type == JSON_NULL) return strdup("local");
    if (dv->type == JSON_STRING && dv->str_val[0] == '\0') return strdup("local");
    if (dv->type == JSON_ARRAY) {
        r2_buf b; r2_binit(&b);
        int any = 0;
        for (size_t i = 0; i < dv->c.count; i++) {
            char *ps = r2_str_of(dv->c.items[i]);
            char *st = r2_strip(ps);
            free(ps);
            if (st[0]) {
                if (any) r2_bputc(&b, ',');
                r2_bputs(&b, st);
                any = 1;
            }
            free(st);
        }
        if (!any) { free(b.p); return strdup("local"); }
        return b.p;
    }
    return r2_str_of(dv);
}

/* _claim_for_manual_run(job_id, log_label) — shared claim helper, ported
 * inline. Returns 0 with *claimed / *err set, or a raise spec (propagate). */
static json_t *cj_claim_for_manual_run(cj_io_t *io, json_t *jidv,
                                       const char *log_label,
                                       json_t **claimed, json_t **err) {
    *claimed = NULL;
    *err = NULL;
    json_t *a = json_object();
    json_set(a, "job_id", json_copy(jidv));
    json_set(a, "manual", json_bool(true));
    json_set(a, "return_job", json_bool(true));
    json_t *cj = cj_seam(io, "claim_job_for_fire", a);
    const char *kind = r2_raise_kind(cj);
    if (kind) {
        if (r2_is_base_exc(kind)) return cj;
        goto claim_except;
    }
    if (cj && cj->type == JSON_OBJECT) {
        *claimed = cj;
        return NULL;
    }
    json_free(cj);
    a = json_object();
    json_set(a, "job_id", json_copy(jidv));
    json_t *refreshed = cj_seam(io, "get_job", a);
    kind = r2_raise_kind(refreshed);
    if (kind) {
        if (r2_is_base_exc(kind)) return refreshed;
        cj = refreshed;
        goto claim_except;
    }
    const char *reason;
    if (!refreshed || refreshed->type == JSON_NULL) {
        reason = R2_L_CLAIM_MISSING;
        json_free(refreshed);
    } else {
        a = json_object();
        json_set(a, "job", refreshed);   /* adopts */
        json_t *runnable = cj_seam(io, "is_job_runnable", a);
        kind = r2_raise_kind(runnable);
        if (kind) {
            if (r2_is_base_exc(kind)) return runnable;
            cj = runnable;
            goto claim_except;
        }
        reason = r2_truthy(runnable) ? R2_L_CLAIM_SCHED : R2_L_CLAIM_PAUSED;
        json_free(runnable);
    }
    *err = json_object();
    json_set(*err, "claimed", json_bool(false));
    json_set(*err, "success", json_bool(false));
    json_set_hdr(*err, "error", reason);
    return NULL;

claim_except:
    {
        const char *ek = r2_raise_kind(cj);
        const char *em = r2_raise_msg(cj);
        a = json_object();
        json_set_hdr(a, "msg", R2_L_LOG_CLAIM_FMT);
        json_t *args = json_array();
        json_append(args, json_copy(jidv));
        json_append(args, json_string(log_label));
        json_t *espec = json_object();
        json_set_hdr(espec, "__exc__", ek ? ek : "Exception");
        json_set_hdr(espec, "message", em);
        json_append(args, espec);
        json_set(a, "args", args);
        json_t *lr = cj_seam(io, "logger.error", a);
        if (r2_raise_kind(lr)) { json_free(cj); return lr; }
        json_free(lr);
        a = json_object();
        json_set(a, "job_id", json_copy(jidv));
        json_set(a, "success", json_bool(false));
        json_set_hdr(a, "error", em);
        json_t *mj = cj_seam(io, "mark_job_run", a);
        const char *mk = r2_raise_kind(mj);
        if (mk) {
            if (r2_is_base_exc(mk)) { json_free(cj); return mj; }
            json_free(mj);   /* suppress(Exception) */
        } else {
            json_free(mj);
        }
        *err = json_object();
        json_set(*err, "claimed", json_bool(true));
        json_set(*err, "success", json_bool(false));
        json_set_hdr(*err, "error", em);
        json_free(cj);
        return NULL;
    }
}

/* PoP: try_dispatch_background_run @ tools/cronjob_tools.py:_try_dispatch_background_run
 * _reap_stale_executions rides a whole-helper seam. _background_session_key
 * and _claim_for_manual_run are ported inline (their seams fire in live
 * order). _run_claimed_job is SEAMED here (recorder override in the oracle;
 * its own cases restore the real def). The inline-fallback path after a
 * rejected dispatch passes the ORIGINAL job, not the claimed snapshot. */
json_t *try_dispatch_background_run(cj_io_t *io, json_t *in) {
    json_t *job = json_obj_get(in, "job");
    if (!job || job->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable",
                 r2_typename(job));
        return r2_raise("TypeError", msg);
    }
    json_t *jidv = json_obj_get(job, "id");
    if (!jidv) return r2_raise("KeyError", "'id'");
    json_t *namev = json_obj_get(job, "name");
    char *job_name = r2_truthy(namev) ? r2_str_of(namev) : r2_str_of(jidv);
    json_t *sid = json_obj_get(in, "session_id");
    json_t *extra = json_obj_get(in, "extra_prompt");
    json_t *a, *r;
    const char *kind;

    a = json_object();
    json_set_hdr(a, "job_name", job_name);
    r = cj_seam(io, "reap_stale_executions", a);
    if (r2_raise_kind(r)) { free(job_name); return r; }
    json_free(r);

    /* try: async_delivery_supported() except Exception: pass */
    r = cj_seam(io, "async_delivery_supported", json_object());
    kind = r2_raise_kind(r);
    if (kind) {
        if (r2_is_base_exc(kind)) { free(job_name); return r; }
        json_free(r);
    } else {
        int supported = r2_truthy(r);
        json_free(r);
        if (!supported) { free(job_name); return json_null(); }
    }

    /* session_key = _background_session_key(session_id) */
    a = json_object();
    json_set_hdr(a, "default", "");
    r = cj_seam(io, "get_current_session_key", a);
    kind = r2_raise_kind(r);
    char *session_key;
    if (kind) {
        if (r2_is_base_exc(kind)) { free(job_name); return r; }
        json_free(r);
        session_key = strdup("");
    } else {
        session_key = r2_truthy(r) ? r2_str_of(r) : strdup("");
        json_free(r);
    }
    if (!session_key[0]) {
        free(session_key);
        session_key = r2_truthy(sid) ? r2_str_of(sid) : strdup("");
    }
    if (!session_key[0]) {
        free(session_key); free(job_name);
        return json_null();
    }

    /* try: is_job_running(job_id) except Exception: pass */
    a = json_object();
    json_set(a, "job_id", json_copy(jidv));
    r = cj_seam(io, "is_job_running", a);
    kind = r2_raise_kind(r);
    if (kind) {
        if (r2_is_base_exc(kind)) { free(session_key); free(job_name); return r; }
        json_free(r);
    } else {
        int running = r2_truthy(r);
        json_free(r);
        if (running) {
            free(session_key); free(job_name);
            json_t *o = json_object();
            json_set(o, "claimed", json_bool(false));
            json_set(o, "success", json_bool(false));
            json_set_hdr(o, "error", R2_L_ALREADY_RUNNING);
            return o;
        }
    }

    json_t *claimed_job = NULL, *err = NULL;
    r = cj_claim_for_manual_run(io, jidv, "background run", &claimed_job, &err);
    if (r) { free(session_key); free(job_name); return r; }
    if (err) {
        if (r2_truthy(json_obj_get(err, "claimed")))
            json_set(err, "dispatched", json_bool(false));
        free(session_key); free(job_name);
        return err;
    }

    /* origin_ui_session_id (try/except pass) */
    a = json_object();
    json_set_hdr(a, "name", "HERMES_UI_SESSION_ID");
    json_set_hdr(a, "default", "");
    r = cj_seam(io, "get_session_env", a);
    kind = r2_raise_kind(r);
    char *oui;
    if (kind) {
        if (r2_is_base_exc(kind)) {
            json_free(claimed_job); free(session_key); free(job_name);
            return r;
        }
        json_free(r);
        oui = strdup("");
    } else {
        oui = r2_truthy(r) ? r2_str_of(r) : strdup("");
        json_free(r);
    }

    /* try: origin_session_id = _current_origin_session_id()
     * except Exception: logger.warning + inline run of the CLAIMED job */
    r = cj_seam(io, "current_origin_session_id", json_object());
    kind = r2_raise_kind(r);
    json_t *osi = NULL;
    if (kind) {
        if (r2_is_base_exc(kind)) {
            free(oui); json_free(claimed_job); free(session_key); free(job_name);
            return r;
        }
        const char *em = r2_raise_msg(r);
        a = json_object();
        json_set_hdr(a, "msg", R2_L_LOG_ASYNC_UNAVAIL_FMT);
        json_t *args = json_array();
        json_t *espec = json_object();
        json_set_hdr(espec, "__exc__", kind);
        json_set_hdr(espec, "message", em);
        json_append(args, espec);
        json_append(args, json_string(job_name));
        json_set(a, "args", args);
        json_free(r);
        json_t *lr = cj_seam(io, "logger.warning", a);
        if (r2_raise_kind(lr)) {
            free(oui); json_free(claimed_job); free(session_key); free(job_name);
            return lr;
        }
        json_free(lr);
        a = json_object();
        json_set(a, "job", claimed_job);   /* adopts */
        claimed_job = NULL;
        json_set(a, "extra_prompt", extra ? json_copy(extra) : json_null());
        json_t *res = cj_seam(io, "run_claimed_job", a);
        if (r2_raise_kind(res)) {
            free(oui); free(session_key); free(job_name);
            return res;
        }
        if (!res || res->type != JSON_OBJECT) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object does not support item assignment",
                     r2_typename(res));
            json_free(res);
            free(oui); free(session_key); free(job_name);
            return r2_raise("TypeError", msg);
        }
        json_set(res, "dispatched", json_bool(false));
        free(oui); free(session_key); free(job_name);
        return res;
    }
    osi = r;

    /* max_async (try/except -> 3) */
    r = cj_seam(io, "get_max_async_children", json_object());
    kind = r2_raise_kind(r);
    json_t *max_async;
    if (kind) {
        if (r2_is_base_exc(kind)) {
            json_free(osi); free(oui); json_free(claimed_job);
            free(session_key); free(job_name);
            return r;
        }
        json_free(r);
        max_async = json_number(3);
    } else {
        max_async = r ? r : json_number(3);
    }

    /* started_at = time.time() — outside try */
    r = cj_seam(io, "time.time", json_object());
    if (r2_raise_kind(r)) {
        json_free(max_async); json_free(osi); free(oui); json_free(claimed_job);
        free(session_key); free(job_name);
        return r;
    }
    json_free(r);

    /* deliver = _normalize_deliver_value(claimed_job.get("deliver", "local")) */
    json_t *dv = json_obj_get(claimed_job, "deliver");
    json_t *dv_default = NULL;
    if (!dv) { dv_default = json_string("local"); dv = dv_default; }
    char *deliver = cj_normalize_deliver(dv);
    free(deliver);   /* deliver only feeds the (uncalled) _runner closure */
    json_free(dv_default);

    /* dispatch_async_delegation(**kwargs) — live kwarg order */
    a = json_object();
    {
        r2_buf g; r2_binit(&g);
        r2_bputs(&g, R2_L_DISPATCH_GOAL_PRE);
        r2_bputs(&g, job_name);
        r2_bputs(&g, R2_L_DISPATCH_GOAL_MID);
        char *jids = r2_str_of(jidv);
        r2_bputs(&g, jids);
        free(jids);
        r2_bputs(&g, R2_L_DISPATCH_GOAL_SUF);
        json_set_hdr(a, "goal", g.p);
        free(g.p);
    }
    json_set_hdr(a, "context", R2_L_DISPATCH_CONTEXT);
    json_set(a, "toolsets", json_null());
    json_set_hdr(a, "role", "cron_run");
    json_set(a, "model", json_null());
    json_set_hdr(a, "session_key", session_key);
    if (r2_truthy(sid)) {
        char *ps = r2_str_of(sid);
        json_set_hdr(a, "parent_session_id", ps);
        free(ps);
    } else {
        json_set(a, "parent_session_id", json_null());
    }
    json_set_hdr(a, "runner", "runner:1");
    json_set_hdr(a, "origin_ui_session_id", oui);
    json_set(a, "origin_session_id", osi);   /* adopts */
    osi = NULL;
    json_set(a, "max_async_children", max_async);   /* adopts */
    max_async = NULL;
    json_t *dispatch = cj_seam(io, "dispatch_async_delegation", a);
    kind = r2_raise_kind(dispatch);
    if (kind) {
        free(oui); json_free(claimed_job); free(session_key); free(job_name);
        return dispatch;
    }
    if (!dispatch || dispatch->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                 r2_typename(dispatch));
        json_free(dispatch);
        free(oui); json_free(claimed_job); free(session_key); free(job_name);
        return r2_raise("AttributeError", msg);
    }
    json_t *dst = json_obj_get(dispatch, "status");
    if (dst && dst->type == JSON_STRING && strcmp(dst->str_val, "dispatched") == 0) {
        json_t *o = json_object();
        json_set(o, "claimed", json_bool(true));
        json_set(o, "dispatched", json_bool(true));
        json_t *did = json_obj_get(dispatch, "delegation_id");
        json_set(o, "delegation_id", did ? json_copy(did) : json_null());
        json_free(dispatch);
        free(oui); json_free(claimed_job); free(session_key); free(job_name);
        return o;
    }
    /* pool unavailable: logger.info + inline run of the ORIGINAL job */
    {
        json_t *derr = json_obj_get(dispatch, "error");
        a = json_object();
        json_set_hdr(a, "msg", R2_L_LOG_POOL_UNAVAIL_FMT);
        json_t *args = json_array();
        json_append(args, derr ? json_copy(derr) : json_string(R2_L_REJECTED_DEFAULT));
        json_append(args, json_string(job_name));
        json_set(a, "args", args);
        json_free(dispatch);
        json_t *lr = cj_seam(io, "logger.info", a);
        if (r2_raise_kind(lr)) {
            free(oui); json_free(claimed_job); free(session_key); free(job_name);
            return lr;
        }
        json_free(lr);
        a = json_object();
        json_set(a, "job", json_copy(job));   /* ORIGINAL job, not claimed */
        json_set(a, "extra_prompt", extra ? json_copy(extra) : json_null());
        json_t *res = cj_seam(io, "run_claimed_job", a);
        if (r2_raise_kind(res)) {
            free(oui); json_free(claimed_job); free(session_key); free(job_name);
            return res;
        }
        if (!res || res->type != JSON_OBJECT) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object does not support item assignment",
                     r2_typename(res));
            json_free(res);
            free(oui); json_free(claimed_job); free(session_key); free(job_name);
            return r2_raise("TypeError", msg);
        }
        json_set(res, "dispatched", json_bool(false));
        free(oui); json_free(claimed_job); free(session_key); free(job_name);
        return res;
    }
}
