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
    while (*p && uc2_is_ws((unsigned char)*p)) p++;
    const char *end = p + strlen(p);
    while (end > p && uc2_is_ws((unsigned char)end[-1])) end--;
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
