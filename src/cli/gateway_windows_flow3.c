/*
 * gateway_windows_flow3.c -- FLOW port of the UNCLAIMED remainder of
 * hermes_cli/gateway_windows.py (LIVE authority), third slice after
 * port_gateway_windows_flow.c and gateway_windows_flow2.c. Self-contained:
 * libjson only. All functions static gw3_*; the harness
 * (#include "cli/gateway_windows_flow3.c") wires the universal seam to
 * scripted results and records the ordered trace.
 *
 * Covered flows (PoP per function):
 *   console encoding / decode: _windows_console_encodings,
 *     _decode_schtasks_output
 *   service roots / launcher settings: hermes_service_roots,
 *     _launcher_settings, _launcher_scripts
 *   dead-launcher refresh / drift: refresh_dead_launcher,
 *     launcher_script_drift
 *   stdio probes: _stdout_isatty, _stdin_console_mode_ok
 *   deep status probes: _probe, _probe_missing, _probe_pid_file,
 *     _probe_lock_file, _probe_running_pid, _probe_pid_exists,
 *     _probe_state_file, _probe_exit_diag
 *
 * SEAM DOCTRINE (same as gateway_windows_flow2.c): every live effect enters
 * through ONE universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returning a NEW json_t* (NULL == Python None). A raising live call is
 * scripted {"__raise__": "<ExcClass>", "msg": ...}; the cores propagate it
 * exactly where the live try/except catches it. except Exception does NOT
 * catch BaseException/SystemExit/KeyboardInterrupt/GeneratorExit/
 * BaseExceptionGroup -- those pass through to the harness ret envelope
 * (BaseException passthrough). A MISSING seam key fail-closes: the harness
 * delivers {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a
 * present JSON null is a valid None. Claimed siblings (_schtasks_encoding,
 * _hermes_home, _preserve_hermes_home_path, _stable_gateway_working_dir,
 * _sanitize_filename, _render_launchers, get_task_script_path,
 * _assert_windows, _write_launcher_scripts) and foreign collaborators
 * (project_root_str, get_python_path, _profile_arg, _vbs_identity,
 * launcher_text, read_definition, is_gateway_runtime_lock_active,
 * get_running_pid, _pid_exists, kernel32 getters, print, json.loads,
 * datetime.fromisoformat/now, open, Path.exists/read_text/read_bytes/resolve,
 * os.path.normcase, sys.stdout.isatty) are SEAMED; in-function imports fire
 * an "import_<name>" seam on module attribute access, mirroring the oracle's
 * stub modules. UNCLAIMED ported siblings (_launcher_settings,
 * _launcher_scripts, _windows_console_encodings, _probe, _probe_missing)
 * COMPOSE for real (no seam, no PoP re-claim).
 *
 * Seam name table (name -> live call, args):
 *   print                          print(text)
 *   schtasks_encoding              _schtasks_encoding() -> str
 *   get_console_output_cp          kernel32.GetConsoleOutputCP() -> number
 *   get_oem_cp                     kernel32.GetOEMCP() -> number
 *   get_acp                        kernel32.GetACP() -> number
 *   project_root_str               project_root_str() -> str
 *   hermes_home                    _hermes_home() -> posix str|null
 *   import_project_root            from hermes_cli.gateway import PROJECT_ROOT (value)
 *   import_profile_arg             from hermes_cli.gateway import _profile_arg (marker)
 *   import_get_python_path         from hermes_cli.gateway import get_python_path (marker)
 *   get_python_path                get_python_path() -> str
 *   preserve_hermes_home_path      _preserve_hermes_home_path(value) -> str
 *   stable_gateway_working_dir     _stable_gateway_working_dir(project_root,home) -> str
 *   profile_arg                    _profile_arg(hermes_home) -> str
 *   get_task_script_path           get_task_script_path() -> posix str
 *   render_launchers               _render_launchers(settings,script_path) -> [[p,c,e],..]
 *   sanitize_filename              _sanitize_filename(name) -> str
 *   import_vbs_identity            from ...gateway_runtime_service_identity import _vbs_identity
 *   import_launcher_text           ... launcher_text
 *   import_read_definition         ... read_definition
 *   read_definition                read_definition(path) -> str
 *   launcher_text                  launcher_text(definition) -> str
 *   vbs_identity                   _vbs_identity(text) -> [configured, argv]
 *   path_exists                    Path(path).exists() -> bool
 *   path_resolve                   Path(path).resolve() -> posix str
 *   normcase                       os.path.normcase(path) -> str
 *   path_read_bytes                Path(path).read_bytes() -> [byte ints]
 *   write_launcher_scripts         _write_launcher_scripts(scripts)
 *   assert_windows                 _assert_windows()
 *   stdout_isatty                  sys.stdout.isatty() -> bool
 *   get_std_handle                 kernel32.GetStdHandle(-10) -> handle
 *   get_console_mode               kernel32.GetConsoleMode(handle,byref) -> number
 *   import_is_gateway_runtime_lock_active  from gateway.status import ...
 *   is_gateway_runtime_lock_active is_gateway_runtime_lock_active(lock_path) -> bool
 *   import_get_running_pid         from gateway.status import get_running_pid
 *   get_running_pid                get_running_pid(cleanup_stale=False) -> int|null
 *   import_pid_exists              from gateway.status import _pid_exists
 *   pid_exists                     _pid_exists(pid) -> bool
 *   path_read_text                 Path(path).read_text(encoding) -> str
 *   json_loads                     json.loads(s) -> value (JSONDecodeError scripted)
 *   datetime_fromisoformat         datetime.fromisoformat(s) -> {epoch,aware}
 *   datetime_now                   datetime.now(UTC) -> epoch seconds (UTC-aware)
 *   open                           open(path,"rb") -> {"data":[byte ints]}
 *
 * Fixture domains (documented exclusions):
 * - Paths are POSIX strings; the path algebra models pathlib PurePosixPath
 *   str/name/parent/with_suffix/is_absolute and / join on that domain
 *   (drop "." components, keep "..", exactly two leading slashes preserved).
 * - Bytes travel as JSON arrays of ints 0..255. Code pages implemented:
 *   cp437, cp1252 (live-generated tables), cp65001/utf-8; OTHER cp numbers
 *   model LookupError (unknown encoding) -- fixture getter results stay in
 *   {437, 1252, 65001, non-positive, unknown}. Multi-byte code pages (cp932)
 *   are outside the corpus.
 * - utf-8 strict/replace transcribes CPython's maximal-subpart replacement
 *   (probe-verified on 30k random byte strings); utf-16 decode requires the
 *   BOM gate (FFFE/FEFF), rejects odd length / lone surrogates.
 * - exit-diag file data bytes are ASCII (0..127): decoded text is ASCII +
 *   U+FFFD only, so splitlines (ASCII boundaries \n \r \r\n \v \f \x1c \x1d
 *   \x1e, NOT \x1f) and strip (ASCII White_Space incl \x1c-\x1f) are
 *   byte-safe; U+0085/U+2028/U+2029-producing sequences are excluded.
 * - Launcher content strings are ASCII; encode("utf-16") emits the LE BOM
 *   (x86 gate host). json.loads / datetime.fromisoformat are SEAMED leaves
 *   (scripted parse results / {epoch,aware}); their internal grammar is not
 *   re-ported here. int() coercion: numbers/bools/decimal strings with
 *   between-digit underscores, |value| < 2^53; no integral-float literals in
 *   str()-computed positions (libjson has no int/float identity).
 * - Scripted raises use the uniform {__raise__, msg} shape; KeyError is kept
 *   OUT of scripted-raise fixtures (str(KeyError(k)) repr-quotes the key).
 *   vbs_identity results are 2-element arrays (str/configured + list/argv);
 *   open results are {"data":[ints]}; render_launchers results are
 *   [path,content,encoding] triples with encoding in {utf-8, utf-16}.
 * - logger is silent (no trace events), matching flow/flow2. The exit-diag
 *   file CM close is silent; seek/tell/read are pure over the scripted data.
 * - ctypes.byref/c_ulong are unobservable FFI scaffolding (no seam).
 *
 * Ownership: seam results and returned json_t / char pointers are malloc'd,
 * caller frees. Inputs (including seam args) are borrowed. A propagated
 * raise dict is returned upward as-is; a caught raise dict is freed at the
 * catch site.
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
#include "json.h"

/* -- universal seam + trace -- */

typedef struct gw3_trace { json_t *events; } gw3_trace_t;

typedef struct gw3_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gw3_seams_t;

static void gw3_trace_add(gw3_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void gw3_trace_free(gw3_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *gw3_call(const gw3_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *gw3_call0(const gw3_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = gw3_call(s, name, a);
    json_free(a);
    return r;
}

/* -- JSON value helpers -- */

static bool gw3_jtruth(const json_t *v) {
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
static const char *gw3_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool gw3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static char *gw3_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

/* growable byte buffer */
typedef struct { char *p; size_t len, cap; } gw3_buf;
static void gw3_binit(gw3_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void gw3_bfree(gw3_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void gw3_bput(gw3_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->len + n + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void gw3_bputs(gw3_buf *b, const char *s) { gw3_bput(b, s, strlen(s)); }
static void gw3_bputc(gw3_buf *b, char c) { gw3_bput(b, &c, 1); }
static char *gw3_btake(gw3_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* -- exception model -- */

static const char *gw3_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool gw3_is_raise(const json_t *r) { return gw3_raise_class(r) != NULL; }

static json_t *gw3_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

/* FULL builtin exception parent hierarchy (audited against the py3.14.9
 * quarry interpreter's builtins; IOError/EnvironmentError/WindowsError are
 * OSError aliases; ExceptionGroup routes through Exception to keep the
 * except-Exception contract; BaseExceptionGroup stays BaseException-only).
 * Unknown kinds are synthetic Exception subclasses (oracle make_exc). */
typedef struct { const char *name; const char *parent; } gw3_exc_parent;
static const gw3_exc_parent GW3_EXC_PARENTS[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"OSError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"ImportWarning", "Warning"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"NotImplementedError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"UnboundLocalError", "NameError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"JSONDecodeError", "ValueError"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"IOError", "OSError"},
    {"EnvironmentError", "OSError"},
    {"WindowsError", "OSError"},
    {NULL, NULL}
};

/* BaseException-only classes: never caught by except Exception/OSError/... */
static bool gw3_base_only(const char *k) {
    return !strcmp(k, "BaseException") || !strcmp(k, "KeyboardInterrupt") ||
           !strcmp(k, "SystemExit") || !strcmp(k, "GeneratorExit") ||
           !strcmp(k, "BaseExceptionGroup");
}
static bool gw3_is_exception(const json_t *r) {
    const char *k = gw3_raise_class(r);
    return k && !gw3_base_only(k);
}
/* caught(r, cls): cls appears in the kind's ancestry walk */
static bool gw3_caught(const json_t *r, const char *cls) {
    const char *k = gw3_raise_class(r);
    if (!k) return false;
    const char *cur = k;
    for (int depth = 0; cur && depth < 64; depth++) {
        if (!strcmp(cur, cls)) return true;
        if (!strcmp(cur, "BaseException")) return false;
        const char *parent = NULL;
        for (size_t i = 0; GW3_EXC_PARENTS[i].name; i++) {
            if (!strcmp(cur, GW3_EXC_PARENTS[i].name)) {
                parent = GW3_EXC_PARENTS[i].parent;
                break;
            }
        }
        if (!parent) {
            if (cur == k) parent = "Exception";  /* synthetic kind */
            else return false;
        }
        cur = parent;
    }
    return false;
}

/* Python type name of a JSON value (integral numbers read as int) */
static const char *gw3_typename(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && isfinite(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* str(exc) of a raise spec (KeyError kept out of the fixture corpus) */
static const char *gw3_exc_msg(const json_t *r) {
    const json_t *m = (r && r->type == JSON_OBJECT) ? json_obj_get(r, "msg") : NULL;
    return (m && m->type == JSON_STRING && m->str_val) ? m->str_val : "";
}

/* -- args builders -- */

static json_t *gw3_args(void) { return json_object(); }
static void gw3_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void gw3_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void gw3_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void gw3_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* -- Python float repr (shortest round-trip, repr formatting rules) -- */

static char *gw3_float_repr(double v) {
    if (v == 0.0) return strdup("0.0");
    if (!isfinite(v)) return strdup(v > 0 ? "inf" : (isnan(v) ? "nan" : "-inf"));
    char tmp[64];
    int prec;
    for (prec = 0; prec <= 16; prec++) {
        snprintf(tmp, sizeof(tmp), "%.*e", prec, v);
        if (strtod(tmp, NULL) == v) break;
    }
    if (prec > 16) { prec = 16; snprintf(tmp, sizeof(tmp), "%.*e", prec, v); }
    const char *p = tmp;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    char digits[32];
    size_t nd = 0;
    digits[nd++] = *p++;
    if (*p == '.') { p++; while (*p && *p != 'e' && *p != 'E') digits[nd++] = *p++; }
    digits[nd] = '\0';
    int exp = 0;
    if (*p == 'e' || *p == 'E') exp = atoi(p + 1);
    while (nd > 1 && digits[nd - 1] == '0') digits[--nd] = '\0';
    gw3_buf b; gw3_binit(&b);
    if (neg) gw3_bputc(&b, '-');
    if (exp >= -4 && exp < 16) {
        if (exp >= 0) {
            size_t point = (size_t)exp + 1;
            if (nd <= point) {
                gw3_bput(&b, digits, nd);
                for (size_t i = nd; i < point; i++) gw3_bputc(&b, '0');
                gw3_bputs(&b, ".0");
            } else {
                gw3_bput(&b, digits, point);
                gw3_bputc(&b, '.');
                gw3_bputs(&b, digits + point);
            }
        } else {
            gw3_bputs(&b, "0.");
            for (int i = 0; i < -exp - 1; i++) gw3_bputc(&b, '0');
            gw3_bputs(&b, digits);
        }
    } else {
        gw3_bputc(&b, digits[0]);
        if (nd > 1) { gw3_bputc(&b, '.'); gw3_bputs(&b, digits + 1); }
        char ex[16];
        snprintf(ex, sizeof(ex), "e%s%02d", exp < 0 ? "-" : "+", exp < 0 ? -exp : exp);
        gw3_bputs(&b, ex);
    }
    return gw3_btake(&b);
}

/* Python repr of a string (quote choice + escapes) */
static char *gw3_repr_string(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    gw3_buf b; gw3_binit(&b);
    gw3_bputc(&b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { gw3_bputc(&b, '\\'); gw3_bputc(&b, (char)c); }
        else if (c == '\n') gw3_bputs(&b, "\\n");
        else if (c == '\r') gw3_bputs(&b, "\\r");
        else if (c == '\t') gw3_bputs(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char x[8]; snprintf(x, sizeof(x), "\\x%02x", c); gw3_bputs(&b, x); }
        else gw3_bputc(&b, (char)c);
    }
    gw3_bputc(&b, q);
    return gw3_btake(&b);
}

static char *gw3_pyrepr(const json_t *v);
/* Python str() of a JSON value */
static char *gw3_pystr(const json_t *v) {
    if (gw3_is_null(v)) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) {
                if (d == 0.0) return strdup("0");
                return gw3_xsf("%.0f", d);
            }
            return gw3_float_repr(d);
        }
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: return gw3_pyrepr(v);
    }
}
static char *gw3_pyrepr(const json_t *v) {
    if (gw3_is_null(v)) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) {
                if (d == 0.0) return strdup("0");
                return gw3_xsf("%.0f", d);
            }
            return gw3_float_repr(d);
        }
        case JSON_STRING: return gw3_repr_string(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            gw3_buf b; gw3_binit(&b);
            gw3_bputc(&b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) gw3_bputs(&b, ", ");
                char *r = gw3_pyrepr(v->c.items[i]);
                gw3_bputs(&b, r); free(r);
            }
            gw3_bputc(&b, ']');
            return gw3_btake(&b);
        }
        case JSON_OBJECT: {
            gw3_buf b; gw3_binit(&b);
            gw3_bputc(&b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) gw3_bputs(&b, ", ");
                char *k = gw3_repr_string(v->c.keys[i]);
                char *r = gw3_pyrepr(v->c.items[i]);
                gw3_bputs(&b, k); gw3_bputs(&b, ": "); gw3_bputs(&b, r);
                free(k); free(r);
            }
            gw3_bputc(&b, '}');
            return gw3_btake(&b);
        }
        default: return strdup("None");
    }
}

/* repr(exc) for {exc!r}: ClassName('msg') -- scripted excs are cls(msg) */
static char *gw3_exc_repr(const json_t *r) {
    const char *k = gw3_raise_class(r);
    char *m = gw3_repr_string(gw3_exc_msg(r));
    char *out = gw3_xsf("%s(%s)", k ? k : "Exception", m);
    free(m);
    return out;
}

/* int() coercion -> number node, or NULL with *exc set to the raise spec.
 * Messages probe-verified on py3.14.9. Underscores must sit between digits. */
static json_t *gw3_int_value(const json_t *v, json_t **exc) {
    *exc = NULL;
    if (!v || v->type == JSON_NULL || v->type == JSON_ARRAY || v->type == JSON_OBJECT) {
        char *msg = gw3_xsf("int() argument must be a string, a bytes-like object or a real number, not '%s'",
                            gw3_typename(v));
        *exc = gw3_raise_spec("TypeError", msg);
        free(msg);
        return NULL;
    }
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        return json_number(d >= 0 ? floor(d) : ceil(d));
    }
    /* string */
    const char *s = v->str_val ? v->str_val : "";
    const char *p = s;
    /* int() strips White_Space minus \x1c-\x1f; ASCII-scoped set */
    while (*p == ' ' || (*p >= 0x09 && *p <= 0x0d)) p++;
    const char *start = p;
    if (*p == '+' || *p == '-') p++;
    size_t nd = 0;
    bool prev_digit = false;
    while (*p) {
        if (*p >= '0' && *p <= '9') { nd++; prev_digit = true; p++; }
        else if (*p == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') { prev_digit = false; p++; }
        else break;
    }
    const char *dend = p;
    while (*p == ' ' || (*p >= 0x09 && *p <= 0x0d)) p++;
    if (!nd || *p) {
        char *rs = gw3_repr_string(s);
        char *msg = gw3_xsf("invalid literal for int() with base 10: %s", rs);
        free(rs);
        *exc = gw3_raise_spec("ValueError", msg);
        free(msg);
        return NULL;
    }
    gw3_buf clean; gw3_binit(&clean);
    /* carry the consumed sign into the clean buffer (start, not dstart) */
    for (const char *q = start; q < dend; q++) if (*q != '_') gw3_bputc(&clean, *q);
    char *cs = gw3_btake(&clean);
    double d = strtod(cs, NULL);
    free(cs);
    return json_number(d);
}
/* -- POSIX path algebra (pathlib PurePosixPath component model) -- */

/* str(PurePosixPath(p)): drop empty and "." components, keep "..", collapse
 * slash runs (POSIX keeps EXACTLY two leading slashes), strip trailing slash */
static char *gw3_path_norm(const char *p) {
    size_t lead = 0;
    while (p[lead] == '/') lead++;
    bool absolute = lead > 0;
    const char *root = (lead == 2) ? "//" : "/";
    gw3_buf b; gw3_binit(&b);
    if (absolute) gw3_bputs(&b, root);
    size_t i = lead, n = strlen(p);
    bool any = false;
    while (i < n) {
        while (i < n && p[i] == '/') i++;
        size_t start = i;
        while (i < n && p[i] != '/') i++;
        size_t len = i - start;
        if (len == 0) continue;
        if (len == 1 && p[start] == '.') continue;
        if (any) gw3_bputc(&b, '/');
        gw3_bput(&b, p + start, len);
        any = true;
    }
    if (!any) { gw3_bfree(&b); return strdup(absolute ? root : "."); }
    return gw3_btake(&b);
}
static bool gw3_path_is_root(const char *n) {
    return n[0] == '/' && n[strspn(n, "/")] == '\0';
}
static char *gw3_path_name(const char *p) {
    char *n = gw3_path_norm(p);
    if (gw3_path_is_root(n) || !strcmp(n, ".")) { free(n); return strdup(""); }
    const char *slash = strrchr(n, '/');
    char *out = strdup(slash ? slash + 1 : n);
    free(n);
    return out;
}
static char *gw3_path_parent(const char *p) {
    char *n = gw3_path_norm(p);
    if (gw3_path_is_root(n)) return n;          /* root is its own parent */
    if (!strcmp(n, ".")) { free(n); return strdup("."); }
    const char *slash = strrchr(n, '/');
    char *out;
    if (!slash) out = strdup(".");
    else {
        size_t k = (size_t)(slash - n);
        bool prefix_root = true;
        for (size_t j = 0; j < k; j++) if (n[j] != '/') { prefix_root = false; break; }
        if (prefix_root) out = strdup((n[0] == '/' && n[1] == '/') ? "//" : "/");
        else out = strndup(n, k);
    }
    free(n);
    return out;
}
/* PurePosixPath(parent) / name: an absolute name replaces; "." drops */
static char *gw3_path_join(const char *parent, const char *name) {
    if (name[0] == '/') return gw3_path_norm(name);
    char *pn = gw3_path_norm(parent);
    size_t l = strlen(pn);
    char *joined = (l && pn[l - 1] == '/') ? gw3_xsf("%s%s", pn, name)
                                           : gw3_xsf("%s/%s", pn, name);
    char *out = gw3_path_norm(joined);
    free(joined); free(pn);
    return out;
}
static bool gw3_path_is_absolute(const char *p) {
    char *n = gw3_path_norm(p);
    bool abs = n[0] == '/';
    free(n);
    return abs;
}
/* with_suffix: returns NULL on ValueError (empty name / invalid suffix).
 * suffix scan: last dot at index > 0, unless the name is all dots (".."). */
static char *gw3_with_suffix(const char *p, const char *suffix) {
    char *name = gw3_path_name(p);
    if (!name[0]) { free(name); return NULL; }
    if (suffix[0] && suffix[0] != '.') { free(name); return NULL; }
    const char *dot = strrchr(name, '.');
    bool all_dots = name[strspn(name, ".")] == '\0';
    char *stem = (dot && (dot - name) > 0 && !all_dots)
                     ? strndup(name, (size_t)(dot - name)) : strdup(name);
    char *joined = gw3_xsf("%s%s", stem, suffix);
    char *parent = gw3_path_parent(p);
    char *out = gw3_path_join(parent, joined);
    free(name); free(stem); free(joined); free(parent);
    return out;
}

/* -- UTF-8 / UTF-16 / code-page codecs -- */

static void gw3_utf8_put(gw3_buf *b, unsigned int cp) {
    if (cp < 0x80) gw3_bputc(b, (char)cp);
    else if (cp < 0x800) {
        gw3_bputc(b, (char)(0xC0 | (cp >> 6)));
        gw3_bputc(b, (char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        gw3_bputc(b, (char)(0xE0 | (cp >> 12)));
        gw3_bputc(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        gw3_bputc(b, (char)(0x80 | (cp & 0x3F)));
    } else {
        gw3_bputc(b, (char)(0xF0 | (cp >> 18)));
        gw3_bputc(b, (char)(0x80 | ((cp >> 12) & 0x3F)));
        gw3_bputc(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        gw3_bputc(b, (char)(0x80 | (cp & 0x3F)));
    }
}

/* strict utf-8 validation; returns a copy of the bytes when valid, else NULL */
static char *gw3_utf8_strict(const unsigned char *b, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned c = b[i];
        if (c < 0x80) { i++; continue; }
        int need; unsigned lo = 0x80, hi = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) need = 1;
        else if (c >= 0xE0 && c <= 0xEF) need = 2;
        else if (c >= 0xF0 && c <= 0xF4) need = 3;
        else return NULL;
        if (c == 0xE0) lo = 0xA0;
        if (c == 0xED) hi = 0x9F;
        if (c == 0xF0) lo = 0x90;
        if (c == 0xF4) hi = 0x8F;
        if (i + (size_t)need >= n) return NULL;
        for (int k = 0; k < need; k++) {
            unsigned char cc = b[i + 1 + (size_t)k];
            unsigned l = (k == 0) ? lo : 0x80, h = (k == 0) ? hi : 0xBF;
            if (cc < l || cc > h) return NULL;
        }
        i += 1 + (size_t)need;
    }
    return strndup((const char *)b, n);
}

/* utf-8 decode with errors="replace": CPython maximal-subpart rule
 * (probe-verified against live on 30k random byte strings). One U+FFFD per
 * invalid start byte or per truncated/invalid maximal subpart. */
static char *gw3_utf8_replace(const unsigned char *b, size_t n) {
    gw3_buf out; gw3_binit(&out);
    size_t i = 0;
    while (i < n) {
        unsigned c = b[i];
        if (c < 0x80) { gw3_bputc(&out, (char)c); i++; continue; }
        int need; unsigned lo = 0x80, hi = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) need = 1;
        else if (c >= 0xE0 && c <= 0xEF) need = 2;
        else if (c >= 0xF0 && c <= 0xF4) need = 3;
        else { gw3_bputs(&out, "\xef\xbf\xbd"); i++; continue; }
        if (c == 0xE0) lo = 0xA0;
        if (c == 0xED) hi = 0x9F;
        if (c == 0xF0) lo = 0x90;
        if (c == 0xF4) hi = 0x8F;
        size_t j = i + 1;
        bool ok = true;
        for (int k = 0; k < need; k++) {
            if (j >= n) { ok = false; break; }
            unsigned l = (k == 0) ? lo : 0x80, h = (k == 0) ? hi : 0xBF;
            if (b[j] < l || b[j] > h) { ok = false; break; }
            j++;
        }
        if (ok) {
            gw3_bput(&out, (const char *)b + i, j - i);
            i = j;
        } else {
            gw3_bputs(&out, "\xef\xbf\xbd");
            size_t adv = 1;
            while (i + adv < n && adv <= (size_t)need &&
                   b[i + adv] >= 0x80 && b[i + adv] <= 0xBF) {
                unsigned l = (adv == 1) ? lo : 0x80, h = (adv == 1) ? hi : 0xBF;
                if (b[i + adv] < l || b[i + adv] > h) break;
                adv++;
            }
            i += adv;
        }
    }
    return gw3_btake(&out);
}

/* utf-16 decode with BOM (caller gated data[:2] in {FFFE, FEFF});
 * NULL on malformed (odd length / lone surrogate) -> UnicodeDecodeError */
static char *gw3_utf16_decode(const unsigned char *b, size_t n) {
    bool le = (b[0] == 0xFF);
    if ((n - 2) % 2) return NULL;
    gw3_buf out; gw3_binit(&out);
    size_t i = 2;
    while (i < n) {
        unsigned u = le ? ((unsigned)b[i] | ((unsigned)b[i + 1] << 8))
                        : (((unsigned)b[i] << 8) | (unsigned)b[i + 1]);
        i += 2;
        if (u >= 0xD800 && u <= 0xDBFF) {
            if (i >= n) { gw3_bfree(&out); return NULL; }
            unsigned v = le ? ((unsigned)b[i] | ((unsigned)b[i + 1] << 8))
                            : (((unsigned)b[i] << 8) | (unsigned)b[i + 1]);
            if (v < 0xDC00 || v > 0xDFFF) { gw3_bfree(&out); return NULL; }
            i += 2;
            unsigned cp = 0x10000u + ((u - 0xD800u) << 10) + (v - 0xDC00u);
            gw3_utf8_put(&out, cp);
        } else if (u >= 0xDC00 && u <= 0xDFFF) {
            gw3_bfree(&out); return NULL;
        } else {
            gw3_utf8_put(&out, u);
        }
    }
    return gw3_btake(&out);
}

/* code-page tables generated from the LIVE py3.14.9 codecs (-1 = undefined) */
static const int GW3_CP437[256] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
    199, 252, 233, 226, 228, 224, 229, 231, 234, 235, 232, 239, 238, 236, 196, 197,
    201, 230, 198, 244, 246, 242, 251, 249, 255, 214, 220, 162, 163, 165, 8359, 402,
    225, 237, 243, 250, 241, 209, 170, 186, 191, 8976, 172, 189, 188, 161, 171, 187,
    9617, 9618, 9619, 9474, 9508, 9569, 9570, 9558, 9557, 9571, 9553, 9559, 9565, 9564, 9563, 9488,
    9492, 9524, 9516, 9500, 9472, 9532, 9566, 9567, 9562, 9556, 9577, 9574, 9568, 9552, 9580, 9575,
    9576, 9572, 9573, 9561, 9560, 9554, 9555, 9579, 9578, 9496, 9484, 9608, 9604, 9612, 9616, 9600,
    945, 223, 915, 960, 931, 963, 181, 964, 934, 920, 937, 948, 8734, 966, 949, 8745,
    8801, 177, 8805, 8804, 8992, 8993, 247, 8776, 176, 8729, 183, 8730, 8319, 178, 9632, 160,
};
static const int GW3_CP1252[256] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
    8364, -1, 8218, 402, 8222, 8230, 8224, 8225, 710, 8240, 352, 8249, 338, -1, 381, -1,
    -1, 8216, 8217, 8220, 8221, 8226, 8211, 8212, 732, 8482, 353, 8250, 339, -1, 382, 376,
    160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
    176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
    192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
    208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
    224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
};

static char *gw3_charmap_decode(const unsigned char *b, size_t n, const int *tab) {
    gw3_buf out; gw3_binit(&out);
    for (size_t i = 0; i < n; i++) {
        int cp = tab[b[i]];
        if (cp < 0) { gw3_bfree(&out); return NULL; }
        gw3_utf8_put(&out, (unsigned)cp);
    }
    return gw3_btake(&out);
}

/* decode by codec name; NULL models UnicodeDecodeError OR LookupError
 * (both caught by the live loop). Domain: utf-8/cp65001/cp1252/cp437;
 * other cpN -> LookupError (unknown encoding). */
static char *gw3_decode_codec(const unsigned char *b, size_t n, const char *enc) {
    if (!enc) return NULL;
    if (!strcmp(enc, "utf-8") || !strcmp(enc, "utf8") || !strcmp(enc, "cp65001"))
        return gw3_utf8_strict(b, n);
    if (enc[0] == 'c' && enc[1] == 'p' && enc[2]) {
        const char *p = enc + 2;
        long num = 0;
        for (; *p; p++) {
            if (*p < '0' || *p > '9') return NULL;
            num = num * 10 + (*p - '0');
            if (num > 999999) return NULL;
        }
        if (num == 65001) return gw3_utf8_strict(b, n);
        if (num == 1252) return gw3_charmap_decode(b, n, GW3_CP1252);
        if (num == 437) return gw3_charmap_decode(b, n, GW3_CP437);
        return NULL;  /* unknown encoding -> LookupError */
    }
    return NULL;
}

/* str.encode(encoding) -> array of byte ints, or a LookupError raise spec */
static json_t *gw3_encode(const char *content, const char *encoding) {
    bool u16 = false;
    if (!encoding || !strcmp(encoding, "utf-8") || !strcmp(encoding, "utf8")) u16 = false;
    else if (!strcmp(encoding, "utf-16")) u16 = true;
    else {
        char *msg = gw3_xsf("unknown encoding: %s", encoding ? encoding : "");
        json_t *r = gw3_raise_spec("LookupError", msg);
        free(msg);
        return r;
    }
    json_t *out = json_array();
    if (u16) {
        json_append(out, json_number(0xFF));
        json_append(out, json_number(0xFE));
    }
    const unsigned char *p = (const unsigned char *)content;
    while (*p) {
        unsigned cp; int len;
        if (*p < 0x80) { cp = *p; len = 1; }
        else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; len = 2; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; len = 3; }
        else { cp = *p & 0x07; len = 4; }
        for (int k = 1; k < len; k++) cp = (cp << 6) | (p[k] & 0x3F);
        p += len;
        if (!u16) {
            gw3_buf b; gw3_binit(&b);
            gw3_utf8_put(&b, cp);
            for (size_t i = 0; i < b.len; i++) json_append(out, json_number((unsigned char)b.p[i]));
            gw3_bfree(&b);
        } else if (cp < 0x10000) {
            json_append(out, json_number(cp & 0xFF));
            json_append(out, json_number(cp >> 8));
        } else {
            unsigned v = cp - 0x10000;
            unsigned hi = 0xD800 + (v >> 10), lo = 0xDC00 + (v & 0x3FF);
            json_append(out, json_number(hi & 0xFF));
            json_append(out, json_number(hi >> 8));
            json_append(out, json_number(lo & 0xFF));
            json_append(out, json_number(lo >> 8));
        }
    }
    return out;
}

static bool gw3_bytes_equal(const json_t *a, const json_t *b) {
    if (!a || !b || a->type != JSON_ARRAY || b->type != JSON_ARRAY) return false;
    if (a->c.count != b->c.count) return false;
    for (size_t i = 0; i < a->c.count; i++) {
        const json_t *x = a->c.items[i];
        const json_t *y = b->c.items[i];
        if (!x || !y || x->type != JSON_NUMBER || y->type != JSON_NUMBER) return false;
        if (x->num_val != y->num_val) return false;
    }
    return true;
}

/* -- text helpers (ASCII-scoped domains, see header) -- */

static bool gw3_ws_char(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f' || (c >= 0x1c && c <= 0x1f);
}
static bool gw3_stripped_nonempty(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (!gw3_ws_char(*p)) return true;
    return false;
}

/* str.splitlines(): ASCII boundaries \n \r \r\n \v \f \x1c \x1d \x1e (NOT
 * \x1f); non-ASCII boundary codepoints excluded by the fixture domain */
static json_t *gw3_splitlines(const char *s) {
    json_t *out = json_array();
    gw3_buf line; gw3_binit(&line);
    size_t i = 0, n = strlen(s);
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        bool brk = (c == '\n' || c == '\r' || c == 0x0b || c == 0x0c ||
                    (c >= 0x1c && c <= 0x1e));
        if (brk) {
            char *l = gw3_btake(&line);
            json_append(out, json_string(l));
            free(l);
            gw3_binit(&line);
            if (c == '\r' && i + 1 < n && s[i + 1] == '\n') i++;
            i++;
        } else {
            gw3_bputc(&line, (char)c);
            i++;
        }
    }
    if (line.len > 0) {
        char *l = gw3_btake(&line);
        json_append(out, json_string(l));
        free(l);
    }
    gw3_bfree(&line);
    return out;
}

/* s[:limit] by CODEPOINTS (UTF-8); the corpus is ASCII + U+FFFD */
static char *gw3_str_slice(const char *s, size_t limit) {
    size_t cps = 0, i = 0, n = strlen(s);
    while (i < n && cps < limit) {
        i++;
        while (i < n && ((unsigned char)s[i] & 0xC0) == 0x80) i++;
        cps++;
    }
    return strndup(s, i);
}

/* -- seam conveniences -- */

static json_t *gw3_path_exists(const char *path, const gw3_seams_t *s) {
    json_t *a = gw3_args();
    gw3_aset_s(a, "path", path);
    json_t *r = gw3_call(s, "path_exists", a);
    json_free(a);
    return r;
}
static json_t *gw3_path_read_text(const char *path, const char *encoding, const gw3_seams_t *s) {
    json_t *a = gw3_args();
    gw3_aset_s(a, "path", path);
    gw3_aset_s(a, "encoding", encoding);
    json_t *r = gw3_call(s, "path_read_text", a);
    json_free(a);
    return r;
}
static json_t *gw3_path_read_bytes(const char *path, const gw3_seams_t *s) {
    json_t *a = gw3_args();
    gw3_aset_s(a, "path", path);
    json_t *r = gw3_call(s, "path_read_bytes", a);
    json_free(a);
    return r;
}
static json_t *gw3_path_resolve(const char *path, const gw3_seams_t *s) {
    json_t *a = gw3_args();
    gw3_aset_s(a, "path", path);
    json_t *r = gw3_call(s, "path_resolve", a);
    json_free(a);
    return r;
}
static json_t *gw3_normcase(const char *path, const gw3_seams_t *s) {
    json_t *a = gw3_args();
    gw3_aset_s(a, "path", path);
    json_t *r = gw3_call(s, "normcase", a);
    json_free(a);
    return r;
}
static json_t *gw3_json_loads(const json_t *text, const gw3_seams_t *s) {
    json_t *a = gw3_args();
    gw3_aset_j(a, "s", text);
    json_t *r = gw3_call(s, "json_loads", a);
    json_free(a);
    return r;
}

/* Path(x) constructor check: non-str raises the py3.14 TypeError */
static json_t *gw3_path_ctor_raise(const json_t *v) {
    char *msg = gw3_xsf("argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
                        gw3_typename(v));
    json_t *r = gw3_raise_spec("TypeError", msg);
    free(msg);
    return r;
}

/* configured, argv = result (2-tuple unpack); raise spec on shape errors */
static json_t *gw3_unpack2(const json_t *r, const json_t **a0, const json_t **a1) {
    if (!r || r->type != JSON_ARRAY) {
        char *msg = gw3_xsf("cannot unpack non-iterable %s object", gw3_typename(r));
        json_t *e = gw3_raise_spec("TypeError", msg);
        free(msg);
        return e;
    }
    if (r->c.count != 2) {
        const char *kind = r->c.count > 2 ? "too many" : "not enough";
        char *msg = gw3_xsf("%s values to unpack (expected 2, got %zu)", kind, r->c.count);
        json_t *e = gw3_raise_spec("ValueError", msg);
        free(msg);
        return e;
    }
    *a0 = r->c.items[0];
    *a1 = r->c.items[1];
    return NULL;
}
/* -- console encodings / schtasks decode -- */

/* PoP: gw3_windows_console_encodings @ hermes_cli/gateway_windows.py:_windows_console_encodings */
static json_t *gw3_windows_console_encodings(const json_t *kernel32, const gw3_seams_t *s) {
    if (gw3_is_null(kernel32)) {
        json_t *enc = gw3_call0(s, "schtasks_encoding");
        if (gw3_is_raise(enc)) return enc;
        json_t *out = json_array();
        json_append(out, enc ? enc : json_null());
        return out;
    }
    static const struct { const char *getter; const char *seam; } GETTERS[3] = {
        {"GetConsoleOutputCP", "get_console_output_cp"},
        {"GetOEMCP", "get_oem_cp"},
        {"GetACP", "get_acp"},
    };
    json_t *pages = json_array();
    for (int i = 0; i < 3; i++) {
        const json_t *fn = json_obj_get(kernel32, GETTERS[i].getter);
        if (!fn) continue;  /* getattr AttributeError -> caught, skip */
        if (fn->type == JSON_NULL) {
            /* None() -> TypeError, NOT in (AttributeError, OSError, ValueError) */
            json_free(pages);
            return gw3_raise_spec("TypeError", "'NoneType' object is not callable");
        }
        json_t *r = gw3_call0(s, GETTERS[i].seam);
        if (gw3_is_raise(r)) {
            if (gw3_caught(r, "AttributeError") || gw3_caught(r, "OSError") ||
                gw3_caught(r, "ValueError")) { json_free(r); continue; }
            json_free(pages);
            return r;
        }
        json_t *ie = NULL;
        json_t *iv = gw3_int_value(r, &ie);
        json_free(r);
        if (ie) {
            if (gw3_caught(ie, "ValueError")) { json_free(ie); continue; }
            json_free(pages);
            return ie;  /* TypeError propagates */
        }
        double cp = iv->num_val;
        json_free(iv);
        if (cp > 0) {
            char *name = gw3_xsf("cp%.0f", cp);
            bool dup = false;
            for (size_t j = 0; j < pages->c.count; j++) {
                const char *p = gw3_jstr(pages->c.items[j]);
                if (p && !strcmp(p, name)) { dup = true; break; }
            }
            if (!dup) json_append(pages, json_string(name));
            free(name);
        }
    }
    if (pages->c.count == 0) {
        json_free(pages);
        json_t *enc = gw3_call0(s, "schtasks_encoding");
        if (gw3_is_raise(enc)) return enc;
        json_t *out = json_array();
        json_append(out, enc ? enc : json_null());
        return out;
    }
    return pages;
}

/* PoP: gw3_decode_schtasks_output @ hermes_cli/gateway_windows.py:_decode_schtasks_output */
static json_t *gw3_decode_schtasks_output(const json_t *data, const json_t *kernel32,
                                          const gw3_seams_t *s) {
    /* data: array of byte ints (bytes domain) */
    size_t n = (data && data->type == JSON_ARRAY) ? data->c.count : 0;
    unsigned char *b = malloc(n ? n : 1);
    for (size_t i = 0; i < n; i++) {
        const json_t *v = data->c.items[i];
        b[i] = (v && v->type == JSON_NUMBER) ? (unsigned char)(int)v->num_val : 0;
    }
    /* BOM gate: a genuine UTF-16 write decodes as such, never as a code page */
    if (n >= 2 && ((b[0] == 0xFF && b[1] == 0xFE) || (b[0] == 0xFE && b[1] == 0xFF))) {
        char *s16 = gw3_utf16_decode(b, n);
        if (s16) {
            json_t *out = json_string(s16);
            free(s16); free(b);
            return out;
        }
        free(s16);  /* malformed UTF-16 falls through to the same fallbacks */
    }
    /* for encoding in ("utf-8", *_windows_console_encodings()) */
    json_t *encs = gw3_windows_console_encodings(kernel32, s);
    if (gw3_is_raise(encs)) { free(b); return encs; }
    char *out = gw3_utf8_strict(b, n);
    if (out) {
        json_t *r = json_string(out);
        free(out); free(b); json_free(encs);
        return r;
    }
    free(out);
    for (size_t i = 0; i < encs->c.count; i++) {
        const char *enc = gw3_jstr(encs->c.items[i]);
        out = gw3_decode_codec(b, n, enc);
        if (out) {
            json_t *r = json_string(out);
            free(out); free(b); json_free(encs);
            return r;
        }
        free(out);
    }
    json_free(encs);
    out = gw3_utf8_replace(b, n);
    free(b);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* -- service roots / launcher settings -- */

/* PoP: gw3_hermes_service_roots @ hermes_cli/gateway_windows.py:hermes_service_roots */
static json_t *gw3_hermes_service_roots(const json_t *executable, const gw3_seams_t *s) {
    json_t *prs = gw3_call0(s, "project_root_str");
    if (gw3_is_raise(prs)) return prs;
    if (!prs || prs->type != JSON_STRING) {
        json_t *e = gw3_path_ctor_raise(prs);
        json_free(prs);
        return e;
    }
    char *root = gw3_path_norm(prs->str_val);
    json_free(prs);
    /* str(Path(sys.executable).parent) -- Path(None) raises BEFORE hermes_home */
    if (!executable || executable->type != JSON_STRING) {
        free(root);
        return gw3_path_ctor_raise(executable);
    }
    char *enorm = gw3_path_norm(executable->str_val);
    char *eparent = gw3_path_parent(enorm);
    free(enorm);
    /* str(_hermes_home() / "gateway-service") */
    json_t *hh = gw3_call0(s, "hermes_home");
    if (gw3_is_raise(hh)) { free(root); free(eparent); return hh; }
    if (gw3_is_null(hh)) {
        free(root); free(eparent); json_free(hh);
        return gw3_raise_spec("TypeError", "unsupported operand type(s) for /: 'NoneType' and 'str'");
    }
    if (hh->type != JSON_STRING) {
        char *msg = gw3_xsf("unsupported operand type(s) for /: '%s' and 'str'", gw3_typename(hh));
        json_t *e = gw3_raise_spec("TypeError", msg);
        free(msg); free(root); free(eparent); json_free(hh);
        return e;
    }
    char *gs = gw3_path_join(hh->str_val, "gateway-service");
    json_free(hh);
    json_t *out = json_array();
    json_append(out, json_string(root));
    json_append(out, json_string(eparent));
    json_append(out, json_string(gs));
    free(root); free(eparent); free(gs);
    return out;
}

/* PoP: gw3_launcher_settings @ hermes_cli/gateway_windows.py:_launcher_settings */
static json_t *gw3_launcher_settings(const char *home, const gw3_seams_t *s) {
    /* from hermes_cli.gateway import PROJECT_ROOT, _profile_arg, get_python_path */
    json_t *pr = gw3_call0(s, "import_project_root");
    if (gw3_is_raise(pr)) return pr;
    json_t *ia = gw3_call0(s, "import_profile_arg");
    if (gw3_is_raise(ia)) { json_free(pr); return ia; }
    json_free(ia);
    json_t *ig = gw3_call0(s, "import_get_python_path");
    if (gw3_is_raise(ig)) { json_free(pr); return ig; }
    json_free(ig);
    /* hermes_home = str(home if home is not None else _hermes_home()) */
    char *hh = NULL;
    if (home) {
        hh = gw3_path_norm(home);  /* str(Path(home)) */
    } else {
        json_t *hhr = gw3_call0(s, "hermes_home");
        if (gw3_is_raise(hhr)) { json_free(pr); return hhr; }
        hh = gw3_pystr(hhr);  /* str(None) == "None" */
        json_free(hhr);
    }
    /* _preserve_hermes_home_path(get_python_path()) */
    json_t *gp = gw3_call0(s, "get_python_path");
    if (gw3_is_raise(gp)) { json_free(pr); free(hh); return gp; }
    json_t *a1 = gw3_args();
    gw3_aset_j(a1, "value", gp);
    json_t *pp = gw3_call(s, "preserve_hermes_home_path", a1);
    json_free(a1); json_free(gp);
    if (gw3_is_raise(pp)) { json_free(pr); free(hh); return pp; }
    /* _stable_gateway_working_dir(PROJECT_ROOT, home) */
    json_t *a2 = gw3_args();
    gw3_aset_j(a2, "project_root", pr);
    gw3_aset_s(a2, "home", home ? hh : NULL);
    json_t *wd = gw3_call(s, "stable_gateway_working_dir", a2);
    json_free(a2);
    if (gw3_is_raise(wd)) { json_free(pr); free(hh); json_free(pp); return wd; }
    /* _profile_arg(hermes_home) */
    json_t *a3 = gw3_args();
    gw3_aset_s(a3, "hermes_home", hh);
    json_t *pa = gw3_call(s, "profile_arg", a3);
    json_free(a3);
    if (gw3_is_raise(pa)) { json_free(pr); free(hh); json_free(pp); json_free(wd); return pa; }
    json_t *out = json_array();
    json_append(out, pp);
    json_append(out, wd);
    json_append(out, json_string(hh));
    json_append(out, pa);
    json_free(pr); free(hh);
    return out;
}

/* PoP: gw3_launcher_scripts @ hermes_cli/gateway_windows.py:_launcher_scripts */
static json_t *gw3_launcher_scripts(const gw3_seams_t *s) {
    json_t *settings = gw3_launcher_settings(NULL, s);
    if (gw3_is_raise(settings)) return settings;
    json_t *sp = gw3_call0(s, "get_task_script_path");
    if (gw3_is_raise(sp)) { json_free(settings); return sp; }
    json_t *a = gw3_args();
    gw3_aset_j(a, "settings", settings);
    gw3_aset_j(a, "script_path", sp);
    json_t *rl = gw3_call(s, "render_launchers", a);
    json_free(a); json_free(settings); json_free(sp);
    return rl;  /* raise or the triples list, returned as-is */
}

/* -- stdio probes -- */

/* PoP: gw3_stdout_isatty @ hermes_cli/gateway_windows.py:_stdout_isatty */
static json_t *gw3_stdout_isatty(const json_t *stdout_node, const gw3_seams_t *s) {
    if (gw3_is_null(stdout_node)) return json_bool(false);
    if (stdout_node->type == JSON_OBJECT && json_obj_get(stdout_node, "__stdout__")) {
        return gw3_call0(s, "stdout_isatty");  /* result returned as-is */
    }
    char *msg = gw3_xsf("'%s' object has no attribute 'isatty'", gw3_typename(stdout_node));
    json_t *e = gw3_raise_spec("AttributeError", msg);
    free(msg);
    return e;
}

/* PoP: gw3_stdin_console_mode_ok @ hermes_cli/gateway_windows.py:_stdin_console_mode_ok */
static json_t *gw3_stdin_console_mode_ok(const char *platform, const json_t *kernel32,
                                         const gw3_seams_t *s) {
    if (strcmp(platform, "win32")) return NULL;  /* None where the fact does not exist */
    if (gw3_is_null(kernel32)) {
        return gw3_raise_spec("AttributeError", "'NoneType' object has no attribute 'GetStdHandle'");
    }
    json_t *a1 = gw3_args();
    gw3_aset_n(a1, "std_handle", -10);
    json_t *h = gw3_call(s, "get_std_handle", a1);
    json_free(a1);
    if (gw3_is_raise(h)) return h;
    json_t *a2 = gw3_args();
    gw3_aset_j(a2, "handle", h);
    json_t *cm = gw3_call(s, "get_console_mode", a2);
    json_free(a2); json_free(h);
    if (gw3_is_raise(cm)) return cm;
    bool ok = gw3_jtruth(cm);
    json_free(cm);
    return json_bool(ok);
}
/* -- dead-launcher refresh / drift -- */

/* scalar JSON equality for the normcase != compare (str domain, null==null) */
static bool gw3_scalar_eq(const json_t *a, const json_t *b) {
    bool an = gw3_is_null(a), bn = gw3_is_null(b);
    if (an || bn) return an && bn;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
        default: return false;
    }
}

/* str(Path(resolve_seam_result)): PathDouble normalizes str scripts; None -> "None" */
static char *gw3_resolve_str(const json_t *r) {
    if (r && r->type == JSON_STRING) return gw3_path_norm(r->str_val);
    return gw3_pystr(r);
}

/* PoP: gw3_refresh_dead_launcher @ hermes_cli/gateway_windows.py:refresh_dead_launcher */
static json_t *gw3_refresh_dead_launcher(const char *home, const char *task_name,
                                         const gw3_seams_t *s) {
    /* from hermes_cli.gateway_runtime_service_identity import
     * _vbs_identity, launcher_text, read_definition -- BEFORE the vbs build */
    json_t *i1 = gw3_call0(s, "import_vbs_identity");
    if (gw3_is_raise(i1)) return i1;
    json_free(i1);
    json_t *i2 = gw3_call0(s, "import_launcher_text");
    if (gw3_is_raise(i2)) return i2;
    json_free(i2);
    json_t *i3 = gw3_call0(s, "import_read_definition");
    if (gw3_is_raise(i3)) return i3;
    json_free(i3);
    if (!home) {
        return gw3_raise_spec("TypeError", "unsupported operand type(s) for /: 'NoneType' and 'str'");
    }
    char *homen = gw3_path_norm(home);
    /* vbs = home / "gateway-service" / f"{_sanitize_filename(task_name)}.vbs" */
    json_t *a = gw3_args();
    gw3_aset_s(a, "name", task_name);
    json_t *sn = gw3_call(s, "sanitize_filename", a);
    json_free(a);
    if (gw3_is_raise(sn)) { free(homen); return sn; }
    char *sns = gw3_pystr(sn);
    json_free(sn);
    char *fname = gw3_xsf("%s.vbs", sns);
    free(sns);
    char *p1 = gw3_path_join(homen, "gateway-service");
    char *vbs = gw3_path_join(p1, fname);
    free(p1); free(fname);
    /* try: configured, argv = _vbs_identity(launcher_text(read_definition(vbs))) */
    json_t *a1 = gw3_args();
    gw3_aset_s(a1, "path", vbs);
    json_t *rd = gw3_call(s, "read_definition", a1);
    json_free(a1);
    if (gw3_is_raise(rd)) {
        if (gw3_caught(rd, "OSError") || gw3_caught(rd, "ValueError")) {
            json_free(rd); free(vbs); free(homen); return json_bool(false);
        }
        free(vbs); free(homen); return rd;
    }
    json_t *a2 = gw3_args();
    gw3_aset_j(a2, "definition", rd);
    json_t *lt = gw3_call(s, "launcher_text", a2);
    json_free(a2); json_free(rd);
    if (gw3_is_raise(lt)) {
        if (gw3_caught(lt, "OSError") || gw3_caught(lt, "ValueError")) {
            json_free(lt); free(vbs); free(homen); return json_bool(false);
        }
        free(vbs); free(homen); return lt;
    }
    json_t *a3 = gw3_args();
    gw3_aset_j(a3, "text", lt);
    json_t *vi = gw3_call(s, "vbs_identity", a3);
    json_free(a3); json_free(lt);
    if (gw3_is_raise(vi)) {
        if (gw3_caught(vi, "OSError") || gw3_caught(vi, "ValueError")) {
            json_free(vi); free(vbs); free(homen); return json_bool(false);
        }
        free(vbs); free(homen); return vi;
    }
    const json_t *configured_n = NULL;
    const json_t *argv_n = NULL;
    json_t *ue = gw3_unpack2(vi, &configured_n, &argv_n);
    if (ue) {
        if (gw3_caught(ue, "ValueError")) {
            json_free(ue); json_free(vi); free(vbs); free(homen); return json_bool(false);
        }
        json_free(vi); free(vbs); free(homen); return ue;
    }
    /* interpreter = Path(argv[0]) if argv else None -- BETWEEN the two tries */
    char *interp = NULL;
    if (argv_n && argv_n->type == JSON_ARRAY && argv_n->c.count > 0) {
        const json_t *a0 = argv_n->c.items[0];
        if (!a0 || a0->type != JSON_STRING) {
            json_t *e = gw3_path_ctor_raise(a0);
            json_free(vi); free(vbs); free(homen);
            return e;
        }
        interp = gw3_path_norm(a0->str_val);
    }
    /* second try: the provably-dead gate */
    bool early_false = false;
    json_t *esc = NULL;
    if (!interp) early_false = true;
    else if (!gw3_path_is_absolute(interp)) early_false = true;
    else {
        json_t *ex = gw3_path_exists(interp, s);
        if (gw3_is_raise(ex)) {
            if (gw3_caught(ex, "OSError")) { json_free(ex); early_false = true; }
            else esc = ex;
        } else if (gw3_jtruth(ex)) { json_free(ex); early_false = true; }
        else json_free(ex);
    }
    if (!esc && !early_false) {
        if (!configured_n || configured_n->type != JSON_STRING) {
            esc = gw3_path_ctor_raise(configured_n);  /* TypeError: except OSError misses */
        } else {
            json_t *r1 = gw3_path_resolve(configured_n->str_val, s);
            if (gw3_is_raise(r1)) {
                if (gw3_caught(r1, "OSError")) { json_free(r1); early_false = true; }
                else esc = r1;
            } else {
                char *r1s = gw3_resolve_str(r1);
                json_free(r1);
                json_t *n1 = gw3_normcase(r1s, s);
                free(r1s);
                if (gw3_is_raise(n1)) {
                    if (gw3_caught(n1, "OSError")) { json_free(n1); early_false = true; }
                    else esc = n1;
                } else {
                    json_t *r2 = gw3_path_resolve(homen, s);
                    if (gw3_is_raise(r2)) {
                        if (gw3_caught(r2, "OSError")) { json_free(r2); early_false = true; }
                        else esc = r2;
                    } else {
                        char *r2s = gw3_resolve_str(r2);
                        json_free(r2);
                        json_t *n2 = gw3_normcase(r2s, s);
                        free(r2s);
                        if (gw3_is_raise(n2)) {
                            if (gw3_caught(n2, "OSError")) { json_free(n2); early_false = true; }
                            else esc = n2;
                        } else {
                            bool same = gw3_scalar_eq(n1, n2);
                            json_free(n1); json_free(n2);
                            if (!same) early_false = true;  /* != -> return False */
                        }
                    }
                }
            }
        }
    }
    if (esc) { free(interp); json_free(vi); free(vbs); free(homen); return esc; }
    if (early_false) { free(interp); json_free(vi); free(vbs); free(homen); return json_bool(false); }
    /* rewrite loop: _render_launchers(_launcher_settings(Path(configured)),
     * vbs.with_suffix(".cmd")) -- OUTSIDE the per-file try */
    json_t *settings = gw3_launcher_settings(configured_n->str_val, s);
    if (gw3_is_raise(settings)) { free(interp); json_free(vi); free(vbs); free(homen); return settings; }
    char *cmdpath = gw3_with_suffix(vbs, ".cmd");
    if (!cmdpath) {
        json_free(settings); free(interp); json_free(vi); free(vbs); free(homen);
        return gw3_raise_spec("ValueError", "empty name");
    }
    json_t *a4 = gw3_args();
    gw3_aset_j(a4, "settings", settings);
    gw3_aset_s(a4, "script_path", cmdpath);
    json_t *triples = gw3_call(s, "render_launchers", a4);
    json_free(a4); free(cmdpath); json_free(settings);
    if (gw3_is_raise(triples)) { free(interp); json_free(vi); free(vbs); free(homen); return triples; }
    if (!triples || triples->type != JSON_ARRAY) {
        char *msg = gw3_xsf("'%s' object is not iterable", gw3_typename(triples));
        json_t *e = gw3_raise_spec("TypeError", msg);
        free(msg);
        json_free(triples); free(interp); json_free(vi); free(vbs); free(homen);
        return e;
    }
    bool rewritten = false;
    json_t *loop_esc = NULL;
    for (size_t i = 0; i < triples->c.count; i++) {
        const json_t *triple = triples->c.items[i];
        if (!triple || triple->type != JSON_ARRAY || triple->c.count < 3) continue;
        const char *praw = gw3_jstr(json_get(triple, 0));
        const char *content = gw3_jstr(json_get(triple, 1));
        const char *encoding = gw3_jstr(json_get(triple, 2));
        char *path = gw3_path_norm(praw ? praw : "");
        json_t *ex = gw3_path_exists(path, s);
        bool need = false;
        if (gw3_is_raise(ex)) {
            if (gw3_caught(ex, "OSError")) { json_free(ex); free(path); continue; }
            loop_esc = ex; free(path); break;
        }
        if (!gw3_jtruth(ex)) need = true;
        else {
            json_t *rb = gw3_path_read_bytes(path, s);
            if (gw3_is_raise(rb)) {
                if (gw3_caught(rb, "OSError")) { json_free(rb); json_free(ex); free(path); continue; }
                loop_esc = rb; json_free(ex); free(path); break;
            }
            json_t *eb = gw3_encode(content ? content : "", encoding);
            if (gw3_is_raise(eb)) { loop_esc = eb; json_free(rb); json_free(ex); free(path); break; }
            need = !gw3_bytes_equal(rb, eb);
            json_free(rb); json_free(eb);
        }
        json_free(ex);
        if (need) {
            json_t *scripts = json_array();
            json_t *t = json_array();
            json_append(t, json_string(path));
            json_append(t, json_string(content ? content : ""));
            json_append(t, json_string(encoding ? encoding : ""));
            json_append(scripts, t);
            json_t *a5 = gw3_args();
            gw3_aset_j(a5, "scripts", scripts);
            json_free(scripts);
            json_t *wr = gw3_call(s, "write_launcher_scripts", a5);
            json_free(a5);
            if (gw3_is_raise(wr)) {
                if (gw3_caught(wr, "OSError")) { json_free(wr); free(path); continue; }
                loop_esc = wr; free(path); break;
            }
            json_free(wr);
            rewritten = true;
        }
        free(path);
    }
    json_free(triples);
    free(interp); json_free(vi); free(vbs); free(homen);
    if (loop_esc) return loop_esc;
    /* logger.info on rewritten is silent (family convention) */
    return json_bool(rewritten);
}

/* PoP: gw3_launcher_script_drift @ hermes_cli/gateway_windows.py:launcher_script_drift */
static json_t *gw3_launcher_script_drift(const gw3_seams_t *s) {
    json_t *aw = gw3_call0(s, "assert_windows");
    if (gw3_is_raise(aw)) return aw;
    json_free(aw);
    json_t *scripts = gw3_launcher_scripts(s);
    if (gw3_is_raise(scripts)) return scripts;
    if (!scripts || scripts->type != JSON_ARRAY) {
        char *msg = gw3_xsf("'%s' object is not iterable", gw3_typename(scripts));
        json_t *e = gw3_raise_spec("TypeError", msg);
        free(msg);
        json_free(scripts);
        return e;
    }
    json_t *drift = json_array();
    json_t *esc = NULL;
    for (size_t i = 0; i < scripts->c.count; i++) {
        const json_t *triple = scripts->c.items[i];
        if (!triple || triple->type != JSON_ARRAY || triple->c.count < 3) continue;
        const char *praw = gw3_jstr(json_get(triple, 0));
        const char *content = gw3_jstr(json_get(triple, 1));
        const char *encoding = gw3_jstr(json_get(triple, 2));
        char *path = gw3_path_norm(praw ? praw : "");
        json_t *rb = gw3_path_read_bytes(path, s);
        bool current = false;
        if (gw3_is_raise(rb)) {
            if (gw3_caught(rb, "OSError")) { json_free(rb); current = false; }
            else { esc = rb; free(path); break; }
        } else {
            json_t *eb = gw3_encode(content ? content : "", encoding);
            if (gw3_is_raise(eb)) { esc = eb; json_free(rb); free(path); break; }
            current = gw3_bytes_equal(rb, eb);
            json_free(rb); json_free(eb);
        }
        if (!current) {
            char *nm = gw3_path_name(path);
            json_append(drift, json_string(nm));
            free(nm);
        }
        free(path);
    }
    json_free(scripts);
    if (esc) { json_free(drift); return esc; }
    return drift;
}

/* -- deep status probes -- */

/* PoP: gw3_probe @ hermes_cli/gateway_windows.py:_probe */
static json_t *gw3_probe(double index, bool ok, const char *message, const gw3_seams_t *s) {
    char *text = gw3_xsf("  [%.0f] %s  %s", index, ok ? "PASS" : "FAIL", message);
    json_t *a = gw3_args();
    gw3_aset_s(a, "text", text);
    free(text);
    json_t *r = gw3_call(s, "print", a);
    json_free(a);
    if (gw3_is_raise(r)) return r;
    json_free(r);
    return NULL;  /* print statement discards its result */
}

/* PoP: gw3_probe_missing @ hermes_cli/gateway_windows.py:_probe_missing */
static json_t *gw3_probe_missing(double index, const char *path, const char *label,
                                 const gw3_seams_t *s) {
    char *pn = gw3_path_norm(path ? path : "");
    json_t *ex = gw3_path_exists(pn, s);
    if (gw3_is_raise(ex)) { free(pn); return ex; }
    bool present = gw3_jtruth(ex);
    json_free(ex);
    if (present) { free(pn); return json_bool(false); }
    char *msg = gw3_xsf("%s missing: %s", label, pn);
    json_t *pr = gw3_probe(index, false, msg, s);
    free(msg);
    free(pn);
    if (gw3_is_raise(pr)) return pr;
    json_free(pr);
    return json_bool(true);
}

/* PoP: gw3_probe_pid_file @ hermes_cli/gateway_windows.py:_probe_pid_file */
static json_t *gw3_probe_pid_file(const char *pid_path, const gw3_seams_t *s) {
    char *path = gw3_path_norm(pid_path);
    json_t *missing = gw3_probe_missing(1, path, "PID file", s);
    if (gw3_is_raise(missing)) { free(path); return missing; }
    bool m = gw3_jtruth(missing);
    json_free(missing);
    if (m) { free(path); return NULL; }
    /* try block */
    json_t *exc = NULL;
    json_t *ret = NULL;
    json_t *rt = gw3_path_read_text(path, "utf-8-sig", s);
    if (gw3_is_raise(rt)) {
        if (gw3_is_exception(rt)) { exc = rt; goto caught; }
        free(path); return rt;
    }
    json_t *data = gw3_json_loads(rt, s);
    json_free(rt);
    if (gw3_is_raise(data)) {
        if (gw3_is_exception(data)) { exc = data; goto caught; }
        free(path); return data;
    }
    if (!data || data->type != JSON_OBJECT) {
        char *msg = gw3_xsf("'%s' object has no attribute 'get'", gw3_typename(data));
        exc = gw3_raise_spec("AttributeError", msg);
        free(msg);
        json_free(data);
        goto caught;
    }
    {
        const json_t *pidv = json_obj_get(data, "pid");
        json_t *pid_value = NULL;
        if (!gw3_is_null(pidv)) {
            json_t *ie = NULL;
            pid_value = gw3_int_value(pidv, &ie);
            if (ie) {
                json_free(data);
                exc = ie;
                goto caught;
            }
        }
        char *pids = gw3_pystr(pid_value);
        char *msg = gw3_xsf("PID file present: %s (pid=%s)", path, pids);
        free(pids);
        json_t *pr = gw3_probe(1, true, msg, s);
        free(msg);
        if (gw3_is_raise(pr)) {
            json_free(data); json_free(pid_value);
            if (gw3_is_exception(pr)) { exc = pr; goto caught; }
            free(path); return pr;
        }
        json_free(pr);
        json_free(data);
        ret = pid_value;  /* ownership transferred (NULL == None) */
    }
    free(path);
    return ret;
caught:
    {
        char *msg2 = gw3_xsf("PID file present but unreadable: %s", gw3_exc_msg(exc));
        json_t *pr2 = gw3_probe(1, false, msg2, s);
        free(msg2);
        json_free(exc);
        free(path);
        if (gw3_is_raise(pr2)) return pr2;
        json_free(pr2);
    }
    return NULL;
}

/* PoP: gw3_probe_lock_file @ hermes_cli/gateway_windows.py:_probe_lock_file */
static json_t *gw3_probe_lock_file(const char *lock_path, const gw3_seams_t *s) {
    char *path = gw3_path_norm(lock_path);
    json_t *missing = gw3_probe_missing(2, path, "Lock file", s);
    if (gw3_is_raise(missing)) { free(path); return missing; }
    bool m = gw3_jtruth(missing);
    json_free(missing);
    if (m) { free(path); return NULL; }
    json_t *exc = NULL;
    /* try: from gateway.status import is_gateway_runtime_lock_active */
    json_t *imp = gw3_call0(s, "import_is_gateway_runtime_lock_active");
    if (gw3_is_raise(imp)) {
        if (gw3_is_exception(imp)) { exc = imp; goto caught; }
        free(path); return imp;
    }
    json_free(imp);
    json_t *a = gw3_args();
    gw3_aset_s(a, "lock_path", path);
    json_t *held = gw3_call(s, "is_gateway_runtime_lock_active", a);
    json_free(a);
    if (gw3_is_raise(held)) {
        if (gw3_is_exception(held)) { exc = held; goto caught; }
        free(path); return held;
    }
    {
        char *msg = gw3_xsf("Lock file held by a live process: %s", path);
        json_t *pr = gw3_probe(2, gw3_jtruth(held), msg, s);
        free(msg);
        json_free(held);
        if (gw3_is_raise(pr)) {
            if (gw3_is_exception(pr)) { exc = pr; goto caught; }
            free(path); return pr;
        }
        json_free(pr);
    }
    free(path);
    return NULL;
caught:
    {
        char *msg2 = gw3_xsf("Could not probe lock: %s", gw3_exc_msg(exc));
        json_t *pr2 = gw3_probe(2, false, msg2, s);
        free(msg2);
        json_free(exc);
        free(path);
        if (gw3_is_raise(pr2)) return pr2;
        json_free(pr2);
    }
    return NULL;
}

/* PoP: gw3_probe_running_pid @ hermes_cli/gateway_windows.py:_probe_running_pid */
static json_t *gw3_probe_running_pid(const gw3_seams_t *s) {
    json_t *exc = NULL;
    json_t *imp = gw3_call0(s, "import_get_running_pid");
    if (gw3_is_raise(imp)) {
        if (gw3_is_exception(imp)) { exc = imp; goto caught; }
        return imp;
    }
    json_free(imp);
    json_t *a = gw3_args();
    gw3_aset_b(a, "cleanup_stale", false);
    json_t *r = gw3_call(s, "get_running_pid", a);
    json_free(a);
    if (gw3_is_raise(r)) {
        if (gw3_is_exception(r)) { exc = r; goto caught; }
        return r;
    }
    {
        char *rs = gw3_pystr(r);
        char *msg = gw3_xsf("get_running_pid() => %s", rs);
        free(rs);
        json_t *pr = gw3_probe(3, !gw3_is_null(r), msg, s);
        free(msg);
        if (gw3_is_raise(pr)) {
            json_free(r);
            if (gw3_is_exception(pr)) { exc = pr; goto caught; }
            return pr;
        }
        json_free(pr);
    }
    return r;  /* ownership transferred */
caught:
    {
        char *er = gw3_exc_repr(exc);
        char *msg2 = gw3_xsf("get_running_pid() raised: %s", er);
        free(er);
        json_t *pr2 = gw3_probe(3, false, msg2, s);
        free(msg2);
        json_free(exc);
        if (gw3_is_raise(pr2)) return pr2;
        json_free(pr2);
    }
    return NULL;
}

/* PoP: gw3_probe_pid_exists @ hermes_cli/gateway_windows.py:_probe_pid_exists */
static json_t *gw3_probe_pid_exists(const json_t *candidate_pid, const gw3_seams_t *s) {
    if (gw3_is_null(candidate_pid)) {
        json_t *pr = gw3_probe(4, false, "No candidate PID to verify", s);
        if (gw3_is_raise(pr)) return pr;
        json_free(pr);
        return NULL;
    }
    json_t *exc = NULL;
    json_t *imp = gw3_call0(s, "import_pid_exists");
    if (gw3_is_raise(imp)) {
        if (gw3_is_exception(imp)) { exc = imp; goto caught; }
        return imp;
    }
    json_free(imp);
    json_t *a = gw3_args();
    gw3_aset_j(a, "pid", candidate_pid);
    json_t *al = gw3_call(s, "pid_exists", a);
    json_free(a);
    if (gw3_is_raise(al)) {
        if (gw3_is_exception(al)) { exc = al; goto caught; }
        return al;
    }
    {
        bool alive = gw3_jtruth(al);
        json_free(al);
        char *cs = gw3_pystr(candidate_pid);
        char *msg = gw3_xsf("_pid_exists(%s) => %s", cs, alive ? "True" : "False");
        free(cs);
        json_t *pr = gw3_probe(4, alive, msg, s);
        free(msg);
        if (gw3_is_raise(pr)) {
            if (gw3_is_exception(pr)) { exc = pr; goto caught; }
            return pr;
        }
        json_free(pr);
    }
    return NULL;
caught:
    {
        char *er = gw3_exc_repr(exc);
        char *msg2 = gw3_xsf("_pid_exists raised: %s", er);
        free(er);
        json_t *pr2 = gw3_probe(4, false, msg2, s);
        free(msg2);
        json_free(exc);
        if (gw3_is_raise(pr2)) return pr2;
        json_free(pr2);
    }
    return NULL;
}

/* PoP: gw3_probe_state_file @ hermes_cli/gateway_windows.py:_probe_state_file */
static json_t *gw3_probe_state_file(const char *state_path, const gw3_seams_t *s) {
    char *path = gw3_path_norm(state_path);
    json_t *missing = gw3_probe_missing(5, path, "gateway_state.json", s);
    if (gw3_is_raise(missing)) { free(path); return missing; }
    bool m = gw3_jtruth(missing);
    json_free(missing);
    if (m) { free(path); return NULL; }
    json_t *exc = NULL;
    json_t *rt = gw3_path_read_text(path, "utf-8-sig", s);
    if (gw3_is_raise(rt)) {
        if (gw3_is_exception(rt)) { exc = rt; goto caught; }
        free(path); return rt;
    }
    json_t *data = gw3_json_loads(rt, s);
    json_free(rt);
    if (gw3_is_raise(data)) {
        if (gw3_is_exception(data)) { exc = data; goto caught; }
        free(path); return data;
    }
    if (!data || data->type != JSON_OBJECT) {
        char *msg = gw3_xsf("'%s' object has no attribute 'get'", gw3_typename(data));
        exc = gw3_raise_spec("AttributeError", msg);
        free(msg);
        json_free(data);
        goto caught;
    }
    {
        const json_t *gs = json_obj_get(data, "gateway_state");
        const json_t *ua = json_obj_get(data, "updated_at");
        char *age_str = strdup("");
        if (gw3_jtruth(ua)) {
            /* inner try: age computation; except Exception: pass */
            json_t *a2 = gw3_args();
            gw3_aset_j(a2, "s", ua);
            json_t *dt = gw3_call(s, "datetime_fromisoformat", a2);
            json_free(a2);
            if (gw3_is_raise(dt)) {
                if (!gw3_is_exception(dt)) {
                    /* BaseException passes the inner AND outer except Exception */
                    /* BaseException passes the inner AND outer except Exception;
                     * dt ownership transfers to the harness (do NOT free) */
                    json_free(data); free(age_str); free(path);
                    return dt;
                }
                json_free(dt);  /* inner swallow: age_str stays "" */
            } else if (!dt || dt->type != JSON_OBJECT) {
                json_free(dt);  /* malformed double -> inner TypeError swallow */
            } else {
                json_t *now = gw3_call0(s, "datetime_now");
                if (gw3_is_raise(now)) {
                    if (!gw3_is_exception(now)) {
                        /* now ownership transfers to the harness (do NOT free) */
                        json_free(dt); json_free(data); free(age_str); free(path);
                        return now;
                    }
                    json_free(now);
                } else {
                    bool aware = json_get_bool(dt, "aware", false);
                    double epoch = json_get_num(dt, "epoch", 0);
                    /* now(UTC) is aware; naive updated_dt -> TypeError (inner caught) */
                    if (aware && now && now->type == JSON_NUMBER) {
                        double diff = now->num_val - epoch;
                        double age = diff >= 0 ? floor(diff) : ceil(diff);
                        free(age_str);
                        age_str = gw3_xsf(" (updated %.0fs ago)", age);
                    }
                    json_free(now);
                }
                json_free(dt);
            }
        }
        bool ok = gs && gs->type == JSON_STRING &&
                  (!strcmp(gs->str_val, "running") || !strcmp(gs->str_val, "degraded"));
        char *gsr = gw3_pyrepr(gs);
        char *msg = gw3_xsf("gateway_state.json state=%s%s", gsr, age_str);
        free(gsr); free(age_str);
        json_t *pr = gw3_probe(5, ok, msg, s);
        free(msg);
        json_free(data);
        if (gw3_is_raise(pr)) {
            if (gw3_is_exception(pr)) { exc = pr; goto caught; }
            free(path); return pr;
        }
        json_free(pr);
    }
    free(path);
    return NULL;
caught:
    {
        char *msg2 = gw3_xsf("gateway_state.json present but unreadable: %s", gw3_exc_msg(exc));
        json_t *pr2 = gw3_probe(5, false, msg2, s);
        free(msg2);
        json_free(exc);
        free(path);
        if (gw3_is_raise(pr2)) return pr2;
        json_free(pr2);
    }
    return NULL;
}

/* PoP: gw3_probe_exit_diag @ hermes_cli/gateway_windows.py:_probe_exit_diag */
static json_t *gw3_probe_exit_diag(const char *diag_path, const gw3_seams_t *s) {
    char *path = gw3_path_norm(diag_path);
    json_t *missing = gw3_probe_missing(6, path, "exit-diag log", s);
    if (gw3_is_raise(missing)) { free(path); return missing; }
    bool m = gw3_jtruth(missing);
    json_free(missing);
    if (m) { free(path); return NULL; }
    json_t *exc = NULL;
    /* outer try: with open(diag_path, "rb") as fh: ... */
    json_t *a = gw3_args();
    gw3_aset_s(a, "path", path);
    gw3_aset_s(a, "mode", "rb");
    json_t *fr = gw3_call(s, "open", a);
    json_free(a);
    if (gw3_is_raise(fr)) {
        if (gw3_is_exception(fr)) { exc = fr; goto outer_caught; }
        free(path); return fr;
    }
    if (gw3_is_null(fr)) {
        /* None["data"] in the file-double materialization */
        exc = gw3_raise_spec("TypeError", "'NoneType' object is not subscriptable");
        json_free(fr);
        goto outer_caught;
    }
    {
        const json_t *fdata = json_obj_get(fr, "data");
        if (fdata && fdata->type == JSON_NULL) {
            /* bytes(None) in the file-double materialization */
            exc = gw3_raise_spec("TypeError", "cannot convert 'NoneType' object to bytes");
            json_free(fr);
            goto outer_caught;
        }
        size_t size = (fdata && fdata->type == JSON_ARRAY) ? fdata->c.count : 0;
        size_t pos = size > 4096 ? size - 4096 : 0;
        unsigned char *b = malloc(size ? size : 1);
        for (size_t i = 0; i < size; i++) {
            const json_t *v = fdata->c.items[i];
            b[i] = (v && v->type == JSON_NUMBER) ? (unsigned char)(int)v->num_val : 0;
        }
        json_free(fr);
        char *text = gw3_utf8_replace(b + pos, size - pos);
        free(b);
        json_t *lines = gw3_splitlines(text);
        free(text);
        /* last_event = next((ln for ln in reversed(tail) if ln.strip()), "") */
        char *last = NULL;
        for (size_t i = lines->c.count; i > 0; i--) {
            const char *ln = gw3_jstr(lines->c.items[i - 1]);
            if (ln && gw3_stripped_nonempty(ln)) { last = strdup(ln); break; }
        }
        json_free(lines);
        if (!last) {
            char *msg = gw3_xsf("exit-diag log empty: %s", path);
            json_t *pr = gw3_probe(6, false, msg, s);
            free(msg);
            if (gw3_is_raise(pr)) {
                if (gw3_is_exception(pr)) { exc = pr; goto outer_caught; }
                free(path); return pr;
            }
            json_free(pr);
            free(path);
            return NULL;
        }
        /* inner try: event = json.loads(last_event) */
        json_t *lastn = json_string(last);
        json_t *ev = gw3_json_loads(lastn, s);
        json_free(lastn);
        if (gw3_is_raise(ev)) {
            if (gw3_is_exception(ev)) { json_free(ev); goto inner_caught; }
            free(last); free(path); return ev;
        }
        if (!ev || ev->type != JSON_OBJECT) {
            char *msg = gw3_xsf("'%s' object has no attribute 'get'", gw3_typename(ev));
            json_free(ev);
            exc = gw3_raise_spec("AttributeError", msg);
            free(msg);
            goto inner_caught;
        }
        {
            const json_t *tag = json_obj_get(ev, "tag");
            const json_t *pid = json_obj_get(ev, "pid");
            const json_t *ts = json_obj_get(ev, "ts");
            char *tags = tag ? gw3_pystr(tag) : strdup("?");
            char *pids = pid ? gw3_pystr(pid) : strdup("?");
            char *tss = ts ? gw3_pystr(ts) : strdup("?");
            bool ok = tag && tag->type == JSON_STRING && !strcmp(tag->str_val, "gateway.start");
            char *msg = gw3_xsf("Last lifecycle event: tag=%s pid=%s ts=%s", tags, pids, tss);
            free(tags); free(pids); free(tss);
            json_t *pr = gw3_probe(6, ok, msg, s);
            free(msg);
            json_free(ev);
            if (gw3_is_raise(pr)) {
                if (gw3_is_exception(pr)) { exc = pr; goto inner_caught; }
                free(last); free(path); return pr;
            }
            json_free(pr);
        }
        free(last);
        free(path);
        return NULL;
inner_caught:
        {
            char *slice = gw3_str_slice(last, 120);
            char *msg2 = gw3_xsf("Last lifecycle line not JSON: %s", slice);
            free(slice);
            free(last);
            json_t *pr2 = gw3_probe(6, false, msg2, s);
            free(msg2);
            if (gw3_is_raise(pr2)) {
                if (gw3_is_exception(pr2)) { exc = pr2; goto outer_caught; }
                free(path); return pr2;
            }
            json_free(pr2);
        }
        free(path);
        return NULL;
    }
outer_caught:
    {
        char *msg3 = gw3_xsf("exit-diag log unreadable: %s", gw3_exc_msg(exc));
        json_t *pr3 = gw3_probe(6, false, msg3, s);
        free(msg3);
        json_free(exc);
        free(path);
        if (gw3_is_raise(pr3)) return pr3;
        json_free(pr3);
    }
    return NULL;
}