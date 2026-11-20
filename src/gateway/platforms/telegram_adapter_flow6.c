/*
 * telegram_adapter_flow6.c - Telegram adapter FLOW part 6 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): the inbound update
 * normalization, entity/mention addressing and error-classification decision
 * surface left UNCLAIMED after telegram_adapter_flow{,2,3}.c.  Ported targets
 * (all PoP-claimed here, all confirmed UNCLAIMED before this file was written):
 *   _normalize_platform_event, _normalize_reaction_event,
 *   _normalize_message_edited_event, _telegram_entity_text, _entity_span,
 *   _chat_id_str, _is_group_chat, _extract_bot_mention_usernames,
 *   _is_guest_mention, _bot_sender_suppressed, _clean_bot_trigger_text,
 *   _current_bot_username, _is_transient_typing_error,
 *   _dm_topic_missing_anchor_error, _telegram_require_mention,
 *   _telegram_bots_require_mention, _telegram_exclusive_bot_mentions.
 *
 * Decision sequences only: every live I/O / injected callable enters through
 * ONE universal seam: ta6_call(ctx, name, args).  The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording stubs emitting the identical event shape {"name","args","ret"}
 * from the SAME scripts.  Claimed siblings / unported collaborators are
 * seamed; pure leaves are transcribed inline (no seam, no PoP re-claim).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and ta6_-prefixed.
 *
 * SEAM NAME SET (must equal oracle SEAM_NAMES exactly):
 *   _normalize_reaction_event, _normalize_message_edited_event, _is_id_like,
 *   isoformat, _telegram_entity_text, _chat_type_str, _entity_sources,
 *   _entity_type, _entity_span, _telegram_guest_mode, _message_mentions_bot,
 *   _telegram_bots_require_mention, _sender_is_other_bot,
 *   clean_bot_trigger_text, _current_bot_username, _extra_bool.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow6.py):
 * - NULL json_t* == Python None / absent; a JSON_NULL node is a real null
 *   VALUE.  Exception specs: {"__raise__": cls, "message": str(exc)}.
 * - Adapter state (in.state) carries the attribute-double fields; a DIRECT
 *   attribute read (self._bot, ...) raises AttributeError
 *   "'TelegramAdapter' object has no attribute '<k>'" when ABSENT; present-null
 *   is a real None.  getattr(self, k, default) reads json_obj_get with default.
 *   Nested doubles (update/message/chat/entity/edit_date/bot) are plain
 *   attribute doubles (SimpleNamespace on the oracle side), so their reads use
 *   "types.SimpleNamespace" as the carrier name.
 * - in.errors: id -> {"class","message","base", ...attrs} exception registry
 *   used by _is_transient_typing_error (str(exc)=message, type name=class,
 *   extra attrs retry_after/status_code/code).
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   value repeats); a {"__script__":true,"ret":v,"raise":{type,message}}
 *   wrapper forces a result or raise.  Missing / exhausted script => fail-closed
 *   RuntimeError recorded in the trace before propagation.
 * - Except sites swallow ONLY their named families via the FULL builtin
 *   exception hierarchy (TA6_EXC_HIER); BaseException-only kinds
 *   (SystemExit/KeyboardInterrupt/GeneratorExit/BaseExceptionGroup) pass
 *   through every except-Exception site.
 * - Fixture domains: str()-position ids are integral ints (libjson has no
 *   int/float identity; integral doubles render as integers); str positions
 *   are codepoint-aware; _entity_span offsets/lengths are ints; UTF-16 slicing
 *   is code-unit aware with unpaired-surrogate decode failure -> "".
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

/* ── universal seam context ── */
typedef struct ta6_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed exception-graph registry */
} ta6_ctx_t;

/* ── basic helpers ── */
static bool ta6_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ta6_raise_kind(const json_t *v) {
    return ta6_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool ta6_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ta6_present(const json_t *v) { return v && v->type != JSON_NULL; }
static bool ta6_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ta6_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *ta6_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const char *ta6_type_name(const json_t *v) {
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
static const json_t *ta6_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *ta6_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *ta6_attr(const json_t *obj, const char *key,
                              const char *tname, json_t **err) {
    char msg[220];
    if (!obj || obj->type == JSON_NULL) {
        snprintf(msg, sizeof msg, "'NoneType' object has no attribute '%s'", key);
        *err = ta6_err("AttributeError", msg);
        return NULL;
    }
    if (obj->type != JSON_OBJECT) {
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", ta6_type_name(obj), key);
        *err = ta6_err("AttributeError", msg);
        return NULL;
    }
    const json_t *v = json_obj_get(obj, key);
    if (!v) {
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", tname, key);
        *err = ta6_err("AttributeError", msg);
        return NULL;
    }
    return v;
}

/* ── string helpers ── */
static char *ta6_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static unsigned long ta6_next_cp(const char **p) {
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
static bool ta6_is_space_cp(unsigned long cp) {
    if (cp >= 0x09 && cp <= 0x0d) return true;
    if (cp >= 0x1c && cp <= 0x1f) return true;
    if (cp == 0x20 || cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}
static char *ta6_strip(const char *s) {
    if (!s) s = "";
    const char *p = s;
    while (*p) {
        const char *q = p;
        unsigned long cp = ta6_next_cp(&q);
        if (!ta6_is_space_cp(cp)) break;
        p = q;
    }
    const char *end = p + strlen(p);
    while (end > p) {
        const char *q = end - 1;
        while (q > p && ((unsigned char)*q & 0xc0) == 0x80) q--;
        const char *r = q;
        unsigned long cp = ta6_next_cp(&r);
        if (r != end || !ta6_is_space_cp(cp)) break;
        end = q;
    }
    size_t n = (size_t)(end - p);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}
static char *ta6_lower(const char *s) {
    char *o = ta6_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *ta6_lstrip_at(const char *s) {
    const char *p = s ? s : "";
    while (*p == '@') p++;
    return ta6_strdup(p);
}
static size_t ta6_cp_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    const char *p = s;
    while (*p) { ta6_next_cp(&p); n++; }
    return n;
}
static char *ta6_slice_cp(const char *s, size_t n) {
    if (!s) return ta6_strdup("");
    const char *p = s, *end = s;
    size_t k = 0;
    while (*p && k < n) { ta6_next_cp(&p); end = p; k++; }
    size_t len = (size_t)(end - s);
    char *o = malloc(len + 1);
    if (!o) return NULL;
    memcpy(o, s, len);
    o[len] = '\0';
    return o;
}
/* Python str(value) for the scalar domains the fixture uses. */
static char *ta6_pystr(const json_t *v) {
    char buf[96];
    if (!v || v->type == JSON_NULL) return ta6_strdup("None");
    switch (v->type) {
        case JSON_STRING: return ta6_strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL: return ta6_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (isfinite(v->num_val) && v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e16)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return ta6_strdup(buf);
        default: return ta6_strdup("");
    }
}
/* Python int(value); narrow strip set (no 0x1c-0x1f). Returns false on failure. */
static bool ta6_pyint(const json_t *v, long *out) {
    if (!v || v->type == JSON_NULL || v->type == JSON_ARRAY || v->type == JSON_OBJECT) return false;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return true; }
    if (v->type == JSON_NUMBER) { *out = (long)v->num_val; return true; }
    const char *s = v->str_val ? v->str_val : "";
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f' ||
                  (unsigned char)*s == 0x85 || (unsigned char)*s == 0xa0)) s++;
    char *end = NULL;
    long n = strtol(s, &end, 10);
    if (end == s) return false;
    while (*end && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\v' || *end == '\f' ||
                    (unsigned char)*end == 0x85 || (unsigned char)*end == 0xa0)) end++;
    if (*end != '\0') return false;
    *out = n;
    return true;
}

/* ── UTF-16 code-unit slicing (Telegram entity offsets) ── */
static size_t ta6_utf8_put(char *o, unsigned long cp) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0]=(char)(0xc0|(cp>>6)); o[1]=(char)(0x80|(cp&0x3f)); return 2; }
    if (cp < 0x10000) {
        o[0]=(char)(0xe0|(cp>>12)); o[1]=(char)(0x80|((cp>>6)&0x3f)); o[2]=(char)(0x80|(cp&0x3f)); return 3;
    }
    o[0]=(char)(0xf0|(cp>>18)); o[1]=(char)(0x80|((cp>>12)&0x3f));
    o[2]=(char)(0x80|((cp>>6)&0x3f)); o[3]=(char)(0x80|(cp&0x3f)); return 4;
}
/* Return malloc'd UTF-8 string for source_text[offset*2:(offset+length)*2]
 * decoded as UTF-16-LE; unpaired surrogate -> empty string. */
static char *ta6_utf16_span(const char *s, long offset, long length) {
    char *empty_out = ta6_strdup("");
    if (offset < 0 || length <= 0) return empty_out;
    size_t cap = strlen(s);
    uint16_t *u = malloc((cap ? cap : 1) * sizeof(uint16_t));
    if (!u) return empty_out;
    size_t n = 0;
    const char *p = s;
    while (*p) {
        unsigned long cp = ta6_next_cp(&p);
        if (cp <= 0xFFFF) u[n++] = (uint16_t)cp;
        else { cp -= 0x10000; u[n++] = (uint16_t)(0xD800 + (cp >> 10)); u[n++] = (uint16_t)(0xDC00 + (cp & 0x3ff)); }
    }
    long start = offset, end = offset + length;
    if (end < start) end = start;
    if ((size_t)start >= n) { free(u); return empty_out; }
    if ((size_t)end > n) end = (long)n;
    size_t bytecap = (size_t)(end - start) * 4 + 1;
    char *o = malloc(bytecap);
    if (!o) { free(u); return empty_out; }
    size_t k = 0;
    long i = start;
    while (i < end) {
        uint16_t w = u[i];
        unsigned long cp;
        if (w >= 0xD800 && w <= 0xDBFF) {
            if (i + 1 >= end) { free(u); free(o); return empty_out; }
            uint16_t lo = u[i + 1];
            if (lo < 0xDC00 || lo > 0xDFFF) { free(u); free(o); return empty_out; }
            cp = 0x10000 + (((unsigned long)(w - 0xD800)) << 10) + (unsigned long)(lo - 0xDC00);
            i += 2;
        } else if (w >= 0xDC00 && w <= 0xDFFF) {
            free(u); free(o); return empty_out;
        } else { cp = w; i += 1; }
        k += ta6_utf8_put(o + k, cp);
    }
    o[k] = '\0';
    free(u);
    free(empty_out);
    return o;
}

/* ── FULL builtin exception hierarchy (CPython 3.14.9) ── */
typedef struct { const char *name; const char *parent; } ta6_exc_row_t;
static const ta6_exc_row_t TA6_EXC_HIER[] = {
    {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"}, {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"}, {"BufferError", "Exception"},
    {"BytesWarning", "Warning"}, {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"}, {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"}, {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"}, {"Exception", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"}, {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"}, {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"}, {"GeneratorExit", "BaseException"},
    {"ImportError", "Exception"}, {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"}, {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"}, {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"}, {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"}, {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"}, {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"}, {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"}, {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"}, {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"}, {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"}, {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"}, {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"}, {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"}, {"SystemError", "Exception"},
    {"SystemExit", "BaseException"}, {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"}, {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"}, {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"}, {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"}, {"ValueError", "Exception"}, {"Warning", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"}, {"_IncompleteInputError", "SyntaxError"},
};
static const char *ta6_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA6_EXC_HIER) / sizeof(TA6_EXC_HIER[0]); i++)
        if (strcmp(name, TA6_EXC_HIER[i].name) == 0) return TA6_EXC_HIER[i].parent;
    return NULL;
}
static bool ta6_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta6_builtin_parent(cur);
    }
    return false;
}
/* unknown/synthetic kinds are treated as Exception subclasses by convention */
static bool ta6_is_exception(const char *kind) {
    if (!kind) return false;
    if (ta6_builtin_parent(kind) == NULL) return true;
    return ta6_chain_has(kind, "Exception");
}
static bool ta6_is_base_only(const char *kind) {
    return kind && ta6_chain_has(kind, "BaseException") && !ta6_chain_has(kind, "Exception");
}

/* ── exception-graph registry ── */
static const json_t *ta6_err_obj(const ta6_ctx_t *ctx, const char *eid) {
    if (!eid || !ctx->errors || ctx->errors->type != JSON_OBJECT) return NULL;
    return json_obj_get(ctx->errors, eid);
}
static const char *ta6_err_class(const ta6_ctx_t *ctx, const char *eid) {
    const json_t *e = ta6_err_obj(ctx, eid);
    return e ? json_get_str(e, "class", "Exception") : NULL;
}
static const char *ta6_err_text(const ta6_ctx_t *ctx, const char *eid) {
    const json_t *e = ta6_err_obj(ctx, eid);
    return e ? json_get_str(e, "message", "") : "";
}

/* ── set helpers (sorted output to mirror canon(set) -> sorted list) ── */
static void ta6_set_add(json_t *arr, const char *val) {
    if (!val || !*val) return;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *x = ta6_str(json_get(arr, i));
        if (x && strcmp(x, val) == 0) return;
    }
    json_append(arr, json_string(val));
}
static int ta6_cmp_str(const void *a, const void *b) {
    const json_t *x = *(const json_t * const *)a;
    const json_t *y = *(const json_t * const *)b;
    const char *xs = ta6_str(x) ? ta6_str(x) : "";
    const char *ys = ta6_str(y) ? ta6_str(y) : "";
    return strcmp(xs, ys);
}
static void ta6_set_sort(json_t *arr) {
    size_t n = json_len(arr);
    if (n < 2) return;
    /* build a temp pointer array, sort, rebuild (libjson owns items array) */
    json_t **tmp = malloc(n * sizeof(json_t *));
    if (!tmp) return;
    for (size_t i = 0; i < n; i++) tmp[i] = json_get(arr, i);
    qsort(tmp, n, sizeof(json_t *), ta6_cmp_str);
    for (size_t i = 0; i < n; i++) arr->c.items[i] = tmp[i];
    free(tmp);
}

/* ── _FOREIGN_BOT_HANDLE_RE.fullmatch : [a-z0-9_]{2,29}bot (IGNORECASE) ── */
static bool ta6_foreign_bot_handle(const char *h, const char *own_lo) {
    if (!h || !*h) return false;
    if (own_lo && *own_lo && strcmp(h, own_lo) == 0) return true;
    size_t n = strlen(h);
    if (n < 5 || n > 32) return false;
    if (tolower((unsigned char)h[n-3]) != 'b' || tolower((unsigned char)h[n-2]) != 'o' ||
        tolower((unsigned char)h[n-1]) != 't') return false;
    for (size_t i = 0; i < n - 3; i++) {
        unsigned char c = (unsigned char)h[i];
        if (!(isalnum(c) || c == '_')) return false;
    }
    return true;
}
/* raw-text fallback: (?i)(?<![A-Za-z0-9_`/])@([A-Za-z0-9_]{2,31})\b */
static void ta6_fallback_handles(const char *text, const char *own_lo, json_t *out) {
    if (!text || !*text) return;
    size_t n = strlen(text);
    for (size_t i = 0; i < n; i++) {
        if (text[i] != '@') continue;
        if (i > 0) {
            unsigned char b = (unsigned char)text[i-1];
            if (isalnum(b) || b == '_' || b == '`' || b == '/') continue;
        }
        size_t j = i + 1, cnt = 0;
        while (j < n && (isalnum((unsigned char)text[j]) || text[j] == '_')) { j++; cnt++; }
        if (cnt < 2 || cnt > 31) continue;
        char *h = malloc(cnt + 1);
        if (!h) continue;
        memcpy(h, text + i + 1, cnt);
        h[cnt] = '\0';
        char *lo = ta6_lower(h);
        if (ta6_foreign_bot_handle(lo, own_lo)) ta6_set_add(out, lo);
        free(lo); free(h);
        i = j - 1;
    }
}

/* ── the one universal seam ──
 * args is ADOPTED into the trace event; the return is a fresh owned node. */
static json_t *ta6_call(ta6_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta6_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta6_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ta6_err(json_get_str(raise, "type", "RuntimeError"),
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

/* ═══════════════════════ ported flows ═══════════════════════ */

/* PoP: ta6_normalize_platform_event @ plugins/platforms/telegram/adapter.py:_normalize_platform_event */
static json_t *ta6_normalize_platform_event(ta6_ctx_t *ctx, const json_t *args) {
    const json_t *upd = ta6_arg(args, "update");
    const json_t *mr = ta6_getattr(upd, "message_reaction");
    if (ta6_present(mr)) {
        json_t *a = json_object();
        json_set(a, "update", upd ? json_copy(upd) : json_null());
        return ta6_call(ctx, "_normalize_reaction_event", a);
    }
    const json_t *ed = ta6_getattr(upd, "edited_message");
    if (ta6_present(ed)) {
        json_t *a = json_object();
        json_set(a, "update", upd ? json_copy(upd) : json_null());
        return ta6_call(ctx, "_normalize_message_edited_event", a);
    }
    return json_null();
}

/* PoP: ta6_normalize_reaction_event @ plugins/platforms/telegram/adapter.py:_normalize_reaction_event */
static json_t *ta6_normalize_reaction_event(ta6_ctx_t *ctx, const json_t *args) {
    const json_t *upd = ta6_arg(args, "update");
    const json_t *mr = ta6_getattr(upd, "message_reaction");
    if (ta6_is_null(mr)) return json_null();
    const json_t *chat = ta6_getattr(mr, "chat");
    const json_t *nr0 = ta6_getattr(mr, "new_reaction");
    json_t *nr_empty = NULL;
    const json_t *nr = nr0;
    if (!ta6_truthy(nr0)) { nr_empty = json_array(); nr = nr_empty; }
    if (!nr || nr->type != JSON_ARRAY) { json_free(nr_empty); return json_null(); }
    const json_t *chat_id = ta6_present(chat) ? ta6_getattr(chat, "id") : NULL;
    const json_t *msg_id = ta6_getattr(mr, "message_id");

    { json_t *a = json_object(); json_set(a, "value", chat_id ? json_copy(chat_id) : json_null());
      json_t *r = ta6_call(ctx, "_is_id_like", a);
      if (ta6_is_raise(r)) { json_free(nr_empty); return r; }
      if (!ta6_truthy(r)) { json_free(r); json_free(nr_empty); return json_null(); }
      json_free(r); }
    { json_t *a = json_object(); json_set(a, "value", msg_id ? json_copy(msg_id) : json_null());
      json_t *r = ta6_call(ctx, "_is_id_like", a);
      if (ta6_is_raise(r)) { json_free(nr_empty); return r; }
      if (!ta6_truthy(r)) { json_free(r); json_free(nr_empty); return json_null(); }
      json_free(r); }

    json_t *emojis = json_array();
    json_t *custom = json_array();
    size_t lim = json_len(nr) < 64 ? json_len(nr) : 64;
    for (size_t i = 0; i < lim; i++) {
        const json_t *r = json_get(nr, i);
        const json_t *emoji = ta6_getattr(r, "emoji");
        if (emoji && emoji->type == JSON_STRING && ta6_truthy(emoji)) {
            char *cut = ta6_slice_cp(emoji->str_val ? emoji->str_val : "", 64);
            json_append(emojis, json_string(cut ? cut : ""));
            free(cut);
        }
        const json_t *cid = ta6_getattr(r, "custom_emoji_id");
        json_t *a = json_object();
        json_set(a, "value", cid ? json_copy(cid) : json_null());
        json_t *idres = ta6_call(ctx, "_is_id_like", a);
        if (ta6_is_raise(idres)) { json_free(emojis); json_free(custom); json_free(nr_empty); return idres; }
        if (ta6_truthy(idres)) {
            char *s = ta6_pystr(cid);
            char *cut = ta6_slice_cp(s ? s : "", 128);
            json_append(custom, json_string(cut ? cut : ""));
            free(cut); free(s);
        }
        json_free(idres);
    }
    char *cs = ta6_pystr(chat_id), *ms = ta6_pystr(msg_id);
    char *ccut = ta6_slice_cp(cs ? cs : "", 128), *mcut = ta6_slice_cp(ms ? ms : "", 128);
    json_t *payload = json_object();
    json_set(payload, "emojis", emojis);
    json_set(payload, "custom_emoji_ids", custom);
    json_set_hdr(payload, "chat_id", ccut ? ccut : "");
    json_set_hdr(payload, "message_id", mcut ? mcut : "");
    json_set(payload, "thread_id", json_null());
    free(ccut); free(mcut); free(cs); free(ms);
    json_t *env = json_object();
    json_set_hdr(env, "platform", "telegram");
    json_set_hdr(env, "event_type", "reaction");
    json_set(env, "payload", payload);
    json_free(nr_empty);
    return env;
}

/* PoP: ta6_normalize_message_edited_event @ plugins/platforms/telegram/adapter.py:_normalize_message_edited_event */
static json_t *ta6_normalize_message_edited_event(ta6_ctx_t *ctx, const json_t *args) {
    const json_t *upd = ta6_arg(args, "update");
    const json_t *msg = ta6_getattr(upd, "edited_message");
    if (ta6_is_null(msg)) return json_null();
    const json_t *chat = ta6_getattr(msg, "chat");
    const json_t *chat_id = ta6_present(chat) ? ta6_getattr(chat, "id") : NULL;
    const json_t *msg_id = ta6_getattr(msg, "message_id");
    { json_t *a = json_object(); json_set(a, "value", chat_id ? json_copy(chat_id) : json_null());
      json_t *r = ta6_call(ctx, "_is_id_like", a);
      if (ta6_is_raise(r)) return r;
      if (!ta6_truthy(r)) { json_free(r); return json_null(); }
      json_free(r); }
    { json_t *a = json_object(); json_set(a, "value", msg_id ? json_copy(msg_id) : json_null());
      json_t *r = ta6_call(ctx, "_is_id_like", a);
      if (ta6_is_raise(r)) return r;
      if (!ta6_truthy(r)) { json_free(r); return json_null(); }
      json_free(r); }

    const json_t *t0 = ta6_getattr(msg, "text");
    const json_t *text = ta6_truthy(t0) ? t0 : ta6_getattr(msg, "caption");
    if (!text || text->type != JSON_STRING) text = NULL;

    json_t *thread_id = NULL; /* owned string node, or NULL for null */
    const json_t *traw = ta6_getattr(msg, "message_thread_id");
    json_t *ta = json_object();
    json_set(ta, "value", traw ? json_copy(traw) : json_null());
    json_t *tr = ta6_call(ctx, "_is_id_like", ta);
    if (ta6_is_raise(tr)) return tr;
    const json_t *topic = ta6_getattr(msg, "is_topic_message");
    if (ta6_truthy(tr) && ta6_truthy(topic)) {
        char *s = ta6_pystr(traw);
        char *cut = ta6_slice_cp(s ? s : "", 128);
        thread_id = json_string(cut ? cut : "");
        free(cut); free(s);
    }
    json_free(tr);

    json_t *edited_at = NULL;
    const json_t *ed = ta6_getattr(msg, "edit_date");
    if (ta6_present(ed)) {
        const json_t *iso = ta6_getattr(ed, "isoformat");
        if (iso) {
            json_t *a = json_object();
            json_t *r = ta6_call(ctx, "isoformat", a);
            if (ta6_is_raise(r)) {
                /* Swallowed Exception: the raise spec is owned here; NULL it so
                 * the shared json_free(r) below cannot double-free it. */
                if (ta6_is_exception(ta6_raise_kind(r))) { json_free(r); r = NULL; }
                else return r; /* BaseException passthrough — caller owns it */
            } else {
                char *s = ta6_pystr(r);
                char *cut = ta6_slice_cp(s ? s : "", 64);
                edited_at = json_string(cut ? cut : "");
                free(cut); free(s);
            }
            json_free(r);
        }
    }
    char *cs = ta6_pystr(chat_id), *ms = ta6_pystr(msg_id);
    char *ccut = ta6_slice_cp(cs ? cs : "", 128), *mcut = ta6_slice_cp(ms ? ms : "", 128);
    char *tc = text ? ta6_slice_cp(text->str_val ? text->str_val : "", 8192) : NULL;
    json_t *payload = json_object();
    json_set_hdr(payload, "chat_id", ccut ? ccut : "");
    json_set_hdr(payload, "message_id", mcut ? mcut : "");
    json_set(payload, "thread_id", thread_id ? thread_id : json_null());
    json_set(payload, "text", text ? json_string(tc ? tc : "") : json_null());
    json_set(payload, "edited_at", edited_at ? edited_at : json_null());
    free(ccut); free(mcut); free(tc); free(cs); free(ms);
    json_t *env = json_object();
    json_set_hdr(env, "platform", "telegram");
    json_set_hdr(env, "event_type", "message_edited");
    json_set(env, "payload", payload);
    return env;
}

/* PoP: ta6_telegram_entity_text @ plugins/platforms/telegram/adapter.py:_telegram_entity_text */
static json_t *ta6_telegram_entity_text(ta6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *snode = ta6_arg(args, "source_text");
    const json_t *off = ta6_arg(args, "offset");
    const json_t *len = ta6_arg(args, "length");
    long offset = 0, length = 0;
    (void)ta6_pyint(off, &offset);
    (void)ta6_pyint(len, &length);
    const char *s = ta6_str(snode);
    char *out = ta6_utf16_span(s ? s : "", offset, length);
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* PoP: ta6_entity_span @ plugins/platforms/telegram/adapter.py:_entity_span */
static json_t *ta6_entity_span(ta6_ctx_t *ctx, const json_t *args) {
    const json_t *snode = ta6_arg(args, "source_text");
    const json_t *ent = ta6_arg(args, "entity");
    const json_t *ov = ta6_getattr(ent, "offset");
    const json_t *lv = ta6_getattr(ent, "length");
    long offset = -1, length = 0;
    if (!ta6_pyint(ta6_present(ov) ? ov : NULL, &offset)) {
        if (!ov || ta6_is_null(ov)) offset = -1;
        else return ta6_err("TypeError", "int() argument has invalid type");
    }
    if (!ta6_pyint(ta6_present(lv) ? lv : NULL, &length)) {
        if (!lv || ta6_is_null(lv)) length = 0;
        else return ta6_err("TypeError", "int() argument has invalid type");
    }
    if (offset < 0 || length <= 0) return json_null();
    json_t *a = json_object();
    json_set(a, "source_text", snode ? json_copy(snode) : json_null());
    json_set(a, "offset", json_number((double)offset));
    json_set(a, "length", json_number((double)length));
    return ta6_call(ctx, "_telegram_entity_text", a);
}

/* PoP: ta6_chat_id_str @ plugins/platforms/telegram/adapter.py:_chat_id_str */
static json_t *ta6_chat_id_str(ta6_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *msg = ta6_arg(args, "message");
    const json_t *chat = ta6_getattr(msg, "chat");
    const json_t *idv = ta6_getattr(chat, "id");
    char *s = ta6_pystr(ta6_present(idv) ? idv : NULL);
    /* getattr default is "" when id absent; Python str("")=="" but str(None)=="None" */
    if (!idv) { free(s); s = ta6_strdup(""); }
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: ta6_is_group_chat @ plugins/platforms/telegram/adapter.py:_is_group_chat */
static json_t *ta6_is_group_chat(ta6_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta6_arg(args, "message");
    const json_t *chat = ta6_getattr(msg, "chat");
    if (!ta6_truthy(chat)) return json_bool(false);
    json_t *a = json_object();
    json_set(a, "chat", json_copy(chat));
    json_t *r = ta6_call(ctx, "_chat_type_str", a);
    if (ta6_is_raise(r)) return r;
    const char *s = ta6_str(r);
    bool ok = s && (strcmp(s, "group") == 0 || strcmp(s, "supergroup") == 0 || strcmp(s, "channel") == 0);
    json_free(r);
    return json_bool(ok);
}

/* PoP: ta6_extract_bot_mention_usernames @ plugins/platforms/telegram/adapter.py:_extract_bot_mention_usernames */
static json_t *ta6_extract_bot_mention_usernames(ta6_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta6_arg(args, "message");
    const json_t *su = ta6_arg(args, "self_username");
    char *own_raw = ta6_str(su) ? ta6_strdup(ta6_str(su)) : ta6_strdup("");
    char *own = ta6_lstrip_at(own_raw ? own_raw : "");
    char *own_lo = ta6_lower(own ? own : "");
    free(own); free(own_raw);

    json_t *out = json_array();
    json_t *ret = NULL;
    /* Owned seam results. NULL-init so ANY `goto done` (including raises from
     * loop 1, before s2 is fetched) has safe, defined cleanup. */
    json_t *s1 = NULL, *s2 = NULL;

    /* loop 1: entity-authoritative */
    json_t *a1 = json_object();
    json_set(a1, "message", msg ? json_copy(msg) : json_null());
    s1 = ta6_call(ctx, "_entity_sources", a1);
    if (ta6_is_raise(s1)) { ret = s1; goto done; }
    if (s1 && s1->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(s1); i++) {
            const json_t *pair = json_get(s1, i);
            if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
            const json_t *ents = json_get(pair, 1);
            if (!ents || ents->type != JSON_ARRAY) continue;
            for (size_t j = 0; j < json_len(ents); j++) {
                const json_t *ent = json_get(ents, j);
                json_t *ae = json_object();
                json_set(ae, "entity", ent ? json_copy(ent) : json_null());
                json_t *et = ta6_call(ctx, "_entity_type", ae);
                if (ta6_is_raise(et)) { ret = et; goto done; }
                const char *ets_borrow = ta6_str(et);
                /* UAF fix: `ets` borrows into `et`; snapshot it BEFORE the free,
                 * the branch re-reads it after the _entity_span seam. */
                char *ets = ets_borrow ? ta6_strdup(ets_borrow) : NULL;
                bool is_mention = ets && strcmp(ets, "mention") == 0;
                bool is_command = ets && strcmp(ets, "bot_command") == 0;
                bool interesting = is_mention || is_command;
                json_free(et);
                if (!interesting) { free(ets); continue; }
                json_t *as = json_object();
                json_set(as, "source_text", json_copy(json_get(pair, 0)));
                json_set(as, "entity", ent ? json_copy(ent) : json_null());
                json_t *sp = ta6_call(ctx, "_entity_span", as);
                if (ta6_is_raise(sp)) { ret = sp; free(ets); goto done; }
                const char *sps = ta6_str(sp);
                if (sps) {
                    char *st = ta6_strip(sps);
                    if (is_mention) {
                        char *h0 = ta6_lstrip_at(st);
                        char *h = ta6_lower(h0);
                        if (ta6_foreign_bot_handle(h, own_lo)) ta6_set_add(out, h);
                        free(h); free(h0);
                    } else if (is_command) {
                        const char *at = strchr(st, '@');
                        if (at) {
                            char *tgt0 = ta6_strdup(at + 1);
                            char *tgt_strip = ta6_strip(tgt0);
                            char *tgt = ta6_lower(tgt_strip);
                            if (ta6_foreign_bot_handle(tgt, own_lo)) ta6_set_add(out, tgt);
                            free(tgt); free(tgt_strip); free(tgt0);
                        }
                    }
                    free(st);
                }
                json_free(sp);
                free(ets);
            }
        }
    }

    /* loop 2: entity-less fallback */
    json_t *a2 = json_object();
    json_set(a2, "message", msg ? json_copy(msg) : json_null());
    s2 = ta6_call(ctx, "_entity_sources", a2);
    if (ta6_is_raise(s2)) { ret = s2; goto done; }
    if (s2 && s2->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(s2); i++) {
            const json_t *pair = json_get(s2, i);
            if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
            const json_t *raw = json_get(pair, 0);
            const json_t *ents = json_get(pair, 1);
            if (!ta6_truthy(raw)) continue;
            if (ents && ents->type == JSON_ARRAY && json_len(ents) > 0) continue;
            ta6_fallback_handles(ta6_str(raw) ? ta6_str(raw) : "", own_lo, out);
        }
    }

done:
    /* `ret` may alias s1/s2 when those seams raised — never free a returned
     * raise spec; the caller adopts it. NULL-init above makes both safe. */
    if (ret != s1) json_free(s1);
    if (ret != s2) json_free(s2);
    free(own_lo);
    if (ret) { json_free(out); return ret; }
    ta6_set_sort(out);
    return out;
}

/* PoP: ta6_is_guest_mention @ plugins/platforms/telegram/adapter.py:_is_guest_mention */
static json_t *ta6_is_guest_mention(ta6_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta6_arg(args, "message");
    const json_t *chat = ta6_getattr(msg, "chat");
    json_t *a1 = json_object();
    json_set(a1, "chat", chat ? json_copy(chat) : json_null());
    json_t *ct = ta6_call(ctx, "_chat_type_str", a1);
    if (ta6_is_raise(ct)) return ct;
    const char *s = ta6_str(ct);
    bool is_channel = s && strcmp(s, "channel") == 0;
    json_free(ct);
    if (is_channel) return json_bool(false);
    json_t *a2 = json_object();
    json_t *gm = ta6_call(ctx, "_telegram_guest_mode", a2);
    if (ta6_is_raise(gm)) return gm;
    bool guest = ta6_truthy(gm);
    json_free(gm);
    if (!guest) return json_bool(false);
    json_t *a3 = json_object();
    json_set(a3, "message", msg ? json_copy(msg) : json_null());
    json_t *mb = ta6_call(ctx, "_message_mentions_bot", a3);
    if (ta6_is_raise(mb)) return mb;
    bool ok = ta6_truthy(mb);
    json_free(mb);
    return json_bool(ok);
}

/* PoP: ta6_bot_sender_suppressed @ plugins/platforms/telegram/adapter.py:_bot_sender_suppressed */
static json_t *ta6_bot_sender_suppressed(ta6_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta6_arg(args, "message");
    json_t *a1 = json_object();
    json_t *br = ta6_call(ctx, "_telegram_bots_require_mention", a1);
    if (ta6_is_raise(br)) return br;
    bool gate = ta6_truthy(br);
    json_free(br);
    if (!gate) return json_bool(false);
    json_t *a2 = json_object();
    json_set(a2, "message", msg ? json_copy(msg) : json_null());
    json_t *other = ta6_call(ctx, "_sender_is_other_bot", a2);
    if (ta6_is_raise(other)) return other;
    bool is_other = ta6_truthy(other);
    json_free(other);
    if (!is_other) return json_bool(false);
    json_t *a3 = json_object();
    json_set(a3, "message", msg ? json_copy(msg) : json_null());
    json_t *mb = ta6_call(ctx, "_message_mentions_bot", a3);
    if (ta6_is_raise(mb)) return mb;
    bool ok = !ta6_truthy(mb);
    json_free(mb);
    return json_bool(ok);
}

/* PoP: ta6_clean_bot_trigger_text @ plugins/platforms/telegram/adapter.py:_clean_bot_trigger_text */
static json_t *ta6_clean_bot_trigger_text(ta6_ctx_t *ctx, const json_t *args) {
    const json_t *text = ta6_arg(args, "text");
    json_t *a1 = json_object();
    json_t *u = ta6_call(ctx, "_current_bot_username", a1);
    if (ta6_is_raise(u)) return u;
    json_t *a2 = json_object();
    json_set(a2, "text", text ? json_copy(text) : json_null());
    json_set(a2, "username", json_copy(u));
    json_free(u);
    return ta6_call(ctx, "clean_bot_trigger_text", a2);
}

/* PoP: ta6_current_bot_username @ plugins/platforms/telegram/adapter.py:_current_bot_username */
static json_t *ta6_current_bot_username(ta6_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *obs = ta6_getattr(ctx->state, "_bot_username_observed");
    if (ta6_truthy(obs)) {
        char *s = ta6_pystr(obs);
        json_t *r = json_string(s ? s : "");
        free(s);
        return r;
    }
    json_t *err = NULL;
    const json_t *bot = ta6_attr(ctx->state, "_bot", "TelegramAdapter", &err);
    if (err) return err;
    const json_t *u = ta6_getattr(bot, "username");
    const char *us = ta6_present(u) ? ta6_str(u) : NULL;
    char *l = ta6_lstrip_at(us ? us : "");
    char *lo = ta6_lower(l ? l : "");
    free(l);
    json_t *r = json_string(lo ? lo : "");
    free(lo);
    return r;
}

/* PoP: ta6_is_transient_typing_error @ plugins/platforms/telegram/adapter.py:_is_transient_typing_error */
static json_t *ta6_is_transient_typing_error(ta6_ctx_t *ctx, const json_t *args) {
    const char *eid = ta6_str(ta6_arg(args, "exc"));
    const json_t *e = ta6_err_obj(ctx, eid);
    if (!e) return json_bool(false);
    const json_t *ra = json_obj_get(e, "retry_after");
    if (ta6_present(ra)) return json_bool(true);
    const json_t *sc0 = json_obj_get(e, "status_code");
    const json_t *sc = ta6_truthy(sc0) ? sc0 : json_obj_get(e, "code");
    if (sc && sc->type == JSON_NUMBER && sc->num_val == floor(sc->num_val) &&
        fabs(sc->num_val) < 1e16 && (sc->num_val == 429 || sc->num_val >= 500))
        return json_bool(true);
    const char *msg = ta6_err_text(ctx, eid);
    char *lo = ta6_lower(msg ? msg : "");
    static const char *const markers[] = {"too many requests", "rate limit", "timed out", "timeout", "temporar"};
    bool hit = false;
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++)
        if (lo && strstr(lo, markers[i]) != NULL) { hit = true; break; }
    free(lo);
    if (hit) return json_bool(true);
    const char *cls = ta6_err_class(ctx, eid);
    bool inst = cls && (ta6_chain_has(cls, "OSError") || ta6_chain_has(cls, "TimeoutError") ||
                        ta6_chain_has(cls, "ConnectionError"));
    return json_bool(inst);
}

/* PoP: ta6_dm_topic_missing_anchor_error @ plugins/platforms/telegram/adapter.py:_dm_topic_missing_anchor_error */
static json_t *ta6_dm_topic_missing_anchor_error(ta6_ctx_t *ctx, const json_t *args) {
    (void)ctx; (void)args;
    return json_string("Telegram DM topic delivery requires a reply anchor; refusing to send outside the requested topic");
}

/* PoP: ta6_telegram_require_mention @ plugins/platforms/telegram/adapter.py:_telegram_require_mention */
static json_t *ta6_telegram_require_mention(ta6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "key", "require_mention");
    json_set_hdr(a, "env", "TELEGRAM_REQUIRE_MENTION");
    json_set_hdr(a, "default", "false");
    return ta6_call(ctx, "_extra_bool", a);
}
/* PoP: ta6_telegram_bots_require_mention @ plugins/platforms/telegram/adapter.py:_telegram_bots_require_mention */
static json_t *ta6_telegram_bots_require_mention(ta6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "key", "bots_require_mention");
    json_set_hdr(a, "env", "TELEGRAM_BOTS_REQUIRE_MENTION");
    json_set_hdr(a, "default", "false");
    return ta6_call(ctx, "_extra_bool", a);
}
/* PoP: ta6_telegram_exclusive_bot_mentions @ plugins/platforms/telegram/adapter.py:_telegram_exclusive_bot_mentions */
static json_t *ta6_telegram_exclusive_bot_mentions(ta6_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "key", "exclusive_bot_mentions");
    json_set_hdr(a, "env", "TELEGRAM_EXCLUSIVE_BOT_MENTIONS");
    json_set_hdr(a, "default", "true");
    return ta6_call(ctx, "_extra_bool", a);
}
