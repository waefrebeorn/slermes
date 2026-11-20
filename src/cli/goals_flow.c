/* goals_flow.c — FLOW-core port of the UNCLAIMED goal surface of
 * hermes_cli/goals.py (the /goal Ralph loop).
 *
 * Ported (PoP credits each definition below):
 *   module: _decision, _render_extra_criteria, goal_kick_prompt,
 *           last_user_message_content, count_active_delegations,
 *           last_user_message_from_db, _goal_judge_setting,
 *           _call_goal_judge_llm, _gate_workspace, _acquire_session_db,
 *           _release_session_db
 *   GoalState.clear_wait
 *   GoalManager.refresh, _save, _require_goal, _require_active,
 *           _pause_state, _pause_decision, _pop_item, _clear_items,
 *           _park, _waiting_decision, _apply_wait_directive, _budget_pause
 *
 * CLAIMED siblings are SEAMED (ported elsewhere; seaming keeps their PoP
 * credit): load_goal, save_goal, has_goal (GoalManager.has_goal),
 * _get_session_db, _goal_judge_max_tokens, wait_on, wait_on_session,
 * wait_for_seconds. Foreign collaborators are seamed too: time.time,
 * call_llm, load_config, cast (the int/float callable ARGUMENT of
 * _goal_judge_setting), scoped_session_cwd, resolve_agent_cwd,
 * Path.is_dir, profile_scope, open_persistence, db.close,
 * _session_records, read_transcript, row.as_wire_row.
 *
 * Universal seam: every collaborator call goes through
 * gl_seam(io, name, args); the dispatcher appends `name` to io->trace,
 * returns the scripted result, and models a raised exception with
 * {"__raise__": {type,message,code?}}. An "import_raise" script entry
 * models a failed in-function import: the dispatcher returns
 * {"__raise__": spec, "__untrace__": true} and gl_seam retracts the trace
 * token (the live import statement records nothing before it raises).
 * Every seam is SCRIPTED and FAIL-CLOSED: a missing/exhausted script
 * raises RuntimeError("unscripted seam: <name>").
 *
 * io->state is a json OBJECT mirroring caller-held state:
 *   manager    {session_id, default_max_turns, _state} for GoalManager
 *              cases; _state is a GoalState dict (default-filled) or null.
 *   goal_state a GoalState dict for GoalState.clear_wait cases.
 * io->cap_saves captures one {session_id, state} row per save_goal seam
 * call (the persisted payload), computed from the seam ARGS.
 *
 * Representation contract (both sides):
 * - GoalState/GoalContract/GoalGate are plain JSON objects; the harness
 *   and the oracle materialize them with the SAME dataclass defaults
 *   (gl_mat_state / mat_goal_state), so partial fixture dicts agree.
 * - load_goal / wait_on / wait_on_session / wait_for_seconds seam results
 *   that are dicts are materialized as GoalState (default-filled); the
 *   oracle binds real GoalState instances, C reads the filled dict.
 * - db handles (_get_session_db, _release_session_db) are OPAQUE: any
 *   non-null handle exposes messages.read_transcript / close; a null
 *   handle raises AttributeError before the call (no seam token).
 * - call_llm's resp is materialized as nested SimpleNamespace doubles on
 *   the oracle side; attribute-error type names on OBJECT nodes read
 *   'types.SimpleNamespace' on BOTH sides (py3.14 module-qualifies;
 *   scalar nodes keep their JSON type name: NoneType/bool/int/float/str/list).
 * - Paths are posix strings; Path.expanduser is a no-op (fixture domain
 *   excludes '~'); Path(non-str) raises the CPython 3.14 TypeError.
 * - str()-computed fixture positions carry str/int/bool/None only
 *   (integral floats are indistinguishable from ints after JSON parse;
 *   container repr() is out of domain). Non-integral floats use Python
 *   repr (shortest round-trip, e-notation for exp < -4 or >= 16).
 * - int() accepts JSON number/bool/decimal-string (sign, underscores
 *   BETWEEN digits, int-strip whitespace = ASCII ws minus \x1c-\x1f).
 * - An uncaught exception rides as {"__raise__": {type,message,code?}};
 *   except-Exception sites swallow exactly the Exception subtree of the
 *   FULL builtin hierarchy (gl_exc_isa); SystemExit/KeyboardInterrupt/
 *   GeneratorExit/BaseException pass through. except OSError /
 *   except ValueError match by the same hierarchy table.
 * - Fixture input domain is ASCII; the non-ASCII glyphs in message
 *   CONSTANTS (hourglass/pause/em-dash) are byte-pinned UTF-8 literals.
 * - Logging (logger.info/debug) is unobserved on both sides.
 *
 * Ownership: seam results are caller-owned (json_free). gl_seam() frees
 * its args. io->trace / state / cap_saves are harness-owned. Every gl_*
 * entry point returns a NEW reference (value or raise sentinel).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── message-constant glyphs (byte-pinned UTF-8) ───────────────────────── */
#define GL_HOURGLASS "\xe2\x8f\xb3"   /* ⏳ */
#define GL_PAUSE     "\xe2\x8f\xb8"   /* ⏸ */
#define GL_EMDASH    "\xe2\x80\x94"   /* — */

#define GOAL_ALREADY_SEEN_KICK \
    "[Goal set] Continue with the goal you were just given; there is no need to re-read it."
#define GL_REPASTE_MIN_CHARS 400
#define GL_REPASTE_MIN_SHARE 0.8
#define GL_DEFAULT_MAX_TURNS 20
#define GL_DEFAULT_GATE_TIMEOUT_SECONDS 300
#define GL_DEFAULT_GATE_MAX_RETRIES 3

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct gl_flow_io_t {
    json_t *(*call)(struct gl_flow_io_t *io, const char *name, json_t *args);
    json_t *trace;       /* ARRAY of string tokens (harness-owned) */
    json_t *state;       /* OBJECT (harness-owned) */
    json_t *cap_saves;   /* ARRAY of {session_id,state} rows (harness-owned) */
    void   *ctx;
} gl_flow_io_t;

static void gl_trace(gl_flow_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}

static json_t *gl_call(gl_flow_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}

static bool gl_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *gl_raise_spec(const json_t *r) {
    if (!gl_raised(r)) return NULL;
    return json_obj_get(r, "__raise__");
}

static json_t *gl_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}

/* ── FULL builtin exception hierarchy (CPython 3.14, quarry venv) ────────
 * name -> parent. IOError/EnvironmentError/WindowsError are OSError
 * aliases on Windows; ExceptionGroup has TWO parents (BaseExceptionGroup
 * and Exception) — handled in gl_exc_isa. Unknown kinds are synthetic
 * Exception subclasses (the oracle's make_exc builds type(t,(Exception,))).
 */
typedef struct { const char *name; const char *parent; } gl_exc_ent_t;
static const gl_exc_ent_t GL_EXC_TABLE[] = {
    {"BaseException", "object"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},   /* + Exception (special) */
    {"Exception", "BaseException"},
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
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
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
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
};

static const char *gl_exc_parent(const char *name) {
    /* Windows aliases: these names ARE OSError. */
    if (strcmp(name, "IOError") == 0 || strcmp(name, "EnvironmentError") == 0 ||
        strcmp(name, "WindowsError") == 0) return "Exception"; /* canonical OSError below */
    for (size_t i = 0; i < sizeof(GL_EXC_TABLE) / sizeof(GL_EXC_TABLE[0]); i++)
        if (strcmp(GL_EXC_TABLE[i].name, name) == 0) return GL_EXC_TABLE[i].parent;
    return "Exception";   /* synthetic kind: type(t, (Exception,), {}) */
}

static const char *gl_exc_canon(const char *name) {
    if (strcmp(name, "IOError") == 0 || strcmp(name, "EnvironmentError") == 0 ||
        strcmp(name, "WindowsError") == 0) return "OSError";
    return name;
}

/* issubclass(kind, base) over the builtin hierarchy. */
static bool gl_exc_isa(const char *kind, const char *base) {
    if (!kind || !base) return false;
    kind = gl_exc_canon(kind);
    base = gl_exc_canon(base);
    if (strcmp(kind, base) == 0) return true;
    /* ExceptionGroup's second parent: Exception. */
    if (strcmp(kind, "ExceptionGroup") == 0 && gl_exc_isa("Exception", base)) return true;
    const char *p = gl_exc_parent(kind);
    while (p && strcmp(p, "object") != 0) {
        if (strcmp(p, base) == 0) return true;
        if (strcmp(p, "ExceptionGroup") == 0 && gl_exc_isa("Exception", base)) return true;
        p = gl_exc_parent(p);
    }
    return false;
}

static bool gl_spec_isa(const json_t *r, const char *base) {
    json_t *spec = gl_raise_spec(r);
    if (!spec) return false;
    return gl_exc_isa(json_get_str(spec, "type", ""), base);
}

/* seam: trace the attempt, dispatch, free args. An "__untrace__" result
 * (failed in-function import) retracts the trace token. */
static json_t *gl_seam(gl_flow_io_t *io, const char *name, json_t *args) {
    gl_trace(io, name);
    json_t *r = gl_call(io, name, args);
    if (args) json_free(args);
    if (!r) return json_null();
    if (r->type == JSON_OBJECT && json_obj_get(r, "__untrace__")) {
        if (io && io->trace && io->trace->type == JSON_ARRAY && io->trace->c.count > 0) {
            size_t last = io->trace->c.count - 1;
            json_free(io->trace->c.items[last]);
            io->trace->c.count = last;
        }
        json_t *spec = json_obj_get(r, "__raise__");
        json_t *out = json_object();
        json_set(out, "__raise__", spec ? json_copy(spec) : json_null());
        json_set(out, "__untrace__", json_bool(true));
        json_free(r);
        return out;
    }
    return r;
}

/* ── Python-faithful helpers ───────────────────────────────────────────── */
static bool gl_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static bool gl_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool gl_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool gl_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool gl_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool gl_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

static const char *gl_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* Python type(x).__name__ for a JSON value (integral numbers read as int). */
static const char *gl_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

static char *gl_join(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *out = malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}

/* malloc'd printf-style concat (caller frees). */
static char *gl_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *out = malloc((size_t)n + 1);
    if (out) vsnprintf(out, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return out;
}

/* str.strip() whitespace: ASCII ws + \x1c-\x1f (fixture domain ASCII). */
static bool gl_is_strip_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}

/* int() strips a NARROWER set: no \x1c-\x1f. */
static bool gl_is_int_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *gl_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    size_t a = 0, b = n;
    while (a < b && gl_is_strip_ws((unsigned char)p[a])) a++;
    while (b > a && gl_is_strip_ws((unsigned char)p[b - 1])) b--;
    char *out = malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, p + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* " ".join(s.split()) — collapse whitespace runs, drop ends. malloc'd. */
static char *gl_split_join(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t o = 0;
    bool pending = false, in_word = false;
    for (size_t i = 0; i < n; i++) {
        if (gl_is_strip_ws((unsigned char)p[i])) {
            if (in_word) { pending = true; in_word = false; }
        } else {
            if (pending) { out[o++] = ' '; pending = false; }
            out[o++] = p[i];
            in_word = true;
        }
    }
    out[o] = '\0';
    return out;
}

/* Python repr of a non-integral double: shortest round-trip significant
 * digits, e-notation when exp < -4 or >= 16, else fixed with >=1 decimal. */
static char *gl_float_repr(double v) {
    if (isnan(v)) return gl_join("nan", "");
    if (isinf(v)) return gl_join(v < 0 ? "-inf" : "inf", "");
    char buf[64];
    int prec = 17;
    for (int p = 1; p <= 17; p++) {
        snprintf(buf, sizeof buf, "%.*g", p, v);
        if (strtod(buf, NULL) == v) { prec = p; break; }
    }
    /* decimal exponent E */
    char ebuf[64];
    snprintf(ebuf, sizeof ebuf, "%.*e", prec - 1, v);
    int E = atoi(strchr(ebuf, 'e') + 1);
    char *out = malloc(96);
    if (!out) return NULL;
    if (E < -4 || E >= 16) {
        /* mantissa: strip trailing zeros of the fraction */
        char *dot = strchr(ebuf, '.');
        char *e = strchr(ebuf, 'e');
        *e = '\0';
        if (dot) {
            size_t l = strlen(dot + 1);
            while (l > 0 && dot[l] == '0') l--;
            dot[l + 1] = '\0';
            if (l == 0) dot[0] = '\0';
        }
        snprintf(out, 96, "%se%c%02d", ebuf, E < 0 ? '-' : '+', E < 0 ? -E : E);
    } else {
        int dec = prec - 1 - E;
        if (dec < 0) dec = 0;
        snprintf(buf, sizeof buf, "%.*f", dec, v);
        char *dot = strchr(buf, '.');
        if (dot) {
            size_t l = strlen(dot + 1);
            while (l > 1 && dot[l] == '0') l--;
            dot[l + 1] = '\0';
        } else {
            size_t l = strlen(buf);
            buf[l] = '.'; buf[l + 1] = '0'; buf[l + 2] = '\0';
        }
        snprintf(out, 96, "%s", buf);
    }
    return out;
}

/* str(x) for the fixture domain: str/int/bool/None + non-integral float.
 * Integral doubles read as int (libjson has no int/float identity).
 * Containers are OUT of the str()-computed fixture domain; they render
 * compact JSON here (never exercised by the fixtures). malloc'd. */
static char *gl_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return gl_join("None", "");
    switch (v->type) {
        case JSON_BOOL: return gl_join(v->bool_val ? "True" : "False", "");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) {
                if (d == 0.0) return gl_join("0", "");   /* str(int(-0.0)) == "0" */
                char buf[40];
                snprintf(buf, sizeof buf, "%.0f", d);
                return gl_join(buf, "");
            }
            return gl_float_repr(d);
        }
        case JSON_STRING: return gl_join(v->str_val ? v->str_val : "", "");
        default: {
            char *ser = json_serialize(v);
            char *out = gl_join(ser ? ser : "", "");
            free(ser);
            return out;
        }
    }
}

/* repr(s) for an ASCII str: quote selection + escapes (used by the int()
 * ValueError message). malloc'd. */
static char *gl_repr_str(const char *s) {
    const char *p = s ? s : "";
    bool has_sq = strchr(p, '\'') != NULL;
    bool has_dq = strchr(p, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(p);
    char *out = malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c == (unsigned char)q || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            o += (size_t)snprintf(out + o, 5, "\\x%02x", c);
        } else out[o++] = (char)c;
    }
    out[o++] = q;
    out[o] = '\0';
    return out;
}

/* int(x) for number/bool/decimal-string. Returns a NEW number node or a
 * raise sentinel. String domain: optional sign, digits with underscores
 * BETWEEN digits, int-strip whitespace padding; base 10 only. */
static json_t *gl_to_int(const json_t *v) {
    if (!v || v->type == JSON_NULL)
        return gl_make_raise("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) return json_number(trunc(v->num_val));
    if (v->type != JSON_STRING) {
        char *m = gl_fmt("int() argument must be a string, a bytes-like object or a real number, not '%s'",
                         gl_type_name(v));
        json_t *r = gl_make_raise("TypeError", m);
        free(m);
        return r;
    }
    const char *s = v->str_val ? v->str_val : "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && gl_is_int_ws((unsigned char)s[a])) a++;
    while (b > a && gl_is_int_ws((unsigned char)s[b - 1])) b--;
    size_t i = a;
    bool neg = false;
    if (i < b && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); i++; }
    /* digits with single underscores between digits */
    char clean[256];
    size_t cl = 0;
    bool prev_digit = false, any = false, ok = (i < b);
    for (; i < b && ok; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            if (cl < sizeof clean - 1) clean[cl++] = c;
            prev_digit = true; any = true;
        } else if (c == '_') {
            if (!prev_digit) ok = false;
            prev_digit = false;
        } else ok = false;
    }
    if (!ok || !any || !prev_digit) {
        char *rs = gl_repr_str(s);
        char *m = gl_fmt("invalid literal for int() with base 10: %s", rs ? rs : "''");
        free(rs);
        json_t *r = gl_make_raise("ValueError", m);
        free(m);
        return r;
    }
    clean[cl] = '\0';
    double d = strtod(clean, NULL);
    return json_number(neg ? -d : d);
}

/* len(x): array/object count, string byte length (ASCII domain).
 * Returns a NEW number node or a raise sentinel. */
static json_t *gl_len_of(const json_t *v) {
    if (v && (v->type == JSON_ARRAY || v->type == JSON_OBJECT))
        return json_number((double)v->c.count);
    if (v && v->type == JSON_STRING)
        return json_number((double)strlen(v->str_val ? v->str_val : ""));
    char *m = gl_fmt("object of type '%s' has no len()", gl_type_name(v));
    json_t *r = gl_make_raise("TypeError", m);
    free(m);
    return r;
}

static json_t *gl_attr_raise(const char *type_name, const char *attr) {
    char *m = gl_fmt("'%s' object has no attribute '%s'", type_name, attr);
    json_t *r = gl_make_raise("AttributeError", m);
    free(m);
    return r;
}

/* ── GoalState / GoalContract / GoalGate materialization ─────────────────
 * Default-fill a fixture dict exactly like the oracle's mat_goal_state:
 * missing keys take the dataclass defaults; present values stay as-is.
 * null -> json_null; non-object non-null -> copied as-is (opaque). */
static const char *GL_CONTRACT_FIELDS[5] = {"outcome", "verification", "constraints", "boundaries", "stop_when"};

static json_t *gl_mat_contract(const json_t *d) {
    json_t *c = json_object();
    for (int i = 0; i < 5; i++) {
        json_t *v = (d && d->type == JSON_OBJECT) ? json_obj_get(d, GL_CONTRACT_FIELDS[i]) : NULL;
        json_set(c, GL_CONTRACT_FIELDS[i], v ? json_copy(v) : json_string(""));
    }
    return c;
}

static json_t *gl_mat_gate(const json_t *d) {
    json_t *g = json_object();
    json_t *v;
    v = (d && d->type == JSON_OBJECT) ? json_obj_get(d, "command") : NULL;
    json_set(g, "command", v ? json_copy(v) : json_string(""));
    v = (d && d->type == JSON_OBJECT) ? json_obj_get(d, "timeout_seconds") : NULL;
    json_set(g, "timeout_seconds", v ? json_copy(v) : json_number(GL_DEFAULT_GATE_TIMEOUT_SECONDS));
    v = (d && d->type == JSON_OBJECT) ? json_obj_get(d, "max_retries") : NULL;
    json_set(g, "max_retries", v ? json_copy(v) : json_number(GL_DEFAULT_GATE_MAX_RETRIES));
    v = (d && d->type == JSON_OBJECT) ? json_obj_get(d, "attempts") : NULL;
    json_set(g, "attempts", v ? json_copy(v) : json_number(0));
    v = (d && d->type == JSON_OBJECT) ? json_obj_get(d, "last_exit_code") : NULL;
    json_set(g, "last_exit_code", v ? json_copy(v) : json_null());
    v = (d && d->type == JSON_OBJECT) ? json_obj_get(d, "last_output_tail") : NULL;
    json_set(g, "last_output_tail", v ? json_copy(v) : json_string(""));
    return g;
}

static json_t *gl_mat_state(const json_t *d) {
    if (gl_is_null(d)) return json_null();
    if (!gl_is_obj(d)) return json_copy(d);
    json_t *s = json_object();
    json_t *v;
    v = json_obj_get(d, "goal");             json_set(s, "goal", v ? json_copy(v) : json_string(""));
    v = json_obj_get(d, "status");           json_set(s, "status", v ? json_copy(v) : json_string("active"));
    v = json_obj_get(d, "turns_used");       json_set(s, "turns_used", v ? json_copy(v) : json_number(0));
    v = json_obj_get(d, "max_turns");        json_set(s, "max_turns", v ? json_copy(v) : json_number(GL_DEFAULT_MAX_TURNS));
    v = json_obj_get(d, "created_at");       json_set(s, "created_at", v ? json_copy(v) : json_number(0));
    v = json_obj_get(d, "last_turn_at");     json_set(s, "last_turn_at", v ? json_copy(v) : json_number(0));
    v = json_obj_get(d, "last_verdict");     json_set(s, "last_verdict", v ? json_copy(v) : json_null());
    v = json_obj_get(d, "last_reason");      json_set(s, "last_reason", v ? json_copy(v) : json_null());
    v = json_obj_get(d, "paused_reason");    json_set(s, "paused_reason", v ? json_copy(v) : json_null());
    v = json_obj_get(d, "consecutive_parse_failures");
    json_set(s, "consecutive_parse_failures", v ? json_copy(v) : json_number(0));
    v = json_obj_get(d, "consecutive_transport_failures");
    json_set(s, "consecutive_transport_failures", v ? json_copy(v) : json_number(0));
    v = json_obj_get(d, "subgoals");         json_set(s, "subgoals", v ? json_copy(v) : json_array());
    v = json_obj_get(d, "waiting_on_pid");   json_set(s, "waiting_on_pid", v ? json_copy(v) : json_null());
    v = json_obj_get(d, "waiting_on_session"); json_set(s, "waiting_on_session", v ? json_copy(v) : json_null());
    v = json_obj_get(d, "waiting_until");    json_set(s, "waiting_until", v ? json_copy(v) : json_number(0));
    v = json_obj_get(d, "waiting_on_delegations");
    json_set(s, "waiting_on_delegations", v ? json_copy(v) : json_number(0));
    v = json_obj_get(d, "waiting_reason");   json_set(s, "waiting_reason", v ? json_copy(v) : json_null());
    v = json_obj_get(d, "waiting_since");    json_set(s, "waiting_since", v ? json_copy(v) : json_number(0));
    json_set(s, "contract", gl_mat_contract(json_obj_get(d, "contract")));
    json_t *gates = json_array();
    json_t *gs = json_obj_get(d, "gates");
    if (gs && gs->type == JSON_ARRAY)
        for (size_t i = 0; i < gs->c.count; i++)
            json_append(gates, gl_mat_gate(gs->c.items[i]));
    json_set(s, "gates", gates);
    return s;
}

/* GoalState field names (getattr/setattr domain). */
static bool gl_state_has_field(const char *k) {
    static const char *FIELDS[] = {
        "goal", "status", "turns_used", "max_turns", "created_at", "last_turn_at",
        "last_verdict", "last_reason", "paused_reason", "consecutive_parse_failures",
        "consecutive_transport_failures", "subgoals", "waiting_on_pid",
        "waiting_on_session", "waiting_until", "waiting_on_delegations",
        "waiting_reason", "waiting_since", "contract", "gates",
    };
    for (size_t i = 0; i < sizeof(FIELDS) / sizeof(FIELDS[0]); i++)
        if (strcmp(FIELDS[i], k) == 0) return true;
    return false;
}

/* manager accessors (harness guarantees io->state.manager is an object). */
static json_t *gl_mgr(gl_flow_io_t *io) {
    return io && io->state ? json_obj_get(io->state, "manager") : NULL;
}
static json_t *gl_mgr_state(gl_flow_io_t *io) {
    json_t *m = gl_mgr(io);
    return m ? json_obj_get(m, "_state") : NULL;
}

/* ── module-level ports ────────────────────────────────────────────────── */

/* PoP: gl_decision @ hermes_cli/goals.py:_decision */
json_t *gl_decision(const json_t *status, bool should_continue, const json_t *prompt,
                    const char *verdict, const char *reason, const char *message) {
    json_t *d = json_object();
    json_set(d, "status", status ? json_copy(status) : json_null());
    json_set(d, "should_continue", json_bool(should_continue));
    json_set(d, "continuation_prompt", prompt ? json_copy(prompt) : json_null());
    json_set(d, "verdict", json_string(verdict ? verdict : ""));
    json_set(d, "reason", json_string(reason ? reason : ""));
    json_set(d, "message", json_string(message ? message : ""));
    return d;
}

/* PoP: gl_render_extra_criteria @ hermes_cli/goals.py:_render_extra_criteria */
json_t *gl_render_extra_criteria(const json_t *subgoals) {
    /* enumerate() accepts any iterable: list items, str chars, dict keys. */
    size_t n = 0;
    if (gl_is_arr(subgoals) || gl_is_obj(subgoals)) n = subgoals->c.count;
    else if (gl_is_str(subgoals)) n = strlen(gl_sval(subgoals));
    else {
        char *m = gl_fmt("'%s' object is not iterable", gl_type_name(subgoals));
        json_t *r = gl_make_raise("TypeError", m);
        free(m);
        return r;
    }
    size_t cap = 64;
    char *out = malloc(cap);
    if (!out) return json_null();
    size_t o = 0;
    out[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        char *t = NULL;
        if (gl_is_arr(subgoals)) t = gl_py_str(subgoals->c.items[i]);
        else if (gl_is_obj(subgoals)) t = gl_join(subgoals->c.keys[i], "");
        else { char two[2] = {gl_sval(subgoals)[i], '\0'}; t = gl_join(two, ""); }
        char *line = gl_fmt("%s- Extra criterion %zu: %s", i ? "\n" : "", i + 1, t ? t : "");
        free(t);
        if (!line) { free(out); return json_null(); }
        size_t ll = strlen(line);
        if (o + ll + 1 > cap) {
            cap = (o + ll + 1) * 2;
            char *nb = realloc(out, cap);
            if (!nb) { free(line); free(out); return json_null(); }
            out = nb;
        }
        memcpy(out + o, line, ll + 1);
        o += ll;
        free(line);
    }
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: gl_goal_kick_prompt @ hermes_cli/goals.py:goal_kick_prompt */
json_t *gl_goal_kick_prompt(const json_t *goal, const json_t *last_user_message) {
    /* content = last_user_message; list -> " ".join(str(b.get("text","")) ...) */
    char *content = NULL;
    bool content_owned = false;
    if (gl_is_arr(last_user_message)) {
        size_t cap = 64, o = 0;
        content = malloc(cap);
        if (!content) return json_null();
        content[0] = '\0';
        content_owned = true;
        bool first = true;
        for (size_t i = 0; i < last_user_message->c.count; i++) {
            json_t *b = last_user_message->c.items[i];
            if (!gl_is_obj(b)) continue;
            json_t *tv = json_obj_get(b, "text");
            char *t = tv ? gl_py_str(tv) : gl_join("", "");   /* missing -> "" ; null -> "None" */
            if (!t) t = gl_join("", "");
            size_t tl = strlen(t);
            size_t need = o + tl + 2;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(content, cap);
                if (!nb) { free(t); free(content); return json_null(); }
                content = nb;
            }
            if (!first) content[o++] = ' ';
            memcpy(content + o, t, tl + 1);
            o += tl;
            first = false;
            free(t);
        }
    }
    /* goal_norm / last_norm = " ".join(str(x or "").split()) */
    char *gs = gl_py_str(gl_truthy(goal) ? goal : NULL);
    char *goal_norm = gl_split_join(gs ? gs : "");
    free(gs);
    char *ls;
    if (content_owned) {
        ls = gl_join(content, "");
    } else {
        ls = gl_py_str(gl_truthy(last_user_message) ? last_user_message : NULL);
    }
    char *last_norm = gl_split_join(ls ? ls : "");
    free(ls);
    if (content_owned) free(content);
    size_t lg = strlen(goal_norm ? goal_norm : "");
    size_t ll = strlen(last_norm ? last_norm : "");
    bool seen = lg >= GL_REPASTE_MIN_CHARS
        && strstr(last_norm ? last_norm : "", goal_norm ? goal_norm : "") != NULL
        && (double)lg >= GL_REPASTE_MIN_SHARE * (double)ll;
    free(goal_norm);
    free(last_norm);
    if (seen) return json_string(GOAL_ALREADY_SEEN_KICK);
    return goal ? json_copy(goal) : json_null();
}

/* PoP: gl_last_user_message_content @ hermes_cli/goals.py:last_user_message_content */
json_t *gl_last_user_message_content(const json_t *history) {
    if (!gl_truthy(history)) return json_string("");
    if (gl_is_arr(history)) {
        for (size_t k = history->c.count; k > 0; k--) {
            json_t *msg = history->c.items[k - 1];
            if (gl_is_obj(msg)) {
                json_t *role = json_obj_get(msg, "role");
                if (gl_is_str(role) && strcmp(role->str_val, "user") == 0) {
                    json_t *c = json_obj_get(msg, "content");
                    return c ? json_copy(c) : json_null();
                }
            }
        }
        return json_string("");
    }
    /* str / dict are reversible but yield no dict items -> "" */
    if (gl_is_str(history) || gl_is_obj(history)) return json_string("");
    char *m = gl_fmt("'%s' object is not reversible", gl_type_name(history));
    json_t *r = gl_make_raise("TypeError", m);
    free(m);
    return r;
}

/* PoP: gl_count_active_delegations @ hermes_cli/goals.py:count_active_delegations */
json_t *gl_count_active_delegations(gl_flow_io_t *io, const json_t *session_id) {
    if (!gl_truthy(session_id)) return json_number(0);
    json_t *imp = gl_seam(io, "async_delegation_import", json_object());
    if (gl_raised(imp)) {
        if (gl_spec_isa(imp, "Exception")) { json_free(imp); return json_number(0); }
        return imp;
    }
    json_free(imp);
    char *sid = gl_py_str(session_id);
    json_t *a = json_object();
    json_set(a, "session_id", json_string(sid ? sid : ""));
    free(sid);
    json_t *recs = gl_seam(io, "_session_records", a);
    if (gl_raised(recs)) {
        if (gl_spec_isa(recs, "Exception")) { json_free(recs); return json_number(0); }
        return recs;
    }
    json_t *len = gl_len_of(recs);
    json_free(recs);
    if (gl_raised(len)) {
        if (gl_spec_isa(len, "Exception")) { json_free(len); return json_number(0); }
        return len;
    }
    return len;
}

/* PoP: gl_last_user_message_from_db @ hermes_cli/goals.py:last_user_message_from_db */
json_t *gl_last_user_message_from_db(gl_flow_io_t *io, const json_t *session_id) {
    if (!gl_truthy(session_id)) return json_string("");
    json_t *imp = gl_seam(io, "transcript_import", json_object());
    if (gl_raised(imp)) {
        if (gl_spec_isa(imp, "Exception")) { json_free(imp); return json_string(""); }
        return imp;
    }
    json_free(imp);
    json_t *db = gl_seam(io, "_get_session_db", json_object());
    if (gl_raised(db)) {
        if (gl_spec_isa(db, "Exception")) { json_free(db); return json_string(""); }
        return db;
    }
    if (gl_is_null(db)) { json_free(db); return json_string(""); }
    json_free(db);   /* opaque handle: any non-null db exposes messages.read_transcript */
    char *sid = gl_py_str(session_id);
    json_t *a = json_object();
    json_set(a, "session_id", json_string(sid ? sid : ""));
    free(sid);
    json_set(a, "limit", json_number(20));
    json_set(a, "latest", json_bool(true));
    json_t *rows = gl_seam(io, "read_transcript", a);
    if (gl_raised(rows)) {
        if (gl_spec_isa(rows, "Exception")) { json_free(rows); return json_string(""); }
        return rows;
    }
    if (!gl_is_arr(rows)) {
        char *m = gl_fmt("'%s' object is not iterable", gl_type_name(rows));
        json_free(rows);
        json_t *r = gl_make_raise("TypeError", m);
        free(m);
        if (gl_spec_isa(r, "Exception")) { json_free(r); return json_string(""); }
        return r;
    }
    json_t *wires = json_array();
    for (size_t i = 0; i < rows->c.count; i++) {
        json_t *wa = json_object();
        json_set(wa, "index", json_number((double)i));
        json_t *wire = gl_seam(io, "as_wire_row", wa);
        if (gl_raised(wire)) {
            json_free(wires);
            json_free(rows);
            if (gl_spec_isa(wire, "Exception")) { json_free(wire); return json_string(""); }
            return wire;
        }
        json_append(wires, wire);
    }
    json_free(rows);
    json_t *ret = gl_last_user_message_content(wires);
    json_free(wires);
    if (gl_raised(ret)) {
        if (gl_spec_isa(ret, "Exception")) { json_free(ret); return json_string(""); }
        return ret;
    }
    return ret;
}

/* PoP: gl_goal_judge_setting @ hermes_cli/goals.py:_goal_judge_setting */
json_t *gl_goal_judge_setting(gl_flow_io_t *io, const json_t *key, const json_t *defv,
                              const char *cast_kind) {
    json_t *fallback = defv ? json_copy(defv) : json_null();
    json_t *imp = gl_seam(io, "config_import", json_object());
    if (gl_raised(imp)) {
        if (gl_spec_isa(imp, "Exception")) { json_free(imp); return fallback; }
        json_free(fallback);
        return imp;
    }
    json_free(imp);
    json_t *cfg = gl_seam(io, "load_config", json_object());
    if (gl_raised(cfg)) {
        if (gl_spec_isa(cfg, "Exception")) { json_free(cfg); return fallback; }
        json_free(fallback);
        return cfg;
    }
    /* (cfg.get("auxiliary") or {}).get("goal_judge", {}).get(key, default) */
    json_t *raw = NULL;
    json_t *err = NULL;
    if (!gl_is_obj(cfg)) {
        err = gl_attr_raise(gl_type_name(cfg), "get");
    } else {
        json_t *aux = json_obj_get(cfg, "auxiliary");
        if (!gl_truthy(aux)) {
            raw = defv ? json_copy(defv) : json_null();
        } else if (!gl_is_obj(aux)) {
            err = gl_attr_raise(gl_type_name(aux), "get");
        } else {
            json_t *gj = json_obj_get(aux, "goal_judge");
            if (!gj) gj = NULL;                 /* .get("goal_judge", {}) */
            if (!gj) {
                raw = defv ? json_copy(defv) : json_null();
            } else if (!gl_is_obj(gj)) {
                err = gl_attr_raise(gl_type_name(gj), "get");
            } else {
                json_t *v = json_obj_get(gj, gl_sval(key));
                raw = v ? json_copy(v) : (defv ? json_copy(defv) : json_null());
            }
        }
    }
    json_free(cfg);
    if (err) {
        if (gl_spec_isa(err, "Exception")) { json_free(err); return fallback; }
        json_free(fallback);
        return err;
    }
    json_t *ca = json_object();
    json_set(ca, "value", raw);   /* adopts raw */
    json_set(ca, "kind", json_string(cast_kind ? cast_kind : "int"));
    json_t *value = gl_seam(io, "cast", ca);
    if (gl_raised(value)) {
        if (gl_spec_isa(value, "Exception")) { json_free(value); return fallback; }
        json_free(fallback);
        return value;
    }
    /* if value > 0: return value */
    bool gt = false;
    if (gl_is_num(value)) gt = value->num_val > 0.0;
    else if (value && value->type == JSON_BOOL) gt = value->bool_val;
    else {
        char *m = gl_fmt("'>' not supported between instances of '%s' and 'int'", gl_type_name(value));
        json_free(value);
        json_t *r = gl_make_raise("TypeError", m);
        free(m);
        if (gl_spec_isa(r, "Exception")) { json_free(r); return fallback; }
        json_free(fallback);
        return r;
    }
    if (gt) { json_free(fallback); return value; }
    json_free(value);
    return fallback;
}

/* resp attribute-chain navigation over SimpleNamespace-materialized dicts.
 * OBJECT nodes read type name 'types.SimpleNamespace' (CPython 3.14
 * module-qualifies the name in attribute/subscript errors). */
static const char *gl_resp_type_name(const json_t *v) {
    if (gl_is_obj(v)) return "types.SimpleNamespace";
    return gl_type_name(v);
}

/* obj.attr — returns a NEW copy or a raise sentinel. */
static json_t *gl_resp_attr(const json_t *obj, const char *attr) {
    if (gl_is_obj(obj)) {
        json_t *v = json_obj_get(obj, attr);
        if (v) return json_copy(v);
    }
    return gl_attr_raise(gl_resp_type_name(obj), attr);
}

/* seq[0] — returns a NEW copy or a raise sentinel. */
static json_t *gl_resp_index0(const json_t *seq) {
    if (gl_is_arr(seq)) {
        if (seq->c.count == 0) return gl_make_raise("IndexError", "list index out of range");
        return json_copy(seq->c.items[0]);
    }
    if (gl_is_str(seq)) {
        const char *s = seq->str_val ? seq->str_val : "";
        if (s[0] == '\0') return gl_make_raise("IndexError", "string index out of range");
        char two[2] = {s[0], '\0'};
        return json_string(two);
    }
    char *m = gl_fmt("'%s' object is not subscriptable", gl_resp_type_name(seq));
    json_t *r = gl_make_raise("TypeError", m);
    free(m);
    return r;
}

/* PoP: gl_call_goal_judge_llm @ hermes_cli/goals.py:_call_goal_judge_llm */
json_t *gl_call_goal_judge_llm(gl_flow_io_t *io, const json_t *system_prompt,
                               const json_t *user_prompt, const json_t *timeout) {
    /* kwargs evaluate left-to-right: max_tokens seam fires BEFORE call_llm. */
    json_t *mt = gl_seam(io, "goal_judge_max_tokens", json_object());
    if (gl_raised(mt)) return mt;
    json_t *messages = json_array();
    json_t *m1 = json_object();
    json_set(m1, "role", json_string("system"));
    json_set(m1, "content", system_prompt ? json_copy(system_prompt) : json_null());
    json_append(messages, m1);
    json_t *m2 = json_object();
    json_set(m2, "role", json_string("user"));
    json_set(m2, "content", user_prompt ? json_copy(user_prompt) : json_null());
    json_append(messages, m2);
    json_t *a = json_object();
    json_set(a, "task", json_string("goal_judge"));
    json_set(a, "messages", messages);
    json_set(a, "temperature", json_number(0));
    json_set(a, "max_tokens", mt);   /* adopts mt */
    json_set(a, "timeout", timeout ? json_copy(timeout) : json_null());
    json_t *resp = gl_seam(io, "call_llm", a);
    if (gl_raised(resp)) return resp;
    /* try: return resp.choices[0].message.content or "" except Exception: "" */
    json_t *choices = gl_resp_attr(resp, "choices");
    json_free(resp);
    if (gl_raised(choices)) {
        if (gl_spec_isa(choices, "Exception")) { json_free(choices); return json_string(""); }
        return choices;
    }
    json_t *first = gl_resp_index0(choices);
    json_free(choices);
    if (gl_raised(first)) {
        if (gl_spec_isa(first, "Exception")) { json_free(first); return json_string(""); }
        return first;
    }
    json_t *message = gl_resp_attr(first, "message");
    json_free(first);
    if (gl_raised(message)) {
        if (gl_spec_isa(message, "Exception")) { json_free(message); return json_string(""); }
        return message;
    }
    json_t *content = gl_resp_attr(message, "content");
    json_free(message);
    if (gl_raised(content)) {
        if (gl_spec_isa(content, "Exception")) { json_free(content); return json_string(""); }
        return content;
    }
    if (!gl_truthy(content)) { json_free(content); return json_string(""); }
    return content;
}

/* PoP: gl_gate_workspace @ hermes_cli/goals.py:_gate_workspace */
json_t *gl_gate_workspace(gl_flow_io_t *io) {
    json_t *imp = gl_seam(io, "runtime_cwd_import", json_object());
    if (gl_raised(imp)) return imp;
    json_free(imp);
    json_t *declared = gl_seam(io, "scoped_session_cwd", json_object());
    if (gl_raised(declared)) return declared;
    if (gl_truthy(declared)) {
        if (!gl_is_str(declared)) {
            char *m = gl_fmt("argument should be a str or an os.PathLike object where "
                             "__fspath__ returns a str, not '%s'", gl_type_name(declared));
            json_free(declared);
            json_t *r = gl_make_raise("TypeError", m);
            free(m);
            return r;
        }
        json_t *a = json_object();
        json_set(a, "path", json_copy(declared));
        json_t *isdir = gl_seam(io, "path_is_dir", a);
        if (gl_raised(isdir)) { json_free(declared); return isdir; }
        bool ok = gl_truthy(isdir);
        json_free(isdir);
        json_t *out = json_array();
        if (ok) {
            json_append(out, declared);   /* adopts declared */
            json_append(out, json_null());
        } else {
            char *m = gl_fmt("the session workspace %s is not a directory on this host, "
                             "and running gates anywhere else would check a different project",
                             gl_sval(declared));
            json_free(declared);
            json_append(out, json_null());
            json_append(out, json_string(m ? m : ""));
            free(m);
        }
        return out;
    }
    json_free(declared);
    /* try: return str(resolve_agent_cwd()), None except OSError: None, None */
    json_t *rc = gl_seam(io, "resolve_agent_cwd", json_object());
    if (gl_raised(rc)) {
        if (gl_spec_isa(rc, "OSError")) {
            json_free(rc);
            json_t *out = json_array();
            json_append(out, json_null());
            json_append(out, json_null());
            return out;
        }
        return rc;
    }
    char *s = gl_py_str(rc);
    json_free(rc);
    json_t *out = json_array();
    json_append(out, json_string(s ? s : ""));
    free(s);
    json_append(out, json_null());
    return out;
}

/* PoP: gl_acquire_session_db @ hermes_cli/goals.py:_acquire_session_db */
json_t *gl_acquire_session_db(gl_flow_io_t *io, const json_t *home) {
    json_t *imp = gl_seam(io, "persistence_assembly_import", json_object());
    if (gl_raised(imp)) return imp;
    json_free(imp);
    /* Path(home): non-str raises BEFORE the profile_scope call. */
    if (!gl_is_str(home)) {
        char *m = gl_fmt("argument should be a str or an os.PathLike object where "
                         "__fspath__ returns a str, not '%s'", gl_type_name(home));
        json_t *r = gl_make_raise("TypeError", m);
        free(m);
        return r;
    }
    json_t *pa = json_object();
    json_set(pa, "home", json_copy(home));
    json_t *scope = gl_seam(io, "profile_scope", pa);
    if (gl_raised(scope)) return scope;
    json_t *oa = json_object();
    json_set(oa, "scope", scope);   /* adopts scope */
    json_set(oa, "eager", json_bool(true));
    json_t *db = gl_seam(io, "open_persistence", oa);
    if (gl_raised(db)) return db;
    return db;
}

/* PoP: gl_release_session_db @ hermes_cli/goals.py:_release_session_db */
json_t *gl_release_session_db(gl_flow_io_t *io, const json_t *db) {
    if (gl_is_null(db)) return json_null();   /* AttributeError on .close, swallowed */
    json_t *r = gl_seam(io, "db_close", json_object());
    if (gl_raised(r)) {
        if (gl_spec_isa(r, "Exception")) { json_free(r); return json_null(); }
        return r;
    }
    json_free(r);
    return json_null();
}

/* PoP: gl_clear_wait @ hermes_cli/goals.py:clear_wait */
json_t *gl_clear_wait(json_t *state) {
    if (!gl_is_obj(state)) return json_null();
    json_set(state, "waiting_on_pid", json_null());
    json_set(state, "waiting_on_session", json_null());
    json_set(state, "waiting_until", json_number(0));
    json_set(state, "waiting_on_delegations", json_number(0));
    json_set(state, "waiting_reason", json_null());
    json_set(state, "waiting_since", json_number(0));
    return json_null();
}

/* ── GoalManager ports ───────────────────────────────────────────────────
 * The manager mirror lives at io->state.manager = {session_id,
 * default_max_turns, _state}; _state is a default-filled GoalState dict
 * or null. Internal calls between ported methods are direct (REAL on
 * both sides); claimed siblings ride the universal seam. */

/* array pop: transfer ownership of the removed item to the caller. */
static json_t *gl_arr_pop(json_t *arr, size_t idx) {
    json_t *item = arr->c.items[idx];
    memmove(&arr->c.items[idx], &arr->c.items[idx + 1],
            (arr->c.count - idx - 1) * sizeof(json_t *));
    arr->c.count--;
    return item;
}

/* PoP: gl_manager_refresh @ hermes_cli/goals.py:refresh */
json_t *gl_manager_refresh(gl_flow_io_t *io) {
    json_t *m = gl_mgr(io);
    json_t *a = json_object();
    json_set(a, "session_id", json_copy(json_obj_get(m, "session_id")));
    json_t *st = gl_seam(io, "load_goal", a);
    if (gl_raised(st)) return st;
    json_t *mat = gl_mat_state(st);
    json_free(st);
    json_set(m, "_state", mat);   /* adopts mat */
    return json_null();
}

/* PoP: gl_manager_save @ hermes_cli/goals.py:_save */
json_t *gl_manager_save(gl_flow_io_t *io) {
    json_t *m = gl_mgr(io);
    json_t *st = gl_mgr_state(io);
    json_t *a = json_object();
    json_set(a, "session_id", json_copy(json_obj_get(m, "session_id")));
    json_set(a, "state", st ? json_copy(st) : json_null());
    json_t *r = gl_seam(io, "save_goal", a);
    if (gl_raised(r)) return r;
    json_free(r);
    return st ? json_copy(st) : json_null();
}

/* PoP: gl_manager_require_goal @ hermes_cli/goals.py:_require_goal */
json_t *gl_manager_require_goal(gl_flow_io_t *io) {
    json_t *st = gl_mgr_state(io);
    if (gl_is_null(st)) return gl_make_raise("RuntimeError", "no active goal");
    json_t *hg = gl_seam(io, "has_goal", json_object());
    if (gl_raised(hg)) return hg;
    bool has = gl_truthy(hg);
    json_free(hg);
    if (!has) return gl_make_raise("RuntimeError", "no active goal");
    return json_copy(st);
}

/* PoP: gl_manager_require_active @ hermes_cli/goals.py:_require_active */
json_t *gl_manager_require_active(gl_flow_io_t *io) {
    json_t *st = gl_mgr_state(io);
    if (gl_is_null(st)) return gl_make_raise("RuntimeError", "no active goal to park");
    json_t *status = json_obj_get(st, "status");
    if (!gl_is_str(status) || strcmp(status->str_val, "active") != 0)
        return gl_make_raise("RuntimeError", "no active goal to park");
    return json_copy(st);
}

/* PoP: gl_manager_pause_state @ hermes_cli/goals.py:_pause_state */
json_t *gl_manager_pause_state(gl_flow_io_t *io, const json_t *reason) {
    json_t *st = gl_mgr_state(io);
    if (gl_is_null(st))
        return gl_make_raise("AttributeError",
            "'NoneType' object has no attribute 'status' and no __dict__ for setting new attributes");
    json_set(st, "status", json_string("paused"));
    json_set(st, "paused_reason", reason ? json_copy(reason) : json_null());
    json_t *r = gl_manager_save(io);
    if (gl_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: gl_manager_pause_decision @ hermes_cli/goals.py:_pause_decision */
json_t *gl_manager_pause_decision(gl_flow_io_t *io, const char *paused_reason,
                                  const char *verdict, const char *reason, const char *message) {
    json_t *pr = json_string(paused_reason ? paused_reason : "");
    json_t *r = gl_manager_pause_state(io, pr);
    json_free(pr);
    if (gl_raised(r)) return r;
    json_free(r);
    json_t *status = json_string("paused");
    json_t *d = gl_decision(status, false, NULL, verdict, reason, message);
    json_free(status);
    return d;
}

/* getattr(state, attr) over the GoalState field domain (borrowed result;
 * NULL == missing, which reads as None downstream). */
static json_t *gl_state_getattr(const json_t *st, const char *attr) {
    if (!gl_state_has_field(attr)) return gl_attr_raise("GoalState", attr);
    return json_obj_get(st, attr);
}

/* PoP: gl_manager_pop_item @ hermes_cli/goals.py:_pop_item */
json_t *gl_manager_pop_item(gl_flow_io_t *io, const char *attr, const json_t *index_1based) {
    json_t *req = gl_manager_require_goal(io);
    if (gl_raised(req)) return req;
    json_free(req);
    json_t *st = gl_mgr_state(io);
    json_t *items = gl_state_getattr(st, attr);
    if (gl_raised(items)) return items;
    json_t *iv = gl_to_int(index_1based);
    if (gl_raised(iv)) return iv;
    double idx = iv->num_val - 1.0;
    json_free(iv);
    json_t *len = gl_len_of(items);
    if (gl_raised(len)) return len;
    double n = len->num_val;
    json_free(len);
    if (idx < 0 || idx >= n) {
        char *m = gl_fmt("index out of range (1..%.0f)", n);
        json_t *r = gl_make_raise("IndexError", m);
        free(m);
        return r;
    }
    if (!gl_is_arr(items)) {
        /* str has no .pop; dict.pop(int) misses every JSON (string) key. */
        if (gl_is_obj(items)) {
            char *m = gl_fmt("%.0f", idx);
            json_t *r = gl_make_raise("KeyError", m);
            free(m);
            return r;
        }
        return gl_attr_raise(gl_type_name(items), "pop");
    }
    json_t *removed = gl_arr_pop(items, (size_t)idx);
    json_t *sv = gl_manager_save(io);
    if (gl_raised(sv)) { json_free(removed); return sv; }
    json_free(sv);
    return removed;
}

/* PoP: gl_manager_clear_items @ hermes_cli/goals.py:_clear_items */
json_t *gl_manager_clear_items(gl_flow_io_t *io, const char *attr) {
    json_t *req = gl_manager_require_goal(io);
    if (gl_raised(req)) return req;
    json_free(req);
    json_t *st = gl_mgr_state(io);
    json_t *items = gl_state_getattr(st, attr);
    if (gl_raised(items)) return items;
    json_t *len = gl_len_of(items);
    if (gl_raised(len)) return len;
    json_set(st, attr, json_array());
    json_t *sv = gl_manager_save(io);
    if (gl_raised(sv)) { json_free(len); return sv; }
    json_free(sv);
    return len;
}

/* PoP: gl_manager_park @ hermes_cli/goals.py:_park */
json_t *gl_manager_park(gl_flow_io_t *io, const json_t *reason, const json_t *barrier) {
    json_t *req = gl_manager_require_active(io);
    if (gl_raised(req)) return req;
    json_free(req);
    json_t *st = gl_mgr_state(io);
    gl_clear_wait(st);
    /* for k, v in barrier.items(): setattr(state, k, v) — asdict projects
     * only dataclass fields, so unknown keys are dropped on both sides. */
    if (gl_is_obj(barrier)) {
        for (size_t i = 0; i < barrier->c.count; i++) {
            const char *k = barrier->c.keys[i];
            if (gl_state_has_field(k))
                json_set(st, k, json_copy(barrier->c.items[i]));
        }
    }
    /* state.waiting_reason = (reason or "").strip() or None */
    if (!gl_truthy(reason)) {
        json_set(st, "waiting_reason", json_null());
    } else if (gl_is_str(reason)) {
        char *s = gl_strip(reason->str_val);
        json_set(st, "waiting_reason", (s && s[0]) ? json_string(s) : json_null());
        free(s);
    } else {
        return gl_attr_raise(gl_type_name(reason), "strip");
    }
    json_t *now = gl_seam(io, "time_time", json_object());
    if (gl_raised(now)) return now;
    json_set(st, "waiting_since", now);   /* adopts now */
    return gl_manager_save(io);
}

/* PoP: gl_manager_waiting_decision @ hermes_cli/goals.py:_waiting_decision */
json_t *gl_manager_waiting_decision(gl_flow_io_t *io, const json_t *state) {
    if (gl_is_null(state)) return gl_attr_raise("NoneType", "waiting_on_session");
    if (!gl_is_obj(state)) return gl_attr_raise(gl_type_name(state), "waiting_on_session");
    char *tgt = NULL;
    json_t *sess = json_obj_get(state, "waiting_on_session");
    json_t *pid = json_obj_get(state, "waiting_on_pid");
    if (sess && sess->type != JSON_NULL) {
        char *s = gl_py_str(sess);
        tgt = gl_fmt("session %s", s ? s : "");
        free(s);
    } else if (pid && pid->type != JSON_NULL) {
        char *s = gl_py_str(pid);
        tgt = gl_fmt("pid %s", s ? s : "");
        free(s);
    } else {
        json_t *now = gl_seam(io, "time_time", json_object());
        if (gl_raised(now)) return now;
        json_t *wu = json_obj_get(state, "waiting_until");
        bool lnum = gl_is_num(wu) || (wu && wu->type == JSON_BOOL);
        bool rnum = gl_is_num(now) || (now && now->type == JSON_BOOL);
        if (!lnum || !rnum) {
            char *m = gl_fmt("unsupported operand type(s) for -: '%s' and '%s'",
                             gl_type_name(wu), gl_type_name(now));
            json_free(now);
            json_t *r = gl_make_raise("TypeError", m);
            free(m);
            return r;
        }
        double lv = gl_is_num(wu) ? wu->num_val : (wu->bool_val ? 1.0 : 0.0);
        double rv = gl_is_num(now) ? now->num_val : (now->bool_val ? 1.0 : 0.0);
        json_free(now);
        long long n = (long long)trunc(lv - rv);
        if (n < 0) n = 0;
        tgt = gl_fmt("%llds remaining", n);
    }
    json_t *wr = json_obj_get(state, "waiting_reason");
    char *reason = gl_truthy(wr) ? gl_py_str(wr) : gl_join(tgt ? tgt : "", "");
    char *msg = gl_fmt(GL_HOURGLASS " Goal parked " GL_EMDASH " waiting on %s: %s",
                       tgt ? tgt : "", reason ? reason : "");
    json_t *status = json_string("active");
    json_t *d = gl_decision(status, false, NULL, "waiting", reason, msg);
    json_free(status);
    free(tgt);
    free(reason);
    free(msg);
    return d;
}

/* read .waiting_on_session / .waiting_on_pid off a wait_on* seam result:
 * dict results are materialized GoalStates (attribute always present);
 * scalars raise AttributeError with their JSON type name. */
static json_t *gl_wait_ret_attr(const json_t *ret, const char *attr) {
    if (gl_is_obj(ret)) {
        json_t *mat = gl_mat_state(ret);
        json_t *v = json_obj_get(mat, attr);
        json_t *out = v ? json_copy(v) : json_null();
        json_free(mat);
        return out;
    }
    return gl_attr_raise(gl_type_name(ret), attr);
}

/* PoP: gl_manager_apply_wait_directive @ hermes_cli/goals.py:_apply_wait_directive */
json_t *gl_manager_apply_wait_directive(gl_flow_io_t *io, const json_t *directive,
                                        const json_t *reason, const json_t *active_delegations) {
    if (!gl_is_obj(directive)) return gl_attr_raise(gl_type_name(directive), "get");
    char *tgt = NULL;
    json_t *sess = json_obj_get(directive, "session_id");
    json_t *pidv = json_obj_get(directive, "pid");
    if (gl_truthy(sess)) {
        char *sid = gl_py_str(sess);
        json_t *a = json_object();
        json_set(a, "session_id", json_string(sid ? sid : ""));
        free(sid);
        json_set(a, "reason", reason ? json_copy(reason) : json_null());
        json_t *ret = gl_seam(io, "wait_on_session", a);
        if (gl_raised(ret)) return ret;
        json_t *v = gl_wait_ret_attr(ret, "waiting_on_session");
        json_free(ret);
        if (gl_raised(v)) return v;
        char *s = gl_py_str(v);
        json_free(v);
        tgt = gl_fmt("session %s", s ? s : "");
        free(s);
    } else if (gl_truthy(pidv)) {
        json_t *iv = gl_to_int(pidv);   /* int() OUTSIDE the try: raises propagate */
        if (gl_raised(iv)) return iv;
        json_t *a = json_object();
        json_set(a, "pid", iv);   /* adopts iv */
        json_set(a, "reason", reason ? json_copy(reason) : json_null());
        json_t *ret = gl_seam(io, "wait_on", a);
        if (gl_raised(ret)) {
            if (gl_spec_isa(ret, "ValueError")) { json_free(ret); return json_null(); }
            return ret;
        }
        json_t *v = gl_wait_ret_attr(ret, "waiting_on_pid");
        json_free(ret);
        if (gl_raised(v)) return v;   /* AttributeError is NOT caught here */
        char *s = gl_py_str(v);
        json_free(v);
        tgt = gl_fmt("pid %s", s ? s : "");
        free(s);
    } else {
        json_t *sec = json_obj_get(directive, "seconds");
        if (!sec) return gl_make_raise("KeyError", "'seconds'");
        json_t *iv = gl_to_int(sec);
        if (gl_raised(iv)) return iv;
        json_t *a = json_object();
        json_set(a, "seconds", iv);   /* adopts iv */
        json_set(a, "reason", reason ? json_copy(reason) : json_null());
        json_set(a, "on_delegations", active_delegations ? json_copy(active_delegations) : json_number(0));
        json_t *ret = gl_seam(io, "wait_for_seconds", a);
        if (gl_raised(ret)) return ret;
        json_free(ret);
        char *s = gl_py_str(sec);   /* the ORIGINAL value, not the int() */
        tgt = gl_fmt("%ss", s ? s : "");
        free(s);
    }
    char *rs = gl_py_str(reason);
    char *msg = gl_fmt(GL_HOURGLASS " Goal parked (judge) " GL_EMDASH " waiting on %s: %s",
                       tgt ? tgt : "", rs ? rs : "");
    json_t *status = json_string("active");
    json_t *d = gl_decision(status, false, NULL, "wait", rs, msg);
    json_free(status);
    free(tgt);
    free(rs);
    free(msg);
    return d;
}

/* PoP: gl_manager_budget_pause @ hermes_cli/goals.py:_budget_pause */
json_t *gl_manager_budget_pause(gl_flow_io_t *io, const json_t *state,
                                const char *verdict, const char *reason, const json_t *note) {
    if (gl_is_null(state)) return gl_attr_raise("NoneType", "turns_used");
    json_t *tu = json_obj_get(state, "turns_used");
    if (!tu) return gl_attr_raise("GoalState", "turns_used");
    json_t *mt = json_obj_get(state, "max_turns");
    if (!mt) return gl_attr_raise("GoalState", "max_turns");
    char *tus = gl_py_str(tu);
    char *mts = gl_py_str(mt);
    char *ns = gl_py_str(note);
    char *pr = gl_fmt("turn budget exhausted (%s/%s)", tus ? tus : "", mts ? mts : "");
    char *msg = gl_fmt(GL_PAUSE " Goal paused " GL_EMDASH " %s/%s turns used%s. "
                       "Use /goal resume to keep going, or /goal clear to stop.",
                       tus ? tus : "", mts ? mts : "", ns ? ns : "");
    json_t *d = gl_manager_pause_decision(io, pr, verdict, reason, msg);
    free(tus); free(mts); free(ns); free(pr); free(msg);
    return d;
}