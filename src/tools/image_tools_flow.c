/* image_tools_flow.c -- FLOW + leaf port of the image-generation tool surface.
 *
 * MODULE RESOLUTION: the task named tools/image_tools.py, which does not exist
 * in the quarry. The real defining module of the image tools surface is
 * tools/image_generation_tool.py (the DebugSession is even named "image_tools");
 * PoP credits THAT module verbatim.
 *
 * Ports the 20 UNCLAIMED functions of tools/image_generation_tool.py: the config
 * readers, the payload builders (catalog-whitelist filtering), the result
 * formatting / source-inlining / budget-shrink chain, the FAL request preparation,
 * the api-key gate, plugin-provider discovery, the provider JSON envelopes, the
 * provider-kwargs / declared-controls helpers, the managed Krea/Portal routing
 * and the Krea->FAL fallback. Every CLAIMED sibling (_read_configured_image_model,
 * _read_configured_image_provider, _upscale_image, fal_key_is_configured,
 * _resolve_managed_fal_gateway, _build_no_backend_setup_message,
 * image_generate_tool) and every foreign sub-call (config, logger, image_source,
 * model_tools._run_async, plugin registry, normalize_reference_images,
 * managed_route, krea gateway, fallback gate, PIL/base64/io doubles) is INJECTED
 * through the it_io_t universal seam: io->call(io, name, args) appends
 * {"name","args"} to io->trace and returns the scripted result (NULL/json_null ==
 * None; {"__raise__":cls,"message":msg} == a raised exception). Unclaimed siblings
 * ported here run REAL when composed (no seam between them); each is also driven
 * directly by its own fixture cases.
 *
 * Seam model / ownership / representation contract (shared with the harness
 * tests/t_port_image_tools_flow.c and oracle tests/oracle_image_tools_flow.py):
 *  - it_call ADOPTS args (frees them; records a copy on the trace) and returns a
 *    NEW owned result. NULL args record {}.
 *  - io->trace / io->state are harness-owned. Inputs borrowed; ports never mutate
 *    `in` (in-place kwargs mutation works on a private copy).
 *  - Integral floats normalize to ints on both sides; fixtures ASCII.
 *  - BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit) PASS THROUGH
 *    every `except Exception` / `except ValueError` / `except ImageResolutionError`
 *    site (full builtin hierarchy table below).
 *  - Fail-closed: a missing/exhausted seam script is a RuntimeError raise spec
 *    returned by the dispatcher (recorded on the trace before propagation).
 *  - None-valued seam arg keys are dropped on BOTH sides (absent == null).
 *
 * Fixture DOMAIN restrictions (where libjson / C cannot observe a Python type
 * distinction, the fixtures stay inside the verified class):
 *  - seed values: int / bool / null only (isinstance(seed, int) is True for bool,
 *    False for float; libjson stores every number as double, so float seeds are
 *    out of domain).
 *  - dicts that reach json.dumps (_provider_result / _fall_back_to_fal results):
 *    no float values at all (dumps renders 5.0 as "5.0", libjson cannot tell 5
 *    from 5.0 after parse). Non-integral floats ARE fine in dict RETURNS
 *    (parsed-JSON comparison normalizes them).
 *  - non-str url elements in _fit_inline_budget: int/bool/null/list/dict (the
 *    AttributeError type name of a float would be ambiguous).
 *  - json.loads inputs (_fall_back_to_fal raw / image_generate_tool seam ret):
 *    strict Python JSON grammar, no lone surrogates, no NaN/Infinity; the
 *    image_generate_tool ret is always a valid JSON object string.
 *  - PIL is fully seamed (pil_open/pil_load/exif_transpose/pil_convert/pil_encode
 *    carry descriptor dicts {bands,info,size}); base64 decode/encode are identity
 *    doubles, so no real raster decoding happens on either side.
 *
 * Seam name table (name -> live call):
 *   load_config                    hermes_cli.config.load_config()
 *   logger.debug / .warning / .info  logger.<level>(fmt, *args) -> {"msg": fmt%args}
 *   read_configured_image_provider _read_configured_image_provider()  (CLAIMED)
 *   read_configured_image_model    _read_configured_image_model()  (CLAIMED)
 *   upscale_image                  _upscale_image(image_url, original_prompt)  (CLAIMED)
 *   fal_key_set                    fal_key_is_configured()  (CLAIMED)
 *   resolve_managed_fal_gateway    _resolve_managed_fal_gateway()  (CLAIMED)
 *   build_no_backend_setup_message _build_no_backend_setup_message()  (CLAIMED)
 *   image_generate_tool            image_generate_tool(prompt, aspect_ratio,
 *                                  upscale=, fal_model=)  (CLAIMED)
 *   resolve_local_source_to_data_url  tools.image_source.resolve_local_source_to_data_url(s, None)
 *   _run_async                     model_tools._run_async(coro)
 *   ensure_plugins_discovered      hermes_cli.plugins._ensure_plugins_discovered(force=)
 *   get_provider                   agent.image_gen_registry.get_provider(name)
 *   normalize_reference_images     agent.image_gen_provider.normalize_reference_images(refs)
 *   managed_route                  tools.image_generation_managed.managed_route(provider, model_id)
 *   resolve_managed_krea_gateway   plugins.image_gen.krea._resolve_managed_krea_gateway()
 *   fallback_to_fal_enabled        plugins.image_gen.krea.fallback_to_fal_enabled()
 *   provider.capabilities          provider.capabilities()  (ProviderDouble)
 *   provider.generate              provider.generate(**kwargs)  (splat -> args object)
 *   pil_open                       PIL.Image.open(io.BytesIO(b64decode(part)))
 *   pil_load                       img.load()
 *   exif_transpose                 PIL.ImageOps.exif_transpose(img)
 *   pil_convert                    img.convert(mode)
 *   pil_encode                     frame.thumbnail+save+base64 encode (one loop iter)
 */
#ifndef IMAGE_TOOLS_FLOW_C
#define IMAGE_TOOLS_FLOW_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

typedef struct it_io_t {
    json_t *(*call)(struct it_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* ordered array of {"name","args"} */
    json_t *state;   /* mutable module-global mirror (harness-owned) */
    void   *ctx;
} it_io_t;

/* -- universal seam dispatch (adopts args, returns owned result) --------- */
static json_t *it_call(it_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    if (io && io->trace && io->trace->type == JSON_ARRAY) json_append(io->trace, ev);
    else json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : NULL;
    json_free(args);
    return r ? r : json_null();
}

static bool it_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *it_raise(const char *cls, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", cls);
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
}

/* -- FULL builtin exception hierarchy (child -> parent) ------------------ */
typedef struct { const char *child; const char *parent; } it_exc_edge_t;
static const it_exc_edge_t IT_EXC_EDGES[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"OSError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"ZipImportError", "ImportError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"JSONDecodeError", "ValueError"},
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
    /* module-local / foreign exception classes used by this surface */
    {"ImageResolutionError", "Exception"},
    {"DecompressionBombError", "Exception"},
    {"ImageGenerationInterrupted", "Exception"},
};

static const char *it_exc_parent(const char *kind) {
    if (!kind) return NULL;
    if (strcmp(kind, "BaseException") == 0) return NULL;
    for (size_t i = 0; i < sizeof(IT_EXC_EDGES) / sizeof(IT_EXC_EDGES[0]); i++)
        if (strcmp(IT_EXC_EDGES[i].child, kind) == 0) return IT_EXC_EDGES[i].parent;
    /* unknown kind == a synthetic Exception subclass (oracle make_exc fallback) */
    return "Exception";
}

static bool it_exc_is_a(const char *kind, const char *ancestor) {
    if (!kind || !ancestor) return false;
    const char *cur = kind;
    for (int depth = 0; cur && depth < 64; depth++) {
        if (strcmp(cur, ancestor) == 0) return true;
        cur = it_exc_parent(cur);
    }
    return false;
}

static const char *it_exc_kind(const json_t *r) {
    if (!it_raised(r)) return "";
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}

static const char *it_exc_msg(const json_t *r) {
    if (!it_raised(r)) return "";
    return json_get_str(r, "message", "");
}

/* `except Exception` swallows Exception-family; the BaseException-only trio
 * passes through. */
static bool it_exc_is_exception(const json_t *r) {
    return it_raised(r) && it_exc_is_a(it_exc_kind(r), "Exception");
}

/* -- truthiness / type predicates ---------------------------------------- */
static bool jtruth(const json_t *v) {
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
static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool j_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }

static const char *sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static json_t *copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }

/* Python type name for AttributeError messages (fixture domain: numbers in
 * these positions are int literals -- see header). */
static const char *it_tname(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return "int";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* f"'{type}' object has no attribute '{attr}'" (caller frees). */
static char *it_attr_msg(const json_t *v, const char *attr) {
    const char *t = it_tname(v);
    size_t n = strlen(t) + strlen(attr) + 40;
    char *m = (char *)malloc(n);
    if (m) snprintf(m, n, "'%s' object has no attribute '%s'", t, attr);
    return m;
}

/* Python str(v) for f-string / %-format interpolation of scalars. */
static char *it_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(sval(v));
    return strdup("");
}

/* Python repr() of a str key (single quotes; domain: simple ASCII keys). */
static char *it_repr_str(const char *s) {
    size_t n = strlen(s);
    char *r = (char *)malloc(n * 2 + 3);
    if (!r) return NULL;
    size_t j = 0;
    r[j++] = '\'';
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\'' || s[i] == '\\') r[j++] = '\\';
        r[j++] = s[i];
    }
    r[j++] = '\'';
    r[j] = '\0';
    return r;
}

static bool it_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *it_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s; while (*b && it_isspace((unsigned char)*b)) b++;
    const char *e = b + strlen(b); while (e > b && it_isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1); if (!r) return NULL;
    memcpy(r, b, n); r[n] = '\0'; return r;
}
static char *it_lower(const char *s) {
    char *r = strdup(s ? s : ""); if (!r) return NULL;
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}
static bool it_startswith(const char *s, const char *pre) {
    return s && pre && strncmp(s, pre, strlen(pre)) == 0;
}
/* Python str[:n] (ASCII domain). Caller frees. */
static char *it_slice(const char *s, size_t n) {
    size_t l = strlen(s); if (n < l) l = n;
    char *r = (char *)malloc(l + 1); if (!r) return NULL;
    memcpy(r, s, l); r[l] = '\0'; return r;
}
/* Python floor division a // b (b != 0). */
static long it_floordiv(long a, long b) {
    long q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}
static bool it_in_arr(const json_t *arr, const char *s) {
    if (!j_is_arr(arr)) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        if (j_is_str(e) && strcmp(sval(e), s) == 0) return true;
    }
    return false;
}

/* logger.<level>(fmt, *args) -- the oracle records the EAGERLY formatted msg. */
static json_t *it_log(it_io_t *io, const char *level, const char *msg) {
    json_t *a = json_object();
    json_set_hdr(a, "msg", msg);
    return it_call(io, level, a);
}

/* -- Python json.dumps(obj) default: separators (', ', ': '), ensure_ascii -- */
typedef struct { char *b; size_t len, cap; } it_buf_t;
static void it_buf_append(it_buf_t *bf, const char *s, size_t n) {
    if (bf->len + n + 1 > bf->cap) {
        size_t nc = bf->cap ? bf->cap : 128;
        while (nc < bf->len + n + 1) nc *= 2;
        char *nb = (char *)realloc(bf->b, nc);
        if (!nb) return;
        bf->b = nb; bf->cap = nc;
    }
    memcpy(bf->b + bf->len, s, n);
    bf->len += n;
    bf->b[bf->len] = '\0';
}
static void it_buf_puts(it_buf_t *bf, const char *s) { it_buf_append(bf, s, strlen(s)); }

/* Decode one UTF-8 codepoint; advance *i. Invalid bytes -> U+FFFD (domain ASCII). */
static unsigned long it_utf8_next(const char *s, size_t n, size_t *i) {
    unsigned char c = (unsigned char)s[*i];
    if (c < 0x80) { *i += 1; return c; }
    unsigned long cp = 0xFFFD; int extra = 0;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { *i += 1; return 0xFFFD; }
    for (int k = 0; k < extra; k++) {
        if (*i + 1 + (size_t)k >= n) { *i = n; return 0xFFFD; }
        unsigned char cc = (unsigned char)s[*i + 1 + (size_t)k];
        if ((cc & 0xC0) != 0x80) { *i = n; return 0xFFFD; }
        cp = (cp << 6) | (cc & 0x3F);
    }
    *i += (size_t)extra + 1;
    return cp;
}

static void it_dumps_str(it_buf_t *bf, const char *s) {
    it_buf_puts(bf, "\"");
    size_t n = strlen(s), i = 0;
    char esc[16];
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') { it_buf_puts(bf, "\\\""); i++; continue; }
        if (c == '\\') { it_buf_puts(bf, "\\\\"); i++; continue; }
        if (c == '\b') { it_buf_puts(bf, "\\b"); i++; continue; }
        if (c == '\f') { it_buf_puts(bf, "\\f"); i++; continue; }
        if (c == '\n') { it_buf_puts(bf, "\\n"); i++; continue; }
        if (c == '\r') { it_buf_puts(bf, "\\r"); i++; continue; }
        if (c == '\t') { it_buf_puts(bf, "\\t"); i++; continue; }
        if (c < 0x20) {
            snprintf(esc, sizeof esc, "\\u%04x", (unsigned)c);
            it_buf_puts(bf, esc); i++; continue;
        }
        if (c < 0x80) { it_buf_append(bf, (const char *)&c, 1); i++; continue; }
        unsigned long cp = it_utf8_next(s, n, &i);
        if (cp <= 0xFFFF) {
            snprintf(esc, sizeof esc, "\\u%04lx", cp);
            it_buf_puts(bf, esc);
        } else {
            unsigned long v = cp - 0x10000;
            snprintf(esc, sizeof esc, "\\u%04lx\\u%04lx",
                     0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
            it_buf_puts(bf, esc);
        }
    }
    it_buf_puts(bf, "\"");
}

static void it_dumps_val(it_buf_t *bf, const json_t *v) {
    if (!v || v->type == JSON_NULL) { it_buf_puts(bf, "null"); return; }
    switch (v->type) {
    case JSON_BOOL: it_buf_puts(bf, v->bool_val ? "true" : "false"); return;
    case JSON_NUMBER: {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e16) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%.17g", d); /* out-of-domain fallback */
        it_buf_puts(bf, buf);
        return;
    }
    case JSON_STRING: it_dumps_str(bf, sval(v)); return;
    case JSON_ARRAY:
        it_buf_puts(bf, "[");
        for (size_t i = 0; i < json_len(v); i++) {
            if (i) it_buf_puts(bf, ", ");
            it_dumps_val(bf, json_get(v, i));
        }
        it_buf_puts(bf, "]");
        return;
    case JSON_OBJECT:
        it_buf_puts(bf, "{");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) it_buf_puts(bf, ", ");
            it_dumps_str(bf, v->c.keys[i]);
            it_buf_puts(bf, ": ");
            it_dumps_val(bf, v->c.items[i]);
        }
        it_buf_puts(bf, "}");
        return;
    default: it_buf_puts(bf, "null"); return;
    }
}

/* Python json.dumps(v) with default separators + ensure_ascii=True.
 * Caller frees. Fixture domain: no float values (see header). */
static char *it_json_dumps(const json_t *v) {
    it_buf_t bf = {NULL, 0, 0};
    it_dumps_val(&bf, v);
    return bf.b ? bf.b : strdup("null");
}

/* -- strict Python-grammar JSON validator before lenient libjson parse ----- */
typedef struct { const char *s; size_t i, n; } it_scan_t;
static void it_skip_ws(it_scan_t *sc) {
    while (sc->i < sc->n) {
        char c = sc->s[sc->i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') sc->i++;
        else break;
    }
}
static bool it_scan_string(it_scan_t *sc) {
    if (sc->i >= sc->n || sc->s[sc->i] != '"') return false;
    sc->i++;
    while (sc->i < sc->n) {
        unsigned char c = (unsigned char)sc->s[sc->i];
        if (c == '"') { sc->i++; return true; }
        if (c < 0x20) return false;
        if (c == '\\') {
            sc->i++;
            if (sc->i >= sc->n) return false;
            char e = sc->s[sc->i];
            if (e == 'u') {
                for (int k = 0; k < 4; k++) {
                    sc->i++;
                    if (sc->i >= sc->n || !isxdigit((unsigned char)sc->s[sc->i])) return false;
                }
                sc->i++;
            } else if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' ||
                       e == 'n' || e == 'r' || e == 't') {
                sc->i++;
            } else return false;
        } else sc->i++;
    }
    return false;
}
static bool it_scan_number(it_scan_t *sc) {
    size_t start = sc->i;
    if (sc->i < sc->n && sc->s[sc->i] == '-') sc->i++;
    if (sc->i >= sc->n) return false;
    if (sc->s[sc->i] == '0') {
        sc->i++;
        if (sc->i < sc->n && isdigit((unsigned char)sc->s[sc->i])) return false;
    } else if (isdigit((unsigned char)sc->s[sc->i])) {
        while (sc->i < sc->n && isdigit((unsigned char)sc->s[sc->i])) sc->i++;
    } else return false;
    if (sc->i < sc->n && sc->s[sc->i] == '.') {
        sc->i++;
        if (sc->i >= sc->n || !isdigit((unsigned char)sc->s[sc->i])) return false;
        while (sc->i < sc->n && isdigit((unsigned char)sc->s[sc->i])) sc->i++;
    }
    if (sc->i < sc->n && (sc->s[sc->i] == 'e' || sc->s[sc->i] == 'E')) {
        sc->i++;
        if (sc->i < sc->n && (sc->s[sc->i] == '+' || sc->s[sc->i] == '-')) sc->i++;
        if (sc->i >= sc->n || !isdigit((unsigned char)sc->s[sc->i])) return false;
        while (sc->i < sc->n && isdigit((unsigned char)sc->s[sc->i])) sc->i++;
    }
    return sc->i > start;
}
static bool it_scan_value(it_scan_t *sc, int depth) {
    if (depth > 200) return false;
    it_skip_ws(sc);
    if (sc->i >= sc->n) return false;
    char c = sc->s[sc->i];
    if (c == '"') return it_scan_string(sc);
    if (c == '{') {
        sc->i++; it_skip_ws(sc);
        if (sc->i < sc->n && sc->s[sc->i] == '}') { sc->i++; return true; }
        while (1) {
            it_skip_ws(sc);
            if (!it_scan_string(sc)) return false;
            it_skip_ws(sc);
            if (sc->i >= sc->n || sc->s[sc->i] != ':') return false;
            sc->i++;
            if (!it_scan_value(sc, depth + 1)) return false;
            it_skip_ws(sc);
            if (sc->i >= sc->n) return false;
            if (sc->s[sc->i] == '}') { sc->i++; return true; }
            if (sc->s[sc->i] != ',') return false;
            sc->i++;
        }
    }
    if (c == '[') {
        sc->i++; it_skip_ws(sc);
        if (sc->i < sc->n && sc->s[sc->i] == ']') { sc->i++; return true; }
        while (1) {
            if (!it_scan_value(sc, depth + 1)) return false;
            it_skip_ws(sc);
            if (sc->i >= sc->n) return false;
            if (sc->s[sc->i] == ']') { sc->i++; return true; }
            if (sc->s[sc->i] != ',') return false;
            sc->i++;
        }
    }
    if (c == 't') {
        if (sc->n - sc->i >= 4 && strncmp(sc->s + sc->i, "true", 4) == 0) { sc->i += 4; return true; }
        return false;
    }
    if (c == 'f') {
        if (sc->n - sc->i >= 5 && strncmp(sc->s + sc->i, "false", 5) == 0) { sc->i += 5; return true; }
        return false;
    }
    if (c == 'n') {
        if (sc->n - sc->i >= 4 && strncmp(sc->s + sc->i, "null", 4) == 0) { sc->i += 4; return true; }
        return false;
    }
    return it_scan_number(sc);
}
/* CPython json.loads raises JSONDecodeError ("Expecting value: line L column C
 * (char N)") when a value is missing at byte offset `pos`. Python positions are
 * str code points, so count UTF-8 non-continuation bytes, not raw bytes. */
static json_t *it_json_decode_expecting(const char *s, size_t pos) {
    size_t char_pos = 0;      /* code points before pos (== CPython pos) */
    size_t line = 1;
    long last_nl = -1;        /* code-point index of the last '\n' before pos */
    for (size_t i = 0; i < pos; i++) {
        unsigned char b = (unsigned char)s[i];
        if ((b & 0xC0) == 0x80) continue;   /* UTF-8 continuation byte */
        if (b == '\n') { line++; last_nl = (long)char_pos; }
        char_pos++;
    }
    long col = (long)char_pos - last_nl;    /* CPython colno (1-based) */
    char msg[256];
    snprintf(msg, sizeof(msg),
             "Expecting value: line %zu column %ld (char %zu)",
             line, col, char_pos);
    return it_raise("JSONDecodeError", msg);
}

/* Python json.loads accept/reject domain. On rejection emit the same
 * JSONDecodeError CPython raises (a ValueError subclass, so existing
 * `except ValueError` callers still catch it). Returns parsed value or raise. */
static json_t *it_json_loads(const char *s) {
    it_scan_t sc = {s ? s : "", 0, s ? strlen(s) : 0};
    if (!it_scan_value(&sc, 0)) return it_json_decode_expecting(sc.s, sc.i);
    it_skip_ws(&sc);
    if (sc.i != sc.n) return it_json_decode_expecting(sc.s, sc.i);
    json_t *v = json_parse(s, NULL);
    return v ? v : it_json_decode_expecting(sc.s, sc.i);
}

/* -- catalog constants (mirrors tools/image_generation_catalog.py) ---------
 * FAL_MODELS embedded verbatim (sets -> sorted arrays; membership-only use).
 * Regenerate from the quarry catalog if it drifts; the oracle execs the LIVE
 * catalog module, and per-case "catalog" overrides drive both sides equally. */
static const char *IT_FAL_MODELS_JSON =
    "{\"fal-ai/flux-2/klein/9b\":{\"display\":\"FLUX 2 Klein 9B\",\"speed\":\"<1s\",\"strengths\":\"Fast,"
    " crisp text\",\"price\":\"$0.006/MP\",\"size_style\":\"image_size_preset\","
    "\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},"
    "\"defaults\":{\"num_inference_steps\":4,\"output_format\":\"png\",\"enable_safety_checker\":false},"
    "\"supports\":[\"enable_safety_checker\",\"image_size\",\"num_inference_steps\",\"output_format\",\"prompt\","
    "\"seed\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/flux-2/klein/9b/edit\","
    "\"edit_supports\":[\"enable_safety_checker\",\"image_urls\",\"num_inference_steps\",\"output_format\","
    "\"prompt\",\"seed\"],\"max_reference_images\":9},\"fal-ai/flux-2-pro\":{\"display\":\"FLUX 2 Pro\","
    "\"speed\":\"~6s\",\"strengths\":\"Studio photorealism\",\"price\":\"$0.03/MP\","
    "\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\","
    "\"portrait\":\"portrait_16_9\"},\"defaults\":{\"num_inference_steps\":50,\"guidance_scale\":4.5,"
    "\"num_images\":1,\"output_format\":\"png\",\"enable_safety_checker\":false,\"safety_tolerance\":\"5\","
    "\"sync_mode\":true},\"supports\":[\"enable_safety_checker\",\"guidance_scale\",\"image_size\",\"num_images\","
    "\"num_inference_steps\",\"output_format\",\"prompt\",\"safety_tolerance\",\"seed\",\"sync_mode\"],"
    "\"upscale\":false,\"edit_endpoint\":\"fal-ai/flux-2-pro/edit\",\"edit_supports\":[\"enable_safety_checker\","
    "\"guidance_scale\",\"image_urls\",\"num_images\",\"num_inference_steps\",\"output_format\",\"prompt\","
    "\"safety_tolerance\",\"seed\",\"sync_mode\"],\"max_reference_images\":9},"
    "\"fal-ai/z-image/turbo\":{\"display\":\"Z-Image Turbo\",\"speed\":\"~2s\",\"strengths\":\"Bilingual EN/CN,"
    " 6B\",\"price\":\"$0.005/MP\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\","
    "\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"num_inference_steps\":8,"
    "\"num_images\":1,\"output_format\":\"png\",\"enable_safety_checker\":false,\"enable_prompt_expansion\":false},"
    "\"supports\":[\"enable_prompt_expansion\",\"enable_safety_checker\",\"image_size\",\"num_images\","
    "\"num_inference_steps\",\"output_format\",\"prompt\",\"seed\"],\"upscale\":false},"
    "\"fal-ai/nano-banana-pro\":{\"display\":\"Nano Banana Pro (Gemini 3 Pro Image)\",\"speed\":\"~8s\","
    "\"strengths\":\"Gemini 3 Pro, reasoning depth, text rendering\",\"price\":\"$0.15/image (1K)\","
    "\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},"
    "\"defaults\":{\"num_images\":1,\"output_format\":\"png\",\"safety_tolerance\":\"5\",\"resolution\":\"1K\"},"
    "\"supports\":[\"aspect_ratio\",\"enable_web_search\",\"limit_generations\",\"num_images\",\"output_format\","
    "\"prompt\",\"resolution\",\"safety_tolerance\",\"seed\",\"sync_mode\"],\"upscale\":false,"
    "\"edit_endpoint\":\"fal-ai/nano-banana-pro/edit\",\"edit_supports\":[\"aspect_ratio\",\"enable_web_search\","
    "\"image_urls\",\"limit_generations\",\"num_images\",\"output_format\",\"prompt\",\"resolution\","
    "\"safety_tolerance\",\"seed\",\"sync_mode\"],\"max_reference_images\":2},"
    "\"fal-ai/nano-banana-2\":{\"display\":\"Nano Banana 2 (Gemini 3.1 Flash Image)\",\"speed\":\"~3s\","
    "\"strengths\":\"Fast reasoning, multilingual text, infographics\",\"price\":\"Lower-cost Flash tier\","
    "\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},"
    "\"defaults\":{\"num_images\":1,\"output_format\":\"png\",\"safety_tolerance\":\"4\",\"resolution\":\"1K\","
    "\"limit_generations\":true},\"supports\":[\"aspect_ratio\",\"enable_web_search\",\"limit_generations\","
    "\"num_images\",\"output_format\",\"prompt\",\"resolution\",\"safety_tolerance\",\"seed\",\"sync_mode\","
    "\"system_prompt\",\"thinking_level\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/nano-banana-2/edit\","
    "\"edit_supports\":[\"aspect_ratio\",\"enable_web_search\",\"image_urls\",\"limit_generations\",\"num_images\","
    "\"output_format\",\"prompt\",\"resolution\",\"safety_tolerance\",\"seed\",\"sync_mode\",\"system_prompt\","
    "\"thinking_level\"],\"max_reference_images\":14},\"fal-ai/gpt-image-1.5\":{\"display\":\"GPT Image 1.5\","
    "\"speed\":\"~15s\",\"strengths\":\"Prompt adherence\",\"price\":\"$0.034/image\","
    "\"size_style\":\"gpt_literal\",\"sizes\":{\"landscape\":\"1536x1024\",\"square\":\"1024x1024\","
    "\"portrait\":\"1024x1536\"},\"defaults\":{\"quality\":\"medium\",\"num_images\":1,\"output_format\":\"png\"},"
    "\"supports\":[\"background\",\"image_size\",\"num_images\",\"output_format\",\"prompt\",\"quality\","
    "\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/gpt-image-1.5/edit\","
    "\"edit_supports\":[\"image_size\",\"image_urls\",\"num_images\",\"output_format\",\"prompt\",\"quality\","
    "\"sync_mode\"],\"max_reference_images\":16},\"fal-ai/gpt-image-2\":{\"display\":\"GPT Image 2\","
    "\"speed\":\"~20s\",\"strengths\":\"SOTA text rendering + CJK, world-aware photorealism\","
    "\"price\":\"$0.04\\u20130.06/image\",\"size_style\":\"image_size_preset\","
    "\"sizes\":{\"landscape\":\"landscape_4_3\",\"square\":\"square_hd\",\"portrait\":\"portrait_4_3\"},"
    "\"defaults\":{\"quality\":\"medium\",\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"image_size\","
    "\"num_images\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"upscale\":false,"
    "\"edit_endpoint\":\"openai/gpt-image-2/edit\",\"edit_supports\":[\"image_urls\",\"mask_image_url\","
    "\"num_images\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"max_reference_images\":16},"
    "\"openai/gpt-image-2.5/flare/text-to-image\":{\"display\":\"GPT Image 2.5 Flare\",\"speed\":\"Fast\","
    "\"strengths\":\"Everyday creation, natural lighting and textures\",\"price\":\"Token-based pricing\","
    "\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_4_3\",\"square\":\"square_hd\","
    "\"portrait\":\"portrait_4_3\"},\"defaults\":{\"quality\":\"medium\",\"num_images\":1,"
    "\"output_format\":\"png\"},\"supports\":[\"background\",\"image_size\",\"num_images\",\"output_compression\","
    "\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"upscale\":false,"
    "\"edit_endpoint\":\"openai/gpt-image-2.5/flare/edit\",\"edit_supports\":[\"background\",\"image_size\","
    "\"image_urls\",\"input_fidelity\",\"mask_url\",\"num_images\",\"output_compression\",\"output_format\","
    "\"prompt\",\"quality\",\"sync_mode\"],\"max_reference_images\":16},"
    "\"openai/gpt-image-2.5/sunburst/text-to-image\":{\"display\":\"GPT Image 2.5 Sunburst\",\"speed\":\"Slower\","
    "\"strengths\":\"Precision editing, subject and composition consistency\",\"price\":\"Token-based pricing\","
    "\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_4_3\",\"square\":\"square_hd\","
    "\"portrait\":\"portrait_4_3\"},\"defaults\":{\"quality\":\"medium\",\"num_images\":1,"
    "\"output_format\":\"png\"},\"supports\":[\"background\",\"image_size\",\"num_images\",\"output_compression\","
    "\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"upscale\":false,"
    "\"edit_endpoint\":\"openai/gpt-image-2.5/sunburst/edit\",\"edit_supports\":[\"background\",\"image_size\","
    "\"image_urls\",\"input_fidelity\",\"mask_url\",\"num_images\",\"output_compression\",\"output_format\","
    "\"prompt\",\"quality\",\"sync_mode\"],\"max_reference_images\":16},"
    "\"openai/gpt-image-2.6/flare/text-to-image\":{\"display\":\"GPT Image 2.6 Flare\",\"speed\":\"Fast\","
    "\"strengths\":\"Latest OpenAI image model, everyday creation and editing\",\"price\":\"Token-based pricing\","
    "\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_4_3\",\"square\":\"square_hd\","
    "\"portrait\":\"portrait_4_3\"},\"defaults\":{\"quality\":\"medium\",\"num_images\":1,"
    "\"output_format\":\"png\"},\"supports\":[\"background\",\"image_size\",\"num_images\",\"output_compression\","
    "\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"upscale\":false,"
    "\"edit_endpoint\":\"openai/gpt-image-2.6/flare/edit\",\"edit_supports\":[\"background\",\"image_size\","
    "\"image_urls\",\"mask_url\",\"num_images\",\"output_compression\",\"output_format\",\"prompt\",\"quality\","
    "\"sync_mode\"],\"max_reference_images\":16},"
    "\"openai/gpt-image-2.6/sunburst/text-to-image\":{\"display\":\"GPT Image 2.6 Sunburst\",\"speed\":\"Slower\","
    "\"strengths\":\"Latest OpenAI image model, precision editing and consistency\","
    "\"price\":\"Token-based pricing\",\"size_style\":\"image_size_preset\","
    "\"sizes\":{\"landscape\":\"landscape_4_3\",\"square\":\"square_hd\",\"portrait\":\"portrait_4_3\"},"
    "\"defaults\":{\"quality\":\"medium\",\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"background\","
    "\"image_size\",\"num_images\",\"output_compression\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],"
    "\"upscale\":false,\"edit_endpoint\":\"openai/gpt-image-2.6/sunburst/edit\",\"edit_supports\":[\"background\","
    "\"image_size\",\"image_urls\",\"mask_url\",\"num_images\",\"output_compression\",\"output_format\",\"prompt\","
    "\"quality\",\"sync_mode\"],\"max_reference_images\":16},\"fal-ai/ideogram/v3\":{\"display\":\"Ideogram V3\","
    "\"speed\":\"~5s\",\"strengths\":\"Best typography\",\"price\":\"$0.03-0.09/image\","
    "\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\","
    "\"portrait\":\"portrait_16_9\"},\"defaults\":{\"rendering_speed\":\"BALANCED\",\"expand_prompt\":true,"
    "\"style\":\"AUTO\"},\"supports\":[\"expand_prompt\",\"image_size\",\"prompt\",\"rendering_speed\",\"seed\","
    "\"style\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/ideogram/v3/edit\","
    "\"edit_supports\":[\"expand_prompt\",\"image_urls\",\"prompt\",\"rendering_speed\",\"seed\",\"style\"],"
    "\"max_reference_images\":1},\"fal-ai/recraft/v4/pro/text-to-image\":{\"display\":\"Recraft V4 Pro\","
    "\"speed\":\"~8s\",\"strengths\":\"Design, brand systems, production-ready\",\"price\":\"$0.25/image\","
    "\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\","
    "\"portrait\":\"portrait_16_9\"},\"defaults\":{\"enable_safety_checker\":false},"
    "\"supports\":[\"background_color\",\"colors\",\"enable_safety_checker\",\"image_size\",\"prompt\"],"
    "\"upscale\":false},\"fal-ai/qwen-image\":{\"display\":\"Qwen Image\",\"speed\":\"~12s\","
    "\"strengths\":\"LLM-based, complex text\",\"price\":\"$0.02/MP\",\"size_style\":\"image_size_preset\","
    "\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},"
    "\"defaults\":{\"num_inference_steps\":30,\"guidance_scale\":2.5,\"num_images\":1,\"output_format\":\"png\","
    "\"acceleration\":\"regular\"},\"supports\":[\"acceleration\",\"guidance_scale\",\"image_size\",\"num_images\","
    "\"num_inference_steps\",\"output_format\",\"prompt\",\"seed\",\"sync_mode\"],\"upscale\":false,"
    "\"edit_endpoint\":\"fal-ai/qwen-image-2/pro/edit\",\"edit_supports\":[\"acceleration\",\"guidance_scale\","
    "\"image_urls\",\"num_images\",\"num_inference_steps\",\"output_format\",\"prompt\",\"seed\",\"sync_mode\"],"
    "\"max_reference_images\":3},\"fal-ai/krea/v2/medium/text-to-image\":{\"display\":\"Krea 2 Medium\","
    "\"speed\":\"~15-25s\",\"strengths\":\"Illustration, anime, painting, expressive/artistic styles\","
    "\"price\":\"$0.030 (text) / $0.035 (style refs)\",\"size_style\":\"aspect_ratio\","
    "\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},"
    "\"defaults\":{\"creativity\":\"medium\"},\"supports\":[\"aspect_ratio\",\"creativity\","
    "\"image_style_references\",\"prompt\",\"seed\"],\"upscale\":false},"
    "\"fal-ai/krea/v2/large/text-to-image\":{\"display\":\"Krea 2 Large\",\"speed\":\"~25-60s\","
    "\"strengths\":\"Photorealism, raw textured looks (motion blur, grain, film)\","
    "\"price\":\"$0.060 (text) / $0.065 (style refs)\",\"size_style\":\"aspect_ratio\","
    "\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},"
    "\"defaults\":{\"creativity\":\"medium\"},\"supports\":[\"aspect_ratio\",\"creativity\","
    "\"image_style_references\",\"prompt\",\"seed\"],\"upscale\":false},"
    "\"bytedance/seedream/v5/pro/text-to-image\":{\"display\":\"Seedream 5.0 Pro\",\"speed\":\"~10s\","
    "\"strengths\":\"ByteDance flagship, dense layouts, native text in 14 languages\","
    "\"price\":\"$0.0675/image (\\u22641536\\u00b2)\",\"size_style\":\"image_size_preset\","
    "\"sizes\":{\"landscape\":{\"width\":2048,\"height\":1152},\"square\":{\"width\":1536,\"height\":1536},"
    "\"portrait\":{\"width\":1152,\"height\":2048}},\"defaults\":{\"num_images\":1,\"output_format\":\"png\","
    "\"enable_safety_checker\":false},\"supports\":[\"enable_safety_checker\",\"image_size\",\"num_images\","
    "\"output_format\",\"prompt\",\"sync_mode\"],\"upscale\":false,"
    "\"edit_endpoint\":\"bytedance/seedream/v5/pro/edit\",\"edit_supports\":[\"enable_safety_checker\","
    "\"image_size\",\"image_urls\",\"num_images\",\"output_format\",\"prompt\",\"sync_mode\"],"
    "\"max_reference_images\":10},\"bytedance/seedream/v5/lite/text-to-image\":{\"display\":\"Seedream 5.0 Lite\","
    "\"speed\":\"~5s\",\"strengths\":\"Fast/cheap Seedream tier, high-res output\",\"price\":\"$0.035/image\","
    "\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\","
    "\"portrait\":\"portrait_16_9\"},\"defaults\":{\"num_images\":1,\"enable_safety_checker\":false},"
    "\"supports\":[\"enable_safety_checker\",\"image_size\",\"max_images\",\"num_images\",\"prompt\",\"sync_mode\"],"
    "\"upscale\":false},\"ideogram/v4/instant\":{\"display\":\"Ideogram V4 (Instant)\",\"speed\":\"<1s\","
    "\"strengths\":\"Latest Ideogram typography, posters/logos, instant\",\"price\":\"$0.0075/MP\","
    "\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\","
    "\"portrait\":\"portrait_16_9\"},\"defaults\":{\"expansion_model\":\"Medium\",\"output_format\":\"png\","
    "\"enable_safety_checker\":false},\"supports\":[\"enable_safety_checker\",\"expansion_model\",\"image_size\","
    "\"num_images\",\"output_format\",\"prompt\",\"seed\",\"sync_mode\"],\"upscale\":false},"
    "\"ideogram/v4/fast\":{\"display\":\"Ideogram V4 (Fast)\",\"speed\":\"~1s\","
    "\"strengths\":\"Ideogram V4 quality tiers via rendering_speed\",\"price\":\"$0.005-0.018/MP\","
    "\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\","
    "\"portrait\":\"portrait_16_9\"},\"defaults\":{\"expansion_model\":\"Medium\",\"rendering_speed\":\"BALANCED\"},"
    "\"supports\":[\"expansion_model\",\"image_size\",\"num_images\",\"prompt\",\"rendering_speed\",\"seed\","
    "\"sync_mode\"],\"upscale\":false},\"alibaba/qwen-image-3/text-to-image\":{\"display\":\"Qwen Image 3\","
    "\"speed\":\"~8s\",\"strengths\":\"Complex CN/EN text rendering, prompt-guided resolution\","
    "\"price\":\"$0.04 (1K) / $0.075 (2K) per image\",\"size_style\":\"image_size_preset\","
    "\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},"
    "\"defaults\":{\"num_images\":1,\"output_format\":\"png\",\"enable_prompt_expansion\":false,"
    "\"enable_safety_checker\":false},\"supports\":[\"enable_prompt_expansion\",\"enable_safety_checker\","
    "\"image_size\",\"negative_prompt\",\"num_images\",\"output_format\",\"prompt\",\"seed\",\"sync_mode\"],"
    "\"upscale\":false,\"edit_endpoint\":\"alibaba/qwen-image-3/edit\","
    "\"edit_supports\":[\"enable_prompt_expansion\",\"enable_safety_checker\",\"image_urls\",\"negative_prompt\","
    "\"num_images\",\"output_format\",\"prompt\",\"seed\",\"sync_mode\"],\"max_reference_images\":3},"
    "\"microsoft/mai-image-2.5-pro\":{\"display\":\"MAI Image 2.5 Pro\",\"speed\":\"~10s\","
    "\"strengths\":\"Microsoft flagship, hero imagery, precise typography\",\"price\":\"~$0.17/image\","
    "\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},"
    "\"defaults\":{\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"aspect_ratio\",\"num_images\","
    "\"output_format\",\"prompt\",\"sync_mode\"],\"upscale\":false},"
    "\"google/nano-banana-2-lite\":{\"display\":\"Nano Banana 2 Lite\",\"speed\":\"<2s\","
    "\"strengths\":\"Gemini image family, sub-2s, 14 aspect ratios incl. extreme\","
    "\"price\":\"~$0.04/image (1K fixed)\",\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\","
    "\"square\":\"1:1\",\"portrait\":\"9:16\"},\"defaults\":{\"num_images\":1,\"output_format\":\"png\","
    "\"safety_tolerance\":\"5\"},\"supports\":[\"aspect_ratio\",\"limit_generations\",\"num_images\","
    "\"output_format\",\"prompt\",\"safety_tolerance\",\"seed\",\"sync_mode\",\"system_prompt\",\"thinking_level\"],"
    "\"upscale\":false,\"edit_endpoint\":\"google/nano-banana-2-lite/edit\",\"edit_supports\":[\"aspect_ratio\","
    "\"image_urls\",\"num_images\",\"output_format\",\"prompt\",\"safety_tolerance\",\"seed\",\"sync_mode\","
    "\"system_prompt\"],\"max_reference_images\":4},"
    "\"fal-ai/recraft/v4.1/text-to-image\":{\"display\":\"Recraft V4.1\",\"speed\":\"~8s\","
    "\"strengths\":\"Design-first raster, brand systems, editorial\",\"price\":\"$0.035/image\","
    "\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\","
    "\"portrait\":\"portrait_16_9\"},\"defaults\":{\"enable_safety_checker\":false},"
    "\"supports\":[\"background_color\",\"colors\",\"enable_safety_checker\",\"image_size\",\"prompt\"],"
    "\"upscale\":false},\"xai/grok-imagine-image/v2.0/text-to-image\":{\"display\":\"Grok Imagine Image 2.0\","
    "\"speed\":\"~5s\",\"strengths\":\"xAI. Design-grade typography/layout, instruction following\","
    "\"price\":\"$0.06/image (1K medium)\",\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\","
    "\"square\":\"1:1\",\"portrait\":\"9:16\"},\"defaults\":{\"num_images\":1,\"output_format\":\"png\","
    "\"resolution\":\"1k\",\"quality\":\"medium\"},\"supports\":[\"aspect_ratio\",\"num_images\",\"output_format\","
    "\"prompt\",\"quality\",\"resolution\",\"sync_mode\"],\"upscale\":false,"
    "\"edit_endpoint\":\"xai/grok-imagine-image/v2.0/edit\",\"edit_supports\":[\"image_urls\",\"num_images\","
    "\"output_format\",\"prompt\",\"quality\",\"resolution\",\"sync_mode\"],\"max_reference_images\":3},"
    "\"fal-ai/kling-image/v3/text-to-image\":{\"display\":\"Kling Image v3\",\"speed\":\"~10s\","
    "\"strengths\":\"Kuaishou. Realistic detail, cheap native 2K, wide AR set\",\"price\":\"$0.028/image\","
    "\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},"
    "\"defaults\":{\"num_images\":1,\"output_format\":\"png\",\"resolution\":\"2K\"},\"supports\":[\"aspect_ratio\","
    "\"negative_prompt\",\"num_images\",\"output_format\",\"prompt\",\"resolution\",\"sync_mode\"],"
    "\"upscale\":false,\"edit_endpoint\":\"fal-ai/kling-image/v3/image-to-image\","
    "\"edit_supports\":[\"aspect_ratio\",\"image_url\",\"num_images\",\"output_format\",\"prompt\",\"resolution\","
    "\"sync_mode\"],\"max_reference_images\":1,\"edit_image_param\":\"image_url\"},"
    "\"meta/muse-image/text-to-image\":{\"display\":\"Meta Muse Image\",\"speed\":\"~5s\","
    "\"strengths\":\"Meta. Realism + typography at commodity price\",\"price\":\"$0.01/image\","
    "\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},"
    "\"defaults\":{\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"aspect_ratio\",\"num_images\","
    "\"output_format\",\"prompt\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"meta/muse-image/edit\","
    "\"edit_supports\":[\"image_urls\",\"num_images\",\"output_format\",\"prompt\",\"sync_mode\"],"
    "\"max_reference_images\":10}}";

#define IT_DEFAULT_MODEL "fal-ai/flux-2/klein/9b"
#define IT_DEFAULT_ASPECT "landscape"
#define IT_NOUS_PROVIDER "nous"
#define IT_UPSCALER_MODEL "fal-ai/clarity-upscaler"
#define IT_FAL_INLINE_BUDGET 3500000L

static json_t *it_default_catalog(void) {
    static json_t *cat = NULL;
    if (!cat) cat = json_parse(IT_FAL_MODELS_JSON, NULL);
    return cat;
}
/* Per-case override (in.catalog) or the embedded default. Borrowed. */
static const json_t *it_catalog(const json_t *in) {
    json_t *ov = json_obj_get(in, "catalog");
    if (j_is_obj(ov)) return ov;
    return it_default_catalog();
}
static const char *it_size_key(const char *style) {
    if (strcmp(style, "image_size_preset") == 0) return "image_size";
    if (strcmp(style, "gpt_literal") == 0) return "image_size";
    if (strcmp(style, "aspect_ratio") == 0) return "aspect_ratio";
    return NULL;
}
static bool it_valid_aspect(const char *s) {
    return strcmp(s, "landscape") == 0 || strcmp(s, "square") == 0 ||
           strcmp(s, "portrait") == 0;
}

/* -- _build_payload core (pure; catalog injected for per-case overrides) -- */
static json_t *img_build_payload_core(const json_t *catalog, const char *model_id,
                                      const json_t *prompt, const json_t *aspect_ratio,
                                      const json_t *seed, const json_t *overrides,
                                      const json_t *image_urls) {
    const json_t *meta = json_obj_get(catalog, model_id);
    if (!meta) {
        char *rk = it_repr_str(model_id);
        json_t *e = it_raise("KeyError", rk ? rk : "''");
        free(rk);
        return e;
    }
    bool edit = jpresent(image_urls);
    const json_t *supports;
    if (edit) {
        const json_t *es = json_obj_get(meta, "edit_supports");
        supports = jtruth(es) ? es : NULL;   /* (meta.get("edit_supports") or set()) */
    } else {
        supports = json_obj_get(meta, "supports");
        if (!supports) return it_raise("KeyError", "'supports'");
    }
    const json_t *sizes = json_obj_get(meta, "sizes");
    if (!sizes) return it_raise("KeyError", "'sizes'");

    /* aspect = (aspect_ratio or DEFAULT).lower().strip() */
    char *aspect;
    if (!jtruth(aspect_ratio)) {
        aspect = strdup(IT_DEFAULT_ASPECT);
    } else if (!j_is_str(aspect_ratio)) {
        char *m = it_attr_msg(aspect_ratio, "lower");
        json_t *e = it_raise("AttributeError", m ? m : "");
        free(m);
        return e;
    } else {
        char *lo = it_lower(sval(aspect_ratio));
        aspect = it_strip(lo);
        free(lo);
    }
    if (!json_has(sizes, aspect)) {
        free(aspect);
        aspect = strdup(IT_DEFAULT_ASPECT);
    }

    /* payload = dict(meta.get("defaults", {})) */
    json_t *payload = json_object();
    const json_t *defaults = json_obj_get(meta, "defaults");
    if (j_is_obj(defaults)) {
        for (size_t i = 0; i < defaults->c.count; i++)
            json_set(payload, defaults->c.keys[i], json_copy(defaults->c.items[i]));
    }
    /* payload["prompt"] = (prompt or "").strip() */
    char *pstr;
    if (!jtruth(prompt)) {
        pstr = strdup("");
    } else if (!j_is_str(prompt)) {
        char *m = it_attr_msg(prompt, "strip");
        json_t *e = it_raise("AttributeError", m ? m : "");
        free(m); free(aspect); json_free(payload);
        return e;
    } else {
        pstr = it_strip(sval(prompt));
    }
    json_set_hdr(payload, "prompt", pstr);
    free(pstr);

    /* required = {"prompt"} (+ the source-image key on edits) */
    json_t *required = json_array();
    json_append(required, json_string("prompt"));
    if (edit) {
        const json_t *eip = json_obj_get(meta, "edit_image_param");
        const char *image_param = jtruth(eip) ? sval(eip) : "image_urls";
        if (strcmp(image_param, "image_urls") != 0) {
            /* next(iter(image_urls)) -- empty -> StopIteration */
            if (!j_is_arr(image_urls) || json_len(image_urls) == 0) {
                free(aspect); json_free(payload); json_free(required);
                return it_raise("StopIteration", "");
            }
            json_set(payload, image_param, json_copy(json_get(image_urls, 0)));
        } else {
            json_set(payload, "image_urls", json_copy(image_urls));
        }
        json_append(required, json_string(image_param));
    }

    const json_t *style = json_obj_get(meta, "size_style");
    if (!style) {
        free(aspect); json_free(payload); json_free(required);
        return it_raise("KeyError", "'size_style'");
    }
    const char *size_key = it_size_key(sval(style));
    if (!size_key && !edit) {
        char *rk = it_repr_str(sval(style));
        size_t n = strlen(rk ? rk : "''") + 32;
        char *m = (char *)malloc(n);
        if (m) snprintf(m, n, "Unknown size_style: %s", rk ? rk : "''");
        json_t *e = it_raise("ValueError", m ? m : "");
        free(m); free(rk); free(aspect); json_free(payload); json_free(required);
        return e;
    }
    if (size_key && (!edit || it_in_arr(supports, size_key))) {
        const json_t *sv = json_obj_get(sizes, aspect);
        if (!sv) {
            char *rk = it_repr_str(aspect);
            json_t *e = it_raise("KeyError", rk ? rk : "''");
            free(rk); free(aspect); json_free(payload); json_free(required);
            return e;
        }
        json_set(payload, size_key, json_copy(sv));
    }
    /* isinstance(seed, int) -- True for bool too; float seeds out of domain */
    if (seed && (seed->type == JSON_NUMBER || seed->type == JSON_BOOL))
        json_set(payload, "seed", json_copy(seed));
    /* payload.update({k: v for overrides if v is not None}) */
    if (j_is_obj(overrides)) {
        for (size_t i = 0; i < overrides->c.count; i++) {
            if (!j_is_null(overrides->c.items[i]))
                json_set(payload, overrides->c.keys[i], json_copy(overrides->c.items[i]));
        }
    }
    /* filter to supports | required (insertion order preserved) */
    json_t *result = json_object();
    for (size_t i = 0; i < payload->c.count; i++) {
        const char *k = payload->c.keys[i];
        if (it_in_arr(supports, k) || it_in_arr(required, k))
            json_set(result, k, json_copy(payload->c.items[i]));
    }
    free(aspect);
    json_free(payload);
    json_free(required);
    return result;
}

/* PoP: img_build_payload @ tools/image_generation_tool.py:_build_payload */
static json_t *img_build_payload(it_io_t *io, json_t *in) {
    (void)io;
    return img_build_payload_core(it_catalog(in), json_get_str(in, "model_id", ""),
                                  json_obj_get(in, "prompt"),
                                  json_obj_get(in, "aspect_ratio"),
                                  json_obj_get(in, "seed"),
                                  json_obj_get(in, "overrides"),
                                  json_obj_get(in, "image_urls"));
}

/* PoP: img_build_fal_payload @ tools/image_generation_tool.py:_build_fal_payload */
static json_t *img_build_fal_payload(it_io_t *io, json_t *in) {
    (void)io;
    return img_build_payload_core(it_catalog(in), json_get_str(in, "model_id", ""),
                                  json_obj_get(in, "prompt"),
                                  json_obj_get(in, "aspect_ratio"),
                                  json_obj_get(in, "seed"),
                                  json_obj_get(in, "overrides"),
                                  NULL);
}

/* PoP: img_build_fal_edit_payload @ tools/image_generation_tool.py:_build_fal_edit_payload */
static json_t *img_build_fal_edit_payload(it_io_t *io, json_t *in) {
    (void)io;
    return img_build_payload_core(it_catalog(in), json_get_str(in, "model_id", ""),
                                  json_obj_get(in, "prompt"),
                                  json_obj_get(in, "aspect_ratio"),
                                  json_obj_get(in, "seed"),
                                  json_obj_get(in, "overrides"),
                                  json_obj_get(in, "image_urls"));
}

/* PoP: img_read_image_gen_key @ tools/image_generation_tool.py:_read_image_gen_key */
static json_t *img_read_image_gen_key(it_io_t *io, json_t *in) {
    const char *key = json_get_str(in, "key", "");
    json_t *cfg = it_call(io, "load_config", json_object());
    if (it_raised(cfg)) {
        if (it_exc_is_exception(cfg)) {
            /* except Exception as exc: logger.debug("Could not read image_gen.%s: %s", key, exc) */
            const char *em = it_exc_msg(cfg);
            size_t n = strlen(key) + strlen(em) + 48;
            char *m = (char *)malloc(n);
            if (m) snprintf(m, n, "Could not read image_gen.%s: %s", key, em);
            json_t *lr = it_log(io, "logger.debug", m ? m : "");
            free(m);
            json_free(cfg);
            if (it_raised(lr)) return lr;
            json_free(lr);
            return json_null();
        }
        return cfg;   /* BaseException passthrough */
    }
    const json_t *section = j_is_obj(cfg) ? json_obj_get(cfg, "image_gen") : NULL;
    const json_t *value = j_is_obj(section) ? json_obj_get(section, key) : NULL;
    json_t *ret = json_null();
    if (j_is_str(value)) {
        char *s = it_strip(sval(value));
        if (s && s[0]) ret = json_string(s);
        free(s);
    }
    json_free(cfg);
    return ret;
}

/* PoP: img_plugin_provider_name @ tools/image_generation_tool.py:_plugin_provider_name */
static json_t *img_plugin_provider_name(it_io_t *io, json_t *in) {
    (void)in;
    json_t *c = it_call(io, "read_configured_image_provider", json_object());
    if (it_raised(c)) return c;
    if (!jtruth(c) ||
        (j_is_str(c) && (strcmp(sval(c), "fal") == 0 || strcmp(sval(c), IT_NOUS_PROVIDER) == 0))) {
        json_free(c);
        return json_null();
    }
    return c;
}

/* PoP: img_format_images @ tools/image_generation_tool.py:_format_images */
static json_t *img_format_images(it_io_t *io, json_t *in) {
    const json_t *images = json_obj_get(in, "images");
    const json_t *should_upscale = json_obj_get(in, "should_upscale");
    const json_t *prompt = json_obj_get(in, "prompt");
    if (!j_is_arr(images)) {
        char *m = (char *)malloc(64);
        if (m) snprintf(m, 64, "'%s' object is not iterable", it_tname(images));
        json_t *e = it_raise("TypeError", m ? m : "");
        free(m);
        return e;
    }
    json_t *result = json_array();
    for (size_t i = 0; i < json_len(images); i++) {
        const json_t *img = json_get(images, i);
        if (!j_is_obj(img) || !json_has(img, "url")) continue;
        if (jtruth(should_upscale)) {
            if (!j_is_str(prompt)) {
                char *m = it_attr_msg(prompt, "strip");
                json_t *e = it_raise("AttributeError", m ? m : "");
                free(m); json_free(result);
                return e;
            }
            char *ps = it_strip(sval(prompt));
            json_t *a = json_object();
            json_set(a, "image_url", json_copy(json_obj_get(img, "url")));
            json_set_hdr(a, "original_prompt", ps ? ps : "");
            free(ps);
            json_t *up = it_call(io, "upscale_image", a);
            if (it_raised(up)) { json_free(result); return up; }
            if (jtruth(up)) { json_append(result, up); continue; }
            json_free(up);
            json_t *lr = it_log(io, "logger.warning",
                                "Using original image as fallback (upscale failed)");
            if (it_raised(lr)) { json_free(result); return lr; }
            json_free(lr);
        }
        json_t *e = json_object();
        json_set(e, "url", json_copy(json_obj_get(img, "url")));
        const json_t *w = json_obj_get(img, "width");
        json_set(e, "width", w ? json_copy(w) : json_number(0));
        const json_t *h = json_obj_get(img, "height");
        json_set(e, "height", h ? json_copy(h) : json_number(0));
        json_set(e, "upscaled", json_bool(false));
        json_append(result, e);
    }
    return result;
}

/* -- _shrink_data_url: PIL fully seamed (descriptor doubles) ---------------
 * pil_open/pil_load raises of OSError/ValueError/DecompressionBombError (or any
 * subclass) return the url untouched; other kinds propagate. The encode loop
 * (side *= 0.75 until len(out) <= budget or side <= 256) runs REAL. */
static bool it_shrink_open_exc(const json_t *r) {
    const char *k = it_exc_kind(r);
    return it_exc_is_a(k, "OSError") || it_exc_is_a(k, "ValueError") ||
           strcmp(k, "DecompressionBombError") == 0;
}

/* PoP: img_shrink_data_url @ tools/image_generation_tool.py:_shrink_data_url */
static json_t *img_shrink_data_url(it_io_t *io, json_t *in) {
    const char *url = json_get_str(in, "url", "");
    long budget = (long)json_get_num(in, "budget", 0);
    /* url.partition(",")[2] */
    const char *comma = strchr(url, ',');
    const char *part = comma ? comma + 1 : "";

    json_t *a = json_object();
    json_set_hdr(a, "b64", part);
    json_t *img = it_call(io, "pil_open", a);
    if (it_raised(img)) {
        if (it_shrink_open_exc(img)) { json_free(img); return json_string(url); }
        return img;
    }
    json_t *lr = it_call(io, "pil_load", json_object());
    if (it_raised(lr)) {
        json_free(img);
        if (it_shrink_open_exc(lr)) { json_free(lr); return json_string(url); }
        return lr;
    }
    json_free(lr);

    a = json_object();
    json_set(a, "img", json_copy(img));
    json_t *tr = it_call(io, "exif_transpose", a);
    if (it_raised(tr)) { json_free(img); return tr; }
    json_free(img);
    img = tr;

    /* has_alpha = "A" in img.getbands() or "transparency" in img.info */
    bool has_alpha = it_in_arr(json_obj_get(img, "bands"), "A");
    const json_t *info = json_obj_get(img, "info");
    if (!has_alpha && j_is_obj(info) && json_has(info, "transparency")) has_alpha = true;
    const char *fmt = has_alpha ? "PNG" : "JPEG";
    const char *mime = has_alpha ? "image/png" : "image/jpeg";

    a = json_object();
    json_set_hdr(a, "mode", has_alpha ? "RGBA" : "RGB");
    json_t *cv = it_call(io, "pil_convert", a);
    if (it_raised(cv)) { json_free(img); return cv; }
    json_free(img);
    img = cv;

    /* side = max(img.size) */
    const json_t *size = json_obj_get(img, "size");
    if (!j_is_arr(size) || json_len(size) == 0) {
        json_free(img);
        return it_raise("ValueError", "max() arg is an empty sequence");
    }
    long side = (long)(j_is_num(json_get(size, 0)) ? json_get(size, 0)->num_val : 0);
    for (size_t i = 1; i < json_len(size); i++) {
        json_t *e = json_get(size, i);
        long v = (long)(j_is_num(e) ? e->num_val : 0);
        if (v > side) side = v;
    }

    while (1) {
        a = json_object();
        json_set(a, "side", json_number((double)side));
        json_set_hdr(a, "fmt", fmt);
        json_set(a, "img", json_copy(img));
        json_t *enc = it_call(io, "pil_encode", a);
        if (it_raised(enc)) { json_free(img); return enc; }
        const char *b64 = json_get_str(enc, "b64", "");
        size_t on = strlen(mime) + strlen(b64) + 24;
        char *out = (char *)malloc(on);
        if (out) snprintf(out, on, "data:%s;base64,%s", mime, b64);
        size_t outlen = out ? strlen(out) : 0;
        if ((long)outlen <= budget || side <= 256) {
            /* logger.info("Downscaled FAL source image to %s (%d bytes inline)",
             *             frame.size, len(out)) -- frame.size from the script */
            const json_t *fs = json_obj_get(enc, "size");
            long w = 0, h = 0;
            if (j_is_arr(fs) && json_len(fs) >= 2) {
                json_t *w0 = json_get(fs, 0); json_t *h0 = json_get(fs, 1);
                w = (long)(j_is_num(w0) ? w0->num_val : 0);
                h = (long)(j_is_num(h0) ? h0->num_val : 0);
            }
            char msg[160];
            snprintf(msg, sizeof msg,
                     "Downscaled FAL source image to (%ld, %ld) (%zu bytes inline)",
                     w, h, outlen);
            json_t *li = it_log(io, "logger.info", msg);
            if (it_raised(li)) { free(out); json_free(enc); json_free(img); return li; }
            json_free(li);
            json_free(enc);
            json_free(img);
            json_t *ret = json_string(out ? out : "");
            free(out);
            return ret;
        }
        free(out);
        json_free(enc);
        side = (long)((double)side * 0.75);   /* int(side * 0.75) truncates */
    }
}

/* PoP: img_fit_inline_budget @ tools/image_generation_tool.py:_fit_inline_budget */
static json_t *img_fit_inline_budget(it_io_t *io, json_t *in) {
    const json_t *urls = json_obj_get(in, "urls");
    long budget = (long)json_get_num(in, "budget", 0);
    if (!j_is_arr(urls)) {
        char *m = (char *)malloc(64);
        if (m) snprintf(m, 64, "'%s' object is not iterable", it_tname(urls));
        json_t *e = it_raise("TypeError", m ? m : "");
        free(m);
        return e;
    }
    /* inline = [u for u in urls if u.startswith("data:")] -- validates ALL */
    long n = 0; long sum = 0;
    for (size_t i = 0; i < json_len(urls); i++) {
        const json_t *u = json_get(urls, i);
        if (!j_is_str(u)) {
            char *m = it_attr_msg(u, "startswith");
            json_t *e = it_raise("AttributeError", m ? m : "");
            free(m);
            return e;
        }
        if (it_startswith(sval(u), "data:")) { n++; sum += (long)strlen(sval(u)); }
    }
    if (sum <= budget) return json_copy(urls);
    if (n == 0) return it_raise("ZeroDivisionError", "division by zero");
    long share = it_floordiv(budget, n);
    json_t *result = json_array();
    for (size_t i = 0; i < json_len(urls); i++) {
        const json_t *u = json_get(urls, i);
        if (it_startswith(sval(u), "data:") && (long)strlen(sval(u)) > share) {
            json_t *sub = json_object();
            json_set_hdr(sub, "url", sval(u));
            json_set(sub, "budget", json_number((double)share));
            json_t *r = img_shrink_data_url(io, sub);
            json_free(sub);
            if (it_raised(r)) { json_free(result); return r; }
            json_append(result, r);
        } else {
            json_append(result, json_copy(u));
        }
    }
    return result;
}

/* PoP: img_inline_local_sources @ tools/image_generation_tool.py:_inline_local_sources */
static json_t *img_inline_local_sources(it_io_t *io, json_t *in) {
    const json_t *sources = json_obj_get(in, "sources");
    if (!j_is_arr(sources)) {
        char *m = (char *)malloc(64);
        if (m) snprintf(m, 64, "'%s' object is not iterable", it_tname(sources));
        json_t *e = it_raise("TypeError", m ? m : "");
        free(m);
        return e;
    }
    json_t *result = json_array();
    for (size_t i = 0; i < json_len(sources); i++) {
        /* resolve_local_source_to_data_url(s, None) -- None task_id key dropped */
        json_t *a = json_object();
        json_set(a, "source", json_copy(json_get(sources, i)));
        json_t *r = it_call(io, "resolve_local_source_to_data_url", a);
        if (it_raised(r)) {
            if (strcmp(it_exc_kind(r), "ImageResolutionError") == 0) {
                const char *em = it_exc_msg(r);
                size_t n = strlen(em) + 40;
                char *m = (char *)malloc(n);
                if (m) snprintf(m, n, "Could not read source image: %s", em);
                json_free(r); json_free(result);
                json_t *e = it_raise("ValueError", m ? m : "");
                free(m);
                return e;
            }
            json_free(result);
            return r;
        }
        /* _run_async(<resolve result>) */
        json_t *a2 = json_object();
        json_set(a2, "arg", r);   /* adopts r */
        json_t *r2 = it_call(io, "_run_async", a2);
        if (it_raised(r2)) {
            if (strcmp(it_exc_kind(r2), "ImageResolutionError") == 0) {
                const char *em = it_exc_msg(r2);
                size_t n = strlen(em) + 40;
                char *m = (char *)malloc(n);
                if (m) snprintf(m, n, "Could not read source image: %s", em);
                json_free(r2); json_free(result);
                json_t *e = it_raise("ValueError", m ? m : "");
                free(m);
                return e;
            }
            json_free(result);
            return r2;
        }
        json_append(result, r2);
    }
    return result;
}

/* PoP: img_inline_fal_sources @ tools/image_generation_tool.py:_inline_fal_sources */
static json_t *img_inline_fal_sources(it_io_t *io, json_t *in) {
    json_t *sub = json_object();
    json_set(sub, "sources", copy_or_null(json_obj_get(in, "sources")));
    json_t *r = img_inline_local_sources(io, sub);
    json_free(sub);
    if (it_raised(r)) return r;
    json_t *sub2 = json_object();
    json_set(sub2, "urls", r);   /* adopts r */
    json_set(sub2, "budget", json_number((double)IT_FAL_INLINE_BUDGET));
    json_t *r2 = img_fit_inline_budget(io, sub2);
    json_free(sub2);
    return r2;
}

/* -- provider JSON envelopes (real json.dumps, default separators) --------- */
static json_t *img_provider_error_str(const char *error, const char *error_type) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(false));
    json_set(o, "image", json_null());
    json_set_hdr(o, "error", error);
    json_set_hdr(o, "error_type", error_type);
    char *s = it_json_dumps(o);
    json_free(o);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: img_provider_error @ tools/image_generation_tool.py:_provider_error */
static json_t *img_provider_error(it_io_t *io, json_t *in) {
    (void)io;
    return img_provider_error_str(json_get_str(in, "error", ""),
                                  json_get_str(in, "error_type", ""));
}

/* PoP: img_provider_result @ tools/image_generation_tool.py:_provider_result */
static json_t *img_provider_result(it_io_t *io, json_t *in) {
    (void)io;
    const json_t *result = json_obj_get(in, "result");
    if (!j_is_obj(result))
        return img_provider_error_str(json_get_str(in, "contract_error", ""),
                                      "provider_contract");
    char *s = it_json_dumps(result);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: img_check_fal_api_key @ tools/image_generation_tool.py:check_fal_api_key */
static json_t *img_check_fal_api_key(it_io_t *io, json_t *in) {
    (void)in;
    json_t *gw = it_call(io, "resolve_managed_fal_gateway", json_object());
    if (it_raised(gw)) {
        if (it_exc_is_a(it_exc_kind(gw), "ValueError")) {
            json_free(gw);
            return json_bool(false);
        }
        return gw;   /* only ValueError is caught; everything else propagates */
    }
    if (jtruth(gw)) { json_free(gw); return json_bool(true); }
    json_free(gw);
    json_t *fk = it_call(io, "fal_key_set", json_object());
    if (it_raised(fk)) return fk;
    /* `bool(gateway) or fal_key_is_configured()` -- falsy gateway returns the
     * RAW key value (str/None), not a coerced bool. */
    return fk;
}

/* PoP: img_get_plugin_provider @ tools/image_generation_tool.py:_get_plugin_provider */
static json_t *img_get_plugin_provider(it_io_t *io, json_t *in) {
    const char *name = json_get_str(in, "name", "");
    bool force = json_get_bool(in, "force", false);
    json_t *a = json_object();
    if (force) json_set(a, "force", json_bool(true));
    json_t *r = it_call(io, "ensure_plugins_discovered", a);
    if (it_raised(r)) return r;
    json_free(r);
    json_t *a2 = json_object();
    json_set_hdr(a2, "name", name);
    return it_call(io, "get_provider", a2);
}

/* PoP: img_add_provider_kwargs @ tools/image_generation_tool.py:_add_provider_kwargs */
static json_t *img_add_provider_kwargs(it_io_t *io, json_t *in) {
    const json_t *kwargs = json_obj_get(in, "kwargs");
    json_t *work = j_is_obj(kwargs) ? json_copy(kwargs) : json_object();
    const json_t *model = json_obj_get(in, "model");
    if (jtruth(model)) json_set(work, "model", json_copy(model));
    const json_t *controls = json_obj_get(in, "controls");
    if (j_is_obj(controls) && jtruth(controls)) {
        for (size_t i = 0; i < controls->c.count; i++)
            json_set(work, controls->c.keys[i], json_copy(controls->c.items[i]));
    }
    const json_t *image_url = json_obj_get(in, "image_url");
    if (j_is_str(image_url)) {
        char *s = it_strip(sval(image_url));
        if (s && s[0]) json_set_hdr(work, "image_url", s);
        free(s);
    }
    const json_t *refs = json_obj_get(in, "reference_image_urls");
    if (jpresent(refs)) {
        json_t *a = json_object();
        json_set(a, "refs", json_copy(refs));
        json_t *r = it_call(io, "normalize_reference_images", a);
        if (it_raised(r)) { json_free(work); return r; }
        if (jtruth(r)) json_set(work, "reference_image_urls", r);
        else json_free(r);
    }
    const json_t *upscale = json_obj_get(in, "upscale");
    if (jpresent(upscale)) json_set(work, "upscale", json_bool(jtruth(upscale)));
    return work;
}

/* PoP: img_declared_controls @ tools/image_generation_tool.py:_declared_controls */
static json_t *img_declared_controls(it_io_t *io, json_t *in) {
    const json_t *provider = json_obj_get(in, "provider");
    const json_t *controls = json_obj_get(in, "controls");
    if (!jtruth(controls)) return json_null();
    /* provider.capabilities() -- a non-double provider (None/str) raises
     * AttributeError INSIDE the try -> swallowed to None with NO seam event. */
    if (!j_is_obj(provider)) return json_null();
    json_t *caps = it_call(io, "provider.capabilities", json_object());
    if (it_raised(caps)) {
        if (it_exc_is_exception(caps)) { json_free(caps); return json_null(); }
        return caps;   /* BaseException passthrough */
    }
    /* OWNED snapshot: caps is freed before the filter loop, so a borrowed
     * creative_controls pointer would dangle (UAF -> silently empty filter). */
    json_t *declared = NULL;
    if (jtruth(caps)) {
        if (!j_is_obj(caps)) {
            /* truthy non-dict: .get raises AttributeError -> caught -> None */
            json_free(caps);
            return json_null();
        }
        const json_t *cc = json_obj_get(caps, "creative_controls");
        if (jtruth(cc)) declared = json_copy(cc);
    }
    json_free(caps);
    /* {name: value for controls.items() if name in declared} or None --
     * controls.items() is OUTSIDE the try: a truthy non-dict raises. */
    if (!j_is_obj(controls)) {
        char *m = it_attr_msg(controls, "items");
        json_t *e = it_raise("AttributeError", m ? m : "");
        free(m);
        json_free(declared);
        return e;
    }
    json_t *result = json_object();
    for (size_t i = 0; i < controls->c.count; i++) {
        if (it_in_arr(declared, controls->c.keys[i]))
            json_set(result, controls->c.keys[i], json_copy(controls->c.items[i]));
    }
    json_free(declared);
    if (result->c.count == 0) { json_free(result); return json_null(); }
    return result;
}

/* PoP: img_prepare_fal_request @ tools/image_generation_tool.py:_prepare_fal_request */
static json_t *img_prepare_fal_request(it_io_t *io, json_t *in) {
    const char *model_id = json_get_str(in, "model_id", "");
    const json_t *meta = json_obj_get(in, "meta");
    const json_t *prompt = json_obj_get(in, "prompt");
    const json_t *aspect_ratio = json_obj_get(in, "aspect_ratio");
    const json_t *source_images = json_obj_get(in, "source_images");

    if (!j_is_str(prompt)) {
        return it_raise("ValueError", "Prompt is required and must be a non-empty string");
    }
    char *ps = it_strip(sval(prompt));
    bool prompt_ok = ps && ps[0];
    free(ps);
    if (!prompt_ok)
        return it_raise("ValueError", "Prompt is required and must be a non-empty string");

    if (!j_is_obj(meta)) {
        char *m = it_attr_msg(meta, "get");
        json_t *e = it_raise("AttributeError", m ? m : "");
        free(m);
        return e;
    }

    /* if not (fal_key_is_configured() or _resolve_managed_fal_gateway()): ... */
    json_t *fk = it_call(io, "fal_key_set", json_object());
    if (it_raised(fk)) return fk;
    if (!jtruth(fk)) {
        json_free(fk);
        json_t *gw = it_call(io, "resolve_managed_fal_gateway", json_object());
        if (it_raised(gw)) return gw;   /* selection-naming ValueError propagates */
        if (!jtruth(gw)) {
            json_free(gw);
            json_t *m = it_call(io, "build_no_backend_setup_message", json_object());
            if (it_raised(m)) return m;
            json_t *e = it_raise("ValueError", sval(m));
            json_free(m);
            return e;
        }
        json_free(gw);
    } else {
        json_free(fk);
    }

    const json_t *edit_endpoint = json_obj_get(meta, "edit_endpoint");
    const json_t *display = json_obj_get(meta, "display");
    char *display_str = display ? it_pystr(display) : strdup(model_id);

    if (jtruth(source_images) && !jtruth(edit_endpoint)) {
        /* f"Model '{display}' ({model_id}) is not capable of image-to-image / editing. ..." */
        const char *tail =
            ") is not capable of image-to-image / editing. "
            "Provide a text-only prompt (omit image_url), or switch to an edit-capable model "
            "via `hermes tools` \xe2\x86\x92 Image Generation.";
        size_t n = strlen(display_str) + strlen(model_id) + strlen(tail) + 16;
        char *m = (char *)malloc(n);
        if (m) snprintf(m, n, "Model '%s' (%s%s", display_str, model_id, tail);
        json_t *e = it_raise("ValueError", m ? m : "");
        free(m); free(display_str);
        return e;
    }

    /* aspect_lc = (aspect_ratio or DEFAULT).lower().strip() */
    char *aspect_lc;
    if (!jtruth(aspect_ratio)) {
        aspect_lc = strdup(IT_DEFAULT_ASPECT);
    } else if (!j_is_str(aspect_ratio)) {
        char *m = it_attr_msg(aspect_ratio, "lower");
        json_t *e = it_raise("AttributeError", m ? m : "");
        free(m); free(display_str);
        return e;
    } else {
        char *lo = it_lower(sval(aspect_ratio));
        aspect_lc = it_strip(lo);
        free(lo);
    }
    if (!it_valid_aspect(aspect_lc)) {
        char *ar = it_pystr(aspect_ratio);
        size_t n = strlen(ar) + 80;
        char *m = (char *)malloc(n);
        if (m) snprintf(m, n, "Invalid aspect_ratio '%s', defaulting to '%s'",
                        ar, IT_DEFAULT_ASPECT);
        free(ar);
        json_t *lr = it_log(io, "logger.warning", m ? m : "");
        free(m);
        if (it_raised(lr)) { free(aspect_lc); free(display_str); return lr; }
        json_free(lr);
        free(aspect_lc);
        aspect_lc = strdup(IT_DEFAULT_ASPECT);
    }

    char *prompt80 = it_slice(sval(prompt), 80);
    json_t *ret = NULL;

    if (jtruth(source_images)) {
        if (!j_is_arr(source_images)) {
            char *m = (char *)malloc(64);
            if (m) snprintf(m, 64, "'%s' object is not subscriptable", it_tname(source_images));
            ret = it_raise("TypeError", m ? m : "");
            free(m); free(prompt80); free(aspect_lc); free(display_str);
            return ret;
        }
        /* max_refs = int(meta.get("max_reference_images") or 1) */
        const json_t *mr = json_obj_get(meta, "max_reference_images");
        long max_refs = 1;
        if (jtruth(mr)) {
            if (j_is_num(mr)) max_refs = (long)mr->num_val;
            else if (mr->type == JSON_BOOL) max_refs = mr->bool_val ? 1 : 0;
        }
        json_t *clamped = json_array();
        size_t take = json_len(source_images);
        if (max_refs > 0 && (size_t)max_refs < take) take = (size_t)max_refs;
        if (max_refs <= 0) take = json_len(source_images);
        for (size_t i = 0; i < take; i++)
            json_append(clamped, json_copy(json_get(source_images, i)));

        json_t *sub = json_object();
        json_set(sub, "sources", clamped);   /* adopts clamped */
        json_t *inlined = img_inline_fal_sources(io, sub);
        json_free(sub);
        if (it_raised(inlined)) {
            free(prompt80); free(aspect_lc); free(display_str);
            return inlined;
        }
        /* _build_fal_edit_payload(model_id, prompt, clamped_sources, aspect_lc,
         *                         seed=seed, overrides=overrides) */
        size_t nsrc = json_len(inlined);
        json_t *sub2 = json_object();
        json_set_hdr(sub2, "model_id", model_id);
        json_set(sub2, "prompt", json_copy(prompt));
        json_set(sub2, "image_urls", inlined);   /* adopts inlined */
        json_set_hdr(sub2, "aspect_ratio", aspect_lc);
        json_set(sub2, "seed", copy_or_null(json_obj_get(in, "seed")));
        json_set(sub2, "overrides", copy_or_null(json_obj_get(in, "overrides")));
        json_t *ov = json_obj_get(in, "catalog");
        if (j_is_obj(ov)) json_set(sub2, "catalog", json_copy(ov));
        json_t *arguments = img_build_fal_edit_payload(io, sub2);
        json_free(sub2);
        if (it_raised(arguments)) {
            free(prompt80); free(aspect_lc); free(display_str);
            return arguments;
        }
        /* logger.info("Editing image with %s (%s) -- %d source image(s), prompt: %s", ...) */
        char *ep = it_pystr(edit_endpoint);
        size_t n = strlen(display_str) + strlen(ep) + strlen(prompt80) + 96;
        char *m = (char *)malloc(n);
        if (m) snprintf(m, n,
                        "Editing image with %s (%s) \xe2\x80\x94 %zu source image(s), prompt: %s",
                        display_str, ep, nsrc, prompt80);
        free(ep);
        json_t *lr = it_log(io, "logger.info", m ? m : "");
        free(m);
        if (it_raised(lr)) {
            json_free(arguments);
            free(prompt80); free(aspect_lc); free(display_str);
            return lr;
        }
        json_free(lr);
        ret = json_array();
        json_append(ret, copy_or_null(edit_endpoint));
        json_append(ret, arguments);   /* adopts arguments */
    } else {
        json_t *sub = json_object();
        json_set_hdr(sub, "model_id", model_id);
        json_set(sub, "prompt", json_copy(prompt));
        json_set_hdr(sub, "aspect_ratio", aspect_lc);
        json_set(sub, "seed", copy_or_null(json_obj_get(in, "seed")));
        json_set(sub, "overrides", copy_or_null(json_obj_get(in, "overrides")));
        json_t *ov = json_obj_get(in, "catalog");
        if (j_is_obj(ov)) json_set(sub, "catalog", json_copy(ov));
        json_t *arguments = img_build_fal_payload(io, sub);
        json_free(sub);
        if (it_raised(arguments)) {
            free(prompt80); free(aspect_lc); free(display_str);
            return arguments;
        }
        /* logger.info("Generating image with %s (%s) -- prompt: %s", ...) */
        size_t n = strlen(display_str) + strlen(model_id) + strlen(prompt80) + 80;
        char *m = (char *)malloc(n);
        if (m) snprintf(m, n,
                        "Generating image with %s (%s) \xe2\x80\x94 prompt: %s",
                        display_str, model_id, prompt80);
        json_t *lr = it_log(io, "logger.info", m ? m : "");
        free(m);
        if (it_raised(lr)) {
            json_free(arguments);
            free(prompt80); free(aspect_lc); free(display_str);
            return lr;
        }
        json_free(lr);
        ret = json_array();
        json_append(ret, json_string(model_id));
        json_append(ret, arguments);   /* adopts arguments */
    }
    free(prompt80);
    free(aspect_lc);
    free(display_str);
    return ret;
}

/* PoP: img_managed_model_plugin @ tools/image_generation_tool.py:_managed_model_plugin */
static json_t *img_managed_model_plugin(it_io_t *io, json_t *in) {
    (void)in;
    json_t *mid = it_call(io, "read_configured_image_model", json_object());
    if (it_raised(mid)) return mid;
    json_t *prov = it_call(io, "read_configured_image_provider", json_object());
    if (it_raised(prov)) { json_free(mid); return prov; }
    /* managed_route(_read_configured_image_provider(), model_id) -- None keys dropped */
    json_t *a = json_object();
    if (jpresent(prov)) json_set(a, "provider", json_copy(prov));
    if (jpresent(mid)) json_set(a, "model_id", json_copy(mid));
    json_free(prov);
    json_t *route = it_call(io, "managed_route", a);
    if (it_raised(route)) { json_free(mid); return route; }
    json_t *ret = json_null();
    if (j_is_str(route)) {
        if (strcmp(sval(route), "krea") == 0) {
            ret = json_array();
            json_append(ret, json_string("krea"));
            json_append(ret, mid);   /* adopts mid */
            mid = NULL;
        } else if (strcmp(sval(route), "portal") == 0) {
            ret = json_array();
            json_append(ret, json_string("nous"));
            json_append(ret, mid);
            mid = NULL;
        }
    }
    json_free(route);
    if (mid) json_free(mid);
    return ret;
}

/* display_name attribute read on the provider double: a dict descriptor missing
 * the key raises 'ProviderDouble'; a non-dict provider raises its own type name. */
static json_t *it_display_name_exc(const json_t *provider) {
    if (j_is_obj(provider))
        return it_raise("AttributeError",
                        "'ProviderDouble' object has no attribute 'display_name'");
    char *m = it_attr_msg(provider, "display_name");
    json_t *e = it_raise("AttributeError", m ? m : "");
    free(m);
    return e;
}

/* PoP: img_maybe_route_managed_model @ tools/image_generation_tool.py:_maybe_route_managed_model */
static json_t *img_maybe_route_managed_model(it_io_t *io, json_t *in) {
    json_t *target = img_managed_model_plugin(io, in);
    if (it_raised(target)) return target;
    if (j_is_null(target)) return target;
    const char *plugin_name = sval(json_get(target, 0));
    const json_t *model_id = json_get(target, 1);   /* borrowed from target */
    bool is_krea = strcmp(plugin_name, "krea") == 0;
    json_t *provider = NULL;
    json_t *exc = NULL;   /* caught Exception from the routing try-block */

    if (is_krea) {
        json_t *g = it_call(io, "resolve_managed_krea_gateway", json_object());
        if (it_raised(g)) {
            if (it_exc_is_exception(g)) exc = g;
            else { json_free(target); return g; }
        } else if (j_is_null(g)) {
            json_free(g); json_free(target);
            return json_null();   /* no reachable Krea gateway -> fall through */
        } else {
            json_free(g);
        }
    }
    if (!exc) {
        json_t *sub = json_object();
        json_set_hdr(sub, "name", plugin_name);
        json_t *p = img_get_plugin_provider(io, sub);
        json_free(sub);
        if (it_raised(p)) {
            if (it_exc_is_exception(p)) exc = p;
            else { json_free(target); return p; }
        } else if (j_is_null(p)) {
            /* scripted None arrives as a JSON null node: provider is None
             * -> the `if provider is None` branch below (error / fallthrough). */
            json_free(p);
        } else {
            provider = p;
        }
    }
    if (exc) {
        /* except Exception as exc: logger.debug("Managed %s routing unavailable: %s", ...) */
        const char *em = it_exc_msg(exc);
        size_t n = strlen(plugin_name) + strlen(em) + 48;
        char *m = (char *)malloc(n);
        if (m) snprintf(m, n, "Managed %s routing unavailable: %s", plugin_name, em);
        json_t *lr = it_log(io, "logger.debug", m ? m : "");
        free(m);
        json_free(exc);
        if (it_raised(lr)) {
            json_free(target);
            if (provider) json_free(provider);
            return lr;
        }
        json_free(lr);
        provider = NULL;
    }
    if (!provider) {
        if (is_krea) { json_free(target); return json_null(); }
        char *mid = it_pystr(model_id);
        const char *tail =
            "' is a Nous Portal model but the Portal image backend is not "
            "available. Pick another model via `hermes tools` \xe2\x86\x92 Image Generation.";
        size_t n = strlen(mid) + strlen(tail) + 24;
        char *m = (char *)malloc(n);
        if (m) snprintf(m, n, "image_gen.model='%s%s", mid, tail);
        free(mid);
        json_t *r = img_provider_error_str(m ? m : "", "provider_not_registered");
        free(m);
        json_free(target);
        return r;
    }

    json_t *kwargs = json_object();
    json_set(kwargs, "prompt", copy_or_null(json_obj_get(in, "prompt")));
    json_set(kwargs, "aspect_ratio", copy_or_null(json_obj_get(in, "aspect_ratio")));
    json_set(kwargs, "model", copy_or_null(model_id));

    /* controls=_declared_controls(provider, controls) -- kwarg evaluated FIRST */
    json_t *dc_in = json_object();
    json_set(dc_in, "provider", json_copy(provider));
    json_set(dc_in, "controls", copy_or_null(json_obj_get(in, "controls")));
    json_t *dc = img_declared_controls(io, dc_in);
    json_free(dc_in);
    if (it_raised(dc)) {
        json_free(kwargs); json_free(provider); json_free(target);
        return dc;
    }
    json_t *ak_in = json_object();
    json_set(ak_in, "kwargs", json_copy(kwargs));
    json_set(ak_in, "image_url", copy_or_null(json_obj_get(in, "image_url")));
    json_set(ak_in, "reference_image_urls", copy_or_null(json_obj_get(in, "reference_image_urls")));
    json_set(ak_in, "upscale", copy_or_null(json_obj_get(in, "upscale")));
    json_set(ak_in, "controls", dc);   /* adopts dc */
    json_t *kw2 = img_add_provider_kwargs(io, ak_in);
    json_free(ak_in);
    if (it_raised(kw2)) {
        json_free(kwargs); json_free(provider); json_free(target);
        return kw2;
    }
    json_free(kwargs);
    kwargs = kw2;

    /* provider.generate(**kwargs) -- splat: args object IS kwargs. A non-dict
     * provider raises AttributeError on the attribute access (NO seam event). */
    json_t *gen;
    if (j_is_obj(provider)) {
        gen = it_call(io, "provider.generate", json_copy(kwargs));
    } else {
        char *m = it_attr_msg(provider, "generate");
        gen = it_raise("AttributeError", m ? m : "");
        free(m);
    }
    if (it_raised(gen)) {
        if (it_exc_is_exception(gen)) {
            const char *em = it_exc_msg(gen);
            size_t n = strlen(plugin_name) + strlen(em) + 40;
            char *m = (char *)malloc(n);
            if (m) snprintf(m, n, "Managed %s routing failed: %s", plugin_name, em);
            json_t *lr = it_log(io, "logger.warning", m ? m : "");
            free(m);
            if (it_raised(lr)) {
                json_free(gen); json_free(kwargs); json_free(provider); json_free(target);
                return lr;
            }
            json_free(lr);
            const json_t *dn = j_is_obj(provider) ? json_obj_get(provider, "display_name") : NULL;
            if (!dn) {
                json_t *e = it_display_name_exc(provider);
                json_free(gen); json_free(kwargs); json_free(provider); json_free(target);
                return e;
            }
            char *dns = it_pystr(dn);
            n = strlen(dns) + strlen(em) + 40;
            m = (char *)malloc(n);
            if (m) snprintf(m, n, "Managed %s generation error: %s", dns, em);
            free(dns);
            json_t *r = img_provider_error_str(m ? m : "", "provider_exception");
            free(m);
            json_free(gen); json_free(kwargs); json_free(provider); json_free(target);
            return r;
        }
        json_free(kwargs); json_free(provider); json_free(target);
        return gen;   /* BaseException passthrough */
    }

    /* success: f"{provider.display_name} provider returned a non-dict result"
     * is evaluated EAGERLY (before _provider_result), even for dict results. */
    const json_t *dn = j_is_obj(provider) ? json_obj_get(provider, "display_name") : NULL;
    if (!dn) {
        json_t *e = it_display_name_exc(provider);
        json_free(gen); json_free(kwargs); json_free(provider); json_free(target);
        return e;
    }
    char *dns = it_pystr(dn);
    size_t n = strlen(dns) + 48;
    char *contract = (char *)malloc(n);
    if (contract) snprintf(contract, n, "%s provider returned a non-dict result", dns);
    free(dns);
    json_t *r;
    if (!j_is_obj(gen)) {
        r = img_provider_error_str(contract ? contract : "", "provider_contract");
    } else {
        char *s = it_json_dumps(gen);
        r = json_string(s ? s : "");
        free(s);
    }
    free(contract);
    json_free(gen); json_free(kwargs); json_free(provider); json_free(target);
    return r;
}

/* PoP: img_fall_back_to_fal @ tools/image_generation_tool.py:_fall_back_to_fal */
static json_t *img_fall_back_to_fal(it_io_t *io, json_t *in) {
    const json_t *raw = json_obj_get(in, "raw");
    const char *prompt = json_get_str(in, "prompt", "");
    const char *aspect = json_get_str(in, "aspect_ratio", IT_DEFAULT_ASPECT);
    const json_t *sources = json_obj_get(in, "sources");

    if (!j_is_str(raw)) {
        char *m = (char *)malloc(96);
        if (m) snprintf(m, 96,
                        "the JSON object must be str, bytes or bytearray, not %s",
                        it_tname(raw));
        json_t *e = it_raise("TypeError", m ? m : "");
        free(m);
        return e;
    }
    json_t *result = it_json_loads(sval(raw));
    if (it_raised(result)) {
        if (it_exc_is_a(it_exc_kind(result), "ValueError")) {
            json_free(result);
            return json_copy(raw);   /* except ValueError: return raw */
        }
        return result;
    }
    if (!j_is_obj(result) || jtruth(json_obj_get(result, "success")) ||
        !jtruth(json_obj_get(result, "fallback_eligible"))) {
        json_free(result);
        return json_copy(raw);
    }
    if (jtruth(json_obj_get(sources, "image_url")) ||
        jtruth(json_obj_get(sources, "reference_image_urls"))) {
        json_free(result);
        return json_copy(raw);
    }
    json_t *fe = it_call(io, "fallback_to_fal_enabled", json_object());
    if (it_raised(fe)) { json_free(result); return fe; }
    if (!jtruth(fe)) {
        json_free(fe); json_free(result);
        return json_copy(raw);
    }
    json_free(fe);

    /* logger.warning("Managed Krea refused ... falling back to FAL: %s", result.get("error")) */
    char *ers = it_pystr(json_obj_get(result, "error"));
    size_t n = strlen(ers) + 96;
    char *m = (char *)malloc(n);
    if (m) snprintf(m, n,
                    "Managed Krea refused the request before starting a job; falling back to FAL: %s",
                    ers);
    free(ers);
    json_t *lr = it_log(io, "logger.warning", m ? m : "");
    free(m);
    if (it_raised(lr)) { json_free(result); return lr; }
    json_free(lr);

    /* image_generate_tool(prompt, aspect_ratio, upscale=sources.get("upscale"),
     *                     fal_model=DEFAULT_MODEL) -- None upscale key dropped */
    json_t *a = json_object();
    json_set_hdr(a, "prompt", prompt);
    json_set_hdr(a, "aspect_ratio", aspect);
    const json_t *up = json_obj_get(sources, "upscale");
    if (jpresent(up)) json_set(a, "upscale", json_copy(up));
    json_set_hdr(a, "fal_model", IT_DEFAULT_MODEL);
    json_t *fr = it_call(io, "image_generate_tool", a);
    if (it_raised(fr)) { json_free(result); return fr; }
    json_t *fal = it_json_loads(sval(fr));   /* domain: valid JSON object string */
    json_free(fr);
    if (it_raised(fal)) { json_free(result); return fal; }
    if (!j_is_obj(fal)) {
        char *am = it_attr_msg(fal, "get");
        json_t *e = it_raise("AttributeError", am ? am : "");
        free(am);
        json_free(fal); json_free(result);
        return e;
    }
    const char *note = jtruth(json_obj_get(fal, "success"))
        ? "Krea was unavailable, so this image was generated on FAL (" IT_DEFAULT_MODEL ") instead."
        : "Krea was unavailable and the fallback to FAL (" IT_DEFAULT_MODEL ") also failed.";
    char *notebuf = strdup(note);
    if (jtruth(json_obj_get(fal, "upscaled"))) {
        const char *suf = " The upscale ran on FAL's " IT_UPSCALER_MODEL ", not Krea Enhance.";
        char *nb = (char *)realloc(notebuf, strlen(notebuf) + strlen(suf) + 1);
        if (nb) { notebuf = nb; strcat(notebuf, suf); }
    }
    /* fal_result.update(provider=, model=, fallback_from=, fallback_reason=, note=) */
    json_set_hdr(fal, "provider", "fal");
    json_set_hdr(fal, "model", IT_DEFAULT_MODEL);
    json_set_hdr(fal, "fallback_from", "krea");
    json_set(fal, "fallback_reason", copy_or_null(json_obj_get(result, "error")));
    json_set_hdr(fal, "note", notebuf ? notebuf : "");
    free(notebuf);
    char *s = it_json_dumps(fal);
    json_free(fal);
    json_free(result);
    json_t *ret = json_string(s ? s : "");
    free(s);
    return ret;
}

/* -- dispatch ------------------------------------------------------------ */
typedef struct { const char *func; json_t *(*fn)(it_io_t *, json_t *); } it_case_t;
static const it_case_t IT_CASES[] = {
    {"_read_image_gen_key", img_read_image_gen_key},
    {"_plugin_provider_name", img_plugin_provider_name},
    {"_build_payload", img_build_payload},
    {"_build_fal_payload", img_build_fal_payload},
    {"_build_fal_edit_payload", img_build_fal_edit_payload},
    {"_format_images", img_format_images},
    {"_inline_local_sources", img_inline_local_sources},
    {"_fit_inline_budget", img_fit_inline_budget},
    {"_inline_fal_sources", img_inline_fal_sources},
    {"_shrink_data_url", img_shrink_data_url},
    {"_prepare_fal_request", img_prepare_fal_request},
    {"check_fal_api_key", img_check_fal_api_key},
    {"_get_plugin_provider", img_get_plugin_provider},
    {"_provider_error", img_provider_error},
    {"_provider_result", img_provider_result},
    {"_add_provider_kwargs", img_add_provider_kwargs},
    {"_declared_controls", img_declared_controls},
    {"_managed_model_plugin", img_managed_model_plugin},
    {"_maybe_route_managed_model", img_maybe_route_managed_model},
    {"_fall_back_to_fal", img_fall_back_to_fal},
};
#define IT_CASE_COUNT (sizeof(IT_CASES) / sizeof(IT_CASES[0]))

static json_t *it_dispatch(it_io_t *io, const char *func, json_t *in) {
    for (size_t i = 0; i < IT_CASE_COUNT; i++)
        if (strcmp(IT_CASES[i].func, func) == 0) return IT_CASES[i].fn(io, in);
    return it_raise("ValueError", "unknown func");
}

#endif /* IMAGE_TOOLS_FLOW_C */
