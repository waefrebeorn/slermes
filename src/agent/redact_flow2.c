/* redact_flow2.c — FLOW-oracle port of the UNCLAIMED agent/redact.py helpers.
 *
 * Ports the 40 gap functions of agent/redact.py as decision-sequence FLOW ports.
 * Every regex / foreign sub-call is INJECTED via the rdf2_io_t universal seam:
 * io->call(io, name, args) appends `name` to io->trace and returns the scripted
 * result. NULL/json_null == None; {"__raise__": cls, "message": msg} is a raised
 * exception whose class is routed through the FULL builtin exception hierarchy
 * (rdf2_exc_parent). Control flow around each seam matches the live Python
 * exactly; the regex leaves themselves are exercised by the oracle's own cases.
 *
 * All symbols are TU-static (rdf2_*); the harness #includes this file.
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state
 * are harness-owned. rdf2_call() frees the args object it is handed.
 */
#ifndef REDACT_FLOW2_C
#define REDACT_FLOW2_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

typedef struct rdf2_io_t {
    json_t *(*call)(struct rdf2_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} rdf2_io_t;

/* ── tiny string builder ──────────────────────────────────────────────── */
typedef struct { char *p; size_t n, cap; } rdf2_sb_t;
static void rdf2_sb_init(rdf2_sb_t *b) { b->cap = 128; b->n = 0; b->p = malloc(b->cap); if (b->p) b->p[0] = '\0'; }
static void rdf2_sb_add(rdf2_sb_t *b, const char *s) {
    if (!s) return; size_t l = strlen(s);
    if (b->n + l + 1 > b->cap) { while (b->n + l + 1 > b->cap) b->cap *= 2; char *q = realloc(b->p, b->cap); if (!q) return; b->p = q; }
    memcpy(b->p + b->n, s, l); b->n += l; b->p[b->n] = '\0';
}
static void rdf2_sb_ch(rdf2_sb_t *b, char c) { char t[2] = { c, '\0' }; rdf2_sb_add(b, t); }
static json_t *rdf2_sb_str(rdf2_sb_t *b) { json_t *j = json_string(b->p ? b->p : ""); free(b->p); b->p = NULL; return j; }
static char *rdf2_sb_take(rdf2_sb_t *b) { char *p = b->p ? b->p : strdup(""); b->p = NULL; return p; }

/* ── core io / exception helpers ──────────────────────────────────────── */
static void rdf2_trace(rdf2_io_t *io, const char *name) {
    if (io && io->trace && io->trace->type == JSON_ARRAY) json_append(io->trace, json_string(name));
}
static json_t *rdf2_call(rdf2_io_t *io, const char *name, json_t *args) {
    rdf2_trace(io, name);
    json_t *r = (io && io->call) ? io->call(io, name, args) : NULL;
    if (args) json_free(args);
    return r ? r : json_null();
}
static bool rdf2_raised(const json_t *r) { return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL; }
static const char *rdf2_exc_kind(const json_t *r) {
    if (!rdf2_raised(r)) return "Exception";
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "Exception";
}
static const char *rdf2_exc_msg(const json_t *r) {
    if (!rdf2_raised(r)) return "";
    json_t *m = json_obj_get(r, "message");
    return (m && m->type == JSON_STRING && m->str_val) ? m->str_val : "";
}
/* Full builtin exception parent table (py3.14). ExceptionGroup routes through
 * Exception (single-parent walk keeps the `except Exception` contract). */
static const char *rdf2_exc_parent(const char *k) {
    if (!k) return NULL;
    struct P { const char *n, *p; } t[] = {
        {"BaseException",""},
        {"Exception","BaseException"},
        {"ArithmeticError","Exception"},{"AssertionError","Exception"},{"AttributeError","Exception"},
        {"BaseExceptionGroup","BaseException"},{"BlockingIOError","OSError"},{"BrokenPipeError","ConnectionError"},
        {"BufferError","Exception"},{"BytesWarning","Warning"},{"ChildProcessError","OSError"},
        {"ConnectionAbortedError","ConnectionError"},{"ConnectionError","OSError"},{"ConnectionRefusedError","ConnectionError"},
        {"ConnectionResetError","ConnectionError"},{"DeprecationWarning","Warning"},{"EOFError","Exception"},
        {"EncodingWarning","Warning"},{"EnvironmentError","OSError"},{"ExceptionGroup","Exception"},
        {"FileExistsError","OSError"},{"FileNotFoundError","OSError"},{"FloatingPointError","ArithmeticError"},
        {"FutureWarning","Warning"},{"GeneratorExit","BaseException"},{"ImportError","Exception"},{"ImportWarning","Warning"},{"RuntimeWarning","Warning"},
        {"IndentationError","SyntaxError"},{"IndexError","LookupError"},{"InterruptedError","OSError"},
        {"IsADirectoryError","OSError"},{"KeyError","LookupError"},{"KeyboardInterrupt","BaseException"},
        {"LookupError","Exception"},{"MemoryError","Exception"},{"ModuleNotFoundError","ImportError"},
        {"NameError","Exception"},{"NotADirectoryError","OSError"},{"NotImplementedError","RuntimeError"},
        {"OSError","Exception"},{"OverflowError","ArithmeticError"},{"PendingDeprecationWarning","Warning"},
        {"PermissionError","OSError"},{"ProcessLookupError","OSError"},{"PythonFinalizationError","RuntimeError"},
        {"RecursionError","RuntimeError"},{"ReferenceError","Exception"},{"ResourceWarning","Warning"},
        {"RuntimeError","Exception"},{"StopAsyncIteration","Exception"},{"StopIteration","Exception"},
        {"SyntaxError","Exception"},{"SyntaxWarning","Warning"},{"SystemError","Exception"},
        {"SystemExit","BaseException"},{"TabError","IndentationError"},{"TimeoutError","OSError"},
        {"TypeError","Exception"},{"UnboundLocalError","NameError"},{"UnicodeDecodeError","UnicodeError"},
        {"UnicodeEncodeError","UnicodeError"},{"UnicodeError","ValueError"},{"UnicodeTranslateError","UnicodeError"},
        {"UnicodeWarning","Warning"},{"UserWarning","Warning"},{"ValueError","Exception"},
        {"Warning","Exception"},{"ZeroDivisionError","ArithmeticError"},
        {"IOError","OSError"},{"WindowsError","OSError"},{"_IncompleteInputError","SyntaxError"},
        {NULL,NULL},
    };
    for (int i = 0; t[i].n; i++) if (strcmp(t[i].n, k) == 0) return t[i].p;
    return NULL; /* unknown custom class -> treats as Exception subclass by default below */
}
static bool rdf2_is_subclass(const char *k, const char *base) {
    if (!k || !base) return false;
    if (strcmp(k, base) == 0) return true;
    const char *p = rdf2_exc_parent(k);
    int guard = 0;
    while (p && guard++ < 32) {
        if (strcmp(p, base) == 0) return true;
        p = rdf2_exc_parent(p);
    }
    /* Unknown class: assume direct Exception subclass (scripted custom types). */
    if (!rdf2_exc_parent(k)) return strcmp(base, "Exception") == 0 || strcmp(base, "BaseException") == 0;
    return false;
}
/* Does an `except (A, B, ...)` (comma list of base names) catch r? */
static bool rdf2_catches(const json_t *r, const char *const *bases) {
    if (!rdf2_raised(r)) return false;
    const char *k = rdf2_exc_kind(r);
    for (int i = 0; bases[i]; i++) if (rdf2_is_subclass(k, bases[i])) return true;
    return false;
}

/* ── scalar / string helpers ──────────────────────────────────────────── */
static json_t *rdf2_get(const json_t *obj, const char *key) { return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL; }
static bool rdf2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool rdf2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *rdf2_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = rdf2_get(obj, key);
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return def;
}
static bool rdf2_bool(const json_t *obj, const char *key, bool def) {
    json_t *v = rdf2_get(obj, key);
    if (v && v->type == JSON_BOOL) return v->bool_val;
    return def;
}
static long rdf2_int(const json_t *obj, const char *key, long def) {
    json_t *v = rdf2_get(obj, key);
    if (v && v->type == JSON_NUMBER) return (long)v->num_val;
    return def;
}
static const char *rdf2_scalar_str(const json_t *v) {
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return "";
}
static char *rdf2_dup(const char *s) { return strdup(s ? s : ""); }
static bool rdf2_isalpha(char c) { return isalpha((unsigned char)c) != 0; }
static bool rdf2_isdigit(char c) { return isdigit((unsigned char)c) != 0; }
static bool rdf2_isupperc(char c) { return isupper((unsigned char)c) != 0; }
static bool rdf2_islowerc(char c) { return islower((unsigned char)c) != 0; }
static char *rdf2_lower(const char *s) {
    size_t n = strlen(s ? s : ""); char *o = malloc(n + 1);
    if (!o) return NULL; for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)s[i]); o[n] = '\0'; return o;
}
static char *rdf2_upper(const char *s) {
    size_t n = strlen(s ? s : ""); char *o = malloc(n + 1);
    if (!o) return NULL; for (size_t i = 0; i < n; i++) o[i] = (char)toupper((unsigned char)s[i]); o[n] = '\0'; return o;
}
static bool rdf2_startswith(const char *s, const char *pre) { return s && pre && strncmp(s, pre, strlen(pre)) == 0; }
static bool rdf2_endswith(const char *s, const char *suf) { if (!s || !suf) return false; size_t n = strlen(s), m = strlen(suf); return n >= m && strcmp(s + n - m, suf) == 0; }
static bool rdf2_contains(const char *s, const char *needle) { return s && needle && strstr(s, needle) != NULL; }
/* strip a set of chars from both ends */
static char *rdf2_strip_chars(const char *s, const char *chars) {
    const char *p = s ? s : ""; size_t n = strlen(p), a = 0, b = n;
    while (a < b && strchr(chars, p[a])) a++;
    while (b > a && strchr(chars, p[b-1])) b--;
    char *o = malloc(b - a + 1); if (!o) return NULL; memcpy(o, p + a, b - a); o[b-a] = '\0'; return o;
}
static bool rdf2_in_strs(const char *const *set, const char *v) { if (!v) return false; for (int i = 0; set[i]; i++) if (!strcmp(set[i], v)) return true; return false; }
/* UTF-8 encode a codepoint into buf (returns buf). */
static char *rdf2_chr_utf8(unsigned long cp, char out[5]) {
    if (cp < 0x80) { out[0] = (char)cp; out[1] = '\0'; }
    else if (cp < 0x800) { out[0] = (char)(0xC0|(cp>>6)); out[1]=(char)(0x80|(cp&0x3F)); out[2]='\0'; }
    else if (cp < 0x10000) { out[0]=(char)(0xE0|(cp>>12)); out[1]=(char)(0x80|((cp>>6)&0x3F)); out[2]=(char)(0x80|(cp&0x3F)); out[3]='\0'; }
    else { out[0]=(char)(0xF0|(cp>>18)); out[1]=(char)(0x80|((cp>>12)&0x3F)); out[2]=(char)(0x80|((cp>>6)&0x3F)); out[3]=(char)(0x80|(cp&0x3F)); out[4]='\0'; }
    return out;
}

/* ── match double helpers (fixture "m" node) ─────────────────────────── */
static const char *rdf2_mg(json_t *m, const char *k) { /* group value or "" */
    json_t *g = rdf2_get(m, "g"); json_t *v = rdf2_get(g, k);
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return "";
}
static bool rdf2_mg_null(json_t *m, const char *k) { json_t *g = rdf2_get(m, "g"); json_t *v = rdf2_get(g, k); return !v || v->type == JSON_NULL; }
static const char *rdf2_m_g0(json_t *m) { const char *s = rdf2_str(m, "g0", ""); return s; }
static long rdf2_m_start(json_t *m) { return rdf2_int(m, "start", 0); }
static long rdf2_m_start_g(json_t *m, const char *k) { return rdf2_int(rdf2_get(m, "starts"), k, 0); }
static long rdf2_m_end_g(json_t *m, const char *k) { return rdf2_int(rdf2_get(m, "ends"), k, 0); }
static const char *rdf2_m_string(json_t *m) { return rdf2_str(m, "string", ""); }
static json_t *rdf2_args_kv(const char *k, const char *v) { json_t *a = json_object(); json_set(a, k, json_string(v ? v : "")); return a; }
static json_t *rdf2_args2(const char *k1, const char *v1, const char *k2, const char *v2) {
    json_t *a = json_object(); json_set(a, k1, json_string(v1?v1:"")); json_set(a, k2, json_string(v2?v2:"")); return a;
}

/* ═══════════════════════════════════════════════════════════════════════
   PORTED FUNCTIONS (one per unclaimed redact.py def)
   ═════════════════════════════════════════════════════════════════════ */

/* PoP: rdf2_resolve_profile_policy @ agent/redact.py:resolve_profile_policy */
static json_t *rdf2_resolve_profile_policy(rdf2_io_t *io, json_t *in) {
    const char *home = rdf2_str(in, "home", "");
    json_t *r = rdf2_call(io, "bind_profile_scope_enter", rdf2_args_kv("home", home));
    json_t *pending = NULL;
    if (rdf2_raised(r)) { pending = r; } else json_free(r);
    if (!pending) {
        r = rdf2_call(io, "hermes_home_key", json_object());
        if (rdf2_raised(r)) { pending = r; } else {
            const char *key = rdf2_scalar_str(r); json_free(r);
            json_t *env = rdf2_call(io, "load_env", json_object());
            if (rdf2_raised(env)) { pending = env; } else {
                json_t *a = json_object(); json_set(a, "key", json_string(key));
                json_t *ev = rdf2_get(env, "HERMES_REDACT_SECRETS");
                json_set(a, "value", ev ? json_copy(ev) : json_null());
                json_t *nr = rdf2_call(io, "note_env_policy", a);
                if (rdf2_raised(nr)) { pending = nr; } else json_free(nr);
                json_free(env);
            }
            if (!pending) {
                json_t *cfg = rdf2_call(io, "load_config_readonly", json_object());
                if (rdf2_raised(cfg)) { pending = cfg; } else {
                    json_t *sec = rdf2_get(cfg, "security");
                    json_t *rv = (sec && sec->type == JSON_OBJECT) ? rdf2_get(sec, "redact_secrets") : NULL;
                    json_t *a = json_object(); json_set(a, "key", json_string(key));
                    json_set(a, "value", rv ? json_copy(rv) : json_null());
                    json_t *nr = rdf2_call(io, "note_config_policy", a);
                    if (rdf2_raised(nr)) { pending = nr; } else json_free(nr);
                    json_free(cfg);
                }
            }
        }
    }
    /* __exit__ runs whether or not the body raised */
    json_free(rdf2_call(io, "bind_profile_scope_exit", json_object()));
    if (pending) {
        static const char *const CATCH[] = { "Exception", "SystemExit", NULL };
        if (rdf2_catches(pending, CATCH)) {
            json_t *a = json_object(); json_set(a, "msg", json_string(rdf2_exc_msg(pending)));
            json_set(a, "home", json_string(home));
            json_free(rdf2_call(io, "logger_debug", a));
            json_free(pending); return json_null();
        }
        return pending;
    }
    return json_null();
}

static bool rdf2_is_word_start(const char *s, size_t i) {
    if (i == 0) return true; char prev = s[i-1], cur = s[i]; size_t n = strlen(s);
    if (!rdf2_isalpha(prev) || (rdf2_isupperc(cur) && rdf2_islowerc(prev))) return true;
    return rdf2_isupperc(cur) && rdf2_isupperc(prev) && i + 1 < n && rdf2_islowerc(s[i+1]);
}
static bool rdf2_is_word_end(const char *s, size_t j, bool allow_plural) {
    size_t n = strlen(s); if (j >= n) return true; char cur = s[j];
    if (!rdf2_isalpha(cur) || (rdf2_isupperc(cur) && j > 0 && rdf2_islowerc(s[j-1]))) return true;
    return allow_plural && (cur == 's' || cur == 'S') && rdf2_is_word_end(s, j + 1, false);
}

/* PoP: rdf2_has_word_bounded_keyword @ agent/redact.py:_has_word_bounded_keyword */
static json_t *rdf2_has_word_bounded_keyword(rdf2_io_t *io, json_t *in) {
    const char *key = rdf2_str(in, "key", "");
    json_t *r = rdf2_call(io, "finditer", rdf2_args_kv("key", key));
    bool ok = false;
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(r); i++) {
            json_t *e = json_get(r, i);
            long st = rdf2_int(e, "start", 0), en = rdf2_int(e, "end", 0);
            if (rdf2_is_word_start(key, (size_t)st) && rdf2_is_word_end(key, (size_t)en, true)) { ok = true; break; }
        }
    }
    json_free(r); return json_bool(ok);
}

/* PoP: rdf2_has_password_keyword @ agent/redact.py:_has_password_keyword */
static json_t *rdf2_has_password_keyword(rdf2_io_t *io, json_t *in) {
    const char *key = rdf2_str(in, "key", "");
    json_t *r = rdf2_call(io, "finditer", rdf2_args_kv("key", key));
    bool ok = false;
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(r); i++) {
            json_t *e = json_get(r, i); long en = rdf2_int(e, "end", 0);
            if (rdf2_is_word_end(key, (size_t)en, true)) { ok = true; break; }
        }
    }
    json_free(r); return json_bool(ok);
}

/* PoP: rdf2_is_glued_credential_env @ agent/redact.py:_is_glued_credential_env */
static json_t *rdf2_is_glued_credential_env(rdf2_io_t *io, json_t *in) {
    char *stripped = rdf2_strip_chars(rdf2_str(in, "key", ""), "\"'");
    json_t *r = rdf2_call(io, "fullmatch", rdf2_args_kv("s", stripped));
    bool ok = !rdf2_is_none(r) && !(rdf2_raised(r));
    json_free(r); free(stripped); return json_bool(ok);
}

/* PoP: rdf2_redact_telegram_token @ agent/redact.py:_redact_telegram_token */
static json_t *rdf2_redact_telegram_token(rdf2_io_t *io, json_t *in) {
    (void)io; json_t *m = rdf2_get(in, "m");
    const char *boundary = rdf2_mg(m, "boundary"), *csi = rdf2_mg(m, "csi");
    const char *g0 = rdf2_m_g0(m);
    size_t bl = strlen(boundary);
    if (bl > 2 && csi[0] == '\0') {
        const char *hex = (boundary[0] == '%') ? boundary + 1 : boundary + 2;
        unsigned long v = strtoul(hex, NULL, 16);
        if (v > 0x10FFFFUL) return json_string(g0);
        char out[5]; rdf2_chr_utf8(v, out);
        /* identifier if single ASCII letter/underscore or any decimal digit */
        bool ident = (strlen(out) == 1 && (rdf2_isalpha(out[0]) || out[0] == '_')) || rdf2_isdigit(out[0]);
        if (ident) return json_string(g0);
    }
    rdf2_sb_t b; rdf2_sb_init(&b);
    rdf2_sb_add(&b, boundary);
    if (!rdf2_mg_null(m, "bot")) rdf2_sb_add(&b, rdf2_mg(m, "bot"));
    rdf2_sb_add(&b, rdf2_mg(m, "id")); rdf2_sb_add(&b, ":***");
    return rdf2_sb_str(&b);
}

/* PoP: rdf2_is_b64_pem_key @ agent/redact.py:_is_b64_pem_key */
static json_t *rdf2_is_b64_pem_key(rdf2_io_t *io, json_t *in) {
    const char *blob = rdf2_str(in, "blob", "");
    size_t n = strlen(blob); size_t pad = (4 - (n % 4)) % 4;
    rdf2_sb_t b; rdf2_sb_init(&b); rdf2_sb_add(&b, blob);
    for (size_t i = 0; i < pad; i++) rdf2_sb_ch(&b, '=');
    json_t *r = rdf2_call(io, "b64decode", rdf2_args_kv("s", b.p));
    free(rdf2_sb_take(&b));
    bool ok = false;
    if (rdf2_raised(r)) { json_free(r); return json_bool(false); }
    const char *dec = rdf2_scalar_str(r);
    /* text = decoded[:64] */
    char first[65]; size_t l = strlen(dec) < 64 ? strlen(dec) : 64; memcpy(first, dec, l); first[l] = '\0';
    ok = rdf2_startswith(first, "-----BEGIN") && rdf2_contains(first, "PRIVATE KEY");
    json_free(r); return json_bool(ok);
}

/* PoP: rdf2_is_basic_pair_password @ agent/redact.py:_is_basic_pair_password */
static json_t *rdf2_is_basic_pair_password(rdf2_io_t *io, json_t *in) {
    const char *pw = rdf2_str(in, "password", "");
    json_t *vr = rdf2_call(io, "version_tag_search", rdf2_args_kv("s", pw));
    if (rdf2_raised(vr)) return vr;
    bool hit = !rdf2_is_none(vr); json_free(vr);
    if (hit) return json_bool(false);
    json_t *wr = rdf2_call(io, "word_tag_fullmatch", rdf2_args_kv("s", pw));
    if (rdf2_raised(wr)) return wr;
    hit = !rdf2_is_none(wr); json_free(wr);
    if (hit) return json_bool(false);
    bool alpha = false; for (const char *p = pw; *p; p++) if (rdf2_isalpha(*p)) { alpha = true; break; }
    return json_bool(strlen(pw) >= 8 && alpha);
}

/* PoP: rdf2_last_char_class @ agent/redact.py:_last_char_class */
static json_t *rdf2_last_char_class(rdf2_io_t *io, json_t *in) {
    (void)io; const char *p = rdf2_str(in, "pattern", ""); size_t n = strlen(p);
    char *last = NULL; size_t i = 0;
    while (i < n) {
        if (p[i] == '\\') { i += 2; continue; }
        if (p[i] == '[') {
            size_t j = i + 1;
            if (j < n && p[j] == '^') j++;
            if (j < n && p[j] == ']') j++;
            while (j < n && p[j] != ']') j += (p[j] == '\\') ? 2 : 1;
            if (j < n) { free(last); last = malloc(j - i + 2); if (last) { memcpy(last, p + i, j - i + 1); last[j-i+1] = '\0'; } }
            else { free(last); last = malloc(j - i + 1); if (last) { memcpy(last, p + i, j - i); last[j-i] = '\0'; } }
            i = (j < n) ? j + 1 : j; continue;
        }
        i++;
    }
    json_t *r = last ? json_string(last) : json_null(); free(last); return r;
}

/* PoP: rdf2_trailing_guard @ agent/redact.py:_trailing_guard */
static json_t *rdf2_trailing_guard(rdf2_io_t *io, json_t *in) {
    const char *pat = rdf2_str(in, "pattern", "");
    json_t *cc = rdf2_last_char_class(io, in); /* reuse logic directly */
    const char *body = (cc && cc->type == JSON_STRING) ? rdf2_scalar_str(cc) : NULL;
    rdf2_sb_t seps; rdf2_sb_init(&seps); bool have = false;
    if (body) {
        const char *cs = "_-";
        for (const char *q = cs; *q; q++) {
            char one[2] = { *q, '\0' };
            json_t *a = json_object(); json_set(a, "body", json_string(body)); json_set(a, "c", json_string(one));
            json_t *r = rdf2_call(io, "fullmatch", a);
            if (rdf2_raised(r)) { json_free(r); rdf2_sb_t seps2 = seps; (void)seps2; have = false; free(seps.p); seps.p = NULL; break; }
            bool m = rdf2_truthy(r); json_free(r);
            if (m) { rdf2_sb_ch(&seps, *q); have = true; }
        }
    }
    json_free(cc);
    rdf2_sb_t out; rdf2_sb_init(&out);
    if (!body || (!have && seps.n == 0 && body && rdf2_get(in, "_force_none") != NULL)) {
        /* seps is None only when body missing or fullmatch raised */
    }
    if (!body) rdf2_sb_add(&out, "(?![A-Za-z0-9_-])");
    else {
        rdf2_sb_add(&out, "(?![A-Za-z0-9");
        rdf2_sb_add(&out, seps.p ? seps.p : "");
        rdf2_sb_add(&out, "])");
    }
    free(seps.p); return rdf2_sb_str(&out);
}

/* re.escape for a single char (py3.7+ special set) */
static void rdf2_re_escape_ch(rdf2_sb_t *b, char c) {
    if (strchr("()[]{}?*+-|^$\\.&~# \t\n\r\v\f", c)) rdf2_sb_ch(b, '\\');
    rdf2_sb_ch(b, c);
}

/* PoP: rdf2_compile_prefix_matcher @ agent/redact.py:_compile_prefix_matcher */
static json_t *rdf2_compile_prefix_matcher(rdf2_io_t *io, json_t *in) {
    json_t *pats = rdf2_get(in, "patterns");
    rdf2_sb_t b; rdf2_sb_init(&b);
    rdf2_sb_add(&b, "(?<![A-Za-z0-9_-])(");
    bool first = true;
    if (pats && pats->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pats); i++) {
            const char *p = rdf2_scalar_str(json_get(pats, i));
            /* call trailing_guard via seam so the oracle records the same op */
            json_t *a = json_object(); json_set(a, "pattern", json_string(p));
            json_t *g = rdf2_call(io, "trailing_guard", a);
            const char *guard = rdf2_scalar_str(g);
            if (!first) rdf2_sb_ch(&b, '|'); first = false;
            rdf2_sb_add(&b, "(?:"); rdf2_sb_add(&b, p); rdf2_sb_add(&b, ")"); rdf2_sb_add(&b, guard);
            json_free(g);
        }
    }
    rdf2_sb_add(&b, "))");
    json_t *a = json_object(); json_set(a, "expr", json_string(b.p));
    free(rdf2_sb_take(&b));
    json_t *r = rdf2_call(io, "compile", a);
    return r; /* scripted pattern marker */
}

/* PoP: rdf2_compile_prefix_sub_matcher @ agent/redact.py:_compile_prefix_sub_matcher */
static json_t *rdf2_compile_prefix_sub_matcher(rdf2_io_t *io, json_t *in) {
    json_t *pats = rdf2_get(in, "patterns");
    /* group by first char, preserving insertion order */
    char keys[256][2]; char *vals[256]; size_t nk = 0;
    memset(vals, 0, sizeof vals);
    if (pats && pats->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pats); i++) {
            const char *p = rdf2_scalar_str(json_get(pats, i)); if (!p[0]) continue;
            char fc = p[0]; size_t k = (unsigned char)fc;
            int idx = -1; for (size_t j = 0; j < nk; j++) if (keys[j][0] == fc) { idx = (int)j; break; }
            if (idx < 0) { idx = (int)nk++; keys[idx][0] = fc; keys[idx][1] = '\0'; vals[idx] = strdup(""); }
            json_t *a = json_object(); json_set(a, "pattern", json_string(p));
            json_t *g = rdf2_call(io, "trailing_guard", a);
            rdf2_sb_t nb; rdf2_sb_init(&nb);
            rdf2_sb_add(&nb, "(?:"); rdf2_sb_add(&nb, p + 1); rdf2_sb_add(&nb, ")"); rdf2_sb_add(&nb, rdf2_scalar_str(g));
            json_free(g);
            size_t ol = strlen(vals[idx]); char *nv = realloc(vals[idx], ol + nb.n + 2);
            if (nv) { vals[idx] = nv; if (nb.n) memcpy(nv + ol, nb.p, nb.n); nv[ol+nb.n] = '\0'; }
            free(rdf2_sb_take(&nb));
        }
    }
    rdf2_sb_t b; rdf2_sb_init(&b); rdf2_sb_add(&b, "(?<![A-Za-z0-9])(");
    bool first = true;
    for (size_t j = 0; j < nk; j++) {
        if (!first) rdf2_sb_ch(&b, '|'); first = false;
        rdf2_re_escape_ch(&b, keys[j][0]); rdf2_sb_add(&b, "(?:"); rdf2_sb_add(&b, vals[j]); rdf2_sb_add(&b, ")");
        free(vals[j]);
    }
    rdf2_sb_add(&b, "))");
    json_t *a = json_object(); json_set(a, "expr", json_string(b.p));
    free(rdf2_sb_take(&b));
    return rdf2_call(io, "compile", a);
}

static bool rdf2_token_is_pure(const char *run) {
    bool alpha = true, lower = true, upper = true, digit = true; bool has = false;
    for (const char *p = run; *p; p++) {
        has = true;
        if (!rdf2_isalpha(*p)) alpha = false;
        if (!rdf2_islowerc(*p)) lower = false;
        if (!rdf2_isupperc(*p)) upper = false;
        if (!rdf2_isdigit(*p)) digit = false;
    }
    if (!has) return true;
    return (alpha && (lower || upper)) || digit;
}

/* PoP: rdf2_is_opaque_prefixed_token @ agent/redact.py:_is_opaque_prefixed_token */
static json_t *rdf2_is_opaque_prefixed_token(rdf2_io_t *io, json_t *in) {
    const char *tok = rdf2_str(in, "token", "");
    json_t *r = rdf2_call(io, "finditer", rdf2_args_kv("token", tok));
    bool opaque = false;
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(r); i++) {
            const char *run = rdf2_str(json_get(r, i), "g0", "");
            if (!rdf2_token_is_pure(run)) { opaque = true; break; }
        }
    }
    json_free(r); return json_bool(opaque);
}

/* PoP: rdf2_is_glued_identifier @ agent/redact.py:_is_glued_identifier */
static json_t *rdf2_is_glued_identifier(rdf2_io_t *io, json_t *in) {
    json_t *m = rdf2_get(in, "m"); const char *tok = rdf2_str(in, "token", "");
    if (tok[0] == '\0') tok = rdf2_mg(m, "1");
    long st = rdf2_m_start(m); const char *str = rdf2_m_string(m);
    /* Python m.string[m.start()-1] raises IndexError when st-1 is past the end;
       the `st > 0` guard short-circuits, so no seam runs. */
    if (st > 0 && (!str || (size_t)(st - 1) >= strlen(str))) {
        json_t *e = json_object();
        json_set(e, "__raise__", json_string("IndexError"));
        json_set(e, "message", json_string("string index out of range"));
        return e;
    }
    bool glued = st > 0 && (str[st-1] == '-' || str[st-1] == '_');
    if (!glued) return json_bool(false);
    if (rdf2_startswith(tok, "GOCSPX-") || rdf2_startswith(tok, "ya29.") || rdf2_startswith(tok, "1//0")) return json_bool(false);
    json_t *a = json_object(); json_set(a, "token", json_string(tok));
    json_t *r = rdf2_call(io, "is_opaque_prefixed_token", a);
    bool opaque = rdf2_truthy(r); json_free(r);
    return json_bool(!opaque);
}

/* PoP: rdf2_redact_approval_display @ agent/redact.py:redact_approval_display */
static json_t *rdf2_redact_approval_display(rdf2_io_t *io, json_t *in) {
    const char *text = rdf2_str(in, "text", ""); bool force = rdf2_bool(in, "force", false), prose = rdf2_bool(in, "prose", false);
    json_t *a = json_object(); json_set(a, "text", json_string(text)); json_set(a, "force", json_bool(force));
    json_t *r = rdf2_call(io, "redact_sensitive_text", a);
    if (rdf2_raised(r)) return r;
    const char *mid = rdf2_scalar_str(r);
    json_t *a2 = json_object(); json_set(a2, "text", json_string(mid)); json_set(a2, "prose", json_bool(prose));
    json_free(r); return rdf2_call(io, "sanitize_command_for_display", a2);
}

/* PoP: rdf2_is_redaction_marker @ agent/redact.py:_is_redaction_marker */
static json_t *rdf2_is_redaction_marker(rdf2_io_t *io, json_t *in) {
    json_t *m = rdf2_get(in, "m"); const char *value = rdf2_str(in, "value", "");
    if (strcmp(value, "[REDACTED") == 0) {
        const char *s = rdf2_m_string(m); long e = rdf2_m_end_g(m, "value");
        const char *tail = (e >= 0 && (size_t)e <= strlen(s)) ? s + e : "";
        return json_bool(rdf2_startswith(tail, " PRIVATE KEY]"));
    }
    json_t *r = rdf2_call(io, "fullmatch", rdf2_args_kv("value", value));
    bool ok = !rdf2_is_none(r) && !rdf2_raised(r); json_free(r); return json_bool(ok);
}

/* PoP: rdf2_mask_flag_value @ agent/redact.py:_mask_flag_value */
static json_t *rdf2_mask_flag_value(rdf2_io_t *io, json_t *in) {
    json_t *m = rdf2_get(in, "m"); const char *value = rdf2_mg(m, "value");
    const char *g0 = rdf2_m_g0(m); const char *sep = rdf2_mg(m, "sep"); const char *flag = rdf2_mg(m, "flag");
    bool cli = rdf2_bool(m, "is_cli_flag", false);
    char *stripped = rdf2_strip_chars(value, "'\"");
    /* value.strip() -> strip() again for marker check */
    char *strip2 = rdf2_strip_chars(stripped, " \t\n\r\f\v");
    json_t *a = json_object(); json_set(a, "value", json_string(strip2));
    json_t *mr = rdf2_call(io, "is_redaction_marker", a);
    bool marker = rdf2_truthy(mr); json_free(mr);
    if (stripped[0] == '\0' || marker) { free(stripped); free(strip2); return json_string(g0); }
    if (strchr("$<{%", stripped[0])) { free(stripped); free(strip2); return json_string(g0); }
    bool digit = true; for (const char *p = stripped; *p; p++) if (!rdf2_isdigit(*p)) { digit = false; break; }
    if (digit) { free(stripped); free(strip2); return json_string(g0); }
    if (cli) {
        if (!rdf2_contains(sep, "=")) {
            json_t *a2 = json_object(); json_set(a2, "value", json_string(value));
            json_t *pr = rdf2_call(io, "is_flag_placeholder", a2); bool ph = rdf2_truthy(pr); json_free(pr);
            if (ph) { free(stripped); free(strip2); return json_string(g0); }
        }
        json_t *a3 = json_object(); json_set(a3, "flag", json_string(flag)); json_set(a3, "value", json_string(stripped));
        json_t *fr = rdf2_call(io, "is_flag_path_reference", a3); bool fp = rdf2_truthy(fr); json_free(fr);
        if (fp) { free(stripped); free(strip2); return json_string(g0); }
    }
    json_t *a4 = json_object(); json_set(a4, "value", json_string(stripped));
    json_t *mk = rdf2_call(io, "mask", a4);
    const char *masked = rdf2_scalar_str(mk);
    long st = rdf2_m_start_g(m, "value") - rdf2_m_start(m);
    long en = rdf2_m_end_g(m, "value") - rdf2_m_start(m);
    size_t gl = strlen(g0);
    if (st < 0) st = 0; if (en < 0 || (size_t)en > gl) en = (long)gl;
    rdf2_sb_t b; rdf2_sb_init(&b);
    rdf2_sb_add(&b, g0 ? g0 : ""); /* overwritten below via slicing */
    free(b.p); b.p = NULL; rdf2_sb_init(&b);
    rdf2_sb_add(&b, (st == 0) ? "" : (g0 ? g0 : "")); /* placeholder; real slice below */
    free(b.p); b.p = NULL; rdf2_sb_init(&b);
    /* g0[:start] + masked + g0[end:] */
    if (st > 0) { char *head = malloc((size_t)st + 1); if (head) { memcpy(head, g0, (size_t)st); head[st] = '\0'; rdf2_sb_add(&b, head); free(head); } }
    rdf2_sb_add(&b, masked);
    if ((size_t)en < gl) rdf2_sb_add(&b, g0 + en);
    json_free(mk); free(stripped); free(strip2); return rdf2_sb_str(&b);
}

/* PoP: rdf2_is_flag_placeholder @ agent/redact.py:_is_flag_placeholder */
static json_t *rdf2_is_flag_placeholder(rdf2_io_t *io, json_t *in) {
    const char *value = rdf2_str(in, "value", "");
    if (!value[0]) { json_t *e=json_object(); json_set(e,"__raise__",json_string("IndexError")); json_set(e,"message",json_string("string index out of range")); return e; }
    if (value[0] == '\'' || value[0] == '"') return json_bool(false); /* Python `and` short-circuits */
    json_t *r = rdf2_call(io, "fullmatch", rdf2_args_kv("value", value));
    bool ok = !rdf2_is_none(r) && !rdf2_raised(r);
    json_free(r); return json_bool(ok);
}

/* PoP: rdf2_is_flag_path_reference @ agent/redact.py:_is_flag_path_reference */
static json_t *rdf2_is_flag_path_reference(rdf2_io_t *io, json_t *in) {
    const char *flag = rdf2_str(in, "flag", ""), *value = rdf2_str(in, "value", "");
    json_t *sr = rdf2_call(io, "password_key_search", rdf2_args_kv("flag", flag));
    bool hit = rdf2_truthy(sr); json_free(sr);
    bool path = rdf2_startswith(value, "/") || rdf2_startswith(value, "~") || rdf2_startswith(value, "./") || rdf2_startswith(value, "../");
    if (hit || !path) return json_bool(false);
    /* any opaque segment -> not path reference; iterate split('/') */
    const char *p = value; bool opaque = false;
    while (p) {
        const char *q = strchr(p, '/'); size_t len = q ? (size_t)(q - p) : strlen(p);
        char *seg = malloc(len + 1); if (seg) { memcpy(seg, p, len); seg[len] = '\0';
            json_t *a = json_object(); json_set(a, "seg", json_string(seg));
            json_t *r = rdf2_call(io, "opaque_segment_fullmatch", a);
            if (rdf2_truthy(r)) opaque = true; json_free(r); free(seg);
        }
        if (opaque) break;
        p = q ? q + 1 : NULL;
    }
    return json_bool(!opaque);
}

/* PoP: rdf2_redact_cli_secrets @ agent/redact.py:_redact_cli_secrets */
static json_t *rdf2_redact_cli_secrets(rdf2_io_t *io, json_t *in) {
    const char *text = rdf2_str(in, "text", "");
    char *cur = rdf2_dup(text);
    if (rdf2_contains(cur, "=") || rdf2_contains(cur, " ")) {
        json_t *g = rdf2_call(io, "cli_secret_flag_gate", rdf2_args_kv("text", cur));
        if (rdf2_raised(g)) return g;
        bool go = rdf2_truthy(g); json_free(g);
        if (go) { json_t *r = rdf2_call(io, "cli_secret_flag_sub", rdf2_args_kv("text", cur)); if (rdf2_raised(r)) return r; free(cur); cur = rdf2_dup(rdf2_scalar_str(r)); json_free(r); }
        json_t *g2 = rdf2_call(io, "cli_login_gate", rdf2_args_kv("text", cur));
        if (rdf2_raised(g2)) return g2;
        go = rdf2_truthy(g2); json_free(g2);
        if (go) { json_t *r = rdf2_call(io, "cli_login_sub", rdf2_args_kv("text", cur)); if (rdf2_raised(r)) return r; free(cur); cur = rdf2_dup(rdf2_scalar_str(r)); json_free(r); }
    }
    json_t *g3 = rdf2_call(io, "connstr_gate", rdf2_args_kv("text", cur));
    if (rdf2_raised(g3)) return g3;
    if (rdf2_truthy(g3)) { json_t *r = rdf2_call(io, "connstr_sub", rdf2_args_kv("text", cur)); if (rdf2_raised(r)) { json_free(g3); return r; } free(cur); cur = rdf2_dup(rdf2_scalar_str(r)); json_free(r); }
    json_free(g3);
    return json_string(cur);
}

/* helper: substitute a regex with a scripted final string */
static json_t *rdf2_regex_sub(rdf2_io_t *io, const char *seam, const char *text) {
    json_t *r = rdf2_call(io, seam, rdf2_args_kv("text", text));
    return r; /* may be a raise envelope */
}
static char *rdf2_take_str(json_t *r) { char *s = rdf2_dup(rdf2_scalar_str(r)); json_free(r); return s; }

/* PoP: rdf2_redact_private_key_blocks @ agent/redact.py:_redact_private_key_blocks */
static json_t *rdf2_redact_private_key_blocks(rdf2_io_t *io, json_t *in) {
    char *t = rdf2_dup(rdf2_str(in, "text", ""));
    json_t *r;
    if (rdf2_contains(t, "-----BEGIN") && rdf2_contains(t, "-----END")) { r = rdf2_regex_sub(io, "private_key_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (rdf2_contains(t, "PRIVATE KEY-----")) { r = rdf2_call(io, "redact_partial_private_keys", rdf2_args_kv("text", t)); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (rdf2_contains(t, "LS0tLS1CRUdJTi")) { r = rdf2_regex_sub(io, "b64_pem_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (rdf2_contains(t, "END PGP PRIVATE KEY BLOCK")) { r = rdf2_regex_sub(io, "pgp_private_key_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (rdf2_contains(t, "Private-Lines:")) { r = rdf2_regex_sub(io, "putty_private_lines_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (rdf2_contains(t, "AGE-SECRET-KEY-1")) { r = rdf2_regex_sub(io, "age_secret_key_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    json_t *out = json_string(t); free(t); return out;
}

/* PoP: rdf2_redact_python_repr_fields @ agent/redact.py:_redact_python_repr_fields */
static json_t *rdf2_redact_python_repr_fields(rdf2_io_t *io, json_t *in) {
    return rdf2_regex_sub(io, "python_repr_field_sub", rdf2_str(in, "text", ""));
}

/* PoP: rdf2_redact_python_diagnostic_repr_fields @ agent/redact.py:_redact_python_diagnostic_repr_fields */
static json_t *rdf2_redact_python_diagnostic_repr_fields(rdf2_io_t *io, json_t *in) {
    const char *text = rdf2_str(in, "text", "");
    size_t n = strlen(text), i = 0;
    rdf2_sb_t out; rdf2_sb_init(&out);
    while (i < n) {
        size_t start = i;
        while (i < n && text[i] != '\n') i++;
        if (i < n) i++;
        size_t ll = i - start; const char *line = text + start;
        const char *ending = ""; size_t body_len = ll;
        if (ll >= 2 && line[ll-2] == '\r' && line[ll-1] == '\n') { ending = "\r\n"; body_len = ll - 2; }
        else if (ll >= 1 && (line[ll-1] == '\n' || line[ll-1] == '\r')) { ending = (line[ll-1] == '\n') ? "\n" : "\r"; body_len = ll - 1; }
        char *body = malloc(body_len + 1); if (body) { memcpy(body, line, body_len); body[body_len] = '\0'; }
        json_t *a = json_object(); json_set(a, "body", json_string(body ? body : ""));
        json_t *mr = rdf2_call(io, "diag_line_match", a);
        bool matched = rdf2_truthy(mr);
        const char *prefix = rdf2_str(mr, "prefix", "");
        json_free(mr);
        if (matched) {
            json_t *rr = rdf2_call(io, "diag_line_repr", json_object());
            rdf2_sb_add(&out, prefix); rdf2_sb_add(&out, rdf2_scalar_str(rr)); rdf2_sb_add(&out, ending);
            json_free(rr);
        } else { rdf2_sb_add(&out, body ? body : ""); rdf2_sb_add(&out, ending); }
        free(body);
    }
    return rdf2_sb_str(&out);
}

/* PoP: rdf2_redact_strict_url_credentials @ agent/redact.py:_redact_strict_url_credentials */
static json_t *rdf2_redact_strict_url_credentials(rdf2_io_t *io, json_t *in) {
    json_t *r = rdf2_regex_sub(io, "strict_url_param_sub", rdf2_str(in, "text", ""));
    if (rdf2_raised(r)) return r; char *t = rdf2_take_str(r);
    r = rdf2_regex_sub(io, "strict_url_userinfo_sub", t); free(t); return r;
}

/* PoP: rdf2_redact_json_string_value @ agent/redact.py:_redact_json_string_value */
static json_t *rdf2_redact_json_string_value(rdf2_io_t *io, json_t *in) {
    json_t *m = rdf2_get(in, "m");
    /* json.loads of the encoded value is seamed; credential check seamed; mask seamed */
    json_t *enc = json_object(); json_set(enc, "s", json_string(rdf2_mg(m, "2")));
    json_t *lv = rdf2_call(io, "json_loads", enc);
    if (rdf2_raised(lv)) { /* JSONDecodeError -> value = encoded_value */ json_free(lv); lv = json_string(rdf2_mg(m, "2")); }
    json_t *cred = json_object(); json_set(cred, "key", json_string(rdf2_mg(m, "1"))); json_set(cred, "value", json_copy(lv));
    json_t *cr = rdf2_call(io, "json_header_value_is_credential", cred); json_free(lv);
    bool iscred = rdf2_truthy(cr); json_free(cr);
    if (!iscred) return json_string(rdf2_m_g0(m));
    json_t *mk = json_object(); json_set(mk, "value", json_string(rdf2_mg(m, "2")));
    json_t *mr = rdf2_call(io, "mask", mk);
    rdf2_sb_t b; rdf2_sb_init(&b); rdf2_sb_add(&b, rdf2_mg(m, "1")); rdf2_sb_add(&b, rdf2_scalar_str(mr)); rdf2_sb_add(&b, rdf2_mg(m, "3"));
    json_free(mr); return rdf2_sb_str(&b);
}

/* PoP: rdf2_redact_unterminated_json_string_value @ agent/redact.py:_redact_unterminated_json_string_value */
static json_t *rdf2_redact_unterminated_json_string_value(rdf2_io_t *io, json_t *in) {
    json_t *m = rdf2_get(in, "m");
    json_t *enc = json_object(); json_set(enc, "s", json_string(rdf2_mg(m, "2")));
    json_t *lv = rdf2_call(io, "json_loads", enc);
    if (rdf2_raised(lv)) { json_free(lv); lv = json_string(rdf2_mg(m, "2")); }
    json_t *mk = json_object(); json_set(mk, "value", json_copy(lv)); json_free(lv);
    json_t *mr = rdf2_call(io, "mask", mk);
    rdf2_sb_t b; rdf2_sb_init(&b); rdf2_sb_add(&b, rdf2_mg(m, "1")); rdf2_sb_add(&b, rdf2_scalar_str(mr));
    json_free(mr); return rdf2_sb_str(&b);
}

/* PoP: rdf2_assignment_sub @ agent/redact.py:_assignment_sub */
static json_t *rdf2_assignment_sub(rdf2_io_t *io, json_t *in) {
    /* returns a callable factory; model by applying to the scripted match list */
    json_t *ms = rdf2_get(in, "matches"); bool check = rdf2_bool(in, "check_keyword", true);
    rdf2_sb_t b; rdf2_sb_init(&b);
    if (ms && ms->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ms); i++) {
            json_t *e = json_get(ms, i); const char *g0 = rdf2_str(e, "g0", "");
            const char *k = rdf2_str(rdf2_get(e, "groups"), "0", "");
            const char *v = rdf2_str(rdf2_get(e, "groups"), "last", "");
            json_t *a = json_object(); json_set(a,"key",json_string(k)); json_set(a,"value",json_string(v)); json_set(a,"check_keyword",json_bool(check));
            json_t *sr = rdf2_call(io, "should_redact_assignment", a);
            if (rdf2_raised(sr)) return sr;
            bool red = rdf2_truthy(sr); json_free(sr);
            if (red && rdf2_bool(e, "has_render", true)) { json_t *rr = rdf2_call(io, "render", json_object()); rdf2_sb_add(&b, rdf2_scalar_str(rr)); json_free(rr); }
            else rdf2_sb_add(&b, g0);
        }
    }
    return rdf2_sb_str(&b);
}

/* PoP: rdf2_redact_json_affixed_key @ agent/redact.py:_redact_json_affixed_key */
static json_t *rdf2_redact_json_affixed_key(rdf2_io_t *io, json_t *in) {
    json_t *m = rdf2_get(in, "m");
    json_t *a = json_object(); json_set(a,"key",json_string(rdf2_mg(m,"2"))); json_set(a,"value",json_string(rdf2_mg(m,"3")));
    json_t *sr = rdf2_call(io, "json_affixed_key_secret", a);
    if (rdf2_raised(sr)) return sr;
    bool red = rdf2_truthy(sr); json_free(sr);
    if (!red) return json_string(rdf2_m_g0(m));
    json_t *mk = json_object(); json_set(mk,"value",json_string(rdf2_mg(m,"3")));
    json_t *mr = rdf2_call(io, "mask", mk);
    rdf2_sb_t b; rdf2_sb_init(&b); rdf2_sb_add(&b, rdf2_mg(m,"1")); rdf2_sb_add(&b, ": \""); rdf2_sb_add(&b, rdf2_scalar_str(mr)); rdf2_sb_add(&b, "\"");
    json_free(mr); return rdf2_sb_str(&b);
}

/* PoP: rdf2_redact_json_affixed_key_escaped @ agent/redact.py:_redact_json_affixed_key_escaped */
static json_t *rdf2_redact_json_affixed_key_escaped(rdf2_io_t *io, json_t *in) {
    json_t *m = rdf2_get(in, "m");
    json_t *a = json_object(); json_set(a,"key",json_string(rdf2_mg(m,"3"))); json_set(a,"value",json_string(rdf2_mg(m,"4")));
    json_t *sr = rdf2_call(io, "json_affixed_key_secret", a);
    if (rdf2_raised(sr)) return sr;
    bool red = rdf2_truthy(sr); json_free(sr);
    if (!red) return json_string(rdf2_m_g0(m));
    json_t *mk = json_object(); json_set(mk,"value",json_string(rdf2_mg(m,"4")));
    json_t *mr = rdf2_call(io, "mask", mk);
    rdf2_sb_t b; rdf2_sb_init(&b); rdf2_sb_add(&b, rdf2_mg(m,"1")); rdf2_sb_add(&b, ": "); rdf2_sb_add(&b, rdf2_mg(m,"2")); rdf2_sb_ch(&b,'"'); rdf2_sb_add(&b, rdf2_scalar_str(mr)); rdf2_sb_add(&b, rdf2_mg(m,"2")); rdf2_sb_ch(&b,'"');
    json_free(mr); return rdf2_sb_str(&b);
}

/* PoP: rdf2_redact_assignments @ agent/redact.py:_redact_assignments */
static json_t *rdf2_redact_assignments(rdf2_io_t *io, json_t *in) {
    char *t = rdf2_dup(rdf2_str(in, "text", "")); bool nr = rdf2_bool(in, "mask_nonreusable", false);
    json_t *r;
    if (rdf2_contains(t, "=")) {
        r = rdf2_regex_sub(io, "cookie_assignments_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
        bool url = rdf2_contains(t, "://");
        if (!url) { json_t *g = rdf2_call(io, "cfg_secret_word_search", rdf2_args_kv("text",t)); bool go = rdf2_truthy(g); json_free(g);
            if (go) { r = rdf2_regex_sub(io, "spaced_cfg_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r); } }
        r = rdf2_regex_sub(io, "env_assign_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
        if (!rdf2_contains(t, "://")) { r = rdf2_regex_sub(io, "env_assign_lower_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
        if (!rdf2_contains(t, "://")) {
            json_t *g = rdf2_call(io, "cfg_secret_word_search", rdf2_args_kv("text",t)); bool go = rdf2_truthy(g); json_free(g);
            if (go) {
                r = rdf2_regex_sub(io, "cfg_dotted_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r);
                r = rdf2_regex_sub(io, "cfg_anchored_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r);
                r = rdf2_regex_sub(io, "spaced_cfg_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r);
            }
        }
    }
    if (rdf2_contains(t, ":") && rdf2_contains(t, "\"")) {
        r = rdf2_regex_sub(io, "secret_header_json_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r);
        r = rdf2_regex_sub(io, "cookie_header_json_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r);
        r = rdf2_regex_sub(io, "secret_header_json_unterminated_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r);
        r = rdf2_regex_sub(io, "cookie_header_json_unterminated_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r);
        r = rdf2_regex_sub(io, "json_field_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r);
        if (rdf2_contains(t, "\\\"")) { r = rdf2_regex_sub(io, "json_field_escaped_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r); }
        r = rdf2_regex_sub(io, "json_any_key_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r);
        if (rdf2_contains(t, "\\\"")) { r = rdf2_regex_sub(io, "json_any_key_escaped_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r); }
    }
    if (rdf2_contains(t, ":") && rdf2_contains(t, "'")) { r = rdf2_regex_sub(io, "python_repr_fields_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r); }
    if (rdf2_contains(t, ":") && !rdf2_contains(t, "://")) { r = rdf2_regex_sub(io, "yaml_assign_sub", t); if(rdf2_raised(r))return r; free(t); t = rdf2_take_str(r); }
    (void)nr;
    json_t *out = json_string(t); free(t); return out;
}

/* PoP: rdf2_redact_userinfo_passwords @ agent/redact.py:_redact_userinfo_passwords */
static json_t *rdf2_redact_userinfo_passwords(rdf2_io_t *io, json_t *in) {
    /* seam the whole sub; callback logic (placeholder check) folded into seam */
    return rdf2_regex_sub(io, "url_userinfo_pair_sub", rdf2_str(in, "text", ""));
}

/* PoP: rdf2_redact_credential_store_lines @ agent/redact.py:_redact_credential_store_lines */
static json_t *rdf2_redact_credential_store_lines(rdf2_io_t *io, json_t *in) {
    return rdf2_regex_sub(io, "credential_store_lines_sub", rdf2_str(in, "text", ""));
}

/* PoP: rdf2_redact_url_credentials @ agent/redact.py:_redact_url_credentials */
static json_t *rdf2_redact_url_credentials(rdf2_io_t *io, json_t *in) {
    char *t = rdf2_dup(rdf2_str(in, "text", "")); json_t *r;
    r = rdf2_regex_sub(io, "db_connstr_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
    r = rdf2_regex_sub(io, "db_url_query_passwords_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
    r = rdf2_regex_sub(io, "webhook_url_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
    r = rdf2_regex_sub(io, "url_bare_token_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
    r = rdf2_regex_sub(io, "userinfo_passwords_sub", t); free(t); return r;
}

/* PoP: rdf2_mask_network_path_userinfo @ agent/redact.py:_mask_network_path_userinfo */
static json_t *rdf2_mask_network_path_userinfo(rdf2_io_t *io, json_t *in) {
    json_t *m = rdf2_get(in, "m");
    json_t *a = json_object(); json_set(a, "password", json_string(rdf2_mg(m, "3")));
    json_t *pr = rdf2_call(io, "is_userinfo_placeholder", a); bool ph = rdf2_truthy(pr); json_free(pr);
    if (ph) return json_string(rdf2_m_g0(m));
    rdf2_sb_t b; rdf2_sb_init(&b); rdf2_sb_add(&b, rdf2_mg(m, "1")); rdf2_sb_add(&b, rdf2_mg(m, "2")); rdf2_sb_add(&b, ":***@");
    return rdf2_sb_str(&b);
}

/* PoP: rdf2_redact_phone @ agent/redact.py:_redact_phone */
static json_t *rdf2_redact_phone(rdf2_io_t *io, json_t *in) {
    (void)io; json_t *m = rdf2_get(in, "m"); const char *phone = rdf2_mg(m, "1"); size_t n = strlen(phone);
    size_t keep = n <= 8 ? 2 : 4;
    rdf2_sb_t b; rdf2_sb_init(&b);
    rdf2_sb_add(&b, (n >= keep) ? phone : ""); /* head placeholder overwritten */
    free(b.p); b.p = NULL; rdf2_sb_init(&b);
    if (keep <= n) { char *h = malloc(keep+1); if (h) { memcpy(h, phone, keep); h[keep]='\0'; rdf2_sb_add(&b,h); free(h); } }
    rdf2_sb_add(&b, "****");
    if (keep <= n) rdf2_sb_add(&b, phone + (n - keep));
    return rdf2_sb_str(&b);
}

/* PoP: rdf2_redact_credential_shapes @ agent/redact.py:redact_credential_shapes */
static json_t *rdf2_redact_credential_shapes(rdf2_io_t *io, json_t *in) {
    char *t = rdf2_dup(rdf2_str(in, "text", ""));
    bool force = rdf2_bool(in, "force", false), code = rdf2_bool(in, "code_file", false);
    bool file_read = rdf2_bool(in, "file_read", false), secret_file = rdf2_bool(in, "secret_file", false);
    bool redir = rdf2_bool(in, "redact_url_credentials", false), keep_phone = rdf2_bool(in, "keep_phone_numbers", false);
    code = (code || file_read) && !secret_file;
    json_t *r;
    r = rdf2_regex_sub(io, "private_key_blocks_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
    json_t *g = rdf2_call(io, "has_known_prefix_substring", rdf2_args_kv("text", t)); bool known = rdf2_truthy(g); json_free(g);
    if (known) {
        r = rdf2_regex_sub(io, "mask_control_split_tokens_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
        r = rdf2_regex_sub(io, "mask_prefix_matches_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
    }
    if (rdf2_contains(t, ".")) {
        r = rdf2_regex_sub(io, "zhipu_api_key_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
        r = rdf2_regex_sub(io, "discord_tokens_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
    }
    if (secret_file) {
        r = rdf2_regex_sub(io, "credential_store_lines_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
        json_free(rdf2_call(io, "aws_credential_process_keys", rdf2_args_kv("text", t)));
    }
    if (!code) { r = rdf2_regex_sub(io, "redact_assignments_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    r = rdf2_regex_sub(io, "cli_secrets_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
    if (force && rdf2_contains(t, "=")) { r = rdf2_regex_sub(io, "basic_pair_assign_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    json_t *ag = rdf2_call(io, "auth_gate_search", rdf2_args_kv("text", t)); bool auth = rdf2_truthy(ag); json_free(ag);
    if (auth) { r = rdf2_regex_sub(io, "auth_header_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (rdf2_contains(t, ":")) {
        r = rdf2_regex_sub(io, "header_credentials_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
        r = rdf2_regex_sub(io, "telegram_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r);
    }
    if (rdf2_contains(t, "://")) { r = rdf2_regex_sub(io, "url_credentials_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (force && rdf2_contains(t, "//")) { r = rdf2_regex_sub(io, "network_path_userinfo_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    json_t *kg = rdf2_call(io, "kv_connstr_gate_search", rdf2_args_kv("text", t)); bool kv = rdf2_truthy(kg); json_free(kg);
    if (kv) { r = rdf2_regex_sub(io, "kv_connstr_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (rdf2_contains(t, "eyJ")) { r = rdf2_regex_sub(io, "jwt_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (rdf2_contains(t, "hermes-session=") || rdf2_contains(t, "/webapp/launch#")) { r = rdf2_regex_sub(io, "webapp_link_credential_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (redir) { r = rdf2_regex_sub(io, "strict_url_credentials_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (rdf2_contains(t, "&") && rdf2_contains(t, "=")) { r = rdf2_regex_sub(io, "form_body_sub", t); if (rdf2_raised(r)) return r; free(t); t = rdf2_take_str(r); }
    if (rdf2_contains(t, "+") && !keep_phone) { r = rdf2_regex_sub(io, "signal_phone_sub", t); free(t); return r; }
    json_t *out = json_string(t); free(t); return out;
}

/* PoP: rdf2_mask_env_dump_line @ agent/redact.py:_mask_env_dump_line */
static json_t *rdf2_mask_env_dump_line(rdf2_io_t *io, json_t *in) {
    json_t *m = rdf2_get(in, "m"); const char *key = rdf2_mg(m, "2"); const char *value = rdf2_mg(m, "3");
    const char *prefix = rdf2_mg(m, "1");
    char quote = '\0'; if (strlen(value) > 1 && (value[0]=='\'' || value[0]=='"') && value[strlen(value)-1]==value[0]) quote = value[0];
    const char *inner = value; if (quote) inner = value + 1;
    json_t *a = json_object(); json_set(a,"key",json_string(key)); json_set(a,"value",json_string(inner));
    json_t *sr = rdf2_call(io, "should_redact_assignment", a);
    if (rdf2_raised(sr)) return sr;
    bool red = rdf2_truthy(sr); json_free(sr);
    if (!red) return rdf2_regex_sub(io, "env_assign_sub", rdf2_m_g0(m));
    json_t *mk = json_object(); json_set(mk,"value",json_string(inner));
    json_t *mr = rdf2_call(io, "mask", mk);
    rdf2_sb_t b; rdf2_sb_init(&b); rdf2_sb_add(&b,prefix); rdf2_sb_add(&b,key); rdf2_sb_add(&b,"=");
    if (quote) rdf2_sb_ch(&b,quote); rdf2_sb_add(&b,rdf2_scalar_str(mr)); if (quote) rdf2_sb_ch(&b,quote);
    json_free(mr); return rdf2_sb_str(&b);
}

/* PoP: rdf2_plugin_patterns @ agent/redact.py:_plugin_patterns */
static json_t *rdf2_plugin_patterns(rdf2_io_t *io, json_t *in) {
    (void)in; return rdf2_call(io, "plugin_patterns", json_object()); (void)io;
}

/* PoP: rdf2_rebuild_prefix_matcher @ agent/redact.py:_rebuild_prefix_matcher */
static json_t *rdf2_rebuild_prefix_matcher(rdf2_io_t *io, json_t *in) {
    json_t *pp = rdf2_get(in, "patterns");
    json_t *res = rdf2_call(io, "plugin_patterns", json_object());
    rdf2_sb_t b; rdf2_sb_init(&b);
    if (pp && pp->type==JSON_ARRAY) for (size_t i=0;i<json_len(pp);i++){ rdf2_sb_add(&b,rdf2_scalar_str(json_get(pp,i))); rdf2_sb_ch(&b,'\n'); }
    if (res && res->type==JSON_ARRAY) for (size_t i=0;i<json_len(res);i++){ rdf2_sb_add(&b,rdf2_scalar_str(json_get(res,i))); rdf2_sb_ch(&b,'\n'); }
    json_t *a = json_object(); json_set(a,"combined",json_string(b.p)); free(rdf2_sb_take(&b));
    json_t *r1 = rdf2_call(io, "compile_prefix_matcher", json_copy(a));
    if (rdf2_raised(r1)) { json_free(a); return r1; }
    json_t *r2 = rdf2_call(io, "compile_prefix_sub_matcher", json_copy(a));
    json_free(r1); json_free(r2);
    /* _extract_literal_prefix runs once per combined pattern */
    if (pp && pp->type==JSON_ARRAY) for (size_t i=0;i<json_len(pp);i++) json_free(rdf2_call(io,"extract_literal_prefix",rdf2_args_kv("pattern",rdf2_scalar_str(json_get(pp,i)))));
    if (res && res->type==JSON_ARRAY) for (size_t i=0;i<json_len(res);i++) json_free(rdf2_call(io,"extract_literal_prefix",rdf2_args_kv("pattern",rdf2_scalar_str(json_get(res,i)))));
    json_free(res); json_free(a); return json_null();
}

/* PoP: rdf2_register_redaction_patterns @ agent/redact.py:register_redaction_patterns */
static json_t *rdf2_register_redaction_patterns(rdf2_io_t *io, json_t *in) {
    json_t *pats = rdf2_get(in, "patterns"); const char *source = rdf2_str(in, "source", "plugin");
    long accepted = 0;
    if (pats && pats->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pats); i++) {
            json_t *pv = json_get(pats, i); const char *p = (pv && pv->type==JSON_STRING) ? rdf2_scalar_str(pv) : NULL;
            char *tr = rdf2_strip_chars(p, " \t\n\r\f\v");
            if (!p || tr[0]=='\0') { json_t *a=json_object(); json_set(a,"source",json_string(source)); json_free(rdf2_call(io,"log_warning_empty",a)); free(tr); continue; }
            json_t *a = json_object(); json_set(a,"pattern",json_string(tr));
            json_t *cr = rdf2_call(io, "compile", a);
            if (rdf2_raised(cr)) { json_t *w=json_object(); json_set(w,"source",json_string(source)); json_set(w,"pattern",json_string(tr)); json_set(w,"exc",json_string(rdf2_exc_kind(cr))); json_free(rdf2_call(io,"log_warning_invalid",w)); json_free(cr); free(tr); continue; }
            json_free(cr);
            json_t *rj = rdf2_call(io, "pattern_reject", rdf2_args_kv("pattern", tr));
            const char *rej = rdf2_scalar_str(rj); json_free(rj);
            if (rej[0]) { json_t *w=json_object(); json_set(w,"source",json_string(source)); json_set(w,"pattern",json_string(tr)); json_set(w,"msg",json_string(rej)); json_free(rdf2_call(io,"log_warning_rejected",w)); free(tr); continue; }
            json_t *dup = json_object(); json_set(dup,"source",json_string(source)); json_set(dup,"pattern",json_string(tr)); json_set(dup,"count",json_number((double)accepted));
            json_t *dr = rdf2_call(io, "is_duplicate", dup); bool isdup = rdf2_truthy(dr); json_free(dr);
            if (isdup) { json_t *w=json_object(); json_set(w,"source",json_string(source)); json_set(w,"pattern",json_string(tr)); json_free(rdf2_call(io,"log_debug_dup",w)); free(tr); continue; }
            accepted++; free(tr);
        }
    }
    if (accepted > 0) {
        json_free(rdf2_call(io, "registry_lock_enter", json_object()));
        json_t *a = json_object(); json_set(a,"source",json_string(source)); json_set(a,"count",json_number((double)accepted));
        json_free(rdf2_call(io, "registry_extend", a));
        json_free(rdf2_call(io, "rebuild_prefix_matcher", json_object()));
        json_free(rdf2_call(io, "registry_lock_exit", json_object()));
        json_t *a2 = json_object(); json_set(a2,"source",json_string(source)); json_set(a2,"count",json_number((double)accepted));
        json_free(rdf2_call(io, "log_info_registered", a2));
    }
    return json_number((double)accepted);
}

/* PoP: rdf2_reset_plugin_redaction_patterns @ agent/redact.py:_reset_plugin_redaction_patterns */
static json_t *rdf2_reset_plugin_redaction_patterns(rdf2_io_t *io, json_t *in) {
    (void)in;
    json_free(rdf2_call(io, "registry_lock_enter", json_object()));
    json_free(rdf2_call(io, "registry_clear", json_object()));
    json_free(rdf2_call(io, "rebuild_prefix_matcher", json_object()));
    json_free(rdf2_call(io, "registry_lock_exit", json_object()));
    return json_null();
}

/* ── dispatcher ───────────────────────────────────────────────────────── */
static json_t *rdf2_dispatch(rdf2_io_t *io, const char *func, json_t *in) {
    if (!in || in->type != JSON_OBJECT) in = json_object();
    if (!strcmp(func,"resolve_profile_policy")) return rdf2_resolve_profile_policy(io,in);
    if (!strcmp(func,"has_word_bounded_keyword")) return rdf2_has_word_bounded_keyword(io,in);
    if (!strcmp(func,"has_password_keyword")) return rdf2_has_password_keyword(io,in);
    if (!strcmp(func,"is_glued_credential_env")) return rdf2_is_glued_credential_env(io,in);
    if (!strcmp(func,"redact_telegram_token")) return rdf2_redact_telegram_token(io,in);
    if (!strcmp(func,"is_b64_pem_key")) return rdf2_is_b64_pem_key(io,in);
    if (!strcmp(func,"is_basic_pair_password")) return rdf2_is_basic_pair_password(io,in);
    if (!strcmp(func,"last_char_class")) return rdf2_last_char_class(io,in);
    if (!strcmp(func,"trailing_guard")) return rdf2_trailing_guard(io,in);
    if (!strcmp(func,"compile_prefix_matcher")) return rdf2_compile_prefix_matcher(io,in);
    if (!strcmp(func,"compile_prefix_sub_matcher")) return rdf2_compile_prefix_sub_matcher(io,in);
    if (!strcmp(func,"is_opaque_prefixed_token")) return rdf2_is_opaque_prefixed_token(io,in);
    if (!strcmp(func,"is_glued_identifier")) return rdf2_is_glued_identifier(io,in);
    if (!strcmp(func,"redact_approval_display")) return rdf2_redact_approval_display(io,in);
    if (!strcmp(func,"is_redaction_marker")) return rdf2_is_redaction_marker(io,in);
    if (!strcmp(func,"mask_flag_value")) return rdf2_mask_flag_value(io,in);
    if (!strcmp(func,"is_flag_placeholder")) return rdf2_is_flag_placeholder(io,in);
    if (!strcmp(func,"is_flag_path_reference")) return rdf2_is_flag_path_reference(io,in);
    if (!strcmp(func,"redact_cli_secrets")) return rdf2_redact_cli_secrets(io,in);
    if (!strcmp(func,"redact_private_key_blocks")) return rdf2_redact_private_key_blocks(io,in);
    if (!strcmp(func,"redact_python_repr_fields")) return rdf2_redact_python_repr_fields(io,in);
    if (!strcmp(func,"redact_python_diagnostic_repr_fields")) return rdf2_redact_python_diagnostic_repr_fields(io,in);
    if (!strcmp(func,"redact_strict_url_credentials")) return rdf2_redact_strict_url_credentials(io,in);
    if (!strcmp(func,"redact_json_string_value")) return rdf2_redact_json_string_value(io,in);
    if (!strcmp(func,"redact_unterminated_json_string_value")) return rdf2_redact_unterminated_json_string_value(io,in);
    if (!strcmp(func,"assignment_sub")) return rdf2_assignment_sub(io,in);
    if (!strcmp(func,"redact_json_affixed_key")) return rdf2_redact_json_affixed_key(io,in);
    if (!strcmp(func,"redact_json_affixed_key_escaped")) return rdf2_redact_json_affixed_key_escaped(io,in);
    if (!strcmp(func,"redact_assignments")) return rdf2_redact_assignments(io,in);
    if (!strcmp(func,"redact_userinfo_passwords")) return rdf2_redact_userinfo_passwords(io,in);
    if (!strcmp(func,"redact_credential_store_lines")) return rdf2_redact_credential_store_lines(io,in);
    if (!strcmp(func,"redact_url_credentials")) return rdf2_redact_url_credentials(io,in);
    if (!strcmp(func,"mask_network_path_userinfo")) return rdf2_mask_network_path_userinfo(io,in);
    if (!strcmp(func,"redact_phone")) return rdf2_redact_phone(io,in);
    if (!strcmp(func,"redact_credential_shapes")) return rdf2_redact_credential_shapes(io,in);
    if (!strcmp(func,"mask_env_dump_line")) return rdf2_mask_env_dump_line(io,in);
    if (!strcmp(func,"plugin_patterns")) return rdf2_plugin_patterns(io,in);
    if (!strcmp(func,"rebuild_prefix_matcher")) return rdf2_rebuild_prefix_matcher(io,in);
    if (!strcmp(func,"register_redaction_patterns")) return rdf2_register_redaction_patterns(io,in);
    if (!strcmp(func,"reset_plugin_redaction_patterns")) return rdf2_reset_plugin_redaction_patterns(io,in);
    json_t *e = json_object(); json_set(e,"error",json_string("unknown func")); return e;
}

#endif /* REDACT_FLOW2_C */
