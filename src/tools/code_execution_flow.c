/* code_execution_flow.c — FLOW + leaf port of the execute_code surface.
 *
 * The quarry has NO tools/code_execution.py: the code-execution surface is the
 * tools/code_execution_{tool,rpc,env}.py family. This port covers 20 UNCLAIMED
 * defs across those three real modules (PoP annotations name the DEFINING
 * module, per the scanner contract). Claimed siblings (_truncate_stdout_text,
 * _spill_full_stdout, _env_temp_dir, _finish_remote_kernel_result, ...) and
 * every foreign sub-call (json.loads, time.*, env.execute, logger, strip_ansi,
 * redact_code_output, the retry callable) are INJECTED through the ce_io_t
 * universal seam: io->call(io, name, args) appends {"name","args"} to io->trace
 * and returns the scripted result (NULL/json_null == None;
 * {"__raise__":cls,"message":msg} == a raised exception). Unclaimed siblings
 * that are themselves ports here (_sandbox_tools_for, _sanitize_output_text,
 * _clean_output, _with_timeout_notice, _error_result, _execute_checked) are
 * seamed when composed and driven REAL by their own direct cases.
 *
 * Ported targets (20):
 *   tools/code_execution_env.py:  is_secret_env_name, _cache_probe_result
 *   tools/code_execution_tool.py: _sandbox_tools_for,
 *       _missing_hermes_tools_import_hint, json_parse, shell_quote, retry,
 *       _sanitize_output_text, _clean_output, _with_timeout_notice,
 *       _error_result, _remote_failure, _remote_result, _apply_timeout
 *   tools/code_execution_rpc.py:  _private_dirs_cmd, _execute_checked,
 *       _remote_write, _rpc_token_ok, _result_error, tool_errors_since
 *
 * Representation contract (shared with tests/oracle_code_execution_flow.py):
 *  - Pure stdlib algorithms (shlex.quote, base64.b64encode, secrets.compare_digest,
 *    str/repr/slice/sort/round) are TRANSCRIBED here; the oracle uses the real
 *    stdlib. I/O-ish stdlib + quarry collaborators are SEAMED.
 *  - json.loads is seamed ("json_loads", args {text, strict}); json.dumps stays
 *    REAL on the oracle side and is transcribed as ce_py_dumps (ensure_ascii=False,
 *    default (', ', ': ') separators) for _error_result.
 *  - Set-valued returns (_sandbox_tools_for) normalize to a SORTED string array
 *    on both sides. Tuple returns (_clean_output) normalize to a 2-element array.
 *  - Integral floats normalize to ints on both sides; fixtures avoid integral-float
 *    JSON literals in str()/repr()/json.dumps float positions (duration_seconds is
 *    kept NON-integral) and non-finite floats.
 *  - ASCII scope: upper/strip/len/slice model ASCII (fixtures ASCII). The single
 *    non-ASCII glyph below (U+23F0 in _with_timeout_notice) is a VERBATIM UTF-8
 *    constant copied from code_execution_tool.py (libjson serializes bytes >= 0x20
 *    as-is); both sides emit the identical bytes.
 *  - Mutable inputs (_cache_probe_result cache, _apply_timeout result) ride
 *    io->state and are echoed even on a raise (partial mutation history).
 *  - A re.Match double is {"group1": <str>}; m.group(1) reads that field.
 *  - An exception input (_remote_failure exc) is {"__raise__":cls,"message":msg};
 *    type(exc).__name__ == cls, str(exc) == msg.
 *
 * Seam name table (name -> live call):
 *   json_loads            json.loads(text, strict=...)
 *   strip_ansi            tools.ansi_strip.strip_ansi(text)
 *   redact_code_output    agent.redact.redact_code_output(text)
 *   fn                    retry's callable fn()
 *   time.sleep            time.sleep(seconds)
 *   time.monotonic        time.monotonic()
 *   env.execute           env.execute(cmd, cwd=..., timeout=..., **kwargs)
 *   logger.error          logger.error(fmt, *args, exc_info=...)
 *   execute_checked       _execute_checked(env, cmd, what, timeout=..., **kw)
 *   clean_output          _clean_output(stdout_text, env)
 *   sanitize_output_text  _sanitize_output_text(text)
 *   truncate_stdout_text  _truncate_stdout_text(stdout_text, env)  [CLAIMED]
 *   sandbox_tools_for     _sandbox_tools_for(enabled_tools)
 *   with_timeout_notice   _with_timeout_notice(stdout_text, timeout_msg)
 *   error_result          _error_result(error, tool_calls_made=..., duration=..., user_summary=...)
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state are
 * harness-owned. ce_call copies args into the trace; the caller frees args.
 */
#ifndef CODE_EXECUTION_FLOW_C
#define CODE_EXECUTION_FLOW_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

typedef struct ce_io_t {
    json_t *(*call)(struct ce_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} ce_io_t;

/* ── universal seam ────────────────────────────────────────────────────── */
static json_t *ce_call(ce_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    if (io && io->trace && io->trace->type == JSON_ARRAY) json_append(io->trace, ev);
    else json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : NULL;
    return r ? r : json_null();
}

static bool ce_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *ce_raise(const char *cls, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", cls);
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
}

/* Full builtin exception parent table (single representative parent that
 * preserves Exception-ancestry). BaseException-only kinds never reach Exception,
 * so they pass through `except Exception` sites (retry, _result_error). */
typedef struct { const char *name; const char *parent; } ce_exc_parent_t;
static const ce_exc_parent_t CE_EXC_PARENTS[] = {
    {"BaseException", ""},
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"EnvironmentError", "Exception"},
    {"IOError", "Exception"},
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
    {"WindowsError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"UnboundLocalError", "NameError"},
    {"IndentationError", "SyntaxError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
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
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {NULL, NULL}
};

static const char *ce_exc_parent(const char *name) {
    for (int i = 0; CE_EXC_PARENTS[i].name; i++)
        if (strcmp(CE_EXC_PARENTS[i].name, name) == 0) return CE_EXC_PARENTS[i].parent;
    return NULL; /* unknown/custom kind */
}

/* True when the raised kind is an Exception subclass (caught by `except
 * Exception`). Unknown/custom kinds default to Exception (make_exc builds
 * type(kind,(Exception,))). The BaseException-only trio (+CancelledError,
 * BaseExceptionGroup) return false and propagate. */
static bool ce_is_exception(const json_t *r) {
    if (!ce_raised(r)) return false;
    json_t *k = json_obj_get(r, "__raise__");
    if (!k || k->type != JSON_STRING) return false;
    const char *s = k->str_val ? k->str_val : "";
    for (int depth = 0; depth < 64; depth++) {
        if (strcmp(s, "Exception") == 0) return true;
        if (strcmp(s, "BaseException") == 0 || s[0] == '\0') return false;
        const char *p = ce_exc_parent(s);
        if (!p) return true; /* custom Exception subclass */
        s = p;
    }
    return true;
}

/* ── truthiness / type predicates ──────────────────────────────────────── */
static bool jtruth(const json_t *v) {
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
static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool j_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static json_t *ce_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}
static const char *ce_get_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = ce_get(obj, key);
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return def;
}
static double ce_get_num(const json_t *obj, const char *key, double def) {
    json_t *v = ce_get(obj, key);
    if (v && v->type == JSON_NUMBER) return v->num_val;
    return def;
}
static const char *sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* Python type name of a JSON value (integral number == int). */
static const char *ce_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (isfinite(d) && d == floor(d) && fabs(d) < 1e15) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* ── float repr (shortest round-trip, Python repr formatting) ──────────── */
static char *ce_float_repr(double x) {
    char buf[64];
    if (!isfinite(x)) return strdup(x != x ? "nan" : (x > 0 ? "inf" : "-inf"));
    /* shortest %.*e precision that round-trips */
    int p;
    char sci[64];
    for (p = 1; p <= 17; p++) {
        snprintf(sci, sizeof sci, "%.*e", p - 1, x);
        if (strtod(sci, NULL) == x) break;
    }
    if (p > 17) p = 17;
    /* parse exponent */
    char *epos = strchr(sci, 'e');
    int exp = epos ? atoi(epos + 1) : 0;
    if (exp >= -4 && exp < 16) {
        int dec = p - 1 - exp;
        if (dec < 0) dec = 0;
        snprintf(buf, sizeof buf, "%.*f", dec, x);
        return strdup(buf);
    }
    /* scientific: mantissa without trailing zeros, exponent sign + >=2 digits */
    snprintf(sci, sizeof sci, "%.*e", p - 1, x);
    epos = strchr(sci, 'e');
    *epos = '\0';
    /* strip trailing zeros in mantissa fractional part */
    char *dot = strchr(sci, '.');
    if (dot) {
        size_t n = strlen(sci);
        while (n > 0 && sci[n - 1] == '0') sci[--n] = '\0';
        if (n > 0 && sci[n - 1] == '.') sci[--n] = '\0';
    }
    int e = atoi(epos + 1);
    snprintf(buf, sizeof buf, "%se%c%02d", sci, e < 0 ? '-' : '+', e < 0 ? -e : e);
    return strdup(buf);
}

static char *ce_py_repr(const json_t *v);
static void ce_buf_append(char **out, size_t *cap, size_t *len, const char *s);

/* str() of a JSON scalar (containers handled by ce_py_repr). */
static char *ce_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (isfinite(x) && x == floor(x) && fabs(x) < 1e16) {
            snprintf(buf, sizeof buf, "%.0f", x);
            return strdup(buf);
        }
        return ce_float_repr(x);
    }
    return ce_py_repr(v);
}

/* ── repr() (Python) for scalars / str / dict / list ───────────────────── */
static void ce_repr_str_into(const char *s, char **out, size_t *cap, size_t *len);
static void ce_buf_append(char **out, size_t *cap, size_t *len, const char *s) {
    size_t n = strlen(s);
    if (*len + n + 1 > *cap) {
        while (*len + n + 1 > *cap) *cap *= 2;
        *out = realloc(*out, *cap);
    }
    memcpy(*out + *len, s, n);
    *len += n;
    (*out)[*len] = '\0';
}
static void ce_repr_str_into(const char *s, char **out, size_t *cap, size_t *len) {
    /* choose quote: single unless contains ' and not " */
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    char tmp[2] = {q, '\0'};
    ce_buf_append(out, cap, len, tmp);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        char esc[8];
        if (c == (unsigned char)q || c == '\\') {
            esc[0] = '\\'; esc[1] = (char)c; esc[2] = '\0';
            ce_buf_append(out, cap, len, esc);
        } else if (c == '\n') ce_buf_append(out, cap, len, "\\n");
        else if (c == '\r') ce_buf_append(out, cap, len, "\\r");
        else if (c == '\t') ce_buf_append(out, cap, len, "\\t");
        else if (c < 0x20) {
            snprintf(esc, sizeof esc, "\\x%02x", c);
            ce_buf_append(out, cap, len, esc);
        } else {
            char ch[2] = {(char)c, '\0'};
            ce_buf_append(out, cap, len, ch);
        }
    }
    ce_buf_append(out, cap, len, tmp);
}
static void ce_py_repr_into(const json_t *v, char **out, size_t *cap, size_t *len) {
    char buf[64];
    if (!v || v->type == JSON_NULL) { ce_buf_append(out, cap, len, "None"); return; }
    if (v->type == JSON_BOOL) { ce_buf_append(out, cap, len, v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (isfinite(x) && x == floor(x) && fabs(x) < 1e16) {
            snprintf(buf, sizeof buf, "%.0f", x);
            ce_buf_append(out, cap, len, buf);
        } else {
            char *r = ce_float_repr(x);
            ce_buf_append(out, cap, len, r);
            free(r);
        }
        return;
    }
    if (v->type == JSON_STRING) { ce_repr_str_into(sval(v), out, cap, len); return; }
    if (v->type == JSON_ARRAY) {
        ce_buf_append(out, cap, len, "[");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) ce_buf_append(out, cap, len, ", ");
            ce_py_repr_into(v->c.items[i], out, cap, len);
        }
        ce_buf_append(out, cap, len, "]");
        return;
    }
    if (v->type == JSON_OBJECT) {
        ce_buf_append(out, cap, len, "{");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) ce_buf_append(out, cap, len, ", ");
            ce_repr_str_into(v->c.keys[i], out, cap, len);
            ce_buf_append(out, cap, len, ": ");
            ce_py_repr_into(v->c.items[i], out, cap, len);
        }
        ce_buf_append(out, cap, len, "}");
        return;
    }
}
static char *ce_py_repr(const json_t *v) {
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    out[0] = '\0';
    ce_py_repr_into(v, &out, &cap, &len);
    return out;
}

/* ── json.dumps(ensure_ascii=False, default separators) ────────────────── */
static void ce_json_escape_into(const char *s, char **out, size_t *cap, size_t *len) {
    ce_buf_append(out, cap, len, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        char esc[8];
        switch (c) {
            case '"': ce_buf_append(out, cap, len, "\\\""); break;
            case '\\': ce_buf_append(out, cap, len, "\\\\"); break;
            case '\n': ce_buf_append(out, cap, len, "\\n"); break;
            case '\r': ce_buf_append(out, cap, len, "\\r"); break;
            case '\t': ce_buf_append(out, cap, len, "\\t"); break;
            case '\b': ce_buf_append(out, cap, len, "\\b"); break;
            case '\f': ce_buf_append(out, cap, len, "\\f"); break;
            default:
                if (c < 0x20) {
                    snprintf(esc, sizeof esc, "\\u%04x", c);
                    ce_buf_append(out, cap, len, esc);
                } else {
                    char ch[2] = {(char)c, '\0'};
                    ce_buf_append(out, cap, len, ch);
                }
        }
    }
    ce_buf_append(out, cap, len, "\"");
}
static void ce_py_dumps_into(const json_t *v, char **out, size_t *cap, size_t *len) {
    char buf[64];
    if (!v || v->type == JSON_NULL) { ce_buf_append(out, cap, len, "null"); return; }
    if (v->type == JSON_BOOL) { ce_buf_append(out, cap, len, v->bool_val ? "true" : "false"); return; }
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (isfinite(x) && x == floor(x) && fabs(x) < 1e16) {
            snprintf(buf, sizeof buf, "%.0f", x);
            ce_buf_append(out, cap, len, buf);
        } else {
            char *r = ce_float_repr(x);
            ce_buf_append(out, cap, len, r);
            free(r);
        }
        return;
    }
    if (v->type == JSON_STRING) { ce_json_escape_into(sval(v), out, cap, len); return; }
    if (v->type == JSON_ARRAY) {
        ce_buf_append(out, cap, len, "[");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) ce_buf_append(out, cap, len, ", ");
            ce_py_dumps_into(v->c.items[i], out, cap, len);
        }
        ce_buf_append(out, cap, len, "]");
        return;
    }
    if (v->type == JSON_OBJECT) {
        ce_buf_append(out, cap, len, "{");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) ce_buf_append(out, cap, len, ", ");
            ce_json_escape_into(v->c.keys[i], out, cap, len);
            ce_buf_append(out, cap, len, ": ");
            ce_py_dumps_into(v->c.items[i], out, cap, len);
        }
        ce_buf_append(out, cap, len, "}");
        return;
    }
}
static char *ce_py_dumps(const json_t *v) {
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    out[0] = '\0';
    ce_py_dumps_into(v, &out, &cap, &len);
    return out;
}

/* ── shlex.quote (re.ASCII _find_unsafe = [^\w@%+=:,./-]) ──────────────── */
static bool ce_shlex_safe_char(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
        return true;
    return c == '@' || c == '%' || c == '+' || c == '=' || c == ':' || c == ',' || c == '.' || c == '/' || c == '-';
}
static char *ce_shlex_quote(const char *s) {
    const char *p = s ? s : "";
    if (!*p) return strdup("''");
    bool unsafe = false;
    for (const unsigned char *q = (const unsigned char *)p; *q; q++)
        if (!ce_shlex_safe_char(*q)) { unsafe = true; break; }
    if (!unsafe) return strdup(p);
    /* "'" + s.replace("'", "'\"'\"'") + "'" */
    size_t cap = strlen(p) * 5 + 3, len = 0;
    char *out = malloc(cap);
    out[0] = '\0';
    ce_buf_append(&out, &cap, &len, "'");
    for (const char *q = p; *q; q++) {
        if (*q == '\'') ce_buf_append(&out, &cap, &len, "'\"'\"'");
        else { char ch[2] = {*q, '\0'}; ce_buf_append(&out, &cap, &len, ch); }
    }
    ce_buf_append(&out, &cap, &len, "'");
    return out;
}

/* ── base64.b64encode (standard alphabet, padded) ──────────────────────── */
static char *ce_b64encode(const unsigned char *data, size_t n) {
    static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t olen = 4 * ((n + 2) / 3);
    char *out = malloc(olen + 1);
    size_t w = 0;
    for (size_t i = 0; i < n; i += 3) {
        unsigned v = (unsigned)data[i] << 16;
        if (i + 1 < n) v |= (unsigned)data[i + 1] << 8;
        if (i + 2 < n) v |= (unsigned)data[i + 2];
        out[w++] = T[(v >> 18) & 63];
        out[w++] = T[(v >> 12) & 63];
        out[w++] = (i + 1 < n) ? T[(v >> 6) & 63] : '=';
        out[w++] = (i + 2 < n) ? T[v & 63] : '=';
    }
    out[w] = '\0';
    return out;
}

/* round(x, 2) — printf %.2f rounds half-to-even on the binary double, matching
 * Python round for the fixture domain (clean two-decimal differences). */
static double ce_round2(double x) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.2f", x);
    return strtod(buf, NULL);
}

/* ── ASCII string helpers ──────────────────────────────────────────────── */
static char *ce_upper(const char *s) {
    char *o = strdup(s ? s : "");
    for (char *p = o; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    return o;
}
static bool ce_startswith(const char *s, const char *pre) {
    if (!s || !pre) return false;
    return strncmp(s, pre, strlen(pre)) == 0;
}
static bool ce_contains(const char *s, const char *sub) {
    return s && sub && strstr(s, sub) != NULL;
}
/* s[:n] by bytes (ASCII scope). Returns malloc'd. */
static char *ce_slice(const char *s, size_t n) {
    const char *p = s ? s : "";
    size_t l = strlen(p);
    if (n > l) n = l;
    char *o = malloc(n + 1);
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}

/* sorted copy of a string array (Python sorted() over a set/list of str). */
static int ce_str_cmp(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}
static json_t *ce_sorted_strings(const json_t *arr) {
    json_t *out = json_array();
    if (!arr || arr->type != JSON_ARRAY) return out;
    size_t n = arr->c.count;
    if (n == 0) return out;
    const char **tmp = malloc(n * sizeof(char *));
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING) tmp[k++] = it->str_val ? it->str_val : "";
    }
    qsort(tmp, k, sizeof(char *), ce_str_cmp);
    for (size_t i = 0; i < k; i++) json_append(out, json_string(tmp[i]));
    free(tmp);
    return out;
}

/* ════════════════════════════════════════════════════════════════════════
 * Ported functions. Each returns a caller-owned json_t* (value, json_null for
 * None, or a {"__raise__","message"} descriptor for an uncaught exception).
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: ce_is_secret_env_name @ tools/code_execution_env.py:is_secret_env_name */
static json_t *ce_is_secret_env_name(ce_io_t *io, const json_t *in) {
    (void)io;
    static const char *SUBS[] = {"KEY", "TOKEN", "SECRET", "PASSWORD", "CREDENTIAL", "PASSWD",
                                 "AUTH", "DSN", "WEBHOOK", "CREDS", "BEARER", "APIKEY", "COOKIE", NULL};
    const char *name = ce_get_str(in, "name", "");
    char *up = ce_upper(name);
    bool secret = false;
    for (int i = 0; SUBS[i]; i++) if (ce_contains(up, SUBS[i])) { secret = true; break; }
    free(up);
    return json_bool(secret);
}

/* PoP: ce_cache_probe_result @ tools/code_execution_env.py:_cache_probe_result */
static json_t *ce_cache_probe_result(ce_io_t *io, const json_t *in) {
    json_t *cache = ce_get(io->state, "cache");
    if (!cache || cache->type != JSON_OBJECT) {
        cache = json_object();
        json_set(io->state, "cache", cache);
    }
    const char *key = ce_get_str(in, "key", "");
    json_t *value = ce_get(in, "value");
    /* if len(cache) >= 32: cache.pop(next(iter(cache))) — FIFO first key */
    if (cache->c.count >= 32 && cache->c.count > 0)
        json_obj_del(cache, cache->c.keys[0]);
    json_set(cache, key, value ? json_copy(value) : json_null());
    return json_null();
}

/* PoP: ce_sandbox_tools_for @ tools/code_execution_tool.py:_sandbox_tools_for */
static json_t *ce_sandbox_tools_for(ce_io_t *io, const json_t *in) {
    (void)io;
    static const char *ALLOWED[] = {"web_search", "web_extract", "read_file", "write_file",
                                    "search_files", "patch", "terminal", NULL};
    json_t *enabled = ce_get(in, "enabled_tools");
    json_t *out = json_array();
    if (j_is_null(enabled)) {
        for (int i = 0; ALLOWED[i]; i++) json_append(out, json_string(ALLOWED[i]));
        json_t *s = ce_sorted_strings(out);
        json_free(out);
        return s;
    }
    /* frozenset(ALLOWED & set(enabled)) */
    for (int i = 0; ALLOWED[i]; i++) {
        bool found = false;
        if (enabled->type == JSON_ARRAY) {
            for (size_t j = 0; j < enabled->c.count; j++) {
                json_t *e = enabled->c.items[j];
                if (e && e->type == JSON_STRING && strcmp(sval(e), ALLOWED[i]) == 0) { found = true; break; }
            }
        }
        if (found) json_append(out, json_string(ALLOWED[i]));
    }
    json_t *s = ce_sorted_strings(out);
    json_free(out);
    return s;
}

/* PoP: ce_missing_hermes_tools_import_hint @ tools/code_execution_tool.py:_missing_hermes_tools_import_hint */
static json_t *ce_missing_hermes_tools_import_hint(ce_io_t *io, const json_t *in) {
    const json_t *m = ce_get(in, "m");
    const char *missing = ce_get_str(m, "group1", "");
    if (strcmp(missing, "json_parse") == 0 || strcmp(missing, "shell_quote") == 0 || strcmp(missing, "retry") == 0) {
        char *buf = malloc(strlen(missing) + 256);
        sprintf(buf, "Import helpers with `from hermes_tools import %s`. "
                     "If that import failed, the generated module may be stale or another "
                     "hermes_tools may be first on sys.path. Check hermes_tools.__file__ "
                     "and retry with reset=true.", missing);
        json_t *r = json_string(buf);
        free(buf);
        return r;
    }
    json_t *args = json_object();
    json_t *enabled = ce_get(in, "enabled_tools");
    json_set(args, "enabled_tools", enabled ? json_copy(enabled) : json_null());
    json_t *tools = ce_call(io, "sandbox_tools_for", args);
    json_free(args);
    if (ce_raised(tools)) return tools;
    json_t *sorted = ce_sorted_strings(tools);
    json_free(tools);
    /* ", ".join(sorted) or "none" */
    size_t cap = 64, len = 0;
    char *avail = malloc(cap); avail[0] = '\0';
    for (size_t i = 0; i < sorted->c.count; i++) {
        if (i) ce_buf_append(&avail, &cap, &len, ", ");
        ce_buf_append(&avail, &cap, &len, sval(sorted->c.items[i]));
    }
    json_free(sorted);
    const char *available_text = (len > 0) ? avail : "none";
    char *buf = malloc(strlen(missing) + strlen(available_text) + 256);
    sprintf(buf, "'%s' is not available inside the execute_code sandbox. "
                 "Importable tools here: %s. For anything "
                 "else, use the normal tool call instead of execute_code.", missing, available_text);
    json_t *r = json_string(buf);
    free(buf);
    free(avail);
    return r;
}

/* PoP: ce_json_parse @ tools/code_execution_tool.py:json_parse */
static json_t *ce_json_parse(ce_io_t *io, const json_t *in) {
    json_t *text = ce_get(in, "text");
    /* if isinstance(text, str) and text.startswith("\ufeff"): text = text[1:] */
    const char *t = NULL;
    char *stripped = NULL;
    if (j_is_str(text)) {
        t = sval(text);
        /* UTF-8 BOM = EF BB BF */
        if ((unsigned char)t[0] == 0xEF && (unsigned char)t[1] == 0xBB && (unsigned char)t[2] == 0xBF) {
            stripped = strdup(t + 3);
            t = stripped;
        }
    }
    json_t *args = json_object();
    json_set(args, "text", t ? json_string(t) : (text ? json_copy(text) : json_null()));
    json_set(args, "strict", json_bool(false));
    json_t *r = ce_call(io, "json_loads", args);
    json_free(args);
    free(stripped);
    return r;
}

/* PoP: ce_shell_quote @ tools/code_execution_tool.py:shell_quote */
static json_t *ce_shell_quote(ce_io_t *io, const json_t *in) {
    (void)io;
    const char *s = ce_get_str(in, "s", "");
    char *q = ce_shlex_quote(s);
    json_t *r = json_string(q);
    free(q);
    return r;
}

/* PoP: ce_retry @ tools/code_execution_tool.py:retry */
static json_t *ce_retry(ce_io_t *io, const json_t *in) {
    double max_attempts = ce_get_num(in, "max_attempts", 3);
    double delay = ce_get_num(in, "delay", 2);
    json_t *last_err = NULL;
    int ma = (int)max_attempts;
    for (int attempt = 0; attempt < ma; attempt++) {
        json_t *args = json_object();
        json_t *r = ce_call(io, "fn", args);
        json_free(args);
        if (!ce_raised(r)) {
            json_free(last_err);
            return r;
        }
        if (!ce_is_exception(r)) {
            /* BaseException propagates immediately (not caught by except Exception) */
            json_free(last_err);
            return r;
        }
        json_free(last_err);
        last_err = r;
        if (attempt < ma - 1) {
            double seconds = delay * pow(2.0, (double)attempt);
            json_t *sa = json_object();
            json_set(sa, "seconds", json_number(seconds));
            json_t *sr = ce_call(io, "time.sleep", sa);
            json_free(sa);
            if (ce_raised(sr)) { json_free(last_err); return sr; }
            json_free(sr);
        }
    }
    /* raise last_err (None -> TypeError: exceptions must derive from BaseException) */
    if (!last_err) return ce_raise("TypeError", "exceptions must derive from BaseException");
    return last_err;
}

/* PoP: ce_sanitize_output_text @ tools/code_execution_tool.py:_sanitize_output_text */
static json_t *ce_sanitize_output_text(ce_io_t *io, const json_t *in) {
    const char *text = ce_get_str(in, "text", "");
    json_t *a1 = json_object();
    json_set_hdr(a1, "text", text);
    json_t *stripped = ce_call(io, "strip_ansi", a1);
    json_free(a1);
    if (ce_raised(stripped)) return stripped;
    json_t *a2 = json_object();
    json_set(a2, "text", json_copy(stripped));
    json_t *r = ce_call(io, "redact_code_output", a2);
    json_free(a2);
    json_free(stripped);
    return r;
}

/* PoP: ce_clean_output @ tools/code_execution_tool.py:_clean_output */
static json_t *ce_clean_output(ce_io_t *io, const json_t *in) {
    const char *stdout_text = ce_get_str(in, "stdout_text", "");
    json_t *env = ce_get(in, "env");
    json_t *a1 = json_object();
    json_set_hdr(a1, "text", stdout_text);
    json_t *sanitized = ce_call(io, "sanitize_output_text", a1);
    json_free(a1);
    if (ce_raised(sanitized)) return sanitized;
    json_t *a2 = json_object();
    json_set(a2, "stdout_text", json_copy(sanitized));
    json_set(a2, "env", env ? json_copy(env) : json_null());
    json_t *r = ce_call(io, "truncate_stdout_text", a2);
    json_free(a2);
    json_free(sanitized);
    return r;
}

/* PoP: ce_with_timeout_notice @ tools/code_execution_tool.py:_with_timeout_notice */
static json_t *ce_with_timeout_notice(ce_io_t *io, const json_t *in) {
    (void)io;
    const char *stdout_text = ce_get_str(in, "stdout_text", "");
    const char *timeout_msg = ce_get_str(in, "timeout_msg", "");
    /* U+23F0 HOURGLASS WITH FLOWING SAND = UTF-8 E2 8F B0 (verbatim from live) */
    const char *HOURGLASS = "\xe2\x8f\xb0";
    char *buf;
    if (stdout_text[0]) {
        buf = malloc(strlen(stdout_text) + strlen(timeout_msg) + 16);
        sprintf(buf, "%s\n\n%s %s", stdout_text, HOURGLASS, timeout_msg);
    } else {
        buf = malloc(strlen(timeout_msg) + 16);
        sprintf(buf, "%s %s", HOURGLASS, timeout_msg);
    }
    json_t *r = json_string(buf);
    free(buf);
    return r;
}

/* PoP: ce_error_result @ tools/code_execution_tool.py:_error_result */
static json_t *ce_error_result(ce_io_t *io, const json_t *in) {
    (void)io;
    const char *error = ce_get_str(in, "error", "");
    double tool_calls_made = ce_get_num(in, "tool_calls_made", 0);
    double duration = ce_get_num(in, "duration", 0);
    json_t *user_summary = ce_get(in, "user_summary");
    json_t *body = json_object();
    json_set_hdr(body, "status", "error");
    json_set_hdr(body, "error", error);
    json_set(body, "tool_calls_made", json_number(tool_calls_made));
    json_set(body, "duration_seconds", json_number(duration));
    if (jtruth(user_summary)) json_set(body, "user_summary", json_copy(user_summary));
    char *s = ce_py_dumps(body);
    json_free(body);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP: ce_remote_failure @ tools/code_execution_tool.py:_remote_failure */
static json_t *ce_remote_failure(ce_io_t *io, const json_t *in) {
    json_t *exc = ce_get(in, "exc");
    double exec_start = ce_get_num(in, "exec_start", 0);
    double tool_calls_made = ce_get_num(in, "tool_calls_made", 0);
    const char *exc_name = ce_get_str(exc, "__raise__", "Exception");
    const char *exc_msg = ce_get_str(exc, "message", "");
    json_t *ca = json_object();
    json_t *clock = ce_call(io, "time.monotonic", ca);
    json_free(ca);
    if (ce_raised(clock)) return clock;
    double duration = ce_round2(clock->num_val - exec_start);
    json_free(clock);
    /* logger.error(fmt, duration, tool_calls_made, type(exc).__name__, exc, exc_info=True) */
    json_t *la = json_object();
    json_set_hdr(la, "fmt", "execute_code remote failed after %ss with %d tool calls: %s: %s");
    json_t *largs = json_array();
    json_append(largs, json_number(duration));
    json_append(largs, json_number(tool_calls_made));
    json_append(largs, json_string(exc_name));
    json_append(largs, json_string(exc_msg));
    json_set(la, "args", largs);
    json_set(la, "exc_info", json_bool(true));
    json_t *lr = ce_call(io, "logger.error", la);
    json_free(la);
    if (ce_raised(lr)) return lr;
    json_free(lr);
    /* return _error_result(str(exc), tool_calls_made=..., duration=...) */
    json_t *ea = json_object();
    json_set_hdr(ea, "error", exc_msg);
    json_set(ea, "tool_calls_made", json_number(tool_calls_made));
    json_set(ea, "duration", json_number(duration));
    json_set(ea, "user_summary", json_null());
    json_t *r = ce_call(io, "error_result", ea);
    json_free(ea);
    return r;
}

/* PoP: ce_remote_result @ tools/code_execution_tool.py:_remote_result */
static json_t *ce_remote_result(ce_io_t *io, const json_t *in) {
    const char *status = ce_get_str(in, "status", "");
    const char *raw_stdout = ce_get_str(in, "raw_stdout", "");
    double exec_start = ce_get_num(in, "exec_start", 0);
    json_t *fields = ce_get(in, "fields");
    json_t *kernel = ce_get(in, "kernel");
    json_t *env = ce_get(in, "env");
    /* stdout_text, stdout_metadata = _clean_output(raw_stdout, env) */
    json_t *ca = json_object();
    json_set_hdr(ca, "stdout_text", raw_stdout);
    json_set(ca, "env", env ? json_copy(env) : json_null());
    json_t *cleaned = ce_call(io, "clean_output", ca);
    json_free(ca);
    if (ce_raised(cleaned)) return cleaned;
    json_t *stdout_text = (cleaned->type == JSON_ARRAY && cleaned->c.count > 0) ? cleaned->c.items[0] : NULL;
    json_t *stdout_metadata = (cleaned->type == JSON_ARRAY && cleaned->c.count > 1) ? cleaned->c.items[1] : NULL;
    json_t *result = json_object();
    json_set_hdr(result, "status", status);
    json_set(result, "output", stdout_text ? json_copy(stdout_text) : json_null());
    if (fields && fields->type == JSON_OBJECT) {
        for (size_t i = 0; i < fields->c.count; i++)
            json_set(result, fields->c.keys[i], json_copy(fields->c.items[i]));
    }
    json_t *ma = json_object();
    json_t *clock = ce_call(io, "time.monotonic", ma);
    json_free(ma);
    if (ce_raised(clock)) { json_free(cleaned); json_free(result); return clock; }
    json_set(result, "duration_seconds", json_number(ce_round2(clock->num_val - exec_start)));
    json_free(clock);
    if (!j_is_null(kernel)) json_set(result, "kernel", json_copy(kernel));
    if (stdout_metadata && stdout_metadata->type == JSON_OBJECT) {
        for (size_t i = 0; i < stdout_metadata->c.count; i++)
            json_set(result, stdout_metadata->c.keys[i], json_copy(stdout_metadata->c.items[i]));
    }
    json_free(cleaned);
    return result;
}

/* PoP: ce_apply_timeout @ tools/code_execution_tool.py:_apply_timeout */
static json_t *ce_apply_timeout(ce_io_t *io, const json_t *in) {
    json_t *result = ce_get(io->state, "result");
    if (!result || result->type != JSON_OBJECT) {
        result = json_object();
        json_set(io->state, "result", result);
    }
    const char *timeout_msg = ce_get_str(in, "timeout_msg", "");
    json_set_hdr(result, "error", timeout_msg);
    /* result["output"] read raises KeyError when absent (after error is set).
     * str(KeyError("output")) == "'output'" (the key is repr-quoted). */
    if (!json_has(result, "output")) return ce_raise("KeyError", "'output'");
    json_t *output = ce_get(result, "output");
    json_t *wa = json_object();
    json_set(wa, "stdout_text", output ? json_copy(output) : json_null());
    json_set_hdr(wa, "timeout_msg", timeout_msg);
    json_t *noticed = ce_call(io, "with_timeout_notice", wa);
    json_free(wa);
    if (ce_raised(noticed)) return noticed;
    json_set(result, "output", noticed);
    return json_null();
}

/* PoP: ce_private_dirs_cmd @ tools/code_execution_rpc.py:_private_dirs_cmd */
static json_t *ce_private_dirs_cmd(ce_io_t *io, const json_t *in) {
    (void)io;
    const char *root = ce_get_str(in, "root", "");
    json_t *subdirs = ce_get(in, "subdirs");
    bool has_sub = subdirs && subdirs->type == JSON_ARRAY && subdirs->c.count > 0;
    /* mkdir = " ".join(shlex.quote(d) for d in (subdirs or (root,))) */
    size_t cap = 128, len = 0;
    char *mkdir = malloc(cap); mkdir[0] = '\0';
    if (has_sub) {
        for (size_t i = 0; i < subdirs->c.count; i++) {
            if (i) ce_buf_append(&mkdir, &cap, &len, " ");
            char *q = ce_shlex_quote(sval(subdirs->c.items[i]));
            ce_buf_append(&mkdir, &cap, &len, q);
            free(q);
        }
    } else {
        char *q = ce_shlex_quote(root);
        ce_buf_append(&mkdir, &cap, &len, q);
        free(q);
    }
    /* chmod = " ".join(shlex.quote(d) for d in (root, *subdirs)) */
    cap = 128; len = 0;
    char *chmod = malloc(cap); chmod[0] = '\0';
    char *qr = ce_shlex_quote(root);
    ce_buf_append(&chmod, &cap, &len, qr);
    free(qr);
    if (has_sub) {
        for (size_t i = 0; i < subdirs->c.count; i++) {
            ce_buf_append(&chmod, &cap, &len, " ");
            char *q = ce_shlex_quote(sval(subdirs->c.items[i]));
            ce_buf_append(&chmod, &cap, &len, q);
            free(q);
        }
    }
    char *buf = malloc(strlen(mkdir) + strlen(chmod) + 64);
    sprintf(buf, "umask 077 && mkdir -p %s && chmod 700 %s", mkdir, chmod);
    json_t *r = json_string(buf);
    free(buf); free(mkdir); free(chmod);
    return r;
}

/* PoP: ce_execute_checked @ tools/code_execution_rpc.py:_execute_checked */
static json_t *ce_execute_checked(ce_io_t *io, const json_t *in) {
    const char *cmd = ce_get_str(in, "cmd", "");
    const char *what = ce_get_str(in, "what", "");
    double timeout = ce_get_num(in, "timeout", 0);
    json_t *kwargs = ce_get(in, "kwargs");
    /* result = env.execute(cmd, cwd="/", timeout=timeout, **kwargs) */
    json_t *ea = json_object();
    json_set_hdr(ea, "cmd", cmd);
    json_set_hdr(ea, "cwd", "/");
    json_set(ea, "timeout", json_number(timeout));
    if (kwargs && kwargs->type == JSON_OBJECT)
        for (size_t i = 0; i < kwargs->c.count; i++)
            json_set(ea, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
    json_t *result = ce_call(io, "env.execute", ea);
    json_free(ea);
    if (ce_raised(result)) return result;
    /* if result.get("returncode", 1) != 0: raise RuntimeError(...) */
    double rc = ce_get_num(result, "returncode", 1);
    if (rc != 0) {
        json_t *output = ce_get(result, "output");
        char *orepr = ce_py_repr(output);
        char *buf = malloc(strlen(what) + strlen(orepr) + 32);
        sprintf(buf, "%s failed: %s", what, orepr);
        json_t *r = ce_raise("RuntimeError", buf);
        free(buf); free(orepr);
        json_free(result);
        return r;
    }
    return result;
}

/* PoP: ce_remote_write @ tools/code_execution_rpc.py:_remote_write */
static json_t *ce_remote_write(ce_io_t *io, const json_t *in) {
    const char *remote_path = ce_get_str(in, "remote_path", "");
    const char *content = ce_get_str(in, "content", "");
    bool atomic = json_get_bool(in, "atomic", false);
    double timeout = ce_get_num(in, "timeout", 30);
    bool check = json_get_bool(in, "check", false);
    /* encoded = base64.b64encode(content.encode("utf-8")).decode("ascii") */
    char *encoded = ce_b64encode((const unsigned char *)content, strlen(content));
    char *target = ce_shlex_quote(remote_path);
    /* write = atomic ? "base64 -d > {t}.tmp && mv -f {t}.tmp {t}" : "base64 -d > {t}" */
    char *write;
    if (atomic) {
        write = malloc(strlen(target) * 3 + 64);
        sprintf(write, "base64 -d > %s.tmp && mv -f %s.tmp %s", target, target, target);
    } else {
        write = malloc(strlen(target) + 32);
        sprintf(write, "base64 -d > %s", target);
    }
    char *cmd = malloc(strlen(write) + 32);
    sprintf(cmd, "umask 077 && %s", write);
    json_t *r;
    if (check) {
        /* _execute_checked(env, cmd, f"remote file ship for {remote_path!r}", timeout=timeout, stdin_data=encoded) */
        json_t *rp_node = json_string(remote_path);
        char *rrepr = ce_py_repr(rp_node);
        json_free(rp_node);
        char *what = malloc(strlen(rrepr) + 32);
        sprintf(what, "remote file ship for %s", rrepr);
        free(rrepr);
        json_t *ea = json_object();
        json_set_hdr(ea, "cmd", cmd);
        json_set_hdr(ea, "what", what);
        json_set(ea, "timeout", json_number(timeout));
        json_set_hdr(ea, "stdin_data", encoded);
        r = ce_call(io, "execute_checked", ea);
        json_free(ea);
        free(what);
    } else {
        json_t *ea = json_object();
        json_set_hdr(ea, "cmd", cmd);
        json_set_hdr(ea, "cwd", "/");
        json_set(ea, "timeout", json_number(timeout));
        json_set_hdr(ea, "stdin_data", encoded);
        r = ce_call(io, "env.execute", ea);
        json_free(ea);
    }
    free(encoded); free(target); free(write); free(cmd);
    return r;
}

/* PoP: ce_rpc_token_ok @ tools/code_execution_rpc.py:_rpc_token_ok */
static json_t *ce_rpc_token_ok(ce_io_t *io, const json_t *in) {
    (void)io;
    json_t *request = ce_get(in, "request");
    const char *rpc_token = ce_get_str(in, "rpc_token", "");
    if (!rpc_token[0]) return json_bool(false);
    /* str(request.get("token") or "").encode() == rpc_token.encode() */
    json_t *token = ce_get(request, "token");
    char *token_str;
    if (jtruth(token)) token_str = ce_pystr(token);
    else token_str = strdup("");
    bool ok = strcmp(token_str, rpc_token) == 0;
    free(token_str);
    return json_bool(ok);
}

/* PoP: ce_result_error @ tools/code_execution_rpc.py:_result_error */
static json_t *ce_result_error(ce_io_t *io, const json_t *in) {
    json_t *result = ce_get(in, "result");
    /* if not isinstance(result,str) or not startswith("{") or '"error"' not in result: return "" */
    if (!j_is_str(result)) return json_string("");
    const char *s = sval(result);
    if (!ce_startswith(s, "{") || !ce_contains(s, "\"error\"")) return json_string("");
    json_t *ja = json_object();
    json_set_hdr(ja, "text", s);
    json_set(ja, "strict", json_bool(true));
    json_t *body = ce_call(io, "json_loads", ja);
    json_free(ja);
    if (ce_raised(body)) {
        /* except ValueError: return "" (ValueError family caught; others propagate) */
        if (ce_is_exception(body)) {
            json_t *k = json_obj_get(body, "__raise__");
            const char *kind = k ? sval(k) : "";
            /* ValueError and subclasses (incl JSONDecodeError) are caught */
            bool is_value_error = false;
            const char *s2 = kind;
            for (int d = 0; d < 64; d++) {
                if (strcmp(s2, "ValueError") == 0) { is_value_error = true; break; }
                if (strcmp(s2, "BaseException") == 0 || s2[0] == '\0') break;
                const char *p = ce_exc_parent(s2);
                if (!p) break;
                s2 = p;
            }
            if (is_value_error) { json_free(body); return json_string(""); }
        }
        return body;
    }
    /* error = body.get("error") if isinstance(body,dict) else None */
    json_t *error = (body && body->type == JSON_OBJECT) ? ce_get(body, "error") : NULL;
    json_t *r;
    if (jtruth(error)) {
        char *es = ce_pystr(error);
        char *sl = ce_slice(es, 300);
        r = json_string(sl);
        free(es); free(sl);
    } else {
        r = json_string("");
    }
    json_free(body);
    return r;
}

/* PoP: ce_tool_errors_since @ tools/code_execution_rpc.py:tool_errors_since */
static json_t *ce_tool_errors_since(ce_io_t *io, const json_t *in) {
    (void)io;
    json_t *log = ce_get(in, "tool_call_log");
    double start = ce_get_num(in, "start", 0);
    json_t *out = json_array();
    if (!log || log->type != JSON_ARRAY) return out;
    size_t n = log->c.count;
    size_t st = (start < 0) ? 0 : (size_t)start;
    int emitted = 0;
    for (size_t i = st; i < n && emitted < 5; i++) {
        json_t *e = log->c.items[i];
        if (!e || e->type != JSON_OBJECT) continue;
        json_t *err = ce_get(e, "error");
        if (!jtruth(err)) continue;
        /* e["tool"] read raises KeyError when absent (error is present by filter). */
        if (!json_has(e, "tool")) { json_free(out); return ce_raise("KeyError", "'tool'"); }
        json_t *entry = json_object();
        json_t *tool = ce_get(e, "tool");
        json_set(entry, "tool", tool ? json_copy(tool) : json_null());
        json_set(entry, "error", json_copy(err));
        json_append(out, entry);
        emitted++;
    }
    return out;
}

/* ── dispatch ──────────────────────────────────────────────────────────── */
static json_t *ce_dispatch(ce_io_t *io, const char *func, const json_t *in) {
    if (strcmp(func, "is_secret_env_name") == 0) return ce_is_secret_env_name(io, in);
    if (strcmp(func, "_cache_probe_result") == 0) return ce_cache_probe_result(io, in);
    if (strcmp(func, "_sandbox_tools_for") == 0) return ce_sandbox_tools_for(io, in);
    if (strcmp(func, "_missing_hermes_tools_import_hint") == 0) return ce_missing_hermes_tools_import_hint(io, in);
    if (strcmp(func, "json_parse") == 0) return ce_json_parse(io, in);
    if (strcmp(func, "shell_quote") == 0) return ce_shell_quote(io, in);
    if (strcmp(func, "retry") == 0) return ce_retry(io, in);
    if (strcmp(func, "_sanitize_output_text") == 0) return ce_sanitize_output_text(io, in);
    if (strcmp(func, "_clean_output") == 0) return ce_clean_output(io, in);
    if (strcmp(func, "_with_timeout_notice") == 0) return ce_with_timeout_notice(io, in);
    if (strcmp(func, "_error_result") == 0) return ce_error_result(io, in);
    if (strcmp(func, "_remote_failure") == 0) return ce_remote_failure(io, in);
    if (strcmp(func, "_remote_result") == 0) return ce_remote_result(io, in);
    if (strcmp(func, "_apply_timeout") == 0) return ce_apply_timeout(io, in);
    if (strcmp(func, "_private_dirs_cmd") == 0) return ce_private_dirs_cmd(io, in);
    if (strcmp(func, "_execute_checked") == 0) return ce_execute_checked(io, in);
    if (strcmp(func, "_remote_write") == 0) return ce_remote_write(io, in);
    if (strcmp(func, "_rpc_token_ok") == 0) return ce_rpc_token_ok(io, in);
    if (strcmp(func, "_result_error") == 0) return ce_result_error(io, in);
    if (strcmp(func, "tool_errors_since") == 0) return ce_tool_errors_since(io, in);
    return ce_raise("ValueError", "unknown func");
}

#endif /* CODE_EXECUTION_FLOW_C */