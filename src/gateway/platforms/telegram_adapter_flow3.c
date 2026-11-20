/*
 * telegram_adapter_flow3.c — Telegram adapter FLOW remainder part 3 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): the pure/near-pure
 * decision surface left unclaimed after telegram_adapter_flow(.c) and
 * telegram_adapter_flow2.c.  Ported targets (all PoP-claimed here, all
 * confirmed UNCLAIMED before this file was written):
 *   _is_id_like, _caption_1024, _ext_from_path, _rows_of_two, _entity_type,
 *   _flood_cap_result, _is_permanent_fatal, _should_drop_delayed_delivery,
 *   _is_own_message, _sender_is_other_bot, _media_message_type,
 *   _text_batch_delay_for, _env_float_clamped, _redact_telegram_error_text.
 *
 * Decision sequences only: every live I/O / injected callable enters through
 * ONE universal seam: tg3_call(ctx, name, args).  The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording stubs emitting the identical event shape {"name","args","ret"}
 * from the SAME scripts.  Claimed siblings / unported collaborators are
 * seamed; pure leaves are transcribed inline (no seam, no PoP re-claim).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tg3_-prefixed.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow3.py):
 * - NULL json_t* == Python None / absent; a JSON_NULL node is a real null
 *   VALUE.  Exception specs: {"__raise__": cls, "message": str(exc)}.
 * - Adapter state (in.state) carries the attribute-double fields; a DIRECT
 *   attribute read (self._bot, self._text_batch_delay_seconds, ...) raises
 *   AttributeError "'TelegramAdapter' object has no attribute '<k>'" when the
 *   key is ABSENT; present-null is a real None.  getattr(self, k, default)
 *   reads json_obj_get with the default.  Nested doubles (message/from_user/
 *   entity/bot) are plain attribute doubles (SimpleNamespace on the oracle
 *   side), so their reads use "types.SimpleNamespace" as the carrier name.
 * - in.errors: id -> {"class","message","base"} exception registry used by
 *   _redact_telegram_error_text's error object (str(error)=message,
 *   type(error).__name__=class).  Registry classes must not be KeyError
 *   (str() reprs the msg).
 * - Seam scripts: a plain value repeats; a LIST is consumed per call; a
 *   {"__script__":true,"ret":v,"raise":{type,message}} wrapper forces a
 *   result or raise.  Missing / exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation.
 * - SendResult (from gateway.platforms.base) projects to the full field dict
 *   {success,message_id,error,raw_response,retryable,retry_after,
 *    continuation_message_ids,error_kind}; _flood_cap_result sets only
 *   success/error/retry_after, the rest take dataclass defaults.
 * - MessageType (gateway.platforms.event) projects to its .value string.
 * - Except sites swallow ONLY their named families via the FULL builtin
 *   exception hierarchy (TG3_EXC_HIER): BaseException-only kinds
 *   (SystemExit/KeyboardInterrupt/GeneratorExit/BaseExceptionGroup) pass
 *   through every except-Exception site.
 * - Fixture domains: str()-position numbers are NON-integral floats (libjson
 *   has no int/float identity; integral floats would render "12" vs live
 *   "12.0"); _is_id_like number nodes are integral ints or non-integral
 *   floats; float() seam values are plain decimals / "inf" / "nan" / null;
 *   codepoint slicing (caption) is codepoint-aware; candidate extensions are
 *   lowercase (only the path is .lower()'d live).
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

/* class constants transcribed verbatim from adapter.py */
#define TG3_SPLIT_THRESHOLD 4000.0
#define TG3_TEXT_BATCH_FAST_LEN 320.0
#define TG3_TEXT_BATCH_SHORT_LEN 1024.0
#define TG3_TEXT_BATCH_FAST_DELAY_S 0.18
#define TG3_TEXT_BATCH_SHORT_DELAY_S 0.24

/* ── universal seam context ── */
typedef struct tg3_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed exception-graph registry */
} tg3_ctx_t;

/* ── basic helpers ── */
static bool tg3_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *tg3_raise_kind(const json_t *v) {
    return tg3_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool tg3_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool tg3_present(const json_t *v) { return v && v->type != JSON_NULL; }
static bool tg3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *tg3_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *tg3_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const char *tg3_type_name(const json_t *v) {
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
static const json_t *tg3_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *tg3_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *tg3_attr(const json_t *obj, const char *key,
                              const char *tname, json_t **err) {
    char msg[200];
    if (!obj || obj->type == JSON_NULL) {
        snprintf(msg, sizeof msg, "'NoneType' object has no attribute '%s'", key);
        *err = tg3_err("AttributeError", msg);
        return NULL;
    }
    if (obj->type != JSON_OBJECT) {
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", tg3_type_name(obj), key);
        *err = tg3_err("AttributeError", msg);
        return NULL;
    }
    const json_t *v = json_obj_get(obj, key);
    if (!v) {
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", tname, key);
        *err = tg3_err("AttributeError", msg);
        return NULL;
    }
    return v;
}

/* ── string helpers ── */
static char *tg3_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static unsigned long tg3_next_cp(const char **p) {
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
static bool tg3_is_space_cp(unsigned long cp) {
    if (cp >= 0x09 && cp <= 0x0d) return true;
    if (cp >= 0x1c && cp <= 0x1f) return true;
    if (cp == 0x20 || cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}
static char *tg3_strip(const char *s) {
    if (!s) s = "";
    const char *p = s;
    while (*p) {
        const char *q = p;
        unsigned long cp = tg3_next_cp(&q);
        if (!tg3_is_space_cp(cp)) break;
        p = q;
    }
    const char *end = p + strlen(p);
    while (end > p) {
        const char *q = end - 1;
        while (q > p && ((unsigned char)*q & 0xc0) == 0x80) q--;
        const char *r = q;
        unsigned long cp = tg3_next_cp(&r);
        if (r != end || !tg3_is_space_cp(cp)) break;
        end = q;
    }
    size_t n = (size_t)(end - p);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}
static char *tg3_lower(const char *s) {
    char *o = tg3_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static size_t tg3_cp_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    const char *p = s;
    while (*p) { tg3_next_cp(&p); n++; }
    return n;
}
/* first N codepoints of s (Python s[:n]) */
static char *tg3_slice_cp(const char *s, size_t n) {
    if (!s) return tg3_strdup("");
    const char *p = s, *end = s;
    size_t k = 0;
    while (*p && k < n) { tg3_next_cp(&p); end = p; k++; }
    size_t len = (size_t)(end - s);
    char *o = malloc(len + 1);
    if (!o) return NULL;
    memcpy(o, s, len);
    o[len] = '\0';
    return o;
}
/* Python str(float): shortest round-trip; integral -> "12.0". */
static void tg3_float_repr(double d, char *buf, size_t cap) {
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) return;
    }
    snprintf(buf, cap, "%.17g", d);
}
static void tg3_float_str(double d, char *buf, size_t cap) {
    if (isfinite(d) && d == floor(d) && fabs(d) < 1e16) snprintf(buf, cap, "%.1f", d);
    else tg3_float_repr(d, buf, cap);
}

/* ── FULL builtin exception hierarchy (CPython 3.14.9) ── */
typedef struct { const char *name; const char *parent; } tg3_exc_row_t;
static const tg3_exc_row_t TG3_EXC_HIER[] = {
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
static const char *tg3_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TG3_EXC_HIER) / sizeof(TG3_EXC_HIER[0]); i++)
        if (strcmp(name, TG3_EXC_HIER[i].name) == 0) return TG3_EXC_HIER[i].parent;
    return NULL;
}
static bool tg3_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = tg3_builtin_parent(cur);
    }
    return false;
}
/* unknown/synthetic kinds are treated as Exception subclasses by convention */
static bool tg3_is_exception(const char *kind) {
    if (!kind) return false;
    if (tg3_builtin_parent(kind) == NULL) return true;
    return tg3_chain_has(kind, "Exception");
}
static bool tg3_is_base_only(const char *kind) {
    return kind && tg3_chain_has(kind, "BaseException") && !tg3_chain_has(kind, "Exception");
}
static bool tg3_is_type_or_value(const char *kind) {
    return kind && (tg3_chain_has(kind, "TypeError") || tg3_chain_has(kind, "ValueError"));
}

/* ── exception-graph registry ── */
static const json_t *tg3_err_obj(const tg3_ctx_t *ctx, const char *eid) {
    if (!eid || !ctx->errors || ctx->errors->type != JSON_OBJECT) return NULL;
    return json_obj_get(ctx->errors, eid);
}
static const char *tg3_err_class(const tg3_ctx_t *ctx, const char *eid) {
    const json_t *e = tg3_err_obj(ctx, eid);
    return e ? json_get_str(e, "class", "Exception") : NULL;
}
static const char *tg3_err_text(const tg3_ctx_t *ctx, const char *eid) {
    const json_t *e = tg3_err_obj(ctx, eid);
    return e ? json_get_str(e, "message", "") : "";
}

/* ── float() projection for _env_float_clamped ──
 * value-or-raise: number node, or {"__raise__", "message"}.  Caller frees. */
static json_t *tg3_pyfloat(const json_t *v) {
    if (!v || v->type == JSON_NULL)
        return tg3_err("TypeError", "float() argument must be a string or a real number, not 'NoneType'");
    if (v->type == JSON_NUMBER) return json_number(v->num_val);
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v->type != JSON_STRING)
        return tg3_err("TypeError", "float() argument must be a string or a real number");
    char *s = tg3_strip(v->str_val ? v->str_val : "");
    if (!s) return tg3_err("RuntimeError", "out of memory");
    char *lo = tg3_lower(s);
    const char *q = lo;
    bool neg = false;
    if (*q == '+' || *q == '-') { neg = (*q == '-'); q++; }
    json_t *out = NULL;
    if (strcmp(q, "inf") == 0 || strcmp(q, "infinity") == 0) {
        out = json_number(neg ? -INFINITY : INFINITY);
    } else if (strcmp(q, "nan") == 0) {
        out = json_number(NAN);
    } else {
        char *end = NULL;
        double d = strtod(s, &end);
        if (end && *end == '\0' && end != s && strpbrk(s, "0123456789") != NULL)
            out = json_number(d);
    }
    free(lo);
    free(s);
    if (out) return out;
    return tg3_err("ValueError", "could not convert string to float");
}

/* ── the one universal seam ──
 * args is ADOPTED into the trace event; the return is a fresh owned node. */
static json_t *tg3_call(tg3_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = tg3_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tg3_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tg3_err(json_get_str(raise, "type", "RuntimeError"),
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

/* PoP: tg3_is_id_like @ plugins/platforms/telegram/adapter.py:_is_id_like */
static json_t *tg3_is_id_like(tg3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *v = tg3_arg(args, "value");
    bool ok = false;
    if (v && !tg3_is_null(v)) {
        switch (v->type) {
            case JSON_STRING: ok = true; break;
            case JSON_NUMBER: {
                double d = v->num_val;
                ok = (d == floor(d) && fabs(d) < 1e16); /* integral: int */
                break;
            }
            case JSON_BOOL: case JSON_ARRAY: case JSON_OBJECT: default: ok = false; break;
        }
    }
    return json_bool(ok);
}

/* PoP: tg3_caption_1024 @ plugins/platforms/telegram/adapter.py:_caption_1024 */
static json_t *tg3_caption_1024(tg3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *c = tg3_arg(args, "caption");
    if (!tg3_truthy(c)) return json_null();
    const char *s = tg3_str(c);
    if (!s) return json_null();
    char *cut = tg3_slice_cp(s, 1024);
    json_t *out = json_string(cut ? cut : "");
    free(cut);
    return out;
}

/* PoP: tg3_ext_from_path @ plugins/platforms/telegram/adapter.py:_ext_from_path */
static json_t *tg3_ext_from_path(tg3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *fp = tg3_arg(args, "file_path");
    const json_t *cands = tg3_arg(args, "candidates");
    const json_t *dflt = tg3_arg(args, "default");
    const char *s = tg3_str(fp);
    if (s && s[0]) {
        char *lo = tg3_lower(s);
        if (cands && cands->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(cands); i++) {
                const char *cand = tg3_str(json_get(cands, i));
                if (cand) {
                    size_t cl = strlen(cand), sl = strlen(lo);
                    if (cl && cl <= sl && strcmp(lo + sl - cl, cand) == 0) {
                        free(lo);
                        return json_string(cand);
                    }
                }
            }
        }
        free(lo);
    }
    return dflt ? json_copy(dflt) : json_null();
}

/* PoP: tg3_rows_of_two @ plugins/platforms/telegram/adapter.py:_rows_of_two */
static json_t *tg3_rows_of_two(tg3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *btns = tg3_arg(args, "buttons");
    json_t *out = json_array();
    if (!btns || btns->type != JSON_ARRAY) return out;
    size_t n = json_len(btns);
    for (size_t i = 0; i < n; i += 2) {
        json_t *row = json_array();
        json_append(row, json_copy(json_get(btns, i)));
        if (i + 1 < n) json_append(row, json_copy(json_get(btns, i + 1)));
        json_append(out, row);
    }
    return out;
}

/* PoP: tg3_entity_type @ plugins/platforms/telegram/adapter.py:_entity_type */
static json_t *tg3_entity_type(tg3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *ent = tg3_arg(args, "entity");
    const json_t *t = tg3_getattr(ent, "type"); /* getattr(..., "") */
    char *s;
    if (tg3_is_null(t)) s = tg3_strdup("");
    else if (t->type == JSON_STRING) s = tg3_strdup(t->str_val ? t->str_val : "");
    else {
        /* str(number/bool) — fixture corpus keeps this a string or absent */
        if (t->type == JSON_BOOL) s = tg3_strdup(t->bool_val ? "True" : "False");
        else if (t->type == JSON_NUMBER) {
            char buf[64];
            tg3_float_str(t->num_val, buf, sizeof buf);
            s = tg3_strdup(buf);
        } else s = tg3_strdup("");
    }
    /* last '.'-separated segment */
    char *last = s;
    for (char *p = s; *p; p++) if (*p == '.') last = p + 1;
    char *lo = tg3_lower(last);
    free(s);
    json_t *out = json_string(lo ? lo : "");
    free(lo);
    return out;
}

/* PoP: tg3_flood_cap_result @ plugins/platforms/telegram/adapter.py:_flood_cap_result */
static json_t *tg3_flood_cap_result(tg3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *w = tg3_arg(args, "wait");
    double wait = (w && w->type == JSON_NUMBER) ? w->num_val : 0.0;
    char buf[96], emsg[160];
    tg3_float_str(wait, buf, sizeof buf);
    snprintf(emsg, sizeof emsg, "flood_control:%s", buf);
    json_t *out = json_object();
    json_set(out, "success", json_bool(false));
    json_set(out, "message_id", json_null());
    json_set(out, "error", json_string(emsg));
    json_set(out, "raw_response", json_null());
    json_set(out, "retryable", json_bool(false));
    json_set(out, "retry_after", json_number(wait));
    json_set(out, "continuation_message_ids", json_array());
    json_set(out, "error_kind", json_null());
    return out;
}

/* PoP: tg3_is_permanent_fatal @ plugins/platforms/telegram/adapter.py:_is_permanent_fatal */
static json_t *tg3_is_permanent_fatal(tg3_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *code = tg3_getattr(ctx->state, "_fatal_error_code");
    if (!tg3_truthy(code)) return json_bool(false);
    const json_t *retry = tg3_getattr(ctx->state, "_fatal_error_retryable");
    bool r;
    if (retry == NULL) r = true;                        /* absent -> getattr default True */
    else if (retry->type == JSON_NULL) r = false;       /* present None -> bool(None) */
    else r = tg3_truthy(retry);
    return json_bool(!r);
}

/* PoP: tg3_should_drop_delayed_delivery @ plugins/platforms/telegram/adapter.py:_should_drop_delayed_delivery */
static json_t *tg3_should_drop_delayed_delivery(tg3_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *v = tg3_getattr(ctx->state, "_drop_delayed_deliveries");
    return json_bool(tg3_is_null(v) ? false : tg3_truthy(v)); /* default False */
}

/* PoP: tg3_is_own_message @ plugins/platforms/telegram/adapter.py:_is_own_message */
static json_t *tg3_is_own_message(tg3_ctx_t *ctx, const json_t *args) {
    json_t *err = NULL;
    const json_t *bot = tg3_attr(ctx->state, "_bot", "TelegramAdapter", &err);
    if (err) return err;
    if (!tg3_truthy(bot)) return json_bool(false);
    const json_t *msg = tg3_arg(args, "message");
    const json_t *from_user = tg3_getattr(msg, "from_user");
    if (tg3_is_null(from_user)) return json_bool(false);
    const json_t *bot_id = tg3_getattr(bot, "id");
    const json_t *user_id = tg3_getattr(from_user, "id");
    bool ok = tg3_present(bot_id) && tg3_present(user_id) &&
              (bot_id->type == JSON_NUMBER && user_id->type == JSON_NUMBER &&
               bot_id->num_val == user_id->num_val);
    return json_bool(ok);
}

/* PoP: tg3_sender_is_other_bot @ plugins/platforms/telegram/adapter.py:_sender_is_other_bot */
static json_t *tg3_sender_is_other_bot(tg3_ctx_t *ctx, const json_t *args) {
    const json_t *msg = tg3_arg(args, "message");
    const json_t *sender = tg3_getattr(msg, "from_user");
    if (tg3_is_null(sender)) return json_bool(false);
    const json_t *is_bot = tg3_getattr(sender, "is_bot");
    if (!tg3_present(is_bot) || !tg3_truthy(is_bot)) return json_bool(false);
    json_t *err = NULL;
    const json_t *bot = tg3_attr(ctx->state, "_bot", "TelegramAdapter", &err);
    if (err) return err;
    const json_t *bot_id = tg3_getattr(bot, "id");
    const json_t *sid = tg3_getattr(sender, "id");
    bool ok = tg3_is_null(bot_id) || tg3_is_null(sid) ||
              !(sid->type == JSON_NUMBER && bot_id->type == JSON_NUMBER &&
                sid->num_val == bot_id->num_val);
    return json_bool(ok);
}

/* PoP: tg3_media_message_type @ plugins/platforms/telegram/adapter.py:_media_message_type */
static json_t *tg3_media_message_type(tg3_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *msg = tg3_arg(args, "msg");
    static const char *const keys[] = {"sticker", "photo", "video", "video_note", "audio", "voice"};
    static const char *const vals[] = {"sticker", "photo", "video", "video", "audio", "voice"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        const json_t *v = tg3_getattr(msg, keys[i]);
        if (tg3_truthy(v)) return json_string(vals[i]);
    }
    return json_string("document");
}

/* PoP: tg3_text_batch_delay_for @ plugins/platforms/telegram/adapter.py:_text_batch_delay_for */
static json_t *tg3_text_batch_delay_for(tg3_ctx_t *ctx, const json_t *args) {
    const json_t *pending = tg3_arg(args, "pending");
    int last_len = 0, total_len = 0;
    if (tg3_present(pending) && pending->type == JSON_OBJECT) {
        const json_t *ll = tg3_getattr(pending, "_last_chunk_len");
        if (ll && ll->type == JSON_NUMBER) last_len = (int)ll->num_val;
        const json_t *tx = tg3_getattr(pending, "text");
        if (tx && tx->type == JSON_STRING) total_len = (int)tg3_cp_len(tx->str_val);
    }
    json_t *err = NULL;
    if ((double)last_len >= TG3_SPLIT_THRESHOLD) {
        const json_t *v = tg3_attr(ctx->state, "_text_batch_split_delay_seconds",
                                   "TelegramAdapter", &err);
        return err ? err : json_copy(v);
    }
    const json_t *cap = tg3_attr(ctx->state, "_text_batch_delay_seconds",
                                 "TelegramAdapter", &err);
    if (err) return err;
    double base = (cap && cap->type == JSON_NUMBER) ? cap->num_val : 0.0;
    if ((double)total_len <= TG3_TEXT_BATCH_FAST_LEN) {
        double m = base < TG3_TEXT_BATCH_FAST_DELAY_S ? base : TG3_TEXT_BATCH_FAST_DELAY_S;
        return json_number(m);
    }
    if ((double)total_len <= TG3_TEXT_BATCH_SHORT_LEN) {
        double m = base < TG3_TEXT_BATCH_SHORT_DELAY_S ? base : TG3_TEXT_BATCH_SHORT_DELAY_S;
        return json_number(m);
    }
    return json_number(base);
}

/* PoP: tg3_env_float_clamped @ plugins/platforms/telegram/adapter.py:_env_float_clamped */
static json_t *tg3_env_float_clamped(tg3_ctx_t *ctx, const json_t *args) {
    const json_t *name = tg3_arg(args, "name");
    const json_t *dflt = tg3_arg(args, "default");
    const json_t *minv = tg3_arg(args, "min_value");
    const json_t *maxv = tg3_arg(args, "max_value");
    double dv = (dflt && dflt->type == JSON_NUMBER) ? dflt->num_val : 0.0;

    json_t *a = json_object();
    json_set(a, "name", name ? json_copy(name) : json_null());
    json_t *raw = tg3_call(ctx, "os.getenv", a); /* OUTSIDE the try */
    if (tg3_is_raise(raw)) return raw;

    json_t *value;
    if (tg3_is_null(raw)) {
        value = json_number(dv); /* float(default) */
    } else {
        value = tg3_pyfloat(raw);
        if (tg3_is_raise(value)) {
            /* except (TypeError, ValueError): FULL builtin hierarchy filter */
            if (tg3_is_type_or_value(tg3_raise_kind(value))) {
                json_free(value);
                value = json_number(dv);
            } else {
                json_free(raw);
                return value;
            }
        }
    }
    json_free(raw);
    if (!isfinite(value->num_val)) { json_free(value); value = json_number(dv); }
    double v = value->num_val;
    if (tg3_present(minv) && minv->type == JSON_NUMBER && v < minv->num_val) v = minv->num_val;
    if (tg3_present(maxv) && maxv->type == JSON_NUMBER && v > maxv->num_val) v = maxv->num_val;
    json_free(value);
    return json_number(v);
}

/* PoP: tg3_redact_telegram_error_text @ plugins/platforms/telegram/adapter.py:_redact_telegram_error_text */
static json_t *tg3_redact_telegram_error_text(tg3_ctx_t *ctx, const json_t *args) {
    const json_t *proxy = tg3_arg(args, "proxy_url");
    const char *eid = tg3_str(tg3_arg(args, "error"));
    const json_t *erec = tg3_err_obj(ctx, eid);
    bool is_exc = (erec != NULL && erec->type == JSON_OBJECT);

    if (tg3_truthy(proxy) && is_exc) {
        json_t *a = json_object();
        json_t *eo = json_object();
        const char *cls = tg3_err_class(ctx, eid);
        json_set_hdr(eo, "class", cls ? cls : "Exception");
        json_set_hdr(eo, "message", tg3_err_text(ctx, eid));
        json_set(a, "error", eo);
        json_set(a, "proxy_url", proxy ? json_copy(proxy) : json_null());
        json_t *shown = tg3_call(ctx, "describe_proxy_error", a);
        if (tg3_is_raise(shown)) return shown;
        if (tg3_truthy(shown)) return shown;
        json_free(shown);
    }

    char *text;
    if (!is_exc) text = tg3_strdup("");
    else text = tg3_strdup(tg3_err_text(ctx, eid));
    if (!text || text[0] == '\0') {
        free(text);
        if (is_exc) {
            const char *cls = tg3_err_class(ctx, eid);
            char msg[256];
            snprintf(msg, sizeof msg, "<%s>", cls ? cls : "Exception");
            return json_string(msg);
        }
        return json_string("");
    }

    json_t *a = json_object();
    json_set(a, "text", json_string(text));
    json_set(a, "force", json_bool(true));
    free(text);
    json_t *r = tg3_call(ctx, "redact_sensitive_text", a);
    if (tg3_is_raise(r)) {
        /* except Exception: full hierarchy; BaseException-only propagates */
        if (tg3_is_exception(tg3_raise_kind(r))) {
            json_free(r);
            return json_string("<telegram error redacted>");
        }
        return r;
    }
    return r;
}
