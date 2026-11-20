/*
 * matrix_adapter_flow4.c -- FLOW port of UNCLAIMED module-level configuration
 * and classification helpers of plugins/platforms/matrix/adapter.py (hermes-agent):
 * message sizing, media-filename classification, E2EE-mode / env / secret
 * resolution, room-list CSV parsing, recovery-key target resolution, URL
 * redaction and dependency presence checks.
 *
 * The flow1/flow2/flow3 siblings and their claimed funcs are NOT re-ported.
 * Every collaborator below (ported or claimed, module or stdlib) enters through
 * the ONE universal seam as a bare module-function name -- none is re-ported.
 *
 * Decision sequences only: every live I/O / injected dependency enters through
 * ma4_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts.
 *
 * Seam name -> live call:
 *   _get_scoped_secret      -> _get_scoped_secret(name, default)
 *   _extra_or_secret        -> _extra_or_secret(extra, key, env, default, blank_is_unset)
 *   platform_registry.get   -> platform_registry.get("matrix")
 *   mimetypes.guess_type    -> mimetypes.guess_type(candidate)
 *   urlsplit                -> urlsplit(str(url))
 *   urlunsplit              -> urlunsplit((scheme, netloc, path, "", ""))
 *   is_available            -> pm.available("matrix")
 *   ensure_matrix_deps      -> ensure_matrix_deps()
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and ma4_-prefixed.
 *
 * Representation contract (shared with tests/oracle_matrix_adapter_flow4.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; scripted raises
 *   round-trip verbatim when they escape a drive. BaseException kinds pass
 *   through every except-Exception site; Exception kinds are swallowed there.
 * - Attribute carriers are {"__attr__": {attrs}} (config, registry entry,
 *   event). Sets return as sorted arrays (Python set -> canon sorted list).
 * - logger calls are silent. No state is mutated by these helpers.
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

/* -- universal seam context -- */
typedef struct ma4_ctx {
    json_t *state;      /* owned; emitted as out.state (empty for these helpers) */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ma4_ctx_t;

/* -- exception classification (FULL builtin hierarchy) -- */
static bool ma4_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ma4_exc_parent(const char *k) {
    static const struct { const char *n, *p; } T[] = {
        {"BaseException", NULL},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"CancelledError", "BaseException"},
        {"Exception", "BaseException"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"AttributeError", "Exception"}, {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"ZeroDivisionError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ImportError", "Exception"},
        {"ModuleNotFoundError", "ImportError"}, {"OSError", "Exception"},
        {"ConnectionError", "OSError"}, {"TimeoutError", "OSError"},
        {NULL, NULL}
    };
    for (size_t i = 0; T[i].n; i++) if (strcmp(T[i].n, k) == 0) return T[i].p;
    return "Exception";
}
static bool ma4_exc_is_exception(const char *k) {
    const char *p = k;
    for (int depth = 0; p && depth < 16; depth++) {
        if (strcmp(p, "Exception") == 0) return true;
        if (strcmp(p, "BaseException") == 0) return false;
        p = ma4_exc_parent(p);
    }
    return false;
}
static bool ma4_raise_is_base(const json_t *v) {
    return ma4_is_raise(v) && !ma4_exc_is_exception(json_get_str(v, "__raise__", ""));
}

/* -- truthiness / access -- */
static bool ma4_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ma4_truthy(const json_t *v) {
    if (ma4_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *ma4_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool ma4_str_eq(const json_t *v, const char *s) {
    const char *t = ma4_str(v);
    return t && s && strcmp(t, s) == 0;
}
static const char *ma4_type_name(const json_t *v) {
    if (ma4_is_null(v)) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static json_t *ma4_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *ma4_make_exc(const json_t *spec) {
    return ma4_err(json_get_str(spec, "type", "RuntimeError"),
                   json_get_str(spec, "message", ""));
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *ma4_call(ma4_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ma4_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ma4_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = ma4_make_exc(raise);
            else {
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

/* -- string helpers -- */
static char *ma4_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static bool ma4_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
/* Python str(v): None->"None", bool->True/False, number, string; else "". */
static char *ma4_py_str(const json_t *v) {
    if (ma4_is_null(v)) return ma4_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ma4_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return ma4_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return ma4_strdup(buf);
        }
        default: return ma4_strdup("");
    }
}
/* Python `v or ""` then str(): falsy -> "". */
static char *ma4_py_str_or_empty(const json_t *v) {
    if (!ma4_truthy(v)) return ma4_strdup("");
    return ma4_py_str(v);
}
/* Python str.strip() over ASCII whitespace (fixtures are ASCII). */
static char *ma4_trim(const char *s) {
    if (!s) return ma4_strdup("");
    while (*s && ma4_is_ascii_ws((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && ma4_is_ascii_ws((unsigned char)s[n - 1])) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}
static void ma4_lower(char *s) {
    for (; s && *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}
/* attribute-double access ({"__attr__": {attrs}}) */
static const json_t *ma4_attr(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *in = json_obj_get(obj, "__attr__");
    if (in && in->type == JSON_OBJECT) return json_obj_get(in, key);
    return json_obj_get(obj, key);
}

/* Python int(str) subset: optional sign, ASCII digits, whitespace. */
static bool ma4_py_int_str(const char *s, long long *out) {
    if (!s) return false;
    while (*s && ma4_is_ascii_ws((unsigned char)*s)) s++;
    if (!*s) return false;
    int sign = 1;
    if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
    if (!*s) return false;
    long long v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    while (*s && ma4_is_ascii_ws((unsigned char)*s)) s++;
    if (*s) return false;
    *out = sign * v;
    return true;
}
/* Python int(v): number truncates toward zero; bool->0/1; str parsed; else TypeError. */
static bool ma4_py_int(const json_t *v, long long *out) {
    if (ma4_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: *out = v->bool_val ? 1 : 0; return true;
        case JSON_NUMBER: *out = (long long)v->num_val; return true;
        case JSON_STRING: return ma4_py_int_str(v->str_val, out);
        default: return false;
    }
}
/* Python float(str) subset. */
static bool ma4_py_float_str(const char *s, double *out) {
    if (!s) return false;
    while (*s && ma4_is_ascii_ws((unsigned char)*s)) s++;
    if (!*s) return false;
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) return false;
    while (*end && ma4_is_ascii_ws((unsigned char)*end)) end++;
    if (*end) return false;
    *out = v;
    return true;
}
static bool ma4_py_float(const json_t *v, double *out) {
    if (ma4_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: *out = v->bool_val ? 1.0 : 0.0; return true;
        case JSON_NUMBER: *out = v->num_val; return true;
        case JSON_STRING: return ma4_py_float_str(v->str_val, out);
        default: return false;
    }
}

/* dedupe + lexicographic sort of a string array (Python sorted(set)). */
static int ma4_cmp_str(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}
static json_t *ma4_sorted_strings(json_t *arr) {
    size_t n = arr ? json_len(arr) : 0;
    char **v = n ? malloc(n * sizeof(char *)) : NULL;
    size_t m = 0;
    if (arr) for (size_t i = 0; i < n; i++) {
        const char *s = ma4_str(json_get(arr, i));
        v[m++] = ma4_strdup(s ? s : "");
    }
    if (m > 1) qsort(v, m, sizeof(char *), ma4_cmp_str);
    json_t *out = json_array();
    for (size_t i = 0; i < m; i++) {
        if (i == 0 || strcmp(v[i], v[i - 1]) != 0) json_append(out, json_string(v[i]));
        free(v[i]);
    }
    free(v);
    return out;
}

/* ========================= ported flows ========================= */

/* PoP: ma4_utf8_len @ plugins/platforms/matrix/adapter.py:_utf8_len */
static json_t *ma4_utf8_len(ma4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *t = json_obj_get(args, "text");
    if (ma4_is_null(t) || t->type != JSON_STRING)
        return ma4_err("AttributeError", "object has no attribute 'encode'");
    /* libjson stores UTF-8 bytes; Python len(text.encode('utf-8')) == strlen. */
    return json_number((double)strlen(t->str_val ? t->str_val : ""));
}

/* PoP: ma4_resolve_max_message_length @ plugins/platforms/matrix/adapter.py:_resolve_max_message_length */
static json_t *ma4_resolve_max_message_length(ma4_ctx_t *ctx, const json_t *args) {
    const json_t *config = json_obj_get(args, "config");
    const json_t *extra = ma4_attr(config, "extra");
    json_t *a = json_object();
    json_set(a, "extra", extra ? json_copy(extra) : json_null());
    json_set(a, "key", json_string("max_message_length"));
    json_set(a, "env_name", json_string("MATRIX_MAX_MESSAGE_LENGTH"));
    json_set(a, "default", json_null());
    json_set(a, "blank_is_unset", json_bool(true));
    json_t *raw = ma4_call(ctx, "_extra_or_secret", a);
    if (ma4_is_raise(raw)) return raw;

    char *rs = ma4_py_str(raw);
    char *rst = ma4_trim(rs);
    bool need_fallback = ma4_is_null(raw) || rst[0] == '\0';
    free(rs); free(rst);

    if (need_fallback) {
        /* with suppress(Exception): entry = platform_registry.get("matrix") */
        json_t *pa = json_object();
        json_set(pa, "key", json_string("matrix"));
        json_t *entry = ma4_call(ctx, "platform_registry.get", pa);
        if (ma4_is_raise(entry)) {
            if (ma4_raise_is_base(entry)) { json_free(raw); return entry; }
            /* Exception swallowed by suppress(Exception) */
        } else if (ma4_truthy(entry)) {
            const json_t *ml = ma4_attr(entry, "max_message_length");
            if (ma4_truthy(ml)) { json_free(raw); raw = json_copy(ml); }
        }
    }
    long long v = 0;
    if (!ma4_py_int(raw, &v)) { json_free(raw); return json_number(15000.0); }
    json_free(raw);
    if (v < 500) v = 500;
    if (v > 15000) v = 15000;
    return json_number((double)v);
}

/* lower-level transport filename classifier. Returns a raise marker or NULL;
 * sets *out on the normal path. */
static json_t *ma4_transport(ma4_ctx_t *ctx, const char *candidate,
                             const json_t *prefixes, const json_t *exts,
                             bool reject_spaces, bool *out) {
    *out = false;
    if (!candidate || !candidate[0] || strchr(candidate, '\n') ||
        candidate[strlen(candidate) - 1] == '/') return NULL;
    if (reject_spaces)
        for (const char *p = candidate; *p; p++)
            if (ma4_is_ascii_ws((unsigned char)*p)) return NULL;
    /* pathlib.Path.name / .suffix on Windows */
    const char *base = candidate;
    for (const char *p = candidate; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;
    if (strcmp(base, candidate) != 0) return NULL;
    const char *dot = strrchr(base, '.');
    if (!dot || dot == base || dot[1] == '\0') return NULL;
    char *suffix = ma4_strdup(dot);
    ma4_lower(suffix);

    json_t *a = json_object();
    json_set(a, "candidate", json_string(candidate));
    json_t *g = ma4_call(ctx, "mimetypes.guess_type", a);
    if (ma4_is_raise(g)) { free(suffix); return g; }
    bool ok = false;
    const char *gt = (g->type == JSON_ARRAY && json_len(g) > 0) ? ma4_str(json_get(g, 0)) : NULL;
    if (gt && gt[0] && prefixes && prefixes->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(prefixes); i++) {
            const char *pfx = ma4_str(json_get(prefixes, i));
            if (pfx && strncmp(gt, pfx, strlen(pfx)) == 0) { ok = true; break; }
        }
    if (!ok && exts && exts->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(exts); i++)
            if (ma4_str_eq(json_get(exts, i), suffix)) { ok = true; break; }
    free(suffix);
    json_free(g);
    *out = ok;
    return NULL;
}

/* PoP: ma4_looks_like_transport_filename @ plugins/platforms/matrix/adapter.py:_looks_like_transport_filename */
static json_t *ma4_looks_like_transport_filename(ma4_ctx_t *ctx, const json_t *args) {
    char *rs = ma4_py_str_or_empty(json_obj_get(args, "text"));
    char *cand = ma4_trim(rs);
    free(rs);
    const json_t *prefixes = json_obj_get(args, "mime_prefixes");
    const json_t *exts = json_obj_get(args, "exts");
    bool reject = json_get_bool(args, "reject_spaces", false);
    bool out = false;
    json_t *r = ma4_transport(ctx, cand, prefixes, exts, reject, &out);
    free(cand);
    if (r) return r;
    return json_bool(out);
}
static json_t *ma4_strarr(const char *const *items) {
    json_t *a = json_array();
    for (int i = 0; items[i]; i++) json_append(a, json_string(items[i]));
    return a;
}
/* PoP: ma4_looks_like_matrix_image_filename @ plugins/platforms/matrix/adapter.py:_looks_like_matrix_image_filename */
static json_t *ma4_looks_like_matrix_image_filename(ma4_ctx_t *ctx, const json_t *args) {
    static const char *const P[] = {"image/", NULL};
    static const char *const E[] = {".jpg", ".jpeg", ".png", ".gif", ".webp", ".bmp",
                                    ".svg", ".heic", ".heif", ".avif", NULL};
    char *rs = ma4_py_str_or_empty(json_obj_get(args, "text"));
    char *cand = ma4_trim(rs);
    free(rs);
    bool out = false;
    json_t *r = ma4_transport(ctx, cand, ma4_strarr(P), ma4_strarr(E), false, &out);
    free(cand);
    if (r) return r;
    return json_bool(out);
}
/* PoP: ma4_looks_like_matrix_media_filename @ plugins/platforms/matrix/adapter.py:_looks_like_matrix_media_filename */
static json_t *ma4_looks_like_matrix_media_filename(ma4_ctx_t *ctx, const json_t *args) {
    static const char *const P[] = {"audio/", "video/", NULL};
    static const char *const E[] = {".ogg", ".oga", ".opus", ".m4a", ".mp3", ".wav",
                                    ".flac", ".aac", ".amr", ".mp4", ".webm", ".mov", ".mkv", NULL};
    char *rs = ma4_py_str_or_empty(json_obj_get(args, "text"));
    char *cand = ma4_trim(rs);
    free(rs);
    bool out = false;
    json_t *r = ma4_transport(ctx, cand, ma4_strarr(P), ma4_strarr(E), true, &out);
    free(cand);
    if (r) return r;
    return json_bool(out);
}
/* PoP: ma4_is_bare_media_filename @ plugins/platforms/matrix/adapter.py:_is_bare_media_filename */
static json_t *ma4_is_bare_media_filename(ma4_ctx_t *ctx, const json_t *args) {
    const char *mt = json_get_str(args, "msgtype", "");
    json_t *sub = json_object();
    json_set(sub, "text", json_obj_get(args, "body") ? json_copy(json_obj_get(args, "body")) : json_null());
    if (strcmp(mt, "m.image") == 0) return ma4_looks_like_matrix_image_filename(ctx, sub);
    if (strcmp(mt, "m.audio") == 0 || strcmp(mt, "m.file") == 0 || strcmp(mt, "m.video") == 0)
        return ma4_looks_like_matrix_media_filename(ctx, sub);
    return json_bool(false);
}

/* PoP: ma4_matrix_event_timestamp_seconds @ plugins/platforms/matrix/adapter.py:_matrix_event_timestamp_seconds */
static json_t *ma4_matrix_event_timestamp_seconds(ma4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *ev = json_obj_get(args, "event");
    const json_t *tsv = ma4_attr(ev, "timestamp");
    if (!ma4_truthy(tsv)) tsv = ma4_attr(ev, "server_timestamp");
    if (!ma4_truthy(tsv)) tsv = NULL;
    double ts = 0.0;
    if (tsv && !ma4_py_float(tsv, &ts)) ts = 0.0;
    if (ts > 10000000000.0) ts /= 1000.0;
    return json_number(ts);
}

/* PoP: ma4_normalize_e2ee_mode @ plugins/platforms/matrix/adapter.py:_normalize_e2ee_mode */
static json_t *ma4_normalize_e2ee_mode(ma4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *rs = ma4_py_str_or_empty(json_obj_get(args, "value"));
    char *t = ma4_trim(rs);
    ma4_lower(t);
    free(rs);
    static const char *const REQ[] = {"required", "require", "true", "1", "yes", "on", NULL};
    static const char *const OPT[] = {"optional", "prefer", "preferred", NULL};
    const char *result = "off";
    for (int i = 0; REQ[i]; i++) if (strcmp(t, REQ[i]) == 0) { result = "required"; break; }
    if (strcmp(result, "off") == 0)
        for (int i = 0; OPT[i]; i++) if (strcmp(t, OPT[i]) == 0) { result = "optional"; break; }
    free(t);
    return json_string(result);
}

/* internal: _env_truthy(name, default) -- fires _get_scoped_secret seam. */
static json_t *ma4_env_truthy_impl(ma4_ctx_t *ctx, const char *name, const char *def) {
    json_t *a = json_object();
    json_set(a, "name", json_string(name));
    json_set(a, "default", json_string(def));
    json_t *r = ma4_call(ctx, "_get_scoped_secret", a);
    if (ma4_is_raise(r)) return r;
    char *s = ma4_py_str(r);
    ma4_lower(s);
    bool ok = strcmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcmp(s, "yes") == 0;
    free(s);
    json_free(r);
    return json_bool(ok);
}
/* PoP: ma4_env_truthy @ plugins/platforms/matrix/adapter.py:_env_truthy */
static json_t *ma4_env_truthy(ma4_ctx_t *ctx, const json_t *args) {
    return ma4_env_truthy_impl(ctx, json_get_str(args, "name", ""), json_get_str(args, "default", ""));
}

/* PoP: ma4_env_number @ plugins/platforms/matrix/adapter.py:_env_number */
static json_t *ma4_env_number(ma4_ctx_t *ctx, const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    const json_t *def = json_obj_get(args, "default");
    const char *cast = json_get_str(args, "cast", "int");
    char *defs = ma4_py_str(def);
    json_t *a = json_object();
    json_set(a, "name", json_string(name));
    json_set(a, "default", json_string(defs));
    free(defs);
    json_t *r = ma4_call(ctx, "_get_scoped_secret", a);
    if (ma4_is_raise(r)) return r;
    char *s = ma4_py_str(r);
    json_t *out = NULL;
    if (strcmp(cast, "float") == 0) {
        double d = 0.0;
        out = ma4_py_float_str(s, &d) ? json_number(d) : json_copy(def);
    } else {
        long long v = 0;
        out = ma4_py_int_str(s, &v) ? json_number((double)v) : json_copy(def);
    }
    free(s);
    json_free(r);
    return out;
}

/* PoP: ma4_csv_set @ plugins/platforms/matrix/adapter.py:_csv_set */
static json_t *ma4_csv_set(ma4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *raw = json_obj_get(args, "raw");
    json_t *acc = json_array();
    if (raw && raw->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(raw); i++) {
            char *s = ma4_py_str(json_get(raw, i));
            char *t = ma4_trim(s);
            free(s);
            if (t[0]) json_append(acc, json_string(t));
            free(t);
        }
    } else {
        char *s = ma4_py_str(raw);
        char *cur = s;
        for (;;) {
            char *comma = strchr(cur, ',');
            if (comma) *comma = '\0';
            char *t = ma4_trim(cur);
            if (t[0]) json_append(acc, json_string(t));
            free(t);
            if (!comma) break;
            cur = comma + 1;
        }
        free(s);
    }
    return ma4_sorted_strings(acc);
}

/* PoP: ma4_thread_root @ plugins/platforms/matrix/adapter.py:_thread_root */
static json_t *ma4_thread_root(ma4_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *rt = json_obj_get(args, "relates_to");
    if (!rt || rt->type != JSON_OBJECT) {
        char m[160];
        snprintf(m, sizeof m, "'%s' object has no attribute 'get'", ma4_type_name(rt));
        return ma4_err("AttributeError", m);
    }
    if (!ma4_str_eq(json_obj_get(rt, "rel_type"), "m.thread")) return json_null();
    const json_t *eid = json_obj_get(rt, "event_id");
    return eid ? json_copy(eid) : json_null();
}

/* PoP: ma4_extra_csv_set @ plugins/platforms/matrix/adapter.py:_extra_csv_set */
static json_t *ma4_extra_csv_set(ma4_ctx_t *ctx, const json_t *args) {
    const json_t *extra = ma4_attr(json_obj_get(args, "config"), "extra");
    json_t *a = json_object();
    json_set(a, "extra", extra ? json_copy(extra) : json_null());
    json_set(a, "key", json_string(json_get_str(args, "key", "")));
    json_set(a, "env_name", json_string(json_get_str(args, "env_name", "")));
    json_set(a, "default", json_string(""));
    json_set(a, "blank_is_unset", json_bool(false));
    json_t *r = ma4_call(ctx, "_extra_or_secret", a);
    if (ma4_is_raise(r)) return r;
    json_t *sub = json_object();
    json_set(sub, "raw", r);
    return ma4_csv_set(ctx, sub);
}

/* PoP: ma4_scoped_recovery_key @ plugins/platforms/matrix/adapter.py:_scoped_recovery_key */
static json_t *ma4_scoped_recovery_key(ma4_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set(a, "name", json_string("MATRIX_RECOVERY_KEY"));
    json_set(a, "default", json_string(""));
    json_t *r = ma4_call(ctx, "_get_scoped_secret", a);
    if (ma4_is_raise(r)) return r;
    if (ma4_is_null(r)) {
        json_free(r);
        return ma4_err("AttributeError", "'NoneType' object has no attribute 'strip'");
    }
    char *s = ma4_py_str(r);
    char *t = ma4_trim(s);
    free(s);
    json_free(r);
    return json_string(t);
}

/* PoP: ma4_redact_url_for_log @ plugins/platforms/matrix/adapter.py:_redact_url_for_log */
static json_t *ma4_redact_url_for_log(ma4_ctx_t *ctx, const json_t *args) {
    char *u = ma4_py_str(json_obj_get(args, "url"));
    json_t *a = json_object();
    json_set(a, "url", json_string(u));
    json_t *r = ma4_call(ctx, "urlsplit", a);
    if (ma4_is_raise(r)) {
        json_t *out = ma4_raise_is_base(r) ? r : json_string("<url>");
        if (out != r) json_free(r);
        free(u);
        return out;
    }
    const json_t *scheme = ma4_attr(r, "scheme");
    const json_t *netloc = ma4_attr(r, "netloc");
    const json_t *path = ma4_attr(r, "path");
    bool structured = ma4_truthy(scheme) || ma4_truthy(netloc);
    /* copy BEFORE freeing r -- borrowed pointers would dangle into the parts array */
    json_t *ps = scheme ? json_copy(scheme) : json_null();
    json_t *pn = netloc ? json_copy(netloc) : json_null();
    json_t *pp = path ? json_copy(path) : json_null();
    json_free(r);
    if (!structured) {
        json_free(ps); json_free(pn); json_free(pp);
        char *p = strchr(u, '?'); if (p) *p = '\0';
        char *q = strchr(u, '#'); if (q) *q = '\0';
        json_t *out = json_string(u);
        free(u);
        return out;
    }
    json_t *parts = json_array();
    json_append(parts, ps);
    json_append(parts, pn);
    json_append(parts, pp);
    json_append(parts, json_string(""));
    json_append(parts, json_string(""));
    json_t *a2 = json_object();
    json_set(a2, "parts", parts);
    json_t *r2 = ma4_call(ctx, "urlunsplit", a2);
    free(u);
    if (ma4_is_raise(r2)) {
        json_t *out = ma4_raise_is_base(r2) ? r2 : json_string("<url>");
        if (out != r2) json_free(r2);
        return out;
    }
    return r2;
}

/* PoP: ma4_matrix_deps_present @ plugins/platforms/matrix/adapter.py:matrix_deps_present */
static json_t *ma4_matrix_deps_present(ma4_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set(a, "name", json_string("matrix"));
    json_t *r = ma4_call(ctx, "is_available", a);
    if (ma4_is_raise(r)) {
        json_t *out = ma4_raise_is_base(r) ? r : json_bool(false);
        if (out != r) json_free(r);
        return out;
    }
    /* return the raw availability value verbatim (live def returns it directly) */
    return r;
}

/* PoP: ma4_check_matrix_requirements @ plugins/platforms/matrix/adapter.py:check_matrix_requirements */
static json_t *ma4_check_matrix_requirements(ma4_ctx_t *ctx, const json_t *args) {
    (void)args;
    static const char *const NAMES[] = {"MATRIX_ACCESS_TOKEN", "MATRIX_PASSWORD", "MATRIX_HOMESERVER", NULL};
    char *vals[3] = {NULL, NULL, NULL};
    for (int i = 0; i < 3; i++) {
        json_t *a = json_object();
        json_set(a, "name", json_string(NAMES[i]));
        json_set(a, "default", json_string(""));
        json_t *r = ma4_call(ctx, "_get_scoped_secret", a);
        if (ma4_is_raise(r)) {
            for (int j = 0; j < 3; j++) free(vals[j]);
            return r;
        }
        char *s = ma4_py_str(r);
        vals[i] = ma4_trim(s);
        free(s);
        json_free(r);
    }
    bool ok = true;
    if (!vals[0][0] && !vals[1][0]) ok = false;
    if (ok && !vals[2][0]) ok = false;
    for (int j = 0; j < 3; j++) free(vals[j]);
    if (!ok) return json_bool(false);
    json_t *e = json_object();
    json_t *r = ma4_call(ctx, "ensure_matrix_deps", e);
    return r;  /* no try/except: propagate any raise verbatim */
}

/* PoP: ma4_resolve_e2ee_mode @ plugins/platforms/matrix/adapter.py:_resolve_e2ee_mode */
static json_t *ma4_resolve_e2ee_mode(ma4_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    if (!ma4_truthy(extra)) extra = NULL;
    const json_t *ev = extra ? ma4_attr(extra, "e2ee_mode") : NULL;
    json_t *explicit = NULL;
    if (ma4_truthy(ev)) {
        explicit = json_copy(ev);
    } else {
        json_t *a = json_object();
        json_set(a, "name", json_string("MATRIX_E2EE_MODE"));
        json_set(a, "default", json_string(""));
        explicit = ma4_call(ctx, "_get_scoped_secret", a);
        if (ma4_is_raise(explicit)) return explicit;
    }
    if (ma4_truthy(explicit)) {
        json_t *sub = json_object();
        json_set(sub, "value", explicit);
        return ma4_normalize_e2ee_mode(ctx, sub);
    }
    json_free(explicit);
    /* legacy_enabled = extra.get("encryption", _env_truthy(...)) -- default eager */
    json_t *envt = ma4_env_truthy_impl(ctx, "MATRIX_ENCRYPTION", "");
    if (ma4_is_raise(envt)) return envt;
    const json_t *encval = extra ? ma4_attr(extra, "encryption") : NULL;
    bool legacy = encval ? ma4_truthy(encval) : ma4_truthy(envt);
    json_free(envt);
    return json_string(legacy ? "required" : "off");
}
