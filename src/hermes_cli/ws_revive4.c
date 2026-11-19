/*
 * ws_revive4.c — deep-stub revival group 4 (web_server_wrappers.c).
 *
 * Faithful C11 ports of 24 hermes_cli web-server helpers under their
 * ORIGINAL cfn names (json_t *fn(json_t *req)). The empty-alloc stubs
 * were deleted from web_server_wrappers.c; the PoP tags moved here.
 *
 * ── Universal seam contract ──────────────────────────────────────────────
 * All I/O rides one injected channel:
 *   g_ws4_io->call(io, name, args) -> fresh json_t* (caller frees).
 * The seam OWNS args: the port builds a fresh args object, hands it over
 * and NEVER frees it afterwards (the harness dispatcher adopts it into
 * the trace event). NULL result == Python None; a result object
 * {"__raise__": Type, "message": msg} models a Python exception.
 * The dispatcher records {ev, args, ret} into the trace in call order.
 * Script semantics (identical in the oracle): missing name -> RuntimeError
 * "no script for <name>"; array entries are per-call queues (exhausted ->
 * RuntimeError "script exhausted for <name>"); {"__script__": true,
 * "ret": v, "raise": {type, message}} wraps list returns / scripted raises;
 * any other value is reused verbatim.
 *
 * Exception propagation out of the fixed json_t* signature rides
 * g_ws4_pending_raise: the port parks the raise spec there and returns
 * NULL; the harness emits the spec as ret. `except Exception` sites
 * swallow plain specs but pass the BaseException kinds (SystemExit,
 * KeyboardInterrupt, GeneratorExit, CancelledError) through. Functions
 * with no try park ANY raise. Production default (no channel installed)
 * is fail-closed: seam-using ports return NULL without calling anything;
 * pure ports run without a channel.
 *
 * ── Modeling / fixture scope (mirrors oracle_ws_revive4.py) ─────────────
 * - Integral JSON numbers model Python ints (libjson cannot distinguish
 *   1 from 1.0); str()-projection cases use int literals only.
 * - str() projections cover scalars (None/bool/int/str); truthy container
 *   inputs to str() are out of fixture scope (fail-closed RuntimeError).
 * - Attribute carriers (entry/policy/m/record/provider) are SimpleNamespace
 *   doubles in the oracle: missing-key errors read 'types.SimpleNamespace'.
 * - Path results are projected to posix strings; path joins cover
 *   drive-less slash paths (no backslashes, no drive letters). pathlib
 *   keeps '..' components and collapses '.'; exactly two leading slashes
 *   are preserved.
 * - .lower() is ASCII-folded: the only comparisons it feeds are ASCII
 *   literal membership/equality ("current", boolean words, slug charset).
 * - str.strip() is Unicode-White_Space-aware (full CPython set).
 * - None-valued top-level seam arg keys are dropped on BOTH sides.
 * - _log.debug/warning calls are silent on both sides (no seam).
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"

typedef struct ws4_io {
    /* Seam OWNS args (frees/adopts them); result is a fresh node. */
    json_t *(*call)(void *io, const char *name, json_t *args);
    void *ctx;
} ws4_io_t;

static ws4_io_t *g_ws4_io;
static json_t *g_ws4_pending_raise;

#define WS4_ENTER() do { json_free(g_ws4_pending_raise); g_ws4_pending_raise = NULL; } while (0)
#define WS4_IO_REQUIRED() do { if (!g_ws4_io || !g_ws4_io->call) return NULL; } while (0)

static int ws4_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static int ws4_truthy(const json_t *v) {
    if (ws4_none(v)) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static const char *ws4_typename(const json_t *v) {
    if (ws4_none(v)) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *ws4_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
}

static const char *ws4_raise_kind(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}

static int ws4_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

/* Park a propagating raise spec (adopts it); port returns NULL after. */
static void ws4_park(json_t *spec) {
    json_free(g_ws4_pending_raise);
    g_ws4_pending_raise = spec;
}

static json_t *ws4_attr_error(const char *typename_, const char *attr) {
    size_t n = strlen(typename_) + strlen(attr) + 48;
    char *msg = malloc(n);
    if (!msg) return ws4_raise("MemoryError", "out of memory");
    snprintf(msg, n, "'%s' object has no attribute '%s'", typename_, attr);
    json_t *e = ws4_raise("AttributeError", msg);
    free(msg);
    return e;
}

/* Universal seam call. args is OWNED BY THE SEAM — never free it after. */
static json_t *ws4_call(const char *name, json_t *args) {
    if (!g_ws4_io || !g_ws4_io->call) {
        json_free(args); /* no-seam fallback path only */
        ws4_park(ws4_raise("RuntimeError", "no seam channel installed"));
        return NULL;
    }
    return g_ws4_io->call(g_ws4_io, name, args);
}

/* Add a borrowed value as an arg key; None-valued keys dropped (both sides). */
static void ws4_arg(json_t *args, const char *key, const json_t *val) {
    if (ws4_none(val)) return;
    json_set(args, key, json_copy(val));
}

static char *ws4_dupn(const char *s, size_t n) {
    char *p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* ── UTF-8 / Unicode helpers ──────────────────────────────────────────── */

static unsigned ws4_cp_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned cp = s[0];
    if (cp < 0x80) { *p += 1; return cp; }
    int extra = 0;
    if ((cp & 0xE0) == 0xC0) { cp &= 0x1F; extra = 1; }
    else if ((cp & 0xF0) == 0xE0) { cp &= 0x0F; extra = 2; }
    else if ((cp & 0xF8) == 0xF0) { cp &= 0x07; extra = 3; }
    else { *p += 1; return cp; } /* invalid lead byte: treat as latin1 */
    for (int i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) { *p += 1; return s[0]; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *p += extra + 1;
    return cp;
}

/* CPython str.isspace() White_Space set (probed on the quarry 3.14). */
static int ws4_is_space(unsigned cp) {
    if (cp <= 0x20)
        return (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F) || cp == 0x20;
    return cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) ||
           cp == 0x2028 || cp == 0x2029 || cp == 0x202F ||
           cp == 0x205F || cp == 0x3000;
}

static size_t ws4_cp_len(const char *s) {
    size_t n = 0;
    const char *p = s;
    while (*p) { ws4_cp_next(&p); n++; }
    return n;
}

/* str.strip() — full Unicode White_Space set, both ends. */
static char *ws4_strip(const char *s) {
    const char *p = s;
    size_t start = 0;
    while (*p) {
        unsigned cp = ws4_cp_next(&p);
        if (!ws4_is_space(cp)) break;
        start = (size_t)(p - s);
    }
    size_t end = start;
    const char *q = s + start;
    while (*q) {
        unsigned cp = ws4_cp_next(&q);
        size_t off = (size_t)(q - s);
        if (!ws4_is_space(cp)) end = off;
    }
    return ws4_dupn(s + start, end - start);
}

/* str.strip(chars) for an ASCII char set, both ends. */
static char *ws4_strip_chars(const char *s, const char *chars) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < n && strchr(chars, (unsigned char)s[a])) a++;
    while (b > a && strchr(chars, (unsigned char)s[b - 1])) b--;
    return ws4_dupn(s + a, b - a);
}

static char *ws4_rstrip_slash(const char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '/') n--;
    return ws4_dupn(s, n);
}

/* ASCII case fold (safe for the ASCII-literal comparisons it feeds). */
static void ws4_lower_ascii(char *s) {
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + ('a' - 'A'));
}

/* str(value) scalar projection; NULL for containers / fractional floats
 * (out of fixture scope — callers park a fail-closed RuntimeError). */
static char *ws4_str_of(const json_t *v) {
    char buf[64];
    if (!v) return ws4_dupn("", 0);
    if (v->type == JSON_NULL) return ws4_dupn("None", 4);
    if (v->type == JSON_BOOL) return ws4_dupn(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && d >= -1e15 && d <= 1e15) {
            snprintf(buf, sizeof buf, "%lld", (long long)d);
            return ws4_dupn(buf, strlen(buf));
        }
        return NULL;
    }
    if (v->type == JSON_STRING) return ws4_dupn(v->str_val, strlen(v->str_val));
    return NULL;
}

/* Python s[:stop] slice semantics over codepoints (stop may be negative). */
static char *ws4_slice_prefix(const char *s, long long stop) {
    size_t n = ws4_cp_len(s);
    if (stop < 0) stop += (long long)n;
    if (stop < 0) stop = 0;
    if (stop > (long long)n) stop = (long long)n;
    const char *p = s;
    for (long long i = 0; i < stop; i++) ws4_cp_next(&p);
    return ws4_dupn(s, (size_t)(p - s));
}

/* ── Path join projection (drive-less posix scope, see header) ────────── */

/* Normalize a slash path: collapse '.' and duplicate slashes, KEEP '..'
 * (pathlib does not resolve it), preserve exactly two leading slashes. */
static char *ws4_path_normalize(const char *s) {
    int absolute = s[0] == '/';
    int dbl = absolute && s[1] == '/' && s[2] != '/';
    /* component list */
    size_t cap = 8, count = 0;
    char **comp = malloc(cap * sizeof(char *));
    if (!comp) return NULL;
    const char *p = s;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *a = p;
        while (*p && *p != '/') p++;
        size_t n = (size_t)(p - a);
        if (n == 1 && a[0] == '.') continue;
        if (count == cap) {
            cap *= 2;
            char **nc = realloc(comp, cap * sizeof(char *));
            if (!nc) { for (size_t i = 0; i < count; i++) free(comp[i]); free(comp); return NULL; }
            comp = nc;
        }
        comp[count++] = ws4_dupn(a, n);
    }
    size_t total = 3;
    for (size_t i = 0; i < count; i++) total += strlen(comp[i]) + 1;
    char *out = malloc(total + 1);
    if (!out) { for (size_t i = 0; i < count; i++) free(comp[i]); free(comp); return NULL; }
    size_t k = 0;
    if (absolute) { out[k++] = '/'; if (dbl) out[k++] = '/'; }
    for (size_t i = 0; i < count; i++) {
        if (i) out[k++] = '/';
        size_t n = strlen(comp[i]);
        memcpy(out + k, comp[i], n);
        k += n;
        free(comp[i]);
    }
    out[k] = '\0';
    free(comp);
    return out;
}

/* Path(home) / name / "config.json" for drive-less slash paths. */
static char *ws4_flat_path(const char *home, const char *name) {
    char *h = ws4_path_normalize(home);
    if (!h) return NULL;
    char *raw;
    if (name[0] == '/') {
        /* absolute second operand resets the root */
        size_t n = strlen(name);
        raw = malloc(n + 16);
        if (raw) { memcpy(raw, name, n); memcpy(raw + n, "/config.json", 13); }
        free(h);
    } else {
        size_t hn = strlen(h), nn = strlen(name);
        int sep = (hn > 0 && h[hn - 1] != '/') ? 1 : 0;
        raw = malloc(hn + (size_t)sep + nn + 16);
        if (raw) {
            memcpy(raw, h, hn);
            size_t k = hn;
            if (sep) raw[k++] = '/';
            memcpy(raw + k, name, nn);
            k += nn;
            memcpy(raw + k, "/config.json", 13);
        }
        free(h);
    }
    if (!raw) return NULL;
    char *norm = ws4_path_normalize(raw);
    free(raw);
    return norm;
}

/* ── _coerce_bool (web_server_memory.py) — static helper port ─────────── */

/* Returns a raise spec (ValueError / out-of-scope) or NULL; *out gets the
 * boolean. default is the keyword `default=` (False at the _field_default
 * call site). */
static json_t *ws4_coerce_bool(const json_t *v, int dflt, int *out) {
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return NULL; }
    if (ws4_none(v) || (v->type == JSON_STRING && v->str_val[0] == '\0')) {
        *out = dflt;
        return NULL;
    }
    if (v->type == JSON_NUMBER) { *out = v->num_val != 0.0 ? 1 : 0; return NULL; }
    if (v->type == JSON_STRING) {
        char *t = ws4_strip(v->str_val);
        if (!t) return ws4_raise("MemoryError", "out of memory");
        ws4_lower_ascii(t);
        int yes = !strcmp(t, "1") || !strcmp(t, "true") || !strcmp(t, "yes") || !strcmp(t, "on");
        int no = !strcmp(t, "0") || !strcmp(t, "false") || !strcmp(t, "no") || !strcmp(t, "off");
        free(t);
        if (yes) { *out = 1; return NULL; }
        if (no) { *out = 0; return NULL; }
        size_t n = strlen(v->str_val) + 32;
        char *msg = malloc(n);
        if (!msg) return ws4_raise("MemoryError", "out of memory");
        snprintf(msg, n, "Invalid boolean value: %s", v->str_val);
        json_t *e = ws4_raise("ValueError", msg);
        free(msg);
        return e;
    }
    return ws4_raise("RuntimeError", "ws4: container bool coercion out of fixture scope");
}

/* ── _oauth_profile_name (web_server_oauth.py) — static helper port ───── */

/* *out = malloc'd profile name, or NULL for None. Returns a raise spec or
 * NULL. requested = (profile or "").strip(); empty/"current" -> None. */
static json_t *ws4_oauth_profile_name(const json_t *profile, char **out) {
    *out = NULL;
    if (!ws4_truthy(profile)) return NULL;
    if (profile->type != JSON_STRING)
        return ws4_attr_error(ws4_typename(profile), "strip");
    char *req = ws4_strip(profile->str_val);
    if (!req) return ws4_raise("MemoryError", "out of memory");
    if (!req[0]) { free(req); return NULL; }
    char *low = ws4_dupn(req, strlen(req));
    if (!low) { free(req); return ws4_raise("MemoryError", "out of memory"); }
    ws4_lower_ascii(low);
    int current = !strcmp(low, "current");
    free(low);
    if (current) { free(req); return NULL; }
    *out = req;
    return NULL;
}

/* ── regex equivalents ────────────────────────────────────────────────── */

/* _MEMORY_PROVIDER_NAME_RE.fullmatch: ^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$ */
static int ws4_provider_name_ok(const char *s) {
    size_t n = strlen(s);
    if (n < 1 || n > 64) return 0;
    unsigned char c0 = (unsigned char)s[0];
    if (!((c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z') || (c0 >= '0' && c0 <= '9')))
        return 0;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

/* re.search(r"\$\{[^}]+\}", s): "${" + >=1 non-"}" chars + "}". */
static int ws4_env_ref_search(const char *s) {
    for (const char *p = s; *p; p++) {
        if (p[0] != '$' || p[1] != '{') continue;
        if (p[2] == '\0' || p[2] == '}') continue;
        for (const char *q = p + 2; *q; q++)
            if (*q == '}') return 1;
    }
    return 0;
}

/* re.sub(r"[^A-Za-z0-9_-]+", "-", s): runs of disallowed bytes -> one '-'. */
static char *ws4_slug_sub(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t k = 0;
    int in_run = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (ok) { out[k++] = (char)c; in_run = 0; }
        else if (!in_run) { out[k++] = '-'; in_run = 1; }
    }
    out[k] = '\0';
    return out;
}

/* ── ports ────────────────────────────────────────────────────────────── */

/* PoP: ws_u_normalize_memory_provider_name @ hermes_cli/web_server_memory.py:_normalize_memory_provider_name */
json_t *ws_u_normalize_memory_provider_name(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    const json_t *name = json_obj_get(req, "name");
    /* provider = str(name or "").strip() */
    char *provider;
    if (!ws4_truthy(name)) {
        provider = ws4_dupn("", 0);
    } else {
        char *s = ws4_str_of(name);
        if (!s) { ws4_park(ws4_raise("RuntimeError", "ws4: container str() out of fixture scope")); return NULL; }
        provider = ws4_strip(s);
        free(s);
    }
    if (!provider) { ws4_park(ws4_raise("MemoryError", "out of memory")); return NULL; }
    json_t *args = json_object();
    json_set_hdr(args, "provider", provider);
    json_t *r = ws4_call("is_core_memory_provider", args);
    const char *kind = ws4_raise_kind(r);
    if (kind) { ws4_park(r); free(provider); return NULL; } /* no try */
    json_t *ret = json_string(ws4_truthy(r) ? "" : provider);
    json_free(r);
    free(provider);
    return ret;
}

/* PoP: ws_u_env_lookup @ hermes_cli/web_server_memory.py:_env_lookup */
json_t *ws_u_env_lookup(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    const json_t *env_key = json_obj_get(req, "env_key");
    if (!ws4_truthy(env_key)) return json_string("");
    json_t *r = ws4_call("load_env", json_object());
    const char *kind = ws4_raise_kind(r);
    if (kind) { ws4_park(r); return NULL; } /* no try */
    /* load_env().get(env_key) — non-dict result raises AttributeError */
    if (!r || r->type != JSON_OBJECT) {
        ws4_park(ws4_attr_error(ws4_typename(r), "get"));
        json_free(r);
        return NULL;
    }
    const json_t *v1 = (env_key->type == JSON_STRING)
        ? json_obj_get(r, env_key->str_val) : NULL;
    const json_t *final_v = NULL;
    if (ws4_truthy(v1)) {
        final_v = v1;
    } else {
        /* os.environ.get(env_key) — os._Environ rejects non-str keys */
        if (env_key->type != JSON_STRING) {
            size_t n = strlen(ws4_typename(env_key)) + 32;
            char *msg = malloc(n);
            snprintf(msg, n, "str expected, not %s", ws4_typename(env_key));
            ws4_park(ws4_raise("TypeError", msg));
            free(msg);
            json_free(r);
            return NULL;
        }
        const json_t *env = json_obj_get(req, "env");
        const json_t *v2 = (env && env->type == JSON_OBJECT)
            ? json_obj_get(env, env_key->str_val) : NULL;
        if (ws4_truthy(v2)) final_v = v2;
    }
    char *s = ws4_str_of(final_v); /* final falsy -> str("") == "" */
    json_free(r);
    if (!s) { ws4_park(ws4_raise("RuntimeError", "ws4: container str() out of fixture scope")); return NULL; }
    json_t *ret = json_string(s);
    free(s);
    return ret;
}

/* PoP: ws_u_field_default @ hermes_cli/web_server_memory.py:_field_default */
json_t *ws_u_field_default(json_t *req) {
    WS4_ENTER();
    const json_t *field = json_obj_get(req, "field");
    if (!field || field->type != JSON_OBJECT) {
        ws4_park(ws4_attr_error(ws4_typename(field), "get"));
        return NULL;
    }
    /* default = field.get("default", "") */
    const json_t *dflt = json_obj_get(field, "default");
    json_t *dflt_owned = dflt ? json_copy(dflt) : json_string("");
    /* field["kind"] — missing key raises KeyError('kind') */
    const json_t *kind = json_obj_get(field, "kind");
    if (!kind) {
        ws4_park(ws4_raise("KeyError", "'kind'"));
        json_free(dflt_owned);
        return NULL;
    }
    if (kind->type == JSON_STRING && strcmp(kind->str_val, "boolean") == 0) {
        int b = 0;
        json_t *err = ws4_coerce_bool(dflt, 0, &b);
        json_free(dflt_owned);
        if (err) { ws4_park(err); return NULL; }
        return json_bool(b != 0);
    }
    return dflt_owned;
}

/* PoP: ws_u_load_memory_provider @ hermes_cli/web_server_memory.py:_load_memory_provider */
json_t *ws_u_load_memory_provider(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    json_t *args = json_object();
    ws4_arg(args, "name", json_obj_get(req, "name"));
    ws4_arg(args, "register_skills", json_obj_get(req, "register_skills"));
    json_t *r = ws4_call("load_memory_provider", args);
    const char *kind = ws4_raise_kind(r);
    if (kind) {
        if (ws4_is_base_exc(kind)) { ws4_park(r); return NULL; }
        json_free(r); /* except Exception -> None (_log.debug silent) */
        return NULL;
    }
    return r;
}

/* PoP: ws_u_memory_provider_manifest @ hermes_cli/web_server_memory.py:_memory_provider_manifest */
json_t *ws_u_memory_provider_manifest(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    json_t *args = json_object();
    ws4_arg(args, "name", json_obj_get(req, "name"));
    json_t *r = ws4_call("memory_provider_dependency_inputs", args);
    const char *kind = ws4_raise_kind(r);
    if (kind) {
        if (ws4_is_base_exc(kind)) { ws4_park(r); return NULL; }
        json_free(r);
        return json_object(); /* except Exception -> {} */
    }
    /* manifest, _inputs = <result> — unpack errors are swallowed to {} */
    json_t *manifest = NULL;
    char msg[96];
    if (ws4_none(r) || r->type == JSON_BOOL || r->type == JSON_NUMBER) {
        snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", ws4_typename(r));
        json_free(r);
        return json_object();
    }
    if (r->type == JSON_ARRAY) {
        size_t n = json_len(r);
        if (n == 2) manifest = json_copy(json_get(r, 0));
        else if (n < 2) snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", n);
        else snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)", n);
    } else if (r->type == JSON_STRING) {
        size_t n = ws4_cp_len(r->str_val);
        if (n == 2) {
            const char *p = r->str_val;
            const char *a = p;
            ws4_cp_next(&p);
            char first[8];
            size_t fn_ = (size_t)(p - a);
            memcpy(first, a, fn_);
            first[fn_] = '\0';
            manifest = json_string(first);
        } else if (n < 2) {
            snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", n);
        } else {
            /* str unpack overflow carries no "got N" (probed 3.14) */
            snprintf(msg, sizeof msg, "too many values to unpack (expected 2)");
        }
    } else { /* JSON_OBJECT: unpack iterates keys */
        size_t n = r->c.count;
        if (n == 2) manifest = json_string(r->c.keys[0]);
        else if (n < 2) snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", n);
        else snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)", n);
    }
    json_free(r);
    if (!manifest) return json_object(); /* ValueError/TypeError swallowed */
    return manifest;
}

/* PoP: ws_u_trim_setup_output @ hermes_cli/web_routers/memory_providers.py:_trim_setup_output */
json_t *ws_u_trim_setup_output(json_t *req) {
    WS4_ENTER();
    const json_t *value = json_obj_get(req, "value");
    char *text;
    if (!ws4_truthy(value)) {
        text = ws4_dupn("", 0);
    } else {
        char *s = ws4_str_of(value);
        if (!s) { ws4_park(ws4_raise("RuntimeError", "ws4: container str() out of fixture scope")); return NULL; }
        text = ws4_dupn(s, strlen(s));
        free(s);
    }
    if (!text) { ws4_park(ws4_raise("MemoryError", "out of memory")); return NULL; }
    long long limit = (long long)json_get_num(req, "limit", 4000);
    size_t n = ws4_cp_len(text);
    json_t *ret;
    if ((long long)n <= limit) {
        ret = json_string(text);
    } else {
        char *head = ws4_slice_prefix(text, limit);
        size_t hn = strlen(head);
        const char *suffix = "\n... truncated ...";
        char *joined = malloc(hn + strlen(suffix) + 1);
        if (!joined) { free(head); free(text); ws4_park(ws4_raise("MemoryError", "out of memory")); return NULL; }
        memcpy(joined, head, hn);
        memcpy(joined + hn, suffix, strlen(suffix) + 1);
        ret = json_string(joined);
        free(joined);
        free(head);
    }
    free(text);
    return ret;
}

/* PoP: ws_u_require_valid_memory_provider_name @ hermes_cli/web_routers/memory_providers.py:_require_valid_memory_provider_name */
json_t *ws_u_require_valid_memory_provider_name(json_t *req) {
    WS4_ENTER();
    const json_t *name = json_obj_get(req, "name");
    /* fullmatch(name or "") — truthy non-str raises TypeError */
    const char *s = "";
    if (ws4_truthy(name)) {
        if (name->type != JSON_STRING) {
            size_t n = strlen(ws4_typename(name)) + 64;
            char *msg = malloc(n);
            snprintf(msg, n, "expected string or bytes-like object, got '%s'", ws4_typename(name));
            ws4_park(ws4_raise("TypeError", msg));
            free(msg);
            return NULL;
        }
        s = name->str_val;
    }
    if (ws4_provider_name_ok(s)) return NULL; /* None */
    /* raise _unknown_provider(name): HTTPException 404, str "404: <detail>".
     * An absent name key models Python None (f-string -> "None"). */
    char *names = name ? ws4_str_of(name) : ws4_dupn("None", 4);
    if (!names) { ws4_park(ws4_raise("RuntimeError", "ws4: container str() out of fixture scope")); return NULL; }
    size_t n = strlen(names) + 48;
    char *msg = malloc(n);
    if (!msg) { free(names); ws4_park(ws4_raise("MemoryError", "out of memory")); return NULL; }
    snprintf(msg, n, "404: Unknown memory provider: %s", names);
    ws4_park(ws4_raise("HTTPException", msg));
    free(msg);
    free(names);
    return NULL;
}

/* PoP: ws_u_write_provider_flat @ hermes_cli/web_routers/memory_providers.py:_write_provider_flat */
json_t *ws_u_write_provider_flat(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    const json_t *provider = json_obj_get(req, "provider"); /* name projection */
    const json_t *values = json_obj_get(req, "values");
    const char *pname = (provider && provider->type == JSON_STRING) ? provider->str_val : "";
    json_t *args;
    json_t *r;
    const char *kind;

    /* existing = _read_flat_json(provider) */
    args = json_object();
    json_set_hdr(args, "provider", pname);
    json_t *existing = ws4_call("read_flat_json", args);
    kind = ws4_raise_kind(existing);
    if (kind) { ws4_park(existing); return NULL; } /* no try */

    /* _save_submitted_secrets(provider, values) */
    args = json_object();
    json_set_hdr(args, "provider", pname);
    ws4_arg(args, "values", values);
    r = ws4_call("save_submitted_secrets", args);
    kind = ws4_raise_kind(r);
    if (kind) { ws4_park(r); json_free(existing); return NULL; }
    json_free(r);

    /* _apply_field_values(provider, values, existing) */
    args = json_object();
    json_set_hdr(args, "provider", pname);
    ws4_arg(args, "values", values);
    ws4_arg(args, "existing", existing);
    r = ws4_call("apply_field_values", args);
    kind = ws4_raise_kind(r);
    if (kind) { ws4_park(r); json_free(existing); return NULL; }
    json_free(r);

    /* _write_json_0600(_flat_json_path(provider), existing) */
    args = json_object();
    json_set_hdr(args, "provider", pname);
    json_t *path = ws4_call("flat_json_path", args);
    kind = ws4_raise_kind(path);
    if (kind) { ws4_park(path); json_free(existing); return NULL; }
    args = json_object();
    ws4_arg(args, "path", path);
    ws4_arg(args, "data", existing);
    r = ws4_call("write_json_0600", args);
    kind = ws4_raise_kind(r);
    if (kind) { ws4_park(r); json_free(path); json_free(existing); return NULL; }
    json_free(r);
    json_free(path);
    json_free(existing);
    return NULL; /* None */
}

/* PoP: ws_u_flat_json_path @ hermes_cli/web_routers/memory_providers.py:_flat_json_path */
json_t *ws_u_flat_json_path(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    /* get_hermes_home() / provider.name / "config.json" — the home call
     * evaluates BEFORE the provider.name attribute access. */
    json_t *r = ws4_call("get_hermes_home", json_object());
    const char *kind = ws4_raise_kind(r);
    if (kind) { ws4_park(r); return NULL; } /* no try */
    if (!r || r->type != JSON_STRING) {
        json_free(r);
        ws4_park(ws4_raise("RuntimeError", "ws4: non-string home out of fixture scope"));
        return NULL;
    }
    const json_t *provider = json_obj_get(req, "provider");
    /* provider.name — SimpleNamespace double in the oracle */
    const json_t *name = (provider && provider->type == JSON_OBJECT)
        ? json_obj_get(provider, "name") : NULL;
    if (!name) {
        json_free(r);
        ws4_park(ws4_attr_error("types.SimpleNamespace", "name"));
        return NULL;
    }
    if (name->type != JSON_STRING) {
        /* Path / non-str: host pathlib class name (probed 3.14 Windows) */
        const char *cls =
#ifdef _WIN32
            "WindowsPath";
#else
            "PosixPath";
#endif
        size_t n = strlen(cls) + strlen(ws4_typename(name)) + 64;
        char *msg = malloc(n);
        snprintf(msg, n, "unsupported operand type(s) for /: '%s' and '%s'",
                 cls, ws4_typename(name));
        ws4_park(ws4_raise("TypeError", msg));
        free(msg);
        json_free(r);
        return NULL;
    }
    char *joined = ws4_flat_path(r->str_val, name->str_val);
    json_free(r);
    if (!joined) { ws4_park(ws4_raise("MemoryError", "out of memory")); return NULL; }
    json_t *ret = json_string(joined);
    free(joined);
    return ret;
}

/* PoP: ws_u_custom_endpoint_id @ hermes_cli/web_routers/config_env.py:_custom_endpoint_id */
json_t *ws_u_custom_endpoint_id(json_t *req) {
    WS4_ENTER();
    const json_t *raw = json_obj_get(req, "raw");
    const char *fallback = json_get_str(req, "fallback", "custom");
    /* coerce_provider_id(raw): None -> ""; else str(raw).strip() */
    char *coerced;
    if (ws4_none(raw)) {
        coerced = ws4_dupn("", 0);
    } else {
        char *s = ws4_str_of(raw);
        if (!s) { ws4_park(ws4_raise("RuntimeError", "ws4: container str() out of fixture scope")); return NULL; }
        coerced = ws4_strip(s);
        free(s);
    }
    if (!coerced) { ws4_park(ws4_raise("MemoryError", "out of memory")); return NULL; }
    char *sub = ws4_slug_sub(coerced);
    free(coerced);
    if (!sub) { ws4_park(ws4_raise("MemoryError", "out of memory")); return NULL; }
    char *slug = ws4_strip_chars(sub, "-_");
    free(sub);
    if (!slug) { ws4_park(ws4_raise("MemoryError", "out of memory")); return NULL; }
    ws4_lower_ascii(slug);
    json_t *ret = json_string(slug[0] ? slug : fallback);
    free(slug);
    return ret;
}

/* PoP: ws_u_config_api_key_is_env_ref @ hermes_cli/web_routers/config_env.py:_config_api_key_is_env_ref */
json_t *ws_u_config_api_key_is_env_ref(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    json_t *args = json_object();
    ws4_arg(args, "endpoint_id", json_obj_get(req, "endpoint_id"));
    json_t *r = ws4_call("_raw_provider_api_key", args);
    const char *kind = ws4_raise_kind(r);
    if (kind) { ws4_park(r); return NULL; } /* no try */
    int hit = r && r->type == JSON_STRING && ws4_env_ref_search(r->str_val);
    json_free(r);
    return json_bool(hit != 0);
}

/* PoP: ws_u_pool_entry_summary @ hermes_cli/web_routers/ops.py:_pool_entry_summary */
json_t *ws_u_pool_entry_summary(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    const json_t *entry = json_obj_get(req, "entry");
    /* entry is a SimpleNamespace double; attribute reads in live order */
    static const char *attrs[] = {
        "access_token", "id", "label", "auth_type", "source",
        "priority", "last_status", "request_count", "refresh_token",
    };
    const json_t *vals[9];
    for (int i = 0; i < 9; i++) {
        vals[i] = (entry && entry->type == JSON_OBJECT)
            ? json_obj_get(entry, attrs[i]) : NULL;
        if (!vals[i] && i < 8) {
            /* access_token..request_count are read before the seam call;
             * refresh_token (i==8) is read AFTER token_preview below. */
            ws4_park(ws4_attr_error("types.SimpleNamespace", attrs[i]));
            return NULL;
        }
    }
    /* token = entry.access_token or "" (any truthy type reaches redact_key) */
    const json_t *token = ws4_truthy(vals[0]) ? vals[0] : NULL;
    json_t *out = json_object();
    const json_t *index = json_obj_get(req, "index");
    json_set(out, "index", index ? json_copy(index) : json_null());
    json_set(out, "id", json_copy(vals[1]));
    json_set(out, "label", json_copy(vals[2]));
    json_set(out, "auth_type", json_copy(vals[3]));
    json_set(out, "source", json_copy(vals[4]));
    json_set(out, "priority", json_copy(vals[5]));
    json_set(out, "last_status", json_copy(vals[6]));
    json_set(out, "request_count", json_copy(vals[7]));
    /* token_preview: redact_key(token) if token else "" */
    if (token) {
        json_t *args = json_object();
        json_set(args, "token", json_copy(token));
        json_t *r = ws4_call("redact_key", args);
        const char *kind = ws4_raise_kind(r);
        if (kind) { ws4_park(r); json_free(out); return NULL; } /* no try */
        json_set(out, "token_preview", r ? r : json_null());
    } else {
        json_set_hdr(out, "token_preview", "");
    }
    /* has_refresh: bool(entry.refresh_token) — attribute read happens here */
    if (!vals[8]) {
        ws4_park(ws4_attr_error("types.SimpleNamespace", "refresh_token"));
        json_free(out);
        return NULL;
    }
    json_set(out, "has_refresh", json_bool(ws4_truthy(vals[8]) != 0));
    return out;
}

/* PoP: ws_u_strip_session_list_rows @ hermes_cli/web_server_gateway.py:_strip_session_list_rows */
json_t *ws_u_strip_session_list_rows(json_t *req) {
    WS4_ENTER();
    json_t *sessions = json_obj_get(req, "sessions");
    if (!sessions || sessions->type != JSON_ARRAY) {
        size_t n = strlen(ws4_typename(sessions)) + 40;
        char *msg = malloc(n);
        snprintf(msg, n, "'%s' object is not iterable", ws4_typename(sessions));
        ws4_park(ws4_raise("TypeError", msg));
        free(msg);
        return NULL;
    }
    /* _SESSION_LIST_HEAVY_FIELDS = ("system_prompt", "model_config") */
    for (size_t i = 0; i < json_len(sessions); i++) {
        json_t *s = json_get(sessions, i);
        if (!s || s->type != JSON_OBJECT) {
            ws4_park(ws4_attr_error(ws4_typename(s), "pop"));
            return NULL;
        }
        json_obj_del(s, "system_prompt");
        json_obj_del(s, "model_config");
    }
    return json_copy(sessions);
}

/* PoP: ws_u_strip_dashboard_manifest @ hermes_cli/web_server_dashboard.py:_strip_dashboard_manifest */
json_t *ws_u_strip_dashboard_manifest(json_t *req) {
    WS4_ENTER();
    const json_t *p = json_obj_get(req, "p");
    if (!p || p->type != JSON_OBJECT) {
        ws4_park(ws4_attr_error(ws4_typename(p), "items"));
        return NULL;
    }
    json_t *out = json_object();
    for (size_t i = 0; i < p->c.count; i++) {
        const char *k = p->c.keys[i];
        if (k[0] == '_') continue;
        json_set(out, k, json_copy(p->c.items[i]));
    }
    return out;
}

/* PoP: ws_u_managed_response_meta @ hermes_cli/web_server_files.py:_managed_response_meta */
json_t *ws_u_managed_response_meta(json_t *req) {
    WS4_ENTER();
    const json_t *policy = json_obj_get(req, "policy");
    const json_t *lr = (policy && policy->type == JSON_OBJECT)
        ? json_obj_get(policy, "locked_root") : NULL;
    if (!lr) {
        ws4_park(ws4_attr_error("types.SimpleNamespace", "locked_root"));
        return NULL;
    }
    /* locked_root = str(policy.locked_root) if ... is not None else None */
    char *lrs = NULL;
    if (!ws4_none(lr)) {
        lrs = ws4_str_of(lr);
        if (!lrs) { ws4_park(ws4_raise("RuntimeError", "ws4: container str() out of fixture scope")); return NULL; }
    }
    const json_t *ccp = json_obj_get(policy, "can_change_path");
    if (!ccp) {
        free(lrs);
        ws4_park(ws4_attr_error("types.SimpleNamespace", "can_change_path"));
        return NULL;
    }
    json_t *out = json_object();
    if (lrs) {
        json_set_hdr(out, "root", lrs);
        json_set_hdr(out, "locked_root", lrs);
    } else {
        json_set(out, "root", json_null());
        json_set(out, "locked_root", json_null());
    }
    json_set(out, "can_change_path", json_copy(ccp));
    free(lrs);
    return out;
}

/* PoP: ws_u_skill_meta_to_payload @ hermes_cli/web_routers/skills.py:_skill_meta_to_payload */
json_t *ws_u_skill_meta_to_payload(json_t *req) {
    WS4_ENTER();
    const json_t *m = json_obj_get(req, "m");
    static const char *attrs[] = {
        "name", "description", "source", "identifier", "trust_level", "repo", "tags",
    };
    const json_t *vals[7];
    for (int i = 0; i < 7; i++) {
        vals[i] = (m && m->type == JSON_OBJECT) ? json_obj_get(m, attrs[i]) : NULL;
        if (!vals[i]) {
            ws4_park(ws4_attr_error("types.SimpleNamespace", attrs[i]));
            return NULL;
        }
    }
    json_t *out = json_object();
    json_set(out, "name", json_copy(vals[0]));
    json_set(out, "description", json_copy(vals[1]));
    json_set(out, "source", json_copy(vals[2]));
    json_set(out, "identifier", json_copy(vals[3]));
    json_set(out, "trust_level", json_copy(vals[4]));
    json_set(out, "repo", json_copy(vals[5]));
    /* tags: list(m.tags or []) */
    const json_t *t = vals[6];
    json_t *tags = json_array();
    if (ws4_truthy(t)) {
        if (t->type == JSON_ARRAY) {
            json_free(tags);
            tags = json_copy(t);
        } else if (t->type == JSON_STRING) {
            const char *p = t->str_val;
            while (*p) {
                const char *a = p;
                ws4_cp_next(&p);
                char *cp = ws4_dupn(a, (size_t)(p - a));
                json_append(tags, json_string(cp));
                free(cp);
            }
        } else if (t->type == JSON_OBJECT) {
            for (size_t i = 0; i < t->c.count; i++)
                json_append(tags, json_string(t->c.keys[i]));
        } else {
            /* list(int/bool) raises TypeError */
            size_t n = strlen(ws4_typename(t)) + 40;
            char *msg = malloc(n);
            snprintf(msg, n, "'%s' object is not iterable", ws4_typename(t));
            ws4_park(ws4_raise("TypeError", msg));
            free(msg);
            json_free(tags);
            json_free(out);
            return NULL;
        }
    }
    json_set(out, "tags", tags);
    return out;
}

/* PoP: ws_u_clear_skills_prompt_cache @ hermes_cli/web_routers/skills.py:_clear_skills_prompt_cache */
json_t *ws_u_clear_skills_prompt_cache(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    (void)req;
    json_t *args = json_object();
    json_set(args, "clear_snapshot", json_bool(true));
    json_t *r = ws4_call("clear_skills_system_prompt_cache", args);
    const char *kind = ws4_raise_kind(r);
    if (kind) {
        if (ws4_is_base_exc(kind)) { ws4_park(r); return NULL; }
        json_free(r); /* except Exception: pass */
    } else {
        json_free(r);
    }
    return NULL; /* None */
}

/* PoP: ws_u_aux_usage_rows @ hermes_cli/web_server_profiles.py:_aux_usage_rows */
json_t *ws_u_aux_usage_rows(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    json_t *args = json_object();
    ws4_arg(args, "started_after", json_obj_get(req, "cutoff"));
    json_t *r = ws4_call("auxiliary_usage_by_model_task", args);
    const char *kind = ws4_raise_kind(r);
    if (kind) {
        if (ws4_is_base_exc(kind)) { ws4_park(r); return NULL; }
        json_free(r);
        return json_array(); /* except Exception -> [] */
    }
    return r;
}

/* PoP: ws_u_write_platform_enabled @ hermes_cli/web_server_messaging.py:_write_platform_enabled */
json_t *ws_u_write_platform_enabled(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    json_t *args = json_object();
    ws4_arg(args, "platform_id", json_obj_get(req, "platform_id"));
    json_set_hdr(args, "field", "enabled");
    ws4_arg(args, "value", json_obj_get(req, "enabled"));
    json_t *r = ws4_call("write_platform_config_field", args);
    const char *kind = ws4_raise_kind(r);
    if (kind) { ws4_park(r); return NULL; } /* no try */
    json_free(r);
    return NULL; /* None */
}

/* PoP: ws_u_whatsapp_session_path @ hermes_cli/web_server_messaging.py:_whatsapp_session_path */
json_t *ws_u_whatsapp_session_path(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    (void)req;
    json_t *args = json_object();
    json_set_hdr(args, "new_subpath", "platforms/whatsapp/session");
    json_set_hdr(args, "old_name", "whatsapp/session");
    json_t *r = ws4_call("get_hermes_dir", args);
    const char *kind = ws4_raise_kind(r);
    if (kind) { ws4_park(r); return NULL; } /* no try */
    return r; /* Path projected to its scripted posix string */
}

/* PoP: ws_u_whatsapp_onboarding_payload @ hermes_cli/web_server_messaging.py:_whatsapp_onboarding_payload */
json_t *ws_u_whatsapp_onboarding_payload(json_t *req) {
    WS4_ENTER();
    const json_t *record = json_obj_get(req, "record");
    /* _WHATSAPP_PAYLOAD_FIELDS order (module constant) */
    static const char *fields[] = {
        "status", "qr_payload", "expires_at", "mode", "allowed_users",
        "account_id", "account_name", "account_phone", "error",
    };
    json_t *out = json_object();
    const json_t *pid = json_obj_get(req, "pairing_id");
    json_set(out, "pairing_id", pid ? json_copy(pid) : json_null());
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        const json_t *v = (record && record->type == JSON_OBJECT)
            ? json_obj_get(record, fields[i]) : NULL;
        if (!v) {
            ws4_park(ws4_attr_error("types.SimpleNamespace", fields[i]));
            json_free(out);
            return NULL;
        }
        json_set(out, fields[i], json_copy(v));
    }
    return out;
}

/* PoP: ws_u_telegram_onboarding_base_url @ hermes_cli/web_server_messaging.py:_telegram_onboarding_base_url */
json_t *ws_u_telegram_onboarding_base_url(json_t *req) {
    WS4_ENTER();
    /* os.getenv("TELEGRAM_ONBOARDING_URL", _TELEGRAM_ONBOARDING_DEFAULT_URL) */
    static const char *DEFAULT_URL = "https://setup.hermes-agent.nousresearch.com";
    const json_t *env = json_obj_get(req, "env");
    const json_t *v = (env && env->type == JSON_OBJECT)
        ? json_obj_get(env, "TELEGRAM_ONBOARDING_URL") : NULL;
    const char *url = DEFAULT_URL;
    if (v) {
        if (v->type != JSON_STRING) {
            ws4_park(ws4_attr_error(ws4_typename(v), "strip"));
            return NULL;
        }
        url = v->str_val;
    }
    char *stripped = ws4_strip(url);
    if (!stripped) { ws4_park(ws4_raise("MemoryError", "out of memory")); return NULL; }
    char *final = ws4_rstrip_slash(stripped);
    free(stripped);
    if (!final) { ws4_park(ws4_raise("MemoryError", "out of memory")); return NULL; }
    json_t *ret = json_string(final);
    free(final);
    return ret;
}

/* PoP: ws_u_profile_setup_command @ hermes_cli/web_routers/profiles.py:_profile_setup_command */
json_t *ws_u_profile_setup_command(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    const json_t *name = json_obj_get(req, "name");
    json_t *args = json_object();
    ws4_arg(args, "name", name);
    json_t *r = ws4_call("_resolve_profile_dir", args);
    const char *kind = ws4_raise_kind(r);
    if (kind) { ws4_park(r); return NULL; } /* no try (404 propagates) */
    json_free(r);
    if (name && name->type == JSON_STRING && strcmp(name->str_val, "default") == 0)
        return json_string("hermes setup");
    /* f"{name} setup" — absent name models Python None -> "None" */
    char *s = name ? ws4_str_of(name) : ws4_dupn("None", 4);
    if (!s) { ws4_park(ws4_raise("RuntimeError", "ws4: container str() out of fixture scope")); return NULL; }
    size_t n = strlen(s) + 8;
    char *cmd = malloc(n);
    if (!cmd) { free(s); ws4_park(ws4_raise("MemoryError", "out of memory")); return NULL; }
    snprintf(cmd, n, "%s setup", s);
    json_t *ret = json_string(cmd);
    free(cmd);
    free(s);
    return ret;
}

/* PoP: ws_u_validate_oauth_profile @ hermes_cli/web_routers/oauth.py:_validate_oauth_profile */
json_t *ws_u_validate_oauth_profile(json_t *req) {
    WS4_ENTER();
    WS4_IO_REQUIRED();
    char *profile_name = NULL;
    json_t *err = ws4_oauth_profile_name(json_obj_get(req, "profile"), &profile_name);
    if (err) { ws4_park(err); return NULL; }
    if (profile_name) {
        /* if profile_name: _resolve_profile_dir(profile_name) */
        json_t *args = json_object();
        json_set_hdr(args, "name", profile_name);
        json_t *r = ws4_call("_resolve_profile_dir", args);
        const char *kind = ws4_raise_kind(r);
        if (kind) { ws4_park(r); free(profile_name); return NULL; } /* no try */
        json_free(r);
        json_t *ret = json_string(profile_name);
        free(profile_name);
        return ret;
    }
    return NULL; /* None */
}
