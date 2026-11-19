/*
 * terminal_tool_lifecycle_flow.c — command-execution lifecycle FLOW cores of
 * tools/terminal_tool.py + live splits (LIVE authority: hermes-agent @ 3371c03df6).
 *
 * Ports decision sequences: call order, branch conditions, early returns,
 * exception->fallback paths, mutation order and dict insertion order. Every
 * live effect (env.execute, registry spawn, approval scans, config/scope reads,
 * cwd resolution, redaction, truncation, logging, sleeps, metrics) enters
 * through the seam structs in terminal_tool_lifecycle_flow.h; the cores below
 * perform no I/O.
 *
 * Python semantics honored: truthiness of dict.get results, `or` fallbacks,
 * short-circuit evaluation order (seam calls happen exactly when live would
 * call them), str()/repr() of scalars, code-point slicing, dict insertion
 * order, int() conversion rules, posixpath lexical normpath/relpath.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "terminal_tool_lifecycle_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

/* ── live module constants (verbatim renderings; generated from the quarry) ── */

static const char TTLF_PROMOTED_NOTE[] =
    "Requested foreground timeout {requested}s exceeds the {cap}s cap, so this command was star"
    "ted as a tracked background process with notify_on_complete=true instead of being refused."
    " Do NOT re-run it. Its completion (exit code + output tail) arrives as a notification; pol"
    "l with process_manage(action=\"poll\", session_id=...) if you need it sooner.";

static const char TTLF_PROMOTED_NOTE_POLL_ONLY[] =
    "Requested foreground timeout {requested}s exceeds the {cap}s cap, so this command was star"
    "ted as a tracked background process instead of being refused. Do NOT re-run it. This sessi"
    "on cannot receive completion notifications, so poll it with process_manage(action=\"poll\""
    ", session_id=...) until it exits.";

static const char TTLF_YIELDED_NOTE[] =
    "The user sent a message while this command was running, so it was moved to the background "
    "WITHOUT being killed and is still running. You will be notified when it exits (notify_on_c"
    "omplete). Read the user's message and respond to it now; use process_manage(action='poll'|"
    "'wait'|'log', session_id=...) to check on this command.";

static const char TTLF_SILENT_BACKGROUND_HINT[] =
    "background=true without notify_on_complete=true means this process runs SILENTLY — you wil"
    "l not be told when it exits. If this is a bounded task (test suite, build, CI poller, depl"
    "oy, anything with a defined end), you almost certainly wanted notify_on_complete=true so t"
    "he system pings you on exit. Re-launch with notify_on_complete=true, or call process_manag"
    "e(action='poll') / process_manage(action='wait') yourself to learn the outcome. Only ignor"
    "e this hint for genuine long-lived processes that never exit (servers, watchers, daemons).";

static const char TTLF_HOMEBREW_CI_POLLER_HINT[] =
    "This looks like a homebrewed CI poller built from `gh pr view --json statusCheckRollup` an"
    "d/or `gh pr checks | jq`. That shape has burned us repeatedly in hermes-agent dev work (PR"
    "s #31329, #31448, #31695, #31709, #31745, #32264, #33131) — stdout buffering kills output "
    "capture, jq null-key edge cases silently exit the loop, conclusion-vs-status field confusi"
    "on exits early with bogus all-green verdicts, TTY-only summary banners never appear when p"
    "iped. Use the canonical snippets in the green-ci-policy skill instead: the exit-code-drive"
    "n `gh pr checks $PR >/dev/null` (rc 0 = green, 8 = pending, else fail) for exit-on-first-f"
    "ail behavior, or the column-2 awk-on-tabs poller (`awk -F\"\\t\" \"$2==\\\"pending\\\"\"`)"
    " for sharded matrices. Load skill_view(name='github/hermes-agent-dev', file_path='referenc"
    "es/green-ci-policy.md') for the verbatim snippets. If you must roll a custom loop with ric"
    "h structured output, write each tick to a known file (`tee -a $TMPDIR/ci.log`) and rely on"
    " `process_manage(action='log')` to read THAT file — do not rely on background-process stdo"
    "ut capture for line-buffered shell loops.";

static const char TTLF_AUTO_BACKGROUND_NOTE[] =
    "Ran in the background (the trailing & was converted to background=true; Hermes tracks the "
    "process). Do NOT re-run it. Use process_manage (poll/log/wait/kill, session_id above) to c"
    "heck it.";

static const char TTLF_SUBAGENT_NOTIFY_NOTE[] =
    "You are a subagent: this process's completion notice will NOT reach your parent, and the p"
    "rocess is killed when you finish. Before you finish, either wait for it (process_manage wa"
    "it), kill it, or hand it to your parent with process_manage(action='handoff', session_id=."
    ".., data='<purpose>') so the parent receives its completion. For CI watchers prefer return"
    "ing the fact (PR number, SHA) and letting the parent watch.";

static const char TTLF_ENV_RECREATED_NOTE[] =
    "The execution environment was recreated while running this command (the previous container"
    "/sandbox was gone). Persistent files were restored where the backend supports it, but back"
    "ground processes and any files outside persisted paths from earlier commands may be lost —"
    " verify state before relying on prior work.";

/* tools/registry.py constants for tool_error's bounded error body. */
#define TTLF_MAX_TOOL_ERROR_CHARS 2048
#define TTLF_MAX_LOGGED_ERROR_CHARS 8192
static const char TTLF_TOOL_ERROR_TRUNCATION_MARKER[] = "\xe2\x80\xa6 [truncated]";

/* ── internal helpers (no Python counterpart; semantics documented inline) ── */

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static char *xprintf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

/* Python truthiness for a JSON value (NULL pointer == Python None). */
static bool jtruth(const json_t *v) {
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

static const json_t *jget(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}

/* dict.get(key, default): presence-based (a present null value is returned). */
static const json_t *jget_def(const json_t *obj, const char *key, const json_t *def) {
    const json_t *v = jget(obj, key);
    return v ? v : def;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* Numeric view of a JSON scalar (bool counts as 0/1, like Python int math). */
static double jnum(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}

/* Shortest round-trip decimal (Python repr/str of float). */
static void py_float_repr(double v, char *out, size_t cap) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}

/* Python str() for scalars. Containers serialize as compact JSON (fixtures
 * keep str()-converted fields scalar, matching the live call sites). */
static char *pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return xstrdup("None");
    switch (v->type) {
        case JSON_BOOL:   return xstrdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return xstrdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                py_float_repr(d, buf, sizeof(buf));
            return xstrdup(buf);
        }
        default: {
            char *s = json_serialize(v);
            return s ? s : xstrdup("");
        }
    }
}

/* Python repr() of a string: delimiter minimizes escapes (a string holding a
 * single quote but no double quote renders with double quotes). */
static char *pyrepr_str(const char *s) {
    if (!s) return xstrdup("None");
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            o += (size_t)snprintf(out + o, cap - o, "\\x%02x", c);
        }
        else out[o++] = (char)c;
    }
    out[o++] = q;
    out[o] = '\0';
    return out;
}

/* Python repr() for the scalar JSON values the %r log sites receive. */
static char *pyrepr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return xstrdup("None");
    if (v->type == JSON_STRING) return pyrepr_str(v->str_val);
    return pystr(v);
}

/* str.strip() / lstrip / rstrip over ASCII whitespace (fixtures are ASCII). */
static char *py_strip(const char *s) {
    if (!s) return xstrdup("");
    const char *b = s;
    while (*b && isspace((unsigned char)*b)) b++;
    const char *e = s + strlen(s);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *d = malloc(n + 1);
    if (d) { memcpy(d, b, n); d[n] = '\0'; }
    return d;
}

static char *py_lower(const char *s) {
    char *d = xstrdup(s ? s : "");
    if (d) for (char *p = d; *p; p++) *p = (char)tolower((unsigned char)*p);
    return d;
}

/* str.replace(old, new) — single pass, non-overlapping, like Python. */
static char *str_replace(const char *s, const char *old, const char *new) {
    size_t oldlen = strlen(old), newlen = strlen(new);
    if (oldlen == 0) return xstrdup(s);
    size_t count = 0;
    for (const char *p = s; (p = strstr(p, old)) != NULL; p += oldlen) count++;
    size_t cap = strlen(s) + count * (newlen - oldlen) + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    char *o = out;
    const char *p = s;
    while (*p) {
        if (strncmp(p, old, oldlen) == 0) {
            memcpy(o, new, newlen); o += newlen; p += oldlen;
        } else *o++ = *p++;
    }
    *o = '\0';
    return out;
}

/* code-point slice [:limit] — fixtures are ASCII so byte slice == char slice. */
static char *str_slice(const char *s, size_t limit) {
    if (!s) return xstrdup("");
    size_t n = strlen(s);
    if (n > limit) n = limit;
    char *d = malloc(n + 1);
    if (d) { memcpy(d, s, n); d[n] = '\0'; }
    return d;
}

/* Python int() over a JSON value: bool -> 0/1, number -> trunc toward zero,
 * string -> optional sign + digits with surrounding whitespace stripped.
 * Returns false where Python raises TypeError/ValueError. */
static bool py_int(const json_t *v, long long *out) {
    if (!v) return false;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return true; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d >= 9.2e18 || d <= -9.2e18) return false;
        *out = (long long)d;
        return true;
    }
    if (v->type != JSON_STRING || !v->str_val) return false;
    const char *p = v->str_val;
    while (*p && isspace((unsigned char)*p)) p++;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!isdigit((unsigned char)*p)) return false;
    long long acc = 0;
    while (isdigit((unsigned char)*p)) {
        acc = acc * 10 + (*p - '0');
        if (acc > 922337203685477580LL) return false;
        p++;
    }
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p) return false;
    *out = neg ? -acc : acc;
    return true;
}

/* ── posixpath lexical math (deterministic; the oracle uses live posixpath) ── */

static bool pp_isabs(const char *p) { return p && p[0] == '/'; }

/* posixpath.normpath: collapse //, ., ..; keep a leading exactly-double slash. */
static char *pp_normpath(const char *p) {
    if (!p || !*p) return xstrdup(".");
    bool abs = p[0] == '/';
    bool dbl = abs && p[1] == '/' && p[2] != '/';
    char **stack = NULL;
    size_t n = 0, cap = 0;
    const char *s = p;
    while (*s) {
        while (*s == '/') s++;
        if (!*s) break;
        const char *e = s;
        while (*e && *e != '/') e++;
        size_t len = (size_t)(e - s);
        if (len == 1 && s[0] == '.') { s = e; continue; }
        if (len == 2 && s[0] == '.' && s[1] == '.') {
            if (n > 0 && strcmp(stack[n - 1], "..") != 0) {
                free(stack[--n]);
            } else if (!abs) {
                if (n == cap) { cap = cap ? cap * 2 : 8; stack = realloc(stack, cap * sizeof(char *)); }
                stack[n++] = xstrdup("..");
            }
            s = e;
            continue;
        }
        if (n == cap) { cap = cap ? cap * 2 : 8; stack = realloc(stack, cap * sizeof(char *)); }
        char *comp = malloc(len + 1);
        memcpy(comp, s, len); comp[len] = '\0';
        stack[n++] = comp;
        s = e;
    }
    size_t total = 3;
    for (size_t i = 0; i < n; i++) total += strlen(stack[i]) + 1;
    char *out = malloc(total);
    size_t o = 0;
    if (abs) { out[o++] = '/'; if (dbl) out[o++] = '/'; }
    for (size_t i = 0; i < n; i++) {
        if (i > 0) out[o++] = '/';
        size_t l = strlen(stack[i]);
        memcpy(out + o, stack[i], l); o += l;
        free(stack[i]);
    }
    free(stack);
    if (o == 0) out[o++] = '.';
    out[o] = '\0';
    return out;
}

/* posixpath.relpath for two absolute normalized paths: walk the common
 * component prefix, ".." per remaining start component, then the path tail. */
static char *pp_relpath(const char *path, const char *start) {
    char **pc = NULL, **sc = NULL;
    size_t pn = 0, sn = 0, pcap = 0, scap = 0;
    const char *s = path;
    while (*s) {
        while (*s == '/') s++;
        if (!*s) break;
        const char *e = s;
        while (*e && *e != '/') e++;
        if (pn == pcap) { pcap = pcap ? pcap * 2 : 8; pc = realloc(pc, pcap * sizeof(char *)); }
        pc[pn] = malloc((size_t)(e - s) + 1);
        memcpy(pc[pn], s, (size_t)(e - s)); pc[pn][e - s] = '\0';
        pn++;
        s = e;
    }
    s = start;
    while (*s) {
        while (*s == '/') s++;
        if (!*s) break;
        const char *e = s;
        while (*e && *e != '/') e++;
        if (sn == scap) { scap = scap ? scap * 2 : 8; sc = realloc(sc, scap * sizeof(char *)); }
        sc[sn] = malloc((size_t)(e - s) + 1);
        memcpy(sc[sn], s, (size_t)(e - s)); sc[sn][e - s] = '\0';
        sn++;
        s = e;
    }
    size_t i = 0;
    while (i < pn && i < sn && strcmp(pc[i], sc[i]) == 0) i++;
    size_t ups = sn - i, tails = pn - i;
    size_t total = 1;
    for (size_t k = 0; k < ups; k++) total += 3;
    for (size_t k = i; k < pn; k++) total += strlen(pc[k]) + 1;
    char *out = malloc(total + 1);
    size_t o = 0;
    for (size_t k = 0; k < ups; k++) {
        if (o) out[o++] = '/';
        out[o++] = '.'; out[o++] = '.';
    }
    for (size_t k = i; k < pn; k++) {
        if (o) out[o++] = '/';
        size_t l = strlen(pc[k]);
        memcpy(out + o, pc[k], l); o += l;
    }
    if (o == 0) out[o++] = '.';
    out[o] = '\0';
    for (size_t k = 0; k < pn; k++) free(pc[k]);
    for (size_t k = 0; k < sn; k++) free(sc[k]);
    free(pc); free(sc);
    (void)tails;
    return out;
}

/* ── trace recorder ── */

void ttlf_trace_add(ttlf_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

void ttlf_trace_free(ttlf_trace_t *t) {
    if (t && t->events) { json_free(t->events); t->events = NULL; }
}

/* ── tool_error (tools/registry.py): {"error": bounded message} ── */

static char *tool_error_bounded(const char *message, void (*log)(void *, const char *, const char *), void *ctx) {
    char *text = xstrdup(message ? message : "");
    size_t len = strlen(text);
    if (len > TTLF_MAX_TOOL_ERROR_CHARS) {
        if (log) {
            char *prefix = str_slice(text, TTLF_MAX_LOGGED_ERROR_CHARS);
            char *msg = xprintf("tool error body truncated for context (%zu chars): %s", len, prefix);
            log(ctx, "debug", msg);
            free(msg); free(prefix);
        }
        char *cut = str_slice(text, TTLF_MAX_TOOL_ERROR_CHARS);
        free(text);
        text = xprintf("%s%s", cut, TTLF_TOOL_ERROR_TRUNCATION_MARKER);
        free(cut);
    }
    json_t *o = json_object();
    json_set(o, "error", json_string(text));
    free(text);
    char *out = json_serialize(o);
    json_free(o);
    return out;
}

/* ── 1. _error_json ── */

/* PoP: ttlf_error_json_flow @ tools/terminal_tool.py:_error_json */
char *ttlf_error_json_flow(const json_t *error, int exit_code,
                           const json_t *status, const json_t *extra) {
    json_t *body = json_object();
    json_set(body, "output", json_string(""));
    json_set(body, "exit_code", json_number(exit_code));
    json_set(body, "error", error ? json_copy(error) : json_null());
    if (status) json_set(body, "status", json_copy(status));
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(body, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    char *out = json_serialize(body);
    json_free(body);
    return out;
}

/* ── 2. _run_approval_guards ── */

static json_t *approval_verdict(const char *note, bool approved_run) {
    json_t *v = json_object();
    json_set(v, "note", note ? json_string(note) : json_null());
    json_set(v, "approved_run", json_bool(approved_run));
    return v;
}

/* PoP: ttlf_run_approval_guards_flow @ tools/terminal_tool.py:_run_approval_guards */
json_t *ttlf_run_approval_guards_flow(const char *command, const char *env_type,
                                      const json_t *config, bool force,
                                      const ttlf_approval_seams_t *s,
                                      char **rejected) {
    *rejected = NULL;
    if (force) return approval_verdict(NULL, true);

    bool has_host_access = s->docker_has_host_access(s->ctx, config);
    const json_t *approval = s->check_all_guards(s->ctx, command, env_type, has_host_access);

    if (!jtruth(jget(approval, "approved"))) {
        const json_t *st = jget(approval, "status");
        const char *sts = jstr(st);
        if (sts && strcmp(sts, "pending_approval") == 0) {
            /* gateway ask mode: the pending envelope carries the ask fields. */
            json_t *extra = json_object();
            json_set(extra, "approval_pending", json_bool(true));
            json_t *cmd_def = json_string(command ? command : "");
            json_set(extra, "command", json_copy(jget_def(approval, "command", cmd_def)));
            json_free(cmd_def);
            json_t *desc_def = json_string("command flagged");
            json_set(extra, "description", json_copy(jget_def(approval, "description", desc_def)));
            json_free(desc_def);
            json_t *pk_def = json_string("");
            json_set(extra, "pattern_key", json_copy(jget_def(approval, "pattern_key", pk_def)));
            json_free(pk_def);
            json_t *sd_def = json_bool(false);
            json_set(extra, "smart_denied", json_copy(jget_def(approval, "smart_denied", sd_def)));
            json_free(sd_def);
            json_t *ap_def = json_bool(true);
            json_set(extra, "allow_permanent", json_copy(jget_def(approval, "allow_permanent", ap_def)));
            json_free(ap_def);
            json_t *err = json_string("");
            json_t *status = json_string("pending_approval");
            *rejected = ttlf_error_json_flow(err, -1, status, extra);
            json_free(err); json_free(status); json_free(extra);
            return NULL;
        }
        const json_t *desc = jget(approval, "description");
        char *desc_s = desc ? pystr(desc) : xstrdup("command flagged");
        char *fallback = xprintf("Command denied: %s. "
                                 "Use the approval prompt to allow it, or rephrase the command.",
                                 desc_s);
        free(desc_s);
        json_t *fb_node = json_string(fallback);
        free(fallback);
        const json_t *msg = jget_def(approval, "message", fb_node);
        json_t *extra = json_object();
        const json_t *us = jget(approval, "user_summary");
        if (jtruth(us)) json_set(extra, "user_summary", json_copy(us));
        json_t *status = json_string("blocked");
        *rejected = ttlf_error_json_flow(msg, -1, status, extra);
        json_free(fb_node); json_free(status); json_free(extra);
        return NULL;
    }

    const json_t *desc = jget(approval, "description");
    char *desc_s = desc ? pystr(desc) : xstrdup("flagged as dangerous");
    json_t *verdict = NULL;
    if (jtruth(jget(approval, "user_approved"))) {
        char *note = xprintf("Command required approval (%s) and was approved by the user.", desc_s);
        verdict = approval_verdict(note, true);
        free(note);
    } else if (jtruth(jget(approval, "smart_approved"))) {
        char *note = xprintf("Command was flagged (%s) and auto-approved by smart approval.", desc_s);
        verdict = approval_verdict(note, false);
        free(note);
    } else if (jtruth(jget(approval, "pre_approved"))) {
        char *note = s->preapproval_note(s->ctx, jget(approval, "pre_approved"));
        verdict = approval_verdict(note, false);
        free(note);
    } else {
        verdict = approval_verdict(NULL, false);
    }
    free(desc_s);
    return verdict;
}

/* ── 3. _pre_exec_block ── */

/* PoP: ttlf_pre_exec_block_flow @ tools/terminal_tool.py:_pre_exec_block */
char *ttlf_pre_exec_block_flow(const char *command, const json_t *env,
                               const char *env_type, const char *cwd,
                               const char *workdir, const char *session_key,
                               const ttlf_preexec_seams_t *s) {
    /* Order matters: gateway lifecycle first (protects the running gateway),
     * then the dangerous-workdir check, then the self-repo guard (local only). */
    json_t *kw = json_object();
    json_set(kw, "command", json_string(command ? command : ""));
    json_set(kw, "env", env ? json_copy(env) : json_null());
    json_set(kw, "env_type", json_string(env_type ? env_type : ""));
    json_set(kw, "cwd", json_string(cwd ? cwd : ""));
    json_set(kw, "workdir", workdir ? json_string(workdir) : json_null());
    json_set(kw, "session_key", json_string(session_key ? session_key : ""));
    char *blocked = s->gateway_lifecycle_block(s->ctx, kw);
    json_free(kw);
    if (blocked) return blocked;

    if (workdir && workdir[0]) {
        char *workdir_error = s->validate_workdir(s->ctx, workdir);
        if (workdir_error) {
            char *wd = str_slice(workdir, 200);
            char *pv = str_slice(command ? command : "", 200);
            char *msg = xprintf("Blocked dangerous workdir: %s (command: %s)", wd, pv);
            s->log(s->ctx, "warning", msg);
            free(msg); free(wd); free(pv);
            json_t *err = json_string(workdir_error);
            json_t *status = json_string("blocked");
            char *rej = ttlf_error_json_flow(err, -1, status, NULL);
            json_free(err); json_free(status);
            free(workdir_error);
            return rej;
        }
        char *refused = s->refuse_missing_local(s->ctx, workdir, env);
        if (refused) return refused;
    }

    if (env_type && strcmp(env_type, "local") == 0) {
        kw = json_object();
        json_set(kw, "command", json_string(command ? command : ""));
        json_set(kw, "cwd", json_string(cwd ? cwd : ""));
        json_set(kw, "workdir", workdir ? json_string(workdir) : json_null());
        json_set(kw, "session_key", json_string(session_key ? session_key : ""));
        /* self_repo_block(...) or locked_skill_block(...) — short-circuit or. */
        blocked = s->self_repo_block(s->ctx, kw);
        if (!blocked) blocked = s->locked_skill_block(s->ctx, kw);
        json_free(kw);
        if (blocked) return blocked;
    }
    return NULL;
}

/* ── 4. _plan_execution ── */

/* PoP: ttlf_plan_execution_flow @ tools/terminal_tool.py:_plan_execution */
json_t *ttlf_plan_execution_flow(const json_t *command, const char *command_type_name,
                                 const char *task_id, const json_t *timeout,
                                 bool background, bool host_local,
                                 const ttlf_plan_seams_t *s,
                                 char **rejected, char **exception) {
    *rejected = NULL;
    *exception = NULL;

    if (!command || command->type != JSON_STRING) {
        const char *tn = command_type_name ? command_type_name : "NoneType";
        char *msg = xprintf("Rejected invalid terminal command value: %s", tn);
        s->log(s->ctx, "warning", msg);
        free(msg);
        char *err = xprintf("Invalid command: expected string, got %s", tn);
        json_t *err_node = json_string(err);
        json_t *status = json_string("error");
        *rejected = ttlf_error_json_flow(err_node, -1, status, NULL);
        free(err); json_free(err_node); json_free(status);
        return NULL;
    }
    const char *cmd = command->str_val;

    const json_t *config = s->get_env_config(s->ctx);
    const char *env_type = host_local ? "local" : jstr(jget(config, "env_type"));
    if (!env_type) env_type = "";

    /* Fail closed under a refusal scope (non-host-local calls only). */
    if (!host_local) {
        char *exc_msg = NULL;
        bool raised = s->enforce_no_refusal(s->ctx, &exc_msg);
        if (raised) {
            *exception = exc_msg ? exc_msg : xstrdup("refusal scope");
            return NULL;
        }
    }

    char *eff = s->resolve_container_task_id(s->ctx, task_id);
    if (host_local) {
        char *hl = xprintf("host-local-%s", eff ? eff : "");
        free(eff);
        eff = hl;
    }

    const json_t *overrides = s->resolve_task_overrides(s->ctx, task_id);
    char *image = s->select_image(s->ctx, env_type, overrides, config);

    /* cwd = coerce_ssh_remote_cwd(overrides.get("cwd") or get_session_cwd(task_id)
     *                             or config["cwd"], env_type) — or-chain order. */
    const json_t *ov_cwd = jget(overrides, "cwd");
    json_t *cwd_input = NULL;
    char *sess_cwd = NULL;
    if (jtruth(ov_cwd)) {
        cwd_input = json_copy(ov_cwd);
    } else {
        sess_cwd = s->get_session_cwd(s->ctx, task_id);
        if (sess_cwd && sess_cwd[0]) {
            cwd_input = json_string(sess_cwd);
        } else {
            cwd_input = json_copy(jget(config, "cwd"));
        }
    }
    json_t *coerced = s->coerce_ssh_remote_cwd(s->ctx, cwd_input, env_type);
    json_free(cwd_input);
    free(sess_cwd);
    char *cwd_s = jstr(coerced) ? xstrdup(jstr(coerced)) : NULL;

    json_t *host_cwd = s->resolve_task_host_cwd(s->ctx, config, task_id);

    /* Re-apply the container-cwd guard to the resolved cwd (override/session
     * records are raw); remap to /workspace when the host cwd is mounted. */
    if (s->is_container_backend(s->ctx, env_type)) {
        if (s->is_unusable_container_cwd(s->ctx, cwd_s, host_cwd)) {
            const char *remapped;
            char *remapped_buf = NULL;
            if (jtruth(host_cwd)) {
                remapped = "/workspace";
            } else {
                remapped_buf = pystr(jget(config, "cwd"));
                remapped = remapped_buf;
            }
            if (!cwd_s || strcmp(cwd_s, remapped) != 0) {
                char *cwd_repr = cwd_s ? pyrepr_str(cwd_s) : xstrdup("None");
                char *rem_repr = pyrepr_str(remapped);
                char *msg = xprintf("Remapping host/relative cwd override %s for %s backend "
                                    "(won't exist in sandbox). Using %s instead.",
                                    cwd_repr, env_type, rem_repr);
                s->log(s->ctx, "info", msg);
                free(msg); free(cwd_repr); free(rem_repr);
            }
            free(cwd_s);
            cwd_s = xstrdup(remapped);
            free(remapped_buf);
        }
    }

    /* Reject non-positive timeouts before deadline math. */
    if (timeout && timeout->type != JSON_NULL &&
        (timeout->type == JSON_NUMBER || timeout->type == JSON_BOOL) && jnum(timeout) <= 0) {
        char *tstr = pystr(timeout);
        char *msg = xprintf("timeout must be a positive number of seconds (got %s).", tstr);
        *rejected = tool_error_bounded(msg, s->log, s->ctx);
        free(msg); free(tstr);
        goto fail;
    }

    json_t *promoted = NULL;
    char *auto_bg = NULL;
    if (!background) {
        auto_bg = s->trailing_amp(s->ctx, cmd);
        if (!auto_bg) {
            char *guidance = s->guidance(s->ctx, cmd);
            if (guidance) {
                json_t *err = json_string(guidance);
                json_t *status = json_string("error");
                *rejected = ttlf_error_json_flow(err, -1, status, NULL);
                json_free(err); json_free(status);
                free(guidance);
                goto fail;
            }
            if (jtruth(timeout) && jnum(timeout) > TTLF_FOREGROUND_MAX_TIMEOUT)
                promoted = json_copy(timeout);
        }
    }

    {
        json_t *plan = json_object();
        json_set(plan, "config", json_copy(config));
        json_set(plan, "env_type", json_string(env_type));
        json_set(plan, "effective_task_id", json_string(eff ? eff : ""));
        json_set(plan, "image", json_string(image ? image : ""));
        json_set(plan, "cwd", cwd_s ? json_string(cwd_s) : json_null());
        json_set(plan, "host_cwd", host_cwd ? json_copy(host_cwd) : json_null());
        json_set(plan, "effective_timeout",
                 jtruth(timeout) ? json_copy(timeout) : json_copy(jget(config, "timeout")));
        json_set(plan, "promoted_from_foreground_timeout", promoted ? promoted : json_null());
        json_set(plan, "auto_background_command", auto_bg ? json_string(auto_bg) : json_null());
        free(eff); free(image); free(cwd_s); free(auto_bg);
        json_free(host_cwd); json_free(coerced);
        return plan;
    }

fail:
    free(eff); free(image); free(cwd_s);
    json_free(host_cwd); json_free(coerced);
    return NULL;
}

/* ── 5. _with_promoted_note ── */

/* PoP: ttlf_with_promoted_note_flow @ tools/terminal_tool.py:_with_promoted_note */
char *ttlf_with_promoted_note_flow(const char *result_json, int requested_timeout) {
    if (!result_json) return NULL;
    char *err = NULL;
    json_t *data = json_parse(result_json, &err);
    free(err);
    if (!data) return xstrdup(result_json);
    if (data->type != JSON_OBJECT || jtruth(jget(data, "error"))) {
        json_free(data);
        return xstrdup(result_json);
    }
    const char *tmpl = jtruth(jget(data, "notify_on_complete"))
                           ? TTLF_PROMOTED_NOTE : TTLF_PROMOTED_NOTE_POLL_ONLY;
    char req[32];
    snprintf(req, sizeof(req), "%d", requested_timeout);
    char *step1 = str_replace(tmpl, "{requested}", req);
    char cap[32];
    snprintf(cap, sizeof(cap), "%d", TTLF_FOREGROUND_MAX_TIMEOUT);
    char *note = str_replace(step1, "{cap}", cap);
    free(step1);
    json_set(data, "promoted_from_foreground", json_string(note));
    free(note);
    char *out = json_serialize(data);
    json_free(data);
    return out;
}/* ── 6. _run_foreground ── */

/* PoP: ttlf_run_foreground_flow @ tools/terminal_tool.py:_run_foreground */
char *ttlf_run_foreground_flow(const char *command, const json_t *env,
                               const char *env_type, const char *effective_task_id,
                               int effective_timeout, const char *plan_cwd,
                               const char *plan_host_cwd,
                               const char *task_id, const char *session_id,
                               const char *session_key, const char *cwd_key,
                               const char *workdir, const char *approval_note,
                               bool clear_interrupt, bool metered,
                               const ttlf_foreground_seams_t *s) {
    const int max_retries = 3;
    /* Clean interrupt slate ONCE before the retry loop (a genuine interrupt
     * during the backoff sleep must survive and abort the next attempt). */
    if (clear_interrupt) s->clear_interrupt(s->ctx);

    const json_t *result = NULL;
    char *command_cwd = NULL;
    char *out = NULL;

    for (int retry_count = 0; retry_count <= max_retries; retry_count++) {
        /* mounted_host = getattr(env, "host_cwd", None) or plan.host_cwd */
        const json_t *env_host = jget(env, "host_cwd");
        json_t *mounted = jtruth(env_host) ? json_copy(env_host)
                        : (plan_host_cwd ? json_string(plan_host_cwd) : json_null());
        json_t *kw = json_object();
        json_set(kw, "workdir", workdir ? json_string(workdir) : json_null());
        json_set(kw, "default_cwd", json_string(plan_cwd ? plan_cwd : ""));
        json_set(kw, "session_key", cwd_key ? json_string(cwd_key) : json_null());
        json_set(kw, "env_type", env_type ? json_string(env_type) : json_null());
        json_set(kw, "mounted_host", mounted);
        json_set(kw, "env", env ? json_copy(env) : json_null());
        free(command_cwd);
        command_cwd = s->resolve_cwd(s->ctx, kw);
        json_free(kw);

        /* _yield_kwargs: handler presence decides the yield_handler kwarg. */
        kw = json_object();
        json_set(kw, "command", json_string(command ? command : ""));
        json_set(kw, "env_type", env_type ? json_string(env_type) : json_null());
        json_set(kw, "cwd", command_cwd ? json_string(command_cwd) : json_null());
        json_set(kw, "effective_task_id", json_string(effective_task_id ? effective_task_id : ""));
        json_set(kw, "task_id", task_id ? json_string(task_id) : json_null());
        json_set(kw, "session_key", session_key ? json_string(session_key) : json_null());
        bool has_yield_handler = s->yield_handler(s->ctx, kw);
        json_free(kw);

        kw = json_object();
        json_set(kw, "command", json_string(command ? command : ""));
        json_set(kw, "timeout", json_number(effective_timeout));
        json_set(kw, "cwd", command_cwd ? json_string(command_cwd) : json_null());
        json_set(kw, "bounded_capture", json_bool(true));
        json_set(kw, "allow_cwd_fallback", json_bool(!(workdir && workdir[0])));
        json_set(kw, "yield_handler", json_bool(has_yield_handler));
        char *exc_type = NULL, *exc_msg = NULL;
        result = s->env_execute(s->ctx, kw, retry_count, &exc_type, &exc_msg);
        json_free(kw);
        if (!exc_type) break;

        /* A backend exception never reached an exit status; Hermes' own
         * deadline arrives as a "timeout" message -> exit 124 envelope. */
        char *low = py_lower(exc_msg ? exc_msg : "");
        bool timed_out = strstr(low, "timeout") != NULL;
        free(low);
        if (timed_out) {
            char *msg = xprintf("Command timed out after %d seconds", effective_timeout);
            json_t *err = json_string(msg);
            out = ttlf_error_json_flow(err, 124, NULL, NULL);
            json_free(err); free(msg);
            free(exc_type); free(exc_msg); free(command_cwd);
            return out;
        }
        char *preview = str_slice(command ? command : "", 200);
        if (retry_count < max_retries) {
            int wait_time = 1 << (retry_count + 1);  /* 2 ** (retry_count + 1) */
            char *msg = xprintf("Execution error, retrying in %ds (attempt %d/%d) - Command: %s "
                                "- Error: %s: %s - Task: %s, Backend: %s",
                                wait_time, retry_count + 1, max_retries, preview,
                                exc_type, exc_msg ? exc_msg : "",
                                effective_task_id ? effective_task_id : "", env_type ? env_type : "");
            s->log(s->ctx, "warning", msg);
            free(msg);
            s->sleep_fn(s->ctx, (double)wait_time);
            free(preview); free(exc_type); free(exc_msg);
            continue;
        }
        char *msg = xprintf("Execution failed after %d retries - Command: %s "
                            "- Error: %s: %s - Task: %s, Backend: %s",
                            max_retries, preview, exc_type, exc_msg ? exc_msg : "",
                            effective_task_id ? effective_task_id : "", env_type ? env_type : "");
        s->log(s->ctx, "error", msg);
        free(msg); free(preview);
        char *raw = xprintf("Command execution failed: %s: %s", exc_type, exc_msg ? exc_msg : "");
        char *red = s->redact(s->ctx, raw);
        free(raw);
        json_t *err = json_string(red ? red : "");
        out = ttlf_error_json_flow(err, -1, NULL, NULL);
        json_free(err); free(red);
        free(exc_type); free(exc_msg); free(command_cwd);
        return out;
    }

    /* handed to the background: no exit status yet */
    if (jtruth(jget(result, "yielded_session_id"))) {
        json_t *o = json_object();
        const json_t *output = jget(result, "output");
        json_set(o, "output", output ? json_copy(output) : json_string(""));
        json_set(o, "exit_code", json_null());
        json_set(o, "error", json_null());
        json_set(o, "status", json_string("yielded_to_background"));
        json_set(o, "session_id", json_copy(jget(result, "yielded_session_id")));
        const json_t *pid = jget(result, "pid");
        json_set(o, "pid", pid ? json_copy(pid) : json_null());
        json_set(o, "notify_on_complete", json_bool(true));
        json_set(o, "note", json_string(TTLF_YIELDED_NOTE));
        char *fb = ttlf_cwd_fallback_note(jget(result, "cwd_fallback"));
        if (fb) {
            json_set(o, "cwd_fallback", json_string(fb));
            free(fb);
        }
        out = json_serialize(o);
        json_free(o);
        free(command_cwd);
        return out;
    }

    if (metered) s->record_outcome(s->ctx, command, env_type, result);

    json_t *kw = json_object();
    json_set(kw, "command", json_string(command ? command : ""));
    json_set(kw, "result", result ? json_copy(result) : json_null());
    json_set(kw, "env", env ? json_copy(env) : json_null());
    json_set(kw, "env_type", env_type ? json_string(env_type) : json_null());
    json_set(kw, "effective_task_id", json_string(effective_task_id ? effective_task_id : ""));
    json_set(kw, "task_id", task_id ? json_string(task_id) : json_null());
    json_set(kw, "session_id", session_id ? json_string(session_id) : json_null());
    json_set(kw, "session_key", cwd_key ? json_string(cwd_key) : json_null());
    json_set(kw, "workdir", workdir ? json_string(workdir) : json_null());
    json_set(kw, "command_cwd", command_cwd ? json_string(command_cwd) : json_null());
    json_set(kw, "approval_note", approval_note ? json_string(approval_note) : json_null());
    out = s->finalize(s->ctx, kw);
    json_free(kw);
    free(command_cwd);
    return out;
}

/* ── 7. _terminal_deadline_floor ── */

/* PoP: ttlf_terminal_deadline_floor_flow @ tools/terminal_tool.py:_terminal_deadline_floor */
bool ttlf_terminal_deadline_floor_flow(const json_t *args,
                                       const ttlf_deadline_seams_t *s, double *out) {
    if (jtruth(jget(args, "background"))) return false;
    const json_t *req = jget(args, "timeout");
    long long requested = 0;
    if (jtruth(req)) {
        long long v = 0;
        requested = py_int(req, &v) ? v : 0;  /* except (TypeError, ValueError) -> 0 */
    }
    if (requested > TTLF_FOREGROUND_MAX_TIMEOUT) return false;
    if (requested > 0) {
        *out = (double)requested;
        return true;
    }
    bool raised = false;
    json_t *r = s->parse_env_var(s->ctx, "TERMINAL_TIMEOUT", "180", &raised);
    if (raised || !r || r->type != JSON_NUMBER) {
        json_free(r);
        return false;
    }
    *out = r->num_val;
    json_free(r);
    return true;
}

/* ── 8. _degraded_result ── */

/* PoP: ttlf_degraded_result_flow @ tools/terminal_tool.py:_degraded_result */
char *ttlf_degraded_result_flow(const json_t *e, const char *task_id,
                                const ttlf_degraded_seams_t *s) {
    char *raw = s->tenv(s->ctx, "TERMINAL_DEGRADED_MODE", "warn");
    char *stripped = py_strip(raw ? raw : "");
    char *norm = py_lower(stripped);
    bool fail_mode = strcmp(norm, "fail") == 0;
    free(raw); free(stripped); free(norm);
    if (fail_mode) return s->fatal_error_json(s->ctx, e);

    char *reason = pystr(jget(e, "reason"));
    char *msg = xprintf("terminal backend degraded: %s", reason);
    s->log(s->ctx, "warning", msg);
    free(msg);

    /* Evict the possibly-broken backend; _quiet swallows eviction failures. */
    bool raised = s->evict(s->ctx, task_id);
    if (raised) s->log(s->ctx, "debug", "degraded-env eviction failed");

    json_t *o = json_object();
    json_set(o, "output", json_string(""));
    json_set(o, "exit_code", json_number(-1));
    json_set(o, "status", json_string("degraded"));
    json_set(o, "reason", json_copy(jget(e, "reason")));
    json_set(o, "retry_hint", json_copy(jget(e, "retry_hint")));
    char *err = xprintf("Terminal backend degraded: %s", reason);
    json_set(o, "error", json_string(err));
    free(err); free(reason);
    char *out = json_serialize(o);
    json_free(o);
    return out;
}

/* ── 9. _handle_terminal ── */

/* PoP: ttlf_handle_terminal_flow @ tools/terminal_tool.py:_handle_terminal */
char *ttlf_handle_terminal_flow(const json_t *args, const char *heartbeat_py_type,
                                const char *task_id, const char *session_id,
                                const ttlf_handle_seams_t *s) {
    s->validate_prepared(s->ctx, args);

    /* Models sometimes send execute_code's ``code`` here. */
    if (!json_has(args, "command") && json_has(args, "code")) {
        return tool_error_bounded(
            "terminal received a 'code' parameter, but it requires a shell "
            "command in 'command'. Use execute_code(code=...) for Python; "
            "for shell, retry as terminal(command=...).", s->log, s->ctx);
    }

    const json_t *notify = jget(args, "notify");
    const json_t *noc = jget(args, "notify_on_complete");
    const json_t *watch = jget(args, "watch_patterns");
    const json_t *hb = jget(args, "heartbeat");

    /* heartbeat = args.get("heartbeat") or 0; then the isinstance gate. */
    long long heartbeat = 0;
    bool hb_valid = true;
    if (jtruth(hb)) {
        if (hb->type == JSON_BOOL) {
            hb_valid = false;  /* isinstance(heartbeat, bool) -> rejected */
        } else if (hb->type == JSON_NUMBER) {
            if (heartbeat_py_type && strcmp(heartbeat_py_type, "float") == 0) {
                hb_valid = false;  /* isinstance(60.0, int) is False */
            } else {
                heartbeat = (long long)hb->num_val;
                if (heartbeat < 0) hb_valid = false;
            }
        } else {
            hb_valid = false;  /* str/list/etc are not int */
        }
    }
    if (!hb_valid) {
        return tool_error_bounded(
            "heartbeat must be a whole number of seconds (0 disables; positive values are clamped to min 60).",
            s->log, s->ctx);
    }

    bool background = jtruth(jget(args, "background"));
    bool persist = jtruth(jget(args, "persist_on_release"));
    if (!background) {
        if (jtruth(notify) || jtruth(watch) || jtruth(noc)) {
            return tool_error_bounded(
                "notify only applies to background commands (foreground "
                "results return directly). Either drop it, or run as "
                "terminal(command=..., background=true, notify=...).", s->log, s->ctx);
        }
        heartbeat = 0;  /* a foreground heartbeat carries no intent */
        if (jtruth(jget(args, "pty"))) {
            return tool_error_bounded(
                "pty requires background=true (a PTY session is interacted "
                "with via process_manage(action='write'/'submit'), which needs a "
                "tracked background process). Retry as terminal(command=..., "
                "background=true, pty=true).", s->log, s->ctx);
        }
        if (persist) {
            return tool_error_bounded(
                "persist_on_release only applies to background commands (a foreground "
                "process is awaited inline and has nothing to persist). Retry as "
                "terminal(command=..., background=true, persist_on_release=true).", s->log, s->ctx);
        }
    }

    /* notify mapping: explicit notify wins; bool -> notify_on_complete,
     * list -> watch_patterns; anything else is refused. */
    json_t *noc_node = noc ? json_copy(noc) : json_bool(false);
    json_t *watch_node = watch ? json_copy(watch) : json_null();
    if (notify) {
        if (notify->type == JSON_BOOL) {
            json_free(noc_node);
            noc_node = json_bool(notify->bool_val);
            json_free(watch_node);
            watch_node = json_null();
        } else if (notify->type == JSON_ARRAY) {
            json_free(watch_node);
            watch_node = json_copy(notify);
            json_free(noc_node);
            noc_node = json_bool(false);
        } else {
            json_free(noc_node); json_free(watch_node);
            return tool_error_bounded(
                "notify must be true/false (notify on exit) or a list of "
                "strings (notify on output pattern match).", s->log, s->ctx);
        }
    }
    if (heartbeat) {
        json_free(noc_node);
        noc_node = json_bool(true);  /* the heartbeat rides the completion path */
    }

    json_t *kw = json_object();
    const json_t *command = jget(args, "command");
    json_set(kw, "command", command ? json_copy(command) : json_null());
    json_set(kw, "background", json_bool(background));
    const json_t *timeout = jget(args, "timeout");
    json_set(kw, "timeout", timeout ? json_copy(timeout) : json_null());
    json_set(kw, "task_id", task_id ? json_string(task_id) : json_null());
    json_set(kw, "session_id", session_id ? json_string(session_id) : json_null());
    const json_t *workdir = jget(args, "workdir");
    json_set(kw, "workdir", workdir ? json_copy(workdir) : json_null());
    json_set(kw, "pty", json_bool(jtruth(jget(args, "pty"))));
    json_set(kw, "notify_on_complete", noc_node);
    json_set(kw, "watch_patterns", watch_node);
    json_set(kw, "heartbeat", json_number((double)heartbeat));
    json_set(kw, "persist_on_release", json_bool(persist));
    char *out = s->terminal_tool(s->ctx, kw);
    json_free(kw);
    return out;
}

/* ── 10. spawn_background_process + _looks_like_homebrew_ci_poller ── */

/* PoP: ttlf_looks_like_homebrew_ci_poller @ tools/terminal_tool_background.py:_looks_like_homebrew_ci_poller */
bool ttlf_looks_like_homebrew_ci_poller(const char *command) {
    if (!command) return false;
    bool has_gh = strstr(command, "gh pr view") != NULL || strstr(command, "gh pr checks") != NULL;
    bool has_jq = strstr(command, " jq ") != NULL || strstr(command, "| jq") != NULL ||
                  strstr(command, "$(jq") != NULL;
    return strstr(command, "statusCheckRollup") != NULL || (has_gh && has_jq);
}

/* PoP: ttlf_spawn_background_flow @ tools/terminal_tool_background.py:spawn_background_process */
char *ttlf_spawn_background_flow(const json_t *p, const ttlf_spawn_seams_t *s,
                                 json_t **session_out) {
    *session_out = NULL;
    const char *command = jstr(jget(p, "command"));
    const json_t *env = jget(p, "env");
    const char *env_type = jstr(jget(p, "env_type"));
    const char *eff = jstr(jget(p, "effective_task_id"));
    const char *task_id = jstr(jget(p, "task_id"));
    const char *session_key = jstr(jget(p, "session_key"));
    const char *workdir = jstr(jget(p, "workdir"));
    const char *cwd = jstr(jget(p, "cwd"));
    const char *cwd_key = jstr(jget(p, "cwd_key"));
    const char *approval_note = jstr(jget(p, "approval_note"));
    const char *pty_disabled_reason = jstr(jget(p, "pty_disabled_reason"));
    bool effective_pty = jtruth(jget(p, "effective_pty"));
    bool notify = jtruth(jget(p, "notify_on_complete"));
    const json_t *watch = jget(p, "watch_patterns");
    bool persist = jtruth(jget(p, "persist_on_release"));
    bool auto_backgrounded = jtruth(jget(p, "auto_backgrounded"));
    int completion_output_chars = (int)jnum(jget(p, "completion_output_chars"));
    int heartbeat_seconds = (int)jnum(jget(p, "heartbeat_seconds"));
    if (!env_type) env_type = "";
    if (!eff) eff = "";
    if (!session_key) session_key = "";

    /* mounted_host if mounted_host is not None else getattr(env, "host_cwd", None) */
    const json_t *mh = jget(p, "mounted_host");
    if (!mh || mh->type == JSON_NULL) mh = jget(env, "host_cwd");

    json_t *kw = json_object();
    json_set(kw, "workdir", workdir ? json_string(workdir) : json_null());
    json_set(kw, "default_cwd", json_string(cwd ? cwd : ""));
    json_set(kw, "session_key", json_string((cwd_key && cwd_key[0]) ? cwd_key : session_key));
    json_set(kw, "env_type", json_string(env_type));
    json_set(kw, "mounted_host", mh ? json_copy(mh) : json_null());
    json_set(kw, "env", env ? json_copy(env) : json_null());
    char *effective_cwd = s->resolve_cwd(s->ctx, kw);
    json_free(kw);

    /* _spawn: local -> spawn_local(env_vars, use_pty, restore_login_path, **common);
     * otherwise -> spawn_via_env(env, allow_cwd_fallback, **common). */
    const char *owner = (task_id && task_id[0]) ? task_id : eff;
    json_t *skw = json_object();
    const char *branch;
    if (strcmp(env_type, "local") == 0) {
        branch = "local";
        const json_t *env_vars = jget(env, "env");  /* hasattr(env, 'env') */
        json_set(skw, "env_vars", env_vars ? json_copy(env_vars) : json_null());
        json_set(skw, "use_pty", json_bool(effective_pty));
        json_set(skw, "restore_login_path", json_bool(true));
    } else {
        branch = "env";
        json_set(skw, "env", env ? json_copy(env) : json_null());
        json_set(skw, "allow_cwd_fallback", json_bool(!(workdir && workdir[0])));
    }
    json_set(skw, "command", json_string(command ? command : ""));
    json_set(skw, "cwd", json_string(effective_cwd ? effective_cwd : ""));
    json_set(skw, "task_id", json_string(eff));
    json_set(skw, "owner_task_id", json_string(owner));
    json_set(skw, "session_key", json_string(session_key));
    json_set(skw, "persist_on_release", json_bool(persist));
    char *spawn_exc = NULL;
    json_t *session = s->spawn(s->ctx, branch, skw, &spawn_exc);
    json_free(skw);

    if (spawn_exc) {
        char *raw = xprintf("Failed to start background process: %s", spawn_exc);
        char *red = s->redact(s->ctx, raw);
        free(raw); free(spawn_exc);
        json_t *o = json_object();
        json_set(o, "output", json_string(""));
        json_set(o, "exit_code", json_number(-1));
        json_set(o, "error", json_string(red ? red : ""));
        free(red); free(effective_cwd);
        char *out = json_serialize(o);
        json_free(o);
        return out;
    }

    *session_out = session;
    /* The live ProcessSession always carries watcher_platform (default ""). */
    if (!json_has(session, "watcher_platform"))
        json_set(session, "watcher_platform", json_string(""));

    json_t *rd = json_object();
    json_set(rd, "output", json_string("Background process started"));
    json_set(rd, "session_id", json_copy(jget(session, "id")));
    json_set(rd, "pid", json_copy(jget(session, "pid")));
    json_set(rd, "exit_code", json_number(0));
    json_set(rd, "error", json_null());
    if (persist) json_set(rd, "persist_on_release", json_bool(true));

    if (effective_cwd && effective_cwd[0]) {
        const json_t *used = jget(session, "cwd");
        if (jtruth(used)) {
            char *msys = s->msys_path(s->ctx, effective_cwd);
            const char *used_s = jstr(used);
            if (used_s && strcmp(used_s, effective_cwd) != 0 &&
                strcmp(used_s, msys ? msys : "") != 0) {
                json_t *fb = json_object();
                json_set(fb, "requested", json_string(effective_cwd));
                json_set(fb, "used", json_string(used_s));
                char *note = ttlf_cwd_fallback_note(fb);
                json_free(fb);
                if (note) {
                    json_set(rd, "cwd_fallback", json_string(note));
                    free(note);
                }
            }
            free(msys);
        }
    }
    if (approval_note && approval_note[0]) json_set(rd, "approval", json_string(approval_note));
    if (pty_disabled_reason && pty_disabled_reason[0])
        json_set(rd, "pty_note", json_string(pty_disabled_reason));
    if (auto_backgrounded) {
        json_set(rd, "auto_backgrounded", json_string(TTLF_AUTO_BACKGROUND_NOTE));
    } else if (!notify && !jtruth(watch)) {
        json_set(rd, "hint", json_string(TTLF_SILENT_BACKGROUND_HINT));
    }
    if (command && command[0] && ttlf_looks_like_homebrew_ci_poller(command)) {
        const char *existing = jstr(jget(rd, "hint"));
        if (existing && existing[0]) {
            char *joined = xprintf("%s\n\n%s", existing, TTLF_HOMEBREW_CI_POLLER_HINT);
            json_set(rd, "hint", json_string(joined));
            free(joined);
        } else {
            json_set(rd, "hint", json_string(TTLF_HOMEBREW_CI_POLLER_HINT));
        }
    }

    /* _apply_async_support (seam models the whole helper incl. routing stamp). */
    bool notify2 = false;
    json_t *watch2 = NULL;
    s->apply_async_support(s->ctx, session, rd, notify, watch, &notify2, &watch2);

    /* _resolve_notification_flag_conflict(background=True): both flags set ->
     * drop watch_patterns (duplicate async notifications). */
    const char *conflict_note = NULL;
    if (notify2 && jtruth(watch2)) {
        json_free(watch2);
        watch2 = NULL;
        conflict_note = "watch_patterns ignored because notify_on_complete=True; "
                        "these two flags produce duplicate notifications when combined";
    }
    if (conflict_note) {
        char *sid = pystr(jget(session, "id"));
        char *msg = xprintf("background proc %s: %s", sid, conflict_note);
        s->log(s->ctx, "warning", msg);
        free(msg); free(sid);
        json_set(rd, "watch_patterns_ignored", json_string(conflict_note));
    }

    if (notify2) {
        json_set(session, "notify_on_complete", json_bool(true));
        json_set(rd, "notify_on_complete", json_bool(true));
        if (completion_output_chars)
            json_set(session, "completion_output_chars", json_number(completion_output_chars));
        if (jtruth(jget(session, "watcher_platform")))
            s->register_watcher(s->ctx, session, session_key);
        if (s->is_delegated_child(s->ctx)) {
            json_set(rd, "notify_on_complete", json_bool(false));
            json_set(rd, "subagent_note", json_string(TTLF_SUBAGENT_NOTIFY_NOTE));
        } else if (heartbeat_seconds) {
            int armed = s->arm_heartbeat(s->ctx, session, heartbeat_seconds);
            json_set(rd, "heartbeat_seconds", json_number(armed));
        }
    } else if (heartbeat_seconds) {
        json_set(rd, "heartbeat_ignored",
                 json_string("heartbeat needs notify=true delivery, which this session cannot receive"));
    }
    if (jtruth(watch2)) {
        json_set(session, "watch_patterns", json_copy(watch2));
        json_set(rd, "watch_patterns", json_copy(watch2));
    }
    json_free(watch2);
    free(effective_cwd);
    char *out = json_serialize(rd);
    json_free(rd);
    return out;
}

/* ── 11. finalize_foreground_result + _cwd_fallback_note ── */

/* PoP: ttlf_cwd_fallback_note @ tools/terminal_tool_result.py:_cwd_fallback_note */
char *ttlf_cwd_fallback_note(const json_t *fallback) {
    if (!fallback || fallback->type != JSON_OBJECT) return NULL;
    const json_t *used = jget(fallback, "used");
    if (!jtruth(used)) return NULL;
    char *req = pystr(jget(fallback, "requested"));
    char *used_s = pystr(used);
    char *out = xprintf("Working directory %s no longer exists or is not accessible; "
                        "this command ran in %s instead, so relative paths resolved there. "
                        "Check where you are before anything destructive and `cd` to the "
                        "intended directory explicitly.", req, used_s);
    free(req); free(used_s);
    return out;
}

/* rstrip(".") — drop every trailing dot. */
static char *rstrip_dots(const char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '.') n--;
    char *d = malloc(n + 1);
    if (d) { memcpy(d, s, n); d[n] = '\0'; }
    return d;
}

/* PoP: ttlf_finalize_foreground_result_flow @ tools/terminal_tool_result.py:finalize_foreground_result */
char *ttlf_finalize_foreground_result_flow(const char *command, const json_t *result,
                                           const json_t *env, const char *env_type,
                                           const char *effective_task_id,
                                           const char *task_id, const char *session_id,
                                           const char *session_key, const char *workdir,
                                           const char *command_cwd, const char *approval_note,
                                           const ttlf_finalize_seams_t *s) {
    /* Record the cwd this command finished in as THIS session's durable cwd —
     * only when the command reported it, never for a per-command workdir. */
    const json_t *observed = NULL;
    if (jtruth(jget(result, "cwd_observed"))) {
        const json_t *rc = jget(result, "cwd");
        observed = jtruth(rc) ? rc : jget(env, "cwd");
    }
    if (!(workdir && workdir[0]) && jtruth(observed)) {
        char *obs_s = pystr(observed);
        s->record_session_cwd(s->ctx, session_key, obs_s);
        free(obs_s);
    }

    char *output = xstrdup(jstr(jget(result, "output")) ? jstr(jget(result, "output")) : "");
    const json_t *rc_node = jget(result, "returncode");
    int returncode = (int)jnum(rc_node);

    /* sudo failure handling -> (output, auth_failed, cache_cleared) */
    json_t *sa = s->sudo_annotations(s->ctx, command, output, env_type);
    char *sa_out = xstrdup(jstr(jget(sa, "output")) ? jstr(jget(sa, "output")) : "");
    bool sudo_auth_failed = jtruth(jget(sa, "auth_failed"));
    bool sudo_cache_cleared = jtruth(jget(sa, "cache_cleared"));
    json_free(sa);
    free(output);
    output = sa_out;

    /* plugin transform hook (seam models the helper incl. its _quiet fail-open) */
    json_t *kw = json_object();
    json_set(kw, "command", json_string(command ? command : ""));
    json_set(kw, "output", json_string(output));
    json_set(kw, "returncode", json_number(returncode));
    json_set(kw, "task_id", json_string(effective_task_id ? effective_task_id : ""));
    json_set(kw, "env_type", json_string(env_type ? env_type : ""));
    bool raised = false;
    char *transformed = s->transform_hook(s->ctx, kw, &raised);
    json_free(kw);
    free(output);
    output = transformed ? transformed : xstrdup("");

    char *truncated = s->truncate(s->ctx, output);
    free(output);
    output = truncated ? truncated : xstrdup("");

    char *stripped_ansi = s->strip_ansi(s->ctx, output);
    free(output);
    output = stripped_ansi ? stripped_ansi : xstrdup("");

    /* redact is command-aware; only runs on truthy output, over .strip() */
    if (output[0]) {
        char *stripped = py_strip(output);
        char *redacted = s->redact_output(s->ctx, stripped, command);
        free(stripped); free(output);
        output = redacted ? redacted : xstrdup("");
    } else {
        free(output);
        output = xstrdup("");
    }

    char *exit_note = s->interpret_exit_code(s->ctx, command, returncode);

    kw = json_object();
    json_set(kw, "command", json_string(command ? command : ""));
    json_set(kw, "returncode", json_number(returncode));
    json_set(kw, "output", json_string(output));
    json_set(kw, "exit_note", exit_note ? json_string(exit_note) : json_null());
    raised = false;
    char *failure_hint = s->failure_hint(s->ctx, kw, &raised);
    json_free(kw);

    /* cwd echo, gated on the observation flag; _quiet("cwd comparison") */
    char *changed_cwd = NULL;
    if (jtruth(observed) && command_cwd && command_cwd[0]) {
        char *obs_s = pystr(observed);
        bool r1 = false, r2 = false;
        char *rp1 = s->realpath(s->ctx, obs_s, &r1);
        if (!r1) {
            char *rp2 = s->realpath(s->ctx, command_cwd, &r2);
            if (!r2 && rp1 && rp2 && strcmp(rp1, rp2) != 0)
                changed_cwd = xstrdup(obs_s);
            free(rp2);
        }
        if (r1 || r2) s->log(s->ctx, "debug", "cwd comparison");
        free(rp1); free(obs_s);
    }

    /* rc=130 is an interrupt only with the executor's marker. */
    char *approval_final = (approval_note && approval_note[0]) ? xstrdup(approval_note) : NULL;
    if (approval_final && returncode == 130 && strstr(output, "[Command interrupted]")) {
        char *trimmed = rstrip_dots(approval_final);
        char *nf = xprintf("%s, then interrupted.", trimmed);
        free(trimmed); free(approval_final);
        approval_final = nf;
    }

    json_t *rd = json_object();
    json_set(rd, "output", json_string(output));
    json_set(rd, "exit_code", rc_node ? json_copy(rc_node) : json_number(0));
    json_set(rd, "error", json_null());

    /* Optional fields in observable JSON key order; None means "omit". */
    if (changed_cwd) json_set(rd, "cwd", json_string(changed_cwd));
    if (jtruth(jget(result, "environment_recreated")))
        json_set(rd, "environment_recreated", json_string(TTLF_ENV_RECREATED_NOTE));
    char *fb = ttlf_cwd_fallback_note(jget(result, "cwd_fallback"));
    if (fb) {
        json_set(rd, "cwd_fallback", json_string(fb));
        free(fb);
    }
    json_t *spill = s->redact_spill(s->ctx, jget(result, "full_output_path"),
                                    jget(result, "output_total_chars"), command);
    if (spill && spill->type == JSON_ARRAY) {
        for (size_t i = 0; i < spill->c.count; i++) {
            const json_t *pair = spill->c.items[i];
            if (pair && pair->type == JSON_ARRAY && pair->c.count >= 2) {
                const char *k = jstr(pair->c.items[0]);
                const json_t *v = pair->c.items[1];
                if (k && v && v->type != JSON_NULL) json_set(rd, k, json_copy(v));
            }
        }
    }
    json_free(spill);

    const char *vsid = (session_id && session_id[0]) ? session_id
                     : (task_id && task_id[0]) ? task_id
                     : (effective_task_id && effective_task_id[0]) ? effective_task_id
                     : "default";
    kw = json_object();
    json_set(kw, "command", json_string(command ? command : ""));
    json_set(kw, "cwd", command_cwd ? json_string(command_cwd) : json_null());
    json_set(kw, "session_id", json_string(vsid));
    json_set(kw, "returncode", json_number(returncode));
    json_set(kw, "output", json_string(output));
    const json_t *pstat = jget(result, "pipeline_status");
    json_set(kw, "pipeline_status", pstat ? json_copy(pstat) : json_null());
    raised = false;
    json_t *verification = s->verification(s->ctx, kw, &raised);
    json_free(kw);
    if (verification) json_set(rd, "verification_evidence", verification);

    if (approval_final) json_set(rd, "approval", json_string(approval_final));
    if (exit_note && exit_note[0]) json_set(rd, "exit_code_meaning", json_string(exit_note));
    if (failure_hint && failure_hint[0]) json_set(rd, "hint", json_string(failure_hint));
    if (sudo_auth_failed) json_set(rd, "sudo_auth_failed", json_bool(true));
    if (sudo_cache_cleared) json_set(rd, "sudo_cache_cleared", json_bool(true));

    char *out = json_serialize(rd);
    json_free(rd);
    free(output); free(changed_cwd); free(exit_note); free(failure_hint); free(approval_final);
    return out;
}

/* ── 12. _interpret_exit_code ── */

/* _EXIT_CODE_SEMANTICS: informational non-zero exit codes per base command. */
typedef struct { const char *cmd; int code; const char *note; } ttlf_exit_sem_t;
static const ttlf_exit_sem_t TTLF_EXIT_SEMANTICS[] = {
    {"grep", 1, "No matches found (not an error)"},
    {"egrep", 1, "No matches found (not an error)"},
    {"fgrep", 1, "No matches found (not an error)"},
    {"rg", 1, "No matches found (not an error)"},
    {"ag", 1, "No matches found (not an error)"},
    {"ack", 1, "No matches found (not an error)"},
    {"diff", 1, "Files differ (expected, not an error)"},
    {"colordiff", 1, "Files differ (expected, not an error)"},
    {"test", 1, "Condition evaluated to false (expected, not an error)"},
    {"[", 1, "Condition evaluated to false (expected, not an error)"},
    {"find", 1, "Some directories were inaccessible (partial results may still be valid)"},
    {"curl", 6, "Could not resolve host"},
    {"curl", 7, "Failed to connect to host"},
    {"curl", 22, "HTTP response code indicated error (e.g. 404, 500)"},
    {"curl", 28, "Operation timed out"},
    {"git", 1, "Non-zero exit (often normal — e.g. 'git diff' returns 1 when files differ)"},
    {NULL, 0, NULL},
};

/* Last segment of re.split(r'\s*(?:\|\||&&|[|;])\s*', command): scan for the
 * separators (two-char ||/&& win over single |/&), skip trailing whitespace.
 * A lone '&' is NOT a separator (backgrounding / fd redirects). */
static char *iec_last_segment(const char *command) {
    const char *start = command;
    const char *p = command;
    while (*p) {
        size_t sep = 0;
        if (p[0] == '|' && p[1] == '|') sep = 2;
        else if (p[0] == '&' && p[1] == '&') sep = 2;
        else if (p[0] == '|' || p[0] == ';') sep = 1;
        if (sep) {
            p += sep;
            while (*p && isspace((unsigned char)*p)) p++;
            start = p;
        } else {
            p++;
        }
    }
    return xstrdup(start);
}

/* PoP: ttlf_interpret_exit_code_flow @ tools/terminal_tool_result.py:_interpret_exit_code */
char *ttlf_interpret_exit_code_flow(const char *command, int exit_code,
                                    const ttlf_iec_seams_t *s) {
    if (exit_code == 0) return NULL;
    char *signal_note = s->signal_note(s->ctx, exit_code);
    if (signal_note) return signal_note;

    /* base command = first word of the last segment that isn't a VAR=val
     * assignment (unless it starts with '-'), basename'd. */
    char *last = iec_last_segment(command ? command : "");
    char *stripped = py_strip(last);
    free(last);
    char *base = NULL;
    const char *w = stripped;
    while (*w) {
        while (*w && isspace((unsigned char)*w)) w++;
        if (!*w) break;
        const char *e = w;
        while (*e && !isspace((unsigned char)*e)) e++;
        size_t len = (size_t)(e - w);
        char *word = malloc(len + 1);
        memcpy(word, w, len); word[len] = '\0';
        bool has_eq = strchr(word, '=') != NULL;
        if (!has_eq || word[0] == '-') {
            const char *slash = strrchr(word, '/');
            base = xstrdup(slash ? slash + 1 : word);
            free(word);
            break;
        }
        free(word);
        w = e;
    }
    free(stripped);
    if (!base) return NULL;
    for (const ttlf_exit_sem_t *e = TTLF_EXIT_SEMANTICS; e->cmd; e++) {
        if (strcmp(base, e->cmd) == 0 && e->code == exit_code) {
            free(base);
            return xstrdup(e->note);
        }
    }
    free(base);
    return NULL;
}

/* ── 13. _host_path_key / translate_mounted_host_path ── */

/* PoP: ttlf_host_path_key @ tools/terminal_tool_config.py:_host_path_key */
char *ttlf_host_path_key(const char *path) {
    char *text = xstrdup(path ? path : "");
    if (!text) return NULL;
    for (char *p = text; *p; p++) if (*p == '\\') *p = '/';
    size_t len = strlen(text);
    if (len >= 2 && text[1] == ':') text[0] = (char)tolower((unsigned char)text[0]);
    if (len > 3) {
        while (len > 0 && text[len - 1] == '/') text[--len] = '\0';
    }
    return text;
}

/* PoP: ttlf_translate_mounted_host_path @ tools/terminal_tool_config.py:translate_mounted_host_path */
char *ttlf_translate_mounted_host_path(const char *path, const char *host_root,
                                       const char *container_root) {
    if (!path || !host_root || !container_root) return NULL;
    if (!path[0] || !host_root[0] || !container_root[0]) return NULL;
    char *key = ttlf_host_path_key(path);
    char *root = ttlf_host_path_key(host_root);
    if (!key || !root || !key[0] || !root[0]) {
        free(key); free(root);
        return NULL;
    }
    /* mount = container_root.rstrip("/") or "/" */
    char *mount = xstrdup(container_root);
    size_t mlen = strlen(mount);
    while (mlen > 0 && mount[mlen - 1] == '/') mount[--mlen] = '\0';
    if (mlen == 0) { mount[0] = '/'; mount[1] = '\0'; }

    char *out = NULL;
    if (strcmp(key, root) == 0) {
        out = mount;
        mount = NULL;
    } else {
        size_t rlen = strlen(root);
        char *prefix = (rlen > 0 && root[rlen - 1] == '/')
                           ? xstrdup(root) : xprintf("%s/", root);
        size_t plen = strlen(prefix);
        if (strncmp(key, prefix, plen) == 0)
            out = xprintf("%s/%s", mount, key + plen);
        free(prefix);
    }
    free(key); free(root); free(mount);
    return out;
}

/* ── 14. coerce_ssh_remote_cwd ── */

/* PoP: ttlf_coerce_ssh_remote_cwd_flow @ tools/terminal_tool_config.py:coerce_ssh_remote_cwd */
json_t *ttlf_coerce_ssh_remote_cwd_flow(const json_t *cwd, const char *env_type,
                                        const ttlf_ssh_seams_t *s) {
    char *et_strip = py_strip(env_type ? env_type : "");
    char *et = py_lower(et_strip);
    bool ssh = strcmp(et, "ssh") == 0;
    free(et_strip); free(et);
    if (!cwd || cwd->type != JSON_STRING || !ssh)
        return cwd ? json_copy(cwd) : json_null();

    char *text = py_strip(cwd->str_val);
    if (!text[0] || text[0] == '~') {
        json_t *r = json_string(text[0] ? text : "~");
        free(text);
        return r;
    }
    if (!pp_isabs(text)) {
        json_t *r = json_string(text);
        free(text);
        return r;
    }
    if (s->is_container_launch_dir(s->ctx, text)) {
        free(text);
        return json_string("~");
    }
    char *home = s->get_subprocess_home(s->ctx);
    if (!home || !home[0]) {
        free(home);
        json_t *r = json_string(text);
        free(text);
        return r;
    }
    char *real = s->get_real_home(s->ctx);
    char *nh = pp_normpath(home);
    char *nr = pp_normpath(real ? real : "");
    bool same_home = strcmp(nh, nr) == 0;
    free(nr);
    json_t *r = NULL;
    if (same_home) {
        r = json_string(text);
    } else {
        char *nt = pp_normpath(text);
        char *rel = pp_relpath(nt, nh);
        free(nt);
        if (strcmp(rel, ".") == 0) {
            r = json_string("~");
        } else if (strcmp(rel, "..") == 0 || strncmp(rel, "../", 3) == 0) {
            r = json_string(text);
        } else {
            char *tilde = xprintf("~/%s", rel);
            r = json_string(tilde);
            free(tilde);
        }
        free(rel);
    }
    free(nh); free(home); free(real); free(text);
    return r;
}
