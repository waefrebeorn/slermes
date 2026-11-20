/*
 * transport_types_flow2.c — agent/transports/types.py UNCLAIMED remainder (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/agent/transports/types.py
 *
 * Sibling of the CLAIMED transport-type surface. A CRLF-aware PoP grep over src/ +
 * include/ shows these names already claimed (seamed here, NEVER re-ported):
 *   _pd (ToolCall + NormalizedResponse, openai_transport_flow.c), from_openai
 *   (openai_transport_flow.c), type / function / call_id / response_item_id /
 *   extra_content / reasoning_content / reasoning_details / anthropic_content_blocks /
 *   bedrock_content_blocks / codex_reasoning_items / codex_message_items
 *   (transports_types_ports.c + types_gaps3.c), build_tool_call / map_finish_reason
 *   (transport_common.c).
 *
 * What NO PoP claims is the dataclass-GENERATED method surface. Python 3.14's
 * @dataclass synthesizes, per class, __init__ / __repr__ / __eq__ / __replace__
 * (the 3.13+ copy.replace hook). Those 4 x 3 classes = 12 defs are the unclaimed
 * remainder this file ports (qualified Class.fn PoPs so they do not dup the bare-name
 * accessor claims above):
 *
 *   ToolCall.__init__ / __repr__ / __eq__ / __replace__
 *   Usage.__init__ / __repr__ / __eq__ / __replace__
 *   NormalizedResponse.__init__ / __repr__ / __eq__ / __replace__
 *
 * Field layout (declaration order; repr=False fields excluded from __repr__):
 *   ToolCall(id, name, arguments, provider_data[repr=False, default None])
 *   Usage(prompt_tokens=0, completion_tokens=0, total_tokens=0, cached_tokens=0)
 *   NormalizedResponse(content, tool_calls, finish_reason, reasoning=None,
 *                      usage=None, provider_data[repr=False, default None])
 *
 * Self-contained, libjson-only. The ONE live collaborator these methods invoke is the
 * class constructor inside __replace__ (`return self.__class__(**changes)`); it enters
 * through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON object (the
 * seam records a copy and must not free it); the result is a NEW json_t* the caller
 * frees. NULL == Python None (never a JSON_NULL node); {"__raise__": cls, "msg": msg}
 * == a raised exception. The harness records {"ev": name, "args": args, "ret": result}
 * per call in order; the oracle's recording stubs emit the identical shape driving LIVE
 * Python with the same scripts. __init__ / __repr__ / __eq__ are pure (no seam call,
 * empty trace). __replace__ has NO except clause, so BaseException kinds (SystemExit /
 * KeyboardInterrupt / GeneratorExit) AND plain Exceptions alike PASS THROUGH verbatim.
 * With no seam installed the dispatcher is fail-closed (RuntimeError raise spec); the
 * harness script_call is ALSO fail-closed: a MISSING script returns a RuntimeError raise
 * spec (a scripted null is a valid None result).
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__": ...} object == a propagated exception. __init__ and
 * __replace__ return a projection OBJECT of the instance's fields (declaration order);
 * __repr__ returns a JSON string of the live repr(); __eq__ returns a JSON bool of the
 * live `self == other` operation (dataclass __eq__ + the NotImplemented cross-class
 * fallback that makes `==` False).
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   ToolCall.__init__            self.__class__(**changes) inside ToolCall.__replace__
 *                                args{merged kwargs} ret null|raise (ctor returns None;
 *                                the flow ignores the ret and constructs via the REAL
 *                                __init__ port, which validates unknown kwargs)
 *   Usage.__init__               same, for Usage.__replace__
 *   NormalizedResponse.__init__  same, for NormalizedResponse.__replace__
 *
 * Representation contract (fixture domain, identical on both sides):
 *  - ASCII text; integral JSON numbers read as int (libjson has no int/float identity;
 *    integral doubles format int-like, the oracle normalizes integral floats to int).
 *  - ctor fixture kwargs restricted to the declared fields for the DIRECT __init__
 *    cases (no binding-error cases there); a PRESENT null passes through verbatim
 *    (dataclasses do not coerce), an ABSENT field takes its default. __replace__ DOES
 *    probe the unknown-kwarg TypeError path (the real ctor raises it).
 *  - field values are plain JSON (str / int / float / bool / null / list / dict); the
 *    dataclass stores them verbatim, so __repr__ / __eq__ operate on plain Python
 *    values (no nested-dataclass reconstruction needed).
 *  - __repr__ mirrors live repr(): repr=False fields omitted, Python str/float/list/
 *    dict repr formatting, "ClassName(field=value, ...)".
 *  - __eq__ mirrors live `==`: cross-class -> False; same-class -> field-tuple equality
 *    (deep, order-independent for dicts, integral float == int, bool == 0/1).
 *  - __replace__ mirrors the generated loop: merged = passed changes (in passed order)
 *    + each init field absent from changes filled from self (declaration order), then
 *    self.__class__(**merged).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool tt2_is_obj(const json_t *v)  { return v && v->type == JSON_OBJECT; }
static bool tt2_is_arr(const json_t *v)  { return v && v->type == JSON_ARRAY; }
static bool tt2_is_str(const json_t *v)  { return v && v->type == JSON_STRING; }
static bool tt2_is_num(const json_t *v)  { return v && v->type == JSON_NUMBER; }
static bool tt2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool tt2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool tt2_jtruth(const json_t *v) {
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
static char *tt2_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}
/* set a key to a copy of v, or an explicit JSON null when v is NULL/JSON_NULL.
 * (json_set(obj,key,NULL) is a no-op in libjson — never pass NULL.) */
static void tt2_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}

/* ── universal seam ── */
typedef struct tt2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} tt2_seams_t;

static json_t *tt2_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool tt2_is_raise(const json_t *r) {
    return tt2_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *tt2_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
/* Fail-closed dispatcher: no seam installed -> RuntimeError raise spec.
 * Ownership: tt2_call OWNS args (frees it after the call); the seam BORROWS args
 * (records a deep copy, must not free it). Results are NEW (caller frees). */
static json_t *tt2_call(const tt2_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return tt2_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}

/* ── FULL builtin exception hierarchy (child -> parent) for isinstance-style folding.
 *    __replace__ has no except clause, so every kind passes through; the full table is
 *    carried so a scripted subclass name folds into its ancestor exactly as live
 *    isinstance does, and tt2_is_base_exc classifies the BaseException trio. ── */
typedef struct tt2_exc_pair { const char *child; const char *parent; } tt2_exc_pair_t;
static const tt2_exc_pair_t TT2_EXC_TREE[] = {
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"Exception", "BaseException"},
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
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
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
    {"Warning", "Exception"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
};
static const char *tt2_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(TT2_EXC_TREE) / sizeof(TT2_EXC_TREE[0]); i++)
        if (!strcmp(TT2_EXC_TREE[i].child, c)) return TT2_EXC_TREE[i].parent;
    return NULL;
}
/* isinstance(c, ancestor): walk child -> parent up to BaseException. */
static bool tt2_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = tt2_exc_parent(c);
    }
    return false;
}
/* A BaseException kind that is NOT an Exception (SystemExit / KeyboardInterrupt /
 * GeneratorExit / BaseExceptionGroup): these escape `except Exception` sites. */
static bool tt2_is_base_exc(const char *c) {
    return tt2_cls_is(c, "BaseException") && !tt2_cls_is(c, "Exception");
}

/* ── Python repr() of a JSON value (ASCII domain) ── */
static char *tt2_repr_string(const char *s) {
    if (!s) s = "";
    /* Python prefers single quotes; switches to double when the string contains a
     * single quote and no double quote. */
    char quote = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    size_t cap = strlen(s) * 4 + 3;
    char *out = malloc(cap);
    if (!out) return strdup("");
    size_t k = 0;
    out[k++] = quote;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\') { out[k++] = '\\'; out[k++] = '\\'; }
        else if (c == (unsigned char)quote) { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            static const char HEX[] = "0123456789abcdef";
            out[k++] = '\\'; out[k++] = 'x';
            out[k++] = HEX[c >> 4]; out[k++] = HEX[c & 0xf];
        }
        else out[k++] = (char)c;
    }
    out[k++] = quote; out[k] = '\0';
    return out;
}
/* Python float repr: shortest string that round-trips. Integral values format
 * int-like (%.0f) because the fixture domain reads integral JSON numbers as int. */
static char *tt2_repr_double(double d) {
    if (isfinite(d) && d == floor(d) && fabs(d) < 1e16) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f", d);
        return strdup(buf);
    }
    if (!isfinite(d)) {
        if (isnan(d)) return strdup("nan");
        return strdup(d > 0 ? "inf" : "-inf");
    }
    char buf[64];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    return strdup(buf);
}
static char *tt2_repr_value(const json_t *v) {
    if (tt2_is_null(v)) return strdup("None");
    if (tt2_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (tt2_is_num(v))  return tt2_repr_double(v->num_val);
    if (tt2_is_str(v))  return tt2_repr_string(v->str_val);
    if (tt2_is_arr(v)) {
        size_t cap = 3;
        char **parts = calloc(v->c.count ? v->c.count : 1, sizeof(char *));
        for (size_t i = 0; i < v->c.count; i++) {
            parts[i] = tt2_repr_value(v->c.items[i]);
            cap += strlen(parts[i]) + 2;
        }
        char *out = malloc(cap);
        size_t k = 0;
        out[k++] = '[';
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) { out[k++] = ','; out[k++] = ' '; }
            size_t n = strlen(parts[i]); memcpy(out + k, parts[i], n); k += n;
            free(parts[i]);
        }
        out[k++] = ']'; out[k] = '\0';
        free(parts);
        return out;
    }
    /* JSON_OBJECT -> Python dict repr (insertion order). */
    size_t cap = 3;
    char **parts = calloc(v->c.count ? v->c.count : 1, sizeof(char *));
    for (size_t i = 0; i < v->c.count; i++) {
        char *kr = tt2_repr_string(v->c.keys[i]);
        char *vr = tt2_repr_value(v->c.items[i]);
        parts[i] = tt2_xsf("%s: %s", kr, vr);
        free(kr); free(vr);
        cap += strlen(parts[i]) + 2;
    }
    char *out = malloc(cap);
    size_t k = 0;
    out[k++] = '{';
    for (size_t i = 0; i < v->c.count; i++) {
        if (i) { out[k++] = ','; out[k++] = ' '; }
        size_t n = strlen(parts[i]); memcpy(out + k, parts[i], n); k += n;
        free(parts[i]);
    }
    out[k++] = '}'; out[k] = '\0';
    free(parts);
    return out;
}

/* ── Python == on JSON values (deep; integral float == int; bool == 0/1) ── */
static bool tt2_numeric(const json_t *v, double *out) {
    if (tt2_is_bool(v)) { *out = v->bool_val ? 1.0 : 0.0; return true; }
    if (tt2_is_num(v))  { *out = v->num_val; return true; }
    return false;
}
static bool tt2_deep_eq(const json_t *a, const json_t *b) {
    bool an = tt2_is_null(a), bn = tt2_is_null(b);
    if (an || bn) return an && bn;
    double na, nb;
    if (tt2_numeric(a, &na) && tt2_numeric(b, &nb)) return na == nb;
    if (tt2_is_str(a) && tt2_is_str(b))
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (tt2_is_arr(a) && tt2_is_arr(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!tt2_deep_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (tt2_is_obj(a) && tt2_is_obj(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv && !json_has(b, a->c.keys[i])) return false;
            if (!tt2_deep_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* ── dataclass field descriptors ── */
typedef enum { TT2_DEF_NULL, TT2_DEF_ZERO } tt2_def_kind;
typedef struct tt2_field {
    const char *name;
    tt2_def_kind def;
    bool repr;
    bool required;
} tt2_field_t;

static const tt2_field_t TT2_TOOL_CALL[] = {
    {"id",            TT2_DEF_NULL, true,  true},
    {"name",          TT2_DEF_NULL, true,  true},
    {"arguments",     TT2_DEF_NULL, true,  true},
    {"provider_data", TT2_DEF_NULL, false, false},
};
static const tt2_field_t TT2_USAGE[] = {
    {"prompt_tokens",     TT2_DEF_ZERO, true, false},
    {"completion_tokens", TT2_DEF_ZERO, true, false},
    {"total_tokens",      TT2_DEF_ZERO, true, false},
    {"cached_tokens",     TT2_DEF_ZERO, true, false},
};
static const tt2_field_t TT2_NORMALIZED_RESPONSE[] = {
    {"content",       TT2_DEF_NULL, true,  true},
    {"tool_calls",    TT2_DEF_NULL, true,  true},
    {"finish_reason", TT2_DEF_NULL, true,  true},
    {"reasoning",     TT2_DEF_NULL, true,  false},
    {"usage",         TT2_DEF_NULL, true,  false},
    {"provider_data", TT2_DEF_NULL, false, false},
};

/* projection: declaration order; PRESENT value verbatim (incl. null), ABSENT default.
 * json_obj_get returns NULL only for a MISSING key (a present null is a JSON_NULL
 * node), so presence is distinguished from null exactly as the live ctor does. */
static json_t *tt2_project(const json_t *fields, const tt2_field_t *ft, size_t n) {
    json_t *o = json_object();
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_obj_get(fields, ft[i].name);
        if (v) json_set(o, ft[i].name, json_copy(v));
        else json_set(o, ft[i].name,
                      ft[i].def == TT2_DEF_ZERO ? json_number(0) : json_null());
    }
    return o;
}
/* the real ctor validates unknown kwargs (first unexpected, in passed order). */
static json_t *tt2_init(const json_t *fields, const tt2_field_t *ft, size_t n,
                        const char *cls) {
    if (tt2_is_obj(fields)) {
        for (size_t i = 0; i < fields->c.count; i++) {
            const char *k = fields->c.keys[i];
            bool known = false;
            for (size_t j = 0; j < n; j++)
                if (!strcmp(ft[j].name, k)) { known = true; break; }
            if (!known) {
                char *m = tt2_xsf("%s.__init__() got an unexpected keyword argument '%s'",
                                  cls, k);
                json_t *r = tt2_raise("TypeError", m);
                free(m);
                return r;
            }
        }
    }
    return tt2_project(fields, ft, n);
}
static json_t *tt2_repr(const json_t *fields, const tt2_field_t *ft, size_t n,
                        const char *cls) {
    json_t *proj = tt2_project(fields, ft, n);
    size_t cap = strlen(cls) + 3;
    char **parts = calloc(n, sizeof(char *));
    size_t np = 0;
    for (size_t i = 0; i < n; i++) {
        if (!ft[i].repr) continue;
        char *vr = tt2_repr_value(json_obj_get(proj, ft[i].name));
        parts[np] = tt2_xsf("%s=%s", ft[i].name, vr);
        free(vr);
        cap += strlen(parts[np]) + 2;
        np++;
    }
    char *buf = malloc(cap);
    size_t k = 0;
    size_t cn = strlen(cls); memcpy(buf + k, cls, cn); k += cn;
    buf[k++] = '(';
    for (size_t i = 0; i < np; i++) {
        if (i) { buf[k++] = ','; buf[k++] = ' '; }
        size_t pn = strlen(parts[i]); memcpy(buf + k, parts[i], pn); k += pn;
        free(parts[i]);
    }
    buf[k++] = ')'; buf[k] = '\0';
    free(parts);
    json_free(proj);
    json_t *r = json_string(buf);
    free(buf);
    return r;
}
/* live `self == other`: cross-class -> False (NotImplemented fallback); same-class ->
 * field-tuple equality on the defaulted projections. */
static json_t *tt2_eq(const json_t *self_fields, const json_t *other_fields,
                      const char *other_class, const tt2_field_t *ft, size_t n,
                      const char *cls) {
    if (other_class && *other_class && strcmp(other_class, cls) != 0)
        return json_bool(false);
    json_t *a = tt2_project(self_fields, ft, n);
    json_t *b = tt2_project(other_fields, ft, n);
    bool eq = true;
    for (size_t i = 0; i < n; i++) {
        if (!tt2_deep_eq(json_obj_get(a, ft[i].name), json_obj_get(b, ft[i].name))) {
            eq = false; break;
        }
    }
    json_free(a); json_free(b);
    return json_bool(eq);
}
/* __replace__: merged = passed changes (passed order) + missing init fields from self
 * (declaration order); seam the ctor call; on raise propagate (no except site); else
 * construct via the REAL __init__ port (validates unknown kwargs). */
static json_t *tt2_replace(const tt2_seams_t *s, const json_t *self_fields,
                           const json_t *changes, const tt2_field_t *ft, size_t n,
                           const char *cls, const char *seam_name) {
    json_t *merged = json_object();
    if (tt2_is_obj(changes)) {
        for (size_t i = 0; i < changes->c.count; i++)
            json_set(merged, changes->c.keys[i], json_copy(changes->c.items[i]));
    }
    json_t *self_proj = tt2_project(self_fields, ft, n);
    for (size_t i = 0; i < n; i++) {
        if (!json_has(merged, ft[i].name))
            json_set(merged, ft[i].name, json_copy(json_obj_get(self_proj, ft[i].name)));
    }
    json_free(self_proj);
    json_t *r = tt2_call(s, seam_name, json_copy(merged));
    if (tt2_is_raise(r)) {
        /* No except clause in __replace__: BaseException AND Exception alike pass
         * through verbatim. Classify for the record; never swallow. */
        bool base = tt2_is_base_exc(tt2_raise_class(r));
        (void)base;
        json_free(merged);
        return r;
    }
    if (r) json_free(r);   /* ctor returns None; the ret is not used */
    json_t *out = tt2_init(merged, ft, n, cls);
    json_free(merged);
    return out;
}

/* ═══════════════════════ ToolCall ═══════════════════════ */

/* PoP: tt2_tool_call_init @ agent/transports/types.py:ToolCall.__init__ */
static json_t *tt2_tool_call_init(const json_t *fields) {
    return tt2_init(fields, TT2_TOOL_CALL,
                    sizeof(TT2_TOOL_CALL) / sizeof(TT2_TOOL_CALL[0]), "ToolCall");
}
/* PoP: tt2_tool_call_repr @ agent/transports/types.py:ToolCall.__repr__ */
static json_t *tt2_tool_call_repr(const json_t *fields) {
    return tt2_repr(fields, TT2_TOOL_CALL,
                    sizeof(TT2_TOOL_CALL) / sizeof(TT2_TOOL_CALL[0]), "ToolCall");
}
/* PoP: tt2_tool_call_eq @ agent/transports/types.py:ToolCall.__eq__ */
static json_t *tt2_tool_call_eq(const json_t *self_fields, const json_t *other_fields,
                                const char *other_class) {
    return tt2_eq(self_fields, other_fields, other_class, TT2_TOOL_CALL,
                  sizeof(TT2_TOOL_CALL) / sizeof(TT2_TOOL_CALL[0]), "ToolCall");
}
/* PoP: tt2_tool_call_replace @ agent/transports/types.py:ToolCall.__replace__ */
static json_t *tt2_tool_call_replace(const tt2_seams_t *s, const json_t *self_fields,
                                     const json_t *changes) {
    return tt2_replace(s, self_fields, changes, TT2_TOOL_CALL,
                       sizeof(TT2_TOOL_CALL) / sizeof(TT2_TOOL_CALL[0]),
                       "ToolCall", "ToolCall.__init__");
}

/* ═══════════════════════ Usage ═══════════════════════ */

/* PoP: tt2_usage_init @ agent/transports/types.py:Usage.__init__ */
static json_t *tt2_usage_init(const json_t *fields) {
    return tt2_init(fields, TT2_USAGE, sizeof(TT2_USAGE) / sizeof(TT2_USAGE[0]), "Usage");
}
/* PoP: tt2_usage_repr @ agent/transports/types.py:Usage.__repr__ */
static json_t *tt2_usage_repr(const json_t *fields) {
    return tt2_repr(fields, TT2_USAGE, sizeof(TT2_USAGE) / sizeof(TT2_USAGE[0]), "Usage");
}
/* PoP: tt2_usage_eq @ agent/transports/types.py:Usage.__eq__ */
static json_t *tt2_usage_eq(const json_t *self_fields, const json_t *other_fields,
                            const char *other_class) {
    return tt2_eq(self_fields, other_fields, other_class, TT2_USAGE,
                  sizeof(TT2_USAGE) / sizeof(TT2_USAGE[0]), "Usage");
}
/* PoP: tt2_usage_replace @ agent/transports/types.py:Usage.__replace__ */
static json_t *tt2_usage_replace(const tt2_seams_t *s, const json_t *self_fields,
                                 const json_t *changes) {
    return tt2_replace(s, self_fields, changes, TT2_USAGE,
                       sizeof(TT2_USAGE) / sizeof(TT2_USAGE[0]),
                       "Usage", "Usage.__init__");
}

/* ═══════════════════════ NormalizedResponse ═══════════════════════ */

/* PoP: tt2_normalized_response_init @ agent/transports/types.py:NormalizedResponse.__init__ */
static json_t *tt2_normalized_response_init(const json_t *fields) {
    return tt2_init(fields, TT2_NORMALIZED_RESPONSE,
                    sizeof(TT2_NORMALIZED_RESPONSE) / sizeof(TT2_NORMALIZED_RESPONSE[0]),
                    "NormalizedResponse");
}
/* PoP: tt2_normalized_response_repr @ agent/transports/types.py:NormalizedResponse.__repr__ */
static json_t *tt2_normalized_response_repr(const json_t *fields) {
    return tt2_repr(fields, TT2_NORMALIZED_RESPONSE,
                    sizeof(TT2_NORMALIZED_RESPONSE) / sizeof(TT2_NORMALIZED_RESPONSE[0]),
                    "NormalizedResponse");
}
/* PoP: tt2_normalized_response_eq @ agent/transports/types.py:NormalizedResponse.__eq__ */
static json_t *tt2_normalized_response_eq(const json_t *self_fields,
                                          const json_t *other_fields,
                                          const char *other_class) {
    return tt2_eq(self_fields, other_fields, other_class, TT2_NORMALIZED_RESPONSE,
                  sizeof(TT2_NORMALIZED_RESPONSE) / sizeof(TT2_NORMALIZED_RESPONSE[0]),
                  "NormalizedResponse");
}
/* PoP: tt2_normalized_response_replace @ agent/transports/types.py:NormalizedResponse.__replace__ */
static json_t *tt2_normalized_response_replace(const tt2_seams_t *s,
                                               const json_t *self_fields,
                                               const json_t *changes) {
    return tt2_replace(s, self_fields, changes, TT2_NORMALIZED_RESPONSE,
                       sizeof(TT2_NORMALIZED_RESPONSE) / sizeof(TT2_NORMALIZED_RESPONSE[0]),
                       "NormalizedResponse", "NormalizedResponse.__init__");
}