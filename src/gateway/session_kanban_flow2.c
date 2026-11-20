/*
 * session_kanban_flow2.c — FLOW port of the REMAINDER of the session_kanban
 * worker-turn surface (hermes-agent).
 *
 * MODULE RESOLUTION: gateway/session_kanban.py itself is fully claimed by
 * src/gateway/tui_methods_kanban_flow.c (all 22 defs, incl. the six nested
 * closures). The unclaimed REMAINDER of that flow is the collaborator cluster
 * session_kanban.py drives but tmk SEAMS: the in-place turn-recovery engine
 * (agent/kanban_turn_recovery.py — recover_failed_kanban_turns is called by
 * _run_task_turns, worker_claim_is_live by _bound_claim_is_live) and the
 * worker exit policy (hermes_cli/turn_exit.py — turn_exit_code /
 * credential_failure_flags are called by _run_worker_turns /
 * record_start_failure). Those are the REAL defining modules; every PoP below
 * credits them (the scanner only credits the defining module). This mirrors
 * tmk's own resolution of a named-but-absent module to its real definers.
 *
 * Ports the DECISION SEQUENCES — branch order, seam call order, exception
 * paths and fail-closed returns — of:
 *   agent/kanban_turn_recovery.py: kanban_task_id, _resolve_task_id,
 *     kanban_turn_recovery_enabled, max_recovery_attempts,
 *     recovery_delay_seconds, _turn_exit_is_terminal, should_recover_turn,
 *     _kanban_db_path, _now, worker_claim_is_live, _truncate,
 *     build_recovery_nudge, _emit_recovery_skipped, recover_failed_kanban_turns.
 *   hermes_cli/turn_exit.py: turn_exit_code, credential_failure_flags.
 *
 * Every live external call enters through ONE universal seam:
 * sk2_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. Ported siblings are SEAMED inside flows (each is exercised by its
 * own direct cases, where the real body runs on both sides).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and sk2_-prefixed.
 *
 * Seam name -> live call (args are informational projections):
 *   kanban_task_id / _kanban_db_path / max_recovery_attempts / _now
 *                            ported siblings, called with no args
 *   _resolve_task_id         _resolve_task_id(task_id)            [sibling]
 *   kanban_turn_recovery_enabled  kanban_turn_recovery_enabled(task_id) [sib]
 *   recovery_delay_seconds   recovery_delay_seconds(attempt)      [sibling]
 *   _turn_exit_is_terminal   _turn_exit_is_terminal(result)       [sibling]
 *   should_recover_turn      should_recover_turn(result, attempt=, task_id=)
 *   _truncate                _truncate(text, limit=300)           [sibling;
 *                            build_recovery_nudge passes text only, the
 *                            defaulted limit:300 is recorded on both sides]
 *   build_recovery_nudge     build_recovery_nudge(result, attempt=,
 *                            max_attempts=, task_id=)             [sibling]
 *   _emit_recovery_skipped   _emit_recovery_skipped(emit, task_id) [sibling;
 *                            emit projects to "emit" when provided, else null]
 *   worker_claim_is_live     the DEFAULT claim check in recover_failed_kanban_
 *                            turns, called with no args          [sibling]
 *   claim_check              an EXPLICIT claim_check callback, called no args
 *   get_result               get_result() callback
 *   turn_fn                  turn_fn(nudge) callback
 *   emit                     emit(message) callback
 *   sleep_fn                 an EXPLICIT sleep_fn(delay) callback
 *   time.sleep               the DEFAULT sleep_fn (time.sleep), {seconds}
 *   time.time                time.time() inside _now
 *   os.getpid                os.getpid()
 *   path.exists              Path(db_path).exists() -> bool [monkeypatched]
 *   path.as_uri              Path(db_path).absolute().as_uri() -> URI string
 *                            [absolute() is identity on the absolute-POSIX
 *                            fixture domain; as_uri monkeypatched]
 *   sqlite3.connect          sqlite3.connect(uri, uri=True) -> conn double
 *   conn.execute             conn.execute(sql, params).fetchone() [the script
 *                            carries the fetchone() row: dict for key access]
 *   conn.close               conn.close() (the inner finally)
 *   log.warning              logger.warning(fmt, *args, exc_info=) — args
 *                            shape per call site: {fmt}, {fmt,args:[...]},
 *                            or {fmt,exc_info:true}
 *   log.debug                logger.debug(fmt, exc_info=True) on a swallowed
 *                            emit failure
 *   is_rate_limited_auth_error  hermes_cli.auth.is_rate_limited_auth_error(exc)
 *                            [exc projects to its fixture node descriptor]
 *
 * Representation contract (shared with tests/oracle_session_kanban_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception site. The FULL builtin hierarchy plus
 *   AuthError(RuntimeError) ride the sk2_exc_parents table, so
 *   isinstance(exc, AuthError) and except Exception behave exactly as live.
 * - credential_failure_flags models the exception cause/context chain as a
 *   fixture ARRAY of node descriptors (node 0 = head); each node carries
 *   {kind, message, relogin_required?, code?, cause?, context?} where cause/
 *   context are INDICES into the array (null = None). Distinct indices are
 *   distinct objects (id() identity); a self-index models a real cycle, which
 *   the seen-set walk terminates exactly as live.
 * - os.environ is a STATE dict (in.state.environ); reads are direct (no seam),
 *   writes never happen here, so out.state echoes in.state verbatim.
 * - Doubles: conn rows are fixture dicts (key access); the fixture domain
 *   carries every key a flow reads. str()/slicing fold the ASCII fixture
 *   corpus only; str()-projected values are int/str/bool/null. Paths are POSIX
 *   (no drive letters/UNC); Path.exists/absolute/as_uri are monkeypatched live.
 * - Tuples project to lists; sets to sorted lists; integral floats to ints.
 * - int() coercion follows Python: number -> trunc toward zero; str -> the
 *   int-strip whitespace set (narrower than str.strip), optional sign, digits
 *   with single underscores BETWEEN digits; failure -> ValueError "invalid
 *   literal for int() with base 10: <repr>". str.strip uses the wider ASCII
 *   White_Space set (incl \x1c-\x1f).
 * - `is True` / `is False` identity gates are JSON_BOOL-only (fixture 1/0 read
 *   as False); truthiness gates use sk2_truthy.
 * - Getter scripts (conn.execute rows) live in {null, non-empty dict}.
 * - Source literals carry their exact UTF-8 bytes (em-dash U+2014 in the
 *   recovery log/nudge texts, ellipsis U+2026 in _truncate); fixture DATA is
 *   ASCII.
 *
 * Ownership: sk2_call ADOPTS args into the trace event; its return is a fresh
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
typedef struct sk2_ctx {
    json_t *state;      /* owned; read-only here, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sk2_ctx_t;

/* ── basic helpers ── */
static bool sk2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *sk2_raise_kind(const json_t *v) {
    return sk2_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool sk2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *sk2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *sk2_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *sk2_type_name(const json_t *v) {
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

static char *sk2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python str() for scalars (containers are outside the str()-projection
 * fixture domain). Integral numbers fold to ints (fixture str() positions
 * carry int/str/bool/null, never floats). */
static char *sk2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sk2_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return sk2_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return sk2_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return sk2_strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool sk2_json_eq(const json_t *a, const json_t *b) {
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
            if (!sk2_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !sk2_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* `is True` / `is False` identity gates: JSON_BOOL only. */
static bool sk2_is_true(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}
static bool sk2_is_false(const json_t *v) {
    return v && v->type == JSON_BOOL && !v->bool_val;
}

/* ── exception hierarchy (FULL builtin tree + the quarry customs) ──
 * Multiple inheritance is a parent LIST. An unknown scripted kind defaults to
 * Exception — exactly the oracle make_exc fallback type(kind, (Exception,)).
 * BaseException-only kinds never reach Exception. AuthError(RuntimeError) is
 * the hermes_cli.auth_constants class credential_failure_flags isinstance-
 * checks. */
typedef struct sk2_exc_parent {
    const char *kind;
    const char *parents[4];   /* NULL-terminated */
} sk2_exc_parent_t;

static const sk2_exc_parent_t SK2_EXC_PARENTS[] = {
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
    {"AuthError", {"RuntimeError", NULL}},
};
#define SK2_EXC_N (sizeof(SK2_EXC_PARENTS) / sizeof(SK2_EXC_PARENTS[0]))

static const sk2_exc_parent_t *sk2_exc_entry(const char *kind) {
    for (size_t i = 0; i < SK2_EXC_N; i++)
        if (!strcmp(SK2_EXC_PARENTS[i].kind, kind)) return &SK2_EXC_PARENTS[i];
    return NULL;
}

/* isinstance(kind, ancestor) over the parent tree; unknown kinds are
 * Exception subclasses (the make_exc fallback). */
static bool sk2_exc_isa(const char *kind, const char *ancestor) {
    if (!kind) return false;
    if (!strcmp(kind, ancestor)) return true;
    const sk2_exc_parent_t *e = sk2_exc_entry(kind);
    if (!e) return !strcmp(ancestor, "Exception") || !strcmp(ancestor, "BaseException");
    for (size_t i = 0; i < 4 && e->parents[i]; i++)
        if (sk2_exc_isa(e->parents[i], ancestor)) return true;
    return false;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool sk2_is_base_exc(const char *kind) {
    return kind && !sk2_exc_isa(kind, "Exception");
}

/* ── the one universal seam (fail-closed) ── */
static json_t *sk2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return sk2_err_spec(type, msg);
}

static json_t *sk2_call(sk2_ctx_t *ctx, const char *name, json_t *args) {
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
        ret = sk2_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sk2_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sk2_make_exc(raise);
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
static json_t *sk2_args0(void) { return json_object(); }
static void sk2_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void sk2_anum(json_t *a, const char *k, double d) {
    json_set(a, k, json_number(d));
}
static void sk2_astr(json_t *a, const char *k, const char *s) {
    if (s) json_set_hdr(a, k, s); else json_set(a, k, json_null());
}

/* ── Python semantics helpers (ASCII fixture domain) ── */
/* str.strip White_Space (ASCII subset incl the \x1c-\x1f file separators). */
static bool sk2_is_strip_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F) || c == 0x20;
}
/* int() strips a NARROWER set: White_Space minus \x1c-\x1f. */
static bool sk2_is_int_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0D) || c == 0x20;
}

/* str.strip -> owned string node, or an AttributeError envelope on non-str. */
static json_t *sk2_strip(const json_t *v) {
    if (!v || v->type == JSON_NULL)
        return sk2_err_spec("AttributeError", "'NoneType' object has no attribute 'strip'");
    if (v->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", sk2_type_name(v));
        return sk2_err_spec("AttributeError", msg);
    }
    const char *s = v->str_val ? v->str_val : "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sk2_is_strip_ws((unsigned char)s[a])) a++;
    while (b > a && sk2_is_strip_ws((unsigned char)s[b - 1])) b--;
    char *out = malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* Python repr() for an ASCII string (int()-error messages). */
static char *sk2_repr_ascii(const char *s) {
    if (!s) s = "";
    int has_sq = strchr(s, '\'') != NULL;
    int has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *o = malloc(cap);
    if (!o) return sk2_strdup("''");
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

/* int() coercion. Returns 0 on success (*out set), -1 on failure (*err owned
 * envelope). number -> trunc toward zero; bool -> 0/1; str -> Python int
 * grammar; null/other -> TypeError. */
static int sk2_py_int(const json_t *v, double *out, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = sk2_err_spec("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return -1;
    }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (v->type == JSON_NUMBER) { *out = trunc(v->num_val); return 0; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && sk2_is_int_ws((unsigned char)s[a])) a++;
        while (b > a && sk2_is_int_ws((unsigned char)s[b - 1])) b--;
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
        char *rp = sk2_repr_ascii(s);
        size_t ml = strlen(rp) + 64;
        char *msg = malloc(ml);
        if (msg) snprintf(msg, ml, "invalid literal for int() with base 10: %s", rp);
        *err = sk2_err_spec("ValueError", msg ? msg : "");
        free(msg); free(rp);
        return -1;
    }
    char msg[160];
    snprintf(msg, sizeof msg,
        "int() argument must be a string, a bytes-like object or a real number, not '%s'",
        sk2_type_name(v));
    *err = sk2_err_spec("TypeError", msg);
    return -1;
}

/* os.environ.get(key) — a direct STATE read (no seam). */
static const json_t *sk2_env_get(sk2_ctx_t *ctx, const char *key) {
    const json_t *env = json_obj_get(ctx->state, "environ");
    if (!env || env->type != JSON_OBJECT) return NULL;
    return json_obj_get(env, key);
}

static bool sk2_str_in_set(const char *s, const char *const *set, size_t n) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (!strcmp(s, set[i])) return true;
    return false;
}

static bool sk2_in_array(const json_t *v, const json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (sk2_json_eq(v, json_get(arr, i))) return true;
    return false;
}

static bool sk2_startswith(const char *s, const char *pre) {
    return s && pre && strncmp(s, pre, strlen(pre)) == 0;
}

/* logger.warning arg shape: {fmt, args?:[...], exc_info?:true}. */
static json_t *sk2_log_args(const char *fmt, const json_t *positional, bool exc_info) {
    json_t *a = json_object();
    json_set_hdr(a, "fmt", fmt);
    if (positional) json_set(a, "args", json_copy(positional));
    if (exc_info) json_set(a, "exc_info", json_bool(true));
    return a;
}

/* ── module constants (hermes_cli/turn_exit.py + kanban_turn_recovery.py) ── */
static const char *SK2_TRANSIENT[] = {
    "rate_limit", "upstream_rate_limit", "billing", "overloaded", "server_error", "timeout",
};
static const char *SK2_TERMINAL[] = {
    "auth", "auth_permanent", "model_not_found", "ssl_cert_verification", "upstream_blocked",
};
static const char *SK2_NON_RECOVERABLE[] = { "rate_limit", "billing", "upstream_rate_limit" };
static const char *SK2_OFF_VALUES[] = { "0", "false", "no", "off" };
#define SK2_DEFAULT_MAX_RECOVERY 3
static const double SK2_RECOVERY_DELAYS[] = { 15.0, 45.0, 90.0 };

/* ════════════════ agent/kanban_turn_recovery.py ════════════════ */

/* PoP: sk2_kanban_task_id @ agent/kanban_turn_recovery.py:kanban_task_id */
static json_t *sk2_kanban_task_id(sk2_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *e = sk2_env_get(ctx, "HERMES_KANBAN_TASK");
    /* (os.environ.get(...) or "").strip() */
    json_t *base = sk2_truthy(e) ? json_copy(e) : json_string("");
    json_t *stripped = sk2_strip(base);
    json_free(base);
    if (sk2_is_raise(stripped)) return stripped;
    /* task_id or None */
    if (!sk2_truthy(stripped)) { json_free(stripped); return json_null(); }
    return stripped;
}

/* PoP: sk2_resolve_task_id @ agent/kanban_turn_recovery.py:_resolve_task_id */
static json_t *sk2_resolve_task_id(sk2_ctx_t *ctx, const json_t *args) {
    const json_t *tid = json_obj_get(args, "task_id");
    if (!tid || tid->type == JSON_NULL)
        return sk2_call(ctx, "kanban_task_id", sk2_args0());
    /* task_id.strip() or None */
    json_t *stripped = sk2_strip(tid);
    if (sk2_is_raise(stripped)) return stripped;
    if (!sk2_truthy(stripped)) { json_free(stripped); return json_null(); }
    return stripped;
}

/* PoP: sk2_kanban_turn_recovery_enabled @ agent/kanban_turn_recovery.py:kanban_turn_recovery_enabled */
static json_t *sk2_kanban_turn_recovery_enabled(sk2_ctx_t *ctx, const json_t *args) {
    json_t *a = sk2_args0();
    sk2_aset(a, "task_id", json_obj_get(args, "task_id"));
    json_t *r = sk2_call(ctx, "_resolve_task_id", a);
    if (sk2_is_raise(r)) return r;
    bool is_none = !r || r->type == JSON_NULL;
    json_free(r);
    if (is_none) return json_bool(false);
    json_t *m = sk2_call(ctx, "max_recovery_attempts", sk2_args0());
    if (sk2_is_raise(m)) return m;
    bool gt = m->type == JSON_NUMBER && m->num_val > 0.0;
    json_free(m);
    return json_bool(gt);
}

/* PoP: sk2_max_recovery_attempts @ agent/kanban_turn_recovery.py:max_recovery_attempts */
static json_t *sk2_max_recovery_attempts(sk2_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *e = sk2_env_get(ctx, "HERMES_KANBAN_TURN_RECOVERY");
    json_t *base = sk2_truthy(e) ? json_copy(e) : json_string("");
    json_t *stripped = sk2_strip(base);
    json_free(base);
    if (sk2_is_raise(stripped)) return stripped;
    const char *raw = sk2_str(stripped);
    if (!raw || !raw[0]) { json_free(stripped); return json_number(SK2_DEFAULT_MAX_RECOVERY); }
    /* raw.lower() in _OFF_VALUES (ASCII lower) */
    char *lower = sk2_strdup(raw);
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);
    bool off = sk2_str_in_set(lower, SK2_OFF_VALUES, sizeof(SK2_OFF_VALUES) / sizeof(SK2_OFF_VALUES[0]));
    free(lower);
    if (off) { json_free(stripped); return json_number(0); }
    double val; json_t *err = NULL;
    if (sk2_py_int(stripped, &val, &err) != 0) {
        /* except (TypeError, ValueError): return DEFAULT */
        json_free(err); json_free(stripped);
        return json_number(SK2_DEFAULT_MAX_RECOVERY);
    }
    json_free(stripped);
    double clamped = val < 0.0 ? 0.0 : (val > 10.0 ? 10.0 : val);
    return json_number(clamped);
}

/* PoP: sk2_recovery_delay_seconds @ agent/kanban_turn_recovery.py:recovery_delay_seconds */
static json_t *sk2_recovery_delay_seconds(sk2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *at = json_obj_get(args, "attempt");
    if (!at)
        return sk2_err_spec("TypeError",
            "recovery_delay_seconds() missing 1 required positional argument: 'attempt'");
    double attempt = at->type == JSON_NUMBER ? at->num_val : 0.0;
    double a = attempt > 1.0 ? attempt : 1.0;   /* max(attempt, 1) */
    double idx = a - 1.0;
    if (idx > 2.0) idx = 2.0;                    /* min(attempt-1, len-1) */
    if (idx < 0.0) idx = 0.0;
    return json_number(SK2_RECOVERY_DELAYS[(size_t)idx]);
}

/* PoP: sk2_turn_exit_is_terminal @ agent/kanban_turn_recovery.py:_turn_exit_is_terminal */
static json_t *sk2_turn_exit_is_terminal(sk2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *result = json_obj_get(args, "result");
    if (!result || result->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", sk2_type_name(result));
        return sk2_err_spec("AttributeError", msg);
    }
    const json_t *v = json_obj_get(result, "turn_exit_reason");
    char *s = sk2_truthy(v) ? sk2_pystr(v) : sk2_strdup("");
    bool term = sk2_startswith(s, "max_iterations_reached");
    free(s);
    return json_bool(term);
}

/* PoP: sk2_should_recover_turn @ agent/kanban_turn_recovery.py:should_recover_turn */
static json_t *sk2_should_recover_turn(sk2_ctx_t *ctx, const json_t *args) {
    if (!json_has(args, "attempt"))
        return sk2_err_spec("TypeError",
            "should_recover_turn() missing 1 required keyword-only argument: 'attempt'");
    const json_t *at = json_obj_get(args, "attempt");
    double attempt = at && at->type == JSON_NUMBER ? at->num_val : 0.0;

    json_t *a = sk2_args0();
    sk2_aset(a, "task_id", json_obj_get(args, "task_id"));
    json_t *en = sk2_call(ctx, "kanban_turn_recovery_enabled", a);
    if (sk2_is_raise(en)) return en;
    bool enabled = sk2_truthy(en);
    json_free(en);
    if (!enabled) return json_bool(false);

    json_t *m = sk2_call(ctx, "max_recovery_attempts", sk2_args0());
    if (sk2_is_raise(m)) return m;
    double maxa = m->type == JSON_NUMBER ? m->num_val : 0.0;
    json_free(m);
    if (attempt >= maxa) return json_bool(false);

    const json_t *result = json_obj_get(args, "result");
    if (!result || result->type != JSON_OBJECT) return json_bool(false);
    if (sk2_is_true(json_obj_get(result, "interrupted"))) return json_bool(false);

    json_t *ta = sk2_args0();
    sk2_aset(ta, "result", result);
    json_t *term = sk2_call(ctx, "_turn_exit_is_terminal", ta);
    if (sk2_is_raise(term)) return term;
    bool is_term = sk2_truthy(term);
    json_free(term);
    if (is_term) return json_bool(false);

    if (!sk2_is_true(json_obj_get(result, "failed"))) return json_bool(false);
    if (!sk2_is_true(json_obj_get(result, "failure_retryable"))) return json_bool(false);

    const json_t *fr = json_obj_get(result, "failure_reason");
    char *reason = sk2_truthy(fr) ? sk2_pystr(fr) : sk2_strdup("");
    bool nonrec = sk2_str_in_set(reason, SK2_NON_RECOVERABLE,
        sizeof(SK2_NON_RECOVERABLE) / sizeof(SK2_NON_RECOVERABLE[0]));
    free(reason);
    if (nonrec) return json_bool(false);
    return json_bool(true);
}

/* PoP: sk2_kanban_db_path @ agent/kanban_turn_recovery.py:_kanban_db_path */
static json_t *sk2_kanban_db_path(sk2_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *e = sk2_env_get(ctx, "HERMES_KANBAN_DB");
    json_t *base = sk2_truthy(e) ? json_copy(e) : json_string("");
    json_t *stripped = sk2_strip(base);
    json_free(base);
    if (sk2_is_raise(stripped)) return stripped;
    if (!sk2_truthy(stripped)) { json_free(stripped); return json_null(); }
    return stripped;
}

/* PoP: sk2_now @ agent/kanban_turn_recovery.py:_now */
static json_t *sk2_now(sk2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *t = sk2_call(ctx, "time.time", sk2_args0());
    if (sk2_is_raise(t)) return t;
    double d = t->type == JSON_NUMBER ? t->num_val : 0.0;
    json_free(t);
    return json_number(trunc(d));   /* int(time.time()) */
}

/* PoP: sk2_truncate @ agent/kanban_turn_recovery.py:_truncate */
static json_t *sk2_truncate(sk2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *text = json_obj_get(args, "text");
    const json_t *lim = json_obj_get(args, "limit");
    double limit = lim && lim->type == JSON_NUMBER ? lim->num_val : 300.0;
    /* (text or "") */
    json_t *base = sk2_truthy(text) ? json_copy(text) : json_string("");
    json_t *stripped = sk2_strip(base);
    json_free(base);
    if (sk2_is_raise(stripped)) return stripped;
    /* .replace("\n", " ") */
    const char *s = sk2_str(stripped);
    size_t n = strlen(s);
    char *buf = malloc(n + 1);
    if (buf) for (size_t i = 0; i < n; i++) buf[i] = s[i] == '\n' ? ' ' : s[i];
    if (buf) buf[n] = '\0';
    json_free(stripped);
    /* len(text) > limit -> text[:limit-1] + "…" (limit >= 1 fixture domain) */
    long long llim = (long long)limit;
    if (llim < 1) llim = 1;
    if ((long long)n > llim) {
        size_t cut = (size_t)(llim - 1);
        /* UTF-8 ellipsis U+2026 */
        char *out = malloc(cut + 4);
        if (out) { memcpy(out, buf, cut); out[cut] = '\xe2'; out[cut + 1] = '\x80'; out[cut + 2] = '\xa6'; out[cut + 3] = '\0'; }
        json_t *r = json_string(out ? out : "");
        free(out); free(buf);
        return r;
    }
    json_t *r = json_string(buf ? buf : "");
    free(buf);
    return r;
}

/* The build_recovery_nudge f-string template (exact live literals; %lld =
 * attempt/max_attempts, first %s = error-or-default, second %s = task_id). */
#define SK2_NUDGE_FMT \
    "[System: the previous turn ended UNFINISHED \xe2\x80\x94 the API call failed after " \
    "all retries and the turn was interrupted (recovery attempt %lld/%lld). " \
    "Error: %s.\n\n" \
    "Task `%s` is still `running`. This is the SAME session, with your full " \
    "context \xe2\x80\x94 nothing you already read, wrote, or computed is lost. Do NOT start over.\n\n" \
    "Do this immediately:\n" \
    "1. If the interrupted turn was mid tool-call, re-check the on-disk state of " \
    "whatever it was writing before continuing (the action may not have executed).\n" \
    "2. Continue the task from where it stopped and finish any remaining deliverable.\n" \
    "3. End with a terminal `kanban_complete(summary=..., artifacts=[...])` if the work " \
    "is done, or `kanban_block(reason=...)` if you are blocked.]"

/* PoP: sk2_build_recovery_nudge @ agent/kanban_turn_recovery.py:build_recovery_nudge */
static json_t *sk2_build_recovery_nudge(sk2_ctx_t *ctx, const json_t *args) {
    bool has_attempt = json_has(args, "attempt");
    bool has_max = json_has(args, "max_attempts");
    if (!has_attempt && !has_max)
        return sk2_err_spec("TypeError",
            "build_recovery_nudge() missing 2 required keyword-only arguments: 'attempt' and 'max_attempts'");
    if (!has_attempt)
        return sk2_err_spec("TypeError",
            "build_recovery_nudge() missing 1 required keyword-only argument: 'attempt'");
    if (!has_max)
        return sk2_err_spec("TypeError",
            "build_recovery_nudge() missing 1 required keyword-only argument: 'max_attempts'");
    const json_t *at = json_obj_get(args, "attempt");
    const json_t *ma = json_obj_get(args, "max_attempts");
    long long attempt = at && at->type == JSON_NUMBER ? (long long)at->num_val : 0;
    long long max_attempts = ma && ma->type == JSON_NUMBER ? (long long)ma->num_val : 0;

    /* error = "" ; if isinstance(result, dict): error = _truncate(str(...)) */
    char *error = sk2_strdup("");
    const json_t *result = json_obj_get(args, "result");
    if (result && result->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(result, "error");
        if (!sk2_truthy(v)) v = json_obj_get(result, "final_response");
        char *s = sk2_truthy(v) ? sk2_pystr(v) : sk2_strdup("");
        json_t *ta = sk2_args0();
        sk2_astr(ta, "text", s);
        sk2_anum(ta, "limit", 300.0);   /* defaulted limit recorded both sides */
        free(s);
        json_t *tr = sk2_call(ctx, "_truncate", ta);
        if (sk2_is_raise(tr)) { free(error); return tr; }
        free(error);
        error = sk2_strdup(sk2_str(tr) ? sk2_str(tr) : "");
        json_free(tr);
    }
    /* task_id = _resolve_task_id(task_id) or "this task" */
    json_t *ra = sk2_args0();
    sk2_aset(ra, "task_id", json_obj_get(args, "task_id"));
    json_t *tid = sk2_call(ctx, "_resolve_task_id", ra);
    if (sk2_is_raise(tid)) { free(error); return tid; }
    char *task_id = sk2_truthy(tid) ? sk2_pystr(tid) : sk2_strdup("this task");
    json_free(tid);

    const char *error_or = (error && error[0]) ? error : "provider stream failure";
    size_t cap = strlen(SK2_NUDGE_FMT) + strlen(error_or) + strlen(task_id) + 64;
    char *out = malloc(cap);
    if (out) snprintf(out, cap, SK2_NUDGE_FMT, attempt, max_attempts, error_or, task_id);
    json_t *r = json_string(out ? out : "");
    free(out); free(error); free(task_id);
    return r;
}

/* The _emit_recovery_skipped / recover-loop message templates. */
#define SK2_SKIP_FMT \
    "[kanban] in-place recovery skipped for %s: this worker no longer holds a live " \
    "run/claim (already settled, reclaimed, or unverifiable)"
#define SK2_LOOP_FMT \
    "[kanban] provider failure on %s \xe2\x80\x94 retrying the turn in place " \
    "(attempt %lld/%lld) after %llds; session context preserved"
#define SK2_EMIT_FAIL_FMT "kanban turn-recovery emit failed"

/* PoP: sk2_emit_recovery_skipped @ agent/kanban_turn_recovery.py:_emit_recovery_skipped */
static json_t *sk2_emit_recovery_skipped(sk2_ctx_t *ctx, const json_t *args) {
    if (!json_has(args, "emit"))
        return sk2_err_spec("TypeError",
            "_emit_recovery_skipped() missing 1 required positional argument: 'emit'");
    json_t *ra = sk2_args0();
    sk2_aset(ra, "task_id", json_obj_get(args, "task_id"));
    json_t *tid = sk2_call(ctx, "_resolve_task_id", ra);
    if (sk2_is_raise(tid)) return tid;
    char *t = sk2_truthy(tid) ? sk2_pystr(tid) : sk2_strdup("task");
    json_free(tid);
    size_t cap = strlen(SK2_SKIP_FMT) + strlen(t) + 8;
    char *message = malloc(cap);
    if (message) snprintf(message, cap, SK2_SKIP_FMT, t);
    free(t);
    /* logger.warning("%s", message) — NOT in a try */
    json_t *arr = json_array();
    json_append(arr, json_string(message ? message : ""));
    json_t *w = sk2_call(ctx, "log.warning", sk2_log_args("%s", arr, false));
    json_free(arr);
    if (sk2_is_raise(w)) { free(message); return w; }
    json_free(w);
    /* if emit is not None: try emit(message) except Exception: logger.debug */
    const json_t *emit = json_obj_get(args, "emit");
    if (emit && emit->type != JSON_NULL) {
        json_t *ea = sk2_args0();
        sk2_astr(ea, "message", message);
        json_t *e = sk2_call(ctx, "emit", ea);
        if (sk2_is_raise(e)) {
            if (sk2_is_base_exc(sk2_raise_kind(e))) { free(message); return e; }
            json_free(e);
            json_t *d = sk2_call(ctx, "log.debug", sk2_log_args(SK2_EMIT_FAIL_FMT, NULL, true));
            if (sk2_is_raise(d)) { free(message); return d; }
            json_free(d);
        } else json_free(e);
    }
    free(message);
    return json_null();
}

/* PoP: sk2_worker_claim_is_live @ agent/kanban_turn_recovery.py:worker_claim_is_live */
static json_t *sk2_worker_claim_is_live(sk2_ctx_t *ctx, const json_t *args) {
    /* task_id = _resolve_task_id(task_id) */
    json_t *ra = sk2_args0();
    sk2_aset(ra, "task_id", json_obj_get(args, "task_id"));
    json_t *tid = sk2_call(ctx, "_resolve_task_id", ra);
    if (sk2_is_raise(tid)) return tid;
    if (!tid || tid->type == JSON_NULL) { json_free(tid); return json_bool(false); }
    char *task_id = sk2_pystr(tid);
    json_free(tid);

    /* db_path = _kanban_db_path() if db_path is None else (db_path.strip() or None) */
    const json_t *db_arg = json_obj_get(args, "db_path");
    json_t *db;
    if (!db_arg || db_arg->type == JSON_NULL) {
        db = sk2_call(ctx, "_kanban_db_path", sk2_args0());
        if (sk2_is_raise(db)) { free(task_id); return db; }
    } else {
        json_t *st = sk2_strip(db_arg);
        if (sk2_is_raise(st)) { free(task_id); return st; }
        if (sk2_truthy(st)) db = st;
        else { json_free(st); db = json_null(); }
    }
    const char *db_str = sk2_str(db);

    /* if not db_path or not Path(db_path).exists(): warning(fmt1, db_path); False */
    bool exists = false;
    if (sk2_truthy(db)) {
        json_t *ea = sk2_args0();
        sk2_astr(ea, "path", db_str);
        json_t *e = sk2_call(ctx, "path.exists", ea);
        if (sk2_is_raise(e)) { json_free(db); free(task_id); return e; }
        exists = sk2_truthy(e);
        json_free(e);
    }
    if (!sk2_truthy(db) || !exists) {
        json_t *arr = json_array();
        json_append(arr, db ? json_copy(db) : json_null());
        json_t *w = sk2_call(ctx, "log.warning", sk2_log_args(
            "kanban claim check: pinned board db %r not found \xe2\x80\x94 not retrying in place",
            arr, false));
        json_free(arr); json_free(db); free(task_id);
        if (sk2_is_raise(w)) return w;
        json_free(w);
        return json_bool(false);
    }

    /* run_id_env / lock_env = str(arg if not None else env.get(...) or "").strip() */
    const json_t *rid_arg = json_obj_get(args, "run_id");
    char *rid_s;
    if (rid_arg && rid_arg->type != JSON_NULL) rid_s = sk2_pystr(rid_arg);
    else {
        const json_t *e = sk2_env_get(ctx, "HERMES_KANBAN_RUN_ID");
        rid_s = sk2_truthy(e) ? sk2_pystr(e) : sk2_strdup("");
    }
    json_t *rid_node = json_string(rid_s); free(rid_s);
    json_t *rid_strip = sk2_strip(rid_node); json_free(rid_node);
    char *run_id_env = sk2_strdup(sk2_str(rid_strip) ? sk2_str(rid_strip) : "");
    json_free(rid_strip);

    const json_t *lock_arg = json_obj_get(args, "claim_lock");
    char *lock_s;
    if (lock_arg && lock_arg->type != JSON_NULL) lock_s = sk2_pystr(lock_arg);
    else {
        const json_t *e = sk2_env_get(ctx, "HERMES_KANBAN_CLAIM_LOCK");
        lock_s = sk2_truthy(e) ? sk2_pystr(e) : sk2_strdup("");
    }
    json_t *lock_node = json_string(lock_s); free(lock_s);
    json_t *lock_strip = sk2_strip(lock_node); json_free(lock_node);
    char *lock_env = sk2_strdup(sk2_str(lock_strip) ? sk2_str(lock_strip) : "");
    json_free(lock_strip);

    if (!run_id_env[0] || !lock_env[0]) {
        json_t *w = sk2_call(ctx, "log.warning", sk2_log_args(
            "kanban claim check: missing dispatcher-pinned run-id/claim-lock carrier "
            "\xe2\x80\x94 no exact authority to re-prove, not retrying in place",
            NULL, false));
        free(run_id_env); free(lock_env); json_free(db); free(task_id);
        if (sk2_is_raise(w)) return w;
        json_free(w);
        return json_bool(false);
    }

    /* now = _now() */
    json_t *now_node = sk2_call(ctx, "_now", sk2_args0());
    if (sk2_is_raise(now_node)) { free(run_id_env); free(lock_env); json_free(db); free(task_id); return now_node; }
    double now = now_node->type == JSON_NUMBER ? now_node->num_val : 0.0;
    json_free(now_node);

    const json_t *vp = json_obj_get(args, "verify_pid");
    bool verify_pid = !vp || vp->type == JSON_NULL ? true : sk2_truthy(vp);

    /* try: conn = sqlite3.connect(f"{Path(db_path).absolute().as_uri()}?mode=ro", uri=True) */
    json_t *exc = NULL;     /* pending exception envelope */
    json_t *ret = NULL;     /* pending return value */
    bool conn_open = false;

    json_t *ua = sk2_args0();
    sk2_astr(ua, "path", db_str);
    json_t *uri = sk2_call(ctx, "path.as_uri", ua);
    if (sk2_is_raise(uri)) { exc = uri; goto outer_except; }
    const char *uri_str = sk2_str(uri) ? sk2_str(uri) : "";
    size_t plen = strlen(uri_str) + 16;
    char *conn_path = malloc(plen);
    if (conn_path) snprintf(conn_path, plen, "%s?mode=ro", uri_str);
    json_free(uri);
    json_t *ca = sk2_args0();
    sk2_astr(ca, "path", conn_path);
    json_set(ca, "uri", json_bool(true));
    free(conn_path);
    json_t *cr = sk2_call(ctx, "sqlite3.connect", ca);
    if (sk2_is_raise(cr)) { exc = cr; goto outer_except; }
    json_free(cr);
    conn_open = true;   /* the conn double exists; row_factory set is silent */

    /* inner try: row = conn.execute(SQL1, (task_id,)).fetchone() */
    {
        json_t *pa = json_array();
        json_append(pa, json_string(task_id));
        json_t *xa = sk2_args0();
        json_set_hdr(xa, "sql",
            "SELECT status, worker_pid, current_run_id, claim_lock, claim_expires FROM tasks WHERE id = ?");
        json_set(xa, "params", pa);
        json_t *row = sk2_call(ctx, "conn.execute", xa);
        if (sk2_is_raise(row)) { exc = row; goto inner_finally; }
        if (!row || row->type == JSON_NULL) { json_free(row); ret = json_bool(false); goto inner_finally; }
        if (row->type != JSON_OBJECT) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object is not subscriptable", sk2_type_name(row));
            json_free(row); exc = sk2_err_spec("TypeError", msg); goto inner_finally;
        }
        /* (row["status"] or "") != "running" */
        const json_t *status = json_obj_get(row, "status");
        char *status_s = sk2_truthy(status) ? sk2_pystr(status) : sk2_strdup("");
        bool running = strcmp(status_s, "running") == 0;
        free(status_s);
        if (!running) { json_free(row); ret = json_bool(false); goto inner_finally; }
        /* verify_pid and worker_pid is not None and int(worker_pid) != os.getpid() */
        const json_t *wpid = json_obj_get(row, "worker_pid");
        if (verify_pid && wpid && wpid->type != JSON_NULL) {
            double iv; json_t *ie = NULL;
            if (sk2_py_int(wpid, &iv, &ie) != 0) { json_free(row); exc = ie; goto inner_finally; }
            json_t *pid = sk2_call(ctx, "os.getpid", sk2_args0());
            if (sk2_is_raise(pid)) { json_free(row); exc = pid; goto inner_finally; }
            double p = pid->type == JSON_NUMBER ? pid->num_val : 0.0;
            json_free(pid);
            if (iv != p) { json_free(row); ret = json_bool(false); goto inner_finally; }
        }
        /* current_run_id checks */
        const json_t *crid = json_obj_get(row, "current_run_id");
        if (!crid || crid->type == JSON_NULL) { json_free(row); ret = json_bool(false); goto inner_finally; }
        char *crid_s = sk2_pystr(crid);
        bool rid_match = strcmp(crid_s, run_id_env) == 0;
        free(crid_s);
        if (!rid_match) { json_free(row); ret = json_bool(false); goto inner_finally; }
        const json_t *cl = json_obj_get(row, "claim_lock");
        char *cl_s = sk2_truthy(cl) ? sk2_pystr(cl) : sk2_strdup("");
        bool cl_match = strcmp(cl_s, lock_env) == 0;
        free(cl_s);
        if (!cl_match) { json_free(row); ret = json_bool(false); goto inner_finally; }
        const json_t *ce = json_obj_get(row, "claim_expires");
        if (!ce || ce->type == JSON_NULL) { json_free(row); ret = json_bool(false); goto inner_finally; }
        double ce_v; json_t *ce_e = NULL;
        if (sk2_py_int(ce, &ce_v, &ce_e) != 0) { json_free(row); exc = ce_e; goto inner_finally; }
        if (ce_v < now) { json_free(row); ret = json_bool(false); goto inner_finally; }
        /* run = conn.execute(SQL2, (int(current_run_id),)).fetchone() */
        double crid_int; json_t *crid_e = NULL;
        if (sk2_py_int(crid, &crid_int, &crid_e) != 0) { json_free(row); exc = crid_e; goto inner_finally; }
        json_free(row);
        json_t *pa2 = json_array();
        json_append(pa2, json_number(crid_int));
        json_t *xa2 = sk2_args0();
        json_set_hdr(xa2, "sql",
            "SELECT ended_at, worker_pid, claim_expires FROM task_runs WHERE id = ?");
        json_set(xa2, "params", pa2);
        json_t *run = sk2_call(ctx, "conn.execute", xa2);
        if (sk2_is_raise(run)) { exc = run; goto inner_finally; }
        if (!run || run->type == JSON_NULL) { json_free(run); ret = json_bool(false); goto inner_finally; }
        if (run->type != JSON_OBJECT) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object is not subscriptable", sk2_type_name(run));
            json_free(run); exc = sk2_err_spec("TypeError", msg); goto inner_finally;
        }
        const json_t *ended = json_obj_get(run, "ended_at");
        if (ended && ended->type != JSON_NULL) { json_free(run); ret = json_bool(false); goto inner_finally; }
        const json_t *rpid = json_obj_get(run, "worker_pid");
        if (verify_pid && rpid && rpid->type != JSON_NULL) {
            double iv; json_t *ie = NULL;
            if (sk2_py_int(rpid, &iv, &ie) != 0) { json_free(run); exc = ie; goto inner_finally; }
            json_t *pid = sk2_call(ctx, "os.getpid", sk2_args0());
            if (sk2_is_raise(pid)) { json_free(run); exc = pid; goto inner_finally; }
            double p = pid->type == JSON_NUMBER ? pid->num_val : 0.0;
            json_free(pid);
            if (iv != p) { json_free(run); ret = json_bool(false); goto inner_finally; }
        }
        const json_t *rce = json_obj_get(run, "claim_expires");
        if (!rce || rce->type == JSON_NULL) { json_free(run); ret = json_bool(false); goto inner_finally; }
        double rce_v; json_t *rce_e = NULL;
        if (sk2_py_int(rce, &rce_v, &rce_e) != 0) { json_free(run); exc = rce_e; goto inner_finally; }
        json_free(run);
        if (rce_v < now) { ret = json_bool(false); goto inner_finally; }
        ret = json_bool(true);
    }

inner_finally:
    if (conn_open) {
        json_t *c = sk2_call(ctx, "conn.close", sk2_args0());
        if (sk2_is_raise(c)) {
            /* a finally raise replaces the pending return/exception */
            if (exc) json_free(exc);
            if (ret) json_free(ret);
            exc = c; ret = NULL;
        } else json_free(c);
    }
    if (exc) goto outer_except;
    free(run_id_env); free(lock_env); json_free(db); free(task_id);
    return ret;

outer_except:
    free(run_id_env); free(lock_env); json_free(db); free(task_id);
    if (sk2_is_base_exc(sk2_raise_kind(exc))) return exc;   /* BaseException passes */
    json_free(exc);
    json_t *w = sk2_call(ctx, "log.warning", sk2_log_args(
        "kanban claim check failed \xe2\x80\x94 not retrying in place", NULL, true));
    if (sk2_is_raise(w)) return w;
    json_free(w);
    return json_bool(false);
}

/* PoP: sk2_recover_failed_kanban_turns @ agent/kanban_turn_recovery.py:recover_failed_kanban_turns */
static json_t *sk2_recover_failed_kanban_turns(sk2_ctx_t *ctx, const json_t *args) {
    bool has_turn = json_has(args, "turn_fn");
    bool has_get = json_has(args, "get_result");
    if (!has_turn && !has_get)
        return sk2_err_spec("TypeError",
            "recover_failed_kanban_turns() missing 2 required positional arguments: 'turn_fn' and 'get_result'");
    if (!has_get)
        return sk2_err_spec("TypeError",
            "recover_failed_kanban_turns() missing 1 required positional argument: 'get_result'");

    const json_t *claim_check = json_obj_get(args, "claim_check");
    bool cc_provided = claim_check && claim_check->type != JSON_NULL && sk2_truthy(claim_check);
    const char *check_name = cc_provided ? "claim_check" : "worker_claim_is_live";
    const json_t *sleep_arg = json_obj_get(args, "sleep_fn");
    bool sleep_provided = sleep_arg && sleep_arg->type != JSON_NULL && sk2_truthy(sleep_arg);
    const char *sleep_name = sleep_provided ? "sleep_fn" : "time.sleep";
    const json_t *emit_arg = json_obj_get(args, "emit");
    bool emit_provided = emit_arg && emit_arg->type != JSON_NULL && sk2_truthy(emit_arg);
    const json_t *task_id = json_obj_get(args, "task_id");

    long long attempts = 0;
    for (;;) {
        /* result = get_result() */
        json_t *result = sk2_call(ctx, "get_result", sk2_args0());
        if (sk2_is_raise(result)) return result;
        /* if not should_recover_turn(result, attempt=attempts, task_id=task_id) */
        json_t *sa = sk2_args0();
        sk2_aset(sa, "result", result);
        sk2_anum(sa, "attempt", (double)attempts);
        sk2_aset(sa, "task_id", task_id);
        json_t *sr = sk2_call(ctx, "should_recover_turn", sa);
        if (sk2_is_raise(sr)) { json_free(result); return sr; }
        bool recover = sk2_truthy(sr);
        json_free(sr);
        if (!recover) { json_free(result); return json_number((double)attempts); }
        /* if not check(): _emit_recovery_skipped(emit, task_id); return attempts */
        json_t *ck = sk2_call(ctx, check_name, sk2_args0());
        if (sk2_is_raise(ck)) { json_free(result); return ck; }
        bool live = sk2_truthy(ck);
        json_free(ck);
        if (!live) {
            json_t *ea = sk2_args0();
            sk2_astr(ea, "emit", emit_provided ? "emit" : NULL);
            sk2_aset(ea, "task_id", task_id);
            json_t *es = sk2_call(ctx, "_emit_recovery_skipped", ea);
            json_free(result);
            if (sk2_is_raise(es)) return es;
            json_free(es);
            return json_number((double)attempts);
        }
        attempts += 1;
        /* delay = recovery_delay_seconds(attempts) */
        json_t *da = sk2_args0();
        sk2_anum(da, "attempt", (double)attempts);
        json_t *dl = sk2_call(ctx, "recovery_delay_seconds", da);
        if (sk2_is_raise(dl)) { json_free(result); return dl; }
        double delay = dl->type == JSON_NUMBER ? dl->num_val : 0.0;
        /* message f-string: _resolve_task_id then max_recovery_attempts */
        json_t *ra = sk2_args0();
        sk2_aset(ra, "task_id", task_id);
        json_t *tid = sk2_call(ctx, "_resolve_task_id", ra);
        if (sk2_is_raise(tid)) { json_free(dl); json_free(result); return tid; }
        char *t = sk2_truthy(tid) ? sk2_pystr(tid) : sk2_strdup("task");
        json_free(tid);
        json_t *ma = sk2_call(ctx, "max_recovery_attempts", sk2_args0());
        if (sk2_is_raise(ma)) { free(t); json_free(dl); json_free(result); return ma; }
        long long maxa = ma->type == JSON_NUMBER ? (long long)ma->num_val : 0;
        json_free(ma);
        size_t cap = strlen(SK2_LOOP_FMT) + strlen(t) + 64;
        char *message = malloc(cap);
        if (message) snprintf(message, cap, SK2_LOOP_FMT, t, attempts, maxa, (long long)trunc(delay));
        free(t);
        /* logger.warning("%s", message) */
        json_t *arr = json_array();
        json_append(arr, json_string(message ? message : ""));
        json_t *w = sk2_call(ctx, "log.warning", sk2_log_args("%s", arr, false));
        json_free(arr);
        if (sk2_is_raise(w)) { free(message); json_free(dl); json_free(result); return w; }
        json_free(w);
        /* if emit is not None: try emit(message) except Exception: logger.debug */
        if (emit_provided) {
            json_t *ea = sk2_args0();
            sk2_astr(ea, "message", message);
            json_t *e = sk2_call(ctx, "emit", ea);
            if (sk2_is_raise(e)) {
                if (sk2_is_base_exc(sk2_raise_kind(e))) { free(message); json_free(dl); json_free(result); return e; }
                json_free(e);
                json_t *d = sk2_call(ctx, "log.debug", sk2_log_args(SK2_EMIT_FAIL_FMT, NULL, true));
                if (sk2_is_raise(d)) { free(message); json_free(dl); json_free(result); return d; }
                json_free(d);
            } else json_free(e);
        }
        free(message);
        /* sleep_fn(delay) */
        json_t *sl_a = sk2_args0();
        sk2_anum(sl_a, "seconds", delay);
        json_t *sl = sk2_call(ctx, sleep_name, sl_a);
        if (sk2_is_raise(sl)) { json_free(dl); json_free(result); return sl; }
        json_free(sl);
        json_free(dl);
        /* re-prove AFTER backoff */
        json_t *ck2 = sk2_call(ctx, check_name, sk2_args0());
        if (sk2_is_raise(ck2)) { json_free(result); return ck2; }
        bool live2 = sk2_truthy(ck2);
        json_free(ck2);
        if (!live2) {
            json_t *ea = sk2_args0();
            sk2_astr(ea, "emit", emit_provided ? "emit" : NULL);
            sk2_aset(ea, "task_id", task_id);
            json_t *es = sk2_call(ctx, "_emit_recovery_skipped", ea);
            json_free(result);
            if (sk2_is_raise(es)) return es;
            json_free(es);
            return json_number((double)attempts);
        }
        /* turn_fn(build_recovery_nudge(result, attempt=, max_attempts=max_recovery_attempts(), task_id=)) */
        json_t *ma2 = sk2_call(ctx, "max_recovery_attempts", sk2_args0());
        if (sk2_is_raise(ma2)) { json_free(result); return ma2; }
        json_t *ba = sk2_args0();
        sk2_aset(ba, "result", result);
        sk2_anum(ba, "attempt", (double)attempts);
        sk2_aset(ba, "max_attempts", ma2);
        sk2_aset(ba, "task_id", task_id);
        json_free(ma2);
        json_t *bn = sk2_call(ctx, "build_recovery_nudge", ba);
        if (sk2_is_raise(bn)) { json_free(result); return bn; }
        json_t *tf_a = sk2_args0();
        sk2_aset(tf_a, "nudge", bn);
        json_free(bn);
        json_t *tf = sk2_call(ctx, "turn_fn", tf_a);
        json_free(result);
        if (sk2_is_raise(tf)) return tf;
        json_free(tf);
        /* loop */
    }
}

/* ════════════════ hermes_cli/turn_exit.py ════════════════ */

/* PoP: sk2_turn_exit_code @ hermes_cli/turn_exit.py:turn_exit_code */
static json_t *sk2_turn_exit_code(sk2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    if (!json_has(args, "kanban_worker"))
        return sk2_err_spec("TypeError",
            "turn_exit_code() missing 1 required keyword-only argument: 'kanban_worker'");
    const json_t *result = json_obj_get(args, "result");
    bool kanban_worker = sk2_truthy(json_obj_get(args, "kanban_worker"));
    bool crl = sk2_truthy(json_obj_get(args, "credentials_rate_limited"));
    bool ct = sk2_truthy(json_obj_get(args, "credentials_terminal"));

    if (!result || result->type != JSON_OBJECT) {
        if (crl && kanban_worker) return json_number(75);   /* KANBAN_RATE_LIMIT_EXIT_CODE */
        if (ct && kanban_worker) return json_number(78);     /* KANBAN_TERMINAL_PROVIDER_EXIT_CODE */
        return json_number(1);
    }
    if (sk2_truthy(json_obj_get(result, "interrupted"))) return json_number(130);
    bool failed = sk2_truthy(json_obj_get(result, "failed"));
    bool partial = sk2_truthy(json_obj_get(result, "partial"));
    bool comp_false = sk2_is_false(json_obj_get(result, "completed"));
    if (!(failed || partial || comp_false)) return json_number(0);
    if (kanban_worker) {
        const json_t *reason = json_obj_get(result, "failure_reason");
        const json_t *transient = json_obj_get(args, "transient_reasons");
        const json_t *terminal = json_obj_get(args, "terminal_reasons");
        bool in_trans, in_term;
        if (transient && transient->type == JSON_ARRAY)
            in_trans = sk2_in_array(reason, transient);
        else {
            const char *rs = sk2_str(reason);
            in_trans = sk2_str_in_set(rs, SK2_TRANSIENT, sizeof(SK2_TRANSIENT) / sizeof(SK2_TRANSIENT[0]));
        }
        if (in_trans) return json_number(75);
        if (terminal && terminal->type == JSON_ARRAY)
            in_term = sk2_in_array(reason, terminal);
        else {
            const char *rs = sk2_str(reason);
            in_term = sk2_str_in_set(rs, SK2_TERMINAL, sizeof(SK2_TERMINAL) / sizeof(SK2_TERMINAL[0]));
        }
        if (in_term) return json_number(78);
    }
    return json_number(1);
}

/* PoP: sk2_credential_failure_flags @ hermes_cli/turn_exit.py:credential_failure_flags */
static json_t *sk2_credential_failure_flags(sk2_ctx_t *ctx, const json_t *args) {
    const json_t *nodes = json_obj_get(args, "exc");
    if (!nodes || nodes->type == JSON_NULL) return json_object();   /* exc is None -> {} */
    if (nodes->type != JSON_ARRAY) return json_object();
    size_t n = json_len(nodes);
    if (n == 0) return json_object();
    bool *seen = calloc(n, sizeof(bool));
    long long cur = 0;
    json_t *ret = NULL;
    while (cur >= 0 && (size_t)cur < n && !seen[cur]) {
        seen[cur] = true;
        const json_t *node = json_get(nodes, (size_t)cur);
        const char *kind = json_get_str(node, "kind", "");
        if (sk2_exc_isa(kind, "AuthError")) {
            /* is_rate_limited_auth_error(exc) — exc projects to its node */
            json_t *ia = sk2_args0();
            sk2_aset(ia, "error", node);
            json_t *rate = sk2_call(ctx, "is_rate_limited_auth_error", ia);
            if (sk2_is_raise(rate)) { ret = rate; break; }
            bool terminal = sk2_truthy(json_obj_get(node, "relogin_required"));
            ret = json_object();
            json_set(ret, "credentials_rate_limited", rate);   /* adopts rate */
            json_set(ret, "credentials_terminal", json_bool(terminal));
            break;
        }
        /* exc = exc.__cause__ or exc.__context__ (cause if not None else context) */
        const json_t *cause = json_obj_get(node, "cause");
        const json_t *context = json_obj_get(node, "context");
        const json_t *next = (cause && cause->type != JSON_NULL) ? cause : context;
        cur = (next && next->type == JSON_NUMBER) ? (long long)next->num_val : -1;
    }
    free(seen);
    return ret ? ret : json_object();
}