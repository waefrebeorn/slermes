/*
 * google_chat_adapter_flow2.c -- UNCLAIMED remainder FLOW cores of
 * plugins/platforms/google_chat/adapter.py (hermes-agent), second wave after
 * google_chat_adapter_flow claimed the module decision chains + inbound/typing
 * leaves. Ports the persisted thread-count store (load/get/incr/decr/_save),
 * the SA-credential loader chain (_load_sa_credentials_from + the adapter
 * _load_sa_credentials error mapping), bot-identity cache path/save + eager
 * members.list resolution, legacy user-OAuth load, Pub/Sub subscription check,
 * the inbound Pub/Sub callback handler + HTTP dispatch + message dispatch +
 * MessageEvent build + attachment download, and the outbound send/send_card/
 * edit/delete/patch/create flows with bounded retry.
 *
 * Decision sequences only: every live I/O / injected callable / sibling
 * method / asyncio primitive / foreign import / logger call enters through ONE
 * universal seam: gca2_call(ctx, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording stubs emitting the identical event shape {"name","args","ret"}
 * from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and gca2_-prefixed.
 *
 * Seam name table (name -> live call):
 *   logger.debug/info/warning/error/exception  logger.<level>(fmt,*args)
 *   asyncio.to_thread        ONE seam {"fn":<site marker>,"args":{...}}; the
 *                            callable is NEVER invoked. Site markers:
 *                            members.list / oauth.build_user_chat /
 *                            get_subscription / messages.delete /
 *                            messages.patch / media.download /
 *                            download_uri.fetch / oauth.load_user_credentials /
 *                            oauth.list_emails / <fixture callable marker>.
 *   asyncio.sleep            {"seconds": <float>}
 *   random.random            {}
 *   json.loads               {"s": <text>} (JSONDecodeError message observable)
 *   path.exists/read_text/write_text/mkdir   POSIX path carrier ops
 *   os.replace               {"src","dst"}
 *   os.path.exists           {"path"} (SA file probe)
 *   import.<module>          in-function import {"fromlist":[...]}
 *   google_auth.default      {"scopes":[...]}
 *   service_account.Credentials.from_service_account_info {"info","scopes"}
 *   get_hermes_home / get_scoped_secret / pubsub_v1.SubscriberClient
 *   retryable_ingress        {} -> scope {"refused":bool}
 *   redact_sensitive_text    {"text"}
 *   message.nack / message.ack
 *   cache_{image,audio,video}_from_bytes_async {"data","ext"}
 *   cache_document_from_bytes_async {"data","filename"}
 *   MessageEvent             constructor kwargs
 *   store._save              self._save() when composed into incr/decr
 *   thread_count_store.incr/decr, dedup.is_duplicate/discard
 *   seamed siblings: _http_status _exc_text _proxy_error _is_retryable_error
 *     _is_google_owned_host _mime_for_message_type _create_kwargs _thread_body
 *     _adc_would_borrow_foreign_credentials _load_sa_credentials_from
 *     _resolve_thread_id _chunk_text _note_rate_limit _track_outbound_thread
 *     _release_unadmitted _connect_failed _prepare_inbound _save_cached_bot_id
 *     _submit_on_loop _dispatch_message _build_message_event
 *     _download_attachment _patch_message _create_message _call_with_retry
 *     _handle_setup_files_command format_message pause_typing_for_chat
 *     resume_typing_for_chat _is_sender_authorized handle_message build_source
 *     _set_fatal_error _bot_id_cache_path
 *
 * Representation contract (shared with tests/oracle_google_chat_adapter_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc)} object (OSError
 *   adds errno, message "[Errno N] msg"; _SACredentialError adds kind + detail
 *   (a raise envelope or null)). BaseException kinds (SystemExit,
 *   KeyboardInterrupt, GeneratorExit, CancelledError, BaseExceptionGroup) pass
 *   through every except-Exception site (full builtin hierarchy table below).
 * - Exception doubles (args.exc): {"cls": name, "msg": str(exc), "attrs":{}}.
 * - Attribute carriers (config / message / event / source) travel as
 *   {"__obj__": {attrs}}; a plain JSON object IS a Python dict.
 * - SendResult returns project to the FULL 8-field dataclass dict; tuple
 *   returns project to arrays; MessageType members project to .value.
 * - logger seams record {"fmt":<literal>,"args":[<raw args>]}; exc_info kwargs
 *   drop on both sides (but the exc_info expression's seams still fire).
 * - Seamed async siblings fire on AWAIT; a never-awaited construction
 *   (self._submit_on_loop(self._dispatch_message(...))) fires NO seam and
 *   projects to {"__coro__": name, "args": {...}}.
 * - Path modeling POSIX; read_text(utf-8-sig) strips ONE leading U+FEFF AFTER
 *   the seam call (trace ret carries the raw scripted text).
 * - json.dumps is REAL/pure: the store _save uses compact separators (",",":");
 *   the bot-id cache + redact dump use default separators (", ",": "). The
 *   corpus is ASCII + integral numbers (no non-integral float repr).
 * - Attachment bytes travel as ASCII str markers; cache seams receive the
 *   marker verbatim. edit_message ellipsis literal is U+2026.
 *
 * Fixture corpus domains (documented restrictions):
 * - ASCII scope: strip/lower/upper fold ASCII whitespace / letters.
 * - json.loads is SEAMED; its scripted JSONDecodeError message is carried into
 *   the SA error templates verbatim (libjson parse messages are NOT modeled).
 * - to_thread callables are never invoked; the inner Chat API chain is out of
 *   seam scope (one seam per thread boundary, site marker + captured args).
 * - SendResult seam rets carry a subset of the 8 dataclass fields; missing
 *   fields take their dataclass defaults on both sides.
 * - members / resp / info / claims seam rets are dicts (or null); .get on a
 *   non-dict raises AttributeError naming the value type.
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

#define GCA2_MAX_TEXT_LENGTH 4000
#define GCA2_RATE_LIMIT_WARN_THRESHOLD 5
#define GCA2_RETRY_MAX_ATTEMPTS 3
#define GCA2_RETRY_BASE_DELAY 1.0
#define GCA2_RETRY_MAX_DELAY 8.0
#define GCA2_RETRY_JITTER 0.3
#define GCA2_TYPING_CONSUMED_SENTINEL "<consumed>"
#define GCA2_PARKED_MSG "The routed profile is not being served right now; retry this delivery"

/* -- universal seam context -- */
typedef struct gca2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} gca2_ctx_t;

/* -- exception hierarchy (FULL builtin tree + asyncio.CancelledError +
 * JSONDecodeError + the model classes). Aliases IOError/EnvironmentError/
 * WindowsError map to OSError; ExceptionGroup carries BOTH bases. -- */
typedef struct { const char *name, *p1, *p2; } gca2_exc_ent_t;
static const gca2_exc_ent_t GCA2_EXC_TABLE[] = {
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

static const gca2_exc_ent_t *gca2_exc_ent(const char *name) {
    for (size_t i = 0; i < sizeof(GCA2_EXC_TABLE) / sizeof(GCA2_EXC_TABLE[0]); i++)
        if (strcmp(GCA2_EXC_TABLE[i].name, name) == 0) return &GCA2_EXC_TABLE[i];
    return NULL;
}

/* issubclass(kind, ancestor) over the table; unknown kinds are Exception
 * subclasses (the oracle's dynamic type(kind,(Exception,),{})). The aliases
 * IOError/EnvironmentError/WindowsError ARE OSError in CPython, so normalize
 * both sides to OSError before walking. */
static const char *gca2_exc_alias(const char *n) {
    if (strcmp(n, "IOError") == 0 || strcmp(n, "EnvironmentError") == 0 ||
        strcmp(n, "WindowsError") == 0) return "OSError";
    return n;
}
static bool gca2_isa(const char *kind, const char *ancestor) {
    if (!kind || !ancestor) return false;
    kind = gca2_exc_alias(kind);
    ancestor = gca2_exc_alias(ancestor);
    if (strcmp(kind, ancestor) == 0) return true;
    const char *cur = kind;
    for (int depth = 0; depth < 16 && cur; depth++) {
        if (strcmp(cur, ancestor) == 0) return true;
        const gca2_exc_ent_t *e = gca2_exc_ent(cur);
        const char *p1 = e ? e->p1 : "Exception";
        const char *p2 = e ? e->p2 : NULL;
        if (p2 && gca2_isa(p2, ancestor)) return true;
        if (!p1) return false;
        if (strcmp(p1, cur) == 0) return false;
        cur = p1;
    }
    return false;
}

/* -- basic helpers -- */
static bool gca2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
/* A raise an except-Exception site SWALLOWS (Exception family). */
static bool gca2_caught_exc(const json_t *r) {
    return gca2_is_raise(r) && gca2_isa(json_get_str(r, "__raise__", ""), "Exception");
}
/* A raise that PASSES THROUGH every except-Exception site (BaseException). */
static bool gca2_raise_passes(const json_t *r) {
    return gca2_is_raise(r) && !gca2_isa(json_get_str(r, "__raise__", ""), "Exception");
}

static bool gca2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool gca2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *gca2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool gca2_str_eq(const json_t *v, const char *s) {
    const char *t = gca2_str(v);
    return t && s && strcmp(t, s) == 0;
}
static json_t *gca2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* CPython OSError(errno, msg) PROMOTES to the errno's subclass when the
 * constructed class is OSError itself (IOError alias included). Map measured
 * against the quarry interpreter (Windows errno values; POSIX values like
 * ECONNRESET=104 do NOT promote on this build). */
static const char *gca2_oserror_subclass(double en) {
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
static bool gca2_oserror_kind(const char *type) {
    if (strcmp(type, "EnvironmentError") == 0 || strcmp(type, "WindowsError") == 0)
        return false;
    return gca2_isa(type, "OSError");
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *gca2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    if (strcmp(type, "ParkedRouteRefused") == 0)
        return gca2_err(type, GCA2_PARKED_MSG);
    if (strcmp(type, "_SACredentialError") == 0) {
        /* live: super().__init__(kind) -> str(exc) == kind (spec kind, else msg) */
        json_t *e = gca2_err(type, json_get_str(spec, "kind", msg));
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
            json_t *mat = gca2_make_exc(dspec);
            json_free(dspec);
            json_set(e, "detail", mat);
        } else {
            json_set(e, "detail", json_null());
        }
        return e;
    }
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && gca2_oserror_kind(type)) {
        const char *cls = gca2_exc_alias(type);
        if (strcmp(cls, "OSError") == 0) cls = gca2_oserror_subclass(en->num_val);
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = gca2_err(cls, buf);
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
        if (!buf) return gca2_err(type, msg);
        snprintf(buf, need, "%s: line %lld column %lld (char %lld)",
                 msg, lineno, colno, pos);
        json_t *e = gca2_err(type, buf);
        free(buf);
        return e;
    }
    return gca2_err(type, msg);
}

/* The one universal seam (mirrors gca_call / the oracle Ctx.call). args is
 * ADOPTED into the trace event; the return is a fresh owned node. A
 * missing/exhausted script RETURNS a fail-closed RuntimeError spec. */
static json_t *gca2_call(gca2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = gca2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = gca2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = gca2_make_exc(raise);
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
static char *gca2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static bool gca2_is_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f) || c == ' ';
}
static char *gca2_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && gca2_is_ws((unsigned char)s[a])) a++;
    while (b > a && gca2_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}
static char *gca2_lstrip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0;
    while (a < n && gca2_is_ws((unsigned char)s[a])) a++;
    return gca2_strdup(s + a);
}
static char *gca2_lower(const char *s) {
    char *o = gca2_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *gca2_upper(const char *s) {
    char *o = gca2_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)toupper((unsigned char)*p);
    return o;
}
static bool gca2_contains(const char *hay, const char *needle) {
    return hay && needle && strstr(hay, needle) != NULL;
}
static bool gca2_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool gca2_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}
static char *gca2_replace(const char *s, const char *from, const char *to) {
    if (!s) s = "";
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return gca2_strdup(s);
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
static const char *gca2_type_name(const json_t *v) {
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
static char *gca2_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return gca2_strdup("None");
    switch (v->type) {
        case JSON_STRING: return gca2_strdup(v->str_val);
        case JSON_BOOL: return gca2_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 9.2e18)
                snprintf(buf, sizeof buf, "%.0f", d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return gca2_strdup(buf);
        }
        default: return NULL;
    }
}

/* -- UTF-8 helpers (Python len counts codepoints) -- */
static size_t gca2_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xc0) != 0x80) n++;
    return n;
}
static const char *gca2_cp_ptr(const char *s, long long i) {
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
static char *gca2_slice_head(const char *s, long long b) {
    if (!s) s = "";
    long long n = (long long)gca2_utf8_len(s);
    if (b < 0) b = n + b;
    if (b < 0) b = 0;
    if (b > n) b = n;
    const char *pb = gca2_cp_ptr(s, b);
    size_t len = (size_t)(pb - s);
    char *o = malloc(len + 1);
    if (!o) return NULL;
    memcpy(o, s, len); o[len] = '\0';
    return o;
}

/* -- object / attribute carriers / state -- */
static const json_t *gca2_obj_attrs(const json_t *v) {
    if (v && v->type == JSON_OBJECT) {
        json_t *inner = json_obj_get(v, "__obj__");
        if (inner && inner->type == JSON_OBJECT) return inner;
    }
    return NULL;
}
/* Direct attribute read on a carrier (self.config.home_channel etc.): absent
 * attr / null carrier raises AttributeError, matching live SimpleNamespace. */
static json_t *gca2_direct_attr(const json_t *node, const char *name) {
    char buf[160];
    if (gca2_is_null(node)) {
        snprintf(buf, sizeof buf, "'NoneType' object has no attribute '%s'", name);
        return gca2_err("AttributeError", buf);
    }
    const json_t *attrs = gca2_obj_attrs(node);
    if (!attrs) {
        snprintf(buf, sizeof buf, "'%s' object has no attribute '%s'", gca2_type_name(node), name);
        return gca2_err("AttributeError", buf);
    }
    if (!json_has(attrs, name)) {
        snprintf(buf, sizeof buf, "'types.SimpleNamespace' object has no attribute '%s'", name);
        return gca2_err("AttributeError", buf);
    }
    return json_copy(json_obj_get(attrs, name));
}
static const json_t *gca2_state(const gca2_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}
static json_t *gca2_state_obj(gca2_ctx_t *ctx, const char *key) {
    json_t *v = ctx->state ? json_obj_get(ctx->state, key) : NULL;
    if (!v || v->type != JSON_OBJECT) {
        v = json_object();
        json_set(ctx->state, key, v);
    }
    return v;
}
/* dict.get on a value that must be a dict; non-dict -> AttributeError raise. */
static const json_t *gca2_dict_get(const json_t *v, const char *key) {
    if (v && v->type == JSON_OBJECT) return json_obj_get(v, key);
    return NULL;
}
static json_t *gca2_get_attr_err(const json_t *v, const char *meth) {
    char buf[160];
    snprintf(buf, sizeof buf, "'%s' object has no attribute '%s'", gca2_type_name(v), meth);
    return gca2_err("AttributeError", buf);
}

/* project a raise envelope into an exc double for seam args (oracle proj). */
static json_t *gca2_exc_double(const json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "cls", json_get_str(spec, "__raise__", "Exception"));
    json_set_hdr(e, "msg", json_get_str(spec, "message", ""));
    json_set(e, "attrs", json_object());
    return e;
}

/* -- logger seam helper -- */
static json_t *gca2_log(gca2_ctx_t *ctx, const char *level, const char *fmt, json_t *args) {
    char name[64];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *a = json_object();
    json_set_hdr(a, "fmt", fmt);
    json_set(a, "args", args ? args : json_array());
    return gca2_call(ctx, name, a);
}

/* -- _exc_text seam helper (exc double + targets) -- */
static json_t *gca2_exc_text(gca2_ctx_t *ctx, const json_t *exc_spec, json_t *targets) {
    json_t *a = json_object();
    json_set(a, "exc", gca2_exc_double(exc_spec));
    json_set(a, "targets", targets ? targets : json_array());
    return gca2_call(ctx, "_exc_text", a);
}

/* -- POSIX path helpers -- */
static char *gca2_path_join(const char *home, const char *name) {
    if (!home) home = "";
    if (!home[0]) return gca2_strdup(name);
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
static char *gca2_path_dirname(const char *p) {
    if (!p) p = "";
    const char *slash = strrchr(p, '/');
    if (!slash) return gca2_strdup(".");
    if (slash == p) return gca2_strdup("/");
    size_t n = (size_t)(slash - p);
    char *o = malloc(n + 1);
    if (o) { memcpy(o, p, n); o[n] = '\0'; }
    return o;
}
static char *gca2_path_with_suffix(const char *p, const char *suf) {
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
static char *gca2_path_suffix(const char *p) {
    if (!p) p = "";
    const char *base = strrchr(p, '/');
    base = base ? base + 1 : p;
    const char *dot = strrchr(base, '.');
    if (dot && dot != base) return gca2_strdup(dot);
    return gca2_strdup("");
}
/* strip ONE leading U+FEFF BOM (EF BB BF) after a utf-8-sig read seam. */
static const char *gca2_strip_bom(const char *s) {
    if (!s) return "";
    if ((unsigned char)s[0] == 0xef && (unsigned char)s[1] == 0xbb && (unsigned char)s[2] == 0xbf)
        return s + 3;
    return s;
}

/* -- JSON string escaping + json.dumps (Python default / compact) -- */
typedef struct { char *buf; size_t cap, len; } gca2_sb_t;
static void gca2_sb_putc(gca2_sb_t *s, char c) {
    if (s->len + 2 >= s->cap) { s->cap = s->cap ? s->cap * 2 : 128; s->buf = realloc(s->buf, s->cap); }
    s->buf[s->len++] = c; s->buf[s->len] = '\0';
}
static void gca2_sb_puts(gca2_sb_t *s, const char *t) { while (t && *t) gca2_sb_putc(s, *t++); }
static void gca2_sb_json_str(gca2_sb_t *s, const char *str) {
    gca2_sb_putc(s, '"');
    if (!str) str = "";
    for (const unsigned char *p = (const unsigned char *)str; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"': gca2_sb_puts(s, "\\\""); break;
            case '\\': gca2_sb_puts(s, "\\\\"); break;
            case '\b': gca2_sb_puts(s, "\\b"); break;
            case '\f': gca2_sb_puts(s, "\\f"); break;
            case '\n': gca2_sb_puts(s, "\\n"); break;
            case '\r': gca2_sb_puts(s, "\\r"); break;
            case '\t': gca2_sb_puts(s, "\\t"); break;
            default:
                if (c < 0x20) { char e[8]; snprintf(e, sizeof e, "\\u%04x", c); gca2_sb_puts(s, e); }
                else gca2_sb_putc(s, (char)c);
        }
    }
    gca2_sb_putc(s, '"');
}
static void gca2_dumps_node(gca2_sb_t *s, const json_t *n, bool compact) {
    if (!n || n->type == JSON_NULL) { gca2_sb_puts(s, "null"); return; }
    switch (n->type) {
        case JSON_BOOL: gca2_sb_puts(s, n->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char tmp[64];
            double d = n->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 9.2e18) snprintf(tmp, sizeof tmp, "%.0f", d);
            else snprintf(tmp, sizeof tmp, "%.17g", d);
            gca2_sb_puts(s, tmp);
            break;
        }
        case JSON_STRING: gca2_sb_json_str(s, n->str_val); break;
        case JSON_ARRAY:
            gca2_sb_putc(s, '[');
            for (size_t i = 0; i < n->c.count; i++) {
                if (i) gca2_sb_puts(s, compact ? "," : ", ");
                gca2_dumps_node(s, n->c.items[i], compact);
            }
            gca2_sb_putc(s, ']');
            break;
        case JSON_OBJECT:
            gca2_sb_putc(s, '{');
            for (size_t i = 0; i < n->c.count; i++) {
                if (i) gca2_sb_puts(s, compact ? "," : ", ");
                gca2_sb_json_str(s, n->c.keys[i]);
                gca2_sb_puts(s, compact ? ":" : ": ");
                gca2_dumps_node(s, n->c.items[i], compact);
            }
            gca2_sb_putc(s, '}');
            break;
        default: gca2_sb_puts(s, "null");
    }
}
static char *gca2_dumps(const json_t *n, bool compact) {
    gca2_sb_t s = {NULL, 0, 0};
    gca2_dumps_node(&s, n, compact);
    if (!s.buf) return gca2_strdup("null");
    return s.buf;
}

/* -- SendResult (full 8-field dataclass projection) -- */
static json_t *gca2_sr_new(bool success, const json_t *message_id, const json_t *error, bool retryable) {
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
static json_t *gca2_sr_norm(const json_t *raw) {
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
static json_t *gca2_unpack2(const json_t *r, json_t **a, json_t **b) {
    if (gca2_is_null(r))
        return gca2_err("TypeError", "cannot unpack non-iterable NoneType object");
    if (r->type != JSON_ARRAY) {
        char buf[128];
        snprintf(buf, sizeof buf, "cannot unpack non-iterable %s object", gca2_type_name(r));
        return gca2_err("TypeError", buf);
    }
    size_t n = json_len(r);
    if (n != 2) {
        char buf[128];
        if (n < 2) snprintf(buf, sizeof buf, "not enough values to unpack (expected 2, got %zu)", n);
        else snprintf(buf, sizeof buf, "too many values to unpack (expected 2, got %zu)", n);
        return gca2_err("ValueError", buf);
    }
    *a = json_get(r, 0); *b = json_get(r, 1);
    return NULL;
}

/* =====================================================================
 * Module flow: _load_sa_credentials_from
 * ===================================================================== */
/* PoP: gca2__load_sa_credentials_from @ plugins/platforms/google_chat/adapter.py:_load_sa_credentials_from */
static json_t *gca2__load_sa_credentials_from(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *sa_value = json_obj_get(args, "sa_value");
    if (gca2_truthy(sa_value)) {
        const char *sv = gca2_str(sa_value);
        char *ls = gca2_lstrip(sv ? sv : "");
        bool inline_json = gca2_starts_with(ls, "{");
        free(ls);
        json_t *info = NULL;
        if (inline_json) {
            json_t *la = json_object();
            json_set(la, "s", json_copy(sa_value));
            json_t *r = gca2_call(ctx, "json.loads", la);
            if (gca2_is_raise(r)) {
                if (gca2_isa(json_get_str(r, "__raise__", ""), "JSONDecodeError")) {
                    json_t *e = gca2_err("_SACredentialError", "inline_invalid");
                    json_set_hdr(e, "kind", "inline_invalid");
                    json_set(e, "detail", r);  /* adopt the raise envelope */
                    return e;
                }
                return r;  /* other / BaseException propagates */
            }
            info = r;
        } else {
            json_t *ea = json_object();
            json_set(ea, "path", json_copy(sa_value));
            json_t *ex = gca2_call(ctx, "os.path.exists", ea);
            if (gca2_is_raise(ex)) { json_free(info); return ex; }
            bool exists = gca2_truthy(ex);
            json_free(ex);
            if (!exists) {
                json_t *e = gca2_err("_SACredentialError", "not_found");
                json_set_hdr(e, "kind", "not_found");
                json_set(e, "detail", json_null());
                return e;
            }
            json_t *ra = json_object();
            json_set(ra, "path", json_copy(sa_value));
            json_set_hdr(ra, "encoding", "utf-8-sig");
            json_t *raw = gca2_call(ctx, "path.read_text", ra);
            if (gca2_is_raise(raw)) return raw;  /* OSError NOT caught here */
            const char *text = gca2_strip_bom(gca2_str(raw));
            json_t *la = json_object();
            json_set_hdr(la, "s", text);
            json_t *r = gca2_call(ctx, "json.loads", la);
            json_free(raw);
            if (gca2_is_raise(r)) {
                if (gca2_isa(json_get_str(r, "__raise__", ""), "JSONDecodeError")) {
                    json_t *e = gca2_err("_SACredentialError", "file_invalid");
                    json_set_hdr(e, "kind", "file_invalid");
                    json_set(e, "detail", r);
                    return e;
                }
                return r;
            }
            info = r;
        }
        /* service_account.Credentials.from_service_account_info(info, scopes=_CHAT_SCOPES) */
        json_t *fa = json_object();
        json_set(fa, "info", info);
        json_t *scopes = json_array();
        json_append(scopes, json_string("https://www.googleapis.com/auth/chat.bot"));
        json_append(scopes, json_string("https://www.googleapis.com/auth/pubsub"));
        json_set(fa, "scopes", scopes);
        return gca2_call(ctx, "service_account.Credentials.from_service_account_info", fa);
    }
    /* ADC branch */
    json_t *mx = gca2_call(ctx, "_adc_would_borrow_foreign_credentials", json_object());
    if (gca2_is_raise(mx)) return mx;
    if (gca2_truthy(mx)) {
        json_free(mx);
        json_t *e = gca2_err("_SACredentialError", "adc_foreign");
        json_set_hdr(e, "kind", "adc_foreign");
        json_set(e, "detail", json_null());
        return e;
    }
    json_free(mx);
    json_t *ia = json_object();
    json_set(ia, "fromlist", json_array());
    json_t *imp = gca2_call(ctx, "import.google.auth", ia);
    if (gca2_is_raise(imp)) {
        if (gca2_isa(json_get_str(imp, "__raise__", ""), "ImportError")) {
            json_free(imp);
            json_t *e = gca2_err("_SACredentialError", "adc_no_auth");
            json_set_hdr(e, "kind", "adc_no_auth");
            json_set(e, "detail", json_null());
            return e;
        }
        return imp;
    }
    json_free(imp);
    json_t *da = json_object();
    json_t *scopes = json_array();
    json_append(scopes, json_string("https://www.googleapis.com/auth/chat.bot"));
    json_append(scopes, json_string("https://www.googleapis.com/auth/pubsub"));
    json_set(da, "scopes", scopes);
    json_t *r = gca2_call(ctx, "google_auth.default", da);
    json_t *exc = NULL;
    json_t *credentials = NULL;
    if (gca2_is_raise(r)) {
        if (gca2_raise_passes(r)) return r;
        exc = r;
    } else {
        json_t *project;
        json_t *bad = gca2_unpack2(r, &credentials, &project);
        if (bad) { json_free(r); exc = bad; }
    }
    if (exc) {
        json_t *e = gca2_err("_SACredentialError", "adc_failed");
        json_set_hdr(e, "kind", "adc_failed");
        json_set(e, "detail", exc);
        return e;
    }
    json_t *out = json_copy(credentials);
    json_free(r);
    return out;
}

/* =====================================================================
 * _ThreadCountStore methods (self = state._path / state._counts)
 * ===================================================================== */
/* PoP: gca2_store_load @ plugins/platforms/google_chat/adapter.py:load */
static json_t *gca2_store_load(gca2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_set(ctx->state, "_counts", json_object());
    const char *path = gca2_str(gca2_state(ctx, "_path"));
    if (!path) path = "";
    json_t *ea = json_object();
    json_set_hdr(ea, "path", path);
    json_t *ex = gca2_call(ctx, "path.exists", ea);
    if (gca2_is_raise(ex)) return ex;
    bool exists = gca2_truthy(ex);
    json_free(ex);
    if (!exists) return NULL;
    json_t *ra = json_object();
    json_set_hdr(ra, "path", path);
    json_set_hdr(ra, "encoding", "utf-8-sig");
    json_t *raw = gca2_call(ctx, "path.read_text", ra);
    json_t *data = NULL;
    if (gca2_is_raise(raw)) {
        const char *kind = json_get_str(raw, "__raise__", "");
        if (gca2_isa(kind, "JSONDecodeError") || gca2_isa(kind, "OSError")) {
            bool is_value = gca2_isa(kind, "ValueError");
            const char *fmt = is_value
                ? "[GoogleChat] thread-count store at %s is corrupt; starting fresh: %s"
                : "[GoogleChat] could not read thread-count store at %s: %s";
            json_t *aa = json_array();
            json_append(aa, json_string(path));
            json_append(aa, json_string(json_get_str(raw, "message", "")));
            json_free(raw);
            json_t *lr = gca2_log(ctx, "warning", fmt, aa);
            if (gca2_is_raise(lr)) return lr;
            json_free(lr);
            return NULL;
        }
        return raw;
    }
    const char *text = gca2_strip_bom(gca2_str(raw));
    char *stripped = gca2_strip(text);
    bool nonempty = stripped[0] != '\0';
    free(stripped);
    if (nonempty) {
        json_t *la = json_object();
        json_set_hdr(la, "s", text);
        data = gca2_call(ctx, "json.loads", la);
        if (gca2_is_raise(data)) {
            const char *kind = json_get_str(data, "__raise__", "");
            if (gca2_isa(kind, "JSONDecodeError") || gca2_isa(kind, "OSError")) {
                bool is_value = gca2_isa(kind, "ValueError");
                const char *fmt = is_value
                    ? "[GoogleChat] thread-count store at %s is corrupt; starting fresh: %s"
                    : "[GoogleChat] could not read thread-count store at %s: %s";
                json_t *aa = json_array();
                json_append(aa, json_string(path));
                json_append(aa, json_string(json_get_str(data, "message", "")));
                json_free(data);
                json_free(raw);
                json_t *lr = gca2_log(ctx, "warning", fmt, aa);
                if (gca2_is_raise(lr)) return lr;
                json_free(lr);
                return NULL;
            }
            json_free(raw);
            return data;
        }
    } else {
        data = json_object();
    }
    json_free(raw);
    /* schema filter: data.items() if isinstance(data, dict) else () */
    json_t *counts = gca2_state_obj(ctx, "_counts");
    if (data && data->type == JSON_OBJECT) {
        for (size_t i = 0; i < data->c.count; i++) {
            const char *chat_id = data->c.keys[i];
            const json_t *threads = data->c.items[i];
            if (!threads || threads->type != JSON_OBJECT) continue;
            json_t *clean = json_object();
            for (size_t j = 0; j < threads->c.count; j++) {
                const json_t *c = threads->c.items[j];
                /* isinstance(c, int): integral number or bool */
                bool is_int = c && (c->type == JSON_BOOL ||
                    (c->type == JSON_NUMBER && c->num_val == floor(c->num_val) && isfinite(c->num_val)));
                if (is_int) json_set(clean, threads->c.keys[j], json_copy(c));
            }
            if (clean->c.count > 0) json_set(counts, chat_id, clean);
            else json_free(clean);
        }
    }
    json_free(data);
    return NULL;
}

/* PoP: gca2_store_get @ plugins/platforms/google_chat/adapter.py:get */
static json_t *gca2_store_get(gca2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *thread_name = json_get_str(args, "thread_name", "");
    const json_t *counts = gca2_state(ctx, "_counts");
    const json_t *chat = (counts && counts->type == JSON_OBJECT) ? json_obj_get(counts, chat_id) : NULL;
    const json_t *v = (chat && chat->type == JSON_OBJECT) ? json_obj_get(chat, thread_name) : NULL;
    if (v && v->type == JSON_NUMBER) return json_copy(v);
    if (v && v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    return json_number(0);
}

/* PoP: gca2_store_incr @ plugins/platforms/google_chat/adapter.py:incr */
static json_t *gca2_store_incr(gca2_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *thread_name = json_get_str(args, "thread_name", "");
    json_t *counts = gca2_state_obj(ctx, "_counts");
    json_t *chat = json_obj_get(counts, chat_id);
    if (!chat || chat->type != JSON_OBJECT) { chat = json_object(); json_set(counts, chat_id, chat); }
    double prev = json_get_num(chat, thread_name, 0);
    json_set(chat, thread_name, json_number(prev + 1));
    json_t *sr = gca2_call(ctx, "store._save", json_object());
    if (gca2_is_raise(sr)) return sr;
    json_free(sr);
    return json_number(prev);
}

/* PoP: gca2_store_decr @ plugins/platforms/google_chat/adapter.py:decr */
static json_t *gca2_store_decr(gca2_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *thread_name = json_get_str(args, "thread_name", "");
    const json_t *counts = gca2_state(ctx, "_counts");
    json_t *chat = (counts && counts->type == JSON_OBJECT) ? json_obj_get(counts, chat_id) : NULL;
    double cur = (chat && chat->type == JSON_OBJECT) ? json_get_num(chat, thread_name, 0) : 0;
    if (cur > 0) {
        json_set(chat, thread_name, json_number(cur - 1));
        json_t *sr = gca2_call(ctx, "store._save", json_object());
        if (gca2_is_raise(sr)) return sr;
        json_free(sr);
    }
    return NULL;
}

/* PoP: gca2_store__save @ plugins/platforms/google_chat/adapter.py:_save */
static json_t *gca2_store__save(gca2_ctx_t *ctx, const json_t *args) {
    (void)args;
    const char *path = gca2_str(gca2_state(ctx, "_path"));
    if (!path) path = "";
    char *parent = gca2_path_dirname(path);
    char *suffix = gca2_path_suffix(path);
    char *suf_tmp = malloc(strlen(suffix) + 5);
    if (suf_tmp) sprintf(suf_tmp, "%s.tmp", suffix);
    char *tmp = gca2_path_with_suffix(path, suf_tmp ? suf_tmp : ".tmp");
    free(suffix); free(suf_tmp);
    json_t *ret = NULL;
    json_t *ma = json_object();
    json_set_hdr(ma, "path", parent);
    json_set(ma, "parents", json_bool(true));
    json_set(ma, "exist_ok", json_bool(true));
    json_t *mr = gca2_call(ctx, "path.mkdir", ma);
    if (gca2_is_raise(mr)) { ret = mr; goto done; }
    json_free(mr);
    const json_t *counts = gca2_state(ctx, "_counts");
    char *text = gca2_dumps(counts ? counts : json_object(), true);
    json_t *wa = json_object();
    json_set_hdr(wa, "path", tmp);
    json_set_hdr(wa, "text", text);
    json_set_hdr(wa, "encoding", "utf-8");
    free(text);
    json_t *wr = gca2_call(ctx, "path.write_text", wa);
    if (gca2_is_raise(wr)) { ret = wr; goto done; }
    json_free(wr);
    json_t *oa = json_object();
    json_set_hdr(oa, "src", tmp);
    json_set_hdr(oa, "dst", path);
    json_t *or_ = gca2_call(ctx, "os.replace", oa);
    if (gca2_is_raise(or_)) { ret = or_; goto done; }
    json_free(or_);
done:
    if (ret && gca2_is_raise(ret)) {
        const char *kind = json_get_str(ret, "__raise__", "");
        if (gca2_isa(kind, "OSError")) {
            json_t *aa = json_array();
            json_append(aa, json_string(path));
            json_append(aa, json_string(json_get_str(ret, "message", "")));
            json_free(ret);
            json_t *lr = gca2_log(ctx, "warning",
                "[GoogleChat] could not persist thread-count store to %s: %s", aa);
            free(parent); free(tmp);
            if (gca2_is_raise(lr)) return lr;
            json_free(lr);
            return NULL;
        }
        free(parent); free(tmp);
        return ret;  /* non-OSError / BaseException propagates */
    }
    free(parent); free(tmp);
    return NULL;
}/* =====================================================================
 * Adapter: SA credentials / bot identity / OAuth / subscription
 * ===================================================================== */
static const char *gca2_sa_template(const char *kind) {
    if (strcmp(kind, "inline_invalid") == 0) return "Inline SA JSON is not valid JSON: {exc}";
    if (strcmp(kind, "not_found") == 0) return "Service Account JSON file not found at configured path.";
    if (strcmp(kind, "file_invalid") == 0) return "Service Account JSON file is not valid JSON: {exc}";
    if (strcmp(kind, "adc_foreign") == 0)
        return "Google Chat ADC skipped for this profile: service-account credentials are set in the process "
               "environment but not in this profile's secret scope. Set GOOGLE_CHAT_SERVICE_ACCOUNT_JSON in this "
               "profile's .env.";
    if (strcmp(kind, "adc_no_auth") == 0)
        return "No Service Account credentials configured. Set GOOGLE_CHAT_SERVICE_ACCOUNT_JSON or "
               "GOOGLE_APPLICATION_CREDENTIALS, or install google-auth to use Application Default Credentials.";
    if (strcmp(kind, "adc_failed") == 0)
        return "No Service Account credentials configured and Application Default Credentials are unavailable. Set "
               "GOOGLE_CHAT_SERVICE_ACCOUNT_JSON or run ``gcloud auth application-default login``. ADC error: {exc}";
    return "{exc}";
}
static char *gca2_sa_message(const char *kind, const char *detail_msg) {
    const char *tpl = gca2_sa_template(kind);
    return gca2_replace(tpl, "{exc}", detail_msg ? detail_msg : "None");
}

/* PoP: gca2__load_sa_credentials @ plugins/platforms/google_chat/adapter.py:_load_sa_credentials */
static json_t *gca2__load_sa_credentials(gca2_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *config = gca2_state(ctx, "config");
    json_t *extra = gca2_direct_attr(config, "extra");
    if (gca2_is_raise(extra)) return extra;
    if (!extra || extra->type != JSON_OBJECT) {
        json_t *e = gca2_get_attr_err(extra, "get");
        json_free(extra);
        return e;
    }
    const json_t *sj = json_obj_get(extra, "service_account_json");
    json_t *sa_path = NULL;
    if (gca2_truthy(sj)) sa_path = json_copy(sj);
    json_free(extra);
    if (!sa_path) {
        json_t *sa = json_object();
        json_set_hdr(sa, "name", "GOOGLE_APPLICATION_CREDENTIALS");
        json_set(sa, "default", json_null());
        sa_path = gca2_call(ctx, "get_scoped_secret", sa);
        if (gca2_is_raise(sa_path)) return sa_path;
    }
    json_t *fa = json_object();
    json_set(fa, "sa_value", json_copy(sa_path));
    json_t *creds = gca2_call(ctx, "_load_sa_credentials_from", fa);
    if (gca2_is_raise(creds)) {
        const char *kind = json_get_str(creds, "__raise__", "");
        if (gca2_isa(kind, "_SACredentialError")) {
            const char *ekind = json_get_str(creds, "kind", kind);
            const json_t *detail = json_obj_get(creds, "detail");
            const char *dmsg = (detail && detail->type == JSON_OBJECT) ? json_get_str(detail, "message", "") : "None";
            char *message = gca2_sa_message(ekind, dmsg);
            json_free(creds);
            json_free(sa_path);
            json_t *e = gca2_err(strcmp(ekind, "not_found") == 0 ? "FileNotFoundError" : "ValueError", message);
            free(message);
            return e;
        }
        json_free(sa_path);
        return creds;
    }
    if (!gca2_truthy(sa_path)) {
        json_t *lr = gca2_log(ctx, "info",
            "[GoogleChat] No SA JSON configured; using Application Default Credentials", json_array());
        if (gca2_is_raise(lr)) { json_free(creds); json_free(sa_path); return lr; }
        json_free(lr);
    }
    json_free(sa_path);
    return creds;
}

/* PoP: gca2__bot_id_cache_path @ plugins/platforms/google_chat/adapter.py:_bot_id_cache_path */
static json_t *gca2__bot_id_cache_path(gca2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *ia = json_object();
    json_t *fl = json_array();
    json_append(fl, json_string("get_hermes_home"));
    json_set(ia, "fromlist", fl);
    json_t *imp = gca2_call(ctx, "import.hermes_constants", ia);
    if (gca2_is_raise(imp)) return imp;
    json_free(imp);
    json_t *home = gca2_call(ctx, "get_hermes_home", json_object());
    if (gca2_is_raise(home)) return home;
    const char *h = gca2_str(home);
    char *joined = gca2_path_join(h ? h : "", "google_chat_bot_id.json");
    json_free(home);
    json_t *out = json_string(joined ? joined : "");
    free(joined);
    return out;
}

/* PoP: gca2__save_cached_bot_id @ plugins/platforms/google_chat/adapter.py:_save_cached_bot_id */
static json_t *gca2__save_cached_bot_id(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *bot_user_id = json_obj_get(args, "bot_user_id");
    json_t *ret = NULL;
    json_t *path = gca2_call(ctx, "_bot_id_cache_path", json_object());
    if (gca2_is_raise(path)) { ret = path; goto handle; }
    const char *path_s = gca2_str(path);
    if (!path_s) path_s = "";
    char *parent = gca2_path_dirname(path_s);
    json_t *ma = json_object();
    json_set_hdr(ma, "path", parent);
    json_set(ma, "parents", json_bool(true));
    json_set(ma, "exist_ok", json_bool(true));
    free(parent);
    json_t *mr = gca2_call(ctx, "path.mkdir", ma);
    if (gca2_is_raise(mr)) { json_free(path); ret = mr; goto handle; }
    json_free(mr);
    json_t *obj = json_object();
    json_set(obj, "bot_user_id", bot_user_id ? json_copy(bot_user_id) : json_null());
    char *text = gca2_dumps(obj, false);
    json_free(obj);
    json_t *wa = json_object();
    json_set_hdr(wa, "path", path_s);
    json_set_hdr(wa, "text", text);
    json_set_hdr(wa, "encoding", "utf-8");
    free(text);
    json_t *wr = gca2_call(ctx, "path.write_text", wa);
    json_free(path);
    if (gca2_is_raise(wr)) { ret = wr; goto handle; }
    json_free(wr);
    return NULL;
handle:
    if (gca2_isa(json_get_str(ret, "__raise__", ""), "OSError")) {
        json_free(ret);
        json_t *lr = gca2_log(ctx, "debug", "[GoogleChat] Could not persist bot_user_id cache", json_array());
        if (gca2_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    return ret;
}

/* PoP: gca2__resolve_bot_user_id @ plugins/platforms/google_chat/adapter.py:_resolve_bot_user_id */
static json_t *gca2__resolve_bot_user_id(gca2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *candidates = json_array();
    const json_t *config = gca2_state(ctx, "config");
    json_t *hc = gca2_direct_attr(config, "home_channel");
    if (gca2_is_raise(hc)) { json_free(candidates); return hc; }
    if (gca2_truthy(hc)) {
        json_t *cid = gca2_direct_attr(hc, "chat_id");
        if (gca2_is_raise(cid)) { json_free(hc); json_free(candidates); return cid; }
        if (gca2_truthy(cid)) json_append(candidates, json_copy(cid));
        json_free(cid);
    }
    json_free(hc);
    const json_t *bs = gca2_state(ctx, "_bootstrap_spaces");
    if (gca2_truthy(bs)) {
        const char *s = gca2_str(bs);
        if (s) {
            const char *p = s;
            while (*p) {
                const char *comma = strchr(p, ',');
                size_t seglen = comma ? (size_t)(comma - p) : strlen(p);
                char *seg = malloc(seglen + 1);
                if (seg) { memcpy(seg, p, seglen); seg[seglen] = '\0'; }
                char *st = gca2_strip(seg ? seg : "");
                free(seg);
                if (st[0]) json_append(candidates, json_string(st));
                free(st);
                if (!comma) break;
                p = comma + 1;
            }
        }
    }
    json_t *result = NULL;
    for (size_t i = 0; i < json_len(candidates); i++) {
        const json_t *space = json_get(candidates, i);
        json_t *ta = json_object();
        json_set_hdr(ta, "fn", "members.list");
        json_t *aa = json_object();
        json_set(aa, "parent", json_copy(space));
        json_set(aa, "pageSize", json_number(50));
        json_set(ta, "args", aa);
        json_t *members = gca2_call(ctx, "asyncio.to_thread", ta);
        if (gca2_is_raise(members)) {
            if (gca2_isa(json_get_str(members, "__raise__", ""), "HttpError")) {
                json_t *txt = gca2_exc_text(ctx, members, json_array());
                if (gca2_is_raise(txt)) { json_free(members); result = txt; goto done; }
                json_t *la = json_array();
                json_append(la, json_copy(space));
                json_append(la, txt);
                json_t *lr = gca2_log(ctx, "debug", "[GoogleChat] members.list failed on %s: %s", la);
                json_free(members);
                if (gca2_is_raise(lr)) { result = lr; goto done; }
                json_free(lr);
                continue;
            }
            result = members; goto done;
        }
        if (!members || members->type != JSON_OBJECT) {
            result = gca2_get_attr_err(members, "get");
            json_free(members);
            goto done;
        }
        const json_t *mships = json_obj_get(members, "memberships");
        if (mships && mships->type != JSON_ARRAY) {
            char buf[128];
            snprintf(buf, sizeof buf, "'%s' object is not iterable", gca2_type_name(mships));
            json_free(members);
            result = gca2_err("TypeError", buf);
            goto done;
        }
        size_t nm = mships ? json_len(mships) : 0;
        for (size_t j = 0; j < nm; j++) {
            const json_t *member = json_get(mships, j);
            if (!member || member->type != JSON_OBJECT) {
                result = gca2_get_attr_err(member, "get");
                json_free(members);
                goto done;
            }
            const json_t *inner = json_obj_get(member, "member");
            bool inner_present = json_has(member, "member");
            if (inner_present && (!inner || inner->type != JSON_OBJECT)) {
                result = gca2_get_attr_err(inner, "get");
                json_free(members);
                goto done;
            }
            const json_t *type = inner ? json_obj_get(inner, "type") : NULL;
            const json_t *name = inner ? json_obj_get(inner, "name") : NULL;
            if (gca2_str_eq(type, "BOT") && gca2_truthy(name)) {
                result = json_copy(name);
                json_free(members);
                goto done;
            }
        }
        json_free(members);
    }
done:
    json_free(candidates);
    return result;
}

/* PoP: gca2__load_user_oauth @ plugins/platforms/google_chat/adapter.py:_load_user_oauth */
static json_t *gca2__load_user_oauth(gca2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *user_creds = NULL;
    json_t *ret = NULL;
    /* from .oauth import load_user_credentials, build_user_chat_service */
    json_t *ia1 = json_object();
    json_t *fl1 = json_array();
    json_append(fl1, json_string("load_user_credentials"));
    json_append(fl1, json_string("build_user_chat_service"));
    json_set(ia1, "fromlist", fl1);
    json_t *imp1 = gca2_call(ctx, "import.oauth", ia1);
    if (gca2_is_raise(imp1)) { ret = imp1; goto except; }
    json_free(imp1);
    /* from .oauth import list_authorized_emails */
    json_t *ia2 = json_object();
    json_t *fl2 = json_array();
    json_append(fl2, json_string("list_authorized_emails"));
    json_set(ia2, "fromlist", fl2);
    json_t *imp2 = gca2_call(ctx, "import.oauth", ia2);
    if (gca2_is_raise(imp2)) { ret = imp2; goto except; }
    json_free(imp2);
    json_t *ta1 = json_object();
    json_set_hdr(ta1, "fn", "oauth.load_user_credentials");
    json_set(ta1, "args", json_object());
    user_creds = gca2_call(ctx, "asyncio.to_thread", ta1);
    if (gca2_is_raise(user_creds)) { ret = user_creds; user_creds = NULL; goto except; }
    if (!gca2_is_null(user_creds)) {
        json_set(ctx->state, "_user_credentials", json_copy(user_creds));
        json_t *ta2 = json_object();
        json_set_hdr(ta2, "fn", "oauth.build_user_chat");
        json_t *aa2 = json_object();
        json_set(aa2, "creds", json_copy(user_creds));
        json_set(ta2, "args", aa2);
        json_t *api = gca2_call(ctx, "asyncio.to_thread", ta2);
        if (gca2_is_raise(api)) { ret = api; goto except; }
        json_set(ctx->state, "_user_chat_api", api);
        json_t *lr = gca2_log(ctx, "info",
            "[GoogleChat] Legacy user OAuth loaded \xe2\x80\x94 fallback attachment delivery enabled", json_array());
        if (gca2_is_raise(lr)) { ret = lr; goto except; }
        json_free(lr);
    }
    json_t *ta3 = json_object();
    json_set_hdr(ta3, "fn", "oauth.list_emails");
    json_set(ta3, "args", json_object());
    json_t *authorized = gca2_call(ctx, "asyncio.to_thread", ta3);
    if (gca2_is_raise(authorized)) { ret = authorized; goto except; }
    if (gca2_truthy(authorized)) {
        long long cnt = (long long)json_len(authorized);
        gca2_sb_t sb = {NULL, 0, 0};
        for (size_t i = 0; i < json_len(authorized); i++) {
            if (i) gca2_sb_puts(&sb, ", ");
            const char *e = gca2_str(json_get(authorized, i));
            gca2_sb_puts(&sb, e ? e : "");
        }
        json_t *la = json_array();
        json_append(la, json_number((double)cnt));
        json_append(la, json_string(sb.buf ? sb.buf : ""));
        free(sb.buf);
        json_t *lr = gca2_log(ctx, "info", "[GoogleChat] %d per-user OAuth tokens on disk: %s", la);
        json_free(authorized);
        if (gca2_is_raise(lr)) { ret = lr; goto except; }
        json_free(lr);
    } else if (gca2_is_null(user_creds)) {
        json_free(authorized);
        json_t *lr = gca2_log(ctx, "info",
            "[GoogleChat] No user OAuth tokens at setup \xe2\x80\x94 file attachments will degrade to text-only fallback. "
            "Each user runs /setup-files once in their own DM to enable native attachments.", json_array());
        if (gca2_is_raise(lr)) { ret = lr; goto except; }
        json_free(lr);
    } else {
        json_free(authorized);
    }
    json_free(user_creds);
    return NULL;
except:
    json_free(user_creds);
    if (gca2_raise_passes(ret)) return ret;
    json_t *txt = gca2_exc_text(ctx, ret, json_array());
    if (gca2_is_raise(txt)) { json_free(ret); return txt; }
    json_t *la = json_array();
    json_append(la, txt);
    json_t *lr = gca2_log(ctx, "warning",
        "[GoogleChat] User OAuth load failed (attachments will degrade to text-only fallback): %s", la);
    json_free(ret);
    if (gca2_is_raise(lr)) return lr;
    json_free(lr);
    json_set(ctx->state, "_user_credentials", json_null());
    json_set(ctx->state, "_user_chat_api", json_null());
    return NULL;
}

/* PoP: gca2__check_subscription @ plugins/platforms/google_chat/adapter.py:_check_subscription */
static json_t *gca2__check_subscription(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *subscription_path = json_obj_get(args, "subscription_path");
    const json_t *credentials = json_obj_get(args, "credentials");
    json_t *sa = json_object();
    json_set(sa, "credentials", credentials ? json_copy(credentials) : json_null());
    json_t *sub = gca2_call(ctx, "pubsub_v1.SubscriberClient", sa);
    if (gca2_is_raise(sub)) return sub;
    json_set(ctx->state, "_subscriber", sub);
    json_t *ta = json_object();
    json_set_hdr(ta, "fn", "get_subscription");
    json_t *aa = json_object();
    json_t *req = json_object();
    json_set(req, "subscription", subscription_path ? json_copy(subscription_path) : json_null());
    json_set(aa, "request", req);
    json_set(ta, "args", aa);
    json_t *r = gca2_call(ctx, "asyncio.to_thread", ta);
    if (gca2_is_raise(r)) {
        const char *kind = json_get_str(r, "__raise__", "");
        if (strcmp(kind, "NotFound") == 0 || strcmp(kind, "PermissionDenied") == 0) {
            const char *code = strcmp(kind, "NotFound") == 0 ? "subscription_not_found" : "subscription_permission";
            const char *message = strcmp(kind, "NotFound") == 0
                ? "Pub/Sub subscription not found at configured path"
                : "Service Account lacks roles/pubsub.subscriber on the subscription";
            json_free(r);
            json_t *fa = json_object();
            json_set_hdr(fa, "code", code);
            json_set_hdr(fa, "message", message);
            json_set(fa, "retryable", json_bool(false));
            json_t *fr = gca2_call(ctx, "_set_fatal_error", fa);
            if (gca2_is_raise(fr)) return fr;
            json_free(fr);
            return json_bool(false);
        }
        if (gca2_raise_passes(r)) return r;
        json_t *ca = json_object();
        json_set_hdr(ca, "log_fmt", "[GoogleChat] subscription.get failed: %s");
        json_set(ca, "exc", gca2_exc_double(r));
        json_set_hdr(ca, "code", "subscription_check");
        json_set(ca, "retryable", json_bool(true));
        json_free(r);
        return gca2_call(ctx, "_connect_failed", ca);
    }
    json_free(r);
    return json_bool(true);
}/* =====================================================================
 * Adapter: inbound (Pub/Sub callback, HTTP dispatch, message dispatch,
 * MessageEvent build, attachment download)
 * ===================================================================== */
/* (x or {}).get(key): falsy x reads as {}; truthy non-dict is out of corpus
 * domain (membership / space / sender intermediates are dicts or null). */
static const json_t *gca2_or_get(const json_t *x, const char *key) {
    return gca2_dict_get(gca2_truthy(x) ? x : NULL, key);
}
static const char *gca2_str_or(const json_t *v, const char *dflt) {
    const char *s = gca2_str(v);
    return (gca2_truthy(v) && s) ? s : dflt;
}

/* PoP: gca2__handle_pubsub_message @ plugins/platforms/google_chat/adapter.py:_handle_pubsub_message */
static json_t *gca2__handle_pubsub_message(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *message = json_obj_get(args, "message");
    const json_t *mattrs = gca2_obj_attrs(message);
    if (gca2_truthy(gca2_state(ctx, "_shutting_down"))) {
        json_t *nr = gca2_call(ctx, "message.nack", json_object());
        if (gca2_is_raise(nr)) return nr;
        json_free(nr);
        return NULL;
    }
    /* try: envelope = json.loads(message.data.decode("utf-8")) */
    const char *data = mattrs ? json_get_str(mattrs, "data", "") : "";
    json_t *la = json_object();
    json_set_hdr(la, "s", data);
    json_t *envelope = gca2_call(ctx, "json.loads", la);
    if (gca2_is_raise(envelope)) {
        if (gca2_raise_passes(envelope)) return envelope;
        json_free(envelope);
        json_t *lr = gca2_log(ctx, "exception", "[GoogleChat] Could not parse Pub/Sub envelope", json_array());
        if (gca2_is_raise(lr)) return lr;
        json_free(lr);
        json_t *ar = gca2_call(ctx, "message.ack", json_object());
        if (gca2_is_raise(ar)) return ar;
        json_free(ar);
        return NULL;
    }
    /* ce_type = dict(getattr(message,"attributes",{}) or {}).get("ce-type") or "" */
    const json_t *attributes = mattrs ? json_obj_get(mattrs, "attributes") : NULL;
    const json_t *cet = gca2_or_get(attributes, "ce-type");
    const char *ce_type = gca2_str_or(cet, "");
    /* logger.debug keys + ce-type (envelope must be a dict) */
    if (!envelope || envelope->type != JSON_OBJECT) {
        json_t *e = gca2_get_attr_err(envelope, "keys");
        json_free(envelope);
        return e;
    }
    json_t *keys = json_array();
    for (size_t i = 0; i < envelope->c.count; i++) json_append(keys, json_string(envelope->c.keys[i]));
    json_t *da = json_array();
    json_append(da, keys);
    json_append(da, json_string(ce_type));
    json_t *lr = gca2_log(ctx, "debug", "[GoogleChat] Envelope keys=%s, ce-type=%s", da);
    if (gca2_is_raise(lr)) { json_free(envelope); return lr; }
    json_free(lr);
    if (gca2_truthy(gca2_state(ctx, "_debug_raw"))) {
        char *dump = NULL;
        json_t *ia = json_object();
        json_t *fl = json_array();
        json_append(fl, json_string("redact_sensitive_text"));
        json_set(ia, "fromlist", fl);
        json_t *imp = gca2_call(ctx, "import.agent.redact", ia);
        if (gca2_is_raise(imp)) {
            if (gca2_raise_passes(imp)) { json_free(envelope); return imp; }
            json_free(imp);
            dump = gca2_strdup("<redact filter unavailable>");
        } else {
            json_free(imp);
            char *dj = gca2_dumps(envelope, false);
            json_t *ra = json_object();
            json_set_hdr(ra, "text", dj);
            free(dj);
            json_t *rr = gca2_call(ctx, "redact_sensitive_text", ra);
            if (gca2_is_raise(rr)) {
                if (gca2_raise_passes(rr)) { json_free(envelope); return rr; }
                json_free(rr);
                dump = gca2_strdup("<redact filter unavailable>");
            } else {
                const char *rs = gca2_str(rr);
                dump = gca2_strdup(rs ? rs : "");
                json_free(rr);
            }
        }
        char *head = gca2_slice_head(dump, 2000);
        free(dump);
        json_t *aa = json_array();
        json_append(aa, json_string(head ? head : ""));
        free(head);
        json_t *l2 = gca2_log(ctx, "debug", "[GoogleChat] RAW envelope (redacted): %s", aa);
        if (gca2_is_raise(l2)) { json_free(envelope); return l2; }
        json_free(l2);
    }
    /* main routing try */
    json_t *ret = NULL;
    bool membership = gca2_contains(ce_type, "membership") || gca2_contains(ce_type, "MEMBERSHIP");
    char *ce_low = gca2_lower(ce_type);
    bool widget = gca2_contains(ce_type, "widget") || gca2_contains(ce_low, "card");
    free(ce_low);
    if (membership) {
        const json_t *chat = json_obj_get(envelope, "chat");
        const json_t *mpl = gca2_or_get(chat, "membershipPayload");
        const json_t *space = gca2_or_get(mpl, "space");
        if (gca2_contains(ce_type, "created")) {
            const json_t *mship = gca2_or_get(mpl, "membership");
            const json_t *member = gca2_or_get(mship, "member");
            const json_t *mtype = gca2_dict_get(member, "type");
            const json_t *mname = gca2_dict_get(member, "name");
            if (gca2_str_eq(mtype, "BOT") && !gca2_truthy(gca2_state(ctx, "_bot_user_id")) && gca2_truthy(mname)) {
                json_set(ctx->state, "_bot_user_id", json_copy(mname));
                json_t *sa = json_object();
                json_set(sa, "bot_user_id", json_copy(mname));
                json_t *sr = gca2_call(ctx, "_save_cached_bot_id", sa);
                if (gca2_is_raise(sr)) { ret = sr; goto outer_except; }
                json_free(sr);
            }
            json_t *aa = json_array();
            json_append(aa, json_string(gca2_str_or(gca2_dict_get(space, "name"), "?")));
            json_t *l3 = gca2_log(ctx, "info", "[GoogleChat] ADDED_TO_SPACE %s", aa);
            if (gca2_is_raise(l3)) { ret = l3; goto outer_except; }
            json_free(l3);
        } else {
            json_t *aa = json_array();
            json_append(aa, json_string(gca2_str_or(gca2_dict_get(space, "name"), "?")));
            json_t *l3 = gca2_log(ctx, "info", "[GoogleChat] REMOVED_FROM_SPACE %s", aa);
            if (gca2_is_raise(l3)) { ret = l3; goto outer_except; }
            json_free(l3);
        }
    } else if (widget) {
        json_t *l3 = gca2_log(ctx, "info", "[GoogleChat] Card/widget event ack'd (v2 feature, deferred)", json_array());
        if (gca2_is_raise(l3)) { ret = l3; goto outer_except; }
        json_free(l3);
    } else {
        json_t *pa = json_object();
        json_set(pa, "envelope", json_copy(envelope));
        json_set_hdr(pa, "ce_type", ce_type);
        json_t *prepared = gca2_call(ctx, "_prepare_inbound", pa);
        if (gca2_is_raise(prepared)) { ret = prepared; goto outer_except; }
        if (!gca2_is_null(prepared)) {
            json_t *msg, *env;
            json_t *bad = gca2_unpack2(prepared, &msg, &env);
            if (bad) { json_free(prepared); ret = bad; goto outer_except; }
            json_t *coro = json_object();
            json_set_hdr(coro, "__coro__", "_dispatch_message");
            json_t *ca = json_object();
            json_set(ca, "msg", json_copy(msg));
            json_set(ca, "envelope", json_copy(env));
            json_set(coro, "args", ca);
            json_free(prepared);
            json_t *sa = json_object();
            json_set(sa, "coro", coro);
            json_t *sr = gca2_call(ctx, "_submit_on_loop", sa);
            if (gca2_is_raise(sr)) { ret = sr; goto outer_except; }
            json_free(sr);
        } else {
            json_free(prepared);
        }
    }
    {
        json_t *ar = gca2_call(ctx, "message.ack", json_object());
        if (gca2_is_raise(ar)) { ret = ar; goto outer_except; }
        json_free(ar);
    }
    json_free(envelope);
    return NULL;
outer_except:
    json_free(envelope);
    if (gca2_raise_passes(ret)) return ret;
    json_free(ret);
    json_t *l4 = gca2_log(ctx, "exception", "[GoogleChat] Error in _on_pubsub_message", json_array());
    if (gca2_is_raise(l4)) return l4;
    json_free(l4);
    json_t *ar = gca2_call(ctx, "message.ack", json_object());
    if (gca2_is_raise(ar)) {
        if (gca2_raise_passes(ar)) return ar;
        json_free(ar);
    } else {
        json_free(ar);
    }
    return NULL;
}

/* PoP: gca2_dispatch_http_event @ plugins/platforms/google_chat/adapter.py:dispatch_http_event */
static json_t *gca2_dispatch_http_event(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *envelope = json_obj_get(args, "envelope");
    json_t *ia = json_object();
    json_t *fl = json_array();
    json_append(fl, json_string("ParkedRouteRefused"));
    json_append(fl, json_string("retryable_ingress"));
    json_set(ia, "fromlist", fl);
    json_t *imp = gca2_call(ctx, "import.gateway.parked_route_refusal", ia);
    if (gca2_is_raise(imp)) return imp;
    json_free(imp);
    json_t *pa = json_object();
    json_set(pa, "envelope", envelope ? json_copy(envelope) : json_null());
    json_set(pa, "ce_type", json_null());
    json_t *prepared = gca2_call(ctx, "_prepare_inbound", pa);
    if (gca2_is_raise(prepared)) return prepared;
    if (gca2_is_null(prepared)) { json_free(prepared); return json_object(); }
    json_t *msg, *env;
    json_t *bad = gca2_unpack2(prepared, &msg, &env);
    if (bad) { json_free(prepared); return bad; }
    json_t *scope = gca2_call(ctx, "retryable_ingress", json_object());
    if (gca2_is_raise(scope)) { json_free(prepared); return scope; }
    json_t *da = json_object();
    json_set(da, "msg", json_copy(msg));
    json_set(da, "envelope", json_copy(env));
    json_t *dr = gca2_call(ctx, "_dispatch_message", da);
    if (gca2_is_raise(dr)) { json_free(scope); json_free(prepared); return dr; }
    json_free(dr);
    bool refused = gca2_truthy(gca2_dict_get(scope, "refused"));
    json_free(scope);
    if (refused) {
        json_t *ra = json_object();
        json_set(ra, "msg", json_copy(msg));
        json_set(ra, "envelope", json_copy(env));
        json_t *rr = gca2_call(ctx, "_release_unadmitted", ra);
        json_free(prepared);
        if (gca2_is_raise(rr)) return rr;
        json_free(rr);
        return gca2_err("ParkedRouteRefused", GCA2_PARKED_MSG);
    }
    json_free(prepared);
    return json_object();
}

/* PoP: gca2__dispatch_message @ plugins/platforms/google_chat/adapter.py:_dispatch_message */
static json_t *gca2__dispatch_message(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *msg = json_obj_get(args, "msg");
    const json_t *envelope = json_obj_get(args, "envelope");
    json_t *ba = json_object();
    json_set(ba, "msg", msg ? json_copy(msg) : json_null());
    json_set(ba, "envelope", envelope ? json_copy(envelope) : json_null());
    json_t *event = gca2_call(ctx, "_build_message_event", ba);
    if (gca2_is_raise(event)) {
        if (gca2_raise_passes(event)) return event;
        goto except;
    }
    if (gca2_is_null(event)) { json_free(event); return NULL; }
    const json_t *eattrs = gca2_obj_attrs(event);
    const json_t *etext = eattrs ? json_obj_get(eattrs, "text") : NULL;
    char *text = gca2_strip(gca2_str_or(etext, ""));
    const json_t *source = eattrs ? json_obj_get(eattrs, "source") : NULL;
    bool intercept = false;
    if (gca2_starts_with(text, "/setup-files") && !gca2_is_null(source)) {
        const json_t *sattrs = gca2_obj_attrs(source);
        json_t *aa = json_object();
        json_set(aa, "user_id", sattrs ? json_copy(json_obj_get(sattrs, "user_id")) : json_null());
        json_set(aa, "chat_type", sattrs ? json_copy(json_obj_get(sattrs, "chat_type")) : json_null());
        json_set(aa, "chat_id", sattrs ? json_copy(json_obj_get(sattrs, "chat_id")) : json_null());
        json_set(aa, "thread_id", json_null());
        json_t *auth = gca2_call(ctx, "_is_sender_authorized", aa);
        if (gca2_is_raise(auth)) { free(text); json_free(event); if (gca2_raise_passes(auth)) return auth; event = auth; goto except; }
        bool is_true = auth && auth->type == JSON_BOOL && auth->bool_val;
        json_free(auth);
        if (is_true) {
            const json_t *ualt = sattrs ? json_obj_get(sattrs, "user_id_alt") : NULL;
            json_t *ha = json_object();
            json_set(ha, "chat_id", sattrs ? json_copy(json_obj_get(sattrs, "chat_id")) : json_null());
            json_set(ha, "thread_id", sattrs ? json_copy(json_obj_get(sattrs, "thread_id")) : json_null());
            json_set_hdr(ha, "raw_text", text);
            json_set(ha, "sender_email", gca2_truthy(ualt) ? json_copy(ualt) : json_null());
            json_t *hr = gca2_call(ctx, "_handle_setup_files_command", ha);
            if (gca2_is_raise(hr)) { free(text); json_free(event); if (gca2_raise_passes(hr)) return hr; event = hr; goto except; }
            intercept = gca2_truthy(hr);
            json_free(hr);
        }
    }
    free(text);
    if (intercept) { json_free(event); return NULL; }
    json_t *ha = json_object();
    json_set(ha, "event", json_copy(event));
    json_t *hr = gca2_call(ctx, "handle_message", ha);
    json_free(event);
    if (gca2_is_raise(hr)) {
        if (gca2_raise_passes(hr)) return hr;
        event = hr;
        goto except;
    }
    json_free(hr);
    return NULL;
except:
    if (gca2_is_raise(event) && gca2_raise_passes(event)) return event;
    json_free(event);
    json_t *lr = gca2_log(ctx, "exception", "[GoogleChat] _dispatch_message failed", json_array());
    if (gca2_is_raise(lr)) return lr;
    json_free(lr);
    return NULL;
}

/* PoP: gca2__build_message_event @ plugins/platforms/google_chat/adapter.py:_build_message_event */
static json_t *gca2__build_message_event(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *msg = json_obj_get(args, "msg");
    const json_t *envelope = json_obj_get(args, "envelope");
    const json_t *esp = gca2_dict_get(envelope, "space");
    const json_t *msp = gca2_dict_get(msg, "space");
    const json_t *space = gca2_truthy(esp) ? esp : (gca2_truthy(msp) ? msp : NULL);
    const char *space_name = gca2_str_or(gca2_dict_get(space, "name"), "");
    const json_t *st1 = gca2_dict_get(space, "type");
    const json_t *st2 = gca2_dict_get(space, "spaceType");
    char *space_type = gca2_upper(gca2_str_or(gca2_truthy(st1) ? st1 : st2, ""));
    const json_t *th = gca2_dict_get(msg, "thread");
    const json_t *tn = gca2_or_get(th, "name");
    const char *thread_name = gca2_truthy(tn) ? gca2_str(tn) : NULL;
    const json_t *sender = gca2_dict_get(msg, "sender");
    const json_t *sender_o = gca2_truthy(sender) ? sender : NULL;
    const char *sender_name = gca2_str_or(gca2_dict_get(sender_o, "name"), "");
    const char *sender_email = gca2_str_or(gca2_dict_get(sender_o, "email"), "");
    if (sender_email[0] && space_name[0]) {
        char *se = gca2_strip(sender_email);
        char *sel = gca2_lower(se);
        free(se);
        json_t *ls = gca2_state_obj(ctx, "_last_sender_by_chat");
        json_set_hdr(ls, space_name, sel);
        free(sel);
    }
    const char *chat_type = (strcmp(space_type, "DIRECT_MESSAGE") == 0 || strcmp(space_type, "DM") == 0) ? "dm" : "group";
    free(space_type);
    const json_t *at = gca2_dict_get(msg, "argumentText");
    const json_t *tx = gca2_dict_get(msg, "text");
    char *text = gca2_strip(gca2_str_or(gca2_truthy(at) ? at : tx, ""));
    const json_t *slash = gca2_dict_get(msg, "slashCommand");
    bool has_slash = gca2_truthy(slash);
    if (has_slash) {
        const json_t *cid = gca2_dict_get(slash, "commandId");
        char *cid_s = gca2_str_of(gca2_truthy(cid) ? cid : NULL);
        if (!gca2_truthy(cid)) { free(cid_s); cid_s = gca2_strdup(""); }
        if (cid_s[0] && !gca2_starts_with(text, "/")) {
            size_t n = strlen("/cmd_") + strlen(cid_s) + 1 + strlen(text) + 1;
            char *buf = malloc(n);
            if (buf) snprintf(buf, n, "/cmd_%s %s", cid_s, text);
            char *nt = gca2_strip(buf ? buf : "");
            free(buf); free(text);
            text = nt;
        }
        free(cid_s);
    }
    /* prev_thread_count */
    double prev = 0;
    if (thread_name && thread_name[0] && space_name[0]) {
        json_t *ia = json_object();
        json_set_hdr(ia, "chat_id", space_name);
        json_set_hdr(ia, "thread_name", thread_name);
        json_t *ir = gca2_call(ctx, "thread_count_store.incr", ia);
        if (gca2_is_raise(ir)) { free(text); return ir; }
        prev = (ir && ir->type == JSON_NUMBER) ? ir->num_val : 0;
        json_free(ir);
    }
    const char *session_thread_id = NULL;
    json_t *lit = gca2_state_obj(ctx, "_last_inbound_thread");
    if (strcmp(chat_type, "dm") == 0) {
        bool is_side = prev > 0;
        session_thread_id = is_side ? thread_name : NULL;
        if (thread_name && thread_name[0] && space_name[0] && is_side)
            json_set_hdr(lit, space_name, thread_name);
        else if (space_name[0])
            json_obj_del(lit, space_name);
    } else {
        session_thread_id = thread_name;
        if (thread_name && thread_name[0] && space_name[0])
            json_set_hdr(lit, space_name, thread_name);
    }
    json_t *media_urls = json_array();
    json_t *media_types = json_array();
    char mt_buf[64] = "text";
    const char *message_type = mt_buf;
    json_t *attachments = json_copy(gca2_dict_get(msg, "attachment"));
    if (!attachments || attachments->type != JSON_ARRAY) { json_free(attachments); attachments = json_array(); }
    if (json_len(attachments) > 0) {
        json_t *aa = json_object();
        json_set_hdr(aa, "user_id", sender_email[0] ? sender_email : sender_name);
        json_set_hdr(aa, "chat_type", chat_type);
        json_set_hdr(aa, "chat_id", space_name);
        json_set(aa, "thread_id", session_thread_id ? json_string(session_thread_id) : json_null());
        json_t *auth = gca2_call(ctx, "_is_sender_authorized", aa);
        if (gca2_is_raise(auth)) { free(text); json_free(media_urls); json_free(media_types); json_free(attachments); return auth; }
        bool is_true = auth && auth->type == JSON_BOOL && auth->bool_val;
        json_free(auth);
        if (!is_true) { json_free(attachments); attachments = json_array(); }
    }
    for (size_t i = 0; i < json_len(attachments); i++) {
        const json_t *att = json_get(attachments, i);
        json_t *da = json_object();
        json_set(da, "attachment", json_copy(att));
        json_t *r = gca2_call(ctx, "_download_attachment", da);
        json_t *exc = NULL;
        json_t *lp = NULL, *mime = NULL;
        if (gca2_is_raise(r)) {
            if (gca2_raise_passes(r)) { free(text); json_free(media_urls); json_free(media_types); json_free(attachments); return r; }
            exc = r;
        } else {
            json_t *bad = gca2_unpack2(r, &lp, &mime);
            if (bad) { json_free(r); exc = bad; }
        }
        if (exc) {
            json_t *txt = gca2_exc_text(ctx, exc, json_array());
            if (gca2_is_raise(txt)) { json_free(exc); free(text); json_free(media_urls); json_free(media_types); json_free(attachments); return txt; }
            json_t *px = gca2_exc_double(exc);
            json_t *pa = json_object();
            json_set(pa, "exc", px);
            json_set(pa, "targets", json_array());
            json_t *pr = gca2_call(ctx, "_proxy_error", pa);
            json_free(exc);
            if (gca2_is_raise(pr)) { json_free(txt); free(text); json_free(media_urls); json_free(media_types); json_free(attachments); return pr; }
            json_free(pr);
            json_t *la = json_array();
            json_append(la, txt);
            json_t *lr = gca2_log(ctx, "error", "[GoogleChat] attachment download failed: %s", la);
            if (gca2_is_raise(lr)) { free(text); json_free(media_urls); json_free(media_types); json_free(attachments); return lr; }
            json_free(lr);
            continue;
        }
        /* lp/mime BORROW into r (gca2_unpack2) — keep r alive until the
         * last use of both (freeing it here was a UAF: the mime seam arg
         * read reused heap garbage on unlucky layouts). */
        if (!gca2_truthy(lp)) { json_free(r); continue; }
        json_append(media_urls, json_copy(lp));
        json_append(media_types, json_string(gca2_str_or(mime, "application/octet-stream")));
        if (strcmp(message_type, "text") == 0 && !text[0]) {
            json_t *ma = json_object();
            json_set_hdr(ma, "mime", gca2_str_or(mime, ""));
            json_t *mr = gca2_call(ctx, "_mime_for_message_type", ma);
            if (gca2_is_raise(mr)) { json_free(r); free(text); json_free(media_urls); json_free(media_types); json_free(attachments); return mr; }
            const char *ms = gca2_str(mr);
            snprintf(mt_buf, sizeof mt_buf, "%s", ms ? ms : "text");
            message_type = mt_buf;
            json_free(mr);
        }
        json_free(r);
    }
    json_free(attachments);
    if (has_slash) message_type = "command";
    /* build_source */
    json_t *bs = json_object();
    json_set_hdr(bs, "chat_id", space_name);
    const json_t *dn = gca2_dict_get(space, "displayName");
    const json_t *sn = gca2_dict_get(space, "name");
    json_set_hdr(bs, "chat_name", gca2_str_or(gca2_truthy(dn) ? dn : sn, ""));
    json_set_hdr(bs, "chat_type", chat_type);
    json_set_hdr(bs, "user_id", sender_email[0] ? sender_email : sender_name);
    const json_t *sdn = gca2_dict_get(sender_o, "displayName");
    json_set_hdr(bs, "user_name", gca2_str_or(gca2_truthy(sdn) ? sdn : NULL, sender_email[0] ? sender_email : sender_name));
    json_set(bs, "thread_id", session_thread_id ? json_string(session_thread_id) : json_null());
    json_set(bs, "user_id_alt", sender_name[0] ? json_string(sender_name) : json_null());
    const json_t *mn = gca2_dict_get(msg, "name");
    json_set(bs, "message_id", gca2_truthy(mn) ? json_copy(mn) : json_null());
    json_t *source = gca2_call(ctx, "build_source", bs);
    if (gca2_is_raise(source)) { free(text); json_free(media_urls); json_free(media_types); return source; }
    /* MessageEvent */
    json_t *me = json_object();
    json_set_hdr(me, "text", text);
    json_set_hdr(me, "message_type", message_type);
    json_set(me, "source", source);
    json_set(me, "raw_message", msg ? json_copy(msg) : json_null());
    json_set(me, "message_id", gca2_truthy(mn) ? json_copy(mn) : json_null());
    json_set(me, "media_urls", media_urls);
    json_set(me, "media_types", media_types);
    free(text);
    return gca2_call(ctx, "MessageEvent", me);
}

/* PoP: gca2__download_attachment @ plugins/platforms/google_chat/adapter.py:_download_attachment */
static json_t *gca2__download_attachment(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *attachment = json_obj_get(args, "attachment");
    const char *mime = gca2_str_or(gca2_dict_get(attachment, "contentType"), "");
    const char *name = gca2_str_or(gca2_dict_get(attachment, "name"), "");
    const json_t *adr = gca2_dict_get(attachment, "attachmentDataRef");
    const char *resource_name = gca2_str_or(gca2_or_get(adr, "resourceName"), "");
    const char *download_uri = gca2_str_or(gca2_dict_get(attachment, "downloadUri"), "");
    const char *src = gca2_str_or(gca2_dict_get(attachment, "source"), "");
    json_t *result = NULL;
    if (strcmp(src, "DRIVE_FILE") == 0 && !resource_name[0]) {
        json_t *lr = gca2_log(ctx, "info",
            "[GoogleChat] Skipping Drive-picker attachment (no resourceName, would need user-OAuth Drive scope)", json_array());
        if (gca2_is_raise(lr)) return lr;
        json_free(lr);
        result = json_array();
        json_append(result, json_null());
        json_append(result, json_string(mime));
        return result;
    }
    json_t *data = NULL;
    if (resource_name[0]) {
        json_t *ta = json_object();
        json_set_hdr(ta, "fn", "media.download");
        json_t *aa = json_object();
        json_set_hdr(aa, "resourceName", resource_name);
        json_set(ta, "args", aa);
        data = gca2_call(ctx, "asyncio.to_thread", ta);
        if (gca2_is_raise(data)) {
            if (gca2_isa(json_get_str(data, "__raise__", ""), "HttpError")) {
                json_t *txt = gca2_exc_text(ctx, data, json_array());
                if (gca2_is_raise(txt)) { json_free(data); return txt; }
                json_t *la = json_array();
                json_append(la, txt);
                json_t *lr = gca2_log(ctx, "warning", "[GoogleChat] media.download_media failed: %s", la);
                json_free(data);
                data = NULL;
                if (gca2_is_raise(lr)) return lr;
                json_free(lr);
            } else {
                return data;
            }
        }
    }
    if (gca2_is_null(data) && download_uri[0]) {
        json_t *ua = json_object();
        json_set_hdr(ua, "url", download_uri);
        json_t *owned = gca2_call(ctx, "_is_google_owned_host", ua);
        if (gca2_is_raise(owned)) { json_free(data); return owned; }
        bool ok = gca2_truthy(owned);
        json_free(owned);
        if (!ok) {
            json_free(data);
            json_t *lr = gca2_log(ctx, "warning", "[GoogleChat] Rejecting attachment fetch: non-Google host", json_array());
            if (gca2_is_raise(lr)) return lr;
            json_free(lr);
            result = json_array();
            json_append(result, json_null());
            json_append(result, json_string(mime));
            return result;
        }
        json_t *ta = json_object();
        json_set_hdr(ta, "fn", "download_uri.fetch");
        json_t *aa = json_object();
        json_set_hdr(aa, "url", download_uri);
        json_set(aa, "timeout", json_number(30));
        json_set(ta, "args", aa);
        data = gca2_call(ctx, "asyncio.to_thread", ta);
        if (gca2_is_raise(data)) {
            if (gca2_raise_passes(data)) return data;
            json_t *targets = json_array();
            json_append(targets, json_string(download_uri));
            json_append(targets, json_string("https://oauth2.googleapis.com"));
            json_t *txt = gca2_exc_text(ctx, data, targets);
            if (gca2_is_raise(txt)) { json_free(data); return txt; }
            json_t *la = json_array();
            json_append(la, txt);
            json_t *lr = gca2_log(ctx, "warning",
                "[GoogleChat] downloadUri fetch failed (SA tokens often lack access here; this is expected for "
                "user-uploaded content): %s", la);
            json_free(data);
            if (gca2_is_raise(lr)) return lr;
            json_free(lr);
            result = json_array();
            json_append(result, json_null());
            json_append(result, json_string(mime));
            return result;
        }
    }
    if (gca2_is_null(data)) {
        json_free(data);
        result = json_array();
        json_append(result, json_null());
        json_append(result, json_string(mime));
        return result;
    }
    /* filename / ext */
    char *filename;
    if (name[0]) {
        const char *slash = strrchr(name, '/');
        filename = gca2_strdup(slash ? slash + 1 : name);
    } else {
        filename = gca2_strdup("attachment");
    }
    char *ext = gca2_strdup("");
    if (strchr(filename, '.')) {
        const char *dot = strrchr(filename, '.');
        char *low = gca2_lower(dot + 1);
        free(ext);
        ext = malloc(strlen(low) + 2);
        if (ext) sprintf(ext, ".%s", low);
        free(low);
    }
    static const struct { const char *prefix, *seam, *dext; } cachers[] = {
        {"image/", "cache_image_from_bytes_async", ".jpg"},
        {"audio/", "cache_audio_from_bytes_async", ".ogg"},
        {"video/", "cache_video_from_bytes_async", ".mp4"},
    };
    for (size_t i = 0; i < 3; i++) {
        if (gca2_starts_with(mime, cachers[i].prefix)) {
            json_t *ca = json_object();
            json_set(ca, "data", data);
            json_set_hdr(ca, "ext", ext[0] ? ext : cachers[i].dext);
            json_t *path = gca2_call(ctx, cachers[i].seam, ca);
            free(filename); free(ext);
            if (gca2_is_raise(path)) return path;
            result = json_array();
            json_append(result, path);
            json_append(result, json_string(mime));
            return result;
        }
    }
    json_t *ca = json_object();
    json_set(ca, "data", data);
    json_set_hdr(ca, "filename", filename);
    json_t *path = gca2_call(ctx, "cache_document_from_bytes_async", ca);
    free(filename); free(ext);
    if (gca2_is_raise(path)) return path;
    result = json_array();
    json_append(result, path);
    json_append(result, json_string(mime));
    return result;
}/* =====================================================================
 * Adapter: outbound (patch / create / retry / send / send_card / edit /
 * delete)
 * ===================================================================== */
/* PoP: gca2__patch_message @ plugins/platforms/google_chat/adapter.py:_patch_message */
static json_t *gca2__patch_message(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *message_name = json_obj_get(args, "message_name");
    const json_t *body = json_obj_get(args, "body");
    /* update_mask = ",".join(k for k in ("text","cardsV2") if k in body) or "text" */
    gca2_sb_t sb = {NULL, 0, 0};
    if (body && body->type == JSON_OBJECT) {
        if (json_has(body, "text")) gca2_sb_puts(&sb, "text");
        if (json_has(body, "cardsV2")) {
            if (sb.len) gca2_sb_putc(&sb, ',');
            gca2_sb_puts(&sb, "cardsV2");
        }
    }
    const char *update_mask = (sb.len && sb.buf) ? sb.buf : "text";
    /* patch_body = {k:v for k,v in body.items() if k != "thread"} */
    json_t *patch_body = json_object();
    if (body && body->type == JSON_OBJECT)
        for (size_t i = 0; i < body->c.count; i++)
            if (strcmp(body->c.keys[i], "thread") != 0)
                json_set(patch_body, body->c.keys[i], json_copy(body->c.items[i]));
    json_t *ta = json_object();
    json_set_hdr(ta, "fn", "messages.patch");
    json_t *aa = json_object();
    json_set(aa, "name", message_name ? json_copy(message_name) : json_null());
    json_set_hdr(aa, "updateMask", update_mask);
    json_set(aa, "body", patch_body);
    json_set(ta, "args", aa);
    free(sb.buf);
    json_t *resp = gca2_call(ctx, "asyncio.to_thread", ta);
    if (gca2_is_raise(resp)) return resp;
    if (!resp || resp->type != JSON_OBJECT) {
        json_t *e = gca2_get_attr_err(resp, "get");
        json_free(resp);
        return e;
    }
    json_t *mid = json_has(resp, "name") ? json_copy(json_obj_get(resp, "name"))
                                         : (message_name ? json_copy(message_name) : json_null());
    json_free(resp);
    return gca2_sr_new(true, mid, NULL, false);
}

/* PoP: gca2__create_message @ plugins/platforms/google_chat/adapter.py:_create_message */
static json_t *gca2__create_message(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *chat_id = json_obj_get(args, "chat_id");
    const json_t *body = json_obj_get(args, "body");
    json_t *ka = json_object();
    json_set(ka, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_set(ka, "body", body ? json_copy(body) : json_null());
    json_t *kwargs = gca2_call(ctx, "_create_kwargs", ka);
    if (gca2_is_raise(kwargs)) return kwargs;
    /* the live sync_fn is a bare lambda: proj_callable -> {"__callable__":
     * "<lambda>", "args": {}} (closure kwargs are not projected). */
    json_t *sync_fn = json_object();
    json_set_hdr(sync_fn, "__callable__", "<lambda>");
    json_set(sync_fn, "args", json_object());
    json_free(kwargs);
    json_t *ca = json_object();
    json_set(ca, "sync_fn", sync_fn);
    json_set_hdr(ca, "op_name", "messages.create");
    json_t *resp = gca2_call(ctx, "_call_with_retry", ca);
    if (gca2_is_raise(resp)) return resp;
    json_t *ta = json_object();
    json_set(ta, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_set(ta, "resp", json_copy(resp));
    json_t *tr = gca2_call(ctx, "_track_outbound_thread", ta);
    if (gca2_is_raise(tr)) { json_free(resp); return tr; }
    json_free(tr);
    if (!resp || resp->type != JSON_OBJECT) {
        json_t *e = gca2_get_attr_err(resp, "get");
        json_free(resp);
        return e;
    }
    json_t *mid = json_has(resp, "name") ? json_copy(json_obj_get(resp, "name")) : json_null();
    json_free(resp);
    return gca2_sr_new(true, mid, NULL, false);
}

/* PoP: gca2__call_with_retry @ plugins/platforms/google_chat/adapter.py:_call_with_retry */
static json_t *gca2__call_with_retry(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *sync_fn = json_obj_get(args, "sync_fn");
    const char *marker = json_get_str(sync_fn, "__callable__", "op");
    const char *op_name = json_get_str(args, "op_name", "chat-api-call");
    double delay = GCA2_RETRY_BASE_DELAY;
    for (int attempt = 1; attempt <= GCA2_RETRY_MAX_ATTEMPTS; attempt++) {
        json_t *ta = json_object();
        json_set_hdr(ta, "fn", marker);
        json_set(ta, "args", json_object());
        json_t *r = gca2_call(ctx, "asyncio.to_thread", ta);
        if (!gca2_is_raise(r)) return r;
        if (gca2_raise_passes(r)) return r;
        /* except Exception */
        json_t *ia = json_object();
        json_set(ia, "exc", gca2_exc_double(r));
        json_t *retry = gca2_call(ctx, "_is_retryable_error", ia);
        if (gca2_is_raise(retry)) { json_free(r); return retry; }
        bool retryable = gca2_truthy(retry);
        json_free(retry);
        if (!retryable || attempt >= GCA2_RETRY_MAX_ATTEMPTS) return r;
        json_t *rr = gca2_call(ctx, "random.random", json_object());
        if (gca2_is_raise(rr)) { json_free(r); return rr; }
        double rv = (rr && rr->type == JSON_NUMBER) ? rr->num_val : 0;
        json_free(rr);
        double wait = delay + delay * GCA2_RETRY_JITTER * rv;
        double cap = GCA2_RETRY_MAX_DELAY + GCA2_RETRY_JITTER;
        if (wait > cap) wait = cap;
        json_t *txt = gca2_exc_text(ctx, r, json_array());
        if (gca2_is_raise(txt)) { json_free(r); return txt; }
        json_t *la = json_array();
        json_append(la, json_string(op_name));
        json_append(la, json_number(attempt));
        json_append(la, json_number(GCA2_RETRY_MAX_ATTEMPTS));
        json_append(la, txt);
        json_append(la, json_number(wait));
        json_t *lr = gca2_log(ctx, "warning",
            "[GoogleChat] %s attempt %d/%d failed (%s); retrying in %.2fs", la);
        json_free(r);
        if (gca2_is_raise(lr)) return lr;
        json_free(lr);
        json_t *sl = json_object();
        json_set(sl, "seconds", json_number(wait));
        json_t *sr = gca2_call(ctx, "asyncio.sleep", sl);
        if (gca2_is_raise(sr)) return sr;
        json_free(sr);
        delay = delay * 2;
        if (delay > GCA2_RETRY_MAX_DELAY) delay = GCA2_RETRY_MAX_DELAY;
    }
    return NULL;
}

/* PoP: gca2_send @ plugins/platforms/google_chat/adapter.py:send */
static json_t *gca2_send(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *chat_id = json_obj_get(args, "chat_id");
    const json_t *content = json_obj_get(args, "content");
    const json_t *reply_to = json_obj_get(args, "reply_to");
    const json_t *metadata = json_obj_get(args, "metadata");
    const char *cid = gca2_str(chat_id);
    if (!cid) cid = "";
    json_t *ret = NULL;
    json_t *ra = json_object();
    json_set(ra, "reply_to", reply_to ? json_copy(reply_to) : json_null());
    json_set(ra, "metadata", metadata ? json_copy(metadata) : json_null());
    json_set(ra, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_t *thread_id = gca2_call(ctx, "_resolve_thread_id", ra);
    if (gca2_is_raise(thread_id)) return thread_id;  /* before try: no resume */
    json_t *pa = json_object();
    json_set(pa, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_t *pr = gca2_call(ctx, "pause_typing_for_chat", pa);
    if (gca2_is_raise(pr)) { json_free(thread_id); return pr; }  /* before try: no resume */
    json_free(pr);
    json_t *fa = json_object();
    json_set(fa, "content", content ? json_copy(content) : json_null());
    json_t *fmt = gca2_call(ctx, "format_message", fa);
    if (gca2_is_raise(fmt)) { json_free(thread_id); ret = fmt; goto finally; }
    json_t *ca = json_object();
    json_set(ca, "text", fmt);
    json_t *chunks = gca2_call(ctx, "_chunk_text", ca);
    if (gca2_is_raise(chunks)) { json_free(thread_id); ret = chunks; goto finally; }
    if (!gca2_truthy(chunks)) {
        json_free(chunks); json_free(thread_id);
        ret = gca2_sr_new(false, NULL, json_string("empty message"), false);
        goto finally;
    }
    if (chunks->type != JSON_ARRAY) {
        char buf[128];
        snprintf(buf, sizeof buf, "'%s' object is not iterable", gca2_type_name(chunks));
        json_free(chunks); json_free(thread_id);
        ret = gca2_err("TypeError", buf);
        goto finally;
    }
    json_t *tm = gca2_state_obj(ctx, "_typing_messages");
    json_t *typing = json_obj_get(tm, cid);
    json_t *typing_msg_name = typing ? json_copy(typing) : NULL;
    json_obj_del(tm, cid);
    if (gca2_str_eq(typing_msg_name, GCA2_TYPING_CONSUMED_SENTINEL)) {
        json_free(typing_msg_name);
        typing_msg_name = NULL;
    }
    bool patched_typing = false;
    json_t *last_result = NULL;
    for (size_t idx = 0; idx < json_len(chunks); idx++) {
        const json_t *chunk = json_get(chunks, idx);
        bool use_thread = (idx > 0) || !gca2_truthy(typing_msg_name);
        json_t *ba = json_object();
        json_set(ba, "text", json_copy(chunk));
        json_set(ba, "thread_id", use_thread ? json_copy(thread_id) : json_null());
        json_t *body = gca2_call(ctx, "_thread_body", ba);
        if (gca2_is_raise(body)) { ret = body; goto loop_done; }
        json_t *exc = NULL;
        if (idx == 0 && gca2_truthy(typing_msg_name)) {
            json_t *ma = json_object();
            json_set(ma, "message_name", json_copy(typing_msg_name));
            json_set(ma, "body", body);
            json_t *mr = gca2_call(ctx, "_patch_message", ma);
            if (gca2_is_raise(mr)) { exc = mr; }
            else { last_result = gca2_sr_norm(mr); json_free(mr); patched_typing = true; }
        } else {
            json_t *ma = json_object();
            json_set(ma, "chat_id", chat_id ? json_copy(chat_id) : json_null());
            json_set(ma, "body", body);
            json_t *mr = gca2_call(ctx, "_create_message", ma);
            if (gca2_is_raise(mr)) { exc = mr; }
            else { last_result = gca2_sr_norm(mr); json_free(mr); }
        }
        if (exc) {
            if (!gca2_isa(json_get_str(exc, "__raise__", ""), "HttpError")) {
                ret = exc; goto loop_done;  /* non-HttpError / BaseException propagates */
            }
            json_t *sa = json_object();
            json_set(sa, "exc", gca2_exc_double(exc));
            json_t *status = gca2_call(ctx, "_http_status", sa);
            if (gca2_is_raise(status)) { json_free(exc); ret = status; goto loop_done; }
            double sv = (status && status->type == JSON_NUMBER) ? status->num_val : -1;
            bool is_bool = status && status->type == JSON_BOOL;
            json_free(status);
            if (!is_bool && sv == 403) {
                json_t *ffa = json_object();
                json_set_hdr(ffa, "code", "chat_forbidden");
                json_set_hdr(ffa, "message", "Bot lacks access (removed from space or perms revoked)");
                json_set(ffa, "retryable", json_bool(false));
                json_t *ffr = gca2_call(ctx, "_set_fatal_error", ffa);
                if (gca2_is_raise(ffr)) { json_free(exc); ret = ffr; goto loop_done; }
                json_free(ffr);
                ret = gca2_sr_new(false, NULL, json_string(json_get_str(exc, "message", "")), false);
                json_free(exc);
                goto loop_done;
            }
            if (!is_bool && sv == 404) {
                if (idx == 0 && gca2_truthy(typing_msg_name)) {
                    json_t *lr = gca2_log(ctx, "info", "[GoogleChat] Typing card disappeared; creating new message", json_array());
                    if (gca2_is_raise(lr)) { json_free(exc); ret = lr; goto loop_done; }
                    json_free(lr);
                    json_free(typing_msg_name);
                    typing_msg_name = NULL;
                    json_t *ma = json_object();
                    json_set(ma, "chat_id", chat_id ? json_copy(chat_id) : json_null());
                    json_set(ma, "body", json_copy(body));
                    json_t *mr = gca2_call(ctx, "_create_message", ma);
                    json_free(exc);
                    if (gca2_is_raise(mr)) { ret = mr; goto loop_done; }
                    last_result = gca2_sr_norm(mr);
                    json_free(mr);
                    continue;
                }
                json_t *lr = gca2_log(ctx, "info", "[GoogleChat] send target 404; skipping", json_array());
                if (gca2_is_raise(lr)) { json_free(exc); ret = lr; goto loop_done; }
                json_free(lr);
                ret = gca2_sr_new(false, NULL, json_string("target not found"), false);
                json_free(exc);
                goto loop_done;
            }
            if (!is_bool && sv == 429) {
                json_t *na = json_object();
                json_set(na, "chat_id", chat_id ? json_copy(chat_id) : json_null());
                json_t *hits = gca2_call(ctx, "_note_rate_limit", na);
                if (gca2_is_raise(hits)) { json_free(exc); ret = hits; goto loop_done; }
                double hv = (hits && hits->type == JSON_NUMBER) ? hits->num_val : 0;
                json_free(hits);
                if (hv >= GCA2_RATE_LIMIT_WARN_THRESHOLD) {
                    json_t *la = json_array();
                    json_append(la, json_number(hv));
                    json_t *lr = gca2_log(ctx, "warning", "[GoogleChat] Rate limit hit %d times on chat; throttling", la);
                    if (gca2_is_raise(lr)) { json_free(exc); ret = lr; goto loop_done; }
                    json_free(lr);
                }
            }
            ret = exc;  /* re-raise */
            goto loop_done;
        }
    }
    if (!last_result) {
        ret = gca2_sr_new(false, NULL, json_string("empty message"), false);
    } else {
        if (patched_typing) {
            json_t *tm2 = gca2_state_obj(ctx, "_typing_messages");
            json_set_hdr(tm2, cid, GCA2_TYPING_CONSUMED_SENTINEL);
        }
        ret = last_result;
        last_result = NULL;
    }
loop_done:
    json_free(last_result);
    json_free(typing_msg_name);
    json_free(chunks);
    json_free(thread_id);
finally:
    {
        json_t *rsa = json_object();
        json_set(rsa, "chat_id", chat_id ? json_copy(chat_id) : json_null());
        json_t *rsr = gca2_call(ctx, "resume_typing_for_chat", rsa);
        if (gca2_is_raise(rsr)) { json_free(ret); ret = rsr; }
        else json_free(rsr);
    }
    return ret;
}

/* PoP: gca2_send_card @ plugins/platforms/google_chat/adapter.py:send_card */
static json_t *gca2_send_card(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *chat_id = json_obj_get(args, "chat_id");
    const json_t *card = json_obj_get(args, "card");
    const json_t *metadata = json_obj_get(args, "metadata");
    json_t *body = json_object();
    json_t *cards = json_array();
    json_append(cards, card ? json_copy(card) : json_null());
    json_set(body, "cardsV2", cards);
    json_t *ra = json_object();
    json_set(ra, "reply_to", json_null());
    json_set(ra, "metadata", metadata ? json_copy(metadata) : json_null());
    json_set(ra, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_t *thread_id = gca2_call(ctx, "_resolve_thread_id", ra);
    if (gca2_is_raise(thread_id)) { json_free(body); return thread_id; }
    if (gca2_truthy(thread_id)) {
        json_t *th = json_object();
        json_set(th, "name", json_copy(thread_id));
        json_set(body, "thread", th);
    }
    json_free(thread_id);
    json_t *ma = json_object();
    json_set(ma, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_set(ma, "body", json_copy(body));
    json_t *mr = gca2_call(ctx, "_create_message", ma);
    if (gca2_is_raise(mr)) {
        const char *kind = json_get_str(mr, "__raise__", "");
        if (gca2_isa(kind, "HttpError")) {
            json_t *txt = gca2_exc_text(ctx, mr, json_array());
            if (gca2_is_raise(txt)) { json_free(mr); json_free(body); return txt; }
            json_t *sa = json_object();
            json_set(sa, "exc", gca2_exc_double(mr));
            json_t *status = gca2_call(ctx, "_http_status", sa);
            if (gca2_is_raise(status)) { json_free(txt); json_free(mr); json_free(body); return status; }
            double sv = (status && status->type == JSON_NUMBER) ? status->num_val : -1;
            bool retryable = (sv == 429 || sv == 500 || sv == 502 || sv == 503 || sv == 504);
            json_free(status);
            json_t *out = gca2_sr_new(false, NULL, txt, retryable);
            json_free(mr); json_free(body);
            return out;
        }
        if (gca2_raise_passes(mr)) { json_free(body); return mr; }
        /* except Exception */
        json_t *pa = json_object();
        json_set(pa, "exc", gca2_exc_double(mr));
        json_set(pa, "targets", json_array());
        json_t *px = gca2_call(ctx, "_proxy_error", pa);
        if (gca2_is_raise(px)) { json_free(mr); json_free(body); return px; }
        json_free(px);
        json_t *lr = gca2_log(ctx, "debug", "[GoogleChat] send_card failed", json_array());
        if (gca2_is_raise(lr)) { json_free(mr); json_free(body); return lr; }
        json_free(lr);
        json_t *txt = gca2_exc_text(ctx, mr, json_array());
        if (gca2_is_raise(txt)) { json_free(mr); json_free(body); return txt; }
        json_t *ia = json_object();
        json_set(ia, "exc", gca2_exc_double(mr));
        json_t *retry = gca2_call(ctx, "_is_retryable_error", ia);
        if (gca2_is_raise(retry)) { json_free(txt); json_free(mr); json_free(body); return retry; }
        bool retryable = gca2_truthy(retry);
        json_free(retry);
        json_t *out = gca2_sr_new(false, NULL, txt, retryable);
        json_free(mr); json_free(body);
        return out;
    }
    json_t *result = gca2_sr_norm(mr);
    json_free(mr);
    const json_t *raw = json_obj_get(result, "raw_response");
    if (!gca2_truthy(raw)) {
        json_t *rr = json_object();
        json_set(rr, "cardsV2", json_copy(json_obj_get(body, "cardsV2")));
        json_set(result, "raw_response", rr);
    }
    json_free(body);
    return result;
}

/* PoP: gca2_edit_message @ plugins/platforms/google_chat/adapter.py:edit_message */
static json_t *gca2_edit_message(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *chat_id = json_obj_get(args, "chat_id");
    const json_t *message_id = json_obj_get(args, "message_id");
    const json_t *content = json_obj_get(args, "content");
    if (!gca2_truthy(message_id))
        return gca2_sr_new(false, NULL, json_string("missing message_id"), false);
    const char *cs = gca2_str(content);
    char *text = gca2_strdup(cs ? cs : "");
    if (gca2_utf8_len(text) > GCA2_MAX_TEXT_LENGTH) {
        char *head = gca2_slice_head(text, GCA2_MAX_TEXT_LENGTH - 1);
        free(text);
        size_t n = strlen(head) + 4;
        text = malloc(n);
        if (text) snprintf(text, n, "%s\xe2\x80\xa6", head);
        free(head);
    }
    json_t *body = json_object();
    json_set_hdr(body, "text", text);
    free(text);
    json_t *ma = json_object();
    json_set(ma, "message_name", json_copy(message_id));
    json_set(ma, "body", body);
    json_t *mr = gca2_call(ctx, "_patch_message", ma);
    if (gca2_is_raise(mr)) {
        const char *kind = json_get_str(mr, "__raise__", "");
        if (gca2_isa(kind, "HttpError")) {
            json_t *sa = json_object();
            json_set(sa, "exc", gca2_exc_double(mr));
            json_t *status = gca2_call(ctx, "_http_status", sa);
            if (gca2_is_raise(status)) { json_free(mr); return status; }
            double sv = (status && status->type == JSON_NUMBER) ? status->num_val : -1;
            json_free(status);
            if (sv == 429) {
                json_t *na = json_object();
                json_set(na, "chat_id", chat_id ? json_copy(chat_id) : json_null());
                json_t *nr = gca2_call(ctx, "_note_rate_limit", na);
                if (gca2_is_raise(nr)) { json_free(mr); return nr; }
                json_free(nr);
            }
            json_t *txt = gca2_exc_text(ctx, mr, json_array());
            json_free(mr);
            if (gca2_is_raise(txt)) return txt;
            return gca2_sr_new(false, NULL, txt, false);
        }
        if (gca2_raise_passes(mr)) return mr;
        json_t *pa = json_object();
        json_set(pa, "exc", gca2_exc_double(mr));
        json_set(pa, "targets", json_array());
        json_t *px = gca2_call(ctx, "_proxy_error", pa);
        if (gca2_is_raise(px)) { json_free(mr); return px; }
        json_free(px);
        json_t *lr = gca2_log(ctx, "debug", "[GoogleChat] edit_message failed", json_array());
        if (gca2_is_raise(lr)) { json_free(mr); return lr; }
        json_free(lr);
        json_t *txt = gca2_exc_text(ctx, mr, json_array());
        json_free(mr);
        if (gca2_is_raise(txt)) return txt;
        return gca2_sr_new(false, NULL, txt, false);
    }
    json_t *result = gca2_sr_norm(mr);
    json_free(mr);
    return result;
}

/* PoP: gca2_delete_message @ plugins/platforms/google_chat/adapter.py:delete_message */
static json_t *gca2_delete_message(gca2_ctx_t *ctx, const json_t *args) {
    const json_t *message_id = json_obj_get(args, "message_id");
    if (!gca2_truthy(message_id)) return json_bool(false);
    json_t *ta = json_object();
    json_set_hdr(ta, "fn", "messages.delete");
    json_t *aa = json_object();
    json_set(aa, "name", json_copy(message_id));
    json_set(ta, "args", aa);
    json_t *r = gca2_call(ctx, "asyncio.to_thread", ta);
    if (gca2_is_raise(r)) {
        const char *kind = json_get_str(r, "__raise__", "");
        if (gca2_isa(kind, "HttpError")) {
            json_t *sa = json_object();
            json_set(sa, "exc", gca2_exc_double(r));
            json_t *status = gca2_call(ctx, "_http_status", sa);
            if (gca2_is_raise(status)) { json_free(r); return status; }
            double sv = (status && status->type == JSON_NUMBER) ? status->num_val : -1;
            json_free(status);
            if (sv != 403 && sv != 404) {
                json_t *txt = gca2_exc_text(ctx, r, json_array());
                if (gca2_is_raise(txt)) { json_free(r); return txt; }
                json_t *la = json_array();
                json_append(la, txt);
                json_t *lr = gca2_log(ctx, "debug", "[GoogleChat] delete_message failed: %s", la);
                json_free(r);
                if (gca2_is_raise(lr)) return lr;
                json_free(lr);
            } else {
                json_free(r);
            }
            return json_bool(false);
        }
        if (gca2_raise_passes(r)) return r;
        json_t *pa = json_object();
        json_set(pa, "exc", gca2_exc_double(r));
        json_set(pa, "targets", json_array());
        json_t *px = gca2_call(ctx, "_proxy_error", pa);
        if (gca2_is_raise(px)) { json_free(r); return px; }
        json_free(px);
        json_t *lr = gca2_log(ctx, "debug", "[GoogleChat] delete_message failed", json_array());
        json_free(r);
        if (gca2_is_raise(lr)) return lr;
        json_free(lr);
        return json_bool(false);
    }
    json_free(r);
    return json_bool(true);
}