/*
 * kanban_db_dispatch_flow2.c — FLOW port of the REMAINDER of the dead-worker
 * classification / crash-accounting cluster of hermes_cli/kanban_db_dispatch.py
 * (hermes-agent).
 *
 * MODULE RESOLUTION: hermes_cli/kanban_db_dispatch.py is partly claimed by
 * src/hermes_cli/port_kanban_db_dispatch.c, kanban_workers.c, kanban_db*.c and
 * src/gateway/session_kanban_flow3.c (which ported _worker_terminal_timeout_env,
 * _process_fingerprint, _pid_recycled, _worker_alive). This port covers the
 * NEXT unclaimed cluster — the dead-worker booking + crash breaker flow:
 *   describe_suppression, _worker_log_exit_code, _exit_summary_marker,
 *   _log_noise_prefixes, _worker_final_output, _DeadWorker.run_outcome,
 *   _classify_dead_worker, _classify_dead_worker_exit, _reclaim_dead_workers,
 *   _account_crashes, _is_handoff_event, adopt_worker_pid.
 * Every PoP below credits the defining module hermes_cli/kanban_db_dispatch.py.
 *
 * Ports the DECISION SEQUENCES — branch order, seam call order, exception
 * paths and fail-closed returns. Every live external call enters through ONE
 * universal seam: kdd2_call(ctx, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording stubs emitting the identical event shape {"name","args","ret"}
 * from the SAME scripts. Ported siblings are SEAMED inside flows (each is
 * exercised by its own direct cases, where the real body runs on both sides).
 * Claimed siblings (_classify_worker_exit, _exit_code_kind, _error_fingerprint,
 * _protocol_violation_streak, _record_task_failure, _worker_alive,
 * _process_fingerprint) and the _kb collaborators are ALWAYS seamed.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and kdd2_-prefixed.
 *
 * Seam name -> live call (args are informational projections):
 *   _worker_log_exit_code      _worker_log_exit_code(task_id, board=None)   [sibling]
 *   _exit_summary_marker       _exit_summary_marker()                       [sibling]
 *   _log_noise_prefixes        _log_noise_prefixes() -> list of prefixes    [sibling]
 *   _worker_final_output       _worker_final_output(task_id, board=None)    [sibling]
 *   _classify_dead_worker_exit _classify_dead_worker_exit(pid, claimer, exit_code,
 *                              task_id=, board=) -> dead record dict        [sibling]
 *   _classify_dead_worker      _classify_dead_worker(pid, claimer, exit_code,
 *                              task_id=, board=) -> dead record dict        [sibling]
 *   _classify_worker_exit      _classify_worker_exit(pid) -> [kind, code]   [claimed]
 *   _exit_code_kind            _exit_code_kind(code) -> [kind, code]        [claimed]
 *   _error_fingerprint         _error_fingerprint(error_text) -> str        [claimed]
 *   _protocol_violation_streak _protocol_violation_streak(conn, task_id)    [claimed]
 *   _record_task_failure       _record_task_failure(conn, task_id, error=, outcome=,
 *                              failure_limit=, force_trip=, release_claim=,
 *                              end_run=, event_payload_extra=)              [claimed]
 *   _worker_alive              _worker_alive(pid, started_at)               [claimed]
 *   _process_fingerprint       _process_fingerprint(pid)                    [claimed]
 *   kb.read_worker_log         _kb.read_worker_log(task_id, tail_bytes=4000, board=)
 *   kb.write_txn.__enter__     with _kb.write_txn(conn): __enter__
 *   kb.write_txn.__exit__      __exit__(exc_type_name_or_null) -> suppress bool
 *   kb._host_prefix            _kb._host_prefix() -> str
 *   kb._to_epoch               _kb._to_epoch(value)
 *   kb._row_get                _kb._row_get(row, col, default=None)
 *   kb._resolve_crash_grace_seconds  _kb._resolve_crash_grace_seconds()
 *   kb._retry_status_for_run   _kb._retry_status_for_run(conn, task_id)
 *   kb._end_run                _kb._end_run(conn, task_id, outcome=, status=, error=,
 *                              metadata=)
 *   kb._append_event           _kb._append_event(conn, task_id, kind, payload, run_id=)
 *   kb._json_or                _kb._json_or(payload, {})
 *   owner_reclaim_blocked      kanban_owner_recovery.owner_reclaim_blocked(conn,
 *                              task_id, crash_sweep=True)
 *   managed_run_exit_code      kanban_owner_recovery.managed_run_exit_code(conn,
 *                              task_id, pid, claim_lock)
 *   apply_attempt_note         kanban_owner_recovery.apply_attempt_note(conn,
 *                              task_id, pid, dead)
 *   i18n.t                     agent.i18n.t(key)
 *   conn.execute               conn.execute(sql, params) -> cursor spec
 *                              {"rows": [...], "rowcount": N}; fetchone()=rows[0]
 *                              or null, fetchall()=rows, rowcount=N
 *   time.time                  time.time()
 *
 * Representation contract (shared with tests/oracle_kanban_db_dispatch_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception site. The FULL builtin hierarchy plus the
 *   quarry customs ride the kdd2_exc_parents table.
 * - A dead worker (_DeadWorker dataclass) projects to a dict with the eight
 *   fields kind, code, error_text, event_kind, event_payload,
 *   protocol_violation, rate_limited, terminal_provider. The three flags
 *   default to false when absent; the five leading fields are required
 *   (absent -> the dataclass __init__ TypeError); an unknown key -> the
 *   unexpected-keyword TypeError. run_outcome reads rate_limited truthiness.
 * - A _CrashSweep projects to {crashed, rate_limited, crash_details,
 *   exited_hook_payloads}; crash_details entries are [task_id, pid, claimer,
 *   dead] lists (tuples project to lists).
 * - describe_suppression results are SimpleNamespace doubles: a result dict
 *   must carry respawn_guarded (list of [task_id, reason]), rate_limited
 *   (list), skipped_locked (bool), memory_pressure (str|null); an absent
 *   attribute raises "'types.SimpleNamespace' object has no attribute ...".
 * - conn rows are fixture dicts (key access); an absent key -> KeyError. The
 *   fixture domain carries every key a flow reads.
 * - The write_txn context manager is modelled by __enter__/__exit__ seams:
 *   an __enter__ raise skips the body (no __exit__); a body raise calls
 *   __exit__ with the exception kind name; an __exit__ raise REPLACES the
 *   in-flight exception; a truthy __exit__ return SUPPRESSES the body raise
 *   (execution continues after the with). A clean body / early return calls
 *   __exit__ with a null exc.
 * - Tuples project to lists; sets to sorted lists; integral floats to ints.
 * - int() coercion follows Python (trunc toward zero; narrow strip set; sign;
 *   digits with single underscores BETWEEN digits); failure -> ValueError
 *   "invalid literal for int() with base 10: <repr>".
 * - _EXIT_TRAILER_RE / _LOG_CHROME are transcribed byte-faithfully (UTF-8
 *   literals); fixture DATA is ASCII so the box-drawing / caduceus
 *   alternatives never fire on it, but the matcher recognises their exact
 *   UTF-8 encodings. re \s and str.strip share the ASCII White_Space set
 *   (0x09-0x0D, 0x1C-0x1F, 0x20). str.splitlines splits on \n \r \r\n \v \f
 *   \x1c \x1d \x1e (ASCII scope). str repr picks double quotes only when the
 *   string contains ' and not ".
 * - `is None` / `is not None` identity gates are JSON_NULL-only; truthiness
 *   gates use kdd2_truthy. Seam args drop None-valued TOP-LEVEL keys on both
 *   sides; nested nulls (conn.execute params, event payloads) are preserved.
 * - Source literals carry their exact bytes; fixture DATA is ASCII.
 *
 * Ownership: kdd2_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node — borrowed
 * pointers to a replaced key are copied first. Seam results are caller-freed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── byte-pinned quarry constants ── */
#define KDD2_WORKER_OUTPUT_SEP " Worker's last output: "
#define KDD2_PROTOCOL_VIOLATION_ERROR \
    "worker exited cleanly (rc=0) without kanban_complete, kanban_block " \
    "or kanban_request_review \xE2\x80\x94 protocol violation. " \
    "If the prior run already did the work, verify it and " \
    "report it via kanban_complete (or kanban_request_review); " \
    "a run without a terminal kanban call counts as failed no " \
    "matter what it did."
#define KDD2_PROTOCOL_VIOLATION_FAILURE_LIMIT 3
#define KDD2_UNVERIFIED "unverified"
#define KDD2_TRAILER "[kanban-worker-exit] rc="

#define KDD2_SQL_SELECT_RUNNING \
    "SELECT id, worker_pid, worker_started_at, claim_lock, started_at, assignee " \
    "FROM tasks WHERE status = 'running' AND worker_pid IS NOT NULL"
#define KDD2_SQL_UPDATE_TASKS \
    "UPDATE tasks SET status = ?, claim_lock = NULL, " \
    "claim_expires = NULL, worker_pid = NULL, worker_started_at = NULL " \
    "WHERE id = ? AND status = 'running' " \
    "  AND worker_pid = ? AND claim_lock IS ?"
#define KDD2_SQL_UPDATE_LFE "UPDATE tasks SET last_failure_error = ? WHERE id = ?"
#define KDD2_SQL_SELECT_MAX_RETRIES "SELECT max_retries FROM tasks WHERE id = ?"
#define KDD2_SQL_ADOPT_SELECT \
    "SELECT status, current_run_id, worker_pid, claim_lock FROM tasks WHERE id = ?"
#define KDD2_SQL_ADOPT_TASKS \
    "UPDATE tasks SET worker_pid = ?, worker_started_at = ? WHERE id = ?"
#define KDD2_SQL_ADOPT_RUNS \
    "UPDATE task_runs SET worker_pid = ?, worker_started_at = ? WHERE id = ?"

/* ── universal seam context ── */
typedef struct kdd2_ctx {
    json_t *state;      /* owned; read-only here, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} kdd2_ctx_t;

/* ── basic helpers ── */
static bool kdd2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *kdd2_raise_kind(const json_t *v) {
    return kdd2_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool kdd2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *kdd2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool kdd2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static json_t *kdd2_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const char *kdd2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == (double)(long long)d) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static char *kdd2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
/* Python str() for scalars; integral numbers fold to ints. */
static char *kdd2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return kdd2_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return kdd2_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return kdd2_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return kdd2_strdup(buf);
        }
        default: return json_serialize(v);
    }
}
/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool kdd2_json_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return false;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER)
        return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL)
        return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type != b->type) return false;
    if (a->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!kdd2_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !kdd2_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}
static bool kdd2_str_eq(const json_t *v, const char *s) {
    const char *vs = kdd2_str(v);
    return vs && !strcmp(vs, s);
}

/* ── exception hierarchy (FULL builtin tree + quarry customs) ── */
typedef struct kdd2_exc_parent {
    const char *kind;
    const char *parents[4];   /* NULL-terminated */
} kdd2_exc_parent_t;

static const kdd2_exc_parent_t KDD2_EXC_PARENTS[] = {
    {"BaseException", {NULL}},
    {"Exception", {"BaseException", NULL}},
    {"SystemExit", {"BaseException", NULL}},
    {"KeyboardInterrupt", {"BaseException", NULL}},
    {"GeneratorExit", {"BaseException", NULL}},
    {"CancelledError", {"BaseException", NULL}},
    {"StopIteration", {"Exception", NULL}},
    {"StopAsyncIteration", {"Exception", NULL}},
    {"ArithmeticError", {"Exception", NULL}},
    {"FloatingPointError", {"ArithmeticError", NULL}},
    {"OverflowError", {"ArithmeticError", NULL}},
    {"ZeroDivisionError", {"ArithmeticError", NULL}},
    {"AssertionError", {"Exception", NULL}},
    {"AttributeError", {"Exception", NULL}},
    {"BufferError", {"Exception", NULL}},
    {"EOFError", {"Exception", NULL}},
    {"ImportError", {"Exception", NULL}},
    {"ModuleNotFoundError", {"ImportError", NULL}},
    {"LookupError", {"Exception", NULL}},
    {"IndexError", {"LookupError", NULL}},
    {"KeyError", {"LookupError", NULL}},
    {"MemoryError", {"Exception", NULL}},
    {"NameError", {"Exception", NULL}},
    {"UnboundLocalError", {"NameError", NULL}},
    {"OSError", {"Exception", NULL}},
    {"BlockingIOError", {"OSError", NULL}},
    {"ChildProcessError", {"OSError", NULL}},
    {"ConnectionError", {"OSError", NULL}},
    {"BrokenPipeError", {"ConnectionError", NULL}},
    {"ConnectionAbortedError", {"ConnectionError", NULL}},
    {"ConnectionRefusedError", {"ConnectionError", NULL}},
    {"ConnectionResetError", {"ConnectionError", NULL}},
    {"FileExistsError", {"OSError", NULL}},
    {"FileNotFoundError", {"OSError", NULL}},
    {"InterruptedError", {"OSError", NULL}},
    {"IsADirectoryError", {"OSError", NULL}},
    {"NotADirectoryError", {"OSError", NULL}},
    {"PermissionError", {"OSError", NULL}},
    {"ProcessLookupError", {"OSError", NULL}},
    {"TimeoutError", {"OSError", NULL}},
    {"ReferenceError", {"Exception", NULL}},
    {"RuntimeError", {"Exception", NULL}},
    {"NotImplementedError", {"RuntimeError", NULL}},
    {"RecursionError", {"RuntimeError", NULL}},
    {"SyntaxError", {"Exception", NULL}},
    {"IndentationError", {"SyntaxError", NULL}},
    {"TabError", {"IndentationError", NULL}},
    {"SystemError", {"Exception", NULL}},
    {"TypeError", {"Exception", NULL}},
    {"ValueError", {"Exception", NULL}},
    {"UnicodeError", {"ValueError", NULL}},
    {"UnicodeDecodeError", {"UnicodeError", NULL}},
    {"UnicodeEncodeError", {"UnicodeError", NULL}},
    {"UnicodeTranslateError", {"UnicodeError", NULL}},
    {"JSONDecodeError", {"ValueError", NULL}},
    {"Warning", {"Exception", NULL}},
    {"DeprecationWarning", {"Warning", NULL}},
    {"PendingDeprecationWarning", {"Warning", NULL}},
    {"RuntimeWarning", {"Warning", NULL}},
    {"SyntaxWarning", {"Warning", NULL}},
    {"UserWarning", {"Warning", NULL}},
    {"FutureWarning", {"Warning", NULL}},
    {"ImportWarning", {"Warning", NULL}},
    {"UnicodeWarning", {"Warning", NULL}},
    {"BytesWarning", {"Warning", NULL}},
    {"ResourceWarning", {"Warning", NULL}},
    {"EncodingWarning", {"Warning", NULL}},
    /* quarry customs */
    {"sqlite3.Error", {"Exception", NULL}},
};
#define KDD2_EXC_N (sizeof(KDD2_EXC_PARENTS) / sizeof(KDD2_EXC_PARENTS[0]))

static const kdd2_exc_parent_t *kdd2_exc_entry(const char *kind) {
    for (size_t i = 0; i < KDD2_EXC_N; i++)
        if (!strcmp(KDD2_EXC_PARENTS[i].kind, kind)) return &KDD2_EXC_PARENTS[i];
    return NULL;
}
static bool kdd2_exc_isa(const char *kind, const char *ancestor) {
    if (!kind) return false;
    if (!strcmp(kind, ancestor)) return true;
    const kdd2_exc_parent_t *e = kdd2_exc_entry(kind);
    if (!e) return !strcmp(ancestor, "Exception") || !strcmp(ancestor, "BaseException");
    for (size_t i = 0; i < 4 && e->parents[i]; i++)
        if (kdd2_exc_isa(e->parents[i], ancestor)) return true;
    return false;
}

/* ── the one universal seam (fail-closed) ── */
static json_t *kdd2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return kdd2_err_spec(type, msg);
}
static json_t *kdd2_call(kdd2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    /* drop null-valued arg keys on both sides (absent-vs-null parity) */
    json_t *clean = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(clean, args->c.keys[i], json_copy(v));
        }
    }
    json_free(args);
    json_set(ev, "args", clean);
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = kdd2_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = kdd2_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = kdd2_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* args builders (values borrowed; copied into the args object) */
static json_t *kdd2_args0(void) { return json_object(); }
static void kdd2_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void kdd2_anum(json_t *a, const char *k, double d) {
    json_set(a, k, json_number(d));
}
static void kdd2_astr(json_t *a, const char *k, const char *s) {
    if (s) json_set_hdr(a, k, s); else json_set(a, k, json_null());
}
static void kdd2_abool(json_t *a, const char *k, bool b) {
    json_set(a, k, json_bool(b));
}

/* ── error-message builders (py3.14 formats, probed live) ── */
static json_t *kdd2_attr_err(const char *type, const char *attr) {
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", type, attr);
    return kdd2_err_spec("AttributeError", msg);
}
/* Python repr() of an ASCII str (quote selection + escape set). */
static char *kdd2_repr_ascii(const char *s) {
    if (!s) s = "";
    int has_sq = strchr(s, '\'') != NULL;
    int has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *o = malloc(cap);
    if (!o) return kdd2_strdup("''");
    size_t oi = 0;
    o[oi++] = q;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\\') { o[oi++] = '\\'; o[oi++] = '\\'; }
        else if (c == (unsigned char)q) { o[oi++] = '\\'; o[oi++] = (char)c; }
        else if (c == '\n') { o[oi++] = '\\'; o[oi++] = 'n'; }
        else if (c == '\r') { o[oi++] = '\\'; o[oi++] = 'r'; }
        else if (c == '\t') { o[oi++] = '\\'; o[oi++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            o[oi++] = '\\'; o[oi++] = 'x';
            o[oi++] = "0123456789abcdef"[c >> 4];
            o[oi++] = "0123456789abcdef"[c & 15];
        } else o[oi++] = (char)c;
    }
    o[oi++] = q; o[oi] = '\0';
    return o;
}
static json_t *kdd2_key_err(const char *key) {
    char *rp = kdd2_repr_ascii(key);
    json_t *e = kdd2_err_spec("KeyError", rp);
    free(rp);
    return e;
}
/* missing-required-positional-argument TypeError (Python message format). */
static json_t *kdd2_missing_args(const json_t *args, const char *fname,
                                 const char *const *names, size_t n) {
    const char *missing[8];
    size_t nm = 0;
    for (size_t i = 0; i < n && nm < 8; i++)
        if (!json_has(args, names[i])) missing[nm++] = names[i];
    if (nm == 0) return NULL;
    char list[512]; size_t li = 0;
    for (size_t i = 0; i < nm; i++) {
        const char *sep = "";
        if (nm == 2 && i == 1) sep = " and ";
        else if (nm > 2 && i == nm - 1) sep = ", and ";
        else if (i > 0) sep = ", ";
        li += (size_t)snprintf(list + li, sizeof list - li, "%s'%s'", sep, missing[i]);
    }
    char msg[768];
    snprintf(msg, sizeof msg, "%s() missing %zu required positional argument%s: %s",
             fname, nm, nm == 1 ? "" : "s", list);
    return kdd2_err_spec("TypeError", msg);
}

/* ── Python semantics helpers (ASCII fixture domain) ── */
/* re \s and str.strip share the ASCII White_Space set (incl \x1c-\x1f). */
static bool kdd2_is_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F) || c == 0x20;
}
/* int()/float() strip a NARROWER set: White_Space minus \x1c-\x1f. */
static bool kdd2_is_int_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0D) || c == 0x20;
}
/* int() coercion -> double out; 0 ok, -1 with *err set. */
static int kdd2_py_int(const json_t *v, double *out, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = kdd2_err_spec("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return -1;
    }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (v->type == JSON_NUMBER) { *out = trunc(v->num_val); return 0; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && kdd2_is_int_ws((unsigned char)s[a])) a++;
        while (b > a && kdd2_is_int_ws((unsigned char)s[b - 1])) b--;
        size_t i = a;
        int neg = 0;
        if (i < b && (s[i] == '+' || s[i] == '-')) { neg = s[i] == '-'; i++; }
        double val = 0.0;
        int prev_digit = 0, any = 0, ok = 1;
        if (i >= b) ok = 0;
        for (; ok && i < b; i++) {
            char c = s[i];
            if (c >= '0' && c <= '9') { val = val * 10.0 + (c - '0'); prev_digit = 1; any = 1; }
            else if (c == '_') { if (!prev_digit) { ok = 0; break; } prev_digit = 0; }
            else { ok = 0; break; }
        }
        if (ok && any && prev_digit) { *out = neg ? -val : val; return 0; }
        char *rp = kdd2_repr_ascii(s);
        size_t ml = strlen(rp) + 64;
        char *msg = malloc(ml);
        if (msg) snprintf(msg, ml, "invalid literal for int() with base 10: %s", rp);
        *err = kdd2_err_spec("ValueError", msg ? msg : "");
        free(msg); free(rp);
        return -1;
    }
    char msg[160];
    snprintf(msg, sizeof msg,
        "int() argument must be a string, a bytes-like object or a real number, not '%s'",
        kdd2_type_name(v));
    *err = kdd2_err_spec("TypeError", msg);
    return -1;
}
/* str.strip -> owned malloc'd string (ASCII White_Space). */
static char *kdd2_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && kdd2_is_ws((unsigned char)s[a])) a++;
    while (b > a && kdd2_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}
/* str.splitlines (ASCII boundaries) -> array of owned string nodes. */
static json_t *kdd2_splitlines(const char *s) {
    json_t *arr = json_array();
    if (!s) s = "";
    size_t n = strlen(s), start = 0, i = 0;
    while (i < n) {
        char c = s[i];
        if (c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
            c == '\x1c' || c == '\x1d' || c == '\x1e') {
            size_t len = i - start;
            char *seg = malloc(len + 1);
            if (seg) { memcpy(seg, s + start, len); seg[len] = '\0'; }
            json_append(arr, json_string(seg ? seg : ""));
            free(seg);
            if (c == '\r' && i + 1 < n && s[i + 1] == '\n') i++;
            i++;
            start = i;
        } else i++;
    }
    if (start < n) {
        char *seg = malloc(n - start + 1);
        if (seg) { memcpy(seg, s + start, n - start); seg[n - start] = '\0'; }
        json_append(arr, json_string(seg ? seg : ""));
        free(seg);
    }
    return arr;
}
/* str.rfind(needle) -> index or -1 (empty needle -> len). */
static long kdd2_rfind(const char *s, const char *needle) {
    if (!s) s = "";
    if (!needle) needle = "";
    size_t n = strlen(s), m = strlen(needle);
    if (m == 0) return (long)n;
    if (m > n) return -1;
    for (long i = (long)(n - m); i >= 0; i--)
        if (!memcmp(s + i, needle, m)) return i;
    return -1;
}
/* str.startswith(prefix) */
static bool kdd2_startswith(const char *s, const char *prefix) {
    if (!s) s = "";
    if (!prefix) prefix = "";
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
/* last `cap` chars of an ASCII string -> owned malloc'd. */
static char *kdd2_tail(const char *s, size_t cap) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = n > cap ? n - cap : 0;
    char *o = malloc(n - a + 1);
    if (o) { memcpy(o, s + a, n - a); o[n - a] = '\0'; }
    return o;
}
/* slice [:cap] of an ASCII string -> owned malloc'd. */
static char *kdd2_head(const char *s, size_t cap) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t b = n < cap ? n : cap;
    char *o = malloc(b + 1);
    if (o) { memcpy(o, s, b); o[b] = '\0'; }
    return o;
}

/* ── _EXIT_TRAILER_RE: ^\[kanban\-worker\-exit\]\ rc=(\d+)\s*$ MULTILINE ──
 * At each line start (^ = pos 0 or after '\n'), match the literal trailer,
 * one+ digits, a greedy \s* run, then $ (end or before '\n'). Backtracking
 * \s* lands on the LAST '\n' inside the whitespace run (or the greedy end if
 * it is the string end). The match span excludes that terminating '\n'. */
static const char *KDD2_TRAILER_BYTES = KDD2_TRAILER;
/* try a match at line-start position `start`; on success set *d0,*d1 (digit
 * span) and *mend (match end) and return true. */
static bool kdd2_trailer_match(const char *s, size_t n, size_t start,
                               size_t *d0, size_t *d1, size_t *mend) {
    size_t tl = strlen(KDD2_TRAILER_BYTES);
    if (start + tl > n || memcmp(s + start, KDD2_TRAILER_BYTES, tl)) return false;
    size_t i = start + tl;
    size_t ds = i;
    while (i < n && s[i] >= '0' && s[i] <= '9') i++;
    if (i == ds) return false;             /* \d+ needs >= 1 digit */
    size_t de = i;
    while (i < n && kdd2_is_ws((unsigned char)s[i])) i++;
    size_t P = i;                          /* greedy \s* end */
    size_t end;
    if (P == n) end = n;                   /* $ at string end */
    else {
        /* largest q in [de, P-1] with s[q] == '\n' */
        long q = -1;
        for (size_t k = de; k < P; k++) if (s[k] == '\n') q = (long)k;
        if (q < 0) return false;           /* $ cannot match */
        end = (size_t)q;
    }
    *d0 = ds; *d1 = de; *mend = end;
    return true;
}
/* findall -> array of digit-group strings (last wins upstream). */
static json_t *kdd2_trailer_findall(const char *s) {
    json_t *arr = json_array();
    if (!s) s = "";
    size_t n = strlen(s), i = 0;
    while (i <= n) {
        if (i == 0 || s[i - 1] == '\n') {
            size_t d0, d1, mend;
            if (kdd2_trailer_match(s, n, i, &d0, &d1, &mend)) {
                char *g = malloc(d1 - d0 + 1);
                if (g) { memcpy(g, s + d0, d1 - d0); g[d1 - d0] = '\0'; }
                json_append(arr, json_string(g ? g : ""));
                free(g);
                i = mend;                  /* resume at match end */
                if (mend == n) break;
                continue;
            }
        }
        if (i == n) break;
        i++;
    }
    return arr;
}
/* sub("", s) -> owned malloc'd string with every match span removed. */
static char *kdd2_trailer_sub(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (!o) return kdd2_strdup("");
    size_t oi = 0, i = 0, copy_from = 0;
    while (i <= n) {
        if (i == 0 || s[i - 1] == '\n') {
            size_t d0, d1, mend;
            if (kdd2_trailer_match(s, n, i, &d0, &d1, &mend)) {
                memcpy(o + oi, s + copy_from, i - copy_from);
                oi += i - copy_from;
                copy_from = mend;
                i = mend;
                if (mend == n) break;
                continue;
            }
        }
        if (i == n) break;
        i++;
    }
    memcpy(o + oi, s + copy_from, n - copy_from);
    oi += n - copy_from;
    o[oi] = '\0';
    return o;
}

/* ── _LOG_CHROME: [box-drawing]+|☤\s*Hermes ──
 * The 13 box-drawing code points and the caduceus, as exact UTF-8 bytes.
 * Fixture DATA is ASCII so these never fire on it; the matcher still
 * recognises them. \s* uses the ASCII White_Space set. */
static const char *KDD2_BOX[] = {
    "\xE2\x94\x80", "\xE2\x94\x81", "\xE2\x95\x90", "\xE2\x95\xAD",
    "\xE2\x95\xAE", "\xE2\x95\xB0", "\xE2\x95\xAF", "\xE2\x94\x82",
    "\xE2\x94\x83", "\xE2\x94\x8C", "\xE2\x94\x90", "\xE2\x94\x94",
    "\xE2\x94\x98",
};
#define KDD2_NBOX (sizeof(KDD2_BOX) / sizeof(KDD2_BOX[0]))
static const char KDD2_CADUCEUS[] = "\xE2\x98\xA4";
static bool kdd2_is_box(const char *p, size_t rem) {
    if (rem < 3) return false;
    for (size_t i = 0; i < KDD2_NBOX; i++)
        if (!memcmp(p, KDD2_BOX[i], 3)) return true;
    return false;
}
/* sub("", ln) -> owned malloc'd. */
static char *kdd2_log_chrome_sub(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (!o) return kdd2_strdup("");
    size_t oi = 0, i = 0;
    while (i < n) {
        if (kdd2_is_box(s + i, n - i)) {
            size_t j = i;
            while (j + 3 <= n && kdd2_is_box(s + j, n - j)) j += 3;
            i = j;                          /* drop the run */
            continue;
        }
        if (n - i >= 3 && !memcmp(s + i, KDD2_CADUCEUS, 3)) {
            size_t j = i + 3;
            while (j < n && kdd2_is_ws((unsigned char)s[j])) j++;
            if (n - j >= 6 && !memcmp(s + j, "Hermes", 6)) {
                i = j + 6;                  /* drop ☤\s*Hermes */
                continue;
            }
        }
        o[oi++] = s[i++];
    }
    o[oi] = '\0';
    return o;
}

/* ── row / cursor access ── */
/* row[key] with KeyError on absent; TypeError on non-dict row. Borrowed. */
static const json_t *kdd2_row_col(const json_t *row, const char *key, json_t **err) {
    *err = NULL;
    if (!row || row->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "%s indices must be integers or slices, not str",
                 kdd2_type_name(row));
        *err = kdd2_err_spec("TypeError", msg);
        return NULL;
    }
    const json_t *v = json_obj_get(row, key);
    if (!v) { *err = kdd2_key_err(key); return NULL; }
    return v;
}
/* cursor spec {"rows":[...], "rowcount":N}: fetchall rows (borrowed array). */
static const json_t *kdd2_cursor_rows(const json_t *cur) {
    const json_t *r = (cur && cur->type == JSON_OBJECT) ? json_obj_get(cur, "rows") : NULL;
    return (r && r->type == JSON_ARRAY) ? r : NULL;
}
static json_t *kdd2_cursor_fetchone(const json_t *cur) {
    const json_t *rows = kdd2_cursor_rows(cur);
    if (rows && json_len(rows) > 0) return json_copy(json_get(rows, 0));
    return json_null();
}
static double kdd2_cursor_rowcount(const json_t *cur) {
    if (cur && cur->type == JSON_OBJECT) {
        const json_t *rc = json_obj_get(cur, "rowcount");
        if (rc && rc->type == JSON_NUMBER) return rc->num_val;
    }
    return -1.0;
}

/* ── _DeadWorker record normalisation ──
 * Validates the five required fields + rejects unknown keys (the dataclass
 * __init__ TypeErrors), then materialises the three flag defaults. `d` is
 * owned; returns NULL on success or a raise spec (caller frees d). */
static const char *KDD2_DEAD_REQ[] = {
    "kind", "code", "error_text", "event_kind", "event_payload"
};
static const char *KDD2_DEAD_FLAGS[] = {
    "protocol_violation", "rate_limited", "terminal_provider"
};
static json_t *kdd2_dead_check(const json_t *d) {
    if (!d || d->type != JSON_OBJECT)
        return kdd2_err_spec("TypeError", "dead worker must be a record dict");
    const char *missing[5]; size_t nm = 0;
    for (size_t i = 0; i < 5; i++)
        if (!json_has(d, KDD2_DEAD_REQ[i])) missing[nm++] = KDD2_DEAD_REQ[i];
    if (nm) {
        char list[256]; size_t li = 0;
        for (size_t i = 0; i < nm; i++) {
            const char *sep = "";
            if (nm == 2 && i == 1) sep = " and ";
            else if (nm > 2 && i == nm - 1) sep = ", and ";
            else if (i > 0) sep = ", ";
            li += (size_t)snprintf(list + li, sizeof list - li, "%s'%s'", sep, missing[i]);
        }
        char msg[384];
        snprintf(msg, sizeof msg,
                 "_DeadWorker.__init__() missing %zu required positional argument%s: %s",
                 nm, nm == 1 ? "" : "s", list);
        return kdd2_err_spec("TypeError", msg);
    }
    for (size_t i = 0; i < d->c.count; i++) {
        const char *k = d->c.keys[i];
        int known = 0;
        for (size_t j = 0; j < 5 && !known; j++) if (!strcmp(k, KDD2_DEAD_REQ[j])) known = 1;
        for (size_t j = 0; j < 3 && !known; j++) if (!strcmp(k, KDD2_DEAD_FLAGS[j])) known = 1;
        if (!known) {
            char msg[256];
            snprintf(msg, sizeof msg,
                     "_DeadWorker.__init__() got an unexpected keyword argument '%s'", k);
            return kdd2_err_spec("TypeError", msg);
        }
    }
    return NULL;
}
static void kdd2_dead_defaults(json_t *d) {
    for (size_t i = 0; i < 3; i++)
        if (!json_has(d, KDD2_DEAD_FLAGS[i]))
            json_set(d, KDD2_DEAD_FLAGS[i], json_bool(false));
}
static bool kdd2_dead_flag(const json_t *d, const char *flag) {
    return kdd2_truthy(json_obj_get(d, flag));
}
/* dead.run_outcome property */
static const char *kdd2_run_outcome_str(const json_t *d) {
    return kdd2_dead_flag(d, "rate_limited") ? "rate_limited" : "crashed";
}

/* unpack a 2-sequence -> borrowed kind/code; raise spec on bad shape. */
static json_t *kdd2_unpack2(const json_t *v, const json_t **a, const json_t **b) {
    if (!v || v->type != JSON_ARRAY) {
        char msg[160];
        snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", kdd2_type_name(v));
        return kdd2_err_spec("TypeError", msg);
    }
    size_t n = json_len(v);
    if (n < 2) {
        char msg[160];
        snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", n);
        return kdd2_err_spec("ValueError", msg);
    }
    if (n > 2) {
        char msg[160];
        snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)", n);
        return kdd2_err_spec("ValueError", msg);
    }
    *a = json_get(v, 0); *b = json_get(v, 1);
    return NULL;
}
/* unpack a 4-sequence (crash_details entry). */
static json_t *kdd2_unpack4(const json_t *v, const json_t **a, const json_t **b,
                            const json_t **c, const json_t **d) {
    if (!v || v->type != JSON_ARRAY) {
        char msg[160];
        snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", kdd2_type_name(v));
        return kdd2_err_spec("TypeError", msg);
    }
    size_t n = json_len(v);
    if (n < 4) {
        char msg[160];
        snprintf(msg, sizeof msg, "not enough values to unpack (expected 4, got %zu)", n);
        return kdd2_err_spec("ValueError", msg);
    }
    if (n > 4) {
        char msg[160];
        snprintf(msg, sizeof msg, "too many values to unpack (expected 4, got %zu)", n);
        return kdd2_err_spec("ValueError", msg);
    }
    *a = json_get(v, 0); *b = json_get(v, 1); *c = json_get(v, 2); *d = json_get(v, 3);
    return NULL;
}

/* ════════════════ describe_suppression ════════════════ */

/* PoP: kdd2_describe_suppression @ hermes_cli/kanban_db_dispatch.py:describe_suppression */
static json_t *kdd2_describe_suppression(kdd2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    static const char *req[] = { "results" };
    json_t *miss = kdd2_missing_args(args, "describe_suppression", req, 1);
    if (miss) return miss;
    const json_t *results = json_obj_get(args, "results");
    if (!results || results->type != JSON_ARRAY) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", kdd2_type_name(results));
        return kdd2_err_spec("TypeError", msg);
    }
    json_t *counts = json_object();
    json_t *pressure = NULL;             /* owned copy or NULL */
    for (size_t ri = 0; ri < json_len(results); ri++) {
        const json_t *res = json_get(results, ri);
        if (kdd2_is_null(res)) continue;
        if (res->type != JSON_OBJECT) {
            json_free(pressure); json_free(counts);
            return kdd2_attr_err(kdd2_type_name(res), "respawn_guarded");
        }
        static const char *attrs[] = {
            "respawn_guarded", "rate_limited", "skipped_locked", "memory_pressure"
        };
        for (size_t ai = 0; ai < 4; ai++) {
            if (!json_has(res, attrs[ai])) {
                json_free(pressure); json_free(counts);
                return kdd2_attr_err("types.SimpleNamespace", attrs[ai]);
            }
        }
        const json_t *rg = json_obj_get(res, "respawn_guarded");
        if (!rg || rg->type != JSON_ARRAY) {
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object is not iterable", kdd2_type_name(rg));
            json_free(pressure); json_free(counts);
            return kdd2_err_spec("TypeError", msg);
        }
        for (size_t ei = 0; ei < json_len(rg); ei++) {
            const json_t *el = json_get(rg, ei);
            const json_t *tid = NULL, *reason = NULL;
            json_t *ue = kdd2_unpack2(el, &tid, &reason);
            if (ue) { json_free(pressure); json_free(counts); return ue; }
            (void)tid;
            if (!reason || reason->type != JSON_STRING) {
                char msg[160];
                snprintf(msg, sizeof msg, "cannot use '%s' as a dict key (unhashable type: '%s')",
                         kdd2_type_name(reason), kdd2_type_name(reason));
                json_free(pressure); json_free(counts);
                return kdd2_err_spec("TypeError", msg);
            }
            const char *rk = reason->str_val ? reason->str_val : "";
            double cur = json_get_num(counts, rk, 0);
            json_set(counts, rk, json_number(cur + 1));
        }
        const json_t *rl = json_obj_get(res, "rate_limited");
        if (kdd2_truthy(rl)) {
            if (!rl || rl->type != JSON_ARRAY) {
                char msg[160];
                snprintf(msg, sizeof msg, "object of type '%s' has no len()", kdd2_type_name(rl));
                json_free(pressure); json_free(counts);
                return kdd2_err_spec("TypeError", msg);
            }
            double cur = json_get_num(counts, "rate_limited", 0);
            json_set(counts, "rate_limited", json_number(cur + (double)json_len(rl)));
        }
        if (kdd2_truthy(json_obj_get(res, "skipped_locked"))) {
            double cur = json_get_num(counts, "skipped_locked", 0);
            json_set(counts, "skipped_locked", json_number(cur + 1));
        }
        const json_t *mp = json_obj_get(res, "memory_pressure");
        if (kdd2_truthy(mp)) { json_free(pressure); pressure = json_copy(mp); }
    }
    /* parts = [f"{k}={v}" for k,v in sorted(counts.items())] */
    size_t nk = counts->c.count;
    const char **keys = malloc(nk * sizeof(char *));
    for (size_t i = 0; i < nk; i++) keys[i] = counts->c.keys[i];
    for (size_t i = 0; i < nk; i++)
        for (size_t j = i + 1; j < nk; j++)
            if (strcmp(keys[j], keys[i]) < 0) { const char *t = keys[i]; keys[i] = keys[j]; keys[j] = t; }
    json_t *parts = json_array();
    for (size_t i = 0; i < nk; i++) {
        char buf[256];
        snprintf(buf, sizeof buf, "%s=%lld", keys[i], (long long)json_get_num(counts, keys[i], 0));
        json_append(parts, json_string(buf));
    }
    free(keys);
    if (kdd2_truthy(pressure)) {
        char *ps = kdd2_pystr(pressure);
        char buf[320];
        snprintf(buf, sizeof buf, "memory_pressure=%s", ps);
        json_append(parts, json_string(buf));
        free(ps);
    }
    json_free(pressure); json_free(counts);
    /* ", ".join(parts) */
    size_t total = 0;
    for (size_t i = 0; i < json_len(parts); i++) {
        const char *s = kdd2_str(json_get(parts, i));
        total += strlen(s ? s : "") + (i ? 2 : 0);
    }
    char *o = malloc(total + 1);
    size_t oi = 0;
    for (size_t i = 0; i < json_len(parts); i++) {
        const char *s = kdd2_str(json_get(parts, i));
        if (!s) s = "";
        if (i) { o[oi++] = ','; o[oi++] = ' '; }
        size_t l = strlen(s); memcpy(o + oi, s, l); oi += l;
    }
    o[oi] = '\0';
    json_free(parts);
    json_t *ret = json_string(o);
    free(o);
    return ret;
}

/* ════════════════ exit-summary / log-noise / final-output ════════════════ */

/* PoP: kdd2_exit_summary_marker @ hermes_cli/kanban_db_dispatch.py:_exit_summary_marker */
static json_t *kdd2_exit_summary_marker(kdd2_ctx_t *ctx, const json_t *args) {
    (void)args;
    /* from agent.i18n import t; return t("cli.session.exit_resume_hint") */
    json_t *a = kdd2_args0(); kdd2_astr(a, "key", "cli.session.exit_resume_hint");
    return kdd2_call(ctx, "i18n.t", a);
}

/* PoP: kdd2_log_noise_prefixes @ hermes_cli/kanban_db_dispatch.py:_log_noise_prefixes */
static json_t *kdd2_log_noise_prefixes(kdd2_ctx_t *ctx, const json_t *args) {
    (void)args;
    /* from agent.i18n import t
     * return ("session_id:", "Query:", t("cli.chat.initializing_agent")) */
    json_t *a = kdd2_args0(); kdd2_astr(a, "key", "cli.chat.initializing_agent");
    json_t *t = kdd2_call(ctx, "i18n.t", a);
    if (kdd2_is_raise(t)) return t;
    json_t *arr = json_array();
    json_append(arr, json_string("session_id:"));
    json_append(arr, json_string("Query:"));
    json_append(arr, t);                 /* adopts t */
    return arr;
}

/* PoP: kdd2_worker_log_exit_code @ hermes_cli/kanban_db_dispatch.py:_worker_log_exit_code */
static json_t *kdd2_worker_log_exit_code(kdd2_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "task_id" };
    json_t *miss = kdd2_missing_args(args, "_worker_log_exit_code", req, 1);
    if (miss) return miss;
    const json_t *task_id = json_obj_get(args, "task_id");
    const json_t *board = json_obj_get(args, "board");
    /* try: raw = _kb.read_worker_log(task_id, tail_bytes=4000, board=board)
     * except Exception: return None */
    json_t *a = kdd2_args0();
    kdd2_aset(a, "task_id", task_id);
    kdd2_anum(a, "tail_bytes", 4000);
    kdd2_aset(a, "board", board);
    json_t *raw = kdd2_call(ctx, "kb.read_worker_log", a);
    if (kdd2_is_raise(raw)) {
        if (kdd2_exc_isa(kdd2_raise_kind(raw), "Exception")) { json_free(raw); return json_null(); }
        return raw;                      /* BaseException passes through */
    }
    /* matches = _EXIT_TRAILER_RE.findall(raw or "") */
    const char *s = "";
    if (kdd2_truthy(raw)) {
        if (raw->type != JSON_STRING) {
            char msg[160];
            snprintf(msg, sizeof msg, "expected string or bytes-like object, got '%s'",
                     kdd2_type_name(raw));
            json_free(raw);
            return kdd2_err_spec("TypeError", msg);
        }
        s = raw->str_val ? raw->str_val : "";
    }
    json_t *matches = kdd2_trailer_findall(s);
    json_free(raw);
    size_t nm = json_len(matches);
    if (nm == 0) { json_free(matches); return json_null(); }
    const char *last = kdd2_str(json_get(matches, nm - 1));
    double code = 0; json_t *err = NULL;
    json_t *lv = json_string(last ? last : "");
    int rc = kdd2_py_int(lv, &code, &err);
    json_free(lv); json_free(matches);
    if (rc != 0) return err;
    return json_number(code);
}

/* PoP: kdd2_worker_final_output @ hermes_cli/kanban_db_dispatch.py:_worker_final_output */
static json_t *kdd2_worker_final_output(kdd2_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "task_id" };
    json_t *miss = kdd2_missing_args(args, "_worker_final_output", req, 1);
    if (miss) return miss;
    const json_t *task_id = json_obj_get(args, "task_id");
    const json_t *board = json_obj_get(args, "board");
    /* try: raw = _kb.read_worker_log(...) except Exception: return "" */
    json_t *a = kdd2_args0();
    kdd2_aset(a, "task_id", task_id);
    kdd2_anum(a, "tail_bytes", 4000);
    kdd2_aset(a, "board", board);
    json_t *raw = kdd2_call(ctx, "kb.read_worker_log", a);
    if (kdd2_is_raise(raw)) {
        if (kdd2_exc_isa(kdd2_raise_kind(raw), "Exception")) { json_free(raw); return json_string(""); }
        return raw;
    }
    if (!kdd2_truthy(raw)) { json_free(raw); return json_string(""); }
    if (raw->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "expected string or bytes-like object, got '%s'",
                 kdd2_type_name(raw));
        json_free(raw);
        return kdd2_err_spec("TypeError", msg);
    }
    /* raw = _EXIT_TRAILER_RE.sub("", raw) */
    char *s = kdd2_trailer_sub(raw->str_val ? raw->str_val : "");
    json_free(raw);
    /* cut = raw.rfind(_exit_summary_marker()); if cut != -1: raw = raw[:cut] */
    json_t *marker = kdd2_call(ctx, "_exit_summary_marker", kdd2_args0());
    if (kdd2_is_raise(marker)) { free(s); return marker; }
    const char *ms = kdd2_str(marker);
    if (!ms) {
        /* rfind(non-str) -> TypeError "must be str, not <type>" */
        char msg[160];
        snprintf(msg, sizeof msg, "must be str, not %s", kdd2_type_name(marker));
        json_free(marker); free(s);
        return kdd2_err_spec("TypeError", msg);
    }
    long cut = kdd2_rfind(s, ms);
    json_free(marker);
    if (cut != -1) {
        char *ns = kdd2_head(s, (size_t)cut);
        free(s); s = ns;
    }
    /* lines = []; for ln in raw.splitlines():
     *     ln = _LOG_CHROME.sub("", ln).strip()
     *     if ln and not ln.startswith(_log_noise_prefixes()): lines.append(ln) */
    json_t *lines = kdd2_splitlines(s);
    free(s);
    json_t *kept = json_array();
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *ln0 = kdd2_str(json_get(lines, i));
        char *chrome = kdd2_log_chrome_sub(ln0 ? ln0 : "");
        char *ln = kdd2_strip(chrome);
        free(chrome);
        if (ln && ln[0]) {
            json_t *pref = kdd2_call(ctx, "_log_noise_prefixes", kdd2_args0());
            if (kdd2_is_raise(pref)) {
                free(ln); json_free(lines); json_free(kept);
                return pref;
            }
            bool noise = false;
            if (pref && pref->type == JSON_ARRAY) {
                for (size_t pi = 0; pi < json_len(pref); pi++) {
                    const json_t *pv = json_get(pref, pi);
                    if (!pv || pv->type != JSON_STRING) {
                        char msg[160];
                        snprintf(msg, sizeof msg,
                                 "tuple for startswith must only contain str, not %s",
                                 kdd2_type_name(pv));
                        json_free(pref); free(ln); json_free(lines); json_free(kept);
                        return kdd2_err_spec("TypeError", msg);
                    }
                    if (kdd2_startswith(ln, pv->str_val ? pv->str_val : "")) { noise = true; break; }
                }
            }
            json_free(pref);
            if (!noise) json_append(kept, json_string(ln));
        }
        free(ln);
    }
    json_free(lines);
    /* return " ".join(lines)[-400:] */
    size_t total = 0;
    for (size_t i = 0; i < json_len(kept); i++) {
        const char *t = kdd2_str(json_get(kept, i));
        total += strlen(t ? t : "") + (i ? 1 : 0);
    }
    char *joined = malloc(total + 1);
    size_t oi = 0;
    for (size_t i = 0; i < json_len(kept); i++) {
        const char *t = kdd2_str(json_get(kept, i));
        if (!t) t = "";
        if (i) joined[oi++] = ' ';
        size_t l = strlen(t); memcpy(joined + oi, t, l); oi += l;
    }
    joined[oi] = '\0';
    json_free(kept);
    char *tail = kdd2_tail(joined, 400);
    free(joined);
    json_t *ret = json_string(tail);
    free(tail);
    return ret;
}

/* ════════════════ _DeadWorker.run_outcome ════════════════ */

/* PoP: kdd2_run_outcome @ hermes_cli/kanban_db_dispatch.py:run_outcome */
static json_t *kdd2_run_outcome(kdd2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    static const char *req[] = { "dead" };
    json_t *miss = kdd2_missing_args(args, "run_outcome", req, 1);
    if (miss) return miss;
    const json_t *dead = json_obj_get(args, "dead");
    json_t *chk = kdd2_dead_check(dead);
    if (chk) return chk;
    /* return "rate_limited" if self.rate_limited else "crashed" */
    return json_string(kdd2_run_outcome_str(dead));
}

/* ════════════════ _classify_dead_worker_exit ════════════════ */

/* PoP: kdd2_classify_dead_worker_exit @ hermes_cli/kanban_db_dispatch.py:_classify_dead_worker_exit */
static json_t *kdd2_classify_dead_worker_exit(kdd2_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "pid", "claimer" };
    json_t *miss = kdd2_missing_args(args, "_classify_dead_worker_exit", req, 2);
    if (miss) return miss;
    const json_t *pid = json_obj_get(args, "pid");
    const json_t *claimer = json_obj_get(args, "claimer");
    const json_t *exit_code = json_obj_get(args, "exit_code");
    const json_t *task_id = json_obj_get(args, "task_id");
    const json_t *board = json_obj_get(args, "board");

    /* kind, code = _classify_worker_exit(pid) */
    json_t *a = kdd2_args0(); kdd2_aset(a, "pid", pid);
    json_t *r = kdd2_call(ctx, "_classify_worker_exit", a);
    if (kdd2_is_raise(r)) return r;
    const json_t *kv = NULL, *cv = NULL;
    json_t *ue = kdd2_unpack2(r, &kv, &cv);
    if (ue) { json_free(r); return ue; }
    json_t *kind = json_copy(kv), *code = json_copy(cv);
    json_free(r);

    if (!kdd2_is_null(exit_code)) {
        /* kind, code = _exit_code_kind(int(exit_code)) */
        double ic = 0; json_t *ierr = NULL;
        if (kdd2_py_int(exit_code, &ic, &ierr) != 0) { json_free(kind); json_free(code); return ierr; }
        json_t *a2 = kdd2_args0(); kdd2_anum(a2, "code", ic);
        json_t *r2 = kdd2_call(ctx, "_exit_code_kind", a2);
        if (kdd2_is_raise(r2)) { json_free(kind); json_free(code); return r2; }
        const json_t *k2 = NULL, *c2 = NULL;
        json_t *ue2 = kdd2_unpack2(r2, &k2, &c2);
        if (ue2) { json_free(r2); json_free(kind); json_free(code); return ue2; }
        json_free(kind); json_free(code);
        kind = json_copy(k2); code = json_copy(c2);
        json_free(r2);
    } else if (kdd2_str_eq(kind, "unknown") && kdd2_truthy(task_id)) {
        /* logged = _worker_log_exit_code(task_id, board=board) */
        json_t *a2 = kdd2_args0();
        kdd2_aset(a2, "task_id", task_id);
        kdd2_aset(a2, "board", board);
        json_t *logged = kdd2_call(ctx, "_worker_log_exit_code", a2);
        if (kdd2_is_raise(logged)) { json_free(kind); json_free(code); return logged; }
        if (!kdd2_is_null(logged)) {
            json_t *a3 = kdd2_args0(); kdd2_aset(a3, "code", logged);
            json_t *r3 = kdd2_call(ctx, "_exit_code_kind", a3);
            if (kdd2_is_raise(r3)) { json_free(logged); json_free(kind); json_free(code); return r3; }
            const json_t *k3 = NULL, *c3 = NULL;
            json_t *ue3 = kdd2_unpack2(r3, &k3, &c3);
            if (ue3) { json_free(r3); json_free(logged); json_free(kind); json_free(code); return ue3; }
            json_free(kind); json_free(code);
            kind = json_copy(k3); code = json_copy(c3);
            json_free(r3);
        }
        json_free(logged);
    }

    /* build the _DeadWorker record per kind */
    char *pids = kdd2_pystr(pid);
    char *codes = kdd2_pystr(code);
    json_t *dead = json_object();
    json_set(dead, "kind", kind);        /* adopts */
    json_set(dead, "code", code);        /* adopts */
    json_set(dead, "event_payload", json_object());
    json_t *ep = json_obj_get(dead, "event_payload");
    kdd2_aset(ep, "pid", pid);
    kdd2_aset(ep, "claimer", claimer);

    if (kdd2_str_eq(kind, "clean_exit")) {
        json_set_hdr(dead, "error_text", KDD2_PROTOCOL_VIOLATION_ERROR);
        json_set_hdr(dead, "event_kind", "protocol_violation");
        kdd2_aset(ep, "exit_code", code);
        json_set(ep, "protocol_violation", json_bool(true));
        json_set(dead, "protocol_violation", json_bool(true));
    } else if (kdd2_str_eq(kind, "rate_limited")) {
        char buf[256];
        snprintf(buf, sizeof buf,
                 "pid %s exited rate-limited (quota wall) \xE2\x80\x94 requeued without counting a failure",
                 pids);
        json_set_hdr(dead, "error_text", buf);
        json_set_hdr(dead, "event_kind", "rate_limited");
        kdd2_aset(ep, "exit_code", code);
        json_set(dead, "rate_limited", json_bool(true));
    } else if (kdd2_str_eq(kind, "terminal_provider")) {
        char buf[384];
        snprintf(buf, sizeof buf,
                 "pid %s exited on a terminal provider error (exit %s): the provider rejected "
                 "this profile's credential or model \xE2\x80\x94 fix the configuration, then unblock.",
                 pids, codes);
        json_set_hdr(dead, "error_text", buf);
        json_set_hdr(dead, "event_kind", "crashed");
        kdd2_aset(ep, "exit_kind", kind);
        kdd2_aset(ep, "exit_code", code);
        json_set(ep, "terminal_provider", json_bool(true));
        json_set(dead, "terminal_provider", json_bool(true));
    } else {
        char buf[256];
        if (kdd2_str_eq(kind, "nonzero_exit"))
            snprintf(buf, sizeof buf, "pid %s exited with code %s", pids, codes);
        else if (kdd2_str_eq(kind, "signaled"))
            snprintf(buf, sizeof buf, "pid %s killed by signal %s", pids, codes);
        else
            snprintf(buf, sizeof buf, "pid %s not alive", pids);
        json_set_hdr(dead, "error_text", buf);
        json_set_hdr(dead, "event_kind", "crashed");
        if (!kdd2_is_null(code) && !kdd2_str_eq(kind, "unknown")) {
            kdd2_aset(ep, "exit_kind", kind);
            kdd2_aset(ep, "exit_code", code);
        }
    }
    free(pids); free(codes);
    kdd2_dead_defaults(dead);
    return dead;
}

/* ════════════════ _classify_dead_worker ════════════════ */

/* PoP: kdd2_classify_dead_worker @ hermes_cli/kanban_db_dispatch.py:_classify_dead_worker */
static json_t *kdd2_classify_dead_worker(kdd2_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "pid", "claimer" };
    json_t *miss = kdd2_missing_args(args, "_classify_dead_worker", req, 2);
    if (miss) return miss;
    const json_t *pid = json_obj_get(args, "pid");
    const json_t *claimer = json_obj_get(args, "claimer");
    const json_t *exit_code = json_obj_get(args, "exit_code");
    const json_t *task_id = json_obj_get(args, "task_id");
    const json_t *board = json_obj_get(args, "board");

    /* dead = _classify_dead_worker_exit(pid, claimer, exit_code, task_id=, board=) */
    json_t *a = kdd2_args0();
    kdd2_aset(a, "pid", pid);
    kdd2_aset(a, "claimer", claimer);
    kdd2_aset(a, "exit_code", exit_code);
    kdd2_aset(a, "task_id", task_id);
    kdd2_aset(a, "board", board);
    json_t *dead = kdd2_call(ctx, "_classify_dead_worker_exit", a);
    if (kdd2_is_raise(dead)) return dead;
    json_t *chk = kdd2_dead_check(dead);
    if (chk) { json_free(dead); return chk; }
    kdd2_dead_defaults(dead);

    /* if task_id and not dead.rate_limited: */
    if (kdd2_truthy(task_id) && !kdd2_dead_flag(dead, "rate_limited")) {
        json_t *a2 = kdd2_args0();
        kdd2_aset(a2, "task_id", task_id);
        kdd2_aset(a2, "board", board);
        json_t *wo = kdd2_call(ctx, "_worker_final_output", a2);
        if (kdd2_is_raise(wo)) { json_free(dead); return wo; }
        if (kdd2_truthy(wo)) {
            const json_t *et = json_obj_get(dead, "error_text");
            if (!et || et->type != JSON_STRING) {
                char msg[192];
                snprintf(msg, sizeof msg, "unsupported operand type(s) for +=: '%s' and 'str'",
                         kdd2_type_name(et));
                json_free(wo); json_free(dead);
                return kdd2_err_spec("TypeError", msg);
            }
            const char *wos = kdd2_str(wo);
            char *rep = kdd2_repr_ascii(wos ? wos : "");
            const char *ets = et->str_val ? et->str_val : "";
            size_t ml = strlen(ets) + strlen(KDD2_WORKER_OUTPUT_SEP) + strlen(rep) + 1;
            char *ne = malloc(ml);
            if (ne) snprintf(ne, ml, "%s%s%s", ets, KDD2_WORKER_OUTPUT_SEP, rep);
            json_set_hdr(dead, "error_text", ne ? ne : ets);
            free(ne); free(rep);
            json_t *ep = json_obj_get(dead, "event_payload");
            if (!ep || ep->type != JSON_OBJECT) {
                char msg[160];
                snprintf(msg, sizeof msg, "'%s' object does not support item assignment",
                         kdd2_type_name(ep));
                json_free(wo); json_free(dead);
                return kdd2_err_spec("TypeError", msg);
            }
            json_set(ep, "worker_output", json_copy(wo));
        }
        json_free(wo);
    }
    return dead;
}

/* ════════════════ _reclaim_dead_workers ════════════════ */

static json_t *kdd2_new_sweep(void) {
    json_t *s = json_object();
    json_set(s, "crashed", json_array());
    json_set(s, "rate_limited", json_array());
    json_set(s, "crash_details", json_array());
    json_set(s, "exited_hook_payloads", json_array());
    return s;
}
/* body of the with-block; *sweep_out always materialised (partial on raise).
 * Returns NULL on success or a raise spec. */
static json_t *kdd2_reclaim_body(kdd2_ctx_t *ctx, const json_t *args, json_t **sweep_out) {
    json_t *sweep = kdd2_new_sweep();
    *sweep_out = sweep;
    const json_t *board = json_obj_get(args, "board");

    /* rows = conn.execute(SELECT_RUNNING).fetchall() */
    json_t *a = kdd2_args0();
    kdd2_astr(a, "sql", KDD2_SQL_SELECT_RUNNING);
    json_set(a, "params", json_array());
    json_t *cur = kdd2_call(ctx, "conn.execute", a);
    if (kdd2_is_raise(cur)) return cur;
    json_t *rows = json_copy(kdd2_cursor_rows(cur));
    json_free(cur);
    if (!rows) rows = json_array();

    /* host_prefix = _kb._host_prefix() */
    json_t *hp = kdd2_call(ctx, "kb._host_prefix", kdd2_args0());
    if (kdd2_is_raise(hp)) { json_free(rows); return hp; }
    const char *hps = kdd2_str(hp);
    if (!hps) hps = "";

    for (size_t ri = 0; ri < json_len(rows); ri++) {
        const json_t *row = json_get(rows, ri);
        json_t *ke = NULL;
        const json_t *claim_lock = kdd2_row_col(row, "claim_lock", &ke);
        if (ke) { json_free(hp); json_free(rows); return ke; }
        /* lock = row["claim_lock"] or "" */
        const char *lock = kdd2_truthy(claim_lock) ? kdd2_str(claim_lock) : "";
        if (!lock) lock = "";
        if (!kdd2_startswith(lock, hps)) continue;

        /* started_at = _kb._to_epoch(_kb._row_get(row, "started_at")) */
        const json_t *id_v = kdd2_row_col(row, "id", &ke);
        if (ke) { json_free(hp); json_free(rows); return ke; }
        json_t *a1 = kdd2_args0();
        kdd2_aset(a1, "row", row); kdd2_astr(a1, "col", "started_at");
        json_t *rg = kdd2_call(ctx, "kb._row_get", a1);
        if (kdd2_is_raise(rg)) { json_free(hp); json_free(rows); return rg; }
        json_t *a2 = kdd2_args0(); kdd2_aset(a2, "value", rg);
        json_t *started_at = kdd2_call(ctx, "kb._to_epoch", a2);
        json_free(rg);
        if (kdd2_is_raise(started_at)) { json_free(hp); json_free(rows); return started_at; }

        /* if started_at is not None and time.time() - started_at < grace: continue */
        if (!kdd2_is_null(started_at)) {
            if (started_at->type != JSON_NUMBER) {
                char msg[192];
                snprintf(msg, sizeof msg,
                         "unsupported operand type(s) for -: 'float' and '%s'",
                         kdd2_type_name(started_at));
                json_free(started_at); json_free(hp); json_free(rows);
                return kdd2_err_spec("TypeError", msg);
            }
            json_t *tt = kdd2_call(ctx, "time.time", kdd2_args0());
            if (kdd2_is_raise(tt)) { json_free(started_at); json_free(hp); json_free(rows); return tt; }
            double diff = (tt->type == JSON_NUMBER ? tt->num_val : 0.0) - started_at->num_val;
            json_free(tt);
            json_t *gr = kdd2_call(ctx, "kb._resolve_crash_grace_seconds", kdd2_args0());
            if (kdd2_is_raise(gr)) { json_free(started_at); json_free(hp); json_free(rows); return gr; }
            double grv = (gr && gr->type == JSON_NUMBER) ? gr->num_val : 0.0;
            json_free(gr);
            if (diff < grv) { json_free(started_at); continue; }
        }
        json_free(started_at);

        /* if _worker_alive(row["worker_pid"], _kb._row_get(row, "worker_started_at")): continue */
        const json_t *wpid = kdd2_row_col(row, "worker_pid", &ke);
        if (ke) { json_free(hp); json_free(rows); return ke; }
        json_t *a3 = kdd2_args0();
        kdd2_aset(a3, "row", row); kdd2_astr(a3, "col", "worker_started_at");
        json_t *rg2 = kdd2_call(ctx, "kb._row_get", a3);
        if (kdd2_is_raise(rg2)) { json_free(hp); json_free(rows); return rg2; }
        json_t *a4 = kdd2_args0();
        kdd2_aset(a4, "pid", wpid); kdd2_aset(a4, "started_at", rg2);
        json_t *wa = kdd2_call(ctx, "_worker_alive", a4);
        json_free(rg2);
        if (kdd2_is_raise(wa)) { json_free(hp); json_free(rows); return wa; }
        bool alive = kdd2_truthy(wa);
        json_free(wa);
        if (alive) continue;

        /* if owner_reclaim_blocked(conn, row["id"], crash_sweep=True): continue */
        json_t *a5 = kdd2_args0();
        kdd2_aset(a5, "task_id", id_v); kdd2_abool(a5, "crash_sweep", true);
        json_t *orb = kdd2_call(ctx, "owner_reclaim_blocked", a5);
        if (kdd2_is_raise(orb)) { json_free(hp); json_free(rows); return orb; }
        bool blocked = kdd2_truthy(orb);
        json_free(orb);
        if (blocked) continue;

        /* pid = int(row["worker_pid"]) */
        double pid_d = 0; json_t *pid_err = NULL;
        if (kdd2_py_int(wpid, &pid_d, &pid_err) != 0) { json_free(hp); json_free(rows); return pid_err; }
        json_t *pid = json_number(pid_d);

        /* exit_code = managed_run_exit_code(conn, row["id"], pid, row["claim_lock"]) */
        json_t *a6 = kdd2_args0();
        kdd2_aset(a6, "task_id", id_v); kdd2_aset(a6, "pid", pid); kdd2_aset(a6, "claim_lock", claim_lock);
        json_t *mrc = kdd2_call(ctx, "managed_run_exit_code", a6);
        if (kdd2_is_raise(mrc)) { json_free(pid); json_free(hp); json_free(rows); return mrc; }

        /* dead = _classify_dead_worker(pid, row["claim_lock"], exit_code, task_id=row["id"], board=board) */
        json_t *a7 = kdd2_args0();
        kdd2_aset(a7, "pid", pid); kdd2_aset(a7, "claimer", claim_lock);
        kdd2_aset(a7, "exit_code", mrc); kdd2_aset(a7, "task_id", id_v); kdd2_aset(a7, "board", board);
        json_t *dead = kdd2_call(ctx, "_classify_dead_worker", a7);
        json_free(mrc);
        if (kdd2_is_raise(dead)) { json_free(pid); json_free(hp); json_free(rows); return dead; }
        json_t *chk = kdd2_dead_check(dead);
        if (chk) { json_free(dead); json_free(pid); json_free(hp); json_free(rows); return chk; }
        kdd2_dead_defaults(dead);

        /* apply_attempt_note(conn, row["id"], pid, dead) */
        json_t *a8 = kdd2_args0();
        kdd2_aset(a8, "task_id", id_v); kdd2_aset(a8, "pid", pid); kdd2_aset(a8, "dead", dead);
        json_t *aan = kdd2_call(ctx, "apply_attempt_note", a8);
        if (kdd2_is_raise(aan)) { json_free(dead); json_free(pid); json_free(hp); json_free(rows); return aan; }
        json_free(aan);

        /* retry_status = _kb._retry_status_for_run(conn, row["id"]) */
        json_t *a9 = kdd2_args0(); kdd2_aset(a9, "task_id", id_v);
        json_t *rs = kdd2_call(ctx, "kb._retry_status_for_run", a9);
        if (kdd2_is_raise(rs)) { json_free(dead); json_free(pid); json_free(hp); json_free(rows); return rs; }

        /* dead.event_payload["retry_status"] = retry_status */
        json_t *ep = json_obj_get(dead, "event_payload");
        if (!ep || ep->type != JSON_OBJECT) {
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object does not support item assignment", kdd2_type_name(ep));
            json_free(rs); json_free(dead); json_free(pid); json_free(hp); json_free(rows);
            return kdd2_err_spec("TypeError", msg);
        }
        json_set(ep, "retry_status", json_copy(rs));

        /* cur = conn.execute(UPDATE_TASKS, (retry_status, row["id"], pid, row["claim_lock"])) */
        json_t *a10 = kdd2_args0();
        kdd2_astr(a10, "sql", KDD2_SQL_UPDATE_TASKS);
        json_t *params = json_array();
        json_append(params, json_copy(rs));
        json_append(params, json_copy(id_v));
        json_append(params, json_copy(pid));
        json_append(params, claim_lock ? json_copy(claim_lock) : json_null());
        json_set(a10, "params", params);
        json_t *cur2 = kdd2_call(ctx, "conn.execute", a10);
        if (kdd2_is_raise(cur2)) { json_free(rs); json_free(dead); json_free(pid); json_free(hp); json_free(rows); return cur2; }
        double rc = kdd2_cursor_rowcount(cur2);
        json_free(cur2);
        if (rc != 1.0) { json_free(rs); json_free(dead); json_free(pid); continue; }

        /* run_id = _kb._end_run(conn, row["id"], outcome=dead.run_outcome,
         *                       status=dead.run_outcome, error=dead.error_text,
         *                       metadata=dict(dead.event_payload)) */
        const char *outcome = kdd2_run_outcome_str(dead);
        const json_t *et = json_obj_get(dead, "error_text");
        json_t *a11 = kdd2_args0();
        kdd2_aset(a11, "task_id", id_v);
        kdd2_astr(a11, "outcome", outcome);
        kdd2_astr(a11, "status", outcome);
        kdd2_aset(a11, "error", et);
        json_set(a11, "metadata", json_copy(ep));
        json_t *run_id = kdd2_call(ctx, "kb._end_run", a11);
        if (kdd2_is_raise(run_id)) { json_free(rs); json_free(dead); json_free(pid); json_free(hp); json_free(rows); return run_id; }

        /* _kb._append_event(conn, row["id"], dead.event_kind, dead.event_payload, run_id=run_id) */
        const json_t *ek = json_obj_get(dead, "event_kind");
        json_t *a12 = kdd2_args0();
        kdd2_aset(a12, "task_id", id_v);
        kdd2_aset(a12, "kind", ek);
        kdd2_aset(a12, "payload", ep);
        kdd2_aset(a12, "run_id", run_id);
        json_t *ae = kdd2_call(ctx, "kb._append_event", a12);
        if (kdd2_is_raise(ae)) { json_free(run_id); json_free(rs); json_free(dead); json_free(pid); json_free(hp); json_free(rows); return ae; }
        json_free(ae);

        /* sweep.exited_hook_payloads.append({...}) */
        const json_t *assignee = kdd2_row_col(row, "assignee", &ke);
        if (ke) { json_free(run_id); json_free(rs); json_free(dead); json_free(pid); json_free(hp); json_free(rows); return ke; }
        const json_t *dk = json_obj_get(dead, "kind");
        const json_t *dc = json_obj_get(dead, "code");
        json_t *hook = json_object();
        kdd2_aset(hook, "task_id", id_v);
        kdd2_aset(hook, "assignee", assignee);
        kdd2_aset(hook, "run_id", run_id);
        kdd2_aset(hook, "worker_pid", pid);
        kdd2_aset(hook, "exit_kind", dk);
        kdd2_aset(hook, "exit_code", dc);
        kdd2_astr(hook, "outcome", outcome);
        kdd2_aset(hook, "retry_status", rs);
        json_append(json_obj_get(sweep, "exited_hook_payloads"), hook);

        /* if dead.rate_limited or dead.protocol_violation: stamp last_failure_error */
        if (kdd2_dead_flag(dead, "rate_limited") || kdd2_dead_flag(dead, "protocol_violation")) {
            const char *ets = kdd2_str(et);
            char *err500 = kdd2_head(ets ? ets : "", 500);
            json_t *a13 = kdd2_args0();
            kdd2_astr(a13, "sql", KDD2_SQL_UPDATE_LFE);
            json_t *p13 = json_array();
            json_append(p13, json_string(err500));
            json_append(p13, json_copy(id_v));
            json_set(a13, "params", p13);
            free(err500);
            json_t *cur3 = kdd2_call(ctx, "conn.execute", a13);
            if (kdd2_is_raise(cur3)) { json_free(run_id); json_free(rs); json_free(dead); json_free(pid); json_free(hp); json_free(rows); return cur3; }
            json_free(cur3);
        }
        if (kdd2_dead_flag(dead, "rate_limited")) {
            json_append(json_obj_get(sweep, "rate_limited"), json_copy(id_v));
        } else {
            json_append(json_obj_get(sweep, "crashed"), json_copy(id_v));
            json_t *detail = json_array();
            json_append(detail, json_copy(id_v));
            json_append(detail, json_copy(pid));
            json_append(detail, claim_lock ? json_copy(claim_lock) : json_null());
            json_append(detail, dead);   /* adopts dead (mutated) */
            json_append(json_obj_get(sweep, "crash_details"), detail);
            dead = NULL;
        }
        json_free(run_id); json_free(rs); json_free(pid);
        if (dead) json_free(dead);
    }
    json_free(hp); json_free(rows);
    return NULL;
}

/* PoP: kdd2_reclaim_dead_workers @ hermes_cli/kanban_db_dispatch.py:_reclaim_dead_workers */
static json_t *kdd2_reclaim_dead_workers(kdd2_ctx_t *ctx, const json_t *args) {
    /* with _kb.write_txn(conn): body; return sweep */
    json_t *enter = kdd2_call(ctx, "kb.write_txn.__enter__", kdd2_args0());
    if (kdd2_is_raise(enter)) return enter;
    json_free(enter);
    json_t *sweep = NULL;
    json_t *body = kdd2_reclaim_body(ctx, args, &sweep);
    json_t *xa = kdd2_args0();
    if (kdd2_is_raise(body)) kdd2_astr(xa, "exc", kdd2_raise_kind(body));
    json_t *ex = kdd2_call(ctx, "kb.write_txn.__exit__", xa);
    if (kdd2_is_raise(ex)) { json_free(body); json_free(sweep); return ex; }
    if (kdd2_is_raise(body)) {
        if (kdd2_truthy(ex)) { json_free(ex); json_free(body); return sweep; }  /* suppressed */
        json_free(ex); json_free(sweep); return body;
    }
    json_free(ex);
    return sweep;
}

/* ════════════════ _account_crashes ════════════════ */

/* PoP: kdd2_account_crashes @ hermes_cli/kanban_db_dispatch.py:_account_crashes */
static json_t *kdd2_account_crashes(kdd2_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "crash_details" };
    json_t *miss = kdd2_missing_args(args, "_account_crashes", req, 1);
    if (miss) return miss;
    const json_t *details = json_obj_get(args, "crash_details");
    if (!details || details->type != JSON_ARRAY) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", kdd2_type_name(details));
        return kdd2_err_spec("TypeError", msg);
    }
    json_t *auto_blocked = json_array();
    json_t *fp_counts = json_object();

    /* first loop: fp_counts over every dead.error_text */
    for (size_t i = 0; i < json_len(details); i++) {
        const json_t *el = json_get(details, i);
        const json_t *tid = NULL, *pid = NULL, *claimer = NULL, *dead = NULL;
        json_t *ue = kdd2_unpack4(el, &tid, &pid, &claimer, &dead);
        if (ue) { json_free(auto_blocked); json_free(fp_counts); return ue; }
        json_t *chk = kdd2_dead_check(dead);
        if (chk) { json_free(auto_blocked); json_free(fp_counts); return chk; }
        const json_t *et = json_obj_get(dead, "error_text");
        json_t *a = kdd2_args0(); kdd2_aset(a, "error_text", et);
        json_t *fp = kdd2_call(ctx, "_error_fingerprint", a);
        if (kdd2_is_raise(fp)) { json_free(auto_blocked); json_free(fp_counts); return fp; }
        if (!fp || fp->type != JSON_STRING) {
            char msg[160];
            snprintf(msg, sizeof msg, "cannot use '%s' as a dict key (unhashable type: '%s')",
                     kdd2_type_name(fp), kdd2_type_name(fp));
            json_free(fp); json_free(auto_blocked); json_free(fp_counts);
            return kdd2_err_spec("TypeError", msg);
        }
        const char *fk = fp->str_val ? fp->str_val : "";
        json_set(fp_counts, fk, json_number(json_get_num(fp_counts, fk, 0) + 1));
        json_free(fp);
    }

    /* second loop: per-detail breaker decision */
    for (size_t i = 0; i < json_len(details); i++) {
        const json_t *el = json_get(details, i);
        const json_t *tid = NULL, *pid = NULL, *claimer = NULL, *dead = NULL;
        json_t *ue = kdd2_unpack4(el, &tid, &pid, &claimer, &dead);
        if (ue) { json_free(auto_blocked); json_free(fp_counts); return ue; }
        json_t *chk = kdd2_dead_check(dead);
        if (chk) { json_free(auto_blocked); json_free(fp_counts); return chk; }
        const json_t *et = json_obj_get(dead, "error_text");
        json_t *tripped = NULL;

        if (kdd2_dead_flag(dead, "protocol_violation")) {
            /* streak = _protocol_violation_streak(conn, tid) */
            json_t *a = kdd2_args0(); kdd2_aset(a, "task_id", tid);
            json_t *st = kdd2_call(ctx, "_protocol_violation_streak", a);
            if (kdd2_is_raise(st)) { json_free(auto_blocked); json_free(fp_counts); return st; }
            double streak = (st && st->type == JSON_NUMBER) ? st->num_val : 0.0;
            json_free(st);
            /* trow = conn.execute(SELECT_MAX_RETRIES, (tid,)).fetchone() */
            json_t *a2 = kdd2_args0();
            kdd2_astr(a2, "sql", KDD2_SQL_SELECT_MAX_RETRIES);
            json_t *p2 = json_array(); json_append(p2, json_copy(tid));
            json_set(a2, "params", p2);
            json_t *cur = kdd2_call(ctx, "conn.execute", a2);
            if (kdd2_is_raise(cur)) { json_free(auto_blocked); json_free(fp_counts); return cur; }
            json_t *trow = kdd2_cursor_fetchone(cur);
            json_free(cur);
            if (kdd2_is_null(trow)) { json_free(trow); continue; }   /* task deleted */
            /* task_override = _kb._row_get(trow, "max_retries") */
            json_t *a3 = kdd2_args0();
            kdd2_aset(a3, "row", trow); kdd2_astr(a3, "col", "max_retries");
            json_t *ov = kdd2_call(ctx, "kb._row_get", a3);
            json_free(trow);
            if (kdd2_is_raise(ov)) { json_free(auto_blocked); json_free(fp_counts); return ov; }
            double violation_limit = KDD2_PROTOCOL_VIOLATION_FAILURE_LIMIT;
            if (!kdd2_is_null(ov)) {
                double vl = 0; json_t *verr = NULL;
                if (kdd2_py_int(ov, &vl, &verr) != 0) {
                    json_free(ov); json_free(auto_blocked); json_free(fp_counts); return verr;
                }
                violation_limit = vl;
            }
            json_free(ov);
            if (streak < violation_limit) continue;   /* below budget */
            /* force_trip with the violation budget */
            json_t *extra = json_object();
            kdd2_aset(extra, "pid", pid);
            kdd2_aset(extra, "claimer", claimer);
            kdd2_anum(extra, "protocol_violations", streak);
            kdd2_anum(extra, "protocol_violation_limit", violation_limit);
            json_t *a4 = kdd2_args0();
            kdd2_aset(a4, "task_id", tid);
            kdd2_aset(a4, "error", et);
            kdd2_astr(a4, "outcome", "crashed");
            kdd2_anum(a4, "failure_limit", violation_limit);
            kdd2_abool(a4, "force_trip", true);
            kdd2_abool(a4, "release_claim", false);
            kdd2_abool(a4, "end_run", false);
            json_set(a4, "event_payload_extra", extra);
            tripped = kdd2_call(ctx, "_record_task_failure", a4);
        } else if (kdd2_dead_flag(dead, "terminal_provider")) {
            json_t *extra = json_object();
            kdd2_aset(extra, "pid", pid);
            kdd2_aset(extra, "claimer", claimer);
            json_set(extra, "terminal_provider", json_bool(true));
            json_t *a4 = kdd2_args0();
            kdd2_aset(a4, "task_id", tid);
            kdd2_aset(a4, "error", et);
            kdd2_astr(a4, "outcome", "crashed");
            kdd2_abool(a4, "force_trip", true);
            kdd2_abool(a4, "release_claim", false);
            kdd2_abool(a4, "end_run", false);
            json_set(a4, "event_payload_extra", extra);
            tripped = kdd2_call(ctx, "_record_task_failure", a4);
        } else {
            /* is_systemic = fp_counts.get(_error_fingerprint(error_text), 0) >= 3 */
            json_t *a = kdd2_args0(); kdd2_aset(a, "error_text", et);
            json_t *fp = kdd2_call(ctx, "_error_fingerprint", a);
            if (kdd2_is_raise(fp)) { json_free(auto_blocked); json_free(fp_counts); return fp; }
            const char *fk = kdd2_str(fp);
            double cnt = fk ? json_get_num(fp_counts, fk, 0) : 0;
            json_free(fp);
            bool is_systemic = cnt >= 3.0;
            json_t *extra = json_object();
            kdd2_aset(extra, "pid", pid);
            kdd2_aset(extra, "claimer", claimer);
            if (is_systemic) json_set(extra, "sticky", json_bool(true));
            json_t *a4 = kdd2_args0();
            kdd2_aset(a4, "task_id", tid);
            kdd2_aset(a4, "error", et);
            kdd2_astr(a4, "outcome", "crashed");
            if (is_systemic) kdd2_anum(a4, "failure_limit", 1);  /* else None -> dropped */
            kdd2_abool(a4, "release_claim", false);
            kdd2_abool(a4, "end_run", false);
            json_set(a4, "event_payload_extra", extra);
            tripped = kdd2_call(ctx, "_record_task_failure", a4);
        }
        if (kdd2_is_raise(tripped)) { json_free(auto_blocked); json_free(fp_counts); return tripped; }
        if (kdd2_truthy(tripped)) json_append(auto_blocked, json_copy(tid));
        json_free(tripped);
    }
    json_free(fp_counts);
    return auto_blocked;
}

/* ════════════════ _is_handoff_event ════════════════ */

/* PoP: kdd2_is_handoff_event @ hermes_cli/kanban_db_dispatch.py:_is_handoff_event */
static json_t *kdd2_is_handoff_event(kdd2_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "kind", "payload" };
    json_t *miss = kdd2_missing_args(args, "_is_handoff_event", req, 2);
    if (miss) return miss;
    const json_t *kind = json_obj_get(args, "kind");
    const json_t *payload = json_obj_get(args, "payload");
    /* if kind != "assigned": return True */
    if (!kdd2_str_eq(kind, "assigned")) return json_bool(true);
    /* data = _kb._json_or(payload, {}) */
    json_t *a = kdd2_args0();
    kdd2_aset(a, "value", payload);
    json_set(a, "default", json_object());
    json_t *data = kdd2_call(ctx, "kb._json_or", a);
    if (kdd2_is_raise(data)) return data;
    /* if not isinstance(data, dict) or data.get("source") == "kanban.default_assignee":
     *     return False */
    if (!data || data->type != JSON_OBJECT) { json_free(data); return json_bool(false); }
    const json_t *src = json_obj_get(data, "source");
    if (kdd2_str_eq(src, "kanban.default_assignee")) { json_free(data); return json_bool(false); }
    /* to = data.get("assignee"); return bool(to) and "from" in data and data["from"] != to */
    const json_t *to = json_obj_get(data, "assignee");
    bool result = kdd2_truthy(to) && json_has(data, "from") &&
                  !kdd2_json_eq(json_obj_get(data, "from"), to);
    json_free(data);
    return json_bool(result);
}

/* ════════════════ adopt_worker_pid ════════════════ */

/* body of the with-block; *ret_out is the post-with return (default True).
 * Returns NULL on success or a raise spec. */
static json_t *kdd2_adopt_body(kdd2_ctx_t *ctx, const json_t *args,
                               const json_t *started_at, json_t **ret_out) {
    *ret_out = json_bool(true);
    const json_t *task_id = json_obj_get(args, "task_id");
    const json_t *run_id = json_obj_get(args, "run_id");
    const json_t *pid = json_obj_get(args, "pid");

    /* row = conn.execute(SELECT, (task_id,)).fetchone() */
    json_t *a = kdd2_args0();
    kdd2_astr(a, "sql", KDD2_SQL_ADOPT_SELECT);
    json_t *p = json_array(); json_append(p, json_copy(task_id));
    json_set(a, "params", p);
    json_t *cur = kdd2_call(ctx, "conn.execute", a);
    if (kdd2_is_raise(cur)) return cur;
    json_t *row = kdd2_cursor_fetchone(cur);
    json_free(cur);

    /* if row is None or row["status"] != "running" or row["current_run_id"] != int(run_id):
     *     return False */
    if (kdd2_is_null(row)) { json_free(row); json_free(*ret_out); *ret_out = json_bool(false); return NULL; }
    json_t *ke = NULL;
    const json_t *status = kdd2_row_col(row, "status", &ke);
    if (ke) { json_free(row); return ke; }
    if (!kdd2_str_eq(status, "running")) {
        json_free(row); json_free(*ret_out); *ret_out = json_bool(false); return NULL;
    }
    const json_t *crid = kdd2_row_col(row, "current_run_id", &ke);
    if (ke) { json_free(row); return ke; }
    double irun = 0; json_t *rerr = NULL;
    if (kdd2_py_int(run_id, &irun, &rerr) != 0) { json_free(row); return rerr; }
    json_t *irun_v = json_number(irun);
    if (!kdd2_json_eq(crid, irun_v)) {
        json_free(irun_v); json_free(row); json_free(*ret_out); *ret_out = json_bool(false); return NULL;
    }
    json_free(irun_v);

    /* if row["worker_pid"] is None and (row["claim_lock"] or "").startswith(_kb._host_prefix()): */
    const json_t *wpid = kdd2_row_col(row, "worker_pid", &ke);
    if (ke) { json_free(row); return ke; }
    if (kdd2_is_null(wpid)) {
        const json_t *claim_lock = kdd2_row_col(row, "claim_lock", &ke);
        if (ke) { json_free(row); return ke; }
        const char *lock = kdd2_truthy(claim_lock) ? kdd2_str(claim_lock) : "";
        if (!lock) lock = "";
        json_t *hp = kdd2_call(ctx, "kb._host_prefix", kdd2_args0());
        if (kdd2_is_raise(hp)) { json_free(row); return hp; }
        const char *hps = kdd2_str(hp);
        bool match = kdd2_startswith(lock, hps ? hps : "");
        json_free(hp);
        if (match) {
            double ipid = 0; json_t *perr = NULL;
            if (kdd2_py_int(pid, &ipid, &perr) != 0) { json_free(row); return perr; }
            json_t *ipid_v = json_number(ipid);
            /* conn.execute(UPDATE_TASKS, (int(pid), started_at, task_id)) */
            json_t *a1 = kdd2_args0();
            kdd2_astr(a1, "sql", KDD2_SQL_ADOPT_TASKS);
            json_t *p1 = json_array();
            json_append(p1, json_copy(ipid_v));
            json_append(p1, json_copy(started_at));
            json_append(p1, json_copy(task_id));
            json_set(a1, "params", p1);
            json_t *c1 = kdd2_call(ctx, "conn.execute", a1);
            if (kdd2_is_raise(c1)) { json_free(ipid_v); json_free(row); return c1; }
            json_free(c1);
            /* conn.execute(UPDATE_RUNS, (int(pid), started_at, int(run_id))) */
            double irun2 = 0; json_t *rerr2 = NULL;
            if (kdd2_py_int(run_id, &irun2, &rerr2) != 0) { json_free(ipid_v); json_free(row); return rerr2; }
            json_t *a2 = kdd2_args0();
            kdd2_astr(a2, "sql", KDD2_SQL_ADOPT_RUNS);
            json_t *p2 = json_array();
            json_append(p2, json_copy(ipid_v));
            json_append(p2, json_copy(started_at));
            json_append(p2, json_number(irun2));
            json_set(a2, "params", p2);
            json_t *c2 = kdd2_call(ctx, "conn.execute", a2);
            if (kdd2_is_raise(c2)) { json_free(ipid_v); json_free(row); return c2; }
            json_free(c2);
            /* _kb._append_event(conn, task_id, "worker_registered",
             *                   {"pid": int(pid), "started_at": started_at}, run_id=int(run_id)) */
            double irun3 = 0; json_t *rerr3 = NULL;
            if (kdd2_py_int(run_id, &irun3, &rerr3) != 0) { json_free(ipid_v); json_free(row); return rerr3; }
            json_t *payload = json_object();
            kdd2_aset(payload, "pid", ipid_v);
            kdd2_aset(payload, "started_at", started_at);
            json_t *a3 = kdd2_args0();
            kdd2_aset(a3, "task_id", task_id);
            kdd2_astr(a3, "kind", "worker_registered");
            json_set(a3, "payload", payload);
            kdd2_anum(a3, "run_id", irun3);
            json_t *ae = kdd2_call(ctx, "kb._append_event", a3);
            json_free(ipid_v);
            if (kdd2_is_raise(ae)) { json_free(row); return ae; }
            json_free(ae);
        }
    }
    json_free(row);
    return NULL;
}

/* PoP: kdd2_adopt_worker_pid @ hermes_cli/kanban_db_dispatch.py:adopt_worker_pid */
static json_t *kdd2_adopt_worker_pid(kdd2_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "task_id", "run_id", "pid" };
    json_t *miss = kdd2_missing_args(args, "adopt_worker_pid", req, 3);
    if (miss) return miss;
    const json_t *pid = json_obj_get(args, "pid");
    /* started_at = _process_fingerprint(int(pid)) or UNVERIFIED_WORKER_FINGERPRINT */
    double ipid = 0; json_t *perr = NULL;
    if (kdd2_py_int(pid, &ipid, &perr) != 0) return perr;
    json_t *a = kdd2_args0(); kdd2_anum(a, "pid", ipid);
    json_t *fp = kdd2_call(ctx, "_process_fingerprint", a);
    if (kdd2_is_raise(fp)) return fp;
    json_t *started_at = kdd2_truthy(fp) ? json_copy(fp) : json_string(KDD2_UNVERIFIED);
    json_free(fp);

    /* with _kb.write_txn(conn): body; return True */
    json_t *enter = kdd2_call(ctx, "kb.write_txn.__enter__", kdd2_args0());
    if (kdd2_is_raise(enter)) { json_free(started_at); return enter; }
    json_free(enter);
    json_t *ret_out = NULL;
    json_t *body = kdd2_adopt_body(ctx, args, started_at, &ret_out);
    json_free(started_at);
    json_t *xa = kdd2_args0();
    if (kdd2_is_raise(body)) kdd2_astr(xa, "exc", kdd2_raise_kind(body));
    json_t *ex = kdd2_call(ctx, "kb.write_txn.__exit__", xa);
    if (kdd2_is_raise(ex)) { json_free(body); json_free(ret_out); return ex; }
    if (kdd2_is_raise(body)) {
        if (kdd2_truthy(ex)) { json_free(ex); json_free(body); return ret_out; }  /* suppressed -> return True */
        json_free(ex); json_free(ret_out); return body;
    }
    json_free(ex);
    return ret_out;
}
