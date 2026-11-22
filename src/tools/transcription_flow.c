/* transcription_flow.c — FLOW port of the UNCLAIMED transcription surface of
 * the quarry (C:/Users/eman5/hermes-agent). The task names the target
 * "tools/transcription.py"; that file does not exist in the quarry — the
 * transcription surface is the tools/transcription_*.py family. This port
 * covers the three FULLY UNCLAIMED modules of that family (PoP grep census):
 *
 *   tools/transcription_common.py    (7 funcs + STTResponseError)
 *     _error_result, normalize_xai_stt_model, _ok_result, _lazy_ensure_quietly,
 *     _process_error_detail, _log_prompt_unsupported, _config_number
 *   tools/transcription_chunking.py  (8 funcs)
 *     upload_limit, _fits, exceeds_upload_limit, _too_large, _silence_midpoints,
 *     _cut_points, _split, transcribe_oversized
 *   tools/transcription_whisper_cpp.py (4 funcs)
 *     _model_dir, _file_signature, whisper_cpp_command, ensure_whisper_cpp_models
 *
 * 19 funcs. PoP annotations credit the REAL defining modules (the scanner only
 * credits the defining module). The CLAIMED siblings (_transcode_audio_for_stt,
 * _find_ffmpeg_binary, _probe_audio_duration — audio/tools ports) and the
 * unclaimed I/O leaves (_run_quiet, subprocess, os.stat/listdir/getsize/getenv,
 * pm, pm.downloader, tempfile, voice_mode_transcript) ride the universal seam
 * tr_call; they are SEAMED here, never re-ported. Ported functions call each
 * other REAL (internal composition): _too_large/_error_result/_ok_result,
 * exceeds_upload_limit->_fits, transcribe_oversized->{_fits,_silence_midpoints,
 * _cut_points,_split,_too_large,_error_result,_ok_result},
 * ensure_whisper_cpp_models->{_model_dir,_file_signature},
 * whisper_cpp_command->_model_dir.
 *
 * Decision-sequence port: ALL collaborator I/O rides the injected universal
 * seam; the cores perform no I/O. The harness scripts results and records the
 * ordered trace the oracle diffs against the LIVE quarry defs under the same
 * scripts.
 *
 * Representation contract (shared with tests/oracle_transcription_flow.py):
 *   - module state travels in ctx->state = {"verified_files": {path: [digest,
 *     ino, size, mtime_ns, ctime_ns]}} (whisper_cpp _verified_files, Path keys
 *     projected to POSIX strings, tuple values to arrays). out.state echoes it
 *     on success; {} on any propagating raise (both sides).
 *   - tuple results -> arrays (upload_limit [max_bytes, max_seconds],
 *     _file_signature [digest, ino, size, mtime_ns, ctime_ns]); None -> JSON
 *     null; integral floats collapse to ints on both sides; bool stays bool.
 *   - uncaught exception -> {"__raise__": cls}; an exception passed as a logger
 *     arg projects to str(exc) (the oracle logger recorder does the same).
 *   - scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion
 *     raises RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m}
 *     raises (trace ret is the WHOLE marker); any other value (incl. arrays)
 *     reused for every call. None-valued seam arg keys dropped on BOTH sides.
 *   - import gates (in.pm_ok / in.pm_downloader_ok, default true) fire NO seam
 *     event; a gated import raises ModuleNotFoundError("import of <mod> halted;
 *     None in sys.modules") exactly like the oracle's sys.modules[mod]=None.
 *   - `except Exception` catches the full Exception family but lets the
 *     BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit) pass
 *     through (FULL builtin hierarchy in tr_exc_parent, plus the custom
 *     STTResponseError -> ValueError and the subprocess family).
 *   - paths are POSIX: the oracle seeds posixpath for os.path and PurePosixPath
 *     for pathlib, so joins/basenames keep '/' on the Windows gate host too.
 *
 * Fixture domain (documented exclusions):
 *   - str()/repr() operands ASCII; CalledProcessError returncode POSITIVE
 *     (negative rc str() carries a platform-dependent Signals enum repr);
 *     cmd is a str or a list of str.
 *   - limit is a 2-element array (or null/int for the unpack/index TypeError
 *     branches); str/dict limits excluded (they unpack chars/keys).
 *   - numbers in type-name positions are integral-int or NON-integral float
 *     (libjson has no int/float identity; an integral float would name 'int'
 *     where live names 'float').
 *   - format positions (%.1f/%.0f/%.3f) avoid exact binary rounding ties:
 *     byte-size/2^20 values cannot tie at .1f/.3f (non-dyadic denominators),
 *     and .0f fixtures avoid x.5 durations.
 *   - seam attribute carriers (_run_quiet .stderr, stat fields, installed
 *     .binary, Source .dest/.sha256, dispatch/cast results) are PRESENT in the
 *     scripts; absent-attribute raises would name the oracle's double class.
 *   - TemporaryDirectory scripts are non-empty strings (the CM is one seam
 *     event at construction+enter; __exit__ with ignore_cleanup_errors=True
 *     fires no event and never raises).
 *   - silence tokens match (-?[\d.]+) / ([\d.]+); float() of a malformed token
 *     (multiple dots) propagates ValueError exactly like live.
 *   - bytes inputs travel as {"__bytes__": "<hex>"}; decode utf-8 replace is
 *     transcribed byte-for-byte (ONE U+FFFD per error span: the lead byte plus
 *     its valid-continuation prefix, up to the first invalid byte or EOF —
 *     probed on py3.14 over random corpora).
 *
 * libjson-only (gate links lib/libjson/json.o). Self-contained: the harness
 * #includes "tools/transcription_flow.c". tr_ prefix (transcription).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct tr_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *state;     /* owned: {"verified_files": {path: signature array}} */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
    bool pm_ok;            /* import pm gate (in.pm_ok) */
    bool pm_downloader_ok; /* from pm.downloader import ... gate */
} tr_ctx;

static bool tr_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool tr_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool tr_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool tr_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool tr_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool tr_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool tr_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool tr_truthy(const json_t *v) {
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
static const char *tr_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static double tr_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
/* borrowed object field or NULL */
static json_t *tr_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
/* dict.get(key, default): present-null keeps the null node; absent -> default. */
static json_t *tr_get_def(const json_t *obj, const char *key, json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}
/* Python type name for a JSON node (numbers: integral -> int, else float;
 * the fixture domain keeps integral floats out of type-name positions). */
static const char *tr_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == floor(d) && fabs(d) < 9.2e18) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* ── exception plumbing (FULL builtin hierarchy) ─────────────────────────── */

static bool tr_exc_pending(const tr_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void tr_exc_clear(tr_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void tr_raise(tr_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static void tr_raise_fmt(tr_ctx *ctx, const char *cls, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    tr_raise(ctx, cls, buf);
}
static const char *tr_exc_class(const tr_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *tr_exc_parent(const char *c) {
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
        {"CalledProcessError", "SubprocessError"}, {"TimeoutExpired", "SubprocessError"},
        /* custom quarry class (AST-extracted by the oracle; identity matters
         * for except ValueError / except Exception catches) */
        {"STTResponseError", "ValueError"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (tr_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool tr_exc_matches(const tr_ctx *ctx, const char *const *classes, size_t n) {
    if (!tr_exc_pending(ctx)) return false;
    const char *cls = tr_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (tr_str_eq(cls, classes[i])) return true;
        const char *p = tr_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool tr_exc_is_exception(const tr_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return tr_exc_matches(ctx, E, 1);
}
/* _silence_midpoints: except (OSError, subprocess.SubprocessError) */
static bool tr_exc_is_silence_caught(const tr_ctx *ctx) {
    static const char *E[] = {"OSError", "SubprocessError"};
    return tr_exc_matches(ctx, E, 2);
}
/* _config_number: except (TypeError, ValueError) — STTResponseError rides
 * ValueError -> caught. */
static bool tr_exc_is_cast_caught(const tr_ctx *ctx) {
    static const char *E[] = {"TypeError", "ValueError"};
    return tr_exc_matches(ctx, E, 2);
}
/* _file_signature: except FileNotFoundError (exact family) */
static bool tr_exc_is_fnf(const tr_ctx *ctx) {
    static const char *E[] = {"FileNotFoundError"};
    return tr_exc_matches(ctx, E, 1);
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *tr_script_result(tr_ctx *ctx, const json_t *script) {
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
static json_t *tr_call(tr_ctx *ctx, const char *name, json_t *args) {
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
    if (chosen != ctx->exc) ret = tr_script_result(ctx, chosen);
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
static void tr_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void tr_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void tr_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}
static void tr_arg_num(json_t *args, const char *key, double val) {
    json_set(args, key, json_number(val));
}
/* logger seam: {"msg","args"}. args_arr ownership transferred. */
static void tr_log(tr_ctx *ctx, const char *level, const char *msg, json_t *args_arr) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = tr_call(ctx, name, a);
    json_free(r);
}
/* str(exc) of the pending exception (logger arg projection). The oracle's
 * make_exc builds subprocess.TimeoutExpired(msg, 300) and
 * CalledProcessError(1, msg), whose str() is NOT the bare marker msg. */
static char *tr_exc_str(const tr_ctx *ctx) {
    const char *cls = tr_exc_class(ctx);
    const char *msg = json_get_str(ctx->exc, "msg", "");
    if (tr_str_eq(cls, "TimeoutExpired")) {
        char *out = (char *)malloc(strlen(msg) + 64);
        sprintf(out, "Command '%s' timed out after 300 seconds", msg);
        return out;
    }
    if (tr_str_eq(cls, "CalledProcessError")) {
        char *out = (char *)malloc(strlen(msg) + 96);
        sprintf(out, "Command '%s' returned non-zero exit status 1.", msg);
        return out;
    }
    return strdup(msg);
}

/* ── string helpers ──────────────────────────────────────────────────────── */

/* Python str.strip() no-arg White_Space set (codepoints). */
static bool tr_is_ws_cp(unsigned int cp) {
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
static unsigned int tr_utf8_next(const char *s, size_t len, size_t *i) {
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
static char *tr_strip(const char *s) {
    if (!s) s = "";
    size_t len = strlen(s);
    size_t a = 0, b = len;
    while (a < b) {
        size_t j = a;
        unsigned int cp = tr_utf8_next(s, len, &j);
        if (!tr_is_ws_cp(cp)) break;
        a = j;
    }
    while (b > a) {
        size_t j = b - 1;
        while (j > a && ((unsigned char)s[j] & 0xC0) == 0x80) j--;
        size_t k = j;
        unsigned int cp = tr_utf8_next(s, len, &k);
        if (!tr_is_ws_cp(cp)) break;
        b = j;
    }
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
/* Python str(float) for NON-integral floats: shortest round-trip significant
 * digits (%.*e loop), positional for exp in [-4,16) else scientific — %.17g
 * alone diverges (probed against str() on random corpora). */
static char *tr_float_repr(double d) {
    char buf[64];
    int p;
    for (p = 1; p <= 17; p++) {
        snprintf(buf, sizeof buf, "%.*e", p - 1, d);
        if (strtod(buf, NULL) == d) break;
    }
    if (p > 17) p = 17;
    const char *ep = strchr(buf, 'e');
    int E = ep ? atoi(ep + 1) : 0;
    char *out = (char *)malloc(64);
    if (E >= -4 && E < 16) {
        int dec = p - 1 - E;
        if (dec < 0) dec = 0;
        snprintf(out, 64, "%.*f", dec, d);
    } else {
        snprintf(out, 64, "%.*e", p - 1, d);
    }
    return out;
}
/* str(node) for a scalar: None -> "None", bool -> True/False, number integral
 * str, string verbatim. Containers are outside the str() fixture domain. */
static char *tr_pystr(const json_t *v) {
    if (tr_is_none(v)) return strdup("None");
    if (tr_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (tr_is_str(v)) return strdup(tr_jstr(v));
    if (tr_is_num(v)) {
        char buf[64];
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 9.2e18) {
            snprintf(buf, sizeof buf, "%.0f", d);
            return strdup(buf);
        }
        return tr_float_repr(d);
    }
    return strdup("");
}
/* repr() of an ASCII str (quote selection + escapes, probed on py3.14). */
static char *tr_repr_str(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t len = strlen(s);
    char *out = (char *)malloc(len * 4 + 3);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = q;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
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
/* str(list) of ASCII str elements: "['a', 'b']" (repr elements, ", " join). */
static char *tr_list_str(const json_t *arr) {
    size_t cap = 3, n = arr->c.count;
    char **parts = (char **)calloc(n ? n : 1, sizeof(char *));
    for (size_t i = 0; i < n; i++) {
        parts[i] = tr_repr_str(tr_jstr(arr->c.items[i]));
        cap += strlen(parts[i]) + 2;
    }
    char *out = (char *)malloc(cap + 1);
    size_t o = 0;
    out[o++] = '[';
    for (size_t i = 0; i < n; i++) {
        if (i) { out[o++] = ','; out[o++] = ' '; }
        size_t l = strlen(parts[i]);
        memcpy(out + o, parts[i], l);
        o += l;
        free(parts[i]);
    }
    out[o++] = ']';
    out[o] = '\0';
    free(parts);
    return out;
}
/* Python format(v, '.Nf') / '%.Nf' — snprintf is correctly rounded; the
 * fixture domain avoids exact binary ties (see header). */
static char *tr_fmt_f(double v, int decimals) {
    char buf[512];
    snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return strdup(buf);
}

/* ── POSIX path helpers (oracle seeds posixpath / PurePosixPath) ─────────── */

/* PurePosixPath normalization: collapse '.'/empty parts, keep '..', preserve
 * EXACTLY two leading slashes (/// -> /). Returns malloc'd. */
static char *tr_posix_norm(const char *p) {
    if (!p) p = "";
    size_t len = strlen(p);
    size_t lead = 0;
    while (lead < len && p[lead] == '/') lead++;
    bool rooted = lead > 0;
    /* split into components */
    char **parts = (char **)calloc(len + 1, sizeof(char *));
    size_t np = 0;
    size_t i = lead;
    while (i < len) {
        size_t j = i;
        while (j < len && p[j] != '/') j++;
        if (j > i) {
            size_t l = j - i;
            if (!(l == 1 && p[i] == '.')) {
                char *c = (char *)malloc(l + 1);
                memcpy(c, p + i, l);
                c[l] = '\0';
                parts[np++] = c;
            }
        }
        i = j + 1;
    }
    size_t cap = len + 3;
    char *out = (char *)malloc(cap);
    size_t o = 0;
    if (rooted) {
        out[o++] = '/';
        if (lead == 2) out[o++] = '/';
    }
    for (size_t k = 0; k < np; k++) {
        if (k) out[o++] = '/';
        size_t l = strlen(parts[k]);
        memcpy(out + o, parts[k], l);
        o += l;
        free(parts[k]);
    }
    free(parts);
    if (o == 0) out[o++] = '.';
    out[o] = '\0';
    return out;
}
/* PurePosixPath(base) / name  (name a single component); also models
 * posixpath.join for the normalized-absolute work_dir domain. */
static char *tr_posix_join(const char *base, const char *name) {
    char *nb = tr_posix_norm(base);
    size_t l = strlen(nb);
    if (tr_str_eq(nb, ".")) {
        free(nb);
        return strdup(name ? name : "");
    }
    bool ends_slash = l > 0 && nb[l - 1] == '/';
    size_t nl = strlen(name ? name : "");
    char *out = (char *)malloc(l + nl + 2);
    strcpy(out, nb);
    if (!ends_slash) strcat(out, "/");
    strcat(out, name ? name : "");
    free(nb);
    return out;
}
/* posixpath.basename: everything AFTER the last '/' (a trailing slash yields
 * ""; no trailing-slash pre-strip). */
static char *tr_basename(const char *p) {
    if (!p) p = "";
    size_t len = strlen(p);
    size_t start = len;
    while (start > 0 && p[start - 1] != '/') start--;
    char *out = (char *)malloc(len - start + 1);
    memcpy(out, p + start, len - start);
    out[len - start] = '\0';
    return out;
}
/* shlex.quote (POSIX): empty -> ''; unsafe char [^\w@%+=:,./-] (ASCII \w) ->
 * single-quote wrap with ' -> '"'"' ; else verbatim. */
static bool tr_shlex_safe(unsigned char c) {
    if (isalnum(c) || c == '_') return true;
    return c == '@' || c == '%' || c == '+' || c == '=' || c == ':'
        || c == ',' || c == '.' || c == '/' || c == '-';
}
static char *tr_shlex_quote(const char *s) {
    if (!s) s = "";
    size_t len = strlen(s);
    if (len == 0) return strdup("''");
    bool unsafe = false;
    for (size_t i = 0; i < len; i++)
        if (!tr_shlex_safe((unsigned char)s[i])) { unsafe = true; break; }
    if (!unsafe) return strdup(s);
    /* count quotes for the '"'"' expansion */
    size_t nq = 0;
    for (size_t i = 0; i < len; i++) if (s[i] == '\'') nq++;
    char *out = (char *)malloc(len + nq * 4 + 3);
    size_t o = 0;
    out[o++] = '\'';
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\'') {
            memcpy(out + o, "'\"'\"'", 5);
            o += 5;
        } else out[o++] = s[i];
    }
    out[o++] = '\'';
    out[o] = '\0';
    return out;
}

/* ── bytes decode utf-8 replace (probed byte-for-byte on py3.14) ─────────── */

static int tr_hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* decode hex bytes -> UTF-8 str with U+FFFD replacement. Algorithm: a valid
 * multi-byte sequence decodes; an invalid lead/continuation emits ONE U+FFFD
 * for the lead byte only; a truncated-but-valid tail sequence emits ONE
 * U+FFFD for the whole remainder. */
static char *tr_decode_utf8_replace(const unsigned char *b, size_t n) {
    char *out = (char *)malloc(n * 3 + 1);
    size_t o = 0;
    size_t i = 0;
    while (i < n) {
        unsigned char c = b[i];
        if (c < 0x80) { out[o++] = (char)c; i++; continue; }
        size_t need = 0;
        unsigned int cp = 0;
        unsigned char lo2 = 0x80, hi2 = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) { need = 2; cp = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) {
            need = 3; cp = c & 0x0F;
            if (c == 0xE0) lo2 = 0xA0;
            if (c == 0xED) hi2 = 0x9F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            need = 4; cp = c & 0x07;
            if (c == 0xF0) lo2 = 0x90;
            if (c == 0xF4) hi2 = 0x8F;
        }
        if (need == 0) { /* invalid lead byte */
            memcpy(out + o, "\xEF\xBF\xBD", 3); o += 3; i++;
            continue;
        }
        size_t k = 1;
        bool bad_cont = false;
        for (; k < need; k++) {
            if (i + k >= n) break; /* truncated tail */
            unsigned char cc = b[i + k];
            unsigned char lo = (k == 1) ? lo2 : 0x80;
            unsigned char hi = (k == 1) ? hi2 : 0xBF;
            if (cc < lo || cc > hi) { bad_cont = true; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (bad_cont) { /* ONE FFFD for the lead + valid-continuation prefix
                         * span (CPython replace-handler span), advance past it */
            memcpy(out + o, "\xEF\xBF\xBD", 3); o += 3; i += k;
            continue;
        }
        if (k < need) { /* truncated-but-valid tail: ONE FFFD, consume rest */
            memcpy(out + o, "\xEF\xBF\xBD", 3); o += 3; i = n;
            continue;
        }
        /* emit codepoint as UTF-8 */
        if (cp < 0x800) {
            out[o++] = (char)(0xC0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out[o++] = (char)(0xE0 | (cp >> 12));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        } else {
            out[o++] = (char)(0xF0 | (cp >> 18));
            out[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
        }
        i += need;
    }
    out[o] = '\0';
    return out;
}
/* {"__bytes__": hex} tag -> decoded+stripped detail; NULL if not a bytes tag */
static char *tr_bytes_detail(const json_t *v) {
    if (!tr_is_obj(v)) return NULL;
    const json_t *h = json_obj_get(v, "__bytes__");
    if (!tr_is_str(h)) return NULL;
    const char *s = tr_jstr(h);
    size_t l = strlen(s);
    unsigned char *b = (unsigned char *)malloc(l / 2 + 1);
    size_t n = 0;
    for (size_t i = 0; i + 1 < l; i += 2) {
        int hi = tr_hex_val(s[i]), lo = tr_hex_val(s[i + 1]);
        if (hi < 0 || lo < 0) break;
        b[n++] = (unsigned char)((hi << 4) | lo);
    }
    char *dec = tr_decode_utf8_replace(b, n);
    free(b);
    char *stripped = tr_strip(dec);
    free(dec);
    return stripped;
}

/* ── float() for the silence-token domain (-?[\d.]+) ─────────────────────── */

/* token grammar: optional '-', digits/dots with at most one dot and at least
 * one digit. Returns true on success; false -> ValueError (caller raises with
 * the repr-quoted token). */
static bool tr_float_token(const char *tok, double *out) {
    size_t i = 0, len = strlen(tok);
    if (i < len && tok[i] == '-') i++;
    bool any_digit = false, dot = false;
    for (; i < len; i++) {
        if (tok[i] == '.') {
            if (dot) return false;
            dot = true;
        } else if (tok[i] >= '0' && tok[i] <= '9') {
            any_digit = true;
        } else return false;
    }
    if (!any_digit) return false;
    *out = strtod(tok, NULL);
    return true;
}

/* ── attribute / carrier guards ──────────────────────────────────────────── */

/* live attribute read on a seam result: non-carrier raises AttributeError
 * naming the Python type + attribute. Returns the object or NULL (raise). */
static json_t *tr_need_obj(tr_ctx *ctx, json_t *v, const char *attr) {
    if (tr_is_obj(v)) return v;
    tr_raise_fmt(ctx, "AttributeError", "'%s' object has no attribute '%s'",
                 tr_type_name(v), attr);
    return NULL;
}
/* a[str] index on a non-array / short array (exceeds_upload_limit domain) */
static json_t *tr_index(tr_ctx *ctx, const json_t *arr, size_t idx) {
    if (!tr_is_arr(arr)) {
        tr_raise_fmt(ctx, "TypeError", "'%s' object is not subscriptable",
                     tr_type_name(arr));
        return NULL;
    }
    if (idx >= arr->c.count) {
        tr_raise(ctx, "IndexError", "list index out of range");
        return NULL;
    }
    return arr->c.items[idx];
}
/* a, b = node  (limit unpack domain: array / null / number / bool) */
static bool tr_unpack2(tr_ctx *ctx, const json_t *node, json_t **a, json_t **b) {
    if (!tr_is_arr(node)) {
        tr_raise_fmt(ctx, "TypeError", "cannot unpack non-iterable %s object",
                     tr_type_name(node));
        return false;
    }
    if (node->c.count < 2) {
        tr_raise_fmt(ctx, "ValueError",
                     "not enough values to unpack (expected 2, got %zu)",
                     node->c.count);
        return false;
    }
    if (node->c.count > 2) {
        tr_raise_fmt(ctx, "ValueError",
                     "too many values to unpack (expected 2, got %zu)",
                     node->c.count);
        return false;
    }
    *a = node->c.items[0];
    *b = node->c.items[1];
    return true;
}
/* numeric '>' with live TypeError messages on non-numbers */
static bool tr_gt(tr_ctx *ctx, const json_t *a, const json_t *b, bool *out) {
    if (!tr_is_num(a) || !tr_is_num(b)) {
        tr_raise_fmt(ctx, "TypeError",
                     "'>' not supported between instances of '%s' and '%s'",
                     tr_type_name(a), tr_type_name(b));
        return false;
    }
    *out = a->num_val > b->num_val;
    return true;
}

/* str(exc) for subprocess.CalledProcessError (POSITIVE returncode domain;
 * negative rc carries a platform-dependent Signals repr — excluded). cmd is
 * inserted RAW (str(cmd)); a list cmd renders via str(list) = repr elements. */
static char *tr_cpe_str(const json_t *exc) {
    const json_t *cmd = tr_get(exc, "cmd");
    char *cmds;
    if (tr_is_arr(cmd)) cmds = tr_list_str(cmd);
    else cmds = strdup(tr_is_str(cmd) ? tr_jstr(cmd) : "");
    long long rc = (long long)tr_num(tr_get(exc, "returncode"));
    char *out = (char *)malloc(strlen(cmds) + 96);
    sprintf(out, "Command '%s' returned non-zero exit status %lld.", cmds, rc);
    free(cmds);
    return out;
}

/* ── module constants (AST-seeded in the oracle; transcribed here) ───────── */

#define TR_MB 1048576.0
#define TR_MAX_FILE_SIZE 26214400.0  /* 25 * 1024 * 1024 */
#define TR_SEGMENT_HEADROOM 0.9
#define TR_MAX_SILENCE_LOOKBACK_SECONDS 60.0
#define TR_LEGACY_XAI_STT_MODEL "grok-stt"
#define TR_XAI_STT_MODEL_ENV "STT_XAI_MODEL"
#define TR_XAI_DEFAULT_MODEL "grok-voice-transcribe-2.0"  /* STT_MODEL_CATALOG["xai"][0] */

typedef struct { const char *provider; double max_bytes; } tr_provider_cap;
static const tr_provider_cap TR_PROVIDER_MAX_BYTES[] = {
    {"openai", 25 * TR_MB}, {"groq", 25 * TR_MB}, {"mistral", 500 * TR_MB},
    {"xai", 500 * TR_MB}, {"elevenlabs", 5 * 1024 * TR_MB - 1},
};
typedef struct { const char *provider; double max_seconds; } tr_provider_secs;
static const tr_provider_secs TR_PROVIDER_MAX_SECONDS[] = {
    {"mistral", 3600.0},
};
static const tr_provider_secs TR_OPENAI_MODEL_MAX_SECONDS[] = {
    {"gpt-4o-transcribe", 450.0}, {"gpt-4o-mini-transcribe", 450.0},
    {"whisper-1", 600.0},
};
typedef struct { const char *name, *url, *sha256; } tr_model_file;
static const tr_model_file TR_MODEL_FILES[] = {
    {"ggml-base.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/5359861c739e955e79d9a303bcbc70fb988958b1/ggml-base.bin",
     "60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe"},
    {"ggml-silero-v6.2.0.bin",
     "https://huggingface.co/ggml-org/whisper-vad/resolve/9ffd54a1e1ee413ddf265af9913beaf518d1639b/ggml-silero-v6.2.0.bin",
     "2aa269b785eeb53a82983a20501ddf7c1d9c48e33ab63a41391ac6c9f7fb6987"},
};

/* ── forward declarations (internal composition) ─────────────────────────── */

static json_t *tr_error_result(tr_ctx *ctx, const json_t *error, const json_t *extra);
static json_t *tr_ok_result(tr_ctx *ctx, const json_t *transcript, const json_t *provider);
static json_t *tr_too_large(tr_ctx *ctx, const char *path, double max_bytes, const char *reason);
static json_t *tr_fits(tr_ctx *ctx, const char *path, const json_t *limit, const json_t *duration);
static json_t *tr_silence_midpoints(tr_ctx *ctx, const char *ffmpeg, const char *path);
static json_t *tr_cut_points(tr_ctx *ctx, double duration, double target, const json_t *silences);
static json_t *tr_split(tr_ctx *ctx, const char *ffmpeg, const char *path,
                        const char *work_dir, const json_t *cuts);
static json_t *tr_model_dir(tr_ctx *ctx);
static json_t *tr_file_signature(tr_ctx *ctx, const char *path, const char *digest);

/* ── tools/transcription_common.py ───────────────────────────────────────── */

/* PoP: tr_error_result @ tools/transcription_common.py:_error_result */
static json_t *tr_error_result(tr_ctx *ctx, const json_t *error, const json_t *extra) {
    /* **extra binding collision fires BEFORE the body (no seam). */
    if (tr_is_obj(extra) && json_obj_get(extra, "error")) {
        tr_raise(ctx, "TypeError",
                 "_error_result() got multiple values for argument 'error'");
        return NULL;
    }
    json_t *r = json_object();
    json_set(r, "success", json_bool(false));
    json_set(r, "transcript", json_string(""));
    json_set(r, "error", error ? json_copy(error) : json_null());
    if (tr_is_obj(extra)) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(r, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    return r;
}

/* PoP: tr_normalize_xai_stt_model @ tools/transcription_common.py:normalize_xai_stt_model */
static json_t *tr_normalize_xai_stt_model(tr_ctx *ctx, const json_t *model) {
    /* value = str(model or "").strip() — falsy collapses to "" (not "None") */
    char *raw = tr_truthy(model) ? tr_pystr(model) : strdup("");
    char *value = tr_strip(raw);
    free(raw);
    if (!value[0] || tr_str_eq(value, TR_LEGACY_XAI_STT_MODEL)) {
        free(value);
        json_t *a = json_object();
        tr_arg_str(a, "name", TR_XAI_STT_MODEL_ENV);
        tr_arg_str(a, "default", "");
        json_t *env = tr_call(ctx, "os.getenv", a);
        if (tr_exc_pending(ctx)) return NULL;
        if (!tr_is_str(env)) {
            tr_raise_fmt(ctx, "AttributeError",
                         "'%s' object has no attribute 'strip'", tr_type_name(env));
            json_free(env);
            return NULL;
        }
        char *ev = tr_strip(tr_jstr(env));
        json_free(env);
        if (!ev[0]) {
            free(ev);
            return json_string(TR_XAI_DEFAULT_MODEL);
        }
        json_t *r = json_string(ev);
        free(ev);
        return r;
    }
    json_t *r = json_string(value);
    free(value);
    return r;
}

/* PoP: tr_ok_result @ tools/transcription_common.py:_ok_result */
static json_t *tr_ok_result(tr_ctx *ctx, const json_t *transcript, const json_t *provider) {
    (void)ctx;
    json_t *r = json_object();
    json_set(r, "success", json_bool(true));
    json_set(r, "transcript", transcript ? json_copy(transcript) : json_null());
    json_set(r, "provider", provider ? json_copy(provider) : json_null());
    return r;
}

/* PoP: tr_lazy_ensure_quietly @ tools/transcription_common.py:_lazy_ensure_quietly */
static json_t *tr_lazy_ensure_quietly(tr_ctx *ctx, const char *extra) {
    /* try: import pm; pm.ensure_import(extra)  except Exception: pass */
    if (!ctx->pm_ok)
        tr_raise(ctx, "ModuleNotFoundError", "import of pm halted; None in sys.modules");
    if (!tr_exc_pending(ctx)) {
        json_t *a = json_object();
        tr_arg_str(a, "extra", extra);
        json_t *r = tr_call(ctx, "pm.ensure_import", a);
        json_free(r);
    }
    if (tr_exc_pending(ctx)) {
        if (tr_exc_is_exception(ctx)) tr_exc_clear(ctx);
        /* BaseException trio propagates */
    }
    return NULL;
}

/* PoP: tr_process_error_detail @ tools/transcription_common.py:_process_error_detail */
static json_t *tr_process_error_detail(tr_ctx *ctx, const json_t *exc) {
    if (!tr_need_obj(ctx, (json_t *)exc, "stderr")) return NULL;
    static const char *FIELDS[] = {"stderr", "stdout"};
    for (size_t f = 0; f < 2; f++) {
        const json_t *output = tr_get(exc, FIELDS[f]);
        char *detail = tr_bytes_detail(output);
        if (!detail) {
            /* str(output or "").strip() */
            char *raw = tr_truthy(output) ? tr_pystr(output) : strdup("");
            detail = tr_strip(raw);
            free(raw);
        }
        if (detail[0]) return json_string(detail);
        free(detail);
    }
    char *s = tr_cpe_str(exc);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP: tr_log_prompt_unsupported @ tools/transcription_common.py:_log_prompt_unsupported */
static json_t *tr_log_prompt_unsupported(tr_ctx *ctx, const char *label) {
    json_t *args = json_array();
    json_append(args, json_string(label ? label : ""));
    tr_log(ctx, "debug",
           "%s does not support transcription prompts \xE2\x80\x94 proceeding without the prompt.",
           args);
    return NULL;
}

/* PoP: tr_config_number @ tools/transcription_common.py:_config_number */
static json_t *tr_config_number(tr_ctx *ctx, const json_t *cfg, const char *key,
                                const json_t *def, const char *cast_seam) {
    if (!tr_need_obj(ctx, (json_t *)cfg, "get")) return NULL;
    json_t *value = tr_get_def(cfg, key, (json_t *)def);
    json_t *a = json_object();
    tr_arg(a, "value", value);
    json_t *r = tr_call(ctx, cast_seam, a);
    if (tr_exc_pending(ctx)) {
        if (tr_exc_is_cast_caught(ctx)) {
            tr_exc_clear(ctx);
            json_free(r);
            return def ? json_copy(def) : json_null();
        }
        json_free(r);
        return NULL;
    }
    return r;
}

/* ── tools/transcription_chunking.py ─────────────────────────────────────── */

/* PoP: tr_upload_limit @ tools/transcription_chunking.py:upload_limit */
static json_t *tr_upload_limit(tr_ctx *ctx, const char *provider, const json_t *model) {
    (void)ctx;
    double max_bytes = TR_MAX_FILE_SIZE;
    for (size_t i = 0; i < sizeof(TR_PROVIDER_MAX_BYTES) / sizeof(TR_PROVIDER_MAX_BYTES[0]); i++)
        if (tr_str_eq(TR_PROVIDER_MAX_BYTES[i].provider, provider)) {
            max_bytes = TR_PROVIDER_MAX_BYTES[i].max_bytes;
            break;
        }
    double max_seconds = 0;
    bool has_secs = false;
    for (size_t i = 0; i < sizeof(TR_PROVIDER_MAX_SECONDS) / sizeof(TR_PROVIDER_MAX_SECONDS[0]); i++)
        if (tr_str_eq(TR_PROVIDER_MAX_SECONDS[i].provider, provider)) {
            has_secs = true;
            max_seconds = TR_PROVIDER_MAX_SECONDS[i].max_seconds;
            break;
        }
    json_t *ms = has_secs ? json_number(max_seconds) : NULL;
    if (tr_str_eq(provider, "openai")) {
        if (ms) { json_free(ms); ms = NULL; }
        const char *mkey = tr_truthy(model) ? tr_jstr(model) : "";
        for (size_t i = 0; i < sizeof(TR_OPENAI_MODEL_MAX_SECONDS) / sizeof(TR_OPENAI_MODEL_MAX_SECONDS[0]); i++)
            if (tr_str_eq(TR_OPENAI_MODEL_MAX_SECONDS[i].provider, mkey)) {
                ms = json_number(TR_OPENAI_MODEL_MAX_SECONDS[i].max_seconds);
                break;
            }
    }
    json_t *r = json_array();
    json_append(r, json_number(max_bytes));
    json_append(r, ms ? ms : json_null());
    return r;
}

/* PoP: tr_fits @ tools/transcription_chunking.py:_fits */
static json_t *tr_fits(tr_ctx *ctx, const char *path, const json_t *limit, const json_t *duration) {
    json_t *max_bytes = NULL, *max_seconds = NULL;
    if (!tr_unpack2(ctx, limit, &max_bytes, &max_seconds)) return NULL;
    json_t *a = json_object();
    tr_arg_str(a, "path", path);
    json_t *size = tr_call(ctx, "os.path.getsize", a);
    if (tr_exc_pending(ctx)) { json_free(size); return NULL; }
    bool over = false;
    if (!tr_gt(ctx, size, max_bytes, &over)) { json_free(size); return NULL; }
    json_free(size);
    if (over) return json_bool(false);
    /* max_seconds is None or duration is None or duration <= max_seconds */
    if (tr_is_none(max_seconds) || tr_is_none(duration)) return json_bool(true);
    if (!tr_is_num(duration) || !tr_is_num(max_seconds)) {
        tr_raise_fmt(ctx, "TypeError",
                     "'<=' not supported between instances of '%s' and '%s'",
                     tr_type_name(duration), tr_type_name(max_seconds));
        return NULL;
    }
    return json_bool(duration->num_val <= max_seconds->num_val);
}

/* PoP: tr_exceeds_upload_limit @ tools/transcription_chunking.py:exceeds_upload_limit */
static json_t *tr_exceeds_upload_limit(tr_ctx *ctx, const char *path, const json_t *limit) {
    /* live evaluates os.path.getsize(path) BEFORE limit[0] — the seam fires
     * even when limit is a non-subscriptable null. */
    json_t *a = json_object();
    tr_arg_str(a, "path", path);
    json_t *size = tr_call(ctx, "os.path.getsize", a);
    if (tr_exc_pending(ctx)) { json_free(size); return NULL; }
    json_t *cap = tr_index(ctx, limit, 0);
    if (!cap) { json_free(size); return NULL; }
    bool over = false;
    if (!tr_gt(ctx, size, cap, &over)) { json_free(size); return NULL; }
    json_free(size);
    if (over) return json_bool(true);
    json_t *secs = tr_index(ctx, limit, 1);
    if (!secs) return NULL;
    if (tr_is_none(secs)) return json_bool(false);
    json_t *pa = json_object();
    tr_arg_str(pa, "path", path);
    json_t *duration = tr_call(ctx, "_probe_audio_duration", pa);
    if (tr_exc_pending(ctx)) { json_free(duration); return NULL; }
    json_t *fits = tr_fits(ctx, path, limit, duration);
    json_free(duration);
    if (tr_exc_pending(ctx)) { json_free(fits); return NULL; }
    bool f = tr_truthy(fits);
    json_free(fits);
    return json_bool(!f);
}

/* PoP: tr_too_large @ tools/transcription_chunking.py:_too_large */
static json_t *tr_too_large(tr_ctx *ctx, const char *path, double max_bytes, const char *reason) {
    json_t *a = json_object();
    tr_arg_str(a, "path", path);
    json_t *size = tr_call(ctx, "os.path.getsize", a);
    if (tr_exc_pending(ctx)) { json_free(size); return NULL; }
    double size_mb = tr_num(size) / TR_MB;
    json_free(size);
    char *s1 = tr_fmt_f(size_mb, 1);
    char *s2 = tr_fmt_f(max_bytes / TR_MB, 0);
    char msg[1024];
    snprintf(msg, sizeof msg,
             "File too large: %sMB (max %sMB) and it could not be split: %s",
             s1, s2, reason ? reason : "");
    free(s1);
    free(s2);
    json_t *err = json_string(msg);
    json_t *r = tr_error_result(ctx, err, NULL);
    json_free(err);
    return r;
}

/* re.findall(r"silence_start: (-?[\d.]+)" / r"silence_end: ([\d.]+)") —
 * literal-prefix scan, greedy token, non-overlapping. Appends token strings. */
static void tr_silence_findall(const char *text, const char *lit, bool allow_neg, json_t *out) {
    size_t litlen = strlen(lit);
    size_t len = strlen(text);
    size_t i = 0;
    while (i < len) {
        const char *p = strstr(text + i, lit);
        if (!p) break;
        size_t at = (size_t)(p - text);
        size_t j = at + litlen;
        size_t start = j;
        if (allow_neg && j < len && text[j] == '-') j++;
        size_t digits = j;
        while (j < len && ((text[j] >= '0' && text[j] <= '9') || text[j] == '.')) j++;
        if (j > digits) {
            char *tok = (char *)malloc(j - start + 1);
            memcpy(tok, text + start, j - start);
            tok[j - start] = '\0';
            json_append(out, json_string(tok));
            free(tok);
            i = j;
        } else {
            i = at + 1;
        }
    }
}

/* PoP: tr_silence_midpoints @ tools/transcription_chunking.py:_silence_midpoints */
static json_t *tr_silence_midpoints(tr_ctx *ctx, const char *ffmpeg, const char *path) {
    json_t *argv = json_array();
    static const char *HEAD[] = {"-hide_banner", "-nostats", "-i"};
    json_append(argv, json_string(ffmpeg ? ffmpeg : ""));
    for (size_t i = 0; i < 3; i++) json_append(argv, json_string(HEAD[i]));
    json_append(argv, json_string(path ? path : ""));
    static const char *TAIL[] = {"-af", "silencedetect=noise=-35dB:d=0.4", "-f", "null", "-"};
    for (size_t i = 0; i < 5; i++) json_append(argv, json_string(TAIL[i]));
    json_t *a = json_object();
    json_set(a, "argv", argv);
    tr_arg_num(a, "timeout", 300);
    json_t *result = tr_call(ctx, "_run_quiet", a);
    if (tr_exc_pending(ctx)) {
        if (tr_exc_is_silence_caught(ctx)) {
            /* logger.debug("silencedetect failed for %s: %s", path, exc) */
            char *es = tr_exc_str(ctx);
            json_t *la = json_array();
            json_append(la, json_string(path ? path : ""));
            json_append(la, json_string(es));
            free(es);
            tr_exc_clear(ctx);  /* clear BEFORE the handler seam */
            tr_log(ctx, "debug", "silencedetect failed for %s: %s", la);
            json_free(result);
            if (tr_exc_pending(ctx)) return NULL; /* logger raise propagates */
            return json_array();
        }
        json_free(result);
        return NULL;
    }
    if (!tr_need_obj(ctx, result, "stderr")) { json_free(result); return NULL; }
    const char *stderr_s = tr_jstr(tr_get(result, "stderr"));
    json_t *starts = json_array(), *ends = json_array();
    tr_silence_findall(stderr_s, "silence_start: ", true, starts);
    tr_silence_findall(stderr_s, "silence_end: ", false, ends);
    json_free(result);
    /* float(v) for each token — ValueError propagates (outside the try) */
    size_t n = starts->c.count < ends->c.count ? starts->c.count : ends->c.count;
    double *sv = (double *)calloc(n ? n : 1, sizeof(double));
    double *ev = (double *)calloc(n ? n : 1, sizeof(double));
    for (size_t i = 0; i < starts->c.count; i++) {
        const char *tok = tr_jstr(starts->c.items[i]);
        double d;
        if (!tr_float_token(tok, &d)) {
            char *r = tr_repr_str(tok);
            tr_raise_fmt(ctx, "ValueError", "could not convert string to float: %s", r);
            free(r);
            goto fail;
        }
        if (i < n) sv[i] = d;
    }
    for (size_t i = 0; i < ends->c.count; i++) {
        const char *tok = tr_jstr(ends->c.items[i]);
        double d;
        if (!tr_float_token(tok, &d)) {
            char *r = tr_repr_str(tok);
            tr_raise_fmt(ctx, "ValueError", "could not convert string to float: %s", r);
            free(r);
            goto fail;
        }
        if (i < n) ev[i] = d;
    }
    json_t *mids = json_array();
    for (size_t i = 0; i < n; i++) {
        double start = sv[i] > 0.0 ? sv[i] : 0.0;
        json_append(mids, json_number((start + ev[i]) / 2));
    }
    free(sv);
    free(ev);
    json_free(starts);
    json_free(ends);
    return mids;
fail:
    free(sv);
    free(ev);
    json_free(starts);
    json_free(ends);
    return NULL;
}

/* PoP: tr_cut_points @ tools/transcription_chunking.py:_cut_points */
static json_t *tr_cut_points(tr_ctx *ctx, double duration, double target, const json_t *silences) {
    (void)ctx;
    json_t *cuts = json_array();
    double position = 0.0;
    double lookback = TR_MAX_SILENCE_LOOKBACK_SECONDS < target / 4
                    ? TR_MAX_SILENCE_LOOKBACK_SECONDS : target / 4;
    while (duration - position > target) {
        double ideal = position + target;
        double best = 0.0;
        bool any = false;
        if (tr_is_arr(silences)) {
            for (size_t i = 0; i < silences->c.count; i++) {
                double s = tr_num(silences->c.items[i]);
                if (ideal - lookback <= s && s <= ideal && s > position) {
                    if (!any || s > best) { best = s; any = true; }
                }
            }
        }
        position = any ? best : ideal;
        json_append(cuts, json_number(position));
    }
    return cuts;
}

/* PoP: tr_split @ tools/transcription_chunking.py:_split */
static json_t *tr_split(tr_ctx *ctx, const char *ffmpeg, const char *path,
                        const char *work_dir, const json_t *cuts) {
    char *pattern = tr_posix_join(work_dir, "part%03d.m4a");
    /* ",".join(f"{c:.3f}" for c in cuts) */
    size_t cap = 1;
    size_t n = tr_is_arr(cuts) ? cuts->c.count : 0;
    char **fs = (char **)calloc(n ? n : 1, sizeof(char *));
    for (size_t i = 0; i < n; i++) {
        fs[i] = tr_fmt_f(tr_num(cuts->c.items[i]), 3);
        cap += strlen(fs[i]) + 1;
    }
    char *times = (char *)malloc(cap + 1);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (i) times[o++] = ',';
        size_t l = strlen(fs[i]);
        memcpy(times + o, fs[i], l);
        o += l;
        free(fs[i]);
    }
    times[o] = '\0';
    free(fs);
    json_t *argv = json_array();
    json_append(argv, json_string(ffmpeg ? ffmpeg : ""));
    static const char *MID[] = {"-y", "-loglevel", "error", "-i"};
    for (size_t i = 0; i < 4; i++) json_append(argv, json_string(MID[i]));
    json_append(argv, json_string(path ? path : ""));
    static const char *SEG[] = {"-vn", "-c", "copy", "-f", "segment", "-segment_times"};
    for (size_t i = 0; i < 6; i++) json_append(argv, json_string(SEG[i]));
    json_append(argv, json_string(times));
    json_append(argv, json_string("-reset_timestamps"));
    json_append(argv, json_string("1"));
    json_append(argv, json_string(pattern));
    free(times);
    free(pattern);
    json_t *a = json_object();
    json_set(a, "argv", argv);
    tr_arg_num(a, "timeout", 300);
    json_t *r = tr_call(ctx, "_run_quiet", a);
    json_free(r);
    if (tr_exc_pending(ctx)) return NULL;
    json_t *la = json_object();
    tr_arg_str(la, "path", work_dir);
    json_t *names = tr_call(ctx, "os.listdir", la);
    if (tr_exc_pending(ctx)) { json_free(names); return NULL; }
    if (!tr_is_arr(names)) {
        tr_raise_fmt(ctx, "TypeError", "'%s' object is not iterable", tr_type_name(names));
        json_free(names);
        return NULL;
    }
    /* sorted(join(work_dir, name) for name in names if name.startswith("part")) */
    json_t *parts = json_array();
    for (size_t i = 0; i < names->c.count; i++) {
        const json_t *nm = names->c.items[i];
        if (!tr_is_str(nm)) {
            tr_raise_fmt(ctx, "AttributeError",
                         "'%s' object has no attribute 'startswith'", tr_type_name(nm));
            json_free(names);
            json_free(parts);
            return NULL;
        }
        const char *s = tr_jstr(nm);
        if (strncmp(s, "part", 4) != 0) continue;
        char *full = tr_posix_join(work_dir, s);
        json_append(parts, json_string(full));
        free(full);
    }
    json_free(names);
    /* insertion sort by strcmp (ASCII domain == Python str order) */
    for (size_t i = 1; i < parts->c.count; i++) {
        json_t *key = parts->c.items[i];
        size_t j = i;
        while (j > 0 && strcmp(tr_jstr(parts->c.items[j - 1]), tr_jstr(key)) > 0) {
            parts->c.items[j] = parts->c.items[j - 1];
            j--;
        }
        parts->c.items[j] = key;
    }
    return parts;
}

/* PoP: tr_transcribe_oversized @ tools/transcription_chunking.py:transcribe_oversized */
static json_t *tr_transcribe_oversized(tr_ctx *ctx, const char *path, const json_t *limit,
                                       const char *dispatch_seam) {
    /* from tools.voice_mode_transcript import is_whisper_hallucination — the
     * stub module always imports; the CALL rides the seam. */
    json_t *max_bytes = NULL, *max_seconds = NULL;
    if (!tr_unpack2(ctx, limit, &max_bytes, &max_seconds)) return NULL;
    double mb = tr_num(max_bytes);
    json_t *fa = json_object();
    json_t *ffmpeg = tr_call(ctx, "_find_ffmpeg_binary", fa);
    if (tr_exc_pending(ctx)) { json_free(ffmpeg); return NULL; }
    if (!tr_truthy(ffmpeg)) {
        json_free(ffmpeg);
        return tr_too_large(ctx, path, mb, "ffmpeg was not found");
    }
    json_t *ta = json_object();
    tr_arg_str(ta, "prefix", "hermes-stt-split-");
    tr_arg_bool(ta, "ignore_cleanup_errors", true);
    json_t *td = tr_call(ctx, "TemporaryDirectory", ta);
    if (tr_exc_pending(ctx)) { json_free(ffmpeg); json_free(td); return NULL; }
    const char *work_dir = tr_jstr(td);  /* domain: non-empty str script */
    json_t *tca = json_object();
    tr_arg_str(tca, "path", path);
    tr_arg_str(tca, "work_dir", work_dir);
    json_t *tc = tr_call(ctx, "_transcode_audio_for_stt", tca);
    if (tr_exc_pending(ctx)) goto fail_td;
    json_t *compact = NULL, *error = NULL;
    if (!tr_unpack2(ctx, tc, &compact, &error)) { json_free(tc); goto fail_td; }
    if (tr_truthy(error) || !tr_truthy(compact)) {
        char *reason = tr_truthy(error) ? tr_pystr(error)
                                        : strdup("re-encoding produced no file");
        json_free(tc);
        json_t *r = tr_too_large(ctx, path, mb, reason);
        free(reason);
        json_free(td);
        json_free(ffmpeg);
        return r;
    }
    char *compact_s = strdup(tr_jstr(compact));
    json_free(tc);
    json_t *pa = json_object();
    tr_arg_str(pa, "path", compact_s);
    json_t *duration = tr_call(ctx, "_probe_audio_duration", pa);
    if (tr_exc_pending(ctx)) { free(compact_s); goto fail_td; }
    json_t *fits = tr_fits(ctx, compact_s, limit, duration);
    if (tr_exc_pending(ctx)) { json_free(duration); json_free(fits); free(compact_s); goto fail_td; }
    if (tr_truthy(fits)) {
        json_free(fits);
        /* logger.info(..., basename(path), getsize(compact) / _MB) — the
         * getsize seam fires INSIDE the arg evaluation, BEFORE logger.info */
        json_t *ga = json_object();
        tr_arg_str(ga, "path", compact_s);
        json_t *sz = tr_call(ctx, "os.path.getsize", ga);
        if (tr_exc_pending(ctx)) { json_free(duration); json_free(sz); free(compact_s); goto fail_td; }
        char *bn = tr_basename(path);
        double size_mb = tr_num(sz) / TR_MB;
        json_free(sz);
        /* logger args are the RAW values (formatting lives in the logger) */
        json_t *la = json_array();
        json_append(la, json_string(bn));
        json_append(la, json_number(size_mb));
        free(bn);
        tr_log(ctx, "info",
               "Re-encoded oversized audio %s to %.1fMB for a single STT upload", la);
        if (tr_exc_pending(ctx)) { json_free(duration); free(compact_s); goto fail_td; }
        json_t *da = json_object();
        tr_arg_str(da, "path", compact_s);
        json_t *r = tr_call(ctx, dispatch_seam, da);
        json_free(duration);
        free(compact_s);
        json_free(td);
        json_free(ffmpeg);
        return r;  /* raise (if any) propagates; result returned verbatim */
    }
    json_free(fits);
    if (!tr_truthy(duration)) {
        json_free(duration);
        free(compact_s);
        json_t *r = tr_too_large(ctx, path, mb, "its duration could not be read");
        json_free(td);
        json_free(ffmpeg);
        return r;
    }
    /* target = max_bytes * _SEGMENT_HEADROOM * duration / getsize(compact) */
    json_t *ga2 = json_object();
    tr_arg_str(ga2, "path", compact_s);
    json_t *sz2 = tr_call(ctx, "os.path.getsize", ga2);
    if (tr_exc_pending(ctx)) { json_free(duration); json_free(sz2); free(compact_s); goto fail_td; }
    double size2 = tr_num(sz2);
    json_free(sz2);
    if (size2 == 0.0) {
        tr_raise(ctx, "ZeroDivisionError", "float division by zero");
        json_free(duration);
        free(compact_s);
        goto fail_td;
    }
    double target = mb * TR_SEGMENT_HEADROOM * tr_num(duration) / size2;
    if (tr_truthy(max_seconds)) {
        double cap = tr_num(max_seconds) * TR_SEGMENT_HEADROOM;
        if (cap < target) target = cap;
    }
    json_t *silences = tr_silence_midpoints(ctx, tr_jstr(ffmpeg), compact_s);
    if (tr_exc_pending(ctx)) { json_free(duration); json_free(silences); free(compact_s); goto fail_td; }
    json_t *cuts = tr_cut_points(ctx, tr_num(duration), target, silences);
    json_free(silences);
    json_t *parts = tr_split(ctx, tr_jstr(ffmpeg), compact_s, work_dir, cuts);
    json_free(cuts);
    if (tr_exc_pending(ctx)) { json_free(duration); json_free(parts); free(compact_s); goto fail_td; }
    /* logger.info("Transcribing %s (%.0fs) in %d segments of <= %.0fs",
     *             basename(path), duration, len(parts), math.ceil(target)) —
     * RAW values as args (the logger formats, not the call site). */
    {
        char *bn = tr_basename(path);
        json_t *la = json_array();
        json_append(la, json_string(bn));
        json_append(la, json_number(tr_num(duration)));
        json_append(la, json_number((double)parts->c.count));
        json_append(la, json_number(ceil(target)));
        free(bn);
        tr_log(ctx, "info", "Transcribing %s (%.0fs) in %d segments of <= %.0fs", la);
    }
    if (tr_exc_pending(ctx)) { json_free(duration); json_free(parts); free(compact_s); goto fail_td; }
    json_t *transcripts = json_array();
    json_t *provider = json_string("");
    size_t nparts = parts->c.count;
    for (size_t idx = 0; idx < nparts; idx++) {
        const char *part = tr_jstr(parts->c.items[idx]);
        json_t *da = json_object();
        tr_arg_str(da, "path", part);
        json_t *result = tr_call(ctx, dispatch_seam, da);
        if (tr_exc_pending(ctx)) goto loop_fail;
        if (!tr_need_obj(ctx, result, "get")) { json_free(result); goto loop_fail; }
        json_t *p = tr_get(result, "provider");
        if (tr_truthy(p)) {
            json_free(provider);
            provider = json_copy(p);
        }
        if (tr_truthy(tr_get(result, "no_speech"))) { json_free(result); continue; }
        if (!tr_truthy(tr_get(result, "success"))) {
            json_t *edef = json_string("unknown error");
            json_t *err = tr_get_def(result, "error", edef);
            char *es = tr_pystr(err);
            json_free(edef);
            char msg[512];
            snprintf(msg, sizeof msg, "Segment %zu/%zu failed: %s", idx + 1, nparts, es);
            free(es);
            json_free(result);
            json_t *extra = json_object();
            json_set(extra, "provider", json_copy(provider));
            json_t *errnode = json_string(msg);
            json_t *r = tr_error_result(ctx, errnode, extra);
            json_free(errnode);
            json_free(extra);
            json_free(transcripts);
            json_free(provider);
            json_free(duration);
            json_free(parts);
            free(compact_s);
            json_free(td);
            json_free(ffmpeg);
            return r;
        }
        json_t *t = tr_get(result, "transcript");
        char *text;
        if (tr_truthy(t)) {
            if (!tr_is_str(t)) {
                tr_raise_fmt(ctx, "AttributeError",
                             "'%s' object has no attribute 'strip'", tr_type_name(t));
                json_free(result);
                goto loop_fail;
            }
            text = tr_strip(tr_jstr(t));
        } else {
            text = strdup("");
        }
        json_free(result);
        if (text[0]) {
            json_t *ha = json_object();
            tr_arg_str(ha, "text", text);
            json_t *hall = tr_call(ctx, "is_whisper_hallucination", ha);
            if (tr_exc_pending(ctx)) { free(text); goto loop_fail; }
            bool hallucinated = tr_truthy(hall);
            json_free(hall);
            if (!hallucinated) json_append(transcripts, json_string(text));
        }
        free(text);
        continue;
    loop_fail:
        json_free(transcripts);
        json_free(provider);
        json_free(duration);
        json_free(parts);
        free(compact_s);
        json_free(td);
        json_free(ffmpeg);
        return NULL;
    }
    json_free(duration);
    json_free(parts);
    free(compact_s);
    json_free(td);
    json_free(ffmpeg);
    if (transcripts->c.count == 0) {
        json_free(transcripts);
        json_t *extra = json_object();
        tr_arg_bool(extra, "no_speech", true);
        json_set(extra, "provider", json_copy(provider));
        json_free(provider);
        json_t *errnode = json_string("Transcription returned an empty transcript");
        json_t *r = tr_error_result(ctx, errnode, extra);
        json_free(errnode);
        json_free(extra);
        return r;
    }
    /* " ".join(transcripts) */
    size_t cap = 1;
    for (size_t i = 0; i < transcripts->c.count; i++)
        cap += strlen(tr_jstr(transcripts->c.items[i])) + 1;
    char *joined = (char *)malloc(cap + 1);
    size_t o = 0;
    for (size_t i = 0; i < transcripts->c.count; i++) {
        if (i) joined[o++] = ' ';
        size_t l = strlen(tr_jstr(transcripts->c.items[i]));
        memcpy(joined + o, tr_jstr(transcripts->c.items[i]), l);
        o += l;
    }
    joined[o] = '\0';
    json_free(transcripts);
    json_t *jn = json_string(joined);
    free(joined);
    json_t *ok = tr_ok_result(ctx, jn, provider);
    json_free(jn);
    json_free(provider);
    if (tr_exc_pending(ctx)) { json_free(ok); return NULL; }
    json_set(ok, "segments", json_number((double)nparts));
    return ok;
fail_td:
    json_free(td);
    json_free(ffmpeg);
    return NULL;
}

/* ── tools/transcription_whisper_cpp.py ──────────────────────────────────── */

/* PoP: tr_model_dir @ tools/transcription_whisper_cpp.py:_model_dir */
static json_t *tr_model_dir(tr_ctx *ctx) {
    json_t *a = json_object();
    json_t *home = tr_call(ctx, "get_hermes_home", a);
    if (tr_exc_pending(ctx)) { json_free(home); return NULL; }
    if (!tr_is_str(home)) {
        tr_raise_fmt(ctx, "TypeError",
                     "unsupported operand type(s) for /: '%s' and 'str'",
                     tr_type_name(home));
        json_free(home);
        return NULL;
    }
    char *j1 = tr_posix_join(tr_jstr(home), "cache");
    char *j2 = tr_posix_join(j1, "whisper.cpp");
    free(j1);
    json_free(home);
    json_t *r = json_string(j2);
    free(j2);
    return r;
}

/* PoP: tr_file_signature @ tools/transcription_whisper_cpp.py:_file_signature */
static json_t *tr_file_signature(tr_ctx *ctx, const char *path, const char *digest) {
    json_t *a = json_object();
    /* path is a Path live: the stat seam arg is .as_posix() (normalized) */
    char *pn = tr_posix_norm(path);
    tr_arg_str(a, "path", pn);
    free(pn);
    json_t *stat = tr_call(ctx, "stat", a);
    if (tr_exc_pending(ctx)) {
        if (tr_exc_is_fnf(ctx)) {
            tr_exc_clear(ctx);
            json_free(stat);
            return NULL;
        }
        json_free(stat);
        return NULL;
    }
    if (!tr_need_obj(ctx, stat, "st_ino")) { json_free(stat); return NULL; }
    json_t *r = json_array();
    json_append(r, json_string(digest ? digest : ""));
    static const char *FIELDS[] = {"st_ino", "st_size", "st_mtime_ns", "st_ctime_ns"};
    for (size_t i = 0; i < 4; i++) {
        json_t *v = tr_get(stat, FIELDS[i]);
        json_append(r, v ? json_copy(v) : json_null());
    }
    json_free(stat);
    return r;
}

/* PoP: tr_whisper_cpp_command @ tools/transcription_whisper_cpp.py:whisper_cpp_command */
static json_t *tr_whisper_cpp_command(tr_ctx *ctx) {
    if (!ctx->pm_ok) {
        tr_raise(ctx, "ModuleNotFoundError", "import of pm halted; None in sys.modules");
        return NULL;
    }
    json_t *a = json_object();
    tr_arg_str(a, "name", "whispercpp-cpu");
    tr_arg_bool(a, "allow_outdated", true);
    json_t *installed = tr_call(ctx, "pm.installed_package", a);
    if (tr_exc_pending(ctx)) { json_free(installed); return NULL; }
    if (tr_is_none(installed)) { json_free(installed); return NULL; }
    if (!tr_need_obj(ctx, installed, "binary")) { json_free(installed); return NULL; }
    char *binary = strdup(tr_jstr(tr_get(installed, "binary")));  /* present domain */
    json_free(installed);
    json_t *models = tr_model_dir(ctx);
    if (tr_exc_pending(ctx)) { json_free(models); return NULL; }
    char *m1 = tr_posix_join(tr_jstr(models), "ggml-base.bin");
    char *m2 = tr_posix_join(tr_jstr(models), "ggml-silero-v6.2.0.bin");
    json_free(models);
    /* binary is a Path live: .as_posix() normalizes ("" -> ".") */
    char *bnorm = tr_posix_norm(binary);
    free(binary);
    char *qb = tr_shlex_quote(bnorm);
    free(bnorm);
    char *q1 = tr_shlex_quote(m1);
    char *q2 = tr_shlex_quote(m2);
    free(m1);
    free(m2);
    size_t cap = strlen(qb) + strlen(q1) + strlen(q2) + 160;
    char *cmd = (char *)malloc(cap);
    snprintf(cmd, cap,
             "%s -m %s -f {input_path} -l {language} -t 4 -bs 5 -bo 5 -ng -nt -otxt "
             "-of {output_dir}/transcript --vad -vm %s -vsd 500",
             qb, q1, q2);
    free(qb);
    free(q1);
    free(q2);
    json_t *r = json_string(cmd);
    free(cmd);
    return r;
}

/* signature array equality (digest str + 4 numbers) */
static bool tr_sig_eq(const json_t *a, const json_t *b) {
    if (!tr_is_arr(a) || !tr_is_arr(b) || a->c.count != b->c.count) return false;
    for (size_t i = 0; i < a->c.count; i++) {
        const json_t *x = a->c.items[i], *y = b->c.items[i];
        if (tr_is_str(x) && tr_is_str(y)) {
            if (!tr_str_eq(tr_jstr(x), tr_jstr(y))) return false;
        } else if (tr_is_num(x) && tr_is_num(y)) {
            if (x->num_val != y->num_val) return false;
        } else return false;
    }
    return true;
}

/* PoP: tr_ensure_whisper_cpp_models @ tools/transcription_whisper_cpp.py:ensure_whisper_cpp_models */
static json_t *tr_ensure_whisper_cpp_models(tr_ctx *ctx, const char *model_name) {
    if (!tr_str_eq(model_name, "base")) {
        char *rp = tr_repr_str(model_name ? model_name : "");
        tr_raise_fmt(ctx, "ValueError",
                     "Windows ARM64 whisper.cpp currently supports stt.local.model: base; "
                     "requested %s", rp);
        free(rp);
        return NULL;
    }
    if (!ctx->pm_downloader_ok) {
        tr_raise(ctx, "ModuleNotFoundError",
                 "import of pm.downloader halted; None in sys.modules");
        return NULL;
    }
    json_t *destination = tr_model_dir(ctx);
    if (tr_exc_pending(ctx)) { json_free(destination); return NULL; }
    const char *dest_s = tr_jstr(destination);
    size_t nfiles = sizeof(TR_MODEL_FILES) / sizeof(TR_MODEL_FILES[0]);
    json_t *sources = json_array();
    for (size_t i = 0; i < nfiles; i++) {
        char *d = tr_posix_join(dest_s, TR_MODEL_FILES[i].name);
        json_t *a = json_object();
        tr_arg_str(a, "url", TR_MODEL_FILES[i].url);
        tr_arg_str(a, "dest", d);
        tr_arg_str(a, "sha256", TR_MODEL_FILES[i].sha256);
        free(d);
        json_t *src = tr_call(ctx, "Source", a);
        if (tr_exc_pending(ctx)) { json_free(sources); json_free(destination); return NULL; }
        json_append(sources, src ? src : json_null());
    }
    /* pending = [s for s in sources if (sig := _file_signature(s.dest, s.sha256))
     *            is None or _verified_files.get(s.dest) != sig] */
    json_t *vf = tr_get(ctx->state, "verified_files");
    json_t *pending = json_array();
    for (size_t i = 0; i < sources->c.count; i++) {
        json_t *src = sources->c.items[i];
        if (!tr_need_obj(ctx, src, "dest")) {
            json_free(sources); json_free(pending); json_free(destination); return NULL;
        }
        const char *sd = tr_jstr(tr_get(src, "dest"));
        const char *sha = tr_jstr(tr_get(src, "sha256"));
        json_t *sig = tr_file_signature(ctx, sd, sha);
        if (tr_exc_pending(ctx)) {
            json_free(sources); json_free(pending); json_free(destination); return NULL;
        }
        bool keep;
        if (!sig) keep = true;
        else {
            json_t *prior = tr_get(vf, sd);
            keep = !prior || !tr_sig_eq(prior, sig);
            json_free(sig);
        }
        if (keep) json_append(pending, json_copy(src));
    }
    json_free(sources);
    if (pending->c.count == 0) {
        json_free(pending);
        json_free(destination);
        return NULL;
    }
    tr_log(ctx, "info",
           "Preparing whisper.cpp Base and VAD models (first use downloads about 149 MB)",
           json_array());
    if (tr_exc_pending(ctx)) { json_free(pending); json_free(destination); return NULL; }
    /* Download(pending, partials_dir=destination / "partials") — destination
     * was computed ONCE above; no second _model_dir call. */
    json_t *da = json_object();
    json_t *sarr = json_array();
    for (size_t i = 0; i < pending->c.count; i++) {
        json_t *src = pending->c.items[i];
        json_t *p = json_object();
        tr_arg(p, "url", tr_get(src, "url"));
        tr_arg(p, "dest", tr_get(src, "dest"));
        tr_arg(p, "sha256", tr_get(src, "sha256"));
        json_append(sarr, p);
    }
    json_set(da, "sources", sarr);
    {
        char *pd = tr_posix_join(dest_s, "partials");
        tr_arg_str(da, "partials_dir", pd);
        free(pd);
    }
    json_free(destination);
    json_t *dl = tr_call(ctx, "Download", da);
    if (tr_exc_pending(ctx)) { json_free(pending); json_free(dl); return NULL; }
    if (!tr_need_obj(ctx, dl, "run")) { json_free(pending); json_free(dl); return NULL; }
    json_free(dl);
    json_t *ra = json_object();
    json_t *rr = tr_call(ctx, "Download.run", ra);
    json_free(rr);
    if (tr_exc_pending(ctx)) { json_free(pending); return NULL; }
    for (size_t i = 0; i < pending->c.count; i++) {
        json_t *src = pending->c.items[i];
        const char *sd = tr_jstr(tr_get(src, "dest"));
        const char *sha = tr_jstr(tr_get(src, "sha256"));
        json_t *sig = tr_file_signature(ctx, sd, sha);
        if (tr_exc_pending(ctx)) { json_free(pending); return NULL; }
        if (sig) json_set(vf, sd, sig);
    }
    json_free(pending);
    return NULL;
}
