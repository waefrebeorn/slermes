/* gw_connections_flow2.c — FLOW / leaves port of hermes_cli/gateway_connections.py
 * (the machine's saved gateway-connections registry + OAuth token envelopes).
 *
 * The strict CRLF-aware PoP grep found ZERO annotations crediting the BASE
 * module hermes_cli/gateway_connections.py. Its siblings are claimed by other
 * ports (gateway_connections_normalize.py -> gw_conn_normalize_flow2,
 * gateway_connections_ssh_remote.py -> gw_conn_ssh_flow2,
 * gateway_connections_ssh.py -> gw_conn_ssh_local_flow2); none credits this
 * module. PoP credits below name the DEFINING module, gateway_connections.py.
 *
 * The pure/seam core is ported: path-free projection + envelope helpers and the
 * OAuth URL-binding guards. I/O (store read/replace, locks, atomic write) stays
 * out; the two URL-binding leaf functions (_entry_url/_bound_url) consume an
 * INJECTED normalize_remote_base_url result via the fixture "norm" seam, so no
 * filesystem or normalize-module code lives here. Internal composition calls
 * each other REAL (_public_connection -> _plain/_token_preview, etc.).
 *
 * Exception fidelity: gco2_exc_isa walks the FULL py3.14 builtin hierarchy plus
 * the module's custom classes (ConnectionStoreError->ValueError,
 * DuplicateConnection->ConnectionStoreError, ConnectionStoreBusy->RuntimeError,
 * NoSuchConnection->KeyError, PreferencesBusy->Exception). _entry_url/_bound_url
 * catch exactly ConnectionStoreError-and-subclasses; the BaseException trio and
 * all non-ConnectionStoreError exceptions propagate. Raised exceptions ride as
 * {"__raise__": {"type","message"}}.
 *
 * Representation contract (both sides): ASCII domain; integral JSON numbers
 * are written without a decimal point (libjson has no int/float identity, so a
 * float-typed "revision" is out of scope); object key iteration preserves JSON
 * insertion order (libjson c.keys[] array), matching Python dict order for
 * headerNames/rest. str.strip() uses the Python White_Space byte set.
 *
 * Ownership: every public gco2_* function returns a NEW reference (or a raise
 * sentinel). Helpers that return malloc'd char* are caller-freed. Code-only
 * port: winlibs gcc -fsyntax-only, literal Python shadow mirror (shadow-diff 0)
 * and the differential gate.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── small value/string helpers ─────────────────────────────────────────── */
static const char *gco2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static bool gco2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool gco2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool gco2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool gco2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool gco2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool gco2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

/* Python type name for AttributeError messages (defined in seam section). */
static const char *gco2_pytype(const json_t *v);

/* Python truthiness (NOT JavaScript falsiness). */
static bool gco2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static char *gco2_fmt(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    va_list ap2; va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *out = (char *)malloc((size_t)n + 1);
    if (out) vsnprintf(out, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return out;
}
static bool gco2_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f) || c == 0x20;
}
/* Python str.strip() for the ASCII domain. */
static char *gco2_strip(const char *s) {
    size_t n = s ? strlen(s) : 0, a = 0, b = n;
    while (a < b && gco2_ws((unsigned char)s[a])) a++;
    while (b > a && gco2_ws((unsigned char)s[b - 1])) b--;
    char *o = (char *)malloc(b - a + 1);
    if (!o) return NULL;
    if (b > a) memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}
/* Python str() for the scalar values used as quarantine reasons. */
static char *gco2_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(gco2_sval(v));
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            return gco2_fmt("%.0f", v->num_val);
        return gco2_fmt("%.17g", v->num_val);
    }
    return strdup("");
}

/* ── FULL py3.14 builtin exception hierarchy + module custom classes ───── */
static const char *gco2_exc_parent(const char *t) {
    if (!t) return "Exception";
    struct P { const char *n, *p; };
    static const struct P T[] = {
        {"BaseException",NULL},{"Exception","BaseException"},
        {"GeneratorExit","BaseException"},{"KeyboardInterrupt","BaseException"},{"SystemExit","BaseException"},
        {"BaseExceptionGroup","BaseException"},{"ExceptionGroup","Exception"},
        {"ArithmeticError","Exception"},{"AssertionError","Exception"},{"AttributeError","Exception"},
        {"BufferError","Exception"},{"EOFError","Exception"},{"ImportError","Exception"},
        {"LookupError","Exception"},{"MemoryError","Exception"},{"NameError","Exception"},
        {"OSError","Exception"},{"ReferenceError","Exception"},{"RuntimeError","Exception"},
        {"StopAsyncIteration","Exception"},{"StopIteration","Exception"},{"SyntaxError","Exception"},
        {"SystemError","Exception"},{"TypeError","Exception"},{"ValueError","Exception"},
        {"Warning","Exception"},
        {"BlockingIOError","OSError"},{"BrokenPipeError","ConnectionError"},
        {"ChildProcessError","OSError"},{"ConnectionError","OSError"},
        {"ConnectionAbortedError","ConnectionError"},{"ConnectionRefusedError","ConnectionError"},
        {"ConnectionResetError","ConnectionError"},{"FileExistsError","OSError"},{"FileNotFoundError","OSError"},
        {"InterruptedError","OSError"},{"IsADirectoryError","OSError"},{"NotADirectoryError","OSError"},
        {"PermissionError","OSError"},{"ProcessLookupError","OSError"},{"TimeoutError","OSError"},
        {"EnvironmentError","OSError"},{"IOError","OSError"},{"WindowsError","OSError"},
        {"FloatingPointError","ArithmeticError"},{"OverflowError","ArithmeticError"},{"ZeroDivisionError","ArithmeticError"},
        {"IndexError","LookupError"},{"KeyError","LookupError"},
        {"UnboundLocalError","NameError"},{"ModuleNotFoundError","ImportError"},
        {"NotImplementedError","RuntimeError"},{"PythonFinalizationError","RuntimeError"},{"RecursionError","RuntimeError"},
        {"IndentationError","SyntaxError"},{"TabError","IndentationError"},{"_IncompleteInputError","SyntaxError"},
        {"UnicodeError","ValueError"},{"UnicodeDecodeError","UnicodeError"},{"UnicodeEncodeError","UnicodeError"},
        {"UnicodeTranslateError","UnicodeError"},
        {"BytesWarning","Warning"},{"DeprecationWarning","Warning"},{"EncodingWarning","Warning"},
        {"FutureWarning","Warning"},{"ImportWarning","Warning"},{"PendingDeprecationWarning","Warning"},
        {"ResourceWarning","Warning"},{"RuntimeWarning","Warning"},{"SyntaxWarning","Warning"},
        {"UnicodeWarning","Warning"},{"UserWarning","Warning"},
        /* module-local classes */
        {"ConnectionStoreError","ValueError"},{"DuplicateConnection","ConnectionStoreError"},
        {"ConnectionStoreBusy","RuntimeError"},{"NoSuchConnection","KeyError"},
        {"PreferencesBusy","Exception"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (strcmp(t, T[i].n) == 0) return T[i].p ? T[i].p : "BaseException";
    return "Exception";
}
static bool gco2_exc_isa(const char *t, const char *base) {
    if (!t || !base) return false;
    const char *cur = t;
    for (int i = 0; i < 32 && cur; i++) {
        if (strcmp(cur, base) == 0) return true;
        cur = gco2_exc_parent(cur);
    }
    return false;
}
static bool gco2_base_exc(const char *t) {
    return t && (strcmp(t, "SystemExit") == 0 || strcmp(t, "KeyboardInterrupt") == 0 ||
                 strcmp(t, "GeneratorExit") == 0);
}
static bool gco2_caught_by(const json_t *r, const char *const *bases, size_t n) {
    json_t *spec = json_obj_get(r, "__raise__");
    if (!spec || spec->type != JSON_OBJECT) return false;
    const char *t = json_get_str(spec, "type", "");
    if (gco2_base_exc(t)) return false;
    for (size_t i = 0; i < n; i++) if (gco2_exc_isa(t, bases[i])) return true;
    return false;
}

/* ── raise sentinels ────────────────────────────────────────────────────── */
static bool gco2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static json_t *gco2_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type ? type : "Exception"));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}
static json_t *gco2_raise_cs(const char *msg) { return gco2_make_raise("ConnectionStoreError", msg); }

/* ── deep equality with absent == absent == None ───────────────────────── */
static bool gco2_eq(const json_t *a, const json_t *b) {
    if (!a) a = json_null();
    if (!b) b = json_null();
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL:   return true;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(gco2_sval(a), gco2_sval(b)) == 0;
        case JSON_ARRAY: case JSON_OBJECT: {
            char *sa = json_serialize(a), *sb = json_serialize(b);
            bool eq = sa && sb && strcmp(sa, sb) == 0;
            free(sa); free(sb); return eq;
        }
    }
    return false;
}

/* ── _revision ──────────────────────────────────────────────────────────── */
static json_t *gco2_revision(const json_t *doc) {
    const json_t *v = gco2_is_obj(doc) ? json_obj_get(doc, "revision") : NULL;
    if (gco2_is_num(v) && v->num_val == (double)(long long)v->num_val)
        return json_number((double)(long long)v->num_val);
    return json_number(0);
}

/* ── _plain / _envelope / _token_preview / _kept_envelope ───────────────── */
static char *gco2_plain_str(const json_t *secret) {
    if (gco2_is_str(secret)) return strdup(gco2_sval(secret));
    if (!gco2_is_obj(secret)) return strdup("");
    const json_t *enc = json_obj_get(secret, "encoding");
    if (gco2_is_str(enc) && strcmp(gco2_sval(enc), "safeStorage") == 0) return strdup("");
    const json_t *val = json_obj_get(secret, "value");
    if (gco2_is_str(val)) return strdup(gco2_sval(val));
    return strdup("");
}
static json_t *gco2_plain(const json_t *secret) {
    char *s = gco2_plain_str(secret);
    json_t *r = json_string(s ? s : ""); free(s); return r;
}
static json_t *gco2_envelope(const json_t *value) {
    json_t *o = json_object();
    json_set(o, "encoding", json_string("plain"));
    json_set(o, "value", json_copy(value ? value : (json_t *)json_null()));
    return o;
}
static json_t *gco2_envelope_str(const char *s) {
    json_t *o = json_object();
    json_set(o, "encoding", json_string("plain"));
    json_set(o, "value", json_string(s ? s : ""));
    return o;
}
static json_t *gco2_token_preview(const json_t *value) {
    const char *v = gco2_is_str(value) ? gco2_sval(value) : "";
    if (!v[0]) return json_null();
    size_t n = strlen(v);
    if (n <= 8) return json_string("set");
    return json_string(gco2_fmt("...%s", v + (n - 6)));
}
static json_t *gco2_kept_envelope(const json_t *token, const json_t *existing) {
    bool ok = gco2_is_obj(token);
    if (ok) {
        const json_t *e = json_obj_get(token, "encoding");
        ok = gco2_is_str(e) && strcmp(gco2_sval(e), "safeStorage") == 0;
    }
    if (ok) { const json_t *v = json_obj_get(token, "value"); ok = gco2_truthy(v); }
    if (ok) ok = existing != NULL && existing->type != JSON_NULL;  /* existing is not None */
    if (ok) {
        const json_t *et = gco2_is_obj(existing) ? json_obj_get(existing, "token") : NULL;
        ok = gco2_eq(et, token);
    }
    return json_bool(ok);
}

/* ── _public_connection / public_registry ──────────────────────────────── */
static json_t *gco2_token_preview_str(const char *v);
static json_t *gco2_public_connection(const json_t *entry) {
    json_t *o = json_object();
    if (gco2_is_obj(entry)) {
        size_t n = json_object_size(entry);
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(entry, i);
            if (!k || !strcmp(k, "token") || !strcmp(k, "headers")) continue;
            json_set(o, k, json_copy(json_object_get_at(entry, i)));
        }
    }
    const json_t *tok = gco2_is_obj(entry) ? json_obj_get(entry, "token") : NULL;
    char *plain = gco2_plain_str(tok);
    json_set(o, "tokenSet", json_bool(plain && plain[0]));
    json_set(o, "tokenPreview", gco2_token_preview_str(plain));
    free(plain);
    const json_t *hdrs = gco2_is_obj(entry) ? json_obj_get(entry, "headers") : NULL;
    json_t *names = json_array();
    if (gco2_is_obj(hdrs)) {
        size_t n = json_object_size(hdrs);
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(hdrs, i);
            if (k) json_append(names, json_string(k));
        }
    }
    json_set(o, "headerNames", names);
    return o;
}
static json_t *gco2_token_preview_str(const char *v) {
    if (!v || !v[0]) return json_null();
    size_t n = strlen(v);
    if (n <= 8) return json_string("set");
    return json_string(gco2_fmt("...%s", v + (n - 6)));
}
static json_t *gco2_public_registry(const json_t *reg) {
    json_t *o = json_object();
    static const char *KEYS[] = {"version", "primary", "launchMode", "lastUsed", NULL};
    for (int i = 0; KEYS[i]; i++) {
        const json_t *v = gco2_is_obj(reg) ? json_obj_get(reg, KEYS[i]) : NULL;
        json_set(o, KEYS[i], json_copy(v ? v : (json_t *)json_null()));
    }
    json_set(o, "secureTokenStorage", json_bool(true));
    json_t *cons = json_array();
    const json_t *src = gco2_is_obj(reg) ? json_obj_get(reg, "connections") : NULL;
    if (gco2_is_arr(src))
        for (size_t i = 0; i < src->c.count; i++)
            json_append(cons, gco2_public_connection(json_get(src, i)));
    json_set(o, "connections", cons);
    json_t *qs = json_array();
    const json_t *qsrc = gco2_is_obj(reg) ? json_obj_get(reg, "quarantined") : NULL;
    if (gco2_is_arr(qsrc)) {
        for (size_t i = 0; i < qsrc->c.count; i++) {
            const json_t *q = json_get(qsrc, i);
            if (!gco2_is_obj(q)) continue;  /* `if isinstance(q, dict)` */
            json_t *qo = json_object();
            const json_t *reason = json_obj_get(q, "reason");
            char *rs = gco2_truthy(reason) ? gco2_str_of(reason) : strdup("unknown");
            json_set(qo, "reason", json_string(rs ? rs : "")); free(rs);
            const json_t *entry = json_obj_get(q, "entry");
            const char *label = "";
            if (gco2_is_obj(entry)) {
                const json_t *lv = json_obj_get(entry, "label");
                if (gco2_is_str(lv)) label = gco2_sval(lv);
            }
            json_set(qo, "label", json_string(label));
            json_append(qs, qo);
        }
    }
    json_set(o, "quarantined", qs);
    return o;
}

/* ── _incoming_headers ──────────────────────────────────────────────────── */
static json_t *gco2_incoming_headers(const json_t *raw, const json_t *existing) {
    const json_t *stored = gco2_is_obj(existing) ? existing : NULL;
    json_t *out = json_object();
    if (!gco2_is_obj(raw)) {
        char *m = gco2_fmt("'%s' object has no attribute 'items'", gco2_pytype(raw));
        json_t *r = gco2_make_raise("AttributeError", m); free(m); return r;
    }
    size_t n = json_object_size(raw);
    for (size_t i = 0; i < n; i++) {
        const char *name = json_object_get_key_at(raw, i);
        const json_t *value = json_object_get_at(raw, i);
        if (!name) continue;
        char *key = gco2_strip(name);  /* str(name or "").strip(); JSON keys are str */
        if (!key || !key[0]) { free(key); continue; }
        if (gco2_is_str(value)) {
            char *sv = gco2_strip(gco2_sval(value));
            if (sv && sv[0]) json_set(out, key, gco2_envelope_str(sv));
            free(sv);
        } else if (gco2_is_null(value)) {
            const json_t *sv = stored ? json_obj_get(stored, key) : NULL;
            if (sv) json_set(out, key, json_copy(sv));
        } else if (gco2_is_obj(value)) {
            json_set(out, key, json_copy(value));
        }
        free(key);
    }
    return out;
}

/* ── _signed_in_ids_gone ────────────────────────────────────────────────── */
static json_t *gco2_signed_in_ids_gone(const json_t *before, const json_t *after) {
    json_t *out = json_array();
    json_t *now = json_object();  /* id -> connection (last wins) */
    const json_t *ac = gco2_is_obj(after) ? json_obj_get(after, "connections") : NULL;
    if (gco2_is_arr(ac)) {
        for (size_t i = 0; i < ac->c.count; i++) {
            const json_t *c = json_get(ac, i);
            if (!gco2_is_obj(c)) continue;
            const json_t *idv = json_obj_get(c, "id");
            if (gco2_is_str(idv)) json_set(now, gco2_sval(idv), json_copy(c));
        }
    }
    const json_t *bc = gco2_is_obj(before) ? json_obj_get(before, "connections") : NULL;
    if (gco2_is_arr(bc)) {
        for (size_t i = 0; i < bc->c.count; i++) {
            const json_t *c = json_get(bc, i);
            if (!gco2_is_obj(c)) continue;
            const json_t *idv = json_obj_get(c, "id");
            if (!gco2_is_str(idv)) continue;
            const char *id = gco2_sval(idv);
            const json_t *nc = json_obj_get(now, id);
            bool gone = !nc;
            if (!gone) {
                static const char *KK[2] = {"url", "authMode"};
                for (int j = 0; j < 2 && !gone; j++)
                    if (!gco2_eq(json_obj_get(c, KK[j]),
                                 gco2_is_obj(nc) ? json_obj_get(nc, KK[j]) : NULL))
                        gone = true;
            }
            if (gone) json_append(out, json_string(id));
        }
    }
    json_free(now);
    return out;
}

/* ── _entry_url / _bound_url (seam + ConnectionStoreError catch) ────────── */
/* Python type name for AttributeError messages. */
static const char *gco2_pytype(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* Python str() of a KeyError-family exception repr()s its single string arg. */
static json_t *gco2_seam_propagate(const json_t *norm) {
    if (!gco2_raised(norm)) return json_copy(norm ? norm : (json_t *)json_null());
    json_t *spec = json_obj_get(norm, "__raise__");
    const char *t = json_get_str(spec, "type", "Exception");
    const char *msg = json_get_str(spec, "message", "");
    if (gco2_exc_isa(t, "KeyError")) {
        char *q = gco2_fmt("'%s'", msg);
        json_t *r = gco2_make_raise(t, q ? q : msg);
        free(q);
        return r;
    }
    return json_copy(norm);
}
static bool gco2_seam_caught(const json_t *norm, const char *base) {
    if (!gco2_raised(norm)) return false;
    const json_t *spec = json_obj_get(norm, "__raise__");
    const char *t = json_get_str(spec, "type", "");
    if (gco2_base_exc(t)) return false;
    return gco2_exc_isa(t, base);
}
static json_t *gco2_entry_url(const json_t *entry, const json_t *norm) {
    const json_t *url = gco2_is_obj(entry) ? json_obj_get(entry, "url") : NULL;
    if (!gco2_truthy(url)) return json_null();  /* `if entry.get("url") else None` */
    if (gco2_seam_caught(norm, "ConnectionStoreError")) return json_null();
    return gco2_seam_propagate(norm);
}
static json_t *gco2_bound_url(const json_t *token_set, const json_t *norm) {
    const json_t *b = gco2_is_obj(token_set) ? json_obj_get(token_set, "baseUrl") : NULL;
    if (!gco2_is_str(b)) return json_null();  /* `isinstance(token_set.get("baseUrl"), str)` */
    if (gco2_seam_caught(norm, "ConnectionStoreError")) return json_null();
    return gco2_seam_propagate(norm);
}

/* ── PoP (Proof of Port) credits — defining module hermes_cli/gateway_connections.py ── */
/* PoP: gco2_revision @ hermes_cli/gateway_connections.py:_revision */
/* PoP: gco2_plain @ hermes_cli/gateway_connections.py:_plain */
/* PoP: gco2_kept_envelope @ hermes_cli/gateway_connections.py:_kept_envelope */
/* PoP: gco2_envelope @ hermes_cli/gateway_connections.py:_envelope */
/* PoP: gco2_token_preview @ hermes_cli/gateway_connections.py:_token_preview */
/* PoP: gco2_public_connection @ hermes_cli/gateway_connections.py:_public_connection */
/* PoP: gco2_public_registry @ hermes_cli/gateway_connections.py:public_registry */
/* PoP: gco2_incoming_headers @ hermes_cli/gateway_connections.py:_incoming_headers */
/* PoP: gco2_signed_in_ids_gone @ hermes_cli/gateway_connections.py:_signed_in_ids_gone */
/* PoP: gco2_entry_url @ hermes_cli/gateway_connections.py:_entry_url */
/* PoP: gco2_bound_url @ hermes_cli/gateway_connections.py:_bound_url */
