/*
 * irc_adapter_flow.c — IRC adapter FLOW cores and leaves of
 * plugins/platforms/irc/adapter.py (hermes-agent).
 *
 * Ports the DECISION SEQUENCES of the IRC protocol parse / nick-collision /
 * PRIVMSG-gating / dispatch cluster, the markdown-strip + UTF-8 chunk-split
 * message pipeline, the env/config resolution leaves, the async receive loop,
 * and the standalone (cron) send lifecycle (_StandaloneConn raw/pump/close +
 * _sa_register/_sa_join/_standalone_send). Call order, short-circuit branches,
 * early returns, self-attribute mutation history and exception propagation
 * mirror the live Python.
 *
 * Universal seam: every SEAMED foreign collaborator enters through ONE
 * callback irc_call(ctx, name, args). The harness wires ctx->io to a recording
 * stub fed the case's scripted results; the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. args is ADOPTED into the trace event; the return is a NEW owned
 * json_t (NULL == Python None; {"__raise__":cls,"message":str(exc)} == a raised
 * exception). A missing/exhausted script is a fail-closed RuntimeError on both
 * sides. BaseException kinds (SystemExit / KeyboardInterrupt / CancelledError /
 * GeneratorExit) pass through every `except Exception` catch site; the full
 * builtin hierarchy is tabulated below (irc_exc_parent).
 *
 * Mode table (how a function is invoked BY ITS CALLERS):
 *  REAL (run inline, direct C call): every ported function below. A REAL
 *    function calls its REAL children directly (core helpers) and its SEAMED
 *    children via irc_call.
 *  SEAMED (irc_call, scripted): the foreign collaborators only —
 *    _get_scoped_secret / _seed_extra_from_env / send_error / t (i18n),
 *    time.time, uuid.uuid4, datetime.now, ssl.create_default_context,
 *    asyncio.open_connection / asyncio.sleep / asyncio.get_running_loop,
 *    logger.error / logger.warning / logger.debug, the stream doubles
 *    (writer.is_closing/write/drain/close/wait_closed, reader.at_eof/read/
 *    readuntil, loop.time), registration_event.set, the base methods
 *    (_set_fatal_error / _notify_fatal_error / build_source / handle_message),
 *    and on_msg (the pump callback, DIRECT drive only). No sibling IRC function
 *    is claimed by another port, so none is seamed.
 *
 * Async model: the live coroutines are driven under asyncio.run by the oracle;
 * awaited seams are async recorders. C is synchronous — an awaited seam is one
 * irc_call. asyncio.wait_for is TRANSPARENT (not a seam): timeouts are scripted
 * as raises on the inner operation (reader.readuntil / reader.read /
 * writer.wait_closed). asyncio.CancelledError / TimeoutError /
 * IncompleteReadError are the real classes on the oracle side.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and irc_-prefixed.
 *
 * Representation contract (shared with tests/oracle_irc_adapter_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - The adapter self double is ctx.state (a JSON object of live attributes);
 *   methods read/write it in place. A DIRECT self._attr read with the key
 *   ABSENT raises AttributeError "'IRCAdapter' object has no attribute 'X'"
 *   (the oracle builds a real type("IRCAdapter", (), clsdict) double). The
 *   _StandaloneConn double uses the class name "_StandaloneConn".
 * - Stream/loop/event doubles are presence markers: state._writer / _reader is
 *   null (None) or a non-empty object marker ({"__writer__":true} etc.); their
 *   methods are SEAMED. registration_event is null or {"__event__":true}.
 *   _message_handler is null (falsy -> early return) or a truthy marker.
 * - bytes project to their utf-8 decoded str (ASCII corpus): _encode_line and
 *   writer.write data carry "...\r\n"; reader.read/readuntil scripts carry the
 *   decoded str (the oracle re-encodes to bytes for the live decode).
 * - tuple returns (_server_channel) project to 2-arrays; the _EOF sentinel
 *   (object()) projects to {"__eof__":true}; SendResult projects via
 *   dataclasses.asdict to ALL 8 fields; MessageEvent projects via asdict to ALL
 *   33 fields (message_type MessageType.TEXT -> "text").
 * - Integral floats == ints (libjson %.0f; oracle canon folds integral floats
 *   to int). str()-computed positions carry str/int/bool/None only.
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - ASCII scope: content / nicks / targets are ASCII; codepoint len == byte len
 *   for the chunk splitter and slices. The only whitespace used is the ASCII
 *   set {09,0a,0b,0c,0d,1c,1d,1e,1f,20}. str.split()/strip() and re \s share
 *   that set on py3.14; int() strips the NARROWER set {09,0a,0b,0c,0d,20}
 *   (NOT 1c-1f) — irc_strip vs irc_int_of keep them apart.
 * - _chunk_paragraph limit >= 1 (limit <= 0 with non-space-leading text is a
 *   live infinite loop, out of domain).
 * - port_value (int()-coerced) is int, bool, None or a clean ASCII decimal
 *   string (optional sign, int-ws padding, NO underscores); the invalid-port
 *   repr() corpus is printable ASCII without quotes/backslashes. No float
 *   infinity/nan (int() raises OverflowError, outside the (TypeError,ValueError)
 *   catch).
 * - _strip_markdown regex corpus is ASCII; the eight _MARKDOWN_RULES are
 *   transcribed as literal scanners with re.sub left-to-right / non-greedy /
 *   match-end-resume semantics (. does not cross \n; [^\]]* / [^)]+ do).
 * - uuid.uuid4 scripts are {"hex":str} objects or null (None.hex raises);
 *   datetime.now / time.time scripts are JSON-able scalars passed through.
 * - asyncio.open_connection scripts are 2-arrays (reader/writer doubles) or a
 *   non-iterable / wrong-arity value (the unpack TypeError/ValueError path).
 * - send_error / _seed_extra_from_env scripts are objects (dicts); a null
 *   _seed_extra_from_env result exercises the {**None} TypeError.
 * - _get_scoped_secret results consumed by .strip()/.lower()/.rstrip() are str
 *   or null (the AttributeError path); check_requirements/_env_enablement use
 *   the "" default, _standalone_send/_env_or_extra use the None default.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct irc_ctx {
    json_t *state;      /* owned self double; mutated in place; emitted as out.state.self */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} irc_ctx_t;

/* ── basic helpers ── */
static bool irc_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *irc_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static const char *irc_msg(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "message") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool irc_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool irc_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool irc_is_true(const json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }
static const char *irc_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *irc_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *irc_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return irc_err(type, msg);
}
static json_t *irc_attr_err(const char *cls, const char *key) {
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", cls, key);
    return irc_err("AttributeError", msg);
}
/* Python type(x).__name__ for error messages (integral number==int). */
static const char *irc_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
/* The one universal seam. args ADOPTED; return fresh owned node. */
static json_t *irc_call(irc_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = irc_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = irc_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = irc_make_exc(raise);
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
static json_t *irc_args(void) { return json_object(); }
static void irc_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void irc_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void irc_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void irc_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void irc_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Propagate a raise result (transfers ownership). */
#define IA_PROP(r) do { if (irc_is_raise(r)) return (r); } while (0)

/* ── exception hierarchy (FULL builtin tree; first-base parent chain; unknown
 * kinds default to the Exception family, matching the oracle's synthetic
 * Exception subclasses). CancelledError -> BaseException ONLY so it passes
 * through `except Exception`. ── */
typedef struct { const char *name, *parent; } irc_exc_parent_t;
static const irc_exc_parent_t IA_EXC_PARENTS[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
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
    {"WindowsError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
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
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"IncompleteReadError", "Exception"},
    {NULL, NULL}
};
static const char *irc_exc_parent(const char *kind) {
    for (size_t i = 0; IA_EXC_PARENTS[i].name; i++)
        if (!strcmp(IA_EXC_PARENTS[i].name, kind)) return IA_EXC_PARENTS[i].parent;
    return NULL;
}
static bool irc_exc_is_exception(const char *kind) {
    if (!kind) return true;
    for (int depth = 0; kind && depth < 64; depth++) {
        if (!strcmp(kind, "Exception")) return true;
        if (!strcmp(kind, "BaseException")) return false;
        const char *p = irc_exc_parent(kind);
        if (!p) return true;  /* unknown -> synthetic Exception subclass */
        kind = p;
    }
    return true;
}
static bool irc_exc_is_base(const char *kind) { return !irc_exc_is_exception(kind); }
/* except TimeoutError (== asyncio.TimeoutError on py3.14) catches the
 * TimeoutError family only (it is an OSError subclass). */
static bool irc_exc_is_timeout(const char *kind) {
    for (int depth = 0; kind && depth < 64; depth++) {
        if (!strcmp(kind, "TimeoutError")) return true;
        const char *p = irc_exc_parent(kind);
        if (!p) return false;
        kind = p;
    }
    return false;
}
static bool irc_exc_is_incomplete(const char *kind) {
    return strcmp(kind, "IncompleteReadError") == 0;
}
static bool irc_exc_is_cancelled(const char *kind) {
    return strcmp(kind, "CancelledError") == 0;
}

/* ── string helpers ── */
static char *irc_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
/* str.split()/strip() / re \s ASCII whitespace set (py3.14 includes 1c-1f). */
static bool irc_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'
        || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
/* int() strips a NARROWER set (NOT 1c-1f). */
static bool irc_is_int_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static bool irc_is_word(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}
static char *irc_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && irc_is_ws((unsigned char)s[a])) a++;
    while (b > a && irc_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}
static char *irc_lstrip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0;
    while (a < n && irc_is_ws((unsigned char)s[a])) a++;
    return irc_strdup(s + a);
}
/* rstrip with an explicit character set (e.g. "_0123456789-"). */
static char *irc_rstrip_set(const char *s, const char *set) {
    if (!s) s = "";
    size_t n = strlen(s);
    while (n > 0 && strchr(set, s[n - 1]) && s[n - 1] != '\0') n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}
static char *irc_lower(const char *s) {
    char *o = irc_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
/* replace all literal occurrences of needle in hay with repl. */
static char *irc_replace_all(const char *hay, const char *needle, const char *repl) {
    size_t nl = strlen(needle), rl = strlen(repl);
    if (nl == 0) return irc_strdup(hay);
    size_t cap = strlen(hay) + 1, len = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    const char *p = hay;
    while (*p) {
        if (strncmp(p, needle, nl) == 0) {
            while (len + rl + 1 > cap) { cap *= 2; o = realloc(o, cap); }
            memcpy(o + len, repl, rl); len += rl; p += nl;
        } else {
            if (len + 2 > cap) { cap *= 2; o = realloc(o, cap); }
            o[len++] = *p++;
        }
    }
    o[len] = '\0';
    return o;
}
/* codepoint-aware prefix slice (ASCII corpus: byte slice). */
static char *irc_slice(const char *s, size_t cap_cp) {
    size_t i = 0, cp = 0;
    while (s[i] && cp < cap_cp) {
        unsigned char c = (unsigned char)s[i];
        size_t adv = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        i += adv; cp++;
    }
    char *o = malloc(i + 1);
    if (o) { memcpy(o, s, i); o[i] = '\0'; }
    return o;
}
/* Python str() projection for scalar nodes (containers out of domain). */
static char *irc_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return irc_strdup("None");
    if (v->type == JSON_BOOL) return irc_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return irc_strdup(buf);
    }
    if (v->type == JSON_STRING) return irc_strdup(v->str_val ? v->str_val : "");
    return irc_strdup("");
}
/* Python repr() for the invalid-port message (None/bool/int/str corpus;
 * str is printable ASCII without quotes/backslashes). */
static char *irc_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return irc_strdup("None");
    if (v->type == JSON_BOOL) return irc_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return irc_strdup(buf);
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s);
        char *o = malloc(n + 3);
        if (o) { o[0] = '\''; memcpy(o + 1, s, n); o[n + 1] = '\''; o[n + 2] = '\0'; }
        return o;
    }
    return irc_strdup("");
}
/* literal case-insensitive prefix match. */
static bool irc_ieq_at(const char *hay, size_t pos, const char *needle) {
    size_t n = strlen(needle);
    if (strlen(hay + pos) < n) return false;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)hay[pos + i]) != tolower((unsigned char)needle[i])) return false;
    return true;
}
/* str.split() with no args: runs of ASCII whitespace, no empty tokens. */
static json_t *irc_split_ws(const char *s) {
    json_t *arr = json_array();
    if (!s) return arr;
    size_t i = 0, n = strlen(s);
    while (i < n) {
        while (i < n && irc_is_ws((unsigned char)s[i])) i++;
        if (i >= n) break;
        size_t start = i;
        while (i < n && !irc_is_ws((unsigned char)s[i])) i++;
        char *tok = malloc(i - start + 1);
        if (tok) { memcpy(tok, s + start, i - start); tok[i - start] = '\0'; }
        json_append(arr, json_string(tok ? tok : ""));
        free(tok);
    }
    return arr;
}
/* str.partition(sep): returns [before, sep, after] (sep "" when absent). */
static void irc_partition(const char *s, const char *sep, char **before, char **after) {
    const char *p = strstr(s, sep);
    if (!p) { *before = irc_strdup(s); *after = irc_strdup(""); return; }
    size_t bl = (size_t)(p - s);
    char *b = malloc(bl + 1);
    if (b) { memcpy(b, s, bl); b[bl] = '\0'; }
    *before = b;
    *after = irc_strdup(p + strlen(sep));
}
/* str.rfind(sub, 0, end): last index < end where sub starts, or -1. */
static long irc_rfind(const char *s, const char *sub, size_t end) {
    size_t sl = strlen(sub), n = strlen(s);
    if (end > n) end = n;
    if (sl == 0) return (long)end;
    if (sl > end) return -1;
    for (size_t i = end - sl + 1; i > 0; i--) {
        size_t j = i - 1;
        if (strncmp(s + j, sub, sl) == 0) return (long)j;
    }
    return -1;
}

/* ── dynamic string buffer ── */
typedef struct { char *p; size_t len, cap; } irc_sb_t;
static void irc_sb_init(irc_sb_t *b) { b->cap = 64; b->len = 0; b->p = malloc(b->cap); if (b->p) b->p[0] = '\0'; }
static void irc_sb_append(irc_sb_t *b, const char *s, size_t n) {
    if (!b->p) return;
    while (b->len + n + 1 > b->cap) { b->cap *= 2; b->p = realloc(b->p, b->cap); }
    memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = '\0';
}
static void irc_sb_puts(irc_sb_t *b, const char *s) { irc_sb_append(b, s, strlen(s)); }

/* ── int() coercion (port) ── */
static bool irc_int_of(const json_t *v, long long *out, char **errmsg) {
    *errmsg = NULL;
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return true; }
    if (v && v->type == JSON_NUMBER) { *out = (long long)v->num_val; return true; }
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && irc_is_int_ws((unsigned char)s[a])) a++;
        while (b > a && irc_is_int_ws((unsigned char)s[b - 1])) b--;
        size_t i = a;
        bool neg = false;
        if (i < b && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); i++; }
        if (i >= b) goto bad;
        long long val = 0;
        for (; i < b; i++) {
            if (s[i] < '0' || s[i] > '9') goto bad;
            val = val * 10 + (s[i] - '0');
        }
        *out = neg ? -val : val;
        return true;
    bad: {
            char *msg = malloc(n + 80);
            snprintf(msg, n + 80, "invalid literal for int() with base 10: '%s'", s);
            *errmsg = msg;
            return false;
        }
    }
    {
        char *msg = malloc(160);
        snprintf(msg, 160, "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 irc_type_name(v));
        *errmsg = msg;
        return false;
    }
}

/* self double attribute reader (borrowed). */
static const json_t *irc_self(const irc_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}
static void irc_set_self(irc_ctx_t *ctx, const char *key, json_t *v) {
    if (ctx->state) json_set(ctx->state, key, v);
}

/* ── _EOF sentinel ── */
static json_t *irc_eof(void) {
    json_t *e = json_object();
    irc_set_b(e, "__eof__", true);
    return e;
}
static bool irc_is_eof(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__eof__", false);
}

/* ================================================================
 * Pure protocol / text leaves
 * ================================================================ */

/* _parse_irc_message */
static json_t *irc_parse_irc_message_c(const char *raw_in) {
    char *raw = irc_strdup(raw_in ? raw_in : "");
    char *prefix = irc_strdup(""), *trailing = irc_strdup("");
    if (raw[0] == ':') {
        char *before, *after;
        irc_partition(raw + 1, " ", &before, &after);
        free(prefix); prefix = before;
        free(raw); raw = after;
    }
    char *sp = strstr(raw, " :");
    if (sp) {
        size_t bl = (size_t)(sp - raw);
        char *nr = malloc(bl + 1);
        if (nr) { memcpy(nr, raw, bl); nr[bl] = '\0'; }
        free(trailing); trailing = irc_strdup(sp + 2);
        free(raw); raw = nr;
    }
    json_t *parts = irc_split_ws(raw);
    size_t np = json_len(parts);
    json_t *params = json_array();
    for (size_t i = 1; i < np; i++) json_append(params, json_copy(json_get(parts, i)));
    if (trailing[0]) json_append(params, json_string(trailing));
    json_t *ret = json_object();
    irc_set_s(ret, "prefix", prefix);
    irc_set_s(ret, "command", np > 0 ? irc_str(json_get(parts, 0)) : "");
    json_set(ret, "params", params);
    json_free(parts);
    free(raw); free(prefix); free(trailing);
    return ret;
}
/* PoP: irc__parse_irc_message @ plugins/platforms/irc/adapter.py:_parse_irc_message */
static json_t *irc__parse_irc_message(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return irc_parse_irc_message_c(json_get_str(args, "raw", ""));
}

/* _extract_nick */
static char *irc_extract_nick_c(const char *prefix) {
    const char *p = prefix ? prefix : "";
    const char *bang = strchr(p, '!');
    size_t n = bang ? (size_t)(bang - p) : strlen(p);
    char *o = malloc(n + 1);
    if (o) { memcpy(o, p, n); o[n] = '\0'; }
    return o;
}
/* PoP: irc__extract_nick @ plugins/platforms/irc/adapter.py:_extract_nick */
static json_t *irc__extract_nick(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *n = irc_extract_nick_c(json_get_str(args, "prefix", ""));
    json_t *r = json_string(n ? n : "");
    free(n);
    return r;
}

/* _ms_id: str(int(time.time() * 1000)) */
static json_t *irc_ms_id_c(irc_ctx_t *ctx) {
    json_t *t = irc_call(ctx, "time.time", irc_args());
    IA_PROP(t);
    double secs = (t && t->type == JSON_NUMBER) ? t->num_val : 0.0;
    json_free(t);
    long long ms = (long long)(secs * 1000.0);
    char buf[40];
    snprintf(buf, sizeof buf, "%lld", ms);
    return json_string(buf);
}
/* PoP: irc__ms_id @ plugins/platforms/irc/adapter.py:_ms_id */
static json_t *irc__ms_id(irc_ctx_t *ctx, const json_t *args) {
    (void)args;
    return irc_ms_id_c(ctx);
}

/* _strip_irc_control_chars */
static char *irc_strip_control_c(const char *text) {
    char *a = irc_replace_all(text ? text : "", "\r", " ");
    char *b = irc_replace_all(a, "\n", " ");
    free(a);
    char *c = irc_replace_all(b, "\x00", "");
    free(b);
    return c;
}
/* PoP: irc__strip_irc_control_chars @ plugins/platforms/irc/adapter.py:_strip_irc_control_chars */
static json_t *irc__strip_irc_control_chars(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *s = irc_strip_control_c(json_get_str(args, "text", ""));
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* _is_irc_channel: bool(target) and target[0] in "#&+!" */
/* PoP: irc__is_irc_channel @ plugins/platforms/irc/adapter.py:_is_irc_channel */
static json_t *irc__is_irc_channel(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *t = json_obj_get(args, "target");
    if (!irc_truthy(t)) return json_bool(false);
    const char *s = irc_str(t);
    if (!s) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable", irc_type_name(t));
        return irc_err("TypeError", msg);
    }
    char c = s[0];
    return json_bool(c == '#' || c == '&' || c == '+' || c == '!');
}
static bool irc_is_channel_str(const char *s) {
    return s && s[0] && (s[0] == '#' || s[0] == '&' || s[0] == '+' || s[0] == '!');
}

/* _encode_line: (line + "\r\n").encode("utf-8") -> str projection */
static char *irc_encode_line_c(const char *line) {
    const char *l = line ? line : "";
    size_t n = strlen(l);
    char *o = malloc(n + 3);
    if (o) { memcpy(o, l, n); o[n] = '\r'; o[n + 1] = '\n'; o[n + 2] = '\0'; }
    return o;
}
/* PoP: irc__encode_line @ plugins/platforms/irc/adapter.py:_encode_line */
static json_t *irc__encode_line(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *s = irc_encode_line_c(json_get_str(args, "line", ""));
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* _privmsg_budget: 510 - (len(f"PRIVMSG {target} :".encode()) + 2) */
static long irc_privmsg_budget_c(const char *target) {
    const char *t = target ? target : "";
    char buf[512];
    int n = snprintf(buf, sizeof buf, "PRIVMSG %s :", t);
    if (n < 0) n = 0;
    return 510 - ((long)n + 2);
}
/* PoP: irc__privmsg_budget @ plugins/platforms/irc/adapter.py:_privmsg_budget */
static json_t *irc__privmsg_budget(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return json_number((double)irc_privmsg_budget_c(json_get_str(args, "target", "")));
}

/* _chunk_paragraph: UTF-8 byte-aware split preferring space boundaries.
 * ASCII corpus: byte len == codepoint len. limit >= 1 (terminating domain). */
static json_t *irc_chunk_paragraph_c(const char *paragraph, long limit) {
    json_t *chunks = json_array();
    char *para = irc_strdup(paragraph ? paragraph : "");
    while (para && para[0]) {
        long plen = (long)strlen(para);
        if (plen <= limit) {
            json_append(chunks, json_string(para));
            break;
        }
        long low = 1, high = plen, split_at = 0;
        while (low <= high) {
            long mid = (low + high) / 2;
            if (mid <= limit) { split_at = mid; low = mid + 1; }
            else high = mid - 1;
        }
        long space = irc_rfind(para, " ", (size_t)split_at);
        if (space > split_at / 3) split_at = space;
        char *head = irc_slice(para, (size_t)split_at);
        char *headr = irc_rstrip_set(head, " \t\n\r\f\v\x1c\x1d\x1e\x1f");
        free(head);
        json_append(chunks, json_string(headr ? headr : ""));
        free(headr);
        char *tail = irc_strdup(para + split_at);
        char *tailr = irc_lstrip(tail);
        free(tail);
        free(para);
        para = tailr;
    }
    free(para);
    return chunks;
}
/* PoP: irc__chunk_paragraph @ plugins/platforms/irc/adapter.py:_chunk_paragraph */
static json_t *irc__chunk_paragraph(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return irc_chunk_paragraph_c(json_get_str(args, "paragraph", ""),
                                (long)json_get_num(args, "limit", 0));
}

/* _split_lines: flatten _chunk_paragraph over paragraphs */
static json_t *irc_split_lines_c(const json_t *paragraphs, long limit) {
    json_t *out = json_array();
    if (paragraphs && paragraphs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(paragraphs); i++) {
            const json_t *p = json_get(paragraphs, i);
            json_t *ch = irc_chunk_paragraph_c(irc_str(p) ? irc_str(p) : "", limit);
            for (size_t j = 0; j < json_len(ch); j++) json_append(out, json_copy(json_get(ch, j)));
            json_free(ch);
        }
    }
    return out;
}
/* PoP: irc__split_lines @ plugins/platforms/irc/adapter.py:_split_lines */
static json_t *irc__split_lines(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return irc_split_lines_c(json_obj_get(args, "paragraphs"), (long)json_get_num(args, "limit", 0));
}

/* ================================================================
 * _strip_markdown — the eight _MARKDOWN_RULES as literal scanners
 * ================================================================ */

/* delim-pair rule: open capture(.+?) close -> capture. lb/la = \w lookarounds.
 * re.sub semantics: left-to-right, non-greedy, resume at match end; '.' does
 * not cross '\n'. */
static char *irc_md_pair(const char *s, const char *open, const char *close, bool lb, bool la) {
    size_t ol = strlen(open), cl = strlen(close), n = strlen(s);
    irc_sb_t out; irc_sb_init(&out);
    size_t i = 0;
    while (i < n) {
        bool matched = false;
        if (i + ol <= n && strncmp(s + i, open, ol) == 0) {
            if (!lb || i == 0 || !irc_is_word((unsigned char)s[i - 1])) {
                size_t j = i + ol;
                while (j + cl <= n) {
                    if (s[j] == '\n') break;
                    if (j > i + ol && strncmp(s + j, close, cl) == 0) {
                        if (!la || j + cl == n || !irc_is_word((unsigned char)s[j + cl])) {
                            irc_sb_append(&out, s + i + ol, j - (i + ol));
                            i = j + cl; matched = true; break;
                        }
                    }
                    j++;
                }
            }
        }
        if (!matched) { irc_sb_append(&out, s + i, 1); i++; }
    }
    return out.p ? out.p : irc_strdup("");
}
/* code-fence rule: ```\w*\n? -> "" */
static char *irc_md_fence(const char *s) {
    size_t n = strlen(s), i = 0;
    irc_sb_t out; irc_sb_init(&out);
    while (i < n) {
        if (i + 3 <= n && s[i] == '`' && s[i + 1] == '`' && s[i + 2] == '`') {
            size_t j = i + 3;
            while (j < n && irc_is_word((unsigned char)s[j])) j++;
            if (j < n && s[j] == '\n') j++;
            i = j;
        } else {
            irc_sb_append(&out, s + i, 1); i++;
        }
    }
    return out.p ? out.p : irc_strdup("");
}
/* image/link rule. image: !\[([^\]]*)\]\(([^)]+)\) -> \2 ;
 * link: \[([^\]]+)\]\(([^)]+)\) -> \1 (\2). [^\]]* / [^)]+ cross '\n'. */
static char *irc_md_link(const char *s, bool image) {
    size_t n = strlen(s), i = 0;
    irc_sb_t out; irc_sb_init(&out);
    while (i < n) {
        bool ok = false;
        size_t inner;
        bool prefix = image ? (s[i] == '!' && i + 1 < n && s[i + 1] == '[')
                            : (s[i] == '[');
        if (prefix) {
            inner = image ? i + 2 : i + 1;
            size_t j = inner;
            while (j < n && s[j] != ']') j++;
            if (j < n && j + 1 < n && s[j + 1] == '(' && (image || j > inner)) {
                size_t k = j + 2;
                while (k < n && s[k] != ')') k++;
                if (k < n && k > j + 2) {
                    if (image) {
                        irc_sb_append(&out, s + j + 2, k - (j + 2));
                    } else {
                        irc_sb_append(&out, s + inner, j - inner);
                        irc_sb_puts(&out, " (");
                        irc_sb_append(&out, s + j + 2, k - (j + 2));
                        irc_sb_puts(&out, ")");
                    }
                    i = k + 1; ok = true;
                }
            }
        }
        if (!ok) { irc_sb_append(&out, s + i, 1); i++; }
    }
    return out.p ? out.p : irc_strdup("");
}
static char *irc_strip_markdown_c(const char *text) {
    char *t = irc_strdup(text ? text : "");
    char *n;
    n = irc_md_pair(t, "**", "**", false, false); free(t); t = n;
    n = irc_md_pair(t, "__", "__", false, false); free(t); t = n;
    n = irc_md_pair(t, "*", "*", false, false); free(t); t = n;
    n = irc_md_pair(t, "_", "_", true, true); free(t); t = n;
    n = irc_md_pair(t, "`", "`", false, false); free(t); t = n;
    n = irc_md_fence(t); free(t); t = n;
    n = irc_md_link(t, true); free(t); t = n;
    n = irc_md_link(t, false); free(t); t = n;
    return t;
}
/* PoP: irc__strip_markdown @ plugins/platforms/irc/adapter.py:_strip_markdown */
static json_t *irc__strip_markdown(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *s = irc_strip_markdown_c(json_get_str(args, "text", ""));
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* ================================================================
 * Env / config resolution leaves
 * ================================================================ */

/* _get_scoped_secret seam: one-arg call records default null. */
static json_t *irc_scoped_secret(irc_ctx_t *ctx, const char *name, const json_t *deflt) {
    json_t *a = irc_args();
    irc_set_s(a, "name", name);
    irc_set_copy(a, "default", deflt);
    return irc_call(ctx, "_get_scoped_secret", a);
}
/* _env_or_extra: _get_scoped_secret(env) or extra.get(key, default) */
static json_t *irc_env_or_extra_c(irc_ctx_t *ctx, const json_t *extra, const char *env,
                                 const char *key, const json_t *deflt) {
    json_t *sec = irc_scoped_secret(ctx, env, NULL);
    IA_PROP(sec);
    if (irc_truthy(sec)) return sec;
    json_free(sec);
    if (!extra || extra->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", irc_type_name(extra));
        return irc_err("AttributeError", msg);
    }
    json_t *v = json_obj_get(extra, key);
    if (v) return json_copy(v);
    return deflt ? json_copy(deflt) : json_null();
}
/* PoP: irc__env_or_extra @ plugins/platforms/irc/adapter.py:_env_or_extra */
static json_t *irc__env_or_extra(irc_ctx_t *ctx, const json_t *args) {
    const json_t *deflt = json_obj_get(args, "default");
    return irc_env_or_extra_c(ctx, json_obj_get(args, "extra"),
                             json_get_str(args, "env", ""), json_get_str(args, "key", ""),
                             deflt ? deflt : json_null());
}
/* _server_channel: extra = getattr(config,"extra",{}) or {}; tuple of two. */
static json_t *irc_server_channel_c(irc_ctx_t *ctx, const json_t *config, json_t **extra_out) {
    json_t *extra = NULL;
    if (config && config->type == JSON_OBJECT) {
        json_t *e = json_obj_get(config, "extra");
        if (irc_truthy(e)) extra = json_copy(e);
    }
    if (!extra) extra = json_object();
    if (extra_out) *extra_out = json_copy(extra);
    /* _server_channel calls _env_or_extra WITHOUT a default arg, so the
     * function default "" applies (not None). */
    json_t *def = json_string("");
    json_t *server = irc_env_or_extra_c(ctx, extra, "IRC_SERVER", "server", def);
    if (irc_is_raise(server)) { json_free(def); json_free(extra); return server; }
    json_t *channel = irc_env_or_extra_c(ctx, extra, "IRC_CHANNEL", "channel", def);
    json_free(def);
    if (irc_is_raise(channel)) { json_free(extra); json_free(server); return channel; }
    json_t *ret = json_array();
    json_append(ret, server);
    json_append(ret, channel);
    json_free(extra);
    return ret;
}
/* PoP: irc__server_channel @ plugins/platforms/irc/adapter.py:_server_channel */
static json_t *irc__server_channel(irc_ctx_t *ctx, const json_t *args) {
    return irc_server_channel_c(ctx, json_obj_get(args, "config"), NULL);
}
/* check_requirements: bool(secret(IRC_SERVER,"") and secret(IRC_CHANNEL,"")) */
/* PoP: irc_check_requirements @ plugins/platforms/irc/adapter.py:check_requirements */
static json_t *irc_check_requirements(irc_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *empty = json_string("");
    json_t *server = irc_scoped_secret(ctx, "IRC_SERVER", empty);
    json_free(empty);
    IA_PROP(server);
    if (!irc_truthy(server)) { json_free(server); return json_bool(false); }
    json_free(server);
    json_t *empty2 = json_string("");
    json_t *channel = irc_scoped_secret(ctx, "IRC_CHANNEL", empty2);
    json_free(empty2);
    IA_PROP(channel);
    bool ok = irc_truthy(channel);
    json_free(channel);
    return json_bool(ok);
}
/* validate_config: bool(server and channel) */
static json_t *irc_validate_config_c(irc_ctx_t *ctx, const json_t *config) {
    json_t *sc = irc_server_channel_c(ctx, config, NULL);
    IA_PROP(sc);
    bool ok = irc_truthy(json_get(sc, 0)) && irc_truthy(json_get(sc, 1));
    json_free(sc);
    return json_bool(ok);
}
/* PoP: irc_validate_config @ plugins/platforms/irc/adapter.py:validate_config */
static json_t *irc_validate_config(irc_ctx_t *ctx, const json_t *args) {
    return irc_validate_config_c(ctx, json_obj_get(args, "config"));
}
/* is_connected: validate_config(config) */
/* PoP: irc_is_connected @ plugins/platforms/irc/adapter.py:is_connected */
static json_t *irc_is_connected(irc_ctx_t *ctx, const json_t *args) {
    return irc_validate_config_c(ctx, json_obj_get(args, "config"));
}
/* _ssl_ctx: ssl.create_default_context() if use_tls else None */
static json_t *irc_ssl_ctx_c(irc_ctx_t *ctx, bool use_tls) {
    if (!use_tls) return json_null();
    return irc_call(ctx, "ssl.create_default_context", irc_args());
}
/* PoP: irc__ssl_ctx @ plugins/platforms/irc/adapter.py:_ssl_ctx */
static json_t *irc__ssl_ctx(irc_ctx_t *ctx, const json_t *args) {
    return irc_ssl_ctx_c(ctx, irc_truthy(json_obj_get(args, "use_tls")));
}
/* _sa_error: send_error(f"IRC standalone send: {detail}") */
static json_t *irc_sa_error_c(irc_ctx_t *ctx, const char *detail) {
    char buf[512];
    snprintf(buf, sizeof buf, "IRC standalone send: %s", detail ? detail : "");
    json_t *a = irc_args();
    irc_set_s(a, "message", buf);
    return irc_call(ctx, "send_error", a);
}
/* PoP: irc__sa_error @ plugins/platforms/irc/adapter.py:_sa_error */
static json_t *irc__sa_error(irc_ctx_t *ctx, const json_t *args) {
    return irc_sa_error_c(ctx, json_get_str(args, "detail", ""));
}
/* _env_enablement */
/* PoP: irc__env_enablement @ plugins/platforms/irc/adapter.py:_env_enablement */
static json_t *irc__env_enablement(irc_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *empty = json_string("");
    json_t *srv_raw = irc_scoped_secret(ctx, "IRC_SERVER", empty);
    json_free(empty);
    IA_PROP(srv_raw);
    const char *srv_s = irc_str(srv_raw);
    if (!srv_s) {
        json_t *e = irc_attr_err(irc_type_name(srv_raw), "strip");
        json_free(srv_raw); return e;
    }
    char *server = irc_strip(srv_s);
    json_free(srv_raw);
    json_t *empty2 = json_string("");
    json_t *ch_raw = irc_scoped_secret(ctx, "IRC_CHANNEL", empty2);
    json_free(empty2);
    IA_PROP(ch_raw);
    const char *ch_s = irc_str(ch_raw);
    if (!ch_s) {
        json_t *e = irc_attr_err(irc_type_name(ch_raw), "strip");
        json_free(ch_raw); free(server); return e;
    }
    char *channel = irc_strip(ch_s);
    json_free(ch_raw);
    if (!server[0] || !channel[0]) { free(server); free(channel); return json_null(); }
    /* _seed_extra_from_env seam */
    json_t *a = irc_args();
    json_t *spec = json_array();
    const char *rows[][3] = {
        {"IRC_PORT", "port", "int"}, {"IRC_NICKNAME", "nickname", NULL},
        {"IRC_USE_TLS", "use_tls", "truthy"}, {"IRC_SERVER_PASSWORD", "server_password", NULL},
        {"IRC_NICKSERV_PASSWORD", "nickserv_password", NULL},
    };
    for (size_t i = 0; i < 5; i++) {
        json_t *row = json_array();
        json_append(row, json_string(rows[i][0]));
        json_append(row, json_string(rows[i][1]));
        json_append(row, rows[i][2] ? json_string(rows[i][2]) : json_null());
        json_append(spec, row);
    }
    json_set(a, "spec", spec);
    irc_set_s(a, "home_env", "IRC_HOME_CHANNEL");
    irc_set_s(a, "home_default", channel);
    json_t *seed = irc_call(ctx, "_seed_extra_from_env", a);
    IA_PROP(seed);
    if (!seed || seed->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not a mapping", irc_type_name(seed));
        json_free(seed); free(server); free(channel);
        return irc_err("TypeError", msg);
    }
    json_t *ret = json_object();
    irc_set_s(ret, "server", server);
    irc_set_s(ret, "channel", channel);
    for (size_t i = 0; i < seed->c.count; i++)
        json_set(ret, seed->c.keys[i], json_copy(seed->c.items[i]));
    json_free(seed); free(server); free(channel);
    return ret;
}

/* ================================================================
 * SendResult / MessageEvent projections
 * ================================================================ */
static json_t *irc_send_result(bool success, const char *message_id, const char *error) {
    json_t *r = json_object();
    irc_set_b(r, "success", success);
    if (message_id) irc_set_s(r, "message_id", message_id); else irc_set_null(r, "message_id");
    if (error) irc_set_s(r, "error", error); else irc_set_null(r, "error");
    irc_set_null(r, "raw_response");
    irc_set_b(r, "retryable", false);
    irc_set_null(r, "retry_after");
    json_set(r, "continuation_message_ids", json_array());
    irc_set_null(r, "error_kind");
    return r;
}
static json_t *irc_message_event(const char *text, const json_t *source,
                                const char *message_id, const json_t *timestamp) {
    json_t *r = json_object();
    irc_set_s(r, "text", text);
    irc_set_s(r, "message_type", "text");
    irc_set_null(r, "user_id");
    irc_set_null(r, "user_name");
    irc_set_copy(r, "source", source);
    irc_set_null(r, "raw_message");
    if (message_id) irc_set_s(r, "message_id", message_id); else irc_set_null(r, "message_id");
    irc_set_null(r, "ledger_message_id");
    irc_set_null(r, "reply_anchor_override");
    irc_set_null(r, "platform_update_id");
    json_set(r, "media_urls", json_array());
    json_set(r, "media_types", json_array());
    json_set(r, "media_text_inlined", json_array());
    irc_set_null(r, "reply_to_message_id");
    irc_set_null(r, "reply_to_text");
    irc_set_null(r, "reply_to_author_id");
    irc_set_null(r, "reply_to_author_name");
    irc_set_b(r, "reply_to_is_own_message", false);
    irc_set_null(r, "prompt_response");
    irc_set_null(r, "auto_skill");
    irc_set_null(r, "channel_prompt");
    irc_set_null(r, "channel_context");
    irc_set_b(r, "internal", false);
    json_set(r, "metadata", json_object());
    irc_set_copy(r, "timestamp", timestamp);
    irc_set_b(r, "allow_gateway_control", true);
    irc_set_null(r, "reply_expected");
    irc_set_b(r, "inherit_channel_pin", false);
    irc_set_null(r, "edit_revision");
    json_set(r, "media_filenames", json_array());
    irc_set_b(r, "_gateway_accepted", false);
    irc_set_b(r, "_admitted_before_drain", false);
    irc_set_null(r, "_notification_reply_muted");
    return r;
}

/* ================================================================
 * IRCAdapter methods
 * ================================================================ */

/* name property -> "IRC" */
/* PoP: irc_name @ plugins/platforms/irc/adapter.py:name */
static json_t *irc_name(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx; (void)args;
    return json_string("IRC");
}
/* send_typing: no-op */
/* PoP: irc_send_typing @ plugins/platforms/irc/adapter.py:send_typing */
static json_t *irc_send_typing(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx; (void)args;
    return json_null();
}
/* get_chat_info */
/* PoP: irc_get_chat_info @ plugins/platforms/irc/adapter.py:get_chat_info */
static json_t *irc_get_chat_info(irc_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *cid = json_obj_get(args, "chat_id");
    const char *s = irc_str(cid);
    if (!s) return irc_attr_err(irc_type_name(cid), "startswith");
    bool group = (s[0] == '#' || s[0] == '&');
    json_t *r = json_object();
    irc_set_s(r, "name", s);
    irc_set_s(r, "type", group ? "group" : "dm");
    return r;
}
/* _fail: self._set_fatal_error(code, message, retryable=retryable); return False */
/* PoP: irc__fail @ plugins/platforms/irc/adapter.py:_fail */
static json_t *irc__fail(irc_ctx_t *ctx, const json_t *args) {
    json_t *a = irc_args();
    irc_set_s(a, "code", json_get_str(args, "code", ""));
    irc_set_s(a, "message", json_get_str(args, "message", ""));
    irc_set_b(a, "retryable", json_get_bool(args, "retryable", false));
    json_t *r = irc_call(ctx, "_set_fatal_error", a);
    IA_PROP(r);
    json_free(r);
    return json_bool(false);
}
/* _send_raw: guard writer, write(_encode_line(line)), drain. */
static json_t *irc_send_raw_c(irc_ctx_t *ctx, const char *line) {
    const json_t *w = irc_self(ctx, "_writer");
    if (!w) return irc_attr_err("IRCAdapter", "_writer");
    if (irc_is_none(w)) return json_null();
    json_t *ic = irc_call(ctx, "writer.is_closing", irc_args());
    IA_PROP(ic);
    bool closing = irc_truthy(ic);
    json_free(ic);
    if (closing) return json_null();
    char *enc = irc_encode_line_c(line);
    json_t *a = irc_args();
    irc_set_s(a, "data", enc ? enc : "");
    free(enc);
    json_t *wr = irc_call(ctx, "writer.write", a);
    IA_PROP(wr);
    json_free(wr);
    json_t *dr = irc_call(ctx, "writer.drain", irc_args());
    IA_PROP(dr);
    json_free(dr);
    return json_null();
}
/* PoP: irc__send_raw @ plugins/platforms/irc/adapter.py:_send_raw */
static json_t *irc__send_raw(irc_ctx_t *ctx, const json_t *args) {
    return irc_send_raw_c(ctx, json_get_str(args, "line", ""));
}
/* _split_message */
static json_t *irc_split_message_c(irc_ctx_t *ctx, const char *content, const char *target) {
    const json_t *mml = irc_self(ctx, "max_message_length");
    if (!mml) return irc_attr_err("IRCAdapter", "max_message_length");
    long max_msg = (long)json_get_num(mml, 0, 0);
    char *plain = irc_strip_markdown_c(content);
    /* paragraphs = [p for p in plain.split("\n") if p.strip()] */
    json_t *paragraphs = json_array();
    char *dup = irc_strdup(plain);
    free(plain);
    char *start = dup;
    while (start) {
        char *nl = strchr(start, '\n');
        if (nl) *nl = '\0';
        char *st = irc_strip(start);
        if (st && st[0]) json_append(paragraphs, json_string(start));
        free(st);
        start = nl ? nl + 1 : NULL;
    }
    free(dup);
    long budget = irc_privmsg_budget_c(target);
    long limit = max_msg < budget ? max_msg : budget;
    json_t *lines = irc_split_lines_c(paragraphs, limit);
    json_free(paragraphs);
    if (json_len(lines) == 0) {
        json_free(lines);
        lines = json_array();
        json_append(lines, json_string(""));
    }
    return lines;
}
/* PoP: irc__split_message @ plugins/platforms/irc/adapter.py:_split_message */
static json_t *irc__split_message(irc_ctx_t *ctx, const json_t *args) {
    return irc_split_message_c(ctx, json_get_str(args, "content", ""), json_get_str(args, "target", ""));
}

/* _dispatch_message */
static json_t *irc_dispatch_message_c(irc_ctx_t *ctx, const char *text, const char *chat_id,
                                     const char *chat_type, const char *user_id, const char *user_name) {
    const json_t *h = irc_self(ctx, "_message_handler");
    if (!h) return irc_attr_err("IRCAdapter", "_message_handler");
    if (!irc_truthy(h)) return json_null();
    json_t *a = irc_args();
    irc_set_s(a, "chat_id", chat_id);
    irc_set_s(a, "chat_name", chat_id);
    irc_set_s(a, "chat_type", chat_type);
    irc_set_s(a, "user_id", user_id);
    irc_set_s(a, "user_name", user_name);
    json_t *source = irc_call(ctx, "build_source", a);
    IA_PROP(source);
    json_t *uuid = irc_call(ctx, "uuid.uuid4", irc_args());
    IA_PROP(uuid);
    const json_t *hexn = (uuid && uuid->type == JSON_OBJECT) ? json_obj_get(uuid, "hex") : NULL;
    if (!hexn) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'hex'", irc_type_name(uuid));
        json_free(uuid); json_free(source);
        return irc_err("AttributeError", msg);
    }
    char *mid = irc_strdup(irc_str(hexn) ? irc_str(hexn) : "");
    json_free(uuid);
    json_t *ts = irc_call(ctx, "datetime.now", irc_args());
    if (irc_is_raise(ts)) { free(mid); json_free(source); return ts; }
    json_t *event = irc_message_event(text, source, mid, ts);
    free(mid); json_free(source); json_free(ts);
    json_t *ha = irc_args();
    json_set(ha, "event", event);
    json_t *hr = irc_call(ctx, "handle_message", ha);
    IA_PROP(hr);
    json_free(hr);
    return json_null();
}
/* PoP: irc__dispatch_message @ plugins/platforms/irc/adapter.py:_dispatch_message */
static json_t *irc__dispatch_message(irc_ctx_t *ctx, const json_t *args) {
    return irc_dispatch_message_c(ctx, json_get_str(args, "text", ""), json_get_str(args, "chat_id", ""),
                                 json_get_str(args, "chat_type", ""), json_get_str(args, "user_id", ""),
                                 json_get_str(args, "user_name", ""));
}

/* _handle_privmsg */
static json_t *irc_handle_privmsg_c(irc_ctx_t *ctx, const char *sender_nick, const char *target,
                                   const char *text_in) {
    const json_t *cn = irc_self(ctx, "_current_nick");
    if (!cn) return irc_attr_err("IRCAdapter", "_current_nick");
    const json_t *allowed = irc_self(ctx, "_allowed_users_lower");
    if (!allowed) return irc_attr_err("IRCAdapter", "_allowed_users_lower");
    char *cur = irc_strdup(irc_str(cn) ? irc_str(cn) : "");
    char *sender_l = irc_lower(sender_nick);
    char *cur_l = irc_lower(cur);
    if (strcmp(sender_l, cur_l) == 0) { free(cur); free(sender_l); free(cur_l); return json_null(); }
    free(cur_l);
    char *text = irc_strdup(text_in ? text_in : "");
    size_t tl = strlen(text);
    if (tl >= 9 && strncmp(text, "\x01" "ACTION ", 8) == 0 && text[tl - 1] == '\x01') {
        char *body = irc_slice(text + 8, tl - 9);
        char *nt = malloc(strlen(sender_nick) + strlen(body) + 4);
        sprintf(nt, "* %s %s", sender_nick, body);
        free(body); free(text); text = nt;
    }
    if (text[0] == '\x01') { free(cur); free(sender_l); free(text); return json_null(); }
    bool is_channel = (target && (target[0] == '#' || target[0] == '&'));
    if (is_channel) {
        char *text_l = irc_lower(text);
        bool addressed = false;
        const char *sufs[] = {":", ",", " "};
        for (int i = 0; i < 3; i++) {
            char *pref = malloc(strlen(cur) + 2);
            sprintf(pref, "%s%s", cur, sufs[i]);
            char *pref_l = irc_lower(pref);
            if (strncmp(text_l, pref_l, strlen(pref_l)) == 0) {
                char *rest = irc_strdup(text + strlen(pref));
                char *rests = irc_strip(rest);
                free(rest); free(text); text = rests;
                addressed = true;
                free(pref); free(pref_l);
                break;
            }
            free(pref); free(pref_l);
        }
        free(text_l);
        if (!addressed) { free(cur); free(sender_l); free(text); return json_null(); }
    }
    /* allowed_users check: non-empty set and sender not in it */
    bool has_allowed = allowed->type == JSON_ARRAY && json_len(allowed) > 0;
    if (has_allowed) {
        bool found = false;
        for (size_t i = 0; i < json_len(allowed); i++) {
            const char *u = irc_str(json_get(allowed, i));
            if (u && strcmp(u, sender_l) == 0) { found = true; break; }
        }
        if (!found) {
            json_t *la = irc_args();
            irc_set_s(la, "fmt", "IRC: ignoring message from unauthorized user %s");
            json_t *aa = json_array();
            json_append(aa, json_string(sender_nick));
            json_set(la, "args", aa);
            json_t *lr = irc_call(ctx, "logger.debug", la);
            IA_PROP(lr);
            json_free(lr);
            free(cur); free(sender_l); free(text);
            return json_null();
        }
    }
    const char *chat_id = is_channel ? target : sender_nick;
    const char *chat_type = is_channel ? "group" : "dm";
    json_t *r = irc_dispatch_message_c(ctx, text, chat_id, chat_type, sender_nick, sender_nick);
    free(cur); free(sender_l); free(text);
    return r;
}
/* PoP: irc__handle_privmsg @ plugins/platforms/irc/adapter.py:_handle_privmsg */
static json_t *irc__handle_privmsg(irc_ctx_t *ctx, const json_t *args) {
    return irc_handle_privmsg_c(ctx, json_get_str(args, "sender_nick", ""),
                               json_get_str(args, "target", ""), json_get_str(args, "text", ""));
}

/* re.search(r"_(\d+)$", nick): leftmost '_' + digits at end (or before final \n).
 * Returns the digit suffix (malloc'd) or NULL. */
static char *irc_nick_suffix(const char *nick) {
    size_t n = strlen(nick);
    size_t end = n;
    if (n > 0 && nick[n - 1] == '\n') end = n - 1;
    for (size_t i = 0; i < end; i++) {
        if (nick[i] != '_') continue;
        size_t j = i + 1;
        while (j < end && nick[j] >= '0' && nick[j] <= '9') j++;
        if (j > i + 1 && j == end) {
            char *o = malloc(j - i);
            if (o) { memcpy(o, nick + i + 1, j - i - 1); o[j - i - 1] = '\0'; }
            return o;
        }
    }
    return NULL;
}
/* _handle_line */
static json_t *irc_handle_line_c(irc_ctx_t *ctx, const char *raw) {
    json_t *msg = irc_parse_irc_message_c(raw);
    const char *command = json_get_str(msg, "command", "");
    json_t *params = json_obj_get(msg, "params");
    size_t np = params ? json_len(params) : 0;
    json_t *ret = json_null();
    if (strcmp(command, "PING") == 0) {
        const char *p0 = np > 0 ? irc_str(json_get(params, 0)) : "";
        char buf[512];
        snprintf(buf, sizeof buf, "PONG :%s", p0 ? p0 : "");
        ret = irc_send_raw_c(ctx, buf);
    } else if (strcmp(command, "001") == 0) {
        irc_set_self(ctx, "_registered", json_bool(true));
        const json_t *ev = irc_self(ctx, "_registration_event");
        if (!ev) { json_free(msg); return irc_attr_err("IRCAdapter", "_registration_event"); }
        if (!irc_is_none(ev)) {
            json_t *sr = irc_call(ctx, "registration_event.set", irc_args());
            if (irc_is_raise(sr)) { json_free(msg); return sr; }
            json_free(sr);
        }
        if (np > 0) {
            const char *p0 = irc_str(json_get(params, 0));
            irc_set_self(ctx, "_current_nick", json_string(p0 ? p0 : ""));
        }
    } else if (strcmp(command, "433") == 0) {
        const json_t *cn = irc_self(ctx, "_current_nick");
        if (!cn) { json_free(msg); return irc_attr_err("IRCAdapter", "_current_nick"); }
        const json_t *nk = irc_self(ctx, "nickname");
        if (!nk) { json_free(msg); return irc_attr_err("IRCAdapter", "nickname"); }
        char *cur = irc_strdup(irc_str(cn) ? irc_str(cn) : "");
        char *base = irc_strdup(irc_str(nk) ? irc_str(nk) : "");
        char *suf = irc_nick_suffix(cur);
        char *newnick;
        if (suf) {
            char *stripped = irc_rstrip_set(base, "_0123456789");
            long long v = strtoll(suf, NULL, 10) + 1;
            newnick = malloc(strlen(stripped) + 32);
            sprintf(newnick, "%s_%lld", stripped, v);
            free(stripped); free(suf);
        } else {
            const char *tail = (strcmp(cur, base) == 0) ? "_" : "_1";
            newnick = malloc(strlen(base) + 4);
            sprintf(newnick, "%s%s", base, tail);
        }
        irc_set_self(ctx, "_current_nick", json_string(newnick));
        char buf[512];
        snprintf(buf, sizeof buf, "NICK %s", newnick);
        free(newnick); free(cur); free(base);
        ret = irc_send_raw_c(ctx, buf);
    } else if (strcmp(command, "PRIVMSG") == 0 && np >= 2) {
        char *nick = irc_extract_nick_c(json_get_str(msg, "prefix", ""));
        const char *p0 = irc_str(json_get(params, 0));
        const char *p1 = irc_str(json_get(params, 1));
        ret = irc_handle_privmsg_c(ctx, nick ? nick : "", p0 ? p0 : "", p1 ? p1 : "");
        free(nick);
    } else if (strcmp(command, "NICK") == 0 && np > 0) {
        const json_t *cn = irc_self(ctx, "_current_nick");
        if (!cn) { json_free(msg); return irc_attr_err("IRCAdapter", "_current_nick"); }
        char *prefix_nick = irc_extract_nick_c(json_get_str(msg, "prefix", ""));
        char *pn_l = irc_lower(prefix_nick);
        char *cur_l = irc_lower(irc_str(cn) ? irc_str(cn) : "");
        if (strcmp(pn_l, cur_l) == 0) {
            const char *p0 = irc_str(json_get(params, 0));
            irc_set_self(ctx, "_current_nick", json_string(p0 ? p0 : ""));
        }
        free(prefix_nick); free(pn_l); free(cur_l);
    }
    json_free(msg);
    return ret;
}
/* PoP: irc__handle_line @ plugins/platforms/irc/adapter.py:_handle_line */
static json_t *irc__handle_line(irc_ctx_t *ctx, const json_t *args) {
    return irc_handle_line_c(ctx, json_get_str(args, "raw", ""));
}

/* send */
/* PoP: irc_send @ plugins/platforms/irc/adapter.py:send */
static json_t *irc_send(irc_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *content = json_get_str(args, "content", "");
    const json_t *w = irc_self(ctx, "_writer");
    if (!w) return irc_attr_err("IRCAdapter", "_writer");
    bool closed = irc_is_none(w);
    if (!closed) {
        json_t *ic = irc_call(ctx, "writer.is_closing", irc_args());
        IA_PROP(ic);
        closed = irc_truthy(ic);
        json_free(ic);
    }
    if (closed) return irc_send_result(false, NULL, "Not connected");
    json_t *lines = irc_split_message_c(ctx, content, chat_id);
    IA_PROP(lines);
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *line = irc_str(json_get(lines, i));
        char buf[1024];
        snprintf(buf, sizeof buf, "PRIVMSG %s :%s", chat_id, line ? line : "");
        json_t *sr = irc_send_raw_c(ctx, buf);
        if (irc_is_raise(sr)) {
            if (irc_exc_is_base(irc_kind(sr))) { json_free(lines); return sr; }
            json_t *r = irc_send_result(false, NULL, irc_msg(sr));
            json_free(sr); json_free(lines);
            return r;
        }
        json_free(sr);
        json_t *sa = irc_args();
        irc_set_n(sa, "seconds", 0.3);
        json_t *sl = irc_call(ctx, "asyncio.sleep", sa);
        if (irc_is_raise(sl)) {
            if (irc_exc_is_base(irc_kind(sl))) { json_free(lines); return sl; }
            json_t *r = irc_send_result(false, NULL, irc_msg(sl));
            json_free(sl); json_free(lines);
            return r;
        }
        json_free(sl);
    }
    json_free(lines);
    json_t *mid = irc_ms_id_c(ctx);
    IA_PROP(mid);
    json_t *r = irc_send_result(true, irc_str(mid), NULL);
    json_free(mid);
    return r;
}

/* _receive_loop */
/* PoP: irc__receive_loop @ plugins/platforms/irc/adapter.py:_receive_loop */
static json_t *irc__receive_loop(irc_ctx_t *ctx, const json_t *args) {
    (void)args;
    /* buffer = b"" (ASCII str projection) */
    char *buffer = irc_strdup("");
    json_t *result = json_null();   /* value-or-raise outcome of the try body */
    bool cancelled = false;
    /* try body */
    for (;;) {
        const json_t *r = irc_self(ctx, "_reader");
        if (!r) { result = irc_attr_err("IRCAdapter", "_reader"); break; }
        if (irc_is_none(r)) break;   /* while self._reader falsy -> exit loop */
        json_t *eof = irc_call(ctx, "reader.at_eof", irc_args());
        if (irc_is_raise(eof)) { result = eof; break; }
        bool at_eof = irc_truthy(eof);
        json_free(eof);
        if (at_eof) break;
        json_t *ra = irc_args();
        irc_set_n(ra, "n", 4096);
        json_t *data = irc_call(ctx, "reader.read", ra);
        if (irc_is_raise(data)) { result = data; break; }
        const char *ds = irc_str(data);
        if (!ds || !ds[0]) { json_free(data); break; }   /* if not data: break */
        char *nb = malloc(strlen(buffer) + strlen(ds) + 1);
        sprintf(nb, "%s%s", buffer, ds);
        free(buffer); buffer = nb;
        json_free(data);
        /* while "\r\n" in buffer */
        for (;;) {
            char *crlf = strstr(buffer, "\r\n");
            if (!crlf) break;
            size_t ll = (size_t)(crlf - buffer);
            char *line = malloc(ll + 1);
            memcpy(line, buffer, ll); line[ll] = '\0';
            char *rest = irc_strdup(crlf + 2);
            free(buffer); buffer = rest;
            json_t *hr = irc_handle_line_c(ctx, line);
            free(line);
            if (irc_is_raise(hr)) {
                if (irc_exc_is_base(irc_kind(hr))) { result = hr; cancelled = irc_exc_is_cancelled(irc_kind(hr)); goto body_done; }
                /* except Exception: logger.warning("IRC: error handling line: %s", e) */
                json_t *la = irc_args();
                irc_set_s(la, "fmt", "IRC: error handling line: %s");
                json_t *aa = json_array();
                json_append(aa, json_string(irc_msg(hr)));
                json_set(la, "args", aa);
                json_free(hr);
                json_t *lr = irc_call(ctx, "logger.warning", la);
                if (irc_is_raise(lr)) { result = lr; goto body_done; }
                json_free(lr);
            } else {
                json_free(hr);
            }
        }
    }
body_done:
    free(buffer);
    /* except asyncio.CancelledError: raise ; except Exception: logger.error */
    if (irc_is_raise(result)) {
        if (cancelled || irc_exc_is_cancelled(irc_kind(result))) {
            /* re-raise; finally still runs */
        } else if (irc_exc_is_exception(irc_kind(result))) {
            json_t *la = irc_args();
            irc_set_s(la, "fmt", "IRC: receive loop error: %s");
            json_t *aa = json_array();
            json_append(aa, json_string(irc_msg(result)));
            json_set(la, "args", aa);
            json_t *lr = irc_call(ctx, "logger.error", la);
            json_free(result);
            if (irc_is_raise(lr)) result = lr; else { json_free(lr); result = json_null(); }
        }
        /* BaseException (non-cancelled) propagates unchanged */
    }
    /* finally: if self.is_connected: warn + _set_fatal_error + _notify_fatal_error.
     * The finally block has NO suppress: any raise from these seams propagates
     * and OVERRIDES the body outcome, aborting the remaining finally steps. */
    const json_t *conn = irc_self(ctx, "is_connected");
    if (!conn) {
        json_free(result);
        return irc_attr_err("IRCAdapter", "is_connected");
    }
    if (irc_truthy(conn)) {
        json_t *la = irc_args();
        irc_set_s(la, "fmt", "IRC: connection lost, marking disconnected");
        json_set(la, "args", json_array());
        json_t *lr = irc_call(ctx, "logger.warning", la);
        if (irc_is_raise(lr)) { json_free(result); return lr; }
        json_free(lr);
        json_t *fa = irc_args();
        irc_set_s(fa, "code", "connection_lost");
        irc_set_s(fa, "message", "IRC connection closed unexpectedly");
        irc_set_b(fa, "retryable", true);
        json_t *fr = irc_call(ctx, "_set_fatal_error", fa);
        if (irc_is_raise(fr)) { json_free(result); return fr; }
        json_free(fr);
        json_t *nr = irc_call(ctx, "_notify_fatal_error", irc_args());
        if (irc_is_raise(nr)) { json_free(result); return nr; }
        json_free(nr);
    }
    return result;
}

/* ================================================================
 * _StandaloneConn methods (raw / pump / close)
 * ================================================================ */

/* raw: writer.write(_encode_line(line)); drain. No guard. */
static json_t *irc_raw_c(irc_ctx_t *ctx, const char *line) {
    const json_t *w = irc_self(ctx, "writer");
    if (!w) return irc_attr_err("_StandaloneConn", "writer");
    if (irc_is_none(w)) return irc_attr_err("NoneType", "write");
    char *enc = irc_encode_line_c(line);
    json_t *a = irc_args();
    irc_set_s(a, "data", enc ? enc : "");
    free(enc);
    json_t *wr = irc_call(ctx, "writer.write", a);
    IA_PROP(wr);
    json_free(wr);
    json_t *dr = irc_call(ctx, "writer.drain", irc_args());
    IA_PROP(dr);
    json_free(dr);
    return json_null();
}
/* PoP: irc_raw @ plugins/platforms/irc/adapter.py:raw */
static json_t *irc_raw(irc_ctx_t *ctx, const json_t *args) {
    return irc_raw_c(ctx, json_get_str(args, "line", ""));
}

/* pump callback modes */
#define IA_PUMP_SEAM 0
#define IA_PUMP_REG  1
#define IA_PUMP_JOIN 2
typedef struct {
    const char *nick_base;   /* registration */
    long nick_attempts;
    char *standalone_nick;
    const char *target;      /* join */
} irc_pump_cb_t;

/* the registration/join closure body: returns value-or-raise (True / null /
 * error dict). Fires conn.raw / send_error seams. */
static json_t *irc_pump_closure(irc_ctx_t *ctx, int mode, irc_pump_cb_t *cb, const char *cmd) {
    if (mode == IA_PUMP_REG) {
        if (strcmp(cmd, "432") == 0 || strcmp(cmd, "433") == 0) {
            cb->nick_attempts += 1;
            if (cb->nick_attempts > 5) return irc_sa_error_c(ctx, "too many nick collisions");
            char buf[64];
            snprintf(buf, sizeof buf, "%s-cron-%ld", cb->nick_base, cb->nick_attempts);
            char *nick = irc_slice(buf, 30);
            free(cb->standalone_nick);
            cb->standalone_nick = nick;
            char line[128];
            snprintf(line, sizeof line, "NICK %s", nick);
            json_t *r = irc_raw_c(ctx, line);
            IA_PROP(r);
            json_free(r);
        } else if (strcmp(cmd, "464") == 0 || strcmp(cmd, "465") == 0) {
            char buf[128];
            snprintf(buf, sizeof buf, "server rejected client (%s)", cmd);
            return irc_sa_error_c(ctx, buf);
        }
        return strcmp(cmd, "001") == 0 ? json_bool(true) : json_null();
    }
    if (mode == IA_PUMP_JOIN) {
        if (strcmp(cmd, "403") == 0 || strcmp(cmd, "405") == 0 || strcmp(cmd, "471") == 0 ||
            strcmp(cmd, "473") == 0 || strcmp(cmd, "474") == 0 || strcmp(cmd, "475") == 0) {
            char buf[256];
            snprintf(buf, sizeof buf, "JOIN %s rejected (%s)", cb->target, cmd);
            return irc_sa_error_c(ctx, buf);
        }
        return (strcmp(cmd, "366") == 0 || strcmp(cmd, "JOIN") == 0) ? json_bool(true) : json_null();
    }
    /* seam mode */
    json_t *a = irc_args();
    irc_set_s(a, "command", cmd);
    return irc_call(ctx, "on_msg", a);
}

/* pump: deadline loop; readuntil; PING -> raw PONG; else on_msg. */
static json_t *irc_pump_c(irc_ctx_t *ctx, double timeout, int mode, irc_pump_cb_t *cb) {
    const json_t *lp = irc_self(ctx, "loop");
    if (!lp) return irc_attr_err("_StandaloneConn", "_loop");
    if (irc_is_none(lp)) return irc_attr_err("NoneType", "time");
    json_t *t0 = irc_call(ctx, "loop.time", irc_args());
    IA_PROP(t0);
    double now0 = (t0 && t0->type == JSON_NUMBER) ? t0->num_val : 0.0;
    json_free(t0);
    double deadline = now0 + timeout;
    for (;;) {
        json_t *tn = irc_call(ctx, "loop.time", irc_args());
        IA_PROP(tn);
        double now = (tn && tn->type == JSON_NUMBER) ? tn->num_val : 0.0;
        json_free(tn);
        double remaining = deadline - now;
        if (!(remaining > 0)) break;
        const json_t *rd = irc_self(ctx, "reader");
        if (!rd) return irc_attr_err("_StandaloneConn", "reader");
        if (irc_is_none(rd)) return irc_attr_err("NoneType", "readuntil");
        json_t *ra = irc_args();
        irc_set_s(ra, "sep", "\r\n");
        json_t *rl = irc_call(ctx, "reader.readuntil", ra);
        if (irc_is_raise(rl)) {
            const char *k = irc_kind(rl);
            if (irc_exc_is_timeout(k)) { json_free(rl); return json_null(); }
            if (irc_exc_is_incomplete(k)) { json_free(rl); return irc_eof(); }
            return rl;   /* other raise propagates */
        }
        const char *line = irc_str(rl);
        char *stripped = irc_rstrip_set(line ? line : "", "\r\n");
        json_free(rl);
        json_t *msg = irc_parse_irc_message_c(stripped);
        free(stripped);
        const char *command = json_get_str(msg, "command", "");
        json_t *params = json_obj_get(msg, "params");
        size_t np = params ? json_len(params) : 0;
        if (strcmp(command, "PING") == 0) {
            const char *p0 = np > 0 ? irc_str(json_get(params, 0)) : "";
            char buf[512];
            snprintf(buf, sizeof buf, "PONG :%s", p0 ? p0 : "");
            json_free(msg);
            json_t *r = irc_raw_c(ctx, buf);
            IA_PROP(r);
            json_free(r);
        } else {
            json_free(msg);
            json_t *result = irc_pump_closure(ctx, mode, cb, command);
            IA_PROP(result);
            if (!irc_is_none(result)) return result;   /* is not None */
            json_free(result);
        }
    }
    return json_null();
}
/* PoP: irc_pump @ plugins/platforms/irc/adapter.py:pump */
static json_t *irc_pump(irc_ctx_t *ctx, const json_t *args) {
    irc_pump_cb_t cb = {0};
    return irc_pump_c(ctx, json_get_num(args, "timeout", 0), IA_PUMP_SEAM, &cb);
}

/* close: suppress(Exception): writer.close(); wait_for(writer.wait_closed(),5) */
static json_t *irc_close_c(irc_ctx_t *ctx) {
    const json_t *w = irc_self(ctx, "writer");
    if (!w) return json_null();   /* AttributeError suppressed */
    if (irc_is_none(w)) return json_null();
    json_t *cr = irc_call(ctx, "writer.close", irc_args());
    if (irc_is_raise(cr)) {
        if (irc_exc_is_base(irc_kind(cr))) return cr;
        json_free(cr); return json_null();
    }
    json_free(cr);
    json_t *wr = irc_call(ctx, "writer.wait_closed", irc_args());
    if (irc_is_raise(wr)) {
        if (irc_exc_is_base(irc_kind(wr))) return wr;
        json_free(wr); return json_null();
    }
    json_free(wr);
    return json_null();
}
/* PoP: irc_close @ plugins/platforms/irc/adapter.py:close */
static json_t *irc_close(irc_ctx_t *ctx, const json_t *args) {
    (void)args;
    return irc_close_c(ctx);
}

/* ================================================================
 * Standalone registration / join / send
 * ================================================================ */

/* _sa_register */
static json_t *irc_sa_register_c(irc_ctx_t *ctx, const char *nick_base, const char *server_password) {
    irc_pump_cb_t cb = {0};
    cb.nick_base = nick_base ? nick_base : "";
    cb.nick_attempts = 0;
    char buf[64];
    snprintf(buf, sizeof buf, "%s-cron", cb.nick_base);
    cb.standalone_nick = irc_slice(buf, 30);
    json_t *ret = json_null();
    if (server_password && server_password[0]) {
        char *clean = irc_strip_control_c(server_password);
        char line[512];
        snprintf(line, sizeof line, "PASS %s", clean);
        free(clean);
        json_t *r = irc_raw_c(ctx, line);
        if (irc_is_raise(r)) { ret = r; goto done; }
        json_free(r);
    }
    {
        char line[128];
        snprintf(line, sizeof line, "NICK %s", cb.standalone_nick);
        json_t *r = irc_raw_c(ctx, line);
        if (irc_is_raise(r)) { ret = r; goto done; }
        json_free(r);
        snprintf(line, sizeof line, "USER %s 0 * :Hermes Agent (cron)", cb.standalone_nick);
        r = irc_raw_c(ctx, line);
        if (irc_is_raise(r)) { ret = r; goto done; }
        json_free(r);
    }
    {
        json_t *registered = irc_pump_c(ctx, 15.0, IA_PUMP_REG, &cb);
        if (irc_is_raise(registered)) { ret = registered; goto done; }
        if (irc_is_none(registered)) {
            json_free(registered);
            ret = irc_sa_error_c(ctx, "registration timeout (no RPL_WELCOME)");
            goto done;
        }
        if (irc_is_eof(registered)) {
            json_free(registered);
            ret = irc_sa_error_c(ctx, "server closed connection during registration");
            goto done;
        }
        if (irc_is_true(registered)) { json_free(registered); ret = json_null(); goto done; }
        ret = registered;   /* error dict */
        goto done;
    }
done:
    free(cb.standalone_nick);
    return ret;
}
/* PoP: irc__sa_register @ plugins/platforms/irc/adapter.py:_sa_register */
static json_t *irc__sa_register(irc_ctx_t *ctx, const json_t *args) {
    const json_t *sp = json_obj_get(args, "server_password");
    return irc_sa_register_c(ctx, json_get_str(args, "nick_base", ""), irc_str(sp));
}

/* _sa_join */
static json_t *irc_sa_join_c(irc_ctx_t *ctx, const char *target) {
    irc_pump_cb_t cb = {0};
    cb.target = target ? target : "";
    char line[256];
    snprintf(line, sizeof line, "JOIN %s", cb.target);
    json_t *r = irc_raw_c(ctx, line);
    IA_PROP(r);
    json_free(r);
    json_t *joined = irc_pump_c(ctx, 5.0, IA_PUMP_JOIN, &cb);
    IA_PROP(joined);
    /* return joined if isinstance(joined, dict) else None (EOF marker is not a dict) */
    if (joined && joined->type == JSON_OBJECT && !irc_is_eof(joined)) return joined;
    json_free(joined);
    return json_null();
}
/* PoP: irc__sa_join @ plugins/platforms/irc/adapter.py:_sa_join */
static json_t *irc__sa_join(irc_ctx_t *ctx, const json_t *args) {
    return irc_sa_join_c(ctx, json_get_str(args, "target", ""));
}

/* _standalone_send */
/* PoP: irc__standalone_send @ plugins/platforms/irc/adapter.py:_standalone_send */
static json_t *irc__standalone_send(irc_ctx_t *ctx, const json_t *args) {
    const json_t *pconfig = json_obj_get(args, "pconfig");
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *message = json_get_str(args, "message", "");
    json_t *extra = NULL;
    json_t *sc = irc_server_channel_c(ctx, pconfig, &extra);
    IA_PROP(sc);
    const char *server = irc_str(json_get(sc, 0));
    const char *channel = irc_str(json_get(sc, 1));
    if (!server || !server[0] || !channel || !channel[0]) {
        json_free(sc); json_free(extra);
        return irc_sa_error_c(ctx, "IRC_SERVER and IRC_CHANNEL must be configured");
    }
    /* port_value = _env_or_extra(extra, IRC_PORT, port, 6697) */
    json_t *defport = json_number(6697);
    json_t *port_value = irc_env_or_extra_c(ctx, extra, "IRC_PORT", "port", defport);
    json_free(defport);
    if (irc_is_raise(port_value)) { json_free(sc); json_free(extra); return port_value; }
    long long port;
    char *errmsg = NULL;
    if (!irc_int_of(port_value, &port, &errmsg)) {
        char *rp = irc_repr(port_value);
        char buf[256];
        snprintf(buf, sizeof buf, "invalid port %s", rp);
        free(rp); free(errmsg);
        json_free(port_value); json_free(sc); json_free(extra);
        return irc_sa_error_c(ctx, buf);
    }
    free(errmsg);
    json_free(port_value);
    /* use_tls */
    json_t *use_tls_env = irc_scoped_secret(ctx, "IRC_USE_TLS", NULL);
    if (irc_is_raise(use_tls_env)) { json_free(sc); json_free(extra); return use_tls_env; }
    bool use_tls;
    if (irc_is_none(use_tls_env)) {
        json_t *ut = json_obj_get(extra, "use_tls");
        use_tls = ut ? irc_truthy(ut) : true;
    } else {
        const char *s = irc_str(use_tls_env);
        if (!s) {
            json_t *e = irc_attr_err(irc_type_name(use_tls_env), "lower");
            json_free(use_tls_env); json_free(sc); json_free(extra);
            return e;
        }
        char *low = irc_lower(s);
        use_tls = (strcmp(low, "1") == 0 || strcmp(low, "true") == 0 || strcmp(low, "yes") == 0);
        free(low);
    }
    json_free(use_tls_env);
    /* target = chat_id or channel; reject control chars / space */
    const char *target = (chat_id && chat_id[0]) ? chat_id : channel;
    for (const char *p = target; *p; p++) {
        if (*p == '\r' || *p == '\n' || *p == '\x00' || *p == ' ') {
            json_free(sc); json_free(extra);
            return irc_sa_error_c(ctx, "chat_id contains illegal IRC characters");
        }
    }
    /* nick_base */
    json_t *defnick = json_string("hermes-bot");
    json_t *nick_raw = irc_env_or_extra_c(ctx, extra, "IRC_NICKNAME", "nickname", defnick);
    json_free(defnick);
    if (irc_is_raise(nick_raw)) { json_free(sc); json_free(extra); return nick_raw; }
    const char *ns = irc_str(nick_raw);
    if (!ns) {
        json_t *e = irc_attr_err(irc_type_name(nick_raw), "rstrip");
        json_free(nick_raw); json_free(sc); json_free(extra);
        return e;
    }
    char *rs = irc_rstrip_set(ns, "_0123456789-");
    char *nick_base = irc_slice(rs, 24);
    free(rs);
    if (!nick_base[0]) { free(nick_base); nick_base = irc_strdup("hermes-bot"); }
    json_free(nick_raw);
    /* plain = IRCAdapter._strip_markdown(message) */
    char *plain = irc_strip_markdown_c(message);
    /* open_connection (ssl=_ssl_ctx(use_tls)) */
    json_t *ssl = irc_ssl_ctx_c(ctx, use_tls);
    if (irc_is_raise(ssl)) { free(nick_base); free(plain); json_free(sc); json_free(extra); return ssl; }
    json_t *oa = irc_args();
    irc_set_s(oa, "server", server);
    irc_set_n(oa, "port", (double)port);
    irc_set_copy(oa, "ssl", ssl);
    json_free(ssl);
    json_t *conn_res = irc_call(ctx, "asyncio.open_connection", oa);
    if (irc_is_raise(conn_res)) {
        const char *k = irc_kind(conn_res);
        if (irc_exc_is_cancelled(k)) { free(nick_base); free(plain); json_free(sc); json_free(extra); return conn_res; }
        if (irc_exc_is_exception(k)) {
            char buf[512];
            snprintf(buf, sizeof buf, "IRC standalone connect failed: %s", irc_msg(conn_res));
            json_free(conn_res); free(nick_base); free(plain); json_free(sc); json_free(extra);
            json_t *a = irc_args(); irc_set_s(a, "message", buf);
            return irc_call(ctx, "send_error", a);
        }
        free(nick_base); free(plain); json_free(sc); json_free(extra);
        return conn_res;
    }
    /* unpack reader, writer = result */
    bool unpack_ok = conn_res && conn_res->type == JSON_ARRAY && json_len(conn_res) == 2;
    if (!unpack_ok) {
        char buf[160];
        if (conn_res && conn_res->type == JSON_ARRAY) {
            size_t got = json_len(conn_res);
            if (got < 2) snprintf(buf, sizeof buf, "not enough values to unpack (expected 2, got %zu)", got);
            else snprintf(buf, sizeof buf, "too many values to unpack (expected 2, got %zu)", got);
            json_free(conn_res); free(nick_base); free(plain); json_free(sc); json_free(extra);
            json_t *a = irc_args();
            char m[256]; snprintf(m, sizeof m, "IRC standalone connect failed: %s", buf);
            irc_set_s(a, "message", m);
            return irc_call(ctx, "send_error", a);
        }
        snprintf(buf, sizeof buf, "cannot unpack non-iterable %s object", irc_type_name(conn_res));
        json_free(conn_res); free(nick_base); free(plain); json_free(sc); json_free(extra);
        json_t *a = irc_args();
        char m[256]; snprintf(m, sizeof m, "IRC standalone connect failed: %s", buf);
        irc_set_s(a, "message", m);
        return irc_call(ctx, "send_error", a);
    }
    /* Materialize the conn doubles into ctx.state (reader/writer from the
     * open_connection result; loop from get_running_loop). null element == None
     * stream (attribute access raises); object marker == live stream double. */
    irc_set_self(ctx, "reader", json_copy(json_get(conn_res, 0)));
    irc_set_self(ctx, "writer", json_copy(json_get(conn_res, 1)));
    json_free(conn_res);
    /* conn = _StandaloneConn(reader, writer) -> get_running_loop seam */
    json_t *loop_res = irc_call(ctx, "asyncio.get_running_loop", irc_args());
    if (irc_is_raise(loop_res)) {
        /* construction raise: NOT inside the try/finally (conn not bound) ->
         * propagates (no close). Exception family caught? No — the try begins
         * AFTER conn construction, so this propagates uncaught. */
        free(nick_base); free(plain); json_free(sc); json_free(extra);
        return loop_res;
    }
    irc_set_self(ctx, "loop", json_copy(loop_res));
    json_free(loop_res);
    /* From here, every exit runs finally: conn.close(). Model the body outcome
     * then run close; a BaseException from close overrides. */
    json_t *body = json_null();
    bool have_body = false;
    /* _sa_register(conn, nick_base, server_password) */
    json_t *spdef = json_string("");
    json_t *sp = irc_env_or_extra_c(ctx, extra, "IRC_SERVER_PASSWORD", "server_password", spdef);
    json_free(spdef);
    if (irc_is_raise(sp)) { body = sp; have_body = true; goto finally; }
    {
        json_t *reg = irc_sa_register_c(ctx, nick_base, irc_str(sp));
        json_free(sp);
        if (irc_is_raise(reg)) { body = reg; have_body = true; goto finally; }
        if (irc_truthy(reg)) { body = reg; have_body = true; goto finally; }
        json_free(reg);
    }
    /* nickserv */
    {
        json_t *npdef = json_string("");
        json_t *np = irc_env_or_extra_c(ctx, extra, "IRC_NICKSERV_PASSWORD", "nickserv_password", npdef);
        json_free(npdef);
        if (irc_is_raise(np)) { body = np; have_body = true; goto finally; }
        if (irc_truthy(np)) {
            char *clean = irc_strip_control_c(irc_str(np));
            char line[512];
            snprintf(line, sizeof line, "PRIVMSG NickServ :IDENTIFY %s", clean ? clean : "");
            free(clean);
            json_t *r = irc_raw_c(ctx, line);
            if (irc_is_raise(r)) { json_free(np); body = r; have_body = true; goto finally; }
            json_free(r);
            json_t *sa = irc_args(); irc_set_n(sa, "seconds", 2);
            json_t *sl = irc_call(ctx, "asyncio.sleep", sa);
            if (irc_is_raise(sl)) { json_free(np); body = sl; have_body = true; goto finally; }
            json_free(sl);
        }
        json_free(np);
    }
    /* JOIN before PRIVMSG */
    if (irc_is_channel_str(target)) {
        json_t *je = irc_sa_join_c(ctx, target);
        if (irc_is_raise(je)) { body = je; have_body = true; goto finally; }
        if (irc_truthy(je)) { body = je; have_body = true; goto finally; }
        json_free(je);
    }
    /* paragraphs / lines */
    {
        json_t *paragraphs = json_array();
        char *dup = irc_strdup(plain);
        char *start = dup;
        while (start) {
            char *nl = strchr(start, '\n');
            if (nl) *nl = '\0';
            char *clean = irc_strip_control_c(start);
            char *cleanr = irc_rstrip_set(clean, " \t\n\r\f\v\x1c\x1d\x1e\x1f");
            free(clean);
            if (cleanr && cleanr[0]) json_append(paragraphs, json_string(cleanr));
            free(cleanr);
            start = nl ? nl + 1 : NULL;
        }
        free(dup);
        long budget = irc_privmsg_budget_c(target);
        json_t *lines = irc_split_lines_c(paragraphs, budget);
        json_free(paragraphs);
        for (size_t i = 0; i < json_len(lines); i++) {
            const char *line = irc_str(json_get(lines, i));
            char buf[1024];
            snprintf(buf, sizeof buf, "PRIVMSG %s :%s", target, line ? line : "");
            json_t *r = irc_raw_c(ctx, buf);
            if (irc_is_raise(r)) { json_free(lines); body = r; have_body = true; goto finally; }
            json_free(r);
            json_t *sa = irc_args(); irc_set_n(sa, "seconds", 0.3);
            json_t *sl = irc_call(ctx, "asyncio.sleep", sa);
            if (irc_is_raise(sl)) { json_free(lines); body = sl; have_body = true; goto finally; }
            json_free(sl);
        }
        if (json_len(lines) == 0) {
            json_free(lines);
            body = irc_sa_error_c(ctx, "empty message after stripping");
            have_body = true; goto finally;
        }
        json_free(lines);
    }
    /* QUIT + t() */
    {
        json_t *ta = irc_args(); irc_set_s(ta, "key", "platform.irc.standalone_quit");
        json_t *tv = irc_call(ctx, "t", ta);
        if (irc_is_raise(tv)) { body = tv; have_body = true; goto finally; }
        const char *qs = irc_str(tv);
        char line[512];
        snprintf(line, sizeof line, "QUIT :%s", qs ? qs : "");
        json_free(tv);
        json_t *r = irc_raw_c(ctx, line);
        if (irc_is_raise(r)) { body = r; have_body = true; goto finally; }
        json_free(r);
    }
    /* suppress(TimeoutError): reader.read(1024) */
    {
        json_t *ra = irc_args(); irc_set_n(ra, "n", 1024);
        json_t *rv = irc_call(ctx, "reader.read", ra);
        if (irc_is_raise(rv)) {
            if (irc_exc_is_timeout(irc_kind(rv))) { json_free(rv); }
            else { body = rv; have_body = true; goto finally; }
        } else json_free(rv);
    }
    /* return {"success":True,"message_id":_ms_id()} */
    {
        json_t *mid = irc_ms_id_c(ctx);
        if (irc_is_raise(mid)) { body = mid; have_body = true; goto finally; }
        json_t *ok = json_object();
        irc_set_b(ok, "success", true);
        irc_set_s(ok, "message_id", irc_str(mid));
        json_free(mid);
        body = ok; have_body = true;
    }
finally:
    /* except asyncio.CancelledError: raise ; except Exception: logger.debug +
     * send_error. Applied to the body outcome BEFORE finally close. */
    if (have_body && irc_is_raise(body)) {
        const char *k = irc_kind(body);
        if (irc_exc_is_cancelled(k)) {
            /* re-raise (finally still runs) */
        } else if (irc_exc_is_exception(k)) {
            json_t *la = irc_args();
            irc_set_s(la, "fmt", "IRC standalone send raised");
            json_set(la, "args", json_array());
            irc_set_b(la, "exc_info", true);
            json_t *lr = irc_call(ctx, "logger.debug", la);
            if (irc_is_raise(lr)) { json_free(body); body = lr; }
            else {
                json_free(lr);
                char buf[512];
                snprintf(buf, sizeof buf, "IRC standalone send failed: %s", irc_msg(body));
                json_free(body);
                json_t *a = irc_args(); irc_set_s(a, "message", buf);
                body = irc_call(ctx, "send_error", a);
            }
        }
        /* BaseException (non-cancelled) propagates */
    }
    /* finally: await conn.close() */
    {
        json_t *cr = irc_close_c(ctx);
        if (irc_is_raise(cr)) {
            /* close only ever returns null or a BaseException raise */
            if (!irc_is_raise(body)) { json_free(body); body = cr; }
            else json_free(cr);
        } else json_free(cr);
    }
    free(nick_base); free(plain); json_free(sc); json_free(extra);
    return body;
}
