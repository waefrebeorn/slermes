/*
 * relay_descriptor_flow4.c — standalone FLOW differential port of the
 * gateway/relay/descriptor.py REMAINDER beyond flow3 (CapabilityDescriptor).
 *
 * CRLF-aware PoP grep (src/**.c, binary CRLF count confirmed) authority:
 *   base relay_descriptor.c claims from_platform_entry (stale; no longer in
 *     the quarry source), from_json, supports_op, to_json;
 *   relay_descriptor_flow3.c claims __init__, __repr__, __eq__ with qualified
 *     PoPs (base/module-level claims stay in force untouched).
 *   NO PoP anywhere claims CapabilityDescriptor.__hash__ -- the frozen
 *     dataclass's generated hash is the ONLY unclaimed surface beyond flow3.
 * flow3 excluded it as "field hash-randomised, out of the cross-process
 * domain". This TU ports it faithfully by SEAMING the `hash()` call that the
 * generated __hash__ performs: the C port never computes CPython's seed, it
 * fires the `hash` seam with the declaration-ordered field tuple and returns
 * the scripted result (or propagates a scripted raise). Per
 * flow2-remainder-scoping.md this standalone oracle ports the whole module
 * with a qualified PoP on the unclaimed generated method and RE-DERIVES the
 * claimed siblings inside this TU for the differential check.
 *
 * PoP: rd4_hash @ gateway/relay/descriptor.py:CapabilityDescriptor.__hash__
 * (rd4_init/repr/eq/supports_op/to_json/from_json are RE-DERIVED here without
 *  a PoP line: base port / flow3 already own those Python names.)
 *
 * Self-contained C11, libjson-only, harness-#included. Every foreign effect
 * enters through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * `args` is borrowed; the result is a NEW json_t* (NULL == None,
 * {"__raise__": cls, "msg": ...} == a raised exception). The seams used are
 * `int_cast` (from_json) and `hash` (the generated __hash__).
 *
 * FULL builtin exception hierarchy (py3.14.9 audited against builtins): every
 * BaseException/Exception subclass below, with aliases (IOError,
 * EnvironmentError, WindowsError) mapping to OSError, py3.14 additions
 * (PythonFinalizationError->RuntimeError, _IncompleteInputError->SyntaxError,
 * EOFError, ExceptionGroup->Exception for the except-Exception contract),
 * and the BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit)
 * never caught by except (TypeError, ValueError). Mirrors the oracle's
 * BUILTIN_EXC / SHADOW exc table.
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
typedef struct rd4_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} rd4_seams_t;

static json_t *rd4_pending = NULL;

static bool rd4_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool rd4_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool rd4_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static const char *rd4_str(const json_t *v) {
    return (rd4_is_str(v) && v->str_val) ? v->str_val : NULL;
}

static char *rd4_xsf(const char *fmt, ...) {
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

static json_t *rd4_call(const rd4_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static const char *rd4_raise_class(const json_t *r) {
    if (rd4_is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (rd4_is_str(c)) return c->str_val;
    }
    return NULL;
}
#define RD4_RAISED(r) (rd4_raise_class(r) != NULL)
static void rd4_park(json_t *r) { rd4_pending = r; }
static json_t *rd4_take_pending(void) { json_t *r = rd4_pending; rd4_pending = NULL; return r; }
static json_t *rd4_make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static void rd4_park_new(const char *cls, const char *msg) {
    rd4_park(rd4_make_raise(cls, msg));
}

/* ── FULL builtin exception hierarchy (mirrors oracle BUILTIN_EXC) ── */
static const char *rd4_exc_parent(const char *c) {
    /* py3.14.9 builtins audit: name -> bases[0] (aliases -> OSError;
     * ExceptionGroup routed through Exception for except-Exception parity). */
    static const struct { const char *c, *p; } T[] = {
        {"BaseException", NULL},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"BaseExceptionGroup", "BaseException"},
        {"Exception", "BaseException"},
        {"ArithmeticError", "Exception"}, {"ZeroDivisionError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"FloatingPointError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"AttributeError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"ExceptionGroup", "Exception"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"MemoryError", "Exception"},
        {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"OSError", "Exception"},
        {"IOError", "OSError"}, {"EnvironmentError", "OSError"}, {"WindowsError", "OSError"},
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"ConnectionError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"ReferenceError", "Exception"},
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"}, {"PythonFinalizationError", "RuntimeError"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"}, {"_IncompleteInputError", "SyntaxError"},
        {"SystemError", "Exception"},
        {"TypeError", "Exception"},
        {"ValueError", "Exception"}, {"JSONDecodeError", "ValueError"},
        {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        {"Warning", "Exception"},
        {"UserWarning", "Warning"}, {"DeprecationWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"}, {"SyntaxWarning", "Warning"},
        {"RuntimeWarning", "Warning"}, {"FutureWarning", "Warning"},
        {"ImportWarning", "Warning"}, {"UnicodeWarning", "Warning"},
        {"BytesWarning", "Warning"}, {"ResourceWarning", "Warning"},
        {"EncodingWarning", "Warning"},
    };
    if (!c) return NULL;
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (!strcmp(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool rd4_exc_matches(const json_t *spec, const char *const *classes, size_t n) {
    const char *cls = rd4_raise_class(spec);
    if (!cls) return false;
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (!strcmp(cls, classes[i])) return true;
        cls = rd4_exc_parent(cls);
        if (!cls) break;
    }
    return false;
}

/* ── field model (a descriptor is a json_t object of field -> value) ── */
static const char *RD4_KNOWN[] = {
    "contract_version", "platform", "label", "max_message_length",
    "supports_draft_streaming", "supports_edit", "supports_threads",
    "markdown_dialect", "len_unit", "emoji", "platform_hint", "pii_safe",
    "supports_context", "supports_inchannel_continuable",
    "supports_block_formatting", "supported_ops"
};
static const char *RD4_DECL_ORDER[] = {
    "contract_version", "platform", "label", "max_message_length",
    "supports_draft_streaming", "supports_edit", "supports_threads",
    "markdown_dialect", "len_unit", "emoji", "platform_hint", "pii_safe",
    "supports_context", "supports_inchannel_continuable",
    "supports_block_formatting", "supported_ops"
};
static const char *RD4_LEGACY_OPS[] = {"send", "edit", "typing", "follow_up"};

static bool rd4_is_known(const char *k) {
    if (!k) return false;
    for (size_t i = 0; i < sizeof(RD4_KNOWN)/sizeof(RD4_KNOWN[0]); i++)
        if (!strcmp(k, RD4_KNOWN[i])) return true;
    return false;
}

static json_t *rd4_defaults(void) {
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
static json_t *rd4_init(const json_t *fields) {
    json_t *d = rd4_defaults();
    if (rd4_is_obj(fields)) {
        for (size_t i = 0; i < fields->c.count; i++) {
            const char *k = fields->c.keys[i];
            if (rd4_is_known(k)) json_set(d, k, json_copy(fields->c.items[i]));
        }
    }
    return d;
}

/* supports_op — `op in (self.supported_ops or self.LEGACY_OPS)`. Empty ops
 * (the frozen default tuple) falls back to the legacy op set. */
static bool rd4_supports_op(const json_t *desc, const char *op) {
    if (!op) return false;
    const json_t *ops = desc ? json_obj_get(desc, "supported_ops") : NULL;
    if (rd4_is_arr(ops) && ops->c.count > 0) {
        for (size_t i = 0; i < ops->c.count; i++) {
            const char *s = rd4_str(ops->c.items[i]);
            if (s && !strcmp(s, op)) return true;
        }
        return false;
    }
    for (size_t i = 0; i < sizeof(RD4_LEGACY_OPS)/sizeof(RD4_LEGACY_OPS[0]); i++)
        if (!strcmp(op, RD4_LEGACY_OPS[i])) return true;
    return false;
}

/* ── Python json.dumps(sort_keys=True, ensure_ascii=False) serializer ── */
typedef struct { char *p; size_t n, cap; } rd4_buf;
static void rd4_put(rd4_buf *b, const char *s, size_t len) {
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
static void rd4_puts(rd4_buf *b, const char *s) { rd4_put(b, s, strlen(s)); }
static void rd4_putc(rd4_buf *b, char c) { rd4_put(b, &c, 1); }
static void rd4_jstr(rd4_buf *b, const char *s) {
    rd4_putc(b, '"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"':  rd4_puts(b, "\\\""); break;
            case '\\': rd4_puts(b, "\\\\"); break;
            case '\b': rd4_puts(b, "\\b");  break;
            case '\f': rd4_puts(b, "\\f");  break;
            case '\n': rd4_puts(b, "\\n");  break;
            case '\r': rd4_puts(b, "\\r");  break;
            case '\t': rd4_puts(b, "\\t");  break;
            default:
                if (c < 0x20) { char t[8]; snprintf(t, sizeof t, "\\u%04x", c); rd4_puts(b, t); }
                else rd4_putc(b, (char)c);  /* ensure_ascii=False: raw UTF-8 */
        }
    }
    rd4_putc(b, '"');
}
static int rd4_cmp_key(const void *a, const void *b) {
    return strcmp(*(const char * const *)a, *(const char * const *)b);
}
static void rd4_dumps_rec(rd4_buf *b, const json_t *v) {
    if (!v) { rd4_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_NULL:   rd4_puts(b, "null"); break;
        case JSON_BOOL:   rd4_puts(b, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char t[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(t, sizeof t, "%.0f", v->num_val);
            else
                snprintf(t, sizeof t, "%.17g", v->num_val);
            rd4_puts(b, t); break;
        }
        case JSON_STRING: rd4_jstr(b, v->str_val); break;
        case JSON_ARRAY:
            rd4_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) rd4_puts(b, ", ");
                rd4_dumps_rec(b, v->c.items[i]);
            }
            rd4_putc(b, ']');
            break;
        case JSON_OBJECT: {
            rd4_putc(b, '{');
            size_t n = v->c.count;
            const char **keys = n ? malloc(n * sizeof(const char *)) : NULL;
            for (size_t i = 0; i < n; i++) keys[i] = v->c.keys[i];
            if (n) qsort(keys, n, sizeof(const char *), rd4_cmp_key);
            for (size_t i = 0; i < n; i++) {
                if (i) rd4_puts(b, ", ");
                rd4_jstr(b, keys[i]);
                rd4_puts(b, ": ");
                rd4_dumps_rec(b, json_obj_get(v, keys[i]));
            }
            free(keys);
            rd4_putc(b, '}');
            break;
        }
    }
}

/* to_json — json.dumps(asdict(self), sort_keys=True, ensure_ascii=False). */
static char *rd4_to_json(const json_t *desc) {
    rd4_buf b = {0};
    rd4_dumps_rec(&b, desc);
    char *r = b.p ? b.p : strdup("");
    return r;
}

/* ── Python repr helpers (frozen-dataclass __repr__) ── */
static void rd4_pystr(rd4_buf *b, const char *s) {
    rd4_putc(b, '\'');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        unsigned char c = *p;
        if (c == '\'' || c == '\\') { rd4_putc(b, '\\'); rd4_putc(b, (char)c); }
        else if (c == '\n') rd4_puts(b, "\\n");
        else if (c == '\r') rd4_puts(b, "\\r");
        else if (c == '\t') rd4_puts(b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char t[8]; snprintf(t, sizeof t, "\\x%02x", c); rd4_puts(b, t); }
        else rd4_putc(b, (char)c);
    }
    rd4_putc(b, '\'');
}
static void rd4_pyval(rd4_buf *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { rd4_puts(b, "None"); return; }
    switch (v->type) {
        case JSON_BOOL:   rd4_puts(b, v->bool_val ? "True" : "False"); break;
        case JSON_NUMBER: {
            char t[64];
            if (v->num_val == (double)(long long)v->num_val) snprintf(t, sizeof t, "%.0f", v->num_val);
            else snprintf(t, sizeof t, "%.17g", v->num_val);
            rd4_puts(b, t); break;
        }
        case JSON_STRING: rd4_pystr(b, v->str_val); break;
        default: rd4_puts(b, "None"); break;
    }
}

/* __repr__ — declaration order; supported_ops is a tuple when it is the
 * frozen default (ops_tuple=true) and a list when the fixture supplied it. */
static char *rd4_repr(const json_t *desc, bool ops_tuple) {
    rd4_buf b = {0};
    rd4_puts(&b, "CapabilityDescriptor(");
    for (size_t i = 0; i < sizeof(RD4_DECL_ORDER)/sizeof(RD4_DECL_ORDER[0]); i++) {
        const char *k = RD4_DECL_ORDER[i];
        if (i) rd4_puts(&b, ", ");
        rd4_puts(&b, k);
        rd4_putc(&b, '=');
        if (!strcmp(k, "supported_ops")) {
            const json_t *arr = json_obj_get(desc, k);
            size_t n = rd4_is_arr(arr) ? arr->c.count : 0;
            rd4_putc(&b, ops_tuple ? '(' : '[');
            for (size_t j = 0; j < n; j++) {
                if (j) rd4_puts(&b, ", ");
                rd4_pystr(&b, rd4_str(arr->c.items[j]));
            }
            if (ops_tuple && n == 1) rd4_putc(&b, ',');
            rd4_putc(&b, ops_tuple ? ')' : ']');
        } else {
            rd4_pyval(&b, json_obj_get(desc, k));
        }
    }
    rd4_putc(&b, ')');
    return b.p ? b.p : strdup("");
}

/* ── deep JSON equality (dataclass __eq__ over field values) ── */
static bool rd4_jeq(const json_t *a, const json_t *b) {
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
                if (!rd4_jeq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !rd4_jeq(a->c.items[i], bv)) return false;
            }
            return true;
    }
    return false;
}
static bool rd4_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return false;
    for (size_t i = 0; i < sizeof(RD4_KNOWN)/sizeof(RD4_KNOWN[0]); i++) {
        if (!rd4_jeq(json_obj_get(a, RD4_KNOWN[i]), json_obj_get(b, RD4_KNOWN[i])))
            return false;
    }
    return true;
}

static const char *rd4_type_name(const json_t *v) {
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

/* CapabilityDescriptor.__hash__ — the frozen-dataclass generated hash.
 * The live generated method calls hash((f1, f2, ..., f16)) in declaration
 * order; that `hash` is a seam so the C port never needs CPython's per-process
 * hash seed. Build the declaration-ordered field tuple as the seam args,
 * fire `hash`, and return the scripted result (or park a scripted raise). */
/* PoP: rd4_hash @ gateway/relay/descriptor.py:CapabilityDescriptor.__hash__ */
static json_t *rd4_hash(const json_t *desc, const rd4_seams_t *sp) {
    json_t *args = json_object();
    json_t *arr = json_array();
    for (size_t i = 0; i < sizeof(RD4_DECL_ORDER)/sizeof(RD4_DECL_ORDER[0]); i++)
        json_append(arr, json_copy(json_obj_get(desc, RD4_DECL_ORDER[i])));
    json_set(args, "fields", arr);
    json_t *r = rd4_call(sp, "hash", args);
    json_free(args);
    if (RD4_RAISED(r)) { rd4_park(r); return NULL; }
    /* CPython's slot_hash requires __hash__ to return an int: a scripted
     * non-int result (e.g. null) raises TypeError before hash() returns. */
    if (!r || r->type != JSON_NUMBER ||
        r->num_val != (double)(long long)r->num_val) {
        if (r) json_free(r);
        rd4_park_new("TypeError", "__hash__ method should return an integer");
        return NULL;
    }
    return r;
}

/* ── Python-faithful json.JSONDecodeError message (see rd3_jerr_msg) ──
 * Recompute CPython's "Expecting value: line L column C (char N)" /
 * "Extra data: line L column C (char N)" from the decoded data text; libjson's
 * own parse error carries no position. A prefix-scan finds the first complete
 * value; a non-whitespace tail is "Extra data", else "Expecting value". */
static void rd4_jerr_msg(const char *text, char *buf, size_t bufsz) {
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
static json_t *rd4_from_json(const char *data, const rd4_seams_t *sp) {
    char *err = NULL;
    json_t *raw = json_parse(data ? data : "", &err);
    if (err) free(err);
    if (!raw) {
        /* malformed JSON -> json.JSONDecodeError propagates with CPython's
         * exact message (notably trailing data -> "Extra data: ..."). */
        char mbuf[192];
        rd4_jerr_msg(data, mbuf, sizeof mbuf);
        rd4_park_new("JSONDecodeError", mbuf);
        return NULL;
    }
    if (raw->type != JSON_OBJECT) {
        char *m = rd4_xsf("'%s' object has no attribute 'items'", rd4_type_name(raw));
        rd4_park_new("AttributeError", m);
        free(m);
        json_free(raw);
        return NULL;
    }
    json_t *filtered = json_object();
    for (size_t i = 0; i < raw->c.count; i++) {
        const char *k = raw->c.keys[i];
        if (rd4_is_known(k)) json_set(filtered, k, json_copy(raw->c.items[i]));
    }
    json_free(raw);

    const json_t *mml = json_obj_get(filtered, "max_message_length");
    if (mml) {
        json_t *args = json_object();
        json_set(args, "value", json_copy(mml));
        json_t *r = rd4_call(sp, "int_cast", args);
        json_free(args);
        if (RD4_RAISED(r)) {
            const char *const CATCH[] = {"TypeError", "ValueError"};
            if (rd4_exc_matches(r, CATCH, 2)) {
                json_set(filtered, "max_message_length", json_number(4096));
                json_free(r);
            } else {
                rd4_park(r);
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
        if (rd4_is_arr(so)) {
            for (size_t i = 0; i < so->c.count; i++) {
                const char *s = rd4_str(so->c.items[i]);
                if (s && s[0]) json_append(arr, json_copy(so->c.items[i]));
            }
        }
        json_set(filtered, "supported_ops", arr);
    }

    json_t *d = rd4_init(filtered);
    json_free(filtered);
    return d;
}