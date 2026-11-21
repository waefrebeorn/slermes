/*
 * slack_adapter_flow14.c -- FLOW cores, wave 14, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's still-unclaimed remainder after flow1-13. Every ported name was
 * UNCLAIMED by a strict PoP grep of the whole main-only repo at port time
 * (each carries its PoP annotation at its definition below). No flow1-13
 * function is re-ported.
 *
 * Ported (16 functions):
 *   _serialize_slack_blocks_for_agent, _set_assistant_suggested_prompts, _set_assistant_thread_title,
 *   _should_wake_on_unmentioned_message, _slack_api_human_users, _slack_json_post,
 *   _slack_mention_patterns, _slack_message_addressed_to_other_user, _slack_message_mentions_self,
 *   _slack_per_request_proxy_middleware, _socket_transport_connected, _socket_watchdog_loop,
 *   _standalone_send, _standalone_send_media, _synthetic_reaction_event,
 *   _thread_watermark_io.
 *
 * Decision sequences only: every injected/unported callable enters through ONE
 * universal seam sa14_call(ctx, name, args). The harness wires it to a recording
 * stub fed in.io; the oracle installs recorders emitting the same
 * {"name","args","ret"} event shape from the SAME scripts. Ordered trace,
 * BaseException passthrough, fail-closed. Self-contained: libjson only.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow14.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - A raise is {"__raise__": cls, "message": str(exc), ...extra}.
 * - isinstance/except families walk the full builtin exception table below.
 * - BaseException-only kinds pass through every except-Exception site.
 * - Fail-closed: a missing/exhausted seam script is a RuntimeError raise
 *   spec recorded in the trace before propagation.
 * - Mutable self state lives in ctx.state and is echoed as out.state.
 * - t() translations are identity (both sides), never a seam.
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
typedef struct sa14_ctx {
    json_t *state;      /* owned; echoed as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa14_ctx_t;

/* -- basic helpers -- */
static bool sa14_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sa14_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}
static json_t *sa14_obj_attrs(const json_t *v) {
    return sa14_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}
static bool sa14_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}
static bool sa14_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}
static bool sa14_collection_has_str(const json_t *coll, const char *s) {
    if (!s || !coll) return false;
    if (sa14_is_set(coll)) coll = json_obj_get(coll, "__set__");
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
static bool sa14_plain_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa14_is_obj(v) &&
           !sa14_is_set(v) && !sa14_is_tuple_marker(v) && !sa14_is_raise(v);
}
static bool sa14_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa14_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}
static const char *sa14_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static char *sa14_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static json_t *sa14_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *sa14_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = sa14_err(type, msg);
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
static json_t *sa14_call(sa14_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa14_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa14_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa14_make_exc(raise);
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
typedef struct sa14_exc_parent { const char *name, *parent1, *parent2; } sa14_exc_parent_t;
static const sa14_exc_parent_t SA14_EXC_TABLE[] = {
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
#define SA14_EXC_TABLE_LEN (sizeof(SA14_EXC_TABLE) / sizeof(SA14_EXC_TABLE[0]))

static const sa14_exc_parent_t *sa14_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA14_EXC_TABLE_LEN; i++)
        if (strcmp(SA14_EXC_TABLE[i].name, name) == 0) return &SA14_EXC_TABLE[i];
    return NULL;
}
typedef struct sa14_chain { char names[64][96]; size_t n; } sa14_chain_t;
static bool sa14_chain_has(const sa14_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}
static void sa14_chain_add(sa14_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa14_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}
static void sa14_chain_walk(sa14_chain_t *c, const char *name) {
    if (!name || !name[0] || sa14_chain_has(c, name)) return;
    sa14_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const sa14_exc_parent_t *be = sa14_builtin_entry(name);
    const char *p1 = be ? be->parent1 : "Exception";
    const char *p2 = be ? be->parent2 : NULL;
    if (p1 && strcmp(p1, name) != 0) sa14_chain_walk(c, p1);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa14_chain_walk(c, p2);
}
static bool sa14_exc_family(const json_t *r, const char *family) {
    if (!sa14_is_raise(r)) return false;
    sa14_chain_t c; c.n = 0;
    sa14_chain_walk(&c, json_get_str(r, "__raise__", "Exception"));
    return sa14_chain_has(&c, family);
}
static bool sa14_raise_passes(const json_t *r) {
    return sa14_is_raise(r) && !sa14_exc_family(r, "Exception");
}

/* -- ASCII whitespace + string kernels -- */
static bool sa14_is_ws(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *sa14_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa14_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa14_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa14_lstrip_char(const char *s, char ch) {
    if (!s) return NULL;
    while (*s == ch) s++;
    return sa14_strdup(s);
}
static char *sa14_strip_chars(const char *s, const char *chars) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && strchr(chars, s[a]) != NULL) a++;
    while (b > a && strchr(chars, s[b - 1]) != NULL) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa14_lower(const char *s) {
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
static char *sa14_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa14_strdup("None");
    if (v->type == JSON_STRING) return sa14_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return sa14_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return sa14_strdup(buf);
    }
    return sa14_strdup("");
}
static char *sa14_replace(const char *s, const char *from, const char *to) {
    if (!s) return NULL;
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return sa14_strdup(s);
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
static bool sa14_startswith(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool sa14_is_num(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}
static double sa14_num(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* -- local helpers -- */
static json_t *sa14_o(void) { return json_object(); }
static void sa14_ss(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
static void sa14_sc(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, json_copy(v ? v : json_null()));
}
static void sa14_sb(json_t *o, const char *k, bool b) { json_set(o, k, json_bool(b)); }
static void sa14_sn(json_t *o, const char *k, double n) { json_set(o, k, json_number(n)); }
static const char *sa14_s(const json_t *v, const char *def) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static long long sa14_to_int(const json_t *v, long long def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
}
static long long sa14_gi(const json_t *o, const char *k, long long def) {
    return sa14_to_int(o ? json_obj_get(o, k) : NULL, def);
}
static bool sa14_gb(const json_t *o, const char *k, bool def) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    return v && v->type == JSON_BOOL ? v->bool_val : def;
}
static json_t *sa14_get(const json_t *obj, const char *key, const json_t *def) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return json_copy(v ? v : def);
}
static json_t *sa14_arr(const json_t *a, size_t i) {
    if (!a || a->type != JSON_ARRAY || i >= json_len(a)) return json_null();
    return json_copy(json_get(a, i));
}
static bool sa14_json_eq(const json_t *a, const char *s) {
    const char *v = sa14_str(a);
    return v && strcmp(v, s) == 0;
}
/* str(arg or "") via pystr. */
static char *sa14_strval(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa14_strdup("");
    return sa14_pystr(v);
}

/* -- mutable self-dict state helpers (echoed as out.state) -- */
static json_t *sa14_sget(sa14_ctx_t *ctx, const char *name, const char *key, const json_t *def) {
    json_t *d = json_obj_get(ctx->state, name);
    json_t *v = d && d->type == JSON_OBJECT ? json_obj_get(d, key) : NULL;
    return json_copy(v ? v : def);
}
static bool sa14_shas(sa14_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    return d && d->type == JSON_OBJECT && json_obj_get(d, key) != NULL;
}
static json_t *sa14_spop(sa14_ctx_t *ctx, const char *name, const char *key, const json_t *def) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_OBJECT) { d = json_object(); json_set(ctx->state, name, d); }
    json_t *v = json_obj_get(d, key);
    json_t *r = json_copy(v ? v : def);
    if (v) json_obj_del(d, key);
    return r;
}
static void sa14_sset(sa14_ctx_t *ctx, const char *name, const char *key, const json_t *v) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_OBJECT) { d = json_object(); json_set(ctx->state, name, d); }
    json_set(d, key, json_copy(v ? v : json_null()));
}
static void sa14_sdel(sa14_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    if (d && d->type == JSON_OBJECT) json_obj_del(d, key);
}

/* Exception-family raise inside a try -> logger.warn/debug/error + return "". */
static json_t *sa14_try_log(sa14_ctx_t *ctx, json_t *r, const char *label, const char *seam) {
    json_t *w = sa14_o(); sa14_ss(w, "label", label);
    sa14_sc(w, "error", json_obj_get(r, "message"));
    json_free(r);
    json_t *lr = sa14_call(ctx, seam, w);
    if (sa14_is_raise(lr)) return lr;
    json_free(lr);
    return json_string("");
}

/* t() -> identity of the key (deterministic stand-in, both ports). */
static json_t *sa14_t(sa14_ctx_t *ctx, const char *key, const json_t *args) {
    (void)ctx; (void)args;
    return json_string(key ? key : "");
}

/* ====================================================================== */

/* -- state-set helpers (deduped arrays; echo in out.state) -- */
static bool sa14_set_has_str(sa14_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_ARRAY || !key) return false;
    for (size_t i = 0; i < json_len(d); i++) {
        const char *v = sa14_str(json_get(d, i));
        if (v && strcmp(v, key) == 0) return true;
    }
    return false;
}
static void sa14_set_add(sa14_ctx_t *ctx, const char *name, const json_t *val) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_ARRAY) {
        if (d) json_obj_del(ctx->state, name);
        d = json_array(); json_set(ctx->state, name, d);
    }
    const char *ks = sa14_str(val);
    if (ks && sa14_set_has_str(ctx, name, ks)) return;
    json_append(d, json_copy(val ? val : json_null()));
}
static json_t *sa14_copy_or_null(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_null();
    return json_copy(v);
}
static bool sa14_env_yes(const char *s) {
    return s && (strcmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcmp(s, "yes") == 0);
}

/* ---- extra helpers for wave 14 ---- */
static bool sa14_contains(const char *hay, const char *needle) {
    if (!hay || !needle) return false;
    return strstr(hay, needle) != NULL;
}
/* Python str(p).strip().strip(":") */
static char *sa14_colon_strip(const json_t *v) {
    char *a = sa14_strval(v);
    char *b = sa14_strip(a);
    size_t n = strlen(b);
    while (n > 0 && b[n - 1] == ':') b[--n] = '\0';
    size_t s = 0;
    while (b[s] == ':') s++;
    if (s) memmove(b, b + s, n - s + 1);
    free(a);
    return b;
}
static int sa14_cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}
/* {"__set__": sorted-unique of str(p).strip().strip(":")} */
static json_t *sa14_set_from(const json_t *arr) {
    json_t *out = sa14_o();
    json_t *sa = json_array();
    if (arr && arr->type == JSON_ARRAY) {
        char **items = (char **)calloc(json_len(arr) + 1, sizeof(char *));
        size_t n = 0;
        for (size_t i = 0; i < json_len(arr); i++) {
            char *v = sa14_colon_strip(json_get(arr, i));
            if (v[0]) {
                bool dup = false;
                for (size_t j = 0; j < n; j++) if (strcmp(items[j], v) == 0) { dup = true; break; }
                if (!dup) items[n++] = v;
                else free(v);
            } else free(v);
        }
        if (n > 1) qsort(items, n, sizeof(char *), sa14_cmp_str);
        for (size_t i = 0; i < n; i++) { json_append(sa, json_string(items[i])); free(items[i]); }
        free(items);
    }
    json_set(out, "__set__", sa);
    return out;
}
/* nested (obj.get(key) or {}).get(idkey, "") -- plain dict access */
static const char *sa14_nested_id(const json_t *obj, const char *key, const char *idkey) {
    const json_t *n = obj ? json_obj_get(obj, key) : NULL;
    if (!n || n->type != JSON_OBJECT) return "";
    const json_t *v = json_obj_get(n, idkey);
    return sa14_s(v, "");
}


/* -- wave-13 extras -- */
static const char *sa14_emsg(const json_t *r) { return json_get_str(r, "message", ""); }
static json_t *sa14_sr(bool ok, const char *err, const json_t *mid) {
    json_t *o = sa14_o();
    sa14_sb(o, "success", ok);
    json_set(o, "error", err ? json_string(err) : json_null());
    json_set(o, "message_id", mid ? json_copy(mid) : json_null());
    return o;
}
static char *sa14_capitalize(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (i == 0 && c >= 'a' && c <= 'z') out[i] = (char)(c - 'a' + 'A');
        else out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
    }
    out[n] = '\0';
    return out;
}
static char *sa14_rstrip(const char *s) {
    char *v = sa14_strdup(s ? s : "");
    if (!v) return NULL;
    size_t n = strlen(v);
    while (n > 0 && sa14_is_ws((unsigned char)v[n - 1])) v[--n] = '\0';
    return v;
}
/* dict state map get as string, with "" default */
static const char *sa14_mapstr(const json_t *map, const char *key) {
    const json_t *v = (map && map->type == JSON_OBJECT) ? json_obj_get(map, key) : NULL;
    return sa14_s(v, "");
}






/* ---- wave-14 set / copy helpers ---- */
static bool sa14_arr_has_str(const json_t *arr, const char *s) {
    if (!arr || !s) return false;
    if (arr->type == JSON_OBJECT) return json_obj_get(arr, s) != NULL;
    if (arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *x = sa14_str(json_get(arr, i));
        if (x && strcmp(x, s) == 0) return true;
    }
    return false;
}
static void sa14_set_add_str(sa14_ctx_t *ctx, const char *key, const char *s) {
    json_t *arr = json_obj_get(ctx->state, key);
    if (!arr || arr->type != JSON_ARRAY) { arr = json_array(); json_set(ctx->state, key, arr); }
    if (s && !sa14_arr_has_str(arr, s)) json_append(arr, json_string(s));
}
static json_t *sa14_copy_obj(const json_t *o) { return o ? json_copy(o) : json_object(); }

/* ---- helpers specific to this wave ---- */
static bool sa14_sanitize_empty(const json_t *v) {
    if (!v || v->type == JSON_NULL) return true;
    if (v->type == JSON_STRING) return v->str_val == NULL || v->str_val[0] == '\0';
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count == 0;
    return false;
}
static bool sa14_str_in(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *x = sa14_str(json_get(arr, i));
        if (x && strcmp(x, s) == 0) return true;
    }
    return false;
}
static json_t *sa14_sanitize(const json_t *v, const json_t *sk, const json_t *rk) {
    if (!v) return json_null();
    if (v->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(v); i++) {
            json_t *item = sa14_sanitize(json_get(v, i), sk, rk);
            if (!sa14_sanitize_empty(item)) json_append(out, item); else json_free(item);
        }
        return out;
    }
    if (v->type == JSON_OBJECT) {
        json_t *out = json_object();
        for (size_t i = 0; i < v->c.count; i++) {
            const char *k = v->c.keys ? v->c.keys[i] : NULL;
            if (!k) continue;
            json_t *item = json_obj_get(v, k);
            if (sa14_str_in(sk, k)) json_set(out, k, json_copy(item));
            else if (sa14_str_in(rk, k)) {
                json_t *cleaned = sa14_sanitize(item, sk, rk);
                if (!sa14_sanitize_empty(cleaned)) json_set(out, k, cleaned); else json_free(cleaned);
            }
        }
        return out;
    }
    return json_copy(v);
}
static char *sa14_fmt_str(const char *pre, const char *a, const char *mid, const char *b, const char *post) {
    size_t n = strlen(pre) + strlen(a) + strlen(mid) + strlen(b) + strlen(post) + 1;
    char *s = (char *)malloc(n);
    if (s) snprintf(s, n, "%s%s%s%s%s", pre, a, mid, b, post);
    return s;
}

/* PoP: sa14_serialize_slack_blocks_for_agent @ plugins/platforms/slack/adapter.py:_serialize_slack_blocks_for_agent */
static json_t *sa14_serialize_slack_blocks_for_agent(sa14_ctx_t *ctx, const json_t *args) {
    const json_t *blocks = json_obj_get(args, "blocks");
    const json_t *sk = json_obj_get(args, "scalar_keys");
    const json_t *rk = json_obj_get(args, "recursive_keys");
    double maxc = json_get_num(args, "max_chars", 6000);
    json_t *inspectable = json_array();
    if (blocks && blocks->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(blocks); i++) {
            const json_t *b = json_get(blocks, i);
            const char *ty = (b && b->type == JSON_OBJECT) ? sa14_str(json_obj_get(b, "type")) : NULL;
            if (ty && (strcmp(ty, "rich_text") == 0 || strcmp(ty, "table") == 0)) continue;
            json_append(inspectable, json_copy(b));
        }
    }
    if (json_len(inspectable) == 0) { json_free(inspectable); return json_string(""); }
    json_t *san = sa14_sanitize(inspectable, sk, rk);
    json_t *ja = sa14_o(); sa14_sc(ja, "value", san); json_free(san);
    json_t *r = sa14_call(ctx, "json_dumps", ja); json_free(ja);
    char *payload;
    if (sa14_is_raise(r)) {
        if (sa14_raise_passes(r)) { json_free(inspectable); return r; }
        json_free(r);
        json_t *pa = sa14_o(); sa14_sc(pa, "value", inspectable);
        json_t *pr = sa14_call(ctx, "repr", pa); json_free(pa);
        if (sa14_is_raise(pr)) { json_free(inspectable); return pr; }
        payload = sa14_strdup(sa14_s(pr, NULL) ? sa14_s(pr, "") : "None");
        json_free(pr);
    } else {
        payload = sa14_strdup(sa14_s(r, NULL) ? sa14_s(r, "") : "None");
        json_free(r);
    }
    json_free(inspectable);
    json_t *ea = sa14_o(); sa14_ss(ea, "text", payload ? payload : ""); sa14_sn(ea, "max_chars", maxc);
    json_t *er = sa14_call(ctx, "elide", ea); json_free(ea);
    if (sa14_is_raise(er)) { free(payload); return er; }
    const char *el = sa14_s(er, NULL) ? sa14_s(er, "") : "None";
    char *res = sa14_fmt_str("[Slack Block Kit payload for this message]\n```json\n",
                             el, "\n```", "", "");
    json_free(er); free(payload);
    json_t *out = json_string(res ? res : ""); free(res);
    return out;
}

/* PoP: sa14_set_assistant_suggested_prompts @ plugins/platforms/slack/adapter.py:_set_assistant_suggested_prompts */
static json_t *sa14_set_assistant_suggested_prompts(sa14_ctx_t *ctx, const json_t *args) {
    const char *channel = sa14_s(json_obj_get(args, "channel_id"), "");
    const char *team = sa14_s(json_obj_get(args, "team_id"), "");
    const char *thread = sa14_s(json_obj_get(args, "thread_ts"), "");
    if (!sa14_truthy(json_obj_get(ctx->state, "_app")) || !channel[0]) return NULL;
    json_t *r = sa14_call(ctx, "assistant_suggested_prompts", sa14_o());
    if (sa14_is_raise(r)) return r;
    const json_t *title = sa14_arr(r, 0);
    const json_t *prompts = sa14_arr(r, 1);
    if (!sa14_truthy(prompts)) { json_free(r); return NULL; }
    json_t *kwargs = sa14_o();
    sa14_ss(kwargs, "channel_id", channel);
    sa14_sc(kwargs, "prompts", prompts);
    if (sa14_truthy(title)) sa14_sc(kwargs, "title", title);
    if (thread[0]) sa14_ss(kwargs, "thread_ts", thread);
    json_t *ca = sa14_o(); sa14_ss(ca, "channel_id", channel); sa14_ss(ca, "team_id", team);
    json_t *cl = sa14_call(ctx, "get_client", ca); json_free(ca);
    json_t *sr = NULL;
    if (sa14_is_raise(cl)) { sr = cl; }
    else { json_free(cl); sr = sa14_call(ctx, "assistant_threads_setSuggestedPrompts", kwargs); }
    json_free(kwargs);
    if (sa14_is_raise(sr)) {
        if (sa14_raise_passes(sr)) return sr;
        json_t *la = sa14_o();
        sa14_ss(la, "fmt", "[Slack] assistant.threads.setSuggestedPrompts failed: %s");
        sa14_sc(la, "error", json_obj_get(sr, "message"));
        json_t *lr = sa14_call(ctx, "logger.debug", la); json_free(la);
        if (sa14_is_raise(lr)) { json_free(sr); return lr; }
        json_free(lr); json_free(sr); return NULL;
    }
    json_free(sr); return NULL;
}

/* PoP: sa14_set_assistant_thread_title @ plugins/platforms/slack/adapter.py:_set_assistant_thread_title */
static json_t *sa14_set_assistant_thread_title(sa14_ctx_t *ctx, const json_t *args) {
    const char *channel = sa14_s(json_obj_get(args, "channel_id"), "");
    const char *thread = sa14_s(json_obj_get(args, "thread_ts"), "");
    const char *source = sa14_s(json_obj_get(args, "title_source"), "");
    const char *team = sa14_s(json_obj_get(args, "team_id"), "");
    if (!sa14_truthy(json_obj_get(ctx->state, "_app")) || !channel[0] || !thread[0] || !source[0]) return NULL;
    json_t *er = sa14_call(ctx, "assistant_thread_title_enabled", sa14_o());
    if (sa14_is_raise(er)) return er;
    if (!sa14_truthy(er)) { json_free(er); return NULL; }
    json_free(er);
    json_t *ka = sa14_o(); sa14_ss(ka, "team_id", team); sa14_ss(ka, "channel_id", channel); sa14_ss(ka, "thread_ts", thread);
    json_t *kr = sa14_call(ctx, "workspace_thread_key", ka); json_free(ka);
    if (sa14_is_raise(kr)) return kr;
    const char *key = sa14_s(kr, NULL);
    json_t *titled = json_obj_get(ctx->state, "_titled_assistant_threads");
    bool have_key = key && key[0];
    bool in_set = have_key && sa14_arr_has_str(titled, key);
    if (!have_key || in_set) { json_free(kr); return NULL; }
    /* re.sub(r"\s+"," ",source).strip() */
    char *collapsed = NULL;
    /* build collapsed whitespace */
    {
        size_t n = strlen(source); char *buf = (char *)malloc(n + 2);
        size_t j = 0; bool ws = false;
        if (buf) {
            for (size_t i = 0; i < n; i++) {
                unsigned char c = (unsigned char)source[i];
                if (sa14_is_ws(c)) { if (!ws && j > 0) buf[j++] = ' '; ws = true; }
                else { buf[j++] = source[i]; ws = false; }
            }
            if (j > 0 && buf[j-1] == ' ') j--;
            buf[j] = '\0';
        }
        collapsed = buf;
    }
    const char *title_src = collapsed ? collapsed : "";
    if (!title_src[0] || title_src[0] == '/') { free(collapsed); json_free(kr); return NULL; }
    size_t tl = strlen(title_src);
    char *final_title;
    if (tl > 80) {
        size_t k = 77;
        while (k > 0 && sa14_is_ws((unsigned char)title_src[k-1])) k--;
        final_title = sa14_fmt_str("", "", "", "", ""); /* overwritten below */
        free(final_title);
        size_t nn = k + 4;
        final_title = (char *)malloc(nn);
        if (final_title) { memcpy(final_title, title_src, k); memcpy(final_title+k, "...", 4); }
    } else { final_title = sa14_strdup(title_src); }
    free(collapsed);
    /* save key into state set on success */
    json_t *ca = sa14_o(); sa14_ss(ca, "channel_id", channel); sa14_ss(ca, "team_id", team);
    json_t *cl = sa14_call(ctx, "get_client", ca); json_free(ca);
    json_t *sm = NULL;
    if (sa14_is_raise(cl)) { sm = cl; }
    else {
        json_t *ma = sa14_o(); sa14_sc(ma, "client", cl); json_free(cl);
        json_t *mr = sa14_call(ctx, "session_title_method", ma); json_free(ma);
        if (sa14_is_raise(mr)) { sm = mr; }
        else {
            json_t *ta = sa14_o(); sa14_ss(ta, "channel_id", channel); sa14_ss(ta, "thread_ts", thread);
            sa14_sc(ta, "title", json_string(final_title ? final_title : ""));
            sm = sa14_call(ctx, "set_thread_title", ta); json_free(ta);
        }
    }
    free(final_title);
    if (sa14_is_raise(sm)) {
        if (sa14_raise_passes(sm)) { json_free(kr); return sm; }
        json_t *la = sa14_o(); sa14_ss(la, "fmt", "[Slack] session title set failed: %s");
        sa14_sc(la, "error", json_obj_get(sm, "message"));
        json_t *lr = sa14_call(ctx, "logger.debug", la); json_free(la);
        if (sa14_is_raise(lr)) { json_free(sm); json_free(kr); return lr; }
        json_free(lr); json_free(sm); json_free(kr); return NULL;
    }
    json_free(sm);
    sa14_set_add_str(ctx, "_titled_assistant_threads", key);
    json_t *ea = sa14_o(); sa14_ss(ea, "field", "e[2]");
    json_t *evr = sa14_call(ctx, "evict_oldest_by_ts", ea); json_free(ea);
    if (sa14_is_raise(evr)) { json_free(kr); return evr; }
    json_free(evr); json_free(kr);
    return NULL;
}

/* PoP: sa14_should_wake_on_unmentioned_message @ plugins/platforms/slack/adapter.py:_should_wake_on_unmentioned_message */
static json_t *sa14_should_wake_on_unmentioned_message(sa14_ctx_t *ctx, const json_t *args) {
    const char *ets = sa14_s(json_obj_get(args, "event_thread_ts"), "");
    if (!ets[0]) return json_bool(false);
    const char *team = sa14_s(json_obj_get(args, "team_id"), "");
    const char *channel = sa14_s(json_obj_get(args, "channel_id"), "");
    const char *user = sa14_s(json_obj_get(args, "user_id"), "");
    const char *ctype = sa14_s(json_obj_get(args, "chat_type"), "group");
    bool reply = sa14_gb(args, "is_thread_reply", false);
    json_t *ma = sa14_o(); sa14_ss(ma, "team_id", team); sa14_ss(ma, "ts", ets);
    json_t *mr = sa14_call(ctx, "workspace_message_marker", ma); json_free(ma);
    if (sa14_is_raise(mr)) return mr;
    const char *marker = sa14_s(mr, NULL);
    json_t *bot_ts = json_obj_get(ctx->state, "_bot_message_ts");
    json_t *ment = json_obj_get(ctx->state, "_mentioned_threads");
    if (reply && ((marker && sa14_arr_has_str(bot_ts, marker)) || sa14_arr_has_str(bot_ts, ets))) { json_free(mr); return json_bool(true); }
    if ((marker && sa14_arr_has_str(ment, marker)) || sa14_arr_has_str(ment, ets)) { json_free(mr); return json_bool(true); }
    json_free(mr);
    if (reply) {
        json_t *aa = sa14_o(); sa14_ss(aa, "channel_id", channel); sa14_ss(aa, "thread_ts", ets);
        sa14_ss(aa, "user_id", user); sa14_ss(aa, "team_id", team); sa14_ss(aa, "chat_type", ctype);
        json_t *ar = sa14_call(ctx, "has_active_session_for_thread", aa); json_free(aa);
        if (sa14_is_raise(ar)) return ar;
        if (sa14_truthy(ar)) { json_free(ar); return json_bool(true); }
        json_free(ar);
        json_t *br = sa14_call(ctx, "bot_authored_thread_root", sa14_copy_obj(args));
        if (sa14_is_raise(br)) return br;
        if (sa14_truthy(br)) { json_free(br); return json_bool(true); }
        json_free(br);
    }
    if (reply) {
        json_t *ids = json_obj_get(ctx->state, "_team_bot_user_ids");
        const char *uid = NULL;
        if (ids && ids->type == JSON_OBJECT) uid = sa14_s(json_obj_get(ids, team), NULL);
        if (!uid) uid = sa14_s(json_obj_get(ctx->state, "_bot_user_id"), NULL);
        if (uid && uid[0]) {
            json_t *pa = sa14_o(); sa14_ss(pa, "channel_id", channel); sa14_ss(pa, "thread_ts", ets);
            sa14_ss(pa, "team_id", team); sa14_sb(pa, "strip_bot_mention", false);
            json_t *pr = sa14_call(ctx, "fetch_thread_parent_text", pa); json_free(pa);
            if (sa14_is_raise(pr)) return pr;
            const char *pt = sa14_s(pr, NULL);
            bool has = pt && sa14_contains(pt, sa14_fmt_str("<@", "", "", uid, ">"));
            json_free(pr);
            if (has) {
                json_t *sr = sa14_call(ctx, "slack_strict_mention", sa14_o());
                if (sa14_is_raise(sr)) return sr;
                bool strict = sa14_truthy(sr); json_free(sr);
                if (!strict) { json_t *rr = sa14_call(ctx, "register_mentioned_thread", sa14_o()); if (sa14_is_raise(rr)) return rr; json_free(rr); }
                return json_bool(true);
            }
        }
    }
    return json_bool(false);
}

/* PoP: sa14_slack_api_human_users @ plugins/platforms/slack/adapter.py:_slack_api_human_users */
static json_t *sa14_slack_api_human_users(sa14_ctx_t *ctx, const json_t *args) {
    json_t *cached = json_obj_get(ctx->state, "_api_human_users_cache");
    if (cached) return json_copy(cached);
    json_t *ga = sa14_o(); sa14_ss(ga, "key", "api_human_users");
    json_t *raw = sa14_call(ctx, "config_extra_get", ga); json_free(ga);
    if (sa14_is_raise(raw)) return raw;
    if (!raw || raw->type == JSON_NULL) {
        json_free(raw);
        json_t *sa = sa14_o(); sa14_ss(sa, "key", "SLACK_API_HUMAN_USERS"); sa14_ss(sa, "default", "");
        raw = sa14_call(ctx, "get_scoped_secret", sa); json_free(sa);
        if (sa14_is_raise(raw)) return raw;
    }
    json_t *parts = NULL;
    if (raw && (raw->type == JSON_ARRAY || raw->type == JSON_OBJECT)) parts = json_copy(raw);
    else {
        char *sv = sa14_pystr(raw);
        parts = json_array();
        char *p = sv ? sv : (char *)""; char *save = NULL;
        for (char *tok = strtok_r(p, ",", &save); tok; tok = strtok_r(NULL, ",", &save))
            json_append(parts, json_string(tok));
        if (sv) free(sv);
    }
    json_free(raw);
    json_t *set = sa14_set_from(parts); json_free(parts);
    json_set(ctx->state, "_api_human_users_cache", json_copy(set));
    return set;
}

/* PoP: sa14_slack_json_post @ plugins/platforms/slack/adapter.py:_slack_json_post */
static json_t *sa14_slack_json_post(sa14_ctx_t *ctx, const json_t *args) {
    const char *token = sa14_s(json_obj_get(args, "token"), "");
    const char *method = sa14_s(json_obj_get(args, "method"), "");
    const json_t *payload = json_obj_get(args, "payload");
    const json_t *reqkw = json_obj_get(args, "req_kw");
    json_t *headers = sa14_o();
    sa14_ss(headers, "Authorization", sa14_fmt_str("Bearer ", token, "", "", ""));
    sa14_ss(headers, "Content-Type", "application/json");
    json_t *pa = sa14_o();
    sa14_ss(pa, "url", sa14_fmt_str("https://slack.com/api/", method, "", "", ""));
    sa14_sc(pa, "headers", headers); json_free(headers);
    sa14_sc(pa, "json", payload);
    if (reqkw && reqkw->type == JSON_OBJECT)
        for (size_t i = 0; i < reqkw->c.count; i++)
            if (reqkw->c.keys) sa14_sc(pa, reqkw->c.keys[i], json_obj_get(reqkw, reqkw->c.keys[i]));
    json_t *resp = sa14_call(ctx, "post", pa); json_free(pa);
    if (sa14_is_raise(resp)) return resp;
    json_t *ja = sa14_o(); sa14_sc(ja, "resp", resp); json_free(resp);
    json_t *data = sa14_call(ctx, "resp_json", ja); json_free(ja);
    return data;
}



/* ---- wave-14 type-name + small regex helpers ---- */
static const char *sa14_pytype(const json_t *v) {
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
/* First <@UID[...]> mention; returns malloc'd uid or NULL. Implements
 * re.match(r"\s*<@([^>|\s]+)(?:\|[^>]*)?>", text). */
static char *sa14_first_user_mention(const char *text) {
    if (!text) return NULL;
    const char *p = text;
    while (*p && sa14_is_ws((unsigned char)*p)) p++;
    if (p[0] != '<' || p[1] != '@') return NULL;
    p += 2;
    const char *start = p;
    while (*p && *p != '>' && *p != '|' && !sa14_is_ws((unsigned char)*p)) p++;
    if (p == start) return NULL;
    size_t n = (size_t)(p - start);
    char *uid = (char *)malloc(n + 1);
    if (!uid) return NULL;
    memcpy(uid, start, n); uid[n] = '\0';
    if (*p == '|') { while (*p && *p != '>') p++; }
    if (*p != '>') { free(uid); return NULL; }
    return uid;
}
/* Does text contain <@uid> or <@uid|...> anywhere? */
static bool sa14_has_user_mention(const char *text, const char *uid) {
    if (!text || !uid || !uid[0]) return false;
    size_t ul = strlen(uid);
    char *needle = sa14_fmt_str("<@", uid, "", "", "");
    const char *p = text;
    while ((p = strstr(p, needle)) != NULL) {
        const char *q = p + strlen(needle);
        if (*q == '>') { free(needle); return true; }
        if (*q == '|') {
            const char *r = q + 1;
            while (*r && *r != '>') r++;
            if (*r == '>') { free(needle); return true; }
        }
        p++;
    }
    free(needle); (void)ul;
    return false;
}

/* PoP: sa14_slack_mention_patterns @ plugins/platforms/slack/adapter.py:_slack_mention_patterns */
static json_t *sa14_slack_mention_patterns(sa14_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cached = json_obj_get(ctx->state, "_compiled_mention_patterns");
    if (cached) return json_copy(cached);
    json_t *ga = sa14_o(); sa14_ss(ga, "key", "mention_patterns");
    json_t *patterns = sa14_call(ctx, "config_extra_get", ga); json_free(ga);
    if (sa14_is_raise(patterns)) return patterns;
    if (!patterns || patterns->type == JSON_NULL) {
        json_free(patterns);
        json_t *sa = sa14_o(); sa14_ss(sa, "key", "SLACK_MENTION_PATTERNS"); sa14_ss(sa, "default", "");
        json_t *raw = sa14_call(ctx, "get_scoped_secret", sa); json_free(sa);
        if (sa14_is_raise(raw)) return raw;
        char *sv = sa14_strip(sa14_strval(raw)); json_free(raw);
        if (sv && sv[0]) {
            json_t *ja = sa14_o(); sa14_ss(ja, "raw", sv);
            json_t *jr = sa14_call(ctx, "json_loads", ja); json_free(ja);
            if (sa14_is_raise(jr)) {
                if (sa14_raise_passes(jr)) { free(sv); return jr; }
                json_free(jr);
                /* raw.replace("\n", ",").split(",") with stripped non-empty parts */
                patterns = json_array();
                char *work = sa14_strdup(sv);
                for (char *q = work; *q; q++) if (*q == '\n') *q = ',';
                char *save = NULL;
                for (char *tok = strtok_r(work, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
                    char *t = sa14_strip(tok);
                    if (t && t[0]) json_append(patterns, json_string(t));
                    free(t);
                }
                free(work); free(sv);
            } else { patterns = jr; free(sv); }
        } else { free(sv); patterns = json_null(); }
    }
    if (patterns && patterns->type == JSON_STRING) {
        json_t *arr = json_array(); json_append(arr, json_copy(patterns)); json_free(patterns); patterns = arr;
    }
    json_t *compiled = json_array();
    if (patterns && patterns->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(patterns); i++) {
            const char *pat = sa14_s(json_get(patterns, i), NULL);
            char *pt = pat ? sa14_strip(pat) : NULL;
            if (!pt || !pt[0]) { free(pt); continue; }
            free(pt);
            json_t *pa = sa14_o(); sa14_ss(pa, "pattern", pat); sa14_ss(pa, "flags", "IGNORECASE");
            json_t *cr = sa14_call(ctx, "re_compile", pa); json_free(pa);
            if (sa14_is_raise(cr)) {
                if (sa14_raise_passes(cr)) { json_free(patterns); return cr; }
                json_t *wa = sa14_o(); sa14_ss(wa, "fmt", "[Slack] Invalid mention pattern %r: %s");
                sa14_ss(wa, "pat", pat); sa14_sc(wa, "error", json_obj_get(cr, "message"));
                json_t *wr = sa14_call(ctx, "logger.warning", wa); json_free(wa);
                if (sa14_is_raise(wr)) { json_free(cr); json_free(patterns); return wr; }
                json_free(wr); json_free(cr); continue;
            }
            json_append(compiled, cr);
        }
    } else if (patterns && patterns->type != JSON_NULL) {
        json_t *wa = sa14_o(); sa14_ss(wa, "fmt", "[Slack] mention_patterns must be a list or string; got %s");
        sa14_ss(wa, "type", sa14_pytype(patterns));
        json_t *wr = sa14_call(ctx, "logger.warning", wa); json_free(wa);
        if (sa14_is_raise(wr)) { json_free(patterns); return wr; }
        json_free(wr);
    }
    json_free(patterns);
    if (json_len(compiled) > 0) {
        json_t *ia = sa14_o(); sa14_ss(ia, "fmt", "[Slack] Loaded %d mention pattern(s)"); sa14_sn(ia, "n", (double)json_len(compiled));
        json_t *ir = sa14_call(ctx, "logger.info", ia); json_free(ia);
        if (sa14_is_raise(ir)) { json_free(compiled); return ir; }
        json_free(ir);
    }
    json_set(ctx->state, "_compiled_mention_patterns", json_copy(compiled));
    return compiled;
}

/* PoP: sa14_slack_message_addressed_to_other_user @ plugins/platforms/slack/adapter.py:_slack_message_addressed_to_other_user */
static json_t *sa14_slack_message_addressed_to_other_user(sa14_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = sa14_s(json_obj_get(args, "text"), "");
    const json_t *uids = json_obj_get(args, "self_uids");
    char *uid = sa14_first_user_mention(text);
    if (!uid) return json_bool(false);
    bool other = !sa14_arr_has_str(uids, uid);
    free(uid);
    return json_bool(other);
}

/* PoP: sa14_slack_message_mentions_self @ plugins/platforms/slack/adapter.py:_slack_message_mentions_self */
static json_t *sa14_slack_message_mentions_self(sa14_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = sa14_s(json_obj_get(args, "text"), "");
    const json_t *uids = json_obj_get(args, "self_uids");
    if (!text[0]) return json_bool(false);
    if (uids && uids->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(uids); i++) {
            const char *u = sa14_s(json_get(uids, i), NULL);
            if (u && sa14_has_user_mention(text, u)) return json_bool(true);
        }
    } else if (uids && uids->type == JSON_OBJECT) {
        for (size_t i = 0; i < uids->c.count; i++) {
            const char *u = uids->c.keys ? uids->c.keys[i] : NULL;
            if (u && sa14_has_user_mention(text, u)) return json_bool(true);
        }
    }
    return json_bool(false);
}

/* PoP: sa14_slack_per_request_proxy_middleware @ plugins/platforms/slack/adapter.py:_slack_per_request_proxy_middleware */
static json_t *sa14_slack_per_request_proxy_middleware(sa14_ctx_t *ctx, const json_t *args) {
    const char *proxy = sa14_s(json_obj_get(args, "proxy_url"), "");
    json_t *pa = sa14_o(); sa14_sc(pa, "client", json_obj_get(args, "client")); sa14_ss(pa, "proxy_url", proxy);
    json_t *pr = sa14_call(ctx, "apply_slack_proxy", pa); json_free(pa);
    if (sa14_is_raise(pr)) return pr;
    json_free(pr);
    json_t *nr = sa14_call(ctx, "next_", sa14_o());
    return nr;
}

/* PoP: sa14_socket_transport_connected @ plugins/platforms/slack/adapter.py:_socket_transport_connected */
static json_t *sa14_socket_transport_connected(sa14_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *handler = json_obj_get(ctx->state, "_handler");
    json_t *client = (handler && handler->type == JSON_OBJECT) ? json_obj_get(handler, "client") : NULL;
    json_t *state = (client && client->type == JSON_OBJECT) ? json_obj_get(client, "is_connected") : NULL;
    if (!state || state->type == JSON_NULL) return json_null();
    json_t *value = json_copy(state);
    if (value && value->type == JSON_OBJECT && json_obj_get(value, "__callable__")) {
        json_t *ca = sa14_o();
        json_t *cr = sa14_call(ctx, "is_connected", ca); json_free(ca);
        json_free(value); value = cr;
        if (sa14_is_raise(value)) {
            if (sa14_raise_passes(value)) return value;
            json_t *la = sa14_o(); sa14_ss(la, "fmt", "[Slack] Could not inspect Socket Mode transport state"); sa14_sb(la, "exc_info", true);
            json_t *lr = sa14_call(ctx, "logger.debug", la); json_free(la);
            if (sa14_is_raise(lr)) return lr;
            json_free(lr); json_free(value); return json_null();
        }
    }
    if (value && value->type == JSON_OBJECT && json_obj_get(value, "__coroutine__")) {
        json_t *ar = sa14_call(ctx, "await", sa14_o());
        json_free(value); value = ar;
        if (sa14_is_raise(value)) {
            if (sa14_raise_passes(value)) return value;
            json_t *la = sa14_o(); sa14_ss(la, "fmt", "[Slack] Could not inspect Socket Mode transport state"); sa14_sb(la, "exc_info", true);
            json_t *lr = sa14_call(ctx, "logger.debug", la); json_free(la);
            if (sa14_is_raise(lr)) return lr;
            json_free(lr); json_free(value); return json_null();
        }
    }
    if (sa14_is_raise(value)) return value;
    json_t *out = json_bool(sa14_truthy(value));
    json_free(value);
    return out;
}



/* ---- wave-14 shared helpers for standalone sends / watchdog ---- */
/* forward decl */
static json_t *sa14_send_error(sa14_ctx_t *ctx, const char *err, bool nothing);

/* full outer-except for _standalone_send (proxy URL re-resolved, nothing_sent) */
static json_t *sa14_se_full(sa14_ctx_t *ctx, json_t *r) {
    if (sa14_raise_passes(r)) return r;
    const char *msg = sa14_emsg(r);
    json_t *pr = sa14_call(ctx, "resolve_proxy_url", sa14_o());
    if (sa14_is_raise(pr)) { json_free(r); return pr; }
    json_t *da = sa14_o(); sa14_ss(da, "error", msg); sa14_sc(da, "proxy", pr); json_free(pr);
    json_t *dr = sa14_call(ctx, "describe_proxy_error", da); json_free(da);
    if (sa14_is_raise(dr)) { json_free(r); return dr; }
    const char *desc = (dr && dr->type == JSON_STRING) ? sa14_s(dr, "") : NULL;
    const char *shown = (desc && desc[0]) ? desc : msg;
    json_t *na = sa14_o(); sa14_ss(na, "error", msg);
    json_t *nr = sa14_call(ctx, "request_never_left", na); json_free(na);
    if (sa14_is_raise(nr)) { json_free(dr); json_free(r); return nr; }
    bool nothing = sa14_truthy(nr); json_free(nr); json_free(dr); json_free(r);
    char *err = sa14_fmt_str("Slack send failed: ", shown, "", "", "");
    json_t *out = sa14_send_error(ctx, err ? err : "", nothing); free(err);
    return out;
}
/* simple except for media post_text (proxy already resolved, no nothing_sent) */
static json_t *sa14_se_simple(sa14_ctx_t *ctx, json_t *r, const json_t *proxy) {
    if (sa14_raise_passes(r)) return r;
    const char *msg = sa14_emsg(r);
    json_t *da = sa14_o(); sa14_ss(da, "error", msg); sa14_sc(da, "proxy", proxy);
    json_t *dr = sa14_call(ctx, "describe_proxy_error", da); json_free(da);
    if (sa14_is_raise(dr)) { json_free(r); return dr; }
    const char *desc = (dr && dr->type == JSON_STRING) ? sa14_s(dr, "") : NULL;
    const char *shown = (desc && desc[0]) ? desc : msg;
    json_free(dr); json_free(r);
    char *err = sa14_fmt_str("Slack send failed: ", shown, "", "", "");
    json_t *out = sa14_send_error(ctx, err ? err : "", false); free(err);
    return out;
}

static json_t *sa14_send_error(sa14_ctx_t *ctx, const char *err, bool nothing) {
    json_t *a = sa14_o(); sa14_ss(a, "error", err);
    if (nothing) sa14_sb(a, "nothing_sent", true);
    return sa14_call(ctx, "send_error", a);
}
static json_t *sa14_describe_proxy(sa14_ctx_t *ctx, const char *msg) {
    json_t *pr = sa14_call(ctx, "resolve_proxy_url", sa14_o());
    if (sa14_is_raise(pr)) return pr;
    json_t *pa = sa14_o(); sa14_ss(pa, "error", msg); sa14_sc(pa, "proxy", pr); json_free(pr);
    json_t *dr = sa14_call(ctx, "describe_proxy_error", pa); json_free(pa);
    return dr;
}
static const char *sa14_dget_str(const json_t *o, const char *k, const char *def) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    return sa14_s(v, def);
}

/* PoP: sa14_socket_watchdog_loop @ plugins/platforms/slack/adapter.py:_socket_watchdog_loop */
static json_t *sa14_wd_exc(sa14_ctx_t *ctx, json_t *r) {
    if (sa14_raise_passes(r)) return r;
    json_t *wa = sa14_o(); sa14_ss(wa, "fmt", "[Slack] Socket Mode watchdog iteration failed; continuing"); sa14_sb(wa, "exc_info", true);
    json_t *wr = sa14_call(ctx, "logger.warning", wa); json_free(wa); json_free(r);
    if (sa14_is_raise(wr)) return wr;
    json_free(wr); return NULL;
}
static json_t *sa14_socket_watchdog_loop(sa14_ctx_t *ctx, const json_t *args) {
    int guard = 0;
    while (sa14_truthy(json_obj_get(ctx->state, "_running"))) {
        if (++guard > 24) break;
        json_t *sa = sa14_o(); sa14_sn(sa, "seconds", json_get_num(args, "interval_s", 30));
        json_t *sr = sa14_call(ctx, "sleep", sa);
        if (sa14_is_raise(sr)) { json_t *h = sa14_wd_exc(ctx, sr); if (h) return h; continue; }
        if (sr && sr->type == JSON_OBJECT && json_obj_get(sr, "stop")) json_set(ctx->state, "_running", json_bool(false));
        json_free(sr);
        if (!sa14_truthy(json_obj_get(ctx->state, "_running"))) break;
        json_t *task = json_obj_get(ctx->state, "_socket_mode_task");
        bool tnull = !task || task->type == JSON_NULL;
        bool done = !tnull && task->type == JSON_OBJECT && json_get_bool(task, "done", false);
        const char *reason = NULL;
        if (tnull) reason = "socket task missing";
        else if (done) reason = "socket task stopped";
        if (reason) {
            json_t *ra = sa14_o(); sa14_ss(ra, "reason", reason);
            json_t *rr = sa14_call(ctx, "restart_socket_mode", ra);
            if (sa14_is_raise(rr)) { json_t *h = sa14_wd_exc(ctx, rr); if (h) return h; continue; }
            json_free(rr); continue;
        }
        json_t *cr = sa14_call(ctx, "socket_transport_connected", sa14_o());
        if (sa14_is_raise(cr)) { json_t *h = sa14_wd_exc(ctx, cr); if (h) return h; continue; }
        bool disc = (cr && cr->type == JSON_BOOL && !cr->bool_val);
        json_free(cr);
        bool stale = false;
        if (!disc) {
            json_t *pr = sa14_call(ctx, "socket_ping_pong_stale", sa14_o());
            if (sa14_is_raise(pr)) { json_t *h = sa14_wd_exc(ctx, pr); if (h) return h; continue; }
            stale = sa14_truthy(pr); json_free(pr);
        }
        if (disc || stale) {
            json_t *ra = sa14_o(); sa14_ss(ra, "reason", disc ? "transport disconnected" : "ping/pong stale");
            json_t *rr = sa14_call(ctx, "restart_socket_mode", ra);
            if (sa14_is_raise(rr)) { json_t *h = sa14_wd_exc(ctx, rr); if (h) return h; continue; }
            json_free(rr);
        }
    }
    return NULL;
}

/* PoP: sa14_standalone_send @ plugins/platforms/slack/adapter.py:_standalone_send */
static json_t *sa14_standalone_send(sa14_ctx_t *ctx, const json_t *args) {
    const json_t *media = json_obj_get(args, "media_files");
    bool has_media = media && media->type == JSON_ARRAY && json_len(media) > 0;
    const char *chat = sa14_s(json_obj_get(args, "chat_id"), "");
    const json_t *message = json_obj_get(args, "message");
    const json_t *caption = json_obj_get(args, "caption");
    const json_t *thread = json_obj_get(args, "thread_id");
    const json_t *wrong = json_obj_get(args, "wrong_workspace_errors");
    json_t *raw_token = sa14_call(ctx, "pconfig_token", sa14_o());
    if (sa14_is_raise(raw_token)) return raw_token;
    if (!sa14_truthy(raw_token)) {
        json_free(raw_token);
        json_t *sa = sa14_o(); sa14_ss(sa, "key", "SLACK_BOT_TOKEN"); sa14_ss(sa, "default", "");
        raw_token = sa14_call(ctx, "get_secret", sa);
        if (sa14_is_raise(raw_token)) return raw_token;
    }
    const char *rts = sa14_truthy(raw_token) ? sa14_strval(raw_token) : "";
    json_t *la = sa14_o(); sa14_ss(la, "raw", rts); sa14_sb(la, "quiet", true);
    json_t *tokens = sa14_call(ctx, "load_slack_bot_tokens", la);
    if (sa14_is_raise(tokens)) return tokens;
    if (!sa14_truthy(tokens)) return sa14_send_error(ctx, "Slack send failed: SLACK_BOT_TOKEN not configured", true);
    const char *token = sa14_s(json_get(tokens, 0), "");
    char *chatbuf = sa14_strdup(chat);
    if (chatbuf[0] == 'U' || chatbuf[0] == 'W') {
        json_t *resolved = NULL; bool found = false;
        for (size_t i = 0; i < json_len(tokens); i++) {
            const char *tok = sa14_s(json_get(tokens, i), "");
            json_t *ra = sa14_o(); sa14_ss(ra, "token", tok); sa14_ss(ra, "chat_id", chatbuf);
            json_t *rr = sa14_call(ctx, "resolve_slack_user_dm", ra);
            if (sa14_is_raise(rr)) { free(chatbuf); return rr; }
            if (rr && rr->type != JSON_NULL) { resolved = rr; token = tok; found = true; break; }
            json_free(rr);
        }
        if (!found) {
            char *err = sa14_fmt_str("Slack user ID resolution failed for ", chatbuf,
                " (conversations.open — check the bot's im:write scope)", "", "");
            json_t *o = sa14_o(); sa14_ss(o, "error", err ? err : ""); free(err); free(chatbuf);
            return o;
        }
        free(chatbuf); chatbuf = sa14_strdup(sa14_s(resolved, "")); json_free(resolved);
    }
    json_t *formatted;
    if (sa14_truthy(message)) {
        json_t *fa = sa14_o(); sa14_sc(fa, "text", message);
        formatted = sa14_call(ctx, "format_mrkdwn", fa);
        if (sa14_is_raise(formatted)) { free(chatbuf); return formatted; }
    } else formatted = json_copy(message);
    json_t *formatted_caption;
    if (sa14_truthy(caption)) {
        json_t *fa = sa14_o(); sa14_sc(fa, "text", caption);
        formatted_caption = sa14_call(ctx, "format_mrkdwn", fa);
        if (sa14_is_raise(formatted_caption)) { free(chatbuf); json_free(formatted); return formatted_caption; }
    } else formatted_caption = json_copy(caption);
    json_t *extra = sa14_call(ctx, "pconfig_extra", sa14_o());
    if (sa14_is_raise(extra)) { free(chatbuf); json_free(formatted); json_free(formatted_caption); return extra; }
    json_t *ua = sa14_o(); sa14_sc(ua, "extra", extra); json_free(extra);
    json_t *unfurl = sa14_call(ctx, "slack_unfurl_kwargs", ua); json_free(ua);
    if (sa14_is_raise(unfurl)) { free(chatbuf); json_free(formatted); json_free(formatted_caption); return unfurl; }
    if (has_media) {
        json_t *ma = sa14_o(); sa14_ss(ma, "token", token); sa14_ss(ma, "chat_id", chatbuf);
        sa14_sc(ma, "media_files", media); sa14_sc(ma, "thread_id", thread);
        sa14_sc(ma, "formatted", formatted); sa14_sc(ma, "formatted_caption", formatted_caption);
        sa14_sc(ma, "unfurl_kwargs", unfurl);
        json_t *mr = sa14_call(ctx, "standalone_send_media", ma); json_free(ma);
        free(chatbuf); json_free(formatted); json_free(formatted_caption); json_free(unfurl);
        return mr;
    }
    char *fstr = sa14_strip(sa14_strval(formatted));
    if (!sa14_truthy(formatted) || !fstr || !fstr[0]) {
        free(fstr); free(chatbuf); json_free(formatted); json_free(formatted_caption); json_free(unfurl);
        json_t *ia = sa14_o(); sa14_ss(ia, "fmt", "[Slack] _standalone_send: skipping empty/whitespace message");
        json_t *ir = sa14_call(ctx, "logger.debug", ia); json_free(ia);
        if (sa14_is_raise(ir)) return ir;
        json_free(ir);
        json_t *o = sa14_o(); sa14_sb(o, "success", true); sa14_ss(o, "platform", "slack"); sa14_ss(o, "skipped", "empty_text");
        return o;
    }
    free(fstr);
    json_t *im = sa14_call(ctx, "import_aiohttp", sa14_o());
    if (sa14_is_raise(im)) {
        if (sa14_exc_family(im, "ImportError")) { json_free(im); return sa14_send_error(ctx, "aiohttp not installed. Run: pip install aiohttp", false); }
        free(chatbuf); json_free(formatted); json_free(formatted_caption); json_free(unfurl); return im;
    }
    json_free(im);
    /* outer try starts here */
    json_t *pk = sa14_call(ctx, "standalone_proxy_kwargs", sa14_o());
    if (sa14_is_raise(pk)) { json_t *h = sa14_se_full(ctx, pk); free(chatbuf); json_free(formatted); json_free(formatted_caption); json_free(unfurl); return h; }
    const json_t *sesskw = sa14_arr(pk, 0); const json_t *reqkw = sa14_arr(pk, 1);
    json_t *ca = sa14_o(); json_t *to = sa14_o(); sa14_sn(to, "total", 30);
    sa14_sc(ca, "timeout", to); json_free(to);
    if (sesskw && sesskw->type == JSON_OBJECT)
        for (size_t i = 0; i < sesskw->c.count; i++) if (sesskw->c.keys) sa14_sc(ca, sesskw->c.keys[i], json_obj_get(sesskw, sesskw->c.keys[i]));
    json_t *sess = sa14_call(ctx, "client_session", ca); json_free(ca);
    if (sa14_is_raise(sess)) { json_t *h = sa14_se_full(ctx, sess); free(chatbuf); json_free(formatted); json_free(formatted_caption); json_free(unfurl); json_free(pk); return h; }
    json_t *pa = sa14_o(); sa14_ss(pa, "chat_id", chatbuf); sa14_sc(pa, "formatted", formatted);
    sa14_sc(pa, "unfurl_kwargs", unfurl); sa14_sc(pa, "thread_id", thread);
    json_t *payload = sa14_call(ctx, "standalone_post_kwargs", pa); json_free(pa);
    if (sa14_is_raise(payload)) { json_t *h = sa14_se_full(ctx, payload); free(chatbuf); json_free(formatted); json_free(formatted_caption); json_free(unfurl); json_free(pk); json_free(sess); return h; }
    const char *last_error = "unknown";
    json_t *result = NULL;
    for (size_t i = 0; i < json_len(tokens); i++) {
        const char *tok = sa14_s(json_get(tokens, i), "");
        json_t *ja = sa14_o(); sa14_sc(ja, "session", sess); sa14_ss(ja, "token", tok);
        sa14_ss(ja, "method", "chat.postMessage"); sa14_sc(ja, "payload", payload); sa14_sc(ja, "req_kw", reqkw);
        json_t *data = sa14_call(ctx, "slack_json_post", ja); json_free(ja);
        if (sa14_is_raise(data)) { result = sa14_se_full(ctx, data); break; }
        if (sa14_truthy(json_obj_get(data, "ok"))) {
            json_t *o = sa14_o(); sa14_sb(o, "success", true); sa14_ss(o, "platform", "slack");
            sa14_ss(o, "chat_id", chatbuf);
            const json_t *ts = json_obj_get(data, "ts");
            sa14_sc(o, "message_id", ts ? json_copy(ts) : json_null());
            result = o; json_free(data); break;
        }
        last_error = sa14_dget_str(data, "error", "unknown");
        bool wrong_tok = sa14_collection_has_str(wrong, last_error);
        json_free(data);
        if (!wrong_tok) break;
    }
    if (!result) {
        char *err = sa14_fmt_str("Slack API error: ", last_error, "", "", "");
        result = sa14_send_error(ctx, err ? err : "", false); free(err);
    }
    free(chatbuf); json_free(formatted); json_free(formatted_caption); json_free(unfurl);
    json_free(payload); json_free(sess); json_free(pk);
    return result;
}

/* PoP: sa14_standalone_send_media @ plugins/platforms/slack/adapter.py:_standalone_send_media */
static json_t *sa14_standalone_send_media(sa14_ctx_t *ctx, const json_t *args) {
    const char *token = sa14_s(json_obj_get(args, "token"), "");
    const char *chat = sa14_s(json_obj_get(args, "chat_id"), "");
    const json_t *media = json_obj_get(args, "media_files");
    const json_t *thread = json_obj_get(args, "thread_id");
    const json_t *formatted = json_obj_get(args, "formatted");
    const json_t *caption = json_obj_get(args, "formatted_caption");
    const json_t *unfurl = json_obj_get(args, "unfurl_kwargs");
    json_t *im = sa14_call(ctx, "import_slack_sdk", sa14_o());
    if (sa14_is_raise(im)) {
        if (sa14_exc_family(im, "ImportError")) { json_free(im); json_t *o = sa14_o(); sa14_ss(o, "error", "slack_sdk not installed. Run: pip install 'slack-sdk' (required for Slack MEDIA delivery via send_message)"); return o; }
        return im;
    }
    json_free(im);
    json_t *ca = sa14_o(); sa14_ss(ca, "token", token);
    json_t *client = sa14_call(ctx, "async_web_client", ca); json_free(ca);
    if (sa14_is_raise(client)) return client;
    json_t *proxy = sa14_call(ctx, "resolve_proxy_url", sa14_o());
    if (sa14_is_raise(proxy)) { json_free(client); return proxy; }
    json_t *pa = sa14_o(); sa14_sc(pa, "client", client); sa14_sc(pa, "proxy_url", proxy);
    json_t *apr = sa14_call(ctx, "apply_slack_proxy", pa); json_free(pa);
    if (sa14_is_raise(apr)) { json_free(client); json_free(proxy); return apr; }
    json_free(apr);
    bool cap_as_comment = sa14_truthy(caption) && !sa14_truthy(unfurl);
    json_t *text_to_send;
    if (cap_as_comment) text_to_send = json_string("");
    else if (sa14_truthy(caption)) text_to_send = json_copy(caption);
    else if (sa14_truthy(formatted)) text_to_send = json_copy(formatted);
    else text_to_send = json_string("");
    char *tstr = sa14_strip(sa14_strval(text_to_send));
    json_t *last_mid = NULL; bool uploaded_any = false; json_t *result = NULL;
    json_t *warnings = json_array();
    if (tstr && tstr[0]) {
        json_t *ta = sa14_o(); sa14_sc(ta, "client", client); sa14_ss(ta, "chat_id", chat);
        sa14_sc(ta, "text", text_to_send); sa14_sc(ta, "unfurl_kwargs", unfurl); sa14_sc(ta, "thread_id", thread);
        json_t *pp = sa14_call(ctx, "standalone_post_text", ta); json_free(ta);
        if (sa14_is_raise(pp)) { result = sa14_se_simple(ctx, pp, proxy); }
        else {
            const json_t *ok = json_obj_get(pp, "ok");
            if (ok && !sa14_truthy(ok)) {
                const char *e = sa14_dget_str(pp, "error", "unknown");
                char *err = sa14_fmt_str("Slack API error: ", e, "", "", "");
                result = sa14_send_error(ctx, err ? err : "", false); free(err);
            } else {
                const json_t *ts = json_obj_get(pp, "ts");
                if (ts) { json_free(last_mid); last_mid = json_copy(ts); }
            }
            json_free(pp);
        }
    }
    free(tstr);
    bool caption_pending = cap_as_comment;
    if (result == NULL && media && media->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(media); i++) {
            const json_t *row = json_get(media, i);
            const char *path = sa14_s(sa14_arr(row, 0), "");
            json_t *ea = sa14_o(); sa14_ss(ea, "path", path);
            json_t *ex = sa14_call(ctx, "exists", ea); json_free(ea);
            if (sa14_is_raise(ex)) { result = ex; break; }
            bool exists = sa14_truthy(ex); json_free(ex);
            if (!exists) {
                char *w = sa14_fmt_str("Media file not found, skipping: ", path, "", "", "");
                json_t *wa = sa14_o(); sa14_ss(wa, "fmt", "[Slack] %s"); sa14_ss(wa, "msg", w ? w : "");
                json_t *wr = sa14_call(ctx, "logger.warning", wa); json_free(wa);
                if (sa14_is_raise(wr)) { free(w); result = wr; break; }
                json_free(wr); json_append(warnings, json_string(w ? w : "")); free(w);
                if (caption_pending) {
                    json_t *ta = sa14_o(); sa14_sc(ta, "client", client); sa14_ss(ta, "chat_id", chat);
                    sa14_sc(ta, "text", caption); sa14_sc(ta, "unfurl_kwargs", unfurl); sa14_sc(ta, "thread_id", thread);
                    json_t *fb = sa14_call(ctx, "standalone_post_text", ta); json_free(ta);
                    if (sa14_is_raise(fb)) {
                        json_t *pr = sa14_call(ctx, "resolve_proxy_url", sa14_o());
                        bool shown = !(sa14_truthy(pr)); json_free(pr);
                        json_t *la = sa14_o(); sa14_ss(la, "fmt", "[Slack] Caption-fallback send failed for missing media"); sa14_sb(la, "exc_info", shown);
                        json_t *lr = sa14_call(ctx, "logger.warning", la); json_free(la);
                        if (sa14_is_raise(lr)) { result = lr; break; }
                        json_free(lr);
                    } else {
                        const json_t *ok = json_obj_get(fb, "ok");
                        if (!ok || sa14_truthy(ok)) {
                            const json_t *ts = json_obj_get(fb, "ts");
                            if (ts && sa14_truthy(ts)) { json_free(last_mid); last_mid = json_copy(ts); caption_pending = false; }
                        }
                        json_free(fb);
                    }
                }
                continue;
            }
            json_t *ua = sa14_o(); sa14_sc(ua, "client", client); sa14_ss(ua, "chat_id", chat);
            sa14_ss(ua, "path", path);
            sa14_sc(ua, "initial_comment", caption_pending ? (json_copy(caption)) : json_string(""));
            sa14_sc(ua, "thread_id", thread);
            json_t *up = sa14_call(ctx, "standalone_upload_file", ua); json_free(ua);
            if (sa14_is_raise(up)) {
                json_t *pr = sa14_call(ctx, "resolve_proxy_url", sa14_o()); if (sa14_is_raise(pr)) { result = pr; break; }
                json_t *da = sa14_o(); sa14_sc(da, "error", json_obj_get(up, "message")); sa14_sc(da, "proxy", pr);
                json_t *dr = sa14_call(ctx, "describe_proxy_error", da); json_free(da); json_free(pr);
                const char *shown = (dr && dr->type == JSON_STRING) ? sa14_s(dr, "") : NULL;
                char *w = sa14_fmt_str("Failed to send media ", path, ": ", shown ? shown : sa14_emsg(up), "");
                json_t *la = sa14_o(); sa14_ss(la, "fmt", "[Slack] %s"); sa14_ss(la, "msg", w ? w : "");
                sa14_sb(la, "exc_info", shown == NULL);
                json_t *lr = sa14_call(ctx, "logger.error", la); json_free(la);
                if (sa14_is_raise(lr)) { free(w); json_free(dr); result = lr; break; }
                json_free(lr); json_append(warnings, json_string(w ? w : "")); free(w);
                json_free(dr); json_free(up); continue;
            }
            const json_t *errv = json_obj_get(up, "error");
            if (sa14_truthy(errv)) {
                char *w = sa14_fmt_str("Failed to send media ", path, ": ", sa14_s(errv, ""), "");
                json_append(warnings, json_string(w ? w : "")); free(w); json_free(up); continue;
            }
            uploaded_any = true; caption_pending = false;
            const json_t *mid = json_obj_get(up, "message_id");
            if (mid && sa14_truthy(mid)) { json_free(last_mid); last_mid = json_copy(mid); }
            json_free(up);
        }
    }
    if (result == NULL) {
        char *ts2 = sa14_strip(sa14_strval(text_to_send));
        bool notext = !ts2 || !ts2[0]; free(ts2);
        if (last_mid == NULL && !uploaded_any && notext) {
            result = sa14_o(); sa14_ss(result, "error", "No deliverable text or media remained after processing");
        } else {
            result = sa14_o(); sa14_sb(result, "success", true); sa14_ss(result, "platform", "slack");
            sa14_ss(result, "chat_id", chat);
            sa14_sc(result, "message_id", last_mid ? json_copy(last_mid) : json_null());
        }
        if (json_len(warnings) > 0) sa14_sc(result, "warnings", warnings);
    }
    json_free(warnings); json_free(last_mid); json_free(text_to_send);
    json_free(client); json_free(proxy);
    return result;
}

/* PoP: sa14_synthetic_reaction_event @ plugins/platforms/slack/adapter.py:_synthetic_reaction_event */
static json_t *sa14_synthetic_reaction_event(sa14_ctx_t *ctx, const json_t *args) {
    const json_t *event = json_obj_get(args, "event");
    const char *action = sa14_s(json_obj_get(args, "action"), "");
    const char *thread = sa14_s(json_obj_get(args, "thread_ts"), "");
    const char *team = sa14_s(json_obj_get(args, "team_id"), "");
    const json_t *emap = json_obj_get(args, "emoji_map");
    const json_t *item = json_obj_get(event, "item");
    const char *channel = sa14_s(json_obj_get(item, "channel"), "");
    const char *msgts = sa14_s(json_obj_get(item, "ts"), "");
    const char *rname = sa14_s(json_obj_get(event, "reaction"), "");
    const json_t *uid = json_obj_get(event, "user");
    const json_t *mapped = emap ? json_obj_get(emap, rname) : NULL;
    const char *emoji = mapped ? sa14_s(mapped, rname) : rname;
    json_t *syn = sa14_o();
    sa14_ss(syn, "type", "message");
    sa14_sc(syn, "user", uid ? json_copy(uid) : json_null());
    char *txt = sa14_fmt_str("reaction:", action, ":", emoji, "");
    sa14_ss(syn, "text", txt ? txt : ""); free(txt);
    sa14_ss(syn, "channel", channel);
    const json_t *ets = json_obj_get(event, "event_ts");
    if (sa14_truthy(ets)) sa14_sc(syn, "ts", ets);
    else {
        char *fb = sa14_fmt_str("reaction-", msgts, "-", rname, "");
        char *fb2 = sa14_fmt_str(fb ? fb : "", "-", sa14_s(uid, ""), "", "");
        sa14_ss(syn, "ts", fb2 ? fb2 : ""); free(fb); free(fb2);
    }
    sa14_ss(syn, "thread_ts", thread);
    sa14_sb(syn, "_hermes_force_process", true);
    json_t *hr = sa14_o(); sa14_ss(hr, "name", rname); sa14_ss(hr, "action", action);
    sa14_sc(hr, "reacted_to_ts", json_string(msgts));
    sa14_sc(hr, "event_ts", ets ? json_copy(ets) : json_null());
    sa14_sc(syn, "_hermes_reaction", hr); json_free(hr);
    if (team[0]) sa14_ss(syn, "team", team);
    json_t *tr = sa14_call(ctx, "reaction_trigger_target", sa14_o());
    if (sa14_is_raise(tr)) { json_free(syn); return tr; }
    const char *tchan = sa14_s(sa14_arr(tr, 0), NULL);
    const char *tthr = sa14_s(sa14_arr(tr, 1), NULL);
    if (tchan && tchan[0]) {
        sa14_ss(syn, "channel", tchan);
        sa14_ss(syn, "channel_type", (tchan[0] == 'D') ? "im" : "channel");
        sa14_sc(syn, "_hermes_reaction_source_channel", json_string(channel));
        if (tthr && tthr[0]) sa14_ss(syn, "thread_ts", tthr);
        else { json_obj_del(syn, "thread_ts"); sa14_sb(syn, "_hermes_no_thread_response", true); }
    }
    json_free(tr);
    return syn;
}

/* PoP: sa14_thread_watermark_io @ plugins/platforms/slack/adapter.py:_thread_watermark_io */
static json_t *sa14_thread_watermark_io(sa14_ctx_t *ctx, const json_t *args) {
    const char *method = sa14_s(json_obj_get(args, "method"), "");
    const char *channel = sa14_s(json_obj_get(args, "channel_id"), "");
    const char *thread = sa14_s(json_obj_get(args, "thread_ts"), "");
    const char *user = sa14_s(json_obj_get(args, "user_id"), "");
    const char *team = sa14_s(json_obj_get(args, "team_id"), "");
    const json_t *extra = json_obj_get(args, "args");
    json_t *sr = sa14_call(ctx, "get_session_store", sa14_o());
    if (sa14_is_raise(sr)) return sr;
    bool has_method = sa14_truthy(sr) && json_obj_get(sr, method) != NULL;
    if (!has_method) { json_free(sr); return json_null(); }
    json_t *ka = sa14_o(); sa14_ss(ka, "channel_id", channel); sa14_ss(ka, "thread_ts", thread);
    sa14_ss(ka, "user_id", user); sa14_ss(ka, "team_id", team);
    json_t *kr = sa14_call(ctx, "build_thread_session_key", ka); json_free(ka); json_free(sr);
    if (sa14_is_raise(kr)) return kr;
    const char *skey = sa14_s(kr, NULL);
    if (!skey || !skey[0]) { json_free(kr); return json_null(); }
    char *mkey = sa14_fmt_str("slack_thread_watermark:", channel, ":", thread, "");
    json_t *ma = sa14_o(); sa14_ss(ma, "method", method); sa14_ss(ma, "session_key", skey);
    sa14_ss(ma, "meta_key", mkey ? mkey : ""); sa14_sc(ma, "args", extra);
    json_t *mr = sa14_call(ctx, "session_store_method", ma); json_free(ma); json_free(kr); free(mkey);
    return mr;
}

