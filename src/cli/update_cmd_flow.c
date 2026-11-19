/*
 * update_cmd_flow.c — FLOW port of the UNCLAIMED update-pipeline surface of
 * hermes_cli/update_cmd.py (LIVE authority). Self-contained: libjson only. All
 * functions static uc_*; the harness (#include "cli/update_cmd_flow.c") wires
 * the universal seam to scripted results and records the ordered trace.
 *
 * Covered flows (PoP per function):
 *   retired shims: get_default_hermes_root, _ensure_venv_pip, _pip_install_prefix
 *   config/receipt leaves: _updates_config, _record_update_step,
 *     _record_update_skip, _record_pre_update_backup_outcome,
 *     _record_snapshot_stage, _finalize_receipt
 *   git plumbing: _git_run, _base_git_cmd, _is_shallow_checkout, _tip_shas,
 *     _resolved_commit, _current_branch_name, _landed_off_target,
 *     _update_movement_baseline, _missing_branch_stop, _refuse_unselected_head,
 *     _verify_head_after_pull, _map_ssl_cert_file_for_git,
 *     _heal_stale_shallow_checkout
 *   channel/resume: _source_update_channel, _update_run_channel,
 *     _settle_windows_resume, _resume_paused_gateways_at_exit,
 *     _pause_gateways_for_update, _upstream_broken_header
 *
 * SEAM DOCTRINE: every live effect enters through ONE universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returning a NEW json_t* (NULL == Python None). A raising live call is
 * scripted {"__raise__": "<ExcClass>", "msg": ...}; the cores propagate it
 * exactly where the live try/except catches it. except Exception does NOT
 * catch KeyboardInterrupt/SystemExit/GeneratorExit/BaseException — those pass
 * through to the harness ret envelope (BaseException passthrough). A MISSING
 * seam key fail-closes: the harness delivers {"__raise__":"RuntimeError",
 * "msg":"unscripted seam: <name>"}; a present JSON null is a valid None.
 *
 * In-function imports are SILENT (the oracle pre-installs stub modules in
 * sys.modules; no import seam fires). Only effectful calls fire seams.
 * Claimed siblings (_capture_head_sha, _resolve_pre_update_backup_mode,
 * _no_prompt_git_kwargs) and foreign collaborators (_record_stop,
 * stop_historical_updater, _m() helpers, _commit.disarm_commit_obligations,
 * _posix_pause.arm_pause, _completion_receipt.record_stage, run_git,
 * heal_shallow_history, validate_name, resolve_update_channel, load_config,
 * get_config_path, require_readable_config_before_write, record_step,
 * record_skip, finalize_update_receipt, owe_followup, atexit.unregister,
 * os.environ) are SEAMED. Unclaimed ported siblings compose for real
 * (_git_run, _resolved_commit, _current_branch_name, _landed_off_target,
 * _source_update_channel, _record_update_step, _record_update_skip).
 * _best_effort (update_cmd_common, unclaimed) is transcribed inline as its
 * suppress-Exception + silent logger.debug semantics.
 *
 * Seam name table (name -> live call, args):
 *   print                       print(text) -> null
 *   record_stop                 _record_stop(reason, without_receipt)
 *   project_root                _m().PROJECT_ROOT -> path str
 *   stop_historical_updater     stop_historical_updater(hook)
 *   load_config                 load_config() -> dict|null
 *   record_step                 record_step(step, ok, detail)
 *   record_skip                 record_skip(step, reason)
 *   resolve_pre_update_backup_mode  _resolve_pre_update_backup_mode(args) -> str
 *   record_stage                _completion_receipt.record_stage(stage, status)
 *   finalize_update_receipt     finalize_update_receipt(status)
 *   no_prompt_git_kwargs        _no_prompt_git_kwargs() -> dict
 *   windows_hide_flags          windows_hide_flags() -> int
 *   run_git                     run_git(git_cmd, args, cwd, capture_output, text,
 *                                 encoding, errors, check, **spawn_kwargs) -> record
 *   capture_head_sha            _capture_head_sha(git_cmd, cwd) -> str|null
 *   disarm_commit_obligations   _commit.disarm_commit_obligations()
 *   resume_windows_gateways_after_update  _m()._resume...(token)
 *   pause_windows_gateways_for_update     _m()._pause...() -> token|null
 *   arm_pause                   _posix_pause.arm_pause(no_gateway_restart, force)
 *   environ_get                 os.environ.get(name) -> str|null
 *   environ_set                 os.environ[name] = value
 *   heal_shallow_history        heal_shallow_history(repo_root, branch, **kw) -> bool
 *   validate_name               validate_name(name) -> str
 *   get_config_path             get_config_path() -> path str
 *   require_readable_config_before_write  require_readable_config_before_write(path)
 *   resolve_update_channel      resolve_update_channel(config, root) -> str
 *   atexit_unregister           atexit.unregister(fn) {fn: marker}
 *   owe_followup                owe_followup(update_id, kind, message)
 *
 * Representation contract (both sides): paths are verbatim POSIX strings;
 * an uncaught exception rides as {"__raise__": cls, "msg": str(exc)} (the
 * oracle drive's BaseException catch builds exactly that shape). SystemExit
 * msg is str(code) ("1"/"2"). Integral JSON numbers read as int (libjson has
 * no int/float identity).
 *
 * Fixture domains (documented exclusions):
 * - ASCII strings; POSIX path strings. Message CONSTANTS carry byte-pinned
 *   UTF-8 glyphs (check/cross/warn/info/em-dash), not fixture data.
 * - git_cmd / args / cmd are JSON arrays of ASCII strings (no quotes,
 *   backslashes or control chars) or a plain string; args is NON-EMPTY where
 *   _git_run's timeout stderr names args[0].
 * - run_git scripts are null/raise or COMPLETE records {returncode, stdout,
 *   stderr} (stdout/stderr str-or-null, returncode int); missing-field
 *   AttributeError doubles are outside the domain.
 * - subprocess raise specs: TimeoutExpired {cmd, timeout}, CalledProcessError
 *   {returncode, cmd, output?, stderr?}, SubprocessError/OSError {msg}. str()
 *   of CalledProcessError/TimeoutExpired is transcribed ("Command '<cmd>'
 *   returned non-zero exit status N." / "... timed out after N seconds").
 * - resume/owe raise scripts use classes whose str()==msg (RuntimeError,
 *   ValueError, OSError, Exception); SystemExit/KeyboardInterrupt pass through.
 * - args/opts/broken/token/request are JSON objects (attribute doubles) or
 *   null; snapshot_id is str-or-null; sha/branch/ref strings are ASCII.
 * - str.strip()/splitlines() whitespace is the ASCII White_Space set (incl
 *   \x1c-\x1f); splitlines boundaries \n \r \v \f (fixtures use \n).
 * - logger is silent (no trace events), matching the sibling flow ports.
 *
 * Ownership: seam results and returned json_t / char pointers are malloc'd,
 * caller frees. Inputs are borrowed. uc_call borrows args (caller frees).
 * A propagated raise dict is returned upward as-is; a caught raise dict is
 * freed at the catch site.
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
#define UC_CROSS "\xe2\x9c\x97"   /* ✗ */
#define UC_CHECK "\xe2\x9c\x93"   /* ✓ */
#define UC_WARN  "\xe2\x9a\xa0"   /* ⚠ */
#define UC_INFO  "\xe2\x84\xb9"   /* ℹ */
#define UC_EMDASH "\xe2\x80\x94"  /* — */

/* NETWORK_GIT_TIMEOUT_SECONDS (update_cmd.py:228). */
#define UC_NETWORK_GIT_TIMEOUT_SECONDS 300

/* ── universal seam + trace ── */

typedef struct uc_trace { json_t *events; } uc_trace_t;

typedef struct uc_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} uc_seams_t;

static void uc_trace_add(uc_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void uc_trace_free(uc_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *uc_call(const uc_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *uc_call0(const uc_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = uc_call(s, name, a);
    json_free(a);
    return r;
}

/* ── JSON value helpers ── */

static bool uc_jtruth(const json_t *v) {
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
static const char *uc_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *uc_jstr_def(const json_t *v, const char *def) {
    const char *s = uc_jstr(v);
    return s ? s : def;
}
static bool uc_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool uc_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool uc_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool uc_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

static char *uc_xsf(const char *fmt, ...) {
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

typedef struct { char *p; size_t len, cap; } uc_buf;
static void uc_binit(uc_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void uc_bfree(uc_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void uc_bput(uc_buf *b, const char *s, size_t n) {
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
static void uc_bputs(uc_buf *b, const char *s) { uc_bput(b, s, strlen(s)); }
static void uc_bputc(uc_buf *b, char c) { uc_bput(b, &c, 1); }
static char *uc_btake(uc_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* ── exception model (FULL builtin parent hierarchy) ── */

static const char *uc_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool uc_is_raise(const json_t *r) { return uc_raise_class(r) != NULL; }

static json_t *uc_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static bool uc_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
/* Parent lookup mirroring the py3.14 builtin hierarchy plus the subprocess
 * family and the custom kinds the oracle registers. Unknown custom kinds
 * default to Exception (the oracle builds type(name,(Exception,)) for them);
 * the BaseException trio stays OUTSIDE Exception so except Exception passes
 * them through. */
static const char *uc_exc_parent(const char *k) {
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
    if (uc_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (uc_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (uc_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (uc_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (uc_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (uc_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError", NULL };
    if (uc_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    static const char *const SUBPROC_CHILDREN[] = { "CalledProcessError", "TimeoutExpired", NULL };
    if (uc_in_names(k, SUBPROC_CHILDREN)) return "SubprocessError";
    if (!strcmp(k, "RetiredUpdaterHook")) return "RuntimeError";
    if (!strcmp(k, "ChannelError")) return "ValueError";
    if (!strcmp(k, "UpstreamTargetBroken")) return "Exception";
    if (!strcmp(k, "PythonFinalizationError")) return "RuntimeError";
    if (!strcmp(k, "_IncompleteInputError")) return "SyntaxError";
    /* BaseExceptionGroup subclasses BaseException directly (outside Exception);
     * ExceptionGroup multiply-inherits BaseExceptionGroup AND Exception — model
     * the Exception path so except Exception catches it (the only group catch
     * any ported flow uses). */
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "Exception";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (uc_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (uc_in_names(k, WARNING_CHILDREN)) return "Warning";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (uc_in_names(k, NAME_CHILDREN)) return "NameError";
    return "Exception";  /* unknown custom kind: oracle bases it on Exception */
}
/* caught(r, cls): cls appears anywhere in r's parent chain */
static bool uc_caught(const json_t *r, const char *cls) {
    const char *k = uc_raise_class(r);
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = uc_exc_parent(k);
    }
    return false;
}
static bool uc_is_exception(const json_t *r) { return uc_caught(r, "Exception"); }

/* Python type name of a JSON value (integral numbers read as int) */
static const char *uc_typename(const json_t *v) {
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
static json_t *uc_attr_raise(const char *tname, const char *attr) {
    char *msg = uc_xsf("'%s' object has no attribute '%s'", tname, attr);
    json_t *e = uc_raise_spec("AttributeError", msg);
    free(msg);
    return e;
}
static json_t *uc_typeerror(const char *msg) { return uc_raise_spec("TypeError", msg); }
static json_t *uc_keyerror(const char *key) {
    char *msg = uc_xsf("'%s'", key);
    json_t *e = uc_raise_spec("KeyError", msg);
    free(msg);
    return e;
}

/* ── args builders ── */

static void uc_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void uc_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void uc_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void uc_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── Python float repr / str / repr ── */

static char *uc_float_repr(double v) {
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
    uc_buf b; uc_binit(&b);
    if (neg) uc_bputc(&b, '-');
    if (exp >= -4 && exp < 16) {
        if (exp >= 0) {
            size_t point = (size_t)exp + 1;
            if (nd <= point) {
                uc_bput(&b, digits, nd);
                for (size_t i = nd; i < point; i++) uc_bputc(&b, '0');
                uc_bputs(&b, ".0");
            } else {
                uc_bput(&b, digits, point);
                uc_bputc(&b, '.');
                uc_bputs(&b, digits + point);
            }
        } else {
            uc_bputs(&b, "0.");
            for (int i = 0; i < -exp - 1; i++) uc_bputc(&b, '0');
            uc_bputs(&b, digits);
        }
    } else {
        uc_bputc(&b, digits[0]);
        if (nd > 1) { uc_bputc(&b, '.'); uc_bputs(&b, digits + 1); }
        char ex[16];
        snprintf(ex, sizeof(ex), "e%s%02d", exp < 0 ? "-" : "+", exp < 0 ? -exp : exp);
        uc_bputs(&b, ex);
    }
    return uc_btake(&b);
}

static char *uc_repr_string(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    uc_buf b; uc_binit(&b);
    uc_bputc(&b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { uc_bputc(&b, '\\'); uc_bputc(&b, (char)c); }
        else if (c == '\n') uc_bputs(&b, "\\n");
        else if (c == '\r') uc_bputs(&b, "\\r");
        else if (c == '\t') uc_bputs(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char x[8]; snprintf(x, sizeof(x), "\\x%02x", c); uc_bputs(&b, x); }
        else uc_bputc(&b, (char)c);
    }
    uc_bputc(&b, q);
    return uc_btake(&b);
}

static char *uc_pyrepr(const json_t *v);
static char *uc_pystr(const json_t *v) {
    if (uc_is_null(v)) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) {
                if (d == 0.0) return strdup("0");
                return uc_xsf("%.0f", d);
            }
            return uc_float_repr(d);
        }
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: return uc_pyrepr(v);
    }
}
static char *uc_pyrepr(const json_t *v) {
    if (uc_is_null(v)) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) {
                if (d == 0.0) return strdup("0");
                return uc_xsf("%.0f", d);
            }
            return uc_float_repr(d);
        }
        case JSON_STRING: return uc_repr_string(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            uc_buf b; uc_binit(&b);
            uc_bputc(&b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) uc_bputs(&b, ", ");
                char *r = uc_pyrepr(v->c.items[i]);
                uc_bputs(&b, r); free(r);
            }
            uc_bputc(&b, ']');
            return uc_btake(&b);
        }
        case JSON_OBJECT: {
            uc_buf b; uc_binit(&b);
            uc_bputc(&b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) uc_bputs(&b, ", ");
                char *k = uc_repr_string(v->c.keys[i]);
                char *r = uc_pyrepr(v->c.items[i]);
                uc_bputs(&b, k); uc_bputs(&b, ": "); uc_bputs(&b, r);
                free(k); free(r);
            }
            uc_bputc(&b, '}');
            return uc_btake(&b);
        }
        default: return strdup("None");
    }
}

/* ── ASCII string helpers ── */

static bool uc_is_ws(unsigned char c) {
    return c == ' ' || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *uc_strip(const char *s) {
    while (*s && uc_is_ws((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && uc_is_ws((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s);
    char *r = malloc(n + 1);
    memcpy(r, s, n); r[n] = '\0';
    return r;
}
static bool uc_startswith(const char *s, const char *pre) {
    return strncmp(s, pre, strlen(pre)) == 0;
}
/* first n bytes (ASCII domain: bytes == chars) */
static char *uc_slice(const char *s, size_t n) {
    size_t l = strlen(s);
    if (n > l) n = l;
    char *r = malloc(n + 1);
    memcpy(r, s, n); r[n] = '\0';
    return r;
}
/* last line of str.splitlines() after strip; NULL when the result is empty.
 * Boundaries: \n \r \r\n \v \f \x1c \x1d \x1e (ASCII domain). After strip the
 * trailing boundaries are gone, so the last line is everything after the LAST
 * boundary char (a \r\n pair's \n is the later char). */
static char *uc_last_line(const char *s) {
    char *st = uc_strip(s);
    size_t l = strlen(st);
    if (l == 0) { free(st); return NULL; }
    size_t i = l;
    while (i > 0) {
        unsigned char c = (unsigned char)st[i - 1];
        if (c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
            c == 0x1c || c == 0x1d || c == 0x1e) {
            char *r = malloc(l - i + 1);
            memcpy(r, st + i, l - i); r[l - i] = '\0';
            free(st);
            return r;
        }
        i--;
    }
    /* no boundary: the whole stripped string is the single line */
    return st;
}

/* ── shared seam helpers ── */

/* print(text) -> null (or a scripted raise). Caller frees. */
static json_t *uc_print(const uc_seams_t *sp, const char *text) {
    json_t *a = json_object();
    uc_aset_s(a, "text", text ? text : "");
    json_t *r = uc_call(sp, "print", a);
    json_free(a);
    return r;
}
/* _record_stop(reason, without_receipt=None). Caller frees / propagates. */
static json_t *uc_record_stop(const uc_seams_t *sp, const char *reason, const char *without_receipt) {
    json_t *a = json_object();
    uc_aset_s(a, "reason", reason);
    uc_aset_s(a, "without_receipt", without_receipt);
    json_t *r = uc_call(sp, "record_stop", a);
    json_free(a);
    return r;
}
/* _m().PROJECT_ROOT -> str node (or raise). Caller frees. */
static json_t *uc_project_root(const uc_seams_t *sp) {
    return uc_call0(sp, "project_root");
}

/* read a str field off a result record; absent/null -> NULL. A non-str
 * present value is returned as-is for the caller's type gate. */
static const json_t *uc_field(const json_t *rec, const char *key) {
    return uc_is_obj(rec) ? json_obj_get(rec, key) : NULL;
}

/* .stdout.strip() with the live or-collapse / type gates.
 * `or_empty`: apply `stdout or ""` before strip (resolved_commit/landed).
 * Returns a malloc'd string, or a raise descriptor (caller frees either). */
static json_t *uc_stdout_strip(const json_t *result, bool or_empty) {
    const json_t *so = uc_field(result, "stdout");
    if (!so) {
        /* missing field on the attribute double */
        return uc_attr_raise("types.SimpleNamespace", "stdout");
    }
    if (or_empty && !uc_jtruth(so)) {
        return json_string("");
    }
    if (!uc_is_str(so)) {
        return uc_attr_raise(uc_typename(so), "strip");
    }
    char *s = uc_strip(so->str_val ? so->str_val : "");
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* ===================================================================
 * retired shims  (update_cmd.py:126 / 136 / 141)
 * =================================================================== */

/* PoP: uc_get_default_hermes_root @ hermes_cli/update_cmd.py:get_default_hermes_root */
static json_t *uc_get_default_hermes_root(const uc_seams_t *sp) {
    json_t *a = json_object();
    uc_aset_s(a, "hook", "hermes_cli.update_cmd.get_default_hermes_root");
    json_t *r = uc_call(sp, "stop_historical_updater", a);
    json_free(a);
    return r;  /* NoReturn live: a scripted raise propagates; null otherwise */
}

/* PoP: uc_ensure_venv_pip @ hermes_cli/update_cmd.py:_ensure_venv_pip */
static json_t *uc_ensure_venv_pip(const uc_seams_t *sp) {
    json_t *a = json_object();
    uc_aset_s(a, "hook", "hermes_cli.update_cmd._ensure_venv_pip");
    json_t *r = uc_call(sp, "stop_historical_updater", a);
    json_free(a);
    return r;
}

/* PoP: uc_pip_install_prefix @ hermes_cli/update_cmd.py:_pip_install_prefix */
static json_t *uc_pip_install_prefix(const uc_seams_t *sp) {
    json_t *a = json_object();
    uc_aset_s(a, "hook", "hermes_cli.update_cmd._pip_install_prefix");
    json_t *r = uc_call(sp, "stop_historical_updater", a);
    json_free(a);
    return r;
}

/* ===================================================================
 * _updates_config  (update_cmd.py:173)
 * =================================================================== */
/* PoP: uc_updates_config @ hermes_cli/update_cmd.py:_updates_config */
static json_t *uc_updates_config(const uc_seams_t *sp) {
    json_t *cfg = uc_call0(sp, "load_config");
    if (uc_is_raise(cfg)) return cfg;
    /* (load_config() or {}) */
    const json_t *base = cfg;
    json_t *empty = NULL;
    if (!uc_jtruth(cfg)) { empty = json_object(); base = empty; }
    if (!uc_is_obj(base)) {
        json_t *e = uc_attr_raise(uc_typename(base), "get");
        json_free(cfg); if (empty) json_free(empty);
        return e;
    }
    const json_t *section = json_obj_get(base, "updates");
    json_t *r;
    if (uc_is_obj(section)) r = json_copy(section);
    else r = json_object();
    json_free(cfg); if (empty) json_free(empty);
    return r;
}

/* ===================================================================
 * _record_update_step  (update_cmd.py:218)  — suppress(Exception)
 * =================================================================== */
/* PoP: uc_record_update_step @ hermes_cli/update_cmd.py:_record_update_step */
static json_t *uc_record_update_step(const uc_seams_t *sp, const char *step, bool ok, const char *detail) {
    json_t *a = json_object();
    uc_aset_s(a, "step", step);
    uc_aset_b(a, "ok", ok);
    uc_aset_s(a, "detail", detail ? detail : "");
    json_t *r = uc_call(sp, "record_step", a);
    json_free(a);
    if (uc_is_raise(r)) {
        if (uc_is_exception(r)) { json_free(r); return NULL; }  /* suppress(Exception) */
        return r;  /* BaseException passthrough */
    }
    json_free(r);
    return NULL;
}

/* ===================================================================
 * _record_update_skip  (update_cmd.py:231)  — suppress(Exception)
 * =================================================================== */
/* PoP: uc_record_update_skip @ hermes_cli/update_cmd.py:_record_update_skip */
static json_t *uc_record_update_skip(const uc_seams_t *sp, const char *step, const char *reason) {
    json_t *a = json_object();
    uc_aset_s(a, "step", step);
    uc_aset_s(a, "reason", reason);
    json_t *r = uc_call(sp, "record_skip", a);
    json_free(a);
    if (uc_is_raise(r)) {
        if (uc_is_exception(r)) { json_free(r); return NULL; }
        return r;
    }
    json_free(r);
    return NULL;
}

/* ===================================================================
 * _record_pre_update_backup_outcome  (update_cmd.py:238)
 * =================================================================== */
/* PoP: uc_record_pre_update_backup_outcome @ hermes_cli/update_cmd.py:_record_pre_update_backup_outcome */
static json_t *uc_record_pre_update_backup_outcome(const uc_seams_t *sp, const json_t *args, const json_t *snapshot_id) {
    if (uc_jtruth(snapshot_id)) {
        char *sid = uc_pystr(snapshot_id);
        char *detail = uc_xsf("snapshot=%s", sid);
        free(sid);
        json_t *r = uc_record_update_step(sp, "pre_update_backup", true, detail);
        free(detail);
        return r;  /* null or a BaseException raise */
    }
    json_t *a = json_object();
    uc_aset_j(a, "args", args);
    json_t *mode = uc_call(sp, "resolve_pre_update_backup_mode", a);
    json_free(a);
    if (uc_is_raise(mode)) return mode;
    const char *ms = uc_jstr(mode);
    bool off = ms && !strcmp(ms, "off");
    json_free(mode);
    if (off) {
        bool no_backup = uc_jtruth(uc_field(args, "no_backup"));
        const char *reason = no_backup
            ? "disabled by --no-backup"
            : "disabled by updates.pre_update_backup (mode: off)";
        return uc_record_update_skip(sp, "pre_update_backup", reason);
    }
    return uc_record_update_step(sp, "pre_update_backup", false, "no snapshot captured");
}

/* ===================================================================
 * _record_snapshot_stage  (update_cmd.py:259)
 * =================================================================== */
/* PoP: uc_record_snapshot_stage @ hermes_cli/update_cmd.py:_record_snapshot_stage */
static json_t *uc_record_snapshot_stage(const uc_seams_t *sp, const json_t *args, const json_t *snapshot_id) {
    bool skipped = false;
    if (!uc_jtruth(snapshot_id)) {
        json_t *a = json_object();
        uc_aset_j(a, "args", args);
        json_t *mode = uc_call(sp, "resolve_pre_update_backup_mode", a);
        json_free(a);
        if (uc_is_raise(mode)) return mode;
        const char *ms = uc_jstr(mode);
        skipped = ms && !strcmp(ms, "off");
        json_free(mode);
    }
    const char *status = uc_jtruth(snapshot_id) ? "success" : (skipped ? "skipped" : "failed");
    json_t *a = json_object();
    uc_aset_s(a, "stage", "snapshot");
    uc_aset_s(a, "status", status);
    json_t *r = uc_call(sp, "record_stage", a);
    json_free(a);
    if (uc_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ===================================================================
 * _finalize_receipt  (update_cmd.py:1696)  — _best_effort CM
 * =================================================================== */
/* PoP: uc_finalize_receipt @ hermes_cli/update_cmd.py:_finalize_receipt */
static json_t *uc_finalize_receipt(const uc_seams_t *sp, const char *status, const char *debug_message) {
    (void)debug_message;  /* _best_effort's logger.debug is silent */
    json_t *a = json_object();
    uc_aset_s(a, "status", status);
    json_t *r = uc_call(sp, "finalize_update_receipt", a);
    json_free(a);
    if (uc_is_raise(r)) {
        if (uc_is_exception(r)) { json_free(r); return NULL; }  /* _best_effort swallows Exception */
        return r;
    }
    json_free(r);
    return NULL;
}

/* ===================================================================
 * _git_run  (update_cmd.py:265)
 * =================================================================== */
/* PoP: uc_git_run @ hermes_cli/update_cmd.py:_git_run */
static json_t *uc_git_run(const uc_seams_t *sp, const json_t *git_cmd, const json_t *args,
                          const json_t *cwd, bool check, bool network) {
    json_t *spawn = json_object();
    if (network) {
        uc_aset_n(spawn, "timeout", UC_NETWORK_GIT_TIMEOUT_SECONDS);
        json_t *np = uc_call0(sp, "no_prompt_git_kwargs");
        if (uc_is_raise(np)) { json_free(spawn); return np; }
        if (uc_is_obj(np)) {
            for (size_t i = 0; i < np->c.count; i++)
                json_set(spawn, np->c.keys[i], json_copy(np->c.items[i]));
        }
        json_free(np);
    }
    /* spawn_kwargs.setdefault("creationflags", windows_hide_flags()) — the
     * default argument is evaluated ALWAYS. */
    json_t *hf = uc_call0(sp, "windows_hide_flags");
    if (uc_is_raise(hf)) { json_free(spawn); return hf; }
    if (!json_obj_get(spawn, "creationflags")) {
        json_set(spawn, "creationflags", hf ? json_copy(hf) : json_null());
    }
    if (hf) json_free(hf);
    /* cwd = _m().PROJECT_ROOT if cwd is None else cwd */
    json_t *cwd_node;
    if (uc_is_null(cwd)) {
        cwd_node = uc_project_root(sp);
        if (uc_is_raise(cwd_node)) { json_free(spawn); return cwd_node; }
    } else {
        cwd_node = json_copy(cwd);
    }
    json_t *a = json_object();
    uc_aset_j(a, "git_cmd", git_cmd);
    uc_aset_j(a, "args", args);
    uc_aset_j(a, "cwd", cwd_node);
    uc_aset_b(a, "capture_output", true);
    uc_aset_b(a, "text", true);
    uc_aset_s(a, "encoding", "utf-8");
    uc_aset_s(a, "errors", "replace");
    uc_aset_b(a, "check", check);
    for (size_t i = 0; i < spawn->c.count; i++)
        json_set(a, spawn->c.keys[i], json_copy(spawn->c.items[i]));
    json_free(spawn);
    json_free(cwd_node);
    json_t *r = uc_call(sp, "run_git", a);
    json_free(a);
    if (uc_is_raise(r)) {
        if (uc_caught(r, "TimeoutExpired")) {
            const json_t *cmd = json_obj_get(r, "cmd");
            const json_t *arg0 = uc_is_arr(args) ? json_get(args, 0) : NULL;
            if (!arg0) {
                json_free(r);
                return uc_raise_spec("IndexError", "list index out of range");
            }
            char *a0 = uc_pystr(arg0);
            char *stderr_msg = uc_xsf(
                "git %s timed out after %ds (a stalled remote, or a transfer too large for the limit)",
                a0, UC_NETWORK_GIT_TIMEOUT_SECONDS);
            free(a0);
            if (check) {
                char *cmdstr = uc_pystr(cmd);
                char *cpe_msg = uc_xsf("Command '%s' returned non-zero exit status %d.", cmdstr, 124);
                free(cmdstr);
                json_t *e = uc_raise_spec("CalledProcessError", cpe_msg);
                free(cpe_msg); free(stderr_msg); json_free(r);
                return e;
            }
            json_t *result = json_object();
            uc_aset_j(result, "args", cmd);
            uc_aset_n(result, "returncode", 124);
            uc_aset_s(result, "stdout", "");
            uc_aset_s(result, "stderr", stderr_msg);
            free(stderr_msg); json_free(r);
            return result;
        }
        return r;  /* other raise propagates */
    }
    return r;  /* success result verbatim */
}

/* ===================================================================
 * _base_git_cmd  (update_cmd.py:674)
 * =================================================================== */
/* PoP: uc_base_git_cmd @ hermes_cli/update_cmd.py:_base_git_cmd */
static json_t *uc_base_git_cmd(const char *platform) {
    json_t *r = json_array();
    json_append(r, json_string("git"));
    if (platform && !strcmp(platform, "win32")) {
        json_append(r, json_string("-c"));
        json_append(r, json_string("windows.appendAtomically=false"));
    }
    return r;
}

/* ===================================================================
 * _is_shallow_checkout  (update_cmd.py:682)
 * =================================================================== */
/* PoP: uc_is_shallow_checkout @ hermes_cli/update_cmd.py:_is_shallow_checkout */
static json_t *uc_is_shallow_checkout(const uc_seams_t *sp, const json_t *git_cmd) {
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("--is-shallow-repository"));
    json_t *r = uc_git_run(sp, git_cmd, args, NULL, false, false);
    json_free(args);
    if (uc_is_raise(r)) return r;
    json_t *s = uc_stdout_strip(r, false);
    json_free(r);
    if (uc_is_raise(s)) return s;
    const char *sv = uc_jstr(s);
    json_t *out = json_bool(sv && !strcmp(sv, "true"));
    json_free(s);
    return out;
}

/* ===================================================================
 * _tip_shas  (update_cmd.py:686)
 * =================================================================== */
/* PoP: uc_tip_shas @ hermes_cli/update_cmd.py:_tip_shas */
static json_t *uc_tip_shas(const uc_seams_t *sp, const json_t *git_cmd, const char *target_ref, const char *base) {
    const char *refs[2] = { base, target_ref };
    json_t *out = json_array();
    for (int i = 0; i < 2; i++) {
        json_t *args = json_array();
        json_append(args, json_string("rev-parse"));
        json_append(args, json_string(refs[i]));
        json_t *r = uc_git_run(sp, git_cmd, args, NULL, false, false);
        json_free(args);
        if (uc_is_raise(r)) { json_free(out); return r; }
        json_t *s = uc_stdout_strip(r, false);
        json_free(r);
        if (uc_is_raise(s)) { json_free(out); return s; }
        json_append(out, s);
    }
    return out;
}

/* ===================================================================
 * _resolved_commit  (update_cmd.py:1012)
 * =================================================================== */
/* PoP: uc_resolved_commit @ hermes_cli/update_cmd.py:_resolved_commit */
static json_t *uc_resolved_commit(const uc_seams_t *sp, const json_t *git_cmd, const char *ref) {
    char *spec = uc_xsf("%s^{commit}", ref ? ref : "");
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("-q"));
    json_append(args, json_string("--verify"));
    json_append(args, json_string(spec));
    free(spec);
    json_t *r = uc_git_run(sp, git_cmd, args, NULL, false, false);
    json_free(args);
    if (uc_is_raise(r)) return r;
    json_t *s = uc_stdout_strip(r, true);  /* (stdout or "").strip() */
    json_free(r);
    return s;
}

/* ===================================================================
 * _current_branch_name  (update_cmd.py:1653)
 * =================================================================== */
/* PoP: uc_current_branch_name @ hermes_cli/update_cmd.py:_current_branch_name */
static json_t *uc_current_branch_name(const uc_seams_t *sp, const json_t *git_cmd, bool check) {
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("--abbrev-ref"));
    json_append(args, json_string("HEAD"));
    json_t *r = uc_git_run(sp, git_cmd, args, NULL, check, false);
    json_free(args);
    if (uc_is_raise(r)) return r;
    json_t *s = uc_stdout_strip(r, false);
    json_free(r);
    return s;
}

/* ===================================================================
 * _landed_off_target  (update_cmd.py:974)
 * =================================================================== */
/* PoP: uc_landed_off_target @ hermes_cli/update_cmd.py:_landed_off_target */
static json_t *uc_landed_off_target(const uc_seams_t *sp, const json_t *git_cmd, const char *target_sha, bool merged) {
    json_t *args = json_array();
    json_append(args, json_string("rev-parse"));
    json_append(args, json_string("-q"));
    json_append(args, json_string("--verify"));
    json_append(args, json_string("HEAD"));
    json_t *r = uc_git_run(sp, git_cmd, args, NULL, false, false);
    json_free(args);
    if (uc_is_raise(r)) return r;
    json_t *landed = uc_stdout_strip(r, true);
    json_free(r);
    if (uc_is_raise(landed)) return landed;
    const char *lv = uc_jstr_def(landed, "");
    if (!strcmp(lv, target_sha ? target_sha : "")) {
        json_free(landed);
        return NULL;
    }
    if (merged && lv[0] != '\0') {
        json_t *a2 = json_array();
        json_append(a2, json_string("merge-base"));
        json_append(a2, json_string("--is-ancestor"));
        json_append(a2, json_string(target_sha ? target_sha : ""));
        json_append(a2, json_string(lv));
        json_t *r2 = uc_git_run(sp, git_cmd, a2, NULL, false, false);
        json_free(a2);
        if (uc_is_raise(r2)) { json_free(landed); return r2; }
        const json_t *rc = uc_field(r2, "returncode");
        bool ancestor = rc && rc->type == JSON_NUMBER && rc->num_val == 0.0;
        json_free(r2);
        if (ancestor) { json_free(landed); return NULL; }
    }
    if (lv[0] != '\0') return landed;  /* landed or "an unreadable HEAD" */
    json_free(landed);
    return json_string("an unreadable HEAD");
}

/* ===================================================================
 * _update_movement_baseline  (update_cmd.py:999)
 * =================================================================== */
/* PoP: uc_update_movement_baseline @ hermes_cli/update_cmd.py:_update_movement_baseline */
static json_t *uc_update_movement_baseline(const uc_seams_t *sp, const json_t *git_cmd,
                                           const json_t *pre_pull_sha, const json_t *pre_sync_sha,
                                           const json_t *rollback_branch, const json_t *target_sha) {
    if (!uc_is_null(rollback_branch) && uc_jtruth(pre_pull_sha) && uc_jtruth(target_sha)) {
        json_t *args = json_array();
        json_append(args, json_string("merge-base"));
        json_append(args, json_string("--is-ancestor"));
        json_append(args, json_string(uc_jstr_def(target_sha, "")));
        json_append(args, json_string(uc_jstr_def(pre_pull_sha, "")));
        json_t *r = uc_git_run(sp, git_cmd, args, NULL, false, false);
        json_free(args);
        if (uc_is_raise(r)) return r;
        const json_t *rc = uc_field(r, "returncode");
        bool nonzero = !(rc && rc->type == JSON_NUMBER && rc->num_val == 0.0);
        json_free(r);
        if (nonzero) return pre_pull_sha ? json_copy(pre_pull_sha) : json_null();
    }
    /* return pre_sync_sha or pre_pull_sha */
    if (uc_jtruth(pre_sync_sha)) return json_copy(pre_sync_sha);
    return pre_pull_sha ? json_copy(pre_pull_sha) : json_null();
}

/* ===================================================================
 * _missing_branch_stop  (update_cmd.py:1275)
 * =================================================================== */
/* PoP: uc_missing_branch_stop @ hermes_cli/update_cmd.py:_missing_branch_stop */
static json_t *uc_missing_branch_stop(const uc_seams_t *sp, const json_t *git_cmd, const char *branch) {
    char *ref1 = uc_xsf("refs/heads/%s", branch ? branch : "");
    char *ref2 = uc_xsf("origin/%s", branch ? branch : "");
    const char *refs[2] = { ref1, ref2 };
    json_t *result = NULL;
    for (int i = 0; i < 2; i++) {
        json_t *r = uc_resolved_commit(sp, git_cmd, refs[i]);
        if (uc_is_raise(r)) {
            if (uc_is_exception(r)) { json_free(r); result = json_string("checkout_move_failed"); break; }
            free(ref1); free(ref2);
            return r;  /* BaseException passthrough */
        }
        bool truthy = uc_jtruth(r);
        json_free(r);
        if (truthy) { result = json_string("checkout_move_failed"); break; }
    }
    free(ref1); free(ref2);
    if (!result) result = json_string("branch_missing");
    return result;
}

/* ===================================================================
 * _refuse_unselected_head  (update_cmd.py:984)
 * =================================================================== */
/* PoP: uc_refuse_unselected_head @ hermes_cli/update_cmd.py:_refuse_unselected_head */
static json_t *uc_refuse_unselected_head(const uc_seams_t *sp, const json_t *git_cmd,
                                         const char *target_sha, const json_t *unmoved) {
    json_t *landed = uc_landed_off_target(sp, git_cmd, target_sha, true);
    if (uc_is_raise(landed)) return landed;
    const char *lv = uc_jstr(landed);
    const char *uv = uc_jstr(unmoved);
    /* landed not in (None, unmoved) */
    bool in_set = (lv == NULL) || (uv != NULL && !strcmp(lv, uv));
    if (in_set) { if (landed) json_free(landed); return NULL; }
    char *l10 = uc_slice(lv, 10);
    char *t10 = uc_slice(target_sha ? target_sha : "", 10);
    char *msg = uc_xsf(UC_CROSS " The checkout landed on %s, not the selected commit %s "
                       "(a ref moved during the update).", l10, t10);
    free(l10); free(t10);
    json_t *pr = uc_print(sp, msg);
    free(msg);
    if (uc_is_raise(pr)) { json_free(landed); return pr; }
    json_free(pr);
    json_t *rs = uc_record_stop(sp, "head_moved", NULL);
    if (uc_is_raise(rs)) { json_free(landed); return rs; }
    json_free(rs);
    json_free(landed);
    return uc_raise_spec("SystemExit", "1");
}

/* ===================================================================
 * _verify_head_after_pull  (update_cmd.py:1608)
 * =================================================================== */
/* PoP: uc_verify_head_after_pull @ hermes_cli/update_cmd.py:_verify_head_after_pull */
static json_t *uc_verify_head_after_pull(const uc_seams_t *sp, const json_t *git_cmd, const char *branch,
                                         const json_t *pre_pull_sha, bool in_place_update,
                                         const json_t *resume_token) {
    /* post_pull_sha = _capture_head_sha(git_cmd, _m().PROJECT_ROOT) */
    json_t *root = uc_project_root(sp);
    if (uc_is_raise(root)) return root;
    json_t *a = json_object();
    uc_aset_j(a, "git_cmd", git_cmd);
    uc_aset_j(a, "cwd", root);
    json_free(root);
    json_t *post = uc_call(sp, "capture_head_sha", a);
    json_free(a);
    if (uc_is_raise(post)) return post;
    const char *pre = uc_jstr(pre_pull_sha);
    const char *ps = uc_jstr(post);
    if (pre && pre[0] && ps && !strcmp(ps, pre)) {
        json_t *pr;
        pr = uc_print(sp, ""); if (uc_is_raise(pr)) { json_free(post); return pr; } json_free(pr);
        pr = uc_print(sp, UC_CROSS " Code did not move " UC_EMDASH " update was a no-op.");
        if (uc_is_raise(pr)) { json_free(post); return pr; } json_free(pr);
        char *p10 = uc_slice(pre, 10);
        char *m2 = uc_xsf("  HEAD is pinned to %s (detached checkout); origin/%s advanced but the working tree stayed put.", p10, branch ? branch : "");
        free(p10);
        pr = uc_print(sp, m2); free(m2);
        if (uc_is_raise(pr)) { json_free(post); return pr; } json_free(pr);
        json_t *root2 = uc_project_root(sp);
        if (uc_is_raise(root2)) { json_free(post); return root2; }
        char *m3 = uc_xsf("  Reattach to the branch and retry: git -C %s checkout %s && hermes update", uc_jstr_def(root2, ""), branch ? branch : "");
        json_free(root2);
        pr = uc_print(sp, m3); free(m3);
        if (uc_is_raise(pr)) { json_free(post); return pr; } json_free(pr);
        json_t *d = uc_call0(sp, "disarm_commit_obligations");
        if (uc_is_raise(d)) { json_free(post); return d; } json_free(d);
        json_t *ra = json_object();
        uc_aset_j(ra, "token", resume_token);
        json_t *rr = uc_call(sp, "resume_windows_gateways_after_update", ra);
        json_free(ra);
        if (uc_is_raise(rr)) { json_free(post); return rr; } json_free(rr);
        json_t *rs = uc_record_stop(sp, "detached_head", NULL);
        if (uc_is_raise(rs)) { json_free(post); return rs; } json_free(rs);
        json_free(post);
        return uc_raise_spec("SystemExit", "1");
    }
    /* post_pull_branch = _current_branch_name(git_cmd) */
    json_t *br = uc_current_branch_name(sp, git_cmd, false);
    if (uc_is_raise(br)) { json_free(post); return br; }
    const char *bv = uc_jstr(br);
    bool off_branch = !in_place_update && bv && bv[0] &&
                      strcmp(bv, branch ? branch : "") != 0 && strcmp(bv, "HEAD") != 0;
    if (off_branch) {
        json_t *pr;
        pr = uc_print(sp, ""); if (uc_is_raise(pr)) { json_free(br); json_free(post); return pr; } json_free(pr);
        char *m1 = uc_xsf(UC_CROSS " Update pulled origin/%s, but the checkout is on '%s' " UC_EMDASH " not claiming success.", branch ? branch : "", bv);
        pr = uc_print(sp, m1); free(m1);
        if (uc_is_raise(pr)) { json_free(br); json_free(post); return pr; } json_free(pr);
        json_t *root2 = uc_project_root(sp);
        if (uc_is_raise(root2)) { json_free(br); json_free(post); return root2; }
        char *m2 = uc_xsf("  Switch to the target branch and retry: git -C %s checkout %s && hermes update", uc_jstr_def(root2, ""), branch ? branch : "");
        json_free(root2);
        pr = uc_print(sp, m2); free(m2);
        if (uc_is_raise(pr)) { json_free(br); json_free(post); return pr; } json_free(pr);
        json_t *ra = json_object();
        uc_aset_j(ra, "token", resume_token);
        json_t *rr = uc_call(sp, "resume_windows_gateways_after_update", ra);
        json_free(ra);
        if (uc_is_raise(rr)) { json_free(br); json_free(post); return rr; } json_free(rr);
        json_t *rs = uc_record_stop(sp, "unexpected_branch", NULL);
        if (uc_is_raise(rs)) { json_free(br); json_free(post); return rs; } json_free(rs);
        json_free(br); json_free(post);
        return uc_raise_spec("SystemExit", "1");
    }
    json_free(br);
    return post;  /* return post_pull_sha */
}

/* ===================================================================
 * _map_ssl_cert_file_for_git  (update_cmd.py:180)
 * =================================================================== */
/* PoP: uc_map_ssl_cert_file_for_git @ hermes_cli/update_cmd.py:_map_ssl_cert_file_for_git */
static json_t *uc_map_ssl_cert_file_for_git(const uc_seams_t *sp, const json_t *git_cmd) {
    json_t *a1 = json_object();
    uc_aset_s(a1, "name", "SSL_CERT_FILE");
    json_t *bundle = uc_call(sp, "environ_get", a1);
    json_free(a1);
    if (uc_is_raise(bundle)) return bundle;
    if (!uc_jtruth(bundle)) { json_free(bundle); return NULL; }
    json_t *a2 = json_object();
    uc_aset_s(a2, "name", "GIT_SSL_CAINFO");
    json_t *cainfo = uc_call(sp, "environ_get", a2);
    json_free(a2);
    if (uc_is_raise(cainfo)) { json_free(bundle); return cainfo; }
    if (uc_jtruth(cainfo)) { json_free(cainfo); json_free(bundle); return NULL; }
    json_free(cainfo);
    json_t *args = json_array();
    json_append(args, json_string("config"));
    json_append(args, json_string("--get"));
    json_append(args, json_string("http.sslCAInfo"));
    json_t *configured = uc_git_run(sp, git_cmd, args, NULL, false, false);
    json_free(args);
    if (uc_is_raise(configured)) { json_free(bundle); return configured; }
    const json_t *rc = uc_field(configured, "returncode");
    bool rc0 = rc && rc->type == JSON_NUMBER && rc->num_val == 0.0;
    if (rc0) {
        json_t *s = uc_stdout_strip(configured, false);
        if (uc_is_raise(s)) { json_free(configured); json_free(bundle); return s; }
        bool nonempty = uc_jtruth(s);
        json_free(s);
        if (nonempty) { json_free(configured); json_free(bundle); return NULL; }
    }
    json_free(configured);
    json_t *a3 = json_object();
    uc_aset_s(a3, "name", "GIT_SSL_CAINFO");
    uc_aset_j(a3, "value", bundle);
    json_t *sr = uc_call(sp, "environ_set", a3);
    json_free(a3);
    json_free(bundle);
    if (uc_is_raise(sr)) return sr;
    json_free(sr);
    return NULL;
}

/* ===================================================================
 * _heal_stale_shallow_checkout  (update_cmd.py:297)
 * =================================================================== */
/* str(exc) for the subprocess family / OSError (msg-based classes use msg). */
static char *uc_exc_str(const json_t *r) {
    const char *cls = uc_raise_class(r);
    const json_t *msgn = json_obj_get(r, "msg");
    const char *msg = uc_jstr_def(msgn, "");
    if (cls && !strcmp(cls, "CalledProcessError")) {
        const json_t *cmd = json_obj_get(r, "cmd");
        const json_t *rcn = json_obj_get(r, "returncode");
        int rc = (rcn && rcn->type == JSON_NUMBER) ? (int)rcn->num_val : 1;
        char *cmdstr = uc_pystr(cmd);
        char *s = uc_xsf("Command '%s' returned non-zero exit status %d.", cmdstr, rc);
        free(cmdstr);
        return s;
    }
    if (cls && !strcmp(cls, "TimeoutExpired")) {
        const json_t *cmd = json_obj_get(r, "cmd");
        const json_t *tn = json_obj_get(r, "timeout");
        char *cmdstr = uc_pystr(cmd);
        char *tstr = tn ? uc_pystr(tn) : strdup("300");
        char *s = uc_xsf("Command '%s' timed out after %s seconds", cmdstr, tstr);
        free(cmdstr); free(tstr);
        return s;
    }
    return strdup(msg);
}
/* PoP: uc_heal_stale_shallow_checkout @ hermes_cli/update_cmd.py:_heal_stale_shallow_checkout */
static json_t *uc_heal_stale_shallow_checkout(const uc_seams_t *sp, const char *repo_root, const char *branch) {
    json_t *np = uc_call0(sp, "no_prompt_git_kwargs");
    if (uc_is_raise(np)) return np;
    json_t *a = json_object();
    uc_aset_s(a, "repo_root", repo_root ? repo_root : "");
    uc_aset_s(a, "branch", branch ? branch : "");
    if (uc_is_obj(np)) {
        for (size_t i = 0; i < np->c.count; i++)
            json_set(a, np->c.keys[i], json_copy(np->c.items[i]));
    }
    json_free(np);
    json_t *r = uc_call(sp, "heal_shallow_history", a);
    json_free(a);
    if (uc_is_raise(r)) {
        if (uc_caught(r, "OSError") || uc_caught(r, "SubprocessError")) {
            /* detail = (getattr(exc,"stderr",None) or str(exc)).strip().splitlines()[-1:] or [type] */
            const json_t *stderrn = json_obj_get(r, "stderr");
            char *base;
            if (uc_jtruth(stderrn)) {
                const char *sv = uc_jstr(stderrn);
                base = strdup(sv ? sv : "");
            } else {
                base = uc_exc_str(r);
            }
            char *last = uc_last_line(base);
            free(base);
            const char *cls = uc_raise_class(r);
            char *detail = last ? last : strdup(cls ? cls : "");
            char *msg = uc_xsf("  " UC_WARN " Could not fetch the missing commit history (%s); a very stale install may time out", detail);
            free(detail);
            json_free(r);
            json_t *pr = uc_print(sp, msg);
            free(msg);
            if (uc_is_raise(pr)) return pr;
            json_free(pr);
            return NULL;
        }
        return r;  /* other raise propagates */
    }
    bool healed = uc_jtruth(r);
    json_free(r);
    if (healed) {
        json_t *pr = uc_print(sp, "  " UC_CHECK " Fetched the commit history this shallow checkout was missing");
        if (uc_is_raise(pr)) return pr;
        json_free(pr);
    }
    return NULL;
}

/* ===================================================================
 * _source_update_channel  (update_cmd.py:593)
 * =================================================================== */
/* PoP: uc_source_update_channel @ hermes_cli/update_cmd.py:_source_update_channel */
static json_t *uc_source_update_channel(const uc_seams_t *sp, const json_t *args,
                                        const json_t *channel, bool branch_explicit) {
    const json_t *branchn = uc_field(args, "branch");
    if (branch_explicit || uc_jtruth(branchn)) {
        return json_string("main");
    }
    /* transient = channel if channel is not None else getattr(args,"channel",None) */
    const json_t *transient = uc_is_null(channel) ? uc_field(args, "channel") : channel;
    if (!uc_is_null(transient)) {
        json_t *a = json_object();
        uc_aset_j(a, "name", transient);
        json_t *r = uc_call(sp, "validate_name", a);
        json_free(a);
        return r;
    }
    json_t *cp = uc_call0(sp, "get_config_path");
    if (uc_is_raise(cp)) return cp;
    json_t *a2 = json_object();
    uc_aset_j(a2, "path", cp);
    json_free(cp);
    json_t *config = uc_call(sp, "require_readable_config_before_write", a2);
    json_free(a2);
    if (uc_is_raise(config)) return config;
    json_t *root = uc_project_root(sp);
    if (uc_is_raise(root)) { json_free(config); return root; }
    json_t *a3 = json_object();
    uc_aset_j(a3, "config", config);
    uc_aset_j(a3, "root", root);
    json_free(config); json_free(root);
    json_t *r = uc_call(sp, "resolve_update_channel", a3);
    json_free(a3);
    return r;
}

/* ===================================================================
 * _update_run_channel  (update_cmd.py:1766)
 * =================================================================== */
/* PoP: uc_update_run_channel @ hermes_cli/update_cmd.py:_update_run_channel */
static json_t *uc_update_run_channel(const uc_seams_t *sp, const json_t *args) {
    json_t *r = uc_source_update_channel(sp, args, NULL, false);
    if (uc_is_raise(r)) {
        if (uc_caught(r, "ValueError")) {
            json_t *rs = uc_record_stop(sp, "channel_unresolved", NULL);
            if (uc_is_raise(rs)) { json_free(r); return rs; }
            json_free(rs);
            return r;  /* re-raise */
        }
        return r;
    }
    return r;
}

/* ===================================================================
 * _settle_windows_resume  (update_cmd.py:710)  — mutates request
 * =================================================================== */
/* PoP: uc_settle_windows_resume @ hermes_cli/update_cmd.py:_settle_windows_resume */
static json_t *uc_settle_windows_resume(const uc_seams_t *sp, json_t *request) {
    json_t *a1 = json_object();
    uc_aset_s(a1, "fn", "m._resume_windows_gateways_after_update");
    json_t *r1 = uc_call(sp, "atexit_unregister", a1);
    json_free(a1);
    if (uc_is_raise(r1)) return r1;
    json_free(r1);
    json_t *a2 = json_object();
    uc_aset_s(a2, "fn", "update_cmd_windows._resume_windows_gateways_after_update");
    json_t *r2 = uc_call(sp, "atexit_unregister", a2);
    json_free(a2);
    if (uc_is_raise(r2)) return r2;
    json_free(r2);
    if (!uc_is_obj(request)) {
        char *m = uc_xsf("'%s' object does not support item assignment", uc_typename(request));
        json_t *e = uc_typeerror(m);
        free(m);
        return e;
    }
    json_set(request, "windows_resume_settled", json_bool(true));
    return NULL;
}

/* ===================================================================
 * _resume_paused_gateways_at_exit  (update_cmd.py:726)
 * =================================================================== */
/* PoP: uc_resume_paused_gateways_at_exit @ hermes_cli/update_cmd.py:_resume_paused_gateways_at_exit */
static json_t *uc_resume_paused_gateways_at_exit(const uc_seams_t *sp, const json_t *token, const json_t *request) {
    /* if not token or not token.get("resume_needed") or (request or {}).get("windows_resume_settled"): return */
    if (!uc_jtruth(token)) return NULL;
    if (!uc_is_obj(token)) return uc_attr_raise(uc_typename(token), "get");
    const json_t *rn = json_obj_get(token, "resume_needed");
    if (!uc_jtruth(rn)) return NULL;
    const json_t *reqbase = uc_jtruth(request) ? request : NULL;
    if (reqbase && !uc_is_obj(reqbase)) return uc_attr_raise(uc_typename(reqbase), "get");
    const json_t *settled = reqbase ? json_obj_get(reqbase, "windows_resume_settled") : NULL;
    if (uc_jtruth(settled)) return NULL;
    json_t *a = json_object();
    uc_aset_j(a, "token", token);
    json_t *r = uc_call(sp, "resume_windows_gateways_after_update", a);
    json_free(a);
    if (uc_is_raise(r)) {
        if (uc_is_exception(r)) {
            /* owe_followup(request["receipt"]["update_id"] if request else None, ...) */
            json_t *update_id = NULL;
            if (uc_jtruth(request)) {
                const json_t *receipt = uc_field(request, "receipt");
                if (!receipt) { json_free(r); return uc_keyerror("receipt"); }
                if (!uc_is_obj(receipt)) { json_free(r); return uc_typeerror("'NoneType' object is not subscriptable"); }
                const json_t *uid = json_obj_get(receipt, "update_id");
                if (!uid) { json_free(r); return uc_keyerror("update_id"); }
                update_id = json_copy(uid);
            }
            char *excstr = uc_exc_str(r);
            char *message = uc_xsf("Windows gateway recovery failed: %s", excstr);
            free(excstr);
            json_t *oa = json_object();
            uc_aset_j(oa, "update_id", update_id);
            uc_aset_s(oa, "kind", "windows_resume");
            uc_aset_s(oa, "message", message);
            free(message);
            if (update_id) json_free(update_id);
            json_t *or = uc_call(sp, "owe_followup", oa);
            json_free(oa);
            json_free(r);
            if (uc_is_raise(or)) return or;
            json_free(or);
            return NULL;
        }
        return r;  /* BaseException passthrough */
    }
    json_free(r);
    return NULL;
}

/* ===================================================================
 * _pause_gateways_for_update  (update_cmd.py:1756)
 * =================================================================== */
/* PoP: uc_pause_gateways_for_update @ hermes_cli/update_cmd.py:_pause_gateways_for_update */
static json_t *uc_pause_gateways_for_update(const uc_seams_t *sp, const json_t *opts) {
    json_t *r = uc_call0(sp, "pause_windows_gateways_for_update");
    if (uc_is_raise(r)) {
        if (uc_caught(r, "RuntimeError")) {
            json_t *rs = uc_record_stop(sp, "gateway_pause_failed", NULL);
            if (uc_is_raise(rs)) { json_free(r); return rs; }
            json_free(rs);
            return r;  /* re-raise */
        }
        return r;
    }
    if (uc_jtruth(r)) return r;  /* `or` short-circuit: return the pause token */
    json_free(r);
    const json_t *ngr = uc_field(opts, "no_gateway_restart");
    const json_t *force = uc_field(opts, "force");
    if (!ngr) return uc_attr_raise("types.SimpleNamespace", "no_gateway_restart");
    if (!force) return uc_attr_raise("types.SimpleNamespace", "force");
    json_t *a = json_object();
    uc_aset_b(a, "no_gateway_restart", uc_jtruth(ngr));
    uc_aset_b(a, "force", uc_jtruth(force));
    json_t *ar = uc_call(sp, "arm_pause", a);
    json_free(a);
    if (uc_is_raise(ar)) {
        if (uc_caught(ar, "RuntimeError")) {
            json_t *rs = uc_record_stop(sp, "gateway_pause_failed", NULL);
            if (uc_is_raise(rs)) { json_free(ar); return rs; }
            json_free(rs);
            return ar;
        }
        return ar;
    }
    return ar;
}

/* ===================================================================
 * _upstream_broken_header  (update_cmd.py:995)
 * =================================================================== */
/* PoP: uc_upstream_broken_header @ hermes_cli/update_cmd.py:_upstream_broken_header */
static json_t *uc_upstream_broken_header(const json_t *broken) {
    const json_t *sha = uc_field(broken, "sha");
    if (!sha) {
        if (uc_is_null(broken)) return uc_attr_raise("NoneType", "sha");
        return uc_attr_raise("types.SimpleNamespace", "sha");
    }
    if (!uc_is_str(sha)) return uc_typeerror("'NoneType' object is not subscriptable");
    char *s10 = uc_slice(sha->str_val ? sha->str_val : "", 10);
    char *msg = uc_xsf(UC_CROSS " upstream/main (%s) has a syntax error in a critical file; it was not merged:", s10);
    free(s10);
    json_t *r = json_string(msg);
    free(msg);
    return r;
}