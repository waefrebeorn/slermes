/*
 * relay_descriptor_flow3.c — standalone FLOW differential port of the
 * gateway/relay/descriptor.py remainder (CapabilityDescriptor).
 *
 * CRLF-aware PoP grep authority: the base port (src/gateway/relay/
 * relay_descriptor.c) claims the three CONCRETE methods (from_json,
 * supports_op, to_json). There is no relay_descriptor_flow2 file. The
 * genuinely UNCLAIMED remainder is the frozen-dataclass surface that has no
 * AST FunctionDef: __init__, __repr__, __eq__ (frozen -> __hash__ is field
 * hash-randomised and out of the cross-process domain). Per
 * flow2-remainder-scoping.md this standalone FLOW oracle ports the whole
 * module with QUALIFIED PoPs on the unclaimed generated methods and RE-DERIVES
 * the claimed siblings inside this TU for the differential check; the older
 * claims stay in force untouched.
 *
 * PoP: rd3_init       @ gateway/relay/descriptor.py:CapabilityDescriptor.__init__
 * PoP: rd3_repr       @ gateway/relay/descriptor.py:CapabilityDescriptor.__repr__
 * PoP: rd3_eq         @ gateway/relay/descriptor.py:CapabilityDescriptor.__eq__
 * (rd3_supports_op / rd3_to_json / rd3_from_json are RE-DERIVED here without a
 *  PoP line: the base port already owns those Python names.)
 *
 * Self-contained C11, libjson-only, harness-#included. Every foreign effect
 * enters through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * `args` is borrowed; the result is a NEW json_t* (NULL == None,
 * {"__raise__": cls, "msg": ...} == a raised exception). The only seam used is
 * `int_cast` (the int() inside from_json's try/except (TypeError, ValueError)).
 *
 * FULL builtin exception hierarchy: the except (TypeError, ValueError) site
 * walks every parent below; TypeError/ValueError and their subclasses
 * (JSONDecodeError, UnicodeError family) are swallowed, every other Exception
 * passes, and the BaseException trio (SystemExit/KeyboardInterrupt/
 * GeneratorExit) is never caught. Mirrors the oracle's BUILTIN_EXC table.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "json.h"

/* ── universal seam plumbing ── */
typedef struct rd3_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} rd3_seams_t;

static json_t *rd3_pending = NULL;

static bool rd3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool rd3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool rd3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static const char *rd3_str(const json_t *v) {
    return (rd3_is_str(v) && v->str_val) ? v->str_val : NULL;
}

static char *rd3_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

static json_t *rd3_call(const rd3_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static const char *rd3_raise_class(const json_t *r) {
    if (rd3_is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (rd3_is_str(c)) return c->str_val;
    }
    return NULL;
}
#define RD3_RAISED(r) (rd3_raise_class(r) != NULL)
static void rd3_park(json_t *r) { rd3_pending = r; }
static json_t *rd3_take_pending(void) { json_t *r = rd3_pending; rd3_pending = NULL; return r; }
static json_t *rd3_make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static void rd3_park_new(const char *cls, const char *msg) {
    rd3_park(rd3_make_raise(cls, msg));
}

/* ── FULL builtin exception hierarchy (mirrors oracle BUILTIN_EXC) ── */
static const char *rd3_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"BaseException", NULL},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"Exception", "BaseException"},
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"ValueError", "Exception"}, {"TypeError", "Exception"},
        {"AttributeError", "Exception"}, {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"ArithmeticError", "Exception"}, {"ZeroDivisionError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"FloatingPointError", "ArithmeticError"},
        {"OSError", "Exception"},
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"ConnectionError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"JSONDecodeError", "ValueError"}, {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"AssertionError", "Exception"}, {"SystemError", "Exception"},
        {"ReferenceError", "Exception"}, {"MemoryError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"ResourceWarning", "Warning"}, {"EncodingWarning", "Warning"},
    };
    if (!c) return NULL;
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (!strcmp(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool rd3_exc_matches(const json_t *spec, const char *const *classes, size_t n) {
    const char *cls = rd3_raise_class(spec);
    if (!cls) return false;
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (!strcmp(cls, classes[i])) return true;
        cls = rd3_exc_parent(cls);
        if (!cls) break;
    }
    return false;
}

/* ── field model (a descriptor is a json_t object of field -> value) ── */
static const char *RD3_KNOWN[] = {
    "contract_version", "platform", "label", "max_message_length",
    "supports_draft_streaming", "supports_edit", "supports_threads",
    "markdown_dialect", "len_unit", "emoji", "platform_hint", "pii_safe",
    "supports_context", "supports_inchannel_continuable",
    "supports_block_formatting", "supported_ops"
};
static const char *RD3_DECL_ORDER[] = {
    "contract_version", "platform", "label", "max_message_length",
    "supports_draft_streaming", "supports_edit", "supports_threads",
    "markdown_dialect", "len_unit", "emoji", "platform_hint", "pii_safe",
    "supports_context", "supports_inchannel_continuable",
    "supports_block_formatting", "supported_ops"
};
static const char *RD3_LEGACY_OPS[] = {"send", "edit", "typing", "follow_up"};

static bool rd3_is_known(const char *k) {
    if (!k) return false;
    for (size_t i = 0; i < sizeof(RD3_KNOWN)/sizeof(RD3_KNOWN[0]); i++)
        if (!strcmp(k, RD3_KNOWN[i])) return true;
    return false;
}

static json_t *rd3_defaults(void) {
    json_t *d = json_object();
    /* emoji default is U+1F50C (🔌) as raw UTF-8, ensure_ascii=False. */
    json_set(d, "emoji", json_string("\xF0\x9F\x94\x8C"));
    json_set(d, "platform_hint", json_string(""));
    json_set(d, "pii_safe", json_bool(false));
    json_set(d, "supports_context", json_bool(false));
    json_set(d, "supports_inchannel_continuable", json_bool(false));
    json_set(d, "supports_block_formatting", json_bool(false));
    json_set(d, "supported_ops", json_array());
    return d;
}

/* CapabilityDescriptor.__init__ — absent fields take defaults, present values
 * are stored verbatim (NO coercion). The fixture domain supplies all required
 * fields; unknown-keyword / missing-required binding errors are out of domain. */
/* PoP: rd3_init @ gateway/relay/descriptor.py:CapabilityDescriptor.__init__ */
static json_t *rd3_init(const json_t *fields) {
    json_t *d = rd3_defaults();
    if (rd3_is_obj(fields)) {
        for (size_t i = 0; i < fields->c.count; i++) {
            const char *k = fields->c.keys[i];
            if (rd3_is_known(k)) json_set(d, k, json_copy(fields->c.items[i]));
        }
    }
    return d;
}

/* supports_op — `op in (self.supported_ops or self.LEGACY_OPS)`. Empty ops
 * (the frozen default tuple) falls back to the legacy op set. */
static bool rd3_supports_op(const json_t *desc, const char *op) {
    if (!op) return false;
    const json_t *ops = desc ? json_obj_get(desc, "supported_ops") : NULL;
    if (rd3_is_arr(ops) && ops->c.count > 0) {
        for (size_t i = 0; i < ops->c.count; i++) {
            const char *s = rd3_str(ops->c.items[i]);
            if (s && !strcmp(s, op)) return true;
        }
        return false;
    }
    for (size_t i = 0; i < sizeof(RD3_LEGACY_OPS)/sizeof(RD3_LEGACY_OPS[0]); i++)
        if (!strcmp(op, RD3_LEGACY_OPS[i])) return true;
    return false;
}

/* ── Python json.dumps(sort_keys=True, ensure_ascii=False) serializer ── */
typedef struct { char *p; size_t n, cap; } rd3_buf;
static void rd3_put(rd3_buf *b, const char *s, size_t len) {
    if (b->n + len + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->n + len + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, len);
    b->n += len; b->p[b->n] = '\0';
}
static void rd3_puts(rd3_buf *b, const char *s) { rd3_put(b, s, strlen(s)); }
static void rd3_putc(rd3_buf *b, char c) { rd3_put(b, &c, 1); }
static void rd3_jstr(rd3_buf *b, const char *s) {
    rd3_putc(b, '"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"':  rd3_puts(b, "\\\""); break;
            case '\\': rd3_puts(b, "\\\\"); break;
            case '\b': rd3_puts(b, "\\b");  break;
            case '\f': rd3_puts(b, "\\f");  break;
            case '\n': rd3_puts(b, "\\n");  break;
            case '\r': rd3_puts(b, "\\r");  break;
            case '\t': rd3_puts(b, "\\t");  break;
            default:
                if (c < 0x20) { char t[8]; snprintf(t, sizeof t, "\\u%04x", c); rd3_puts(b, t); }
                else rd3_putc(b, (char)c);  /* ensure_ascii=False: raw UTF-8 */
        }
    }
    rd3_putc(b, '"');
}
static int rd3_cmp_key(const void *a, const void *b) {
    return strcmp(*(const char * const *)a, *(const char * const *)b);
}
static void rd3_dumps_rec(rd3_buf *b, const json_t *v) {
    if (!v) { rd3_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_NULL:   rd3_puts(b, "null"); break;
        case JSON_BOOL:   rd3_puts(b, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char t[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(t, sizeof t, "%.0f", v->num_val);
            else
                snprintf(t, sizeof t, "%.17g", v->num_val);
            rd3_puts(b, t); break;
        }
        case JSON_STRING: rd3_jstr(b, v->str_val); break;
        case JSON_ARRAY:
            rd3_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) rd3_puts(b, ", ");
                rd3_dumps_rec(b, v->c.items[i]);
            }
            rd3_putc(b, ']');
            break;
        case JSON_OBJECT: {
            rd3_putc(b, '{');
            size_t n = v->c.count;
            const char **keys = n ? malloc(n * sizeof(const char *)) : NULL;
            for (size_t i = 0; i < n; i++) keys[i] = v->c.keys[i];
            if (n) qsort(keys, n, sizeof(const char *), rd3_cmp_key);
            for (size_t i = 0; i < n; i++) {
                if (i) rd3_puts(b, ", ");
                rd3_jstr(b, keys[i]);
                rd3_puts(b, ": ");
                rd3_dumps_rec(b, json_obj_get(v, keys[i]));
            }
            free(keys);
            rd3_putc(b, '}');
            break;
        }
    }
}

/* to_json — json.dumps(asdict(self), sort_keys=True, ensure_ascii=False). */
static char *rd3_to_json(const json_t *desc) {
    rd3_buf b = {0};
    rd3_dumps_rec(&b, desc);
    char *r = b.p ? b.p : strdup("");
    return r;
}

/* ── Python repr helpers (frozen-dataclass __repr__) ── */
static void rd3_pystr(rd3_buf *b, const char *s) {
    rd3_putc(b, '\'');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        if (c == '\'' || c == '\\') { rd3_putc(b, '\\'); rd3_putc(b, (char)c); }
        else if (c == '\n') rd3_puts(b, "\\n");
        else if (c == '\r') rd3_puts(b, "\\r");
        else if (c == '\t') rd3_puts(b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char t[8]; snprintf(t, sizeof t, "\\x%02x", c); rd3_puts(b, t); }
        else rd3_putc(b, (char)c);
    }
    rd3_putc(b, '\'');
}
static void rd3_pyval(rd3_buf *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { rd3_puts(b, "None"); return; }
    switch (v->type) {
        case JSON_BOOL:   rd3_puts(b, v->bool_val ? "True" : "False"); break;
        case JSON_NUMBER: {
            char t[64];
            if (v->num_val == (double)(long long)v->num_val) snprintf(t, sizeof t, "%.0f", v->num_val);
            else snprintf(t, sizeof t, "%.17g", v->num_val);
            rd3_puts(b, t); break;
        }
        case JSON_STRING: rd3_pystr(b, v->str_val); break;
        default: rd3_puts(b, "None"); break;
    }
}

/* __repr__ — declaration order; supported_ops is a tuple when it is the
 * frozen default (ops_tuple=true) and a list when the fixture supplied it. */
/* PoP: rd3_repr @ gateway/relay/descriptor.py:CapabilityDescriptor.__repr__ */
static char *rd3_repr(const json_t *desc, bool ops_tuple) {
    rd3_buf b = {0};
    rd3_puts(&b, "CapabilityDescriptor(");
    for (size_t i = 0; i < sizeof(RD3_DECL_ORDER)/sizeof(RD3_DECL_ORDER[0]); i++) {
        const char *k = RD3_DECL_ORDER[i];
        if (i) rd3_puts(&b, ", ");
        rd3_puts(&b, k);
        rd3_putc(&b, '=');
        if (!strcmp(k, "supported_ops")) {
            const json_t *arr = json_obj_get(desc, k);
            size_t n = rd3_is_arr(arr) ? arr->c.count : 0;
            rd3_putc(&b, ops_tuple ? '(' : '[');
            for (size_t j = 0; j < n; j++) {
                if (j) rd3_puts(&b, ", ");
                rd3_pystr(&b, rd3_str(arr->c.items[j]));
            }
            if (ops_tuple && n == 1) rd3_putc(&b, ',');
            rd3_putc(&b, ops_tuple ? ')' : ']');
        } else {
            rd3_pyval(&b, json_obj_get(desc, k));
        }
    }
    rd3_putc(&b, ')');
    return b.p ? b.p : strdup("");
}

/* ── deep JSON equality (dataclass __eq__ over field values) ── */
static bool rd3_jeq(const json_t *a, const json_t *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->type != b->type) {
        /* Python 1 == 1.0: JSON_NUMBER vs JSON_NUMBER only; types equal here. */
        return false;
    }
    switch (a->type) {
        case JSON_NULL:  return true;
        case JSON_BOOL:  return a->bool_val == b->bool_val;
        case JSON_NUMBER:return a->num_val == b->num_val;
        case JSON_STRING:return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!rd3_jeq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !rd3_jeq(a->c.items[i], bv)) return false;
            }
            return true;
    }
    return false;
}
/* PoP: rd3_eq @ gateway/relay/descriptor.py:CapabilityDescriptor.__eq__ */
static bool rd3_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return false;
    for (size_t i = 0; i < sizeof(RD3_KNOWN)/sizeof(RD3_KNOWN[0]); i++) {
        if (!rd3_jeq(json_obj_get(a, RD3_KNOWN[i]), json_obj_get(b, RD3_KNOWN[i])))
            return false;
    }
    return true;
}

static const char *rd3_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* ── Python-faithful json.JSONDecodeError message ──
 * libjson's parse error carries no position, so recompute CPython's
 * "Expecting value: line L column C (char N)" / "Extra data: ..." from the
 * decoded text. Prefix-scan with json_parse finds the end of the first value;
 * any non-whitespace tail after it is "Extra data", otherwise the failure is
 * at the first value position. Used by the unhandled from_json parse site. */
static void rd3_jerr_msg(const char *text, char *buf, size_t bufsz) {
    if (!text) text = "";
    size_t n = strlen(text), pos = 0;
    while (pos < n && (text[pos] == ' ' || text[pos] == '\t' ||
                       text[pos] == '\n' || text[pos] == '\r')) pos++;
    size_t errpos = pos;
    const char *kind = "Expecting value";
    if (pos < n) {
        size_t best = 0;
        for (size_t j = pos + 1; j <= n; j++) {
            char *tmp = malloc(j + 1);
            if (!tmp) break;
            memcpy(tmp, text, j); tmp[j] = '\0';
            char *pe = NULL;
            json_t *v = json_parse(tmp, &pe);
            free(pe);
            bool ok = (v != NULL);
            json_free(v);
            free(tmp);
            if (ok) { best = j; break; }
        }
        if (best) {
            size_t k = best;
            while (k < n && (text[k] == ' ' || text[k] == '\t' ||
                             text[k] == '\n' || text[k] == '\r')) k++;
            if (k < n) { errpos = k; kind = "Extra data"; }
        }
    }
    size_t line = 1, col = 1;
    for (size_t i = 0; i < errpos; i++) {
        if (text[i] == '\n') { line++; col = 1; } else col++;
    }
    snprintf(buf, bufsz, "%s: line %zu column %zu (char %zu)", kind, line, col, errpos);
}

/* from_json — trust-boundary normalization. json.loads -> filter known keys,
 * int() inside try/except (TypeError, ValueError) with FULL hierarchy walk,
 * supported_ops -> tuple of non-empty strings or (), then cls(**filtered). */
static json_t *rd3_from_json(const char *data, const rd3_seams_t *sp) {
    char *err = NULL;
    json_t *raw = json_parse(data ? data : "", &err);
    if (err) free(err);
    if (!raw) {
        /* malformed JSON -> json.JSONDecodeError propagates with CPython's
         * exact message (computed from the decoded data text). */
        char mbuf[192];
        rd3_jerr_msg(data, mbuf, sizeof mbuf);
        rd3_park_new("JSONDecodeError", mbuf);
        return NULL;
    }
    if (raw->type != JSON_OBJECT) {
        char *m = rd3_xsf("'%s' object has no attribute 'items'", rd3_type_name(raw));
        rd3_park_new("AttributeError", m);
        free(m);
        json_free(raw);
        return NULL;
    }
    json_t *filtered = json_object();
    for (size_t i = 0; i < raw->c.count; i++) {
        const char *k = raw->c.keys[i];
        if (rd3_is_known(k)) json_set(filtered, k, json_copy(raw->c.items[i]));
    }
    json_free(raw);

    const json_t *mml = json_obj_get(filtered, "max_message_length");
    if (mml) {
        json_t *args = json_object();
        json_set(args, "value", json_copy(mml));
        json_t *r = rd3_call(sp, "int_cast", args);
        json_free(args);
        if (RD3_RAISED(r)) {
            const char *const CATCH[] = {"TypeError", "ValueError"};
            if (rd3_exc_matches(r, CATCH, 2)) {
                json_set(filtered, "max_message_length", json_number(4096));
                json_free(r);
            } else {
                rd3_park(r);
                json_free(filtered);
                return NULL;
            }
        } else {
            double nv = (r && r->type == JSON_NUMBER) ? r->num_val : 0.0;
            if (r) json_free(r);
            if (nv <= 0)
                json_set(filtered, "max_message_length", json_number(4096));
            /* else: keep the ORIGINAL value verbatim (no coercion). */
        }
    }

    const json_t *so = json_obj_get(filtered, "supported_ops");
    if (so) {
        json_t *arr = json_array();
        if (rd3_is_arr(so)) {
            for (size_t i = 0; i < so->c.count; i++) {
                const char *s = rd3_str(so->c.items[i]);
                if (s && s[0]) json_append(arr, json_copy(so->c.items[i]));
            }
        }
        json_set(filtered, "supported_ops", arr);
    }

    json_t *d = rd3_init(filtered);
    json_free(filtered);
    return d;
}
