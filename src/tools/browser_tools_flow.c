/* browser_tools_flow.c — FLOW port of the UNCLAIMED browser-tools surface of
 * tools/browser_use_cli.py (quarry: C:/Users/eman5/hermes-agent). The task names
 * the target "tools/browser_tools.py"; that file does not exist in the quarry —
 * the browser tools surface is tools/browser_use_cli.py (browser_exec; the
 * frozen tools/browser_tool.py stub says "browser_exec (tools/browser_use_cli.py)
 * is the browser"). PoP annotations therefore credit the REAL defining module
 * tools/browser_use_cli.py (the scanner only credits the defining module).
 *
 * This port covers the CDP-env / routing / credentials / config cluster plus
 * the pure leaves: _set_cdp_env, _has_cdp_env, _clamp_timeout, _no_code_error,
 * _sidecar_daemon_name, _served_profile_tag, _browser_use_api_key_set,
 * _use_gateway, _harness_credentials, _export_real_profile_note,
 * _mark_local_browser, _quiet, _lazy_call, _real_profile_consented,
 * _code_url_verdict, _attach_vault_supervisor, _record_driven_tab,
 * _published_daemon_pid, _take_real_profile_note, _route_backend, _route_exec,
 * browser_exec_available (22 funcs).
 *
 * The CLAIMED siblings (_backend_cache_key, _base_subprocess_env, _find_cli,
 * _read_browser_cfg, _resolve_backend_cdp, _resolve_real_profile_cdp,
 * is_legacy_browser_use_cloud_config, _find_screenshot, _native_screenshot_result,
 * _workspace_dir, browser_exec, _description_header, _dynamic_schema_overrides)
 * live in browser_use_cli.c / browser_use_cli_gaps.c; they are SEAMED here, never
 * re-ported. Unclaimed I/O leaves this cluster calls (real_profile_note,
 * real_profile_fallback_note, the secret_scope / hermes_constants / url_policy /
 * supervisor / harness_dirs collaborators, importlib) ride the universal seam.
 *
 * Decision-sequence port: ALL collaborator I/O rides the injected universal seam
 * bt_call; the cores perform no I/O. Ported functions call each other REAL
 * (internal composition). The harness scripts results and records the ordered
 * trace the oracle diffs against LIVE tools/browser_use_cli.py under the same
 * scripts.
 *
 * Representation contract (shared with tests/oracle_browser_tools_flow.py):
 *   - env is a mutable dict argument; the harness materializes a working copy
 *     from in.env, the ported code mutates it in place, and out.state echoes it.
 *   - module state travels in ctx->state = {"real_profile_fallback":{key:note},
 *     "real_profile_note_delivered":[keys]}; the delivered set echoes SORTED.
 *   - tuple/NamedTuple -> array (_CodeUrlVerdict -> [blocked, local_sidecar],
 *     _route_exec -> [route_err, sidecar]); None -> JSON null; integral floats
 *     collapse to ints on both sides; bool stays bool.
 *   - uncaught exception -> {"__raise__": cls}; an exception passed as a logger
 *     arg projects to {"__raise__": cls, "msg": str(e)}.
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m}
 *     raises (trace ret is the WHOLE marker); any other value reused.
 *     None-valued seam arg keys dropped on BOTH sides.
 *   - out-state is {} on any propagating raise (both sides); the ordered trace
 *     still captures every seam call up to the raise.
 *   - `except Exception` catches the full Exception family but lets the
 *     BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit) pass
 *     through (full builtin hierarchy in bt_exc_parent).
 *
 * Fixture domain: str() operands ASCII; _clamp_timeout inputs are finite
 * ints/floats/bools/int-strings or TypeError/ValueError forms (NaN/inf excluded —
 * int(inf) raises OverflowError which the live except (TypeError,ValueError)
 * does NOT catch); _published_daemon_pid pid-file texts are strict JSON or
 * clearly-invalid (libjson json_parse is lenient where json.loads is strict);
 * _code_url_verdict code is ASCII. The _URL_RE scanner is transcribed inline and
 * fuzzed against re.findall.
 *
 * libjson-only (gate links lib/libjson/json.o). Self-contained: the harness
 * #includes "tools/browser_tools_flow.c". bt_ prefix (browser_tools).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct bt_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *state;     /* owned: {real_profile_fallback,real_profile_note_delivered} */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} bt_ctx;

static bool bt_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool bt_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool bt_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool bt_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool bt_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool bt_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool bt_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool bt_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *bt_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static double bt_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
/* borrowed object field or NULL */
static json_t *bt_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
/* dict.get(key, default): present-null keeps the null node; absent -> default. */
static json_t *bt_get_def(const json_t *obj, const char *key, json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}

/* ── exception plumbing (FULL builtin hierarchy) ─────────────────────────── */

static bool bt_exc_pending(const bt_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void bt_exc_clear(bt_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void bt_raise(bt_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *bt_exc_class(const bt_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *bt_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        /* BaseException trio: ONLY BaseException (pass through except Exception) */
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"BaseExceptionGroup", "BaseException"},
        /* Exception -> BaseException */
        {"Exception", "BaseException"},
        /* direct Exception subclasses */
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
        {"AttributeError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ImportError", "Exception"},
        {"LookupError", "Exception"}, {"MemoryError", "Exception"},
        {"NameError", "Exception"}, {"OSError", "Exception"},
        {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
        {"SyntaxError", "Exception"}, {"SystemError", "Exception"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"Warning", "Exception"}, {"ExceptionGroup", "Exception"},
        {"SubprocessError", "Exception"},
        /* ArithmeticError family */
        {"FloatingPointError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"},
        /* ImportError family */
        {"ModuleNotFoundError", "ImportError"}, {"ZipImportError", "ImportError"},
        /* LookupError family */
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        /* NameError family */
        {"UnboundLocalError", "NameError"},
        /* OSError family */
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ConnectionError", "OSError"}, {"FileExistsError", "OSError"},
        {"FileNotFoundError", "OSError"}, {"InterruptedError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"PermissionError", "OSError"}, {"ProcessLookupError", "OSError"},
        {"TimeoutError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        /* RuntimeError family */
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        /* SyntaxError family */
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        /* ValueError family */
        {"UnicodeError", "ValueError"}, {"JSONDecodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        /* Warning family */
        {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
        {"RuntimeWarning", "Warning"}, {"SyntaxWarning", "Warning"},
        {"UserWarning", "Warning"}, {"FutureWarning", "Warning"},
        {"ImportWarning", "Warning"}, {"UnicodeWarning", "Warning"},
        {"BytesWarning", "Warning"}, {"ResourceWarning", "Warning"},
        {"EncodingWarning", "Warning"},
        /* subprocess family */
        {"TimeoutExpired", "SubprocessError"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (bt_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool bt_exc_matches(const bt_ctx *ctx, const char *const *classes, size_t n) {
    if (!bt_exc_pending(ctx)) return false;
    const char *cls = bt_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (bt_str_eq(cls, classes[i])) return true;
        const char *p = bt_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool bt_exc_is_exception(const bt_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return bt_exc_matches(ctx, E, 1);
}
/* _published_daemon_pid's narrow except (OSError, ValueError, KeyError,
 * TypeError, IndexError) — NOT the broad Exception family. */
static bool bt_exc_is_pidfile_caught(const bt_ctx *ctx) {
    static const char *E[] = {"OSError", "ValueError", "KeyError", "TypeError", "IndexError"};
    return bt_exc_matches(ctx, E, 5);
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *bt_script_result(bt_ctx *ctx, const json_t *script) {
    if (!script || script->type == JSON_NULL) return NULL;
    if (script->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(script, "__raise__");
        if (raise && raise->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(script);
            return NULL;
        }
    }
    return json_copy(script);
}
/* args ownership TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *bt_call(bt_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *chosen = script;
    if (script && script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                if (ctx->exc) json_free(ctx->exc);
                ctx->exc = json_object();
                json_set(ctx->exc, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof msg, "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = bt_script_result(ctx, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (ctx->exc && chosen == ctx->exc) json_set(ev, "ret", json_copy(ctx->exc));
    else if (ctx->exc && chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))
        json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    return ret;
}

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). */
static void bt_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void bt_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void bt_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}
static void bt_arg_num(json_t *args, const char *key, double val) {
    json_set(args, key, json_number(val));
}
/* logger seam: {"msg","args"}. args_arr ownership transferred. */
static void bt_log(bt_ctx *ctx, const char *level, const char *msg, json_t *args_arr) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = bt_call(ctx, name, a);
    json_free(r);
}
/* project the pending exception as a logger arg node {"__raise__","msg"}. */
static json_t *bt_exc_arg(const bt_ctx *ctx) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(bt_exc_class(ctx)));
    json_set(e, "msg", json_string(json_get_str(ctx->exc, "msg", "")));
    return e;
}
/* tuple result helpers */
static json_t *bt_pair(json_t *a, json_t *b) {
    json_t *r = json_array();
    json_append(r, a ? a : json_null());
    json_append(r, b ? b : json_bool(false));
    return r;
}

/* ── string helpers ──────────────────────────────────────────────────────── */

/* Python str.strip() no-arg White_Space set (codepoints). ASCII + the common
 * Unicode spaces; the fixture domain is ASCII so the ASCII subset suffices, but
 * the full set is transcribed for faithfulness. Returns a malloc'd copy. */
static bool bt_is_ws_cp(unsigned int cp) {
    switch (cp) {
        case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: case 0x20:
        case 0x1C: case 0x1D: case 0x1E: case 0x1F:
        case 0x85: case 0xA0: case 0x1680:
        case 0x2000: case 0x2001: case 0x2002: case 0x2003: case 0x2004:
        case 0x2005: case 0x2006: case 0x2007: case 0x2008: case 0x2009:
        case 0x200A: case 0x2028: case 0x2029: case 0x202F: case 0x205F:
        case 0x3000:
            return true;
        default: return false;
    }
}
/* decode one UTF-8 codepoint at s[i], advance i. Returns the codepoint. */
static unsigned int bt_utf8_next(const char *s, size_t len, size_t *i) {
    unsigned char c = (unsigned char)s[*i];
    unsigned int cp = c;
    size_t extra = 0;
    if (c >= 0xF0) { cp = c & 0x07; extra = 3; }
    else if (c >= 0xE0) { cp = c & 0x0F; extra = 2; }
    else if (c >= 0xC0) { cp = c & 0x1F; extra = 1; }
    (*i)++;
    for (size_t k = 0; k < extra && *i < len; k++) {
        cp = (cp << 6) | ((unsigned char)s[*i] & 0x3F);
        (*i)++;
    }
    return cp;
}
static char *bt_strip(const char *s) {
    if (!s) s = "";
    size_t len = strlen(s);
    size_t a = 0, b = len;
    while (a < b) {
        size_t j = a;
        unsigned int cp = bt_utf8_next(s, len, &j);
        if (!bt_is_ws_cp(cp)) break;
        a = j;
    }
    while (b > a) {
        /* find start of the last codepoint before b */
        size_t j = b - 1;
        while (j > a && ((unsigned char)s[j] & 0xC0) == 0x80) j--;
        size_t k = j;
        unsigned int cp = bt_utf8_next(s, len, &k);
        if (!bt_is_ws_cp(cp)) break;
        b = j;
    }
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
/* str(node) for a scalar: None -> "None", bool -> True/False, number integral
 * str, string verbatim. Containers are outside the str() fixture domain. */
static char *bt_pystr(const json_t *v) {
    if (bt_is_none(v)) return strdup("None");
    if (bt_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (bt_is_str(v)) return strdup(bt_jstr(v));
    if (bt_is_num(v)) {
        char buf[64];
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 9.2e18) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%.17g", d);
        return strdup(buf);
    }
    return strdup("");
}

/* ── _URL_RE scanner (transcribed; fuzzed against re.findall) ────────────── */
/* _URL_RE = re.compile(r"https?://[^\s'\"\\)]+", re.IGNORECASE). findall returns
 * each match: "http://" or "https://" (case-insensitive) followed by >=1 chars
 * that are NOT whitespace, ' " \ ). Greedy. */
static bool bt_url_term(unsigned char c) {
    /* terminator set: \s (ASCII ws + the str.strip set is NOT used here; \s in
     * re matches [ \t\n\r\f\v] plus Unicode ws, but the fixture domain is ASCII)
     * plus ' " \ ) */
    if (c == '\'' || c == '"' || c == '\\' || c == ')') return true;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') return true;
    return false;
}
static bool bt_ci_prefix(const char *s, const char *lit) {
    size_t n = strlen(lit);
    for (size_t i = 0; i < n; i++) {
        if (!s[i]) return false;
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)lit[i])) return false;
    }
    return true;
}
/* append all URL matches to arr (owned strings). */
static void bt_url_findall(const char *code, json_t *arr) {
    if (!code) code = "";
    size_t len = strlen(code);
    size_t i = 0;
    while (i < len) {
        /* try to match https?:// at i (case-insensitive) */
        size_t scheme = 0;
        if (bt_ci_prefix(code + i, "https://")) scheme = 8;
        else if (bt_ci_prefix(code + i, "http://")) scheme = 7;
        if (scheme) {
            size_t j = i + scheme;
            size_t start = j;
            while (j < len && !bt_url_term((unsigned char)code[j])) j++;
            if (j > start) {
                char *m = (char *)malloc(j - i + 1);
                memcpy(m, code + i, j - i);
                m[j - i] = '\0';
                json_append(arr, json_string(m));
                free(m);
                i = j;
                continue;
            }
        }
        i++;
    }
}

/* ── int() transcription for _clamp_timeout / _published_daemon_pid ──────── */
/* Python int(x) for the fixture domain. Returns true on success (value in
 * *out); false on TypeError/ValueError (the caller catches). OverflowError is
 * outside the domain (finite inputs only). */
static bool bt_py_int(const json_t *v, long long *out) {
    if (bt_is_bool(v)) { *out = v->bool_val ? 1 : 0; return true; }
    if (bt_is_num(v)) {
        double d = v->num_val;
        if (!isfinite(d)) return false;  /* OverflowError/ValueError: outside domain */
        *out = (long long)trunc(d);
        return true;
    }
    if (bt_is_str(v)) {
        const char *s = bt_jstr(v);
        /* int() strips a NARROWER ws set than str.strip (no \x1c-\x1f). */
        size_t len = strlen(s);
        size_t a = 0, b = len;
        while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n'
                         || s[a] == '\r' || s[a] == '\f' || s[a] == '\v')) a++;
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n'
                         || s[b - 1] == '\r' || s[b - 1] == '\f' || s[b - 1] == '\v')) b--;
        size_t i = a;
        bool neg = false;
        if (i < b && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); i++; }
        if (i >= b) return false;
        long long val = 0;
        bool prev_digit = false;
        bool any = false;
        for (; i < b; i++) {
            char c = s[i];
            if (c == '_') {
                if (!prev_digit) return false;
                if (i + 1 >= b || !isdigit((unsigned char)s[i + 1])) return false;
                prev_digit = false;
                continue;
            }
            if (!isdigit((unsigned char)c)) return false;
            val = val * 10 + (c - '0');
            prev_digit = true;
            any = true;
        }
        if (!any) return false;
        *out = neg ? -val : val;
        return true;
    }
    /* None / array / object -> TypeError */
    return false;
}

/* ── state accessors ─────────────────────────────────────────────────────── */

static json_t *bt_rpf(bt_ctx *ctx) { return bt_get(ctx->state, "real_profile_fallback"); }
static json_t *bt_rpnd(bt_ctx *ctx) { return bt_get(ctx->state, "real_profile_note_delivered"); }

/* sentinel constants */
#define BT_PRIVATE "_HERMES_BU_PRIVATE_BROWSER"
#define BT_BOT_DESKTOP "_HERMES_BU_BOT_DESKTOP_BROWSER"
#define BT_RP_FALLBACK "_HERMES_BU_REAL_PROFILE_FALLBACK"
#define BT_LOCAL_FALLBACK "_HERMES_BU_LOCAL_FALLBACK"
#define BT_CLOUD "_HERMES_BU_CLOUD_BROWSER"

/* forward declarations (internal composition) */
static json_t *bt_set_cdp_env(bt_ctx *ctx, json_t *env, const char *cdp);
static json_t *bt_has_cdp_env(bt_ctx *ctx, const json_t *env);
static json_t *bt_clamp_timeout(bt_ctx *ctx, const json_t *timeout_s);
static json_t *bt_no_code_error(bt_ctx *ctx, const char *code);
static json_t *bt_sidecar_daemon_name(bt_ctx *ctx, const char *name);
static json_t *bt_served_profile_tag(bt_ctx *ctx);
static json_t *bt_browser_use_api_key_set(bt_ctx *ctx);
static json_t *bt_use_gateway(bt_ctx *ctx, const json_t *browser_cfg);
static json_t *bt_harness_credentials(bt_ctx *ctx);
static json_t *bt_export_real_profile_note(bt_ctx *ctx, json_t *env, const char *key);
static json_t *bt_mark_local_browser(bt_ctx *ctx, json_t *env, const json_t *session, const char *key);
static json_t *bt_quiet(bt_ctx *ctx, const char *fn_seam, json_t *def, const char *log_prefix);
static json_t *bt_lazy_call(bt_ctx *ctx, const char *module, const char *name, json_t *def, const char *log_prefix);
static json_t *bt_real_profile_consented(bt_ctx *ctx);
static json_t *bt_code_url_verdict(bt_ctx *ctx, const char *code);
static json_t *bt_attach_vault_supervisor(bt_ctx *ctx, const json_t *env, const char *task_id);
static json_t *bt_record_driven_tab(bt_ctx *ctx, const json_t *env, const char *task_id);
static json_t *bt_published_daemon_pid(bt_ctx *ctx, const json_t *env);
static json_t *bt_take_real_profile_note(bt_ctx *ctx, const char *route_key);
static json_t *bt_route_backend(bt_ctx *ctx, json_t *env, const char *session, const char *task_id,
                                bool local, bool force_local_sidecar, bool real_profile);
static json_t *bt_route_exec(bt_ctx *ctx, json_t *env, const char *session, const char *task_id,
                             bool local, bool local_sidecar, const char *route_key);
static json_t *bt_browser_exec_available(bt_ctx *ctx);

/* ── 1. _set_cdp_env ─────────────────────────────────────────────────────── */
/* PoP: bt_set_cdp_env @ tools/browser_use_cli.py:_set_cdp_env */
static json_t *bt_set_cdp_env(bt_ctx *ctx, json_t *env, const char *cdp) {
    (void)ctx;
    if (!cdp) cdp = "";
    bool http = (strncmp(cdp, "http://", 7) == 0) || (strncmp(cdp, "https://", 8) == 0);
    if (bt_is_obj(env)) json_set(env, http ? "BU_CDP_URL" : "BU_CDP_WS", json_string(cdp));
    return json_null();
}

/* ── 2. _has_cdp_env ─────────────────────────────────────────────────────── */
/* PoP: bt_has_cdp_env @ tools/browser_use_cli.py:_has_cdp_env */
static json_t *bt_has_cdp_env(bt_ctx *ctx, const json_t *env) {
    (void)ctx;
    json_t *ws = bt_get(env, "BU_CDP_WS");
    json_t *url = bt_get(env, "BU_CDP_URL");
    return json_bool(bt_truthy(ws) || bt_truthy(url));
}

/* ── 3. _clamp_timeout ───────────────────────────────────────────────────── */
/* PoP: bt_clamp_timeout @ tools/browser_use_cli.py:_clamp_timeout */
static json_t *bt_clamp_timeout(bt_ctx *ctx, const json_t *timeout_s) {
    (void)ctx;
    long long v;
    if (!bt_py_int(timeout_s, &v)) return json_number(300);  /* _DEFAULT_TIMEOUT_S */
    if (v < 5) v = 5;        /* _MIN_TIMEOUT_S */
    if (v > 1800) v = 1800;  /* _MAX_TIMEOUT_S */
    return json_number((double)v);
}

/* ── 4. _no_code_error ───────────────────────────────────────────────────── */
/* PoP: bt_no_code_error @ tools/browser_use_cli.py:_no_code_error */
static json_t *bt_no_code_error(bt_ctx *ctx, const char *code) {
    (void)ctx;
    if (!code || !code[0]) {
        return json_string("No code provided. Pass Python that uses the pre-imported helpers, e.g. "
                           "new_tab(\"https://example.com\") then print(page_info()).");
    }
    char *s = bt_strip(code);
    bool empty = (s && s[0] == '\0');
    free(s);
    if (empty) {
        return json_string("No code provided. Pass Python that uses the pre-imported helpers, e.g. "
                           "new_tab(\"https://example.com\") then print(page_info()).");
    }
    return json_null();
}

/* ── 5. _sidecar_daemon_name ─────────────────────────────────────────────── */
/* PoP: bt_sidecar_daemon_name @ tools/browser_use_cli.py:_sidecar_daemon_name */
static json_t *bt_sidecar_daemon_name(bt_ctx *ctx, const char *name) {
    (void)ctx;
    const char *base = (name && name[0]) ? name : "default";
    char buf[128];
    snprintf(buf, sizeof buf, "_local_%s", base);
    /* [:64] codepoint slice; ASCII domain so byte slice == codepoint slice */
    if (strlen(buf) > 64) buf[64] = '\0';
    return json_string(buf);
}

/* ── 6. _served_profile_tag ──────────────────────────────────────────────── */
/* PoP: bt_served_profile_tag @ tools/browser_use_cli.py:_served_profile_tag */
static json_t *bt_served_profile_tag(bt_ctx *ctx) {
    json_t *ov = bt_call(ctx, "get_hermes_home_override", json_object());
    if (bt_exc_pending(ctx)) { json_free(ov); return NULL; }
    bool is_none = bt_is_none(ov);
    json_free(ov);
    if (is_none) return json_string("");
    json_t *hk = bt_call(ctx, "hermes_home_key", json_object());
    if (bt_exc_pending(ctx)) { json_free(hk); return NULL; }
    return hk;
}

/* ── 7. _browser_use_api_key_set ─────────────────────────────────────────── */
/* PoP: bt_browser_use_api_key_set @ tools/browser_use_cli.py:_browser_use_api_key_set */
static json_t *bt_browser_use_api_key_set(bt_ctx *ctx) {
    json_t *a = json_object();
    bt_arg_str(a, "key", "BROWSER_USE_API_KEY");
    bt_arg_str(a, "default", "");
    json_t *r = bt_call(ctx, "get_secret", a);
    if (bt_exc_pending(ctx)) { json_free(r); return NULL; }
    bool set = bt_truthy(r);
    json_free(r);
    return json_bool(set);
}

/* ── 8. _use_gateway ─────────────────────────────────────────────────────── */
/* PoP: bt_use_gateway @ tools/browser_use_cli.py:_use_gateway */
static json_t *bt_use_gateway(bt_ctx *ctx, const json_t *browser_cfg) {
    json_t *ug = bt_get(browser_cfg, "use_gateway");
    json_t *a = json_object();
    bt_arg(a, "value", ug);
    bt_arg_bool(a, "default", false);
    json_t *truth = bt_call(ctx, "is_truthy_value", a);
    if (bt_exc_pending(ctx)) { json_free(truth); return NULL; }
    bool t = bt_truthy(truth);
    json_free(truth);
    if (t) return json_bool(true);
    /* try: from tools.tool_backend_helpers import NOUS_MANAGED_PROVIDER */
    json_t *prov = bt_call(ctx, "nous_managed_provider", json_object());
    if (bt_exc_pending(ctx)) {
        if (bt_exc_is_exception(ctx)) { bt_exc_clear(ctx); json_free(prov); return json_bool(false); }
        json_free(prov); return NULL;
    }
    const char *nous = bt_jstr(prov);
    json_free(prov);
    /* str(browser_cfg.get("cloud_provider") or "").strip().lower() == NOUS... */
    json_t *cp = bt_get(browser_cfg, "cloud_provider");
    char *raw = bt_truthy(cp) ? bt_pystr(cp) : strdup("");
    char *stripped = bt_strip(raw);
    free(raw);
    for (char *p = stripped; *p; p++) *p = (char)tolower((unsigned char)*p);
    bool eq = bt_str_eq(stripped, nous);
    free(stripped);
    return json_bool(eq);
}

/* ── 9. _harness_credentials ─────────────────────────────────────────────── */
/* PoP: bt_harness_credentials @ tools/browser_use_cli.py:_harness_credentials */
static json_t *bt_harness_credentials(bt_ctx *ctx) {
    json_t *routed_r = bt_call(ctx, "serves_routed_profile", json_object());
    if (bt_exc_pending(ctx)) { json_free(routed_r); return NULL; }
    bool routed = bt_truthy(routed_r);
    json_free(routed_r);
    json_t *scope = NULL;
    if (routed) {
        json_t *cs = bt_call(ctx, "current_secret_scope", json_object());
        if (bt_exc_pending(ctx)) { json_free(cs); return NULL; }
        /* (current_secret_scope() or {}) */
        scope = bt_truthy(cs) ? cs : json_object();
        if (scope != cs) json_free(cs);
    }
    /* values = {key: (scope.get(key) if routed else get_secret(key)) for key in KEYS} */
    static const char *KEYS[] = {"BROWSER_USE_API_KEY"};
    json_t *values = json_object();
    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        json_t *val = NULL;
        if (routed) {
            val = bt_get_def(scope, KEYS[i], NULL);  /* scope.get(key) -> None if absent */
            val = val ? json_copy(val) : json_null();
        } else {
            json_t *a = json_object();
            bt_arg_str(a, "key", KEYS[i]);
            val = bt_call(ctx, "get_secret", a);
            if (bt_exc_pending(ctx)) { json_free(val); json_free(values); json_free(scope); return NULL; }
            if (!val) val = json_null();
        }
        json_set(values, KEYS[i], val);
    }
    json_free(scope);
    /* return {key: value for ... if value is not None} */
    json_t *out = json_object();
    for (size_t i = 0; i < values->c.count; i++) {
        json_t *v = values->c.items[i];
        if (!bt_is_none(v)) json_set(out, values->c.keys[i], json_copy(v));
    }
    json_free(values);
    return out;
}

/* ── 10. _export_real_profile_note ───────────────────────────────────────── */
/* PoP: bt_export_real_profile_note @ tools/browser_use_cli.py:_export_real_profile_note */
static json_t *bt_export_real_profile_note(bt_ctx *ctx, json_t *env, const char *key) {
    json_t *a = json_object();
    bt_arg_str(a, "key", key ? key : "");
    json_t *note = bt_call(ctx, "real_profile_note", a);
    if (bt_exc_pending(ctx)) { json_free(note); return NULL; }
    if (bt_truthy(note) && bt_is_obj(env)) {
        if (!json_has(env, BT_RP_FALLBACK)) json_set(env, BT_RP_FALLBACK, json_copy(note));
    }
    json_free(note);
    return json_null();
}

/* ── 11. _mark_local_browser ─────────────────────────────────────────────── */
/* PoP: bt_mark_local_browser @ tools/browser_use_cli.py:_mark_local_browser */
static json_t *bt_mark_local_browser(bt_ctx *ctx, json_t *env, const json_t *session, const char *key) {
    json_t *features = bt_get(session, "features");
    if (!bt_truthy(features)) features = NULL;  /* session.get("features") or {} */
    json_t *lp = bt_get(features, "lightpanda");
    const char *engine = bt_truthy(lp) ? "lightpanda" : "local";
    if (bt_is_obj(env)) json_set(env, BT_BOT_DESKTOP, json_string(engine));
    json_t *shared = bt_get(session, "shared_profile");
    json_t *rp = bt_get(features, "real_profile");
    if (!bt_truthy(shared) && !bt_truthy(rp)) {
        if (bt_is_obj(env)) json_set(env, BT_PRIVATE, json_string("1"));
    }
    json_t *r = bt_export_real_profile_note(ctx, env, key);
    if (bt_exc_pending(ctx)) { json_free(r); return NULL; }
    json_free(r);
    return json_null();
}

/* ── 12. _quiet ──────────────────────────────────────────────────────────── */
/* fn() is a no-arg seam (fn_seam). except Exception -> debug-log + default;
 * BaseException trio propagates. */
/* PoP: bt_quiet @ tools/browser_use_cli.py:_quiet */
static json_t *bt_quiet(bt_ctx *ctx, const char *fn_seam, json_t *def, const char *log_prefix) {
    json_t *r = bt_call(ctx, fn_seam, json_object());
    if (bt_exc_pending(ctx)) {
        if (bt_exc_is_exception(ctx)) {
            json_t *earg = bt_exc_arg(ctx);
            bt_exc_clear(ctx);
            json_free(r);
            if (log_prefix && log_prefix[0]) {
                json_t *la = json_array();
                json_append(la, json_string(log_prefix));
                json_append(la, earg);
                bt_log(ctx, "debug", "%s: %s", la);
            } else {
                json_free(earg);
            }
            return def ? json_copy(def) : json_null();
        }
        json_free(r);
        return NULL;  /* BaseException propagates */
    }
    return r ? r : json_null();
}

/* ── 13. _lazy_call ──────────────────────────────────────────────────────── */
/* _quiet(lambda: getattr(importlib.import_module(module), name)(), default, lp).
 * The lambda fires import_module then getattr(mod, name)(). A None import_module
 * result makes getattr raise AttributeError("'NoneType' object has no attribute
 * '<name>'") BEFORE the name call seam fires; _quiet's except Exception catches
 * it (debug-log + default). The BaseException trio propagates. */
static json_t *bt_lazy_catch(bt_ctx *ctx, json_t *def, const char *log_prefix) {
    if (bt_exc_is_exception(ctx)) {
        json_t *earg = bt_exc_arg(ctx);
        bt_exc_clear(ctx);
        if (log_prefix && log_prefix[0]) {
            json_t *la = json_array();
            json_append(la, json_string(log_prefix));
            json_append(la, earg);
            bt_log(ctx, "debug", "%s: %s", la);
        } else {
            json_free(earg);
        }
        return def ? json_copy(def) : json_null();
    }
    return NULL;  /* BaseException propagates */
}
/* PoP: bt_lazy_call @ tools/browser_use_cli.py:_lazy_call */
static json_t *bt_lazy_call(bt_ctx *ctx, const char *module, const char *name, json_t *def, const char *log_prefix) {
    json_t *a = json_object();
    bt_arg_str(a, "module", module ? module : "");
    json_t *mod = bt_call(ctx, "import_module", a);
    if (bt_exc_pending(ctx)) { json_free(mod); return bt_lazy_catch(ctx, def, log_prefix); }
    if (bt_is_none(mod)) {
        /* getattr(None, name) -> AttributeError, caught by _quiet */
        json_free(mod);
        char msg[256];
        snprintf(msg, sizeof msg, "'NoneType' object has no attribute '%s'", name ? name : "");
        bt_raise(ctx, "AttributeError", msg);
        return bt_lazy_catch(ctx, def, log_prefix);
    }
    json_free(mod);
    /* getattr(module, name)() -> the `name` seam (no args) */
    json_t *r = bt_call(ctx, name, json_object());
    if (bt_exc_pending(ctx)) { json_free(r); return bt_lazy_catch(ctx, def, log_prefix); }
    return r ? r : json_null();
}

/* ── 14. _real_profile_consented ─────────────────────────────────────────── */
/* PoP: bt_real_profile_consented @ tools/browser_use_cli.py:_real_profile_consented */
static json_t *bt_real_profile_consented(bt_ctx *ctx) {
    json_t *def = json_bool(false);
    json_t *r = bt_lazy_call(ctx, "tools.browser_session_cloud", "use_real_profile", def,
                             "real-profile consent lookup failed");
    json_free(def);
    return r;
}

/* ── 15. _code_url_verdict ───────────────────────────────────────────────── */
/* PoP: bt_code_url_verdict @ tools/browser_use_cli.py:_code_url_verdict */
static json_t *bt_code_url_verdict(bt_ctx *ctx, const char *code) {
    json_t *urls = json_array();
    bt_url_findall(code ? code : "", urls);
    bool local_sidecar = false;
    json_t *blocked = NULL;
    for (size_t i = 0; i < urls->c.count; i++) {
        const char *url = bt_jstr(urls->c.items[i]);
        json_t *a1 = json_object();
        bt_arg_str(a1, "url", url);
        json_t *rl = bt_call(ctx, "hybrid_routes_locally", a1);
        if (bt_exc_pending(ctx)) { json_free(rl); json_free(urls); json_free(blocked); return NULL; }
        bool routes_local = bt_truthy(rl);
        json_free(rl);
        local_sidecar = local_sidecar || routes_local;
        json_t *a2 = json_object();
        bt_arg_str(a2, "url", url);
        bt_arg_bool(a2, "auto_local", routes_local);
        json_t *err = bt_call(ctx, "evaluate_url_safety", a2);
        if (bt_exc_pending(ctx)) { json_free(err); json_free(urls); json_free(blocked); return NULL; }
        if (bt_truthy(err)) {
            /* err.get("error", "Blocked: unsafe URL"): present-null keeps null,
             * absent -> the default; a present non-str value passes through. */
            json_t *e = bt_get(err, "error");
            blocked = e ? json_copy(e) : json_string("Blocked: unsafe URL");
            json_free(err);
            break;
        }
        json_free(err);
    }
    json_free(urls);
    json_t *r = json_array();
    json_append(r, blocked ? blocked : json_null());
    json_append(r, json_bool(local_sidecar));
    return r;
}

/* ── 16. _attach_vault_supervisor ────────────────────────────────────────── */
/* PoP: bt_attach_vault_supervisor @ tools/browser_use_cli.py:_attach_vault_supervisor */
static json_t *bt_attach_vault_supervisor(bt_ctx *ctx, const json_t *env, const char *task_id) {
    json_t *ws = bt_get(env, "BU_CDP_WS");
    json_t *url = bt_get(env, "BU_CDP_URL");
    json_t *cdp = bt_truthy(ws) ? ws : url;
    if (!bt_truthy(cdp)) return json_null();
    const char *cdp_s = bt_jstr(cdp);
    /* try: policy, timeout_s = get_dialog_policy_config(); REGISTRY.get_or_start(...) */
    json_t *pol = bt_call(ctx, "get_dialog_policy_config", json_object());
    if (bt_exc_pending(ctx)) goto fail;
    /* pol is a tuple [policy, timeout_s] */
    json_t *policy = bt_is_arr(pol) ? json_get(pol, 0) : NULL;
    json_t *timeout_s = bt_is_arr(pol) ? json_get(pol, 1) : NULL;
    json_t *ra = json_object();
    bt_arg_str(ra, "cdp", cdp_s);
    json_t *resolved = bt_call(ctx, "resolve_cdp_override", ra);
    if (bt_exc_pending(ctx)) { json_free(pol); goto fail; }
    json_t *ga = json_object();
    bt_arg_str(ga, "task_id", (task_id && task_id[0]) ? task_id : "default");
    bt_arg(ga, "cdp_url", resolved);
    bt_arg(ga, "dialog_policy", policy);
    bt_arg(ga, "dialog_timeout_s", timeout_s);
    json_t *gr = bt_call(ctx, "SUPERVISOR_REGISTRY.get_or_start", ga);
    if (bt_exc_pending(ctx)) { json_free(pol); json_free(resolved); json_free(gr); goto fail; }
    json_free(pol); json_free(resolved); json_free(gr);
    return json_null();
fail:
    if (bt_exc_is_exception(ctx)) {
        json_t *earg = bt_exc_arg(ctx);
        bt_exc_clear(ctx);
        json_t *la = json_array();
        json_append(la, earg);
        bt_log(ctx, "debug", "browser_exec: CDP supervisor attach failed (non-fatal): %s", la);
        return json_null();
    }
    return NULL;  /* BaseException propagates */
}

/* ── 17. _record_driven_tab ──────────────────────────────────────────────── */
/* PoP: bt_record_driven_tab @ tools/browser_use_cli.py:_record_driven_tab */
static json_t *bt_record_driven_tab(bt_ctx *ctx, const json_t *env, const char *task_id) {
    json_t *ws = bt_get(env, "BU_CDP_WS");
    json_t *url = bt_get(env, "BU_CDP_URL");
    if (!bt_truthy(ws) && !bt_truthy(url)) return json_null();
    json_t *ga = json_object();
    bt_arg_str(ga, "task_id", (task_id && task_id[0]) ? task_id : "default");
    json_t *sup = bt_call(ctx, "SUPERVISOR_REGISTRY.get", ga);
    if (bt_exc_pending(ctx)) { json_free(sup); return NULL; }
    if (!bt_is_none(sup)) {
        json_t *ha = json_object();
        bt_arg(ha, "env", env);
        json_t *target = bt_call(ctx, "harness_current_target", ha);
        if (bt_exc_pending(ctx)) { json_free(sup); json_free(target); return NULL; }
        json_t *sa = json_object();
        bt_arg(sa, "target", target);
        json_t *sr = bt_call(ctx, "supervisor.set_driven_target", sa);
        if (bt_exc_pending(ctx)) { json_free(sup); json_free(target); json_free(sr); return NULL; }
        json_free(target); json_free(sr);
    }
    json_free(sup);
    return json_null();
}

/* ── 18. _published_daemon_pid ───────────────────────────────────────────── */
/* PoP: bt_published_daemon_pid @ tools/browser_use_cli.py:_published_daemon_pid */
static json_t *bt_published_daemon_pid(bt_ctx *ctx, const json_t *env) {
    json_t *ha = json_object();
    bt_arg(ha, "env", env);
    json_t *pf = bt_call(ctx, "harness_pid_file", ha);
    if (bt_exc_pending(ctx)) {
        if (bt_exc_is_pidfile_caught(ctx)) { bt_exc_clear(ctx); json_free(pf); return json_null(); }
        json_free(pf); return NULL;
    }
    /* harness_pid_file(env).read_text(encoding="utf-8-sig").strip() */
    char *path = bt_pystr(pf);
    json_free(pf);
    json_t *ra = json_object();
    bt_arg_str(ra, "path", path);
    bt_arg_str(ra, "encoding", "utf-8-sig");
    free(path);
    json_t *raw_r = bt_call(ctx, "read_text", ra);
    if (bt_exc_pending(ctx)) {
        if (bt_exc_is_pidfile_caught(ctx)) { bt_exc_clear(ctx); json_free(raw_r); return json_null(); }
        json_free(raw_r); return NULL;
    }
    char *raw = bt_pystr(raw_r);
    json_free(raw_r);
    char *stripped = bt_strip(raw);
    free(raw);
    /* int(json.loads(raw)["pid"]) if raw.startswith("{") else int(raw.split()[0]) */
    long long pid = 0;
    bool ok = false;
    if (stripped[0] == '{') {
        json_t *doc = json_parse(stripped, NULL);
        if (doc) {
            json_t *pidn = bt_get(doc, "pid");
            if (pidn) ok = bt_py_int(pidn, &pid);
            /* missing pid -> KeyError -> caught -> None (ok stays false) */
            json_free(doc);
        }
        /* json_parse NULL -> JSONDecodeError(ValueError) -> caught -> None */
    } else {
        /* raw.split()[0]: first whitespace-delimited token */
        size_t i = 0, len = strlen(stripped);
        while (i < len && (stripped[i] == ' ' || stripped[i] == '\t' || stripped[i] == '\n'
                           || stripped[i] == '\r' || stripped[i] == '\f' || stripped[i] == '\v')) i++;
        size_t start = i;
        while (i < len && !(stripped[i] == ' ' || stripped[i] == '\t' || stripped[i] == '\n'
                            || stripped[i] == '\r' || stripped[i] == '\f' || stripped[i] == '\v')) i++;
        if (i > start) {
            char *tok = (char *)malloc(i - start + 1);
            memcpy(tok, stripped + start, i - start);
            tok[i - start] = '\0';
            json_t *tokn = json_string(tok);
            free(tok);
            ok = bt_py_int(tokn, &pid);
            json_free(tokn);
        }
        /* empty split -> IndexError -> caught -> None */
    }
    free(stripped);
    if (!ok) return json_null();
    return json_number((double)pid);
}

/* ── 19. _take_real_profile_note ─────────────────────────────────────────── */
/* PoP: bt_take_real_profile_note @ tools/browser_use_cli.py:_take_real_profile_note */
static json_t *bt_take_real_profile_note(bt_ctx *ctx, const char *route_key) {
    json_t *rpf = bt_rpf(ctx);
    json_t *rpnd = bt_rpnd(ctx);
    bool in_rpf = json_has(rpf, route_key);
    /* route_key in _real_profile_note_delivered (a set; modeled as sorted array) */
    bool delivered = false;
    if (bt_is_arr(rpnd))
        for (size_t i = 0; i < rpnd->c.count; i++)
            if (bt_str_eq(bt_jstr(rpnd->c.items[i]), route_key)) { delivered = true; break; }
    if (!in_rpf || delivered) return json_object();
    /* _real_profile_note_delivered.add(route_key) */
    if (bt_is_arr(rpnd)) json_append(rpnd, json_string(route_key));
    json_t *note = bt_get(rpf, route_key);
    json_t *out = json_object();
    json_set(out, "real_profile_unavailable", note ? json_copy(note) : json_null());
    return out;
}

/* ── 20. _route_backend ──────────────────────────────────────────────────── */
/* PoP: bt_route_backend @ tools/browser_use_cli.py:_route_backend */
static json_t *bt_route_backend(bt_ctx *ctx, json_t *env, const char *session, const char *task_id,
                                bool local, bool force_local_sidecar, bool real_profile) {
    /* rp_err = _resolve_real_profile_cdp(env, force_local=local) if real_profile else None */
    json_t *rp_err = NULL;
    if (real_profile) {
        json_t *a = json_object();
        bt_arg(a, "env", env);
        bt_arg_bool(a, "force_local", local);
        rp_err = bt_call(ctx, "_resolve_real_profile_cdp", a);
        if (bt_exc_pending(ctx)) { json_free(rp_err); return NULL; }
    }
    /* if local and not _has_cdp_env(env) and not _real_profile_consented(): return msg */
    if (local) {
        json_t *h = bt_has_cdp_env(ctx, env);
        bool has = bt_truthy(h);
        json_free(h);
        if (!has) {
            json_t *c = bt_real_profile_consented(ctx);
            if (bt_exc_pending(ctx)) { json_free(c); json_free(rp_err); return NULL; }
            bool consented = bt_truthy(c);
            json_free(c);
            if (!consented) {
                json_free(rp_err);
                return json_string("local=true was requested but browser.use_real_profile is off. Enable it in config.yaml "
                                   "(browser.use_real_profile: true) or the desktop Settings → Browser section, then retry.");
            }
        }
    }
    /* if local and not _has_cdp_env(env) and (rp_err or not real_profile): */
    if (local) {
        json_t *h = bt_has_cdp_env(ctx, env);
        bool has = bt_truthy(h);
        json_free(h);
        if (!has && (bt_truthy(rp_err) || !real_profile)) {
            if (bt_truthy(rp_err)) {
                json_t *a = json_object();
                bt_arg(a, "err", rp_err);
                json_t *note = bt_call(ctx, "real_profile_fallback_note", a);
                if (bt_exc_pending(ctx)) { json_free(note); json_free(rp_err); return NULL; }
                if (bt_is_obj(env)) json_set(env, BT_RP_FALLBACK, note ? note : json_null());
                else json_free(note);
            }
            if (bt_is_obj(env)) json_set(env, BT_LOCAL_FALLBACK, json_string("1"));
            force_local_sidecar = true;
        }
    }
    json_free(rp_err);
    /* return _resolve_backend_cdp(env, task_id, session_name=session, force_local_sidecar=...) */
    json_t *a = json_object();
    bt_arg(a, "env", env);
    bt_arg_str(a, "task_id", task_id ? task_id : "");
    bt_arg_str(a, "session_name", session ? session : "");
    bt_arg_bool(a, "force_local_sidecar", force_local_sidecar);
    json_t *r = bt_call(ctx, "_resolve_backend_cdp", a);
    if (bt_exc_pending(ctx)) { json_free(r); return NULL; }
    return r ? r : json_null();
}

/* ── 21. _route_exec ─────────────────────────────────────────────────────── */
/* PoP: bt_route_exec @ tools/browser_use_cli.py:_route_exec */
static json_t *bt_route_exec(bt_ctx *ctx, json_t *env, const char *session, const char *task_id,
                             bool local, bool local_sidecar, const char *route_key) {
    json_t *rpf = bt_rpf(ctx);
    bool real_profile = !json_has(rpf, route_key);
    json_t *route_err = bt_route_backend(ctx, env, session, task_id, local, local_sidecar, real_profile);
    if (bt_exc_pending(ctx)) { json_free(route_err); return NULL; }
    /* note = env.pop(_REAL_PROFILE_FALLBACK_SENTINEL, None) */
    json_t *note = bt_get(env, BT_RP_FALLBACK);
    json_t *note_copy = note ? json_copy(note) : NULL;
    if (bt_is_obj(env)) json_obj_del(env, BT_RP_FALLBACK);
    if (bt_truthy(note_copy)) {
        /* _real_profile_fallback.setdefault(route_key, note) */
        if (bt_is_obj(rpf) && !json_has(rpf, route_key))
            json_set(rpf, route_key, json_copy(note_copy));
    }
    /* local_sidecar or bool(env.pop(_LOCAL_FALLBACK_SENTINEL, None)) */
    json_t *lf = bt_get(env, BT_LOCAL_FALLBACK);
    bool lf_bool = bt_truthy(lf);
    if (bt_is_obj(env)) json_obj_del(env, BT_LOCAL_FALLBACK);
    bool sidecar = local_sidecar || lf_bool;
    json_free(note_copy);
    json_t *r = json_array();
    json_append(r, route_err ? route_err : json_null());
    json_append(r, json_bool(sidecar));
    return r;
}

/* ── 22. browser_exec_available ──────────────────────────────────────────── */
/* PoP: bt_browser_exec_available @ tools/browser_use_cli.py:browser_exec_available */
static json_t *bt_browser_exec_available(bt_ctx *ctx) {
    json_t *cli = bt_call(ctx, "_find_cli", json_object());
    if (bt_exc_pending(ctx)) { json_free(cli); return NULL; }
    bool avail = !bt_is_none(cli);
    json_free(cli);
    return json_bool(avail);
}