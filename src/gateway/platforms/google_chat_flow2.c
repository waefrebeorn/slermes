/*
 * google_chat_flow2.c -- UNCLAIMED remainder FLOW cores of
 * plugins/platforms/google_chat/adapter.py (hermes-agent), third wave.
 * google_chat_adapter_flow / _flow2 already claimed the module decision
 * chains, the credential store, the inbound Pub/Sub + message dispatch and
 * the outbound send/edit/delete/patch/create cores; this file ports the
 * still-unclaimed leaves: the format/delegation wrappers (format_message,
 * send_animation/document/image_file/video/voice, _send_file_reply), the
 * auth/token helpers (_verify_google_id_token, _get_google_id_token_request,
 * _load_google_modules, _new_authed_http, _refresh_or_none), the quiet patch
 * / typing-card / attachment-fallback helpers, the per-user chat API chain
 * (_acquire_user_chat_api, _load_per_user_chat_api), the Pub/Sub loop hand-off
 * (_on_pubsub_message, _submit_on_loop) and the in-chat setup delegation.
 *
 * Decision sequences only: every live I/O / injected callable / sibling
 * method / asyncio primitive / foreign import / logger / translation call
 * enters through ONE universal seam: gc2_call(ctx, name, args). The harness
 * wires it to a recording stub fed the case's scripted results (in.io); the
 * oracle installs recording stubs emitting the identical event shape
 * {"name","args","ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and gc2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_google_chat_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)} (OSError adds
 *   errno; JSONDecodeError carries its suffix). BaseException kinds pass
 *   through every except-Exception site; Exception kinds are swallowed.
 * - Attribute carriers (config / credentials / module doubles) travel as
 *   {"__obj__": {attrs}}; a plain JSON object IS a Python dict.
 * - SendResult returns project to the full 8-field dataclass dict; tuple
 *   returns project to arrays.
 * - logger seams record {"fmt":<literal>,"args":[...]}; exc_info kwargs drop.
 * - t() seams record {"key":<literal>,"kwargs":{...}}.
 * - In-function imports / asyncio.to_thread / context.run fire one seam each.
 * - Fixture corpus is ASCII + integral numbers; no non-integral float repr.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GC2_MAX_TEXT_LENGTH 4000
#define GC2_RATE_LIMIT_WARN_THRESHOLD 5
#define GC2_RETRY_MAX_ATTEMPTS 3
#define GC2_RETRY_BASE_DELAY 1.0
#define GC2_RETRY_MAX_DELAY 8.0
#define GC2_RETRY_JITTER 0.3
#define GC2_TYPING_CONSUMED_SENTINEL "<consumed>"
#define GC2_PARKED_MSG "The routed profile is not being served right now; retry this delivery"

/* -- universal seam context -- */
typedef struct gc2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} gc2_ctx_t;

/* -- exception hierarchy (FULL builtin tree + asyncio.CancelledError +
 * JSONDecodeError + the model classes). Aliases IOError/EnvironmentError/
 * WindowsError map to OSError; ExceptionGroup carries BOTH bases. -- */
typedef struct { const char *name, *p1, *p2; } gc2_exc_ent_t;
static const gc2_exc_ent_t GC2_EXC_TABLE[] = {
    {"BaseException", NULL, NULL},
    {"Exception", "BaseException", NULL},
    {"SystemExit", "BaseException", NULL},
    {"KeyboardInterrupt", "BaseException", NULL},
    {"GeneratorExit", "BaseException", NULL},
    {"CancelledError", "BaseException", NULL},
    {"BaseExceptionGroup", "BaseException", NULL},
    {"ExceptionGroup", "BaseExceptionGroup", "Exception"},
    {"StopIteration", "Exception", NULL},
    {"StopAsyncIteration", "Exception", NULL},
    {"ArithmeticError", "Exception", NULL},
    {"FloatingPointError", "ArithmeticError", NULL},
    {"OverflowError", "ArithmeticError", NULL},
    {"ZeroDivisionError", "ArithmeticError", NULL},
    {"AssertionError", "Exception", NULL},
    {"AttributeError", "Exception", NULL},
    {"BufferError", "Exception", NULL},
    {"EOFError", "Exception", NULL},
    {"ImportError", "Exception", NULL},
    {"ModuleNotFoundError", "ImportError", NULL},
    {"LookupError", "Exception", NULL},
    {"IndexError", "LookupError", NULL},
    {"KeyError", "LookupError", NULL},
    {"MemoryError", "Exception", NULL},
    {"NameError", "Exception", NULL},
    {"UnboundLocalError", "NameError", NULL},
    {"OSError", "Exception", NULL},
    {"IOError", "OSError", NULL},
    {"EnvironmentError", "OSError", NULL},
    {"WindowsError", "OSError", NULL},
    {"BlockingIOError", "OSError", NULL},
    {"ChildProcessError", "OSError", NULL},
    {"ConnectionError", "OSError", NULL},
    {"BrokenPipeError", "ConnectionError", NULL},
    {"ConnectionAbortedError", "ConnectionError", NULL},
    {"ConnectionRefusedError", "ConnectionError", NULL},
    {"ConnectionResetError", "ConnectionError", NULL},
    {"FileExistsError", "OSError", NULL},
    {"FileNotFoundError", "OSError", NULL},
    {"InterruptedError", "OSError", NULL},
    {"IsADirectoryError", "OSError", NULL},
    {"NotADirectoryError", "OSError", NULL},
    {"PermissionError", "OSError", NULL},
    {"ProcessLookupError", "OSError", NULL},
    {"TimeoutError", "OSError", NULL},
    {"ReferenceError", "Exception", NULL},
    {"RuntimeError", "Exception", NULL},
    {"NotImplementedError", "RuntimeError", NULL},
    {"RecursionError", "RuntimeError", NULL},
    {"PythonFinalizationError", "RuntimeError", NULL},
    {"SyntaxError", "Exception", NULL},
    {"IndentationError", "SyntaxError", NULL},
    {"TabError", "IndentationError", NULL},
    {"_IncompleteInputError", "SyntaxError", NULL},
    {"SystemError", "Exception", NULL},
    {"TypeError", "Exception", NULL},
    {"ValueError", "Exception", NULL},
    {"UnicodeError", "ValueError", NULL},
    {"UnicodeDecodeError", "UnicodeError", NULL},
    {"UnicodeEncodeError", "UnicodeError", NULL},
    {"UnicodeTranslateError", "UnicodeError", NULL},
    {"JSONDecodeError", "ValueError", NULL},
    {"Warning", "Exception", NULL},
    {"UserWarning", "Warning", NULL},
    {"DeprecationWarning", "Warning", NULL},
    {"PendingDeprecationWarning", "Warning", NULL},
    {"SyntaxWarning", "Warning", NULL},
    {"RuntimeWarning", "Warning", NULL},
    {"FutureWarning", "Warning", NULL},
    {"ImportWarning", "Warning", NULL},
    {"UnicodeWarning", "Warning", NULL},
    {"BytesWarning", "Warning", NULL},
    {"ResourceWarning", "Warning", NULL},
    {"EncodingWarning", "Warning", NULL},
    {"HttpError", "Exception", NULL},
    {"NotFound", "Exception", NULL},
    {"PermissionDenied", "Exception", NULL},
    {"Unauthenticated", "Exception", NULL},
    {"InstallError", "Exception", NULL},
    {"ParkedRouteRefused", "Exception", NULL},
    {"_SACredentialError", "Exception", NULL},
};

static const gc2_exc_ent_t *gc2_exc_ent(const char *name) {
    for (size_t i = 0; i < sizeof(GC2_EXC_TABLE) / sizeof(GC2_EXC_TABLE[0]); i++)
        if (strcmp(GC2_EXC_TABLE[i].name, name) == 0) return &GC2_EXC_TABLE[i];
    return NULL;
}

/* issubclass(kind, ancestor) over the table; unknown kinds are Exception
 * subclasses (the oracle's dynamic type(kind,(Exception,),{})). The aliases
 * IOError/EnvironmentError/WindowsError ARE OSError in CPython, so normalize
 * both sides to OSError before walking. */
static const char *gc2_exc_alias(const char *n) {
    if (strcmp(n, "IOError") == 0 || strcmp(n, "EnvironmentError") == 0 ||
        strcmp(n, "WindowsError") == 0) return "OSError";
    return n;
}
static bool gc2_isa(const char *kind, const char *ancestor) {
    if (!kind || !ancestor) return false;
    kind = gc2_exc_alias(kind);
    ancestor = gc2_exc_alias(ancestor);
    if (strcmp(kind, ancestor) == 0) return true;
    const char *cur = kind;
    for (int depth = 0; depth < 16 && cur; depth++) {
        if (strcmp(cur, ancestor) == 0) return true;
        const gc2_exc_ent_t *e = gc2_exc_ent(cur);
        const char *p1 = e ? e->p1 : "Exception";
        const char *p2 = e ? e->p2 : NULL;
        if (p2 && gc2_isa(p2, ancestor)) return true;
        if (!p1) return false;
        if (strcmp(p1, cur) == 0) return false;
        cur = p1;
    }
    return false;
}

/* -- basic helpers -- */
static bool gc2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
/* A raise an except-Exception site SWALLOWS (Exception family). */
static bool gc2_caught_exc(const json_t *r) {
    return gc2_is_raise(r) && gc2_isa(json_get_str(r, "__raise__", ""), "Exception");
}
/* A raise that PASSES THROUGH every except-Exception site (BaseException). */
static bool gc2_raise_passes(const json_t *r) {
    return gc2_is_raise(r) && !gc2_isa(json_get_str(r, "__raise__", ""), "Exception");
}

static bool gc2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool gc2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *gc2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool gc2_str_eq(const json_t *v, const char *s) {
    const char *t = gc2_str(v);
    return t && s && strcmp(t, s) == 0;
}
static json_t *gc2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* CPython OSError(errno, msg) PROMOTES to the errno's subclass when the
 * constructed class is OSError itself (IOError alias included). Map measured
 * against the quarry interpreter (Windows errno values; POSIX values like
 * ECONNRESET=104 do NOT promote on this build). */
static const char *gc2_oserror_subclass(double en) {
    long long e = (long long)en;
    switch (e) {
        case 1: case 13: return "PermissionError";
        case 2: return "FileNotFoundError";
        case 3: return "ProcessLookupError";
        case 4: return "InterruptedError";
        case 10: return "ChildProcessError";
        case 11: case 10035: case 10036: case 10037: return "BlockingIOError";
        case 17: return "FileExistsError";
        case 20: return "NotADirectoryError";
        case 21: return "IsADirectoryError";
        case 32: case 10058: return "BrokenPipeError";
        case 10053: return "ConnectionAbortedError";
        case 10054: return "ConnectionResetError";
        case 10061: return "ConnectionRefusedError";
        case 138: case 10060: return "TimeoutError";
        default: return "OSError";
    }
}
/* Kinds the oracle's EXC_CLASSES maps to a REAL OSError subclass.
 * EnvironmentError/WindowsError are ABSENT there -> dynamic Exception
 * subclasses (no errno formatting, no errno projection). */
static bool gc2_oserror_kind(const char *type) {
    if (strcmp(type, "EnvironmentError") == 0 || strcmp(type, "WindowsError") == 0)
        return false;
    return gc2_isa(type, "OSError");
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *gc2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    if (strcmp(type, "ParkedRouteRefused") == 0)
        return gc2_err(type, GC2_PARKED_MSG);
    if (strcmp(type, "_SACredentialError") == 0) {
        /* live: super().__init__(kind) -> str(exc) == kind (spec kind, else msg) */
        json_t *e = gc2_err(type, json_get_str(spec, "kind", msg));
        const json_t *kind = json_obj_get(spec, "kind");
        json_set(e, "kind", kind ? json_copy(kind) : json_string(msg));
        const json_t *detail = json_obj_get(spec, "detail");
        if (detail && detail->type == JSON_OBJECT) {
            /* oracle: e.detail = make_exc({type: det.__raise__, message:
             * det.message}) — the inner exception is MATERIALIZED (a
             * JSONDecodeError detail gains its ": line L column C (char P)"
             * suffix), then projected back to an envelope. */
            json_t *dspec = json_object();
            json_set_hdr(dspec, "type", json_get_str(detail, "__raise__", "Exception"));
            json_set_hdr(dspec, "message", json_get_str(detail, "message", ""));
            json_t *mat = gc2_make_exc(dspec);
            json_free(dspec);
            json_set(e, "detail", mat);
        } else {
            json_set(e, "detail", json_null());
        }
        return e;
    }
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && gc2_oserror_kind(type)) {
        const char *cls = gc2_exc_alias(type);
        if (strcmp(cls, "OSError") == 0) cls = gc2_oserror_subclass(en->num_val);
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = gc2_err(cls, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    if (strcmp(type, "JSONDecodeError") == 0) {
        /* str(JSONDecodeError(msg, doc, pos)) appends ": line L column C
         * (char P)" with L = doc.count('\n', 0, pos) + 1 and
         * C = pos - doc.rfind('\n', 0, pos) (rfind -1 when absent). */
        const char *doc = json_get_str(spec, "doc", "");
        long long pos = (long long)json_get_num(spec, "pos", 0);
        long long lineno = 1, lastnl = -1;
        for (long long i = 0; i < pos && doc[i]; i++)
            if (doc[i] == '\n') { lineno++; lastnl = i; }
        long long colno = pos - lastnl;
        size_t need = strlen(msg) + 96;
        char *buf = malloc(need);
        if (!buf) return gc2_err(type, msg);
        snprintf(buf, need, "%s: line %lld column %lld (char %lld)",
                 msg, lineno, colno, pos);
        json_t *e = gc2_err(type, buf);
        free(buf);
        return e;
    }
    return gc2_err(type, msg);
}

/* The one universal seam (mirrors gca_call / the oracle Ctx.call). args is
 * ADOPTED into the trace event; the return is a fresh owned node. A
 * missing/exhausted script RETURNS a fail-closed RuntimeError spec. */
static json_t *gc2_call(gc2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = gc2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = gc2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = gc2_make_exc(raise);
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

/* -- string helpers (Python str semantics on the ASCII fixture corpus) -- */
static char *gc2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static bool gc2_is_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f) || c == ' ';
}
static char *gc2_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && gc2_is_ws((unsigned char)s[a])) a++;
    while (b > a && gc2_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}
static char *gc2_lstrip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0;
    while (a < n && gc2_is_ws((unsigned char)s[a])) a++;
    return gc2_strdup(s + a);
}
static char *gc2_lower(const char *s) {
    char *o = gc2_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *gc2_upper(const char *s) {
    char *o = gc2_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)toupper((unsigned char)*p);
    return o;
}
static bool gc2_contains(const char *hay, const char *needle) {
    return hay && needle && strstr(hay, needle) != NULL;
}
static bool gc2_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool gc2_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}
static char *gc2_replace(const char *s, const char *from, const char *to) {
    if (!s) s = "";
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return gc2_strdup(s);
    size_t cap = strlen(s) + 1, n = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    const char *p = s;
    while (*p) {
        if (strncmp(p, from, fl) == 0) {
            size_t need = n + tl + strlen(p + fl) + 1;
            if (need > cap) { cap = need * 2; char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
            memcpy(o + n, to, tl); n += tl; p += fl;
        } else {
            if (n + 2 > cap) { cap = cap * 2 + strlen(p); char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
            o[n++] = *p++;
        }
    }
    o[n] = '\0';
    return o;
}

/* Python type name for a JSON value. */
static const char *gc2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
/* str() projection for the fixture domain (str/int/bool/None). */
static char *gc2_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return gc2_strdup("None");
    switch (v->type) {
        case JSON_STRING: return gc2_strdup(v->str_val);
        case JSON_BOOL: return gc2_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 9.2e18)
                snprintf(buf, sizeof buf, "%.0f", d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return gc2_strdup(buf);
        }
        default: return NULL;
    }
}

/* -- UTF-8 helpers (Python len counts codepoints) -- */
static size_t gc2_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xc0) != 0x80) n++;
    return n;
}
static const char *gc2_cp_ptr(const char *s, long long i) {
    if (!s) return "";
    if (i < 0) i = 0;
    const char *p = s;
    while (i > 0 && *p) {
        p++;
        while (*p && ((unsigned char)*p & 0xc0) == 0x80) p++;
        i--;
    }
    return p;
}
static char *gc2_slice_head(const char *s, long long b) {
    if (!s) s = "";
    long long n = (long long)gc2_utf8_len(s);
    if (b < 0) b = n + b;
    if (b < 0) b = 0;
    if (b > n) b = n;
    const char *pb = gc2_cp_ptr(s, b);
    size_t len = (size_t)(pb - s);
    char *o = malloc(len + 1);
    if (!o) return NULL;
    memcpy(o, s, len); o[len] = '\0';
    return o;
}

/* -- object / attribute carriers / state -- */
static const json_t *gc2_obj_attrs(const json_t *v) {
    if (v && v->type == JSON_OBJECT) {
        json_t *inner = json_obj_get(v, "__obj__");
        if (inner && inner->type == JSON_OBJECT) return inner;
    }
    return NULL;
}
/* Direct attribute read on a carrier (self.config.home_channel etc.): absent
 * attr / null carrier raises AttributeError, matching live SimpleNamespace. */
static json_t *gc2_direct_attr(const json_t *node, const char *name) {
    char buf[160];
    if (gc2_is_null(node)) {
        snprintf(buf, sizeof buf, "'NoneType' object has no attribute '%s'", name);
        return gc2_err("AttributeError", buf);
    }
    const json_t *attrs = gc2_obj_attrs(node);
    if (!attrs) {
        snprintf(buf, sizeof buf, "'%s' object has no attribute '%s'", gc2_type_name(node), name);
        return gc2_err("AttributeError", buf);
    }
    if (!json_has(attrs, name)) {
        snprintf(buf, sizeof buf, "'types.SimpleNamespace' object has no attribute '%s'", name);
        return gc2_err("AttributeError", buf);
    }
    return json_copy(json_obj_get(attrs, name));
}
static const json_t *gc2_state(const gc2_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}
static json_t *gc2_state_obj(gc2_ctx_t *ctx, const char *key) {
    json_t *v = ctx->state ? json_obj_get(ctx->state, key) : NULL;
    if (!v || v->type != JSON_OBJECT) {
        v = json_object();
        json_set(ctx->state, key, v);
    }
    return v;
}
/* dict.get on a value that must be a dict; non-dict -> AttributeError raise. */
static const json_t *gc2_dict_get(const json_t *v, const char *key) {
    if (v && v->type == JSON_OBJECT) return json_obj_get(v, key);
    return NULL;
}
static json_t *gc2_get_attr_err(const json_t *v, const char *meth) {
    char buf[160];
    snprintf(buf, sizeof buf, "'%s' object has no attribute '%s'", gc2_type_name(v), meth);
    return gc2_err("AttributeError", buf);
}

/* project a raise envelope into an exc double for seam args (oracle proj). */
static json_t *gc2_exc_double(const json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "cls", json_get_str(spec, "__raise__", "Exception"));
    json_set_hdr(e, "msg", json_get_str(spec, "message", ""));
    json_set(e, "attrs", json_object());
    return e;
}

/* -- logger seam helper -- */
static json_t *gc2_log(gc2_ctx_t *ctx, const char *level, const char *fmt, json_t *args) {
    char name[64];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *a = json_object();
    json_set_hdr(a, "fmt", fmt);
    json_set(a, "args", args ? args : json_array());
    return gc2_call(ctx, name, a);
}

/* -- _exc_text seam helper (exc double + targets) -- */
static json_t *gc2_exc_text(gc2_ctx_t *ctx, const json_t *exc_spec, json_t *targets) {
    json_t *a = json_object();
    json_set(a, "exc", gc2_exc_double(exc_spec));
    json_set(a, "targets", targets ? targets : json_array());
    return gc2_call(ctx, "_exc_text", a);
}

/* -- POSIX path helpers -- */
static char *gc2_path_join(const char *home, const char *name) {
    if (!home) home = "";
    if (!home[0]) return gc2_strdup(name);
    size_t n = strlen(home);
    while (n > 0 && home[n - 1] == '/') n--;
    if (n == 0) {  /* home was all slashes -> root */
        char *o = malloc(strlen(name) + 2);
        if (o) { o[0] = '/'; strcpy(o + 1, name); }
        return o;
    }
    char *o = malloc(n + strlen(name) + 2);
    if (o) { memcpy(o, home, n); o[n] = '/'; strcpy(o + n + 1, name); }
    return o;
}
static char *gc2_path_dirname(const char *p) {
    if (!p) p = "";
    const char *slash = strrchr(p, '/');
    if (!slash) return gc2_strdup(".");
    if (slash == p) return gc2_strdup("/");
    size_t n = (size_t)(slash - p);
    char *o = malloc(n + 1);
    if (o) { memcpy(o, p, n); o[n] = '\0'; }
    return o;
}
static char *gc2_path_with_suffix(const char *p, const char *suf) {
    if (!p) p = "";
    const char *base = strrchr(p, '/');
    base = base ? base + 1 : p;
    size_t headlen = (size_t)(base - p);
    const char *dot = strrchr(base, '.');
    size_t baselen;
    if (dot && dot != base) baselen = (size_t)(dot - base);
    else baselen = strlen(base);
    char *o = malloc(headlen + baselen + strlen(suf) + 1);
    if (o) { memcpy(o, p, headlen); memcpy(o + headlen, base, baselen); strcpy(o + headlen + baselen, suf); }
    return o;
}
/* Path.suffix: ".ext" of the basename ("" when none / dotfile). */
static char *gc2_path_suffix(const char *p) {
    if (!p) p = "";
    const char *base = strrchr(p, '/');
    base = base ? base + 1 : p;
    const char *dot = strrchr(base, '.');
    if (dot && dot != base) return gc2_strdup(dot);
    return gc2_strdup("");
}
/* strip ONE leading U+FEFF BOM (EF BB BF) after a utf-8-sig read seam. */
static const char *gc2_strip_bom(const char *s) {
    if (!s) return "";
    if ((unsigned char)s[0] == 0xef && (unsigned char)s[1] == 0xbb && (unsigned char)s[2] == 0xbf)
        return s + 3;
    return s;
}

/* -- JSON string escaping + json.dumps (Python default / compact) -- */
typedef struct { char *buf; size_t cap, len; } gc2_sb_t;
static void gc2_sb_putc(gc2_sb_t *s, char c) {
    if (s->len + 2 >= s->cap) { s->cap = s->cap ? s->cap * 2 : 128; s->buf = realloc(s->buf, s->cap); }
    s->buf[s->len++] = c; s->buf[s->len] = '\0';
}
static void gc2_sb_puts(gc2_sb_t *s, const char *t) { while (t && *t) gc2_sb_putc(s, *t++); }
static void gc2_sb_json_str(gc2_sb_t *s, const char *str) {
    gc2_sb_putc(s, '"');
    if (!str) str = "";
    for (const unsigned char *p = (const unsigned char *)str; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"': gc2_sb_puts(s, "\\\""); break;
            case '\\': gc2_sb_puts(s, "\\\\"); break;
            case '\b': gc2_sb_puts(s, "\\b"); break;
            case '\f': gc2_sb_puts(s, "\\f"); break;
            case '\n': gc2_sb_puts(s, "\\n"); break;
            case '\r': gc2_sb_puts(s, "\\r"); break;
            case '\t': gc2_sb_puts(s, "\\t"); break;
            default:
                if (c < 0x20) { char e[8]; snprintf(e, sizeof e, "\\u%04x", c); gc2_sb_puts(s, e); }
                else gc2_sb_putc(s, (char)c);
        }
    }
    gc2_sb_putc(s, '"');
}
static void gc2_dumps_node(gc2_sb_t *s, const json_t *n, bool compact) {
    if (!n || n->type == JSON_NULL) { gc2_sb_puts(s, "null"); return; }
    switch (n->type) {
        case JSON_BOOL: gc2_sb_puts(s, n->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char tmp[64];
            double d = n->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 9.2e18) snprintf(tmp, sizeof tmp, "%.0f", d);
            else snprintf(tmp, sizeof tmp, "%.17g", d);
            gc2_sb_puts(s, tmp);
            break;
        }
        case JSON_STRING: gc2_sb_json_str(s, n->str_val); break;
        case JSON_ARRAY:
            gc2_sb_putc(s, '[');
            for (size_t i = 0; i < n->c.count; i++) {
                if (i) gc2_sb_puts(s, compact ? "," : ", ");
                gc2_dumps_node(s, n->c.items[i], compact);
            }
            gc2_sb_putc(s, ']');
            break;
        case JSON_OBJECT:
            gc2_sb_putc(s, '{');
            for (size_t i = 0; i < n->c.count; i++) {
                if (i) gc2_sb_puts(s, compact ? "," : ", ");
                gc2_sb_json_str(s, n->c.keys[i]);
                gc2_sb_puts(s, compact ? ":" : ": ");
                gc2_dumps_node(s, n->c.items[i], compact);
            }
            gc2_sb_putc(s, '}');
            break;
        default: gc2_sb_puts(s, "null");
    }
}
static char *gc2_dumps(const json_t *n, bool compact) {
    gc2_sb_t s = {NULL, 0, 0};
    gc2_dumps_node(&s, n, compact);
    if (!s.buf) return gc2_strdup("null");
    return s.buf;
}

/* -- SendResult (full 8-field dataclass projection) -- */
static json_t *gc2_sr_new(bool success, const json_t *message_id, const json_t *error, bool retryable) {
    json_t *r = json_object();
    json_set(r, "success", json_bool(success));
    json_set(r, "message_id", message_id ? json_copy(message_id) : json_null());
    json_set(r, "error", error ? json_copy(error) : json_null());
    json_set(r, "raw_response", json_null());
    json_set(r, "retryable", json_bool(retryable));
    json_set(r, "retry_after", json_null());
    json_set(r, "continuation_message_ids", json_array());
    json_set(r, "error_kind", json_null());
    return r;
}
/* normalize a scripted seam dict into the full SendResult shape (defaults). */
static json_t *gc2_sr_norm(const json_t *raw) {
    static const char *keys[] = {"success", "message_id", "error", "raw_response",
                                 "retryable", "retry_after", "continuation_message_ids", "error_kind"};
    json_t *r = json_object();
    for (size_t i = 0; i < 8; i++) {
        const json_t *v = (raw && raw->type == JSON_OBJECT) ? json_obj_get(raw, keys[i]) : NULL;
        if (v) { json_set(r, keys[i], json_copy(v)); continue; }
        if (strcmp(keys[i], "success") == 0 || strcmp(keys[i], "retryable") == 0)
            json_set(r, keys[i], json_bool(false));
        else if (strcmp(keys[i], "continuation_message_ids") == 0)
            json_set(r, keys[i], json_array());
        else
            json_set(r, keys[i], json_null());
    }
    return r;
}

/* unpack a 2-sequence seam result; bad shape -> raise spec. */
static json_t *gc2_unpack2(const json_t *r, json_t **a, json_t **b) {
    if (gc2_is_null(r))
        return gc2_err("TypeError", "cannot unpack non-iterable NoneType object");
    if (r->type != JSON_ARRAY) {
        char buf[128];
        snprintf(buf, sizeof buf, "cannot unpack non-iterable %s object", gc2_type_name(r));
        return gc2_err("TypeError", buf);
    }
    size_t n = json_len(r);
    if (n != 2) {
        char buf[128];
        if (n < 2) snprintf(buf, sizeof buf, "not enough values to unpack (expected 2, got %zu)", n);
        else snprintf(buf, sizeof buf, "too many values to unpack (expected 2, got %zu)", n);
        return gc2_err("ValueError", buf);
    }
    *a = json_get(r, 0); *b = json_get(r, 1);
    return NULL;
}

/* =====================================================================
 * Additional helpers for google_chat_flow2
 * ===================================================================== */

/* value-node string accessor with "" fallback. */
static const char *gc2_sval(const json_t *v) { const char *s = gc2_str(v); return s ? s : ""; }

/* t(key, **kwargs) seam: args {key, kwargs}. */
static json_t *gc2_t(gc2_ctx_t *ctx, const char *key, json_t *kwargs) {
    json_t *a = json_object();
    json_set_hdr(a, "key", key);
    json_set(a, "kwargs", kwargs ? kwargs : json_object());
    return gc2_call(ctx, "t", a);
}

/* logger args helper: {fmt, args}. */
static json_t *gc2_logargs(const char *fmt, json_t *args) {
    json_t *a = json_object();
    json_set_hdr(a, "fmt", fmt);
    json_set(a, "args", args ? args : json_array());
    return a;
}

static json_t *gc2_proxy_error(gc2_ctx_t *ctx, const json_t *exc) {
    json_t *a = json_object();
    json_set(a, "exc", gc2_exc_double(exc));
    return gc2_call(ctx, "_proxy_error", a);
}
static json_t *gc2_http_status(gc2_ctx_t *ctx, const json_t *exc) {
    json_t *a = json_object();
    json_set(a, "exc", gc2_exc_double(exc));
    return gc2_call(ctx, "_http_status", a);
}

/* dict.get on a fixture dict (args/kwargs/state carriers). */
static const json_t *gc2_dget(const json_t *v, const char *key) {
    return (v && v->type == JSON_OBJECT) ? json_obj_get(v, key) : NULL;
}

/* SendResult object from the full 8-field dict (defaults applied). */
static json_t *gc2_sr(const json_t *raw) { return gc2_sr_norm(raw); }
static json_t *gc2_sr_fail(const char *error) {
    json_t *r = gc2_sr_new(false, NULL, json_string(error ? error : ""), false);
    return r;
}

/* =====================================================================
 * Module-level flows
 * ===================================================================== */

/* PoP: gc2_format_message @ plugins/platforms/google_chat/adapter.py:format_message */
static json_t *gc2_format_message(gc2_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "content", json_copy(json_obj_get(args, "content") ? json_obj_get(args, "content") : json_null()));
    return gc2_call(ctx, "_format_message", a);
}

/* PoP: gc2__verify_google_id_token @ plugins/platforms/google_chat/adapter.py:_verify_google_id_token */
static json_t *gc2__verify_google_id_token(gc2_ctx_t *ctx, const json_t *args) {
    /* from google.oauth2 import id_token */
    json_t *ia = json_object();
    json_t *fl = json_array(); json_append(fl, json_string("id_token"));
    json_set(ia, "fromlist", fl);
    json_t *mod = gc2_call(ctx, "import.google.oauth2", ia);
    if (gc2_is_raise(mod)) {
        if (gc2_isa(json_get_str(mod, "__raise__", ""), "ImportError"))
            return gc2_err("RuntimeError", "google-auth is required for Google Chat HTTP callbacks");
        return mod;
    }
    /* _get_google_id_token_request() */
    json_t *req = gc2_call(ctx, "_get_google_id_token_request", json_object());
    if (gc2_is_raise(req)) return req;
    json_t *a = json_object();
    json_set(a, "token", json_copy(json_obj_get(args, "token") ? json_obj_get(args, "token") : json_null()));
    json_set(a, "request", req);
    json_set(a, "audience", json_copy(json_obj_get(args, "audience") ? json_obj_get(args, "audience") : json_null()));
    return gc2_call(ctx, "id_token.verify_oauth2_token", a);
}

/* PoP: gc2__get_google_id_token_request @ plugins/platforms/google_chat/adapter.py:_get_google_id_token_request */
static json_t *gc2__get_google_id_token_request(gc2_ctx_t *ctx, const json_t *args) {
    const json_t *cached = gc2_state(ctx, "_google_id_token_request");
    if (!gc2_is_null(cached)) return json_copy(cached);
    /* with lock: (silent) ; import google.auth.transport.requests */
    json_t *ia = json_object();
    json_t *fl = json_array(); json_append(fl, json_string("requests"));
    json_set(ia, "fromlist", fl);
    json_t *mod = gc2_call(ctx, "import.google.auth.transport", ia);
    if (gc2_is_raise(mod)) {
        if (gc2_isa(json_get_str(mod, "__raise__", ""), "ImportError"))
            return gc2_err("RuntimeError", "google-auth is required for Google Chat HTTP callbacks");
        return mod;
    }
    json_t *req = gc2_call(ctx, "google_requests.Request", json_object());
    if (gc2_is_raise(req)) return req;
    json_t *a = json_object();
    json_set(a, "request", req);
    json_t *val = gc2_call(ctx, "_CachedGoogleAuthRequest", a);
    if (gc2_is_raise(val)) return val;
    json_set(ctx->state, "_google_id_token_request", json_copy(val));
    return val;
}

/* PoP: gc2__load_google_modules @ plugins/platforms/google_chat/adapter.py:_load_google_modules */
static json_t *gc2__load_google_modules(gc2_ctx_t *ctx, const json_t *args) {
    const json_t *loaded_flag = gc2_state(ctx, "_google_modules_loaded");
    if (gc2_truthy(loaded_flag)) {
        const json_t *avail = gc2_state(ctx, "GOOGLE_CHAT_AVAILABLE");
        return json_copy(avail && !gc2_is_null(avail) ? avail : json_null());
    }
    json_set(ctx->state, "_google_modules_loaded", json_bool(true));
    /* the module's real _GOOGLE_IMPORTS table (name, module, attr|NULL) */
    static const char *IMPORTS[][3] = {
        {"httplib2", "httplib2", NULL},
        {"pubsub_v1", "google.cloud.pubsub_v1", NULL},
        {"gax_exceptions", "google.api_core.exceptions", NULL},
        {"service_account", "google.oauth2.service_account", NULL},
        {"AuthorizedHttp", "google_auth_httplib2", "AuthorizedHttp"},
        {"build_service", "googleapiclient.discovery", "build"},
        {"HttpError", "googleapiclient.errors", "HttpError"},
        {"MediaFileUpload", "googleapiclient.http", "MediaFileUpload"},
    };
    const size_t NIMP = sizeof(IMPORTS) / sizeof(IMPORTS[0]);
    json_t *loaded = json_object();  /* globals().update(loaded) is AFTER the comprehension */
    for (size_t i = 0; i < NIMP; i++) {
        const char *name = IMPORTS[i][0];
        const char *module = IMPORTS[i][1];
        const char *attrn = IMPORTS[i][2];
        json_t *ia = json_object();
        json_set_hdr(ia, "module", module);
        json_t *mod = gc2_call(ctx, "importlib.import_module", ia);
        if (gc2_is_raise(mod)) {
            if (gc2_isa(json_get_str(mod, "__raise__", ""), "ImportError")) {
                json_set(ctx->state, "GOOGLE_CHAT_AVAILABLE", json_bool(false));
                return json_bool(false);
            }
            return mod;
        }
        json_t *val = mod;
        if (attrn) {
            json_t *ga = json_object();
            json_set(ga, "obj", mod);
            json_set_hdr(ga, "attr", attrn);
            val = gc2_call(ctx, "getattr", ga);
            if (gc2_is_raise(val)) return val;  /* except ImportError only */
        }
        json_set(loaded, name, val);
    }
    for (size_t i = 0; i < loaded->c.count; i++)
        json_set(ctx->state, loaded->c.keys[i], json_copy(loaded->c.items[i]));
    json_free(loaded);
    json_set(ctx->state, "GOOGLE_CHAT_AVAILABLE", json_bool(true));
    return json_bool(true);
}

/* =====================================================================
 * Adapter method flows
 * ===================================================================== */

/* PoP: gc2_send_animation @ plugins/platforms/google_chat/adapter.py:send_animation */
static json_t *gc2_send_animation(gc2_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "chat_id", json_copy(json_obj_get(args, "chat_id") ? json_obj_get(args, "chat_id") : json_null()));
    json_set(a, "image_url", json_copy(json_obj_get(args, "animation_url") ? json_obj_get(args, "animation_url") : json_null()));
    json_set(a, "caption", json_copy(json_obj_get(args, "caption") ? json_obj_get(args, "caption") : json_null()));
    json_set(a, "reply_to", json_copy(json_obj_get(args, "reply_to") ? json_obj_get(args, "reply_to") : json_null()));
    json_set(a, "metadata", json_copy(json_obj_get(args, "metadata") ? json_obj_get(args, "metadata") : json_null()));
    return gc2_call(ctx, "send_image", a);
}

/* Helper: the file-reply family. mime_hint is a literal (or NULL for send_document). */
static json_t *gc2_send_file_reply_impl(gc2_ctx_t *ctx, const json_t *args,
                                        const char *mime_hint, bool use_mime_field) {
    const json_t *path = json_obj_get(args, "path");
    if (!path) path = json_obj_get(args, "file_path");
    json_t *a = json_object();
    json_set(a, "chat_id", json_copy(json_obj_get(args, "chat_id") ? json_obj_get(args, "chat_id") : json_null()));
    json_set(a, "path", json_copy(path ? path : json_null()));
    json_set(a, "caption", json_copy(json_obj_get(args, "caption") ? json_obj_get(args, "caption") : json_null()));
    json_set(a, "reply_to", json_copy(json_obj_get(args, "reply_to") ? json_obj_get(args, "reply_to") : json_null()));
    json_set(a, "kwargs", json_copy(json_obj_get(args, "kwargs") ? json_obj_get(args, "kwargs") : json_object()));
    if (use_mime_field) {
        const json_t *mh = json_obj_get(args, "mime_hint");
        json_set(a, "mime_hint", mh ? json_copy(mh) : json_null());
    } else {
        json_set(a, "mime_hint", mime_hint ? json_string(mime_hint) : json_null());
    }
    json_set(a, "override_filename", json_copy(json_obj_get(args, "override_filename") ? json_obj_get(args, "override_filename") :
                                               json_obj_get(args, "file_name") ? json_obj_get(args, "file_name") : json_null()));
    return gc2_call(ctx, "_send_file_reply", a);
}
/* PoP: gc2_send_document @ plugins/platforms/google_chat/adapter.py:send_document */
static json_t *gc2_send_document(gc2_ctx_t *ctx, const json_t *args) {
    return gc2_send_file_reply_impl(ctx, args, NULL, true);
}
/* PoP: gc2_send_image_file @ plugins/platforms/google_chat/adapter.py:send_image_file */
static json_t *gc2_send_image_file(gc2_ctx_t *ctx, const json_t *args) {
    return gc2_send_file_reply_impl(ctx, args, "image/*", false);
}
/* PoP: gc2_send_video @ plugins/platforms/google_chat/adapter.py:send_video */
static json_t *gc2_send_video(gc2_ctx_t *ctx, const json_t *args) {
    return gc2_send_file_reply_impl(ctx, args, "video/mp4", false);
}
/* PoP: gc2_send_voice @ plugins/platforms/google_chat/adapter.py:send_voice */
static json_t *gc2_send_voice(gc2_ctx_t *ctx, const json_t *args) {
    return gc2_send_file_reply_impl(ctx, args, "audio/ogg", false);
}

/* PoP: gc2__send_file_reply @ plugins/platforms/google_chat/adapter.py:_send_file_reply */
static json_t *gc2__send_file_reply(gc2_ctx_t *ctx, const json_t *args) {
    const json_t *kwargs = json_obj_get(args, "kwargs");
    json_t *a = json_object();
    json_set(a, "reply_to", json_copy(json_obj_get(args, "reply_to") ? json_obj_get(args, "reply_to") : json_null()));
    json_set(a, "metadata", json_copy(gc2_dget(kwargs, "metadata") ? gc2_dget(kwargs, "metadata") : json_null()));
    json_set(a, "chat_id", json_copy(json_obj_get(args, "chat_id") ? json_obj_get(args, "chat_id") : json_null()));
    json_t *thread_id = gc2_call(ctx, "_resolve_thread_id", a);
    if (gc2_is_raise(thread_id)) return thread_id;
    json_t *b = json_object();
    json_set(b, "chat_id", json_copy(json_obj_get(args, "chat_id") ? json_obj_get(args, "chat_id") : json_null()));
    json_set(b, "path", json_copy(json_obj_get(args, "path") ? json_obj_get(args, "path") : json_null()));
    json_set(b, "caption", json_copy(json_obj_get(args, "caption") ? json_obj_get(args, "caption") : json_null()));
    json_set(b, "mime_hint", json_copy(json_obj_get(args, "mime_hint") ? json_obj_get(args, "mime_hint") : json_null()));
    json_set(b, "thread_id", thread_id);
    json_set(b, "override_filename", json_copy(json_obj_get(args, "override_filename") ? json_obj_get(args, "override_filename") : json_null()));
    return gc2_call(ctx, "_send_file", b);
}

/* PoP: gc2__new_authed_http @ plugins/platforms/google_chat/adapter.py:_new_authed_http */
static json_t *gc2__new_authed_http(gc2_ctx_t *ctx, const json_t *args) {
    json_t *ha = json_object();
    json_set(ha, "timeout", json_number(30));
    json_t *http = gc2_call(ctx, "httplib2.Http", ha);
    if (gc2_is_raise(http)) return http;
    json_t *a = json_object();
    json_set(a, "credentials", json_copy(gc2_state(ctx, "_credentials") ? gc2_state(ctx, "_credentials") : json_null()));
    json_set(a, "http", http);
    return gc2_call(ctx, "AuthorizedHttp", a);
}

/* PoP: gc2__handle_setup_files_command @ plugins/platforms/google_chat/adapter.py:_handle_setup_files_command */
static json_t *gc2__handle_setup_files_command(gc2_ctx_t *ctx, const json_t *args) {
    json_t *ia = json_object();
    json_t *fl = json_array(); json_append(fl, json_string("handle_setup_files_command"));
    json_set(ia, "fromlist", fl);
    json_t *mod = gc2_call(ctx, "import.setup_files", ia);
    if (gc2_is_raise(mod)) return mod;
    json_t *a = json_object();
    json_set(a, "chat_id", json_copy(json_obj_get(args, "chat_id") ? json_obj_get(args, "chat_id") : json_null()));
    json_set(a, "thread_id", json_copy(json_obj_get(args, "thread_id") ? json_obj_get(args, "thread_id") : json_null()));
    json_set(a, "raw_text", json_copy(json_obj_get(args, "raw_text") ? json_obj_get(args, "raw_text") : json_null()));
    json_set(a, "sender_email", json_copy(json_obj_get(args, "sender_email") ? json_obj_get(args, "sender_email") : json_null()));
    return gc2_call(ctx, "handle_setup_files_command", a);
}

/* PoP: gc2__refresh_or_none @ plugins/platforms/google_chat/adapter.py:_refresh_or_none */
static json_t *gc2__refresh_or_none(gc2_ctx_t *ctx, const json_t *args) {
    json_t *ia = json_object();
    json_t *fl = json_array(); json_append(fl, json_string("refresh_or_none"));
    json_set(ia, "fromlist", fl);
    json_t *mod = gc2_call(ctx, "import.oauth", ia);
    if (gc2_is_raise(mod)) return mod;
    json_t *ta = json_object();
    json_set_hdr(ta, "fn", "refresh_or_none");
    json_t *sa = json_object();
    json_set(sa, "creds", json_copy(json_obj_get(args, "creds") ? json_obj_get(args, "creds") : json_null()));
    json_set(sa, "email", json_copy(json_obj_get(args, "email") ? json_obj_get(args, "email") : json_null()));
    json_set(ta, "args", sa);
    json_t *ret = gc2_call(ctx, "asyncio.to_thread", ta);
    if (!gc2_is_raise(ret)) return ret;
    if (gc2_raise_passes(ret)) return ret;
    /* except Exception */
    json_t *pe = gc2_proxy_error(ctx, ret);
    if (gc2_is_raise(pe)) return pe;
    json_t *a = json_object();
    json_set_hdr(a, "fmt", gc2_sval(json_obj_get(args, "log_msg")));
    json_set(a, "args", json_array());
    gc2_call(ctx, "logger.debug", a);
    return NULL;
}

/* PoP: gc2__patch_quietly @ plugins/platforms/google_chat/adapter.py:_patch_quietly */
static json_t *gc2__patch_quietly(gc2_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "message_name", json_copy(json_obj_get(args, "message_name") ? json_obj_get(args, "message_name") : json_null()));
    json_t *body = json_object();
    json_set(body, "text", json_copy(json_obj_get(args, "text") ? json_obj_get(args, "text") : json_null()));
    json_set(a, "body", body);
    json_t *ret = gc2_call(ctx, "_patch_message", a);
    if (!gc2_is_raise(ret)) return NULL;
    if (gc2_raise_passes(ret)) return ret;
    json_t *pe = gc2_proxy_error(ctx, ret);
    if (gc2_is_raise(pe)) return pe;
    json_t *la = json_object();
    json_set_hdr(la, "fmt", gc2_sval(json_obj_get(args, "log_msg")));
    json_set(la, "args", json_copy(json_obj_get(args, "log_args") ? json_obj_get(args, "log_args") : json_array()));
    gc2_call(ctx, "logger.debug", la);
    return NULL;
}

/* PoP: gc2__on_pubsub_message @ plugins/platforms/google_chat/adapter.py:_on_pubsub_message */
static json_t *gc2__on_pubsub_message(gc2_ctx_t *ctx, const json_t *args) {
    const json_t *scope = gc2_state(ctx, "_scope_ctx");
    json_t *c = json_object();
    json_t *cp;
    if (!gc2_is_null(scope)) {
        cp = gc2_call(ctx, "scope_ctx.copy", c);
    } else {
        cp = gc2_call(ctx, "contextvars.copy_context", c);
    }
    if (gc2_is_raise(cp)) return cp;
    /* ctx.run(self._handle_pubsub_message, message): ignore ctx ret (no return) */
    json_t *a = json_object();
    json_set_hdr(a, "fn", "_handle_pubsub_message");
    json_set(a, "message", json_copy(json_obj_get(args, "message") ? json_obj_get(args, "message") : json_null()));
    json_t *ret = gc2_call(ctx, "context.run", a);
    if (gc2_is_raise(ret)) return ret;
    return NULL;
}

/* PoP: gc2__post_attachment_fallback @ plugins/platforms/google_chat/adapter.py:_post_attachment_fallback */
static json_t *gc2__post_attachment_fallback(gc2_ctx_t *ctx, const json_t *args) {
    const json_t *filename = json_obj_get(args, "filename");
    const json_t *path = json_obj_get(args, "path");
    const json_t *caption = json_obj_get(args, "caption");
    /* four t() seams, joined with '\n' */
    json_t *kw = json_object(); json_set(kw, "filename", json_copy(filename ? filename : json_null()));
    json_t *l1 = gc2_t(ctx, "platform.google_chat.attachment_fallback.header", kw);
    if (gc2_is_raise(l1)) return l1;
    json_t *l2 = gc2_t(ctx, "platform.google_chat.attachment_fallback.explain", json_object());
    if (gc2_is_raise(l2)) return l2;
    json_t *l3 = gc2_t(ctx, "platform.google_chat.attachment_fallback.activate", json_object());
    if (gc2_is_raise(l3)) return l3;
    json_t *kw4 = json_object(); json_set(kw4, "path", json_copy(path ? path : json_null()));
    json_t *l4 = gc2_t(ctx, "platform.google_chat.attachment_fallback.host_path", kw4);
    if (gc2_is_raise(l4)) return l4;
    char notice[8192];
    snprintf(notice, sizeof notice, "%s\n%s\n%s\n%s",
             gc2_str(l1) ? gc2_str(l1) : "", gc2_str(l2) ? gc2_str(l2) : "",
             gc2_str(l3) ? gc2_str(l3) : "", gc2_str(l4) ? gc2_str(l4) : "");
    char text[8192];
    if (gc2_truthy(caption)) {
        char *cs = gc2_str_of(caption);
        snprintf(text, sizeof text, "%s\n%s", cs ? cs : "", notice);
        free(cs);
    } else {
        snprintf(text, sizeof text, "%s", notice);
    }
    const char *cap_or_empty = gc2_truthy(caption) ? gc2_str(caption) : "";
    json_t *wa = json_object();
    json_set_hdr(wa, "text", text);
    json_set_hdr(wa, "caption", cap_or_empty ? cap_or_empty : "");
    json_t *body = gc2_call(ctx, "warning_text", wa);
    if (gc2_is_raise(body)) return body;
    if (gc2_truthy(body)) {
        json_t *ba = json_object();
        json_set_hdr(ba, "body", gc2_str(body) ? gc2_str(body) : "");
        json_set(ba, "thread_id", json_copy(json_obj_get(args, "thread_id") ? json_obj_get(args, "thread_id") : json_null()));
        json_t *tb = gc2_call(ctx, "_thread_body", ba);
        if (gc2_is_raise(tb)) {
            if (!gc2_raise_passes(tb)) {
                json_t *pe = gc2_proxy_error(ctx, tb);
                if (gc2_is_raise(pe)) return pe;
                json_t *la = json_object();
                json_set_hdr(la, "fmt", "[GoogleChat] attachment fallback notice send failed");
                json_set(la, "args", json_array());
                gc2_call(ctx, "logger.debug", la);
                goto fallback_ret;
            }
            return tb;
        }
        json_t *ca = json_object();
        json_set(ca, "chat_id", json_copy(json_obj_get(args, "chat_id") ? json_obj_get(args, "chat_id") : json_null()));
        json_set(ca, "body", tb);
        json_t *cr = gc2_call(ctx, "_create_message", ca);
        if (gc2_is_raise(cr)) {
            if (!gc2_raise_passes(cr)) {
                json_t *pe = gc2_proxy_error(ctx, cr);
                if (gc2_is_raise(pe)) return pe;
                json_t *la = json_object();
                json_set_hdr(la, "fmt", "[GoogleChat] attachment fallback notice send failed");
                json_set(la, "args", json_array());
                gc2_call(ctx, "logger.debug", la);
                goto fallback_ret;
            }
            return cr;
        }
    }
fallback_ret:
    return gc2_sr_fail("google_chat: native attachment requires user OAuth — run /setup-files in chat");
}

/* PoP: gc2__consume_typing_card_with_text @ plugins/platforms/google_chat/adapter.py:_consume_typing_card_with_text */
static json_t *gc2__consume_typing_card_with_text(gc2_ctx_t *ctx, const json_t *args) {
    json_t *tmap = gc2_state_obj(ctx, "_typing_messages");
    const char *chat_id = gc2_sval(json_obj_get(args, "chat_id"));
    const json_t *current = gc2_dget(tmap, chat_id);
    if (!gc2_truthy(current) || gc2_str_eq(current, "<consumed>")) return NULL;
    json_t *cur = json_copy(current);
    json_obj_del(tmap, chat_id);
    json_t *a = json_object();
    json_set(a, "message_name", cur);
    json_t *body = json_object();
    json_set(body, "text", json_copy(json_obj_get(args, "text") ? json_obj_get(args, "text") : json_null()));
    json_set(a, "body", body);
    json_t *ret = gc2_call(ctx, "_patch_message", a);
    if (gc2_is_raise(ret)) {
        if (gc2_isa(json_get_str(ret, "__raise__", ""), "HttpError")) {
            json_t *st = gc2_http_status(ctx, ret);
            if (gc2_is_raise(st)) return st;
            if (st->type == JSON_NUMBER && st->num_val == 404.0)
                return NULL;
            return ret;
        }
        return ret;
    }
    json_set(tmap, chat_id, json_string("<consumed>"));
    return gc2_sr(ret);
}

/* PoP: gc2__acquire_user_chat_api @ plugins/platforms/google_chat/adapter.py:_acquire_user_chat_api */
static json_t *gc2__acquire_user_chat_api(gc2_ctx_t *ctx, const json_t *args) {
    const json_t *email = json_obj_get(args, "sender_email");
    if (gc2_truthy(email)) {
        json_t *a = json_object();
        json_set(a, "email", json_copy(email));
        json_t *api = gc2_call(ctx, "_load_per_user_chat_api", a);
        if (gc2_is_raise(api)) return api;
        if (!gc2_is_null(api)) {
            json_t *r = json_array();
            json_append(r, api); json_append(r, json_copy(email));
            return r;
        }
    }
    const json_t *legacy = gc2_state(ctx, "_user_chat_api");
    if (!gc2_is_null(legacy)) {
        json_t *a = json_object();
        json_set(a, "creds", json_copy(gc2_state(ctx, "_user_credentials") ? gc2_state(ctx, "_user_credentials") : json_null()));
        json_set(a, "email", json_null());
        json_set_hdr(a, "log_msg", "[GoogleChat] legacy creds refresh raised");
        json_t *refreshed = gc2_call(ctx, "_refresh_or_none", a);
        if (gc2_is_raise(refreshed)) return refreshed;
        if (gc2_is_null(refreshed)) {
            gc2_call(ctx, "logger.warning", gc2_logargs("[GoogleChat] legacy user-OAuth refresh returned None — evicting fallback creds", NULL));
            json_t *ia = json_object(); json_set_hdr(ia, "identity", "__legacy__");
            json_t *inv = gc2_call(ctx, "_invalidate_user_creds", ia);
            if (gc2_is_raise(inv)) return inv;
            json_t *r = json_array(); json_append(r, json_null()); json_append(r, json_null());
            return r;
        }
        json_set(ctx->state, "_user_credentials", json_copy(refreshed));
        json_t *r = json_array();
        json_append(r, json_copy(legacy)); json_append(r, json_string("__legacy__"));
        return r;
    }
    json_t *r = json_array(); json_append(r, json_null()); json_append(r, json_null());
    return r;
}

/* PoP: gc2__load_per_user_chat_api @ plugins/platforms/google_chat/adapter.py:_load_per_user_chat_api */
static json_t *gc2__load_per_user_chat_api(gc2_ctx_t *ctx, const json_t *args) {
    json_t *ia = json_object();
    json_t *fl = json_array();
    json_append(fl, json_string("load_user_credentials"));
    json_append(fl, json_string("build_user_chat_service"));
    json_set(ia, "fromlist", fl);
    json_t *mod = gc2_call(ctx, "import.oauth", ia);
    if (gc2_is_raise(mod)) return mod;
    const json_t *email = json_obj_get(args, "email");
    const char *es = gc2_sval(email);
    json_t *apis = gc2_state_obj(ctx, "_user_chat_api_by_email");
    json_t *creds_map = gc2_state_obj(ctx, "_user_creds_by_email");
    const json_t *cached_api = gc2_dget(apis, es);
    const json_t *cached_creds = gc2_dget(creds_map, es);
    if (!gc2_is_null(cached_api) && !gc2_is_null(cached_creds)) {
        json_t *a = json_object();
        json_set(a, "creds", json_copy(cached_creds));
        json_set(a, "email", json_copy(email ? email : json_null()));
        json_set_hdr(a, "log_msg", "[GoogleChat] cached per-user refresh raised");
        json_t *refreshed = gc2_call(ctx, "_refresh_or_none", a);
        if (gc2_is_raise(refreshed)) return refreshed;
        if (gc2_is_null(refreshed)) {
            json_t *ia2 = json_object(); json_set(ia2, "identity", json_copy(email ? email : json_null()));
            json_t *inv = gc2_call(ctx, "_invalidate_user_creds", ia2);
            if (gc2_is_raise(inv)) return inv;
            return NULL;
        }
        json_set(creds_map, es, json_copy(refreshed));
        return json_copy(cached_api);
    }
    /* try: creds = await asyncio.to_thread(_load, email) */
    json_t *ta = json_object();
    json_set_hdr(ta, "fn", "load_user_credentials");
    json_t *sa = json_object(); json_set(sa, "email", json_copy(email ? email : json_null()));
    json_set(ta, "args", sa);
    json_t *creds = gc2_call(ctx, "asyncio.to_thread", ta);
    if (gc2_is_raise(creds)) {
        if (gc2_raise_passes(creds)) return creds;
        goto load_except;
    }
    if (gc2_is_null(creds)) return NULL;
    json_t *tb = json_object();
    json_set_hdr(tb, "fn", "build_user_chat_service");
    json_t *sb = json_object(); json_set(sb, "creds", json_copy(creds));
    json_set(tb, "args", sb);
    json_t *api = gc2_call(ctx, "asyncio.to_thread", tb);
    if (gc2_is_raise(api)) {
        if (gc2_raise_passes(api)) return api;
        goto load_except;
    }
    json_set(creds_map, es, json_copy(creds));
    json_set(apis, es, json_copy(api));
    return api;
load_except:
    {
        const json_t *exc = creds && gc2_is_raise(creds) ? creds : api;
        json_t *pe = gc2_proxy_error(ctx, exc);
        if (gc2_is_raise(pe)) return pe;
        json_t *la = json_object();
        json_set_hdr(la, "fmt", "[GoogleChat] per-user creds load/build failed for %s");
        json_t *argsarr = json_array(); json_append(argsarr, json_copy(email ? email : json_null()));
        json_set(la, "args", argsarr);
        gc2_call(ctx, "logger.debug", la);
        return NULL;
    }
}

/* PoP: gc2__submit_on_loop @ plugins/platforms/google_chat/adapter.py:_submit_on_loop */
static json_t *gc2__submit_on_loop(gc2_ctx_t *ctx, const json_t *args) {
    const json_t *loop = gc2_state(ctx, "_loop");
    json_t *la = json_object(); json_set(la, "loop", json_copy(loop ? loop : json_null()));
    json_t *acc = gc2_call(ctx, "_loop_accepts_callbacks", la);
    if (gc2_is_raise(acc)) return acc;
    if (!gc2_truthy(acc)) {
        gc2_call(ctx, "logger.warning", gc2_logargs("[GoogleChat] Loop not accepting callbacks; dropping event", NULL));
        return NULL;
    }
    json_t *ia = json_object();
    json_t *fl = json_array(); json_append(fl, json_string("safe_schedule_threadsafe"));
    json_set(ia, "fromlist", fl);
    json_t *imod = gc2_call(ctx, "import.agent.async_utils", ia);
    if (gc2_is_raise(imod)) return imod;
    json_t *cp;
    if (!gc2_is_null(gc2_state(ctx, "_scope_ctx")))
        cp = gc2_call(ctx, "scope_ctx.copy", json_object());
    else
        cp = gc2_call(ctx, "contextvars.copy_context", json_object());
    if (gc2_is_raise(cp)) return cp;
    json_t *ra = json_object();
    json_set_hdr(ra, "fn", "safe_schedule_threadsafe");
    json_set(ra, "coro", json_copy(json_obj_get(args, "coro") ? json_obj_get(args, "coro") : json_null()));
    json_set(ra, "loop", json_copy(loop ? loop : json_null()));
    json_set_hdr(ra, "log_message", "[GoogleChat] Failed to schedule background callback");
    json_set(ra, "log_level", json_number(30));
    json_t *future = gc2_call(ctx, "context.run", ra);
    if (gc2_is_raise(future)) {
        if (gc2_isa(json_get_str(future, "__raise__", ""), "RuntimeError")) {
            gc2_call(ctx, "logger.warning", gc2_logargs("[GoogleChat] Loop closed between check and submit", NULL));
            return NULL;
        }
        return future;
    }
    if (!gc2_is_null(future)) {
        json_t *ca = json_object(); json_set_hdr(ca, "callback", "_log_background_failure");
        json_t *rc = gc2_call(ctx, "future.add_done_callback", ca);
        if (gc2_is_raise(rc)) return rc;
    }
    return NULL;
}

