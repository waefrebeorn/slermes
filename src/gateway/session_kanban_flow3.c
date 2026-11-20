/*
 * session_kanban_flow3.c — FLOW port of the REMAINDER of the session_kanban
 * collaborator surface (hermes-agent).
 *
 * MODULE RESOLUTION: gateway/session_kanban.py itself is fully claimed by
 * src/gateway/tui_methods_kanban_flow.c (all 22 defs incl. the six nested
 * closures). session_kanban_flow2.c already ported the turn-recovery engine
 * (agent/kanban_turn_recovery.py) and the worker exit policy
 * (hermes_cli/turn_exit.py). The unclaimed REMAINDER of the flow is the
 * kanban DB / dispatch / process-identity / session-key collaborator cluster
 * that session_kanban.py drives but tmk + flow2 SEAM. Those are the REAL
 * defining modules; every PoP below credits them (the scanner only credits
 * the defining module). This mirrors flow2's resolution of a named-but-absent
 * module to its real definers.
 *
 * Ports the DECISION SEQUENCES — branch order, seam call order, exception
 * paths and fail-closed returns — of:
 *   hermes_cli/kanban_db.py: _normalize_board_slug, _require_slug, _board_path,
 *     kanban_db_path, workspaces_root, get_task, _json_or, _json_or_null,
 *     _append_event, _current_run_id, _latest_event, _json_dict, _row_get,
 *     _retry_status_for_run, goal_run_status.
 *   hermes_cli/kanban_db_dispatch.py: _worker_terminal_timeout_env,
 *     _process_fingerprint, _pid_recycled, _worker_alive.
 *   hermes_cli/process_identity.py: process_birth, worker_birth.
 *   session_keys.py: profile_from_session_key.
 *
 * Every live external call enters through ONE universal seam:
 * sk3_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. Ported siblings are SEAMED inside flows (each is exercised by its
 * own direct cases, where the real body runs on both sides).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and sk3_-prefixed.
 *
 * Seam name -> live call (args are informational projections):
 *   _normalize_board_slug    _normalize_board_slug(slug)            [sibling]
 *   _board_path              _board_path(env_var, board, default_parts, leaf)
 *                                                                 [sibling]
 *   get_task                 get_task(conn, task_id)                [sibling;
 *                            conn is per-case infrastructure, NOT projected]
 *   _json_or                 _json_or(value, default=None)          [sibling]
 *   _json_or_null            _json_or_null(obj)                     [sibling]
 *   _current_run_id          _current_run_id(conn, task_id)         [sibling]
 *   _latest_event            _latest_event(conn, task_id, kind, run_id=None)
 *                                                                 [sibling]
 *   _json_dict               _json_dict(value)                      [sibling]
 *   _row_get                 _row_get(row, col, default=None)       [sibling]
 *   _process_fingerprint     _process_fingerprint(pid)              [sibling]
 *   _pid_recycled            _pid_recycled(pid, started_at)         [sibling]
 *   process_birth            process_birth(proc)                    [sibling;
 *                            proc projects to the marker "proc"]
 *   _explicit_board_slug     _explicit_board_slug(board)
 *   _explicit_board_intent_pinned  _explicit_board_intent_pinned()
 *   get_current_board        get_current_board()
 *   kanban_home              kanban_home() -> path string
 *   board_dir                board_dir(slug) -> path string
 *   path.expanduser          Path(pin).expanduser() -> string [monkeypatched]
 *   conn.execute             conn.execute(sql, params).fetchone() [the script
 *                            carries the fetchone() row: dict for key access]
 *   task.from_row            Task.from_row(row) -> task record dict
 *   time.time                time.time()
 *   get_process_start_time   gateway.status.get_process_start_time(pid)
 *   current_instantiation_epoch  gateway.drain_control.current_instantiation_epoch()
 *   _start_times_agree       gateway.status._start_times_agree(current, started_at)
 *   _pid_alive               _kb._pid_alive(pid)
 *   proc.create_time         proc.create_time() -> birth
 *   proc._proc.create_time   proc._proc.create_time(monotonic=True) -> birth
 *   profile_from_session_key_namespace  profile_from_session_key_namespace(ns)
 *
 * Representation contract (shared with tests/oracle_session_kanban_flow3.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception site. The FULL builtin hierarchy plus the
 *   quarry customs ride the sk3_exc_parents table, so except (TypeError,
 *   ValueError) and except Exception behave exactly as live.
 * - os.environ is a STATE dict (in.state.environ); reads are direct (no seam).
 *   psutil platform flags are STATE (in.state.psutil.{MACOS,LINUX}); reads are
 *   direct. No function here mutates state, so out.state echoes in.state.
 * - Doubles: conn rows are fixture dicts (key access); the fixture domain
 *   carries every key a flow reads (absent key -> KeyError / AttributeError as
 *   live). task records materialize as SimpleNamespace; an absent attribute
 *   read raises "'types.SimpleNamespace' object has no attribute '<name>'".
 *   proc is always a SimpleNamespace double built from the args.proc spec; its
 *   _proc presence (absent / present-null / present-double) is read from the
 *   spec. Paths are POSIX (no drive letters/UNC); Path.expanduser is
 *   monkeypatched live; joinpath / '/' fold to normalized POSIX concatenation.
 * - Tuples project to lists; sets to sorted lists; integral floats to ints.
 * - int() coercion follows Python: number -> trunc toward zero; str -> the
 *   int-strip whitespace set (narrower than str.strip), optional sign, digits
 *   with single underscores BETWEEN digits; failure -> ValueError "invalid
 *   literal for int() with base 10: <repr>". float() uses the SAME narrow
 *   strip set, decimal grammar (leading zeros allowed, '.5'/'5.' allowed,
 *   underscores between digits, optional exponent); failure -> ValueError
 *   "could not convert string to float: <repr>". str.strip uses the wider
 *   ASCII White_Space set (incl \x1c-\x1f).
 * - _json_or models json.loads with a STRICT Python-grammar pre-validator
 *   (libjson json_parse is lenient): rejects leading zeros, bare '-'/'1.'/'.5',
 *   '+1', NaN/Infinity, trailing commas, invalid escapes, raw control chars,
 *   trailing garbage; depth cap 64. Fixture domain: valid JSON or clearly
 *   invalid ASCII; parsed string values are printable ASCII (no control /
 *   non-ASCII code points). Non-str truthy values -> TypeError -> default.
 * - _json_or_null models json.dumps(obj, ensure_ascii=False) with Python's
 *   default separators (', ', ': ') and escape set. Payload fixture domain:
 *   int/str/bool/null/list/dict (NO floats — Python float repr is out of scope).
 * - Getter scripts (conn.execute rows / task.from_row / get_task) live in
 *   {null, non-empty dict}. kanban_home / board_dir / path.expanduser scripts
 *   are non-empty path strings.
 * - `is None` / `is not None` identity gates are JSON_NULL-only; truthiness
 *   gates use sk3_truthy.
 * - Source literals carry their exact bytes; fixture DATA is ASCII.
 *
 * Ownership: sk3_call ADOPTS args into the trace event; its return is a fresh
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

/* ── universal seam context ── */
typedef struct sk3_ctx {
    json_t *state;      /* owned; read-only here, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sk3_ctx_t;

/* ── basic helpers ── */
static bool sk3_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *sk3_raise_kind(const json_t *v) {
    return sk3_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool sk3_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *sk3_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *sk3_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *sk3_type_name(const json_t *v) {
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

static char *sk3_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python str() for scalars (containers are outside the str()-projection
 * fixture domain). Integral numbers fold to ints. */
static char *sk3_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sk3_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return sk3_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return sk3_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return sk3_strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool sk3_json_eq(const json_t *a, const json_t *b) {
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
            if (!sk3_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !sk3_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

static bool sk3_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

/* ── exception hierarchy (FULL builtin tree + the quarry customs) ──
 * Multiple inheritance is a parent LIST. An unknown scripted kind defaults to
 * Exception — exactly the oracle make_exc fallback type(kind, (Exception,)).
 * BaseException-only kinds never reach Exception. */
typedef struct sk3_exc_parent {
    const char *kind;
    const char *parents[4];   /* NULL-terminated */
} sk3_exc_parent_t;

static const sk3_exc_parent_t SK3_EXC_PARENTS[] = {
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
    {"PersistenceError", {"Exception", NULL}},
    {"AuthorityRefusal", {"PersistenceError", "ValueError", NULL}},
    {"sqlite3.Error", {"Exception", NULL}},
};
#define SK3_EXC_N (sizeof(SK3_EXC_PARENTS) / sizeof(SK3_EXC_PARENTS[0]))

static const sk3_exc_parent_t *sk3_exc_entry(const char *kind) {
    for (size_t i = 0; i < SK3_EXC_N; i++)
        if (!strcmp(SK3_EXC_PARENTS[i].kind, kind)) return &SK3_EXC_PARENTS[i];
    return NULL;
}

/* isinstance(kind, ancestor) over the parent tree; unknown kinds are
 * Exception subclasses (the make_exc fallback). */
static bool sk3_exc_isa(const char *kind, const char *ancestor) {
    if (!kind) return false;
    if (!strcmp(kind, ancestor)) return true;
    const sk3_exc_parent_t *e = sk3_exc_entry(kind);
    if (!e) return !strcmp(ancestor, "Exception") || !strcmp(ancestor, "BaseException");
    for (size_t i = 0; i < 4 && e->parents[i]; i++)
        if (sk3_exc_isa(e->parents[i], ancestor)) return true;
    return false;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool sk3_is_base_exc(const char *kind) {
    return kind && !sk3_exc_isa(kind, "Exception");
}

/* ── the one universal seam (fail-closed) ── */
static json_t *sk3_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return sk3_err_spec(type, msg);
}

static json_t *sk3_call(sk3_ctx_t *ctx, const char *name, json_t *args) {
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
        ret = sk3_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sk3_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sk3_make_exc(raise);
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
static json_t *sk3_args0(void) { return json_object(); }
static void sk3_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void sk3_anum(json_t *a, const char *k, double d) {
    json_set(a, k, json_number(d));
}
static void sk3_astr(json_t *a, const char *k, const char *s) {
    if (s) json_set_hdr(a, k, s); else json_set(a, k, json_null());
}

/* ── Python semantics helpers (ASCII fixture domain) ── */
/* str.strip White_Space (ASCII subset incl the \x1c-\x1f file separators). */
static bool sk3_is_strip_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F) || c == 0x20;
}
/* int()/float() strip a NARROWER set: White_Space minus \x1c-\x1f. */
static bool sk3_is_int_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0D) || c == 0x20;
}

/* str.strip -> owned string node, or an AttributeError envelope on non-str. */
static json_t *sk3_strip(const json_t *v) {
    if (!v || v->type == JSON_NULL)
        return sk3_err_spec("AttributeError", "'NoneType' object has no attribute 'strip'");
    if (v->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", sk3_type_name(v));
        return sk3_err_spec("AttributeError", msg);
    }
    const char *s = v->str_val ? v->str_val : "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sk3_is_strip_ws((unsigned char)s[a])) a++;
    while (b > a && sk3_is_strip_ws((unsigned char)s[b - 1])) b--;
    char *out = malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* ASCII lower() -> owned string node (str input). */
static json_t *sk3_lower(const json_t *v) {
    const char *s = sk3_str(v);
    if (!s) return json_string("");
    char *o = sk3_strdup(s);
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    json_t *r = json_string(o);
    free(o);
    return r;
}

/* Python repr() for an ASCII string. */
static char *sk3_repr_ascii(const char *s) {
    if (!s) s = "";
    int has_sq = strchr(s, '\'') != NULL;
    int has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *o = malloc(cap);
    if (!o) return sk3_strdup("''");
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

/* Python repr() for a scalar (str/int/bool/null) — {slug!r} positions. */
static char *sk3_repr_scalar(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sk3_strdup("None");
    if (v->type == JSON_STRING) return sk3_repr_ascii(v->str_val ? v->str_val : "");
    return sk3_pystr(v);   /* int / bool fold to their str() which equals repr */
}

/* int() coercion. Returns 0 on success (*out set), -1 on failure (*err owned
 * envelope). number -> trunc toward zero; bool -> 0/1; str -> Python int
 * grammar; null/other -> TypeError. */
static int sk3_py_int(const json_t *v, double *out, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = sk3_err_spec("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return -1;
    }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (v->type == JSON_NUMBER) { *out = trunc(v->num_val); return 0; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && sk3_is_int_ws((unsigned char)s[a])) a++;
        while (b > a && sk3_is_int_ws((unsigned char)s[b - 1])) b--;
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
        char *rp = sk3_repr_ascii(s);
        size_t ml = strlen(rp) + 64;
        char *msg = malloc(ml);
        if (msg) snprintf(msg, ml, "invalid literal for int() with base 10: %s", rp);
        *err = sk3_err_spec("ValueError", msg ? msg : "");
        free(msg); free(rp);
        return -1;
    }
    char msg[160];
    snprintf(msg, sizeof msg,
        "int() argument must be a string, a bytes-like object or a real number, not '%s'",
        sk3_type_name(v));
    *err = sk3_err_spec("TypeError", msg);
    return -1;
}

/* scan a digit run allowing single underscores BETWEEN digits; returns the
 * digit count, advances *i, sets *bad on a misplaced underscore. */
static size_t sk3_scan_digits(const char *s, size_t *i, size_t b, int *bad) {
    size_t n = 0; int prev = 0;
    while (*i < b) {
        char c = s[*i];
        if (c >= '0' && c <= '9') { n++; prev = 1; (*i)++; }
        else if (c == '_') {
            if (!prev || *i + 1 >= b || !(s[*i + 1] >= '0' && s[*i + 1] <= '9')) { *bad = 1; break; }
            prev = 0; (*i)++;
        } else break;
    }
    return n;
}

/* float() coercion. Returns 0 on success, -1 on failure (*err owned). */
static int sk3_py_float(const json_t *v, double *out, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = sk3_err_spec("TypeError",
            "float() argument must be a string or a real number, not 'NoneType'");
        return -1;
    }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 0; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && sk3_is_int_ws((unsigned char)s[a])) a++;
        while (b > a && sk3_is_int_ws((unsigned char)s[b - 1])) b--;
        size_t i = a;
        if (i < b && (s[i] == '+' || s[i] == '-')) i++;
        int bad = 0;
        size_t nd_int = sk3_scan_digits(s, &i, b, &bad);
        size_t nd_frac = 0;
        if (!bad && i < b && s[i] == '.') { i++; nd_frac = sk3_scan_digits(s, &i, b, &bad); }
        if (bad || (nd_int == 0 && nd_frac == 0)) goto fail;
        if (!bad && i < b && (s[i] == 'e' || s[i] == 'E')) {
            i++;
            if (i < b && (s[i] == '+' || s[i] == '-')) i++;
            size_t nd_exp = sk3_scan_digits(s, &i, b, &bad);
            if (bad || nd_exp == 0) goto fail;
        }
        if (bad || i != b) goto fail;
        {
            /* strip underscores then strtod (grammar already validated) */
            char *clean = malloc(b - a + 1);
            if (!clean) goto fail;
            size_t ci = 0;
            for (size_t k = a; k < b; k++) if (s[k] != '_') clean[ci++] = s[k];
            clean[ci] = '\0';
            char *end = NULL;
            double d = strtod(clean, &end);
            int ok = (end && *end == '\0' && ci > 0);
            free(clean);
            if (!ok) goto fail;
            *out = d;
            return 0;
        }
    fail:;
        char *rp = sk3_repr_ascii(s);
        size_t ml = strlen(rp) + 64;
        char *msg = malloc(ml);
        if (msg) snprintf(msg, ml, "could not convert string to float: %s", rp);
        *err = sk3_err_spec("ValueError", msg ? msg : "");
        free(msg); free(rp);
        return -1;
    }
    char msg[160];
    snprintf(msg, sizeof msg,
        "float() argument must be a string or a real number, not '%s'",
        sk3_type_name(v));
    *err = sk3_err_spec("TypeError", msg);
    return -1;
}

/* os.environ.get(key) — a direct STATE read (no seam). */
static const json_t *sk3_env_get(sk3_ctx_t *ctx, const char *key) {
    const json_t *env = json_obj_get(ctx->state, "environ");
    if (!env || env->type != JSON_OBJECT) return NULL;
    return json_obj_get(env, key);
}

/* psutil.<FLAG> — a direct STATE read (no seam), default False. */
static bool sk3_psutil_flag(sk3_ctx_t *ctx, const char *flag) {
    const json_t *p = json_obj_get(ctx->state, "psutil");
    if (!p || p->type != JSON_OBJECT) return false;
    return json_get_bool(p, flag, false);
}

/* AttributeError "'<type>' object has no attribute '<attr>'". */
static json_t *sk3_attr_err(const char *type, const char *attr) {
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", type, attr);
    return sk3_err_spec("AttributeError", msg);
}

/* KeyError str() is the repr of the key. */
static json_t *sk3_key_err(const char *key) {
    char *rp = sk3_repr_ascii(key);
    json_t *e = sk3_err_spec("KeyError", rp);
    free(rp);
    return e;
}

/* missing-required-positional-argument TypeError (Python message format). */
static json_t *sk3_missing_args(const json_t *args, const char *fname,
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
    return sk3_err_spec("TypeError", msg);
}

/* split(s, ":") -> owned array of string nodes. */
static json_t *sk3_split_colon(const char *s) {
    json_t *arr = json_array();
    if (!s) s = "";
    const char *start = s;
    for (const char *p = s; ; p++) {
        if (*p == ':' || *p == '\0') {
            size_t len = (size_t)(p - start);
            char *seg = malloc(len + 1);
            if (seg) { memcpy(seg, start, len); seg[len] = '\0'; }
            json_append(arr, json_string(seg ? seg : ""));
            free(seg);
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    return arr;
}

/* normalized POSIX path join: drop ""/"." segments, rebuild with single '/'.
 * Models Path(root).joinpath(*parts) / Path(root)/leaf on the absolute-POSIX
 * fixture domain (no '..' resolution — PurePath keeps '..'; out of domain). */
static json_t *sk3_path_join(const char *root, const json_t *parts) {
    /* collect segments */
    const char *segs[64]; size_t ns = 0;
    char *work = sk3_strdup(root ? root : "");
    /* root segments */
    {
        char *save = NULL;
        for (char *tok = strtok_r(work, "/", &save); tok && ns < 64; tok = strtok_r(NULL, "/", &save))
            if (strcmp(tok, ".") != 0) segs[ns++] = tok;
    }
    /* part segments (each part may itself contain '/') */
    char *partwork[64]; size_t npw = 0;
    if (parts && parts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(parts) && ns < 64; i++) {
            const char *ps = sk3_str(json_get(parts, i));
            if (!ps) continue;
            char *pw = sk3_strdup(ps);
            partwork[npw++] = pw;
            char *save = NULL;
            for (char *tok = strtok_r(pw, "/", &save); tok && ns < 64; tok = strtok_r(NULL, "/", &save))
                if (strcmp(tok, ".") != 0) segs[ns++] = tok;
        }
    }
    size_t cap = 2;
    for (size_t i = 0; i < ns; i++) cap += strlen(segs[i]) + 1;
    char *out = malloc(cap);
    size_t oi = 0;
    out[oi++] = '/';
    for (size_t i = 0; i < ns; i++) {
        if (i) out[oi++] = '/';
        size_t l = strlen(segs[i]);
        memcpy(out + oi, segs[i], l); oi += l;
    }
    out[oi] = '\0';
    json_t *r = json_string(out);
    free(out);
    free(work);
    for (size_t i = 0; i < npw; i++) free(partwork[i]);
    return r;
}

/* board slug regex ^[a-z0-9][a-z0-9\-_]{0,63}$ (post strip+lower). */
static bool sk3_slug_valid(const char *s) {
    if (!s) return false;
    size_t n = strlen(s);
    if (n < 1 || n > 64) return false;
    unsigned char c0 = (unsigned char)s[0];
    if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= '0' && c0 <= '9'))) return false;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return false;
    }
    return true;
}

/* ── Python json.dumps(obj, ensure_ascii=False), default separators ── */
typedef struct { char *b; size_t len, cap; } sk3_sb_t;
static void sk3_sb_init(sk3_sb_t *s) { s->cap = 64; s->len = 0; s->b = malloc(s->cap); if (s->b) s->b[0] = '\0'; }
static void sk3_sb_put(sk3_sb_t *s, const char *t, size_t n) {
    if (!s->b) return;
    if (s->len + n + 1 > s->cap) {
        while (s->len + n + 1 > s->cap) s->cap *= 2;
        char *nb = realloc(s->b, s->cap);
        if (!nb) { free(s->b); s->b = NULL; return; }
        s->b = nb;
    }
    memcpy(s->b + s->len, t, n); s->len += n; s->b[s->len] = '\0';
}
static void sk3_sb_puts(sk3_sb_t *s, const char *t) { sk3_sb_put(s, t, strlen(t)); }
static void sk3_sb_str(sk3_sb_t *s, const char *v) {
    sk3_sb_puts(s, "\"");
    for (const char *p = v ? v : ""; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') sk3_sb_puts(s, "\\\"");
        else if (c == '\\') sk3_sb_puts(s, "\\\\");
        else if (c == '\n') sk3_sb_puts(s, "\\n");
        else if (c == '\r') sk3_sb_puts(s, "\\r");
        else if (c == '\t') sk3_sb_puts(s, "\\t");
        else if (c == 0x08) sk3_sb_puts(s, "\\b");
        else if (c == 0x0c) sk3_sb_puts(s, "\\f");
        else if (c < 0x20) {
            char e[8]; snprintf(e, sizeof e, "\\u%04x", c); sk3_sb_puts(s, e);
        } else { char ch[2] = { (char)c, '\0' }; sk3_sb_put(s, ch, 1); }
    }
    sk3_sb_puts(s, "\"");
}
static void sk3_dumps_rec(sk3_sb_t *s, const json_t *v) {
    if (!v || v->type == JSON_NULL) { sk3_sb_puts(s, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: sk3_sb_puts(s, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            char buf[64]; double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            sk3_sb_puts(s, buf); return;
        }
        case JSON_STRING: sk3_sb_str(s, v->str_val); return;
        case JSON_ARRAY:
            sk3_sb_puts(s, "[");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sk3_sb_puts(s, ", ");
                sk3_dumps_rec(s, v->c.items[i]);
            }
            sk3_sb_puts(s, "]"); return;
        case JSON_OBJECT:
            sk3_sb_puts(s, "{");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sk3_sb_puts(s, ", ");
                sk3_sb_str(s, v->c.keys[i]);
                sk3_sb_puts(s, ": ");
                sk3_dumps_rec(s, v->c.items[i]);
            }
            sk3_sb_puts(s, "}"); return;
        default: sk3_sb_puts(s, "null"); return;
    }
}
/* json.dumps(obj, ensure_ascii=False) -> owned string node. */
static json_t *sk3_py_dumps(const json_t *v) {
    sk3_sb_t s; sk3_sb_init(&s);
    sk3_dumps_rec(&s, v);
    json_t *r = json_string(s.b ? s.b : "");
    free(s.b);
    return r;
}

/* ── strict Python json.loads pre-validator (ASCII domain) ── */
typedef struct { const char *s; size_t n, i; int depth; } sk3_jv_t;
static void sk3_jv_ws(sk3_jv_t *p) {
    while (p->i < p->n) {
        char c = p->s[p->i];
        if (c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D) p->i++;
        else break;
    }
}
static int sk3_jv_hex4(sk3_jv_t *p) {
    if (p->i + 4 > p->n) return 0;
    unsigned cp = 0;
    for (int k = 0; k < 4; k++) {
        char c = p->s[p->i + k];
        unsigned d;
        if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A' + 10);
        else return 0;
        cp = cp * 16 + d;
    }
    p->i += 4;
    /* domain: printable ASCII only (no control / non-ASCII code points) */
    return cp >= 0x20 && cp <= 0x7e;
}
static int sk3_jv_string(sk3_jv_t *p) {
    if (p->i >= p->n || p->s[p->i] != '"') return 0;
    p->i++;
    while (p->i < p->n) {
        unsigned char c = (unsigned char)p->s[p->i];
        if (c == '"') { p->i++; return 1; }
        if (c == '\\') {
            p->i++;
            if (p->i >= p->n) return 0;
            char e = p->s[p->i];
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' ||
                e == 'n' || e == 'r' || e == 't') { p->i++; }
            else if (e == 'u') { p->i++; if (!sk3_jv_hex4(p)) return 0; }
            else return 0;
        } else if (c < 0x20) {
            return 0;   /* raw control character */
        } else {
            p->i++;
        }
    }
    return 0;   /* unterminated */
}
static int sk3_jv_digits(sk3_jv_t *p) {
    size_t start = p->i;
    while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    return p->i > start;
}
static int sk3_jv_number(sk3_jv_t *p) {
    if (p->i < p->n && p->s[p->i] == '-') p->i++;
    if (p->i >= p->n) return 0;
    if (p->s[p->i] == '0') {
        p->i++;
        if (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') return 0; /* leading zero */
    } else if (p->s[p->i] >= '1' && p->s[p->i] <= '9') {
        if (!sk3_jv_digits(p)) return 0;
    } else return 0;
    if (p->i < p->n && p->s[p->i] == '.') { p->i++; if (!sk3_jv_digits(p)) return 0; }
    if (p->i < p->n && (p->s[p->i] == 'e' || p->s[p->i] == 'E')) {
        p->i++;
        if (p->i < p->n && (p->s[p->i] == '+' || p->s[p->i] == '-')) p->i++;
        if (!sk3_jv_digits(p)) return 0;
    }
    return 1;
}
static int sk3_jv_value(sk3_jv_t *p) {
    sk3_jv_ws(p);
    if (p->i >= p->n) return 0;
    char c = p->s[p->i];
    if (c == '{') {
        if (++p->depth > 64) return 0;
        p->i++; sk3_jv_ws(p);
        if (p->i < p->n && p->s[p->i] == '}') { p->i++; p->depth--; return 1; }
        for (;;) {
            if (!sk3_jv_string(p)) return 0;
            sk3_jv_ws(p);
            if (p->i >= p->n || p->s[p->i] != ':') return 0;
            p->i++;
            if (!sk3_jv_value(p)) return 0;
            sk3_jv_ws(p);
            if (p->i < p->n && p->s[p->i] == ',') { p->i++; sk3_jv_ws(p); continue; }
            if (p->i < p->n && p->s[p->i] == '}') { p->i++; p->depth--; return 1; }
            return 0;
        }
    }
    if (c == '[') {
        if (++p->depth > 64) return 0;
        p->i++; sk3_jv_ws(p);
        if (p->i < p->n && p->s[p->i] == ']') { p->i++; p->depth--; return 1; }
        for (;;) {
            if (!sk3_jv_value(p)) return 0;
            sk3_jv_ws(p);
            if (p->i < p->n && p->s[p->i] == ',') { p->i++; sk3_jv_ws(p); continue; }
            if (p->i < p->n && p->s[p->i] == ']') { p->i++; p->depth--; return 1; }
            return 0;
        }
    }
    if (c == '"') return sk3_jv_string(p);
    if (c == 't') { if (p->n - p->i >= 4 && !strncmp(p->s + p->i, "true", 4)) { p->i += 4; return 1; } return 0; }
    if (c == 'f') { if (p->n - p->i >= 5 && !strncmp(p->s + p->i, "false", 5)) { p->i += 5; return 1; } return 0; }
    if (c == 'n') { if (p->n - p->i >= 4 && !strncmp(p->s + p->i, "null", 4)) { p->i += 4; return 1; } return 0; }
    if (c == '-' || (c >= '0' && c <= '9')) return sk3_jv_number(p);
    return 0;   /* rejects NaN / Infinity / +1 / .5 / single quotes */
}
/* json.loads(s) -> owned node, or NULL on any decode failure. A JSON_NULL
 * node (non-NULL pointer) is the valid Python None result. */
static json_t *sk3_json_loads(const char *s) {
    if (!s) return NULL;
    sk3_jv_t p = { s, strlen(s), 0, 0 };
    sk3_jv_ws(&p);
    if (!sk3_jv_value(&p)) return NULL;
    sk3_jv_ws(&p);
    if (p.i != p.n) return NULL;   /* trailing garbage */
    return json_parse(s, NULL);
}

/* ── module constants ── */
#define SK3_DEFAULT_BOARD "default"
#define SK3_GRACE_SECONDS 30.0
#define SK3_UNVERIFIED "unverified"
typedef struct { const char *outcome; const char *status; } sk3_outcome_t;
static const sk3_outcome_t SK3_RUN_OUTCOME_TERMINAL[] = {
    {"completed", "done"},
    {"review_requested", "review"},
    {"changes_requested", "changes_requested"},
    {"blocked", "blocked"},
    {"dependency_wait", "blocked"},
};
#define SK3_OUTCOME_N (sizeof(SK3_RUN_OUTCOME_TERMINAL) / sizeof(SK3_RUN_OUTCOME_TERMINAL[0]))
static const char *sk3_terminal_status(const char *outcome) {
    if (!outcome) return NULL;
    for (size_t i = 0; i < SK3_OUTCOME_N; i++)
        if (!strcmp(SK3_RUN_OUTCOME_TERMINAL[i].outcome, outcome))
            return SK3_RUN_OUTCOME_TERMINAL[i].status;
    return NULL;
}

/* ════════════════ hermes_cli/kanban_db.py ════════════════ */

/* PoP: sk3_normalize_board_slug @ hermes_cli/kanban_db.py:_normalize_board_slug */
static json_t *sk3_normalize_board_slug(sk3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    static const char *req[] = { "slug" };
    json_t *miss = sk3_missing_args(args, "_normalize_board_slug", req, 1);
    if (miss) return miss;
    const json_t *slug = json_obj_get(args, "slug");
    /* s = str(slug).strip().lower() if slug is not None else "" */
    char *s;
    if (sk3_is_null(slug)) {
        s = sk3_strdup("");
    } else {
        char *ps = sk3_pystr(slug);
        json_t *pn = json_string(ps); free(ps);
        json_t *st = sk3_strip(pn); json_free(pn);
        if (sk3_is_raise(st)) return st;
        json_t *lo = sk3_lower(st); json_free(st);
        s = sk3_strdup(sk3_str(lo)); json_free(lo);
    }
    if (!s[0]) { free(s); return json_null(); }
    if (!sk3_slug_valid(s)) {
        char *rp = sk3_repr_scalar(slug);
        const char *tail = ": must be 1-64 chars, lowercase "
                           "alphanumerics / hyphens / underscores, not starting with '-' or '_'";
        size_t ml = strlen(rp) + strlen(tail) + 32;
        char *msg = malloc(ml);
        if (msg) snprintf(msg, ml, "invalid board slug %s%s", rp, tail);
        free(rp); free(s);
        json_t *e = sk3_err_spec("ValueError", msg ? msg : "");
        free(msg);
        return e;
    }
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP: sk3_require_slug @ hermes_cli/kanban_db.py:_require_slug */
static json_t *sk3_require_slug(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "slug" };
    json_t *miss = sk3_missing_args(args, "_require_slug", req, 1);
    if (miss) return miss;
    const json_t *slug = json_obj_get(args, "slug");
    json_t *a = sk3_args0(); sk3_aset(a, "slug", slug);
    json_t *normed = sk3_call(ctx, "_normalize_board_slug", a);
    if (sk3_is_raise(normed)) return normed;
    if (!sk3_truthy(normed)) {
        json_free(normed);
        return sk3_err_spec("ValueError", "board slug is required");
    }
    return normed;
}

/* PoP: sk3_board_path @ hermes_cli/kanban_db.py:_board_path */
static json_t *sk3_board_path(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "env_var", "board", "default_parts", "leaf" };
    json_t *miss = sk3_missing_args(args, "_board_path", req, 4);
    if (miss) return miss;
    const json_t *env_var = json_obj_get(args, "env_var");
    const json_t *board = json_obj_get(args, "board");
    const json_t *default_parts = json_obj_get(args, "default_parts");
    const json_t *leaf = json_obj_get(args, "leaf");

    /* pin = os.environ.get(env_var, "").strip() if env_var else "" */
    char *pin;
    if (!sk3_truthy(env_var)) {
        pin = sk3_strdup("");
    } else {
        const json_t *ev = sk3_env_get(ctx, sk3_str(env_var));
        json_t *evn = ev ? json_copy(ev) : json_string("");
        json_t *st = sk3_strip(evn); json_free(evn);
        if (sk3_is_raise(st)) return st;
        pin = sk3_strdup(sk3_str(st)); json_free(st);
    }

    /* slug = _explicit_board_slug(board) */
    json_t *a1 = sk3_args0(); sk3_aset(a1, "board", board);
    json_t *slug = sk3_call(ctx, "_explicit_board_slug", a1);
    if (sk3_is_raise(slug)) { free(pin); return slug; }

    /* if pin and (slug is None or _explicit_board_intent_pinned()):
     * return Path(pin).expanduser() */
    bool take_pin = false;
    if (pin[0]) {
        if (sk3_is_null(slug)) {
            take_pin = true;
        } else {
            json_t *pinned = sk3_call(ctx, "_explicit_board_intent_pinned", sk3_args0());
            if (sk3_is_raise(pinned)) { free(pin); json_free(slug); return pinned; }
            take_pin = sk3_truthy(pinned);
            json_free(pinned);
        }
    }
    if (take_pin) {
        json_free(slug);
        json_t *a2 = sk3_args0(); sk3_astr(a2, "path", pin);
        free(pin);
        json_t *r = sk3_call(ctx, "path.expanduser", a2);
        return r;
    }
    free(pin);

    /* if slug is None: slug = get_current_board() */
    if (sk3_is_null(slug)) {
        json_free(slug);
        slug = sk3_call(ctx, "get_current_board", sk3_args0());
        if (sk3_is_raise(slug)) return slug;
    }

    /* if slug == DEFAULT_BOARD: return kanban_home().joinpath(*default_parts) */
    const char *slugs = sk3_str(slug);
    if (slugs && !strcmp(slugs, SK3_DEFAULT_BOARD)) {
        json_free(slug);
        json_t *home = sk3_call(ctx, "kanban_home", sk3_args0());
        if (sk3_is_raise(home)) return home;
        json_t *r = sk3_path_join(sk3_str(home), default_parts);
        json_free(home);
        return r;
    }

    /* return board_dir(slug) / leaf */
    json_t *a3 = sk3_args0(); sk3_aset(a3, "board", slug);
    json_free(slug);
    json_t *bd = sk3_call(ctx, "board_dir", a3);
    if (sk3_is_raise(bd)) return bd;
    json_t *parts = json_array();
    json_append(parts, leaf ? json_copy(leaf) : json_null());
    json_t *r = sk3_path_join(sk3_str(bd), parts);
    json_free(bd); json_free(parts);
    return r;
}

/* PoP: sk3_kanban_db_path @ hermes_cli/kanban_db.py:kanban_db_path */
static json_t *sk3_kanban_db_path(sk3_ctx_t *ctx, const json_t *args) {
    const json_t *board = json_obj_get(args, "board");
    json_t *a = sk3_args0();
    sk3_astr(a, "env_var", "HERMES_KANBAN_DB");
    sk3_aset(a, "board", board);
    json_t *parts = json_array();
    json_append(parts, json_string("kanban.db"));
    json_set(a, "default_parts", parts);
    sk3_astr(a, "leaf", "kanban.db");
    return sk3_call(ctx, "_board_path", a);
}

/* PoP: sk3_workspaces_root @ hermes_cli/kanban_db.py:workspaces_root */
static json_t *sk3_workspaces_root(sk3_ctx_t *ctx, const json_t *args) {
    const json_t *board = json_obj_get(args, "board");
    json_t *a = sk3_args0();
    sk3_astr(a, "env_var", "HERMES_KANBAN_WORKSPACES_ROOT");
    sk3_aset(a, "board", board);
    json_t *parts = json_array();
    json_append(parts, json_string("kanban"));
    json_append(parts, json_string("workspaces"));
    json_set(a, "default_parts", parts);
    sk3_astr(a, "leaf", "workspaces");
    return sk3_call(ctx, "_board_path", a);
}

/* PoP: sk3_get_task @ hermes_cli/kanban_db.py:get_task */
static json_t *sk3_get_task(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "task_id" };
    json_t *miss = sk3_missing_args(args, "get_task", req, 1);
    if (miss) return miss;
    const json_t *task_id = json_obj_get(args, "task_id");
    json_t *a = sk3_args0();
    sk3_astr(a, "sql", "SELECT * FROM tasks WHERE id = ?");
    json_t *params = json_array();
    json_append(params, task_id ? json_copy(task_id) : json_null());
    json_set(a, "params", params);
    json_t *row = sk3_call(ctx, "conn.execute", a);
    if (sk3_is_raise(row)) return row;
    if (!sk3_truthy(row)) { json_free(row); return json_null(); }
    json_t *a2 = sk3_args0(); sk3_aset(a2, "row", row);
    json_free(row);
    return sk3_call(ctx, "task.from_row", a2);
}

/* PoP: sk3_json_or @ hermes_cli/kanban_db.py:_json_or */
static json_t *sk3_json_or(sk3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    static const char *req[] = { "value" };
    json_t *miss = sk3_missing_args(args, "_json_or", req, 1);
    if (miss) return miss;
    const json_t *value = json_obj_get(args, "value");
    const json_t *dflt = json_obj_get(args, "default");
    json_t *def = dflt ? json_copy(dflt) : json_null();
    /* if not value: return default */
    if (!sk3_truthy(value)) return def;
    /* try: return json.loads(value)  except Exception: return default */
    if (value->type != JSON_STRING) {
        /* json.loads(non-str) raises TypeError -> caught -> default */
        return def;
    }
    json_t *parsed = sk3_json_loads(value->str_val ? value->str_val : "");
    if (!parsed) return def;   /* decode failure -> default */
    json_free(def);
    return parsed;
}

/* PoP: sk3_json_or_null @ hermes_cli/kanban_db.py:_json_or_null */
static json_t *sk3_json_or_null(sk3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    static const char *req[] = { "obj" };
    json_t *miss = sk3_missing_args(args, "_json_or_null", req, 1);
    if (miss) return miss;
    const json_t *obj = json_obj_get(args, "obj");
    if (!sk3_truthy(obj)) return json_null();
    return sk3_py_dumps(obj);
}

/* PoP: sk3_append_event @ hermes_cli/kanban_db.py:_append_event */
static json_t *sk3_append_event(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "task_id", "kind" };
    json_t *miss = sk3_missing_args(args, "_append_event", req, 2);
    if (miss) return miss;
    const json_t *task_id = json_obj_get(args, "task_id");
    const json_t *kind = json_obj_get(args, "kind");
    const json_t *payload = json_obj_get(args, "payload");
    const json_t *run_id = json_obj_get(args, "run_id");

    /* params tuple evaluates left-to-right: task_id, run_id, kind,
     * _json_or_null(payload), int(time.time()) */
    json_t *a1 = sk3_args0(); sk3_aset(a1, "obj", payload);
    json_t *pj = sk3_call(ctx, "_json_or_null", a1);
    if (sk3_is_raise(pj)) return pj;

    json_t *tt = sk3_call(ctx, "time.time", sk3_args0());
    if (sk3_is_raise(tt)) { json_free(pj); return tt; }
    double epoch; json_t *ierr = NULL;
    if (sk3_py_int(tt, &epoch, &ierr) != 0) {
        json_free(tt); json_free(pj);
        return ierr;   /* int() failure propagates */
    }
    json_free(tt);

    json_t *a2 = sk3_args0();
    sk3_astr(a2, "sql",
        "INSERT INTO task_events (task_id, run_id, kind, payload, created_at) "
        "VALUES (?, ?, ?, ?, ?)");
    json_t *params = json_array();
    json_append(params, task_id ? json_copy(task_id) : json_null());
    json_append(params, run_id ? json_copy(run_id) : json_null());
    json_append(params, kind ? json_copy(kind) : json_null());
    json_append(params, pj);   /* adopts pj */
    json_append(params, json_number(epoch));
    json_set(a2, "params", params);
    json_t *r = sk3_call(ctx, "conn.execute", a2);
    if (sk3_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: sk3_current_run_id @ hermes_cli/kanban_db.py:_current_run_id */
static json_t *sk3_current_run_id(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "task_id" };
    json_t *miss = sk3_missing_args(args, "_current_run_id", req, 1);
    if (miss) return miss;
    const json_t *task_id = json_obj_get(args, "task_id");
    json_t *a = sk3_args0();
    sk3_astr(a, "sql", "SELECT current_run_id FROM tasks WHERE id = ?");
    json_t *params = json_array();
    json_append(params, task_id ? json_copy(task_id) : json_null());
    json_set(a, "params", params);
    json_t *row = sk3_call(ctx, "conn.execute", a);
    if (sk3_is_raise(row)) return row;
    /* return int(row["current_run_id"]) if row and row["current_run_id"] else None */
    if (!sk3_truthy(row)) { json_free(row); return json_null(); }
    const json_t *crid = json_obj_get(row, "current_run_id");
    if (!crid) { json_free(row); return sk3_key_err("current_run_id"); }
    if (!sk3_truthy(crid)) { json_free(row); return json_null(); }
    double v; json_t *ierr = NULL;
    int rc = sk3_py_int(crid, &v, &ierr);
    json_free(row);
    if (rc != 0) return ierr;
    return json_number(v);
}

/* PoP: sk3_latest_event @ hermes_cli/kanban_db.py:_latest_event */
static json_t *sk3_latest_event(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "task_id", "kind" };
    json_t *miss = sk3_missing_args(args, "_latest_event", req, 2);
    if (miss) return miss;
    const json_t *task_id = json_obj_get(args, "task_id");
    const json_t *kind = json_obj_get(args, "kind");
    const json_t *run_id = json_obj_get(args, "run_id");

    /* sql = "SELECT payload FROM task_events WHERE task_id = ? AND kind = ?"
     * params = (task_id, kind); if run_id is not None: sql += " AND run_id = ?";
     * params = (*params, int(run_id)) */
    char sql[256];
    snprintf(sql, sizeof sql, "SELECT payload FROM task_events WHERE task_id = ? AND kind = ?");
    json_t *params = json_array();
    json_append(params, task_id ? json_copy(task_id) : json_null());
    json_append(params, kind ? json_copy(kind) : json_null());
    if (!sk3_is_null(run_id)) {
        double v; json_t *ierr = NULL;
        if (sk3_py_int(run_id, &v, &ierr) != 0) { json_free(params); return ierr; }
        strcat(sql, " AND run_id = ?");
        json_append(params, json_number(v));
    }
    strcat(sql, " ORDER BY id DESC LIMIT 1");
    json_t *a = sk3_args0();
    sk3_astr(a, "sql", sql);
    json_set(a, "params", params);
    json_t *row = sk3_call(ctx, "conn.execute", a);
    if (sk3_is_raise(row)) return row;
    return row;   /* .fetchone() */
}

/* PoP: sk3_json_dict @ hermes_cli/kanban_db.py:_json_dict */
static json_t *sk3_json_dict(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "value" };
    json_t *miss = sk3_missing_args(args, "_json_dict", req, 1);
    if (miss) return miss;
    const json_t *value = json_obj_get(args, "value");
    json_t *a = sk3_args0();
    sk3_aset(a, "value", value);
    json_set(a, "default", json_object());   /* _json_or(value, {}) */
    json_t *parsed = sk3_call(ctx, "_json_or", a);
    if (sk3_is_raise(parsed)) return parsed;
    /* return parsed if isinstance(parsed, dict) else {} */
    if (parsed && parsed->type == JSON_OBJECT) return parsed;
    json_free(parsed);
    return json_object();
}

/* PoP: sk3_row_get @ hermes_cli/kanban_db.py:_row_get */
static json_t *sk3_row_get(sk3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    static const char *req[] = { "row", "col" };
    json_t *miss = sk3_missing_args(args, "_row_get", req, 2);
    if (miss) return miss;
    const json_t *row = json_obj_get(args, "row");
    const json_t *col = json_obj_get(args, "col");
    const json_t *dflt = json_obj_get(args, "default");
    /* if row is None or col not in row.keys(): return default */
    if (sk3_is_null(row)) return dflt ? json_copy(dflt) : json_null();
    if (row->type != JSON_OBJECT)
        return sk3_attr_err(sk3_type_name(row), "keys");
    const char *cols = sk3_str(col);
    const json_t *v = cols ? json_obj_get(row, cols) : NULL;
    if (!v) return dflt ? json_copy(dflt) : json_null();
    return json_copy(v);
}

/* PoP: sk3_retry_status_for_run @ hermes_cli/kanban_db.py:_retry_status_for_run */
static json_t *sk3_retry_status_for_run(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "task_id" };
    json_t *miss = sk3_missing_args(args, "_retry_status_for_run", req, 1);
    if (miss) return miss;
    const json_t *task_id = json_obj_get(args, "task_id");
    const json_t *run_id_in = json_obj_get(args, "run_id");

    /* if run_id is None: run_id = _current_run_id(conn, task_id) */
    json_t *run_id;
    if (sk3_is_null(run_id_in)) {
        json_t *a = sk3_args0(); sk3_aset(a, "task_id", task_id);
        run_id = sk3_call(ctx, "_current_run_id", a);
        if (sk3_is_raise(run_id)) return run_id;
    } else {
        run_id = json_copy(run_id_in);
    }
    /* if run_id is None: return "ready" */
    if (sk3_is_null(run_id)) { json_free(run_id); return json_string("ready"); }

    /* event = _latest_event(conn, task_id, "claimed", run_id) */
    json_t *a2 = sk3_args0();
    sk3_aset(a2, "task_id", task_id);
    sk3_astr(a2, "kind", "claimed");
    sk3_aset(a2, "run_id", run_id);
    json_free(run_id);
    json_t *event = sk3_call(ctx, "_latest_event", a2);
    if (sk3_is_raise(event)) return event;

    /* payload = _json_dict(_row_get(event, "payload")) */
    json_t *a3 = sk3_args0();
    sk3_aset(a3, "row", event);
    sk3_astr(a3, "col", "payload");
    json_free(event);
    json_t *raw = sk3_call(ctx, "_row_get", a3);
    if (sk3_is_raise(raw)) return raw;
    json_t *a4 = sk3_args0(); sk3_aset(a4, "value", raw);
    json_free(raw);
    json_t *payload = sk3_call(ctx, "_json_dict", a4);
    if (sk3_is_raise(payload)) return payload;

    /* return "review" if payload.get("source_status") == "review" else "ready" */
    if (sk3_is_null(payload)) { json_free(payload); return sk3_attr_err("NoneType", "get"); }
    if (payload->type != JSON_OBJECT) {
        json_t *e = sk3_attr_err(sk3_type_name(payload), "get");
        json_free(payload); return e;
    }
    const json_t *ss = json_obj_get(payload, "source_status");
    bool review = ss && ss->type == JSON_STRING && !strcmp(ss->str_val ? ss->str_val : "", "review");
    json_free(payload);
    return json_string(review ? "review" : "ready");
}

/* PoP: sk3_goal_run_status @ hermes_cli/kanban_db.py:goal_run_status */
static json_t *sk3_goal_run_status(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "task_id" };
    json_t *miss = sk3_missing_args(args, "goal_run_status", req, 1);
    if (miss) return miss;
    const json_t *task_id = json_obj_get(args, "task_id");
    const json_t *expected = json_obj_get(args, "expected_run_id");

    /* task = get_task(conn, task_id) */
    json_t *a = sk3_args0(); sk3_aset(a, "task_id", task_id);
    json_t *task = sk3_call(ctx, "get_task", a);
    if (sk3_is_raise(task)) return task;
    /* if task is None: return None */
    if (sk3_is_null(task)) { json_free(task); return json_null(); }

    if (!sk3_is_null(expected)) {
        /* int(expected_run_id) */
        double exp; json_t *ierr = NULL;
        if (sk3_py_int(expected, &exp, &ierr) != 0) { json_free(task); return ierr; }
        /* row = conn.execute("SELECT outcome FROM task_runs WHERE id = ? AND task_id = ?",
         *                    (int(expected_run_id), task_id)).fetchone() */
        json_t *a2 = sk3_args0();
        sk3_astr(a2, "sql", "SELECT outcome FROM task_runs WHERE id = ? AND task_id = ?");
        json_t *params = json_array();
        json_append(params, json_number(exp));
        json_append(params, task_id ? json_copy(task_id) : json_null());
        json_set(a2, "params", params);
        json_t *row = sk3_call(ctx, "conn.execute", a2);
        if (sk3_is_raise(row)) { json_free(task); return row; }
        /* outcome = str(row["outcome"]) if row and row["outcome"] is not None else None */
        char *outcome = NULL;
        if (sk3_truthy(row)) {
            const json_t *oc = json_obj_get(row, "outcome");
            if (!oc) { json_free(row); json_free(task); return sk3_key_err("outcome"); }
            if (!sk3_is_null(oc)) outcome = sk3_pystr(oc);
        }
        json_free(row);
        /* terminal_status = _RUN_OUTCOME_TERMINAL_STATUS.get(outcome) */
        const char *term = sk3_terminal_status(outcome);
        if (term) { free(outcome); json_free(task); return json_string(term); }
        /* if outcome is not None or task.current_run_id != int(expected_run_id):
         *     return "superseded"   (or short-circuits: crid read only when
         *     outcome is None) */
        if (outcome != NULL) {
            free(outcome); json_free(task);
            return json_string("superseded");
        }
        const json_t *crid = json_obj_get(task, "current_run_id");
        if (!crid) { json_free(task); return sk3_attr_err("types.SimpleNamespace", "current_run_id"); }
        json_t *expn = json_number(exp);
        bool neq = !sk3_json_eq(crid, expn);
        json_free(expn);
        if (neq) {
            json_free(task);
            return json_string("superseded");
        }
    }

    /* if task.status in {"ready","todo"}: */
    const json_t *status = json_obj_get(task, "status");
    if (!status) { json_free(task); return sk3_attr_err("types.SimpleNamespace", "status"); }
    const char *st = sk3_str(status);
    if (st && (!strcmp(st, "ready") || !strcmp(st, "todo"))) {
        json_t *a3 = sk3_args0();
        sk3_astr(a3, "sql", "SELECT kind FROM task_events WHERE task_id = ? ORDER BY id DESC LIMIT 1");
        json_t *params = json_array();
        json_append(params, task_id ? json_copy(task_id) : json_null());
        json_set(a3, "params", params);
        json_t *event = sk3_call(ctx, "conn.execute", a3);
        if (sk3_is_raise(event)) { json_free(task); return event; }
        /* if event and event["kind"] == "changes_requested": return "changes_requested" */
        if (sk3_truthy(event)) {
            const json_t *k = json_obj_get(event, "kind");
            if (!k) { json_free(event); json_free(task); return sk3_key_err("kind"); }
            const char *ks = sk3_str(k);
            if (ks && !strcmp(ks, "changes_requested")) {
                json_free(event); json_free(task);
                return json_string("changes_requested");
            }
        }
        json_free(event);
    }
    /* return task.status */
    json_t *r = json_copy(status);
    json_free(task);
    return r;
}

/* ════════════════ hermes_cli/kanban_db_dispatch.py ════════════════ */

/* PoP: sk3_worker_terminal_timeout_env @ hermes_cli/kanban_db_dispatch.py:_worker_terminal_timeout_env */
static json_t *sk3_worker_terminal_timeout_env(sk3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    static const char *req[] = { "max_runtime_seconds", "current_timeout" };
    json_t *miss = sk3_missing_args(args, "_worker_terminal_timeout_env", req, 2);
    if (miss) return miss;
    const json_t *mrs = json_obj_get(args, "max_runtime_seconds");
    const json_t *ct = json_obj_get(args, "current_timeout");

    /* if max_runtime_seconds is None: return None */
    if (sk3_is_null(mrs)) return json_null();
    /* try: runtime = int(max_runtime_seconds) except (TypeError, ValueError): return None */
    double runtime; json_t *ierr = NULL;
    if (sk3_py_int(mrs, &runtime, &ierr) != 0) {
        const char *k = sk3_raise_kind(ierr);
        if (sk3_exc_isa(k, "TypeError") || sk3_exc_isa(k, "ValueError")) {
            json_free(ierr); return json_null();
        }
        return ierr;   /* BaseException passthrough */
    }
    /* if runtime <= 0: return None */
    if (runtime <= 0) return json_null();
    /* desired = max(1, runtime - GRACE) */
    double desired = runtime - SK3_GRACE_SECONDS;
    if (desired < 1) desired = 1;
    /* try: existing = int(str(current_timeout).strip()) if current_timeout else 0
     * except (TypeError, ValueError): existing = 0 */
    double existing = 0;
    if (sk3_truthy(ct)) {
        char *ps = sk3_pystr(ct);
        json_t *pn = json_string(ps); free(ps);
        json_t *st = sk3_strip(pn); json_free(pn);
        if (sk3_is_raise(st)) return st;   /* str/strip won't raise on scalars */
        json_t *e2 = NULL; double ev;
        if (sk3_py_int(st, &ev, &e2) != 0) {
            const char *k2 = sk3_raise_kind(e2);
            if (sk3_exc_isa(k2, "TypeError") || sk3_exc_isa(k2, "ValueError")) {
                json_free(e2); existing = 0;
            } else { json_free(st); return e2; }
        } else {
            existing = ev;
        }
        json_free(st);
    }
    /* if existing >= desired: return None */
    if (existing >= desired) return json_null();
    /* return str(desired) */
    char buf[64];
    snprintf(buf, sizeof buf, "%lld", (long long)desired);
    return json_string(buf);
}

/* PoP: sk3_process_fingerprint @ hermes_cli/kanban_db_dispatch.py:_process_fingerprint */
static json_t *sk3_process_fingerprint(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "pid" };
    json_t *miss = sk3_missing_args(args, "_process_fingerprint", req, 1);
    if (miss) return miss;
    const json_t *pid = json_obj_get(args, "pid");
    /* start = get_process_start_time(int(pid)) */
    double p; json_t *ierr = NULL;
    if (sk3_py_int(pid, &p, &ierr) != 0) return ierr;
    json_t *a = sk3_args0(); sk3_anum(a, "pid", p);
    json_t *start = sk3_call(ctx, "get_process_start_time", a);
    if (sk3_is_raise(start)) return start;
    /* if start is None: return None */
    if (sk3_is_null(start)) { json_free(start); return json_null(); }
    /* return f"{current_instantiation_epoch()}|{start}" */
    json_t *epoch = sk3_call(ctx, "current_instantiation_epoch", sk3_args0());
    if (sk3_is_raise(epoch)) { json_free(start); return epoch; }
    char *es = sk3_pystr(epoch); json_free(epoch);
    char *ss = sk3_pystr(start); json_free(start);
    size_t ml = strlen(es) + strlen(ss) + 2;
    char *msg = malloc(ml);
    if (msg) snprintf(msg, ml, "%s|%s", es, ss);
    free(es); free(ss);
    json_t *r = json_string(msg ? msg : "");
    free(msg);
    return r;
}

/* PoP: sk3_pid_recycled @ hermes_cli/kanban_db_dispatch.py:_pid_recycled */
static json_t *sk3_pid_recycled(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "pid", "started_at" };
    json_t *miss = sk3_missing_args(args, "_pid_recycled", req, 2);
    if (miss) return miss;
    const json_t *pid = json_obj_get(args, "pid");
    const json_t *started_at = json_obj_get(args, "started_at");

    /* if started_at is None or not pid: return False */
    if (sk3_is_null(started_at) || !sk3_truthy(pid)) return json_bool(false);
    /* if started_at == UNVERIFIED_WORKER_FINGERPRINT: return True */
    const char *sa = sk3_str(started_at);
    if (sa && !strcmp(sa, SK3_UNVERIFIED)) return json_bool(true);
    /* if isinstance(started_at, str) and "|" in started_at:
     *     return _process_fingerprint(int(pid)) != started_at */
    if (sa && strchr(sa, '|')) {
        double p; json_t *ierr = NULL;
        if (sk3_py_int(pid, &p, &ierr) != 0) return ierr;
        json_t *a = sk3_args0(); sk3_anum(a, "pid", p);
        json_t *fp = sk3_call(ctx, "_process_fingerprint", a);
        if (sk3_is_raise(fp)) return fp;
        bool neq = !sk3_json_eq(fp, started_at);
        json_free(fp);
        return json_bool(neq);
    }
    /* current = get_process_start_time(int(pid)) */
    double p; json_t *ierr = NULL;
    if (sk3_py_int(pid, &p, &ierr) != 0) return ierr;
    json_t *a2 = sk3_args0(); sk3_anum(a2, "pid", p);
    json_t *current = sk3_call(ctx, "get_process_start_time", a2);
    if (sk3_is_raise(current)) return current;
    /* if current is None: return True */
    if (sk3_is_null(current)) { json_free(current); return json_bool(true); }
    /* try: return not _start_times_agree(current, started_at)
     * except (TypeError, ValueError): return True */
    json_t *a3 = sk3_args0();
    sk3_aset(a3, "current", current);
    sk3_aset(a3, "started_at", started_at);
    json_free(current);
    json_t *agree = sk3_call(ctx, "_start_times_agree", a3);
    if (sk3_is_raise(agree)) {
        const char *k = sk3_raise_kind(agree);
        if (sk3_exc_isa(k, "TypeError") || sk3_exc_isa(k, "ValueError")) {
            json_free(agree); return json_bool(true);
        }
        return agree;   /* BaseException passthrough */
    }
    bool not_agree = !sk3_truthy(agree);
    json_free(agree);
    return json_bool(not_agree);
}

/* PoP: sk3_worker_alive @ hermes_cli/kanban_db_dispatch.py:_worker_alive */
static json_t *sk3_worker_alive(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "pid", "started_at" };
    json_t *miss = sk3_missing_args(args, "_worker_alive", req, 2);
    if (miss) return miss;
    const json_t *pid = json_obj_get(args, "pid");
    const json_t *started_at = json_obj_get(args, "started_at");
    /* if not _kb._pid_alive(pid): return False */
    json_t *a = sk3_args0(); sk3_aset(a, "pid", pid);
    json_t *alive = sk3_call(ctx, "_pid_alive", a);
    if (sk3_is_raise(alive)) return alive;
    if (!sk3_truthy(alive)) { json_free(alive); return json_bool(false); }
    json_free(alive);
    /* if started_at == UNVERIFIED_WORKER_FINGERPRINT: return True */
    const char *sa = sk3_str(started_at);
    if (sa && !strcmp(sa, SK3_UNVERIFIED)) return json_bool(true);
    /* return not _pid_recycled(pid, started_at) */
    json_t *a2 = sk3_args0();
    sk3_aset(a2, "pid", pid);
    sk3_aset(a2, "started_at", started_at);
    json_t *rec = sk3_call(ctx, "_pid_recycled", a2);
    if (sk3_is_raise(rec)) return rec;
    bool not_rec = !sk3_truthy(rec);
    json_free(rec);
    return json_bool(not_rec);
}

/* ════════════════ hermes_cli/process_identity.py ════════════════ */

/* PoP: sk3_process_birth @ hermes_cli/process_identity.py:process_birth */
static json_t *sk3_process_birth(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "proc" };
    json_t *miss = sk3_missing_args(args, "process_birth", req, 1);
    if (miss) return miss;
    const json_t *spec = json_obj_get(args, "proc");
    /* if getattr(psutil, "MACOS", False): */
    if (sk3_psutil_flag(ctx, "MACOS")) {
        /* platform_proc = getattr(proc, "_proc", None); if platform_proc is not None: */
        const json_t *pp = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "_proc") : NULL;
        if (pp && !sk3_is_null(pp)) {
            /* try: return float(platform_proc.create_time(monotonic=True))
             * except TypeError: pass */
            json_t *a = sk3_args0();
            json_set(a, "monotonic", json_bool(true));
            json_t *r = sk3_call(ctx, "proc._proc.create_time", a);
            bool fallback = false;
            if (sk3_is_raise(r)) {
                if (sk3_exc_isa(sk3_raise_kind(r), "TypeError")) { json_free(r); fallback = true; }
                else return r;   /* other exceptions propagate */
            } else {
                double f; json_t *ferr = NULL;
                if (sk3_py_float(r, &f, &ferr) != 0) {
                    json_free(r);
                    if (sk3_exc_isa(sk3_raise_kind(ferr), "TypeError")) { json_free(ferr); fallback = true; }
                    else return ferr;
                } else {
                    json_free(r);
                    return json_number(f);
                }
            }
            (void)fallback;   /* fall through to proc.create_time() */
        }
    }
    /* return float(proc.create_time()) — proc is always a double */
    json_t *r = sk3_call(ctx, "proc.create_time", sk3_args0());
    if (sk3_is_raise(r)) return r;
    double f; json_t *ferr = NULL;
    if (sk3_py_float(r, &f, &ferr) != 0) { json_free(r); return ferr; }
    json_free(r);
    return json_number(f);
}

/* PoP: sk3_worker_birth @ hermes_cli/process_identity.py:worker_birth */
static json_t *sk3_worker_birth(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "proc" };
    json_t *miss = sk3_missing_args(args, "worker_birth", req, 1);
    if (miss) return miss;
    const json_t *spec = json_obj_get(args, "proc");
    /* if getattr(psutil, "LINUX", False):
     *     return float(proc._proc.create_time(monotonic=True)) */
    if (sk3_psutil_flag(ctx, "LINUX")) {
        bool has_pp = (spec && spec->type == JSON_OBJECT) && json_has(spec, "_proc");
        if (!has_pp)
            return sk3_attr_err("types.SimpleNamespace", "_proc");
        const json_t *pp = json_obj_get(spec, "_proc");
        if (sk3_is_null(pp))
            return sk3_attr_err("NoneType", "create_time");
        json_t *a = sk3_args0();
        json_set(a, "monotonic", json_bool(true));
        json_t *r = sk3_call(ctx, "proc._proc.create_time", a);
        if (sk3_is_raise(r)) return r;
        double f; json_t *ferr = NULL;
        if (sk3_py_float(r, &f, &ferr) != 0) { json_free(r); return ferr; }
        json_free(r);
        return json_number(f);
    }
    /* return process_birth(proc) */
    json_t *a = sk3_args0(); sk3_astr(a, "proc", "proc");
    return sk3_call(ctx, "process_birth", a);
}

/* ════════════════ session_keys.py ════════════════ */

/* PoP: sk3_profile_from_session_key @ session_keys.py:profile_from_session_key */
static json_t *sk3_profile_from_session_key(sk3_ctx_t *ctx, const json_t *args) {
    static const char *req[] = { "session_key" };
    json_t *miss = sk3_missing_args(args, "profile_from_session_key", req, 1);
    if (miss) return miss;
    const json_t *session_key = json_obj_get(args, "session_key");
    /* if not session_key: return None */
    if (!sk3_truthy(session_key)) return json_null();
    /* parts = str(session_key).split(":") */
    char *ps = sk3_pystr(session_key);
    json_t *parts = sk3_split_colon(ps);
    free(ps);
    /* if len(parts) < 2 or parts[0] != "agent": return None */
    if (json_len(parts) < 2) { json_free(parts); return json_null(); }
    const char *p0 = sk3_str(json_get(parts, 0));
    if (!p0 || strcmp(p0, "agent")) { json_free(parts); return json_null(); }
    /* return profile_from_session_key_namespace(parts[1] or "main") */
    const char *p1 = sk3_str(json_get(parts, 1));
    const char *ns = (p1 && p1[0]) ? p1 : "main";
    json_free(parts);
    json_t *a = sk3_args0(); sk3_astr(a, "namespace", ns);
    return sk3_call(ctx, "profile_from_session_key_namespace", a);
}
