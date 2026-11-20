/*
 * irc_adapter_flow2.c — IRC adapter FLOW remainder of
 * plugins/platforms/irc/adapter.py (hermes-agent): the adapter construction /
 * connect / disconnect lifecycle (IRCAdapter.__init__, connect, disconnect),
 * the interactive CLI setup flow (interactive_setup + its nested info /
 * _required helpers), the standalone-conn constructor (_StandaloneConn.__init__),
 * the registration / join pump closures (_on_registration, _on_join) and the
 * plugin entry point (register).
 *
 * The 35 flow1 siblings stay CLAIMED in irc_adapter_flow.c; every flow1
 * sibling called here is SEAMED (scripted) through the universal seam
 * (_env_or_extra, _ssl_ctx, _fail, _send_raw, _sa_error, conn.raw). The flow2
 * siblings below call each other REAL (inline C): connect -> disconnect,
 * interactive_setup -> info / _required. Foreign collaborators (base adapter
 * methods, Platform enum construction, coerce_port, _get_scoped_secret, the
 * platform_registry import + lookup, asyncio Event/open_connection/sleep/
 * create_task/get_running_loop, the stream/event doubles, t (i18n), cancel_task,
 * logger, the hermes_cli.setup / setup_platforms wizard functions, builtin
 * print, and ctx.register_platform) are SEAMED.
 *
 * Universal seam: every SEAMED call enters through ONE callback
 * irc2_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts. args
 * is ADOPTED into the trace event; the return is a NEW owned json_t (NULL ==
 * Python None; {"__raise__":cls,"message":str(exc)} == a raised exception). A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 * BaseException kinds (SystemExit / KeyboardInterrupt / GeneratorExit /
 * CancelledError / BaseExceptionGroup) pass through every `except Exception`
 * and `contextlib.suppress(Exception)` catch site; the full builtin hierarchy
 * is tabulated below (irc2_exc_parent).
 *
 * Async model: the live coroutines (connect / disconnect / _on_registration /
 * _on_join) are driven under asyncio.run by the oracle; awaited seams are async
 * recorders. C is synchronous — an awaited seam is one irc2_call.
 * asyncio.wait_for is TRANSPARENT (not a seam): timeouts are scripted as raises
 * on the inner operation (registration_event.wait). asyncio.create_task does
 * NOT run the receive loop: self._receive_loop() merely creates a coroutine,
 * projected to the marker {"__coro__":"_receive_loop"} (no seam fires for the
 * coroutine creation itself).
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and irc2_-prefixed.
 *
 * Seam name table (name -> live call):
 *   Platform                  Platform("irc") -> value projection "irc"
 *   base.__init__             super().__init__(config=, platform=)
 *   _env_or_extra             flow1 sibling (extra, env, key, default)
 *   coerce_port               _shared.coerce_port(value, default)
 *   _get_scoped_secret        _shared.get_scoped_secret(name, default)
 *   import_platform_registry  from gateway.platform_registry import platform_registry
 *   platform_registry.get     platform_registry.get("irc") -> attr double
 *   asyncio.Event             asyncio.Event() -> event marker
 *   asyncio.open_connection   asyncio.open_connection(server, port, ssl=)
 *   asyncio.sleep             asyncio.sleep(seconds)
 *   asyncio.create_task       asyncio.create_task(coro)
 *   asyncio.get_running_loop  asyncio.get_running_loop()
 *   _ssl_ctx                  flow1 sibling (use_tls)
 *   _fail                     flow1 sibling (code, message, retryable=)
 *   _send_raw                 flow1 sibling (line)
 *   _sa_error                 flow1 sibling (detail)
 *   conn.raw                  flow1 sibling _StandaloneConn.raw(line)
 *   _acquire_platform_lock    base (scope, identity, resource_desc)
 *   _release_platform_lock    base ()
 *   _mark_connected           base ()
 *   _mark_disconnected        base ()
 *   _wire_plugin_handlers     base (native)
 *   cancel_task               helpers.cancel_task(task)
 *   registration_event.wait   event.wait() (async)
 *   registration_event.clear  event.clear()
 *   writer.is_closing / writer.close / writer.wait_closed   stream double
 *   t                         i18n t(key)
 *   logger.error / logger.info / logger.warning   logger.<level>(fmt, *args)
 *   print                     builtin print() (bare; args [])
 *   print_header / print_info / print_warning / print_success   hermes_cli
 *   prompt                    hermes_cli prompt(question, default= / password=)
 *   prompt_yes_no             hermes_cli prompt_yes_no(question, default)
 *   save_env_value            hermes_cli save_env_value(key, value)
 *   get_env_value             hermes_cli get_env_value(key)
 *   declines_reconfigure      hermes_cli declines_reconfigure(label, question, *env_vars)
 *   ctx.register_platform     ctx.register_platform(**kwargs)
 *
 * Representation contract (shared with tests/oracle_irc_adapter_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - The adapter self double is ctx.state (a JSON object of live attributes);
 *   methods read/write it in place. A DIRECT self._attr read with the key
 *   ABSENT raises AttributeError "'IRCAdapter' object has no attribute 'X'".
 *   The conn double uses class name "_StandaloneConn"; its state keys are
 *   reader / writer / _loop. The pump closures' nonlocal cells travel as
 *   ctx.state keys nick_base / nick_attempts / standalone_nick (_on_registration)
 *   and target (_on_join).
 * - Stream/event doubles are presence markers: null (None) or a non-empty
 *   object marker ({"__writer__":true}, {"__event__":true}, ...); their methods
 *   are SEAMED. They project BACK to their marker in the state echo.
 * - platform_registry.get scripts are objects carrying max_message_length, or
 *   null / non-dict scalars (the AttributeError path, suppressed). A dict
 *   WITHOUT the key raises "'types.SimpleNamespace' object has no attribute
 *   'max_message_length'" (the oracle materializes SimpleNamespace(**dict)).
 * - Callable kwargs in register project to {"__fn__":name} (functions) /
 *   {"__cls__":"IRCAdapter"} (the adapter class); the coroutine marker is
 *   {"__coro__":"_receive_loop"}.
 * - Integral floats == ints (libjson %.0f; oracle canon folds). Values embedded
 *   into f-strings / str() carry str/int/bool/None only (no floats: str(6697.0)
 *   is "6697.0" while the folded wire value is 6697).
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - ASCII scope: all fixture INPUT text is ASCII. The only non-ASCII bytes are
 *   the FIXED source literals (em-dash U+2014, key U+1F511, lock U+1F512,
 *   warning U+26A0 U+FE0F, speech-balloon U+1F4AC), emitted as UTF-8 byte
 *   escapes below and round-tripped raw by libjson / the oracle's utf-8 IO.
 * - config / pconfig inputs are objects or null (materialized SimpleNamespace).
 * - open_connection scripts are 2-arrays (reader/writer markers), wrong-arity
 *   arrays, 2-char / wrong-length strings, 2-key / wrong-key dicts, or
 *   non-iterable scalars (the unpack TypeError/ValueError path). Sets are out
 *   of domain (arbitrary iteration order).
 * - int()-coerced strings (max_message_length, port) are clean ASCII decimals
 *   (optional sign, narrower-ws padding, NO underscores) or printable-ASCII
 *   invalid tokens without quotes/backslashes; no float infinity/nan.
 * - allowed_users is a list / str / dict(keys) / null / non-iterable scalar;
 *   the lower-set echo is sorted+deduped on both sides (canon sorts sets).
 * - Values reaching f-strings (server/nickname/channel/passwords/what) are
 *   str/int/bool/null. prompt/get_env_value/seam rets consumed by .strip()/
 *   .replace()/.lower() are str or the AttributeError path.
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
typedef struct irc2_ctx {
    json_t *state;      /* owned self/conn/cell double; mutated in place */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} irc2_ctx_t;

/* ── basic helpers ── */
static bool irc2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *irc2_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static const char *irc2_msg(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "message") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static bool irc2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool irc2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *irc2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *irc2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *irc2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return irc2_err(type, msg);
}
static json_t *irc2_attr_err(const char *cls, const char *key) {
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", cls, key);
    return irc2_err("AttributeError", msg);
}
static const char *irc2_type_name(const json_t *v) {
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
static json_t *irc2_call(irc2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = irc2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = irc2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = irc2_make_exc(raise);
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
static json_t *irc2_args(void) { return json_object(); }
static void irc2_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void irc2_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void irc2_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void irc2_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void irc2_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Propagate a raise result (transfers ownership). */
#define IA2_PROP(r) do { if (irc2_is_raise(r)) return (r); } while (0)

/* ── exception hierarchy (FULL builtin tree; first-base parent chain; unknown
 * kinds default to the Exception family, matching the oracle's synthetic
 * Exception subclasses). CancelledError -> BaseException ONLY so it passes
 * through `except Exception`. ── */
typedef struct { const char *name, *parent; } irc2_exc_parent_t;
static const irc2_exc_parent_t IA2_EXC_PARENTS[] = {
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
static const char *irc2_exc_parent(const char *kind) {
    for (size_t i = 0; IA2_EXC_PARENTS[i].name; i++)
        if (!strcmp(IA2_EXC_PARENTS[i].name, kind)) return IA2_EXC_PARENTS[i].parent;
    return NULL;
}
static bool irc2_exc_is_exception(const char *kind) {
    if (!kind) return true;
    for (int depth = 0; kind && depth < 64; depth++) {
        if (!strcmp(kind, "Exception")) return true;
        if (!strcmp(kind, "BaseException")) return false;
        const char *p = irc2_exc_parent(kind);
        if (!p) return true;  /* unknown -> synthetic Exception subclass */
        kind = p;
    }
    return true;
}
static bool irc2_exc_is_base(const char *kind) { return !irc2_exc_is_exception(kind); }
static bool irc2_exc_is_timeout(const char *kind) {
    for (int depth = 0; kind && depth < 64; depth++) {
        if (!strcmp(kind, "TimeoutError")) return true;
        const char *p = irc2_exc_parent(kind);
        if (!p) return false;
        kind = p;
    }
    return false;
}

/* ── string helpers ── */
static char *irc2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
/* str.split()/strip() / re \s ASCII whitespace set (py3.14 includes 1c-1f). */
static bool irc2_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'
        || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
/* int() strips a NARROWER set (NOT 1c-1f). */
static bool irc2_is_int_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static char *irc2_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && irc2_is_ws((unsigned char)s[a])) a++;
    while (b > a && irc2_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}
static char *irc2_lower(const char *s) {
    char *o = irc2_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
/* replace all literal occurrences of needle in hay with repl. */
static char *irc2_replace_all(const char *hay, const char *needle, const char *repl) {
    size_t nl = strlen(needle), rl = strlen(repl);
    if (nl == 0) return irc2_strdup(hay);
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
static char *irc2_slice(const char *s, size_t cap_cp) {
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
static char *irc2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return irc2_strdup("None");
    if (v->type == JSON_BOOL) return irc2_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return irc2_strdup(buf);
    }
    if (v->type == JSON_STRING) return irc2_strdup(v->str_val ? v->str_val : "");
    return irc2_strdup("");
}
/* int() coercion (clean-decimal corpus; no underscores). Returns false + errmsg
 * on ValueError/TypeError; truncates floats toward zero like Python int(). */
static bool irc2_int_of(const json_t *v, long long *out, char **errmsg) {
    *errmsg = NULL;
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return true; }
    if (v && v->type == JSON_NUMBER) { *out = (long long)v->num_val; return true; }
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && irc2_is_int_ws((unsigned char)s[a])) a++;
        while (b > a && irc2_is_int_ws((unsigned char)s[b - 1])) b--;
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
                 irc2_type_name(v));
        *errmsg = msg;
        return false;
    }
}

/* self double attribute reader (borrowed); ABSENT key -> NULL. */
static const json_t *irc2_self(const irc2_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}
static void irc2_set_self(irc2_ctx_t *ctx, const char *key, json_t *v) {
    if (ctx->state) json_set(ctx->state, key, v);
}
/* DIRECT self._attr read: absent key raises AttributeError naming the class. */
static const json_t *irc2_req(irc2_ctx_t *ctx, const char *cls, const char *key, json_t **err) {
    const json_t *v = irc2_self(ctx, key);
    if (!v) { *err = irc2_attr_err(cls, key); return NULL; }
    *err = NULL;
    return v;
}

/* logger.<level>(fmt, *args) seam: args array of raw values (BaseException
 * args project to str on the oracle side; here the caller passes str). */
static json_t *irc2_log(irc2_ctx_t *ctx, const char *level, const char *fmt, json_t *args) {
    char name[64];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *a = irc2_args();
    irc2_set_s(a, "fmt", fmt);
    json_set(a, "args", args ? args : json_array());
    return irc2_call(ctx, name, a);
}

/* ── sorted+deduped lower-set projection (canon sorts sets) ── */
static int irc2_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static json_t *irc2_lower_set(const json_t *iterable, json_t **err) {
    *err = NULL;
    /* iterate: array elements / object keys / str chars; else TypeError */
    char **items = NULL;
    size_t n = 0, cap = 0;
    if (iterable && iterable->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(iterable); i++) {
            const json_t *u = json_get(iterable, i);
            if (u && u->type == JSON_STRING) {
                if (n == cap) { cap = cap ? cap * 2 : 8; items = realloc(items, cap * sizeof(char *)); }
                items[n++] = irc2_lower(u->str_val ? u->str_val : "");
            }
        }
    } else if (iterable && iterable->type == JSON_OBJECT) {
        for (size_t i = 0; i < iterable->c.count; i++) {
            if (n == cap) { cap = cap ? cap * 2 : 8; items = realloc(items, cap * sizeof(char *)); }
            items[n++] = irc2_lower(iterable->c.keys[i]);
        }
    } else if (iterable && iterable->type == JSON_STRING) {
        const char *s = iterable->str_val ? iterable->str_val : "";
        for (size_t i = 0; s[i]; i++) {
            char ch[2] = { s[i], '\0' };
            if (n == cap) { cap = cap ? cap * 2 : 8; items = realloc(items, cap * sizeof(char *)); }
            items[n++] = irc2_lower(ch);
        }
    } else {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", irc2_type_name(iterable));
        *err = irc2_err("TypeError", msg);
        return NULL;
    }
    if (n > 1) qsort(items, n, sizeof(char *), irc2_cmp_str);
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++) {
        if (i == 0 || strcmp(items[i], items[i - 1]) != 0) json_append(arr, json_string(items[i]));
        free(items[i]);
    }
    free(items);
    return arr;
}

/* ── 2-target unpack (reader, writer = result). Returns NULL on success (fills
 * r/w with owned copies) or a TypeError/ValueError raise node. ── */
static json_t *irc2_unpack2(const json_t *v, json_t **r, json_t **w) {
    *r = NULL; *w = NULL;
    if (v && v->type == JSON_ARRAY) {
        size_t got = json_len(v);
        if (got == 2) { *r = json_copy(json_get(v, 0)); *w = json_copy(json_get(v, 1)); return NULL; }
        char msg[128];
        if (got < 2) snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", got);
        else snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)", got);
        return irc2_err("ValueError", msg);
    }
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t got = strlen(s);
        if (got == 2) {
            char a[2] = { s[0], '\0' }, b[2] = { s[1], '\0' };
            *r = json_string(a); *w = json_string(b); return NULL;
        }
        char msg[128];
        /* str unpack: not-enough carries got N; too-many omits it (py3.14). */
        if (got < 2) snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", got);
        else snprintf(msg, sizeof msg, "too many values to unpack (expected 2)");
        return irc2_err("ValueError", msg);
    }
    if (v && v->type == JSON_OBJECT) {
        size_t got = v->c.count;
        if (got == 2) { *r = json_string(v->c.keys[0]); *w = json_string(v->c.keys[1]); return NULL; }
        char msg[128];
        if (got < 2) snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", got);
        else snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)", got);
        return irc2_err("ValueError", msg);
    }
    char msg[128];
    snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", irc2_type_name(v));
    return irc2_err("TypeError", msg);
}

/* ── seamed flow1 siblings / foreign leaves ── */
static json_t *irc2_env_or_extra(irc2_ctx_t *ctx, const json_t *extra, const char *env,
                                 const char *key, const json_t *deflt) {
    json_t *a = irc2_args();
    irc2_set_copy(a, "extra", extra);
    irc2_set_s(a, "env", env);
    irc2_set_s(a, "key", key);
    irc2_set_copy(a, "default", deflt);
    return irc2_call(ctx, "_env_or_extra", a);
}
static json_t *irc2_scoped_secret(irc2_ctx_t *ctx, const char *name, const json_t *deflt) {
    json_t *a = irc2_args();
    irc2_set_s(a, "name", name);
    irc2_set_copy(a, "default", deflt);
    return irc2_call(ctx, "_get_scoped_secret", a);
}
static json_t *irc2_ssl_ctx(irc2_ctx_t *ctx, const json_t *use_tls) {
    json_t *a = irc2_args();
    irc2_set_copy(a, "use_tls", use_tls);
    return irc2_call(ctx, "_ssl_ctx", a);
}
static json_t *irc2_fail(irc2_ctx_t *ctx, const char *code, const char *message, bool retryable) {
    json_t *a = irc2_args();
    irc2_set_s(a, "code", code);
    irc2_set_s(a, "message", message);
    irc2_set_b(a, "retryable", retryable);
    return irc2_call(ctx, "_fail", a);
}
static json_t *irc2_send_raw(irc2_ctx_t *ctx, const char *line) {
    json_t *a = irc2_args();
    irc2_set_s(a, "line", line);
    return irc2_call(ctx, "_send_raw", a);
}
static json_t *irc2_sa_error(irc2_ctx_t *ctx, const char *detail) {
    json_t *a = irc2_args();
    irc2_set_s(a, "detail", detail);
    return irc2_call(ctx, "_sa_error", a);
}
static json_t *irc2_conn_raw(irc2_ctx_t *ctx, const json_t *conn, const char *line, json_t **err) {
    *err = NULL;
    if (irc2_is_none(conn)) { *err = irc2_attr_err("NoneType", "raw"); return NULL; }
    json_t *a = irc2_args();
    irc2_set_s(a, "line", line);
    return irc2_call(ctx, "conn.raw", a);
}

/* ================================================================
 * IRCAdapter.__init__
 * ================================================================ */
static json_t *irc2_adapter_init_c(irc2_ctx_t *ctx, const json_t *config) {
    /* super().__init__(config=config, platform=Platform("irc")) — Platform
     * evaluated first (kwarg order), then the base constructor. */
    json_t *pa = irc2_args();
    irc2_set_s(pa, "value", "irc");
    json_t *plat = irc2_call(ctx, "Platform", pa);
    IA2_PROP(plat);
    json_t *ba = irc2_args();
    irc2_set_copy(ba, "config", config);
    irc2_set_copy(ba, "platform", plat);
    json_free(plat);
    json_t *br = irc2_call(ctx, "base.__init__", ba);
    IA2_PROP(br);
    json_free(br);
    /* extra = getattr(config, "extra", {}) or {} */
    json_t *extra = NULL;
    if (config && config->type == JSON_OBJECT) {
        json_t *e = json_obj_get(config, "extra");
        if (irc2_truthy(e)) extra = json_copy(e);
    }
    if (!extra) extra = json_object();
    /* self.server = _env_or_extra(extra, "IRC_SERVER", "server") — default "" */
    json_t *def_empty = json_string("");
    json_t *server = irc2_env_or_extra(ctx, extra, "IRC_SERVER", "server", def_empty);
    if (irc2_is_raise(server)) { json_free(def_empty); json_free(extra); return server; }
    irc2_set_self(ctx, "server", server);
    /* self.port = coerce_port(_env_or_extra(extra, "IRC_PORT", "port", 6697), 6697) */
    json_t *def_port = json_number(6697);
    json_t *pv = irc2_env_or_extra(ctx, extra, "IRC_PORT", "port", def_port);
    if (irc2_is_raise(pv)) { json_free(def_port); json_free(def_empty); json_free(extra); return pv; }
    json_t *ca = irc2_args();
    irc2_set_copy(ca, "value", pv);
    irc2_set_n(ca, "default", 6697);
    json_free(pv);
    json_t *port = irc2_call(ctx, "coerce_port", ca);
    json_free(def_port);
    if (irc2_is_raise(port)) { json_free(def_empty); json_free(extra); return port; }
    irc2_set_self(ctx, "port", port);
    /* self.nickname = _env_or_extra(extra, "IRC_NICKNAME", "nickname", "hermes-bot") */
    json_t *def_nick = json_string("hermes-bot");
    json_t *nick = irc2_env_or_extra(ctx, extra, "IRC_NICKNAME", "nickname", def_nick);
    json_free(def_nick);
    if (irc2_is_raise(nick)) { json_free(def_empty); json_free(extra); return nick; }
    irc2_set_self(ctx, "nickname", nick);
    /* self.channel = _env_or_extra(extra, "IRC_CHANNEL", "channel") — default "" */
    json_t *chan = irc2_env_or_extra(ctx, extra, "IRC_CHANNEL", "channel", def_empty);
    json_free(def_empty);
    if (irc2_is_raise(chan)) { json_free(extra); return chan; }
    irc2_set_self(ctx, "channel", chan);
    /* _use_tls_raw = _get_scoped_secret("IRC_USE_TLS") — default None */
    json_t *ut_raw = irc2_scoped_secret(ctx, "IRC_USE_TLS", NULL);
    IA2_PROP(ut_raw);
    json_t *use_tls = NULL;
    if (irc2_truthy(ut_raw)) {
        const char *s = irc2_str(ut_raw);
        if (!s) {
            json_t *e = irc2_attr_err(irc2_type_name(ut_raw), "lower");
            json_free(ut_raw); json_free(extra); return e;
        }
        char *low = irc2_lower(s);
        bool truthy = (strcmp(low, "1") == 0 || strcmp(low, "true") == 0 || strcmp(low, "yes") == 0);
        free(low);
        use_tls = json_bool(truthy);
    } else {
        /* extra.get("use_tls", True) */
        if (extra->type != JSON_OBJECT) {
            json_t *e = irc2_attr_err(irc2_type_name(extra), "get");
            json_free(ut_raw); json_free(extra); return e;
        }
        json_t *v = json_obj_get(extra, "use_tls");
        use_tls = v ? json_copy(v) : json_bool(true);
    }
    json_free(ut_raw);
    irc2_set_self(ctx, "use_tls", use_tls);
    /* self.server_password / nickserv_password — default "" */
    json_t *de2 = json_string("");
    json_t *sp = irc2_env_or_extra(ctx, extra, "IRC_SERVER_PASSWORD", "server_password", de2);
    if (irc2_is_raise(sp)) { json_free(de2); json_free(extra); return sp; }
    irc2_set_self(ctx, "server_password", sp);
    json_t *np = irc2_env_or_extra(ctx, extra, "IRC_NICKSERV_PASSWORD", "nickserv_password", de2);
    json_free(de2);
    if (irc2_is_raise(np)) { json_free(extra); return np; }
    irc2_set_self(ctx, "nickserv_password", np);
    /* self.allowed_users = extra.get("allowed_users", []) */
    if (extra->type != JSON_OBJECT) {
        json_t *e = irc2_attr_err(irc2_type_name(extra), "get");
        json_free(extra); return e;
    }
    json_t *au = json_obj_get(extra, "allowed_users");
    irc2_set_self(ctx, "allowed_users", au ? json_copy(au) : json_array());
    /* self._allowed_users_lower = {u.lower() for u in allowed_users if isinstance(u, str)} */
    json_t *lerr = NULL;
    json_t *lower = irc2_lower_set(irc2_self(ctx, "allowed_users"), &lerr);
    if (lerr) { json_free(extra); return lerr; }
    irc2_set_self(ctx, "_allowed_users_lower", lower);
    /* max_msg = extra.get("max_message_length") */
    json_t *max_msg = NULL;
    json_t *mm = json_obj_get(extra, "max_message_length");
    if (mm) max_msg = json_copy(mm);
    /* if max_msg is None: with suppress(Exception): import + registry lookup */
    if (irc2_is_none(max_msg)) {
        json_t *ir = irc2_call(ctx, "import_platform_registry", irc2_args());
        if (irc2_is_raise(ir)) {
            if (irc2_exc_is_base(irc2_kind(ir))) { json_free(max_msg); json_free(extra); return ir; }
            json_free(ir);   /* Exception suppressed; max_msg stays None */
        } else {
            json_t *reg = ir;   /* registry double (null -> None.get raises) */
            json_t *gres = NULL;
            if (irc2_is_none(reg)) {
                /* AttributeError 'NoneType' 'get' — suppressed */
            } else {
                json_t *ga = irc2_args();
                irc2_set_s(ga, "name", "irc");
                gres = irc2_call(ctx, "platform_registry.get", ga);
                if (irc2_is_raise(gres)) {
                    if (irc2_exc_is_base(irc2_kind(gres))) { json_free(reg); json_free(max_msg); json_free(extra); return gres; }
                    json_free(gres); gres = NULL;   /* suppressed */
                }
            }
            if (gres) {
                if (gres->type == JSON_OBJECT) {
                    json_t *mval = json_obj_get(gres, "max_message_length");
                    if (mval) { json_free(max_msg); max_msg = json_copy(mval); }
                    /* else: SimpleNamespace missing attr -> AttributeError suppressed */
                }
                /* non-dict: AttributeError '<type>' 'max_message_length' suppressed */
                json_free(gres);
            }
            json_free(reg);
        }
    }
    json_free(extra);
    /* self.max_message_length = int(max_msg or 450) — NOT suppressed */
    long long mml = 450;
    if (irc2_truthy(max_msg)) {
        char *errmsg = NULL;
        if (!irc2_int_of(max_msg, &mml, &errmsg)) {
            /* invalid-literal -> ValueError; type message -> TypeError */
            bool is_value = errmsg && strncmp(errmsg, "invalid literal", 15) == 0;
            json_t *e = irc2_err(is_value ? "ValueError" : "TypeError", errmsg ? errmsg : "");
            free(errmsg);
            json_free(max_msg);
            return e;
        }
        free(errmsg);
    }
    json_free(max_msg);
    irc2_set_self(ctx, "max_message_length", json_number((double)mml));
    /* self._reader / _writer / _recv_task = None */
    irc2_set_self(ctx, "_reader", json_null());
    irc2_set_self(ctx, "_writer", json_null());
    irc2_set_self(ctx, "_recv_task", json_null());
    /* self._current_nick = self.nickname */
    irc2_set_self(ctx, "_current_nick", json_copy(irc2_self(ctx, "nickname")));
    /* self._registered = False */
    irc2_set_self(ctx, "_registered", json_bool(false));
    /* self._registration_event = asyncio.Event() */
    json_t *ev = irc2_call(ctx, "asyncio.Event", irc2_args());
    IA2_PROP(ev);
    irc2_set_self(ctx, "_registration_event", ev);
    return json_null();
}
/* PoP: irc2_adapter_init @ plugins/platforms/irc/adapter.py:__init__ */
static json_t *irc2_adapter_init(irc2_ctx_t *ctx, const json_t *args) {
    return irc2_adapter_init_c(ctx, json_obj_get(args, "config"));
}

/* ================================================================
 * IRCAdapter.disconnect (REAL sibling of connect)
 * ================================================================ */
static json_t *irc2_disconnect_c(irc2_ctx_t *ctx) {
    /* with suppress(Exception): self._release_platform_lock() */
    json_t *rl = irc2_call(ctx, "_release_platform_lock", irc2_args());
    if (irc2_is_raise(rl)) {
        if (irc2_exc_is_base(irc2_kind(rl))) return rl;
        json_free(rl);
    } else json_free(rl);
    /* self._mark_disconnected() — no suppress */
    json_t *md = irc2_call(ctx, "_mark_disconnected", irc2_args());
    IA2_PROP(md);
    json_free(md);
    /* if self._writer and not self._writer.is_closing(): */
    json_t *err = NULL;
    const json_t *w = irc2_req(ctx, "IRCAdapter", "_writer", &err);
    if (err) return err;
    bool enter = irc2_truthy(w);
    if (enter) {
        json_t *ic = irc2_call(ctx, "writer.is_closing", irc2_args());
        IA2_PROP(ic);
        enter = !irc2_truthy(ic);
        json_free(ic);
    }
    if (enter) {
        /* with suppress(Exception): _send_raw("QUIT :" + t(...)); sleep(0.5) */
        bool block_ok = true;
        json_t *ta = irc2_args();
        irc2_set_s(ta, "key", "platform.irc.quit_message");
        json_t *tv = irc2_call(ctx, "t", ta);
        if (irc2_is_raise(tv)) {
            if (irc2_exc_is_base(irc2_kind(tv))) return tv;
            json_free(tv); block_ok = false;
        } else {
            const char *qs = irc2_str(tv);
            if (!qs) {
                /* "QUIT :" + non-str -> TypeError (suppressed) */
                json_free(tv); block_ok = false;
            } else {
                char line[512];
                snprintf(line, sizeof line, "QUIT :%s", qs);
                json_free(tv);
                json_t *sr = irc2_send_raw(ctx, line);
                if (irc2_is_raise(sr)) {
                    if (irc2_exc_is_base(irc2_kind(sr))) return sr;
                    json_free(sr); block_ok = false;
                } else json_free(sr);
                if (block_ok) {
                    json_t *sa = irc2_args();
                    irc2_set_n(sa, "seconds", 0.5);
                    json_t *sl = irc2_call(ctx, "asyncio.sleep", sa);
                    if (irc2_is_raise(sl)) {
                        if (irc2_exc_is_base(irc2_kind(sl))) return sl;
                        json_free(sl);
                    } else json_free(sl);
                }
            }
        }
        /* with suppress(Exception): self._writer.close(); wait_closed() */
        const json_t *w2 = irc2_req(ctx, "IRCAdapter", "_writer", &err);
        if (err) { json_free(err); /* suppressed */ }
        else if (irc2_is_none(w2)) { /* None.close AttributeError suppressed */ }
        else {
            json_t *cr = irc2_call(ctx, "writer.close", irc2_args());
            if (irc2_is_raise(cr)) {
                if (irc2_exc_is_base(irc2_kind(cr))) return cr;
                json_free(cr);
            } else {
                json_free(cr);
                json_t *wr = irc2_call(ctx, "writer.wait_closed", irc2_args());
                if (irc2_is_raise(wr)) {
                    if (irc2_exc_is_base(irc2_kind(wr))) return wr;
                    json_free(wr);
                } else json_free(wr);
            }
        }
    }
    /* await cancel_task(self._recv_task) — no suppress */
    const json_t *task = irc2_req(ctx, "IRCAdapter", "_recv_task", &err);
    if (err) return err;
    json_t *cta = irc2_args();
    irc2_set_copy(cta, "task", task);
    json_t *ct = irc2_call(ctx, "cancel_task", cta);
    IA2_PROP(ct);
    json_free(ct);
    /* self._reader = None; self._writer = None; self._registered = False */
    irc2_set_self(ctx, "_reader", json_null());
    irc2_set_self(ctx, "_writer", json_null());
    irc2_set_self(ctx, "_registered", json_bool(false));
    /* self._registration_event.clear() */
    const json_t *ev = irc2_req(ctx, "IRCAdapter", "_registration_event", &err);
    if (err) return err;
    if (irc2_is_none(ev)) return irc2_attr_err("NoneType", "clear");
    json_t *clr = irc2_call(ctx, "registration_event.clear", irc2_args());
    IA2_PROP(clr);
    json_free(clr);
    return json_null();
}
/* PoP: irc2_disconnect @ plugins/platforms/irc/adapter.py:disconnect */
static json_t *irc2_disconnect(irc2_ctx_t *ctx, const json_t *args) {
    (void)args;
    return irc2_disconnect_c(ctx);
}

/* ================================================================
 * IRCAdapter.connect
 * ================================================================ */
/* PoP: irc2_connect @ plugins/platforms/irc/adapter.py:connect */
static json_t *irc2_connect(irc2_ctx_t *ctx, const json_t *args) {
    (void)args;   /* is_reconnect accepted for signature parity, unused */
    json_t *err = NULL;
    /* if not self.server or not self.channel: */
    const json_t *server = irc2_req(ctx, "IRCAdapter", "server", &err);
    if (err) return err;
    bool missing = !irc2_truthy(server);
    if (!missing) {
        const json_t *channel = irc2_req(ctx, "IRCAdapter", "channel", &err);
        if (err) return err;
        missing = !irc2_truthy(channel);
    }
    if (missing) {
        json_t *lr = irc2_log(ctx, "error", "IRC: server and channel must be configured", json_array());
        IA2_PROP(lr);
        json_free(lr);
        return irc2_fail(ctx, "config_missing", "IRC_SERVER and IRC_CHANNEL must be set", false);
    }
    /* if not self._acquire_platform_lock("irc", f"{server}:{nickname}", f"IRC identity {nickname}@{server}"): */
    const json_t *nick = irc2_req(ctx, "IRCAdapter", "nickname", &err);
    if (err) return err;
    char *srv_s = irc2_pystr(server);
    char *nick_s = irc2_pystr(nick);
    char *identity = malloc(strlen(srv_s) + strlen(nick_s) + 2);
    sprintf(identity, "%s:%s", srv_s, nick_s);
    char *resource = malloc(strlen(nick_s) + strlen(srv_s) + 32);
    sprintf(resource, "IRC identity %s@%s", nick_s, srv_s);
    free(srv_s); free(nick_s);
    json_t *la = irc2_args();
    irc2_set_s(la, "scope", "irc");
    irc2_set_s(la, "identity", identity);
    irc2_set_s(la, "resource_desc", resource);
    free(identity); free(resource);
    json_t *lock = irc2_call(ctx, "_acquire_platform_lock", la);
    IA2_PROP(lock);
    bool acquired = irc2_truthy(lock);
    json_free(lock);
    if (!acquired) return json_bool(false);
    /* try: self._reader, self._writer = await wait_for(open_connection(server, port, ssl=_ssl_ctx(use_tls)), 30) */
    const json_t *server2 = irc2_req(ctx, "IRCAdapter", "server", &err);
    if (err) return err;
    const json_t *port = irc2_req(ctx, "IRCAdapter", "port", &err);
    if (err) return err;
    const json_t *use_tls = irc2_req(ctx, "IRCAdapter", "use_tls", &err);
    if (err) return err;
    json_t *ssl = irc2_ssl_ctx(ctx, use_tls);
    json_t *conn_res = NULL;
    json_t *unpack_err = NULL;
    if (irc2_is_raise(ssl)) {
        conn_res = ssl;   /* treated as the try-body raise */
    } else {
        json_t *oa = irc2_args();
        irc2_set_copy(oa, "server", server2);
        irc2_set_copy(oa, "port", port);
        irc2_set_copy(oa, "ssl", ssl);
        json_free(ssl);
        conn_res = irc2_call(ctx, "asyncio.open_connection", oa);
        if (!irc2_is_raise(conn_res)) {
            json_t *r = NULL, *w = NULL;
            unpack_err = irc2_unpack2(conn_res, &r, &w);
            if (!unpack_err) {
                irc2_set_self(ctx, "_reader", r);
                irc2_set_self(ctx, "_writer", w);
            }
            json_free(conn_res);
            conn_res = NULL;
        }
    }
    /* except Exception as e: logger.error(...); return self._fail("connect_failed", str(e), retryable=True) */
    json_t *body_raise = conn_res ? conn_res : unpack_err;
    if (body_raise) {
        if (irc2_exc_is_base(irc2_kind(body_raise))) return body_raise;   /* BaseException passthrough */
        char *emsg = irc2_strdup(irc2_msg(body_raise));   /* snapshot BEFORE free */
        const json_t *server3 = irc2_req(ctx, "IRCAdapter", "server", &err);
        if (err) { json_free(body_raise); free(emsg); return err; }
        const json_t *port3 = irc2_req(ctx, "IRCAdapter", "port", &err);
        if (err) { json_free(body_raise); free(emsg); return err; }
        json_t *aa = json_array();
        json_append(aa, json_copy(server3));
        json_append(aa, json_copy(port3));
        json_append(aa, json_string(emsg));
        json_t *lr = irc2_log(ctx, "error", "IRC: failed to connect to %s:%s \xe2\x80\x94 %s", aa);
        json_free(body_raise);
        if (irc2_is_raise(lr)) { free(emsg); return lr; }
        json_free(lr);
        json_t *fr = irc2_fail(ctx, "connect_failed", emsg, true);
        free(emsg);
        return fr;
    }
    /* if self.server_password: await self._send_raw(f"PASS {server_password}") */
    const json_t *sp = irc2_req(ctx, "IRCAdapter", "server_password", &err);
    if (err) return err;
    if (irc2_truthy(sp)) {
        char *s = irc2_pystr(sp);
        char *line = malloc(strlen(s) + 8);
        sprintf(line, "PASS %s", s);
        free(s);
        json_t *r = irc2_send_raw(ctx, line);
        free(line);
        IA2_PROP(r);
        json_free(r);
    }
    /* await self._send_raw(f"NICK {nickname}") */
    const json_t *nick2 = irc2_req(ctx, "IRCAdapter", "nickname", &err);
    if (err) return err;
    {
        char *s = irc2_pystr(nick2);
        char *line = malloc(strlen(s) + 8);
        sprintf(line, "NICK %s", s);
        free(s);
        json_t *r = irc2_send_raw(ctx, line);
        free(line);
        IA2_PROP(r);
        json_free(r);
    }
    /* await self._send_raw(f"USER {nickname} 0 * :Hermes Agent") */
    const json_t *nick3 = irc2_req(ctx, "IRCAdapter", "nickname", &err);
    if (err) return err;
    {
        char *s = irc2_pystr(nick3);
        char *line = malloc(strlen(s) + 32);
        sprintf(line, "USER %s 0 * :Hermes Agent", s);
        free(s);
        json_t *r = irc2_send_raw(ctx, line);
        free(line);
        IA2_PROP(r);
        json_free(r);
    }
    /* self._recv_task = asyncio.create_task(self._receive_loop()) — coroutine
     * creation fires NO seam; the marker is the create_task arg. */
    json_t *cta = irc2_args();
    json_t *coro = json_object();
    irc2_set_s(coro, "__coro__", "_receive_loop");
    json_set(cta, "coro", coro);
    json_t *task = irc2_call(ctx, "asyncio.create_task", cta);
    IA2_PROP(task);
    irc2_set_self(ctx, "_recv_task", task);
    /* try: await wait_for(self._registration_event.wait(), 30) except TimeoutError */
    const json_t *ev = irc2_req(ctx, "IRCAdapter", "_registration_event", &err);
    if (err) return err;
    if (irc2_is_none(ev)) return irc2_attr_err("NoneType", "wait");
    json_t *wr = irc2_call(ctx, "registration_event.wait", irc2_args());
    if (irc2_is_raise(wr)) {
        if (irc2_exc_is_timeout(irc2_kind(wr))) {
            json_free(wr);
            json_t *lr = irc2_log(ctx, "error", "IRC: registration timed out", json_array());
            IA2_PROP(lr);
            json_free(lr);
            json_t *dr = irc2_disconnect_c(ctx);   /* REAL sibling */
            IA2_PROP(dr);
            json_free(dr);
            return irc2_fail(ctx, "registration_timeout", "IRC server did not send RPL_WELCOME", true);
        }
        return wr;   /* other raise propagates (except TimeoutError only) */
    }
    json_free(wr);
    /* if self.nickserv_password: _send_raw(IDENTIFY); sleep(2) */
    const json_t *np = irc2_req(ctx, "IRCAdapter", "nickserv_password", &err);
    if (err) return err;
    if (irc2_truthy(np)) {
        char *s = irc2_pystr(np);
        char *line = malloc(strlen(s) + 32);
        sprintf(line, "PRIVMSG NickServ :IDENTIFY %s", s);
        free(s);
        json_t *r = irc2_send_raw(ctx, line);
        free(line);
        IA2_PROP(r);
        json_free(r);
        json_t *sa = irc2_args();
        irc2_set_n(sa, "seconds", 2);
        json_t *sl = irc2_call(ctx, "asyncio.sleep", sa);
        IA2_PROP(sl);
        json_free(sl);
    }
    /* await self._send_raw(f"JOIN {channel}") */
    const json_t *chan2 = irc2_req(ctx, "IRCAdapter", "channel", &err);
    if (err) return err;
    {
        char *s = irc2_pystr(chan2);
        char *line = malloc(strlen(s) + 8);
        sprintf(line, "JOIN %s", s);
        free(s);
        json_t *r = irc2_send_raw(ctx, line);
        free(line);
        IA2_PROP(r);
        json_free(r);
    }
    /* self._mark_connected() */
    json_t *mc = irc2_call(ctx, "_mark_connected", irc2_args());
    IA2_PROP(mc);
    json_free(mc);
    /* logger.info("IRC: connected to %s:%s as %s, joined %s", server, port, _current_nick, channel) */
    const json_t *server4 = irc2_req(ctx, "IRCAdapter", "server", &err);
    if (err) return err;
    const json_t *port4 = irc2_req(ctx, "IRCAdapter", "port", &err);
    if (err) return err;
    const json_t *cn = irc2_req(ctx, "IRCAdapter", "_current_nick", &err);
    if (err) return err;
    const json_t *chan3 = irc2_req(ctx, "IRCAdapter", "channel", &err);
    if (err) return err;
    json_t *ia = json_array();
    json_append(ia, json_copy(server4));
    json_append(ia, json_copy(port4));
    json_append(ia, json_copy(cn));
    json_append(ia, json_copy(chan3));
    json_t *li = irc2_log(ctx, "info", "IRC: connected to %s:%s as %s, joined %s", ia);
    IA2_PROP(li);
    json_free(li);
    /* self._wire_plugin_handlers(None) */
    json_t *wa = irc2_args();
    irc2_set_null(wa, "native");
    json_t *wp = irc2_call(ctx, "_wire_plugin_handlers", wa);
    IA2_PROP(wp);
    json_free(wp);
    return json_bool(true);
}

/* ================================================================
 * interactive_setup nested helpers (REAL siblings)
 * ================================================================ */
/* info(*lines): for line in lines: print_info(line) */
static json_t *irc2_info_c(irc2_ctx_t *ctx, const json_t *lines) {
    if (lines && lines->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(lines); i++) {
            json_t *a = irc2_args();
            irc2_set_copy(a, "text", json_get(lines, i));
            json_t *r = irc2_call(ctx, "print_info", a);
            IA2_PROP(r);
            json_free(r);
        }
    }
    return json_null();
}
/* PoP: irc2_info @ plugins/platforms/irc/adapter.py:info */
static json_t *irc2_info(irc2_ctx_t *ctx, const json_t *args) {
    return irc2_info_c(ctx, json_obj_get(args, "lines"));
}

/* _required(label, env, default, what): prompt; falsy -> warn + False;
 * save_env_value(env, value.strip()); True. label/env/what travel as raw nodes
 * (seam args record them verbatim); the warning f-string str()-projects what. */
static json_t *irc2__required_c(irc2_ctx_t *ctx, const json_t *label, const json_t *env,
                                const json_t *deflt, const json_t *what) {
    json_t *pa = irc2_args();
    irc2_set_copy(pa, "question", label);
    irc2_set_copy(pa, "default", deflt);
    json_t *value = irc2_call(ctx, "prompt", pa);
    IA2_PROP(value);
    if (!irc2_truthy(value)) {
        json_free(value);
        char *what_s = irc2_pystr(what);
        char *msg = malloc(strlen(what_s) + 64);
        sprintf(msg, "%s is required \xe2\x80\x94 skipping IRC setup", what_s);
        free(what_s);
        json_t *wa = irc2_args();
        irc2_set_s(wa, "text", msg);
        free(msg);
        json_t *wr = irc2_call(ctx, "print_warning", wa);
        IA2_PROP(wr);
        json_free(wr);
        return json_bool(false);
    }
    const char *vs = irc2_str(value);
    if (!vs) {
        json_t *e = irc2_attr_err(irc2_type_name(value), "strip");
        json_free(value);
        return e;
    }
    char *stripped = irc2_strip(vs);
    json_free(value);
    json_t *sa = irc2_args();
    irc2_set_copy(sa, "key", env);
    irc2_set_s(sa, "value", stripped);
    free(stripped);
    json_t *sr = irc2_call(ctx, "save_env_value", sa);
    IA2_PROP(sr);
    json_free(sr);
    return json_bool(true);
}
/* PoP: irc2__required @ plugins/platforms/irc/adapter.py:_required */
static json_t *irc2__required(irc2_ctx_t *ctx, const json_t *args) {
    return irc2__required_c(ctx, json_obj_get(args, "label"), json_obj_get(args, "env"),
                            json_obj_get(args, "default"), json_obj_get(args, "what"));
}

/* ================================================================
 * interactive_setup
 * ================================================================ */
/* print() seam (bare). */
static json_t *irc2_print(irc2_ctx_t *ctx) {
    json_t *a = irc2_args();
    json_set(a, "args", json_array());
    return irc2_call(ctx, "print", a);
}
static json_t *irc2_print_text(irc2_ctx_t *ctx, const char *name, const char *text) {
    json_t *a = irc2_args();
    irc2_set_s(a, "text", text);
    return irc2_call(ctx, name, a);
}
static json_t *irc2_get_env(irc2_ctx_t *ctx, const char *key) {
    json_t *a = irc2_args();
    irc2_set_s(a, "key", key);
    return irc2_call(ctx, "get_env_value", a);
}
static json_t *irc2_save_env(irc2_ctx_t *ctx, const char *key, const char *value) {
    json_t *a = irc2_args();
    irc2_set_s(a, "key", key);
    irc2_set_s(a, "value", value);
    return irc2_call(ctx, "save_env_value", a);
}
static json_t *irc2_prompt_yes_no(irc2_ctx_t *ctx, const char *question, bool deflt) {
    json_t *a = irc2_args();
    irc2_set_s(a, "question", question);
    irc2_set_b(a, "default", deflt);
    return irc2_call(ctx, "prompt_yes_no", a);
}
/* PoP: irc2_interactive_setup @ plugins/platforms/irc/adapter.py:interactive_setup */
static json_t *irc2_interactive_setup(irc2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *r;
    /* print_header("IRC") */
    json_t *ha = irc2_args();
    irc2_set_s(ha, "title", "IRC");
    r = irc2_call(ctx, "print_header", ha);
    IA2_PROP(r); json_free(r);
    /* existing_server = get_env_value("IRC_SERVER") */
    json_t *existing_server = irc2_get_env(ctx, "IRC_SERVER");
    IA2_PROP(existing_server);
    /* if declines_reconfigure("IRC", "Reconfigure IRC?", "IRC_SERVER"): return */
    json_t *da = irc2_args();
    irc2_set_s(da, "label", "IRC");
    irc2_set_s(da, "question", "Reconfigure IRC?");
    json_t *envs = json_array();
    json_append(envs, json_string("IRC_SERVER"));
    json_set(da, "env_vars", envs);
    json_t *decl = irc2_call(ctx, "declines_reconfigure", da);
    IA2_PROP(decl);
    bool declines = irc2_truthy(decl);
    json_free(decl);
    if (declines) { json_free(existing_server); return json_null(); }
    /* info(two lines) */
    json_t *lines1 = json_array();
    json_append(lines1, json_string("Connect Hermes to an IRC network. Uses Python stdlib \xe2\x80\x94 no extra packages needed."));
    json_append(lines1, json_string("   Works with Libera.Chat, OFTC, your own ZNC/InspIRCd, etc."));
    r = irc2_info_c(ctx, lines1);
    json_free(lines1);
    IA2_PROP(r); json_free(r);
    /* print() */
    r = irc2_print(ctx); IA2_PROP(r); json_free(r);
    /* if not _required("IRC server hostname (e.g. irc.libera.chat)", "IRC_SERVER", existing_server or "", "Server"): return */
    json_t *def_srv = irc2_truthy(existing_server) ? json_copy(existing_server) : json_string("");
    json_free(existing_server);
    json_t *lbl1 = json_string("IRC server hostname (e.g. irc.libera.chat)");
    json_t *env1 = json_string("IRC_SERVER");
    json_t *what1 = json_string("Server");
    json_t *req1 = irc2__required_c(ctx, lbl1, env1, def_srv, what1);
    json_free(lbl1); json_free(env1); json_free(what1);
    json_free(def_srv);
    IA2_PROP(req1);
    bool ok1 = irc2_truthy(req1);
    json_free(req1);
    if (!ok1) return json_null();
    /* use_tls = prompt_yes_no("Use TLS (recommended)?", True) */
    json_t *use_tls = irc2_prompt_yes_no(ctx, "Use TLS (recommended)?", true);
    IA2_PROP(use_tls);
    bool tls = irc2_truthy(use_tls);
    json_free(use_tls);
    /* save_env_value("IRC_USE_TLS", "true" if use_tls else "false") */
    r = irc2_save_env(ctx, "IRC_USE_TLS", tls ? "true" : "false");
    IA2_PROP(r); json_free(r);
    const char *default_port = tls ? "6697" : "6667";
    /* port = prompt(f"Port (default {default_port})", default=get_env_value("IRC_PORT") or "") */
    json_t *gv_port = irc2_get_env(ctx, "IRC_PORT");
    IA2_PROP(gv_port);
    char *qport = malloc(strlen(default_port) + 32);
    sprintf(qport, "Port (default %s)", default_port);
    json_t *pa = irc2_args();
    irc2_set_s(pa, "question", qport);
    free(qport);
    irc2_set_copy(pa, "default", irc2_truthy(gv_port) ? gv_port : json_string(""));
    json_free(gv_port);
    json_t *port = irc2_call(ctx, "prompt", pa);
    IA2_PROP(port);
    if (irc2_truthy(port)) {
        /* try: save_env_value("IRC_PORT", str(int(port))) except ValueError: warn */
        long long pv = 0;
        char *errmsg = NULL;
        if (irc2_int_of(port, &pv, &errmsg)) {
            free(errmsg);
            char buf[40];
            snprintf(buf, sizeof buf, "%lld", pv);
            json_free(port);
            r = irc2_save_env(ctx, "IRC_PORT", buf);
            IA2_PROP(r); json_free(r);
        } else {
            bool is_value = errmsg && strncmp(errmsg, "invalid literal", 15) == 0;
            if (is_value) {
                free(errmsg);
                json_free(port);
                char *wm = malloc(strlen(default_port) + 48);
                sprintf(wm, "Invalid port \xe2\x80\x94 using default %s", default_port);
                r = irc2_print_text(ctx, "print_warning", wm);
                free(wm);
                IA2_PROP(r); json_free(r);
            } else {
                /* TypeError propagates (only ValueError caught) */
                json_t *e = irc2_err("TypeError", errmsg ? errmsg : "");
                free(errmsg);
                json_free(port);
                return e;
            }
        }
    } else {
        json_free(port);
        /* elif get_env_value("IRC_PORT"): save_env_value("IRC_PORT", "") */
        json_t *gv2 = irc2_get_env(ctx, "IRC_PORT");
        IA2_PROP(gv2);
        bool has = irc2_truthy(gv2);
        json_free(gv2);
        if (has) { r = irc2_save_env(ctx, "IRC_PORT", ""); IA2_PROP(r); json_free(r); }
    }
    /* for (nickname, channel): if not _required(label, env, get_env_value(env) or "", what): return */
    {
        json_t *gv = irc2_get_env(ctx, "IRC_NICKNAME");
        IA2_PROP(gv);
        json_t *deflt = irc2_truthy(gv) ? json_copy(gv) : json_string("");
        json_free(gv);
        json_t *lbl = json_string("Bot nickname (e.g. hermes-bot)");
        json_t *envn = json_string("IRC_NICKNAME");
        json_t *whatn = json_string("Nickname");
        json_t *req = irc2__required_c(ctx, lbl, envn, deflt, whatn);
        json_free(lbl); json_free(envn); json_free(whatn);
        json_free(deflt);
        IA2_PROP(req);
        bool ok = irc2_truthy(req);
        json_free(req);
        if (!ok) return json_null();
    }
    {
        json_t *gv = irc2_get_env(ctx, "IRC_CHANNEL");
        IA2_PROP(gv);
        json_t *deflt = irc2_truthy(gv) ? json_copy(gv) : json_string("");
        json_free(gv);
        json_t *lbl = json_string("Channel to join (e.g. #hermes \xe2\x80\x94 comma-separate for multiple)");
        json_t *envn = json_string("IRC_CHANNEL");
        json_t *whatn = json_string("Channel");
        json_t *req = irc2__required_c(ctx, lbl, envn, deflt, whatn);
        json_free(lbl); json_free(envn); json_free(whatn);
        json_free(deflt);
        IA2_PROP(req);
        bool ok = irc2_truthy(req);
        json_free(req);
        if (!ok) return json_null();
    }
    /* print() */
    r = irc2_print(ctx); IA2_PROP(r); json_free(r);
    /* info("🔑 Optional authentication", "   Leave blank to skip.") */
    json_t *lines2 = json_array();
    json_append(lines2, json_string("\xf0\x9f\x94\x91 Optional authentication"));
    json_append(lines2, json_string("   Leave blank to skip."));
    r = irc2_info_c(ctx, lines2);
    json_free(lines2);
    IA2_PROP(r); json_free(r);
    /* auth loop: server password then nickserv password */
    {
        json_t *yn = irc2_prompt_yes_no(ctx, "Configure a server password (PASS command)?", false);
        IA2_PROP(yn);
        bool yes = irc2_truthy(yn);
        json_free(yn);
        if (yes) {
            json_t *sa = irc2_args();
            irc2_set_s(sa, "question", "Server password");
            irc2_set_b(sa, "password", true);
            json_t *secret = irc2_call(ctx, "prompt", sa);
            IA2_PROP(secret);
            if (irc2_truthy(secret)) {
                json_t *sva = irc2_args();
                irc2_set_s(sva, "key", "IRC_SERVER_PASSWORD");
                irc2_set_copy(sva, "value", secret);
                json_free(secret);
                r = irc2_call(ctx, "save_env_value", sva);
                IA2_PROP(r); json_free(r);
            } else json_free(secret);
        }
    }
    {
        json_t *yn = irc2_prompt_yes_no(ctx, "Identify with NickServ on connect?", false);
        IA2_PROP(yn);
        bool yes = irc2_truthy(yn);
        json_free(yn);
        if (yes) {
            json_t *sa = irc2_args();
            irc2_set_s(sa, "question", "NickServ password");
            irc2_set_b(sa, "password", true);
            json_t *secret = irc2_call(ctx, "prompt", sa);
            IA2_PROP(secret);
            if (irc2_truthy(secret)) {
                json_t *sva = irc2_args();
                irc2_set_s(sva, "key", "IRC_NICKSERV_PASSWORD");
                irc2_set_copy(sva, "value", secret);
                json_free(secret);
                r = irc2_call(ctx, "save_env_value", sva);
                IA2_PROP(r); json_free(r);
            } else json_free(secret);
        }
    }
    /* print() */
    r = irc2_print(ctx); IA2_PROP(r); json_free(r);
    /* info(access control, 4 lines) */
    json_t *lines3 = json_array();
    json_append(lines3, json_string("\xf0\x9f\x94\x92 Access control: restrict who can message the bot"));
    json_append(lines3, json_string("   IRC nicks are not authenticated \xe2\x80\x94 anyone can claim any nick."));
    json_append(lines3, json_string("   For public channels, pair with NickServ-only mode on your network"));
    json_append(lines3, json_string("   if you want stronger identity guarantees."));
    r = irc2_info_c(ctx, lines3);
    json_free(lines3);
    IA2_PROP(r); json_free(r);
    /* if prompt_yes_no("Allow all users ...?", False): */
    json_t *allow_all = irc2_prompt_yes_no(ctx, "Allow all users in the channel to talk to the bot?", false);
    IA2_PROP(allow_all);
    bool all = irc2_truthy(allow_all);
    json_free(allow_all);
    if (all) {
        r = irc2_save_env(ctx, "IRC_ALLOW_ALL_USERS", "true"); IA2_PROP(r); json_free(r);
        r = irc2_save_env(ctx, "IRC_ALLOWED_USERS", ""); IA2_PROP(r); json_free(r);
        r = irc2_print_text(ctx, "print_warning", "\xe2\x9a\xa0\xef\xb8\x8f  Open access \xe2\x80\x94 any nick in the channel can command the bot.");
        IA2_PROP(r); json_free(r);
    } else {
        r = irc2_save_env(ctx, "IRC_ALLOW_ALL_USERS", "false"); IA2_PROP(r); json_free(r);
        json_t *gv = irc2_get_env(ctx, "IRC_ALLOWED_USERS");
        IA2_PROP(gv);
        json_t *pa2 = irc2_args();
        irc2_set_s(pa2, "question", "Allowed nicks (comma-separated, leave empty to deny everyone)");
        irc2_set_copy(pa2, "default", irc2_truthy(gv) ? gv : json_string(""));
        json_free(gv);
        json_t *allowed = irc2_call(ctx, "prompt", pa2);
        IA2_PROP(allowed);
        if (irc2_truthy(allowed)) {
            const char *as = irc2_str(allowed);
            if (!as) {
                json_t *e = irc2_attr_err(irc2_type_name(allowed), "replace");
                json_free(allowed);
                return e;
            }
            char *repl = irc2_replace_all(as, " ", "");
            json_free(allowed);
            r = irc2_save_env(ctx, "IRC_ALLOWED_USERS", repl);
            free(repl);
            IA2_PROP(r); json_free(r);
            r = irc2_print_text(ctx, "print_success", "Allowlist configured");
            IA2_PROP(r); json_free(r);
        } else {
            json_free(allowed);
            r = irc2_save_env(ctx, "IRC_ALLOWED_USERS", ""); IA2_PROP(r); json_free(r);
            r = irc2_print_text(ctx, "print_info", "No nicks allowed \xe2\x80\x94 the bot will ignore all messages until you add nicks.");
            IA2_PROP(r); json_free(r);
        }
    }
    /* print() */
    r = irc2_print(ctx); IA2_PROP(r); json_free(r);
    /* print_success / print_info finale */
    r = irc2_print_text(ctx, "print_success", "IRC configuration saved to ~/.hermes/.env");
    IA2_PROP(r); json_free(r);
    r = irc2_print_text(ctx, "print_info", "Restart the gateway for changes to take effect: hermes gateway restart");
    IA2_PROP(r); json_free(r);
    return json_null();
}

/* ================================================================
 * _StandaloneConn.__init__
 * ================================================================ */
/* PoP: irc2_conn_init @ plugins/platforms/irc/adapter.py:__init__ */
static json_t *irc2_conn_init(irc2_ctx_t *ctx, const json_t *args) {
    /* self.reader, self.writer = reader, writer (None must survive as null:
     * json_set drops a NULL value, losing the state key entirely). */
    json_t *rd = json_obj_get(args, "reader");
    irc2_set_self(ctx, "reader", rd ? json_copy(rd) : json_null());
    json_t *wr = json_obj_get(args, "writer");
    irc2_set_self(ctx, "writer", wr ? json_copy(wr) : json_null());
    /* self._loop = asyncio.get_running_loop() */
    json_t *loop = irc2_call(ctx, "asyncio.get_running_loop", irc2_args());
    IA2_PROP(loop);
    irc2_set_self(ctx, "_loop", loop);
    return json_null();
}

/* ================================================================
 * _on_registration / _on_join pump closures
 * ================================================================ */
/* PoP: irc2__on_registration @ plugins/platforms/irc/adapter.py:_on_registration */
static json_t *irc2__on_registration(irc2_ctx_t *ctx, const json_t *args) {
    const char *cmd = json_get_str(args, "cmd", "");
    if (strcmp(cmd, "432") == 0 || strcmp(cmd, "433") == 0) {
        /* nick_attempts += 1 (nonlocal cell) */
        const json_t *na = irc2_self(ctx, "nick_attempts");
        long long attempts = (na && na->type == JSON_NUMBER) ? (long long)na->num_val : 0;
        attempts += 1;
        irc2_set_self(ctx, "nick_attempts", json_number((double)attempts));
        if (attempts > 5) return irc2_sa_error(ctx, "too many nick collisions");
        /* standalone_nick = f"{nick_base}-cron-{nick_attempts}"[:30] */
        const json_t *nb = irc2_self(ctx, "nick_base");
        char *base = irc2_pystr(nb);
        char *buf = malloc(strlen(base) + 40);
        sprintf(buf, "%s-cron-%lld", base, attempts);
        free(base);
        char *nick = irc2_slice(buf, 30);
        free(buf);
        irc2_set_self(ctx, "standalone_nick", json_string(nick));
        /* await conn.raw(f"NICK {standalone_nick}") */
        char *line = malloc(strlen(nick) + 8);
        sprintf(line, "NICK %s", nick);
        json_t *err = NULL;
        json_t *r = irc2_conn_raw(ctx, json_obj_get(args, "conn"), line, &err);
        free(line); free(nick);
        if (err) return err;
        IA2_PROP(r);
        json_free(r);
    } else if (strcmp(cmd, "464") == 0 || strcmp(cmd, "465") == 0) {
        char buf[128];
        snprintf(buf, sizeof buf, "server rejected client (%s)", cmd);
        return irc2_sa_error(ctx, buf);
    }
    return strcmp(cmd, "001") == 0 ? json_bool(true) : json_null();
}
/* PoP: irc2__on_join @ plugins/platforms/irc/adapter.py:_on_join */
static json_t *irc2__on_join(irc2_ctx_t *ctx, const json_t *args) {
    const char *cmd = json_get_str(args, "cmd", "");
    if (strcmp(cmd, "403") == 0 || strcmp(cmd, "405") == 0 || strcmp(cmd, "471") == 0 ||
        strcmp(cmd, "473") == 0 || strcmp(cmd, "474") == 0 || strcmp(cmd, "475") == 0) {
        const json_t *tg = irc2_self(ctx, "target");
        char *target = irc2_pystr(tg);
        char *buf = malloc(strlen(target) + strlen(cmd) + 32);
        sprintf(buf, "JOIN %s rejected (%s)", target, cmd);
        free(target);
        json_t *r = irc2_sa_error(ctx, buf);
        free(buf);
        return r;
    }
    return (strcmp(cmd, "366") == 0 || strcmp(cmd, "JOIN") == 0) ? json_bool(true) : json_null();
}

/* ================================================================
 * register (plugin entry point)
 * ================================================================ */
/* PoP: irc2_register @ plugins/platforms/irc/adapter.py:register */
static json_t *irc2_register(irc2_ctx_t *ctx, const json_t *args) {
    const json_t *ctxv = json_obj_get(args, "ctx");
    if (irc2_is_none(ctxv)) return irc2_attr_err("NoneType", "register_platform");
    json_t *kw = json_object();
    irc2_set_s(kw, "name", "irc");
    irc2_set_s(kw, "dashboard_description", "Relay messages between an IRC channel (or DMs) and Hermes.");
    irc2_set_s(kw, "docs_url", "https://hermes-agent.nousresearch.com/docs/user-guide/messaging/irc");
    irc2_set_s(kw, "label", "IRC");
    json_t *af = json_object(); irc2_set_s(af, "__cls__", "IRCAdapter");
    json_set(kw, "adapter_factory", af);
    json_t *cf = json_object(); irc2_set_s(cf, "__fn__", "check_requirements");
    json_set(kw, "check_fn", cf);
    json_t *vc = json_object(); irc2_set_s(vc, "__fn__", "validate_config");
    json_set(kw, "validate_config", vc);
    json_t *ic = json_object(); irc2_set_s(ic, "__fn__", "is_connected");
    json_set(kw, "is_connected", ic);
    json_t *re = json_array();
    json_append(re, json_string("IRC_SERVER"));
    json_append(re, json_string("IRC_CHANNEL"));
    json_append(re, json_string("IRC_NICKNAME"));
    json_set(kw, "required_env", re);
    irc2_set_s(kw, "install_hint", "No extra packages needed (stdlib only)");
    json_t *sf = json_object(); irc2_set_s(sf, "__fn__", "interactive_setup");
    json_set(kw, "setup_fn", sf);
    json_t *ee = json_object(); irc2_set_s(ee, "__fn__", "_env_enablement");
    json_set(kw, "env_enablement_fn", ee);
    irc2_set_s(kw, "cron_deliver_env_var", "IRC_HOME_CHANNEL");
    json_t *ss = json_object(); irc2_set_s(ss, "__fn__", "_standalone_send");
    json_set(kw, "standalone_sender_fn", ss);
    irc2_set_s(kw, "allowed_users_env", "IRC_ALLOWED_USERS");
    irc2_set_s(kw, "allow_all_env", "IRC_ALLOW_ALL_USERS");
    irc2_set_n(kw, "max_message_length", 450);
    irc2_set_s(kw, "emoji", "\xf0\x9f\x92\xac");
    irc2_set_b(kw, "pii_safe", false);
    irc2_set_b(kw, "allow_update_command", true);
    irc2_set_s(kw, "platform_hint",
        "You are chatting via IRC. IRC does not support markdown formatting \xe2\x80\x94 use plain text only. "
        "Messages are limited to ~450 characters per line (long messages are automatically split). "
        "In channels, users address you by prefixing your nick. Keep responses concise and conversational.");
    json_t *r = irc2_call(ctx, "ctx.register_platform", kw);
    IA2_PROP(r);
    json_free(r);
    return json_null();
}