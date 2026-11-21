/*
 * slack_adapter_flow12.c -- FLOW cores, wave 11, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's still-unclaimed remainder after flow1-11. Every ported name was
 * UNCLAIMED by a strict PoP grep of the whole main-only repo at port time
 * (each carries its PoP annotation at its definition below). No flow1-11
 * function is re-ported.
 *
 * Ported (16 functions):
 *   _post_interactive_blocks, _prefilter_inbound, _react, _reaction_thread_ts,
 *   _read_error_text_limited, _record_uploaded_file_thread, _refuse_other_requester,
 *   _register_bolt_handlers, _register_plugin_action_handlers,
 *   _remember_processed_message_ts, _remove_reaction, _render_message_text,
 *   _resolve_author_team, _resolve_channel_name, _resolve_download_token,
 *   _slack_reaction_triggers.
 *
 * Decision sequences only: every injected/unported callable enters through ONE
 * universal seam sa12_call(ctx, name, args). The harness wires it to a recording
 * stub fed in.io; the oracle installs recorders emitting the same
 * {"name","args","ret"} event shape from the SAME scripts. Ordered trace,
 * BaseException passthrough, fail-closed. Self-contained: libjson only.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow12.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - A raise is {"__raise__": cls, "message": str(exc), ...extra}.
 * - isinstance/except families walk the full builtin exception table below.
 * - BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) PASS THROUGH every except-Exception site.
 * - Fail-closed: a missing/exhausted seam script is a RuntimeError raise
 *   spec recorded in the trace before propagation.
 * - Mutable self state (sets, gate env, auth marker, assistant-thread map)
 *   lives in ctx.state and is echoed as out.state. Sets travel as deduped
 *   arrays (insertion order); the fixture domain keeps membership order-stable.
 * - Regex-only transformations in a pure helper are modeled as named seams
 *   (each sub is a scripted callable), so no PCRE dependency is introduced.
 * - UID sets are processed in the SCRIPTED order returned by find_user_mentions
 *   (fixture domain: order-insensitive mentions).
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct sa12_ctx {
    json_t *state;      /* owned; echoed as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa12_ctx_t;

/* -- basic helpers -- */
static bool sa12_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sa12_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}
static json_t *sa12_obj_attrs(const json_t *v) {
    return sa12_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}
static bool sa12_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}
static bool sa12_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}
static bool sa12_collection_has_str(const json_t *coll, const char *s) {
    if (!s || !coll) return false;
    if (sa12_is_set(coll)) coll = json_obj_get(coll, "__set__");
    if (!coll) return false;
    if (coll->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(coll); i++) {
            const json_t *mi = json_get(coll, i); const char *m = (mi && mi->type == JSON_STRING) ? mi->str_val : NULL;
            if (m && strcmp(m, s) == 0) return true;
        }
        return false;
    }
    if (coll->type == JSON_OBJECT) return json_obj_get(coll, s) != NULL;
    return false;
}
static bool sa12_plain_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa12_is_obj(v) &&
           !sa12_is_set(v) && !sa12_is_tuple_marker(v) && !sa12_is_raise(v);
}
static bool sa12_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa12_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}
static const char *sa12_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static char *sa12_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static json_t *sa12_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *sa12_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = sa12_err(type, msg);
    if (spec && spec->type == JSON_OBJECT) {
        for (size_t i = 0; i < spec->c.count; i++) {
            const char *k = spec->c.keys ? spec->c.keys[i] : NULL;
            if (!k || strcmp(k, "type") == 0 || strcmp(k, "message") == 0) continue;
            json_set(e, k, json_copy(spec->c.items[i]));
        }
    }
    return e;
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *sa12_call(sa12_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa12_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa12_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa12_make_exc(raise);
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

/* -- full builtin exception hierarchy (CPython 3.14, quarry) -- */
typedef struct sa12_exc_parent { const char *name, *parent1, *parent2; } sa12_exc_parent_t;
static const sa12_exc_parent_t SA11_EXC_TABLE[] = {
    {"ArithmeticError", "Exception", ""},
    {"AssertionError", "Exception", ""},
    {"AttributeError", "Exception", ""},
    {"BaseException", "object", ""},
    {"BaseExceptionGroup", "BaseException", ""},
    {"BlockingIOError", "OSError", ""},
    {"BrokenPipeError", "ConnectionError", ""},
    {"BufferError", "Exception", ""},
    {"BytesWarning", "Warning", ""},
    {"CancelledError", "BaseException", ""},
    {"ChildProcessError", "OSError", ""},
    {"ConnectionAbortedError", "ConnectionError", ""},
    {"ConnectionError", "OSError", ""},
    {"ConnectionRefusedError", "ConnectionError", ""},
    {"ConnectionResetError", "ConnectionError", ""},
    {"DeprecationWarning", "Warning", ""},
    {"EOFError", "Exception", ""},
    {"EncodingWarning", "Warning", ""},
    {"EnvironmentError", "OSError", ""},
    {"Exception", "BaseException", ""},
    {"ExceptionGroup", "BaseExceptionGroup", "Exception"},
    {"FileExistsError", "OSError", ""},
    {"FileNotFoundError", "OSError", ""},
    {"FloatingPointError", "ArithmeticError", ""},
    {"FutureWarning", "Warning", ""},
    {"GeneratorExit", "BaseException", ""},
    {"IOError", "OSError", ""},
    {"ImportError", "Exception", ""},
    {"ImportWarning", "Warning", ""},
    {"IndentationError", "SyntaxError", ""},
    {"IndexError", "LookupError", ""},
    {"InterruptedError", "OSError", ""},
    {"IsADirectoryError", "OSError", ""},
    {"KeyError", "LookupError", ""},
    {"KeyboardInterrupt", "BaseException", ""},
    {"LookupError", "Exception", ""},
    {"MemoryError", "Exception", ""},
    {"ModuleNotFoundError", "ImportError", ""},
    {"NameError", "Exception", ""},
    {"NotADirectoryError", "OSError", ""},
    {"NotImplementedError", "RuntimeError", ""},
    {"OSError", "Exception", ""},
    {"OverflowError", "ArithmeticError", ""},
    {"PendingDeprecationWarning", "Warning", ""},
    {"PermissionError", "OSError", ""},
    {"ProcessLookupError", "OSError", ""},
    {"PythonFinalizationError", "RuntimeError", ""},
    {"RecursionError", "RuntimeError", ""},
    {"ReferenceError", "Exception", ""},
    {"ResourceWarning", "Warning", ""},
    {"RuntimeError", "Exception", ""},
    {"RuntimeWarning", "Warning", ""},
    {"StopAsyncIteration", "Exception", ""},
    {"StopIteration", "Exception", ""},
    {"SyntaxError", "Exception", ""},
    {"SyntaxWarning", "Warning", ""},
    {"SystemError", "Exception", ""},
    {"SystemExit", "BaseException", ""},
    {"TabError", "IndentationError", ""},
    {"TimeoutError", "OSError", ""},
    {"TypeError", "Exception", ""},
    {"UnboundLocalError", "NameError", ""},
    {"UnicodeDecodeError", "UnicodeError", ""},
    {"UnicodeEncodeError", "UnicodeError", ""},
    {"UnicodeError", "ValueError", ""},
    {"UnicodeTranslateError", "UnicodeError", ""},
    {"UnicodeWarning", "Warning", ""},
    {"UserWarning", "Warning", ""},
    {"ValueError", "Exception", ""},
    {"Warning", "Exception", ""},
    {"WindowsError", "OSError", ""},
    {"ZeroDivisionError", "ArithmeticError", ""},
    {"_IncompleteInputError", "SyntaxError", ""},
};
#define SA11_EXC_TABLE_LEN (sizeof(SA11_EXC_TABLE) / sizeof(SA11_EXC_TABLE[0]))

static const sa12_exc_parent_t *sa12_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA11_EXC_TABLE_LEN; i++)
        if (strcmp(SA11_EXC_TABLE[i].name, name) == 0) return &SA11_EXC_TABLE[i];
    return NULL;
}
typedef struct sa12_chain { char names[64][96]; size_t n; } sa12_chain_t;
static bool sa12_chain_has(const sa12_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}
static void sa12_chain_add(sa12_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa12_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}
static void sa12_chain_walk(sa12_chain_t *c, const char *name) {
    if (!name || !name[0] || sa12_chain_has(c, name)) return;
    sa12_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const sa12_exc_parent_t *be = sa12_builtin_entry(name);
    const char *p1 = be ? be->parent1 : "Exception";
    const char *p2 = be ? be->parent2 : NULL;
    if (p1 && strcmp(p1, name) != 0) sa12_chain_walk(c, p1);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa12_chain_walk(c, p2);
}
static bool sa12_exc_family(const json_t *r, const char *family) {
    if (!sa12_is_raise(r)) return false;
    sa12_chain_t c; c.n = 0;
    sa12_chain_walk(&c, json_get_str(r, "__raise__", "Exception"));
    return sa12_chain_has(&c, family);
}
static bool sa12_raise_passes(const json_t *r) {
    return sa12_is_raise(r) && !sa12_exc_family(r, "Exception");
}

/* -- ASCII whitespace + string kernels -- */
static bool sa12_is_ws(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *sa12_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa12_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa12_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa12_lstrip_char(const char *s, char ch) {
    if (!s) return NULL;
    while (*s == ch) s++;
    return sa12_strdup(s);
}
static char *sa12_strip_chars(const char *s, const char *chars) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && strchr(chars, s[a]) != NULL) a++;
    while (b > a && strchr(chars, s[b - 1]) != NULL) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa12_lower(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (out) {
        for (size_t i = 0; i < n; i++)
            out[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] - 'A' + 'a') : s[i];
        out[n] = '\0';
    }
    return out;
}
/* Python str() of a scalar (ASCII domain: str/int/bool/None only). */
static char *sa12_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa12_strdup("None");
    if (v->type == JSON_STRING) return sa12_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return sa12_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return sa12_strdup(buf);
    }
    return sa12_strdup("");
}
static char *sa12_replace(const char *s, const char *from, const char *to) {
    if (!s) return NULL;
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return sa12_strdup(s);
    size_t cap = strlen(s) + 1, used = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    const char *p = s, *q;
    while ((q = strstr(p, from)) != NULL) {
        size_t pre = (size_t)(q - p);
        size_t need = used + pre + tl + 1;
        if (need > cap) { cap = need * 2; char *nb = (char *)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; }
        memcpy(out + used, p, pre); used += pre;
        memcpy(out + used, to, tl); used += tl;
        p = q + fl;
    }
    size_t tail = strlen(p);
    size_t need = used + tail + 1;
    if (need > cap) { char *nb = (char *)realloc(out, need); if (!nb) { free(out); return NULL; } out = nb; }
    memcpy(out + used, p, tail); used += tail;
    out[used] = '\0';
    return out;
}
static bool sa12_startswith(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool sa12_is_num(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}
static double sa12_num(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* -- local helpers -- */
static json_t *sa12_o(void) { return json_object(); }
static void sa12_ss(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
static void sa12_sc(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, json_copy(v ? v : json_null()));
}
static void sa12_sb(json_t *o, const char *k, bool b) { json_set(o, k, json_bool(b)); }
static void sa12_sn(json_t *o, const char *k, double n) { json_set(o, k, json_number(n)); }
static const char *sa12_s(const json_t *v, const char *def) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static long long sa12_to_int(const json_t *v, long long def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
}
static long long sa12_gi(const json_t *o, const char *k, long long def) {
    return sa12_to_int(o ? json_obj_get(o, k) : NULL, def);
}
static bool sa12_gb(const json_t *o, const char *k, bool def) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    return v && v->type == JSON_BOOL ? v->bool_val : def;
}
static json_t *sa12_get(const json_t *obj, const char *key, const json_t *def) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return json_copy(v ? v : def);
}
static json_t *sa12_arr(const json_t *a, size_t i) {
    if (!a || a->type != JSON_ARRAY || i >= json_len(a)) return json_null();
    return json_copy(json_get(a, i));
}
static bool sa12_json_eq(const json_t *a, const char *s) {
    const char *v = sa12_str(a);
    return v && strcmp(v, s) == 0;
}
/* str(arg or "") via pystr. */
static char *sa12_strval(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa12_strdup("");
    return sa12_pystr(v);
}

/* -- mutable self-dict state helpers (echoed as out.state) -- */
static json_t *sa12_sget(sa12_ctx_t *ctx, const char *name, const char *key, const json_t *def) {
    json_t *d = json_obj_get(ctx->state, name);
    json_t *v = d && d->type == JSON_OBJECT ? json_obj_get(d, key) : NULL;
    return json_copy(v ? v : def);
}
static bool sa12_shas(sa12_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    return d && d->type == JSON_OBJECT && json_obj_get(d, key) != NULL;
}
static json_t *sa12_spop(sa12_ctx_t *ctx, const char *name, const char *key, const json_t *def) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_OBJECT) { d = json_object(); json_set(ctx->state, name, d); }
    json_t *v = json_obj_get(d, key);
    json_t *r = json_copy(v ? v : def);
    if (v) json_obj_del(d, key);
    return r;
}
static void sa12_sset(sa12_ctx_t *ctx, const char *name, const char *key, const json_t *v) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_OBJECT) { d = json_object(); json_set(ctx->state, name, d); }
    json_set(d, key, json_copy(v ? v : json_null()));
}
static void sa12_sdel(sa12_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    if (d && d->type == JSON_OBJECT) json_obj_del(d, key);
}

/* Exception-family raise inside a try -> logger.warn/debug/error + return "". */
static json_t *sa12_try_log(sa12_ctx_t *ctx, json_t *r, const char *label, const char *seam) {
    json_t *w = sa12_o(); sa12_ss(w, "label", label);
    sa12_sc(w, "error", json_obj_get(r, "message"));
    json_free(r);
    json_t *lr = sa12_call(ctx, seam, w);
    if (sa12_is_raise(lr)) return lr;
    json_free(lr);
    return json_string("");
}

/* t() -> identity of the key (deterministic stand-in, both ports). */
static json_t *sa12_t(sa12_ctx_t *ctx, const char *key, const json_t *args) {
    (void)ctx; (void)args;
    return json_string(key ? key : "");
}

/* ====================================================================== */

/* -- state-set helpers (deduped arrays; echo in out.state) -- */
static bool sa12_set_has_str(sa12_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_ARRAY || !key) return false;
    for (size_t i = 0; i < json_len(d); i++) {
        const char *v = sa12_str(json_get(d, i));
        if (v && strcmp(v, key) == 0) return true;
    }
    return false;
}
static void sa12_set_add(sa12_ctx_t *ctx, const char *name, const json_t *val) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_ARRAY) {
        if (d) json_obj_del(ctx->state, name);
        d = json_array(); json_set(ctx->state, name, d);
    }
    const char *ks = sa12_str(val);
    if (ks && sa12_set_has_str(ctx, name, ks)) return;
    json_append(d, json_copy(val ? val : json_null()));
}
static json_t *sa12_copy_or_null(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_null();
    return json_copy(v);
}
static bool sa12_env_yes(const char *s) {
    return s && (strcmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcmp(s, "yes") == 0);
}

/* ---- extra helpers for wave 12 ---- */
static bool sa12_contains(const char *hay, const char *needle) {
    if (!hay || !needle) return false;
    return strstr(hay, needle) != NULL;
}
/* Python str(p).strip().strip(":") */
static char *sa12_colon_strip(const json_t *v) {
    char *a = sa12_strval(v);
    char *b = sa12_strip(a);
    size_t n = strlen(b);
    while (n > 0 && b[n - 1] == ':') b[--n] = '\0';
    size_t s = 0;
    while (b[s] == ':') s++;
    if (s) memmove(b, b + s, n - s + 1);
    free(a);
    return b;
}
static int sa12_cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}
/* {"__set__": sorted-unique of str(p).strip().strip(":")} */
static json_t *sa12_set_from(const json_t *arr) {
    json_t *out = sa12_o();
    json_t *sa = json_array();
    if (arr && arr->type == JSON_ARRAY) {
        char **items = (char **)calloc(json_len(arr) + 1, sizeof(char *));
        size_t n = 0;
        for (size_t i = 0; i < json_len(arr); i++) {
            char *v = sa12_colon_strip(json_get(arr, i));
            if (v[0]) {
                bool dup = false;
                for (size_t j = 0; j < n; j++) if (strcmp(items[j], v) == 0) { dup = true; break; }
                if (!dup) items[n++] = v;
                else free(v);
            } else free(v);
        }
        if (n > 1) qsort(items, n, sizeof(char *), sa12_cmp_str);
        for (size_t i = 0; i < n; i++) { json_append(sa, json_string(items[i])); free(items[i]); }
        free(items);
    }
    json_set(out, "__set__", sa);
    return out;
}
/* nested (obj.get(key) or {}).get(idkey, "") -- plain dict access */
static const char *sa12_nested_id(const json_t *obj, const char *key, const char *idkey) {
    const json_t *n = obj ? json_obj_get(obj, key) : NULL;
    if (!n || n->type != JSON_OBJECT) return "";
    const json_t *v = json_obj_get(n, idkey);
    return sa12_s(v, "");
}

/* PoP: sa12_read_error_text_limited @ plugins/platforms/slack/adapter.py:_read_error_text_limited */
static json_t *sa12_read_error_text_limited(sa12_ctx_t *ctx, const json_t *args) {
    json_t *resp = sa12_get(args, "response", json_null());
    if (!resp || resp->type != JSON_OBJECT) { json_free(resp); resp = json_object(); }
    long long lim = sa12_gi(args, "limit", 8192);
    json_t *content = json_obj_get(resp, "content");
    bool callable = content && content->type == JSON_OBJECT &&
                    sa12_truthy(json_obj_get(content, "read"));
    if (!callable) {
        json_t *aw = sa12_o();
        json_t *r = sa12_call(ctx, "response_text", aw);
        if (sa12_is_raise(r)) { json_free(resp); return r; }
        char *s = sa12_strval(r);
        json_free(r); json_free(resp);
        size_t n = strlen(s);
        if ((long long)n > lim) s[lim < 0 ? 0 : (size_t)lim] = '\0';
        json_t *out = json_string(s); free(s); return out;
    }
    json_t *chunks = json_array();
    long long total = 0;
    while (total <= lim) {
        long long size = 4096, cap = lim + 1 - total;
        if (cap < size) size = cap;
        json_t *aw = sa12_o(); sa12_sn(aw, "size", (double)size);
        json_t *chunk = sa12_call(ctx, "read_chunk", aw);
        if (sa12_is_raise(chunk)) { json_free(chunks); json_free(resp); return chunk; }
        if (!sa12_truthy(chunk)) { json_free(chunk); break; }
        char *s = sa12_strval(chunk);
        json_append(chunks, json_string(s));
        total += (long long)strlen(s);
        free(s); json_free(chunk);
    }
    if (total > lim && sa12_truthy(json_obj_get(resp, "release"))) {
        json_t *rw = sa12_o();
        json_t *rr = sa12_call(ctx, "release_response", rw);
        if (sa12_is_raise(rr)) { json_free(chunks); json_free(resp); return rr; }
        json_free(rr);
    }
    json_free(resp);
    size_t cap = 0;
    for (size_t i = 0; i < json_len(chunks); i++) cap += strlen(sa12_s(json_get(chunks, i), ""));
    char *buf = (char *)calloc(cap + 1, 1);
    size_t pos = 0;
    for (size_t i = 0; i < json_len(chunks); i++) {
        const char *s = sa12_s(json_get(chunks, i), "");
        size_t n = strlen(s);
        memcpy(buf + pos, s, n); pos += n;
    }
    if ((long long)pos > lim) buf[lim < 0 ? 0 : (size_t)lim] = '\0';
    json_t *out = json_string(buf);
    free(buf); json_free(chunks);
    return out;
}

/* PoP: sa12_react @ plugins/platforms/slack/adapter.py:_react */
static json_t *sa12_react(sa12_ctx_t *ctx, const json_t *args, bool remove) {
    json_t *h = sa12_call(ctx, "has_app", sa12_o());
    if (sa12_is_raise(h)) return h;
    if (!sa12_truthy(h)) { json_free(h); return json_bool(false); }
    json_free(h);
    json_t *gw = sa12_o();
    sa12_sc(gw, "channel", json_obj_get(args, "channel"));
    const json_t *tid = json_obj_get(args, "team_id");
    json_set(gw, "team_id", (tid && sa12_truthy(tid)) ? json_copy(tid) : json_null());
    json_t *client = sa12_call(ctx, "get_client", gw);
    if (sa12_is_raise(client)) return client;
    json_t *cw = sa12_o();
    sa12_sc(cw, "channel", json_obj_get(args, "channel"));
    sa12_sc(cw, "timestamp", json_obj_get(args, "timestamp"));
    sa12_sc(cw, "name", json_obj_get(args, "emoji"));
    json_set(cw, "client", json_copy(client));
    json_free(client);
    json_t *r = remove ? sa12_call(ctx, "reactions_remove", cw) : sa12_call(ctx, "reactions_add", cw);
    if (!sa12_is_raise(r)) { json_free(r); return json_bool(true); }
    if (sa12_raise_passes(r)) return r;
    json_t *msg = json_obj_get(r, "message");
    json_t *wm = sa12_o();
    sa12_ss(wm, "msg", "[Slack] reactions.%s failed (%s): %s");
    json_t *ar2 = json_array();
    json_append(ar2, json_string(remove ? "remove" : "add"));
    json_append(ar2, json_obj_get(args, "emoji") ? json_copy(json_obj_get(args, "emoji")) : json_null());
    json_append(ar2, json_copy(msg));
    json_set(wm, "args", ar2);
    json_free(r);
    json_t *d = sa12_call(ctx, "logger.debug", wm);
    if (sa12_is_raise(d)) return d;
    json_free(d);
    return json_bool(false);
}

/* harness dispatch wrapper: _react is add-mode; _remove_reaction calls the
 * same core in remove-mode. */
static json_t *sa12_react_dispatch(sa12_ctx_t *ctx, const json_t *args) {
    return sa12_react(ctx, args, false);
}

/* PoP: sa12_remove_reaction @ plugins/platforms/slack/adapter.py:_remove_reaction */
static json_t *sa12_remove_reaction(sa12_ctx_t *ctx, const json_t *args) {
    return sa12_react(ctx, args, true);
}

/* PoP: sa12_record_uploaded_file_thread @ plugins/platforms/slack/adapter.py:_record_uploaded_file_thread */
static json_t *sa12_record_uploaded_file_thread(sa12_ctx_t *ctx, const json_t *args) {
    if (!sa12_truthy(json_obj_get(args, "thread_ts"))) return json_null();
    json_t *aw = sa12_o();
    json_set(aw, "metadata", json_obj_get(args, "metadata") ? json_copy(json_obj_get(args, "metadata")) : json_null());
    json_t *team = sa12_call(ctx, "metadata_team_id", aw);
    if (sa12_is_raise(team)) return team;
    json_t *mw = sa12_o();
    json_set(mw, "team_id", json_copy(team));
    sa12_sc(mw, "ts", json_obj_get(args, "thread_ts"));
    json_free(team);
    json_t *marker = sa12_call(ctx, "workspace_message_marker", mw);
    if (sa12_is_raise(marker)) return marker;
    sa12_set_add(ctx, "_bot_message_ts", marker);
    json_free(marker);
    json_t *tw = sa12_o();
    const json_t *items = json_obj_get(ctx->state, "_bot_message_ts");
    json_set(tw, "items", items ? json_copy(items) : json_array());
    json_t *r = sa12_call(ctx, "trim_bot_message_timestamps", tw);
    if (sa12_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

typedef struct sa12_pair { const char *key; json_t *val; double num; size_t idx; } sa12_pair_t;
static int sa12_cmp_pair(const void *a, const void *b) {
    const sa12_pair_t *x = (const sa12_pair_t *)a, *y = (const sa12_pair_t *)b;
    if (x->num < y->num) return -1;
    if (x->num > y->num) return 1;
    return (x->idx < y->idx) ? -1 : (x->idx > y->idx ? 1 : 0);
}
/* PoP: sa12_remember_processed_message_ts @ plugins/platforms/slack/adapter.py:_remember_processed_message_ts */
static json_t *sa12_remember_processed_message_ts(sa12_ctx_t *ctx, const json_t *args) {
    const json_t *tsv = json_obj_get(args, "ts");
    if (!sa12_truthy(tsv)) return json_null();
    json_t *now = sa12_call(ctx, "time.time", sa12_o());
    if (sa12_is_raise(now)) return now;
    json_t *m = json_obj_get(ctx->state, "_processed_message_ts");
    if (!m || m->type != JSON_OBJECT) {
        m = json_object(); json_set(ctx->state, "_processed_message_ts", m);
    }
    json_set(m, sa12_s(tsv, ""), json_copy(now));
    json_free(now);
    long long mx = sa12_gi(args, "max_size", 5000);
    if (mx < 0) mx = 0;
    if ((long long)m->c.count > mx) {
        size_t n = m->c.count;
        sa12_pair_t *ps = (sa12_pair_t *)calloc(n + 1, sizeof(sa12_pair_t));
        for (size_t i = 0; i < n; i++) {
            ps[i].key = m->c.keys[i]; ps[i].val = json_copy(m->c.items[i]);
            ps[i].num = sa12_num(m->c.items[i], 0); ps[i].idx = i;
        }
        qsort(ps, n, sizeof(sa12_pair_t), sa12_cmp_pair);
        json_t *nm = json_object();
        size_t start = n - (size_t)mx;
        for (size_t i = start; i < n; i++) json_set(nm, ps[i].key, json_copy(ps[i].val));
        for (size_t i = 0; i < n; i++) json_free(ps[i].val);
        free(ps);
        json_obj_del(ctx->state, "_processed_message_ts");
        json_set(ctx->state, "_processed_message_ts", nm);
    }
    return json_null();
}

/* PoP: sa12_refuse_other_requester @ plugins/platforms/slack/adapter.py:_refuse_other_requester */
static json_t *sa12_refuse_other_requester(sa12_ctx_t *ctx, const json_t *args) {
    const json_t *req = json_obj_get(args, "requester");
    if (!sa12_truthy(req)) return json_bool(false);
    json_t *body = sa12_get(args, "body", json_null());
    if (!body || body->type != JSON_OBJECT) { json_free(body); body = json_object(); }
    const char *ch = sa12_nested_id(body, "channel", "id");
    const char *uid = sa12_nested_id(body, "user", "id");
    json_t *ew = sa12_o();
    json_set(ew, "event", json_object());
    json_set(ew, "body", json_copy(body));
    json_t *team = sa12_call(ctx, "event_team_id", ew);
    if (sa12_is_raise(team)) { json_free(body); return team; }
    json_t *gw = sa12_o();
    sa12_ss(gw, "channel_id", ch);
    json_set(gw, "team_id", sa12_truthy(team) ? json_copy(team) : json_null());
    json_free(team);
    json_t *client = sa12_call(ctx, "get_client", gw);
    if (sa12_is_raise(client)) { json_free(body); return client; }
    json_t *rw = sa12_o(); sa12_sc(rw, "requester", req);
    json_t *text = sa12_call(ctx, "refusal_text", rw);
    if (sa12_is_raise(text)) { json_free(client); json_free(body); return text; }
    json_t *cw = sa12_o();
    sa12_ss(cw, "channel", ch);
    sa12_ss(cw, "user", uid);
    json_set(cw, "text", json_copy(text));
    const char *tts = sa12_nested_id(body, "message", "thread_ts");
    json_set(cw, "thread_ts", json_copy(json_obj_get(body, "message") ? json_obj_get(json_obj_get(body, "message"), "thread_ts") : json_null()));
    (void)tts;
    json_set(cw, "client", json_copy(client));
    json_free(text); json_free(client); json_free(body);
    json_t *r = sa12_call(ctx, "chat_postEphemeral", cw);
    if (sa12_is_raise(r)) return r;
    json_free(r);
    return json_bool(true);
}

/* PoP: sa12_post_interactive_blocks @ plugins/platforms/slack/adapter.py:_post_interactive_blocks */
static json_t *sa12_post_interactive_blocks(sa12_ctx_t *ctx, const json_t *args) {
    bool san = sa12_gb(args, "sanitize", true), tsc = sa12_gb(args, "team_scoped", true);
    json_t *kw = sa12_o();
    sa12_sc(kw, "channel", json_obj_get(args, "chat_id"));
    sa12_sc(kw, "text", json_obj_get(args, "text"));
    json_t *blocks = json_obj_get(args, "blocks") ? json_copy(json_obj_get(args, "blocks")) : json_null();
    if (san) {
        json_t *aw = sa12_o(); json_set(aw, "blocks", json_copy(blocks));
        json_t *sb = sa12_call(ctx, "sanitize_blocks", aw);
        if (sa12_is_raise(sb)) { json_free(blocks); json_free(kw); return sb; }
        json_free(blocks); blocks = json_copy(sb); json_free(sb);
    }
    json_set(kw, "blocks", blocks);
    json_t *tw = sa12_o();
    json_set(tw, "event", json_null());
    json_set(tw, "metadata", json_obj_get(args, "metadata") ? json_copy(json_obj_get(args, "metadata")) : json_null());
    json_t *ts = sa12_call(ctx, "resolve_thread_ts", tw);
    if (sa12_is_raise(ts)) { json_free(kw); return ts; }
    if (sa12_truthy(ts)) json_set(kw, "thread_ts", json_copy(ts));
    json_free(ts);
    json_t *team = NULL;
    if (tsc) {
        json_t *aw = sa12_o();
        json_set(aw, "metadata", json_obj_get(args, "metadata") ? json_copy(json_obj_get(args, "metadata")) : json_null());
        team = sa12_call(ctx, "metadata_team_id", aw);
        if (sa12_is_raise(team)) { json_free(kw); return team; }
    } else team = json_null();
    json_t *gw = sa12_o();
    sa12_sc(gw, "chat_id", json_obj_get(args, "chat_id"));
    json_set(gw, "team_id", json_copy(team));
    json_free(team);
    json_t *client = sa12_call(ctx, "get_client", gw);
    if (sa12_is_raise(client)) { json_free(kw); return client; }
    json_set(kw, "client", json_copy(client));
    json_free(client);
    return sa12_call(ctx, "chat_postMessage", kw);
}

/* PoP: sa12_render_message_text @ plugins/platforms/slack/adapter.py:_render_message_text */
static bool sa12_unseen(json_t *extras, const char *piece, const char *base) {
    if (sa12_contains(base, piece)) return false;
    for (size_t i = 0; i < json_len(extras); i++)
        if (sa12_contains(sa12_s(json_get(extras, i), ""), piece)) return false;
    return true;
}
static json_t *sa12_render_message_text(sa12_ctx_t *ctx, const json_t *args) {
    json_t *msg = sa12_get(args, "msg", json_null());
    if (!msg || msg->type != JSON_OBJECT) { json_free(msg); msg = json_object(); }
    const char *uid = sa12_s(json_obj_get(args, "bot_uid"), "");
    char *mt0 = sa12_strip(sa12_s(json_obj_get(msg, "text"), ""));
    char *mt = mt0;
    if (uid[0]) {
        char *pat = (char *)malloc(strlen(uid) + 8);
        snprintf(pat, strlen(uid) + 8, "<@%s>", uid);
        char *rep = sa12_replace(mt, pat, "");
        free(pat); free(mt0);
        mt = sa12_strip(rep); free(rep);
    }
    json_t *blocks = json_obj_get(msg, "blocks");
    bool has_blocks = blocks && blocks->type == JSON_ARRAY && json_len(blocks) > 0;
    json_t *extras = json_array();
    if (has_blocks) {
        json_t *aw = sa12_o();
        json_set(aw, "blocks", json_copy(blocks));
        sa12_ss(aw, "msg_text", mt);
        sa12_ss(aw, "bot_uid", uid);
        json_t *rt0 = sa12_call(ctx, "extract_additional_text_from_slack_blocks", aw);
        if (sa12_is_raise(rt0)) { json_free(extras); json_free(msg); free(mt); return rt0; }
        char *rt = sa12_strip(sa12_strval(rt0)); json_free(rt0);
        if (rt[0]) json_append(extras, json_string(rt));
        free(rt);
        for (size_t i = 0; i < json_len(blocks); i++) {
            json_t *blk = json_get(blocks, i);
            if (!blk || blk->type != JSON_OBJECT) continue;
            const char *ty = sa12_s(json_obj_get(blk, "type"), "");
            if (strcmp(ty, "section") && strcmp(ty, "header") && strcmp(ty, "context")) continue;
            json_t *to = json_obj_get(blk, "text");
            if (!to || to->type != JSON_OBJECT) continue;
            char *st = sa12_strip(sa12_s(json_obj_get(to, "text"), ""));
            if (st[0] && sa12_unseen(extras, st, mt)) json_append(extras, json_string(st));
            free(st);
        }
    }
    json_t *atts = json_obj_get(msg, "attachments");
    json_t *aw = sa12_o();
    json_set(aw, "attachments", (atts && atts->type == JSON_ARRAY) ? json_copy(atts) : json_array());
    sa12_ss(aw, "msg_text", mt);
    json_t *at0 = sa12_call(ctx, "extract_text_from_slack_attachments", aw);
    if (sa12_is_raise(at0)) { json_free(extras); json_free(msg); free(mt); return at0; }
    char *at = sa12_strip(sa12_strval(at0)); json_free(at0);
    if (at[0] && sa12_unseen(extras, at, mt)) json_append(extras, json_string(at));
    free(at);
    if (has_blocks) {
        json_t *uw = sa12_o(); sa12_ss(uw, "text", mt);
        json_t *raw0 = sa12_call(ctx, "unescape_slack_entities", uw);
        if (sa12_is_raise(raw0)) { json_free(extras); json_free(msg); free(mt); return raw0; }
        char *raw = sa12_strval(raw0); json_free(raw0);
        json_t *bw = sa12_o(); json_set(bw, "blocks", json_copy(blocks));
        json_t *urls = sa12_call(ctx, "extract_urls_from_slack_blocks", bw);
        if (sa12_is_raise(urls)) { free(raw); json_free(extras); json_free(msg); free(mt); return urls; }
        json_t *nu = json_array();
        for (size_t i = 0; i < json_len(urls); i++) {
            char *u = sa12_strval(json_get(urls, i));
            if (sa12_unseen(extras, u, raw)) json_append(nu, json_string(u));
            free(u);
        }
        json_free(urls);
        if (json_len(nu) > 0) {
            size_t cap = 16; for (size_t i = 0; i < json_len(nu); i++) cap += strlen(sa12_s(json_get(nu,i),"")) + 2;
            char *buf = (char *)calloc(cap + 1, 1);
            strcat(buf, "URLs: ");
            for (size_t i = 0; i < json_len(nu); i++) {
                if (i) strcat(buf, ", ");
                strcat(buf, sa12_s(json_get(nu, i), ""));
            }
            json_append(extras, json_string(buf)); free(buf);
        }
        json_free(nu); free(raw);
    }
    json_t *files = json_obj_get(msg, "files");
    bool has_files = files && files->type == JSON_ARRAY;
    json_t *markers = json_array();
    if (has_files) {
        for (size_t i = 0; i < json_len(files); i++) {
            json_t *f = json_get(files, i);
            if (!f || f->type != JSON_OBJECT) continue;
            json_t *fw = sa12_o(); json_set(fw, "file", json_copy(f));
            json_t *r = sa12_call(ctx, "slack_file_marker", fw);
            if (sa12_is_raise(r)) { json_free(markers); json_free(extras); json_free(msg); free(mt); return r; }
            char *s = sa12_strval(r); json_append(markers, json_string(s)); free(s); json_free(r);
        }
    }
    if (json_len(markers) > 0) {
        size_t cap = 1; for (size_t i = 0; i < json_len(markers); i++) cap += strlen(sa12_s(json_get(markers,i),"")) + 1;
        char *buf = (char *)calloc(cap + 1, 1);
        for (size_t i = 0; i < json_len(markers); i++) {
            if (i) strcat(buf, " ");
            strcat(buf, sa12_s(json_get(markers, i), ""));
        }
        json_append(extras, json_string(buf)); free(buf);
    }
    json_free(markers);
    json_t *out;
    if (json_len(extras) > 0) {
        size_t cap = 64; for (size_t i = 0; i < json_len(extras); i++) cap += strlen(sa12_s(json_get(extras,i),"")) + 2;
        char *add = (char *)calloc(cap + 1, 1);
        for (size_t i = 0; i < json_len(extras); i++) {
            if (i) strcat(add, "\n");
            strcat(add, sa12_s(json_get(extras, i), ""));
        }
        if (mt[0]) {
            size_t n = strlen(mt) + strlen(add) + 2;
            char *cmb = (char *)calloc(n, 1);
            snprintf(cmb, n, "%s\n%s", mt, add);
            char *sp = sa12_strip(cmb);
            out = json_string(sp); free(sp); free(cmb);
        } else out = json_string(add);
        free(add);
    } else out = json_string(mt);
    json_free(extras); json_free(msg); free(mt);
    return out;
}

/* PoP: sa12_resolve_author_team @ plugins/platforms/slack/adapter.py:_resolve_author_team */
static json_t *sa12_resolve_author_team(sa12_ctx_t *ctx, const json_t *args) {
    const char *uid = sa12_s(json_obj_get(args, "user_id"), "");
    if (!uid[0]) return json_string("");
    json_t *cache = json_obj_get(ctx->state, "_user_team_cache");
    if (!cache || cache->type != JSON_OBJECT) {
        cache = json_object(); json_set(ctx->state, "_user_team_cache", cache);
    }
    const char *tid = sa12_s(json_obj_get(args, "team_id"), "");
    char *key = (char *)malloc(strlen(tid) + strlen(uid) + 2);
    snprintf(key, strlen(tid) + strlen(uid) + 2, "%s|%s", tid, uid);
    if (sa12_shas(ctx, "_user_team_cache", key)) {
        json_t *v = sa12_sget(ctx, "_user_team_cache", key, json_string(""));
        free(key); return v;
    }
    json_t *tc = json_obj_get(ctx->state, "_team_clients");
    json_t *client = (tid[0] && tc && tc->type == JSON_OBJECT) ? json_obj_get(tc, tid) : NULL;
    bool haveclient = client && sa12_truthy(client);
    json_t *info;
    if (haveclient) {
        json_t *iw = sa12_o();
        sa12_ss(iw, "user", uid);
        json_set(iw, "client", json_copy(client));
        info = sa12_call(ctx, "users_info", iw);
    } else {
        json_t *gw = sa12_o(); sa12_sc(gw, "channel_id", json_obj_get(args, "channel_id"));
        json_t *c = sa12_call(ctx, "get_client", gw);
        if (sa12_is_raise(c)) { free(key); return c; }
        json_t *iw = sa12_o(); sa12_ss(iw, "user", uid); json_set(iw, "client", json_copy(c));
        json_free(c);
        info = sa12_call(ctx, "users_info", iw);
    }
    json_t *payload = NULL;
    if (!sa12_is_raise(info)) {
        json_t *pw = sa12_o(); json_set(pw, "response", json_copy(info));
        json_free(info);
        payload = sa12_call(ctx, "slack_response_payload", pw);
    } else payload = info;
    if (sa12_is_raise(payload)) {
        if (sa12_raise_passes(payload)) { free(key); return payload; }
        json_t *dw = sa12_o();
        sa12_ss(dw, "label", "users.info team lookup");
        sa12_ss(dw, "user_id", uid);
        sa12_sc(dw, "error", json_obj_get(payload, "message"));
        json_free(payload);
        json_t *r = sa12_call(ctx, "logger.debug", dw);
        if (sa12_is_raise(r)) { free(key); return r; }
        json_free(r); free(key); return json_string("");
    }
    json_t *user = json_obj_get(payload, "user");
    char *ut = sa12_strdup((user && user->type == JSON_OBJECT) ? sa12_s(json_obj_get(user, "team_id"), "") : "");
    json_free(payload);
    if (!ut[0] || ut[0] != 'T') { free(ut); free(key); return json_string(""); }
    sa12_sset(ctx, "_user_team_cache", key, json_string(ut));
    json_t *tw = sa12_o();
    json_set(tw, "items", json_copy(json_obj_get(ctx->state, "_user_team_cache")));
    sa12_sn(tw, "max_size", 5000);
    json_t *r = sa12_call(ctx, "trim_oldest_dict_entries", tw);
    if (sa12_is_raise(r)) { free(key); return r; }
    json_free(r); free(key);
    json_t *utret = json_string(ut); free(ut); return utret;
}

/* PoP: sa12_reaction_thread_ts @ plugins/platforms/slack/adapter.py:_reaction_thread_ts */
static json_t *sa12_reaction_thread_ts(sa12_ctx_t *ctx, const json_t *args) {
    const char *tts = sa12_s(json_obj_get(args, "msg_ts"), "");
    char *cur = sa12_strdup(tts);
    json_t *ev = sa12_get(args, "event", json_null());
    if (!ev || ev->type != JSON_OBJECT) { json_free(ev); ev = json_object(); }
    const char *item = sa12_s(json_obj_get(ev, "item_user"), "");
    char *iu = sa12_strdup(item);
    const json_t *client = json_obj_get(args, "client");
    if (client && client->type != JSON_NULL) {
        json_t *rw = sa12_o();
        sa12_sc(rw, "channel", json_obj_get(args, "channel_id"));
        sa12_sc(rw, "ts", json_obj_get(args, "msg_ts"));
        sa12_sn(rw, "limit", 1); sa12_sb(rw, "inclusive", true);
        json_set(rw, "client", json_copy(client));
        json_t *hist = sa12_call(ctx, "conversations_replies", rw);
        if (sa12_is_raise(hist)) {
            if (sa12_raise_passes(hist)) { free(cur); free(iu); json_free(ev); return hist; }
            json_t *dw = sa12_o();
            sa12_ss(dw, "label", "reaction thread_ts lookup");
            sa12_sc(dw, "msg_ts", json_obj_get(args, "msg_ts"));
            sa12_sc(dw, "error", json_obj_get(hist, "message"));
            json_free(hist);
            json_t *r = sa12_call(ctx, "logger.debug", dw);
            if (sa12_is_raise(r)) { free(cur); free(iu); json_free(ev); return r; }
            json_free(r); json_free(ev); return json_string(cur);
        }
        json_t *msgs = json_obj_get(hist, "messages");
        if (!msgs || msgs->type != JSON_ARRAY || json_len(msgs) == 0) {
            json_free(hist); free(iu); json_free(ev); return json_string(cur);
        }
        json_t *first = json_get(msgs, 0);
        if (!first || first->type != JSON_OBJECT) first = NULL;
        if (first) {
            const char *a = sa12_s(json_obj_get(first, "thread_ts"), "");
            const char *b = sa12_s(json_obj_get(first, "ts"), "");
            if (a[0]) { free(cur); cur = sa12_strdup(a); }
            else if (b[0]) { free(cur); cur = sa12_strdup(b); }
            if (!iu[0]) { const char *u = sa12_s(json_obj_get(first, "user"), ""); if (u[0]) { free(iu); iu = sa12_strdup(u); } }
        }
        json_free(hist);
    }
    json_free(ev);
    bool allow = sa12_gb(args, "explicit_allowlist", false);
    if (!allow) {
        const char *tid = sa12_s(json_obj_get(args, "team_id"), "");
        json_t *tb = json_obj_get(ctx->state, "_team_bot_user_ids");
        const char *bot = (tb && tb->type == JSON_OBJECT) ? sa12_s(json_obj_get(tb, tid), "") : "";
        if (!bot[0]) bot = sa12_s(json_obj_get(ctx->state, "_bot_user_id"), "");
        if (iu[0] && bot[0] && strcmp(iu, bot) != 0) { free(cur); free(iu); return json_null(); }
    }
    free(iu);
    return json_string(cur);
}

/* PoP: sa12_resolve_channel_name @ plugins/platforms/slack/adapter.py:_resolve_channel_name */
static json_t *sa12_resolve_channel_name(sa12_ctx_t *ctx, const json_t *args) {
    const char *cid = sa12_s(json_obj_get(args, "channel_id"), "");
    if (!cid[0]) return json_string(cid);
    const char *prov = sa12_s(json_obj_get(args, "team_id"), "");
    const char *tid = prov;
    if (!tid[0]) {
        json_t *ct = json_obj_get(ctx->state, "_channel_team");
        tid = (ct && ct->type == JSON_OBJECT) ? sa12_s(json_obj_get(ct, cid), "") : "";
    }
    char *key = (char *)malloc(strlen(tid) + strlen(cid) + 2);
    snprintf(key, strlen(tid) + strlen(cid) + 2, "%s|%s", tid, cid);
    json_t *cache = json_obj_get(ctx->state, "_channel_name_cache");
    if (!cache || cache->type != JSON_OBJECT) {
        cache = json_object(); json_set(ctx->state, "_channel_name_cache", cache);
    }
    if (sa12_shas(ctx, "_channel_name_cache", key)) {
        json_t *v = sa12_sget(ctx, "_channel_name_cache", key, json_null());
        if (!sa12_is_raise(v) && v->type != JSON_NULL) { free(key); return v; }
        json_free(v);
    }
    json_t *h = sa12_call(ctx, "has_app", sa12_o());
    if (sa12_is_raise(h)) { free(key); return h; }
    if (!sa12_truthy(h)) { json_free(h); free(key); return json_string(cid); }
    json_free(h);
    json_t *gw = sa12_o();
    sa12_ss(gw, "channel_id", cid);
    json_set(gw, "team_id", tid[0] ? json_string(tid) : json_null());
    json_t *client = sa12_call(ctx, "get_client", gw);
    if (sa12_is_raise(client)) { free(key); return client; }
    json_t *iw = sa12_o();
    sa12_ss(iw, "channel", cid); json_set(iw, "client", json_copy(client)); json_free(client);
    json_t *resp = sa12_call(ctx, "conversations_info", iw);
    json_t *payload = NULL;
    if (!sa12_is_raise(resp)) {
        json_t *pw = sa12_o(); json_set(pw, "response", json_copy(resp)); json_free(resp);
        payload = sa12_call(ctx, "slack_response_payload", pw);
    } else payload = resp;
    char *name = NULL;
    if (sa12_is_raise(payload)) {
        if (sa12_raise_passes(payload)) { free(key); return payload; }
        json_t *dw = sa12_o();
        sa12_ss(dw, "label", "conversations.info");
        sa12_ss(dw, "channel_id", cid);
        sa12_sc(dw, "error", json_obj_get(payload, "message"));
        json_free(payload);
        json_t *r = sa12_call(ctx, "logger.debug", dw);
        if (sa12_is_raise(r)) { free(key); return r; }
        json_free(r);
        name = sa12_strdup(cid);
    } else {
        json_t *ch = json_obj_get(payload, "channel");
        bool ok = sa12_truthy(json_obj_get(payload, "ok"));
        if (!ok) name = sa12_strdup(cid);
        else if (ch && ch->type == JSON_OBJECT && sa12_truthy(json_obj_get(ch, "is_im"))) {
            const char *peer = sa12_s(json_obj_get(ch, "user"), "");
            if (peer[0]) {
                json_t *uw = sa12_o();
                sa12_ss(uw, "user", peer); sa12_ss(uw, "chat_id", cid); sa12_ss(uw, "team_id", tid);
                json_t *r = sa12_call(ctx, "resolve_user_name", uw);
                if (sa12_is_raise(r)) { free(key); json_free(payload); return r; }
                name = sa12_strval(r); json_free(r);
            } else name = sa12_strdup(cid);
        } else {
            const char *n = sa12_s(json_obj_get(ch, "name"), "");
            if (!n[0]) n = sa12_s(json_obj_get(ch, "name_normalized"), "");
            name = sa12_strdup(n[0] ? n : cid);
        }
        json_free(payload);
    }
    sa12_sset(ctx, "_channel_name_cache", key, json_string(name));
    json_t *tw = sa12_o();
    json_set(tw, "items", json_copy(json_obj_get(ctx->state, "_channel_name_cache")));
    sa12_sn(tw, "max_size", 5000);
    json_t *r = sa12_call(ctx, "trim_oldest_dict_entries", tw);
    if (sa12_is_raise(r)) { free(name); free(key); return r; }
    json_free(r); free(key);
    json_t *ret = json_string(name); free(name);
    return ret;
}

/* PoP: sa12_prefilter_inbound @ plugins/platforms/slack/adapter.py:_prefilter_inbound */
static json_t *sa12_prefilter_inbound(sa12_ctx_t *ctx, const json_t *args) {
    json_t *event = sa12_get(args, "event", json_null());
    if (!event || event->type != JSON_OBJECT) { json_free(event); event = json_object(); }
    json_t *payload = sa12_get(args, "payload", json_null());
    json_t *h = sa12_call(ctx, "is_enabled_debug", sa12_o());
    if (sa12_is_raise(h)) { json_free(event); json_free(payload); return h; }
    if (sa12_truthy(h)) {
        json_t *dw = sa12_o();
        sa12_ss(dw, "msg", "[Slack] event received");
        json_set(dw, "event", json_copy(event));
        json_t *bp = json_obj_get(event, "bot_profile");
        const char *bn = (bp && bp->type == JSON_OBJECT) ? sa12_s(json_obj_get(bp, "name"), "") : "";
        sa12_ss(dw, "bot_name", bn);
        json_t *r = sa12_call(ctx, "logger.debug", dw);
        if (sa12_is_raise(r)) { json_free(event); json_free(payload); return r; }
        json_free(r);
    }
    json_free(h);
    if (strcmp(sa12_s(json_obj_get(event, "subtype"), ""), "message_changed") == 0) {
        json_t *nw = sa12_o(); json_set(nw, "event", json_copy(event));
        json_t *ne = sa12_call(ctx, "normalize_changed_message", nw);
        if (sa12_is_raise(ne)) { json_free(event); json_free(payload); return ne; }
        if (ne->type == JSON_NULL) { json_free(ne); json_free(event); json_free(payload); return json_null(); }
        json_free(event); event = json_copy(ne); json_free(ne);
    }
    const char *ets = sa12_s(json_obj_get(event, "_slack_changed_event_ts"), "");
    if (!ets[0]) ets = sa12_s(json_obj_get(event, "ts"), "");
    json_t *tc = json_obj_get(ctx->state, "_team_clients");
    bool has_tc = tc && tc->type == JSON_OBJECT && tc->c.count > 0;
    json_t *ew = sa12_o();
    json_set(ew, "event", json_copy(event));
    json_set(ew, "payload", json_copy(payload));
    sa12_sb(ew, "install_set", has_tc);
    json_t *tv = sa12_call(ctx, "event_team_id", ew);
    if (sa12_is_raise(tv)) { json_free(event); json_free(payload); return tv; }
    const char *tid = sa12_s(tv, "");
    if (has_tc && !tid[0]) {
        json_t *iw = sa12_o(); sa12_ss(iw, "msg", "[Slack] Ignoring event with no authenticated workspace");
        json_free(tv);
        json_t *r = sa12_call(ctx, "logger.info", iw);
        if (sa12_is_raise(r)) { json_free(event); json_free(payload); return r; }
        json_free(r); json_free(event); json_free(payload); return json_null();
    }
    if (ets[0]) {
        json_t *ww = sa12_o(); sa12_ss(ww, "team_id", tid); sa12_ss(ww, "ts", ets);
        json_t *wid = sa12_call(ctx, "workspace_event_id", ww);
        if (sa12_is_raise(wid)) { json_free(event); json_free(payload); return wid; }
        json_t *dw = sa12_o(); json_set(dw, "id", json_copy(wid)); json_free(wid);
        json_t *dup = sa12_call(ctx, "is_duplicate", dw);
        if (sa12_is_raise(dup)) { json_free(event); json_free(payload); return dup; }
        if (sa12_truthy(dup)) { json_free(dup); json_free(event); json_free(payload); return json_null(); }
        json_free(dup);
    }
    const char *cid = sa12_s(json_obj_get(event, "channel"), "");
    json_t *iw = sa12_o(); sa12_ss(iw, "channel_id", cid);
    json_t *ign = sa12_call(ctx, "is_ignored_channel", iw);
    if (sa12_is_raise(ign)) { json_free(event); json_free(payload); return ign; }
    if (sa12_truthy(ign)) {
        json_free(ign);
        json_t *lw = sa12_o(); sa12_ss(lw, "msg", "[Slack] Ignoring message in configured ignored channel");
        sa12_ss(lw, "channel_id", cid);
        json_t *r = sa12_call(ctx, "logger.info", lw);
        if (sa12_is_raise(r)) { json_free(event); json_free(payload); return r; }
        json_free(r); json_free(event); json_free(payload); return json_null();
    }
    json_free(ign);
    const char *author = "";
    const char *FLDS[2] = {"user_team", "team"};
    for (int i = 0; i < 2; i++) {
        const char *v = sa12_s(json_obj_get(event, FLDS[i]), "");
        if (v[0] == 'T') { author = v; break; }
    }
    const char *ctype = sa12_s(json_obj_get(event, "channel_type"), "");
    const char *rsrc = sa12_s(json_obj_get(event, "_hermes_reaction_source_channel"), "");
    bool is_dm = (strcmp(ctype, "im") == 0 || cid[0] == 'D' || rsrc[0] == 'D');
    if (is_dm) {
        json_t *fw = sa12_o(); sa12_ss(fw, "author_team", author); sa12_ss(fw, "team_id", tid);
        json_t *foreign = sa12_call(ctx, "is_foreign_author", fw);
        if (sa12_is_raise(foreign)) { json_free(event); json_free(payload); return foreign; }
        if (sa12_truthy(foreign)) {
            json_free(foreign);
            json_t *lw = sa12_o(); sa12_ss(lw, "msg", "[Slack] Ignoring DM from external-org user");
            json_t *r = sa12_call(ctx, "logger.info", lw);
            if (sa12_is_raise(r)) { json_free(event); json_free(payload); return r; }
            json_free(r); json_free(event); json_free(payload); return json_null();
        }
        json_free(foreign);
    }
    json_t *bw = sa12_o(); json_set(bw, "event", json_copy(event));
    json_t *bot = sa12_call(ctx, "drop_bot_sender", bw);
    if (sa12_is_raise(bot)) { json_free(event); json_free(payload); return bot; }
    if (sa12_truthy(bot)) { json_free(bot); json_free(event); json_free(payload); return json_null(); }
    json_free(bot);
    const json_t *st = json_obj_get(event, "subtype");
    bool allowed = (!st || st->type == JSON_NULL);
    if (!allowed) {
        const char *sv = sa12_s(st, "SET");
        const char *OK[6] = {"", "file_share", "thread_broadcast", "me_message", "bot_message", "document_mention"};
        for (int i = 0; i < 6; i++) if (strcmp(sv, OK[i]) == 0) { allowed = true; break; }
    }
    if (!allowed) {
        json_t *dw = sa12_o(); sa12_ss(dw, "label", "drop subtype");
        json_set(dw, "subtype", st ? json_copy(st) : json_null());
        sa12_ss(dw, "channel_id", cid);
        json_t *r = sa12_call(ctx, "logger.debug", dw);
        if (sa12_is_raise(r)) { json_free(event); json_free(payload); return r; }
        json_free(r); json_free(event); json_free(payload); return json_null();
    }
    json_t *out = json_array();
    json_append(out, json_copy(event));
    json_append(out, json_string(tid));
    json_append(out, json_string(cid));
    json_free(event); json_free(payload);
    return out;
}

/* PoP: sa12_register_bolt_handlers @ plugins/platforms/slack/adapter.py:_register_bolt_handlers */
static json_t *sa12_register_bolt_handlers(sa12_ctx_t *ctx, const json_t *args) {
    (void)args;
    static const char *PAIRS[][2] = {
        {"message", "_handle_slack_message"}, {"app_mention", "_handle_slack_message"},
        {"app_home_opened", "_handle_app_home_opened"},
        {"app_context_changed", "_handle_app_context_changed"},
        {"file_shared", "_handle_slack_file_shared"}, {"file_created", "_noop"},
        {"file_change", "_noop"}, {"reaction_added", "_reaction(False)"},
        {"reaction_removed", "_reaction(True)"},
        {"assistant_thread_started", "_handle_assistant_thread_lifecycle_event"},
        {"assistant_thread_context_changed", "_handle_assistant_thread_lifecycle_event"}}
    ;
    for (size_t i = 0; i < 11; i++) {
        json_t *w = sa12_o(); sa12_ss(w, "event_type", PAIRS[i][0]); sa12_ss(w, "handler", PAIRS[i][1]);
        json_t *r = sa12_call(ctx, "app_event", w);
        if (sa12_is_raise(r)) return r;
        json_free(r);
    }
    json_t *paw = sa12_o(); sa12_ss(paw, "pattern", ".*");
    json_t *pat = sa12_call(ctx, "re_compile", paw);
    if (sa12_is_raise(pat)) return pat;
    json_t *cw = sa12_o(); json_set(cw, "event_type", json_copy(pat)); sa12_ss(cw, "handler", "handle_unhandled_event");
    json_free(pat);
    json_t *r = sa12_call(ctx, "app_event", cw);
    if (sa12_is_raise(r)) return r; json_free(r);
    json_t *names = sa12_call(ctx, "slack_native_slashes", sa12_o());
    if (sa12_is_raise(names)) return names;
    json_t *nw = sa12_o(); json_set(nw, "names", json_copy(names)); json_free(names);
    json_t *slash = sa12_call(ctx, "re_compile", nw);
    if (sa12_is_raise(slash)) return slash;
    json_t *cw2 = sa12_o(); json_set(cw2, "pattern", json_copy(slash)); sa12_ss(cw2, "handler", "_handle_hermes_command");
    json_free(slash);
    r = sa12_call(ctx, "app_command", cw2);
    if (sa12_is_raise(r)) return r; json_free(r);
    static const char *LIST_KEYS[2] = {"_APPROVAL_CHOICES", "_CONFIRM_CHOICES"};
    static const char *LIST_HANDLERS[2] = {"_handle_approval_action", "_handle_slash_confirm_action"};
    for (int k = 0; k < 2; k++) {
        json_t *lst = json_obj_get(ctx->state, LIST_KEYS[k]);
        if (lst && lst->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(lst); i++) {
                json_t *w = sa12_o(); json_set(w, "action_id", json_copy(json_get(lst, i)));
                sa12_ss(w, "handler", LIST_HANDLERS[k]);
                json_t *rr = sa12_call(ctx, "app_action", w);
                if (sa12_is_raise(rr)) return rr; json_free(rr);
            }
        }
    }
    json_t *aw = sa12_o(); sa12_ss(aw, "action_id", "hermes_feedback"); sa12_ss(aw, "handler", "_handle_feedback_action");
    r = sa12_call(ctx, "app_action", aw); if (sa12_is_raise(r)) return r; json_free(r);
    json_t *clarw = sa12_o(); sa12_ss(clarw, "pattern", "^hermes_clarify_choice_\\d+$");
    json_t *clar = sa12_call(ctx, "re_compile", clarw);
    if (sa12_is_raise(clar)) return clar;
    aw = sa12_o(); json_set(aw, "action_id", json_copy(clar)); sa12_ss(aw, "handler", "_handle_clarify_action");
    json_free(clar);
    r = sa12_call(ctx, "app_action", aw); if (sa12_is_raise(r)) return r; json_free(r);
    aw = sa12_o(); sa12_ss(aw, "action_id", "hermes_clarify_other"); sa12_ss(aw, "handler", "_handle_clarify_action");
    r = sa12_call(ctx, "app_action", aw); if (sa12_is_raise(r)) return r; json_free(r);
    json_t *mlst = json_obj_get(ctx->state, "_MODEL_PICKER_ACTION_IDS");
    if (mlst && mlst->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(mlst); i++) {
            json_t *w = sa12_o(); json_set(w, "action_id", json_copy(json_get(mlst, i)));
            sa12_ss(w, "handler", "_handle_model_picker_action");
            json_t *rr = sa12_call(ctx, "app_action", w);
            if (sa12_is_raise(rr)) return rr; json_free(rr);
        }
    }
    r = sa12_call(ctx, "register_vault_code_handlers", sa12_o());
    if (sa12_is_raise(r)) return r; json_free(r);
    r = sa12_call(ctx, "register_plugin_action_handlers", sa12_o());
    if (sa12_is_raise(r)) return r; json_free(r);
    r = sa12_call(ctx, "wire_plugin_handlers", sa12_o());
    if (sa12_is_raise(r)) return r; json_free(r);
    return json_null();
}

/* PoP: sa12_register_plugin_action_handlers @ plugins/platforms/slack/adapter.py:_register_plugin_action_handlers */
static json_t *sa12_register_plugin_action_handlers(sa12_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *handlers = sa12_call(ctx, "get_slack_action_handlers", sa12_o());
    if (sa12_is_raise(handlers)) {
        if (sa12_raise_passes(handlers)) return handlers;
        json_t *ww = sa12_o();
        sa12_ss(ww, "label", "Could not load plugin action handlers");
        sa12_sc(ww, "error", json_obj_get(handlers, "message"));
        json_free(handlers);
        json_t *r = sa12_call(ctx, "logger.warning", ww);
        if (sa12_is_raise(r)) return r; json_free(r);
        handlers = json_array();
    }
    if (!handlers || handlers->type != JSON_ARRAY) { json_free(handlers); handlers = json_array(); }
    const json_t *app = json_obj_get(ctx->state, "_app");
    json_t *cur = json_obj_get(ctx->state, "_plugin_actions_app");
    bool same = (cur == NULL && app == NULL) || (cur && app && sa12_json_eq(cur, sa12_s(app, "")));
    if (!same) {
        json_set(ctx->state, "_plugin_actions_app", app ? json_copy(app) : json_null());
        json_set(ctx->state, "_plugin_actions_wired", json_object());
    }
    json_t *wired = json_obj_get(ctx->state, "_plugin_actions_wired");
    if (!wired || wired->type != JSON_OBJECT) {
        wired = json_object(); json_set(ctx->state, "_plugin_actions_wired", wired);
    }
    size_t fresh = 0;
    for (size_t i = 0; i < json_len(handlers); i++) {
        json_t *h = json_get(handlers, i);
        if (!h || h->type != JSON_ARRAY || json_len(h) < 3) continue;
        const json_t *aid = json_get(h, 0), *nm = json_get(h, 2);
        char *akey = sa12_strval(aid), *nkey = sa12_strval(nm);
        char *key = (char *)malloc(strlen(akey) + strlen(nkey) + 2);
        snprintf(key, strlen(akey) + strlen(nkey) + 2, "%s|%s", akey, nkey);
        free(akey); free(nkey);
        if (sa12_shas(ctx, "_plugin_actions_wired", key)) { free(key); continue; }
        json_t *aw = sa12_o(); json_set(aw, "action_id", json_copy(aid)); sa12_ss(aw, "handler", "_make_wrapper");
        json_t *r = sa12_call(ctx, "app_action", aw);
        if (sa12_is_raise(r)) { free(key); json_free(handlers); return r; }
        json_free(r);
        sa12_sset(ctx, "_plugin_actions_wired", key, json_bool(true));
        json_t *dw = sa12_o(); json_set(dw, "action_id", json_copy(aid)); json_set(dw, "plugin_name", json_copy(nm));
        r = sa12_call(ctx, "logger.debug", dw);
        if (sa12_is_raise(r)) { free(key); json_free(handlers); return r; }
        json_free(r); free(key); fresh++;
    }
    json_free(handlers);
    if (fresh) {
        json_t *iw = sa12_o(); sa12_sn(iw, "count", (double)fresh);
        json_t *r = sa12_call(ctx, "logger.info", iw);
        if (sa12_is_raise(r)) return r; json_free(r);
    }
    return json_null();
}

/* PoP: sa12_resolve_download_token @ plugins/platforms/slack/adapter.py:_resolve_download_token */
static json_t *sa12_resolve_download_token(sa12_ctx_t *ctx, const json_t *args) {
    json_t *tc = json_obj_get(ctx->state, "_team_clients");
    const char *tid = sa12_s(json_obj_get(args, "team_id"), "");
    if (tid[0] && tc && tc->type == JSON_OBJECT && sa12_shas(ctx, "_team_clients", tid)) {
        json_t *e = json_obj_get(tc, tid);
        return json_string((e && e->type == JSON_OBJECT) ? sa12_s(json_obj_get(e, "token"), "") : "");
    }
    json_t *rw = sa12_o();
    sa12_ss(rw, "pattern", "/files-pri/(T[A-Z0-9]+)-");
    sa12_ss(rw, "url", sa12_s(json_obj_get(args, "url"), ""));
    json_t *m = sa12_call(ctx, "re_search", rw);
    if (sa12_is_raise(m)) {
        if (sa12_raise_passes(m)) return m;
        json_free(m);
    } else {
        const char *g = (m && m->type == JSON_OBJECT) ? sa12_s(json_obj_get(m, "group1"), "") : "";
        json_free(m);
        if (g[0] && tc && tc->type == JSON_OBJECT && sa12_shas(ctx, "_team_clients", g)) {
            json_t *e = json_obj_get(tc, g);
            return json_string((e && e->type == JSON_OBJECT) ? sa12_s(json_obj_get(e, "token"), "") : "");
        }
    }
    json_t *cfg = json_obj_get(ctx->state, "config");
    return json_string((cfg && cfg->type == JSON_OBJECT) ? sa12_s(json_obj_get(cfg, "token"), "") : "");
}

/* PoP: sa12_slack_reaction_triggers @ plugins/platforms/slack/adapter.py:_slack_reaction_triggers */
static json_t *sa12_slack_reaction_triggers(sa12_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cfg = json_obj_get(ctx->state, "config");
    json_t *extra = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "extra") : NULL;
    const json_t *raw = (extra && extra->type == JSON_OBJECT) ? json_obj_get(extra, "reaction_triggers") : NULL;
    bool is_null = !raw || raw->type == JSON_NULL;
    if (is_null) {
        json_t *sw = sa12_o(); sa12_ss(sw, "key", "SLACK_REACTION_TRIGGERS");
        json_t *sec = sa12_call(ctx, "get_scoped_secret", sw);
        if (sa12_is_raise(sec)) return sec;
        if (sec->type == JSON_NULL) { json_free(sec); return json_null(); }
        raw = sec;
        is_null = false;
        /* raw borrowed from sec kept alive until end of function (not freed) */
    }
    if (is_null) return json_null();
    if (raw->type == JSON_BOOL) {
        json_t *out = sa12_o(); json_set(out, "__set__", json_array());
        if (raw->bool_val) return out;
        json_free(out); return json_null();
    }
    if (raw->type == JSON_ARRAY) return sa12_set_from(raw);
    char *tv = sa12_pystr(raw);
    char *text = sa12_strip(tv); free(tv);
    char *low = sa12_lower(text);
    static const char *OFF[4] = {"false", "0", "no", "off"};
    static const char *ON[6] = {"true", "1", "yes", "on", "all", "*"};
    bool off = !text[0];
    for (int i = 0; i < 4; i++) if (strcmp(low, OFF[i]) == 0) off = true;
    bool on = false;
    for (int i = 0; i < 6; i++) if (strcmp(low, ON[i]) == 0) on = true;
    free(low);
    if (off) { free(text); return json_null(); }
    if (on) { free(text); json_t *out = sa12_o(); json_set(out, "__set__", json_array()); return out; }
    json_t *sw = sa12_o(); sa12_ss(sw, "text", text); sa12_ss(sw, "pattern", "[,\\s]+");
    free(text);
    json_t *parts = sa12_call(ctx, "re_split", sw);
    if (sa12_is_raise(parts)) return parts;
    json_t *out = sa12_set_from(parts); json_free(parts);
    return out;
}

