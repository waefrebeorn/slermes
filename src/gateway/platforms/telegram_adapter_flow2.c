/*
 * telegram_adapter_flow2.c — Telegram adapter FLOW remainder of
 * plugins/platforms/telegram/adapter.py (hermes-agent): the polling
 * progress/lifecycle watchdog chain (fence, generation start, progress
 * recording, request-result observation, update counting, verifier staleness,
 * recovery-in-flight, ingress dispatch stall), the bot-identity / mention
 * routing chain (current handle, rename noting, identity observation, TTL
 * freshness, reply-to-bot, effective thread id), the config-extra gate chain
 * (bool gate, str-set allowlist, ignored threads, observe-allowed intersection,
 * free-response topic), the media size/routing chain (int coercion, too-large
 * note, size gate, missing-path error, send kwargs, raster magic sniff) plus
 * cold-boot drop-pending and the polling-conflict classifier.
 *
 * Decision sequences only: every live I/O / injected callable / unported
 * collaborator enters through ONE universal seam: ta2_call(ctx, name, args).
 * The harness wires it to a recording stub fed the case's scripted results
 * (in.io); the oracle installs recording stubs emitting the identical event
 * shape {"name", "args", "ret"} from the SAME scripts. Ported siblings compose
 * INLINE (real C calls); claimed flow1 siblings and unported collaborators are
 * seamed as "self.<name>".
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and ta2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow2.py):
 * - NULL json_t* == Python None / absent; a JSON_NULL node is a real null
 *   VALUE. Exception specs: {"__raise__": cls, "message": str(exc)}.
 * - Adapter state (in.state) is MUTATED in place and echoed as out.state.
 *   Object carriers materialize as attribute doubles on the oracle side:
 *     "config"        null | {"extra": <dict>}        (extra stays a dict)
 *     "_bot"          null | {"id":..,"username":..}  (SimpleNamespace)
 *     "platform"      {"value": "telegram"}           (name property source)
 *     "_polling_progress_event"        null | {"id":..,"set":bool}
 *     "_polling_progress_verifier_task"/"_polling_error_task"  null | {"id":..}
 *   Event/Task doubles are OBJECTS: present-non-null is truthy regardless of
 *   field count; identity is compared by the "id" field (the oracle keeps a
 *   per-id object registry so `is` agrees). Event.set() flips the state node's
 *   "set" flag on BOTH sides; Event.is_set()/Task.done()/Task.cancel() are
 *   seams ("event_is_set"/"task_done"/"task_cancel" {id}). asyncio.Event()
 *   construction is the "event_create" {} seam returning the new id string.
 *   The base properties are modeled from state: name == platform.value.title()
 *   (ASCII title()), has_fatal_error == (_fatal_error_message is not None),
 *   _teardown_started == getattr(_polling_teardown_started, False).
 * - Direct attribute reads (self._bot, self._polling_generation, ...) raise
 *   AttributeError "'TelegramAdapter' object has no attribute '<k>'" when the
 *   state key is ABSENT; present-null is a real None. getattr(self,k,default)
 *   reads json_obj_get with the default. Nested double reads use the
 *   'types.SimpleNamespace' carrier name; dict .get uses the JSON type name.
 * - in.errors: id -> {"class", "message", "base"} exception registry (as in
 *   flow1); _looks_like_polling_conflict reads str(error)=message and the
 *   class name. Registry classes must not be KeyError (str() reprs the msg).
 * - Seam scripts: a plain value repeats; a LIST is consumed per call; a
 *   {"__script__":true,"ret":v,"raise":{type,message}} wrapper forces a result
 *   or raise. Missing/exhausted script => fail-closed RuntimeError recorded in
 *   the trace before propagation. Set-returning seams are scripted as SORTED
 *   lists; the port returns sorted deduped lists (Python set projection).
 * - logger calls are seams "logger.info"/"logger.warning" {msg, args} carrying
 *   the FORMAT string and the raw projected args (never the formatted text);
 *   time.monotonic is the "monotonic" {} seam.
 * - int() ports carry CPython 3.14.9's exact ValueError/TypeError messages;
 *   int(str) strips White_Space MINUS \x1c-\x1f, accepts a sign and underscores
 *   BETWEEN digits (ASCII digit corpus); int(float) truncates toward zero.
 *   str() projection covers scalars; integral numbers print int-like (fixture
 *   corpus keeps integral floats out of str()-computed slots). lower()/title()
 *   fold ASCII only; strip() uses the full Unicode White_Space set.
 * - Fail-closed: with no io channel every seam call fails closed. except
 *   sites swallow ONLY their named families (OSError / TypeError+ValueError /
 *   Exception) via the FULL builtin hierarchy; BaseException-only kinds
 *   (SystemExit/KeyboardInterrupt/GeneratorExit/BaseExceptionGroup) pass
 *   through every except-Exception site.
 * - Fixture domains: result unpack carries lists/null/numbers/bools (str/dict
 *   results excluded); event_create scripts fresh id strings; max_bytes and
 *   counter state are ints; float scripts are exactly-representable short
 *   decimals (libjson %.17g); task/event nodes carry an "id".
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

#define TA2_GENERAL_TOPIC_THREAD_ID "1"
#define TA2_BOT_IDENTITY_TTL_SECONDS 300.0
#define TA2_INGRESS_DISPATCH_STALL_HEARTBEATS 4
#define TA2_MEDIA_SEND_READ_TIMEOUT 60.0
#define TA2_MAX_DOC_BYTES_DEFAULT 20971520.0
#define TA2_MB 1048576.0

/* ── universal seam context ── */
typedef struct ta2_ctx {
    json_t *state;        /* owned; MUTATED; echoed as out.state */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed exception-graph registry */
} ta2_ctx_t;

/* ── basic helpers ── */
static bool ta2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *ta2_raise_kind(const json_t *v) {
    return ta2_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool ta2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

/* Object-carrier presence (SimpleNamespace doubles are always truthy). */
static bool ta2_present(const json_t *v) {
    return v && v->type != JSON_NULL;
}

static bool ta2_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static const char *ta2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *ta2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static const char *ta2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == floor(d) && fabs(d) < 1e16) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* Python scalar == (bool<->number cross-type; containers outside corpus). */
static bool ta2_eq(const json_t *a, const json_t *b) {
    bool an = ta2_is_null(a), bn = ta2_is_null(b);
    if (an || bn) return an && bn;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    double da, db;
    if (a->type == JSON_NUMBER) da = a->num_val;
    else if (a->type == JSON_BOOL) da = a->bool_val ? 1.0 : 0.0;
    else return false;
    if (b->type == JSON_NUMBER) db = b->num_val;
    else if (b->type == JSON_BOOL) db = b->bool_val ? 1.0 : 0.0;
    else return false;
    return da == db;
}

static const json_t *ta2_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}

/* ── attribute access ── */
/* getattr(obj, key, default): never raises; non-object carriers read absent. */
static const json_t *ta2getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

/* Direct attribute read: absent / non-carrier raises AttributeError. */
static const json_t *ta2attr(const json_t *obj, const char *key, const char *tname, json_t **err) {
    char msg[200];
    if (!obj || obj->type == JSON_NULL) {
        snprintf(msg, sizeof msg, "'NoneType' object has no attribute '%s'", key);
        *err = ta2_err("AttributeError", msg);
        return NULL;
    }
    if (obj->type != JSON_OBJECT) {
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", ta2_type_name(obj), key);
        *err = ta2_err("AttributeError", msg);
        return NULL;
    }
    const json_t *v = json_obj_get(obj, key);
    if (!v) {
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", tname, key);
        *err = ta2_err("AttributeError", msg);
        return NULL;
    }
    return v;
}

/* dict.get(key): non-dict carrier raises AttributeError '... has no get'. */
static const json_t *ta2dictget(const json_t *obj, const char *key, json_t **err) {
    if (obj && obj->type == JSON_OBJECT) return json_obj_get(obj, key);
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ta2_type_name(obj));
    *err = ta2_err("AttributeError", msg);
    return NULL;
}

/* ── string helpers ── */
static char *ta2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static unsigned long ta2_next_cp(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned char c = s[0];
    if (c == 0) return 0;
    if (c < 0x80) { *p += 1; return c; }
    unsigned long cp; int n;
    if ((c & 0xe0) == 0xc0) { cp = c & 0x1f; n = 2; }
    else if ((c & 0xf0) == 0xe0) { cp = c & 0x0f; n = 3; }
    else if ((c & 0xf8) == 0xf0) { cp = c & 0x07; n = 4; }
    else { *p += 1; return 0xfffd; }
    for (int i = 1; i < n; i++) {
        if ((s[i] & 0xc0) != 0x80) { *p += 1; return 0xfffd; }
        cp = (cp << 6) | (s[i] & 0x3f);
    }
    *p += n;
    return cp;
}

/* Full Unicode White_Space set (str.strip). */
static bool ta2_is_space_cp(unsigned long cp) {
    if (cp >= 0x09 && cp <= 0x0d) return true;
    if (cp >= 0x1c && cp <= 0x1f) return true;
    if (cp == 0x20 || cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}

/* int() trimming: White_Space MINUS \x1c-\x1f (probed on CPython 3.14.9). */
static bool ta2_is_int_space_cp(unsigned long cp) {
    if (cp >= 0x1c && cp <= 0x1f) return false;
    return ta2_is_space_cp(cp);
}

static char *ta2_strip_gen(const char *s, bool int_mode) {
    if (!s) s = "";
    const char *p = s;
    while (*p) {
        const char *q = p;
        unsigned long cp = ta2_next_cp(&q);
        if (!(int_mode ? ta2_is_int_space_cp(cp) : ta2_is_space_cp(cp))) break;
        p = q;
    }
    const char *end = p + strlen(p);
    while (end > p) {
        const char *q = end - 1;
        while (q > p && ((unsigned char)*q & 0xc0) == 0x80) q--;
        const char *r = q;
        unsigned long cp = ta2_next_cp(&r);
        if (r != end || !(int_mode ? ta2_is_int_space_cp(cp) : ta2_is_space_cp(cp))) break;
        end = q;
    }
    size_t n = (size_t)(end - p);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}

static char *ta2_strip(const char *s) { return ta2_strip_gen(s, false); }

/* str.lower(): ASCII fold (fixture corpus folds ASCII only). */
static char *ta2_lower(const char *s) {
    char *o = ta2_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* str.lstrip("@"): strip ALL leading '@'. */
static char *ta2_lstrip_at(const char *s) {
    if (!s) s = "";
    while (*s == '@') s++;
    return ta2_strdup(s);
}

/* str.title(): ASCII word-case (first cased char of each word upper). */
static char *ta2_title(const char *s) {
    char *o = ta2_strdup(s);
    if (!o) return NULL;
    bool prev_cased = false;
    for (char *p = o; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalpha(c)) {
            *p = (char)(prev_cased ? tolower(c) : toupper(c));
            prev_cased = true;
        } else {
            prev_cased = false;
        }
    }
    return o;
}

/* Python float repr: shortest round-trip decimal (plain-decimal corpus). */
static void ta2_float_repr(double d, char *buf, size_t cap) {
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) return;
    }
    snprintf(buf, cap, "%.17g", d);
}

/* str() projection for scalar JSON values. Caller frees. */
static char *ta2_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ta2_strdup("None");
    switch (v->type) {
        case JSON_STRING: return ta2_strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL: return ta2_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) snprintf(buf, sizeof buf, "%.0f", d);
            else ta2_float_repr(d, buf, sizeof buf);
            return ta2_strdup(buf);
        }
        default: {
            char *s = json_serialize(v);
            char *o = ta2_strdup(s ? s : "");
            free(s);
            return o;
        }
    }
}

/* Python repr() of an ASCII str for int() ValueError messages. */
static char *ta2_repr_str(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n * 4 + 3);
    if (!o) return NULL;
    size_t k = 0;
    o[k++] = '\'';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\'' || c == '\\') { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (c == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (c == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) { k += (size_t)snprintf(o + k, 5, "\\x%02x", c); }
        else o[k++] = (char)c;
    }
    o[k++] = '\'';
    o[k] = '\0';
    return o;
}

/* Python int() on a scalar JSON value: fresh number node or raise spec. */
static json_t *ta2_pyint(const json_t *v) {
    if (!v || v->type == JSON_NULL)
        return ta2_err("TypeError",
                       "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isinf(d)) return ta2_err("OverflowError", "cannot convert float infinity to integer");
        if (isnan(d)) return ta2_err("ValueError", "cannot convert float NaN to integer");
        return json_number(trunc(d));
    }
    if (v->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg,
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 ta2_type_name(v));
        return ta2_err("TypeError", msg);
    }
    const char *raw = v->str_val ? v->str_val : "";
    char *clean = ta2_strip_gen(raw, true);
    if (!clean) return ta2_err("RuntimeError", "out of memory");
    const char *p = clean;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    const char *digits = p;
    size_t nd = 0;
    bool ok = *p != '\0';
    for (const char *q = p; *q; q++) {
        if (*q == '_') {
            if (q == digits || q[1] < '0' || q[1] > '9' || q[-1] < '0' || q[-1] > '9') { ok = false; break; }
        } else if (*q >= '0' && *q <= '9') {
            nd++;
        } else { ok = false; break; }
    }
    if (!ok || nd == 0) {
        char *r = ta2_repr_str(raw);
        char msg[512];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", r ? r : "''");
        free(r);
        free(clean);
        return ta2_err("ValueError", msg);
    }
    long long acc = 0;
    for (const char *q = digits; *q; q++)
        if (*q != '_') acc = acc * 10 + (*q - '0');
    free(clean);
    return json_number((double)(neg ? -acc : acc));
}

/* ── FULL builtin exception hierarchy (CPython 3.14.9) ── */
typedef struct { const char *name; const char *parent; } ta2_exc_row_t;
static const ta2_exc_row_t TA2_EXC_HIER[] = {
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"BufferError", "Exception"},
    {"BytesWarning", "Warning"},
    {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"},
    {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"},
    {"GeneratorExit", "BaseException"},
    {"ImportError", "Exception"},
    {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"},
    {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"},
    {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"},
    {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"},
    {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"},
    {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"},
    {"SystemError", "Exception"},
    {"SystemExit", "BaseException"},
    {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"},
    {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"_IncompleteInputError", "SyntaxError"},
};

static const char *ta2_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA2_EXC_HIER) / sizeof(TA2_EXC_HIER[0]); i++)
        if (strcmp(name, TA2_EXC_HIER[i].name) == 0) return TA2_EXC_HIER[i].parent;
    return NULL;
}

static bool ta2_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta2_builtin_parent(cur);
    }
    return false;
}

/* BaseException-only kinds pass through every except-Exception site. Unknown
 * (synthetic) kinds are treated as Exception subclasses (fixture corpus uses
 * builtin kinds for scripted raises). */
static bool ta2_is_base_only(const char *kind) {
    if (!kind) return false;
    return ta2_chain_has(kind, "BaseException") && !ta2_chain_has(kind, "Exception");
}

static bool ta2_is_oserror(const char *kind) {
    return kind && ta2_chain_has(kind, "OSError");
}

static bool ta2_is_type_or_value(const char *kind) {
    return kind && (ta2_chain_has(kind, "TypeError") || ta2_chain_has(kind, "ValueError"));
}

/* ── exception-graph registry ── */
static const json_t *ta2_err_obj(const ta2_ctx_t *ctx, const char *eid) {
    if (!eid || !ctx->errors || ctx->errors->type != JSON_OBJECT) return NULL;
    return json_obj_get(ctx->errors, eid);
}

static const char *ta2_err_class(const ta2_ctx_t *ctx, const char *eid) {
    const json_t *e = ta2_err_obj(ctx, eid);
    return e ? json_get_str(e, "class", "Exception") : "Exception";
}

static const char *ta2_err_text(const ta2_ctx_t *ctx, const char *eid) {
    const json_t *e = ta2_err_obj(ctx, eid);
    return e ? json_get_str(e, "message", "") : "";
}

/* ── the one universal seam ──
 * args is ADOPTED into the trace event; the return is a fresh owned node. */
static json_t *ta2_call(ta2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ta2_err(json_get_str(raise, "type", "RuntimeError"),
                              json_get_str(raise, "message", ""));
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

/* ── seam wrappers ── */
static json_t *ta2_log(ta2_ctx_t *ctx, const char *level, const char *msg, json_t *args_arr) {
    json_t *a = json_object();
    json_set_hdr(a, "msg", msg);
    json_set(a, "args", args_arr ? args_arr : json_array());
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    return ta2_call(ctx, name, a);
}

/* self.name property == platform.value.title() (ASCII corpus). */
static char *ta2_name(ta2_ctx_t *ctx, json_t **err) {
    const json_t *plat = ta2attr(ctx->state, "platform", "TelegramAdapter", err);
    if (*err) return NULL;
    const json_t *val = ta2attr(plat, "value", "types.SimpleNamespace", err);
    if (*err) return NULL;
    if (val->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'title'", ta2_type_name(val));
        *err = ta2_err("AttributeError", msg);
        return NULL;
    }
    return ta2_title(val->str_val ? val->str_val : "");
}

static json_t *ta2_monotonic(ta2_ctx_t *ctx) {
    return ta2_call(ctx, "monotonic", json_object());
}

static json_t *ta2_obj_method_err(const json_t *obj, const char *method) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", ta2_type_name(obj), method);
    return ta2_err("AttributeError", msg);
}

static json_t *ta2_event_is_set(ta2_ctx_t *ctx, const json_t *ev, json_t **err) {
    if (!ev || ev->type != JSON_OBJECT) { *err = ta2_obj_method_err(ev, "is_set"); return NULL; }
    json_t *a = json_object();
    const json_t *id = json_obj_get(ev, "id");
    json_set(a, "id", id ? json_copy(id) : json_null());
    return ta2_call(ctx, "event_is_set", a);
}

static json_t *ta2_event_set(ta2_ctx_t *ctx, const json_t *ev, json_t **err) {
    if (!ev || ev->type != JSON_OBJECT) { *err = ta2_obj_method_err(ev, "set"); return NULL; }
    json_t *a = json_object();
    const json_t *id = json_obj_get(ev, "id");
    json_set(a, "id", id ? json_copy(id) : json_null());
    return ta2_call(ctx, "event_set", a);
}

/* asyncio.Event() -> {"id": <scripted id>, "set": false}. */
static json_t *ta2_event_create(ta2_ctx_t *ctx) {
    json_t *r = ta2_call(ctx, "event_create", json_object());
    if (ta2_is_raise(r)) return r;
    json_t *ev = json_object();
    json_set(ev, "id", r ? r : json_null());
    json_set(ev, "set", json_bool(false));
    return ev;
}

static json_t *ta2_task_done(ta2_ctx_t *ctx, const json_t *task, json_t **err) {
    if (!task || task->type != JSON_OBJECT) { *err = ta2_obj_method_err(task, "done"); return NULL; }
    json_t *a = json_object();
    const json_t *id = json_obj_get(task, "id");
    json_set(a, "id", id ? json_copy(id) : json_null());
    return ta2_call(ctx, "task_done", a);
}

static json_t *ta2_task_cancel(ta2_ctx_t *ctx, const json_t *task, json_t **err) {
    if (!task || task->type != JSON_OBJECT) { *err = ta2_obj_method_err(task, "cancel"); return NULL; }
    json_t *a = json_object();
    const json_t *id = json_obj_get(task, "id");
    json_set(a, "id", id ? json_copy(id) : json_null());
    return ta2_call(ctx, "task_cancel", a);
}

/* sorted deduped string array from a char* list (set projection). */
static int ta2_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *ta2_str_set_array(char **items, size_t n) {
    /* dedup */
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        bool dup = false;
        for (size_t j = 0; j < k; j++)
            if (strcmp(items[i], items[j]) == 0) { dup = true; break; }
        if (dup) { free(items[i]); continue; }
        items[k++] = items[i];
    }
    qsort(items, k, sizeof(char *), ta2_cmp_str);
    json_t *arr = json_array();
    for (size_t i = 0; i < k; i++) {
        json_append(arr, json_string(items[i]));
        free(items[i]);
    }
    free(items);
    return arr;
}

static int ta2_cmp_num(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

static json_t *ta2_num_set_array(double *items, size_t n) {
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        bool dup = false;
        for (size_t j = 0; j < k; j++)
            if (items[i] == items[j]) { dup = true; break; }
        if (!dup) items[k++] = items[i];
    }
    qsort(items, k, sizeof(double), ta2_cmp_num);
    json_t *arr = json_array();
    for (size_t i = 0; i < k; i++) json_append(arr, json_number(items[i]));
    free(items);
    return arr;
}

/* split a string on literal ',' (Python str.split(",")). */
static char **ta2_split_comma(const char *s, size_t *n) {
    if (!s) s = "";
    size_t cap = 8, k = 0;
    char **out = malloc(cap * sizeof(char *));
    const char *p = s;
    while (1) {
        const char *c = strchr(p, ',');
        size_t len = c ? (size_t)(c - p) : strlen(p);
        if (k == cap) { cap *= 2; out = realloc(out, cap * sizeof(char *)); }
        char *piece = malloc(len + 1);
        memcpy(piece, p, len);
        piece[len] = '\0';
        out[k++] = piece;
        if (!c) break;
        p = c + 1;
    }
    *n = k;
    return out;
}

/* ═══════════════════════ ported flows ═══════════════════════ */

/* PoP: ta2_fence_polling @ plugins/platforms/telegram/adapter.py:_fence_polling */
static json_t *ta2_fence_polling(ta2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_set(ctx->state, "_polling_progress_accepting", json_bool(false));
    json_set(ctx->state, "_send_path_degraded", json_bool(true));
    return json_null();
}

/* PoP: ta2_begin_polling_generation @ plugins/platforms/telegram/adapter.py:_begin_polling_generation */
static json_t *ta2_begin_polling_generation(ta2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *err = NULL;
    const json_t *td = json_obj_get(ctx->state, "_polling_teardown_started");
    if (ta2_truthy(td)) {
        json_t *f = ta2_fence_polling(ctx, json_object());
        if (ta2_is_raise(f)) return f;
        json_free(f);
        const json_t *prog = json_obj_get(ctx->state, "_polling_progress_event");
        if (ta2_is_null(prog)) {
            json_t *ev = ta2_event_create(ctx);
            if (ta2_is_raise(ev)) return ev;
            json_set(ctx->state, "_polling_progress_event", ev);
            prog = json_obj_get(ctx->state, "_polling_progress_event");
        }
        const json_t *gen = json_obj_get(ctx->state, "_polling_generation");
        json_t *ret = json_array();
        json_append(ret, gen ? json_copy(gen) : json_number(0));
        json_append(ret, json_copy(prog));
        return ret;
    }
    const json_t *ver = json_obj_get(ctx->state, "_polling_progress_verifier_task");
    if (ta2_present(ver)) {
        json_t *done = ta2_task_done(ctx, ver, &err);
        if (err) return err;
        if (ta2_is_raise(done)) return done;
        bool d = ta2_truthy(done);
        json_free(done);
        if (!d) {
            json_t *c = ta2_task_cancel(ctx, ver, &err);
            if (err) return err;
            if (ta2_is_raise(c)) return c;
            json_free(c);
        }
    }
    json_set(ctx->state, "_polling_progress_verifier_task", json_null());
    const json_t *g0 = json_obj_get(ctx->state, "_polling_generation");
    double gen;
    if (!g0) gen = 0;
    else if (g0->type == JSON_NUMBER) gen = g0->num_val;
    else if (g0->type == JSON_BOOL) gen = g0->bool_val ? 1 : 0;
    else {
        char msg[160];
        snprintf(msg, sizeof msg, "unsupported operand type(s) for +: '%s' and 'int'", ta2_type_name(g0));
        return ta2_err("TypeError", msg);
    }
    gen += 1;
    json_set(ctx->state, "_polling_generation", json_number(gen));
    json_t *ev = ta2_event_create(ctx);
    if (ta2_is_raise(ev)) return ev;
    json_set(ctx->state, "_polling_progress_event", ev);
    json_set(ctx->state, "_polling_progress_accepting", json_bool(true));
    json_set(ctx->state, "_send_path_degraded", json_bool(true));
    json_t *b = ta2_call(ctx, "self._bank_conflict_quiet_window", json_object());
    if (ta2_is_raise(b)) return b;
    json_free(b);
    json_t *m = ta2_monotonic(ctx);
    if (ta2_is_raise(m)) return m;
    json_set(ctx->state, "_polling_generation_started_monotonic", m);
    json_set(ctx->state, "_polling_last_progress_monotonic", json_null());
    json_set(ctx->state, "_updates_received_total", json_number(0));
    json_set(ctx->state, "_updates_dispatched_total", json_number(0));
    json_set(ctx->state, "_ingress_dispatched_seen", json_number(0));
    json_set(ctx->state, "_ingress_stalled_heartbeats", json_number(0));
    json_t *ret = json_array();
    json_append(ret, json_number(gen));
    json_append(ret, json_copy(json_obj_get(ctx->state, "_polling_progress_event")));
    return ret;
}

/* PoP: ta2_record_polling_progress @ plugins/platforms/telegram/adapter.py:_record_polling_progress */
static json_t *ta2_record_polling_progress(ta2_ctx_t *ctx, const json_t *args) {
    json_t *err = NULL;
    const json_t *td = json_obj_get(ctx->state, "_polling_teardown_started");
    if (ta2_truthy(td)) return json_bool(false);
    const json_t *accepting = ta2attr(ctx->state, "_polling_progress_accepting", "TelegramAdapter", &err);
    if (err) return err;
    if (!ta2_truthy(accepting)) return json_bool(false);
    const json_t *gen_state = ta2attr(ctx->state, "_polling_generation", "TelegramAdapter", &err);
    if (err) return err;
    const json_t *generation = ta2_arg(args, "generation");
    if (!ta2_eq(generation, gen_state)) return json_bool(false);
    const json_t *ev = ta2attr(ctx->state, "_polling_progress_event", "TelegramAdapter", &err);
    if (err) return err;
    json_t *is_set = ta2_event_is_set(ctx, ev, &err);
    if (err) return err;
    if (ta2_is_raise(is_set)) return is_set;
    bool already = ta2_truthy(is_set);
    json_free(is_set);
    if (!already) {
        const json_t *nec = ta2attr(ctx->state, "_polling_network_error_count", "TelegramAdapter", &err);
        if (err) return err;
        const char *st = ta2_truthy(nec) ? "recovered" : "confirmed healthy";
        char *name = ta2_name(ctx, &err);
        if (err) return err;
        json_t *la = json_array();
        json_append(la, json_string(name));
        json_append(la, json_string(st));
        json_append(la, generation ? json_copy(generation) : json_null());
        free(name);
        json_t *lg = ta2_log(ctx, "info",
                             "[%s] Telegram polling %s: getUpdates progressing (generation %d)", la);
        if (ta2_is_raise(lg)) return lg;
        json_free(lg);
    }
    json_t *setr = ta2_event_set(ctx, ev, &err);
    if (err) return err;
    if (ta2_is_raise(setr)) return setr;
    json_free(setr);
    json_set((json_t *)ev, "set", json_bool(true));  /* ev IS the state node */
    json_t *m = ta2_monotonic(ctx);
    if (ta2_is_raise(m)) return m;
    json_set(ctx->state, "_polling_last_progress_monotonic", m);
    json_set(ctx->state, "_polling_network_error_count", json_number(0));
    json_t *na = json_object();
    json_set(na, "now", json_copy(m));
    json_t *n = ta2_call(ctx, "self._note_conflict_quiet_progress", na);
    if (ta2_is_raise(n)) return n;
    json_free(n);
    json_t *ma = json_object();
    json_set_hdr(ma, "adapter", "self");
    json_t *ms = ta2_call(ctx, "mark_send_path_recovered", ma);
    if (ta2_is_raise(ms)) return ms;
    json_free(ms);
    return json_bool(true);
}

/* PoP: ta2_record_updates_received @ plugins/platforms/telegram/adapter.py:_record_updates_received */
static json_t *ta2_record_updates_received(ta2_ctx_t *ctx, const json_t *args) {
    const json_t *result = ta2_arg(args, "result");
    if (result && result->type == JSON_ARRAY && json_len(result) > 0) {
        json_t *err = NULL;
        const json_t *cur = ta2attr(ctx->state, "_updates_received_total", "TelegramAdapter", &err);
        if (err) return err;
        double base;
        if (cur->type == JSON_NUMBER) base = cur->num_val;
        else if (cur->type == JSON_BOOL) base = cur->bool_val ? 1 : 0;
        else {
            char msg[160];
            snprintf(msg, sizeof msg, "unsupported operand type(s) for +=: '%s' and 'int'", ta2_type_name(cur));
            return ta2_err("TypeError", msg);
        }
        json_set(ctx->state, "_updates_received_total", json_number(base + (double)json_len(result)));
    }
    return json_null();
}

/* PoP: ta2_observe_polling_request_result @ plugins/platforms/telegram/adapter.py:_observe_polling_request_result */
static json_t *ta2_observe_polling_request_result(ta2_ctx_t *ctx, const json_t *args) {
    const json_t *result = ta2_arg(args, "result");
    /* status_code, payload = result  (list/null/number/bool corpus) */
    if (!result || result->type != JSON_ARRAY) {
        char msg[160];
        snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", ta2_type_name(result));
        return ta2_err("TypeError", msg);
    }
    size_t n = json_len(result);
    if (n < 2) {
        char msg[128];
        snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", n);
        return ta2_err("ValueError", msg);
    }
    if (n > 2) {
        char msg[128];
        snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)", n);
        return ta2_err("ValueError", msg);
    }
    const json_t *status = json_get(result, 0);
    const json_t *payload = json_get(result, 1);
    const json_t *generation = ta2_arg(args, "generation");
    if (ta2_is_null(generation)) return json_null();
    if (!status || (status->type != JSON_NUMBER && status->type != JSON_BOOL)) {
        char msg[160];
        snprintf(msg, sizeof msg, "'<=' not supported between instances of 'int' and '%s'", ta2_type_name(status));
        return ta2_err("TypeError", msg);
    }
    double s = status->type == JSON_BOOL ? (status->bool_val ? 1.0 : 0.0) : status->num_val;
    if (!(200.0 <= s && s < 300.0)) return json_null();
    const json_t *request = ta2_arg(args, "request");
    json_t *env;
    if (!request || request->type != JSON_OBJECT) {
        /* the attribute access is INSIDE the live try -> except Exception swallows it */
        env = ta2_obj_method_err(request, "parse_json_payload");
    } else {
        json_t *pa = json_object();
        json_set(pa, "payload", payload ? json_copy(payload) : json_null());
        env = ta2_call(ctx, "request_parse_json_payload", pa);
    }
    if (ta2_is_raise(env)) {
        if (ta2_is_base_only(ta2_raise_kind(env))) return env;
        json_free(env);
        return json_null();  /* except Exception: return */
    }
    if (env && env->type == JSON_OBJECT) {
        const json_t *ok = json_obj_get(env, "ok");
        if (ok && ok->type == JSON_BOOL && ok->bool_val && json_has(env, "result")) {
            json_t *ga = json_object();
            json_set(ga, "generation", generation ? json_copy(generation) : json_null());
            json_t *rec = ta2_record_polling_progress(ctx, ga);
            if (ta2_is_raise(rec)) { json_free(env); return rec; }
            bool accepted = ta2_truthy(rec);
            json_free(rec);
            if (accepted) {
                json_t *ra = json_object();
                const json_t *res = json_obj_get(env, "result");
                json_set(ra, "result", res ? json_copy(res) : json_null());
                json_t *ru = ta2_record_updates_received(ctx, ra);
                if (ta2_is_raise(ru)) { json_free(env); return ru; }
                json_free(ru);
            }
        }
    }
    json_free(env);
    return json_null();
}

/* PoP: ta2_verifier_stale @ plugins/platforms/telegram/adapter.py:_verifier_stale */
static json_t *ta2_verifier_stale(ta2_ctx_t *ctx, const json_t *args) {
    json_t *err = NULL;
    const json_t *td = json_obj_get(ctx->state, "_polling_teardown_started");
    if (ta2_truthy(td)) return json_bool(true);
    const json_t *progress = ta2_arg(args, "progress");
    json_t *is_set = ta2_event_is_set(ctx, progress, &err);
    if (err) return err;
    if (ta2_is_raise(is_set)) return is_set;
    bool set = ta2_truthy(is_set);
    json_free(is_set);
    if (set) return json_bool(true);
    const json_t *fm = ta2attr(ctx->state, "_fatal_error_message", "TelegramAdapter", &err);
    if (err) return err;
    if (ta2_present(fm)) return json_bool(true);
    const json_t *accepting = ta2attr(ctx->state, "_polling_progress_accepting", "TelegramAdapter", &err);
    if (err) return err;
    if (!ta2_truthy(accepting)) return json_bool(true);
    const json_t *gen_state = ta2attr(ctx->state, "_polling_generation", "TelegramAdapter", &err);
    if (err) return err;
    const json_t *generation = ta2_arg(args, "generation");
    if (!ta2_eq(generation, gen_state)) return json_bool(true);
    const json_t *ev_state = ta2attr(ctx->state, "_polling_progress_event", "TelegramAdapter", &err);
    if (err) return err;
    /* progress is not self._polling_progress_event (identity by id) */
    if (ev_state->type != JSON_OBJECT) return json_bool(true);
    const json_t *id_p = ta2getattr(progress, "id");
    const json_t *id_e = json_obj_get(ev_state, "id");
    return json_bool(!ta2_eq(id_p, id_e));
}

/* PoP: ta2_recovery_in_flight @ plugins/platforms/telegram/adapter.py:_recovery_in_flight */
static json_t *ta2_recovery_in_flight(ta2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *err = NULL;
    const json_t *task = ta2attr(ctx->state, "_polling_error_task", "TelegramAdapter", &err);
    if (err) return err;
    if (!ta2_present(task)) return json_bool(false);
    json_t *done = ta2_task_done(ctx, task, &err);
    if (err) return err;
    if (ta2_is_raise(done)) return done;
    bool d = ta2_truthy(done);
    json_free(done);
    return json_bool(!d);
}

/* PoP: ta2_check_ingress_dispatch_stall @ plugins/platforms/telegram/adapter.py:_check_ingress_dispatch_stall */
static json_t *ta2_check_ingress_dispatch_stall(ta2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *err = NULL;
    const json_t *webhook = ta2attr(ctx->state, "_webhook_mode", "TelegramAdapter", &err);
    if (err) return err;
    if (ta2_truthy(webhook)) return json_null();
    const json_t *td = json_obj_get(ctx->state, "_polling_teardown_started");
    if (ta2_truthy(td)) return json_null();
    const json_t *fm = ta2attr(ctx->state, "_fatal_error_message", "TelegramAdapter", &err);
    if (err) return err;
    if (ta2_present(fm)) return json_null();
    json_t *rif = ta2_recovery_in_flight(ctx, json_object());
    if (ta2_is_raise(rif)) return rif;
    bool inflight = ta2_truthy(rif);
    json_free(rif);
    if (inflight) return json_null();
    const json_t *received = json_obj_get(ctx->state, "_updates_received_total");
    const json_t *dispatched = json_obj_get(ctx->state, "_updates_dispatched_total");
    const json_t *seen = json_obj_get(ctx->state, "_ingress_dispatched_seen");
    /* received <= dispatched (numbers/bools; absent default 0) */
    double rv = received ? (received->type == JSON_BOOL ? (received->bool_val ? 1 : 0) : received->num_val) : 0;
    double dv = dispatched ? (dispatched->type == JSON_BOOL ? (dispatched->bool_val ? 1 : 0) : dispatched->num_val) : 0;
    if ((received && received->type != JSON_NUMBER && received->type != JSON_BOOL) ||
        (dispatched && dispatched->type != JSON_NUMBER && dispatched->type != JSON_BOOL)) {
        char msg[160];
        snprintf(msg, sizeof msg, "'<=' not supported between instances of '%s' and '%s'",
                 ta2_type_name(received), ta2_type_name(dispatched));
        return ta2_err("TypeError", msg);
    }
    if (rv <= dv || !ta2_eq(dispatched, seen)) {
        json_set(ctx->state, "_ingress_dispatched_seen", dispatched ? json_copy(dispatched) : json_number(0));
        json_set(ctx->state, "_ingress_stalled_heartbeats", json_number(0));
        return json_null();
    }
    const json_t *stalled_n = json_obj_get(ctx->state, "_ingress_stalled_heartbeats");
    if (stalled_n && stalled_n->type != JSON_NUMBER && stalled_n->type != JSON_BOOL) {
        char msg[160];
        snprintf(msg, sizeof msg, "'>=' not supported between instances of '%s' and 'int'", ta2_type_name(stalled_n));
        return ta2_err("TypeError", msg);
    }
    double stalled = stalled_n ? (stalled_n->type == JSON_BOOL ? (stalled_n->bool_val ? 1 : 0) : stalled_n->num_val) : 0;
    if (stalled >= TA2_INGRESS_DISPATCH_STALL_HEARTBEATS) return json_null();
    json_set(ctx->state, "_ingress_stalled_heartbeats", json_number(stalled + 1));
    if (stalled + 1 < TA2_INGRESS_DISPATCH_STALL_HEARTBEATS) return json_null();
    const json_t *generation = json_obj_get(ctx->state, "_polling_generation");
    char *rs = ta2_str_of(received);
    char *ds = ta2_str_of(dispatched);
    char *gs = ta2_str_of(generation);
    char diff[64];
    snprintf(diff, sizeof diff, "%.0f", rv - dv);
    char msg[512];
    snprintf(msg, sizeof msg,
             "ingress healthy but deaf: PTB dispatcher made no progress for %d heartbeats with %s "
             "update(s) fetched but not dispatched (%s received, %s dispatched, generation %s)",
             TA2_INGRESS_DISPATCH_STALL_HEARTBEATS, diff, rs, ds, gs);
    free(rs); free(ds); free(gs);
    json_t *ea = json_object();
    json_t *eo = json_object();
    json_set_hdr(eo, "class", "_PollingStallError");
    json_set_hdr(eo, "message", msg);
    json_set(ea, "error", eo);
    json_set_hdr(ea, "reason", "ingress dispatch stall watchdog");
    json_t *spr = ta2_call(ctx, "self._schedule_polling_recovery", ea);
    if (ta2_is_raise(spr)) return spr;
    json_free(spr);
    return json_null();
}

/* PoP: ta2_current_bot_username @ plugins/platforms/telegram/adapter.py:_current_bot_username */
static json_t *ta2_current_bot_username(ta2_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *observed = json_obj_get(ctx->state, "_bot_username_observed");
    if (ta2_truthy(observed)) return json_copy(observed);
    json_t *err = NULL;
    const json_t *bot = ta2attr(ctx->state, "_bot", "TelegramAdapter", &err);
    if (err) return err;
    const json_t *u = ta2getattr(bot, "username");
    if (ta2_truthy(u) && u->type != JSON_STRING)
        return ta2_obj_method_err(u, "lstrip");
    const char *us = ta2_truthy(u) ? (u->str_val ? u->str_val : "") : "";
    char *ls = ta2_lstrip_at(us);
    char *lo = ta2_lower(ls);
    free(ls);
    json_t *ret = json_string(lo);
    free(lo);
    return ret;
}

/* PoP: ta2_note_bot_username @ plugins/platforms/telegram/adapter.py:_note_bot_username */
static json_t *ta2_note_bot_username(ta2_ctx_t *ctx, const json_t *args) {
    const json_t *username = ta2_arg(args, "username");
    if (ta2_truthy(username) && username->type != JSON_STRING)
        return ta2_obj_method_err(username, "lstrip");
    const char *us = ta2_truthy(username) ? (username->str_val ? username->str_val : "") : "";
    char *ls = ta2_lstrip_at(us);
    char *handle = ta2_lower(ls);
    free(ls);
    if (!handle[0]) { free(handle); return json_null(); }
    const json_t *previous = json_obj_get(ctx->state, "_bot_username_observed");
    if (previous && previous->type == JSON_STRING &&
        strcmp(previous->str_val ? previous->str_val : "", handle) == 0) {
        free(handle);
        return json_null();
    }
    /* OWNED snapshot: the json_set below frees the displaced state slot, so
     * the borrowed `previous` would dangle before the logger.info copy. */
    json_t *prev = previous ? json_copy(previous) : NULL;
    json_set(ctx->state, "_bot_username_observed", json_string(handle));
    json_t *m = ta2_monotonic(ctx);
    if (ta2_is_raise(m)) { free(handle); json_free(prev); return m; }
    json_set(ctx->state, "_bot_identity_checked_at", m);
    if (ta2_truthy(prev)) {
        json_t *err = NULL;
        char *name = ta2_name(ctx, &err);
        if (err) { free(handle); json_free(prev); return err; }
        json_t *la = json_array();
        json_append(la, json_string(name));
        json_append(la, json_copy(prev));
        json_append(la, json_string(handle));
        free(name);
        json_t *lg = ta2_log(ctx, "info",
                             "[%s] Telegram bot username changed: @%s -> @%s (mention routing now follows the new handle)",
                             la);
        if (ta2_is_raise(lg)) { free(handle); json_free(prev); return lg; }
        json_free(lg);
    }
    json_free(prev);
    free(handle);
    return json_null();
}

/* PoP: ta2_observe_bot_identity_from_message @ plugins/platforms/telegram/adapter.py:_observe_bot_identity_from_message */
static json_t *ta2_observe_bot_identity_from_message(ta2_ctx_t *ctx, const json_t *args) {
    json_t *err = NULL;
    const json_t *bot = ta2attr(ctx->state, "_bot", "TelegramAdapter", &err);
    if (err) return err;
    const json_t *bot_id = ta2getattr(bot, "id");
    if (ta2_is_null(bot_id)) return json_null();
    const json_t *message = ta2_arg(args, "message");
    const json_t *cand1 = ta2getattr(message, "from_user");
    const json_t *rtm = ta2getattr(message, "reply_to_message");
    const json_t *cand2 = ta2getattr(rtm, "from_user");
    const json_t *cands[2] = {cand1, cand2};
    for (int i = 0; i < 2; i++) {
        const json_t *cand = cands[i];
        if (ta2_present(cand)) {
            const json_t *cid = ta2getattr(cand, "id");
            if (ta2_eq(cid, bot_id)) {
                const json_t *uname = ta2getattr(cand, "username");
                json_t *a = json_object();
                json_set(a, "username", uname ? json_copy(uname) : json_null());
                json_t *r = ta2_note_bot_username(ctx, a);
                if (ta2_is_raise(r)) return r;
                json_free(r);
            }
        }
    }
    return json_null();
}

/* PoP: ta2_bot_identity_is_fresh @ plugins/platforms/telegram/adapter.py:_bot_identity_is_fresh */
static json_t *ta2_bot_identity_is_fresh(ta2_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *checked = json_obj_get(ctx->state, "_bot_identity_checked_at");
    if (ta2_is_null(checked)) return json_bool(false);
    json_t *m = ta2_monotonic(ctx);
    if (ta2_is_raise(m)) return m;
    if (checked->type != JSON_NUMBER && checked->type != JSON_BOOL) {
        char msg[160];
        snprintf(msg, sizeof msg, "unsupported operand type(s) for -: 'float' and '%s'", ta2_type_name(checked));
        json_free(m);
        return ta2_err("TypeError", msg);
    }
    double cv = checked->type == JSON_BOOL ? (checked->bool_val ? 1 : 0) : checked->num_val;
    double diff = m->num_val - cv;
    json_free(m);
    return json_bool(diff < TA2_BOT_IDENTITY_TTL_SECONDS);
}

/* PoP: ta2_is_reply_to_bot @ plugins/platforms/telegram/adapter.py:_is_reply_to_bot */
static json_t *ta2_is_reply_to_bot(ta2_ctx_t *ctx, const json_t *args) {
    const json_t *message = ta2_arg(args, "message");
    const json_t *replied = ta2getattr(message, "reply_to_message");
    const json_t *thread_id = ta2getattr(message, "message_thread_id");
    json_t *err = NULL;
    const json_t *bot = ta2attr(ctx->state, "_bot", "TelegramAdapter", &err);
    if (err) return err;
    if (!ta2_present(bot)) return json_bool(false);
    if (!ta2_present(replied)) return json_bool(false);
    const json_t *ftc = ta2getattr(replied, "forum_topic_created");
    if (ta2_present(ftc) && !ta2_is_null(thread_id)) {
        const json_t *mid = ta2getattr(replied, "message_id");
        if (ta2_eq(mid, thread_id)) return json_bool(false);
    }
    const json_t *reply_user = ta2getattr(replied, "from_user");
    if (!ta2_present(reply_user)) return json_bool(false);
    const json_t *ruid = ta2getattr(reply_user, "id");
    const json_t *botid = ta2getattr(bot, "id");
    return json_bool(ta2_eq(ruid, botid));
}

/* PoP: ta2_effective_message_thread_id @ plugins/platforms/telegram/adapter.py:_effective_message_thread_id */
static json_t *ta2_effective_message_thread_id(ta2_ctx_t *ctx, const json_t *args) {
    const json_t *message = ta2_arg(args, "message");
    const json_t *chat = ta2getattr(message, "chat");
    json_t *ca = json_object();
    json_set(ca, "chat", chat ? json_copy(chat) : json_null());
    json_t *ct = ta2_call(ctx, "self._chat_type_str", ca);
    if (ta2_is_raise(ct)) return ct;
    const char *chat_type = ta2_str(ct);
    const json_t *raw = ta2getattr(message, "message_thread_id");
    bool itm = ta2_truthy(ta2getattr(message, "is_topic_message"));
    bool is_group = chat_type && (strcmp(chat_type, "group") == 0 || strcmp(chat_type, "supergroup") == 0);
    bool is_forum = false;
    if (is_group) {
        const json_t *ifv = ta2getattr(chat, "is_forum");
        is_forum = ifv && ifv->type == JSON_BOOL && ifv->bool_val;  /* is True */
    }
    json_t *ret;
    if (!ta2_is_null(raw)) {
        if (is_forum || (is_group && itm) ||
            (chat_type && strcmp(chat_type, "private") == 0 && itm)) {
            char *s = ta2_str_of(raw);
            ret = json_string(s);
            free(s);
        } else {
            ret = json_null();
        }
    } else {
        ret = is_forum ? json_string(TA2_GENERAL_TOPIC_THREAD_ID) : json_null();
    }
    json_free(ct);
    return ret;
}

/* config.extra carrier read shared by the config-extra gates. */
static const json_t *ta2_config_extra(ta2_ctx_t *ctx, json_t **err) {
    const json_t *config = ta2attr(ctx->state, "config", "TelegramAdapter", err);
    if (*err) return NULL;
    return ta2attr(config, "extra", "types.SimpleNamespace", err);
}

/* PoP: ta2_extra_bool @ plugins/platforms/telegram/adapter.py:_extra_bool */
static json_t *ta2_extra_bool(ta2_ctx_t *ctx, const json_t *args) {
    json_t *err = NULL;
    const json_t *extra = ta2_config_extra(ctx, &err);
    if (err) return err;
    const json_t *key = ta2_arg(args, "key");
    const json_t *env_name = ta2_arg(args, "env_name");
    const json_t *deflt = ta2_arg(args, "default");
    json_t *a = json_object();
    json_set(a, "extra", extra ? json_copy(extra) : json_null());
    json_set(a, "key", key ? json_copy(key) : json_null());
    json_set(a, "env_name", env_name ? json_copy(env_name) : json_null());
    json_set(a, "default", json_null());  /* live passes literal None, not the default param */
    json_t *cfg = ta2_call(ctx, "_extra_or_secret", a);
    if (ta2_is_raise(cfg)) return cfg;
    const json_t *fks = ta2_arg(args, "fallback_keys");
    if (fks && fks->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(fks); i++) {
            if (!ta2_is_null(cfg)) break;
            const json_t *alt = json_get(fks, i);
            const char *alts = ta2_str(alt);
            const json_t *v = ta2dictget(extra, alts ? alts : "", &err);
            if (err) { json_free(cfg); return err; }
            json_free(cfg);
            cfg = v ? json_copy(v) : json_null();
        }
    }
    if (ta2_is_null(cfg)) {
        json_free(cfg);
        cfg = deflt ? json_copy(deflt) : json_null();
    }
    if (cfg && cfg->type == JSON_BOOL) {
        json_t *ret = json_bool(cfg->bool_val);
        json_free(cfg);
        return ret;
    }
    char *s = ta2_str_of(cfg);
    char *st = ta2_strip(s);
    char *lo = ta2_lower(st);
    bool ok = strcmp(lo, "true") == 0 || strcmp(lo, "1") == 0 ||
              strcmp(lo, "yes") == 0 || strcmp(lo, "on") == 0;
    free(s); free(st); free(lo);
    json_free(cfg);
    return json_bool(ok);
}

/* PoP: ta2_extra_str_set @ plugins/platforms/telegram/adapter.py:_extra_str_set */
static json_t *ta2_extra_str_set(ta2_ctx_t *ctx, const json_t *args) {
    json_t *err = NULL;
    const json_t *extra = ta2_config_extra(ctx, &err);
    if (err) return err;
    const json_t *key = ta2_arg(args, "key");
    const json_t *env_name = ta2_arg(args, "env_name");
    json_t *a = json_object();
    json_set(a, "extra", extra ? json_copy(extra) : json_null());
    json_set(a, "key", key ? json_copy(key) : json_null());
    json_set(a, "env_name", env_name ? json_copy(env_name) : json_null());
    json_set_hdr(a, "default", "");
    json_set(a, "blank_is_unset", json_bool(false));
    json_t *raw = ta2_call(ctx, "_extra_or_secret", a);
    if (ta2_is_raise(raw)) return raw;
    json_t *da = json_object();
    json_set(da, "raw", raw);  /* adopt */
    json_t *dec = ta2_call(ctx, "_decode_json_list_literal", da);
    if (ta2_is_raise(dec)) return dec;
    size_t cap = 16, n = 0;
    char **items = malloc(cap * sizeof(char *));
    if (dec && dec->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(dec); i++) {
            char *s = ta2_str_of(json_get(dec, i));
            char *st = ta2_strip(s);
            free(s);
            if (st[0]) {
                if (n == cap) { cap *= 2; items = realloc(items, cap * sizeof(char *)); }
                items[n++] = st;
            } else free(st);
        }
    } else {
        char *s = ta2_str_of(dec);
        size_t np;
        char **pieces = ta2_split_comma(s, &np);
        free(s);
        for (size_t i = 0; i < np; i++) {
            char *st = ta2_strip(pieces[i]);
            free(pieces[i]);
            if (st[0]) {
                if (n == cap) { cap *= 2; items = realloc(items, cap * sizeof(char *)); }
                items[n++] = st;
            } else free(st);
        }
        free(pieces);
    }
    json_free(dec);
    return ta2_str_set_array(items, n);
}

/* PoP: ta2_telegram_ignored_threads @ plugins/platforms/telegram/adapter.py:_telegram_ignored_threads */
static json_t *ta2_telegram_ignored_threads(ta2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *err = NULL;
    const json_t *extra = ta2_config_extra(ctx, &err);
    if (err) return err;
    json_t *a = json_object();
    json_set(a, "extra", extra ? json_copy(extra) : json_null());
    json_set_hdr(a, "key", "ignored_threads");
    json_set_hdr(a, "env_name", "TELEGRAM_IGNORED_THREADS");
    json_set_hdr(a, "default", "");
    json_set(a, "blank_is_unset", json_bool(false));
    json_t *raw = ta2_call(ctx, "_extra_or_secret", a);
    if (ta2_is_raise(raw)) return raw;
    json_t *da = json_object();
    json_set(da, "raw", raw);
    json_t *dec = ta2_call(ctx, "_decode_json_list_literal", da);
    if (ta2_is_raise(dec)) return dec;
    size_t cap = 16, n = 0;
    double *items = malloc(cap * sizeof(double));
    /* iterate raw list elements or comma-split pieces */
    size_t nv;
    char **pieces = NULL;
    bool is_list = dec && dec->type == JSON_ARRAY;
    if (is_list) {
        nv = json_len(dec);
    } else {
        char *s = ta2_str_of(dec);
        pieces = ta2_split_comma(s, &nv);
        free(s);
    }
    for (size_t i = 0; i < nv; i++) {
        const json_t *value = is_list ? json_get(dec, i) : NULL;
        char *vs = is_list ? ta2_str_of(value) : ta2_strdup(pieces[i]);
        char *text = ta2_strip(vs);
        if (!text[0]) { free(vs); free(text); continue; }
        json_t *sv = json_string(text);
        json_t *iv = ta2_pyint(sv);
        json_free(sv);
        free(text);
        if (ta2_is_raise(iv)) {
            if (ta2_is_type_or_value(ta2_raise_kind(iv))) {
                json_free(iv);
                char *name = ta2_name(ctx, &err);
                if (err) { free(vs); goto fail; }
                json_t *la = json_array();
                json_append(la, json_string(name));
                json_append(la, is_list ? json_copy(value) : json_string(pieces[i]));
                free(name);
                json_t *lg = ta2_log(ctx, "warning",
                                     "[%s] Ignoring invalid Telegram thread id: %r", la);
                if (ta2_is_raise(lg)) { free(vs); goto fail; }
                json_free(lg);
                free(vs);
                continue;
            }
            free(vs);
            err = iv;
            goto fail;
        }
        if (n == cap) { cap *= 2; items = realloc(items, cap * sizeof(double)); }
        items[n++] = iv->num_val;
        json_free(iv);
        free(vs);
    }
    if (pieces) {
        for (size_t i = 0; i < nv; i++) free(pieces[i]);
        free(pieces);
    }
    json_free(dec);
    return ta2_num_set_array(items, n);
fail:
    if (pieces) {
        for (size_t i = 0; i < nv; i++) free(pieces[i]);
        free(pieces);
    }
    free(items);
    json_free(dec);
    return err;
}

/* set-returning seam result -> sorted string list (set projection). */
static json_t *ta2_set_seam(ta2_ctx_t *ctx, const char *name) {
    json_t *r = ta2_call(ctx, name, json_object());
    if (ta2_is_raise(r)) return r;
    if (!r || r->type != JSON_ARRAY) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", ta2_type_name(r));
        json_free(r);
        return ta2_err("TypeError", msg);
    }
    return r;
}

/* PoP: ta2_telegram_observe_allowed_chats @ plugins/platforms/telegram/adapter.py:_telegram_observe_allowed_chats */
static json_t *ta2_telegram_observe_allowed_chats(ta2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *ga = ta2_set_seam(ctx, "self._telegram_group_allowed_chats");
    if (ta2_is_raise(ga)) return ga;
    if (json_len(ga) == 0) { json_free(ga); return json_array(); }
    json_t *ra = ta2_set_seam(ctx, "self._telegram_allowed_chats");
    if (ta2_is_raise(ra)) { json_free(ga); return ra; }
    size_t cap = json_len(ga) + 1, n = 0;
    char **items = malloc(cap * sizeof(char *));
    bool intersect = json_len(ra) > 0;
    for (size_t i = 0; i < json_len(ga); i++) {
        const char *s = ta2_str(json_get(ga, i));
        if (!s) continue;
        if (intersect) {
            bool found = false;
            for (size_t j = 0; j < json_len(ra); j++) {
                const char *t = ta2_str(json_get(ra, j));
                if (t && strcmp(s, t) == 0) { found = true; break; }
            }
            if (!found) continue;
        }
        items[n++] = ta2_strdup(s);
    }
    json_free(ga);
    json_free(ra);
    return ta2_str_set_array(items, n);
}

/* PoP: ta2_telegram_is_free_response_topic @ plugins/platforms/telegram/adapter.py:_telegram_is_free_response_topic */
static json_t *ta2_telegram_is_free_response_topic(ta2_ctx_t *ctx, const json_t *args) {
    json_t *topics = ta2_set_seam(ctx, "self._telegram_free_response_topics");
    if (ta2_is_raise(topics)) return topics;
    if (json_len(topics) == 0) { json_free(topics); return json_bool(false); }
    const json_t *message = ta2_arg(args, "message");
    /* _chat_id_str leaf: str(getattr(getattr(message, "chat", None), "id", "")) */
    const json_t *chat = ta2getattr(message, "chat");
    const json_t *cid = ta2getattr(chat, "id");
    char *chat_id = cid ? ta2_str_of(cid) : ta2_strdup("");
    if (!chat_id[0]) { free(chat_id); json_free(topics); return json_bool(false); }
    json_t *ma = json_object();
    json_set(ma, "message", message ? json_copy(message) : json_null());
    json_t *tid = ta2_effective_message_thread_id(ctx, ma);
    if (ta2_is_raise(tid)) { free(chat_id); json_free(topics); return tid; }
    /* _topic_id_or_general leaf: str(thread_id) if not None else "1" */
    char *tids = ta2_is_null(tid) ? ta2_strdup(TA2_GENERAL_TOPIC_THREAD_ID) : ta2_str_of(tid);
    json_free(tid);
    size_t kn = strlen(chat_id) + 1 + strlen(tids) + 1;
    char *key = malloc(kn);
    snprintf(key, kn, "%s:%s", chat_id, tids);
    bool found = false;
    for (size_t i = 0; i < json_len(topics); i++) {
        const char *t = ta2_str(json_get(topics, i));
        if (t && strcmp(key, t) == 0) { found = true; break; }
    }
    free(key); free(chat_id); free(tids);
    json_free(topics);
    return json_bool(found);
}

/* PoP: ta2_int_or_zero @ plugins/platforms/telegram/adapter.py:_int_or_zero */
static json_t *ta2_int_or_zero(ta2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *value = ta2_arg(args, "value");
    json_t *v = ta2_truthy(value) ? json_copy(value) : json_number(0);
    json_t *r = ta2_pyint(v);
    json_free(v);
    if (ta2_is_raise(r)) {
        if (ta2_is_type_or_value(ta2_raise_kind(r))) {
            json_free(r);
            return json_number(0);
        }
        return r;
    }
    return r;
}

/* PoP: ta2_telegram_media_too_large_note @ plugins/platforms/telegram/adapter.py:_telegram_media_too_large_note */
static json_t *ta2_telegram_media_too_large_note(ta2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *label = ta2_arg(args, "label");
    const json_t *file_size = ta2_arg(args, "file_size");
    const json_t *max_bytes = ta2_arg(args, "max_bytes");
    if (!max_bytes || (max_bytes->type != JSON_NUMBER && max_bytes->type != JSON_BOOL)) {
        char msg[160];
        snprintf(msg, sizeof msg, "unsupported operand type(s) for //: '%s' and 'int'", ta2_type_name(max_bytes));
        return ta2_err("TypeError", msg);
    }
    double mb = max_bytes->type == JSON_BOOL ? (max_bytes->bool_val ? 1 : 0) : max_bytes->num_val;
    double limit = floor(mb / TA2_MB);
    if (limit < 1) limit = 1;
    json_t *fs = ta2_truthy(file_size) ? json_copy(file_size) : json_number(0);
    json_t *iv = ta2_pyint(fs);
    json_free(fs);
    char size_text[64];
    if (ta2_is_raise(iv)) {
        if (ta2_is_type_or_value(ta2_raise_kind(iv))) {
            snprintf(size_text, sizeof size_text, "unknown size");
            json_free(iv);
        } else return iv;
    } else {
        snprintf(size_text, sizeof size_text, "%.1f MB", iv->num_val / TA2_MB);
        json_free(iv);
    }
    char limstr[64];
    if (limit == floor(limit) && fabs(limit) < 1e16) snprintf(limstr, sizeof limstr, "%.0f", limit);
    else ta2_float_repr(limit, limstr, sizeof limstr);
    char *ls = ta2_str_of(label);
    char buf[512];
    snprintf(buf, sizeof buf,
             "[Telegram %s skipped: file size %s exceeds the %s MB limit. Ask the user to send a smaller file.]",
             ls, size_text, limstr);
    free(ls);
    return json_string(buf);
}

/* PoP: ta2_telegram_media_size_allowed @ plugins/platforms/telegram/adapter.py:_telegram_media_size_allowed */
static json_t *ta2_telegram_media_size_allowed(ta2_ctx_t *ctx, const json_t *args) {
    const json_t *mdb = json_obj_get(ctx->state, "_max_doc_bytes");
    json_t *v = ta2_truthy(mdb) ? json_copy(mdb) : json_number(TA2_MAX_DOC_BYTES_DEFAULT);
    json_t *mb = ta2_pyint(v);
    json_free(v);
    if (ta2_is_raise(mb)) return mb;
    double max_bytes = mb->num_val;
    json_free(mb);
    const json_t *source = ta2_arg(args, "source");
    const json_t *fs = ta2getattr(source, "file_size");
    json_t *ia = json_object();
    json_set(ia, "value", fs ? json_copy(fs) : json_null());
    json_t *size = ta2_int_or_zero(ctx, ia);
    if (ta2_is_raise(size)) return size;
    double sz = size->num_val;
    json_free(size);
    json_t *ret = json_array();
    if (sz <= 0 || sz <= max_bytes) {
        json_append(ret, json_bool(true));
        json_append(ret, json_null());
        return ret;
    }
    const json_t *label = ta2_arg(args, "label");
    json_t *na = json_object();
    json_set(na, "label", label ? json_copy(label) : json_null());
    json_set(na, "file_size", json_number(sz));
    json_set(na, "max_bytes", json_number(max_bytes));
    json_t *note = ta2_telegram_media_too_large_note(ctx, na);
    if (ta2_is_raise(note)) { json_free(ret); return note; }
    json_append(ret, json_bool(false));
    json_append(ret, note);
    return ret;
}

/* PoP: ta2_missing_media_path_error @ plugins/platforms/telegram/adapter.py:_missing_media_path_error */
static json_t *ta2_missing_media_path_error(ta2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *label = ta2_arg(args, "label");
    const json_t *path = ta2_arg(args, "path");
    if (!path || path->type != JSON_STRING)
        return ta2_obj_method_err(path, "startswith");
    const char *ps = path->str_val ? path->str_val : "";
    char *ls = ta2_str_of(label);
    char buf[1024];
    snprintf(buf, sizeof buf, "%s file not found: %s", ls, ps);
    free(ls);
    if (strncmp(ps, "/workspace/", 11) == 0 || strncmp(ps, "/output/", 8) == 0 ||
        strncmp(ps, "/outputs/", 9) == 0) {
        size_t k = strlen(buf);
        snprintf(buf + k, sizeof buf - k,
                 " (path may only exist inside the Docker sandbox. Bind-mount a host directory and "
                 "emit the host-visible path in MEDIA: for gateway file delivery.)");
    }
    return json_string(buf);
}

/* PoP: ta2_media_send_kwargs @ plugins/platforms/telegram/adapter.py:_media_send_kwargs */
static json_t *ta2_media_send_kwargs(ta2_ctx_t *ctx, const json_t *args) {
    json_t *err = NULL;
    const json_t *rtm = ta2attr(ctx->state, "_reply_to_mode", "TelegramAdapter", &err);
    if (err) return err;
    const json_t *chat_id = ta2_arg(args, "chat_id");
    const json_t *reply_to = ta2_arg(args, "reply_to");
    const json_t *metadata = ta2_arg(args, "metadata");
    json_t *a1 = json_object();
    json_set(a1, "reply_to", reply_to ? json_copy(reply_to) : json_null());
    json_set(a1, "metadata", metadata ? json_copy(metadata) : json_null());
    json_set(a1, "reply_to_mode", json_copy(rtm));
    json_t *rid = ta2_call(ctx, "self._reply_to_message_id_for_send", a1);
    if (ta2_is_raise(rid)) return rid;
    json_t *a2 = json_object();
    json_set(a2, "metadata", metadata ? json_copy(metadata) : json_null());
    json_t *tid = ta2_call(ctx, "self._metadata_thread_id", a2);
    if (ta2_is_raise(tid)) { json_free(rid); return tid; }
    json_t *a3 = json_object();
    json_set(a3, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_set(a3, "thread_id", tid);  /* adopt */
    json_set(a3, "metadata", metadata ? json_copy(metadata) : json_null());
    json_set(a3, "reply_to_message_id", json_copy(rid));
    json_set(a3, "reply_to_mode", json_copy(rtm));
    json_t *tk = ta2_call(ctx, "self._thread_kwargs_for_send", a3);
    if (ta2_is_raise(tk)) { json_free(rid); return tk; }
    /* dict display evaluates chat_id=normalize(...) BEFORE the **tk merge */
    json_t *kw = json_object();
    json_t *a4 = json_object();
    json_set(a4, "chat_id", chat_id ? json_copy(chat_id) : json_null());
    json_t *ncid = ta2_call(ctx, "normalize_telegram_chat_id", a4);
    if (ta2_is_raise(ncid)) { json_free(tk); json_free(rid); json_free(kw); return ncid; }
    json_set(kw, "chat_id", ncid);
    json_set(kw, "reply_to_message_id", json_copy(rid));
    json_set(kw, "read_timeout", json_number(TA2_MEDIA_SEND_READ_TIMEOUT));
    if (!tk || tk->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object is not a mapping", ta2_type_name(tk));
        json_free(tk); json_free(rid); json_free(kw);
        return ta2_err("TypeError", msg);
    }
    for (size_t i = 0; i < json_object_size(tk); i++) {
        const char *k = json_object_get_key_at(tk, i);
        json_set(kw, k, json_copy(json_object_get_at(tk, i)));
    }
    json_t *a5 = json_object();
    json_set(a5, "metadata", metadata ? json_copy(metadata) : json_null());
    json_t *nk = ta2_call(ctx, "self._notification_kwargs", a5);
    if (ta2_is_raise(nk)) { json_free(tk); json_free(rid); json_free(kw); return nk; }
    if (!nk || nk->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object is not a mapping", ta2_type_name(nk));
        json_free(nk); json_free(tk); json_free(rid); json_free(kw);
        return ta2_err("TypeError", msg);
    }
    for (size_t i = 0; i < json_object_size(nk); i++) {
        const char *k = json_object_get_key_at(nk, i);
        json_set(kw, k, json_copy(json_object_get_at(nk, i)));
    }
    json_free(tk);
    json_free(nk);
    json_t *ret = json_array();
    json_append(ret, rid);  /* adopt */
    json_append(ret, kw);
    return ret;
}

/* PoP: ta2_sniff_raster_format @ plugins/platforms/telegram/adapter.py:_sniff_raster_format */
static json_t *ta2_sniff_raster_format(ta2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *image_path = ta2_arg(args, "image_path");
    json_t *oa = json_object();
    json_set(oa, "path", image_path ? json_copy(image_path) : json_null());
    json_set_hdr(oa, "mode", "rb");
    json_t *of = ta2_call(ctx, "open", oa);
    if (ta2_is_raise(of)) {
        if (ta2_is_oserror(ta2_raise_kind(of))) { json_free(of); return json_null(); }
        return of;
    }
    json_free(of);
    json_t *ra = json_object();
    json_set(ra, "n", json_number(16));
    json_t *hr = ta2_call(ctx, "file_read", ra);
    if (ta2_is_raise(hr)) {
        if (ta2_is_oserror(ta2_raise_kind(hr))) { json_free(hr); return json_null(); }
        return hr;
    }
    /* hex string -> bytes (<= 16) with bytes.fromhex() validation.
     * ASCII whitespace is skipped before each byte; an invalid nibble raises
     * ValueError with the exact CPython position message. */
    unsigned char head[16];
    size_t hn = 0;
    const char *hex = ta2_str(hr);
    if (hex) {
        size_t L = strlen(hex);
        size_t i = 0;
        while (i < L) {
            /* skip ASCII whitespace before each byte, like bytes.fromhex */
            while (i < L && (hex[i] == ' ' || hex[i] == '\t' || hex[i] == '\n' ||
                             hex[i] == '\r' || hex[i] == '\x0b' || hex[i] == '\x0c')) i++;
            if (i >= L) break;
            int nib[2];
            for (int j = 0; j < 2; j++) {
                if (i >= L) {
                    json_free(hr);
                    return ta2_err("ValueError", "fromhex() arg must contain an even number of hexadecimal digits");
                }
                unsigned char c = (unsigned char)hex[i];
                int v;
                if (c >= '0' && c <= '9') v = c - '0';
                else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
                else {
                    char msg[96];
                    snprintf(msg, sizeof msg,
                             "non-hexadecimal number found in fromhex() arg at position %zu", i);
                    json_free(hr);
                    return ta2_err("ValueError", msg);
                }
                nib[j] = v;
                i++;
            }
            if (hn < 16) head[hn++] = (unsigned char)((nib[0] << 4) | nib[1]);
        }
    }
    json_free(hr);
    static const unsigned char png[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (hn >= 8 && memcmp(head, png, 8) == 0) return json_string("png");
    if ((hn >= 6 && memcmp(head, "GIF87a", 6) == 0) ||
        (hn >= 6 && memcmp(head, "GIF89a", 6) == 0)) return json_null();
    if (hn >= 4 && memcmp(head, "RIFF", 4) == 0 && hn >= 12 && memcmp(head + 8, "WEBP", 4) == 0)
        return json_string("webp");
    if (hn >= 2 && memcmp(head, "BM", 2) == 0) return json_string("bmp");
    static const unsigned char tiff_le[] = {'I', 'I', '*', 0x00};
    static const unsigned char tiff_be[] = {'M', 'M', 0x00, '*'};
    if ((hn >= 4 && memcmp(head, tiff_le, 4) == 0) ||
        (hn >= 4 && memcmp(head, tiff_be, 4) == 0)) return json_string("tiff");
    return json_null();
}

/* PoP: ta2_cold_boot_drop_pending @ plugins/platforms/telegram/adapter.py:_cold_boot_drop_pending */
static json_t *ta2_cold_boot_drop_pending(ta2_ctx_t *ctx, const json_t *args) {
    const json_t *is_reconnect = ta2_arg(args, "is_reconnect");
    bool rec = ta2_truthy(is_reconnect);
    json_t *err = NULL;
    const json_t *dp = NULL;
    if (!rec) {
        dp = ta2attr(ctx->state, "_drop_pending_on_cold_boot", "TelegramAdapter", &err);
        if (err) return err;
    }
    if (!rec) {
        char *name = ta2_name(ctx, &err);
        if (err) return err;
        const char *act = ta2_truthy(dp) ? "dropping" : "preserving";
        const char *flag = ta2_truthy(dp) ? "true" : "false";
        json_t *la = json_array();
        json_append(la, json_string(name));
        json_append(la, json_string(act));
        json_append(la, json_string(flag));
        free(name);
        json_t *lg = ta2_log(ctx, "info",
                             "[%s] Cold boot: %s Telegram updates queued while offline "
                             "(platforms.telegram.extra.drop_pending_on_cold_boot: %s)", la);
        if (ta2_is_raise(lg)) return lg;
        json_free(lg);
    }
    return dp ? json_copy(dp) : json_bool(false);
}

/* PoP: ta2_looks_like_polling_conflict @ plugins/platforms/telegram/adapter.py:_looks_like_polling_conflict */
static json_t *ta2_looks_like_polling_conflict(ta2_ctx_t *ctx, const json_t *args) {
    const json_t *error = ta2_arg(args, "error");
    const char *eid = ta2_str(error);
    const char *text = ta2_err_text(ctx, eid);
    const char *cls = ta2_err_class(ctx, eid);
    char *lower = ta2_lower(text);
    char *clslower = ta2_lower(cls);
    bool hit = strcmp(clslower, "conflict") == 0 ||
               strstr(lower, "terminated by other getupdates request") != NULL ||
               strstr(lower, "another bot instance is running") != NULL;
    free(lower);
    free(clslower);
    return json_bool(hit);
}
