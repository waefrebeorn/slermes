/*
 * cron_flow4.c — C11 FLOW port of the hermes_cli/cron.py REMAINDER (fourth wave).
 *
 * Ported live defs (all verified unclaimed by a CRLF-aware PoP grep):
 *   _builtin_gateway_liveness, cron_incidents, _ticker_age_is_fresh,
 *   _store_unwritable_report, _probe_then_list_jobs, _print_store_unwritable,
 *   cron_doctor.
 *
 * Universal seam: json_t *(*call)(void *ctx, const char *name, const json_t *args).
 * NULL result == Python None; {"__raise__":kind,"msg":...} == a scripted
 * exception. Seam ARGS are borrowed; the callee returns a NEW json_t the port
 * owns (and frees on every swallow/cleanup path). Fail-closed is implemented in
 * the harness dispatcher, not here.
 *
 * Modeling contract (mirrored by tests/oracle_cron_flow4.py):
 *  - color(text, Colors.X) projects to `text` (no ANSI codes on either side).
 *  - print(...) fires seam "print" with args {"text": " ".join(args)}.
 *  - `from cron.jobs import TICKER_INTERVAL_SECONDS` projects to the caller's
 *    `ticker_interval` fixture field (default 60).
 *  - `_current_cron_store().cron_dir` fires seam "current_cron_store" and
 *    projects the scripted string straight into probe_report's "dir" arg.
 *  - CLAIMED siblings are seamed, never re-ported: _active_cron_provider_name,
 *    _cron_doctor_issues_for_job, _print_banner.
 *  - Full builtin exception hierarchy: cr4_exc_caught_by() walks the parent
 *    table so `except Exception` swallows every Exception subclass, while
 *    SystemExit/KeyboardInterrupt/GeneratorExit/BaseExceptionGroup/CancelledError
 *    propagate. Unknown kinds default to Exception subclasses (the oracle
 *    builds them as synthetic Exception subclasses).
 *  - `except OSError` matches the whole OSError subtree; a non-OSError raise
 *    propagates; an OSError with no store report is RE-RAISED verbatim.
 */
#ifndef CR4_CRON_FLOW4_H
#define CR4_CRON_FLOW4_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"

typedef struct cr4_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} cr4_seams_t;

/* ---- full builtin exception parent table (py3.14 names included) ---- */
typedef struct { const char *name; const char *parent; } cr4_exc_edge_t;
static const cr4_exc_edge_t cr4_exc_edges[] = {
    { "BaseException", NULL },
    { "SystemExit", "BaseException" },
    { "KeyboardInterrupt", "BaseException" },
    { "GeneratorExit", "BaseException" },
    { "CancelledError", "BaseException" },
    { "BaseExceptionGroup", "BaseException" },
    { "Exception", "BaseException" },
    { "ExceptionGroup", "Exception" },
    { "StopIteration", "Exception" },
    { "StopAsyncIteration", "Exception" },
    { "ArithmeticError", "Exception" },
    { "FloatingPointError", "ArithmeticError" },
    { "OverflowError", "ArithmeticError" },
    { "ZeroDivisionError", "ArithmeticError" },
    { "AssertionError", "Exception" },
    { "AttributeError", "Exception" },
    { "BufferError", "Exception" },
    { "EOFError", "Exception" },
    { "ImportError", "Exception" },
    { "ModuleNotFoundError", "ImportError" },
    { "LookupError", "Exception" },
    { "IndexError", "LookupError" },
    { "KeyError", "LookupError" },
    { "MemoryError", "Exception" },
    { "NameError", "Exception" },
    { "UnboundLocalError", "NameError" },
    { "OSError", "Exception" },
    { "EnvironmentError", "OSError" },
    { "IOError", "OSError" },
    { "WindowsError", "OSError" },
    { "BlockingIOError", "OSError" },
    { "ChildProcessError", "OSError" },
    { "ConnectionError", "OSError" },
    { "BrokenPipeError", "ConnectionError" },
    { "ConnectionAbortedError", "ConnectionError" },
    { "ConnectionRefusedError", "ConnectionError" },
    { "ConnectionResetError", "ConnectionError" },
    { "FileExistsError", "OSError" },
    { "FileNotFoundError", "OSError" },
    { "InterruptedError", "OSError" },
    { "IsADirectoryError", "OSError" },
    { "NotADirectoryError", "OSError" },
    { "PermissionError", "OSError" },
    { "ProcessLookupError", "OSError" },
    { "TimeoutError", "OSError" },
    { "ReferenceError", "Exception" },
    { "RuntimeError", "Exception" },
    { "NotImplementedError", "RuntimeError" },
    { "RecursionError", "RuntimeError" },
    { "PythonFinalizationError", "RuntimeError" },
    { "SyntaxError", "Exception" },
    { "IndentationError", "SyntaxError" },
    { "TabError", "IndentationError" },
    { "_IncompleteInputError", "SyntaxError" },
    { "SystemError", "Exception" },
    { "TypeError", "Exception" },
    { "ValueError", "Exception" },
    { "UnicodeError", "ValueError" },
    { "UnicodeDecodeError", "UnicodeError" },
    { "UnicodeEncodeError", "UnicodeError" },
    { "UnicodeTranslateError", "UnicodeError" },
    { "Warning", "Exception" },
    { "DeprecationWarning", "Warning" },
    { "PendingDeprecationWarning", "Warning" },
    { "RuntimeWarning", "Warning" },
    { "SyntaxWarning", "Warning" },
    { "UserWarning", "Warning" },
    { "FutureWarning", "Warning" },
    { "ImportWarning", "Warning" },
    { "UnicodeWarning", "Warning" },
    { "BytesWarning", "Warning" },
    { "EncodingWarning", "Warning" },
    { "ResourceWarning", "Warning" },
};

static const char *cr4_exc_parent(const char *n) {
    if (!n) return NULL;
    for (size_t i = 0; i < sizeof(cr4_exc_edges) / sizeof(cr4_exc_edges[0]); i++)
        if (strcmp(n, cr4_exc_edges[i].name) == 0) return cr4_exc_edges[i].parent;
    return NULL;
}

/* True when a scripted kind is caught by `except <base>`. Unknown kinds are
 * synthetic Exception subclasses on the Python side, so only BaseException
 * itself (and its explicitly-listed BaseException-only children) escapes an
 * `except Exception`. */
static bool cr4_exc_caught_by(const char *kind, const char *base) {
    if (!kind) return false;
    if (strcmp(base, "Exception") == 0 && !cr4_exc_parent(kind) &&
        strcmp(kind, "BaseException") != 0) return true;
    const char *cur = kind;
    for (int i = 0; i < 48 && cur; i++) {
        if (strcmp(cur, base) == 0) return true;
        cur = cr4_exc_parent(cur);
    }
    return false;
}

static bool cr4_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *cr4_raise_kind(const json_t *v) {
    if (!cr4_is_raise(v)) return NULL;
    const json_t *k = json_obj_get(v, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
/* Copy just {__raise__, msg} — the drive wrapper's raise_spec projection. */
static json_t *cr4_raise_only(const json_t *v) {
    json_t *o = json_object();
    const json_t *k = json_obj_get(v, "__raise__");
    const json_t *m = json_obj_get(v, "msg");
    json_set(o, "__raise__", json_copy(k));
    json_set(o, "msg", m ? json_copy(m) : json_string(""));
    return o;
}

static json_t *cr4_call(const cr4_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *cr4_args(void) { return json_object(); }
static json_t *cr4_args1(const char *k, const json_t *v) {
    json_t *o = json_object();
    json_set(o, k, v ? json_copy(v) : json_null());
    return o;
}
static json_t *cr4_print_seam(const cr4_seams_t *s, const char *text) {
    json_t *a = json_object();
    json_set(a, "text", json_string(text ? text : ""));
    json_t *r = cr4_call(s, "print", a);
    json_free(a);
    return r;
}
static const char *cr4_gets(const json_t *o, const char *k, const char *d) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : d;
}
/* Read a STRING SCALAR node (json_get_* with a NULL key always returns the
 * default — it is an object+key accessor, not a scalar reader). */
static const char *cr4_strval(const json_t *v, const char *d) {
    return (v && v->type == JSON_STRING) ? v->str_val : d;
}
static bool cr4_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] != '\0';
    if (v->type == JSON_ARRAY) return json_len(v) > 0;
    if (v->type == JSON_OBJECT) return v->c.count > 0;
    return false;
}
/* re.sub(r"\s+", " ", s).strip() for the ASCII whitespace fixture domain. */
static char *cr4_ws_collapse(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 2);
    size_t k = 0;
    bool inws = false, started = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        bool ws = (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                   c == '\x0b' || c == '\x0c');
        if (ws) { inws = true; continue; }
        if (inws && started) out[k++] = ' ';
        out[k++] = s[i];
        started = true;
        inws = false;
    }
    out[k] = '\0';
    return out;
}
/* error_text[:157].rstrip() + "..." when the collapsed text exceeds 160. */
static char *cr4_clip_error(const char *text) {
    size_t n = strlen(text);
    if (n <= 160) return strdup(text);
    size_t k = n > 157 ? 157 : n;
    while (k > 0) {
        unsigned char c = (unsigned char)text[k - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '\x0b' || c == '\x0c') k--;
        else break;
    }
    char *out = (char *)malloc(k + 4);
    memcpy(out, text, k);
    memcpy(out + k, "...", 4);
    return out;
}

/* PoP: cr4_ticker_age_is_fresh @ hermes_cli/cron.py:_ticker_age_is_fresh */
static json_t *cr4_ticker_age_is_fresh(const json_t *age, double interval) {
    if (!age || age->type == JSON_NULL) return json_bool(false);
    double a = (age->type == JSON_NUMBER) ? age->num_val : 0.0;
    return json_bool(a <= interval * 3.0 + 20.0);
}

/* PoP: cr4_builtin_gateway_liveness @ hermes_cli/cron.py:_builtin_gateway_liveness */
static json_t *cr4_builtin_gateway_liveness(const cr4_seams_t *s, double interval) {
    json_t *r = cr4_call(s, "active_provider_name", cr4_args());
    if (cr4_is_raise(r)) {
        if (cr4_exc_caught_by(cr4_raise_kind(r), "Exception")) { json_free(r); return json_null(); }
        return r;
    }
    const char *pn = (r && r->type == JSON_STRING) ? r->str_val : "";
    bool builtin = (strcmp(pn, "builtin") == 0);
    json_free(r);
    if (!builtin) return json_bool(true);

    /* contextlib.suppress(Exception) — same swallow rule as the outer try. */
    json_t *lk = cr4_call(s, "runtime_lock_active", cr4_args());
    if (cr4_is_raise(lk)) {
        if (cr4_exc_caught_by(cr4_raise_kind(lk), "Exception")) json_free(lk);
        else return lk;
    } else {
        bool active = cr4_truthy(lk);
        json_free(lk);
        if (active) return json_bool(true);
    }

    json_t *pids = cr4_call(s, "find_gateway_pids", cr4_args());
    if (cr4_is_raise(pids)) {
        if (cr4_exc_caught_by(cr4_raise_kind(pids), "Exception")) { json_free(pids); return json_null(); }
        return pids;
    }
    bool have_pids = (pids && pids->type == JSON_ARRAY && json_len(pids) > 0);
    json_free(pids);
    if (have_pids) return json_bool(true);

    json_t *age = cr4_call(s, "ticker_heartbeat_age", cr4_args());
    if (cr4_is_raise(age)) {
        if (cr4_exc_caught_by(cr4_raise_kind(age), "Exception")) { json_free(age); return json_null(); }
        return age;
    }
    json_t *fresh = cr4_ticker_age_is_fresh(age, interval);
    json_free(age);
    if (!cr4_truthy(fresh)) { json_free(fresh); return json_bool(false); }
    json_free(fresh);

    json_t *mx = cr4_call(s, "multiplexer_serves", cr4_args());
    if (cr4_is_raise(mx)) {
        if (cr4_exc_caught_by(cr4_raise_kind(mx), "Exception")) { json_free(mx); return json_null(); }
        return mx;
    }
    bool served = cr4_truthy(mx);
    json_free(mx);
    /* Python `A or B` SHORT-CIRCUITS: B is never called when A is truthy. */
    bool alive = false;
    if (!served) {
        json_t *wr = cr4_call(s, "ticker_writer_alive", cr4_args());
        if (cr4_is_raise(wr)) {
            if (cr4_exc_caught_by(cr4_raise_kind(wr), "Exception")) { json_free(wr); return json_null(); }
            return wr;
        }
        alive = cr4_truthy(wr);
        json_free(wr);
    }
    return json_bool(served || alive);
}

/* PoP: cr4_store_unwritable_report @ hermes_cli/cron.py:_store_unwritable_report */
static json_t *cr4_store_unwritable_report(const cr4_seams_t *s) {
    json_t *store = cr4_call(s, "current_cron_store", cr4_args());
    if (cr4_is_raise(store)) return store;
    const char *dir = (store && store->type == JSON_STRING) ? store->str_val : "";
    json_t *a = cr4_args1("dir", store);
    json_t *r = cr4_call(s, "probe_report", a);
    json_free(a);
    json_free(store);
    (void)dir;
    return r;
}

/* PoP: cr4_print_store_unwritable @ hermes_cli/cron.py:_print_store_unwritable */
static json_t *cr4_print_store_unwritable(const json_t *rep, bool brief, const cr4_seams_t *s) {
    char buf[1536];
    const char *store = cr4_gets(rep, "store", "");
    const char *err = cr4_gets(rep, "error", "");
    const char *fix = cr4_gets(rep, "fix", "");
    const char *since = cr4_gets(rep, "since", "");
    if (brief) {
        snprintf(buf, sizeof(buf),
                 "\xe2\x9a\xa0 Cron store %s is NOT writable (%s) \xe2\x80\x94 scheduled jobs "
                 "are being skipped; %s. See `hermes cron status`.", store, err, fix);
        return cr4_print_seam(s, buf);
    }
    json_t *r = cr4_print_seam(s,
        "\xe2\x9a\xa0 Cron store is NOT writable \xe2\x80\x94 scheduled jobs are being skipped");
    if (cr4_is_raise(r)) return r;
    json_free(r);
    snprintf(buf, sizeof(buf), "  %s: %s (last successful write %s)", store, err, since);
    r = cr4_print_seam(s, buf);
    if (cr4_is_raise(r)) return r;
    json_free(r);
    snprintf(buf, sizeof(buf), "  Fix: %s. Each due job then fires once on the next tick.", fix);
    return cr4_print_seam(s, buf);
}

/* PoP: cr4_probe_then_list_jobs @ hermes_cli/cron.py:_probe_then_list_jobs */
static json_t *cr4_probe_then_list_jobs(bool include_disabled, bool brief, const cr4_seams_t *s) {
    json_t *report = cr4_store_unwritable_report(s);
    if (cr4_is_raise(report)) return report;
    if (report) {
        json_t *pr = cr4_print_store_unwritable(report, brief, s);
        if (cr4_is_raise(pr)) { json_free(report); return pr; }
        json_free(pr);
    }
    json_t *a = json_object();
    json_set(a, "include_disabled", json_bool(include_disabled));
    json_t *jobs = cr4_call(s, "list_jobs", a);
    json_free(a);

    json_t *out = json_array();
    if (cr4_is_raise(jobs)) {
        if (cr4_exc_caught_by(cr4_raise_kind(jobs), "OSError")) {
            json_t *saved = cr4_raise_only(jobs);
            json_free(jobs);
            if (!report) return saved;                       /* bare `raise` */
            json_append(out, report ? report : json_null()); /* takes ownership */
            json_append(out, json_null());
            json_free(saved);
            return out;
        }
        json_free(out);
        return jobs;  /* non-OSError propagates */
    }
    json_append(out, report ? report : json_null());
    json_append(out, jobs ? jobs : json_null());
    return out;
}

/* PoP: cr4_incidents @ hermes_cli/cron.py:cron_incidents */
static json_t *cr4_incidents(const json_t *args, const cr4_seams_t *s) {
    const char *action = cr4_gets(args, "incident_action", "list");
    json_t *r;
    if (strcmp(action, "ack") == 0) {
        const json_t *idv = json_obj_get(args, "incident_id");
        const char *id = (idv && idv->type == JSON_STRING) ? idv->str_val : "";
        if (!id[0]) {
            r = cr4_print_seam(s, "\xe2\x9c\x97 Incident ID required: hermes cron incidents ack <incident_id>");
            if (cr4_is_raise(r)) return r;
            json_free(r);
            return json_number(1);
        }
        json_t *a = cr4_args1("incident_id", idv);
        r = cr4_call(s, "ack_incident", a);
        json_free(a);
        if (cr4_is_raise(r)) return r;
        bool ok = cr4_truthy(r);
        json_free(r);
        char buf[512];
        if (ok) snprintf(buf, sizeof(buf), "\xe2\x9c\x93 Incident %s acknowledged (closed).", id);
        else snprintf(buf, sizeof(buf), "Incident %s not found or already closed.", id);
        r = cr4_print_seam(s, buf);
        if (cr4_is_raise(r)) return r;
        json_free(r);
        return json_number(0);
    }

    json_t *stv = json_obj_get(args, "state");
    json_t *a = json_object();
    json_set(a, "state", stv ? json_copy(stv) : json_null());
    json_t *incs = cr4_call(s, "list_incidents", a);
    json_free(a);
    if (cr4_is_raise(incs)) return incs;
    size_t n = (incs && incs->type == JSON_ARRAY) ? json_len(incs) : 0;
    if (n == 0) {
        json_free(incs);
        r = cr4_print_seam(s, "No cron failure incidents recorded.");
        if (cr4_is_raise(r)) return r;
        json_free(r);
        const char *state = (stv && stv->type == JSON_STRING) ? stv->str_val : "";
        if (state[0]) {
            char buf[512];
            snprintf(buf, sizeof(buf), "  (filtered by state '%s')", state);
            r = cr4_print_seam(s, buf);
            if (cr4_is_raise(r)) return r;
            json_free(r);
        }
        return json_number(0);
    }

    json_t *ba = json_object();
    json_set(ba, "title", json_string("Cron Failure Incidents"));
    r = cr4_call(s, "print_banner", ba);
    json_free(ba);
    if (cr4_is_raise(r)) { json_free(incs); return r; }
    json_free(r);

    char buf[2048];
    for (size_t i = 0; i < n; i++) {
        const json_t *inc = json_get(incs, i);
        const char *state = cr4_gets(inc, "state", "");
        const char *iid = cr4_gets(inc, "id", "");
        char *col = cr4_ws_collapse(cr4_gets(inc, "error", ""));
        char *err = cr4_clip_error(col);
        free(col);
        const struct { const char *label; const char *key; const char *def; } rows[] = {
            { "Job", "job_id", "?" }, { "Type", "failure_type", "unknown" },
            { "First seen", "first_seen_at", "?" }, { "Last seen", "last_seen_at", "?" },
            { "Error", NULL, NULL }, { "Output", "output_file", NULL }
        };
        snprintf(buf, sizeof(buf), "  %s  %s", iid, state);
        r = cr4_print_seam(s, buf);
        if (cr4_is_raise(r)) { free(err); json_free(incs); return r; }
        json_free(r);
        for (size_t j = 0; j < sizeof(rows) / sizeof(rows[0]); j++) {
            const char *val = rows[j].key ? cr4_gets(inc, rows[j].key, rows[j].def) : err;
            if (strcmp(rows[j].label, "Output") == 0 && (!val || !val[0])) continue;
            char lc[64];
            snprintf(lc, sizeof(lc), "%s:", rows[j].label);
            snprintf(buf, sizeof(buf), "    %-12s%s", lc, val ? val : "");
            r = cr4_print_seam(s, buf);
            if (cr4_is_raise(r)) { free(err); json_free(incs); return r; }
            json_free(r);
        }
        free(err);
        r = cr4_print_seam(s, "");
        if (cr4_is_raise(r)) { json_free(incs); return r; }
        json_free(r);
    }
    json_free(incs);
    snprintf(buf, sizeof(buf),
             "  %zu incident(s)  |  ack one with: hermes cron incidents ack <id>", n);
    r = cr4_print_seam(s, buf);
    if (cr4_is_raise(r)) return r;
    json_free(r);
    return json_number(0);
}

/* PoP: cr4_doctor @ hermes_cli/cron.py:cron_doctor */
static json_t *cr4_doctor(const cr4_seams_t *s) {
    json_t *a = json_object();
    json_set(a, "include_disabled", json_bool(false));
    json_t *jobs = cr4_call(s, "list_jobs", a);
    json_free(a);
    if (cr4_is_raise(jobs)) return jobs;
    size_t nj = (jobs && jobs->type == JSON_ARRAY) ? json_len(jobs) : 0;

    json_t *findings = json_array();
    for (size_t i = 0; i < nj; i++) {
        const json_t *job = json_get(jobs, i);
        json_t *ia = json_object();
        json_set(ia, "job", json_copy(job));
        json_t *issues = cr4_call(s, "doctor_issues", ia);
        json_free(ia);
        if (cr4_is_raise(issues)) { json_free(findings); json_free(jobs); return issues; }
        if (issues && issues->type == JSON_ARRAY && json_len(issues) > 0) {
            json_t *f = json_object();
            json_set(f, "job", json_copy(job));
            json_set(f, "issues", json_copy(issues));
            json_append(findings, f);
        }
        json_free(issues);
    }

    json_t *r;
    char buf[1024];
    if (json_len(findings) == 0) {
        json_free(findings);
        json_free(jobs);
        r = cr4_print_seam(s, "\xe2\x9c\x93 Cron doctor found no issues");
        if (cr4_is_raise(r)) return r;
        json_free(r);
        snprintf(buf, sizeof(buf), "%s", nj ? "  Checked jobs." : "  No active jobs configured.");
        if (nj) snprintf(buf, sizeof(buf), "  Checked %zu active job(s).", nj);
        r = cr4_print_seam(s, buf);
        if (cr4_is_raise(r)) return r;
        json_free(r);
        return json_number(0);
    }

    size_t count = 0;
    for (size_t i = 0; i < json_len(findings); i++)
        count += json_len(json_obj_get(json_get(findings, i), "issues"));
    snprintf(buf, sizeof(buf), "Cron doctor found %zu issue(s) across %zu job(s):",
             count, json_len(findings));
    r = cr4_print_seam(s, buf);
    if (cr4_is_raise(r)) { json_free(findings); json_free(jobs); return r; }
    json_free(r);
    r = cr4_print_seam(s, "");
    if (cr4_is_raise(r)) { json_free(findings); json_free(jobs); return r; }
    json_free(r);
    for (size_t i = 0; i < json_len(findings); i++) {
        const json_t *f = json_get(findings, i);
        const json_t *job = json_obj_get(f, "job");
        snprintf(buf, sizeof(buf), "  %s %s",
                 cr4_gets(job, "id", "?"), cr4_gets(job, "name", "(unnamed)"));
        r = cr4_print_seam(s, buf);
        if (cr4_is_raise(r)) { json_free(findings); json_free(jobs); return r; }
        json_free(r);
        const json_t *issues = json_obj_get(f, "issues");
        for (size_t k = 0; k < json_len(issues); k++) {
            snprintf(buf, sizeof(buf), "    - %s", cr4_strval(json_get(issues, k), ""));
            r = cr4_print_seam(s, buf);
            if (cr4_is_raise(r)) { json_free(findings); json_free(jobs); return r; }
            json_free(r);
        }
    }
    /* live `print()` sits AFTER the for-loop, not inside it. */
    r = cr4_print_seam(s, "");
    if (cr4_is_raise(r)) { json_free(findings); json_free(jobs); return r; }
    json_free(r);
    json_free(findings);
    json_free(jobs);
    r = cr4_print_seam(s, "Review the findings above, then run `hermes cron doctor` again.");
    if (cr4_is_raise(r)) return r;
    json_free(r);
    return json_number(1);
}

#endif /* CR4_CRON_FLOW4_H */
