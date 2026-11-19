/*
 * session_transcript_flow.c — UNCLAIMED transcript FLOW cores + leaves of
 * gateway/session_transcript.py (SessionTranscriptMixin + module helpers).
 *
 * Ports the DECISION sequences: spool-drop fallback classification, bindable
 * TEXT projection (int64 range gate + json.dumps(default=str)), the transcript
 * append-kwargs field order / assistant-only gating / fallback_ts is-None
 * gate, cycle-guarded reroute following, retry-queue cap eviction + spool,
 * replaced-DB divert ordering (queue detach -> per-message spool -> JSONL
 * fallback), live compression-child adoption, parent->child queue migration
 * (head identity pop, backlog order, failure-counter max, reroute record),
 * route republication + owner-hint pop, stalled-backlog spool rotation,
 * spooled-session marking, platform-id dedupe probe, tail-role lookup,
 * published-compression-child probe chain, rotated-child persistence
 * short-circuit, input-owner lineage walk (fail-closed TranscriptReadError),
 * load_transcript routing + suppress(Exception) tip heal + fail-closed read,
 * refused-turn retention. Claimed siblings (_compression_tip_for_session_id,
 * _get_transcript_drain_lock, _clear_dirty_transcript, rewrite_transcript,
 * append_to_transcript*, _drain_spooled_drops, _rebuild_fts_once,
 * _is_fts_corruption_error, rewind_session, advance_compression_session,
 * _heal_compression_tip_locked, _get_transcript_drain_lock) enter through the
 * universal seam; _clear_dirty_transcript additionally applies its known
 * queue-drop state effect on both sides.
 *
 * Modeling contract (shared with tests/oracle_session_transcript_flow.py):
 *  - State mirror shape (in["state"], copied by the harness before mutation):
 *      dirty_transcripts:      {sid: [message...]}   (_dirty_transcripts)
 *      append_failures:        {sid: count}          (_transcript_append_failures)
 *      reroutes:               {sid: child} | null   (_transcript_reroutes;
 *                              null == attribute ABSENT; _lazy creates {})
 *      spooled_drop_sessions:  [sid...] | null       (set; sorted on dump;
 *                              null == attribute ABSENT)
 *      session_owner_hints:    {child: key} | null   (null == ABSENT)
 *      entries:                {key: {"session_id": sid, ...}}
 *                              (_entries; SimpleNamespace doubles)
 *      stores:                 {store_id: {}}        (input-only registry of
 *                              persistence handles; never in out state)
 *  - A persistence_for_session_id seam result that is a STRING registered in
 *    state.stores is a store double (attributes resolve inertly, then the
 *    method seam fires); any other value is the raw Python value (None, "",
 *    unknown str, ...) and attribute access on it raises AttributeError with
 *    the JSON-domain type name BEFORE any method seam fires.
 *  - Row / watermark seam results that are objects materialize as
 *    SimpleNamespace doubles: a missing field raises AttributeError
 *    'SimpleNamespace'; null results raise AttributeError 'NoneType'.
 *  - Scripted seam results: NULL == None; {"__raise__": kind, "message": m}
 *    == a raised exception. BaseException-only kinds (SystemExit,
 *    KeyboardInterrupt, GeneratorExit, CancelledError, BaseExceptionGroup)
 *    pass through EVERY `except Exception` site via env->exc. The FULL
 *    builtin exception hierarchy is tabled below for the classification.
 *    TranscriptReadError raises carry session_id; their message is always
 *    rebuilt from session_id (the ctor f-string), never the scripted text.
 *  - Script sequences: a bare JSON array is ONE RESULT PER CALL (last value
 *    repeats); an empty array is a constant None; {"__script__": true,
 *    "ret": v} is a constant; absent == None.
 *  - Log seam records raw fmt + args (no printf formatting); exception args
 *    record str(exc); other args stay raw JSON values (None stays null);
 *    exc_info is recorded only when the live call passes it.
 *  - Locks (_lock, _transcript_retry_lock) are inert in the single-threaded
 *    harness (no events). _get_transcript_drain_lock is a CLAIMED sibling and
 *    fires a seam.
 *  - Integer domain: JSON numbers are doubles; integral-valued numbers are
 *    ints (fixtures write integer literals, never 1.0 / 1e30, on paths where
 *    int-vs-float is observable: _bindable_text range gate / dumps).
 *  - _bindable_text containers serialize with Python json.dumps(default=str)
 *    semantics: ", " / ": " separators, ensure_ascii, insertion key order,
 *    shortest-roundtrip float repr. Pure JSON values never hit the
 *    TypeError/ValueError -> str(value) fallback (kept for fidelity).
 *  - Path domain: get_hermes_home scripts a clean absolute "C:/..." home;
 *    sessions_dir is its WindowsPath str() projection + "\\sessions"
 *    (backslashes, collapsed separators). "" home -> "sessions".
 *  - _migrate head identity (`pending[0] is msg`) is modeled by the
 *    in["msg_is_head"] flag; the harness seeds dirty_transcripts[queue] with
 *    in["pending"] (the SAME list the live caller passes).
 *  - in["cap"] overrides _MAX_PENDING_PER_SESSION for _enqueue (default 200).
 *  - Self-contained: the harness #includes this .c; every symbol is static;
 *    only libjson + the C standard library are used.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "libjson/json.h"

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct stx_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
    json_t *exc;     /* owned unhandled raise descriptor; NULL when none */
    json_t *cur;     /* owned descriptor of the exception being caught */
} stx_env;

/* Universal seam call: args BORROWED by the stub (it copies into the trace),
 * freed here; result NEW (caller frees). NULL result == Python None. */
static json_t *stx_call(stx_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}

/* ── small JSON helpers ───────────────────────────────────────────────────── */
static bool stx_truth(const json_t *v) {
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
static bool stx_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool stx_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool stx_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool stx_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const json_t *stx_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static json_t *stx_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}
static bool stx_json_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL:   return true;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                        b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!stx_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !stx_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
    }
    return false;
}
static bool stx_arr_has(const json_t *arr, const json_t *v) {
    if (!stx_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (stx_json_eq(arr->c.items[i], v)) return true;
    return false;
}
static void stx_arr_add_once(json_t *arr, const json_t *v) {
    if (!stx_is_arr(arr) || !v || stx_arr_has(arr, v)) return;
    json_append(arr, json_copy(v));
}
static void stx_arr_pop0(json_t *arr) {
    if (!stx_is_arr(arr) || arr->c.count == 0) return;
    json_free(arr->c.items[0]);
    for (size_t j = 0; j + 1 < arr->c.count; j++)
        arr->c.items[j] = arr->c.items[j + 1];
    arr->c.count--;
}
/* Detach a key's value WITHOUT freeing it (dict aliasing moves). */
static json_t *stx_obj_take(json_t *obj, const char *key) {
    if (!stx_is_obj(obj) || !key) return NULL;
    for (size_t i = 0; i < obj->c.count; i++) {
        if (obj->c.keys[i] && strcmp(obj->c.keys[i], key) == 0) {
            json_t *v = obj->c.items[i];
            free(obj->c.keys[i]);
            for (size_t j = i; j + 1 < obj->c.count; j++) {
                obj->c.keys[j] = obj->c.keys[j + 1];
                obj->c.items[j] = obj->c.items[j + 1];
            }
            obj->c.count--;
            return v;
        }
    }
    return NULL;
}

/* ── Python str() for the scalar fixture domain ───────────────────────────── */
static bool stx_integral(double d) { return d == floor(d); }
/* Shortest-roundtrip float repr with Python formatting rules. */
static char *stx_float_repr(double d) {
    char sci[64];
    int prec;
    for (prec = 1; prec <= 17; prec++) {
        snprintf(sci, sizeof(sci), "%.*e", prec - 1, d);
        if (strtod(sci, NULL) == d) break;
    }
    bool neg = sci[0] == '-';
    const char *p = neg ? sci + 1 : sci;
    char dig[64];
    size_t nd = 0;
    while (*p && *p != 'e' && *p != 'E') {
        if (*p >= '0' && *p <= '9') dig[nd++] = *p;
        p++;
    }
    dig[nd] = '\0';
    int E = (*p == 'e' || *p == 'E') ? atoi(p + 1) : 0;
    while (nd > 1 && dig[nd - 1] == '0') dig[--nd] = '\0';
    char *out = (char *)malloc(nd + 64);
    if (!out) return strdup("");
    if (E >= -4 && E < 16) {
        int point = E + 1;
        if (point <= 0) {
            sprintf(out, "%s0.%s%s", neg ? "-" : "",
                    "000000000000000000000000000000" + (30 + point), dig);
            /* point <= 0: -point zeros (bounded by E >= -4 -> <= 4) */
        } else if ((size_t)point >= nd) {
            size_t zeros = (size_t)point - nd;
            sprintf(out, "%s%s%.*s.0", neg ? "-" : "", dig, (int)zeros,
                    "000000000000000000000000000000");
        } else {
            sprintf(out, "%s%.*s.%s", neg ? "-" : "", point, dig, dig + point);
        }
    } else {
        if (nd > 1)
            sprintf(out, "%s%c.%s%s%c%02d", neg ? "-" : "", dig[0], dig + 1,
                    "e", E < 0 ? '-' : '+', E < 0 ? -E : E);
        else
            sprintf(out, "%s%c%s%c%02d", neg ? "-" : "", dig[0],
                    "e", E < 0 ? '-' : '+', E < 0 ? -E : E);
    }
    return out;
}
static char *stx_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (stx_integral(d) && d < 9.2e18 && d > -9.2e18) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
                return strdup(buf);
            }
            return stx_float_repr(d);
        }
        default: {
            char *s = json_serialize(v);
            return s ? s : strdup("");
        }
    }
}
/* JSON-domain Python type name (for AttributeError messages). */
static const char *stx_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return stx_integral(v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

/* ── Python json.dumps(default=str) for pure JSON values ─────────────────── */
typedef struct { char *buf; size_t len, cap; } stx_strbuf;
static void stx_sb_init(stx_strbuf *b) {
    b->cap = 128; b->len = 0;
    b->buf = (char *)malloc(b->cap);
    if (b->buf) b->buf[0] = '\0';
}
static void stx_sb_put(stx_strbuf *b, const char *s, size_t n) {
    if (!b->buf) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap * 2 + n + 1;
        char *nb = (char *)realloc(b->buf, nc);
        if (!nb) return;
        b->buf = nb; b->cap = nc;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}
static void stx_sb_puts(stx_strbuf *b, const char *s) { stx_sb_put(b, s, strlen(s)); }
static void stx_sb_hex4(stx_strbuf *b, unsigned cp) {
    char tmp[8];
    snprintf(tmp, sizeof(tmp), "\\u%04x", cp);
    stx_sb_puts(b, tmp);
}
static void stx_dumps_str(stx_strbuf *b, const char *s) {
    stx_sb_puts(b, "\"");
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p;
        if (c == '"') { stx_sb_puts(b, "\\\""); p++; }
        else if (c == '\\') { stx_sb_puts(b, "\\\\"); p++; }
        else if (c == '\n') { stx_sb_puts(b, "\\n"); p++; }
        else if (c == '\r') { stx_sb_puts(b, "\\r"); p++; }
        else if (c == '\t') { stx_sb_puts(b, "\\t"); p++; }
        else if (c == '\b') { stx_sb_puts(b, "\\b"); p++; }
        else if (c == '\f') { stx_sb_puts(b, "\\f"); p++; }
        else if (c < 0x20) { stx_sb_hex4(b, c); p++; }
        else if (c < 0x7f) { stx_sb_put(b, (const char *)p, 1); p++; }
        else if (c == 0x7f) { stx_sb_hex4(b, 0x7f); p++; }
        else {
            /* UTF-8 -> codepoint -> \uXXXX (surrogate pair for astral) */
            unsigned cp = 0; int n = 0;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
            else { cp = c; n = 1; }
            for (int i = 1; i < n && p[i]; i++)
                cp = (cp << 6) | (p[i] & 0x3F);
            p += n;
            if (cp >= 0x10000) {
                cp -= 0x10000;
                stx_sb_hex4(b, 0xD800 + (cp >> 10));
                stx_sb_hex4(b, 0xDC00 + (cp & 0x3FF));
            } else {
                stx_sb_hex4(b, cp);
            }
        }
    }
    stx_sb_puts(b, "\"");
}
static void stx_dumps_val(stx_strbuf *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { stx_sb_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: stx_sb_puts(b, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (stx_integral(d)) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%.0f", d);
                stx_sb_puts(b, tmp);
            } else {
                char *r = stx_float_repr(d);
                stx_sb_puts(b, r);
                free(r);
            }
            return;
        }
        case JSON_STRING: stx_dumps_str(b, v->str_val ? v->str_val : ""); return;
        case JSON_ARRAY:
            stx_sb_puts(b, "[");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) stx_sb_puts(b, ", ");
                stx_dumps_val(b, v->c.items[i]);
            }
            stx_sb_puts(b, "]");
            return;
        case JSON_OBJECT:
            stx_sb_puts(b, "{");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) stx_sb_puts(b, ", ");
                stx_dumps_str(b, v->c.keys[i]);
                stx_sb_puts(b, ": ");
                stx_dumps_val(b, v->c.items[i]);
            }
            stx_sb_puts(b, "}");
            return;
        default: stx_sb_puts(b, "null"); return;
    }
}
static char *stx_dumps(const json_t *v) {
    stx_strbuf b;
    stx_sb_init(&b);
    stx_dumps_val(&b, v);
    return b.buf ? b.buf : strdup("");
}

/* ── FULL builtin exception hierarchy (parent table) ────────────────────────
 * Classification walks the chain: a kind whose ancestry reaches Exception is
 * caught by `except Exception`; BaseException-only kinds pass through.
 * Unknown kinds classify as Exception (the oracle materializes them as
 * dynamic Exception subclasses). ExceptionGroup is the one builtin with
 * mixed ancestry (BaseExceptionGroup + Exception) and is special-cased. */
typedef struct { const char *name, *parent; } stx_exc_parent;
static const stx_exc_parent stx_exc_table[] = {
    { "BaseException", "" },
    { "SystemExit", "BaseException" },
    { "KeyboardInterrupt", "BaseException" },
    { "GeneratorExit", "BaseException" },
    { "Exception", "BaseException" },
    { "StopIteration", "Exception" },
    { "StopAsyncIteration", "Exception" },
    { "ArithmeticError", "Exception" },
    { "FloatingPointError", "ArithmeticError" },
    { "OverflowError", "ArithmeticError" },
    { "ZeroDivisionError", "ArithmeticError" },
    { "AssertionError", "Exception" },
    { "AttributeError", "Exception" },
    { "BufferError", "Exception" },
    { "EOFError", "Exception" },
    { "ImportError", "Exception" },
    { "ModuleNotFoundError", "ImportError" },
    { "LookupError", "Exception" },
    { "IndexError", "LookupError" },
    { "KeyError", "LookupError" },
    { "MemoryError", "Exception" },
    { "NameError", "Exception" },
    { "UnboundLocalError", "NameError" },
    { "OSError", "Exception" },
    { "BlockingIOError", "OSError" },
    { "ChildProcessError", "OSError" },
    { "ConnectionError", "OSError" },
    { "BrokenPipeError", "ConnectionError" },
    { "ConnectionAbortedError", "ConnectionError" },
    { "ConnectionRefusedError", "ConnectionError" },
    { "ConnectionResetError", "ConnectionError" },
    { "FileExistsError", "OSError" },
    { "FileNotFoundError", "OSError" },
    { "InterruptedError", "OSError" },
    { "IsADirectoryError", "OSError" },
    { "NotADirectoryError", "OSError" },
    { "PermissionError", "OSError" },
    { "ProcessLookupError", "OSError" },
    { "TimeoutError", "OSError" },
    { "ReferenceError", "Exception" },
    { "RuntimeError", "Exception" },
    { "NotImplementedError", "RuntimeError" },
    { "RecursionError", "RuntimeError" },
    { "SyntaxError", "Exception" },
    { "IndentationError", "SyntaxError" },
    { "TabError", "IndentationError" },
    { "SystemError", "Exception" },
    { "TypeError", "Exception" },
    { "ValueError", "Exception" },
    { "UnicodeError", "ValueError" },
    { "UnicodeDecodeError", "UnicodeError" },
    { "UnicodeEncodeError", "UnicodeError" },
    { "UnicodeTranslateError", "UnicodeError" },
    { "Warning", "Exception" },
    { "DeprecationWarning", "Warning" },
    { "PendingDeprecationWarning", "Warning" },
    { "RuntimeWarning", "Warning" },
    { "SyntaxWarning", "Warning" },
    { "UserWarning", "Warning" },
    { "FutureWarning", "Warning" },
    { "ImportWarning", "Warning" },
    { "UnicodeWarning", "Warning" },
    { "BytesWarning", "Warning" },
    { "EncodingWarning", "Warning" },
    { "ResourceWarning", "Warning" },
    { "BaseExceptionGroup", "BaseException" },
    { "ExceptionGroup", "BaseExceptionGroup" },
    /* non-builtin BaseException-only kinds the fixtures script */
    { "CancelledError", "BaseException" },
    { "TranscriptReadError", "RuntimeError" },
};
static bool stx_is_exception_kind(const char *kind) {
    if (!kind || !kind[0]) return true;
    if (strcmp(kind, "ExceptionGroup") == 0) return true;
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", kind);
    for (int hops = 0; hops < 64; hops++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        const char *parent = NULL;
        for (size_t i = 0; i < sizeof(stx_exc_table) / sizeof(stx_exc_table[0]); i++) {
            if (strcmp(stx_exc_table[i].name, cur) == 0) {
                parent = stx_exc_table[i].parent;
                break;
            }
        }
        if (!parent) return true;   /* unknown kind -> Exception subclass */
        if (!parent[0]) return false;
        snprintf(cur, sizeof(cur), "%s", parent);
    }
    return true;
}

/* ── raise descriptors ────────────────────────────────────────────────────── */
static bool stx_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && stx_is_str(json_obj_get(r, "__raise__"));
}
/* Normalize a raise spec to the oracle descriptor shape (BORROWS r).
 * TranscriptReadError messages are rebuilt from session_id (ctor f-string). */
static json_t *stx_norm_raise(const json_t *r) {
    const char *kind = json_get_str(r, "__raise__", "Exception");
    json_t *d = json_object();
    json_set(d, "__raise__", json_string(kind));
    if (strcmp(kind, "TranscriptReadError") == 0) {
        const json_t *sid = json_obj_get(r, "session_id");
        char *s = stx_pystr(sid);
        size_t n = strlen(s) + 64;
        char *msg = (char *)malloc(n);
        if (msg) snprintf(msg, n, "transcript read failed for session %s", s);
        json_set(d, "message", json_string(msg ? msg : ""));
        json_set(d, "session_id", stx_copy_or_null(sid));
        free(s); free(msg);
    } else {
        json_set(d, "message", json_string(json_get_str(r, "message", "")));
    }
    return d;
}
static void stx_raise(stx_env *e, const char *kind, const char *msg) {
    json_t *d = json_object();
    json_set(d, "__raise__", json_string(kind));
    json_set(d, "message", json_string(msg ? msg : ""));
    if (e->exc) json_free(e->exc);
    e->exc = d;
}
static void stx_raise_transcript(stx_env *e, const json_t *sid_v) {
    json_t *d = json_object();
    json_set(d, "__raise__", json_string("TranscriptReadError"));
    char *s = stx_pystr(sid_v);
    size_t n = strlen(s) + 64;
    char *msg = (char *)malloc(n);
    if (msg) snprintf(msg, n, "transcript read failed for session %s", s);
    json_set(d, "message", json_string(msg ? msg : ""));
    json_set(d, "session_id", stx_copy_or_null(sid_v));
    free(s); free(msg);
    if (e->exc) json_free(e->exc);
    e->exc = d;
}
/* Any seam raise propagates (consumes r when it IS a raise). True == bail. */
static bool stx_prop(stx_env *e, json_t *r) {
    if (!stx_is_raise(r)) return false;
    if (e->exc) json_free(e->exc);
    e->exc = stx_norm_raise(r);
    json_free(r);
    return true;
}
/* Classify a seam result at an `except Exception` site. NON-CONSUMING: r
 * stays owned by the caller on EVERY path (free it exactly once).
 * 0 = not a raise; 1 = Exception raise caught (copied into e->cur);
 * 2 = BaseException-only (copied into e->exc; caller bails). */
static int stx_exc_class(stx_env *e, const json_t *r) {
    if (!stx_is_raise(r)) return 0;
    const char *kind = json_get_str(r, "__raise__", "Exception");
    if (stx_is_exception_kind(kind)) {
        if (e->cur) json_free(e->cur);
        e->cur = stx_norm_raise(r);
        return 1;
    }
    if (e->exc) json_free(e->exc);
    e->exc = stx_norm_raise(r);
    return 2;
}
/* Internal (port-raised) exception at a try site: always an Exception kind
 * here; stored in e->cur for the except path. Returns 1. */
static int stx_internal_exc(stx_env *e, const char *kind, const char *msg) {
    if (!stx_is_exception_kind(kind)) {
        stx_raise(e, kind, msg);
        return 2;
    }
    json_t *d = json_object();
    json_set(d, "__raise__", json_string(kind));
    json_set(d, "message", json_string(msg ? msg : ""));
    if (e->cur) json_free(e->cur);
    e->cur = d;
    return 1;
}
static void stx_end_catch(stx_env *e) {
    if (e->cur) { json_free(e->cur); e->cur = NULL; }
}
static const char *stx_cur_msg(const stx_env *e) {
    return e->cur ? json_get_str(e->cur, "message", "") : "";
}
static void stx_attr_msg(char *buf, size_t cap, const char *type, const char *attr) {
    snprintf(buf, cap, "'%s' object has no attribute '%s'", type, attr);
}

/* ── state mirror access ──────────────────────────────────────────────────── */
static json_t *stx_sect(stx_env *e, const char *name) {
    return json_obj_get(e->state, name);
}
static json_t *stx_dirty(stx_env *e) { return stx_sect(e, "dirty_transcripts"); }
static json_t *stx_failures(stx_env *e) { return stx_sect(e, "append_failures"); }
static json_t *stx_entries(stx_env *e) { return stx_sect(e, "entries"); }
static bool stx_attr_present(stx_env *e, const char *name) {
    const json_t *v = stx_sect(e, name);
    return v && v->type != JSON_NULL;
}
/* _lazy(name, dict/set): create when missing/None. */
static json_t *stx_lazy_obj(stx_env *e, const char *name) {
    json_t *v = stx_sect(e, name);
    if (!v || v->type != JSON_OBJECT) {
        v = json_object();
        json_set(e->state, name, v);
    }
    return v;
}
static json_t *stx_lazy_set(stx_env *e, const char *name) {
    json_t *v = stx_sect(e, name);
    if (!v || v->type != JSON_ARRAY) {
        v = json_array();
        json_set(e->state, name, v);
    }
    return v;
}
/* Store doubles: a string handle registered in state.stores. */
static bool stx_is_store(stx_env *e, const json_t *v) {
    return stx_is_str(v) && v->str_val
        && json_has(stx_sect(e, "stores"), v->str_val);
}
/* `not store` on a handle: a registered store is an OBJECT (always truthy,
 * even for an empty-string id); anything else uses JSON truthiness. */
static bool stx_store_truthy(stx_env *e, const json_t *v) {
    return stx_is_store(e, v) || stx_truth(v);
}
/* `not row` on a sessions.get result: a dict projection materializes as a
 * types.SimpleNamespace OBJECT (always truthy, even {}); null is falsy and
 * raw non-dict values use JSON truthiness. */
static bool stx_row_truthy(const json_t *v) {
    if (stx_is_obj(v)) return true;
    return stx_truth(v);
}

/* ── log seam (raw fmt + args; stub emits {ev:"log",level,fmt,args[,exc_info]}) */
static void stx_log(stx_env *e, int level, const char *fmt, json_t *args,
                    bool exc_info) {
    json_t *a = json_object();
    json_set(a, "level", json_number(level));
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args ? args : json_array());   /* adopts */
    if (exc_info) json_set(a, "exc_info", json_bool(true));
    json_free(stx_call(e, "log", a));
}
static json_t *stx_log_args1(const json_t *v) {
    json_t *a = json_array();
    json_append(a, stx_copy_or_null(v));
    return a;
}
static json_t *stx_log_args2(const json_t *v, const char *s) {
    json_t *a = json_array();
    json_append(a, stx_copy_or_null(v));
    json_append(a, json_string(s ? s : ""));
    return a;
}

/* ── WindowsPath str() projection for the divert sessions_dir ────────────── */
static char *stx_sessions_dir(const char *home) {
    /* clean absolute "C:/..." domain: collapse separators, drop empty and
     * "." components, backslash join; "" -> "." -> "sessions". */
    char *norm = (char *)malloc(strlen(home) + 1);
    if (!norm) return strdup("sessions");
    size_t n = 0;
    const char *p = home;
    bool first = true;
    while (*p) {
        while (*p == '/' || *p == '\\') p++;
        if (!*p) break;
        const char *q = p;
        while (*q && *q != '/' && *q != '\\') q++;
        size_t len = (size_t)(q - p);
        if (!(len == 1 && p[0] == '.')) {
            if (!first) norm[n++] = '\\';
            memcpy(norm + n, p, len);
            n += len;
            first = false;
        }
        p = q;
    }
    norm[n] = '\0';
    char *out;
    if (n == 0) {
        out = strdup("sessions");
    } else {
        out = (char *)malloc(n + 16);
        if (out) sprintf(out, "%s\\sessions", norm);
        else out = strdup("sessions");
    }
    free(norm);
    return out;
}

/* ── shared leaves ────────────────────────────────────────────────────────── */

/* extract_api_content_sidecar (agent/turn_context.py, pure leaf inlined):
 * msg.get("api_content") when it is a str, else None. */
static json_t *stx_api_content_sidecar(const json_t *msg) {
    const json_t *v = stx_get(msg, "api_content");
    return stx_is_str(v) ? json_copy(v) : json_null();
}

/* _bindable_text core: None/str/float pass through; bool is an int subclass
 * and passes; an int outside sqlite's signed 64-bit range serializes via
 * json.dumps(default=str); containers serialize the same way. The
 * TypeError/ValueError -> str(value) fallback is unreachable for pure JSON
 * values (kept for fidelity). */
static json_t *stx_bindable_value(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_null();
    if (v->type == JSON_STRING || v->type == JSON_BOOL) return json_copy(v);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (stx_integral(d)) {
            if (d >= -9223372036854775808.0 && d < 9223372036854775808.0)
                return json_copy(v);
            char buf[64];
            snprintf(buf, sizeof(buf), "%.0f", d);
            return json_string(buf);
        }
        return json_copy(v);   /* float */
    }
    char *s = stx_dumps(v);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* _spool_dropped: import + call inside try; except Exception -> None.
 * Returns NULL for None OR a propagating BaseException (check e->exc). */
static json_t *stx_spool_dropped_fn(stx_env *e, const json_t *sid_v,
                                   const json_t *msg) {
    json_t *a = json_object();
    json_set(a, "session_id", stx_copy_or_null(sid_v));
    json_set(a, "message", stx_copy_or_null(msg));
    json_t *r = stx_call(e, "spool_dropped_transcript_message", a);
    int rc = stx_exc_class(e, r);
    if (rc == 2) { json_free(r); return NULL; }
    if (rc == 1) { stx_end_catch(e); json_free(r); return json_null(); }
    return r;
}

/* _follow_reroutes core: _lazy dict + cycle-guarded walk. */
static json_t *stx_follow_reroutes_fn(stx_env *e, const json_t *sid_v) {
    json_t *rr = stx_lazy_obj(e, "reroutes");
    json_t *cur = stx_copy_or_null(sid_v);
    json_t *seen = json_array();
    for (;;) {
        if (!stx_is_str(cur) || !cur->str_val) break;
        const json_t *nxt = json_obj_get(rr, cur->str_val);
        if (!nxt) break;
        if (stx_arr_has(seen, cur)) break;
        json_append(seen, json_copy(cur));
        json_free(cur);
        cur = json_copy(nxt);
    }
    json_free(seen);
    return cur;
}

/* ── 1. TranscriptReadError.__init__ ─────────────────────────────────────── */
/* PoP: stx_transcript_read_error_init @ gateway/session_transcript.py:TranscriptReadError.__init__ */
static json_t *stx_transcript_read_error_init(stx_env *e, const json_t *in) {
    (void)e;
    const json_t *sid = json_obj_get(in, "session_id");
    char *s = stx_pystr(sid);
    size_t n = strlen(s) + 64;
    char *msg = (char *)malloc(n);
    if (msg) snprintf(msg, n, "transcript read failed for session %s", s);
    json_t *d = json_object();
    json_set(d, "__raise__", json_string("TranscriptReadError"));
    json_set(d, "message", json_string(msg ? msg : ""));
    json_set(d, "session_id", stx_copy_or_null(sid));
    free(s); free(msg);
    return d;
}

/* ── 2. _spool_dropped ────────────────────────────────────────────────────── */
/* PoP: stx_spool_dropped @ gateway/session_transcript.py:_spool_dropped */
static json_t *stx_spool_dropped(stx_env *e, const json_t *in) {
    json_t *r = stx_spool_dropped_fn(e, json_obj_get(in, "session_id"),
                                     json_obj_get(in, "message"));
    if (e->exc) { json_free(r); return NULL; }
    return r ? r : json_null();
}

/* ── 3. _bindable_text ────────────────────────────────────────────────────── */
/* PoP: stx_bindable_text @ gateway/session_transcript.py:_bindable_text */
static json_t *stx_bindable_text(stx_env *e, const json_t *in) {
    (void)e;
    return stx_bindable_value(json_obj_get(in, "value"));
}

/* ── 4. transcript_append_kwargs ─────────────────────────────────────────── */
/* PoP: stx_transcript_append_kwargs @ gateway/session_transcript.py:transcript_append_kwargs */
static json_t *stx_transcript_append_kwargs(stx_env *e, const json_t *in) {
    (void)e;
    static const char *assistant_keys[] = {
        "reasoning", "reasoning_content", "reasoning_details",
        "codex_reasoning_items", "codex_message_items",
    };
    static const char *bound_text_keys[] = {
        "role", "tool_name", "tool_call_id", "reasoning", "reasoning_content",
        "platform_message_id",
    };
    const json_t *msg = json_obj_get(in, "message");
    if (!stx_is_obj(msg)) {
        char buf[128];
        stx_attr_msg(buf, sizeof(buf), stx_type_name(msg), "get");
        stx_raise(e, "AttributeError", buf);
        return NULL;
    }
    const json_t *role = stx_get(msg, "role");
    bool is_assistant = stx_is_str(role) && role->str_val
        && strcmp(role->str_val, "assistant") == 0;
    const json_t *ts = stx_get(msg, "timestamp");
    json_t *kw = json_object();
    json_set(kw, "session_id", stx_copy_or_null(json_obj_get(in, "session_id")));
    json_set(kw, "role", role ? json_copy(role) : json_string("unknown"));
    static const char *plain_keys[] = { "content", "tool_name", "tool_calls",
                                        "tool_call_id" };
    for (size_t i = 0; i < 4; i++)
        json_set(kw, plain_keys[i], stx_copy_or_null(stx_get(msg, plain_keys[i])));
    for (size_t i = 0; i < 5; i++)
        json_set(kw, assistant_keys[i],
                 is_assistant ? stx_copy_or_null(stx_get(msg, assistant_keys[i]))
                              : json_null());
    const json_t *pmid = stx_get(msg, "platform_message_id");
    if (!stx_truth(pmid)) pmid = stx_get(msg, "message_id");
    json_set(kw, "platform_message_id", stx_copy_or_null(pmid));
    json_set(kw, "observed", json_bool(stx_truth(stx_get(msg, "observed"))));
    json_set(kw, "timestamp",
             stx_is_null(ts) ? stx_copy_or_null(json_obj_get(in, "fallback_ts"))
                             : json_copy(ts));
    json_set(kw, "api_content", stx_api_content_sidecar(msg));
    json_set(kw, "display_kind", stx_copy_or_null(stx_get(msg, "display_kind")));
    json_set(kw, "display_metadata",
             stx_copy_or_null(stx_get(msg, "display_metadata")));
    for (size_t i = 0; i < 6; i++)
        json_set(kw, bound_text_keys[i],
                 stx_bindable_value(json_obj_get(kw, bound_text_keys[i])));
    return kw;
}

/* ── 5. _follow_reroutes ─────────────────────────────────────────────────── */
/* PoP: stx_follow_reroutes @ gateway/session_transcript.py:_follow_reroutes */
static json_t *stx_follow_reroutes(stx_env *e, const json_t *in) {
    return stx_follow_reroutes_fn(e, json_obj_get(in, "session_id"));
}

/* ── 6. _enqueue_transcript_message ──────────────────────────────────────── */
/* PoP: stx_enqueue_transcript_message @ gateway/session_transcript.py:_enqueue_transcript_message */
static json_t *stx_enqueue_transcript_message(stx_env *e, const json_t *in) {
    const char *sid = json_get_str(in, "session_id", "");
    const json_t *msg = json_obj_get(in, "message");
    double cap = json_get_num(in, "cap", 200);
    json_t *dirty = stx_dirty(e);
    json_t *pending = json_obj_get(dirty, sid);
    if (!pending) {
        pending = json_array();
        json_set(dirty, sid, pending);
    }
    if (!stx_is_obj(msg)) {
        /* dict(message) on a non-mapping: fixture domain keeps messages
         * dicts; fail closed with the not-iterable TypeError otherwise. */
        char buf[128];
        snprintf(buf, sizeof(buf), "'%s' object is not iterable",
                 stx_type_name(msg));
        stx_raise(e, "TypeError", buf);
        return NULL;
    }
    json_append(pending, json_copy(msg));
    if ((double)pending->c.count > cap) {
        json_t *oldest = pending->c.items[0];
        /* detach the oldest BEFORE the spool call (pending.pop(0)) */
        for (size_t j = 0; j + 1 < pending->c.count; j++)
            pending->c.items[j] = pending->c.items[j + 1];
        pending->c.count--;
        json_t *sid_v = json_string(sid);
        json_t *sp = stx_spool_dropped_fn(e, sid_v, oldest);
        json_free(oldest);
        json_free(sid_v);
        if (e->exc) { json_free(sp); return NULL; }
        if (!stx_is_null(sp)) {
            stx_arr_add_once(stx_lazy_set(e, "spooled_drop_sessions"),
                             json_string(sid));
            json_t *args = json_array();
            json_append(args, json_string(sid));
            json_append(args, json_number(cap));
            json_append(args, json_copy(sp));
            stx_log(e, 30,
                    "Session DB transcript pending queue full for %s (cap=%d); "
                    "spooled oldest message to %s for replay after DB recovery",
                    args, false);
        } else {
            json_t *args = json_array();
            json_append(args, json_string(sid));
            json_append(args, json_number(cap));
            stx_log(e, 30,
                    "Session DB transcript pending queue full for %s (cap=%d); "
                    "dropping oldest message to make room (on-disk spool "
                    "unavailable)",
                    args, false);
        }
        json_free(sp);
    }
    return json_copy(pending);
}

/* ── 7. _divert_transcript_after_db_replaced ─────────────────────────────── */
/* PoP: stx_divert_transcript_after_db_replaced @ gateway/session_transcript.py:_divert_transcript_after_db_replaced */
static json_t *stx_divert_transcript_after_db_replaced(stx_env *e,
                                                       const json_t *in) {
    const char *sid = json_get_str(in, "session_id", "");
    const char *qsid = json_get_str(in, "queue_session_id", "");
    const json_t *exc = json_obj_get(in, "exc");
    const char *kind = json_get_str(exc, "__raise__", "Exception");
    const char *emsg = json_get_str(exc, "message", "");
    json_t *args = json_array();
    json_append(args, json_string(sid));
    json_append(args, json_string(kind));
    json_append(args, json_string(emsg));
    stx_log(e, 40,
            "Session DB refused further writes on this handle for %s (%s); "
            "stopping SQLite writes and diverting pending transcripts to the "
            "on-disk fallback: %s",
            args, false);
    /* retry lock held (inert): detach the queue + failure counter */
    json_t *dirty = stx_dirty(e);
    json_t *remaining = json_copy(stx_get(dirty, qsid));
    if (!remaining) remaining = json_array();
    json_obj_del(dirty, qsid);
    json_obj_del(stx_failures(e), sid);
    json_t *sid_v = json_string(sid);
    for (size_t i = 0; i < remaining->c.count; i++) {
        json_t *sp = stx_spool_dropped_fn(e, sid_v, remaining->c.items[i]);
        if (e->exc) {
            json_free(sp); json_free(sid_v); json_free(remaining);
            return NULL;
        }
        if (stx_is_null(sp))
            stx_log(e, 30,
                    "pending fallback failed for replaced state.db transcript "
                    "on %s",
                    stx_log_args1(sid_v), true);
        json_free(sp);
    }
    /* try: JSONL divert (imports stubbed; failures stay best-effort) */
    json_t *home = stx_call(e, "get_hermes_home", json_object());
    int rc = stx_exc_class(e, home);
    if (rc == 2) {
        json_free(home); json_free(sid_v); json_free(remaining);
        return NULL;
    }
    if (rc == 0) {
        if (!stx_is_str(home)) {
            char buf[160];
            snprintf(buf, sizeof(buf),
                     "unsupported operand type(s) for /: '%s' and 'str'",
                     stx_type_name(home));
            stx_internal_exc(e, "TypeError", buf);
            rc = 1;
        } else {
            char *dir = stx_sessions_dir(home->str_val ? home->str_val : "");
            json_t *a = json_object();
            json_set(a, "session_id", json_string(sid));
            json_set(a, "remaining", json_copy(remaining));
            json_set(a, "sessions_dir", json_string(dir));
            free(dir);
            json_t *r2 = stx_call(e, "divert_session_transcript_jsonl", a);
            rc = stx_exc_class(e, r2);
            json_free(r2);
        }
    }
    if (rc == 2) {
        json_free(home); json_free(sid_v); json_free(remaining);
        return NULL;
    }
    if (rc == 1) {
        stx_log(e, 30,
                "JSONL divert failed for replaced state.db transcript on %s",
                stx_log_args1(sid_v), true);
        stx_end_catch(e);
    }
    json_free(home);
    json_free(sid_v);
    json_free(remaining);
    return json_null();
}

/* ── 8. _live_compression_child ──────────────────────────────────────────── */
/* PoP: stx_live_compression_child @ gateway/session_transcript.py:_live_compression_child */
static json_t *stx_live_compression_child(stx_env *e, const json_t *in) {
    const json_t *sid_v = json_obj_get(in, "session_id");
    json_t *a = json_object();
    json_set(a, "session_id", stx_copy_or_null(sid_v));
    json_t *owner = stx_call(e, "persistence_for_session_id", a);
    if (stx_prop(e, owner)) return NULL;
    json_t *tip = NULL;
    if (!stx_is_null(owner)) {
        if (!stx_is_store(e, owner)) {
            char buf[128];
            stx_attr_msg(buf, sizeof(buf), stx_type_name(owner), "lineage");
            stx_raise(e, "AttributeError", buf);
            json_free(owner);
            return NULL;
        }
        a = json_object();
        json_set(a, "store", json_copy(owner));
        json_set(a, "session_id", stx_copy_or_null(sid_v));
        tip = stx_call(e, "lineage.tip", a);
        if (stx_prop(e, tip)) { json_free(owner); return NULL; }
    }
    json_t *ret = json_string("");
    if (stx_truth(tip) && !stx_json_eq(tip, sid_v)) {
        a = json_object();
        json_set(a, "store", json_copy(owner));
        json_set(a, "session_id", json_copy(tip));
        json_t *row = stx_call(e, "sessions.get", a);
        if (stx_prop(e, row)) {
            json_free(owner); json_free(tip); json_free(ret);
            return NULL;
        }
        if (!stx_is_null(row)) {
            char buf[128];
            if (!stx_is_obj(row)) {
                stx_attr_msg(buf, sizeof(buf), stx_type_name(row), "ended_at");
                stx_raise(e, "AttributeError", buf);
                json_free(row); json_free(owner); json_free(tip); json_free(ret);
                return NULL;
            }
            if (!json_has(row, "ended_at")) {
                stx_attr_msg(buf, sizeof(buf), "types.SimpleNamespace", "ended_at");
                stx_raise(e, "AttributeError", buf);
                json_free(row); json_free(owner); json_free(tip); json_free(ret);
                return NULL;
            }
            if (stx_is_null(json_obj_get(row, "ended_at"))) {
                char *s = stx_pystr(tip);
                json_free(ret);
                ret = json_string(s);
                free(s);
            }
        }
        json_free(row);
    }
    json_free(owner);
    json_free(tip);
    return ret;
}

/* ── 9. _migrate_transcript_queue_to_child ───────────────────────────────── */
/* PoP: stx_migrate_transcript_queue_to_child @ gateway/session_transcript.py:_migrate_transcript_queue_to_child */
static json_t *stx_migrate_transcript_queue_to_child(stx_env *e,
                                                     const json_t *in) {
    const char *sid = json_get_str(in, "session_id", "");
    const char *qsid = json_get_str(in, "queue_session_id", "");
    const char *child = json_get_str(in, "child_id", "");
    bool head = json_get_bool(in, "msg_is_head", false);
    json_t *dirty = stx_dirty(e);
    /* the harness seeded dirty[qsid] with in["pending"] (the caller's list) */
    json_t *pending = json_obj_get(dirty, qsid);
    if (pending && pending->c.count > 0 && head) stx_arr_pop0(pending);
    json_t *existing = json_obj_get(dirty, child);
    bool existing_nonempty = stx_is_arr(existing) && existing->c.count > 0;
    json_t *ret = NULL;
    if (pending && pending->c.count > 0) {
        if (existing_nonempty) {
            /* snapshot the count: child == queue extends the list with itself */
            size_t n = existing->c.count;
            for (size_t i = 0; i < n; i++)
                json_append(pending, json_copy(existing->c.items[i]));
        }
        json_t *P = stx_obj_take(dirty, qsid);
        json_set(dirty, child, P);   /* replaces (frees) any old child list */
        if (strcmp(child, qsid) == 0) {
            /* live sets dirty[child] then pops dirty[queue]: same key -> gone */
            ret = stx_obj_take(dirty, child);
        } else {
            ret = json_copy(P);
        }
    } else {
        if (existing_nonempty) ret = json_copy(existing);
        else ret = json_array();
        json_obj_del(dirty, qsid);
    }
    json_t *failures = stx_failures(e);
    double previous = json_get_num(failures, qsid, 0);
    json_obj_del(failures, qsid);
    if (previous != 0) {
        double child_prev = json_get_num(failures, child, 0);
        json_set(failures, child,
                 json_number(previous > child_prev ? previous : child_prev));
    }
    /* direct attribute access LAST: partial mutations above survive a raise */
    if (!stx_attr_present(e, "reroutes")) {
        stx_raise(e, "AttributeError",
                  "'StoreDouble' object has no attribute '_transcript_reroutes'");
        json_free(ret);
        return NULL;
    }
    json_set(stx_sect(e, "reroutes"), sid, json_string(child));
    return ret;
}

/* ── 10. _publish_transcript_reroute ─────────────────────────────────────── */
/* PoP: stx_publish_transcript_reroute @ gateway/session_transcript.py:_publish_transcript_reroute */
static json_t *stx_publish_transcript_reroute(stx_env *e, const json_t *in) {
    const json_t *sid_v = json_obj_get(in, "session_id");
    const char *child = json_get_str(in, "child_id", "");
    json_t *entries = stx_entries(e);
    if (stx_is_obj(entries)) {
        for (size_t i = 0; i < entries->c.count; i++) {
            json_t *entry = entries->c.items[i];
            char buf[128];
            if (!stx_is_obj(entry)) {
                stx_attr_msg(buf, sizeof(buf), stx_type_name(entry),
                             "session_id");
                stx_raise(e, "AttributeError", buf);
                return NULL;
            }
            if (!json_has(entry, "session_id")) {
                stx_attr_msg(buf, sizeof(buf), "types.SimpleNamespace", "session_id");
                stx_raise(e, "AttributeError", buf);
                return NULL;
            }
            if (stx_json_eq(json_obj_get(entry, "session_id"), sid_v))
                json_set(entry, "session_id", json_string(child));
        }
    }
    json_t *r = stx_call(e, "save", json_object());
    if (stx_prop(e, r)) return NULL;
    json_free(r);
    /* (getattr(self, "_session_owner_hints", None) or {}).pop(child, None) */
    json_t *hints = stx_sect(e, "session_owner_hints");
    if (stx_truth(hints) && stx_is_obj(hints)) json_obj_del(hints, child);
    return json_null();
}

/* ── 11. _spool_stalled_backlog ──────────────────────────────────────────── */
/* PoP: stx_spool_stalled_backlog @ gateway/session_transcript.py:_spool_stalled_backlog */
static json_t *stx_spool_stalled_backlog(stx_env *e, const json_t *in) {
    const char *sid = json_get_str(in, "session_id", "");
    const char *qsid = json_get_str(in, "queue_session_id", "");
    json_t *dirty = stx_dirty(e);
    json_t *pending = json_obj_get(dirty, qsid);
    json_t *backlog = pending ? json_copy(pending) : json_array();
    double spooled = 0;
    json_t *sid_v = json_string(sid);
    for (size_t i = 0; i < backlog->c.count; i++) {
        json_t *sp = stx_spool_dropped_fn(e, sid_v, backlog->c.items[i]);
        if (e->exc) {
            json_free(sp); json_free(sid_v); json_free(backlog);
            return NULL;
        }
        if (stx_is_null(sp)) { json_free(sp); break; }
        spooled += 1;
        json_free(sp);
    }
    if (spooled > 0) {
        stx_arr_add_once(stx_lazy_set(e, "spooled_drop_sessions"), sid_v);
        /* del pending[:spooled] on the STATE list */
        for (double k = 0; k < spooled; k++) stx_arr_pop0(pending);
        if (pending->c.count == 0) json_obj_del(dirty, qsid);
    }
    json_free(sid_v);
    json_free(backlog);
    return json_number(spooled);
}

/* ── 12. mark_spooled_drop_sessions ──────────────────────────────────────── */
/* PoP: stx_mark_spooled_drop_sessions @ gateway/session_transcript.py:mark_spooled_drop_sessions */
static json_t *stx_mark_spooled_drop_sessions(stx_env *e, const json_t *in) {
    const json_t *ids = json_obj_get(in, "session_ids");
    if (!stx_truth(ids)) return json_null();
    json_t *set = stx_lazy_set(e, "spooled_drop_sessions");
    if (stx_is_arr(ids)) {
        for (size_t i = 0; i < ids->c.count; i++)
            stx_arr_add_once(set, ids->c.items[i]);
    } else if (stx_is_str(ids)) {
        /* set.update(str) iterates characters (ASCII fixture domain) */
        const char *s = ids->str_val ? ids->str_val : "";
        for (const char *p = s; *p; p++) {
            char ch[2] = { *p, '\0' };
            stx_arr_add_once(set, json_string(ch));
        }
    }
    return json_null();
}

/* ── 13. has_platform_message_id ─────────────────────────────────────────── */
/* PoP: stx_has_platform_message_id @ gateway/session_transcript.py:has_platform_message_id */
static json_t *stx_has_platform_message_id(stx_env *e, const json_t *in) {
    const json_t *sid_v = json_obj_get(in, "session_id");
    json_t *a = json_object();
    json_set(a, "session_id", stx_copy_or_null(sid_v));
    json_t *store = stx_call(e, "persistence_for_session_id", a);
    if (stx_prop(e, store)) return NULL;
    if (!stx_store_truthy(e, store)) { json_free(store); return json_bool(false); }
    json_t *ret = NULL;
    int rc = 0;
    if (!stx_is_store(e, store)) {
        char buf[128];
        stx_attr_msg(buf, sizeof(buf), stx_type_name(store), "messages");
        rc = stx_internal_exc(e, "AttributeError", buf);
    } else {
        a = json_object();
        json_set(a, "store", json_copy(store));
        json_set(a, "session_id", stx_copy_or_null(sid_v));
        json_set(a, "platform_message_id",
                 stx_copy_or_null(json_obj_get(in, "platform_message_id")));
        ret = stx_call(e, "messages.has_platform_message_id", a);
        rc = stx_exc_class(e, ret);
        if (rc == 1) { json_free(ret); ret = NULL; }
    }
    if (rc == 2) { json_free(store); json_free(ret); return NULL; }
    if (rc == 1) {
        stx_log(e, 10, "has_platform_message_id lookup failed",
                json_array(), true);
        stx_end_catch(e);
        json_free(store);
        return json_bool(false);
    }
    json_free(store);
    return ret ? ret : json_null();
}

/* ── 14. transcript_tail_role ────────────────────────────────────────────── */
/* PoP: stx_transcript_tail_role @ gateway/session_transcript.py:transcript_tail_role */
static json_t *stx_transcript_tail_role(stx_env *e, const json_t *in) {
    json_t *cur = stx_follow_reroutes_fn(e, json_obj_get(in, "session_id"));
    json_t *a = json_object();
    json_set(a, "session_id", json_copy(cur));
    json_t *tip = stx_call(e, "compression_tip_for_session_id", a);
    if (stx_prop(e, tip)) { json_free(cur); return NULL; }
    json_free(cur);
    a = json_object();
    json_set(a, "session_id", stx_copy_or_null(tip));
    json_t *store = stx_call(e, "persistence_for_session_id", a);
    if (stx_prop(e, store)) { json_free(tip); return NULL; }
    if (!stx_store_truthy(e, store)) {
        json_free(store); json_free(tip);
        return json_null();
    }
    json_t *ret = NULL;
    int rc = 0;
    if (!stx_is_store(e, store)) {
        char buf[128];
        stx_attr_msg(buf, sizeof(buf), stx_type_name(store), "messages");
        rc = stx_internal_exc(e, "AttributeError", buf);
    } else {
        a = json_object();
        json_set(a, "store", json_copy(store));
        json_set(a, "session_id", stx_copy_or_null(tip));
        ret = stx_call(e, "messages.latest_conversation_role", a);
        rc = stx_exc_class(e, ret);
        if (rc == 1) { json_free(ret); ret = NULL; }
    }
    if (rc == 2) { json_free(store); json_free(tip); json_free(ret); return NULL; }
    if (rc == 1) {
        stx_log(e, 10, "transcript tail lookup failed for %s",
                stx_log_args1(tip), true);
        stx_end_catch(e);
        json_free(store); json_free(tip);
        return json_null();
    }
    json_free(store); json_free(tip);
    return ret ? ret : json_null();
}

/* ── 15. is_published_compression_child ──────────────────────────────────── */
/* PoP: stx_is_published_compression_child @ gateway/session_transcript.py:is_published_compression_child */
static json_t *stx_is_published_compression_child(stx_env *e, const json_t *in) {
    const json_t *p_v = json_obj_get(in, "parent_session_id");
    const json_t *c_v = json_obj_get(in, "child_session_id");
    if (!stx_truth(p_v) || !stx_truth(c_v)) return json_bool(false);
    json_t *a = json_object();
    json_set(a, "session_id", json_copy(p_v));
    json_t *store = stx_call(e, "persistence_for_session_id", a);
    if (stx_prop(e, store)) return NULL;
    if (stx_is_null(store)) { json_free(store); return json_bool(false); }
    json_t *ret = json_bool(false);
    int rc = 0;
    json_t *child = NULL, *r = NULL, *w = NULL;
    char buf[160];
    if (!stx_is_store(e, store)) {
        stx_attr_msg(buf, sizeof(buf), stx_type_name(store), "sessions");
        rc = stx_internal_exc(e, "AttributeError", buf);
        goto done;
    }
    a = json_object();
    json_set(a, "store", json_copy(store));
    json_set(a, "session_id", json_copy(c_v));
    child = stx_call(e, "sessions.get", a);
    rc = stx_exc_class(e, child);
    if (rc) { if (rc == 1) { json_free(child); child = NULL; } goto done; }
    if (!stx_row_truthy(child)) goto done;
    if (!stx_is_obj(child)) {
        stx_attr_msg(buf, sizeof(buf), stx_type_name(child), "parent_session_id");
        rc = stx_internal_exc(e, "AttributeError", buf);
        goto done;
    }
    if (!json_has(child, "parent_session_id")) {
        stx_attr_msg(buf, sizeof(buf), "types.SimpleNamespace", "parent_session_id");
        rc = stx_internal_exc(e, "AttributeError", buf);
        goto done;
    }
    if (!stx_json_eq(json_obj_get(child, "parent_session_id"), p_v)) goto done;
    a = json_object();
    json_set(a, "store", json_copy(store));
    json_set(a, "child", json_copy(child));
    r = stx_call(e, "lineage.is_compression_child", a);
    rc = stx_exc_class(e, r);
    if (rc) { if (rc == 1) { json_free(r); r = NULL; } goto done; }
    if (!stx_truth(r)) goto done;
    a = json_object();
    json_set(a, "store", json_copy(store));
    json_set(a, "session_id", json_copy(c_v));
    w = stx_call(e, "messages.active_watermark", a);
    rc = stx_exc_class(e, w);
    if (rc) { if (rc == 1) { json_free(w); w = NULL; } goto done; }
    if (stx_is_null(w)) {
        stx_attr_msg(buf, sizeof(buf), "NoneType", "last_active");
        rc = stx_internal_exc(e, "AttributeError", buf);
        goto done;
    }
    if (!stx_is_obj(w)) {
        stx_attr_msg(buf, sizeof(buf), stx_type_name(w), "last_active");
        rc = stx_internal_exc(e, "AttributeError", buf);
        goto done;
    }
    if (!json_has(w, "last_active")) {
        stx_attr_msg(buf, sizeof(buf), "types.SimpleNamespace", "last_active");
        rc = stx_internal_exc(e, "AttributeError", buf);
        goto done;
    }
    json_free(ret);
    ret = json_bool(!stx_is_null(json_obj_get(w, "last_active")));
done:
    if (rc == 2) {
        json_free(ret); json_free(child); json_free(r); json_free(w);
        json_free(store);
        return NULL;
    }
    if (rc == 1) {
        stx_log(e, 10, "published-compression-child probe failed for %s",
                stx_log_args1(c_v), true);
        stx_end_catch(e);
        json_free(ret);
        ret = json_bool(false);
    }
    json_free(child); json_free(r); json_free(w); json_free(store);
    return ret;
}

/* ── 16. persist_rotated_compression_child ───────────────────────────────── */
/* PoP: stx_persist_rotated_compression_child @ gateway/session_transcript.py:persist_rotated_compression_child */
static json_t *stx_persist_rotated_compression_child(stx_env *e,
                                                     const json_t *in) {
    json_t *r = stx_is_published_compression_child(e, in);
    if (e->exc) { json_free(r); return NULL; }
    if (stx_truth(r)) return r;
    json_free(r);
    json_t *a = json_object();
    json_set(a, "session_id",
             stx_copy_or_null(json_obj_get(in, "child_session_id")));
    json_set(a, "messages", stx_copy_or_null(json_obj_get(in, "messages")));
    json_set(a, "active_only", json_bool(false));
    json_set(a, "reject_active_turn_lease", json_bool(false));
    json_t *rw = stx_call(e, "rewrite_transcript", a);
    if (stx_prop(e, rw)) return NULL;
    return rw ? rw : json_null();
}

/* ── 17. has_input_owner ─────────────────────────────────────────────────── */
/* PoP: stx_has_input_owner @ gateway/session_transcript.py:has_input_owner */
static json_t *stx_has_input_owner(stx_env *e, const json_t *in) {
    const json_t *sid_v = json_obj_get(in, "session_id");
    const json_t *owner_v = json_obj_get(in, "owner");
    json_t *cur = NULL, *store = NULL, *t = NULL, *r = NULL, *row = NULL;
    json_t *seen = json_array();
    json_t *ret = NULL;
    int rc = 0;
    char buf[160];
    cur = stx_follow_reroutes_fn(e, sid_v);
    json_t *a = json_object();
    json_set(a, "session_id", json_copy(cur));
    store = stx_call(e, "persistence_for_session_id", a);
    rc = stx_exc_class(e, store);
    if (rc) goto done;
    if (!stx_is_store(e, store)) {
        stx_attr_msg(buf, sizeof(buf), stx_type_name(store), "lineage");
        rc = stx_internal_exc(e, "AttributeError", buf);
        goto done;
    }
    a = json_object();
    json_set(a, "store", json_copy(store));
    json_set(a, "session_id", json_copy(cur));
    t = stx_call(e, "lineage.tip", a);
    rc = stx_exc_class(e, t);
    if (rc) goto done;
    if (stx_truth(t)) { json_free(cur); cur = json_copy(t); }
    while (stx_truth(cur) && !stx_arr_has(seen, cur)) {
        json_append(seen, json_copy(cur));
        a = json_object();
        json_set(a, "store", json_copy(store));
        json_set(a, "session_id", json_copy(cur));
        json_set(a, "owner", stx_copy_or_null(owner_v));
        r = stx_call(e, "messages.has_gateway_input_owner", a);
        rc = stx_exc_class(e, r);
        if (rc) goto done;
        if (stx_truth(r)) { ret = json_bool(true); goto done; }
        json_free(r); r = NULL;
        a = json_object();
        json_set(a, "store", json_copy(store));
        json_set(a, "session_id", json_copy(cur));
        row = stx_call(e, "sessions.get", a);
        rc = stx_exc_class(e, row);
        if (rc) goto done;
        if (!stx_row_truthy(row)) break;
        a = json_object();
        json_set(a, "store", json_copy(store));
        json_set(a, "child", json_copy(row));
        json_t *ic = stx_call(e, "lineage.is_compression_child", a);
        rc = stx_exc_class(e, ic);
        if (rc) { json_free(ic); goto done; }
        bool is_child = stx_truth(ic);
        json_free(ic);
        if (!is_child) break;
        if (!stx_is_obj(row)) {
            stx_attr_msg(buf, sizeof(buf), stx_type_name(row),
                         "parent_session_id");
            rc = stx_internal_exc(e, "AttributeError", buf);
            goto done;
        }
        if (!json_has(row, "parent_session_id")) {
            stx_attr_msg(buf, sizeof(buf), "types.SimpleNamespace",
                         "parent_session_id");
            rc = stx_internal_exc(e, "AttributeError", buf);
            goto done;
        }
        json_free(cur);
        cur = json_copy(json_obj_get(row, "parent_session_id"));
        json_free(row); row = NULL;
    }
    ret = json_bool(false);
done:
    if (rc == 2) {
        json_free(cur); json_free(store); json_free(t); json_free(r);
        json_free(row); json_free(seen); json_free(ret);
        return NULL;
    }
    if (rc == 1) {
        stx_end_catch(e);
        json_free(ret);
        stx_raise_transcript(e, sid_v);
        json_free(cur); json_free(store); json_free(t); json_free(r);
        json_free(row); json_free(seen);
        return NULL;
    }
    json_free(cur); json_free(store); json_free(t); json_free(r);
    json_free(row); json_free(seen);
    return ret ? ret : json_null();
}

/* ── 18. load_transcript ─────────────────────────────────────────────────── */
/* PoP: stx_load_transcript @ gateway/session_transcript.py:load_transcript */
static json_t *stx_load_transcript(stx_env *e, const json_t *in) {
    const json_t *sid_v = json_obj_get(in, "session_id");
    json_t *a = json_object();
    json_set(a, "session_id", stx_copy_or_null(sid_v));
    json_t *probe = stx_call(e, "persistence_for_session_id", a);
    if (stx_prop(e, probe)) return NULL;
    if (!stx_store_truthy(e, probe)) { json_free(probe); return json_array(); }
    json_free(probe);
    json_t *cur = stx_follow_reroutes_fn(e, sid_v);
    /* with contextlib.suppress(Exception): durable tip heal */
    a = json_object();
    json_set(a, "session_id", json_copy(cur));
    json_t *store1 = stx_call(e, "persistence_for_session_id", a);
    int rc = stx_exc_class(e, store1);
    if (rc == 2) { json_free(store1); json_free(cur); return NULL; }
    if (rc == 0) {
        if (stx_is_store(e, store1)) {
            a = json_object();
            json_set(a, "store", json_copy(store1));
            json_set(a, "session_id", json_copy(cur));
            json_t *t = stx_call(e, "lineage.tip", a);
            int rc2 = stx_exc_class(e, t);
            if (rc2 == 2) {
                json_free(t); json_free(cur); json_free(store1);
                return NULL;
            }
            if (rc2 == 0) {
                if (stx_truth(t)) { json_free(cur); cur = json_copy(t); }
            } else {
                stx_end_catch(e);   /* suppressed */
            }
            json_free(t);
        }
        /* non-store handle: AttributeError on .lineage, suppressed */
        json_free(store1);
    } else {
        stx_end_catch(e);   /* suppressed */
        json_free(store1);
    }
    /* try: canonical read (fail closed, never []) */
    json_t *ret = NULL, *rows = NULL, *store2 = NULL;
    rc = 0;
    a = json_object();
    json_set(a, "session_id", json_copy(cur));
    store2 = stx_call(e, "persistence_for_session_id", a);
    rc = stx_exc_class(e, store2);
    if (rc == 2) { json_free(cur); json_free(store2); return NULL; }
    if (rc == 1) goto fail;
    if (!stx_is_store(e, store2)) {
        char buf[128];
        stx_attr_msg(buf, sizeof(buf), stx_type_name(store2), "messages");
        stx_internal_exc(e, "AttributeError", buf);
        goto fail;
    }
    a = json_object();
    json_set(a, "store", json_copy(store2));
    json_set(a, "session_id", json_copy(cur));
    json_set(a, "repair_alternation", json_bool(true));
    rows = stx_call(e, "messages.read_conversation", a);
    rc = stx_exc_class(e, rows);
    if (rc == 2) { json_free(cur); json_free(store2); json_free(rows); return NULL; }
    if (rc == 1) goto fail;
    if (!stx_is_arr(rows)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "'%s' object is not iterable",
                 stx_type_name(rows));
        stx_internal_exc(e, "TypeError", buf);
        goto fail;
    }
    ret = json_array();
    for (size_t i = 0; i < rows->c.count; i++) {
        a = json_object();
        json_set(a, "message", json_copy(rows->c.items[i]));
        json_t *m = stx_call(e, "as_openai_message", a);
        rc = stx_exc_class(e, m);
        if (rc == 2) {
            json_free(m);
            json_free(cur); json_free(store2); json_free(rows); json_free(ret);
            return NULL;
        }
        if (rc == 1) { json_free(m); json_free(ret); ret = NULL; goto fail; }
        json_append(ret, m ? m : json_null());
    }
    json_free(rows); json_free(store2); json_free(cur);
    return ret;
fail:
    stx_log(e, 40,
            "Transcript read failed for session %s; refusing to treat the "
            "conversation as empty: %s",
            stx_log_args2(cur, stx_cur_msg(e)),
            true);
    stx_end_catch(e);
    stx_raise_transcript(e, cur);
    json_free(ret); json_free(rows); json_free(store2); json_free(cur);
    return NULL;
}

/* ── 19. retain_refused_turn_for_display ─────────────────────────────────── */
/* PoP: stx_retain_refused_turn_for_display @ gateway/session_transcript.py:retain_refused_turn_for_display */
static json_t *stx_retain_refused_turn_for_display(stx_env *e, const json_t *in) {
    const json_t *sid_v = json_obj_get(in, "session_id");
    const char *sid = json_get_str(in, "session_id", "");
    json_t *a = json_object();
    json_set(a, "session_id", stx_copy_or_null(sid_v));
    json_t *store = stx_call(e, "persistence_for_session_id", a);
    if (stx_prop(e, store)) return NULL;
    if (!stx_store_truthy(e, store)) { json_free(store); return json_null(); }
    /* claimed sibling: drain-lock acquisition is a seam */
    json_t *lock = stx_call(e, "get_transcript_drain_lock", json_object());
    if (stx_prop(e, lock)) { json_free(store); return NULL; }
    json_free(lock);
    json_t *ret = NULL;
    int rc = 0;
    if (!stx_is_store(e, store)) {
        char buf[128];
        stx_attr_msg(buf, sizeof(buf), stx_type_name(store), "messages");
        rc = stx_internal_exc(e, "AttributeError", buf);
    } else {
        a = json_object();
        json_set(a, "store", json_copy(store));
        json_set(a, "session_id", stx_copy_or_null(sid_v));
        json_set(a, "notice", stx_copy_or_null(json_obj_get(in, "notice")));
        json_set(a, "boundary_row_id",
                 stx_copy_or_null(json_obj_get(in, "boundary_row_id")));
        ret = stx_call(e, "messages.retain_refused_turn_for_display", a);
        rc = stx_exc_class(e, ret);
        if (rc == 1) { json_free(ret); ret = NULL; }
    }
    if (rc == 2) { json_free(store); json_free(ret); return NULL; }
    if (rc == 1) {
        json_t *la = json_array();
        json_append(la, json_string(stx_cur_msg(e)));
        stx_log(e, 10, "retain_refused_turn_for_display: failed: %s", la, false);
        stx_end_catch(e);
        json_free(store);
        return json_null();
    }
    if (!stx_truth(ret)) {
        json_free(ret); json_free(store);
        return json_null();
    }
    json_free(ret);
    /* claimed sibling _clear_dirty_transcript: seam + known queue-drop effect */
    a = json_object();
    json_set(a, "session_id", json_string(sid));
    json_t *c = stx_call(e, "clear_dirty_transcript", a);
    if (stx_prop(e, c)) { json_free(store); return NULL; }
    json_free(c);
    json_obj_del(stx_dirty(e), sid);
    json_obj_del(stx_failures(e), sid);
    json_free(store);
    json_t *out = json_object();
    json_set(out, "retained", json_bool(true));
    return out;
}