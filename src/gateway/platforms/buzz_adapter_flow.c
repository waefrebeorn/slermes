/*
 * buzz_adapter_flow.c — Buzz (Block Nostr relay) adapter FLOW cores and
 * leaves of plugins/platforms/buzz/adapter.py (hermes-agent).
 *
 * Ports the DECISION SEQUENCES of the inbound event-gating / DM-classification
 * / thread-anchoring / event-meta state cluster, the CLI receipt/error text
 * leaves, the identity/allow-list normalization leaves and the scoped
 * config/credentials resolution chain. Call order, short-circuit branches,
 * early returns, OrderedDict mutation history (seen / event_meta /
 * thread_roots eviction + move_to_end) and exception propagation mirror the
 * live Python.
 *
 * Universal seam: every SEAMED sibling call enters through ONE callback
 * ba_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts. args
 * is ADOPTED into the trace event; the return is a NEW owned json_t (NULL ==
 * Python None; {"__raise__":cls,"message":str(exc)} == a raised exception). A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 * BaseException kinds (SystemExit / KeyboardInterrupt / CancelledError /
 * GeneratorExit) pass through every catch site (this cluster's only catches
 * are _resolve_credentials_data's except (OSError, ValueError), narrower than
 * Exception, so scripted BaseException raises from seams always propagate).
 *
 * Mode table (how a function is invoked BY ITS CALLERS):
 *  REAL (run inline, direct C call): every ported function below. A REAL
 *    function calls its REAL children directly (core helpers) and its SEAMED
 *    children via ba_call.
 *  SEAMED (ba_call, scripted): the foreign collaborators only —
 *    npub_to_hex (gateway/authz_mixin.py), _profile_scoped
 *    (gateway/platforms/_shared.py), _get_scoped_secret (module wrapper over
 *    _shared.get_scoped_secret), os.getenv, time.monotonic,
 *    _credentials_candidates (filesystem glob), read_text (Path.read_text).
 *    No sibling buzz function is claimed by another port, so none is seamed.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and ba_-prefixed.
 *
 * Representation contract (shared with tests/oracle_buzz_adapter_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - The adapter self double is ctx.state (a JSON object of live attributes);
 *   methods read/write it in place. A DIRECT self._attr read with the key
 *   ABSENT raises AttributeError "'BuzzAdapter' object has no attribute 'X'"
 *   (the oracle builds a real type("BuzzAdapter", (), clsdict) double).
 * - OrderedDict-backed maps (seen / event_meta / thread_roots) are JSON
 *   objects whose key order is insertion order; popitem(last=False) removes
 *   the FIRST key, move_to_end removes + re-appends. seen values are null;
 *   event_meta values are 2-arrays [pubkey, snippet] (live tuples).
 * - tuple returns (_e_tags pairs, _parse_send_receipt, _lookup_event_meta)
 *   project to 2-arrays; the _e_tags generator to a list of pairs; the
 *   _pubkey_set set to a SORTED unique list.
 * - redact_path / credentials path doubles are JSON objects {path, name}
 *   (Path attr double; name == posixpath.basename(path), generator-enforced).
 * - Integral floats == ints (libjson %.0f; oracle canon folds integral floats
 *   to int). str()-computed positions carry str/int/bool/None only.
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - ASCII scope: content / identities / names are ASCII; the only whitespace
 *   used is the ASCII set {09,0a,0b,0c,0d,1c,1d,1e,1f,20}. str.strip() and
 *   re \s share that set on py3.14; int() strips the NARROWER set
 *   {09,0a,0b,0c,0d,20} (NOT 1c-1f) — ba_strip vs ba_int_of keep them apart.
 * - _json_or / _resolve_credentials_data JSON text is json.dumps-generated
 *   canonical JSON or an obviously-invalid string. Leading-zero / trailing-dot
 *   / bare-exponent numbers, NaN/Infinity, lone surrogate escapes, duplicate
 *   keys and raw control chars are out of domain (libjson json_parse matches
 *   json.loads on this corpus). text is str or None; default is JSON-able.
 * - str()-computed scalars (tag markers/targets, pubkey/name fields, mode
 *   values) carry str/int/bool/None only — no containers, no non-integral
 *   floats (Python str(5.0)=="5.0" vs C %.0f=="5").
 * - int()-coerced kind values are int, bool, None or a clean ASCII decimal
 *   string (optional sign, {09,0a,0b,0c,0d,20} padding, NO underscores).
 * - redact_path / message / content truncation positions are ASCII so
 *   codepoint len == byte len.
 * - _credentials_candidates scripts return a list of {path} objects;
 *   read_text scripts return the POST-DECODE text (utf-8-sig BOM already
 *   stripped by the live decode) or an OSError/ValueError raise spec.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct ba_ctx {
    json_t *state;      /* owned self double; mutated in place; emitted as out.state.self */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ba_ctx_t;

/* ── basic helpers ── */
static bool ba_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ba_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static bool ba_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool ba_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ba_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ba_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool ba_str_eq(const json_t *v, const char *s) {
    const char *t = ba_str(v);
    return t && s && strcmp(t, s) == 0;
}
static const char *ba_arr_str(const json_t *arr, size_t i, const char *def) {
    const json_t *v = json_get(arr, i);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static json_t *ba_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *ba_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return ba_err(type, msg);
}
static json_t *ba_attr_err(const char *key) {
    char msg[160];
    snprintf(msg, sizeof msg, "'BuzzAdapter' object has no attribute '%s'", key);
    return ba_err("AttributeError", msg);
}
/* Python type(x).__name__ for AttributeError messages (integral number==int). */
static const char *ba_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
/* The one universal seam. args ADOPTED; return fresh owned node. */
static json_t *ba_call(ba_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ba_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ba_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ba_make_exc(raise);
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
static json_t *ba_args(void) { return json_object(); }
static void ba_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void ba_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void ba_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void ba_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void ba_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── string helpers ── */
static char *ba_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
/* str.strip() / re \s ASCII whitespace set (py3.14 includes 1c-1f). */
static bool ba_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'
        || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
/* int() strips a NARROWER set (NOT 1c-1f). */
static bool ba_is_int_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static char *ba_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && ba_is_ws((unsigned char)s[a])) a++;
    while (b > a && ba_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}
static char *ba_lower(const char *s) {
    char *o = ba_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
/* Python str() projection for scalar nodes (containers out of domain). */
static char *ba_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ba_strdup("None");
    if (v->type == JSON_BOOL) return ba_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return ba_strdup(buf);
    }
    if (v->type == JSON_STRING) return ba_strdup(v->str_val ? v->str_val : "");
    return ba_strdup("");
}
/* str(x or "") — falsy x collapses to "" before str(). */
static char *ba_pystr_or_empty(const json_t *v) {
    if (!ba_truthy(v)) return ba_strdup("");
    return ba_pystr(v);
}
static bool ba_hex64(const char *s) {
    if (!s || strlen(s) != 64) return false;
    for (const char *p = s; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) return false;
    return true;
}
/* literal case-insensitive match of needle at hay[pos]. */
static bool ba_ieq_at(const char *hay, size_t pos, const char *needle) {
    size_t n = strlen(needle);
    if (strlen(hay + pos) < n) return false;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)hay[pos + i]) != tolower((unsigned char)needle[i])) return false;
    return true;
}
/* replace all literal occurrences of needle in hay with repl. */
static char *ba_replace_all(const char *hay, const char *needle, const char *repl) {
    size_t nl = strlen(needle), rl = strlen(repl);
    if (nl == 0) return ba_strdup(hay);
    size_t cap = strlen(hay) + 1, len = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    const char *p = hay;
    while (*p) {
        if (strncmp(p, needle, nl) == 0) {
            while (len + rl + 1 > cap) { cap *= 2; o = realloc(o, cap); }
            memcpy(o + len, repl, rl); len += rl; p += nl;
        } else {
            if (len + 2 > cap) { cap *= 2; o = realloc(o, cap); }
            o[len++] = *p++;
        }
    }
    o[len] = '\0';
    return o;
}
/* codepoint-aware prefix slice (ASCII corpus: byte slice). */
static char *ba_slice(const char *s, size_t cap_cp) {
    size_t i = 0, cp = 0;
    while (s[i] && cp < cap_cp) {
        unsigned char c = (unsigned char)s[i];
        size_t adv = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        i += adv; cp++;
    }
    char *o = malloc(i + 1);
    if (o) { memcpy(o, s, i); o[i] = '\0'; }
    return o;
}

/* ── ordered-map helpers (OrderedDict semantics) ── */
static void ba_pop_first(json_t *obj) {
    if (!obj || obj->type != JSON_OBJECT || obj->c.count == 0) return;
    free(obj->c.keys[0]);
    json_free(obj->c.items[0]);
    for (size_t j = 0; j + 1 < obj->c.count; j++) {
        obj->c.keys[j] = obj->c.keys[j + 1];
        obj->c.items[j] = obj->c.items[j + 1];
    }
    obj->c.count--;
}
static void ba_move_to_end(json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    if (!v) return;
    json_t *copy = json_copy(v);
    json_obj_del(obj, key);
    json_set(obj, key, copy);
}
static void ba_trim_to(json_t *obj, size_t cap) {
    while (obj && obj->type == JSON_OBJECT && obj->c.count > cap) ba_pop_first(obj);
}

/* ── int() coercion (kind) ── */
static bool ba_int_of(const json_t *v, long long *out, char **errmsg) {
    *errmsg = NULL;
    if (!v || v->type == JSON_NULL) { *out = 0; return true; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return true; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return true; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && ba_is_int_ws((unsigned char)s[a])) a++;
        while (b > a && ba_is_int_ws((unsigned char)s[b - 1])) b--;
        size_t i = a;
        bool neg = false;
        if (i < b && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); i++; }
        if (i >= b) goto bad;
        long long val = 0;
        for (; i < b; i++) {
            if (s[i] < '0' || s[i] > '9') goto bad;
            val = val * 10 + (s[i] - '0');
        }
        *out = neg ? -val : val;
        return true;
    bad: {
            char *msg = malloc(n + 80);
            snprintf(msg, n + 80, "invalid literal for int() with base 10: '%s'", s);
            *errmsg = msg;
            return false;
        }
    }
    {
        char *msg = malloc(128);
        snprintf(msg, 128, "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 ba_type_name(v));
        *errmsg = msg;
        return false;
    }
}

/* ── libjson parse wrapper (json.loads on the documented corpus) ── */
static json_t *ba_json_loads(const char *text) {
    if (!text) return NULL;
    return json_parse(text, NULL);
}

/* self double attribute reader (borrowed). */
static const json_t *ba_self(const ba_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}

/* ================================================================
 * Group A — CLI text leaves
 * ================================================================ */

/* PoP: ba__bounded_cli_message @ plugins/platforms/buzz/adapter.py:_bounded_cli_message */
static json_t *ba_bounded_cli_message_c(const char *message, const json_t *redact) {
    char *msg = ba_strdup(message ? message : "");
    if (redact && redact->type == JSON_OBJECT) {
        const char *path = json_get_str(redact, "path", "");
        const char *name = json_get_str(redact, "name", "");
        char *r = ba_replace_all(msg, path, name);
        free(msg); msg = r;
    }
    json_t *ret;
    if (strlen(msg) <= 900) {
        ret = json_string(msg);
    } else {
        char *cut = ba_slice(msg, 897);
        size_t n = strlen(cut);
        char *buf = malloc(n + 4);
        memcpy(buf, cut, n); memcpy(buf + n, "...", 4);
        ret = json_string(buf);
        free(buf); free(cut);
    }
    free(msg);
    return ret;
}
static json_t *ba__bounded_cli_message(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *message = json_get_str(args, "message", "");
    return ba_bounded_cli_message_c(message, json_obj_get(args, "redact_path"));
}

/* PoP: ba__json_or @ plugins/platforms/buzz/adapter.py:_json_or */
static json_t *ba_json_or_c(const json_t *text, const json_t *deflt) {
    const char *t = ba_str(text);
    if (!t || t[0] == '\0') return deflt ? json_copy(deflt) : json_null();
    json_t *parsed = ba_json_loads(t);
    if (!parsed) return deflt ? json_copy(deflt) : json_null();
    return parsed;
}
static json_t *ba__json_or(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ba_json_or_c(json_obj_get(args, "text"), json_obj_get(args, "default"));
}

/* PoP: ba__cli_error_message @ plugins/platforms/buzz/adapter.py:_cli_error_message */
static json_t *ba__cli_error_message(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *stderr_n = json_obj_get(args, "stderr");
    char *raw = ba_pystr_or_empty(stderr_n);
    char *text = ba_strip(raw);
    free(raw);
    double rc = json_get_num(args, "returncode", 0);
    const json_t *redact = json_obj_get(args, "redact_path");
    json_t *data = text[0] ? ba_json_loads(text) : NULL;
    json_t *ret = NULL;
    if (data && data->type == JSON_OBJECT) {
        const json_t *detail = json_obj_get(data, "message");
        const json_t *category = json_obj_get(data, "error");
        const char *ds = ba_str(detail);
        if (ds) {
            char *dstrip = ba_strip(ds);
            if (dstrip[0]) {
                char *label;
                const char *cs = ba_str(category);
                if (cs) {
                    char *cstrip = ba_strip(cs);
                    label = cstrip[0] ? cstrip : ba_strdup("error");
                    if (cstrip[0] == '\0') free(cstrip);
                } else {
                    label = ba_strdup("error");
                }
                char *buf = malloc(strlen(label) + strlen(dstrip) + 64);
                sprintf(buf, "%s: %s (exit %.0f)", label, dstrip, rc);
                ret = ba_bounded_cli_message_c(buf, redact);
                free(buf); free(label); free(dstrip);
            } else {
                free(dstrip);
            }
        }
    }
    if (!ret) {
        char *fallback = NULL;
        if (text[0]) {
            fallback = ba_strdup(text);
        } else {
            fallback = malloc(80);
            snprintf(fallback, 80, "buzz CLI failed with exit code %.0f", rc);
        }
        ret = ba_bounded_cli_message_c(fallback, redact);
        free(fallback);
    }
    if (data) json_free(data);
    free(text);
    return ret;
}

/* PoP: ba__parse_send_receipt @ plugins/platforms/buzz/adapter.py:_parse_send_receipt */
static json_t *ba__parse_send_receipt(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *data = ba_json_or_c(json_obj_get(args, "stdout"), NULL);
    json_t *ret = json_array();
    if (!data || data->type != JSON_OBJECT) {
        json_append(ret, json_null());
        json_append(ret, json_string("invalid CLI response"));
        if (data) json_free(data);
        return ret;
    }
    const json_t *accepted = json_obj_get(data, "accepted");
    if (accepted && accepted->type == JSON_BOOL && !accepted->bool_val) {
        const json_t *detail = json_obj_get(data, "message");
        const char *ds = ba_str(detail);
        char *dstrip = ds ? ba_strip(ds) : ba_strdup("");
        const char *use = dstrip[0] ? dstrip : "message was not accepted";
        json_t *bounded = ba_bounded_cli_message_c(use, NULL);
        json_append(ret, json_null());
        json_append(ret, bounded);
        free(dstrip); json_free(data);
        return ret;
    }
    const json_t *event_id = json_obj_get(data, "event_id");
    const char *es = ba_str(event_id);
    char *estrip = es ? ba_strip(es) : ba_strdup("");
    bool ok = accepted && accepted->type == JSON_BOOL && accepted->bool_val && es && estrip[0];
    if (!ok) {
        json_append(ret, json_null());
        json_append(ret, json_string("invalid CLI response"));
    } else {
        json_append(ret, json_string(estrip));
        json_append(ret, json_null());
    }
    free(estrip); json_free(data);
    return ret;
}

/* PoP: ba__parse_json_list @ plugins/platforms/buzz/adapter.py:_parse_json_list */
static json_t *ba__parse_json_list(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *deflt = json_array();
    json_t *data = ba_json_or_c(json_obj_get(args, "stdout"), deflt);
    json_free(deflt);
    json_t *ret = json_array();
    if (data && data->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(data); i++) {
            const json_t *item = json_get(data, i);
            if (item && item->type == JSON_OBJECT) json_append(ret, json_copy(item));
        }
    }
    if (data) json_free(data);
    return ret;
}

/* PoP: ba__attachment_rejection_note @ plugins/platforms/buzz/adapter.py:_attachment_rejection_note */
static json_t *ba__attachment_rejection_note(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    double rejected = json_get_num(args, "rejected", 0);
    char buf[160];
    if (rejected <= 999)
        snprintf(buf, sizeof buf, "[%.0f Buzz attachment(s) rejected as malformed or over limits.]", rejected);
    else
        snprintf(buf, sizeof buf, "[999+ Buzz attachment(s) rejected as malformed or over limits.]");
    return json_string(buf);
}

/* ================================================================
 * Group B — identity / allow-list leaves
 * ================================================================ */

/* PoP: ba__split_csv @ plugins/platforms/buzz/adapter.py:_split_csv */
static json_t *ba_split_csv_c(const json_t *raw) {
    const char *s = ba_str(raw);
    if (!s) return raw ? json_copy(raw) : json_null();
    json_t *ret = json_array();
    const char *p = s;
    while (1) {
        const char *comma = strchr(p, ',');
        if (!comma) { json_append(ret, json_string(p)); break; }
        char *seg = malloc((size_t)(comma - p) + 1);
        memcpy(seg, p, (size_t)(comma - p)); seg[comma - p] = '\0';
        json_append(ret, json_string(seg));
        free(seg);
        p = comma + 1;
    }
    return ret;
}
static json_t *ba__split_csv(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ba_split_csv_c(json_obj_get(args, "raw"));
}

/* PoP: ba__normalize_user_ref @ plugins/platforms/buzz/adapter.py:_normalize_user_ref */
static json_t *ba_normalize_user_ref_c(ba_ctx_t *ctx, const json_t *ref) {
    if (ba_truthy(ref) && ref->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", ba_type_name(ref));
        return ba_err("AttributeError", msg);
    }
    const char *rs = ba_truthy(ref) ? ba_str(ref) : "";
    char *stripped = ba_strip(rs);
    char *low = ba_lower(stripped);
    free(stripped);
    json_t *ret;
    if (!low[0]) {
        ret = json_null();
    } else if (strncmp(low, "npub1", 5) == 0) {
        json_t *a = ba_args(); ba_set_s(a, "ref", low);
        ret = ba_call(ctx, "npub_to_hex", a);
    } else if (ba_hex64(low)) {
        ret = json_string(low);
    } else {
        ret = json_null();
    }
    free(low);
    return ret;
}
static json_t *ba__normalize_user_ref(ba_ctx_t *ctx, const json_t *args) {
    return ba_normalize_user_ref_c(ctx, json_obj_get(args, "ref"));
}

/* PoP: ba_normalize_user_id @ plugins/platforms/buzz/adapter.py:normalize_user_id */
static json_t *ba_normalize_user_id(ba_ctx_t *ctx, const json_t *args) {
    return ba_normalize_user_ref_c(ctx, json_obj_get(args, "user_id"));
}

/* PoP: ba__pubkey_set @ plugins/platforms/buzz/adapter.py:_pubkey_set */
static json_t *ba__pubkey_set(ba_ctx_t *ctx, const json_t *args) {
    json_t *split = ba_split_csv_c(json_obj_get(args, "raw"));
    if (!split || split->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", ba_type_name(split));
        if (split) json_free(split);
        return ba_err("TypeError", msg);
    }
    /* collect unique normalized hex refs, then sort */
    json_t *uniq = json_array();
    for (size_t i = 0; i < json_len(split); i++) {
        const json_t *entry = json_get(split, i);
        if (!entry || entry->type != JSON_STRING) continue;
        json_t *n = ba_normalize_user_ref_c(ctx, entry);
        if (ba_is_raise(n)) { json_free(split); json_free(uniq); return n; }
        const char *ns = ba_str(n);
        if (ns && ns[0]) {
            bool found = false;
            for (size_t j = 0; j < json_len(uniq); j++)
                if (ba_str_eq(json_get(uniq, j), ns)) { found = true; break; }
            if (!found) json_append(uniq, json_string(ns));
        }
        if (n) json_free(n);
    }
    json_free(split);
    /* insertion sort by strcmp */
    for (size_t i = 1; i < json_len(uniq); i++) {
        json_t *key = json_get(uniq, i);
        const char *ks = ba_str(key);
        size_t j = i;
        while (j > 0 && strcmp(ba_str(json_get(uniq, j - 1)), ks) > 0) {
            uniq->c.items[j] = uniq->c.items[j - 1];
            j--;
        }
        uniq->c.items[j] = key;
    }
    return uniq;
}

/* PoP: ba__add_pubkey @ plugins/platforms/buzz/adapter.py:_add_pubkey */
static json_t *ba__add_pubkey(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *bucket = json_obj_get(args, "bucket");
    char *raw = ba_pystr_or_empty(json_obj_get(args, "raw"));
    char *pk = ba_lower(raw);
    free(raw);
    if (pk[0] && bucket && bucket->type == JSON_ARRAY) {
        bool found = false;
        for (size_t i = 0; i < json_len(bucket); i++)
            if (ba_str_eq(json_get(bucket, i), pk)) { found = true; break; }
        if (!found) json_append(bucket, json_string(pk));
    }
    free(pk);
    return json_null();
}

/* PoP: ba__ttl_get @ plugins/platforms/buzz/adapter.py:_ttl_get */
static json_t *ba__ttl_get(ba_ctx_t *ctx, const json_t *args) {
    const json_t *cache = json_obj_get(args, "cache");
    const char *key = json_get_str(args, "key", "");
    double ttl = json_get_num(args, "ttl", 0);
    const json_t *cached = cache ? json_obj_get(cache, key) : NULL;
    if (ba_is_none(cached)) return json_null();
    json_t *a = ba_args();
    json_t *mono = ba_call(ctx, "time.monotonic", a);
    if (ba_is_raise(mono)) return mono;
    double now = (mono && mono->type == JSON_NUMBER) ? mono->num_val : 0;
    if (mono) json_free(mono);
    const json_t *tsn = json_get(cached, 0);
    double ts = (tsn && tsn->type == JSON_NUMBER) ? tsn->num_val : 0;
    if ((now - ts) < ttl) {
        const json_t *val = json_get(cached, 1);
        return val ? json_copy(val) : json_null();
    }
    return json_null();
}

/* PoP: ba__credentials_key @ plugins/platforms/buzz/adapter.py:_credentials_key */
static json_t *ba_credentials_key_c(const json_t *data) {
    if (!data || data->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ba_type_name(data));
        return ba_err("AttributeError", msg);
    }
    static const char *fields[] = { "nsec", "private_key_hex", "private_key" };
    for (int i = 0; i < 3; i++) {
        const json_t *v = json_obj_get(data, fields[i]);
        const char *s = ba_str(v);
        if (s) {
            char *st = ba_strip(s);
            if (st[0]) { json_t *r = json_string(st); free(st); return r; }
            free(st);
        }
    }
    return json_string("");
}
static json_t *ba__credentials_key(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ba_credentials_key_c(json_obj_get(args, "data"));
}

/* ================================================================
 * Group C — NIP-10 tags / thread / event-meta state
 * ================================================================ */

/* PoP: ba__e_tags @ plugins/platforms/buzz/adapter.py:_e_tags */
static json_t *ba_e_tags_c(const json_t *event) {
    json_t *ret = json_array();
    const json_t *tags = event ? json_obj_get(event, "tags") : NULL;
    if (!tags || tags->type != JSON_ARRAY) return ret;
    for (size_t i = 0; i < json_len(tags); i++) {
        const json_t *tag = json_get(tags, i);
        if (!tag || tag->type != JSON_ARRAY || json_len(tag) < 2) continue;
        char *t0 = ba_pystr(json_get(tag, 0));
        bool is_e = strcmp(t0, "e") == 0;
        free(t0);
        if (!is_e) continue;
        json_t *pair = json_array();
        const json_t *target = json_get(tag, 1);
        json_append(pair, target ? json_copy(target) : json_null());
        char *marker = json_len(tag) > 3 ? ba_pystr(json_get(tag, 3)) : ba_strdup("");
        json_append(pair, json_string(marker));
        free(marker);
        json_append(ret, pair);
    }
    return ret;
}
static json_t *ba__e_tags(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ba_e_tags_c(json_obj_get(args, "event"));
}

/* PoP: ba__event_reply_parent_id @ plugins/platforms/buzz/adapter.py:_event_reply_parent_id */
static json_t *ba__event_reply_parent_id(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *tags = ba_e_tags_c(json_obj_get(args, "event"));
    char *reply_id = NULL, *root_id = NULL, *last_e = NULL;
    for (size_t i = 0; i < json_len(tags); i++) {
        const json_t *pair = json_get(tags, i);
        const json_t *raw_target = json_get(pair, 0);
        char *ts = ba_pystr_or_empty(raw_target);
        char *target = ba_strip(ts);
        free(ts);
        const char *marker = ba_arr_str(pair, 1, "");
        if (target[0]) {
            free(last_e); last_e = ba_strdup(target);
            if (strcmp(marker, "reply") == 0) { free(reply_id); reply_id = ba_strdup(target); }
            else if (strcmp(marker, "root") == 0) { free(root_id); root_id = ba_strdup(target); }
        }
        free(target);
    }
    json_free(tags);
    const char *pick = reply_id ? reply_id : root_id ? root_id : last_e;
    json_t *ret = pick ? json_string(pick) : json_null();
    free(reply_id); free(root_id); free(last_e);
    return ret;
}

/* PoP: ba__p_tagged @ plugins/platforms/buzz/adapter.py:_p_tagged */
static bool ba_p_tagged_c(const json_t *event, const char *pubkey) {
    const json_t *tags = event ? json_obj_get(event, "tags") : NULL;
    if (!tags || tags->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(tags); i++) {
        const json_t *tag = json_get(tags, i);
        if (!tag || tag->type != JSON_ARRAY || json_len(tag) <= 1) continue;
        const json_t *t0 = json_get(tag, 0);
        if (!ba_str_eq(t0, "p")) continue;
        char *t1 = ba_pystr(json_get(tag, 1));
        char *low = ba_lower(t1);
        free(t1);
        bool eq = pubkey && strcmp(low, pubkey) == 0;
        free(low);
        if (eq) return true;
    }
    return false;
}
static json_t *ba__p_tagged(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *pubkey = json_get_str(args, "pubkey", "");
    return json_bool(ba_p_tagged_c(json_obj_get(args, "event"), pubkey));
}

/* PoP: ba__extract_thread_root @ plugins/platforms/buzz/adapter.py:_extract_thread_root */
static json_t *ba_extract_thread_root_c(const json_t *event) {
    json_t *tags = ba_e_tags_c(event);
    char *root = NULL, *reply = NULL;
    for (size_t i = 0; i < json_len(tags); i++) {
        const json_t *pair = json_get(tags, i);
        const json_t *target = json_get(pair, 0);
        char *mk_raw = ba_strdup(ba_arr_str(pair, 1, ""));
        char *marker = ba_lower(mk_raw);
        free(mk_raw);
        if (strcmp(marker, "root") == 0) {
            free(root); root = ba_pystr(target);
        } else if (strcmp(marker, "reply") == 0) {
            free(reply); reply = ba_pystr(target);
        } else if (!marker[0] && !reply) {
            reply = ba_pystr(target);
        }
        free(marker);
    }
    json_free(tags);
    const char *pick = root ? root : reply;
    json_t *ret = pick ? json_string(pick) : json_null();
    free(root); free(reply);
    return ret;
}
static json_t *ba__extract_thread_root(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ba_extract_thread_root_c(json_obj_get(args, "event"));
}

/* PoP: ba__store_event_meta @ plugins/platforms/buzz/adapter.py:_store_event_meta */
static json_t *ba_store_event_meta_c(json_t *state, const char *event_id, const char *pubkey, const char *snippet) {
    if (!state || state->type != JSON_OBJECT)
        return ba_err("TypeError", "'state' must be a dict");
    json_t *cache = json_obj_get(state, "event_meta");
    if (!cache) {
        cache = json_object();
        json_set(state, "event_meta", cache);
    }
    json_t *entry = json_array();
    json_append(entry, json_string(pubkey ? pubkey : ""));
    json_append(entry, json_string(snippet ? snippet : ""));
    json_set(cache, event_id, entry);
    ba_move_to_end(cache, event_id);
    ba_trim_to(cache, 500);
    return NULL;
}
static json_t *ba__store_event_meta(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *state = json_obj_get(args, "state");
    return ba_store_event_meta_c(state, json_get_str(args, "event_id", ""),
                                 json_get_str(args, "pubkey", ""), json_get_str(args, "snippet", ""));
}

/* PoP: ba__lookup_event_meta @ plugins/platforms/buzz/adapter.py:_lookup_event_meta */
static json_t *ba__lookup_event_meta(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *state = json_obj_get(args, "state");
    const json_t *event_id = json_obj_get(args, "event_id");
    if (!ba_truthy(event_id)) return json_null();
    const char *eid = ba_str(event_id);
    const json_t *meta = state ? json_obj_get(state, "event_meta") : NULL;
    const json_t *entry = (meta && meta->type == JSON_OBJECT && eid) ? json_obj_get(meta, eid) : NULL;
    if (!ba_truthy(entry) || !entry || entry->type != JSON_ARRAY || json_len(entry) < 2)
        return json_null();
    char *a = ba_pystr_or_empty(json_get(entry, 0));
    char *b = ba_pystr_or_empty(json_get(entry, 1));
    json_t *ret = json_array();
    json_append(ret, json_string(a));
    json_append(ret, json_string(b));
    free(a); free(b);
    return ret;
}

/* PoP: ba__trim_seen @ plugins/platforms/buzz/adapter.py:_trim_seen */
static json_t *ba_trim_seen_c(json_t *state) {
    if (!state || state->type != JSON_OBJECT || !json_obj_get(state, "seen"))
        return ba_err("KeyError", "'seen'");
    json_t *seen = json_obj_get(state, "seen");
    ba_trim_to(seen, 500);
    json_t *meta = json_obj_get(state, "event_meta");
    if (meta && meta->type == JSON_OBJECT) ba_trim_to(meta, 500);
    return NULL;
}
static json_t *ba__trim_seen(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *r = ba_trim_seen_c(json_obj_get(args, "state"));
    return r ? r : json_null();
}

/* PoP: ba__remember_event @ plugins/platforms/buzz/adapter.py:_remember_event */
static json_t *ba_remember_event_c(ba_ctx_t *ctx, json_t *state, const json_t *event) {
    (void)ctx;
    char *eid = ba_pystr_or_empty(event ? json_obj_get(event, "id") : NULL);
    const json_t *content = event ? json_obj_get(event, "content") : NULL;
    json_t *r = NULL;
    if (eid[0]) {
        char *snippet = (content && content->type == JSON_STRING)
            ? ba_slice(content->str_val ? content->str_val : "", 500) : ba_strdup("");
        char *pk_raw = ba_pystr_or_empty(event ? json_obj_get(event, "pubkey") : NULL);
        char *pk = ba_lower(pk_raw);
        free(pk_raw);
        r = ba_store_event_meta_c(state, eid, pk, snippet);
        free(snippet); free(pk);
    }
    free(eid);
    return r;
}
static json_t *ba__remember_event(ba_ctx_t *ctx, const json_t *args) {
    json_t *r = ba_remember_event_c(ctx, json_obj_get(args, "state"), json_obj_get(args, "event"));
    return r ? r : json_null();
}

/* PoP: ba__remember_event_meta @ plugins/platforms/buzz/adapter.py:_remember_event_meta */
static json_t *ba__remember_event_meta(ba_ctx_t *ctx, const json_t *args) {
    if (!json_has(ctx->state, "_channel_state")) return ba_attr_err("_channel_state");
    const json_t *cs = ba_self(ctx, "_channel_state");
    const char *channel_id = json_get_str(args, "channel_id", "");
    const json_t *state = cs ? json_obj_get(cs, channel_id) : NULL;
    if (ba_is_none(state)) return json_null();
    json_t *ev = json_object();
    ba_set_s(ev, "id", json_get_str(args, "event_id", ""));
    ba_set_s(ev, "pubkey", json_get_str(args, "pubkey", ""));
    const json_t *content = json_obj_get(args, "content");
    if (ba_truthy(content)) ba_set_copy(ev, "content", content);
    else ba_set_s(ev, "content", "");
    json_t *r = ba_remember_event_c(ctx, (json_t *)state, ev);
    json_free(ev);
    return r ? r : json_null();
}

/* PoP: ba__new_channel_state @ plugins/platforms/buzz/adapter.py:_new_channel_state */
static json_t *ba_new_channel_state_c(const char *chat_type) {
    json_t *st = json_object();
    ba_set_s(st, "chat_type", chat_type);
    ba_set_n(st, "last_ts", 0);
    json_set(st, "seen", json_object());
    json_set(st, "event_meta", json_object());
    return st;
}
static json_t *ba__new_channel_state(ba_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ba_new_channel_state_c(json_get_str(args, "chat_type", ""));
}

/* PoP: ba__restore_channel_state @ plugins/platforms/buzz/adapter.py:_restore_channel_state */
static json_t *ba__restore_channel_state(ba_ctx_t *ctx, const json_t *args) {
    if (!json_has(ctx->state, "_restored_cursors")) return ba_attr_err("_restored_cursors");
    json_t *restored_map = (json_t *)ba_self(ctx, "_restored_cursors");
    const char *channel_id = json_get_str(args, "channel_id", "");
    const char *chat_type = json_get_str(args, "chat_type", "");
    json_t *restored = restored_map ? json_obj_get(restored_map, channel_id) : NULL;
    if (ba_is_none(restored)) return json_bool(false);
    if (!json_has(ctx->state, "_channel_state")) return ba_attr_err("_channel_state");
    const json_t *rct = json_obj_get(restored, "chat_type");
    char *ct = ba_truthy(rct) ? ba_pystr(rct) : ba_strdup(chat_type);
    json_t *state = ba_new_channel_state_c(ct);
    free(ct);
    const json_t *lts = json_obj_get(restored, "last_ts");
    json_set(state, "last_ts", lts ? json_copy(lts) : json_number(0));
    json_t *seen = json_object();
    const json_t *rseen = json_obj_get(restored, "seen");
    if (rseen && rseen->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rseen); i++) {
            char *eid = ba_pystr(json_get(rseen, i));
            json_set(seen, eid, json_null());
            free(eid);
        }
    }
    json_set(state, "seen", seen);
    json_t *cs = (json_t *)ba_self(ctx, "_channel_state");
    json_set(cs, channel_id, state);
    json_obj_del(restored_map, channel_id);
    return json_bool(true);
}

/* PoP: ba__mark_seen @ plugins/platforms/buzz/adapter.py:_mark_seen */
static json_t *ba__mark_seen(ba_ctx_t *ctx, const json_t *args) {
    if (!json_has(ctx->state, "_channel_state")) return ba_attr_err("_channel_state");
    const json_t *cs = ba_self(ctx, "_channel_state");
    const char *channel_id = json_get_str(args, "channel_id", "");
    const json_t *state = cs ? json_obj_get(cs, channel_id) : NULL;
    if (ba_is_none(state)) return json_null();
    json_t *seen = json_obj_get(state, "seen");
    if (!seen) return ba_err("KeyError", "'seen'");
    json_set(seen, json_get_str(args, "event_id", ""), json_null());
    json_t *r = ba_trim_seen_c((json_t *)state);
    return r ? r : json_null();
}

/* PoP: ba__record_thread_root @ plugins/platforms/buzz/adapter.py:_record_thread_root */
static json_t *ba__record_thread_root(ba_ctx_t *ctx, const json_t *args) {
    const json_t *event_id = json_obj_get(args, "event_id");
    if (!ba_truthy(event_id)) return json_null();
    if (!json_has(ctx->state, "_thread_roots")) return ba_attr_err("_thread_roots");
    json_t *roots = (json_t *)ba_self(ctx, "_thread_roots");
    const char *eid = ba_str(event_id);
    json_t *root = ba_extract_thread_root_c(json_obj_get(args, "event"));
    json_set(roots, eid, root);
    ba_move_to_end(roots, eid);
    ba_trim_to(roots, 512);
    return json_null();
}

/* PoP: ba__resolve_reply_anchor @ plugins/platforms/buzz/adapter.py:_resolve_reply_anchor */
static json_t *ba_resolve_reply_anchor_c(ba_ctx_t *ctx, const json_t *anchor) {
    if (!ba_truthy(anchor)) return anchor ? json_copy(anchor) : json_null();
    if (!json_has(ctx->state, "_thread_roots")) return ba_attr_err("_thread_roots");
    const json_t *roots = ba_self(ctx, "_thread_roots");
    char *as = ba_pystr(anchor);
    const json_t *root = roots ? json_obj_get(roots, as) : NULL;
    free(as);
    if (ba_truthy(root)) return json_copy(root);
    return json_copy(anchor);
}
static json_t *ba__resolve_reply_anchor(ba_ctx_t *ctx, const json_t *args) {
    return ba_resolve_reply_anchor_c(ctx, json_obj_get(args, "anchor"));
}

/* PoP: ba__reply_args @ plugins/platforms/buzz/adapter.py:_reply_args */
static json_t *ba__reply_args(ba_ctx_t *ctx, const json_t *args) {
    json_t *target = ba_resolve_reply_anchor_c(ctx, json_obj_get(args, "anchor"));
    if (ba_is_raise(target)) return target;
    json_t *ret = json_array();
    if (ba_truthy(target)) {
        if (!json_has(ctx->state, "_reply_to_mode")) { json_free(target); return ba_attr_err("_reply_to_mode"); }
        const json_t *mode = ba_self(ctx, "_reply_to_mode");
        if (!ba_str_eq(mode, "off")) {
            char *ts = ba_pystr(target);
            json_append(ret, json_string("--reply-to"));
            json_append(ret, json_string(ts));
            free(ts);
        }
    }
    json_free(target);
    return ret;
}

/* ================================================================
 * Group D — DM classification / mention gating
 * ================================================================ */

/* PoP: ba__may_reclassify_as_dm @ plugins/platforms/buzz/adapter.py:_may_reclassify_as_dm */
static json_t *ba_may_reclassify_c(ba_ctx_t *ctx, const char *channel_id) {
    if (!json_has(ctx->state, "_channel_meta")) return ba_attr_err("_channel_meta");
    const json_t *cm = ba_self(ctx, "_channel_meta");
    const json_t *meta = cm ? json_obj_get(cm, channel_id) : NULL;
    if (ba_is_none(meta)) return json_bool(false);
    if (meta->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ba_type_name(meta));
        return ba_err("AttributeError", msg);
    }
    char *name_raw = ba_pystr_or_empty(json_obj_get(meta, "name"));
    char *name = ba_strip(name_raw); free(name_raw);
    char *desc_raw = ba_pystr_or_empty(json_obj_get(meta, "description"));
    char *desc = ba_strip(desc_raw); free(desc_raw);
    bool ok = strcmp(name, "DM") == 0 && desc[0] == '\0';
    free(name); free(desc);
    return json_bool(ok);
}
static json_t *ba__may_reclassify_as_dm(ba_ctx_t *ctx, const json_t *args) {
    return ba_may_reclassify_c(ctx, json_get_str(args, "channel_id", ""));
}

/* PoP: ba__p_tagged_to_self @ plugins/platforms/buzz/adapter.py:_p_tagged_to_self */
static json_t *ba_p_tagged_to_self_c(ba_ctx_t *ctx, const json_t *event) {
    if (!json_has(ctx->state, "_self_pubkey")) return ba_attr_err("_self_pubkey");
    const json_t *pk = ba_self(ctx, "_self_pubkey");
    if (!ba_truthy(pk)) return json_bool(false);
    const char *pks = ba_str(pk);
    return json_bool(ba_p_tagged_c(event, pks ? pks : ""));
}
static json_t *ba__p_tagged_to_self(ba_ctx_t *ctx, const json_t *args) {
    return ba_p_tagged_to_self_c(ctx, json_obj_get(args, "event"));
}

/* negative lookbehind/lookahead search: needle bounded by charset (end OK). */
static bool ba_search_bounded(const char *hay, const char *needle, bool (*in_set)(unsigned char)) {
    size_t nl = strlen(needle);
    if (nl == 0) return false;
    const char *p = hay;
    while ((p = strstr(p, needle)) != NULL) {
        size_t pos = (size_t)(p - hay);
        bool before_ok = (pos == 0) || !in_set((unsigned char)hay[pos - 1]);
        unsigned char after = (unsigned char)p[nl];
        bool after_ok = (after == '\0') || !in_set(after);
        if (before_ok && after_ok) return true;
        p += 1;
    }
    return false;
}
static bool ba_in_hex(unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }
static bool ba_in_alnum(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }
static bool ba_in_word_at(unsigned char c) { return ba_in_alnum(c) || c == '_' || c == '@'; }
static bool ba_in_disp_after(unsigned char c) {
    return ba_is_ws(c) || c == ',' || c == ';' || c == '.' || c == '!' || c == '?'
        || c == ':' || c == ')' || c == ']' || c == '}';
}
/* display mention: '@name' with negative [\w@] before, positive (end|set) after. */
static bool ba_search_display(const char *hay, const char *needle) {
    size_t nl = strlen(needle);
    if (nl == 0) return false;
    const char *p = hay;
    while ((p = strstr(p, needle)) != NULL) {
        size_t pos = (size_t)(p - hay);
        bool before_ok = (pos == 0) || !ba_in_word_at((unsigned char)hay[pos - 1]);
        unsigned char after = (unsigned char)p[nl];
        bool after_ok = (after == '\0') || ba_in_disp_after(after);
        if (before_ok && after_ok) return true;
        p += 1;
    }
    return false;
}

/* PoP: ba__is_mentioned @ plugins/platforms/buzz/adapter.py:_is_mentioned */
static json_t *ba_is_mentioned_c(ba_ctx_t *ctx, const char *content) {
    if (!json_has(ctx->state, "_self_pubkey")) return ba_attr_err("_self_pubkey");
    if (!json_has(ctx->state, "_self_npub")) return ba_attr_err("_self_npub");
    if (!json_has(ctx->state, "_display_name")) return ba_attr_err("_display_name");
    const json_t *pk = ba_self(ctx, "_self_pubkey");
    const json_t *npub = ba_self(ctx, "_self_npub");
    const json_t *dn = ba_self(ctx, "_display_name");
    char *low = ba_lower(content ? content : "");
    bool found = false;
    const char *pks = ba_str(pk);
    if (ba_truthy(pk) && pks && ba_hex64(pks)) {
        if (ba_search_bounded(low, pks, ba_in_hex)) found = true;
    }
    if (!found && ba_truthy(npub)) {
        char *nl = ba_lower(ba_str(npub) ? ba_str(npub) : "");
        if (ba_search_bounded(low, nl, ba_in_alnum)) found = true;
        free(nl);
    }
    if (!found && ba_truthy(dn)) {
        char *dl = ba_lower(ba_str(dn) ? ba_str(dn) : "");
        char *needle = malloc(strlen(dl) + 2);
        needle[0] = '@'; memcpy(needle + 1, dl, strlen(dl) + 1);
        if (ba_search_display(low, needle)) found = true;
        free(needle); free(dl);
    }
    free(low);
    return json_bool(found);
}
static json_t *ba__is_mentioned(ba_ctx_t *ctx, const json_t *args) {
    return ba_is_mentioned_c(ctx, json_get_str(args, "content", ""));
}

/* PoP: ba__is_addressed @ plugins/platforms/buzz/adapter.py:_is_addressed */
static json_t *ba__is_addressed(ba_ctx_t *ctx, const json_t *args) {
    const json_t *event = json_obj_get(args, "event");
    const json_t *content = event ? json_obj_get(event, "content") : NULL;
    if (!content || content->type != JSON_STRING) return json_bool(false);
    json_t *m = ba_is_mentioned_c(ctx, content->str_val ? content->str_val : "");
    if (ba_is_raise(m)) return m;
    bool mentioned = m && m->type == JSON_BOOL && m->bool_val;
    if (m) json_free(m);
    if (mentioned) return json_bool(true);
    return ba_p_tagged_to_self_c(ctx, event);
}

/* PoP: ba__is_direct_message_event @ plugins/platforms/buzz/adapter.py:_is_direct_message_event */
static json_t *ba_is_direct_c(ba_ctx_t *ctx, const char *channel_id, const json_t *event) {
    if (!json_has(ctx->state, "_self_pubkey")) return ba_attr_err("_self_pubkey");
    const json_t *self_pk = ba_self(ctx, "_self_pubkey");
    if (!ba_truthy(self_pk)) return json_bool(false);
    json_t *may = ba_may_reclassify_c(ctx, channel_id);
    if (ba_is_raise(may)) return may;
    bool may_dm = may && may->type == JSON_BOOL && may->bool_val;
    if (may) json_free(may);
    if (!may_dm) return json_bool(false);
    const json_t *kind = event ? json_obj_get(event, "kind") : NULL;
    long long kv = 0;
    if (ba_truthy(kind)) {
        char *emsg = NULL;
        if (!ba_int_of(kind, &kv, &emsg)) {
            json_t *e = ba_err("ValueError", emsg);
            free(emsg);
            return e;
        }
    }
    if (kv != 9) return json_bool(false);
    char *pk_raw = ba_pystr_or_empty(event ? json_obj_get(event, "pubkey") : NULL);
    char *pubkey = ba_lower(pk_raw); free(pk_raw);
    const char *spks = ba_str(self_pk);
    if (!pubkey[0] || (spks && strcmp(pubkey, spks) == 0)) { free(pubkey); return json_bool(false); }
    free(pubkey);
    json_t *pt = ba_p_tagged_to_self_c(ctx, event);
    if (ba_is_raise(pt)) return pt;
    bool tagged = pt && pt->type == JSON_BOOL && pt->bool_val;
    if (pt) json_free(pt);
    if (!tagged) return json_bool(false);
    const json_t *content = event ? json_obj_get(event, "content") : NULL;
    if (!content || content->type != JSON_STRING) return json_bool(false);
    json_t *m = ba_is_mentioned_c(ctx, content->str_val ? content->str_val : "");
    if (ba_is_raise(m)) return m;
    bool mentioned = m && m->type == JSON_BOOL && m->bool_val;
    if (m) json_free(m);
    return json_bool(!mentioned);
}
static json_t *ba__is_direct_message_event(ba_ctx_t *ctx, const json_t *args) {
    return ba_is_direct_c(ctx, json_get_str(args, "channel_id", ""), json_obj_get(args, "event"));
}

/* PoP: ba__maybe_latch_dm @ plugins/platforms/buzz/adapter.py:_maybe_latch_dm */
static json_t *ba__maybe_latch_dm(ba_ctx_t *ctx, const json_t *args) {
    json_t *state = json_obj_get(args, "state");
    if (!state || state->type != JSON_OBJECT || !json_obj_get(state, "chat_type"))
        return ba_err("KeyError", "'chat_type'");
    const json_t *ct = json_obj_get(state, "chat_type");
    if (ba_str_eq(ct, "dm")) return json_null();
    const char *channel_id = json_get_str(args, "channel_id", "");
    json_t *is_dm = ba_is_direct_c(ctx, channel_id, json_obj_get(args, "event"));
    if (ba_is_raise(is_dm)) return is_dm;
    bool direct = is_dm && is_dm->type == JSON_BOOL && is_dm->bool_val;
    if (is_dm) json_free(is_dm);
    if (!direct) return json_null();
    json_set_hdr(state, "chat_type", "dm");
    if (!json_has(ctx->state, "_channel_names")) return ba_attr_err("_channel_names");
    json_t *names = (json_t *)ba_self(ctx, "_channel_names");
    if (!json_obj_get(names, channel_id)) json_set_hdr(names, channel_id, "DM");
    return json_null();
}

/* PoP: ba__strip_mention @ plugins/platforms/buzz/adapter.py:_strip_mention */
static json_t *ba_strip_mention_c(ba_ctx_t *ctx, const char *content) {
    if (!json_has(ctx->state, "_display_name")) return ba_attr_err("_display_name");
    if (!json_has(ctx->state, "_self_npub")) return ba_attr_err("_self_npub");
    if (!json_has(ctx->state, "_self_pubkey")) return ba_attr_err("_self_pubkey");
    const json_t *dn = ba_self(ctx, "_display_name");
    const json_t *npub = ba_self(ctx, "_self_npub");
    const json_t *pk = ba_self(ctx, "_self_pubkey");
    char *text = ba_strip(content ? content : "");
    const char *dns = ba_truthy(dn) ? ba_str(dn) : NULL;
    const char *npubs = ba_truthy(npub) ? ba_str(npub) : NULL;
    const char *pks = ba_truthy(pk) ? ba_str(pk) : NULL;
    if (!dns && !npubs && !pks) { json_t *r = json_string(text); free(text); return r; }
    size_t end = 0;
    bool matched = false;
    size_t tlen = strlen(text);
    /* display candidate: requires '@', positive lookahead */
    if (!matched && dns && text[0] == '@' && ba_ieq_at(text, 1, dns)) {
        size_t e = 1 + strlen(dns);
        unsigned char after = (unsigned char)text[e];
        if (after == '\0' || ba_in_disp_after(after)) { end = e; matched = true; }
    }
    /* npub candidate: optional '@', negative [a-z0-9] lookahead (IGNORECASE) */
    if (!matched && npubs) {
        size_t off = (text[0] == '@') ? 1 : 0;
        if (ba_ieq_at(text, off, npubs)) {
            size_t e = off + strlen(npubs);
            unsigned char after = (unsigned char)text[e];
            if (!(ba_in_alnum((unsigned char)tolower(after)))) { end = e; matched = true; }
        }
    }
    /* pubkey candidate: optional '@', negative [0-9a-f] lookahead (IGNORECASE) */
    if (!matched && pks) {
        size_t off = (text[0] == '@') ? 1 : 0;
        if (ba_ieq_at(text, off, pks)) {
            size_t e = off + strlen(pks);
            unsigned char after = (unsigned char)text[e];
            if (!(ba_in_hex((unsigned char)tolower(after)))) { end = e; matched = true; }
        }
    }
    if (matched) {
        while (end < tlen && (ba_is_ws((unsigned char)text[end]) || text[end] == ':' || text[end] == ',')) end++;
        char *rest = ba_strip(text + end);
        free(text);
        json_t *r = json_string(rest); free(rest); return r;
    }
    json_t *r = json_string(text); free(text); return r;
}
static json_t *ba__strip_mention(ba_ctx_t *ctx, const json_t *args) {
    return ba_strip_mention_c(ctx, json_get_str(args, "content", ""));
}

/* ================================================================
 * Group E — scoped config / credentials resolution
 * ================================================================ */

/* PoP: ba__scoped_platform_setting @ plugins/platforms/buzz/adapter.py:_scoped_platform_setting */
static json_t *ba_scoped_platform_setting_c(ba_ctx_t *ctx, const char *env_name, const json_t *extra, const char *key) {
    json_t *a = ba_args();
    json_t *scoped_flag = ba_call(ctx, "_profile_scoped", a);
    if (ba_is_raise(scoped_flag)) return scoped_flag;
    bool scoped = scoped_flag && scoped_flag->type == JSON_BOOL && scoped_flag->bool_val;
    if (scoped_flag) json_free(scoped_flag);
    if (scoped) {
        json_t *sa = ba_args(); ba_set_s(sa, "name", env_name); ba_set_null(sa, "default");
        json_t *secret = ba_call(ctx, "_get_scoped_secret", sa);
        if (ba_is_raise(secret)) return secret;
        if (!ba_is_none(secret)) return secret;
        if (secret) json_free(secret);
        const json_t *ex = ba_truthy(extra) ? extra : NULL;
        const json_t *v = ex ? json_obj_get(ex, key) : NULL;
        return v ? json_copy(v) : json_null();
    }
    json_t *ga = ba_args(); ba_set_s(ga, "name", env_name);
    return ba_call(ctx, "os.getenv", ga);
}
static json_t *ba__scoped_platform_setting(ba_ctx_t *ctx, const json_t *args) {
    return ba_scoped_platform_setting_c(ctx, json_get_str(args, "env_name", ""),
                                        json_obj_get(args, "extra"), json_get_str(args, "key", ""));
}

/* PoP: ba__setting_or @ plugins/platforms/buzz/adapter.py:_setting_or */
static json_t *ba_setting_or_c(ba_ctx_t *ctx, const char *env_name, const json_t *extra, const char *key, const json_t *deflt) {
    json_t *raw = ba_scoped_platform_setting_c(ctx, env_name, extra, key);
    if (ba_is_raise(raw)) return raw;
    if (!ba_is_none(raw)) return raw;
    if (raw) json_free(raw);
    if (!extra || extra->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ba_type_name(extra));
        return ba_err("AttributeError", msg);
    }
    const json_t *v = json_obj_get(extra, key);
    return v ? json_copy(v) : (deflt ? json_copy(deflt) : json_null());
}
static json_t *ba__setting_or(ba_ctx_t *ctx, const json_t *args) {
    return ba_setting_or_c(ctx, json_get_str(args, "env_name", ""), json_obj_get(args, "extra"),
                           json_get_str(args, "key", ""), json_obj_get(args, "default"));
}

/* PoP: ba__configured_relay @ plugins/platforms/buzz/adapter.py:_configured_relay */
static json_t *ba__configured_relay(ba_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    json_t *scoped = ba_scoped_platform_setting_c(ctx, "BUZZ_RELAY_URL", extra, "relay_url");
    if (ba_is_raise(scoped)) return scoped;
    const json_t *v = NULL;
    bool v_owned = false;
    if (ba_truthy(scoped)) {
        v = scoped;
    } else {
        if (!extra || extra->type != JSON_OBJECT) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ba_type_name(extra));
            if (scoped) json_free(scoped);
            return ba_err("AttributeError", msg);
        }
        v = json_obj_get(extra, "relay_url");
        if (!v) { v = json_string(""); v_owned = true; }
    }
    if (!v || v->type != JSON_STRING) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", ba_type_name(v));
        if (v_owned) json_free((json_t *)v);
        if (scoped) json_free(scoped);
        return ba_err("AttributeError", msg);
    }
    char *st = ba_strip(v->str_val ? v->str_val : "");
    json_t *ret = json_string(st);
    free(st);
    if (v_owned) json_free((json_t *)v);
    if (scoped) json_free(scoped);
    return ret;
}

/* PoP: ba__configured_home_channel @ plugins/platforms/buzz/adapter.py:_configured_home_channel */
static json_t *ba__configured_home_channel(ba_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    json_t *raw = ba_scoped_platform_setting_c(ctx, "BUZZ_HOME_CHANNEL", extra, "home_channel");
    if (ba_is_raise(raw)) return raw;
    char *inner;
    if (ba_truthy(raw)) {
        if (raw->type != JSON_STRING) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", ba_type_name(raw));
            json_free(raw);
            return ba_err("AttributeError", msg);
        }
        inner = ba_strdup(raw->str_val ? raw->str_val : "");
    } else {
        const json_t *hv = (extra && extra->type == JSON_OBJECT) ? json_obj_get(extra, "home_channel") : NULL;
        inner = ba_pystr_or_empty(hv);
    }
    char *st = ba_strip(inner);
    free(inner);
    if (raw) json_free(raw);
    return json_string(st);
}

/* PoP: ba__configured_credentials_file @ plugins/platforms/buzz/adapter.py:_configured_credentials_file */
static json_t *ba__configured_credentials_file(ba_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    json_t *a = ba_args(); ba_set_s(a, "name", "BUZZ_CREDENTIALS_FILE"); ba_set_s(a, "default", "");
    json_t *secret = ba_call(ctx, "_get_scoped_secret", a);
    if (ba_is_raise(secret)) return secret;
    char *raw = ba_pystr_or_empty(secret);
    if (secret) json_free(secret);
    char *configured = ba_strip(raw); free(raw);
    if (configured[0]) { json_t *r = json_string(configured); free(configured); return r; }
    free(configured);
    const json_t *ex = ba_truthy(extra) ? extra : NULL;
    if (ex && ex->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ba_type_name(ex));
        return ba_err("AttributeError", msg);
    }
    const json_t *cv = ex ? json_obj_get(ex, "credentials_file") : NULL;
    char *inner = ba_pystr_or_empty(cv);
    char *st = ba_strip(inner); free(inner);
    json_t *r = json_string(st); free(st);
    return r;
}

/* PoP: ba__reply_to_mode @ plugins/platforms/buzz/adapter.py:_reply_to_mode */
static json_t *ba__reply_to_mode(ba_ctx_t *ctx, const json_t *args) {
    const json_t *config = json_obj_get(args, "config");
    const json_t *extra = json_obj_get(args, "extra");
    json_t *a = ba_args();
    json_t *flag = ba_call(ctx, "_profile_scoped", a);
    if (ba_is_raise(flag)) return flag;
    bool scoped = flag && flag->type == JSON_BOOL && flag->bool_val;
    if (flag) json_free(flag);
    json_t *mode_env;
    if (scoped) {
        json_t *sa = ba_args(); ba_set_s(sa, "name", "BUZZ_REPLY_TO_MODE"); ba_set_null(sa, "default");
        mode_env = ba_call(ctx, "_get_scoped_secret", sa);
    } else {
        json_t *ga = ba_args(); ba_set_s(ga, "name", "BUZZ_REPLY_TO_MODE");
        mode_env = ba_call(ctx, "os.getenv", ga);
    }
    if (ba_is_raise(mode_env)) return mode_env;
    const json_t *cfg_mode = (config && config->type == JSON_OBJECT) ? json_obj_get(config, "reply_to_mode") : NULL;
    const json_t *pick = ba_truthy(mode_env) ? mode_env : (ba_truthy(cfg_mode) ? cfg_mode : NULL);
    char *ms = pick ? ba_pystr(pick) : ba_strdup("first");
    char *mstrip = ba_strip(ms); free(ms);
    char *mode = ba_lower(mstrip); free(mstrip);
    if (mode_env) json_free(mode_env);
    json_t *rit = ba_setting_or_c(ctx, "BUZZ_REPLY_IN_THREAD", extra, "reply_in_thread", NULL);
    if (ba_is_raise(rit)) { free(mode); return rit; }
    bool off = false;
    if (!ba_is_none(rit)) {
        char *rs = ba_pystr(rit);
        char *rstrip = ba_strip(rs); free(rs);
        char *rlow = ba_lower(rstrip); free(rstrip);
        off = strcmp(rlow, "false") == 0 || strcmp(rlow, "0") == 0
            || strcmp(rlow, "no") == 0 || strcmp(rlow, "off") == 0;
        free(rlow);
    }
    if (rit) json_free(rit);
    json_t *ret = json_string(off ? "off" : mode);
    free(mode);
    return ret;
}

/* PoP: ba__resolve_credentials_data @ plugins/platforms/buzz/adapter.py:_resolve_credentials_data */
static json_t *ba_resolve_credentials_data_c(ba_ctx_t *ctx, const json_t *extra) {
    json_t *ca = ba_args(); ba_set_copy(ca, "extra", extra);
    json_t *candidates = ba_call(ctx, "_credentials_candidates", ca);
    if (ba_is_raise(candidates)) return candidates;
    if (!candidates || candidates->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", ba_type_name(candidates));
        if (candidates) json_free(candidates);
        return ba_err("TypeError", msg);
    }
    json_t *result = NULL;
    for (size_t i = 0; i < json_len(candidates); i++) {
        const json_t *path = json_get(candidates, i);
        const char *ps = (path && path->type == JSON_OBJECT) ? json_get_str(path, "path", "") : "";
        json_t *ra = ba_args(); ba_set_s(ra, "path", ps); ba_set_s(ra, "encoding", "utf-8-sig");
        json_t *content = ba_call(ctx, "read_text", ra);
        if (ba_is_raise(content)) {
            const char *k = ba_kind(content);
            if (ba_is_base_exc(k)) { json_free(content); json_free(candidates); return content; }
            if (strcmp(k, "OSError") == 0 || strcmp(k, "ValueError") == 0) { json_free(content); continue; }
            json_free(content); json_free(candidates); return content;
        }
        const char *cs = ba_str(content);
        json_t *data = cs ? ba_json_loads(cs) : NULL;
        if (content) json_free(content);
        if (!data) continue; /* json.loads ValueError caught */
        if (data->type == JSON_OBJECT) {
            json_t *key = ba_credentials_key_c(data);
            bool has = key && key->type == JSON_STRING && key->str_val && key->str_val[0];
            if (key) json_free(key);
            if (has) { result = data; break; }
        }
        json_free(data);
    }
    json_free(candidates);
    return result ? result : json_object();
}
static json_t *ba__resolve_credentials_data(ba_ctx_t *ctx, const json_t *args) {
    return ba_resolve_credentials_data_c(ctx, json_obj_get(args, "extra"));
}

/* PoP: ba__resolve_private_key @ plugins/platforms/buzz/adapter.py:_resolve_private_key */
static json_t *ba__resolve_private_key(ba_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    json_t *a = ba_args(); ba_set_s(a, "name", "BUZZ_PRIVATE_KEY"); ba_set_s(a, "default", "");
    json_t *secret = ba_call(ctx, "_get_scoped_secret", a);
    if (ba_is_raise(secret)) return secret;
    char *raw = ba_pystr_or_empty(secret);
    if (secret) json_free(secret);
    char *key = ba_strip(raw); free(raw);
    if (key[0]) { json_t *r = json_string(key); free(key); return r; }
    free(key);
    json_t *data = ba_resolve_credentials_data_c(ctx, extra);
    if (ba_is_raise(data)) return data;
    json_t *ck = ba_credentials_key_c(data);
    json_free(data);
    return ck;
}
