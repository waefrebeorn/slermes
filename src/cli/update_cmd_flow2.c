/*
 * update_cmd_flow2.c — FLOW port of the REMAINING UNCLAIMED update-pipeline
 * surface of hermes_cli/update_cmd.py (LIVE authority). Self-contained: libjson
 * only. All functions static uc2_*; the harness (#include "cli/update_cmd_flow2.c")
 * wires the universal seam to scripted results and records the ordered trace.
 *
 * Covered flows (PoP per function), the unclaimed remainder after update_cmd_flow:
 *   completion: _source_completion_request, _complete_source_update,
 *     _finish_already_up_to_date, _apply_pulled_update
 *   divergence/rollback: _reconcile_diverged_checkout,
 *     _rollback_if_pulled_syntax_error, _roll_back_broken_update, _move_checkout_to
 *   pull/switch: _pull_updates, _switch_branch_at_commit_point,
 *     _refuse_before_commit_point, _exit_after_failed_branch_switch
 *   checkout prep: _apply_parked_branch_guard, _prepare_checkout_for_update
 *   options/receipt/git: _resolve_update_options, _begin_update_receipt_and_plan,
 *     _record_update_initiator, _prepare_git_command,
 *     _handle_update_called_process_error
 *
 * SEAM DOCTRINE: every live effect enters through ONE universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returning a NEW json_t* (NULL == Python None). A raising live call is scripted
 * {"__raise__": "<ExcClass>", "msg": ...}; the cores propagate it exactly where
 * the live try/except catches it. except Exception does NOT catch
 * KeyboardInterrupt/SystemExit/GeneratorExit/BaseException — those pass through
 * to the harness ret envelope (BaseException passthrough). A MISSING seam key
 * fail-closes: the harness delivers {"__raise__":"RuntimeError","msg":"unscripted
 * seam: <name>"}; a present JSON null is a valid None.
 *
 * In-function imports are SILENT (the oracle pre-installs stub modules in
 * sys.modules; no import seam fires). Only effectful calls fire seams.
 * CLAIMED siblings (ported by update_cmd_flow) are SEAMED: _git_run,
 * _capture_head_sha, _update_movement_baseline, _refuse_unselected_head,
 * _verify_head_after_pull, _landed_off_target, _missing_branch_stop,
 * _resolved_commit, _is_shallow_checkout, _tip_shas, _base_git_cmd,
 * _map_ssl_cert_file_for_git, _upstream_broken_header, _updates_config,
 * _finalize_receipt, _settle_windows_resume. UNCLAIMED ported siblings compose
 * for real (_reconcile_diverged_checkout, _roll_back_broken_update,
 * _move_checkout_to, _complete_source_update, _apply_parked_branch_guard,
 * _switch_branch_at_commit_point, _exit_after_failed_branch_switch,
 * _rollback_if_pulled_syntax_error, _record_update_initiator).
 * _best_effort (update_cmd_common, unclaimed) is transcribed inline as its
 * suppress-Exception + silent logger.debug semantics. _checkout_move
 * (update_cmd_pause_gate, foreign) is a SEAMED enter/exit CM pair; its internal
 * pause-record writes are opaque (no token mutation).
 *
 * Representation contract (both sides): paths are verbatim POSIX strings; an
 * uncaught exception rides as {"__raise__": cls, "msg": str(exc)} (the oracle
 * drive's BaseException catch builds exactly that shape). SystemExit msg is
 * str(code) ("1"/"130"). Integral JSON numbers read as int (libjson has no
 * int/float identity). A callable marker projects to {"__callable__": true}.
 *
 * Fixture domains (documented exclusions):
 * - ASCII strings; POSIX path strings; PROJECT_ROOT a non-empty absolute path
 *   (no "//" root). Message CONSTANTS carry byte-pinned UTF-8 glyphs.
 * - git_cmd / args / cmd are JSON arrays of ASCII strings (no quotes/backslashes/
 *   control chars). git_run scripts are COMPLETE {returncode, stdout, stderr}
 *   records (stdout/stderr str-or-null, returncode int) or raises.
 * - subprocess raise specs: CalledProcessError {returncode, cmd, output?, stderr?},
 *   TimeoutExpired {cmd, timeout}. str() transcribed. UpstreamTargetBroken carries
 *   {sha, path, error} and msg == "upstream/main (<sha[:10]>) has a syntax error in
 *   <path>". Other raise specs use classes whose str()==msg.
 * - opts/args/plan/track_result are JSON objects (attribute doubles) or null;
 *   request/result/token are JSON objects (dicts) or null. Direct attribute reads
 *   raise AttributeError on an absent key; dict subscripts raise KeyError.
 * - int()-position strings are printable ASCII without quotes/backslashes; counts
 *   fit a 64-bit int. str.strip() whitespace is the ASCII White_Space set (incl
 *   \x1c-\x1f); splitlines boundaries \n \r \r\n \v \f \x1c \x1d \x1e.
 * - gw_input_fn is null or a truthy marker object (projects {"__callable__":true}).
 * - logger is silent (no trace events), matching the sibling flow ports.
 *
 * Ownership: seam results and returned json_t / char pointers are malloc'd,
 * caller frees. Inputs are borrowed. uc2_call borrows args (caller frees). A
 * propagated raise dict is returned upward as-is; a caught raise dict is freed
 * at the catch site. Mutable request carriers are owned by the harness.
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

/* ── message-constant glyphs (byte-pinned UTF-8) ── */
#define UC2_CROSS  "\xe2\x9c\x97"        /* ✗ */
#define UC2_CHECK  "\xe2\x9c\x93"        /* ✓ */
#define UC2_WARN   "\xe2\x9a\xa0"        /* ⚠ */
#define UC2_INFO   "\xe2\x84\xb9"        /* ℹ */
#define UC2_INFOVS "\xe2\x84\xb9\xef\xb8\x8f" /* ℹ️ (U+2139 U+FE0F) */
#define UC2_EMDASH "\xe2\x80\x94"        /* — */
#define UC2_ARROW  "\xe2\x86\x92"        /* → */

/* _ORPHAN_RESCUE_REF_MAX_AGE_DAYS (update_cmd_git.py:24). */
#define UC2_ORPHAN_RESCUE_REF_MAX_AGE_DAYS 30

/* _UPDATE_CRITICAL_FILES (update_cmd.py:209). */
static const char *const UC2_CRITICAL_FILES[] = {
    "hermes_bootstrap.py", "hermes_cli/_parser.py", "hermes_cli/_launchers.py",
    "hermes_cli/_early_recovery.py", "hermes_cli/update_lock.py", "hermes_cli/update_custody.py",
    "hermes_cli/update_handoff.py", "hermes_cli/venv_sync.py", "pm/__init__.py", "pm/environments.py",
    "hermes_cli/main.py", "hermes_cli/config.py", "hermes_cli/__init__.py",
    "hermes_cli/web_server.py", "cli.py", "run_agent.py", "model_tools.py", "toolsets.py",
    "hermes_constants.py", NULL
};

/* ── universal seam + trace ── */

typedef struct uc2_trace { json_t *events; } uc2_trace_t;

typedef struct uc2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} uc2_seams_t;

static void uc2_trace_add(uc2_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void uc2_trace_free(uc2_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *uc2_call(const uc2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *uc2_call0(const uc2_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = uc2_call(s, name, a);
    json_free(a);
    return r;
}

/* ── JSON value helpers ── */

static bool uc2_jtruth(const json_t *v) {
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
static const char *uc2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *uc2_jstr_def(const json_t *v, const char *def) {
    const char *s = uc2_jstr(v);
    return s ? s : def;
}
static bool uc2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool uc2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool uc2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool uc2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool uc2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

static char *uc2_xsf(const char *fmt, ...) {
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

typedef struct { char *p; size_t len, cap; } uc2_buf;
static void uc2_binit(uc2_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void uc2_bfree(uc2_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void uc2_bput(uc2_buf *b, const char *s, size_t n) {
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
static void uc2_bputs(uc2_buf *b, const char *s) { uc2_bput(b, s, strlen(s)); }
static void uc2_bputc(uc2_buf *b, char c) { uc2_bput(b, &c, 1); }
static char *uc2_btake(uc2_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* ── exception model (FULL builtin parent hierarchy) ── */

static const char *uc2_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool uc2_is_raise(const json_t *r) { return uc2_raise_class(r) != NULL; }

static json_t *uc2_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static bool uc2_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
/* Parent lookup mirroring the py3.14 builtin hierarchy plus the subprocess
 * family and the custom kinds the oracle registers. Unknown custom kinds
 * default to Exception; the BaseException trio stays OUTSIDE Exception so
 * except Exception passes them through. */
static const char *uc2_exc_parent(const char *k) {
    if (!strcmp(k, "BaseException")) return NULL;
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
        !strcmp(k, "GeneratorExit")) return "BaseException";
    if (!strcmp(k, "Exception")) return "BaseException";
    static const char *const EXC_CHILDREN[] = {
        "StopIteration", "StopAsyncIteration", "ArithmeticError", "AssertionError",
        "AttributeError", "BufferError", "EOFError", "ImportError", "LookupError",
        "MemoryError", "NameError", "OSError", "ReferenceError", "RuntimeError",
        "SyntaxError", "SystemError", "TypeError", "ValueError", "Warning",
        "SubprocessError", NULL
    };
    if (uc2_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (uc2_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (uc2_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (uc2_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (uc2_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (uc2_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError", NULL };
    if (uc2_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    static const char *const SUBPROC_CHILDREN[] = { "CalledProcessError", "TimeoutExpired", NULL };
    if (uc2_in_names(k, SUBPROC_CHILDREN)) return "SubprocessError";
    if (!strcmp(k, "RetiredUpdaterHook")) return "RuntimeError";
    if (!strcmp(k, "ChannelError")) return "ValueError";
    if (!strcmp(k, "UpstreamTargetBroken")) return "Exception";
    if (!strcmp(k, "PythonFinalizationError")) return "RuntimeError";
    if (!strcmp(k, "_IncompleteInputError")) return "SyntaxError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "Exception";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (uc2_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (uc2_in_names(k, WARNING_CHILDREN)) return "Warning";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (uc2_in_names(k, NAME_CHILDREN)) return "NameError";
    return "Exception";  /* unknown custom kind: oracle bases it on Exception */
}
static bool uc2_caught(const json_t *r, const char *cls) {
    const char *k = uc2_raise_class(r);
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = uc2_exc_parent(k);
    }
    return false;
}
static bool uc2_is_exception(const json_t *r) { return uc2_caught(r, "Exception"); }

/* Python type name of a JSON value (integral numbers read as int) */
static const char *uc2_typename(const json_t *v) {
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
static json_t *uc2_attr_raise(const char *tname, const char *attr) {
    char *msg = uc2_xsf("'%s' object has no attribute '%s'", tname, attr);
    json_t *e = uc2_raise_spec("AttributeError", msg);
    free(msg);
    return e;
}
static json_t *uc2_typeerror(const char *msg) { return uc2_raise_spec("TypeError", msg); }
static json_t *uc2_keyerror(const char *key) {
    char *msg = uc2_xsf("'%s'", key);
    json_t *e = uc2_raise_spec("KeyError", msg);
    free(msg);
    return e;
}

/* ── args builders ── */

static void uc2_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void uc2_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void uc2_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void uc2_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── Python float repr / str / repr ── */

static char *uc2_float_repr(double v) {
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
    uc2_buf b; uc2_binit(&b);
    if (neg) uc2_bputc(&b, '-');
    if (exp >= -4 && exp < 16) {
        if (exp >= 0) {
            size_t point = (size_t)exp + 1;
            if (nd <= point) {
                uc2_bput(&b, digits, nd);
                for (size_t i = nd; i < point; i++) uc2_bputc(&b, '0');
                uc2_bputs(&b, ".0");
            } else {
                uc2_bput(&b, digits, point);
                uc2_bputc(&b, '.');
                uc2_bputs(&b, digits + point);
            }
        } else {
            uc2_bputs(&b, "0.");
            for (int i = 0; i < -exp - 1; i++) uc2_bputc(&b, '0');
            uc2_bputs(&b, digits);
        }
    } else {
        uc2_bputc(&b, digits[0]);
        if (nd > 1) { uc2_bputc(&b, '.'); uc2_bputs(&b, digits + 1); }
        char ex[16];
        snprintf(ex, sizeof(ex), "e%s%02d", exp < 0 ? "-" : "+", exp < 0 ? -exp : exp);
        uc2_bputs(&b, ex);
    }
    return uc2_btake(&b);
}

static char *uc2_repr_string(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    uc2_buf b; uc2_binit(&b);
    uc2_bputc(&b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { uc2_bputc(&b, '\\'); uc2_bputc(&b, (char)c); }
        else if (c == '\n') uc2_bputs(&b, "\\n");
        else if (c == '\r') uc2_bputs(&b, "\\r");
        else if (c == '\t') uc2_bputs(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char x[8]; snprintf(x, sizeof(x), "\\x%02x", c); uc2_bputs(&b, x); }
        else uc2_bputc(&b, (char)c);
    }
    uc2_bputc(&b, q);
    return uc2_btake(&b);
}

static char *uc2_pyrepr(const json_t *v);
static char *uc2_pystr(const json_t *v) {
    if (uc2_is_null(v)) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) {
                if (d == 0.0) return strdup("0");
                return uc2_xsf("%.0f", d);
            }
            return uc2_float_repr(d);
        }
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: return uc2_pyrepr(v);
    }
}
static char *uc2_pyrepr(const json_t *v) {
    if (uc2_is_null(v)) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) {
                if (d == 0.0) return strdup("0");
                return uc2_xsf("%.0f", d);
            }
            return uc2_float_repr(d);
        }
        case JSON_STRING: return uc2_repr_string(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            uc2_buf b; uc2_binit(&b);
            uc2_bputc(&b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) uc2_bputs(&b, ", ");
                char *r = uc2_pyrepr(v->c.items[i]);
                uc2_bputs(&b, r); free(r);
            }
            uc2_bputc(&b, ']');
            return uc2_btake(&b);
        }
        case JSON_OBJECT: {
            uc2_buf b; uc2_binit(&b);
            uc2_bputc(&b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) uc2_bputs(&b, ", ");
                char *k = uc2_repr_string(v->c.keys[i]);
                char *r = uc2_pyrepr(v->c.items[i]);
                uc2_bputs(&b, k); uc2_bputs(&b, ": "); uc2_bputs(&b, r);
                free(k); free(r);
            }
            uc2_bputc(&b, '}');
            return uc2_btake(&b);
        }
        default: return strdup("None");
    }
}

/* ── ASCII string helpers ── */

static bool uc2_is_ws(unsigned char c) {
    return c == ' ' || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
/* int() strips a NARROWER set than str.strip(): no \x1c-\x1f. */
static bool uc2_is_int_ws(unsigned char c) {
    return c == ' ' || (c >= 0x09 && c <= 0x0d);
}
static char *uc2_strip(const char *s) {
    while (*s && uc2_is_ws((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && uc2_is_ws((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s);
    char *r = malloc(n + 1);
    memcpy(r, s, n); r[n] = '\0';
    return r;
}
static bool uc2_startswith(const char *s, const char *pre) {
    return strncmp(s, pre, strlen(pre)) == 0;
}
static char *uc2_slice(const char *s, size_t n) {
    size_t l = strlen(s);
    if (n > l) n = l;
    char *r = malloc(n + 1);
    memcpy(r, s, n); r[n] = '\0';
    return r;
}
static char *uc2_ascii_lower(const char *s) {
    char *r = strdup(s);
    for (char *p = r; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return r;
}
static bool uc2_isdigit(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return false;
    return true;
}
/* str.splitlines() boundaries (ASCII domain): \n \r \r\n \v \f \x1c \x1d \x1e.
 * Returns a malloc'd array of malloc'd lines; *n set. Empty string -> 0 lines. */
static char **uc2_splitlines(const char *s, size_t *n) {
    char **lines = NULL; size_t cnt = 0, cap = 0;
    size_t l = strlen(s), i = 0, start = 0;
    while (i < l) {
        unsigned char c = (unsigned char)s[i];
        bool brk = (c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
                    c == 0x1c || c == 0x1d || c == 0x1e);
        if (brk) {
            size_t len = i - start;
            char *ln = malloc(len + 1); memcpy(ln, s + start, len); ln[len] = '\0';
            if (cnt == cap) { cap = cap ? cap * 2 : 8; lines = realloc(lines, cap * sizeof(char *)); }
            lines[cnt++] = ln;
            if (c == '\r' && i + 1 < l && s[i + 1] == '\n') i++;
            i++; start = i;
        } else i++;
    }
    if (start < l) {
        size_t len = l - start;
        char *ln = malloc(len + 1); memcpy(ln, s + start, len); ln[len] = '\0';
        if (cnt == cap) { cap = cap ? cap * 2 : 8; lines = realloc(lines, cap * sizeof(char *)); }
        lines[cnt++] = ln;
    }
    *n = cnt;
    return lines;
}
static void uc2_free_lines(char **lines, size_t n) {
    for (size_t i = 0; i < n; i++) free(lines[i]);
    free(lines);
}
/* first line of splitlines(); NULL when no lines (empty string). */
static char *uc2_first_line(const char *s) {
    size_t n; char **ln = uc2_splitlines(s, &n);
    char *r = n ? strdup(ln[0]) : NULL;
    uc2_free_lines(ln, n);
    return r;
}
/* PurePosixPath(base) / name join (verbatim-string model): rstrip('/') + '/' + name. */
static char *uc2_posix_join(const char *base, const char *name) {
    size_t bl = strlen(base);
    while (bl > 0 && base[bl - 1] == '/') bl--;
    return uc2_xsf("%.*s/%s", (int)bl, base, name);
}
/* shlex.join: quote args with chars outside [\w@%+=:,./-] (ASCII). */
static bool uc2_shlex_safe(const char *s) {
    if (!*s) return false;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '@' || c == '%' ||
                  c == '+' || c == '=' || c == ':' || c == ',' || c == '.' ||
                  c == '/' || c == '-';
        if (!ok) return false;
    }
    return true;
}
static char *uc2_shlex_join(const json_t *arr) {
    uc2_buf b; uc2_binit(&b);
    if (uc2_is_arr(arr)) {
        for (size_t i = 0; i < arr->c.count; i++) {
            if (i) uc2_bputc(&b, ' ');
            const char *s = uc2_jstr_def(arr->c.items[i], "");
            if (uc2_shlex_safe(s)) uc2_bputs(&b, s);
            else {
                uc2_bputc(&b, '\'');
                for (const char *p = s; *p; p++) {
                    if (*p == '\'') uc2_bputs(&b, "'\"'\"'");
                    else uc2_bputc(&b, *p);
                }
                uc2_bputc(&b, '\'');
            }
        }
    }
    return uc2_btake(&b);
}
/* int(str) grammar: optional sign, digits with single underscores between
 * digits. Returns 0 on success and sets *out; on failure returns a ValueError
 * raise descriptor (caller frees). Domain: printable ASCII, |value| < 2^63. */
static json_t *uc2_py_int(const char *s, long long *out) {
    const char *p = s;
    while (*p && uc2_is_int_ws((unsigned char)*p)) p++;
    const char *end = p + strlen(p);
    while (end > p && uc2_is_int_ws((unsigned char)end[-1])) end--;
    bool neg = false;
    if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
    if (p == end) goto bad;
    long long val = 0; bool any = false, last_us = false;
    for (; p < end; p++) {
        char c = *p;
        if (c == '_') {
            if (!any || last_us) goto bad;
            last_us = true; continue;
        }
        if (c < '0' || c > '9') goto bad;
        last_us = false; any = true;
        val = val * 10 + (c - '0');
    }
    if (!any || last_us) goto bad;
    *out = neg ? -val : val;
    return NULL;
bad: {
    char *rp = uc2_repr_string(s);
    char *msg = uc2_xsf("invalid literal for int() with base 10: %s", rp);
    free(rp);
    json_t *e = uc2_raise_spec("ValueError", msg);
    free(msg);
    return e;
}
}
/* sorted unique join of a str array's elements (profiles). */
static int uc2_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static char *uc2_sorted_unique_join(const json_t *arr, const char *sep) {
    const char **items = NULL; size_t n = 0;
    if (uc2_is_arr(arr)) {
        items = malloc(arr->c.count * sizeof(char *));
        for (size_t i = 0; i < arr->c.count; i++) items[n++] = uc2_jstr_def(arr->c.items[i], "");
    }
    qsort(items, n, sizeof(char *), uc2_cmp_str);
    uc2_buf b; uc2_binit(&b);
    for (size_t i = 0; i < n; i++) {
        if (i && !strcmp(items[i], items[i - 1])) continue;
        if (b.len) uc2_bputs(&b, sep);
        uc2_bputs(&b, items[i]);
    }
    free(items);
    return uc2_btake(&b);
}
static void uc2_obj_clear(json_t *obj) {
    if (!uc2_is_obj(obj)) return;
    while (obj->c.count > 0) json_obj_del(obj, obj->c.keys[0]);
}

/* ── carrier reads (attribute doubles vs dict subscripts) ── */

/* attribute read on a SimpleNamespace double: absent key raises. Carrier null
 * -> NoneType; non-object -> its type name; object -> types.SimpleNamespace. */
static json_t *uc2_attr(const json_t *obj, const char *key) {
    if (uc2_is_null(obj)) return uc2_attr_raise("NoneType", key);
    if (!uc2_is_obj(obj)) return uc2_attr_raise(uc2_typename(obj), key);
    const json_t *v = json_obj_get(obj, key);
    if (!v) return uc2_attr_raise("types.SimpleNamespace", key);
    return json_copy(v);
}
/* getattr(obj, key, default): never raises; default borrowed (NULL==None). */
static json_t *uc2_getattr(const json_t *obj, const char *key, const json_t *deflt) {
    if (uc2_is_obj(obj)) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return json_copy(v);
    }
    return deflt ? json_copy(deflt) : json_null();
}
/* dict subscript obj[key]: missing -> KeyError; non-dict -> TypeError. */
static json_t *uc2_sub(const json_t *obj, const char *key) {
    if (!uc2_is_obj(obj)) {
        char *m = uc2_xsf("'%s' object is not subscriptable", uc2_typename(obj));
        json_t *e = uc2_typeerror(m); free(m); return e;
    }
    const json_t *v = json_obj_get(obj, key);
    if (!v) return uc2_keyerror(key);
    return json_copy(v);
}
/* dict.get(key, default): non-dict -> AttributeError 'get'; absent -> default. */
static json_t *uc2_dget(const json_t *obj, const char *key, const json_t *deflt) {
    if (uc2_is_null(obj)) return uc2_attr_raise("NoneType", "get");
    if (!uc2_is_obj(obj)) return uc2_attr_raise(uc2_typename(obj), "get");
    const json_t *v = json_obj_get(obj, key);
    if (v) return json_copy(v);
    return deflt ? json_copy(deflt) : json_null();
}
/* field read on a run_git/CompletedProcess result double (SimpleNamespace). */
static json_t *uc2_rec_field(const json_t *rec, const char *key) {
    if (uc2_is_null(rec)) return uc2_attr_raise("NoneType", key);
    if (!uc2_is_obj(rec)) return uc2_attr_raise(uc2_typename(rec), key);
    const json_t *v = json_obj_get(rec, key);
    if (!v) return uc2_attr_raise("types.SimpleNamespace", key);
    return json_copy(v);
}

/* ── shared seam helpers ── */

static json_t *uc2_print(const uc2_seams_t *sp, const char *text) {
    json_t *a = json_object();
    uc2_aset_s(a, "text", text ? text : "");
    json_t *r = uc2_call(sp, "print", a);
    json_free(a);
    return r;
}
static json_t *uc2_record_stop(const uc2_seams_t *sp, const char *reason, const char *without_receipt) {
    json_t *a = json_object();
    uc2_aset_s(a, "reason", reason);
    uc2_aset_s(a, "without_receipt", without_receipt);
    json_t *r = uc2_call(sp, "record_stop", a);
    json_free(a);
    return r;
}
static json_t *uc2_project_root(const uc2_seams_t *sp) {
    return uc2_call0(sp, "project_root");
}
/* str(exc) for the subprocess family / UpstreamTargetBroken / msg classes. */
static char *uc2_exc_str(const json_t *r) {
    const char *cls = uc2_raise_class(r);
    const json_t *msgn = json_obj_get(r, "msg");
    const char *msg = uc2_jstr_def(msgn, "");
    if (cls && !strcmp(cls, "CalledProcessError")) {
        const json_t *cmd = json_obj_get(r, "cmd");
        const json_t *rcn = json_obj_get(r, "returncode");
        int rc = (rcn && rcn->type == JSON_NUMBER) ? (int)rcn->num_val : 1;
        char *cmdstr = uc2_pystr(cmd);
        char *s = uc2_xsf("Command '%s' returned non-zero exit status %d.", cmdstr, rc);
        free(cmdstr);
        return s;
    }
    if (cls && !strcmp(cls, "TimeoutExpired")) {
        const json_t *cmd = json_obj_get(r, "cmd");
        const json_t *tn = json_obj_get(r, "timeout");
        char *cmdstr = uc2_pystr(cmd);
        char *tstr = tn ? uc2_pystr(tn) : strdup("300");
        char *s = uc2_xsf("Command '%s' timed out after %s seconds", cmdstr, tstr);
        free(cmdstr); free(tstr);
        return s;
    }
    if (cls && !strcmp(cls, "UpstreamTargetBroken")) {
        const json_t *sha = json_obj_get(r, "sha");
        const json_t *path = json_obj_get(r, "path");
        char *s10 = uc2_slice(uc2_jstr_def(sha, ""), 10);
        char *s = uc2_xsf("upstream/main (%s) has a syntax error in %s", s10, uc2_jstr_def(path, ""));
        free(s10);
        return s;
    }
    return strdup(msg);
}
/* (rec.<k1> or rec.<k2> or "").strip() — k2 may be NULL. Returns str or raise. */
static json_t *uc2_or_strip(const json_t *rec, const char *k1, const char *k2) {
    json_t *f = uc2_rec_field(rec, k1);
    if (uc2_is_raise(f)) return f;
    if (!uc2_jtruth(f) && k2) {
        json_free(f);
        f = uc2_rec_field(rec, k2);
        if (uc2_is_raise(f)) return f;
    }
    if (!uc2_jtruth(f)) { json_free(f); return json_string(""); }
    if (!uc2_is_str(f)) {
        json_t *e = uc2_attr_raise(uc2_typename(f), "strip");
        json_free(f); return e;
    }
    char *s = uc2_strip(f->str_val ? f->str_val : "");
    json_free(f);
    json_t *r = json_string(s);
    free(s);
    return r;
}
/* rec.stdout.strip() (no or-collapse). Returns str or raise. */
static json_t *uc2_stdout_strip(const json_t *rec) {
    json_t *f = uc2_rec_field(rec, "stdout");
    if (uc2_is_raise(f)) return f;
    if (!uc2_is_str(f)) {
        json_t *e = uc2_attr_raise(uc2_typename(f), "strip");
        json_free(f); return e;
    }
    char *s = uc2_strip(f->str_val ? f->str_val : "");
    json_free(f);
    json_t *r = json_string(s);
    free(s);
    return r;
}
/* input_fn callable marker projection: truthy marker -> {"__callable__":true}. */
static json_t *uc2_input_fn_proj(const json_t *marker) {
    if (uc2_jtruth(marker)) {
        json_t *o = json_object();
        json_set(o, "__callable__", json_bool(true));
        return o;
    }
    return json_null();
}
/* e (CalledProcessError) projection {returncode, cmd, output, stderr}. */
static json_t *uc2_e_proj(const json_t *e) {
    json_t *o = json_object();
    uc2_aset_j(o, "returncode", uc2_is_obj(e) ? json_obj_get(e, "returncode") : NULL);
    uc2_aset_j(o, "cmd", uc2_is_obj(e) ? json_obj_get(e, "cmd") : NULL);
    uc2_aset_j(o, "output", uc2_is_obj(e) ? json_obj_get(e, "output") : NULL);
    uc2_aset_j(o, "stderr", uc2_is_obj(e) ? json_obj_get(e, "stderr") : NULL);
    return o;
}

/* ── claimed-sibling seams (update_cmd_flow ports) ── */

static json_t *uc2_git_run(const uc2_seams_t *sp, const json_t *git_cmd, const json_t *args, bool check) {
    json_t *a = json_object();
    uc2_aset_j(a, "git_cmd", git_cmd);
    uc2_aset_j(a, "args", args);
    uc2_aset_s(a, "cwd", NULL);
    uc2_aset_b(a, "check", check);
    uc2_aset_b(a, "network", false);
    json_t *r = uc2_call(sp, "git_run", a);
    json_free(a);
    return r;
}
static json_t *uc2_capture_head_sha(const uc2_seams_t *sp, const json_t *git_cmd, const char *cwd) {
    json_t *a = json_object();
    uc2_aset_j(a, "git_cmd", git_cmd);
    uc2_aset_s(a, "cwd", cwd);
    json_t *r = uc2_call(sp, "capture_head_sha", a);
    json_free(a);
    return r;
}
static json_t *uc2_resolved_commit(const uc2_seams_t *sp, const json_t *git_cmd, const char *ref) {
    json_t *a = json_object();
    uc2_aset_j(a, "git_cmd", git_cmd);
    uc2_aset_s(a, "ref", ref);
    json_t *r = uc2_call(sp, "resolved_commit", a);
    json_free(a);
    return r;
}
static json_t *uc2_landed_off_target(const uc2_seams_t *sp, const json_t *git_cmd, const char *target_sha, bool merged) {
    json_t *a = json_object();
    uc2_aset_j(a, "git_cmd", git_cmd);
    uc2_aset_s(a, "target_sha", target_sha);
    uc2_aset_b(a, "merged", merged);
    json_t *r = uc2_call(sp, "landed_off_target", a);
    json_free(a);
    return r;
}
static json_t *uc2_refuse_unselected_head(const uc2_seams_t *sp, const json_t *git_cmd,
                                          const char *target_sha, const json_t *unmoved) {
    json_t *a = json_object();
    uc2_aset_j(a, "git_cmd", git_cmd);
    uc2_aset_s(a, "target_sha", target_sha);
    uc2_aset_j(a, "unmoved", unmoved);
    json_t *r = uc2_call(sp, "refuse_unselected_head", a);
    json_free(a);
    return r;
}
static json_t *uc2_verify_head_after_pull(const uc2_seams_t *sp, const json_t *git_cmd, const char *branch,
                                          const json_t *pre_pull_sha, bool in_place_update, const json_t *token) {
    json_t *a = json_object();
    uc2_aset_j(a, "git_cmd", git_cmd);
    uc2_aset_s(a, "branch", branch);
    uc2_aset_j(a, "pre_pull_sha", pre_pull_sha);
    uc2_aset_b(a, "in_place_update", in_place_update);
    uc2_aset_j(a, "token", token);
    json_t *r = uc2_call(sp, "verify_head_after_pull", a);
    json_free(a);
    return r;
}
static json_t *uc2_upstream_broken_header(const uc2_seams_t *sp, const json_t *broken) {
    json_t *b = json_object();
    uc2_aset_j(b, "sha", uc2_is_obj(broken) ? json_obj_get(broken, "sha") : NULL);
    uc2_aset_j(b, "path", uc2_is_obj(broken) ? json_obj_get(broken, "path") : NULL);
    uc2_aset_j(b, "error", uc2_is_obj(broken) ? json_obj_get(broken, "error") : NULL);
    json_t *a = json_object();
    uc2_aset_j(a, "broken", b);
    json_free(b);
    json_t *r = uc2_call(sp, "upstream_broken_header", a);
    json_free(a);
    return r;
}
static json_t *uc2_updates_config(const uc2_seams_t *sp) {
    return uc2_call0(sp, "updates_config");
}
static json_t *uc2_finalize_receipt(const uc2_seams_t *sp, const char *status, const char *debug_message) {
    json_t *a = json_object();
    uc2_aset_s(a, "status", status);
    uc2_aset_s(a, "debug_message", debug_message);
    json_t *r = uc2_call(sp, "finalize_receipt", a);
    json_free(a);
    return r;
}
/* _settle_windows_resume (claimed): seam + contract mutation on success. */
static json_t *uc2_settle_windows_resume(const uc2_seams_t *sp, json_t *request) {
    json_t *a = json_object();
    uc2_aset_j(a, "request", request);
    json_t *r = uc2_call(sp, "settle_windows_resume", a);
    json_free(a);
    if (uc2_is_raise(r)) return r;
    json_free(r);
    if (uc2_is_obj(request)) json_set(request, "windows_resume_settled", json_bool(true));
    return NULL;
}

/* ── foreign collaborator seams ── */

static json_t *uc2_moves_for(const uc2_seams_t *sp, const json_t *token) {
    json_t *a = json_object();
    uc2_aset_j(a, "token", token);
    json_t *r = uc2_call(sp, "moves_for", a);
    json_free(a);
    return r;
}
static json_t *uc2_resume_gateways(const uc2_seams_t *sp, const json_t *token) {
    json_t *a = json_object();
    uc2_aset_j(a, "token", token);
    json_t *r = uc2_call(sp, "resume_windows_gateways_after_update", a);
    json_free(a);
    return r;
}
static json_t *uc2_owe_followup(const uc2_seams_t *sp, const json_t *update_id, const char *kind,
                                const char *message, const char *retry) {
    json_t *a = json_object();
    uc2_aset_j(a, "update_id", update_id);
    uc2_aset_s(a, "kind", kind);
    uc2_aset_s(a, "message", message);
    uc2_aset_s(a, "retry", retry);
    json_t *r = uc2_call(sp, "owe_followup", a);
    json_free(a);
    return r;
}
static json_t *uc2_marker(const uc2_seams_t *sp, const char *root) {
    json_t *a = json_object();
    uc2_aset_s(a, "root", root);
    json_t *r = uc2_call(sp, "interrupted_pull_marker", a);
    json_free(a);
    return r;
}
static json_t *uc2_marker_unlink(const uc2_seams_t *sp, const char *path) {
    json_t *a = json_object();
    uc2_aset_s(a, "path", path);
    json_t *r = uc2_call(sp, "marker_unlink", a);
    json_free(a);
    return r;
}
static json_t *uc2_marker_is_file(const uc2_seams_t *sp, const char *path) {
    json_t *a = json_object();
    uc2_aset_s(a, "path", path);
    json_t *r = uc2_call(sp, "marker_is_file", a);
    json_free(a);
    return r;
}
static json_t *uc2_settle_failed_tree_move(const uc2_seams_t *sp, const char *root) {
    json_t *a = json_object();
    uc2_aset_s(a, "root", root);
    json_t *r = uc2_call(sp, "settle_failed_tree_move", a);
    json_free(a);
    return r;
}
static json_t *uc2_disarm(const uc2_seams_t *sp) {
    return uc2_call0(sp, "disarm_commit_obligations");
}
static json_t *uc2_owe_restore_of(const uc2_seams_t *sp, const char *root, const json_t *sha) {
    json_t *a = json_object();
    uc2_aset_s(a, "root", root);
    uc2_aset_j(a, "sha", sha);
    json_t *r = uc2_call(sp, "owe_restore_of", a);
    json_free(a);
    return r;
}
static json_t *uc2_arm_commit_point(const uc2_seams_t *sp, const json_t *git_cmd, const char *root,
                                    const char *target_sha, const json_t *pre, const json_t *target,
                                    const json_t *stash) {
    json_t *a = json_object();
    uc2_aset_j(a, "git_cmd", git_cmd);
    uc2_aset_s(a, "root", root);
    uc2_aset_s(a, "target_sha", target_sha);
    uc2_aset_j(a, "pre", pre);
    uc2_aset_j(a, "target", target);
    uc2_aset_j(a, "stash", stash);
    json_t *r = uc2_call(sp, "arm_commit_point", a);
    json_free(a);
    return r;
}
/* _checkout_move CM enter/exit pair. targets/paths are arrays. */
static json_t *uc2_cm_enter(const uc2_seams_t *sp, const json_t *token, const json_t *targets,
                            const json_t *paths, bool revert) {
    json_t *a = json_object();
    uc2_aset_j(a, "token", token);
    uc2_aset_j(a, "targets", targets);
    uc2_aset_j(a, "paths", paths);
    uc2_aset_b(a, "revert", revert);
    json_t *r = uc2_call(sp, "checkout_move.enter", a);
    json_free(a);
    return r;
}
static json_t *uc2_cm_exit(const uc2_seams_t *sp, const json_t *token, const json_t *targets,
                           const json_t *paths, bool revert, const json_t *body_raise) {
    json_t *exc = NULL;
    if (uc2_is_raise(body_raise)) {
        exc = json_object();
        uc2_aset_s(exc, "kind", uc2_raise_class(body_raise));
        char *m = uc2_exc_str(body_raise);
        uc2_aset_s(exc, "msg", m);
        free(m);
    }
    json_t *a = json_object();
    uc2_aset_j(a, "token", token);
    uc2_aset_j(a, "targets", targets);
    uc2_aset_j(a, "paths", paths);
    uc2_aset_b(a, "revert", revert);
    uc2_aset_j(a, "exc", exc);
    if (exc) json_free(exc);
    json_t *r = uc2_call(sp, "checkout_move.exit", a);
    json_free(a);
    return r;
}
static json_t *uc2_restore_stashed_changes(const uc2_seams_t *sp, const json_t *git_cmd, const char *root,
                                           const json_t *ref, bool prompt_user, const json_t *gw_input_fn,
                                           const json_t *checkout_move) {
    json_t *a = json_object();
    uc2_aset_j(a, "git_cmd", git_cmd);
    uc2_aset_s(a, "root", root);
    uc2_aset_j(a, "ref", ref);
    uc2_aset_b(a, "prompt_user", prompt_user);
    json_t *fn = uc2_input_fn_proj(gw_input_fn);
    uc2_aset_j(a, "input_fn", fn);
    json_free(fn);
    uc2_aset_j(a, "checkout_move", checkout_move);
    json_t *r = uc2_call(sp, "restore_stashed_changes", a);
    json_free(a);
    return r;
}
static json_t *uc2_sync_with_upstream(const uc2_seams_t *sp, const json_t *git_cmd, const char *root,
                                      bool assume_yes, const json_t *gw_input_fn, const json_t *checkout_move) {
    json_t *a = json_object();
    uc2_aset_j(a, "git_cmd", git_cmd);
    uc2_aset_s(a, "root", root);
    uc2_aset_b(a, "assume_yes", assume_yes);
    json_t *fn = uc2_input_fn_proj(gw_input_fn);
    uc2_aset_j(a, "input_fn", fn);
    json_free(fn);
    uc2_aset_j(a, "checkout_move", checkout_move);
    json_t *r = uc2_call(sp, "sync_with_upstream_if_needed", a);
    json_free(a);
    return r;
}
static json_t *uc2_stash_preserved_notice(const uc2_seams_t *sp, const json_t *ref) {
    /* print(ℹ️ Local changes preserved...) + print(Restore manually...) + clear. */
    char *m1 = uc2_xsf("  " UC2_INFOVS "  Local changes preserved in stash (ref: %s)", uc2_jstr_def(ref, ""));
    json_t *pr = uc2_print(sp, m1); free(m1);
    if (uc2_is_raise(pr)) return pr;
    json_free(pr);
    pr = uc2_print(sp, "  Restore manually with: git stash apply");
    if (uc2_is_raise(pr)) return pr;
    json_free(pr);
    json_t *cr = uc2_call0(sp, "clear_pending_autostash");
    if (uc2_is_raise(cr)) return cr;
    json_free(cr);
    return NULL;
}
/* ===================================================================
 * _source_completion_request  (update_cmd.py:691)
 * =================================================================== */
/* PoP: uc2_source_completion_request @ hermes_cli/update_cmd.py:_source_completion_request */
static json_t *uc2_source_completion_request(const uc2_seams_t *sp, const json_t *opts, const json_t *plan,
                                             const json_t *snapshot_id, const json_t *windows_resume,
                                             bool desktop, bool gateway_mode) {
    json_t *out = NULL, *current = NULL, *root = NULL, *resolved = NULL, *home = NULL;
    json_t *sib = NULL, *plan_d = NULL, *rdata = NULL, *assume = NULL, *prever = NULL, *nogw = NULL;
    json_t *ret = NULL;
    current = uc2_call0(sp, "receipt_current_get");
    if (uc2_is_raise(current)) { ret = current; current = NULL; goto done; }
    if (uc2_is_null(current)) {
        json_t *b = uc2_call0(sp, "begin_update_receipt");
        if (uc2_is_raise(b)) { ret = b; goto done; }
        json_free(b);
        json_free(current);
        current = uc2_call0(sp, "receipt_current_get");
        if (uc2_is_raise(current)) { ret = current; current = NULL; goto done; }
    }
    out = json_object();
    json_set(out, "schema", json_number(1));
    /* source = str(_m().PROJECT_ROOT.resolve()) */
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    json_t *pa = json_object();
    uc2_aset_s(pa, "path", uc2_jstr_def(root, ""));
    resolved = uc2_call(sp, "path_resolve", pa);
    json_free(pa);
    if (uc2_is_raise(resolved)) { ret = resolved; resolved = NULL; goto done; }
    uc2_aset_s(out, "source", uc2_jstr_def(resolved, ""));
    /* home = str(get_hermes_home()) */
    home = uc2_call0(sp, "get_hermes_home");
    if (uc2_is_raise(home)) { ret = home; home = NULL; goto done; }
    char *hs = uc2_pystr(home);
    uc2_aset_s(out, "home", hs); free(hs);
    uc2_aset_s(out, "branch", "main");
    uc2_aset_b(out, "desktop", desktop);
    assume = uc2_attr(opts, "assume_yes");
    if (uc2_is_raise(assume)) { ret = assume; assume = NULL; goto done; }
    uc2_aset_j(out, "assume_yes", assume);
    uc2_aset_b(out, "gateway_mode", gateway_mode);
    json_t *fdef = json_bool(false);
    nogw = uc2_getattr(opts, "no_gateway_restart", fdef);
    json_free(fdef);
    uc2_aset_j(out, "no_gateway_restart", nogw);
    prever = uc2_attr(opts, "pre_update_version");
    if (uc2_is_raise(prever)) { ret = prever; prever = NULL; goto done; }
    uc2_aset_j(out, "pre_update_version", prever);
    uc2_aset_j(out, "snapshot_id", snapshot_id);
    sib = uc2_call0(sp, "last_sibling_snapshots");
    if (uc2_is_raise(sib)) { ret = sib; sib = NULL; goto done; }
    uc2_aset_j(out, "sibling_snapshots", sib);
    if (!uc2_is_null(plan)) {
        plan_d = uc2_call0(sp, "plan_to_dict");
        if (uc2_is_raise(plan_d)) { ret = plan_d; plan_d = NULL; goto done; }
        uc2_aset_j(out, "plan", plan_d);
    } else uc2_aset_j(out, "plan", NULL);
    rdata = uc2_attr(current, "data");
    if (uc2_is_raise(rdata)) { ret = rdata; rdata = NULL; goto done; }
    uc2_aset_j(out, "receipt", rdata);
    uc2_aset_j(out, "windows_resume", windows_resume);
    ret = out; out = NULL;
done:
    if (out) json_free(out);
    if (current) json_free(current);
    if (root) json_free(root);
    if (resolved) json_free(resolved);
    if (home) json_free(home);
    if (sib) json_free(sib);
    if (plan_d) json_free(plan_d);
    if (rdata) json_free(rdata);
    if (assume) json_free(assume);
    if (prever) json_free(prever);
    if (nogw) json_free(nogw);
    return ret;
}

/* ===================================================================
 * _complete_source_update  (update_cmd.py:743)  — mutates request
 * =================================================================== */
/* PoP: uc2_complete_source_update @ hermes_cli/update_cmd.py:_complete_source_update */
static json_t *uc2_complete_source_update(const uc2_seams_t *sp, json_t *request) {
    json_t *ret = NULL, *unrestored = NULL, *current = NULL, *result = NULL, *tmp = NULL;
    json_t *uid = NULL, *token = NULL, *resumed = NULL;
    bool lost = false;
    unrestored = uc2_call0(sp, "unrestored_autostash_notice");
    if (uc2_is_raise(unrestored)) { ret = unrestored; unrestored = NULL; goto done; }
    if (uc2_is_null(request)) {
        if (uc2_jtruth(unrestored)) {
            json_t *pr = uc2_print(sp, uc2_jstr_def(unrestored, ""));
            if (uc2_is_raise(pr)) { ret = pr; goto done; }
            json_free(pr);
        }
        json_t *a = json_object();
        uc2_aset_b(a, "incomplete", true);
        json_t *sr = uc2_call(sp, "stop_for_relaunch", a);
        json_free(a);
        if (uc2_is_raise(sr)) { ret = sr; goto done; }
        json_free(sr);
        /* live stop_for_relaunch is NoReturn; a scripted null falls through to
         * request["completion_message"] on None -> TypeError. */
        if (uc2_jtruth(unrestored)) {
            ret = uc2_typeerror("'NoneType' object does not support item assignment");
            goto done;
        }
        ret = uc2_attr_raise("NoneType", "get");
        goto done;
    }
    if (uc2_jtruth(unrestored))
        json_set(request, "completion_message", json_copy(unrestored));
    /* record_stage("apply", skipped/success, mode=request.get("apply_mode","git")) */
    json_t *cm = uc2_dget(request, "completion_message", NULL);
    if (uc2_is_raise(cm)) { ret = cm; goto done; }
    const char *status = uc2_jtruth(cm) ? "skipped" : "success";
    json_free(cm);
    json_t *amdef = json_string("git");
    json_t *am = uc2_dget(request, "apply_mode", amdef);
    json_free(amdef);
    if (uc2_is_raise(am)) { ret = am; goto done; }
    json_t *rsa = json_object();
    uc2_aset_s(rsa, "stage", "apply");
    uc2_aset_s(rsa, "status", status);
    uc2_aset_j(rsa, "mode", am);
    json_free(am);
    tmp = uc2_call(sp, "record_stage", rsa);
    json_free(rsa);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    /* publish_stage("Updating dependencies") */
    json_t *psa = json_object();
    uc2_aset_s(psa, "stage", "Updating dependencies");
    tmp = uc2_call(sp, "publish_stage", psa);
    json_free(psa);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    current = uc2_call0(sp, "receipt_current_get");
    if (uc2_is_raise(current)) { ret = current; current = NULL; goto done; }
    if (!uc2_is_null(current)) {
        json_t *data = uc2_attr(current, "data");
        if (uc2_is_raise(data)) { ret = data; goto done; }
        json_set(request, "receipt", data);
    }
    /* _write_fleet_restart_pending_marker(expected_sha=request.get("expected_sha")
     *     or _current_checkout_sha() or "") */
    json_t *es = uc2_dget(request, "expected_sha", NULL);
    if (uc2_is_raise(es)) { ret = es; goto done; }
    const char *expected = NULL;
    char *esbuf = NULL;
    if (uc2_jtruth(es)) { esbuf = uc2_pystr(es); expected = esbuf; }
    else {
        json_t *ccs = uc2_call0(sp, "current_checkout_sha");
        if (uc2_is_raise(ccs)) { json_free(es); ret = ccs; goto done; }
        if (uc2_jtruth(ccs)) { esbuf = uc2_pystr(ccs); expected = esbuf; }
        else expected = "";
        json_free(ccs);
    }
    json_free(es);
    json_t *wfa = json_object();
    uc2_aset_s(wfa, "expected_sha", expected);
    tmp = uc2_call(sp, "write_fleet_restart_pending_marker", wfa);
    json_free(wfa); free(esbuf);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    /* run_completion(request) with KeyboardInterrupt / Exception handlers */
    json_t *rca = json_object();
    uc2_aset_j(rca, "request", request);
    result = uc2_call(sp, "run_completion", rca);
    json_free(rca);
    if (uc2_is_raise(result)) {
        if (uc2_caught(result, "KeyboardInterrupt")) {
            json_free(result); result = NULL;
            json_t *fa = json_object();
            uc2_aset_s(fa, "message", "KeyboardInterrupt: interrupted after the code was updated");
            tmp = uc2_call(sp, "finalize_interrupted_update_receipt", fa);
            json_free(fa);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
            ret = uc2_raise_spec("SystemExit", "130");
            goto done;
        }
        if (uc2_is_exception(result)) {
            char *es2 = uc2_exc_str(result);
            char *m = uc2_xsf("%s: %s", uc2_raise_class(result), es2);
            free(es2);
            json_free(result); result = NULL;
            json_t *sa = json_object();
            uc2_aset_j(sa, "request", request);
            uc2_aset_s(sa, "message", m);
            free(m);
            result = uc2_call(sp, "settle_lost_completion", sa);
            json_free(sa);
            if (uc2_is_raise(result)) { ret = result; result = NULL; goto done; }
            lost = true;
        } else { ret = result; result = NULL; goto done; }  /* BaseException passthrough */
    }
    /* if not lost and result.get("receipt") is None and result["exit_code"] == 130 */
    json_t *rcpt = uc2_dget(result, "receipt", NULL);
    if (uc2_is_raise(rcpt)) { ret = rcpt; goto done; }
    json_t *ec = uc2_sub(result, "exit_code");
    if (uc2_is_raise(ec)) { json_free(rcpt); ret = ec; goto done; }
    double ecv = uc2_is_num(ec) ? ec->num_val : 0.0;
    if (!lost && uc2_is_null(rcpt) && ecv == 130.0) {
        json_t *fa = json_object();
        uc2_aset_s(fa, "message", "the completion was interrupted after the code was updated");
        tmp = uc2_call(sp, "finalize_interrupted_update_receipt", fa);
        json_free(fa);
        if (uc2_is_raise(tmp)) { json_free(rcpt); json_free(ec); ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    } else if (!lost && uc2_is_null(rcpt) && uc2_jtruth(ec)) {
        json_t *err = uc2_dget(result, "error", NULL);
        if (uc2_is_raise(err)) { json_free(rcpt); json_free(ec); ret = err; goto done; }
        char *msg;
        if (uc2_jtruth(err)) msg = uc2_pystr(err);
        else { char *ecs = uc2_pystr(ec); msg = uc2_xsf("the completion process exited %s", ecs); free(ecs); }
        json_free(err);
        json_t *sa = json_object();
        uc2_aset_j(sa, "request", request);
        uc2_aset_s(sa, "message", msg);
        free(msg);
        json_free(result);
        result = uc2_call(sp, "settle_lost_completion", sa);
        json_free(sa);
        if (uc2_is_raise(result)) { json_free(rcpt); json_free(ec); ret = result; result = NULL; goto done; }
        lost = true;
    }
    json_free(rcpt);
    /* result may have been reassigned by settle_lost_completion: re-read exit_code */
    json_free(ec);
    ec = uc2_sub(result, "exit_code");
    if (uc2_is_raise(ec)) { ret = ec; ec = NULL; goto done; }
    /* _accept_completion_pm_receipt(result.get("pm_receipt"), request["receipt"]["update_id"]) */
    json_t *pm = uc2_dget(result, "pm_receipt", NULL);
    if (uc2_is_raise(pm)) { json_free(ec); ret = pm; goto done; }
    json_t *rq_receipt = uc2_sub(request, "receipt");
    if (uc2_is_raise(rq_receipt)) { json_free(pm); json_free(ec); ret = rq_receipt; goto done; }
    uid = uc2_sub(rq_receipt, "update_id");
    json_free(rq_receipt);
    if (uc2_is_raise(uid)) { json_free(pm); json_free(ec); ret = uid; uid = NULL; goto done; }
    json_t *aca = json_object();
    uc2_aset_j(aca, "pm_receipt", pm);
    uc2_aset_j(aca, "update_id", uid);
    json_free(pm);
    tmp = uc2_call(sp, "accept_completion_pm_receipt", aca);
    json_free(aca);
    if (uc2_is_raise(tmp)) { json_free(ec); ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    /* if result.get("receipt") is not None: adopt_terminal_receipt + reset */
    rcpt = uc2_dget(result, "receipt", NULL);
    if (uc2_is_raise(rcpt)) { json_free(ec); ret = rcpt; goto done; }
    if (!uc2_is_null(rcpt)) {
        json_t *ata = json_object();
        uc2_aset_j(ata, "receipt", rcpt);
        tmp = uc2_call(sp, "adopt_terminal_receipt", ata);
        json_free(ata);
        if (uc2_is_raise(tmp)) { json_free(rcpt); json_free(ec); ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        json_free(current);
        current = uc2_call0(sp, "receipt_current_get");
        if (uc2_is_raise(current)) { json_free(rcpt); json_free(ec); ret = current; current = NULL; goto done; }
        if (!uc2_is_null(current)) {
            json_t *ct = uc2_attr(current, "current_token");
            if (uc2_is_raise(ct)) { json_free(rcpt); json_free(ec); ret = ct; goto done; }
            json_t *rsta = json_object();
            uc2_aset_j(rsta, "token", ct);
            json_free(ct);
            tmp = uc2_call(sp, "receipt_current_reset", rsta);
            json_free(rsta);
            if (uc2_is_raise(tmp)) { json_free(rcpt); json_free(ec); ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
    }
    json_free(rcpt);
    /* token = request["windows_resume"] */
    token = uc2_sub(request, "windows_resume");
    if (uc2_is_raise(token)) { json_free(ec); ret = token; token = NULL; goto done; }
    json_t *rw = uc2_dget(result, "windows_resume", NULL);
    if (uc2_is_raise(rw)) { json_free(ec); ret = rw; goto done; }
    if (!uc2_is_null(token) && !uc2_is_null(rw)) {
        /* resumed = dict(result["windows_resume"]); token.clear(); token.update(resumed) */
        json_t *toklive = json_obj_get(request, "windows_resume");
        uc2_obj_clear(toklive);
        if (uc2_is_obj(rw))
            for (size_t i = 0; i < rw->c.count; i++)
                json_set(toklive, rw->c.keys[i], json_copy(rw->c.items[i]));
        json_free(rw); rw = NULL;
        json_t *sr = uc2_settle_windows_resume(sp, request);
        if (uc2_is_raise(sr)) { json_free(ec); ret = sr; goto done; }
        json_free(sr);
    } else {
        json_free(rw); rw = NULL;
        /* elif token and token.get("resume_needed") and not result["exit_code"] */
        bool need = false;
        if (uc2_jtruth(token)) {
            json_t *rn = uc2_dget(token, "resume_needed", NULL);
            if (uc2_is_raise(rn)) { json_free(ec); ret = rn; goto done; }
            need = uc2_jtruth(rn);
            json_free(rn);
        }
        if (need && !uc2_jtruth(ec)) {
            json_t *ra = json_object();
            uc2_aset_j(ra, "token", token);
            tmp = uc2_call(sp, "resume_windows_gateways_after_update", ra);
            json_free(ra);
            if (uc2_is_raise(tmp)) {
                if (uc2_is_exception(tmp)) {
                    char *es3 = uc2_exc_str(tmp);
                    char *m = uc2_xsf("Windows gateway recovery failed: %s", es3);
                    free(es3);
                    json_free(tmp); tmp = NULL;
                    json_t *or = uc2_owe_followup(sp, uid, "windows_resume", m, NULL);
                    free(m);
                    if (uc2_is_raise(or)) { json_free(ec); ret = or; goto done; }
                    json_free(or);
                } else { json_free(ec); ret = tmp; tmp = NULL; goto done; }
            } else json_free(tmp); tmp = NULL;
            json_t *sr = uc2_settle_windows_resume(sp, request);
            if (uc2_is_raise(sr)) { json_free(ec); ret = sr; goto done; }
            json_free(sr);
        }
    }
    /* if result["exit_code"]: raise SystemExit(exit_code) */
    if (uc2_jtruth(ec)) {
        char *ecs = uc2_pystr(ec);
        ret = uc2_raise_spec("SystemExit", ecs);
        free(ecs);
        json_free(ec); ec = NULL;
        goto done;
    }
    json_free(ec); ec = NULL;
    if (lost) { ret = NULL; goto done; }
    /* try: if adopt_retired_channel(request): print(... channel_retirement ...)
     * except Exception: owe_followup(channel_adoption). The print's subscripts
     * are INSIDE the try, so a KeyError there is owed, not propagated. */
    json_t *adopt_raise = NULL;
    json_t *ara = json_object();
    uc2_aset_j(ara, "request", request);
    tmp = uc2_call(sp, "adopt_retired_channel", ara);
    json_free(ara);
    if (uc2_is_raise(tmp)) { adopt_raise = tmp; tmp = NULL; }
    else if (uc2_jtruth(tmp)) {
        json_t *cr = uc2_sub(request, "channel_retirement");
        if (uc2_is_raise(cr)) { adopt_raise = cr; }
        else {
            json_t *dest = uc2_sub(cr, "destination");
            json_free(cr);
            if (uc2_is_raise(dest)) { adopt_raise = dest; }
            else {
                char *ds = uc2_pystr(dest);
                char *m = uc2_xsf(UC2_ARROW " Source subscription moved to %s", ds);
                free(ds); json_free(dest);
                json_t *pr = uc2_print(sp, m); free(m);
                if (uc2_is_raise(pr)) { adopt_raise = pr; }
                else json_free(pr);
            }
        }
        json_free(tmp); tmp = NULL;
    } else { json_free(tmp); tmp = NULL; }
    if (adopt_raise) {
        if (uc2_is_exception(adopt_raise)) {
            char *es4 = uc2_exc_str(adopt_raise);
            const char *m = (es4 && es4[0]) ? es4 : uc2_raise_class(adopt_raise);
            json_t *or = uc2_owe_followup(sp, uid, "channel_adoption", m,
                                          "the next `hermes update` adopts it again");
            free(es4); json_free(adopt_raise); adopt_raise = NULL;
            if (uc2_is_raise(or)) { ret = or; goto done; }
            json_free(or);
            ret = NULL; goto done;
        }
        ret = adopt_raise; adopt_raise = NULL; goto done;  /* BaseException passthrough */
    }
    ret = NULL;
done:
    if (unrestored) json_free(unrestored);
    if (current) json_free(current);
    if (result) json_free(result);
    if (tmp) json_free(tmp);
    if (uid) json_free(uid);
    if (token) json_free(token);
    if (resumed) json_free(resumed);
    return ret;
}
/* returncode as a number (0 for NULL/non-num); internal tracking helper. */
static double uc2_rc_val(const json_t *rec) {
    if (!uc2_is_obj(rec)) return 0.0;
    const json_t *f = json_obj_get(rec, "returncode");
    return uc2_is_num(f) ? f->num_val : 0.0;
}
/* rec.<key>.strip() (no or-collapse); null/non-str -> AttributeError 'strip'. */
static json_t *uc2_field_strip(const json_t *rec, const char *key) {
    json_t *f = uc2_rec_field(rec, key);
    if (uc2_is_raise(f)) return f;
    if (!uc2_is_str(f)) {
        json_t *e = uc2_attr_raise(uc2_typename(f), "strip");
        json_free(f); return e;
    }
    char *s = uc2_strip(f->str_val ? f->str_val : "");
    json_free(f);
    json_t *r = json_string(s); free(s);
    return r;
}
/* synthetic CompletedProcess record {returncode, stdout, stderr}. */
static json_t *uc2_completed(double rc, const char *out, const char *err) {
    json_t *o = json_object();
    uc2_aset_n(o, "returncode", rc);
    uc2_aset_s(o, "stdout", out);
    uc2_aset_s(o, "stderr", err);
    return o;
}

/* ===================================================================
 * _reconcile_diverged_checkout  (update_cmd.py:819)
 * =================================================================== */
/* PoP: uc2_reconcile_diverged_checkout @ hermes_cli/update_cmd.py:_reconcile_diverged_checkout */
static json_t *uc2_reconcile_diverged_checkout(const uc2_seams_t *sp, const json_t *git_cmd,
                                               const char *branch, const json_t *pre_pull_sha,
                                               const json_t *target_ref) {
    json_t *ret = NULL, *rec = NULL, *s = NULL, *root = NULL, *tmp = NULL, *rcf = NULL;
    char *merge_ref = uc2_is_null(target_ref)
        ? uc2_xsf("origin/%s", branch ? branch : "")
        : strdup(uc2_jstr_def(target_ref, ""));
    const char *br = branch ? branch : "";
    json_t *a = json_array();
    json_append(a, json_string("branch")); json_append(a, json_string("--show-current"));
    rec = uc2_git_run(sp, git_cmd, a, false); json_free(a);
    if (uc2_is_raise(rec)) { ret = rec; rec = NULL; goto done; }
    s = uc2_or_strip(rec, "stdout", NULL); json_free(rec); rec = NULL;
    if (uc2_is_raise(s)) { ret = s; s = NULL; goto done; }
    const char *cur = uc2_jstr_def(s, "");
    if (cur[0] && strcmp(cur, br) != 0) {
        char *m = uc2_xsf("  " UC2_WARN " Checkout is on custom branch '%s' " UC2_EMDASH
                          " merging origin/%s instead of resetting so local commits survive...", cur, br);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        json_t *ta = json_object(); uc2_aset_s(ta, "fmt", "%Y%m%d-%H%M%S");
        json_t *ts = uc2_call(sp, "time_strftime", ta); json_free(ta);
        if (uc2_is_raise(ts)) { ret = ts; goto done; }
        char *tag = uc2_xsf("pre-update-%s", uc2_jstr_def(ts, "")); json_free(ts);
        json_t *ga = json_array();
        json_append(ga, json_string("tag")); json_append(ga, json_string(tag)); free(tag);
        rec = uc2_git_run(sp, git_cmd, ga, false); json_free(ga);
        if (uc2_is_raise(rec)) { ret = rec; rec = NULL; goto done; }
        json_free(rec); rec = NULL;
        ga = json_array();
        json_append(ga, json_string("merge")); json_append(ga, json_string("--no-edit"));
        json_append(ga, json_string(merge_ref));
        rec = uc2_git_run(sp, git_cmd, ga, false); json_free(ga);
        if (uc2_is_raise(rec)) { ret = rec; rec = NULL; goto done; }
        rcf = uc2_rec_field(rec, "returncode");
        if (uc2_is_raise(rcf)) { json_free(rec); rec = NULL; ret = rcf; rcf = NULL; goto done; }
        bool nonzero = !(uc2_is_num(rcf) && rcf->num_val == 0.0);
        json_free(rcf); rcf = NULL; json_free(rec); rec = NULL;
        if (nonzero) {
            ga = json_array();
            json_append(ga, json_string("merge")); json_append(ga, json_string("--abort"));
            rec = uc2_git_run(sp, git_cmd, ga, false); json_free(ga);
            if (uc2_is_raise(rec)) { ret = rec; rec = NULL; goto done; }
            json_free(rec); rec = NULL;
            tmp = uc2_print(sp, UC2_CROSS " Merge conflict between local commits and upstream " UC2_EMDASH " update stopped, nothing was changed.");
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
            root = uc2_project_root(sp);
            if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
            m = uc2_xsf("  Resolve manually: cd %s && git merge origin/%s", uc2_jstr_def(root, ""), br);
            tmp = uc2_print(sp, m); free(m);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp);
            tmp = uc2_print(sp, "  Then re-run the update. Local work is untouched.");
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp);
            tmp = uc2_record_stop(sp, "merge_conflict", NULL);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
            ret = uc2_raise_spec("SystemExit", "1"); goto done;
        }
        ret = NULL; goto done;
    }
    a = json_array();
    json_append(a, json_string("merge-base")); json_append(a, json_string("HEAD"));
    json_append(a, json_string(merge_ref));
    rec = uc2_git_run(sp, git_cmd, a, false); json_free(a);
    if (uc2_is_raise(rec)) { ret = rec; rec = NULL; goto done; }
    rcf = uc2_rec_field(rec, "returncode");
    if (uc2_is_raise(rcf)) { json_free(rec); rec = NULL; ret = rcf; rcf = NULL; goto done; }
    bool rc0 = uc2_is_num(rcf) && rcf->num_val == 0.0;
    json_free(rcf); rcf = NULL;
    bool has_common = false;
    if (rc0) {
        s = uc2_stdout_strip(rec);
        if (uc2_is_raise(s)) { json_free(rec); rec = NULL; ret = s; s = NULL; goto done; }
        has_common = uc2_jtruth(s);
        json_free(s); s = NULL;
    }
    json_free(rec); rec = NULL;
    if (uc2_jtruth(pre_pull_sha)) {
        const char *pre = uc2_jstr_def(pre_pull_sha, "");
        const char *kind = has_common ? "diverged" : "orphan";
        json_t *ua = json_object(); uc2_aset_s(ua, "fmt", "%Y%m%d-%H%M%S");
        json_t *us = uc2_call(sp, "utc_now_strftime", ua); json_free(ua);
        if (uc2_is_raise(us)) { ret = us; goto done; }
        char *pre12 = uc2_slice(pre, 12);
        char *rescue_ref = uc2_xsf("refs/hermes-update-backups/%s-%s-%s-%s", kind, br, uc2_jstr_def(us, ""), pre12);
        free(pre12); json_free(us);
        char *head = has_common
            ? uc2_xsf("  " UC2_WARN " Local history has diverged from origin/%s " UC2_EMDASH " ", br)
            : uc2_xsf("  " UC2_WARN " Local history shares no common ancestor with origin/%s (orphan divergence) " UC2_EMDASH " ", br);
        json_t *ga = json_array();
        json_append(ga, json_string("update-ref")); json_append(ga, json_string(rescue_ref));
        json_append(ga, json_string(pre));
        rec = uc2_git_run(sp, git_cmd, ga, false); json_free(ga);
        if (uc2_is_raise(rec)) { free(rescue_ref); free(head); ret = rec; rec = NULL; goto done; }
        rcf = uc2_rec_field(rec, "returncode");
        if (uc2_is_raise(rcf)) { json_free(rec); rec = NULL; free(rescue_ref); free(head); ret = rcf; rcf = NULL; goto done; }
        bool urc0 = uc2_is_num(rcf) && rcf->num_val == 0.0;
        json_free(rcf); rcf = NULL; json_free(rec); rec = NULL;
        if (urc0) {
            char *m = uc2_xsf("%sbacked up current HEAD to %s before resetting. This backup expires after %d days.",
                              head, rescue_ref, UC2_ORPHAN_RESCUE_REF_MAX_AGE_DAYS);
            tmp = uc2_print(sp, m); free(m);
            if (uc2_is_raise(tmp)) { free(rescue_ref); free(head); ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
            if (has_common) {
                char *spec = uc2_xsf("origin/%s..%s", br, pre);
                ga = json_array();
                json_append(ga, json_string("rev-list")); json_append(ga, json_string("--count"));
                json_append(ga, json_string(spec)); free(spec);
                rec = uc2_git_run(sp, git_cmd, ga, false); json_free(ga);
                if (uc2_is_raise(rec)) { free(rescue_ref); free(head); ret = rec; rec = NULL; goto done; }
                s = uc2_or_strip(rec, "stdout", NULL); json_free(rec); rec = NULL;
                if (uc2_is_raise(s)) { free(rescue_ref); free(head); ret = s; s = NULL; goto done; }
                const char *dv = uc2_jtruth(s) ? uc2_jstr_def(s, "") : "Some";
                m = uc2_xsf("    %s commit(s) not on origin/%s leave the branch; list them with: git log origin/%s..%s",
                            dv, br, br, rescue_ref);
                tmp = uc2_print(sp, m); free(m);
                if (uc2_is_raise(tmp)) { free(rescue_ref); free(head); ret = tmp; tmp = NULL; goto done; }
                json_free(tmp); tmp = NULL;
            }
        } else {
            char *m = uc2_xsf("%sattempted to back up current HEAD to %s before resetting, but the backup write failed (pre-reset SHA was %s).",
                              head, rescue_ref, pre);
            tmp = uc2_print(sp, m); free(m);
            if (uc2_is_raise(tmp)) { free(rescue_ref); free(head); ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
        free(rescue_ref); free(head);
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        json_t *pra = json_object();
        uc2_aset_j(pra, "git_cmd", git_cmd);
        uc2_aset_s(pra, "root", uc2_jstr_def(root, ""));
        uc2_aset_s(pra, "branch", br);
        tmp = uc2_call(sp, "prune_orphan_rescue_refs", pra); json_free(pra);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    }
    tmp = uc2_print(sp, "  " UC2_WARN " Fast-forward not possible (history diverged), resetting to match remote...");
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    a = json_array();
    json_append(a, json_string("reset")); json_append(a, json_string("--hard"));
    json_append(a, json_string(merge_ref));
    rec = uc2_git_run(sp, git_cmd, a, false); json_free(a);
    if (uc2_is_raise(rec)) { ret = rec; rec = NULL; goto done; }
    rcf = uc2_rec_field(rec, "returncode");
    if (uc2_is_raise(rcf)) { json_free(rec); rec = NULL; ret = rcf; rcf = NULL; goto done; }
    bool reset_bad = !(uc2_is_num(rcf) && rcf->num_val == 0.0);
    json_free(rcf); rcf = NULL;
    if (reset_bad) {
        char *m = uc2_xsf(UC2_CROSS " Failed to reset to origin/%s.", br);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { json_free(rec); rec = NULL; ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        s = uc2_field_strip(rec, "stderr");
        if (uc2_is_raise(s)) { json_free(rec); rec = NULL; ret = s; s = NULL; goto done; }
        if (uc2_jtruth(s)) {
            m = uc2_xsf("  %s", uc2_jstr_def(s, ""));
            tmp = uc2_print(sp, m); free(m);
            if (uc2_is_raise(tmp)) { json_free(rec); rec = NULL; ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
        json_free(s); s = NULL;
        m = uc2_xsf("  Try manually: git fetch origin && git reset --hard origin/%s", br);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { json_free(rec); rec = NULL; ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        json_free(rec); rec = NULL;
        tmp = uc2_record_stop(sp, "checkout_move_failed", NULL);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        ret = uc2_raise_spec("SystemExit", "1"); goto done;
    }
    json_free(rec); rec = NULL;
    ret = NULL;
done:
    free(merge_ref);
    if (rec) json_free(rec);
    if (s) json_free(s);
    if (root) json_free(root);
    if (tmp) json_free(tmp);
    if (rcf) json_free(rcf);
    return ret;
}

/* ===================================================================
 * _roll_back_broken_update  (update_cmd.py:898)  — NoReturn
 * =================================================================== */
/* PoP: uc2_roll_back_broken_update @ hermes_cli/update_cmd.py:_roll_back_broken_update */
static json_t *uc2_roll_back_broken_update(const uc2_seams_t *sp, const json_t *git_cmd,
                                           const json_t *pre_pull_sha, const char *failing_path,
                                           const json_t *syntax_error, const json_t *rollback_branch,
                                           const char *header, const json_t *token, const char *stop_class) {
    json_t *ret = NULL, *tmp = NULL, *root = NULL, *rec = NULL, *rr = NULL, *rargs = NULL;
    json_t *added = NULL, *targets = NULL, *paths = NULL, *body_raise = NULL, *exitr = NULL;
    json_t *tsha = NULL;
    char *root_s = NULL;
    tmp = uc2_print(sp, ""); if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; } json_free(tmp);
    tmp = uc2_print(sp, header ? header : ""); if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; } json_free(tmp); tmp = NULL;
    char *fp = uc2_xsf("  %s", failing_path ? failing_path : "");
    tmp = uc2_print(sp, fp); free(fp); if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; } json_free(tmp); tmp = NULL;
    if (uc2_jtruth(syntax_error)) {
        char *se = uc2_pystr(syntax_error);
        size_t nl; char **lines = uc2_splitlines(se, &nl); free(se);
        for (size_t i = 0; i < nl && i < 6; i++) {
            char *ln = uc2_xsf("    %s", lines[i]);
            tmp = uc2_print(sp, ln); free(ln);
            if (uc2_is_raise(tmp)) { uc2_free_lines(lines, nl); ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
        uc2_free_lines(lines, nl);
    }
    tmp = uc2_print(sp, ""); if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; } json_free(tmp); tmp = NULL;
    if (uc2_jtruth(pre_pull_sha)) {
        const char *pre = uc2_jstr_def(pre_pull_sha, "");
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        root_s = strdup(uc2_jstr_def(root, ""));
        const char *rb = uc2_jstr(rollback_branch);
        bool parked = !uc2_is_null(rollback_branch) && !(rb && !strcmp(rb, "HEAD"));
        char *rollto = parked ? strdup(rb ? rb : "") : uc2_slice(pre, 10);
        char *m = uc2_xsf(UC2_ARROW " Rolling back to %s...", rollto); free(rollto);
        tmp = uc2_print(sp, m); free(m); if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; } json_free(tmp); tmp = NULL;
        const char *mode = uc2_is_null(rollback_branch) ? "branch" : "detach";
        tsha = uc2_capture_head_sha(sp, git_cmd, root_s);
        if (uc2_is_raise(tsha)) { ret = tsha; tsha = NULL; goto done; }
        json_t *ama = json_object();
        uc2_aset_j(ama, "git_cmd", git_cmd);
        uc2_aset_s(ama, "root", root_s);
        uc2_aset_j(ama, "pre", pre_pull_sha);
        uc2_aset_j(ama, "target", tsha);
        uc2_aset_j(ama, "stash", NULL);
        uc2_aset_s(ama, "rollback", mode);
        tmp = uc2_call(sp, "arm_tree_move", ama); json_free(ama);
        if (uc2_is_raise(tmp)) {
            if (uc2_caught(tmp, "OSError") || uc2_caught(tmp, "SubprocessError")) {
                char *es = uc2_exc_str(tmp);
                m = uc2_xsf("  " UC2_WARN " Could not write the interrupted-pull marker (%s); rolling back without it.", es);
                free(es); json_free(tmp); tmp = NULL;
                json_t *pr = uc2_print(sp, m); free(m);
                if (uc2_is_raise(pr)) { ret = pr; goto done; }
                json_free(pr);
            } else { ret = tmp; tmp = NULL; goto done; }
        } else json_free(tmp); tmp = NULL;
        targets = json_array(); json_append(targets, json_copy(pre_pull_sha));
        paths = json_array();
        tmp = uc2_cm_enter(sp, token, targets, paths, false);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        json_t *fa = json_object();
        uc2_aset_j(fa, "git_cmd", git_cmd);
        uc2_aset_s(fa, "root", root_s);
        uc2_aset_j(fa, "pre", pre_pull_sha);
        uc2_aset_j(fa, "target", tsha);
        added = uc2_call(sp, "files_added_by", fa); json_free(fa);
        if (uc2_is_raise(added)) { body_raise = added; added = NULL; goto cm_exit; }
        if (!strcmp(mode, "detach")) {
            rargs = json_array();
            json_append(rargs, json_string("update-ref")); json_append(rargs, json_string("--no-deref"));
            json_append(rargs, json_string("HEAD")); json_append(rargs, json_string(pre));
            rr = uc2_git_run(sp, git_cmd, rargs, false);
            if (uc2_is_raise(rr)) { body_raise = rr; rr = NULL; goto cm_exit; }
        }
        if (!strcmp(mode, "branch") || uc2_rc_val(rr) == 0.0) {
            if (rargs) json_free(rargs);
            rargs = json_array();
            json_append(rargs, json_string("reset")); json_append(rargs, json_string("-q"));
            json_append(rargs, json_string(pre));
            if (rr) json_free(rr);
            rr = uc2_git_run(sp, git_cmd, rargs, false);
            if (uc2_is_raise(rr)) { body_raise = rr; rr = NULL; goto cm_exit; }
        }
        if (uc2_rc_val(rr) == 0.0) {
            if (rargs) json_free(rargs);
            rargs = json_array();
            json_append(rargs, json_string("reset")); json_append(rargs, json_string("--hard"));
            json_append(rargs, json_string(pre));
            if (rr) json_free(rr);
            rr = uc2_git_run(sp, git_cmd, rargs, false);
            if (uc2_is_raise(rr)) { body_raise = rr; rr = NULL; goto cm_exit; }
            if (uc2_rc_val(rr) == 0.0) {
                json_t *da = json_object();
                uc2_aset_s(da, "root", root_s);
                uc2_aset_j(da, "added", added);
                tmp = uc2_call(sp, "drop_added_files", da); json_free(da);
                if (uc2_is_raise(tmp)) { body_raise = tmp; tmp = NULL; goto cm_exit; }
                bool dropped = uc2_jtruth(tmp); json_free(tmp); tmp = NULL;
                if (!dropped) {
                    json_free(rr);
                    rr = uc2_completed(1, "", "could not remove the broken release's added files");
                }
            }
        }
        if (uc2_rc_val(rr) == 0.0) {
            json_t *tw = json_object();
            uc2_aset_j(tw, "git_cmd", git_cmd);
            uc2_aset_s(tw, "root", root_s);
            uc2_aset_j(tw, "sha", pre_pull_sha);
            tmp = uc2_call(sp, "tree_whole_at", tw); json_free(tw);
            if (uc2_is_raise(tmp)) { body_raise = tmp; tmp = NULL; goto cm_exit; }
            bool whole = uc2_jtruth(tmp); json_free(tmp); tmp = NULL;
            if (!whole) {
                char *pre10 = uc2_slice(pre, 10);
                char *em = uc2_xsf("the checkout is not whole at %s", pre10); free(pre10);
                json_free(rr);
                rr = uc2_completed(1, "", em); free(em);
            }
        }
        if (uc2_rc_val(rr) != 0.0) {
            tmp = uc2_settle_failed_tree_move(sp, root_s);
            if (uc2_is_raise(tmp)) { body_raise = tmp; tmp = NULL; goto cm_exit; }
            bool settled = uc2_jtruth(tmp); json_free(tmp); tmp = NULL;
            if (settled) { json_free(rr); rr = uc2_completed(0, "", ""); }
        }
        if (uc2_rc_val(rr) == 0.0 && parked) {
            json_t *ca = json_array();
            json_append(ca, json_string("checkout")); json_append(ca, json_string(rb ? rb : ""));
            rec = uc2_git_run(sp, git_cmd, ca, false); json_free(ca);
            if (uc2_is_raise(rec)) { body_raise = rec; rec = NULL; goto cm_exit; }
            if (uc2_rc_val(rec) != 0.0) {
                m = uc2_xsf("  " UC2_CROSS " Could not check out %s; restoring its commit detached.", rb ? rb : "");
                json_t *pr = uc2_print(sp, m); free(m);
                if (uc2_is_raise(pr)) { body_raise = pr; goto cm_exit; }
                json_free(pr);
            }
            json_free(rec); rec = NULL;
        }
cm_exit:
        exitr = uc2_cm_exit(sp, token, targets, paths, false, body_raise);
        if (uc2_is_raise(exitr)) { ret = exitr; exitr = NULL; goto done; }
        json_free(exitr); exitr = NULL;
        if (body_raise) { ret = body_raise; body_raise = NULL; goto done; }
        if (uc2_rc_val(rr) == 0.0) {
            json_t *mk = uc2_marker(sp, root_s);
            if (uc2_is_raise(mk)) { ret = mk; goto done; }
            json_t *ul = uc2_marker_unlink(sp, uc2_jstr_def(mk, "")); json_free(mk);
            if (uc2_is_raise(ul)) { ret = ul; goto done; }
            json_free(ul);
            tmp = uc2_disarm(sp);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp);
            tmp = uc2_print(sp, "  " UC2_CHECK " Rollback complete " UC2_EMDASH " your install is unchanged.");
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp);
            tmp = uc2_print(sp, "  Try ``hermes update`` again later once a fix lands.");
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        } else {
            tmp = uc2_print(sp, "  " UC2_CROSS " Rollback failed. Recover manually with:");
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
            char *joined = uc2_shlex_join(rargs);
            m = uc2_xsf("    cd %s && git %s", root_s, joined); free(joined);
            tmp = uc2_print(sp, m); free(m);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
            json_t *se = uc2_field_strip(rr, "stderr");
            if (uc2_is_raise(se)) { ret = se; goto done; }
            if (uc2_jtruth(se)) {
                char *fl = uc2_first_line(uc2_jstr_def(se, ""));
                m = uc2_xsf("    (%s)", fl ? fl : ""); free(fl);
                tmp = uc2_print(sp, m); free(m);
                if (uc2_is_raise(tmp)) { json_free(se); ret = tmp; tmp = NULL; goto done; }
                json_free(tmp); tmp = NULL;
            }
            json_free(se);
            json_t *mk = uc2_marker(sp, root_s);
            if (uc2_is_raise(mk)) { ret = mk; goto done; }
            json_t *isf = uc2_marker_is_file(sp, uc2_jstr_def(mk, "")); json_free(mk);
            if (uc2_is_raise(isf)) { ret = isf; goto done; }
            bool isfile = uc2_jtruth(isf); json_free(isf);
            if (isfile) {
                tmp = uc2_owe_restore_of(sp, root_s, pre_pull_sha);
                if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
                json_free(tmp); tmp = NULL;
                char *pre10 = uc2_slice(pre, 10);
                m = uc2_xsf("  The next `hermes` launch also finishes the rollback to %s on its own.", pre10); free(pre10);
                tmp = uc2_print(sp, m); free(m);
                if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
                json_free(tmp); tmp = NULL;
            }
        }
    } else {
        tmp = uc2_print(sp, "  Could not capture pre-pull SHA " UC2_EMDASH " recover manually with:");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        char *m = uc2_xsf("    cd %s && git reflog && git reset --hard <prev-sha>", uc2_jstr_def(root, ""));
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    }
    tmp = uc2_record_stop(sp, stop_class ? stop_class : "syntax_rollback", NULL);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    ret = uc2_raise_spec("SystemExit", "1");
done:
    if (tmp) json_free(tmp);
    if (root) json_free(root);
    free(root_s);
    if (rec) json_free(rec);
    if (rr) json_free(rr);
    if (rargs) json_free(rargs);
    if (added) json_free(added);
    if (targets) json_free(targets);
    if (paths) json_free(paths);
    if (body_raise) json_free(body_raise);
    if (exitr) json_free(exitr);
    if (tsha) json_free(tsha);
    return ret;
}

/* ===================================================================
 * _rollback_if_pulled_syntax_error  (update_cmd.py:888)
 * =================================================================== */
/* PoP: uc2_rollback_if_pulled_syntax_error @ hermes_cli/update_cmd.py:_rollback_if_pulled_syntax_error */
static json_t *uc2_rollback_if_pulled_syntax_error(const uc2_seams_t *sp, const json_t *git_cmd,
                                                   const json_t *pre_pull_sha, const json_t *rollback_branch,
                                                   const json_t *token) {
    json_t *root = uc2_project_root(sp);
    if (uc2_is_raise(root)) return root;
    json_t *cf = json_array();
    for (size_t i = 0; UC2_CRITICAL_FILES[i]; i++) json_append(cf, json_string(UC2_CRITICAL_FILES[i]));
    json_t *a = json_object();
    uc2_aset_s(a, "root", uc2_jstr_def(root, ""));
    uc2_aset_j(a, "critical_files", cf);
    json_free(cf); json_free(root);
    json_t *broken = uc2_call(sp, "tree_syntax_error", a);
    json_free(a);
    if (uc2_is_raise(broken)) return broken;
    if (uc2_is_null(broken)) { json_free(broken); return NULL; }
    if (!uc2_is_arr(broken) || json_len(broken) != 2) {
        json_free(broken);
        return uc2_typeerror("cannot unpack non-iterable or wrong-length tree_syntax_error result");
    }
    const char *fp = uc2_jstr_def(json_get(broken, 0), "");
    json_t *se = json_copy(json_get(broken, 1));
    json_free(broken);
    json_t *r = uc2_roll_back_broken_update(sp, git_cmd, pre_pull_sha, fp, se, rollback_branch,
                                            UC2_CROSS " Pulled code has a syntax error in a critical file:",
                                            token, "syntax_rollback");
    json_free(se);
    return r;
}

/* ===================================================================
 * _move_checkout_to  (update_cmd.py:1017)
 * =================================================================== */
/* PoP: uc2_move_checkout_to @ hermes_cli/update_cmd.py:_move_checkout_to */
static json_t *uc2_move_checkout_to(const uc2_seams_t *sp, const json_t *git_cmd, const char *branch,
                                    const char *merge_ref, const char *target_sha, const json_t *pre_pull_sha) {
    json_t *ret = NULL, *rec = NULL, *tmp = NULL, *root = NULL, *s = NULL, *anc = NULL;
    const char *br = branch ? branch : "";
    char *origin_ref = uc2_xsf("origin/%s", br);
    if (strcmp(merge_ref ? merge_ref : "", origin_ref) != 0) {
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        json_t *pa = json_object();
        uc2_aset_j(pa, "git_cmd", git_cmd);
        uc2_aset_s(pa, "root", uc2_jstr_def(root, ""));
        uc2_aset_s(pa, "branch", br);
        tmp = uc2_call(sp, "park_detached_head", pa); json_free(pa);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        json_t *ca = json_array();
        json_append(ca, json_string("checkout")); json_append(ca, json_string("--detach"));
        json_append(ca, json_string(target_sha ? target_sha : ""));
        rec = uc2_git_run(sp, git_cmd, ca, true); json_free(ca);
        if (uc2_is_raise(rec)) { ret = rec; rec = NULL; goto done; }
        json_free(rec); rec = NULL;
        ret = NULL; goto done;
    }
    json_t *ma = json_array();
    json_append(ma, json_string("merge")); json_append(ma, json_string("--ff-only"));
    json_append(ma, json_string(target_sha ? target_sha : ""));
    rec = uc2_git_run(sp, git_cmd, ma, false); json_free(ma);
    if (uc2_is_raise(rec)) { ret = rec; rec = NULL; goto done; }
    if (uc2_rc_val(rec) == 0.0) { json_free(rec); rec = NULL; ret = NULL; goto done; }
    json_t *aa = json_array();
    json_append(aa, json_string("merge-base")); json_append(aa, json_string("--is-ancestor"));
    json_append(aa, json_string("HEAD")); json_append(aa, json_string(target_sha ? target_sha : ""));
    anc = uc2_git_run(sp, git_cmd, aa, false); json_free(aa);
    if (uc2_is_raise(anc)) { json_free(rec); rec = NULL; ret = anc; anc = NULL; goto done; }
    double anc_rc = uc2_rc_val(anc);
    if (anc_rc == 1.0) {
        json_free(anc); anc = NULL; json_free(rec); rec = NULL;
        json_t *tref = json_string(target_sha ? target_sha : "");
        ret = uc2_reconcile_diverged_checkout(sp, git_cmd, br, pre_pull_sha, tref);
        json_free(tref);
        goto done;
    }
    tmp = uc2_print(sp, UC2_CROSS " Fast-forward failed; refusing to reset because history divergence was not proven.");
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    s = uc2_or_strip(rec, "stderr", "stdout");
    if (uc2_is_raise(s)) { ret = s; s = NULL; goto done; }
    const char *detail = uc2_jstr_def(s, "");
    if (detail[0]) {
        char *m = uc2_xsf("  %s", detail);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    }
    if (anc_rc == 0.0) {
        char *m = uc2_xsf("  HEAD is still an ancestor of %s.", merge_ref ? merge_ref : "");
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    } else {
        char *m = uc2_xsf("  Could not verify whether HEAD is an ancestor of %s.", merge_ref ? merge_ref : "");
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        json_t *ad = uc2_or_strip(anc, "stderr", "stdout");
        if (uc2_is_raise(ad)) { ret = ad; goto done; }
        if (uc2_jtruth(ad)) {
            m = uc2_xsf("  %s", uc2_jstr_def(ad, ""));
            tmp = uc2_print(sp, m); free(m);
            if (uc2_is_raise(tmp)) { json_free(ad); ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
        json_free(ad);
    }
    tmp = uc2_print(sp, "  Resolve the Git error and re-run `hermes update`; no reset was attempted.");
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    json_t *cmds = json_array(); json_append(cmds, json_string("merge"));
    json_t *ga = json_object();
    uc2_aset_s(ga, "detail", detail);
    uc2_aset_j(ga, "cmds", cmds);
    uc2_aset_n(ga, "returncode", uc2_rc_val(rec));
    json_free(cmds);
    json_t *gsc = uc2_call(sp, "git_output_stop_class", ga); json_free(ga);
    if (uc2_is_raise(gsc)) { ret = gsc; goto done; }
    const char *stop = uc2_jtruth(gsc) ? uc2_jstr_def(gsc, "") : "checkout_move_failed";
    tmp = uc2_record_stop(sp, stop, NULL);
    json_free(gsc);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    ret = uc2_raise_spec("SystemExit", "1");
done:
    free(origin_ref);
    if (rec) json_free(rec);
    if (tmp) json_free(tmp);
    if (root) json_free(root);
    if (s) json_free(s);
    if (anc) json_free(anc);
    return ret;
}
/* ===================================================================
 * _pull_updates  (update_cmd.py:1054)  — try/finally, returns baseline
 * =================================================================== */
/* PoP: uc2_pull_updates @ hermes_cli/update_cmd.py:_pull_updates */
static json_t *uc2_pull_updates(const uc2_seams_t *sp, const json_t *git_cmd, const char *branch,
                                const json_t *auto_stash_ref, bool prompt_for_restore, const json_t *gw_input_fn,
                                bool discard_local_changes, bool keep_stash, const json_t *target_ref,
                                const json_t *pre_sync_sha, const json_t *rollback_branch, bool sync_upstream,
                                bool assume_yes, bool in_place_update, const json_t *token, const json_t *expected_sha) {
    json_t *ret = NULL, *pending = NULL, *tmp = NULL, *root = NULL, *pre = NULL, *marker = NULL;
    json_t *tsha = NULL, *baseline = NULL, *refused = NULL, *targets = NULL, *paths = NULL;
    json_t *body_raise = NULL, *exitr = NULL, *mv = NULL, *sync = NULL, *cm = NULL, *hdr = NULL;
    char *merge_ref = NULL, *root_s = NULL, *marker_s = NULL;
    bool update_succeeded = false;
    const char *br = branch ? branch : "";
    /* pre_pull_sha = _capture_head_sha(git_cmd, _m().PROJECT_ROOT) */
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    pre = uc2_capture_head_sha(sp, git_cmd, uc2_jstr_def(root, ""));
    json_free(root); root = NULL;
    if (uc2_is_raise(pre)) { ret = pre; pre = NULL; goto done; }
    /* pull_marker = interrupted_pull_marker(_m().PROJECT_ROOT) */
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    marker = uc2_marker(sp, uc2_jstr_def(root, ""));
    json_free(root); root = NULL;
    if (uc2_is_raise(marker)) { ret = marker; marker = NULL; goto done; }
    marker_s = strdup(uc2_jstr_def(marker, ""));
    merge_ref = uc2_is_null(target_ref) ? uc2_xsf("origin/%s", br) : strdup(uc2_jstr_def(target_ref, ""));
    /* try: */
    char *spec = uc2_xsf("%s^{commit}", merge_ref);
    json_t *ra = json_array();
    json_append(ra, json_string("rev-parse")); json_append(ra, json_string("-q"));
    json_append(ra, json_string("--verify")); json_append(ra, json_string(spec)); free(spec);
    tmp = uc2_git_run(sp, git_cmd, ra, false); json_free(ra);
    if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
    tsha = uc2_or_strip(tmp, "stdout", NULL); json_free(tmp); tmp = NULL;
    if (uc2_is_raise(tsha)) { pending = tsha; tsha = NULL; goto finally; }
    json_t *ioa = json_object(); uc2_aset_s(ioa, "value", uc2_jstr_def(tsha, ""));
    json_t *isobj = uc2_call(sp, "is_object_id", ioa); json_free(ioa);
    if (uc2_is_raise(isobj)) { pending = isobj; goto finally; }
    bool valid = uc2_jtruth(isobj); json_free(isobj);
    if (!valid) {
        char *m = uc2_xsf(UC2_CROSS " Could not resolve %s to a commit. No update was applied.", merge_ref);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
        json_free(tmp);
        tmp = uc2_record_stop(sp, "target_unresolved", NULL);
        if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
        json_free(tmp); tmp = NULL;
        pending = uc2_raise_spec("SystemExit", "1"); goto finally;
    }
    /* movement_baseline = _update_movement_baseline(...) */
    json_t *mba = json_object();
    uc2_aset_j(mba, "git_cmd", git_cmd);
    uc2_aset_j(mba, "pre_pull_sha", pre);
    uc2_aset_j(mba, "pre_sync_sha", pre_sync_sha);
    uc2_aset_j(mba, "rollback_branch", rollback_branch);
    uc2_aset_j(mba, "target_sha", tsha);
    baseline = uc2_call(sp, "update_movement_baseline", mba); json_free(mba);
    if (uc2_is_raise(baseline)) { pending = baseline; baseline = NULL; goto finally; }
    if (uc2_jtruth(expected_sha) && strcmp(uc2_jstr_def(tsha, ""), uc2_jstr_def(expected_sha, "")) != 0) {
        char *t10 = uc2_slice(uc2_jstr_def(tsha, ""), 10);
        char *e10 = uc2_slice(uc2_jstr_def(expected_sha, ""), 10);
        char *m = uc2_xsf(UC2_CROSS " %s resolves to %s, not the selected channel commit %s. No update was applied.",
                          merge_ref, t10, e10);
        free(t10); free(e10);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
        json_free(tmp);
        tmp = uc2_record_stop(sp, "head_moved", NULL);
        if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
        json_free(tmp); tmp = NULL;
        pending = uc2_raise_spec("SystemExit", "1"); goto finally;
    }
    /* refused = _commit.arm_commit_point(git_cmd, root, target_sha, pre=pre, target=target_sha, stash=auto_stash_ref) */
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { pending = root; root = NULL; goto finally; }
    refused = uc2_arm_commit_point(sp, git_cmd, uc2_jstr_def(root, ""), uc2_jstr_def(tsha, ""), pre, tsha, auto_stash_ref);
    json_free(root); root = NULL;
    if (uc2_is_raise(refused)) { pending = refused; refused = NULL; goto finally; }
    if (uc2_jtruth(refused)) {
        char *m = uc2_xsf(UC2_CROSS " %s.", uc2_jstr_def(refused, ""));
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
        json_free(tmp);
        tmp = uc2_record_stop(sp, "commit_point_refused", NULL);
        if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
        json_free(tmp); tmp = NULL;
        pending = uc2_raise_spec("SystemExit", "1"); goto finally;
    }
    /* inner try: with _checkout_move(token, target_sha): move; THEN
     * _refuse_unselected_head OUTSIDE the with (after CM exit). Any raise from
     * enter/move/exit/refuse rides the except-BaseException settle/owe handler. */
    targets = json_array(); json_append(targets, json_copy(tsha));
    paths = json_array();
    tmp = uc2_cm_enter(sp, token, targets, paths, false);
    if (uc2_is_raise(tmp)) { body_raise = tmp; tmp = NULL; goto inner_handler; }
    json_free(tmp); tmp = NULL;
    mv = uc2_move_checkout_to(sp, git_cmd, br, merge_ref, uc2_jstr_def(tsha, ""), pre);
    if (uc2_is_raise(mv)) { body_raise = mv; mv = NULL; }
    else { json_free(mv); mv = NULL; }
    exitr = uc2_cm_exit(sp, token, targets, paths, false, body_raise);
    if (uc2_is_raise(exitr)) {
        if (body_raise) json_free(body_raise);
        body_raise = exitr; exitr = NULL; goto inner_handler;
    }
    json_free(exitr); exitr = NULL;
    if (body_raise) goto inner_handler;
    tmp = uc2_refuse_unselected_head(sp, git_cmd, uc2_jstr_def(tsha, ""), pre);
    if (uc2_is_raise(tmp)) { body_raise = tmp; tmp = NULL; goto inner_handler; }
    json_free(tmp); tmp = NULL;
    goto inner_done;
inner_handler:
    if (uc2_caught(body_raise, "KeyboardInterrupt")) { pending = body_raise; body_raise = NULL; goto finally; }
    /* except BaseException: settle/disarm/owe then raise */
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { pending = root; root = NULL; goto finally; }
    tmp = uc2_settle_failed_tree_move(sp, uc2_jstr_def(root, ""));
    json_free(root); root = NULL;
    if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
    bool settled = uc2_jtruth(tmp); json_free(tmp); tmp = NULL;
    if (settled) {
        tmp = uc2_disarm(sp);
        if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
        json_free(tmp); tmp = NULL;
    } else {
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { pending = root; root = NULL; goto finally; }
        tmp = uc2_owe_restore_of(sp, uc2_jstr_def(root, ""), pre);
        json_free(root); root = NULL;
        if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
        json_free(tmp); tmp = NULL;
    }
    pending = body_raise; body_raise = NULL; goto finally;
inner_done:
    /* pull_marker.unlink(missing_ok=True) */
    tmp = uc2_marker_unlink(sp, marker_s);
    if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
    json_free(tmp); tmp = NULL;
    if (sync_upstream) {
        tmp = uc2_verify_head_after_pull(sp, git_cmd, br, baseline, in_place_update, token);
        if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
        json_free(tmp); tmp = NULL;
        /* try: sync except UpstreamTargetBroken */
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { pending = root; root = NULL; goto finally; }
        root_s = strdup(uc2_jstr_def(root, "")); json_free(root); root = NULL;
        cm = uc2_moves_for(sp, token);
        if (uc2_is_raise(cm)) { pending = cm; cm = NULL; goto finally; }
        sync = uc2_sync_with_upstream(sp, git_cmd, root_s, assume_yes, gw_input_fn, cm);
        json_free(cm); cm = NULL;
        if (uc2_is_raise(sync)) {
            if (uc2_caught(sync, "UpstreamTargetBroken")) {
                json_t *broken = sync; sync = NULL;
                tmp = uc2_verify_head_after_pull(sp, git_cmd, br, baseline, in_place_update, token);
                if (uc2_is_raise(tmp)) { json_free(broken); pending = tmp; tmp = NULL; goto finally; }
                json_free(tmp); tmp = NULL;
                hdr = uc2_upstream_broken_header(sp, broken);
                if (uc2_is_raise(hdr)) { json_free(broken); pending = hdr; hdr = NULL; goto finally; }
                const json_t *bpath = json_obj_get(broken, "path");
                const json_t *berr = json_obj_get(broken, "error");
                const json_t *rollpre = uc2_jtruth(pre_sync_sha) ? pre_sync_sha : pre;
                json_free(broken);
                mv = uc2_roll_back_broken_update(sp, git_cmd, rollpre, uc2_jstr_def(bpath, ""),
                        berr, rollback_branch, uc2_jstr_def(hdr, ""), token, "syntax_rollback");
                json_free(hdr); hdr = NULL;
                pending = mv; mv = NULL; goto finally;
            }
            pending = sync; sync = NULL; goto finally;
        }
        json_free(sync); sync = NULL;
        free(root_s); root_s = NULL;
    }
    /* _verify_head_after_pull(...) */
    tmp = uc2_verify_head_after_pull(sp, git_cmd, br, baseline, in_place_update, token);
    if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
    json_free(tmp); tmp = NULL;
    /* _rollback_if_pulled_syntax_error(git_cmd, pre_sync_sha or pre_pull_sha, ...) */
    {
        const json_t *rollpre = uc2_jtruth(pre_sync_sha) ? pre_sync_sha : pre;
        tmp = uc2_rollback_if_pulled_syntax_error(sp, git_cmd, rollpre, rollback_branch, token);
        if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
        json_free(tmp); tmp = NULL;
    }
    if (sync_upstream) {
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { pending = root; root = NULL; goto finally; }
        json_t *pfa = json_object();
        uc2_aset_j(pfa, "git_cmd", git_cmd);
        uc2_aset_s(pfa, "root", uc2_jstr_def(root, ""));
        json_free(root); root = NULL;
        tmp = uc2_call(sp, "push_synced_fork", pfa); json_free(pfa);
        if (uc2_is_raise(tmp)) { pending = tmp; tmp = NULL; goto finally; }
        json_free(tmp); tmp = NULL;
    }
    update_succeeded = true;
finally:
    if (!uc2_is_null(auto_stash_ref)) {
        if (!update_succeeded) {
            tmp = uc2_stash_preserved_notice(sp, auto_stash_ref);
            if (uc2_is_raise(tmp)) { if (pending) json_free(pending); pending = tmp; tmp = NULL; goto fin_exit; }
            json_free(tmp); tmp = NULL;
        } else if (discard_local_changes) {
            root = uc2_project_root(sp);
            if (uc2_is_raise(root)) { if (pending) json_free(pending); pending = root; root = NULL; goto fin_exit; }
            json_t *da = json_object();
            uc2_aset_j(da, "git_cmd", git_cmd);
            uc2_aset_s(da, "root", uc2_jstr_def(root, ""));
            uc2_aset_j(da, "ref", auto_stash_ref);
            json_free(root); root = NULL;
            tmp = uc2_call(sp, "discard_stashed_changes", da); json_free(da);
            if (uc2_is_raise(tmp)) { if (pending) json_free(pending); pending = tmp; tmp = NULL; goto fin_exit; }
            json_free(tmp); tmp = NULL;
        } else if (keep_stash) {
            json_t *ka = json_object(); uc2_aset_j(ka, "ref", auto_stash_ref);
            tmp = uc2_call(sp, "park_stashed_changes", ka); json_free(ka);
            if (uc2_is_raise(tmp)) { if (pending) json_free(pending); pending = tmp; tmp = NULL; goto fin_exit; }
            json_free(tmp); tmp = NULL;
        } else {
            root = uc2_project_root(sp);
            if (uc2_is_raise(root)) { if (pending) json_free(pending); pending = root; root = NULL; goto fin_exit; }
            root_s = strdup(uc2_jstr_def(root, "")); json_free(root); root = NULL;
            cm = uc2_moves_for(sp, token);
            if (uc2_is_raise(cm)) { if (pending) json_free(pending); pending = cm; cm = NULL; goto fin_exit; }
            tmp = uc2_restore_stashed_changes(sp, git_cmd, root_s, auto_stash_ref, prompt_for_restore, gw_input_fn, cm);
            json_free(cm); cm = NULL;
            if (uc2_is_raise(tmp)) { if (pending) json_free(pending); pending = tmp; tmp = NULL; goto fin_exit; }
            json_free(tmp); tmp = NULL;
        }
    }
fin_exit:
    ret = pending ? pending : baseline;
    pending = NULL; baseline = NULL;
done:
    free(merge_ref); free(root_s); free(marker_s);
    if (tmp) json_free(tmp);
    if (root) json_free(root);
    if (pre) json_free(pre);
    if (marker) json_free(marker);
    if (tsha) json_free(tsha);
    if (baseline) json_free(baseline);
    if (refused) json_free(refused);
    if (targets) json_free(targets);
    if (paths) json_free(paths);
    if (body_raise) json_free(body_raise);
    if (exitr) json_free(exitr);
    if (mv) json_free(mv);
    if (sync) json_free(sync);
    if (cm) json_free(cm);
    if (hdr) json_free(hdr);
    if (pending) json_free(pending);
    return ret;
}

/* ===================================================================
 * _switch_branch_at_commit_point  (update_cmd.py:1167)
 * =================================================================== */
/* PoP: uc2_switch_branch_at_commit_point @ hermes_cli/update_cmd.py:_switch_branch_at_commit_point */
static json_t *uc2_switch_branch_at_commit_point(const uc2_seams_t *sp, const json_t *git_cmd,
                                                 const char *branch, const char *target_ref,
                                                 const json_t *pre, const json_t *stash) {
    json_t *ret = NULL, *root = NULL, *tmp = NULL, *result = NULL, *rec = NULL, *landed = NULL;
    json_t *local = NULL, *remote = NULL;
    char *root_s = NULL;
    const char *br = branch ? branch : "";
    (void)target_ref;
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    root_s = strdup(uc2_jstr_def(root, ""));
    /* resolve(ref) = (git_run(rev-parse -q --verify ref^{commit}).stdout or "").strip() */
    char *lref = uc2_xsf("refs/heads/%s", br);
    char *rref = uc2_xsf("origin/%s", br);
    for (int i = 0; i < 2; i++) {
        char *spec = uc2_xsf("%s^{commit}", i == 0 ? lref : rref);
        json_t *ra = json_array();
        json_append(ra, json_string("rev-parse")); json_append(ra, json_string("-q"));
        json_append(ra, json_string("--verify")); json_append(ra, json_string(spec)); free(spec);
        rec = uc2_git_run(sp, git_cmd, ra, false); json_free(ra);
        if (uc2_is_raise(rec)) { ret = rec; rec = NULL; free(lref); free(rref); goto done; }
        json_t *s = uc2_or_strip(rec, "stdout", NULL); json_free(rec); rec = NULL;
        if (uc2_is_raise(s)) { ret = s; free(lref); free(rref); goto done; }
        if (i == 0) local = s; else remote = s;
    }
    /* attempts: [checkout --no-guess branch, local], [checkout -B branch (remote or origin/br), remote] */
    for (int i = 0; i < 2; i++) {
        json_t *target = i == 0 ? local : remote;
        json_t *args = json_array();
        json_append(args, json_string("checkout"));
        if (i == 0) { json_append(args, json_string("--no-guess")); json_append(args, json_string(br)); }
        else {
            json_append(args, json_string("-B")); json_append(args, json_string(br));
            const char *rv = uc2_jstr_def(remote, "");
            json_append(args, json_string(rv[0] ? rv : rref));
        }
        if (!uc2_jtruth(target)) {
            if (i == 0) { json_free(args); continue; }
            tmp = uc2_disarm(sp);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; json_free(args); free(lref); free(rref); goto done; }
            json_free(tmp); tmp = NULL;
            ret = uc2_git_run(sp, git_cmd, args, false);
            json_free(args); free(lref); free(rref); goto done;
        }
        /* refused = arm_commit_point(git_cmd, root, target, pre=pre, target=target, stash=stash) */
        json_t *refused = uc2_arm_commit_point(sp, git_cmd, root_s, uc2_jstr_def(target, ""), pre, target, stash);
        if (uc2_is_raise(refused)) { ret = refused; json_free(args); free(lref); free(rref); goto done; }
        if (uc2_jtruth(refused)) {
            ret = json_object();
            json_t *aa = json_array(); json_append(aa, json_string("arm_commit_point"));
            uc2_aset_j(ret, "args", aa);
            uc2_aset_n(ret, "returncode", 1);
            uc2_aset_s(ret, "stdout", "");
            uc2_aset_j(ret, "stderr", refused);
            json_free(refused); json_free(args); free(lref); free(rref); goto done;
        }
        json_free(refused);
        /* try: result = git_run(args) except KeyboardInterrupt raise; except BaseException settle/owe raise */
        result = uc2_git_run(sp, git_cmd, args, false);
        json_free(args); args = NULL;
        if (uc2_is_raise(result)) {
            if (uc2_caught(result, "KeyboardInterrupt")) { ret = result; result = NULL; free(lref); free(rref); goto done; }
            tmp = uc2_settle_failed_tree_move(sp, root_s);
            if (uc2_is_raise(tmp)) { json_free(result); result = NULL; ret = tmp; tmp = NULL; free(lref); free(rref); goto done; }
            bool settled = uc2_jtruth(tmp); json_free(tmp); tmp = NULL;
            if (settled) {
                tmp = uc2_disarm(sp);
                if (uc2_is_raise(tmp)) { json_free(result); result = NULL; ret = tmp; tmp = NULL; free(lref); free(rref); goto done; }
                json_free(tmp); tmp = NULL;
            } else {
                tmp = uc2_owe_restore_of(sp, root_s, pre);
                if (uc2_is_raise(tmp)) { json_free(result); result = NULL; ret = tmp; tmp = NULL; free(lref); free(rref); goto done; }
                json_free(tmp); tmp = NULL;
            }
            ret = result; result = NULL; free(lref); free(rref); goto done;
        }
        /* if result.rc==0 and landed_off_target(git_cmd, target, merged=False) is not None */
        if (uc2_rc_val(result) == 0.0) {
            landed = uc2_landed_off_target(sp, git_cmd, uc2_jstr_def(target, ""), false);
            if (uc2_is_raise(landed)) { ret = landed; landed = NULL; json_free(result); result = NULL; free(lref); free(rref); goto done; }
            if (!uc2_is_null(landed)) {
                char *l10 = uc2_slice(uc2_jstr_def(landed, ""), 10);
                char *t10 = uc2_slice(uc2_jstr_def(target, ""), 10);
                char *m = uc2_xsf("  " UC2_CROSS " The branch moved to %s after %s was selected.", l10, t10);
                tmp = uc2_print(sp, m); free(m);
                if (uc2_is_raise(tmp)) { free(l10); free(t10); json_free(landed); landed = NULL; json_free(result); result = NULL; ret = tmp; tmp = NULL; free(lref); free(rref); goto done; }
                json_free(tmp);
                tmp = uc2_settle_failed_tree_move(sp, root_s);
                if (uc2_is_raise(tmp)) { free(l10); free(t10); json_free(landed); landed = NULL; json_free(result); result = NULL; ret = tmp; tmp = NULL; free(lref); free(rref); goto done; }
                bool settled = uc2_jtruth(tmp); json_free(tmp); tmp = NULL;
                if (settled) {
                    tmp = uc2_disarm(sp);
                    if (uc2_is_raise(tmp)) { free(l10); free(t10); json_free(landed); landed = NULL; json_free(result); result = NULL; ret = tmp; tmp = NULL; free(lref); free(rref); goto done; }
                    json_free(tmp); tmp = NULL;
                } else {
                    tmp = uc2_owe_restore_of(sp, root_s, pre);
                    if (uc2_is_raise(tmp)) { free(l10); free(t10); json_free(landed); landed = NULL; json_free(result); result = NULL; ret = tmp; tmp = NULL; free(lref); free(rref); goto done; }
                    json_free(tmp); tmp = NULL;
                }
                ret = json_object();
                json_t *aa = json_array(); json_append(aa, json_string("ref_moved"));
                uc2_aset_j(ret, "args", aa);
                uc2_aset_n(ret, "returncode", 1);
                uc2_aset_j(ret, "stdout", json_obj_get(result, "stdout"));
                char *em = uc2_xsf("'%s' moved to %s during the switch; re-run `hermes update`", br, l10);
                uc2_aset_s(ret, "stderr", em); free(em);
                free(l10); free(t10); json_free(landed); landed = NULL; json_free(result); result = NULL;
                free(lref); free(rref); goto done;
            }
            json_free(landed); landed = NULL;
            /* interrupted_pull_marker(root).unlink */
            json_t *mk = uc2_marker(sp, root_s);
            if (uc2_is_raise(mk)) { ret = mk; json_free(result); result = NULL; free(lref); free(rref); goto done; }
            tmp = uc2_marker_unlink(sp, uc2_jstr_def(mk, "")); json_free(mk);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; json_free(result); result = NULL; free(lref); free(rref); goto done; }
            json_free(tmp); tmp = NULL;
            ret = result; result = NULL; free(lref); free(rref); goto done;
        }
        /* if not settle_failed_tree_move(root): owe_restore_of(root, pre); return result */
        tmp = uc2_settle_failed_tree_move(sp, root_s);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; json_free(result); result = NULL; free(lref); free(rref); goto done; }
        bool settled = uc2_jtruth(tmp); json_free(tmp); tmp = NULL;
        if (!settled) {
            tmp = uc2_owe_restore_of(sp, root_s, pre);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; json_free(result); result = NULL; free(lref); free(rref); goto done; }
            json_free(tmp); tmp = NULL;
            ret = result; result = NULL; free(lref); free(rref); goto done;
        }
        /* else loop continues (result kept) */
    }
    free(lref); free(rref);
    tmp = uc2_disarm(sp);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    ret = result; result = NULL;
done:
    free(root_s);
    if (root) json_free(root);
    if (tmp) json_free(tmp);
    if (result) json_free(result);
    if (rec) json_free(rec);
    if (landed) json_free(landed);
    if (local) json_free(local);
    if (remote) json_free(remote);
    return ret;
}

/* ===================================================================
 * _refuse_before_commit_point  (update_cmd.py:1225)
 * =================================================================== */
/* PoP: uc2_refuse_before_commit_point @ hermes_cli/update_cmd.py:_refuse_before_commit_point */
static json_t *uc2_refuse_before_commit_point(const uc2_seams_t *sp, const json_t *git_cmd,
                                              const char *target_ref, const json_t *token) {
    json_t *ret = NULL, *root = NULL, *reason = NULL, *tmp = NULL, *cf = NULL;
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    cf = json_array();
    for (size_t i = 0; UC2_CRITICAL_FILES[i]; i++) json_append(cf, json_string(UC2_CRITICAL_FILES[i]));
    json_t *pa = json_object();
    uc2_aset_j(pa, "git_cmd", git_cmd);
    uc2_aset_s(pa, "root", uc2_jstr_def(root, ""));
    uc2_aset_s(pa, "target_ref", target_ref ? target_ref : "");
    uc2_aset_j(pa, "critical_files", cf);
    json_free(cf); cf = NULL;
    reason = uc2_call(sp, "preflight_refusal", pa); json_free(pa);
    if (uc2_is_raise(reason)) { ret = reason; reason = NULL; goto done; }
    if (uc2_is_null(reason)) {
        json_free(root);
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        json_t *da = json_object();
        uc2_aset_j(da, "git_cmd", git_cmd);
        uc2_aset_s(da, "root", uc2_jstr_def(root, ""));
        uc2_aset_s(da, "target_ref", target_ref ? target_ref : "");
        json_free(reason);
        reason = uc2_call(sp, "downgrade_refusal", da); json_free(da);
        if (uc2_is_raise(reason)) { ret = reason; reason = NULL; goto done; }
        if (!uc2_is_null(reason)) {
            tmp = uc2_record_stop(sp, "downgrade_refused", NULL);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
    }
    if (uc2_is_null(reason)) { ret = NULL; goto done; }
    tmp = uc2_print(sp, uc2_jstr_def(reason, ""));
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp);
    tmp = uc2_print(sp, "  No update was applied; your install is unchanged.");
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp);
    tmp = uc2_resume_gateways(sp, token);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    ret = uc2_raise_spec("SystemExit", "1");
done:
    if (root) json_free(root);
    if (reason) json_free(reason);
    if (tmp) json_free(tmp);
    if (cf) json_free(cf);
    return ret;
}

/* ===================================================================
 * _exit_after_failed_branch_switch  (update_cmd.py:1241)  — NoReturn
 * =================================================================== */
/* PoP: uc2_exit_after_failed_branch_switch @ hermes_cli/update_cmd.py:_exit_after_failed_branch_switch */
static json_t *uc2_exit_after_failed_branch_switch(const uc2_seams_t *sp, const json_t *git_cmd,
                                                   const char *branch, const json_t *track_result,
                                                   const json_t *auto_stash_ref, const json_t *gw_input_fn,
                                                   const json_t *token) {
    json_t *ret = NULL, *tmp = NULL, *root = NULL, *s = NULL, *cm = NULL, *mk = NULL, *isf = NULL;
    char *root_s = NULL;
    const char *br = branch ? branch : "";
    s = uc2_or_strip(track_result, "stderr", NULL);
    if (uc2_is_raise(s)) { ret = s; s = NULL; goto done; }
    const char *detail = uc2_jstr_def(s, "");
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    root_s = strdup(uc2_jstr_def(root, "")); json_free(root); root = NULL;
    mk = uc2_marker(sp, root_s);
    if (uc2_is_raise(mk)) { ret = mk; mk = NULL; goto done; }
    isf = uc2_marker_is_file(sp, uc2_jstr_def(mk, "")); json_free(mk); mk = NULL;
    if (uc2_is_raise(isf)) { ret = isf; isf = NULL; goto done; }
    bool torn = uc2_jtruth(isf); json_free(isf); isf = NULL;
    if (torn) {
        char *m = uc2_xsf(UC2_CROSS " Switching to '%s' stopped part-way; the next `hermes` launch puts the checkout back.", br);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        if (detail[0]) {
            char *fl = uc2_first_line(detail);
            m = uc2_xsf("  %s", fl ? fl : ""); free(fl);
            tmp = uc2_print(sp, m); free(m);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
        if (!uc2_is_null(auto_stash_ref)) {
            tmp = uc2_stash_preserved_notice(sp, auto_stash_ref);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
        tmp = uc2_record_stop(sp, "checkout_move_failed", NULL);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        ret = uc2_raise_spec("SystemExit", "1"); goto done;
    }
    if (!uc2_is_null(auto_stash_ref)) {
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        cm = uc2_moves_for(sp, token);
        if (uc2_is_raise(cm)) { ret = cm; cm = NULL; goto done; }
        tmp = uc2_restore_stashed_changes(sp, git_cmd, uc2_jstr_def(root, ""), auto_stash_ref, false, gw_input_fn, cm);
        json_free(cm); cm = NULL; json_free(root); root = NULL;
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    }
    /* track_result.args in (_ARM_REFUSED, _REF_MOVED) — identity projected by value */
    const json_t *targs = uc2_is_obj(track_result) ? json_obj_get(track_result, "args") : NULL;
    bool is_arm = uc2_is_arr(targs) && json_len(targs) == 1 &&
                  !strcmp(uc2_jstr_def(json_get(targs, 0), ""), "arm_commit_point");
    bool is_ref = uc2_is_arr(targs) && json_len(targs) == 1 &&
                  !strcmp(uc2_jstr_def(json_get(targs, 0), ""), "ref_moved");
    if (is_arm || is_ref) {
        char *m = uc2_xsf(UC2_CROSS " %s.", detail);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_record_stop(sp, is_arm ? "commit_point_refused" : "head_moved", NULL);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    } else {
        char *m = uc2_xsf(UC2_CROSS " Branch '%s' does not exist locally or on origin.", br);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        if (detail[0]) {
            char *fl = uc2_first_line(detail);
            m = uc2_xsf("  %s", fl ? fl : ""); free(fl);
            tmp = uc2_print(sp, m); free(m);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
        json_t *mba = json_object();
        uc2_aset_j(mba, "git_cmd", git_cmd);
        uc2_aset_s(mba, "branch", br);
        tmp = uc2_call(sp, "missing_branch_stop", mba); json_free(mba);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_t *rs = uc2_record_stop(sp, uc2_jstr_def(tmp, ""), NULL);
        json_free(tmp); tmp = NULL;
        if (uc2_is_raise(rs)) { ret = rs; goto done; }
        json_free(rs);
    }
    ret = uc2_raise_spec("SystemExit", "1");
done:
    free(root_s);
    if (tmp) json_free(tmp);
    if (root) json_free(root);
    if (s) json_free(s);
    if (cm) json_free(cm);
    if (mk) json_free(mk);
    if (isf) json_free(isf);
    return ret;
}
/* str(CalledProcessError) from a fixture e record {returncode, cmd}. */
static char *uc2_cpe_str(const json_t *e) {
    const json_t *cmd = uc2_is_obj(e) ? json_obj_get(e, "cmd") : NULL;
    const json_t *rcn = uc2_is_obj(e) ? json_obj_get(e, "returncode") : NULL;
    int rc = uc2_is_num(rcn) ? (int)rcn->num_val : 1;
    char *cmdstr = uc2_pystr(cmd);
    char *s = uc2_xsf("Command '%s' returned non-zero exit status %d.", cmdstr, rc);
    free(cmdstr);
    return s;
}
/* best_effort updates_config().get(key, default) == want; *err carries a
 * BaseException raise (Exception swallowed -> false). */
static bool uc2_config_eq(const uc2_seams_t *sp, const char *key, const char *deflt,
                          const char *want, bool lower, json_t **err) {
    *err = NULL;
    json_t *cfg = uc2_updates_config(sp);
    if (uc2_is_raise(cfg)) {
        if (uc2_is_exception(cfg)) { json_free(cfg); return false; }
        *err = cfg; return false;
    }
    json_t *dv = json_string(deflt);
    json_t *v = uc2_dget(cfg, key, dv);
    json_free(dv); json_free(cfg);
    if (uc2_is_raise(v)) {
        if (uc2_is_exception(v)) { json_free(v); return false; }
        *err = v; return false;
    }
    char *vs = uc2_pystr(v); json_free(v);
    char *lv = lower ? uc2_ascii_lower(vs) : strdup(vs);
    free(vs);
    bool eq = !strcmp(lv, want);
    free(lv);
    return eq;
}

/* ===================================================================
 * _apply_parked_branch_guard  (update_cmd.py:1301)
 * =================================================================== */
/* PoP: uc2_apply_parked_branch_guard @ hermes_cli/update_cmd.py:_apply_parked_branch_guard */
static json_t *uc2_apply_parked_branch_guard(const uc2_seams_t *sp, const json_t *git_cmd,
                                             const char *branch, const char *current_branch,
                                             bool switch_branch, const json_t *token) {
    json_t *ret = NULL, *root = NULL, *tmp = NULL, *assess = NULL, *suf = NULL, *err = NULL;
    const char *br = branch ? branch : "";
    const char *cur = current_branch ? current_branch : "";
    if (!strcmp(cur, br) || !strcmp(cur, "HEAD")) {
        ret = json_array();
        json_append(ret, json_bool(false)); json_append(ret, json_bool(false)); json_append(ret, json_null());
        return ret;
    }
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    json_t *aa = json_object();
    uc2_aset_j(aa, "git_cmd", git_cmd);
    uc2_aset_s(aa, "root", uc2_jstr_def(root, ""));
    uc2_aset_s(aa, "current_branch", cur);
    uc2_aset_s(aa, "branch", br);
    assess = uc2_call(sp, "assess_parked_branch_switch", aa); json_free(aa);
    if (uc2_is_raise(assess)) { ret = assess; assess = NULL; goto done; }
    if (!uc2_is_arr(assess) || json_len(assess) != 2) {
        json_free(assess); assess = NULL;
        ret = uc2_typeerror("cannot unpack non-iterable or wrong-length assess result");
        goto done;
    }
    bool switch_safe = uc2_jtruth(json_get(assess, 0));
    const char *reason = uc2_jstr_def(json_get(assess, 1), "");
    if (!switch_safe) {
        /* _m().PROJECT_ROOT is read fresh for EACH collaborator argument */
        json_free(root);
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        json_t *wa = json_object();
        uc2_aset_j(wa, "git_cmd", git_cmd);
        uc2_aset_s(wa, "root", uc2_jstr_def(root, ""));
        uc2_aset_s(wa, "current_branch", cur);
        uc2_aset_s(wa, "branch", br);
        uc2_aset_s(wa, "reason", reason);
        tmp = uc2_call(sp, "print_parked_branch_skip_warning", wa); json_free(wa);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_print(sp, "");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        json_free(root);
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        json_t *sa = json_object();
        uc2_aset_j(sa, "git_cmd", git_cmd);
        uc2_aset_s(sa, "root", uc2_jstr_def(root, ""));
        suf = uc2_call(sp, "branch_head_suffix", sa); json_free(sa);
        if (uc2_is_raise(suf)) { ret = suf; suf = NULL; goto done; }
        char *m = uc2_xsf(UC2_WARN " Update finished " UC2_EMDASH " code update SKIPPED%s", uc2_jstr_def(suf, ""));
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_resume_gateways(sp, token);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_record_stop(sp, "parked_branch_blocked", NULL);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        ret = uc2_raise_spec("SystemExit", "1"); goto done;
    }
    if (!uc2_startswith(reason, "unmerged:")) {
        char *m = uc2_xsf("  " UC2_WARN " Checkout was parked on '%s' (fully merged) " UC2_EMDASH " switching back to %s...", cur, br);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        ret = json_array();
        json_append(ret, json_bool(true)); json_append(ret, json_bool(false)); json_append(ret, json_string(reason));
        goto done;
    }
    bool in_place_configured = uc2_config_eq(sp, "parked_branch_strategy", "switch", "update_in_place", false, &err);
    if (err) { ret = err; err = NULL; goto done; }
    if (!in_place_configured || switch_branch) {
        const char *colon = strchr(reason, ':');
        const char *detail = colon ? colon + 1 : "";
        json_t *ka = json_object();
        uc2_aset_s(ka, "current_branch", cur);
        uc2_aset_s(ka, "branch", br);
        uc2_aset_s(ka, "detail", detail);
        tmp = uc2_call(sp, "print_parked_branch_kept_notice", ka); json_free(ka);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        ret = json_array();
        json_append(ret, json_bool(true)); json_append(ret, json_bool(false)); json_append(ret, json_string(reason));
        goto done;
    }
    char *oref = uc2_xsf("origin/%s", br);
    json_t *ga = json_array();
    json_append(ga, json_string("rev-parse")); json_append(ga, json_string("--verify"));
    json_append(ga, json_string("--quiet")); json_append(ga, json_string(oref)); free(oref);
    tmp = uc2_git_run(sp, git_cmd, ga, false); json_free(ga);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    bool missing = uc2_rc_val(tmp) != 0.0;
    json_free(tmp); tmp = NULL;
    if (missing) {
        char *m = uc2_xsf(UC2_CROSS " Branch '%s' does not exist locally or on origin.", br);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_record_stop(sp, "branch_missing", NULL);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        ret = uc2_raise_spec("SystemExit", "1"); goto done;
    }
    char *m = uc2_xsf("  " UC2_INFO " On branch '%s' " UC2_EMDASH " updating it in place from origin/%s (no branch switch; local commits preserved).", cur, br);
    tmp = uc2_print(sp, m); free(m);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    ret = json_array();
    json_append(ret, json_bool(false)); json_append(ret, json_bool(true)); json_append(ret, json_string(reason));
done:
    if (root) json_free(root);
    if (tmp) json_free(tmp);
    if (assess) json_free(assess);
    if (suf) json_free(suf);
    if (err) json_free(err);
    return ret;
}

/* ===================================================================
 * _prepare_checkout_for_update  (update_cmd.py:1347)  — returns plan dict
 * =================================================================== */
/* PoP: uc2_prepare_checkout_for_update @ hermes_cli/update_cmd.py:_prepare_checkout_for_update */
static json_t *uc2_prepare_checkout_for_update(const uc2_seams_t *sp, const json_t *git_cmd,
                                               const char *branch, const char *current_branch,
                                               bool is_fork, bool assume_yes, bool gateway_mode,
                                               const json_t *gw_input_fn, bool switch_branch,
                                               const json_t *target_ref, const json_t *token) {
    json_t *ret = NULL, *tmp = NULL, *root = NULL, *guard = NULL, *stash = NULL, *cm = NULL;
    json_t *moved = NULL, *rec = NULL, *track = NULL, *targets = NULL, *paths = NULL;
    json_t *body_raise = NULL, *exitr = NULL, *head = NULL, *landed = NULL, *pre_sync = NULL, *post_sync = NULL;
    json_t *sync = NULL, *hdr = NULL, *broken = NULL;
    char *tref = NULL, *root_s = NULL;
    const char *br = branch ? branch : "";
    const char *cur = current_branch ? current_branch : "";
    bool release_tag, parked_switched = false, in_place = false;
    const char *block_reason = NULL;
    json_t *block_reason_node = NULL;
    long long commit_count = 0;
    bool switched_without = false, upstream_checked = true;
    bool prompt_for_restore, ask_first;
    char *origin_ref = uc2_xsf("origin/%s", br);
    tref = uc2_is_null(target_ref) ? strdup(origin_ref) : strdup(uc2_jstr_def(target_ref, ""));
    release_tag = strcmp(tref, origin_ref) != 0;
    if (release_tag) { parked_switched = false; in_place = true; block_reason = NULL; }
    else {
        guard = uc2_apply_parked_branch_guard(sp, git_cmd, br, cur, switch_branch, token);
        if (uc2_is_raise(guard)) { ret = guard; guard = NULL; goto done; }
        parked_switched = uc2_jtruth(json_get(guard, 0));
        in_place = uc2_jtruth(json_get(guard, 1));
        block_reason_node = json_copy(json_get(guard, 2));
        block_reason = uc2_jstr(block_reason_node);
        json_free(guard); guard = NULL;
    }
    if (!release_tag && !in_place && !strcmp(cur, "HEAD") && strcmp("HEAD", br) != 0) {
        char *m = uc2_xsf("  " UC2_WARN " Currently on detached HEAD " UC2_EMDASH " switching to %s for update...", br);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        json_t *pa = json_object();
        uc2_aset_j(pa, "git_cmd", git_cmd);
        uc2_aset_s(pa, "root", uc2_jstr_def(root, ""));
        uc2_aset_s(pa, "branch", br);
        json_free(root); root = NULL;
        tmp = uc2_call(sp, "park_detached_head", pa); json_free(pa);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    }
    /* prompt_for_restore = not assume_yes and (gateway_mode or (stdin.isatty() and stdout.isatty())) */
    prompt_for_restore = false;
    if (!assume_yes) {
        if (gateway_mode) prompt_for_restore = true;
        else {
            json_t *si = uc2_call0(sp, "stdin_isatty");
            if (uc2_is_raise(si)) { ret = si; goto done; }
            bool tty = uc2_jtruth(si); json_free(si);
            if (tty) {
                json_t *so = uc2_call0(sp, "stdout_isatty");
                if (uc2_is_raise(so)) { ret = so; goto done; }
                tty = uc2_jtruth(so); json_free(so);
            }
            prompt_for_restore = tty;
        }
    }
    /* ask_first = prompt_for_restore and not gateway_mode and ((not release and not in_place and cur != br) or target_would_move_head) */
    ask_first = false;
    if (prompt_for_restore && !gateway_mode) {
        if (!release_tag && !in_place && strcmp(cur, br) != 0) ask_first = true;
        else {
            root = uc2_project_root(sp);
            if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
            json_t *ta = json_object();
            uc2_aset_j(ta, "git_cmd", git_cmd);
            uc2_aset_s(ta, "root", uc2_jstr_def(root, ""));
            uc2_aset_s(ta, "target_ref", tref);
            json_free(root); root = NULL;
            tmp = uc2_call(sp, "target_would_move_head", ta); json_free(ta);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            ask_first = uc2_jtruth(tmp); json_free(tmp); tmp = NULL;
        }
    }
    /* auto_stash_ref = _stash_local_changes_if_needed(git_cmd, root, checkout_move=moves_for(token), ask_first, untracked_targets) */
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    root_s = strdup(uc2_jstr_def(root, "")); json_free(root); root = NULL;
    cm = uc2_moves_for(sp, token);
    if (uc2_is_raise(cm)) { ret = cm; cm = NULL; goto done; }
    json_t *ut = json_array();
    if (!is_fork) {
        json_append(ut, json_string(tref));
        json_append(ut, json_string(origin_ref));
        char *hr = uc2_xsf("refs/heads/%s", br);
        json_append(ut, json_string(hr)); free(hr);
    }
    json_t *sla = json_object();
    uc2_aset_j(sla, "git_cmd", git_cmd);
    uc2_aset_s(sla, "root", root_s);
    uc2_aset_j(sla, "checkout_move", cm);
    uc2_aset_b(sla, "ask_first", ask_first);
    uc2_aset_j(sla, "untracked_targets", ut);
    json_free(cm); cm = NULL; json_free(ut);
    stash = uc2_call(sp, "stash_local_changes_if_needed", sla); json_free(sla);
    if (uc2_is_raise(stash)) { ret = stash; stash = NULL; goto done; }
    /* moved_from_sha / rollback_branch */
    const char *rollback_branch = NULL;
    if (!release_tag && !in_place && strcmp(cur, br) != 0) {
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        moved = uc2_capture_head_sha(sp, git_cmd, uc2_jstr_def(root, ""));
        json_free(root); root = NULL;
        if (uc2_is_raise(moved)) { ret = moved; moved = NULL; goto done; }
        rollback_branch = cur;
        tmp = uc2_call0(sp, "pause_at_commit_point");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        /* with _checkout_move(token, *(resolved_commit(ref) for ref in (refs/heads/br, origin/br))) */
        targets = json_array();
        for (int i = 0; i < 2; i++) {
            char *rf = i == 0 ? uc2_xsf("refs/heads/%s", br) : uc2_xsf("origin/%s", br);
            json_t *rc = uc2_resolved_commit(sp, git_cmd, rf); free(rf);
            if (uc2_is_raise(rc)) { ret = rc; goto done; }
            json_append(targets, rc);
        }
        paths = json_array();
        tmp = uc2_cm_enter(sp, token, targets, paths, false);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        track = uc2_switch_branch_at_commit_point(sp, git_cmd, br, tref, moved, stash);
        if (uc2_is_raise(track)) { body_raise = track; track = NULL; goto pc_cm_exit; }
pc_cm_exit:
        exitr = uc2_cm_exit(sp, token, targets, paths, false, body_raise);
        if (uc2_is_raise(exitr)) { ret = exitr; exitr = NULL; goto done; }
        json_free(exitr); exitr = NULL;
        if (body_raise) { ret = body_raise; body_raise = NULL; goto done; }
        if (uc2_rc_val(track) != 0.0) {
            tmp = uc2_exit_after_failed_branch_switch(sp, git_cmd, br, track, stash, gw_input_fn, token);
            json_free(track); track = NULL;
            ret = tmp; tmp = NULL; goto done;
        }
        json_free(track); track = NULL;
    }
    prompt_for_restore = !uc2_is_null(stash) && prompt_for_restore && !ask_first;
    if (release_tag) {
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        head = uc2_capture_head_sha(sp, git_cmd, uc2_jstr_def(root, ""));
        json_free(root); root = NULL;
        if (uc2_is_raise(head)) { ret = head; head = NULL; goto done; }
        commit_count = (!strcmp(uc2_jstr_def(head, ""), tref)) ? 0 : -1;
        ret = json_object();
        uc2_aset_j(ret, "auto_stash_ref", stash);
        uc2_aset_n(ret, "commit_count", (double)commit_count);
        uc2_aset_b(ret, "in_place_update", true);
        uc2_aset_b(ret, "parked_branch_switched", false);
        uc2_aset_b(ret, "prompt_for_restore", prompt_for_restore);
        uc2_aset_j(ret, "switch_block_reason", NULL);
        uc2_aset_b(ret, "upstream_checked", true);
        uc2_aset_j(ret, "pre_sync_sha", NULL);
        uc2_aset_j(ret, "rollback_branch", NULL);
        uc2_aset_b(ret, "switched_without_new_commits", false);
        goto done;
    }
    /* base = moved_from_sha or "HEAD"; rev-list base..tref --count (check=True) */
    const char *base = uc2_jtruth(moved) ? uc2_jstr_def(moved, "") : "HEAD";
    char *range = uc2_xsf("%s..%s", base, tref);
    json_t *ga = json_array();
    json_append(ga, json_string("rev-list")); json_append(ga, json_string(range));
    json_append(ga, json_string("--count")); free(range);
    rec = uc2_git_run(sp, git_cmd, ga, true); json_free(ga);
    if (uc2_is_raise(rec)) { ret = rec; rec = NULL; goto done; }
    json_t *so = uc2_stdout_strip(rec); json_free(rec); rec = NULL;
    if (uc2_is_raise(so)) { ret = so; so = NULL; goto done; }
    json_t *ie = uc2_py_int(uc2_jstr_def(so, ""), &commit_count);
    json_free(so); so = NULL;
    if (ie) { ret = ie; goto done; }
    /* apply_is_shallow = _is_shallow_checkout(git_cmd) */
    json_t *isa = json_object(); uc2_aset_j(isa, "git_cmd", git_cmd);
    json_t *shallow = uc2_call(sp, "is_shallow_checkout", isa); json_free(isa);
    if (uc2_is_raise(shallow)) { ret = shallow; goto done; }
    bool apply_shallow = uc2_jtruth(shallow); json_free(shallow);
    if (commit_count > 0 && apply_shallow) {
        json_t *tsa = json_object();
        uc2_aset_j(tsa, "git_cmd", git_cmd);
        uc2_aset_s(tsa, "target_ref", tref);
        uc2_aset_s(tsa, "base", base);
        json_t *tips = uc2_call(sp, "tip_shas", tsa); json_free(tsa);
        if (uc2_is_raise(tips)) { ret = tips; goto done; }
        json_t *gca = json_object();
        uc2_aset_j(gca, "base_sha", uc2_is_arr(tips) ? json_get(tips, 0) : NULL);
        uc2_aset_j(gca, "target_sha", uc2_is_arr(tips) ? json_get(tips, 1) : NULL);
        json_free(tips);
        json_t *counted = uc2_call(sp, "github_compare_behind", gca); json_free(gca);
        if (uc2_is_raise(counted)) { ret = counted; goto done; }
        commit_count = uc2_is_null(counted) ? -1 : (uc2_is_num(counted) ? (long long)counted->num_val : -1);
        json_free(counted);
    }
    if (commit_count == 0 && uc2_jtruth(moved)) {
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        landed = uc2_capture_head_sha(sp, git_cmd, uc2_jstr_def(root, ""));
        json_free(root); root = NULL;
        if (uc2_is_raise(landed)) { ret = landed; landed = NULL; goto done; }
        if (uc2_jtruth(landed) && strcmp(uc2_jstr_def(landed, ""), uc2_jstr_def(moved, "")) != 0) {
            commit_count = -1; switched_without = true;
        }
    }
    if (commit_count == 0 && is_fork && !strcmp(br, "main") && !release_tag) {
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        pre_sync = uc2_capture_head_sha(sp, git_cmd, uc2_jstr_def(root, ""));
        json_free(root); root = NULL;
        if (uc2_is_raise(pre_sync)) { ret = pre_sync; pre_sync = NULL; goto done; }
        /* try: upstream_checked = sync(...) except UpstreamTargetBroken */
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        root_s = strdup(uc2_jstr_def(root, "")); json_free(root); root = NULL;
        cm = uc2_moves_for(sp, token);
        if (uc2_is_raise(cm)) { ret = cm; cm = NULL; goto done; }
        sync = uc2_sync_with_upstream(sp, git_cmd, root_s, assume_yes, gw_input_fn, cm);
        json_free(cm); cm = NULL;
        if (uc2_is_raise(sync)) {
            if (uc2_caught(sync, "UpstreamTargetBroken")) {
                broken = sync; sync = NULL;
                if (!uc2_is_null(stash)) {
                    tmp = uc2_stash_preserved_notice(sp, stash);
                    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
                    json_free(tmp); tmp = NULL;
                }
                hdr = uc2_upstream_broken_header(sp, broken);
                if (uc2_is_raise(hdr)) { ret = hdr; hdr = NULL; goto done; }
                const json_t *bpath = json_obj_get(broken, "path");
                const json_t *berr = json_obj_get(broken, "error");
                tmp = uc2_roll_back_broken_update(sp, git_cmd, pre_sync, uc2_jstr_def(bpath, ""), berr,
                        NULL, uc2_jstr_def(hdr, ""), token, "target_syntax_error");
                json_free(hdr); hdr = NULL;
                ret = tmp; tmp = NULL; goto done;
            }
            ret = sync; sync = NULL; goto done;
        }
        upstream_checked = uc2_jtruth(sync); json_free(sync); sync = NULL;
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        post_sync = uc2_capture_head_sha(sp, git_cmd, uc2_jstr_def(root, ""));
        json_free(root); root = NULL;
        if (uc2_is_raise(post_sync)) { ret = post_sync; post_sync = NULL; goto done; }
        if (uc2_jtruth(pre_sync) && uc2_jtruth(post_sync) &&
            strcmp(uc2_jstr_def(pre_sync, ""), uc2_jstr_def(post_sync, "")) != 0) {
            root = uc2_project_root(sp);
            if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
            json_t *cba = json_object();
            uc2_aset_j(cba, "git_cmd", git_cmd);
            uc2_aset_s(cba, "root", uc2_jstr_def(root, ""));
            uc2_aset_j(cba, "pre_sha", pre_sync);
            uc2_aset_j(cba, "post_sha", post_sync);
            json_free(root); root = NULL;
            json_t *sc = uc2_call(sp, "count_commits_between", cba); json_free(cba);
            if (uc2_is_raise(sc)) { ret = sc; goto done; }
            long long synced = uc2_is_num(sc) ? (long long)sc->num_val : 0;
            json_free(sc);
            commit_count = synced > 1 ? synced : 1;
            if (!uc2_jtruth(moved)) { json_free(moved); moved = json_copy(pre_sync); }
        }
    }
    ret = json_object();
    uc2_aset_j(ret, "auto_stash_ref", stash);
    uc2_aset_n(ret, "commit_count", (double)commit_count);
    uc2_aset_b(ret, "in_place_update", in_place);
    uc2_aset_b(ret, "parked_branch_switched", parked_switched);
    uc2_aset_b(ret, "prompt_for_restore", prompt_for_restore);
    uc2_aset_j(ret, "switch_block_reason", block_reason_node);
    uc2_aset_b(ret, "upstream_checked", upstream_checked);
    uc2_aset_j(ret, "pre_sync_sha", moved);
    uc2_aset_s(ret, "rollback_branch", rollback_branch);
    uc2_aset_b(ret, "switched_without_new_commits", switched_without);
done:
    free(tref); free(origin_ref); free(root_s);
    if (tmp) json_free(tmp);
    if (root) json_free(root);
    if (guard) json_free(guard);
    if (stash) json_free(stash);
    if (cm) json_free(cm);
    if (moved) json_free(moved);
    if (rec) json_free(rec);
    if (track) json_free(track);
    if (targets) json_free(targets);
    if (paths) json_free(paths);
    if (body_raise) json_free(body_raise);
    if (exitr) json_free(exitr);
    if (head) json_free(head);
    if (landed) json_free(landed);
    if (pre_sync) json_free(pre_sync);
    if (post_sync) json_free(post_sync);
    if (sync) json_free(sync);
    if (hdr) json_free(hdr);
    if (broken) json_free(broken);
    if (block_reason_node) json_free(block_reason_node);
    return ret;
}

/* ===================================================================
 * _resolve_update_options  (update_cmd.py:1485)  — returns options dict
 * =================================================================== */
/* PoP: uc2_resolve_update_options @ hermes_cli/update_cmd.py:_resolve_update_options */
static json_t *uc2_resolve_update_options(const uc2_seams_t *sp, const json_t *args, bool gateway_mode) {
    json_t *ret = NULL, *ver = NULL, *err = NULL, *si = NULL, *so = NULL;
    ver = uc2_call0(sp, "checkout_version");
    if (uc2_is_raise(ver)) { ret = ver; ver = NULL; goto done; }
    json_t *fdef = json_bool(false);
    json_t *yes = uc2_getattr(args, "yes", fdef);
    json_t *ks = uc2_getattr(args, "keep_stash", fdef);
    json_t *sb = uc2_getattr(args, "switch_branch", fdef);
    json_t *ngr = uc2_getattr(args, "no_gateway_restart", fdef);
    json_t *force = uc2_getattr(args, "force", fdef);
    json_free(fdef);
    bool assume_yes = uc2_jtruth(yes);
    bool discard = false;
    if (gateway_mode || assume_yes) {
        discard = uc2_config_eq(sp, "non_interactive_local_changes", "stash", "discard", true, &err);
        if (err) { ret = err; err = NULL; goto done; }
    } else {
        si = uc2_call0(sp, "stdin_isatty");
        if (uc2_is_raise(si)) { ret = si; si = NULL; goto done; }
        bool tty = uc2_jtruth(si); json_free(si); si = NULL;
        if (tty) {
            so = uc2_call0(sp, "stdout_isatty");
            if (uc2_is_raise(so)) { ret = so; so = NULL; goto done; }
            tty = uc2_jtruth(so); json_free(so); so = NULL;
        }
        if (!tty) {
            discard = uc2_config_eq(sp, "non_interactive_local_changes", "stash", "discard", true, &err);
            if (err) { ret = err; err = NULL; goto done; }
        }
    }
    ret = json_object();
    uc2_aset_j(ret, "pre_update_version", ver); ver = NULL;
    if (gateway_mode) {
        json_t *fn = json_object(); json_set(fn, "__callable__", json_bool(true));
        uc2_aset_j(ret, "gw_input_fn", fn); json_free(fn);
    } else uc2_aset_j(ret, "gw_input_fn", NULL);
    uc2_aset_b(ret, "assume_yes", assume_yes);
    uc2_aset_b(ret, "keep_stash", uc2_jtruth(ks));
    uc2_aset_b(ret, "switch_branch", uc2_jtruth(sb));
    uc2_aset_b(ret, "discard_local_changes", discard);
    uc2_aset_b(ret, "no_gateway_restart", uc2_jtruth(ngr));
    uc2_aset_b(ret, "force", uc2_jtruth(force));
    json_free(yes); json_free(ks); json_free(sb); json_free(ngr); json_free(force);
done:
    if (ver) json_free(ver);
    if (err) json_free(err);
    if (si) json_free(si);
    if (so) json_free(so);
    return ret;
}

/* ===================================================================
 * _record_update_initiator  (update_cmd.py:1549)
 * =================================================================== */
/* PoP: uc2_record_update_initiator @ hermes_cli/update_cmd.py:_record_update_initiator */
static json_t *uc2_record_update_initiator(const uc2_seams_t *sp) {
    json_t *ret = NULL, *path = NULL, *text = NULL, *tmp = NULL, *pid = NULL;
    path = uc2_call0(sp, "update_marker_path");
    if (uc2_is_raise(path)) {
        if (uc2_is_exception(path)) { json_free(path); return NULL; }
        return path;
    }
    json_t *ra = json_object();
    uc2_aset_s(ra, "path", uc2_jstr_def(path, ""));
    uc2_aset_s(ra, "encoding", "utf-8-sig");
    text = uc2_call(sp, "marker_read_text", ra); json_free(ra);
    if (uc2_is_raise(text)) {
        json_free(path);
        if (uc2_is_exception(text)) { json_free(text); return NULL; }
        return text;
    }
    json_free(path);
    const char *tv = uc2_jstr_def(text, "");
    const char *nl = strchr(tv, '\n');
    char *first = nl ? uc2_slice(tv, (size_t)(nl - tv)) : strdup(tv);
    char *stripped = uc2_strip(first); free(first);
    bool dig = uc2_isdigit(stripped);
    if (dig) {
        long long fv = 0;
        json_t *ie = uc2_py_int(stripped, &fv);
        if (ie) { free(stripped); json_free(text);
            if (uc2_is_exception(ie)) { json_free(ie); return NULL; } return ie; }
        pid = uc2_call0(sp, "getpid");
        if (uc2_is_raise(pid)) { free(stripped); json_free(text);
            if (uc2_is_exception(pid)) { json_free(pid); return NULL; } return pid; }
        long long pv = uc2_is_num(pid) ? (long long)pid->num_val : 0;
        json_free(pid);
        if (fv != pv) {
            json_t *fa = json_object();
            uc2_aset_s(fa, "key", "initiator");
            uc2_aset_s(fa, "value", "desktop");
            tmp = uc2_call(sp, "record_fact", fa); json_free(fa);
            if (uc2_is_raise(tmp)) { free(stripped); json_free(text);
                if (uc2_is_exception(tmp)) { json_free(tmp); return NULL; } return tmp; }
            json_free(tmp);
        }
    }
    free(stripped); json_free(text);
    ret = NULL;
    return ret;
}

/* ===================================================================
 * _begin_update_receipt_and_plan  (update_cmd.py:1520)
 * =================================================================== */
/* PoP: uc2_begin_update_receipt_and_plan @ hermes_cli/update_cmd.py:_begin_update_receipt_and_plan */
static json_t *uc2_begin_update_receipt_and_plan(const uc2_seams_t *sp) {
    json_t *ret = NULL, *tmp = NULL, *plan = NULL, *runtimes = NULL, *err = NULL;
    /* with _best_effort('Update receipt unavailable'): begin_update_receipt(); _record_update_initiator() */
    tmp = uc2_call0(sp, "begin_update_receipt");
    if (uc2_is_raise(tmp)) {
        if (!uc2_is_exception(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    } else {
        json_free(tmp);
        tmp = uc2_record_update_initiator(sp);
        if (uc2_is_raise(tmp)) {
            if (!uc2_is_exception(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        } else json_free(tmp); tmp = NULL;
    }
    /* with _best_effort('Update plan phase failed'): collect; record_plan; if runtimes print */
    plan = uc2_call0(sp, "collect_runtime_inventory");
    if (uc2_is_raise(plan)) {
        if (!uc2_is_exception(plan)) { ret = plan; plan = NULL; goto done; }
        json_free(plan); plan = NULL;
    } else {
        json_t *rpa = json_object(); uc2_aset_j(rpa, "plan", plan);
        tmp = uc2_call(sp, "record_plan_in_receipt", rpa); json_free(rpa);
        if (uc2_is_raise(tmp)) {
            if (!uc2_is_exception(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        } else {
            json_free(tmp); tmp = NULL;
            runtimes = uc2_attr(plan, "runtimes");
            if (uc2_is_raise(runtimes)) {
                if (!uc2_is_exception(runtimes)) { ret = runtimes; runtimes = NULL; goto done; }
                json_free(runtimes); runtimes = NULL;
            } else {
                if (uc2_jtruth(runtimes)) {
                    size_t n = uc2_is_arr(runtimes) ? json_len(runtimes) : 0;
                    /* live: ", ".join(sorted({r.profile for r in runtimes}))
                     * — attribute projection inside the best-effort try. A
                     * non-str profile makes join() raise TypeError (swallowed,
                     * no print); a missing .profile raises AttributeError
                     * (swallowed); BaseException propagates. A truthy
                     * non-array is not iterable / yields str keys whose
                     * .profile raises — also swallowed, no print. */
                    json_t *projs = json_array();
                    json_t *perr = NULL;
                    bool all_str = true;
                    if (uc2_is_arr(runtimes)) {
                        for (size_t i = 0; i < runtimes->c.count; i++) {
                            json_t *p = uc2_attr(runtimes->c.items[i], "profile");
                            if (uc2_is_raise(p)) { perr = p; break; }
                            if (!uc2_is_str(p)) all_str = false;
                            json_append(projs, p);
                        }
                    } else {
                        all_str = false;
                    }
                    if (perr) {
                        json_free(projs);
                        if (!uc2_is_exception(perr)) { ret = perr; goto done; }
                        json_free(perr);
                    } else if (all_str) {
                        char *profiles = uc2_sorted_unique_join(projs, ", ");
                        json_free(projs);
                        char *m = uc2_xsf(UC2_ARROW " Fleet: %zu running service(s) across profiles: %s", n, profiles);
                        free(profiles);
                        tmp = uc2_print(sp, m); free(m);
                        if (uc2_is_raise(tmp)) {
                            if (!uc2_is_exception(tmp)) { ret = tmp; tmp = NULL; goto done; }
                            json_free(tmp); tmp = NULL;
                        } else { json_free(tmp); tmp = NULL; }
                    } else {
                        json_free(projs);
                    }
                }
                json_free(runtimes); runtimes = NULL;
            }
        }
    }
    /* record_stage("plan", "failed" if plan is None else "success") */
    json_t *rsa = json_object();
    uc2_aset_s(rsa, "stage", "plan");
    uc2_aset_s(rsa, "status", uc2_is_null(plan) ? "failed" : "success");
    tmp = uc2_call(sp, "record_stage", rsa); json_free(rsa);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    ret = plan; plan = NULL;
done:
    if (tmp) json_free(tmp);
    if (plan) json_free(plan);
    if (runtimes) json_free(runtimes);
    if (err) json_free(err);
    return ret;
}
/* ===================================================================
 * _prepare_git_command  (update_cmd.py:1560)  — returns [use_zip, git_cmd, is_fork]
 * =================================================================== */
/* PoP: uc2_prepare_git_command @ hermes_cli/update_cmd.py:_prepare_git_command */
static json_t *uc2_prepare_git_command(const uc2_seams_t *sp, const json_t *checkout_move, const char *platform) {
    json_t *ret = NULL, *root = NULL, *tmp = NULL, *git_cmd = NULL, *ex = NULL, *enc = NULL, *origin = NULL;
    char *root_s = NULL, *git_dir = NULL;
    bool use_zip, is_fork = false;
    const char *plat = platform ? platform : "linux";
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    root_s = strdup(uc2_jstr_def(root, ""));
    git_dir = uc2_posix_join(root_s, ".git");
    json_t *ea = json_object(); uc2_aset_s(ea, "path", git_dir);
    ex = uc2_call(sp, "path_exists", ea); json_free(ea);
    if (uc2_is_raise(ex)) { ret = ex; ex = NULL; goto done; }
    use_zip = !uc2_jtruth(ex); json_free(ex); ex = NULL;
    if (use_zip && strcmp(plat, "win32") != 0) {
        tmp = uc2_print(sp, UC2_CROSS " Not a git repository. Please reinstall:");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_print(sp, "  curl -fsSL https://hermes-agent.nousresearch.com/install.sh | bash");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_record_stop(sp, "not_git_checkout", NULL);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        ret = uc2_raise_spec("SystemExit", "1"); goto done;
    }
    /* expose_pm_git(_m().PROJECT_ROOT) */
    json_free(root);
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    json_t *pa = json_object(); uc2_aset_s(pa, "root", uc2_jstr_def(root, ""));
    tmp = uc2_call(sp, "expose_pm_git", pa); json_free(pa);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    git_cmd = uc2_call0(sp, "base_git_cmd");
    if (uc2_is_raise(git_cmd)) { ret = git_cmd; git_cmd = NULL; goto done; }
    if (!strcmp(plat, "win32")) {
        ea = json_object(); uc2_aset_s(ea, "path", git_dir);
        ex = uc2_call(sp, "path_exists", ea); json_free(ea);
        if (uc2_is_raise(ex)) { ret = ex; ex = NULL; goto done; }
        bool exists = uc2_jtruth(ex); json_free(ex); ex = NULL;
        if (exists) {
            json_t *ca = json_array();
            json_append(ca, json_string("config")); json_append(ca, json_string("windows.appendAtomically"));
            json_append(ca, json_string("false"));
            tmp = uc2_git_run(sp, git_cmd, ca, false); json_free(ca);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
    }
    /* _map_ssl_cert_file_for_git(git_cmd) */
    json_t *ma = json_object(); uc2_aset_j(ma, "git_cmd", git_cmd);
    tmp = uc2_call(sp, "map_ssl_cert_file_for_git", ma); json_free(ma);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    /* git_cmd = _ensure_non_trampoline_git(git_cmd) */
    json_t *na = json_object(); uc2_aset_j(na, "git_cmd", git_cmd);
    json_free(git_cmd);
    git_cmd = uc2_call(sp, "ensure_non_trampoline_git", na); json_free(na);
    if (uc2_is_raise(git_cmd)) { ret = git_cmd; git_cmd = NULL; goto done; }
    /* if git_dir.exists() and (enclosing := _enclosing_repo_of_checkout(git_cmd, root)) */
    ea = json_object(); uc2_aset_s(ea, "path", git_dir);
    ex = uc2_call(sp, "path_exists", ea); json_free(ea);
    if (uc2_is_raise(ex)) { ret = ex; ex = NULL; goto done; }
    bool exists = uc2_jtruth(ex); json_free(ex); ex = NULL;
    if (exists) {
        json_free(root);
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        json_t *eca = json_object();
        uc2_aset_j(eca, "git_cmd", git_cmd);
        uc2_aset_s(eca, "root", uc2_jstr_def(root, ""));
        enc = uc2_call(sp, "enclosing_repo_of_checkout", eca); json_free(eca);
        if (uc2_is_raise(enc)) { ret = enc; enc = NULL; goto done; }
        if (uc2_jtruth(enc)) {
            char *m = uc2_xsf(UC2_CROSS " git resolves this checkout to the enclosing repository %s; refusing to stash, reset or clean it.",
                              uc2_jstr_def(enc, ""));
            tmp = uc2_print(sp, m); free(m);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
            json_free(root);
            root = uc2_project_root(sp);
            if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
            json_t *da = json_object(); uc2_aset_s(da, "root", uc2_jstr_def(root, ""));
            json_t *dm = uc2_call(sp, "damaged_checkout_message", da); json_free(da);
            if (uc2_is_raise(dm)) { ret = dm; goto done; }
            tmp = uc2_print(sp, uc2_jstr_def(dm, "")); json_free(dm);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp);
            tmp = uc2_record_stop(sp, "not_git_checkout", NULL);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
            ret = uc2_raise_spec("SystemExit", "1"); goto done;
        }
        json_free(enc); enc = NULL;
    }
    /* _discard_lockfile_churn / _normalize_managed_eol (git_cmd, root, checkout_move=) */
    json_free(root);
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    json_t *dla = json_object();
    uc2_aset_j(dla, "git_cmd", git_cmd);
    uc2_aset_s(dla, "root", uc2_jstr_def(root, ""));
    uc2_aset_j(dla, "checkout_move", checkout_move);
    tmp = uc2_call(sp, "discard_lockfile_churn", dla); json_free(dla);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    json_free(root);
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    json_t *nla = json_object();
    uc2_aset_j(nla, "git_cmd", git_cmd);
    uc2_aset_s(nla, "root", uc2_jstr_def(root, ""));
    uc2_aset_j(nla, "checkout_move", checkout_move);
    tmp = uc2_call(sp, "normalize_managed_eol", nla); json_free(nla);
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    /* origin_url = _m()._get_origin_url(git_cmd, root); is_fork = _is_fork(origin_url) */
    json_free(root);
    root = uc2_project_root(sp);
    if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
    json_t *oa = json_object();
    uc2_aset_j(oa, "git_cmd", git_cmd);
    uc2_aset_s(oa, "root", uc2_jstr_def(root, ""));
    origin = uc2_call(sp, "get_origin_url", oa); json_free(oa);
    if (uc2_is_raise(origin)) { ret = origin; origin = NULL; goto done; }
    json_t *fa = json_object(); uc2_aset_j(fa, "origin_url", origin);
    json_t *fr = uc2_call(sp, "is_fork", fa); json_free(fa);
    if (uc2_is_raise(fr)) { ret = fr; goto done; }
    is_fork = uc2_jtruth(fr); json_free(fr);
    if (is_fork) {
        tmp = uc2_print(sp, UC2_WARN " Updating from fork:");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        char *m = uc2_xsf("  %s", uc2_jstr_def(origin, ""));
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_print(sp, "");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    }
    ret = json_array();
    json_append(ret, json_bool(use_zip));
    json_append(ret, git_cmd); git_cmd = NULL;
    json_append(ret, json_bool(is_fork));
done:
    free(root_s); free(git_dir);
    if (root) json_free(root);
    if (tmp) json_free(tmp);
    if (git_cmd) json_free(git_cmd);
    if (ex) json_free(ex);
    if (enc) json_free(enc);
    if (origin) json_free(origin);
    return ret;
}

/* ===================================================================
 * _handle_update_called_process_error  (update_cmd.py:1658)
 * =================================================================== */
/* PoP: uc2_handle_update_called_process_error @ hermes_cli/update_cmd.py:_handle_update_called_process_error */
static json_t *uc2_handle_update_called_process_error(const uc2_seams_t *sp, const json_t *e,
                                                      const json_t *args, bool gateway_mode,
                                                      bool had_desktop, const json_t *target_sha,
                                                      const json_t *target_repository, const json_t *completion_request) {
    json_t *ret = NULL, *tmp = NULL, *unrestored = NULL, *stage = NULL, *ep = NULL, *stop = NULL;
    (void)gateway_mode;
    unrestored = uc2_call0(sp, "unrestored_autostash_notice");
    if (uc2_is_raise(unrestored)) { ret = unrestored; unrestored = NULL; goto done; }
    if (uc2_jtruth(unrestored)) {
        tmp = uc2_print(sp, uc2_jstr_def(unrestored, ""));
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    }
    ep = uc2_e_proj(e);
    json_t *sa = json_object(); uc2_aset_j(sa, "e", ep);
    stage = uc2_call(sp, "format_update_failure_stage", sa); json_free(sa);
    if (uc2_is_raise(stage)) { ret = stage; stage = NULL; goto done; }
    json_t *za = json_object(); uc2_aset_j(za, "e", ep);
    json_t *zf = uc2_call(sp, "should_zip_fallback_on_update_error", za); json_free(za);
    if (uc2_is_raise(zf)) { ret = zf; goto done; }
    bool zip = uc2_jtruth(zf); json_free(zf);
    if (zip) {
        char *es = uc2_cpe_str(e);
        char *m = uc2_xsf(UC2_WARN " %s: %s", uc2_jstr_def(stage, ""), es); free(es);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_print(sp, UC2_ARROW " Falling back to ZIP download...");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_print(sp, "");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        json_t *ua = json_object();
        uc2_aset_j(ua, "args", args);
        uc2_aset_b(ua, "had_desktop_app_before_update", had_desktop);
        uc2_aset_j(ua, "target_sha", target_sha);
        uc2_aset_j(ua, "completion_request", completion_request);
        if (uc2_jtruth(target_repository)) uc2_aset_j(ua, "target_repository", target_repository);
        tmp = uc2_call(sp, "update_via_zip", ua); json_free(ua);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        ret = NULL; goto done;
    }
    json_t *da = json_object(); uc2_aset_j(da, "e", ep);
    json_t *dep = uc2_call(sp, "called_process_error_is_python_dep_install", da); json_free(da);
    if (uc2_is_raise(dep)) { ret = dep; goto done; }
    bool is_dep = uc2_jtruth(dep); json_free(dep);
    if (is_dep) {
        char *m = uc2_xsf(UC2_CROSS " %s (the code update itself succeeded).", uc2_jstr_def(stage, ""));
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        json_t *ta = json_object(); uc2_aset_j(ta, "e", ep);
        tmp = uc2_call(sp, "print_called_process_error_tail", ta); json_free(ta);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_print(sp, "");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_print(sp, "  Hermes may not start until the dependencies are installed. Fix the error above");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp);
        tmp = uc2_print(sp, "  (usually network or disk space), then run `hermes update` again.");
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        json_t *iw = uc2_call0(sp, "is_windows");
        if (uc2_is_raise(iw)) { ret = iw; goto done; }
        bool win = uc2_jtruth(iw); json_free(iw);
        if (win) {
            tmp = uc2_print(sp, "  If `hermes update` itself will not start, retry through the venv interpreter:");
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp);
            tmp = uc2_print(sp, "    venv\\Scripts\\python.exe -c \"from hermes_cli.main import main; main()\" update --yes");
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
    } else {
        char *m = uc2_xsf(UC2_CROSS " %s.", uc2_jstr_def(stage, ""));
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        char *es = uc2_cpe_str(e);
        m = uc2_xsf("  Details: %s", es); free(es);
        tmp = uc2_print(sp, m); free(m);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
        json_t *ta = json_object(); uc2_aset_j(ta, "e", ep);
        tmp = uc2_call(sp, "print_called_process_error_tail", ta); json_free(ta);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    }
    json_t *ga = json_object(); uc2_aset_j(ga, "e", ep);
    stop = uc2_call(sp, "git_error_stop_class", ga); json_free(ga);
    if (uc2_is_raise(stop)) { ret = stop; stop = NULL; goto done; }
    if (uc2_jtruth(stop)) {
        tmp = uc2_record_stop(sp, uc2_jstr_def(stop, ""), NULL);
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    }
    tmp = uc2_finalize_receipt(sp, "failed", "Update receipt finalize failed: %s");
    if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
    json_free(tmp); tmp = NULL;
    ret = uc2_raise_spec("SystemExit", "1");
done:
    if (tmp) json_free(tmp);
    if (unrestored) json_free(unrestored);
    if (stage) json_free(stage);
    if (ep) json_free(ep);
    if (stop) json_free(stop);
    return ret;
}

/* ===================================================================
 * _finish_already_up_to_date  (update_cmd.py:1703)  — mutates request
 * =================================================================== */
/* PoP: uc2_finish_already_up_to_date @ hermes_cli/update_cmd.py:_finish_already_up_to_date */
static json_t *uc2_finish_already_up_to_date(const uc2_seams_t *sp, const json_t *git_cmd,
                                             const char *branch, const char *current_branch,
                                             const json_t *plan, const json_t *gw_input_fn,
                                             json_t *completion_request, const json_t *token,
                                             const json_t *held_message) {
    json_t *ret = NULL, *tmp = NULL, *root = NULL, *cm = NULL, *asr = NULL, *pfr = NULL;
    json_t *pbs = NULL, *sbr = NULL, *cap = NULL, *uc = NULL;
    const char *br = branch ? branch : "";
    const char *cur = current_branch ? current_branch : "";
    asr = uc2_attr(plan, "auto_stash_ref");
    if (uc2_is_raise(asr)) { ret = asr; asr = NULL; goto done; }
    if (!uc2_is_null(asr)) {
        pfr = uc2_attr(plan, "prompt_for_restore");
        if (uc2_is_raise(pfr)) { ret = pfr; pfr = NULL; goto done; }
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        cm = uc2_moves_for(sp, token);
        if (uc2_is_raise(cm)) { ret = cm; cm = NULL; goto done; }
        tmp = uc2_restore_stashed_changes(sp, git_cmd, uc2_jstr_def(root, ""), asr, uc2_jtruth(pfr), gw_input_fn, cm);
        json_free(cm); cm = NULL; json_free(root); root = NULL;
        if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
        json_free(tmp); tmp = NULL;
    }
    pbs = uc2_attr(plan, "parked_branch_switched");
    if (uc2_is_raise(pbs)) { ret = pbs; pbs = NULL; goto done; }
    if (uc2_jtruth(pbs)) {
        sbr = uc2_attr(plan, "switch_block_reason");
        if (uc2_is_raise(sbr)) { ret = sbr; sbr = NULL; goto done; }
        const char *rv = uc2_jstr_def(sbr, "");
        if (uc2_startswith(rv, "unmerged:")) {
            const char *colon = strchr(rv, ':');
            const char *count = colon ? colon + 1 : "";
            char *m = uc2_xsf("  " UC2_CHECK " Checkout was parked on '%s' " UC2_EMDASH " switched back to %s; %s unmerged commit(s) kept on '%s'.",
                              cur, br, count, cur);
            tmp = uc2_print(sp, m); free(m);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        } else {
            char *m = uc2_xsf("  " UC2_CHECK " Checkout was parked on '%s' (fully merged) " UC2_EMDASH " switched back to %s.", cur, br);
            tmp = uc2_print(sp, m); free(m);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
        }
    }
    if (!uc2_is_null(completion_request)) {
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        cap = uc2_capture_head_sha(sp, git_cmd, uc2_jstr_def(root, ""));
        json_free(root); root = NULL;
        if (uc2_is_raise(cap)) { ret = cap; cap = NULL; goto done; }
        const char *exp = uc2_jtruth(cap) ? uc2_jstr_def(cap, "") : "";
        json_set(completion_request, "expected_sha", json_string(exp));
        const char *msg;
        if (uc2_jtruth(held_message)) msg = uc2_jstr_def(held_message, "");
        else {
            uc = uc2_attr(plan, "upstream_checked");
            if (uc2_is_raise(uc)) { ret = uc; uc = NULL; goto done; }
            msg = uc2_jtruth(uc) ? UC2_CHECK " Already up to date!"
                                 : UC2_CHECK " Up to date with your fork (official repo not checked).";
        }
        json_set(completion_request, "completion_message", json_string(msg));
    }
    ret = uc2_complete_source_update(sp, completion_request);
done:
    if (tmp) json_free(tmp);
    if (root) json_free(root);
    if (cm) json_free(cm);
    if (asr) json_free(asr);
    if (pfr) json_free(pfr);
    if (pbs) json_free(pbs);
    if (sbr) json_free(sbr);
    if (cap) json_free(cap);
    if (uc) json_free(uc);
    return ret;
}

/* ===================================================================
 * _apply_pulled_update  (update_cmd.py:1735)  — mutates request
 * =================================================================== */
/* PoP: uc2_apply_pulled_update @ hermes_cli/update_cmd.py:_apply_pulled_update */
static json_t *uc2_apply_pulled_update(const uc2_seams_t *sp, const json_t *git_cmd, const char *branch,
                                       const json_t *movement_baseline, const json_t *plan,
                                       const json_t *token, json_t *completion_request) {
    json_t *ret = NULL, *tmp = NULL, *root = NULL, *ip = NULL, *post = NULL, *cap = NULL, *pinned = NULL;
    const char *br = branch ? branch : "";
    ip = uc2_attr(plan, "in_place_update");
    if (uc2_is_raise(ip)) { ret = ip; ip = NULL; goto done; }
    post = uc2_verify_head_after_pull(sp, git_cmd, br, movement_baseline, uc2_jtruth(ip), token);
    if (uc2_is_raise(post)) { ret = post; post = NULL; goto done; }
    if (!uc2_is_null(completion_request)) {
        root = uc2_project_root(sp);
        if (uc2_is_raise(root)) { ret = root; root = NULL; goto done; }
        cap = uc2_capture_head_sha(sp, git_cmd, uc2_jstr_def(root, ""));
        json_free(root); root = NULL;
        if (uc2_is_raise(cap)) { ret = cap; cap = NULL; goto done; }
        const char *observed = uc2_jtruth(cap) ? uc2_jstr_def(cap, "") : uc2_jstr_def(post, "");
        pinned = uc2_dget(completion_request, "expected_sha", NULL);
        if (uc2_is_raise(pinned)) { ret = pinned; pinned = NULL; goto done; }
        if (!uc2_is_null(pinned) && strcmp(observed, uc2_jstr_def(pinned, "")) != 0) {
            tmp = uc2_print(sp, UC2_CROSS " Checkout no longer matches the selected channel commit. No completion was applied.");
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp);
            tmp = uc2_resume_gateways(sp, token);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp);
            tmp = uc2_record_stop(sp, "head_moved", NULL);
            if (uc2_is_raise(tmp)) { ret = tmp; tmp = NULL; goto done; }
            json_free(tmp); tmp = NULL;
            ret = uc2_raise_spec("SystemExit", "1"); goto done;
        }
        const char *newexp = uc2_jtruth(pinned) ? uc2_jstr_def(pinned, "") : observed;
        json_set(completion_request, "expected_sha", json_string(newexp));
    }
    ret = uc2_complete_source_update(sp, completion_request);
done:
    if (tmp) json_free(tmp);
    if (root) json_free(root);
    if (ip) json_free(ip);
    if (post) json_free(post);
    if (cap) json_free(cap);
    if (pinned) json_free(pinned);
    return ret;
}
