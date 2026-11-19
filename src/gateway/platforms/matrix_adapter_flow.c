/*
 * matrix_adapter_flow.c -- FLOW cores + leaves of
 * plugins/platforms/matrix/adapter.py (hermes-agent): the Matrix gateway
 * adapter's config/credential decision chains (scoped-secret env readers,
 * E2EE mode resolution, bang-command dispatch, outbound chunk budget),
 * reply-fallback parsing, media-filename classification, URL log redaction,
 * and the sender/identity/state helper methods.
 *
 * Decision sequences only: every live I/O / injected dependency enters
 * through ONE universal seam: ma_call(ctx, name, args). The harness wires it
 * to a recording stub fed the case's scripted results (in.io); the oracle
 * installs recording stubs emitting the identical event shape
 * {"name","args","ret"} from the SAME scripts.
 *
 * Seam name -> live call:
 *   get_scoped_secret        -> gateway.platforms._shared.get_scoped_secret(name, default)
 *   extra_or_secret          -> gateway.platforms._shared.extra_or_secret(extra,key,env,default,blank_is_unset=)
 *   is_gateway_known_command -> hermes_cli.commands.is_gateway_known_command(candidate)
 *   get_skill_commands       -> agent.skill_commands.get_skill_commands()
 *   platform_registry.get    -> gateway.platform_registry.platform_registry.get("matrix")
 *   mimetypes.guess_type     -> mimetypes.guess_type(filename)  (ret [type, encoding])
 *   urlsplit                 -> urllib.parse.urlsplit(url)      (ret {scheme,netloc,path,query,fragment})
 *   urlunsplit               -> urllib.parse.urlunsplit(parts)  (args parts=[scheme,netloc,path,query,fragment])
 *   time.monotonic           -> time.monotonic()
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and ma_-prefixed.
 *
 * Representation contract (shared with tests/oracle_matrix_adapter_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc)} object.
 * - Plain JSON numbers are ints; JSON booleans are bools (bool IS an int
 *   instance, but type() is not int). Integral floats are formatted as ints
 *   on both sides (canon); fixtures avoid integral floats in str()-computed
 *   positions and non-integral floats in _content_bytes/_matrix_event_timestamp.
 * - SendResult returns (_media_too_large) project to
 *   {"success":bool,"message_id":null,"error":str|null}.
 * - Attribute doubles (event/device_keys/config/prompt/self) are JSON objects
 *   whose keys are the attribute names; the oracle wraps them in
 *   SimpleNamespace. dict-vs-attr access reads the same key on both sides.
 * - _content_bytes uses libjson json_serialize (compact, insertion-ordered,
 *   ensure_ascii=False equivalent) + UTF-8 byte length; fixtures carry no
 *   floats so it equals json.dumps(content,ensure_ascii=False,separators=(",",":")).
 * - str.strip()/lower() fold ASCII whitespace/case only on the fixture corpus.
 * - int()/float() string parsers cover the verified domain: optional ASCII
 *   whitespace, optional sign, digits (int) / decimal+exponent (float), NO
 *   underscores / hex / inf / nan. Exact CPython 3.14 ValueError/TypeError text.
 * - Path(candidate).name/suffix model bare filenames + forward-slash paths;
 *   fixtures avoid trailing-dot / leading-dot / "." / ".." suffix edges.
 * - logger calls are silent on both sides (not traced).
 * - BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) pass through every except-Exception site.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MA_MAX_CONTENT_BYTES 45000.0
#define MA_MAX_MESSAGE_LENGTH_CEILING 15000.0
#define MA_DEFAULT_MAX_MESSAGE_LENGTH 15000.0

/* -- universal seam context -- */
typedef struct ma_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ma_ctx_t;

/* -- basic helpers -- */
static bool ma_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static bool ma_is_base_exc_kind(const char *t) {
    return t && (strcmp(t, "SystemExit") == 0 || strcmp(t, "KeyboardInterrupt") == 0
              || strcmp(t, "GeneratorExit") == 0 || strcmp(t, "CancelledError") == 0);
}

static bool ma_raise_is_base(const json_t *v) {
    return ma_is_raise(v) && ma_is_base_exc_kind(json_get_str(v, "__raise__", ""));
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool ma_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool ma_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static const char *ma_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool ma_str_eq(const json_t *v, const char *s) {
    const char *t = ma_str(v);
    return t && s && strcmp(t, s) == 0;
}

static json_t *ma_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *ma_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return ma_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). */
static json_t *ma_call(ma_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ma_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ma_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ma_make_exc(raise);
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
static char *ma_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static bool ma_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip(): ASCII whitespace both ends. Caller frees. */
static char *ma_strip(const char *s) {
    if (!s) s = "";
    while (*s && ma_is_ascii_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && ma_is_ascii_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* str.lower(): ASCII fold. Caller frees. */
static char *ma_lower(const char *s) {
    char *o = ma_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

static bool ma_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* _utf8_len: UTF-8 byte count == strlen of the C (UTF-8) string. */
static size_t ma_utf8_bytelen(const char *s) { return s ? strlen(s) : 0; }

/* str.split('\n'): "" -> [""], trailing newline keeps a final "". */
typedef struct { char **lines; size_t count; } ma_lines_t;

static ma_lines_t ma_split_nl(const char *s) {
    ma_lines_t out = {NULL, 0};
    if (!s) s = "";
    size_t cap = 8;
    out.lines = malloc(cap * sizeof(char *));
    if (!out.lines) return out;
    const char *start = s;
    for (const char *p = s; ; p++) {
        if (*p == '\n' || *p == '\0') {
            if (out.count == cap) {
                cap *= 2;
                char **nl = realloc(out.lines, cap * sizeof(char *));
                if (!nl) return out;
                out.lines = nl;
            }
            size_t len = (size_t)(p - start);
            char *line = malloc(len + 1);
            if (!line) return out;
            memcpy(line, start, len);
            line[len] = '\0';
            out.lines[out.count++] = line;
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    return out;
}

static void ma_lines_free(ma_lines_t *ls) {
    if (!ls) return;
    for (size_t i = 0; i < ls->count; i++) free(ls->lines[i]);
    free(ls->lines);
    ls->lines = NULL;
    ls->count = 0;
}

static char *ma_join_nl(char *const *lines, size_t n) {
    size_t total = 0;
    for (size_t i = 0; i < n; i++) total += strlen(lines[i]) + 1;
    char *o = malloc(total + 1);
    if (!o) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (i) o[k++] = '\n';
        size_t len = strlen(lines[i]);
        memcpy(o + k, lines[i], len);
        k += len;
    }
    o[k] = '\0';
    return o;
}

/* str() projection of a scalar JSON value (fixtures avoid containers here).
 * Integral numbers format as ints; bool -> True/False; null -> None. */
static char *ma_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ma_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ma_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return ma_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return ma_strdup(buf);
        }
        default: return ma_strdup(""); /* containers excluded from corpus */
    }
}

/* repr of a plain string for exception messages: 's' (corpus has no
 * quotes/backslashes/control chars needing escapes). Caller frees. */
static char *ma_repr_str(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 3);
    if (!o) return NULL;
    o[0] = '\'';
    memcpy(o + 1, s, n);
    o[n + 1] = '\'';
    o[n + 2] = '\0';
    return o;
}

/* -- int()/float() coercion (verified domain; exact CPython 3.14 messages) -- */
/* Parse an int string: [ws][+-]?digits[ws], >=1 digit, no underscores. */
static bool ma_parse_int_str(const char *s, long long *out) {
    if (!s) return false;
    const char *p = s;
    while (*p && ma_is_ascii_ws((unsigned char)*p)) p++;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!isdigit((unsigned char)*p)) return false;
    long long v = 0;
    while (isdigit((unsigned char)*p)) {
        v = v * 10 + (*p - '0');
        p++;
    }
    while (*p && ma_is_ascii_ws((unsigned char)*p)) p++;
    if (*p != '\0') return false;
    *out = neg ? -v : v;
    return true;
}

/* Parse a float string: [ws][+-]?(digits[.digits?]|.digits)([eE][+-]?digits)?[ws] */
static bool ma_parse_float_str(const char *s, double *out) {
    if (!s) return false;
    const char *p = s;
    while (*p && ma_is_ascii_ws((unsigned char)*p)) p++;
    const char *start = p;
    if (*p == '+' || *p == '-') p++;
    size_t int_digits = 0, frac_digits = 0;
    while (isdigit((unsigned char)*p)) { p++; int_digits++; }
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) { p++; frac_digits++; }
    }
    if (int_digits == 0 && frac_digits == 0) return false;
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!isdigit((unsigned char)*p)) return false;
        while (isdigit((unsigned char)*p)) p++;
    }
    const char *num_end = p; /* span ends BEFORE the trailing-ws skip below */
    while (*p && ma_is_ascii_ws((unsigned char)*p)) p++;
    if (*p != '\0') return false;
    /* shape validated; strtod on the cleaned span (sign included, no ws) */
    size_t n = (size_t)(num_end - start);
    char *clean = malloc(n + 1);
    if (!clean) return false;
    memcpy(clean, start, n);
    clean[n] = '\0';
    char *endp = NULL;
    double d = strtod(clean, &endp);
    bool ok = endp && *endp == '\0';
    free(clean);
    if (!ok) return false;
    *out = d;
    return true;
}

static const char *ma_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* int(v) */
static json_t *ma_int_of(const json_t *v) {
    if (ma_is_null(v))
        return ma_err("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) return json_number((double)(long long)v->num_val);
    if (v->type == JSON_STRING) {
        long long out;
        if (ma_parse_int_str(v->str_val, &out)) return json_number((double)out);
        char *r = ma_repr_str(v->str_val);
        char msg[512];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", r ? r : "''");
        free(r);
        return ma_err("ValueError", msg);
    }
    char msg[256];
    snprintf(msg, sizeof msg,
        "int() argument must be a string, a bytes-like object or a real number, not '%s'",
        ma_type_name(v));
    return ma_err("TypeError", msg);
}

/* float(v) */
static json_t *ma_float_of(const json_t *v) {
    if (ma_is_null(v))
        return ma_err("TypeError",
            "float() argument must be a string or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) return json_number(v->num_val);
    if (v->type == JSON_STRING) {
        double out;
        if (ma_parse_float_str(v->str_val, &out)) return json_number(out);
        char *r = ma_repr_str(v->str_val);
        char msg[512];
        snprintf(msg, sizeof msg, "could not convert string to float: %s", r ? r : "''");
        free(r);
        return ma_err("ValueError", msg);
    }
    char msg[256];
    snprintf(msg, sizeof msg, "float() argument must be a string or a real number, not '%s'",
        ma_type_name(v));
    return ma_err("TypeError", msg);
}
/* -- small shared utilities -- */
static char *ma_strndup(const char *s, size_t n) {
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static bool ma_is_float_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__float__", false);
}

/* part of s before the first occurrence of ch (whole s if absent). Caller frees. */
static char *ma_before_first(const char *s, char ch) {
    if (!s) s = "";
    const char *p = strchr(s, ch);
    return p ? ma_strndup(s, (size_t)(p - s)) : ma_strdup(s);
}

typedef struct { char **v; size_t n, cap; } ma_strvec_t;
static void ma_sv_push(ma_strvec_t *sv, char *s) { /* adopts s */
    if (sv->n == sv->cap) {
        sv->cap = sv->cap ? sv->cap * 2 : 8;
        sv->v = realloc(sv->v, sv->cap * sizeof(char *));
    }
    sv->v[sv->n++] = s;
}
static int ma_sv_cmp(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}
/* sort + dedup the vector, build a JSON string array, free the vector. */
static json_t *ma_sv_to_sorted_set(ma_strvec_t *sv) {
    if (sv->n > 1) qsort(sv->v, sv->n, sizeof(char *), ma_sv_cmp);
    json_t *arr = json_array();
    for (size_t i = 0; i < sv->n; i++) {
        if (i == 0 || strcmp(sv->v[i], sv->v[i - 1]) != 0)
            json_append(arr, json_string(sv->v[i]));
        free(sv->v[i]);
    }
    free(sv->v);
    return arr;
}

/* Path(name).suffix, lowercased, on the restricted bare-filename corpus. */
static char *ma_path_suffix_lower(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name || dot[1] == '\0') return ma_strdup("");
    return ma_lower(dot);
}

/* -- module leaves -- */

/* PoP: ma_utf8_len @ plugins/platforms/matrix/adapter.py:_utf8_len */
static json_t *ma_utf8_len(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = json_get_str(args, "text", "");
    return json_number((double)ma_utf8_bytelen(text));
}

/* PoP: ma_content_bytes @ plugins/platforms/matrix/adapter.py:_content_bytes */
static json_t *ma_content_bytes(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *content = json_obj_get(args, "content");
    char *s = json_serialize(content); /* compact, insertion-ordered, raw UTF-8 */
    size_t n = s ? strlen(s) : 0;
    free(s);
    return json_number((double)n);
}

/* PoP: ma_csv_set @ plugins/platforms/matrix/adapter.py:_csv_set */
static json_t *ma_csv_set(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *raw = json_obj_get(args, "raw");
    ma_strvec_t sv = {NULL, 0, 0};
    if (raw && raw->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(raw); i++) {
            char *s = ma_py_str(json_get(raw, i));
            char *t = ma_strip(s);
            free(s);
            if (t && t[0]) ma_sv_push(&sv, t); else free(t);
        }
    } else {
        char *s = ma_py_str(raw);
        /* split on ',' */
        const char *start = s;
        for (const char *p = s; ; p++) {
            if (*p == ',' || *p == '\0') {
                char *tok = ma_strndup(start, (size_t)(p - start));
                char *t = ma_strip(tok);
                free(tok);
                if (t && t[0]) ma_sv_push(&sv, t); else free(t);
                if (*p == '\0') break;
                start = p + 1;
            }
        }
        free(s);
    }
    return ma_sv_to_sorted_set(&sv);
}

/* PoP: ma_thread_root @ plugins/platforms/matrix/adapter.py:_thread_root */
static json_t *ma_thread_root(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *relates_to = json_obj_get(args, "relates_to");
    const json_t *rel_type = relates_to ? json_obj_get(relates_to, "rel_type") : NULL;
    if (ma_str_eq(rel_type, "m.thread")) {
        const json_t *eid = json_obj_get(relates_to, "event_id");
        return eid ? json_copy(eid) : json_null();
    }
    return json_null();
}

/* PoP: ma_normalize_e2ee_mode @ plugins/platforms/matrix/adapter.py:_normalize_e2ee_mode */
static json_t *ma_normalize_e2ee_mode(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *value = json_obj_get(args, "value");
    char *s = ma_truthy(value) ? ma_py_str(value) : ma_strdup("");
    char *t = ma_strip(s);
    free(s);
    char *raw = ma_lower(t);
    free(t);
    const char *required[] = {"required", "require", "true", "1", "yes", "on"};
    const char *optional[] = {"optional", "prefer", "preferred"};
    const char *out = "off";
    for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); i++)
        if (strcmp(raw, required[i]) == 0) { out = "required"; break; }
    if (strcmp(out, "off") == 0)
        for (size_t i = 0; i < sizeof(optional) / sizeof(optional[0]); i++)
            if (strcmp(raw, optional[i]) == 0) { out = "optional"; break; }
    free(raw);
    return json_string(out);
}

/* Reply-fallback pill: ^>\s*<(@[^>]+)>\s*(.*)$  (ASCII \s on the corpus). */
static bool ma_pill_match(const char *line, char **author, char **content) {
    const char *p = line;
    if (*p != '>') return false;
    p++;
    while (*p && ma_is_ascii_ws((unsigned char)*p)) p++;
    if (*p != '<') return false;
    p++;
    if (*p != '@') return false;
    const char *g1 = p; /* includes the '@' */
    p++;
    if (*p == '>' || *p == '\0') return false; /* [^>]+ needs >=1 char */
    while (*p && *p != '>') p++;
    if (*p != '>') return false;
    size_t g1len = (size_t)(p - g1);
    p++;
    while (*p && ma_is_ascii_ws((unsigned char)*p)) p++;
    *author = ma_strndup(g1, g1len);
    *content = ma_strdup(p);
    return true;
}

/* PoP: ma_extract_reply_fallback @ plugins/platforms/matrix/adapter.py:_extract_reply_fallback */
static json_t *ma_extract_reply_fallback(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *body = json_get_str(args, "body", "");
    json_t *out = json_array();
    if (!body || !ma_starts_with(body, "> ")) {
        json_append(out, json_null());
        json_append(out, json_null());
        return out;
    }
    ma_lines_t ls = ma_split_nl(body);
    ma_strvec_t quoted = {NULL, 0, 0};
    char *author = NULL;
    for (size_t i = 0; i < ls.count; i++) {
        const char *line = ls.lines[i];
        if (!ma_starts_with(line, "> ")) break;
        char *content = ma_strdup(line + 2);
        if (!author) {
            char *a = NULL, *c = NULL;
            if (ma_pill_match(line, &a, &c)) {
                author = a;
                free(content);
                content = c;
            }
        }
        ma_sv_push(&quoted, content);
    }
    char *joined = ma_join_nl(quoted.v, quoted.n);
    for (size_t i = 0; i < quoted.n; i++) free(quoted.v[i]);
    free(quoted.v);
    char *stripped = ma_strip(joined);
    free(joined);
    json_append(out, (stripped && stripped[0]) ? json_string(stripped) : json_null());
    json_append(out, author ? json_string(author) : json_null());
    free(stripped);
    free(author);
    ma_lines_free(&ls);
    return out;
}

/* PoP: ma_strip_reply_fallback @ plugins/platforms/matrix/adapter.py:_strip_reply_fallback */
static json_t *ma_strip_reply_fallback(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *body = json_get_str(args, "body", "");
    if (!body || !ma_starts_with(body, "> ")) return json_string(body ? body : "");
    ma_lines_t ls = ma_split_nl(body);
    ma_strvec_t stripped = {NULL, 0, 0};
    bool past_fallback = false;
    for (size_t i = 0; i < ls.count; i++) {
        const char *line = ls.lines[i];
        if (!past_fallback) {
            if (ma_starts_with(line, "> ") || strcmp(line, ">") == 0) continue;
            past_fallback = true;
            if (line[0] == '\0') continue;
        }
        ma_sv_push(&stripped, ma_strdup(line));
    }
    json_t *r;
    if (stripped.n == 0) {
        r = json_string(body);
    } else {
        char *joined = ma_join_nl(stripped.v, stripped.n);
        r = json_string(joined ? joined : "");
        free(joined);
    }
    for (size_t i = 0; i < stripped.n; i++) free(stripped.v[i]);
    free(stripped.v);
    ma_lines_free(&ls);
    return r;
}

/* PoP: ma_split_reply_fallback @ plugins/platforms/matrix/adapter.py:_split_reply_fallback */
static json_t *ma_split_reply_fallback(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *body = json_get_str(args, "body", "");
    json_t *out = json_array();
    if (!body || !ma_starts_with(body, "> ")) {
        json_append(out, json_string(""));
        json_append(out, json_string(body ? body : ""));
        return out;
    }
    ma_lines_t ls = ma_split_nl(body);
    size_t idx = 0;
    while (idx < ls.count && (ma_starts_with(ls.lines[idx], "> ") || strcmp(ls.lines[idx], ">") == 0))
        idx++;
    if (idx < ls.count && ls.lines[idx][0] == '\0') idx++;
    char *head = ma_join_nl(ls.lines, idx);
    if (idx >= ls.count) {
        json_append(out, json_string(head ? head : ""));
        json_append(out, json_string(""));
    } else {
        size_t hlen = head ? strlen(head) : 0;
        char *head_nl = malloc(hlen + 2);
        if (head_nl) { memcpy(head_nl, head ? head : "", hlen); head_nl[hlen] = '\n'; head_nl[hlen + 1] = '\0'; }
        char *tail = ma_join_nl(ls.lines + idx, ls.count - idx);
        json_append(out, json_string(head_nl ? head_nl : ""));
        json_append(out, json_string(tail ? tail : ""));
        free(head_nl);
        free(tail);
    }
    free(head);
    ma_lines_free(&ls);
    return out;
}

/* PoP: ma_is_permanent_matrix_auth_error @ plugins/platforms/matrix/adapter.py:_is_permanent_matrix_auth_error */
static json_t *ma_is_permanent_matrix_auth_error(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *exc = json_obj_get(args, "exc");
    const json_t *errcode = exc ? json_obj_get(exc, "errcode") : NULL;
    if (errcode && errcode->type == JSON_STRING) {
        char *t = ma_strip(errcode->str_val);
        char *low = ma_lower(t);
        free(t);
        bool perm = strcmp(low, "m_unknown_token") == 0 || strcmp(low, "m_missing_token") == 0
                 || strcmp(low, "m_forbidden") == 0;
        free(low);
        if (perm) return json_bool(true);
    }
    const json_t *status = exc ? json_obj_get(exc, "http_status") : NULL;
    /* isinstance(status, int): plain JSON number (bool is an int instance but
     * can never equal 401/403); a __float__-tagged value is a float -> reject. */
    if (status && status->type == JSON_NUMBER && !ma_is_float_tag(status)) {
        if (status->num_val == 401.0 || status->num_val == 403.0) return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: ma_matrix_event_timestamp_seconds @ plugins/platforms/matrix/adapter.py:_matrix_event_timestamp_seconds */
static json_t *ma_matrix_event_timestamp_seconds(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *event = json_obj_get(args, "event");
    const json_t *ts_node = event ? json_obj_get(event, "timestamp") : NULL;
    if (!ma_truthy(ts_node)) ts_node = event ? json_obj_get(event, "server_timestamp") : NULL;
    json_t *fallback = NULL;
    if (!ma_truthy(ts_node)) { fallback = json_number(0); ts_node = fallback; }
    json_t *f = ma_float_of(ts_node);
    json_free(fallback);
    if (ma_is_raise(f)) {
        const char *kind = json_get_str(f, "__raise__", "");
        if (strcmp(kind, "TypeError") == 0 || strcmp(kind, "ValueError") == 0) {
            json_free(f);
            return json_number(0);
        }
        return f; /* BaseException passthrough */
    }
    double ts = f->num_val;
    json_free(f);
    return json_number(ts > 10000000000.0 ? ts / 1000.0 : ts);
}

/* PoP: ma_redact_url_for_log @ plugins/platforms/matrix/adapter.py:_redact_url_for_log */
static json_t *ma_redact_url_for_log(ma_ctx_t *ctx, const json_t *args) {
    const json_t *url_node = json_obj_get(args, "url");
    char *url = ma_py_str(url_node);
    json_t *a = json_object();
    json_set_hdr(a, "url", url);
    json_t *parts = ma_call(ctx, "urlsplit", a);
    if (ma_is_raise(parts)) {
        if (ma_raise_is_base(parts)) { free(url); return parts; }
        json_free(parts); free(url);
        return json_string("<url>");
    }
    const json_t *scheme = json_obj_get(parts, "scheme");
    const json_t *netloc = json_obj_get(parts, "netloc");
    const json_t *path = json_obj_get(parts, "path");
    json_t *r;
    if (!ma_truthy(scheme) && !ma_truthy(netloc)) {
        char *q = ma_before_first(url, '?');
        char *h = ma_before_first(q, '#');
        r = json_string(h ? h : "");
        free(q); free(h);
    } else {
        json_t *pa = json_array();
        json_append(pa, scheme ? json_copy(scheme) : json_string(""));
        json_append(pa, netloc ? json_copy(netloc) : json_string(""));
        json_append(pa, path ? json_copy(path) : json_string(""));
        json_append(pa, json_string(""));
        json_append(pa, json_string(""));
        json_t *a2 = json_object();
        json_set(a2, "parts", pa);
        r = ma_call(ctx, "urlunsplit", a2);
        if (ma_is_raise(r)) {
            if (ma_raise_is_base(r)) { json_free(parts); free(url); return r; }
            json_free(r);
            r = json_string("<url>");
        }
    }
    json_free(parts);
    free(url);
    return r;
}

/* -- media filename classification -- */
static const char *MA_IMAGE_EXTS[] = {
    ".jpg", ".jpeg", ".png", ".gif", ".webp", ".bmp", ".svg", ".heic", ".heif", ".avif"};
static const char *MA_MEDIA_EXTS[] = {
    ".ogg", ".oga", ".opus", ".m4a", ".mp3", ".wav", ".flac", ".aac", ".amr",
    ".mp4", ".webm", ".mov", ".mkv"};

static json_t *ma_str_array(const char *const *items, size_t n) {
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(items[i]));
    return a;
}

static bool ma_str_in_array(const char *s, const json_t *arr) {
    if (!s || !arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (ma_str_eq(json_get(arr, i), s)) return true;
    return false;
}

/* core of _looks_like_transport_filename; prefixes/exts are JSON string arrays. */
static json_t *ma_transport_core(ma_ctx_t *ctx, const json_t *text,
                                 const json_t *prefixes, const json_t *exts,
                                 bool reject_spaces) {
    char *s = ma_truthy(text) ? ma_py_str(text) : ma_strdup("");
    char *candidate = ma_strip(s);
    free(s);
    bool no = false;
    if (!candidate || !candidate[0] || strchr(candidate, '\n')) no = true;
    if (!no && candidate[strlen(candidate) - 1] == '/') no = true;
    if (!no && reject_spaces) {
        for (const char *p = candidate; *p; p++)
            if (ma_is_ascii_ws((unsigned char)*p)) { no = true; break; }
    }
    if (!no && (strchr(candidate, '/') || strchr(candidate, '\\') || strcmp(candidate, ".") == 0))
        no = true;
    char *suffix = NULL;
    if (!no) {
        suffix = ma_path_suffix_lower(candidate);
        if (!suffix[0]) no = true;
    }
    bool result = false;
    if (!no) {
        json_t *a = json_object();
        json_set_hdr(a, "filename", candidate);
        json_t *gt = ma_call(ctx, "mimetypes.guess_type", a);
        if (ma_is_raise(gt)) {
            free(candidate); free(suffix);
            return gt; /* no try/except in live: propagate (incl. BaseException) */
        }
        const json_t *guessed = json_get(gt, 0);
        const char *gstr = ma_str(guessed);
        bool mime_match = false;
        if (gstr && gstr[0] && prefixes && prefixes->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(prefixes); i++) {
                const char *pref = ma_str(json_get(prefixes, i));
                if (pref && ma_starts_with(gstr, pref)) { mime_match = true; break; }
            }
        }
        result = mime_match || ma_str_in_array(suffix, exts);
        json_free(gt);
    }
    free(candidate);
    free(suffix);
    return json_bool(result);
}

/* PoP: ma_looks_like_transport_filename @ plugins/platforms/matrix/adapter.py:_looks_like_transport_filename */
static json_t *ma_looks_like_transport_filename(ma_ctx_t *ctx, const json_t *args) {
    const json_t *text = json_obj_get(args, "text");
    const json_t *prefixes = json_obj_get(args, "mime_prefixes");
    const json_t *exts = json_obj_get(args, "exts");
    bool reject_spaces = json_get_bool(args, "reject_spaces", false);
    return ma_transport_core(ctx, text, prefixes, exts, reject_spaces);
}

/* PoP: ma_looks_like_matrix_image_filename @ plugins/platforms/matrix/adapter.py:_looks_like_matrix_image_filename */
static json_t *ma_looks_like_matrix_image_filename(ma_ctx_t *ctx, const json_t *args) {
    const json_t *text = json_obj_get(args, "text");
    json_t *prefixes = ma_str_array((const char *const[]){ "image/" }, 1);
    json_t *exts = ma_str_array(MA_IMAGE_EXTS, sizeof(MA_IMAGE_EXTS) / sizeof(MA_IMAGE_EXTS[0]));
    json_t *r = ma_transport_core(ctx, text, prefixes, exts, false);
    json_free(prefixes); json_free(exts);
    return r;
}

/* PoP: ma_looks_like_matrix_media_filename @ plugins/platforms/matrix/adapter.py:_looks_like_matrix_media_filename */
static json_t *ma_looks_like_matrix_media_filename(ma_ctx_t *ctx, const json_t *args) {
    const json_t *text = json_obj_get(args, "text");
    json_t *prefixes = ma_str_array((const char *const[]){ "audio/", "video/" }, 2);
    json_t *exts = ma_str_array(MA_MEDIA_EXTS, sizeof(MA_MEDIA_EXTS) / sizeof(MA_MEDIA_EXTS[0]));
    json_t *r = ma_transport_core(ctx, text, prefixes, exts, true);
    json_free(prefixes); json_free(exts);
    return r;
}

/* PoP: ma_is_bare_media_filename @ plugins/platforms/matrix/adapter.py:_is_bare_media_filename */
static json_t *ma_is_bare_media_filename(ma_ctx_t *ctx, const json_t *args) {
    const char *msgtype = json_get_str(args, "msgtype", "");
    const json_t *body = json_obj_get(args, "body");
    if (strcmp(msgtype, "m.image") == 0) {
        json_t *prefixes = ma_str_array((const char *const[]){ "image/" }, 1);
        json_t *exts = ma_str_array(MA_IMAGE_EXTS, sizeof(MA_IMAGE_EXTS) / sizeof(MA_IMAGE_EXTS[0]));
        json_t *r = ma_transport_core(ctx, body, prefixes, exts, false);
        json_free(prefixes); json_free(exts);
        return r;
    }
    if (strcmp(msgtype, "m.audio") == 0 || strcmp(msgtype, "m.file") == 0 || strcmp(msgtype, "m.video") == 0) {
        json_t *prefixes = ma_str_array((const char *const[]){ "audio/", "video/" }, 2);
        json_t *exts = ma_str_array(MA_MEDIA_EXTS, sizeof(MA_MEDIA_EXTS) / sizeof(MA_MEDIA_EXTS[0]));
        json_t *r = ma_transport_core(ctx, body, prefixes, exts, true);
        json_free(prefixes); json_free(exts);
        return r;
    }
    return json_bool(false);
}
/* -- scoped-secret / config seam flows -- */

/* PoP: ma_env_truthy @ plugins/platforms/matrix/adapter.py:_env_truthy */
static json_t *ma_env_truthy(ma_ctx_t *ctx, const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    const char *def = json_get_str(args, "default", "");
    json_t *a = json_object();
    json_set_hdr(a, "name", name);
    json_set_hdr(a, "default", def);
    json_t *r = ma_call(ctx, "get_scoped_secret", a);
    if (ma_is_raise(r)) return r; /* no try/except in live: propagate */
    char *s = ma_py_str(r);
    char *low = ma_lower(s);
    free(s);
    bool t = strcmp(low, "true") == 0 || strcmp(low, "1") == 0 || strcmp(low, "yes") == 0;
    free(low);
    json_free(r);
    return json_bool(t);
}

/* PoP: ma_env_number @ plugins/platforms/matrix/adapter.py:_env_number */
static json_t *ma_env_number(ma_ctx_t *ctx, const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    const json_t *def_node = json_obj_get(args, "default");
    const char *cast = json_get_str(args, "cast", "float");
    char *dstr = ma_py_str(def_node);
    json_t *a = json_object();
    json_set_hdr(a, "name", name);
    json_set_hdr(a, "default", dstr);
    free(dstr);
    json_t *r = ma_call(ctx, "get_scoped_secret", a);
    if (ma_is_raise(r)) {
        if (strcmp(json_get_str(r, "__raise__", ""), "ValueError") == 0) {
            json_free(r);
            return def_node ? json_copy(def_node) : json_null();
        }
        return r; /* only ValueError is caught */
    }
    json_t *casted = (strcmp(cast, "int") == 0) ? ma_int_of(r) : ma_float_of(r);
    json_free(r);
    if (ma_is_raise(casted)) {
        if (strcmp(json_get_str(casted, "__raise__", ""), "ValueError") == 0) {
            json_free(casted);
            return def_node ? json_copy(def_node) : json_null();
        }
        return casted;
    }
    return casted;
}

/* PoP: ma_scoped_recovery_key @ plugins/platforms/matrix/adapter.py:_scoped_recovery_key */
static json_t *ma_scoped_recovery_key(ma_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "name", "MATRIX_RECOVERY_KEY");
    json_set_hdr(a, "default", "");
    json_t *r = ma_call(ctx, "get_scoped_secret", a);
    if (ma_is_raise(r)) return r;
    if (ma_is_null(r)) {
        json_free(r);
        return ma_err("AttributeError", "'NoneType' object has no attribute 'strip'");
    }
    if (r->type != JSON_STRING) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", ma_type_name(r));
        json_free(r);
        return ma_err("AttributeError", msg);
    }
    char *s = ma_strip(r->str_val);
    json_free(r);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

/* PoP: ma_extra_csv_set @ plugins/platforms/matrix/adapter.py:_extra_csv_set */
static json_t *ma_extra_csv_set(ma_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    const char *key = json_get_str(args, "key", "");
    const char *env = json_get_str(args, "env_name", "");
    json_t *a = json_object();
    json_set(a, "extra", extra ? json_copy(extra) : json_null());
    json_set_hdr(a, "key", key);
    json_set_hdr(a, "env", env);
    json_set_hdr(a, "default", "");
    json_set(a, "blank_is_unset", json_bool(false));
    json_t *r = ma_call(ctx, "extra_or_secret", a);
    if (ma_is_raise(r)) return r;
    json_t *ca = json_object();
    json_set(ca, "raw", r); /* adopts r */
    json_t *out = ma_csv_set(ctx, ca);
    json_free(ca);
    return out;
}

/* PoP: ma_resolve_e2ee_mode @ plugins/platforms/matrix/adapter.py:_resolve_e2ee_mode */
static json_t *ma_resolve_e2ee_mode(ma_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    if (ma_is_null(extra)) extra = NULL; /* extra or {} */
    const json_t *e2ee_mode = extra ? json_obj_get(extra, "e2ee_mode") : NULL;
    json_t *explicit;
    bool explicit_owned = false;
    if (ma_truthy(e2ee_mode)) {
        explicit = (json_t *)e2ee_mode; /* borrowed; only copied, never freed/returned */
    } else {
        json_t *a = json_object();
        json_set_hdr(a, "name", "MATRIX_E2EE_MODE");
        json_set_hdr(a, "default", "");
        explicit = ma_call(ctx, "get_scoped_secret", a);
        explicit_owned = true;
        if (ma_is_raise(explicit)) return explicit;
    }
    if (ma_truthy(explicit)) {
        json_t *na = json_object();
        json_set(na, "value", json_copy(explicit));
        json_t *norm = ma_normalize_e2ee_mode(ctx, na);
        json_free(na);
        if (explicit_owned) json_free((json_t *)explicit);
        return norm;
    }
    if (explicit_owned) json_free((json_t *)explicit);
    /* legacy_enabled = extra.get("encryption", _env_truthy("MATRIX_ENCRYPTION"))
     * -- the _env_truthy default arg is evaluated EAGERLY, even when the
     * "encryption" key is present. */
    json_t *ea = json_object();
    json_set_hdr(ea, "name", "MATRIX_ENCRYPTION");
    json_set_hdr(ea, "default", "");
    json_t *env_t = ma_env_truthy(ctx, ea);
    json_free(ea);
    if (ma_is_raise(env_t)) return env_t;
    const json_t *legacy;
    if (extra && json_has(extra, "encryption")) legacy = json_obj_get(extra, "encryption");
    else legacy = env_t;
    bool on = ma_truthy(legacy);
    json_free(env_t);
    return json_string(on ? "required" : "off");
}

/* PoP: ma_resolve_matrix_bang_command @ plugins/platforms/matrix/adapter.py:_resolve_matrix_bang_command */
static json_t *ma_resolve_matrix_bang_command(ma_ctx_t *ctx, const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    if (!name || !name[0]) return json_null();
    char *low = ma_lower(name);
    char *low_dash = ma_strdup(low);
    for (char *p = low_dash; *p; p++) if (*p == '_') *p = '-';
    const char *cands[2];
    size_t ncand = 0;
    cands[ncand++] = low;
    if (strcmp(low, low_dash) != 0) cands[ncand++] = low_dash;
    json_t *result = NULL;
    /* first try: is_gateway_known_command per candidate */
    bool first_aborted = false;
    for (size_t i = 0; i < ncand && !first_aborted; i++) {
        json_t *a = json_object();
        json_set_hdr(a, "candidate", cands[i]);
        json_t *r = ma_call(ctx, "is_gateway_known_command", a);
        if (ma_is_raise(r)) {
            if (ma_raise_is_base(r)) { result = r; goto done; }
            json_free(r);
            first_aborted = true; /* except Exception: leave the whole try */
            break;
        }
        bool known = ma_truthy(r);
        json_free(r);
        if (known) { result = json_string(cands[i]); goto done; }
    }
    /* second try: get_skill_commands */
    {
        json_t *r = ma_call(ctx, "get_skill_commands", json_object());
        if (ma_is_raise(r)) {
            if (ma_raise_is_base(r)) { result = r; goto done; }
            json_free(r);
            result = json_null();
            goto done;
        }
        json_t *skills = ma_truthy(r) ? r : NULL;
        if (!skills) { json_free(r); skills = json_object(); }
        for (size_t i = 0; i < ncand; i++) {
            char key[512];
            snprintf(key, sizeof key, "/%s", cands[i]);
            if (skills->type == JSON_OBJECT && json_has(skills, key)) {
                result = json_string(cands[i]);
                json_free(skills);
                goto done;
            }
        }
        json_free(skills);
        result = json_null();
    }
done:
    free(low);
    free(low_dash);
    return result ? result : json_null();
}

/* PoP: ma_normalize_matrix_bang_command @ plugins/platforms/matrix/adapter.py:_normalize_matrix_bang_command */
static json_t *ma_normalize_matrix_bang_command(ma_ctx_t *ctx, const json_t *args) {
    const char *text = json_get_str(args, "text", "");
    if (!text || !text[0] || text[0] != '!') return json_string(text ? text : "");
    /* ^!([A-Za-z][A-Za-z0-9_-]*)(?=$|\s)(.*)$  (DOTALL; ASCII \s corpus) */
    const char *p = text + 1;
    bool is_letter = (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z');
    if (!is_letter) return json_string(text);
    const char *g1 = p;
    while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')
        || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-') p++;
    if (*p != '\0' && !ma_is_ascii_ws((unsigned char)*p)) return json_string(text);
    char *group1 = ma_strndup(g1, (size_t)(p - g1));
    const char *group2 = p; /* rest of the text (may be "") */
    json_t *ra = json_object();
    json_set_hdr(ra, "name", group1);
    json_t *resolved = ma_resolve_matrix_bang_command(ctx, ra);
    json_free(ra);
    free(group1);
    if (ma_is_raise(resolved)) return resolved; /* BaseException propagates */
    if (ma_is_null(resolved)) {
        json_free(resolved);
        return json_string(text);
    }
    const char *rs = ma_str(resolved);
    size_t len = 1 + (rs ? strlen(rs) : 0) + strlen(group2) + 1;
    char *o = malloc(len);
    if (o) snprintf(o, len, "/%s%s", rs ? rs : "", group2);
    json_free(resolved);
    json_t *out = json_string(o ? o : "");
    free(o);
    return out;
}

/* PoP: ma_resolve_max_message_length @ plugins/platforms/matrix/adapter.py:_resolve_max_message_length */
static json_t *ma_resolve_max_message_length(ma_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    json_t *a = json_object();
    json_set(a, "extra", extra ? json_copy(extra) : json_null());
    json_set_hdr(a, "key", "max_message_length");
    json_set_hdr(a, "env", "MATRIX_MAX_MESSAGE_LENGTH");
    json_set(a, "default", json_null());
    json_set(a, "blank_is_unset", json_bool(true)); /* live default (not passed) */
    json_t *raw = ma_call(ctx, "extra_or_secret", a);
    if (ma_is_raise(raw)) return raw; /* not inside a try */
    bool need_registry = ma_is_null(raw);
    if (!need_registry) {
        char *s = ma_py_str(raw);
        char *t = ma_strip(s);
        need_registry = !(t && t[0]);
        free(s); free(t);
    }
    if (need_registry) {
        json_t *a2 = json_object();
        json_set_hdr(a2, "name", "matrix");
        json_t *entry = ma_call(ctx, "platform_registry.get", a2);
        if (ma_is_raise(entry)) {
            if (ma_raise_is_base(entry)) { json_free(raw); return entry; }
            json_free(entry); /* suppress(Exception): raw unchanged */
        } else {
            if (ma_truthy(entry)) {
                const json_t *mml = json_obj_get(entry, "max_message_length");
                if (ma_truthy(mml)) {
                    json_free(raw);
                    raw = json_copy(mml);
                }
            }
            json_free(entry);
        }
    }
    json_t *iv = ma_int_of(raw);
    json_free(raw);
    if (ma_is_raise(iv)) {
        const char *kind = json_get_str(iv, "__raise__", "");
        if (strcmp(kind, "TypeError") == 0 || strcmp(kind, "ValueError") == 0) {
            json_free(iv);
            return json_number(MA_DEFAULT_MAX_MESSAGE_LENGTH);
        }
        return iv; /* BaseException passthrough */
    }
    double value = iv->num_val;
    json_free(iv);
    double clamped = value < MA_MAX_MESSAGE_LENGTH_CEILING ? value : MA_MAX_MESSAGE_LENGTH_CEILING;
    if (clamped < 500.0) clamped = 500.0;
    return json_number(clamped);
}
/* -- adapter method leaves / state flows --
 * self attributes ride in args.self (a JSON object); the oracle wraps them in
 * a SimpleNamespace double. Mutable state rides in ctx.state (in.state). */
static const json_t *ma_self_attr(const json_t *args, const char *name) {
    const json_t *slf = json_obj_get(args, "self");
    return slf ? json_obj_get(slf, name) : NULL;
}

/* PoP: ma_is_self_sender @ plugins/platforms/matrix/adapter.py:_is_self_sender */
static json_t *ma_is_self_sender(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *uid = ma_self_attr(args, "_user_id");
    char *us = ma_truthy(uid) ? ma_py_str(uid) : ma_strdup("");
    char *ut = ma_strip(us); free(us);
    char *own = ma_lower(ut); free(ut);
    const char *sender = json_get_str(args, "sender", "");
    char *st = ma_strip(sender);
    char *sl = ma_lower(st); free(st);
    bool r = !own[0] || strcmp(sl, own) == 0;
    free(own); free(sl);
    return json_bool(r);
}

/* PoP: ma_is_system_or_bridge_sender @ plugins/platforms/matrix/adapter.py:_is_system_or_bridge_sender */
static json_t *ma_is_system_or_bridge_sender(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *sender = json_obj_get(args, "sender");
    char *s = ma_truthy(sender) ? ma_py_str(sender) : ma_strdup("");
    char *t = ma_strip(s); free(s);
    const char *p = t;
    while (*p == '@') p++; /* lstrip('@') */
    const char *colon = strchr(p, ':');
    char *localpart = colon ? ma_strndup(p, (size_t)(colon - p)) : ma_strdup(p);
    bool r = !localpart[0] || localpart[0] == '_';
    free(localpart); free(t);
    return json_bool(r);
}

/* PoP: ma_user_localpart @ plugins/platforms/matrix/adapter.py:_user_localpart */
static json_t *ma_user_localpart(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *uid = ma_self_attr(args, "_user_id");
    const char *us = ma_str(uid);
    if (!ma_truthy(uid) || !us || !strchr(us, ':')) return json_string("");
    const char *colon = strchr(us, ':');
    char *part = ma_strndup(us, (size_t)(colon - us));
    const char *p = part;
    while (*p == '@') p++;
    json_t *r = json_string(p);
    free(part);
    return r;
}

/* PoP: ma_mxc_to_http @ plugins/platforms/matrix/adapter.py:_mxc_to_http */
static json_t *ma_mxc_to_http(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *u = json_get_str(args, "mxc_url", "");
    if (!ma_starts_with(u, "mxc://")) return json_string(u);
    const char *hs = ma_str(ma_self_attr(args, "_homeserver"));
    if (!hs) hs = "";
    const char *rest = u + 6;
    size_t len = strlen(hs) + strlen("/_matrix/client/v1/media/download/") + strlen(rest) + 1;
    char *o = malloc(len);
    if (o) snprintf(o, len, "%s/_matrix/client/v1/media/download/%s", hs, rest);
    json_t *r = json_string(o ? o : "");
    free(o);
    return r;
}

/* PoP: ma_sanitize_link_url @ plugins/platforms/matrix/adapter.py:_sanitize_link_url */
static json_t *ma_sanitize_link_url(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *url = json_get_str(args, "url", "");
    char *stripped = ma_strip(url);
    const char *colon = strchr(stripped, ':');
    if (colon) {
        char *scheme = ma_strndup(stripped, (size_t)(colon - stripped));
        char *low = ma_lower(scheme); free(scheme);
        char *t = ma_strip(low); free(low);
        bool bad = strcmp(t, "javascript") == 0 || strcmp(t, "data") == 0 || strcmp(t, "vbscript") == 0;
        free(t);
        if (bad) { free(stripped); return json_string(""); }
    }
    /* replace '"' with "&quot;" */
    size_t quotes = 0;
    for (const char *p = stripped; *p; p++) if (*p == '"') quotes++;
    size_t len = strlen(stripped) + quotes * 5 + 1; /* &quot; is 6 chars, +5 per quote */
    char *o = malloc(len);
    if (o) {
        size_t k = 0;
        for (const char *p = stripped; *p; p++) {
            if (*p == '"') { memcpy(o + k, "&quot;", 6); k += 6; }
            else o[k++] = *p;
        }
        o[k] = '\0';
    }
    free(stripped);
    json_t *r = json_string(o ? o : "");
    free(o);
    return r;
}

/* PoP: ma_state_event_value @ plugins/platforms/matrix/adapter.py:_state_event_value */
static json_t *ma_state_event_value(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *event = json_obj_get(args, "event");
    const char *key = json_get_str(args, "key", "");
    if (ma_is_null(event)) return json_null();
    const json_t *value = json_obj_get(event, key);
    if (ma_truthy(value)) {
        char *s = ma_py_str(value);
        json_t *r = json_string(s ? s : "");
        free(s);
        return r;
    }
    const json_t *content = json_obj_get(event, "content");
    if (content && content->type == JSON_OBJECT) {
        value = json_obj_get(content, key);
        if (ma_truthy(value)) {
            char *s = ma_py_str(value);
            json_t *r = json_string(s ? s : "");
            free(s);
            return r;
        }
    }
    return json_null();
}

/* PoP: ma_extract_server_ed25519 @ plugins/platforms/matrix/adapter.py:_extract_server_ed25519 */
static json_t *ma_extract_server_ed25519(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *dk = json_obj_get(args, "device_keys");
    const json_t *keys = dk ? json_obj_get(dk, "keys") : NULL;
    if (!keys || keys->type != JSON_OBJECT) return json_null(); /* getattr(...,{}) or {} */
    for (size_t i = 0; i < json_object_size(keys); i++) {
        const char *kid = json_object_get_key_at(keys, i);
        if (kid && ma_starts_with(kid, "ed25519:")) {
            char *s = ma_py_str(json_object_get_at(keys, i));
            json_t *r = json_string(s ? s : "");
            free(s);
            return r;
        }
    }
    return json_null();
}

/* PoP: ma_configured_bool @ plugins/platforms/matrix/adapter.py:_configured_bool */
static json_t *ma_configured_bool(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *extra = json_obj_get(args, "extra");
    const char *key = json_get_str(args, "key", "");
    const json_t *configured = extra ? json_obj_get(extra, key) : NULL;
    if (ma_is_null(configured)) return json_null();
    if (configured->type == JSON_BOOL) return json_copy(configured);
    if (configured->type == JSON_STRING) {
        char *low = ma_lower(configured->str_val);
        bool falsy = strcmp(low, "false") == 0 || strcmp(low, "0") == 0
                  || strcmp(low, "no") == 0 || strcmp(low, "off") == 0;
        free(low);
        return json_bool(!falsy);
    }
    return json_bool(ma_truthy(configured));
}

/* PoP: ma_media_too_large @ plugins/platforms/matrix/adapter.py:_media_too_large */
static json_t *ma_media_too_large(ma_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    long long size = (long long)json_get_num(args, "size", 0);
    const json_t *mmb_node = ma_self_attr(args, "_max_media_bytes");
    long long mmb = (mmb_node && mmb_node->type == JSON_NUMBER) ? (long long)mmb_node->num_val : 0;
    char msg[256];
    snprintf(msg, sizeof msg, "Media file exceeds Matrix limit (%lld > %lld bytes)", size, mmb);
    json_t *r = json_object();
    json_set(r, "success", json_bool(false));
    json_set(r, "message_id", json_null());
    json_set_hdr(r, "error", msg);
    return r;
}

/* PoP: ma_matrix_prompt_expired @ plugins/platforms/matrix/adapter.py:_matrix_prompt_expired */
static json_t *ma_matrix_prompt_expired(ma_ctx_t *ctx, const json_t *args) {
    const json_t *prompt = json_obj_get(args, "prompt");
    const json_t *expires_at = prompt ? json_obj_get(prompt, "expires_at") : NULL;
    if (ma_is_null(expires_at)) return json_bool(false);
    json_t *mono = ma_call(ctx, "time.monotonic", json_object());
    if (ma_is_raise(mono)) return mono; /* no try/except: propagate */
    json_t *f = ma_float_of(expires_at);
    if (ma_is_raise(f)) { json_free(mono); return f; }
    bool expired = mono->num_val > f->num_val;
    json_free(mono); json_free(f);
    return json_bool(expired);
}

/* PoP: ma_apply_relation_metadata @ plugins/platforms/matrix/adapter.py:_apply_relation_metadata */
static json_t *ma_apply_relation_metadata(ma_ctx_t *ctx, const json_t *args) {
    const json_t *mc_in = json_obj_get(args, "msg_content");
    json_t *mc = (mc_in && mc_in->type == JSON_OBJECT) ? json_copy(mc_in) : json_object();
    const json_t *metadata = json_obj_get(args, "metadata");
    const json_t *reply_to = json_obj_get(args, "reply_to");
    const json_t *tid = (metadata && metadata->type == JSON_OBJECT)
        ? json_obj_get(metadata, "thread_id") : NULL;
    char *thread_id = ma_truthy(tid) ? ma_py_str(tid) : ma_strdup("");
    if (ma_truthy(reply_to)) {
        json_t *rt = json_object();
        json_t *irt = json_object();
        json_set(irt, "event_id", json_copy(reply_to));
        json_set(rt, "m.in_reply_to", irt);
        json_set(mc, "m.relates_to", rt);
    }
    if (thread_id[0]) {
        json_t *relates_to = json_obj_get(mc, "m.relates_to");
        bool existing = relates_to && relates_to->type == JSON_OBJECT;
        if (!existing) relates_to = json_object();
        json_set_hdr(relates_to, "rel_type", "m.thread");
        json_set_hdr(relates_to, "event_id", thread_id);
        json_set(relates_to, "is_falling_back", json_bool(true));
        if (!json_has(relates_to, "m.in_reply_to")) {
            const char *fallback = ma_truthy(reply_to) ? ma_str(reply_to) : thread_id;
            json_t *irt = json_object();
            json_set_hdr(irt, "event_id", fallback ? fallback : "");
            json_set(relates_to, "m.in_reply_to", irt);
        }
        if (!existing) json_set(mc, "m.relates_to", relates_to);
        /* existing: already in mc, mutated in place (no re-set: avoids freeing
         * the borrowed node that json_set would replace). */
    }
    free(thread_id);
    json_set(ctx->state, "msg_content", mc);
    return json_null();
}

/* PoP: ma_is_duplicate_event @ plugins/platforms/matrix/adapter.py:_is_duplicate_event */
static json_t *ma_is_duplicate_event(ma_ctx_t *ctx, const json_t *args) {
    const json_t *event_id = json_obj_get(args, "event_id");
    long long maxlen = (long long)json_get_num(args, "maxlen", 1000);
    json_t *events = json_obj_get(ctx->state, "processed_events");
    if (!events || events->type != JSON_ARRAY) {
        events = json_array();
        json_set(ctx->state, "processed_events", events);
    }
    json_t *ret = json_bool(false);
    if (ma_truthy(event_id)) {
        const char *eid = ma_str(event_id);
        bool dup = false;
        if (eid) {
            for (size_t i = 0; i < json_len(events); i++)
                if (ma_str_eq(json_get(events, i), eid)) { dup = true; break; }
        }
        if (dup) {
            ret = json_bool(true);
        } else {
            if ((long long)json_len(events) == maxlen && json_len(events) > 0)
                json_array_remove(events, 0); /* discard oldest (deque maxlen) */
            json_append(events, json_string(eid ? eid : ""));
        }
    }
    /* emit the set as a sorted array (both sides normalize set -> sorted list) */
    ma_strvec_t sv = {NULL, 0, 0};
    for (size_t i = 0; i < json_len(events); i++) {
        const char *s = ma_str(json_get(events, i));
        ma_sv_push(&sv, ma_strdup(s ? s : ""));
    }
    json_set(ctx->state, "processed_events_set", ma_sv_to_sorted_set(&sv));
    return ret;
}

/* PoP: ma_invalidate_room_identities @ plugins/platforms/matrix/adapter.py:_invalidate_room_identities */
static json_t *ma_invalidate_room_identities(ma_ctx_t *ctx, const json_t *args) {
    const json_t *room_id = json_obj_get(args, "room_id");
    json_t *ri = json_obj_get(ctx->state, "room_identities");
    if (!ri || ri->type != JSON_OBJECT) { ri = json_object(); json_set(ctx->state, "room_identities", ri); }
    json_t *ca = json_obj_get(ctx->state, "room_identity_cached_at");
    if (!ca || ca->type != JSON_OBJECT) { ca = json_object(); json_set(ctx->state, "room_identity_cached_at", ca); }
    if (ma_is_null(room_id)) {
        while (json_object_size(ri) > 0) json_obj_del(ri, json_object_get_key_at(ri, 0));
        while (json_object_size(ca) > 0) json_obj_del(ca, json_object_get_key_at(ca, 0));
    } else {
        const char *rid = ma_str(room_id);
        if (rid) { json_obj_del(ri, rid); json_obj_del(ca, rid); }
    }
    return json_null();
}
