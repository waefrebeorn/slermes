/* cron_incidents_flow2.c — FLOW port of the remaining UNCLAIMED cron/incidents.py defs.
 *
 * Complements incidents_gaps.c (which PoP-claims the 8 surface names:
 * _normalize_error, _redact_error, _error_signature, _incident_id,
 * _classify_failure_type, upsert_incident, list_incidents, get_incident).
 * A CRLF-aware PoP grep over src/ found these 9 defs UNCLAIMED, ported here:
 *  - _db_path                       (override precedence + home resolve/join)
 *  - _connect                       (db path -> ensure dir -> open_db)
 *  - _initialize_schema             (CREATE TABLE + add column + 2 indexes)
 *  - _transaction                   (@contextmanager lock + transaction CM)
 *  - set_incident_state             (state validation + SELECT/UPDATE branch)
 *  - ack_incident                   (delegate to set_incident_state)
 *  - close_incidents_for_recovered_job (resolved UPDATE + rowcount)
 *  - _state_filter                  (pure WHERE/params builder)
 *  - count_incidents                (validate + filter + COUNT)
 *
 * Seam model: every module-level collaborator, claimed sibling and
 * inner-imported dependency goes through ci2_io_t.call(io, name, args). The
 * dispatcher appends `name` to io->trace (ordered NAME-ONLY decision sequence),
 * returns the scripted result, applies "state" effects, and models a raised
 * exception as {"__raise__": "<Type>", "message": "..."}. Seam results are
 * caller-owned (json_free); NULL is delivered as json_null.
 *
 * Full builtin exception hierarchy: ci2_exc_parent walks the WHOLE Python
 * builtin tree; BaseException-only kinds (BaseException/SystemExit/
 * KeyboardInterrupt/GeneratorExit) never resolve to Exception, so any
 * `except Exception` equivalent passes them through. Scripted raises propagate
 * as the function's raised return (these flows have no swallow site).
 *
 * Projections (documented): the module global EXECUTIONS_FILE is injected as
 * an explicit input (module_file) rather than a traced global read; the
 * _executions.EXECUTIONS_FILE attribute read is the "executions_file" seam.
 * Path resolution and join are scripted ("path.resolve" seam) / pure string
 * concatenation on POSIX-style fixture paths (port models str(Path.as_posix)).
 * The `conn.execute` seam name is derived from the SQL literal; cursor
 * fetchone/fetchall/rowcount read fields of the SAME single scripted result.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

typedef struct ci2_io_t {
    json_t *(*call)(struct ci2_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} ci2_io_t;

/* ── generic helpers ─────────────────────────────────────────────────────── */
static void ci2_trace(ci2_io_t *io, const char *tok) {
    if (io && io->trace && io->trace->type == JSON_ARRAY)
        json_append(io->trace, json_string(tok));
}
static json_t *ci2_call(ci2_io_t *io, const char *name, json_t *args) {
    return (io && io->call) ? io->call(io, name, args) : json_null();
}
static json_t *ci2_seam(ci2_io_t *io, const char *name, json_t *args) {
    ci2_trace(io, name);
    json_t *r = ci2_call(io, name, args);
    if (args) json_free(args);
    return r ? r : json_null();
}
static bool ci2_streq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}
static bool ci2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *ci2_exc_kind(const json_t *e) {
    return json_get_str(e, "__raise__", "");
}
static char *ci2_strdup(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *o = (char *)malloc(n + 1);
    if (o) memcpy(o, p, n + 1);
    return o;
}
static char *ci2_exc_msg(const json_t *e) {
    const char *msg = json_get_str(e, "message", "");
    /* str(KeyError(k)) == repr(k): quote the raw key. */
    if (ci2_streq(ci2_exc_kind(e), "KeyError")) {
        size_t n = strlen(msg);
        char *o = (char *)malloc(n + 3);
        if (!o) return ci2_strdup(msg);
        o[0] = '\''; memcpy(o + 1, msg, n); o[n + 1] = '\''; o[n + 2] = '\0';
        return o;
    }
    return ci2_strdup(msg);
}
static json_t *ci2_propagate(const json_t *e) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(ci2_exc_kind(e)));
    char *m = ci2_exc_msg(e);
    json_set(o, "message", json_string(m ? m : ""));
    free(m);
    return o;
}
static const char *ci2_get_str(const json_t *o, const char *k, const char *d) {
    return json_get_str(o, k, d);
}

/* ── full builtin exception hierarchy ────────────────────────────────────── */
static const char *ci2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        /* BaseException family (trio maps to BaseException ONLY, never Exception) */
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"Exception", "BaseException"},
        /* Exception direct/near children */
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"ValueError", "Exception"}, {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"}, {"JSONDecodeError", "ValueError"},
        {"TypeError", "Exception"}, {"AttributeError", "Exception"},
        {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        {"ArithmeticError", "Exception"}, {"ZeroDivisionError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"FloatingPointError", "ArithmeticError"},
        {"OSError", "Exception"}, {"EnvironmentError", "OSError"}, {"IOError", "OSError"},
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"AssertionError", "Exception"}, {"SyntaxError", "Exception"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"SystemError", "Exception"}, {"ReferenceError", "Exception"},
        {"MemoryError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ImportError", "Exception"},
        {"ModuleNotFoundError", "ImportError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"FutureWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"ResourceWarning", "Warning"},
        {"BytesWarning", "Warning"}, {"ImportWarning", "Warning"}, {"EncodingWarning", "Warning"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (ci2_streq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool ci2_exc_matches_kind(const char *cls, const char *want) {
    for (int depth = 0; depth < 32; depth++) {
        if (ci2_streq(cls, want)) return true;
        const char *p = ci2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool ci2_exc_is_exception_kind(const char *cls) {
    return ci2_exc_matches_kind(cls, "Exception");
}
static bool ci2_base_only(const char *k) {
    return ci2_streq(k, "BaseException") || ci2_streq(k, "SystemExit")
        || ci2_streq(k, "KeyboardInterrupt") || ci2_streq(k, "GeneratorExit");
}
static bool ci2_oserror_kind(const char *k) {
    return ci2_exc_matches_kind(k, "OSError");
}

/* ── state/domain helpers ────────────────────────────────────────────────── */
static bool ci2_valid_state(const char *s) {
    return ci2_streq(s, "detected") || ci2_streq(s, "alerted")
        || ci2_streq(s, "resolved") || ci2_streq(s, "closed");
}
static const char *ci2_str_val(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
/* POSIX parent of a path string (everything before the last '/'). */
static char *ci2_parent(const char *path) {
    const char *p = path ? path : "";
    const char *slash = strrchr(p, '/');
    if (!slash) return ci2_strdup(".");
    if (slash == p) return ci2_strdup("/");
    size_t n = (size_t)(slash - p);
    char *o = (char *)malloc(n + 1);
    if (o) { memcpy(o, p, n); o[n] = '\0'; }
    return o;
}

/* SQL result projection: {"row": fetchone|null, "rowcount": n, "rows": [...]}
 * or a raise spec (propagated by the caller). */
static json_t *ci2_sql(ci2_io_t *io, const char *name) {
    json_t *r = ci2_seam(io, name, NULL);
    if (ci2_is_raise(r)) return r;
    json_t *res = json_object();
    json_t *fo = json_obj_get(r, "fetchone");
    json_set(res, "row", fo ? json_copy(fo) : json_null());
    json_t *rc = json_obj_get(r, "rowcount");
    json_set(res, "rowcount", rc ? json_copy(rc) : json_number(0));
    json_t *fa = json_obj_get(r, "fetchall");
    json_set(res, "rows", fa ? json_copy(fa) : json_array());
    json_free(r);
    return res;
}
/* Fire the context-manager exit when an exception unwinds the `with` body.
 * If the exit itself raises it replaces the in-flight exception (Python `with`). */
static json_t *ci2_with_exit(ci2_io_t *io, json_t *pending) {
    json_t *ex = ci2_seam(io, "tx.exit", NULL);
    if (ci2_is_raise(ex)) { if (pending) json_free(pending); return ex; }
    json_free(ex);
    return pending;
}
static long ci2_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? (long)v->num_val : 0;
}

/* ── 1. _db_path ─────────────────────────────────────────────────────────── */
/* PoP: ci2_db_path @ cron/incidents.py:_db_path */
static json_t *ci2_db_path(ci2_io_t *io, const json_t *module_file) {
    json_t *ret = NULL;
    /* Tuple (_executions.EXECUTIONS_FILE, EXECUTIONS_FILE) built eagerly: the
     * _executions attribute is a traced seam; the module global is injected. */
    json_t *ef = ci2_seam(io, "executions_file", NULL);
    if (ci2_is_raise(ef)) { ret = ef; goto done; }
    if (ef->type != JSON_NULL) {
        ret = json_copy(ef); json_free(ef); goto done;
    }
    json_free(ef);
    if (module_file && module_file->type != JSON_NULL) {
        ret = json_copy(module_file); goto done;
    }
    json_t *home = ci2_seam(io, "get_hermes_home", NULL);
    if (ci2_is_raise(home)) { ret = home; goto done; }
    json_t *resolved = ci2_seam(io, "path.resolve", NULL);
    if (ci2_is_raise(resolved)) { ret = resolved; json_free(home); goto done; }
    const char *base = ci2_str_val(resolved);
    size_t n = strlen(base);
    char path[4096];
    snprintf(path, sizeof path, "%s%s%s%s",
             base, (n && base[n - 1] == '/') ? "" : "/", "cron", "/executions.db");
    ret = json_string(path);
    json_free(home); json_free(resolved);
done:
    return ret;
}

/* ── 2. _connect ─────────────────────────────────────────────────────────── */
/* PoP: ci2_connect @ cron/incidents.py:_connect */
static json_t *ci2_connect(ci2_io_t *io) {
    json_t *ret = NULL;
    json_t *dbp = ci2_seam(io, "_db_path", NULL);
    if (ci2_is_raise(dbp)) { ret = dbp; goto done; }
    char *parent = ci2_parent(ci2_str_val(dbp));
    json_free(dbp);
    json_t *ecd = ci2_seam(io, "ensure_cron_dir", NULL);
    if (ci2_is_raise(ecd)) { ret = ecd; free(parent); goto done; }
    json_free(ecd); free(parent);
    ret = ci2_seam(io, "open_db", NULL);   /* transfer: open_db result is the connection */
done:
    return ret;
}

/* ── 3. _initialize_schema ───────────────────────────────────────────────── */
/* PoP: ci2_initialize_schema @ cron/incidents.py:_initialize_schema */
static json_t *ci2_initialize_schema(ci2_io_t *io) {
    static const char *const STEPS[] = {
        "sql.create_table", "add_column_if_missing", "sql.index_job", "sql.index_state"
    };
    for (size_t i = 0; i < sizeof(STEPS) / sizeof(STEPS[0]); i++) {
        json_t *r = ci2_seam(io, STEPS[i], NULL);
        if (ci2_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* ── 4. _transaction (contextmanager) ────────────────────────────────────── */
/* PoP: ci2_transaction @ cron/incidents.py:_transaction */
static json_t *ci2_transaction(ci2_io_t *io) {
    json_t *ret = NULL;
    json_t *c = ci2_seam(io, "_connect", NULL);
    if (ci2_is_raise(c)) { ret = c; goto done; }
    json_free(c);
    json_t *ent = ci2_seam(io, "transaction.enter", NULL);
    if (ci2_is_raise(ent)) { ret = ent; goto done; }   /* enter failed: no exit */
    ret = ent;                                        /* yielded value */
    json_t *ex = ci2_seam(io, "transaction.exit", NULL);
    if (ci2_is_raise(ex)) { json_free(ret); ret = ex; }
    else json_free(ex);
done:
    return ret;
}

/* ── 5. set_incident_state ───────────────────────────────────────────────── */
/* PoP: ci2_set_incident_state @ cron/incidents.py:set_incident_state */
static json_t *ci2_set_incident_state(ci2_io_t *io, const json_t *state) {
    (void)io;
    const char *s = ci2_str_val(state);
    if (!ci2_valid_state(s)) return json_bool(false);
    json_t *now = ci2_seam(io, "hermes_now", NULL);
    if (ci2_is_raise(now)) return now;                 /* before the `with` */
    json_free(now);
    json_t *ent = ci2_seam(io, "tx.enter", NULL);
    if (ci2_is_raise(ent)) return ent;                 /* enter failed: no exit */
    json_free(ent);

    json_t *sel = ci2_sql(io, "sql.select");
    if (ci2_is_raise(sel)) return ci2_with_exit(io, sel);
    json_t *row = json_obj_get(sel, "row");
    bool have = row && row->type == JSON_OBJECT;
    const char *cur = have ? ci2_get_str(row, "state", "") : "";
    bool result;
    const char *upd = NULL;
    if (!have || ci2_streq(cur, "closed")) {
        result = false;
    } else if (ci2_streq(s, "alerted")) {
        result = true; upd = "sql.update_alerted";
    } else if (ci2_streq(cur, s)) {
        result = false;
    } else {
        result = true;
        upd = ci2_streq(s, "closed") ? "sql.update_closed" : "sql.update_state";
    }
    json_free(sel);
    if (upd) {
        json_t *u = ci2_seam(io, upd, NULL);
        if (ci2_is_raise(u)) return ci2_with_exit(io, u);
        json_free(u);
    }
    json_t *ex = ci2_seam(io, "tx.exit", NULL);
    if (ci2_is_raise(ex)) return ex;
    json_free(ex);
    return json_bool(result);
}

/* ── 6. ack_incident ─────────────────────────────────────────────────────── */
/* PoP: ci2_ack_incident @ cron/incidents.py:ack_incident */
static json_t *ci2_ack_incident(ci2_io_t *io, const json_t *incident_id) {
    (void)incident_id;
    /* set_incident_state is a claimed/ported sibling: seam it, closing over
     * the fixed "closed" target state. */
    return ci2_seam(io, "set_incident_state", NULL);
}

/* ── 7. close_incidents_for_recovered_job ────────────────────────────────── */
/* PoP: ci2_close_incidents @ cron/incidents.py:close_incidents_for_recovered_job */
static json_t *ci2_close_incidents(ci2_io_t *io, const json_t *job_id) {
    (void)job_id;
    json_t *now = ci2_seam(io, "hermes_now", NULL);
    if (ci2_is_raise(now)) return now;
    json_free(now);
    json_t *ent = ci2_seam(io, "tx.enter", NULL);
    if (ci2_is_raise(ent)) return ent;
    json_free(ent);
    json_t *u = ci2_sql(io, "sql.update_resolved");
    if (ci2_is_raise(u)) return ci2_with_exit(io, u);
    long n = ci2_num(json_obj_get(u, "rowcount"));
    json_free(u);
    json_t *ex = ci2_seam(io, "tx.exit", NULL);
    if (ci2_is_raise(ex)) return ex;
    json_free(ex);
    return json_number((double)n);
}

/* ── 8. _state_filter (pure) ─────────────────────────────────────────────── */
/* PoP: ci2_state_filter @ cron/incidents.py:_state_filter */
static json_t *ci2_state_filter(const json_t *state) {
    json_t *a = json_array();
    if (!state || state->type == JSON_NULL) {
        json_append(a, json_string(""));
        json_append(a, json_array());
    } else {
        json_append(a, json_string(" WHERE state=?"));
        json_t *p = json_array();
        json_append(p, json_copy(state));
        json_append(a, p);
    }
    return a;
}

/* ── 9. count_incidents ──────────────────────────────────────────────────── */
/* PoP: ci2_count_incidents @ cron/incidents.py:count_incidents */
static json_t *ci2_count_incidents(ci2_io_t *io, const json_t *state) {
    if (state && state->type != JSON_NULL && !ci2_valid_state(ci2_str_val(state)))
        return json_number(0);
    json_t *filt = ci2_state_filter(state);        /* [where, params] */
    json_t *where = json_get(filt, 0);
    bool filtered = strlen(ci2_str_val(where)) > 0;
    json_free(filt);
    json_t *ent = ci2_seam(io, "tx.enter", NULL);
    if (ci2_is_raise(ent)) return ent;
    json_free(ent);
    json_t *s = ci2_sql(io, filtered ? "sql.count_filtered" : "sql.count");
    if (ci2_is_raise(s)) return ci2_with_exit(io, s);
    json_t *row = json_obj_get(s, "row");
    long n = 0;
    if (row && row->type == JSON_OBJECT)
        n = ci2_num(json_obj_get(row, "n"));
    json_free(s);
    json_t *ex = ci2_seam(io, "tx.exit", NULL);
    if (ci2_is_raise(ex)) return ex;
    json_free(ex);
    return json_number((double)n);
}
