/*
 * kanban_cli_flow.c — FLOW cores + leaves of hermes_cli/kanban.py (LIVE
 * authority) that NO existing PoP claims by PYTHON name: the flag-parsing
 * leaves (_none_profile, _parse_metadata_flag), the text/args leaves
 * (_joined_words, _stripped_or_none, _ok_or_err, _bulk_ids, _require_ids),
 * the human-render leaves (_print_diagnostics, _print_section), the row
 * grouper (_rows_by_task), the goal-judge gate (_goal_gate_error), the
 * comment-wrapping closure (_commented + its run()), the notify identity
 * leaf (_caller_identity_for_target), the specify/decompose driver
 * (_run_triage_sweep), the outcome-line leaves (_retitled_suffix,
 * _decompose_ok_line) and the two verb handlers whose sibling PoP lines
 * credit the UNDERSCORE-LESS names cmd_log / cmd_request_changes (which do
 * not exist in kanban.py): _cmd_log and _cmd_request_changes.
 *
 * Self-contained C11, libjson-only. Every claimed sibling (_profile_author,
 * _worker_run_id_for, _goal_mode_handoff_rejection) and every foreign
 * collaborator (hermes_cli.kanban_output._err/_obj_dict, kanban_db kb.*,
 * kanban_db_connect kbc.connect_closing, gateway.session_context.
 * get_session_env, json.loads/json.dumps, builtin print, sys.stdout.write,
 * the mod.list_triage_ids / run_one / human_ok / op callables) enters through
 * ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; kc_call frees args); the result is a
 * NEW json_t* the caller frees (NULL == None; a scripted JSON null is
 * delivered as a NULL pointer; {"__raise__": kind, "msg": ...} == a raised
 * exception). The harness records {"ev": name, "args": args, "ret": result}
 * per call in order; the oracle's recording stubs emit the identical shape
 * driving LIVE Python with the same scripts. The ports perform NO real I/O.
 *
 * Ported-to-ported calls compose REAL (_decompose_ok_line ->
 * _retitled_suffix); the oracle keeps both extracted defs real on that path,
 * so no seam event fires for the internal call on either side.
 *
 * Exception propagation: every kc flow returns json_t* — a normal result
 * (NULL == None) or a raise spec. The ONLY except site in this cluster is
 * _parse_metadata_flag's `except (ValueError, json.JSONDecodeError)`, which
 * swallows the FULL ValueError family (UnicodeError + the three Unicode*
 * errors + JSONDecodeError) and nothing else; the BaseException trio
 * (SystemExit / KeyboardInterrupt / GeneratorExit) ALWAYS passes through.
 * The kc_exc_parent table below is the FULL py3.14 builtin hierarchy (audited
 * against the quarry interpreter) plus JSONDecodeError->ValueError and the
 * IOError/EnvironmentError/WindowsError aliases->OSError; ExceptionGroup is
 * routed through Exception to keep the single-parent walk's issubclass
 * contract. Fail-closed: a missing/empty seam script yields a RuntimeError
 * raise spec (recorded, then propagated/swallowed exactly as live); no seam
 * installed at all yields the same spec.
 *
 * Projections (documented, applied identically by the oracle):
 *  - tuple returns -> JSON arrays (_parse_metadata_flag (dict|None, rc),
 *    _require_ids (ids, rc), _goal_mode_handoff_rejection's (verdict,
 *    rejection) script, _caller_identity_for_target (uid, alt),
 *    kb.request_changes's (ok, detail) script).
 *  - argparse.Namespace / outcome / diag doubles are JSON objects; attribute
 *    access models SimpleNamespace: a MISSING key raises AttributeError
 *    "'types.SimpleNamespace' object has no attribute '<name>'" (py3.14
 *    module-qualified), a non-object receiver raises with its own type name.
 *    conn is an opaque fixture marker string ("CONN"); _rows_by_task's conn
 *    double fires "conn.execute" with {sql, params} only.
 *  - kbc.connect_closing() is ONE seam call returning the conn marker; the
 *    context manager is silent (__enter__ returns the marker, __exit__
 *    returns False), so no enter/exit events fire on either side.
 *  - _commented returns a closure descriptor {"__closure__": "_commented",
 *    conn, reason, author, prefix}; the harness drives it through
 *    kc_commented_run(io, closure, tid) which fires kb.add_comment (when
 *    reason is truthy) then the "op" seam — exactly the live run(tid).
 *  - str()/repr() of scalars follow Python (None/True/False, integral
 *    floats render N.0 in repr, shortest-round-trip float str); the fixture
 *    domain keeps floats NON-integral and inside [1e-4, 1e16) in str()
 *    positions, strings ASCII, and data-dict values scalar-or-list-of-
 *    scalars. Unicode*Error scripted raises str() to "" (their __str__
 *    reads ctor attributes the bypass does not set) — modeled identically.
 *  - integral floats normalize to ints on both sides before comparison.
 *
 * Seam name table (name -> live call):
 *   print                     builtin print(*values, file=)  args{args:[...],file?}
 *   sys.stdout.write          sys.stdout.write(text)         args{text}
 *   _err                      kanban_output._err(msg, rc=1)  args{msg,rc}
 *   _obj_dict                 kanban_output._obj_dict(obj, fields) args{obj,fields}
 *   json.loads / json.dumps   json module functions          args{raw} / {obj}
 *   _profile_author           hermes_cli.kanban._profile_author [claimed] args{}
 *   _worker_run_id_for        hermes_cli.kanban._worker_run_id_for [claimed] args{task_id}
 *   _goal_mode_handoff_rejection  hermes_cli.kanban._goal_mode_handoff_rejection
 *                             [claimed]                      args{task?,evidence}
 *   kb.get_task               kanban_db.get_task(conn, tid)  args{conn,tid}
 *   kb.read_worker_log        kanban_db.read_worker_log(task_id, tail_bytes=)
 *                                                            args{task_id,tail_bytes?}
 *   kb.request_changes        kanban_db.request_changes(conn, tid, reason=,
 *                             expected_run_id=)              args{conn,tid,reason,expected_run_id?}
 *   kb.add_comment            kanban_db.add_comment(conn, tid, author, body)
 *                                                            args{conn,tid,author?,body}
 *   kbc.connect_closing       kanban_db_connect.connect_closing()  args{}
 *   conn.execute              _rows_by_task's conn.execute(sql, tuple(ids))
 *                                                            args{sql,params}
 *   get_session_env           gateway.session_context.get_session_env(key, default)
 *                                                            args{key,default}
 *   mod.list_triage_ids       _run_triage_sweep's mod.list_triage_ids(tenant=)
 *                                                            args{tenant?}
 *   run_one                   _run_triage_sweep's run_one(tid, author=)
 *                                                            args{tid,author}
 *   human_ok                  _run_triage_sweep's human_ok(outcome) args{outcome}
 *   op                        _commented's op(tid)           args{tid}
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include "hermes_json.h"

/* ── universal seam plumbing ── */

typedef json_t *(*kc_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct kc_io {
    kc_seam_fn call;
    void *ctx;
} kc_io_t;

static int kc_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *kc_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}

static const char *kc_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}

/* ── FULL py3.14 builtin exception hierarchy (name -> first direct base).
 * Audited against the quarry CPython 3.14.9: every builtins name that is-a
 * BaseException appears here. The trio stays DIRECTLY under BaseException so
 * it passes through every except-Exception/except-ValueError site. Aliases
 * (IOError/EnvironmentError/WindowsError are OSError) map to OSError so
 * family walks work; ExceptionGroup's first base is BaseExceptionGroup but
 * the single-parent walk routes it through Exception (issubclass contract).
 * JSONDecodeError is json's ValueError subclass (not a builtin). */
typedef struct { const char *name; const char *parent; } kc_exc_ent_t;
static const kc_exc_ent_t KC_EXC_TABLE[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"Exception", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"IOError", "OSError"},
    {"EnvironmentError", "OSError"},
    {"WindowsError", "OSError"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"JSONDecodeError", "ValueError"},
    {"Warning", "Exception"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"ResourceWarning", "Warning"},
};

static const char *kc_exc_parent(const char *name) {
    for (size_t i = 0; i < sizeof(KC_EXC_TABLE) / sizeof(KC_EXC_TABLE[0]); i++)
        if (strcmp(KC_EXC_TABLE[i].name, name) == 0) return KC_EXC_TABLE[i].parent;
    return NULL;
}

/* issubclass(kind, family) via the single-parent walk. Unknown kinds (a
 * synthetic fixture class) walk to NULL and match only themselves. */
static int kc_exc_is(const char *kind, const char *family) {
    const char *cur = kind;
    while (cur) {
        if (strcmp(cur, family) == 0) return 1;
        cur = kc_exc_parent(cur);
    }
    return 0;
}

static json_t *kc_raise(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *kc_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return kc_raise(kind, buf);
}

static char *kc_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return strdup("");
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* Universal seam call: args owned by the caller (freed here); result owned
 * by the caller. Fail-closed when no seam is installed. A JSON null result
 * is delivered as a NULL pointer (Python None). */
static json_t *kc_call(kc_io_t *io, const char *name, json_t *args) {
    json_t *r;
    if (io && io->call) {
        r = io->call(io->ctx, name, args);
    } else {
        r = kc_raise_fmt("RuntimeError", "no script for %s", name);
    }
    json_free(args);
    if (r && r->type == JSON_NULL) { json_free(r); r = NULL; }
    return r;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static int kc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static const char *kc_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static const char *kc_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* Shortest-round-trip float str (Python float_repr_style for the documented
 * fixture domain: non-integral, 1e-4 <= |d| < 1e16). */
static char *kc_float_str(double d) {
    char buf[64];
    for (int p = 1; p <= 17; p++) {
        snprintf(buf, sizeof buf, "%.*g", p, d);
        if (strtod(buf, NULL) == d) break;
    }
    return strdup(buf);
}

/* str() of a scalar JSON value (containers are out of the str() fixture
 * domain; integral floats are kept out of str() positions). */
static char *kc_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) {
            snprintf(buf, sizeof buf, "%.0f", d);
            return strdup(buf);
        }
        return kc_float_str(d);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* repr() of a scalar JSON value. Strings use Python's quote choice: double
 * quotes when the text contains ' but no ", else single quotes with ' and \
 * escaped; control chars render \n / \r / \t / \xNN. */
static char *kc_py_repr(const json_t *v) {
    if (!v || v->type != JSON_STRING) {
        if (v && v->type == JSON_NUMBER) {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) {
                char *s = kc_fmt("%.0f", d);
                char *out = kc_fmt("%s.0", s);
                free(s);
                return out;
            }
            return kc_float_str(d);
        }
        return kc_py_str(v);
    }
    const char *s = v->str_val ? v->str_val : "";
    int has_sq = strchr(s, '\'') != NULL;
    int has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t on = 0;
    out[on++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { out[on++] = '\\'; out[on++] = (char)c; }
        else if (c == '\n') { out[on++] = '\\'; out[on++] = 'n'; }
        else if (c == '\r') { out[on++] = '\\'; out[on++] = 'r'; }
        else if (c == '\t') { out[on++] = '\\'; out[on++] = 't'; }
        else if (c < 0x20) { on += (size_t)snprintf(out + on, cap - on, "\\x%02x", c); }
        else out[on++] = (char)c;
    }
    out[on++] = q;
    out[on] = '\0';
    return out;
}

/* ── string helpers (ASCII fixture domain) ── */

static char *kc_strip(const char *s) {
    if (!s) return strdup("");
    const char *ws = " \t\n\r\v\f";
    while (*s && strchr(ws, *s)) s++;
    int n = (int)strlen(s);
    while (n > 0 && strchr(ws, s[n - 1])) n--;
    char *out = (char *)malloc((size_t)n + 1);
    if (!out) return NULL;
    memcpy(out, s, (size_t)n);
    out[n] = '\0';
    return out;
}

static char *kc_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *q = out; *q; q++)
        if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    return out;
}

static int kc_endswith(const char *s, const char *suf) {
    if (!s || !suf) return 0;
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

/* " ".join(words) / ", ".join(items): arrays require str elements (TypeError
 * "sequence item N: expected str instance, T found"); a str iterates chars;
 * a dict iterates keys; any other carrier raises TypeError "can only join
 * an iterable" (the live str.join message, NOT the list() message). */
static char *kc_join(const json_t *seq, const char *sep, json_t **err) {
    *err = NULL;
    if (!seq || (seq->type != JSON_ARRAY && seq->type != JSON_STRING &&
                 seq->type != JSON_OBJECT)) {
        *err = kc_raise("TypeError", "can only join an iterable");
        return NULL;
    }
    size_t n = 0;
    if (seq->type == JSON_ARRAY) n = seq->c.count;
    else if (seq->type == JSON_STRING) n = strlen(kc_jstr(seq));
    else n = seq->c.count;
    size_t sl = strlen(sep);
    size_t cap = 64, on = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        char *piece = NULL;
        int owned = 0;
        if (seq->type == JSON_ARRAY) {
            const json_t *el = seq->c.items[i];
            if (!el || el->type != JSON_STRING) {
                free(out);
                *err = kc_raise_fmt("TypeError",
                    "sequence item %zu: expected str instance, %s found",
                    i, kc_type_name(el));
                return NULL;
            }
            piece = el->str_val ? el->str_val : "";
        } else if (seq->type == JSON_STRING) {
            piece = (char *)malloc(2);
            if (!piece) { free(out); return NULL; }
            piece[0] = kc_jstr(seq)[i]; piece[1] = '\0';
            owned = 1;
        } else {
            piece = seq->c.keys[i];
        }
        size_t pl = strlen(piece);
        if (on + pl + sl + 1 > cap) {
            cap = (on + pl + sl + 1) * 2;
            char *nb = (char *)realloc(out, cap);
            if (!nb) { if (owned) free(piece); free(out); return NULL; }
            out = nb;
        }
        if (i > 0) { memcpy(out + on, sep, sl); on += sl; }
        memcpy(out + on, piece, pl); on += pl;
        out[on] = '\0';
        if (owned) free(piece);
    }
    return out;
}

/* list(x): array -> element copies; str -> char strings; dict -> keys;
 * anything else raises TypeError "'T' object is not iterable". */
static json_t *kc_iter_list(const json_t *v, json_t **err) {
    *err = NULL;
    json_t *out = json_array();
    if (!v || v->type == JSON_NULL || v->type == JSON_NUMBER || v->type == JSON_BOOL) {
        json_free(out);
        *err = kc_raise_fmt("TypeError", "'%s' object is not iterable", kc_type_name(v));
        return NULL;
    }
    if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < v->c.count; i++)
            json_append(out, json_copy(v->c.items[i]));
    } else if (v->type == JSON_STRING) {
        const char *s = kc_jstr(v);
        for (size_t i = 0; i < strlen(s); i++) {
            char ch[2] = {s[i], '\0'};
            json_append(out, json_string(ch));
        }
    } else { /* dict -> keys */
        for (size_t i = 0; i < v->c.count; i++)
            json_append(out, json_string(v->c.keys[i]));
    }
    return out;
}

/* Namespace attribute access (SimpleNamespace double): non-object receiver
 * -> AttributeError with the receiver's type name; missing key ->
 * AttributeError with the MODULE-QUALIFIED double name (py3.14). Returns a
 * NEW node (NULL for a present null) or a raise spec. */
static json_t *kc_ns_attr(const json_t *obj, const char *name) {
    if (!obj || obj->type != JSON_OBJECT)
        return kc_raise_fmt("AttributeError", "'%s' object has no attribute '%s'",
                            kc_type_name(obj), name);
    const json_t *v = json_obj_get(obj, name);
    if (!v)
        return kc_raise_fmt("AttributeError",
                            "'types.SimpleNamespace' object has no attribute '%s'", name);
    if (v->type == JSON_NULL) return NULL;
    return json_copy(v);
}

/* getattr(obj, name, None): missing key is None, not an error. */
static json_t *kc_getattr(const json_t *obj, const char *name) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *v = json_obj_get(obj, name);
    if (!v || v->type == JSON_NULL) return NULL;
    return json_copy(v);
}

/* Borrowed getattr (truthiness probes only; no allocation). */
static const json_t *kc_getattr_b(const json_t *obj, const char *name) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *v = json_obj_get(obj, name);
    return (v && v->type != JSON_NULL) ? v : NULL;
}

/* row["task_id"] subscript: non-object -> TypeError "'T' object is not
 * subscriptable"; missing key -> KeyError whose str() is the REPR of the
 * key (msg carries the quotes). */
static json_t *kc_dict_get(const json_t *obj, const char *key, json_t **err) {
    *err = NULL;
    if (!obj || obj->type != JSON_OBJECT) {
        *err = kc_raise_fmt("TypeError", "'%s' object is not subscriptable",
                            kc_type_name(obj));
        return NULL;
    }
    const json_t *v = json_obj_get(obj, key);
    if (!v) {
        *err = kc_raise_fmt("KeyError", "'%s'", key);
        return NULL;
    }
    if (v->type == JSON_NULL) return NULL;
    return json_copy(v);
}

/* a, b = x unpack: 2-element array/str/dict-keys unpack; wrong count ->
 * ValueError with the py3.14 got-count; non-iterable -> TypeError. */
static json_t *kc_unpack2(const json_t *r, json_t **a, json_t **b) {
    *a = NULL; *b = NULL;
    size_t n = 0;
    int iterable = 0;
    if (r && r->type == JSON_ARRAY) { n = r->c.count; iterable = 1; }
    else if (r && r->type == JSON_STRING) { n = strlen(kc_jstr(r)); iterable = 1; }
    else if (r && r->type == JSON_OBJECT) { n = r->c.count; iterable = 1; }
    if (!iterable)
        return kc_raise_fmt("TypeError", "cannot unpack non-iterable %s object",
                            kc_type_name(r));
    if (n != 2)
        return kc_raise_fmt("ValueError", "%s values to unpack (expected 2, got %zu)",
                            n > 2 ? "too many" : "not enough", n);
    if (r->type == JSON_ARRAY) {
        const json_t *x = r->c.items[0]; const json_t *y = r->c.items[1];
        *a = (x && x->type != JSON_NULL) ? json_copy(x) : NULL;
        *b = (y && y->type != JSON_NULL) ? json_copy(y) : NULL;
    } else if (r->type == JSON_STRING) {
        char ca[2] = {kc_jstr(r)[0], '\0'};
        char cb[2] = {kc_jstr(r)[1], '\0'};
        *a = json_string(ca); *b = json_string(cb);
    } else {
        *a = json_string(r->c.keys[0]); *b = json_string(r->c.keys[1]);
    }
    return NULL;
}

/* ── output seams ── */

/* builtin print(*values, file=): args{args:[values], file:"stderr"?}. */
static json_t *kc_print_node(kc_io_t *io, const json_t *value, int stderr_) {
    json_t *args = json_object();
    json_t *vals = json_array();
    json_append(vals, value ? json_copy(value) : json_null());
    json_set(args, "args", vals);
    if (stderr_) json_set(args, "file", json_string("stderr"));
    return kc_call(io, "print", args);
}

static json_t *kc_print_str(kc_io_t *io, const char *text, int stderr_) {
    json_t *v = json_string(text ? text : "");
    json_t *r = kc_print_node(io, v, stderr_);
    json_free(v);
    return r;
}

/* print() with no values. */
static json_t *kc_print_empty(kc_io_t *io) {
    json_t *args = json_object();
    json_set(args, "args", json_array());
    return kc_call(io, "print", args);
}

/* kanban_output._err(msg, rc=1) — the rc kwarg is always materialized. */
static json_t *kc_err(kc_io_t *io, const char *msg, double rc) {
    json_t *args = json_object();
    json_set(args, "msg", json_string(msg ? msg : ""));
    json_set(args, "rc", json_number(rc));
    return kc_call(io, "_err", args);
}

/* ═══════════════════════ ported targets ═══════════════════════ */

/* PoP: kc_none_profile @ hermes_cli/kanban.py:_none_profile */
static json_t *kc_none_profile(const json_t *value) {
    if (!value || value->type != JSON_STRING)
        return kc_raise_fmt("AttributeError", "'%s' object has no attribute 'lower'",
                            kc_type_name(value));
    char *low = kc_lower(value->str_val);
    int unassign = low && (strcmp(low, "none") == 0 || strcmp(low, "-") == 0 ||
                           strcmp(low, "null") == 0);
    free(low);
    if (unassign) return NULL;
    return json_copy(value);
}

/* PoP: kc_parse_metadata_flag @ hermes_cli/kanban.py:_parse_metadata_flag */
static json_t *kc_parse_metadata_flag(kc_io_t *io, const json_t *raw) {
    json_t *out = json_array();
    if (!kc_truthy(raw)) {
        json_append(out, json_null());
        json_append(out, json_number(0));
        return out;
    }
    json_t *args = json_object();
    json_set(args, "raw", json_copy(raw));
    json_t *meta = kc_call(io, "json.loads", args);
    json_t *caught = NULL;
    if (kc_is_raise(meta)) {
        /* except (ValueError, json.JSONDecodeError): the full ValueError
         * family; the BaseException trio and everything else passes through. */
        if (kc_exc_is(kc_raise_kind(meta), "ValueError")) {
            caught = meta;
            meta = NULL;
        } else {
            json_free(out);
            return meta;
        }
    } else if (!meta || meta->type != JSON_OBJECT) {
        /* raise ValueError("must be a JSON object") — caught by the same
         * except clause (internal raise, no seam event). */
        json_free(meta);
        meta = NULL;
        caught = kc_raise("ValueError", "must be a JSON object");
    }
    if (caught) {
        /* f"kanban: --metadata: {exc}" — str(exc). The Unicode*Error trio
         * str() to "" (their __str__ reads unset ctor attributes). */
        const char *k = kc_raise_kind(caught);
        const char *m = kc_raise_msg(caught);
        if (strcmp(k, "UnicodeDecodeError") == 0 ||
            strcmp(k, "UnicodeEncodeError") == 0 ||
            strcmp(k, "UnicodeTranslateError") == 0) m = "";
        char *msg = kc_fmt("kanban: --metadata: %s", m);
        json_free(caught);
        json_t *rc = kc_err(io, msg, 2);
        free(msg);
        if (kc_is_raise(rc)) { json_free(out); return rc; }
        json_append(out, json_null());
        json_append(out, rc ? rc : json_null());
        return out;
    }
    json_append(out, meta);
    json_append(out, json_number(0));
    return out;
}

/* PoP: kc_joined_words @ hermes_cli/kanban.py:_joined_words */
static json_t *kc_joined_words(const json_t *words) {
    if (!kc_truthy(words)) return NULL;
    json_t *err = NULL;
    char *joined = kc_join(words, " ", &err);
    if (err) return err;
    char *stripped = kc_strip(joined);
    free(joined);
    json_t *r = json_string(stripped ? stripped : "");
    free(stripped);
    return r;
}

/* PoP: kc_stripped_or_none @ hermes_cli/kanban.py:_stripped_or_none */
static json_t *kc_stripped_or_none(const json_t *value) {
    if (!value || value->type == JSON_NULL) return NULL;
    if (value->type != JSON_STRING)
        return kc_raise_fmt("AttributeError", "'%s' object has no attribute 'strip'",
                            kc_type_name(value));
    char *s = kc_strip(value->str_val);
    json_t *r = (s && s[0]) ? json_string(s) : NULL;
    free(s);
    return r;
}

/* PoP: kc_ok_or_err @ hermes_cli/kanban.py:_ok_or_err */
static json_t *kc_ok_or_err(kc_io_t *io, const json_t *ok, const char *fail,
                            const char *done) {
    if (!kc_truthy(ok)) return kc_err(io, fail, 1);
    json_t *r = kc_print_str(io, done, 0);
    if (kc_is_raise(r)) return r;
    json_free(r);
    return json_number(0);
}

/* PoP: kc_bulk_ids @ hermes_cli/kanban.py:_bulk_ids */
static json_t *kc_bulk_ids(const json_t *args) {
    json_t *tid = kc_ns_attr(args, "task_id");
    if (kc_is_raise(tid)) return tid;
    json_t *ids = kc_getattr(args, "ids");
    json_t *extra = NULL;
    if (kc_truthy(ids)) {
        json_t *err = NULL;
        extra = kc_iter_list(ids, &err);
        json_free(ids);
        if (err) { json_free(tid); return err; }
    } else {
        json_free(ids);
        extra = json_array();
    }
    json_t *out = json_array();
    json_append(out, tid ? tid : json_null());
    for (size_t i = 0; i < extra->c.count; i++)
        json_append(out, json_copy(extra->c.items[i]));
    json_free(extra);
    return out;
}

/* PoP: kc_require_ids @ hermes_cli/kanban.py:_require_ids */
static json_t *kc_require_ids(kc_io_t *io, const json_t *args) {
    json_t *tids = kc_ns_attr(args, "task_ids");
    if (kc_is_raise(tids)) return tids;
    json_t *ids = NULL;
    if (kc_truthy(tids)) {
        json_t *err = NULL;
        ids = kc_iter_list(tids, &err);
        json_free(tids);
        if (err) return err;
    } else {
        json_free(tids);
        ids = json_array();
    }
    json_t *out = json_array();
    if (ids->c.count == 0) {
        json_t *rc = kc_err(io, "at least one task_id is required", 1);
        if (kc_is_raise(rc)) { json_free(ids); return rc; }
        json_append(out, ids);
        json_append(out, rc ? rc : json_null());
        return out;
    }
    json_append(out, ids);
    json_append(out, json_number(0));
    return out;
}

/* PoP: kc_print_diagnostics @ hermes_cli/kanban.py:_print_diagnostics */
static json_t *kc_print_diagnostics(kc_io_t *io, const json_t *diags,
                                    const char *indent, int with_kind) {
    if (!diags || diags->type != JSON_ARRAY)
        return kc_raise_fmt("TypeError", "'%s' object is not iterable",
                            kc_type_name(diags));
    const char *marker;
    for (size_t i = 0; i < diags->c.count; i++) {
        const json_t *d = diags->c.items[i];
        json_t *kind = NULL, *title = NULL;
        if (with_kind) {
            kind = kc_ns_attr(d, "kind");
            if (kc_is_raise(kind)) return kind;
        }
        title = kc_ns_attr(d, "title");
        if (kc_is_raise(title)) { json_free(kind); return title; }
        json_t *sev = kc_ns_attr(d, "severity");
        if (kc_is_raise(sev)) { json_free(kind); json_free(title); return sev; }
        const char *sv = kc_jstr(sev);
        if (strcmp(sv, "warning") == 0) marker = "\xe2\x9a\xa0";       /* ⚠ */
        else if (strcmp(sv, "error") == 0) marker = "!!";
        else if (strcmp(sv, "critical") == 0) marker = "!!!";
        else marker = "?";
        char *head;
        if (with_kind) head = kc_fmt("%s: %s", kc_jstr(kind), kc_jstr(title));
        else head = strdup(kc_jstr(title));
        char *sev_s = kc_py_str(sev);
        char *line = kc_fmt("%s%s [%s] %s", indent ? indent : "", marker, sev_s, head);
        free(head); free(sev_s);
        json_t *pr = kc_print_str(io, line, 0);
        free(line);
        json_free(kind); json_free(title); json_free(sev);
        if (kc_is_raise(pr)) return pr;
        json_free(pr);
        json_t *data = kc_ns_attr(d, "data");
        if (kc_is_raise(data)) return data;
        if (kc_truthy(data)) {
            if (data->type != JSON_OBJECT) {
                json_free(data);
                return kc_raise_fmt("AttributeError",
                                    "'%s' object has no attribute 'items'",
                                    kc_type_name(data));
            }
            /* bits = [f"{k}={','.join(str(x) for x in v)}" if list else
             *         f"{k}={v}" for k, v in d.data.items()] */
            char *bits = NULL;
            size_t bon = 0, bcap = 64;
            bits = (char *)malloc(bcap);
            if (!bits) { json_free(data); return NULL; }
            bits[0] = '\0';
            json_t *bit_err = NULL;
            for (size_t j = 0; j < data->c.count && !bit_err; j++) {
                const char *k = data->c.keys[j];
                const json_t *v = data->c.items[j];
                char *val = NULL;
                if (v && v->type == JSON_ARRAY) {
                    size_t von = 0, vcap = 64;
                    val = (char *)malloc(vcap);
                    if (!val) { bit_err = kc_raise("MemoryError", "oom"); break; }
                    val[0] = '\0';
                    for (size_t x = 0; x < v->c.count; x++) {
                        char *xs = kc_py_str(v->c.items[x]);
                        size_t xl = strlen(xs);
                        if (von + xl + 2 > vcap) {
                            vcap = (von + xl + 2) * 2;
                            char *nb = (char *)realloc(val, vcap);
                            if (!nb) { free(xs); bit_err = kc_raise("MemoryError", "oom"); break; }
                            val = nb;
                        }
                        if (x > 0) { val[von++] = ','; }
                        memcpy(val + von, xs, xl); von += xl;
                        val[von] = '\0';
                        free(xs);
                    }
                } else {
                    val = kc_py_str(v);
                }
                if (bit_err) { free(val); break; }
                size_t kl = strlen(k), vl = strlen(val);
                if (bon + kl + vl + 4 > bcap) {
                    bcap = (bon + kl + vl + 4) * 2;
                    char *nb = (char *)realloc(bits, bcap);
                    if (!nb) { free(val); bit_err = kc_raise("MemoryError", "oom"); break; }
                    bits = nb;
                }
                if (j > 0) { memcpy(bits + bon, " | ", 3); bon += 3; }
                memcpy(bits + bon, k, kl); bon += kl;
                bits[bon++] = '=';
                memcpy(bits + bon, val, vl); bon += vl;
                bits[bon] = '\0';
                free(val);
            }
            json_free(data);
            if (bit_err) { free(bits); return bit_err; }
            if (bon > 0) {
                char *dl = kc_fmt("%s   data: %s", indent ? indent : "", bits);
                json_t *dp = kc_print_str(io, dl, 0);
                free(dl);
                if (kc_is_raise(dp)) { free(bits); return dp; }
                json_free(dp);
            }
            free(bits);
        } else {
            json_free(data);
        }
        json_t *actions = kc_ns_attr(d, "actions");
        if (kc_is_raise(actions)) return actions;
        if (!actions || actions->type != JSON_ARRAY) {
            json_t *e = kc_raise_fmt("TypeError", "'%s' object is not iterable",
                                     kc_type_name(actions));
            json_free(actions);
            return e;
        }
        for (size_t j = 0; j < actions->c.count; j++) {
            const json_t *a = actions->c.items[j];
            json_t *sugg = kc_ns_attr(a, "suggested");
            if (kc_is_raise(sugg)) { json_free(actions); return sugg; }
            if (kc_truthy(sugg)) {
                json_t *label = kc_ns_attr(a, "label");
                if (kc_is_raise(label)) {
                    json_free(sugg); json_free(actions); return label;
                }
                char *al = kc_fmt("%s   \xe2\x86\x92 %s", indent ? indent : "",
                                  kc_jstr(label));
                json_t *ap = kc_print_str(io, al, 0);
                free(al); json_free(label);
                if (kc_is_raise(ap)) { json_free(sugg); json_free(actions); return ap; }
                json_free(ap);
            }
            json_free(sugg);
        }
        json_free(actions);
    }
    return NULL;
}

/* PoP: kc_print_section @ hermes_cli/kanban.py:_print_section */
static json_t *kc_print_section(kc_io_t *io, const char *title, const json_t *lines) {
    json_t *r = kc_print_empty(io);
    if (kc_is_raise(r)) return r;
    json_free(r);
    r = kc_print_str(io, title, 0);
    if (kc_is_raise(r)) return r;
    json_free(r);
    if (!lines || lines->type != JSON_ARRAY)
        return kc_raise_fmt("TypeError", "'%s' object is not iterable",
                            kc_type_name(lines));
    for (size_t i = 0; i < lines->c.count; i++) {
        r = kc_print_node(io, lines->c.items[i], 0);
        if (kc_is_raise(r)) return r;
        json_free(r);
    }
    return NULL;
}

/* PoP: kc_rows_by_task @ hermes_cli/kanban.py:_rows_by_task */
static json_t *kc_rows_by_task(kc_io_t *io, const json_t *conn, const char *table,
                               const json_t *ids) {
    (void)conn;  /* the conn double's .execute fires the seam below */
    if (!ids || ids->type != JSON_ARRAY)
        return kc_raise_fmt("TypeError", "'%s' object is not iterable",
                            kc_type_name(ids));
    json_t *by = json_object();
    for (size_t i = 0; i < ids->c.count; i++) {
        char *key = kc_py_str(ids->c.items[i]);
        json_set(by, key, json_array());  /* dup ids keep first position */
        free(key);
    }
    /* placeholders = ",".join(["?"] * len(ids)) */
    size_t n = ids->c.count;
    size_t pcap = n * 2 + 1;
    char *ph = (char *)malloc(pcap);
    if (!ph) { json_free(by); return NULL; }
    ph[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (i > 0) strcat(ph, ",");
        strcat(ph, "?");
    }
    char *sql = kc_fmt("SELECT * FROM %s WHERE task_id IN (%s) ORDER BY id",
                       table ? table : "", ph);
    free(ph);
    json_t *args = json_object();
    json_set(args, "sql", json_string(sql));
    free(sql);
    json_set(args, "params", json_copy(ids));
    json_t *rows = kc_call(io, "conn.execute", args);
    if (kc_is_raise(rows)) { json_free(by); return rows; }
    if (!rows || rows->type != JSON_ARRAY) {
        json_t *e = kc_raise_fmt("TypeError", "'%s' object is not iterable",
                                 kc_type_name(rows));
        json_free(rows); json_free(by);
        return e;
    }
    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        json_t *kerr = NULL;
        json_t *tid = kc_dict_get(row, "task_id", &kerr);
        if (kerr) { json_free(rows); json_free(by); return kerr; }
        char *key = kc_py_str(tid);
        json_free(tid);
        json_t *lst = json_obj_get(by, key);
        if (!lst) {
            lst = json_array();
            json_set(by, key, lst);
        }
        json_append(lst, json_copy(row));
        free(key);
    }
    json_free(rows);
    return by;
}

/* PoP: kc_goal_gate_error @ hermes_cli/kanban.py:_goal_gate_error */
static json_t *kc_goal_gate_error(kc_io_t *io, const json_t *conn, const char *tid,
                                  const char *evidence, const char *handoff,
                                  const char *blocked_hint, const char *continue_hint) {
    json_t *gargs = json_object();
    json_set(gargs, "conn", conn ? json_copy(conn) : json_null());
    json_set(gargs, "tid", json_string(tid ? tid : ""));
    json_t *task = kc_call(io, "kb.get_task", gargs);
    if (kc_is_raise(task)) return task;
    json_t *rargs = json_object();
    if (task) json_set(rargs, "task", task);  /* NULL (None) key dropped */
    json_set(rargs, "evidence", json_string(evidence ? evidence : ""));
    json_t *rej = kc_call(io, "_goal_mode_handoff_rejection", rargs);
    if (kc_is_raise(rej)) return rej;
    json_t *verdict = NULL, *rejection = NULL;
    json_t *uerr = kc_unpack2(rej, &verdict, &rejection);
    json_free(rej);
    if (uerr) return uerr;
    json_t *out = NULL;
    if (verdict && verdict->type == JSON_STRING &&
        strcmp(verdict->str_val, "blocked") == 0) {
        char *rs = kc_py_str(rejection);
        char *s = kc_fmt("kanban: goal %s of %s rejected: judge ruled "
                         "the goal unachievable \xe2\x80\x94 %s. %s",
                         handoff ? handoff : "", tid ? tid : "", rs,
                         blocked_hint ? blocked_hint : "");
        free(rs);
        out = json_string(s);
        free(s);
    } else if (rejection) {  /* rejection is not None */
        char *rs = kc_py_str(rejection);
        char *s = kc_fmt("kanban: goal %s of %s rejected by judge: %s. %s",
                         handoff ? handoff : "", tid ? tid : "", rs,
                         continue_hint ? continue_hint : "");
        free(rs);
        out = json_string(s);
        free(s);
    }
    json_free(verdict); json_free(rejection);
    return out;
}

/* PoP: kc_commented @ hermes_cli/kanban.py:_commented
 * Returns the run(tid) closure as a descriptor; kc_commented_run drives it. */
static json_t *kc_commented(const json_t *conn, const json_t *reason,
                            const json_t *author, const char *prefix) {
    json_t *c = json_object();
    json_set(c, "__closure__", json_string("_commented"));
    json_set(c, "conn", conn ? json_copy(conn) : json_null());
    json_set(c, "reason", reason ? json_copy(reason) : json_null());
    json_set(c, "author", author ? json_copy(author) : json_null());
    json_set(c, "prefix", json_string(prefix ? prefix : ""));
    return c;
}

static json_t *kc_commented_run(kc_io_t *io, const json_t *closure, const char *tid) {
    const json_t *reason = json_obj_get(closure, "reason");
    if (kc_truthy(reason)) {
        char *rs = kc_py_str(reason);
        char *body = kc_fmt("%s: %s", json_get_str(closure, "prefix", ""), rs);
        free(rs);
        json_t *args = json_object();
        const json_t *conn = json_obj_get(closure, "conn");
        json_set(args, "conn", conn ? json_copy(conn) : json_null());
        json_set(args, "tid", json_string(tid ? tid : ""));
        const json_t *author = json_obj_get(closure, "author");
        if (author && author->type != JSON_NULL)
            json_set(args, "author", json_copy(author));
        json_set(args, "body", json_string(body));
        free(body);
        json_t *r = kc_call(io, "kb.add_comment", args);
        if (kc_is_raise(r)) return r;
        json_free(r);
    }
    json_t *args = json_object();
    json_set(args, "tid", json_string(tid ? tid : ""));
    return kc_call(io, "op", args);
}

/* PoP: kc_caller_identity_for_target @ hermes_cli/kanban.py:_caller_identity_for_target */
static json_t *kc_caller_identity_for_target(kc_io_t *io, const json_t *platform,
                                             const json_t *chat_id,
                                             const json_t *thread_id,
                                             const json_t *user_id,
                                             const json_t *user_id_alt) {
    json_t *out = json_array();
    if (kc_truthy(user_id) || kc_truthy(user_id_alt)) {
        json_append(out, user_id ? json_copy(user_id) : json_null());
        json_append(out, user_id_alt ? json_copy(user_id_alt) : json_null());
        return out;
    }
    /* str(get_session_env(K, "")).lower() != str(platform or "").lower() —
     * the three comparisons short-circuit left to right. */
    const char *keys[3] = {"HERMES_SESSION_PLATFORM", "HERMES_SESSION_CHAT_ID",
                           "HERMES_SESSION_THREAD_ID"};
    const json_t *vals[3] = {platform, chat_id, thread_id};
    int lower[3] = {1, 0, 0};
    int mismatch = 0;
    for (int i = 0; i < 3 && !mismatch; i++) {
        json_t *args = json_object();
        json_set(args, "key", json_string(keys[i]));
        json_set(args, "default", json_string(""));
        json_t *env = kc_call(io, "get_session_env", args);
        if (kc_is_raise(env)) { json_free(out); return env; }
        char *es = kc_py_str(env);
        json_free(env);
        char *vs;
        if (kc_truthy(vals[i])) vs = kc_py_str(vals[i]);
        else vs = strdup("");
        if (lower[i]) {
            char *el = kc_lower(es); free(es); es = el;
            char *vl = kc_lower(vs); free(vs); vs = vl;
        }
        mismatch = strcmp(es, vs) != 0;
        free(es); free(vs);
    }
    if (mismatch) {
        json_append(out, user_id ? json_copy(user_id) : json_null());
        json_append(out, user_id_alt ? json_copy(user_id_alt) : json_null());
        return out;
    }
    const char *ukeys[2] = {"HERMES_SESSION_USER_ID", "HERMES_SESSION_USER_ID_ALT"};
    for (int i = 0; i < 2; i++) {
        json_t *args = json_object();
        json_set(args, "key", json_string(ukeys[i]));
        json_set(args, "default", json_string(""));
        json_t *env = kc_call(io, "get_session_env", args);
        if (kc_is_raise(env)) { json_free(out); return env; }
        if (kc_truthy(env)) {
            json_append(out, env);  /* ownership transferred */
        } else {
            json_free(env);
            json_append(out, json_null());
        }
    }
    return out;
}

/* PoP: kc_run_triage_sweep @ hermes_cli/kanban.py:_run_triage_sweep */
static json_t *kc_run_triage_sweep(kc_io_t *io, const json_t *args, const char *verb,
                                   const char *json_key, const json_t *json_fields) {
    int all_flag = kc_truthy(kc_getattr_b(args, "all_triage"));
    json_t *author = kc_getattr(args, "author");
    if (!kc_truthy(author)) {
        json_free(author);
        author = kc_call(io, "_profile_author", json_object());
        if (kc_is_raise(author)) return author;
    }
    int want_json = kc_truthy(kc_getattr_b(args, "json"));
    json_t *tenant = kc_getattr(args, "tenant");
    json_t *task_id = kc_ns_attr(args, "task_id");
    if (kc_is_raise(task_id)) {
        json_free(author); json_free(tenant);
        return task_id;
    }
    if (kc_truthy(task_id) && all_flag) {
        json_free(task_id); json_free(author); json_free(tenant);
        return kc_err(io, "kanban: pass either a task id OR --all, not both", 2);
    }
    json_t *ids = NULL;
    if (all_flag) {
        json_t *margs = json_object();
        if (tenant) json_set(margs, "tenant", json_copy(tenant));
        ids = kc_call(io, "mod.list_triage_ids", margs);
        if (kc_is_raise(ids)) {
            json_free(task_id); json_free(author); json_free(tenant);
            return ids;
        }
        if (!kc_truthy(ids)) {
            json_free(ids);
            json_t *r;
            if (want_json) {
                json_t *obj = json_object();
                json_set(obj, json_key ? json_key : "", json_number(0));
                json_set(obj, "total", json_number(0));
                json_t *dargs = json_object();
                json_set(dargs, "obj", obj);
                json_t *ds = kc_call(io, "json.dumps", dargs);
                if (kc_is_raise(ds)) {
                    json_free(task_id); json_free(author); json_free(tenant);
                    return ds;
                }
                r = kc_print_node(io, ds, 0);
                json_free(ds);
            } else {
                char *trep = kc_truthy(tenant) ? kc_py_repr(tenant) : NULL;
                char *line = kc_fmt("No triage tasks%s%s.",
                                    trep ? " for tenant " : "", trep ? trep : "");
                free(trep);
                r = kc_print_str(io, line, 0);
                free(line);
            }
            json_free(task_id); json_free(author); json_free(tenant);
            if (kc_is_raise(r)) return r;
            json_free(r);
            return json_number(0);
        }
    } else if (kc_truthy(task_id)) {
        ids = json_array();
        json_append(ids, json_copy(task_id));
    } else {
        json_free(task_id); json_free(author); json_free(tenant);
        char *msg = kc_fmt("kanban: %s requires a task id or --all", verb ? verb : "");
        json_t *r = kc_err(io, msg, 2);
        free(msg);
        return r;
    }
    json_free(task_id);
    if (!ids || ids->type != JSON_ARRAY) {
        json_t *e = kc_raise_fmt("TypeError", "'%s' object is not iterable",
                                 kc_type_name(ids));
        json_free(ids); json_free(author); json_free(tenant);
        return e;
    }
    long ok_count = 0;
    json_t *ret = NULL;
    for (size_t i = 0; i < ids->c.count && !ret; i++) {
        const json_t *tid = ids->c.items[i];
        json_t *rargs = json_object();
        json_set(rargs, "tid", tid ? json_copy(tid) : json_null());
        json_set(rargs, "author", author ? json_copy(author) : json_null());
        json_t *outcome = kc_call(io, "run_one", rargs);
        if (kc_is_raise(outcome)) { ret = outcome; break; }
        json_t *ok = kc_ns_attr(outcome, "ok");
        if (kc_is_raise(ok)) { json_free(outcome); ret = ok; break; }
        /* live re-reads outcome.ok in the elif; SimpleNamespace attribute
         * reads are pure, so the first read's truthiness is reused. */
        int ok_truthy = kc_truthy(ok);
        json_free(ok);
        if (ok_truthy) ok_count++;
        if (want_json) {
            json_t *oargs = json_object();
            json_set(oargs, "obj", outcome ? json_copy(outcome) : json_null());
            json_set(oargs, "fields", json_fields ? json_copy(json_fields) : json_null());
            json_t *od = kc_call(io, "_obj_dict", oargs);
            if (kc_is_raise(od)) { json_free(outcome); ret = od; break; }
            json_t *dargs = json_object();
            json_set(dargs, "obj", od ? od : json_null());
            json_t *ds = kc_call(io, "json.dumps", dargs);
            if (kc_is_raise(ds)) { json_free(outcome); ret = ds; break; }
            json_t *pr = kc_print_node(io, ds, 0);
            json_free(ds);
            if (kc_is_raise(pr)) { json_free(outcome); ret = pr; break; }
            json_free(pr);
        } else if (ok_truthy) {
            json_t *hargs = json_object();
            json_set(hargs, "outcome", outcome ? json_copy(outcome) : json_null());
            json_t *line = kc_call(io, "human_ok", hargs);
            if (kc_is_raise(line)) { json_free(outcome); ret = line; break; }
            json_t *pr = kc_print_node(io, line, 0);
            json_free(line);
            if (kc_is_raise(pr)) { json_free(outcome); ret = pr; break; }
            json_free(pr);
        } else {
            json_t *otid = kc_ns_attr(outcome, "task_id");
            if (kc_is_raise(otid)) { json_free(outcome); ret = otid; break; }
            json_t *oreason = kc_ns_attr(outcome, "reason");
            if (kc_is_raise(oreason)) {
                json_free(otid); json_free(outcome); ret = oreason; break;
            }
            char *ts = kc_py_str(otid);
            char *rs = kc_py_str(oreason);
            char *text = kc_fmt("kanban: %s %s: %s", verb ? verb : "", ts, rs);
            free(ts); free(rs); json_free(otid); json_free(oreason);
            json_t *pr = kc_print_str(io, text, 1);
            free(text);
            if (kc_is_raise(pr)) { json_free(outcome); ret = pr; break; }
            json_free(pr);
        }
        json_free(outcome);
    }
    int ids_empty = (ids->c.count == 0);
    json_free(ids); json_free(author); json_free(tenant);
    if (ret) return ret;
    if (!all_flag) return json_number(ok_count == 1 ? 0 : 1);
    /* --all: exit 1 only when every candidate failed (empty ids -> 0). */
    return json_number((ok_count > 0 || ids_empty) ? 0 : 1);
}

/* PoP: kc_retitled_suffix @ hermes_cli/kanban.py:_retitled_suffix */
static json_t *kc_retitled_suffix(const json_t *outcome) {
    json_t *nt = kc_ns_attr(outcome, "new_title");
    if (kc_is_raise(nt)) return nt;
    if (!kc_truthy(nt)) { json_free(nt); return json_string(""); }
    char *rep = kc_py_repr(nt);
    json_free(nt);
    char *s = kc_fmt(" \xe2\x80\x94 retitled: %s", rep);
    free(rep);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP: kc_decompose_ok_line @ hermes_cli/kanban.py:_decompose_ok_line */
static json_t *kc_decompose_ok_line(const json_t *o) {
    json_t *fanout = kc_ns_attr(o, "fanout");
    if (kc_is_raise(fanout)) return fanout;
    json_t *child_ids = kc_ns_attr(o, "child_ids");
    if (kc_is_raise(child_ids)) { json_free(fanout); return child_ids; }
    if (kc_truthy(fanout) && kc_truthy(child_ids)) {
        json_free(fanout);
        json_t *tid = kc_ns_attr(o, "task_id");
        if (kc_is_raise(tid)) { json_free(child_ids); return tid; }
        /* len(o.child_ids) */
        size_t n = 0;
        if (child_ids->type == JSON_ARRAY || child_ids->type == JSON_OBJECT)
            n = child_ids->c.count;
        else if (child_ids->type == JSON_STRING) n = strlen(kc_jstr(child_ids));
        else {
            json_free(tid); json_free(child_ids);
            return kc_raise_fmt("TypeError", "object of type '%s' has no len()",
                                kc_type_name(child_ids));
        }
        json_t *jerr = NULL;
        char *joined = kc_join(child_ids, ", ", &jerr);
        json_free(child_ids);
        if (jerr) { json_free(tid); return jerr; }
        char *ts = kc_py_str(tid);
        json_free(tid);
        char *s = kc_fmt("Decomposed %s \xe2\x86\x92 %zu children (%s); "
                         "root promoted to todo", ts, n, joined);
        free(ts); free(joined);
        json_t *r = json_string(s);
        free(s);
        return r;
    }
    json_free(fanout); json_free(child_ids);
    json_t *tid = kc_ns_attr(o, "task_id");
    if (kc_is_raise(tid)) return tid;
    char *ts = kc_py_str(tid);
    json_free(tid);
    json_t *suffix = kc_retitled_suffix(o);  /* ported-to-ported: REAL */
    if (kc_is_raise(suffix)) { free(ts); return suffix; }
    char *s = kc_fmt("Specified %s \xe2\x86\x92 todo (no fanout)%s",
                     ts, kc_jstr(suffix));
    free(ts); json_free(suffix);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP: kc_cmd_log @ hermes_cli/kanban.py:_cmd_log */
static json_t *kc_cmd_log(kc_io_t *io, const json_t *args) {
    json_t *tid = kc_ns_attr(args, "task_id");
    if (kc_is_raise(tid)) return tid;
    json_t *tail = kc_ns_attr(args, "tail");
    if (kc_is_raise(tail)) { json_free(tid); return tail; }
    json_t *rargs = json_object();
    json_set(rargs, "task_id", tid ? json_copy(tid) : json_null());
    if (tail) json_set(rargs, "tail_bytes", tail);  /* NULL key dropped */
    json_t *content = kc_call(io, "kb.read_worker_log", rargs);
    if (kc_is_raise(content)) { json_free(tid); return content; }
    if (!content) {
        char *ts = kc_py_str(tid);
        json_free(tid);
        char *msg = kc_fmt("(no log for %s \xe2\x80\x94 task may not have spawned yet)", ts);
        free(ts);
        json_t *r = kc_err(io, msg, 1);
        free(msg);
        return r;
    }
    json_free(tid);
    json_t *wargs = json_object();
    json_set(wargs, "text", json_copy(content));
    json_t *w = kc_call(io, "sys.stdout.write", wargs);
    if (kc_is_raise(w)) { json_free(content); return w; }
    json_free(w);
    if (content->type != JSON_STRING) {
        json_t *e = kc_raise_fmt("AttributeError",
                                 "'%s' object has no attribute 'endswith'",
                                 kc_type_name(content));
        json_free(content);
        return e;
    }
    if (!kc_endswith(content->str_val, "\n")) {
        json_t *w2args = json_object();
        json_set(w2args, "text", json_string("\n"));
        json_t *w2 = kc_call(io, "sys.stdout.write", w2args);
        if (kc_is_raise(w2)) { json_free(content); return w2; }
        json_free(w2);
    }
    json_free(content);
    return json_number(0);
}

/* PoP: kc_cmd_request_changes @ hermes_cli/kanban.py:_cmd_request_changes */
static json_t *kc_cmd_request_changes(kc_io_t *io, const json_t *args) {
    json_t *tid = kc_ns_attr(args, "task_id");
    if (kc_is_raise(tid)) return tid;
    json_t *words = kc_ns_attr(args, "reason");
    if (kc_is_raise(words)) { json_free(tid); return words; }
    json_t *jerr = NULL;
    char *joined = kc_join(words, " ", &jerr);
    json_free(words);
    if (jerr) { json_free(tid); return jerr; }
    char *reason = kc_strip(joined);
    free(joined);
    /* with kbc.connect_closing() as conn: silent CM, one seam call */
    json_t *conn = kc_call(io, "kbc.connect_closing", json_object());
    if (kc_is_raise(conn)) { free(reason); json_free(tid); return conn; }
    /* kwarg evaluation order: _worker_run_id_for fires BEFORE request_changes */
    json_t *wargs = json_object();
    json_set(wargs, "task_id", tid ? json_copy(tid) : json_null());
    json_t *erid = kc_call(io, "_worker_run_id_for", wargs);
    if (kc_is_raise(erid)) {
        json_free(conn); free(reason); json_free(tid);
        return erid;
    }
    json_t *rargs = json_object();
    json_set(rargs, "conn", conn ? json_copy(conn) : json_null());
    json_set(rargs, "tid", tid ? json_copy(tid) : json_null());
    json_set(rargs, "reason", json_string(reason));
    if (erid) json_set(rargs, "expected_run_id", erid);  /* NULL key dropped */
    json_free(conn);
    json_t *res = kc_call(io, "kb.request_changes", rargs);
    if (kc_is_raise(res)) { free(reason); json_free(tid); return res; }
    json_t *ok = NULL, *detail = NULL;
    json_t *uerr = kc_unpack2(res, &ok, &detail);
    json_free(res);
    if (uerr) { free(reason); json_free(tid); return uerr; }
    if (!kc_truthy(ok)) {
        char *ds = kc_truthy(detail) ? kc_py_str(detail) : NULL;
        char *ts = kc_py_str(tid);
        char *msg = kc_fmt("cannot request changes for %s: %s", ts,
                           ds ? ds : "invalid review state");
        free(ds); free(ts);
        json_free(ok); json_free(detail); free(reason); json_free(tid);
        json_t *r = kc_err(io, msg, 1);
        free(msg);
        return r;
    }
    char *ts = kc_py_str(tid);
    char *line;
    if (kc_truthy(detail)) {
        char *ds = kc_py_str(detail);
        line = kc_fmt("Requested changes for %s; routed to %s", ts, ds);
        free(ds);
    } else {
        line = kc_fmt("Requested changes for %s", ts);
    }
    free(ts);
    json_free(ok); json_free(detail); free(reason); json_free(tid);
    json_t *pr = kc_print_str(io, line, 0);
    free(line);
    if (kc_is_raise(pr)) return pr;
    json_free(pr);
    return json_number(0);
}
