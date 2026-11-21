/*
 * slack_adapter_flow13.c -- FLOW cores, wave 13, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's still-unclaimed remainder after flow1-12. Every ported name was
 * UNCLAIMED by a strict PoP grep of the whole main-only repo at port time
 * (each carries its PoP annotation at its definition below). No flow1-12
 * function is re-ported.
 *
 * Ported (16 functions):
 *   _resolve_file_stub, _resolve_slack_user_dm, _resolve_user_is_bot,
 *   _resolve_user_name, _restart_socket_mode, _seal_stale_streams,
 *   _seal_stream, _seed_dm_session, _send_exec_approval_prompt,
 *   _send_failure_notice, _send_interactive_prompt, _send_local_file,
 *   _send_slash_ephemeral, _send_slash_reply, _set_thread_status,
 *   _set_thread_watermark.
 *
 * Decision sequences only: every injected/unported callable enters through ONE
 * universal seam sa13_call(ctx, name, args). The harness wires it to a recording
 * stub fed in.io; the oracle installs recorders emitting the same
 * {"name","args","ret"} event shape from the SAME scripts. Ordered trace,
 * BaseException passthrough, fail-closed. Self-contained: libjson only.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow13.py):
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
typedef struct sa13_ctx {
    json_t *state;      /* owned; echoed as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa13_ctx_t;

/* -- basic helpers -- */
static bool sa13_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sa13_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}
static json_t *sa13_obj_attrs(const json_t *v) {
    return sa13_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}
static bool sa13_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}
static bool sa13_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}
static bool sa13_collection_has_str(const json_t *coll, const char *s) {
    if (!s || !coll) return false;
    if (sa13_is_set(coll)) coll = json_obj_get(coll, "__set__");
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
static bool sa13_plain_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa13_is_obj(v) &&
           !sa13_is_set(v) && !sa13_is_tuple_marker(v) && !sa13_is_raise(v);
}
static bool sa13_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa13_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}
static const char *sa13_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static char *sa13_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static json_t *sa13_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *sa13_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = sa13_err(type, msg);
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
static json_t *sa13_call(sa13_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa13_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa13_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa13_make_exc(raise);
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
typedef struct sa13_exc_parent { const char *name, *parent1, *parent2; } sa13_exc_parent_t;
static const sa13_exc_parent_t SA13_EXC_TABLE[] = {
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
#define SA13_EXC_TABLE_LEN (sizeof(SA13_EXC_TABLE) / sizeof(SA13_EXC_TABLE[0]))

static const sa13_exc_parent_t *sa13_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA13_EXC_TABLE_LEN; i++)
        if (strcmp(SA13_EXC_TABLE[i].name, name) == 0) return &SA13_EXC_TABLE[i];
    return NULL;
}
typedef struct sa13_chain { char names[64][96]; size_t n; } sa13_chain_t;
static bool sa13_chain_has(const sa13_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}
static void sa13_chain_add(sa13_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa13_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}
static void sa13_chain_walk(sa13_chain_t *c, const char *name) {
    if (!name || !name[0] || sa13_chain_has(c, name)) return;
    sa13_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const sa13_exc_parent_t *be = sa13_builtin_entry(name);
    const char *p1 = be ? be->parent1 : "Exception";
    const char *p2 = be ? be->parent2 : NULL;
    if (p1 && strcmp(p1, name) != 0) sa13_chain_walk(c, p1);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa13_chain_walk(c, p2);
}
static bool sa13_exc_family(const json_t *r, const char *family) {
    if (!sa13_is_raise(r)) return false;
    sa13_chain_t c; c.n = 0;
    sa13_chain_walk(&c, json_get_str(r, "__raise__", "Exception"));
    return sa13_chain_has(&c, family);
}
static bool sa13_raise_passes(const json_t *r) {
    return sa13_is_raise(r) && !sa13_exc_family(r, "Exception");
}

/* -- ASCII whitespace + string kernels -- */
static bool sa13_is_ws(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *sa13_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa13_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa13_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa13_lstrip_char(const char *s, char ch) {
    if (!s) return NULL;
    while (*s == ch) s++;
    return sa13_strdup(s);
}
static char *sa13_strip_chars(const char *s, const char *chars) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && strchr(chars, s[a]) != NULL) a++;
    while (b > a && strchr(chars, s[b - 1]) != NULL) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa13_lower(const char *s) {
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
static char *sa13_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa13_strdup("None");
    if (v->type == JSON_STRING) return sa13_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return sa13_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return sa13_strdup(buf);
    }
    return sa13_strdup("");
}
static char *sa13_replace(const char *s, const char *from, const char *to) {
    if (!s) return NULL;
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return sa13_strdup(s);
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
static bool sa13_startswith(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool sa13_is_num(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}
static double sa13_num(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* -- local helpers -- */
static json_t *sa13_o(void) { return json_object(); }
static void sa13_ss(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
static void sa13_sc(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, json_copy(v ? v : json_null()));
}
static void sa13_sb(json_t *o, const char *k, bool b) { json_set(o, k, json_bool(b)); }
static void sa13_sn(json_t *o, const char *k, double n) { json_set(o, k, json_number(n)); }
static const char *sa13_s(const json_t *v, const char *def) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static long long sa13_to_int(const json_t *v, long long def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
}
static long long sa13_gi(const json_t *o, const char *k, long long def) {
    return sa13_to_int(o ? json_obj_get(o, k) : NULL, def);
}
static bool sa13_gb(const json_t *o, const char *k, bool def) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    return v && v->type == JSON_BOOL ? v->bool_val : def;
}
static json_t *sa13_get(const json_t *obj, const char *key, const json_t *def) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return json_copy(v ? v : def);
}
static json_t *sa13_arr(const json_t *a, size_t i) {
    if (!a || a->type != JSON_ARRAY || i >= json_len(a)) return json_null();
    return json_copy(json_get(a, i));
}
static bool sa13_json_eq(const json_t *a, const char *s) {
    const char *v = sa13_str(a);
    return v && strcmp(v, s) == 0;
}
/* str(arg or "") via pystr. */
static char *sa13_strval(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa13_strdup("");
    return sa13_pystr(v);
}

/* -- mutable self-dict state helpers (echoed as out.state) -- */
static json_t *sa13_sget(sa13_ctx_t *ctx, const char *name, const char *key, const json_t *def) {
    json_t *d = json_obj_get(ctx->state, name);
    json_t *v = d && d->type == JSON_OBJECT ? json_obj_get(d, key) : NULL;
    return json_copy(v ? v : def);
}
static bool sa13_shas(sa13_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    return d && d->type == JSON_OBJECT && json_obj_get(d, key) != NULL;
}
static json_t *sa13_spop(sa13_ctx_t *ctx, const char *name, const char *key, const json_t *def) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_OBJECT) { d = json_object(); json_set(ctx->state, name, d); }
    json_t *v = json_obj_get(d, key);
    json_t *r = json_copy(v ? v : def);
    if (v) json_obj_del(d, key);
    return r;
}
static void sa13_sset(sa13_ctx_t *ctx, const char *name, const char *key, const json_t *v) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_OBJECT) { d = json_object(); json_set(ctx->state, name, d); }
    json_set(d, key, json_copy(v ? v : json_null()));
}
static void sa13_sdel(sa13_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    if (d && d->type == JSON_OBJECT) json_obj_del(d, key);
}

/* Exception-family raise inside a try -> logger.warn/debug/error + return "". */
static json_t *sa13_try_log(sa13_ctx_t *ctx, json_t *r, const char *label, const char *seam) {
    json_t *w = sa13_o(); sa13_ss(w, "label", label);
    sa13_sc(w, "error", json_obj_get(r, "message"));
    json_free(r);
    json_t *lr = sa13_call(ctx, seam, w);
    if (sa13_is_raise(lr)) return lr;
    json_free(lr);
    return json_string("");
}

/* t() -> identity of the key (deterministic stand-in, both ports). */
static json_t *sa13_t(sa13_ctx_t *ctx, const char *key, const json_t *args) {
    (void)ctx; (void)args;
    return json_string(key ? key : "");
}

/* ====================================================================== */

/* -- state-set helpers (deduped arrays; echo in out.state) -- */
static bool sa13_set_has_str(sa13_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_ARRAY || !key) return false;
    for (size_t i = 0; i < json_len(d); i++) {
        const char *v = sa13_str(json_get(d, i));
        if (v && strcmp(v, key) == 0) return true;
    }
    return false;
}
static void sa13_set_add(sa13_ctx_t *ctx, const char *name, const json_t *val) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_ARRAY) {
        if (d) json_obj_del(ctx->state, name);
        d = json_array(); json_set(ctx->state, name, d);
    }
    const char *ks = sa13_str(val);
    if (ks && sa13_set_has_str(ctx, name, ks)) return;
    json_append(d, json_copy(val ? val : json_null()));
}
static json_t *sa13_copy_or_null(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_null();
    return json_copy(v);
}
static bool sa13_env_yes(const char *s) {
    return s && (strcmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcmp(s, "yes") == 0);
}

/* ---- extra helpers for wave 13 ---- */
static bool sa13_contains(const char *hay, const char *needle) {
    if (!hay || !needle) return false;
    return strstr(hay, needle) != NULL;
}
/* Python str(p).strip().strip(":") */
static char *sa13_colon_strip(const json_t *v) {
    char *a = sa13_strval(v);
    char *b = sa13_strip(a);
    size_t n = strlen(b);
    while (n > 0 && b[n - 1] == ':') b[--n] = '\0';
    size_t s = 0;
    while (b[s] == ':') s++;
    if (s) memmove(b, b + s, n - s + 1);
    free(a);
    return b;
}
static int sa13_cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}
/* {"__set__": sorted-unique of str(p).strip().strip(":")} */
static json_t *sa13_set_from(const json_t *arr) {
    json_t *out = sa13_o();
    json_t *sa = json_array();
    if (arr && arr->type == JSON_ARRAY) {
        char **items = (char **)calloc(json_len(arr) + 1, sizeof(char *));
        size_t n = 0;
        for (size_t i = 0; i < json_len(arr); i++) {
            char *v = sa13_colon_strip(json_get(arr, i));
            if (v[0]) {
                bool dup = false;
                for (size_t j = 0; j < n; j++) if (strcmp(items[j], v) == 0) { dup = true; break; }
                if (!dup) items[n++] = v;
                else free(v);
            } else free(v);
        }
        if (n > 1) qsort(items, n, sizeof(char *), sa13_cmp_str);
        for (size_t i = 0; i < n; i++) { json_append(sa, json_string(items[i])); free(items[i]); }
        free(items);
    }
    json_set(out, "__set__", sa);
    return out;
}
/* nested (obj.get(key) or {}).get(idkey, "") -- plain dict access */
static const char *sa13_nested_id(const json_t *obj, const char *key, const char *idkey) {
    const json_t *n = obj ? json_obj_get(obj, key) : NULL;
    if (!n || n->type != JSON_OBJECT) return "";
    const json_t *v = json_obj_get(n, idkey);
    return sa13_s(v, "");
}


/* -- wave-13 extras -- */
static const char *sa13_emsg(const json_t *r) { return json_get_str(r, "message", ""); }
static json_t *sa13_sr(bool ok, const char *err, const json_t *mid) {
    json_t *o = sa13_o();
    sa13_sb(o, "success", ok);
    json_set(o, "error", err ? json_string(err) : json_null());
    json_set(o, "message_id", mid ? json_copy(mid) : json_null());
    return o;
}
static char *sa13_capitalize(const char *s) {
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
static char *sa13_rstrip(const char *s) {
    char *v = sa13_strdup(s ? s : "");
    if (!v) return NULL;
    size_t n = strlen(v);
    while (n > 0 && sa13_is_ws((unsigned char)v[n - 1])) v[--n] = '\0';
    return v;
}
/* dict state map get as string, with "" default */
static const char *sa13_mapstr(const json_t *map, const char *key) {
    const json_t *v = (map && map->type == JSON_OBJECT) ? json_obj_get(map, key) : NULL;
    return sa13_s(v, "");
}



/* PoP: sa13_resolve_file_stub @ plugins/platforms/slack/adapter.py:_resolve_file_stub */
static json_t *sa13_resolve_file_stub(sa13_ctx_t *ctx, const json_t *args) {
    const json_t *f0 = json_obj_get(args, "f");
    json_t *f = (f0 && f0->type == JSON_OBJECT) ? json_copy(f0) : json_object();
    const char *fid = sa13_s(json_obj_get(f, "id"), "");
    if (!fid[0]) { json_free(f); return json_null(); }
    const json_t *notices0 = json_obj_get(args, "notices");
    bool has_notices = notices0 && notices0->type != JSON_NULL;
    json_t *aw = sa13_o();
    sa13_ss(aw, "channel_id", sa13_s(json_obj_get(args, "channel_id"), ""));
    sa13_sc(aw, "team_id", json_obj_get(args, "team_id"));
    json_t *client = sa13_call(ctx, "get_client", aw);
    json_t *info = NULL;
    if (sa13_is_raise(client)) { info = client; client = NULL; }
    else {
        json_t *iw = sa13_o(); sa13_ss(iw, "file", fid); sa13_sc(iw, "client", client); json_free(client);
        info = sa13_call(ctx, "files_info", iw);
    }
    if (sa13_is_raise(info)) {
        if (sa13_raise_passes(info)) { json_free(f); return info; }
        if (has_notices) {
            json_t *dw = sa13_o(); sa13_sc(dw, "error", json_obj_get(info, "message")); sa13_sc(dw, "file", f);
            json_t *detail = sa13_call(ctx, "describe_slack_api_error", dw);
            json_t *nw = sa13_o();
            sa13_sc(nw, "notices", notices0); sa13_sc(nw, "detail", detail);
            sa13_ss(nw, "label", "files.info error for %s: %s"); sa13_ss(nw, "arg", fid);
            sa13_sc(nw, "exc", json_obj_get(info, "message")); json_free(detail);
            json_t *nr = sa13_call(ctx, "note_attachment_failure", nw);
            if (sa13_is_raise(nr) && sa13_raise_passes(nr)) { json_free(info); json_free(f); return nr; }
            json_free(nr);
        }
        json_free(info); json_free(f); return json_null();
    }
    if (sa13_truthy(json_obj_get(info, "ok"))) {
        const json_t *fl = json_obj_get(info, "file");
        json_t *r = json_copy(fl ? fl : json_null()); json_free(info); json_free(f); return r;
    }
    if (has_notices) {
        json_t *dw = sa13_o(); sa13_sc(dw, "error", info); sa13_sc(dw, "file", f);
        json_t *detail = sa13_call(ctx, "describe_slack_api_error", dw);
        json_t *nw = sa13_o();
        sa13_sc(nw, "notices", notices0); sa13_sc(nw, "detail", detail);
        sa13_ss(nw, "label", "files.info failed for %s: %s"); sa13_ss(nw, "arg", fid);
        sa13_sc(nw, "extra", json_obj_get(info, "error")); json_free(detail);
        json_t *nr = sa13_call(ctx, "note_attachment_failure", nw);
        if (sa13_is_raise(nr) && sa13_raise_passes(nr)) { json_free(info); json_free(f); return nr; }
        json_free(nr);
    }
    json_free(info); json_free(f); return json_null();
}

/* PoP: sa13_resolve_slack_user_dm @ plugins/platforms/slack/adapter.py:_resolve_slack_user_dm */
static json_t *sa13_resolve_slack_user_dm(sa13_ctx_t *ctx, const json_t *args) {
    const char *token = sa13_s(json_obj_get(args, "token"), "");
    const char *uid = sa13_s(json_obj_get(args, "user_id"), "");
    char key[600]; snprintf(key, sizeof key, "%s:%s", token, uid);
    json_t *cache = json_obj_get(ctx->state, "_slack_dm_cache");
    if (!cache || cache->type != JSON_OBJECT) { cache = json_object(); json_set(ctx->state, "_slack_dm_cache", cache); }
    const json_t *hit = json_obj_get(cache, key);
    if (hit) return json_copy(hit);
    json_t *imp = sa13_call(ctx, "import_aiohttp", sa13_o());
    if (sa13_is_raise(imp)) { if (sa13_raise_passes(imp)) return imp; json_free(imp); return json_null(); }
    bool have = sa13_truthy(imp); json_free(imp);
    if (!have) return json_null();
    json_t *kw = sa13_o(); sa13_ss(kw, "token", token); sa13_ss(kw, "user_id", uid);
    json_t *sesskw = sa13_call(ctx, "standalone_proxy_kwargs", kw);
    if (sa13_is_raise(sesskw)) { if (sa13_raise_passes(sesskw)) return sesskw; json_free(sesskw); return json_null(); }
    json_t *pw = sa13_o();
    sa13_ss(pw, "token", token); sa13_ss(pw, "method", "conversations.open");
    sa13_ss(pw, "users", uid); sa13_sc(pw, "kwargs", sesskw); json_free(sesskw);
    json_t *data = sa13_call(ctx, "slack_json_post", pw);
    if (sa13_is_raise(data)) {
        if (sa13_raise_passes(data)) return data;
        json_t *ww = sa13_o(); sa13_ss(ww, "label", "[Slack] conversations.open exception for %s: %s");
        sa13_ss(ww, "arg", uid); sa13_sc(ww, "error", json_obj_get(data, "message")); json_free(data);
        json_t *lr = sa13_call(ctx, "logger.warning", ww);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) return lr; json_free(lr);
        return json_null();
    }
    const json_t *ch = json_obj_get(data, "channel");
    const char *cid = (ch && ch->type == JSON_OBJECT) ? sa13_s(json_obj_get((json_t *)ch, "id"), "") : "";
    if (sa13_truthy(json_obj_get(data, "ok")) && cid[0]) {
        json_set(cache, key, json_string(cid));
        json_t *tw = sa13_o(); sa13_sc(tw, "cache", cache);
        json_t *tr = sa13_call(ctx, "trim_slack_dm_cache", tw);
        if (sa13_is_raise(tr) && sa13_raise_passes(tr)) { json_free(data); return tr; }
        json_free(tr); json_free(data); return json_string(cid);
    }
    json_t *ww = sa13_o();
    sa13_ss(ww, "label", "[Slack] conversations.open failed for %s: %s"); sa13_ss(ww, "arg", uid);
    const char *err = sa13_s(json_obj_get(data, "error"), "unknown");
    sa13_ss(ww, "error", err); json_free(data);
    json_t *lr = sa13_call(ctx, "logger.warning", ww);
    if (sa13_is_raise(lr) && sa13_raise_passes(lr)) return lr; json_free(lr);
    return json_null();
}

/* PoP: sa13_resolve_user_is_bot @ plugins/platforms/slack/adapter.py:_resolve_user_is_bot */
static json_t *sa13_resolve_user_is_bot(sa13_ctx_t *ctx, const json_t *args) {
    const json_t *uidv = json_obj_get(args, "user_id");
    if (!sa13_truthy(uidv)) return json_bool(false);
    char *uids = sa13_strval(uidv);
    const json_t *ct = json_obj_get(ctx->state, "_channel_team");
    const char *team0 = sa13_s(json_obj_get(args, "team_id"), "");
    char tid[256];
    if (team0[0]) snprintf(tid, sizeof tid, "%s", team0);
    else snprintf(tid, sizeof tid, "%s", sa13_mapstr(ct, sa13_s(json_obj_get(args, "chat_id"), "")));
    char key[600]; snprintf(key, sizeof key, "%s|%s", tid, uids);
    json_t *bc = json_obj_get(ctx->state, "_user_is_bot_cache");
    if (!bc || bc->type != JSON_OBJECT) { bc = json_object(); json_set(ctx->state, "_user_is_bot_cache", bc); }
    const json_t *hit = json_obj_get(bc, key);
    if (hit) { json_t *r = json_copy(hit); free(uids); return r; }
    json_t *ap = sa13_call(ctx, "has_app", sa13_o());
    if (sa13_is_raise(ap)) {
        if (sa13_raise_passes(ap)) { free(uids); return ap; }
        json_free(ap);
    } else if (!sa13_truthy(ap)) {
        json_free(ap); json_set(bc, key, json_bool(false)); free(uids); return json_bool(false);
    } else json_free(ap);
    json_t *pw = sa13_o();
    sa13_sc(pw, "user_id", uidv); sa13_sc(pw, "chat_id", json_obj_get(args, "chat_id")); sa13_ss(pw, "team_id", tid);
    json_t *payload = sa13_call(ctx, "users_info_payload", pw);
    if (sa13_is_raise(payload)) {
        if (sa13_raise_passes(payload)) { free(uids); return payload; }
        json_t *dw = sa13_o(); sa13_ss(dw, "label", "[Slack] users.info bot check failed for %s: %s");
        sa13_ss(dw, "arg", uids); sa13_sc(dw, "error", json_obj_get(payload, "message")); json_free(payload);
        json_t *lr = sa13_call(ctx, "logger.debug", dw);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) { free(uids); return lr; } json_free(lr);
        json_set(bc, key, json_bool(false)); free(uids); return json_bool(false);
    }
    if (!sa13_truthy(payload)) {
        json_set(bc, key, json_bool(false));
        json_t *nc = json_obj_get(ctx->state, "_user_name_cache");
        if (!nc || nc->type != JSON_OBJECT) { nc = json_object(); json_set(ctx->state, "_user_name_cache", nc); }
        if (!json_obj_get(nc, key)) json_set(nc, key, json_copy(uidv));
        json_free(payload); free(uids); return json_bool(false);
    }
    json_t *qw = sa13_o(); sa13_sc(qw, "payload", payload); sa13_sc(qw, "user_id", uidv); json_free(payload);
    json_t *parsed = sa13_call(ctx, "parse_users_info", qw);
    if (sa13_is_raise(parsed) && sa13_raise_passes(parsed)) { free(uids); return parsed; }
    const json_t *nm = (parsed && parsed->type == JSON_ARRAY && json_len(parsed) > 0) ? json_get(parsed, 0) : NULL;
    const json_t *ib = (parsed && parsed->type == JSON_ARRAY && json_len(parsed) > 1) ? json_get(parsed, 1) : NULL;
    bool isbot = sa13_truthy(ib);
    char *name = sa13_strval(nm ? nm : uidv);
    json_set(bc, key, json_bool(isbot));
    json_t *tw = sa13_o(); sa13_sc(tw, "items", bc);
    sa13_sn(tw, "max_size", (double)sa13_gi(ctx->state, "_USER_NAME_CACHE_MAX", 5000));
    json_t *tr = sa13_call(ctx, "trim_oldest_dict_entries", tw);
    if (sa13_is_raise(tr) && sa13_raise_passes(tr)) { json_free(parsed); free(name); free(uids); return tr; } json_free(tr);
    json_t *nc = json_obj_get(ctx->state, "_user_name_cache");
    if (!nc || nc->type != JSON_OBJECT) { nc = json_object(); json_set(ctx->state, "_user_name_cache", nc); }
    json_set(nc, key, json_string(name));
    json_t *out = json_bool(isbot);
    json_free(parsed); free(name); free(uids); return out;
}

/* PoP: sa13_resolve_user_name @ plugins/platforms/slack/adapter.py:_resolve_user_name */
static json_t *sa13_resolve_user_name(sa13_ctx_t *ctx, const json_t *args) {
    const json_t *uidv = json_obj_get(args, "user_id");
    if (!sa13_truthy(uidv)) return json_string("");
    char *uids = sa13_strval(uidv);
    const json_t *ct = json_obj_get(ctx->state, "_channel_team");
    const char *team0 = sa13_s(json_obj_get(args, "team_id"), "");
    char tid[256];
    if (team0[0]) snprintf(tid, sizeof tid, "%s", team0);
    else snprintf(tid, sizeof tid, "%s", sa13_mapstr(ct, sa13_s(json_obj_get(args, "chat_id"), "")));
    char key[600]; snprintf(key, sizeof key, "%s|%s", tid, uids);
    json_t *nc = json_obj_get(ctx->state, "_user_name_cache");
    if (!nc || nc->type != JSON_OBJECT) { nc = json_object(); json_set(ctx->state, "_user_name_cache", nc); }
    const json_t *hit = json_obj_get(nc, key);
    if (hit && hit->type != JSON_NULL) { json_t *r = json_copy(hit); free(uids); return r; }
    json_t *ap = sa13_call(ctx, "has_app", sa13_o());
    if (sa13_is_raise(ap)) {
        if (sa13_raise_passes(ap)) { free(uids); return ap; }
        json_free(ap);
    } else if (!sa13_truthy(ap)) {
        json_free(ap); json_t *r = json_string(uids); free(uids); return r;
    } else json_free(ap);
    json_t *pw = sa13_o();
    sa13_sc(pw, "user_id", uidv); sa13_sc(pw, "chat_id", json_obj_get(args, "chat_id")); sa13_ss(pw, "team_id", tid);
    json_t *payload = sa13_call(ctx, "users_info_payload", pw);
    char *name = NULL;
    if (sa13_is_raise(payload)) {
        if (sa13_raise_passes(payload)) { free(uids); return payload; }
        json_t *dw = sa13_o(); sa13_ss(dw, "label", "[Slack] users.info failed for %s: %s");
        sa13_ss(dw, "arg", uids); sa13_sc(dw, "error", json_obj_get(payload, "message")); json_free(payload);
        json_t *lr = sa13_call(ctx, "logger.debug", dw);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) { free(uids); return lr; } json_free(lr);
        name = sa13_strdup(uids);
    } else if (!sa13_truthy(payload)) {
        json_t *bc = json_obj_get(ctx->state, "_user_is_bot_cache");
        if (!bc || bc->type != JSON_OBJECT) { bc = json_object(); json_set(ctx->state, "_user_is_bot_cache", bc); }
        json_set(bc, key, json_bool(false));
        json_set(nc, key, json_copy(uidv)); json_free(payload);
        name = sa13_strdup(uids);
    } else {
        json_t *qw = sa13_o(); sa13_sc(qw, "payload", payload); sa13_sc(qw, "user_id", uidv); json_free(payload);
        json_t *parsed = sa13_call(ctx, "parse_users_info", qw);
        if (sa13_is_raise(parsed) && sa13_raise_passes(parsed)) { free(uids); return parsed; }
        const json_t *nm = (parsed && parsed->type == JSON_ARRAY && json_len(parsed) > 0) ? json_get(parsed, 0) : NULL;
        const json_t *ib = (parsed && parsed->type == JSON_ARRAY && json_len(parsed) > 1) ? json_get(parsed, 1) : NULL;
        json_t *bc = json_obj_get(ctx->state, "_user_is_bot_cache");
        if (!bc || bc->type != JSON_OBJECT) { bc = json_object(); json_set(ctx->state, "_user_is_bot_cache", bc); }
        json_set(bc, key, json_copy(ib ? ib : json_bool(false)));
        name = sa13_strval(nm ? nm : uidv);
        json_free(parsed);
    }
    json_set(nc, key, json_string(name));
    json_t *tw = sa13_o(); sa13_sc(tw, "items", nc);
    sa13_sn(tw, "max_size", (double)sa13_gi(ctx->state, "_USER_NAME_CACHE_MAX", 5000));
    json_t *tr = sa13_call(ctx, "trim_oldest_dict_entries", tw);
    if (sa13_is_raise(tr) && sa13_raise_passes(tr)) { free(name); free(uids); return tr; } json_free(tr);
    json_t *out = json_string(name); free(name); free(uids); return out;
}

/* PoP: sa13_restart_socket_mode @ plugins/platforms/slack/adapter.py:_restart_socket_mode */
static json_t *sa13_restart_socket_mode(sa13_ctx_t *ctx, const json_t *args) {
    json_t *r1 = sa13_call(ctx, "is_running", sa13_o());
    if (sa13_is_raise(r1)) { if (sa13_raise_passes(r1)) return r1; json_free(r1); }
    else if (!sa13_truthy(r1)) { json_free(r1); return json_null(); }
    else json_free(r1);
    json_t *r2 = sa13_call(ctx, "is_running", sa13_o());
    if (sa13_is_raise(r2)) { if (sa13_raise_passes(r2)) return r2; json_free(r2); }
    else if (!sa13_truthy(r2)) { json_free(r2); return json_null(); }
    else json_free(r2);
    json_t *app = sa13_call(ctx, "has_app", sa13_o());
    if (sa13_is_raise(app)) { if (sa13_raise_passes(app)) return app; json_free(app); }
    else if (!sa13_truthy(app)) { json_free(app); return json_null(); }
    else json_free(app);
    json_t *tok = sa13_call(ctx, "has_app_token", sa13_o());
    if (sa13_is_raise(tok)) { if (sa13_raise_passes(tok)) return tok; json_free(tok); }
    else if (!sa13_truthy(tok)) { json_free(tok); return json_null(); }
    else json_free(tok);
    json_t *ww = sa13_o(); sa13_ss(ww, "label", "[Slack] Socket Mode unhealthy (%s); reconnecting");
    sa13_sc(ww, "arg", json_obj_get(args, "reason"));
    json_t *lr = sa13_call(ctx, "logger.warning", ww);
    if (sa13_is_raise(lr) && sa13_raise_passes(lr)) return lr; json_free(lr);
    json_t *sr = sa13_call(ctx, "stop_socket_mode_handler", sa13_o());
    if (sa13_is_raise(sr) && sa13_raise_passes(sr)) return sr; json_free(sr);
    json_t *st = sa13_call(ctx, "start_socket_mode_handler", sa13_o());
    if (sa13_is_raise(st)) {
        if (sa13_raise_passes(st)) return st;
        json_t *ew = sa13_o(); sa13_ss(ew, "label", "[Slack] Socket Mode reconnect failed: %s");
        sa13_sc(ew, "error", json_obj_get(st, "message")); json_free(st);
        json_t *er = sa13_call(ctx, "logger.error", ew);
        if (sa13_is_raise(er) && sa13_raise_passes(er)) return er; json_free(er);
    } else json_free(st);
    return json_null();
}

/* PoP: sa13_seal_stale_streams @ plugins/platforms/slack/adapter.py:_seal_stale_streams */
static json_t *sa13_seal_stale_streams(sa13_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *now = sa13_call(ctx, "time.time", sa13_o());
    if (sa13_is_raise(now)) { if (sa13_raise_passes(now)) return now; json_free(now); return json_null(); }
    double cutoff = sa13_num(now, 0.0) - (double)sa13_gi(ctx->state, "_STREAM_MAX_AGE_S", 0); json_free(now);
    json_t *streams = json_obj_get(ctx->state, "_active_streams");
    if (!streams || streams->type != JSON_OBJECT) return json_null();
    /* snapshot keys: iterate in object insertion order */
    size_t n = streams->c.count;
    char **keys = (char **)calloc(n + 1, sizeof(char *));
    if (!keys) return json_null();
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        const char *ky = streams->c.keys ? streams->c.keys[i] : NULL;
        if (ky) keys[k++] = sa13_strdup(ky);
    }
    for (size_t i = 0; i < k; i++) {
        const json_t *st = json_obj_get(streams, keys[i]);
        double started = (st && st->type == JSON_OBJECT) ? sa13_num(json_obj_get((json_t *)st, "started"), 0.0) : 0.0;
        if (started < cutoff) {
            json_t *sw = sa13_o(); sa13_ss(sw, "key", keys[i]); sa13_sc(sw, "stream", st);
            json_obj_del(streams, keys[i]);
            json_t *r = sa13_call(ctx, "seal_stream", sw);
            if (sa13_is_raise(r) && sa13_raise_passes(r)) { for (size_t j = 0; j < k; j++) free(keys[j]); free(keys); return r; }
            json_free(r);
        }
        free(keys[i]);
    }
    free(keys);
    return json_null();
}

/* PoP: sa13_seal_stream @ plugins/platforms/slack/adapter.py:_seal_stream */
static json_t *sa13_seal_stream(sa13_ctx_t *ctx, const json_t *args) {
    const json_t *key = json_obj_get(args, "key");
    const char *team = (key && key->type == JSON_ARRAY && json_len(key) > 0) ? sa13_strval(json_get(key, 0)) : "";
    const char *chat = (key && key->type == JSON_ARRAY && json_len(key) > 1) ? sa13_strval(json_get(key, 1)) : "";
    const json_t *stream0 = json_obj_get(args, "stream");
    json_t *stream = (stream0 && stream0->type == JSON_OBJECT) ? json_copy(stream0) : json_object();
    const json_t *delta = json_obj_get(args, "delta");
    char *dtxt = sa13_strval(delta);
    char *dstrip = sa13_strip(dtxt);
    bool add_delta = sa13_truthy(delta) && dstrip[0] != '\0';
    free(dstrip); free(dtxt);
    json_t *aw = sa13_o(); sa13_ss(aw, "channel", chat);
    sa13_sc(aw, "team_id", (team && team[0]) ? json_string(team) : json_null());
    json_t *client = sa13_call(ctx, "get_client", aw);
    if (sa13_is_raise(client)) {
        if (sa13_raise_passes(client)) { json_free(stream); return client; }
        json_t *dw = sa13_o(); sa13_ss(dw, "label", "[Slack] chat.stopStream failed for %s/%s: %s");
        json_t *ar = json_array(); json_append(ar, json_string(chat));
        json_append(ar, json_copy(json_obj_get(stream, "ts") ? json_obj_get(stream, "ts") : json_null()));
        json_append(ar, json_copy(json_obj_get(client, "message")));
        json_set(dw, "args", ar); json_free(client);
        json_t *lr = sa13_call(ctx, "logger.debug", dw);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) { json_free(stream); return lr; } json_free(lr);
        json_free(stream); return json_bool(false);
    }
    json_t *kw = sa13_o();
    sa13_ss(kw, "channel", chat); sa13_sc(kw, "ts", json_obj_get(stream, "ts"));
    if (add_delta) sa13_ss(kw, "markdown_text", sa13_s(delta, ""));
    sa13_sc(kw, "client", client); json_free(client);
    json_t *r = sa13_call(ctx, "chat_stopStream", kw);
    if (sa13_is_raise(r) && sa13_raise_passes(r)) { json_free(stream); return r; }
    if (sa13_is_raise(r)) {
        json_t *dw = sa13_o(); sa13_ss(dw, "label", "[Slack] chat.stopStream failed for %s/%s: %s");
        json_t *ar = json_array(); json_append(ar, json_string(chat));
        json_append(ar, json_copy(json_obj_get(stream, "ts") ? json_obj_get(stream, "ts") : json_null()));
        json_append(ar, json_copy(json_obj_get(r, "message")));
        json_set(dw, "args", ar); json_free(r);
        json_t *lr = sa13_call(ctx, "logger.debug", dw);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) { json_free(stream); return lr; } json_free(lr);
        json_free(stream); return json_bool(false);
    }
    json_free(r); json_free(stream); return json_bool(true);
}

/* PoP: sa13_seed_dm_session @ plugins/platforms/slack/adapter.py:_seed_dm_session */
static json_t *sa13_seed_dm_session(sa13_ctx_t *ctx, const json_t *args) {
    json_t *ss0 = sa13_call(ctx, "get_session_store", sa13_o());
    if (sa13_is_raise(ss0)) { if (sa13_raise_passes(ss0)) return ss0; json_free(ss0); }
    bool have_ss = sa13_truthy(ss0); json_free(ss0);
    const json_t *meta0 = json_obj_get(args, "metadata");
    json_t *meta = (meta0 && meta0->type == JSON_OBJECT) ? json_copy(meta0) : json_object();
    const char *cid = sa13_s(json_obj_get(meta, "channel_id"), "");
    const char *uid = sa13_s(json_obj_get(meta, "user_id"), "");
    if (!have_ss || !cid[0] || !uid[0]) { json_free(meta); return json_null(); }
    const json_t *nc0 = json_obj_get(ctx->state, "_channel_name_cache");
    const char *tid = sa13_s(json_obj_get(meta, "team_id"), "");
    char ck[600]; snprintf(ck, sizeof ck, "%s|%s", tid, cid);
    const char *cname = (nc0 && nc0->type == JSON_OBJECT && json_obj_get((json_t *)nc0, ck))
                        ? sa13_s(json_obj_get((json_t *)nc0, ck), cid) : cid;
    json_t *sw = sa13_o();
    sa13_ss(sw, "chat_id", cid); sa13_ss(sw, "chat_name", cname); sa13_ss(sw, "chat_type", "dm");
    sa13_ss(sw, "user_id", uid); sa13_sc(sw, "thread_id", json_obj_get(args, "thread_ts"));
    const json_t *topic = json_obj_get(meta, "context_channel_id");
    json_set(sw, "chat_topic", (topic && sa13_truthy(topic)) ? json_copy(topic) : json_null());
    const json_t *scope = json_obj_get(meta, "team_id");
    json_set(sw, "scope_id", (scope && sa13_truthy(scope)) ? json_copy(scope) : json_null());
    json_free(meta);
    json_t *src = sa13_call(ctx, "build_source", sw);
    if (sa13_is_raise(src) && sa13_raise_passes(src)) return src;
    json_t *tw = sa13_o(); sa13_ss(tw, "method", "get_or_create_session"); sa13_sc(tw, "args", json_array());
    json_t *ar = json_array(); json_append(ar, json_copy(src)); json_set(tw, "args", ar); json_free(src);
    json_t *r = sa13_call(ctx, "to_thread", tw);
    if (sa13_is_raise(r)) {
        if (sa13_raise_passes(r)) return r;
        const json_t *fl = json_obj_get(args, "fail_log");
        json_t *lw = sa13_o(); json_set(lw, "args", json_copy(fl ? fl : json_array())); sa13_sb(lw, "exc_info", true);
        json_free(r);
        json_t *lr = sa13_call(ctx, "logger.debug", lw);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) return lr; json_free(lr);
    } else json_free(r);
    return json_null();
}

/* PoP: sa13_send_failure_notice @ plugins/platforms/slack/adapter.py:_send_failure_notice */
static json_t *sa13_send_failure_notice(sa13_ctx_t *ctx, const json_t *args) {
    json_t *w = sa13_o();
    sa13_sc(w, "chat_id", json_obj_get(args, "chat_id")); sa13_sc(w, "notice", json_obj_get(args, "notice"));
    sa13_sc(w, "caption", json_obj_get(args, "caption")); sa13_sc(w, "reply_to", json_obj_get(args, "reply_to"));
    sa13_sc(w, "metadata", json_obj_get(args, "metadata"));
    sa13_sc(w, "shown_metadata", json_obj_get(args, "metadata"));
    return sa13_call(ctx, "emit_media_warning", w);
}

/* PoP: sa13_send_interactive_prompt @ plugins/platforms/slack/adapter.py:_send_interactive_prompt */
static json_t *sa13_send_interactive_prompt(sa13_ctx_t *ctx, const json_t *args) {
    json_t *ap = sa13_call(ctx, "has_app", sa13_o());
    if (sa13_is_raise(ap)) { if (sa13_raise_passes(ap)) return ap; json_free(ap); }
    else if (!sa13_truthy(ap)) { json_free(ap); return sa13_sr(false, "Not connected", NULL); }
    else json_free(ap);
    json_t *dw = sa13_o(); sa13_sc(dw, "chat_id", json_obj_get(args, "chat_id")); sa13_sc(dw, "metadata", json_obj_get(args, "metadata"));
    json_t *cidv = sa13_call(ctx, "dm_target", dw);
    if (sa13_is_raise(cidv) && sa13_raise_passes(cidv)) return cidv;
    json_t *bw = sa13_o(); sa13_sc(bw, "chat_id", cidv); sa13_sc(bw, "metadata", json_obj_get(args, "metadata"));
    json_t *built = sa13_call(ctx, "build", bw);
    if (sa13_is_raise(built) && sa13_raise_passes(built)) { json_free(cidv); return built; }
    const json_t *text = (built && built->type == JSON_ARRAY && json_len(built) > 0) ? json_get(built, 0) : NULL;
    const json_t *blocks = (built && built->type == JSON_ARRAY && json_len(built) > 1) ? json_get(built, 1) : NULL;
    json_t *pw = sa13_o(); sa13_sc(pw, "chat_id", cidv); sa13_sc(pw, "text", text); sa13_sc(pw, "blocks", blocks);
    sa13_sc(pw, "metadata", json_obj_get(args, "metadata"));
    sa13_sc(pw, "sanitize", json_obj_get(args, "sanitize")); sa13_sc(pw, "team_scoped", json_obj_get(args, "team_scoped_key"));
    json_free(built);
    json_t *result = sa13_call(ctx, "post_interactive_blocks", pw);
    if (sa13_is_raise(result)) {
        if (sa13_raise_passes(result)) { json_free(cidv); return result; }
        json_t *ew = sa13_o(); sa13_sc(ew, "label", json_obj_get(args, "label")); sa13_sc(ew, "error", json_obj_get(result, "message")); json_free(result);
        json_t *lr = sa13_call(ctx, "logger.error", ew);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) { json_free(cidv); return lr; } json_free(lr);
        json_free(cidv); return sa13_sr(false, NULL, NULL);
    }
    const char *ts = sa13_s(json_obj_get(result, "ts"), "");
    const json_t *resolved = json_obj_get(args, "resolved");
    bool use_resolved = resolved && resolved->type == JSON_OBJECT;
    json_t *rd = use_resolved ? json_copy(resolved) : NULL;
    if (ts[0] && use_resolved) {
        const char *key = ts;
        bool scoped = true;
        const json_t *sv = json_obj_get(args, "team_scoped_key");
        if (sv && sv->type == JSON_BOOL) scoped = sv->bool_val;
        char *owned = NULL;
        if (scoped) {
            json_t *mw = sa13_o(); sa13_sc(mw, "metadata", json_obj_get(args, "metadata"));
            json_t *team = sa13_call(ctx, "metadata_team_id", mw);
            if (sa13_is_raise(team) && sa13_raise_passes(team)) { json_free(team); json_free(rd); json_free(cidv); json_free(result); return team; }
            json_t *kw = sa13_o(); sa13_sc(kw, "team_id", team); sa13_ss(kw, "ts", ts); json_free(team);
            json_t *marker = sa13_call(ctx, "workspace_message_marker", kw);
            if (sa13_is_raise(marker) && sa13_raise_passes(marker)) { json_free(marker); json_free(rd); json_free(cidv); json_free(result); return marker; }
            owned = sa13_strval(marker); json_free(marker); key = owned;
        }
        json_set(rd, key, json_bool(false));
        json_t *tw = sa13_o(); sa13_sc(tw, "items", rd);
        sa13_sn(tw, "max_size", (double)sa13_gi(args, "resolved_max", 0));
        json_t *tr = sa13_call(ctx, "trim_oldest_dict_entries", tw);
        if (sa13_is_raise(tr) && sa13_raise_passes(tr)) { json_free(tr); free(owned); json_free(rd); json_free(cidv); json_free(result); return tr; }
        json_free(tr); free(owned);
    }
    json_t *out = sa13_sr(true, NULL, json_string(ts));
    json_set(out, "raw_response", json_copy(result));
    json_free(rd); json_free(cidv); json_free(result);
    return out;
}

/* PoP: sa13_send_local_file @ plugins/platforms/slack/adapter.py:_send_local_file */
static json_t *sa13_send_local_file(sa13_ctx_t *ctx, const json_t *args) {
    json_t *ap = sa13_call(ctx, "has_app", sa13_o());
    if (sa13_is_raise(ap)) { if (sa13_raise_passes(ap)) return ap; json_free(ap); }
    else if (!sa13_truthy(ap)) { json_free(ap); return sa13_sr(false, "Not connected", NULL); }
    else json_free(ap);
    json_t *ew = sa13_o(); sa13_sc(ew, "path", json_obj_get(args, "file_path"));
    json_t *ex = sa13_call(ctx, "exists", ew);
    if (sa13_is_raise(ex)) { if (sa13_raise_passes(ex)) return ex; json_free(ex); }
    else if (!sa13_truthy(ex)) {
        json_free(ex); const char *nf = sa13_s(json_obj_get(args, "not_found_error"), "");
        json_t *r = sa13_sr(false, nf, NULL); return r;
    } else json_free(ex);
    json_t *dw = sa13_o(); sa13_sc(dw, "chat_id", json_obj_get(args, "chat_id")); sa13_sc(dw, "metadata", json_obj_get(args, "metadata"));
    json_t *cidv = sa13_call(ctx, "dm_target", dw);
    if (sa13_is_raise(cidv) && sa13_raise_passes(cidv)) return cidv;
    json_t *rw = sa13_o(); sa13_sc(rw, "reply_to", json_obj_get(args, "reply_to")); sa13_sc(rw, "metadata", json_obj_get(args, "metadata"));
    json_t *ts = sa13_call(ctx, "resolve_thread_ts", rw);
    if (sa13_is_raise(ts) && sa13_raise_passes(ts)) { json_free(cidv); return ts; }
    char *kind = sa13_strdup(sa13_s(json_obj_get(args, "kind"), ""));
    char *label = sa13_capitalize(kind);
    char lbl[512]; snprintf(lbl, sizeof lbl, "%s upload", label ? label : "");
    free(kind); free(label);
    json_t *uw = sa13_o();
    sa13_sc(uw, "chat_id", cidv); sa13_sc(uw, "file_path", json_obj_get(args, "file_path"));
    sa13_sc(uw, "filename", json_obj_get(args, "filename")); sa13_sc(uw, "caption", json_obj_get(args, "caption"));
    sa13_sc(uw, "thread_ts", ts); sa13_sc(uw, "metadata", json_obj_get(args, "metadata")); sa13_ss(uw, "label", lbl);
    json_free(ts);
    json_t *r = sa13_call(ctx, "upload_with_retry", uw);
    if (sa13_is_raise(r)) {
        if (sa13_raise_passes(r)) { json_free(cidv); return r; }
        json_t *lw = sa13_o(); sa13_ss(lw, "label", "[%s] Failed to send %s %s: %s");
        json_t *ar = json_array();
        json_append(ar, json_string(sa13_mapstr(json_obj_get(ctx->state, "config"), "name")));
        json_append(ar, json_copy(json_obj_get(args, "kind")));
        json_append(ar, json_copy(json_obj_get(args, "file_path")));
        json_append(ar, json_copy(json_obj_get(r, "message")));
        json_set(lw, "args", ar); json_free(r);
        json_t *lr = sa13_call(ctx, "logger.error", lw);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) { json_free(cidv); return lr; } json_free(lr);
        json_t *fw = sa13_o(); sa13_sc(fw, "chat_id", cidv); sa13_sc(fw, "caption", json_obj_get(args, "caption"));
        sa13_sc(fw, "notice", json_obj_get(args, "failure_notice")); sa13_sc(fw, "reply_to", json_obj_get(args, "reply_to"));
        sa13_sc(fw, "metadata", json_obj_get(args, "metadata")); json_free(cidv);
        return sa13_call(ctx, "send_failure_notice", fw);
    }
    json_free(cidv); return r;
}

/* PoP: sa13_send_exec_approval_prompt @ plugins/platforms/slack/adapter.py:_send_exec_approval_prompt */
static json_t *sa13_send_exec_approval_prompt(sa13_ctx_t *ctx, const json_t *args) {
    const json_t *prompt = json_obj_get(args, "prompt");
    const json_t *acts = (prompt && prompt->type == JSON_OBJECT) ? json_obj_get((json_t *)prompt, "actions") : NULL;
    const json_t *ea = json_obj_get(ctx->state, "_EA_ACTION_IDS");
    json_t *actions = json_array();
    if (acts && acts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(acts); i++) {
            const json_t *it = json_get(acts, i);
            if (!it || it->type != JSON_ARRAY || json_len(it) < 3) continue;
            const json_t *label = json_get(it, 0), *choice = json_get(it, 1), *style = json_get(it, 2);
            const char *aid = sa13_mapstr(ea, sa13_s(choice, ""));
            json_t *bw = sa13_o(); sa13_sc(bw, "label", label); sa13_ss(bw, "action_id", aid);
            sa13_sc(bw, "session_key", (prompt && prompt->type == JSON_OBJECT) ? json_obj_get((json_t *)prompt, "session_key") : NULL);
            sa13_sc(bw, "style", style);
            json_t *b = sa13_call(ctx, "button", bw);
            if (sa13_is_raise(b) && sa13_raise_passes(b)) { json_free(actions); return b; }
            json_free(b);
            json_t *rec = json_array(); json_append(rec, json_copy(label)); json_append(rec, json_string(aid));
            json_append(rec, json_copy(style)); json_append(rec, json_copy(json_null()));
            json_append(actions, rec);
        }
    }
    json_t *blocks = json_array();
    json_t *sec = sa13_o(); sa13_ss(sec, "type", "section");
    json_t *txt = sa13_o(); sa13_ss(txt, "type", "mrkdwn");
    sa13_sc(txt, "text", (prompt && prompt->type == JSON_OBJECT) ? json_obj_get((json_t *)prompt, "text") : NULL);
    json_set(sec, "text", txt); json_append(blocks, sec);
    json_t *actsb = sa13_o(); sa13_ss(actsb, "type", "actions"); json_set(actsb, "elements", actions); json_append(blocks, actsb);
    /* t() identity fallback text */
    char *cmd = sa13_strval((prompt && prompt->type == JSON_OBJECT) ? json_obj_get((json_t *)prompt, "command") : NULL);
    size_t cl = strlen(cmd); if (cl > 100) cmd[100] = '\0';
    char fallback[256]; snprintf(fallback, sizeof fallback, "platform.slack.approval.fallback_text"); (void)cmd;
    json_t *iw = sa13_o();
    sa13_sc(iw, "chat_id", (prompt && prompt->type == JSON_OBJECT) ? json_obj_get((json_t *)prompt, "chat_id") : NULL);
    sa13_sc(iw, "metadata", (prompt && prompt->type == JSON_OBJECT) ? json_obj_get((json_t *)prompt, "metadata") : NULL);
    sa13_ss(iw, "fallback_text", fallback); sa13_sc(iw, "blocks", blocks); sa13_ss(iw, "label", "send_exec_approval");
    sa13_sc(iw, "resolved", json_obj_get(args, "resolved"));
    sa13_sn(iw, "resolved_max", (double)sa13_gi(args, "resolved_max", 5000));
    free(cmd);
    json_t *result = sa13_call(ctx, "send_interactive_prompt", iw);
    if (sa13_is_raise(result) && sa13_raise_passes(result)) return result;
    bool ok = sa13_truthy(json_obj_get(result, "success"));
    const json_t *rid = (prompt && prompt->type == JSON_OBJECT) ? json_obj_get((json_t *)prompt, "request_id") : NULL;
    if (ok && sa13_truthy(rid)) {
        json_t *mw = sa13_o(); sa13_sc(mw, "metadata", (prompt && prompt->type == JSON_OBJECT) ? json_obj_get((json_t *)prompt, "metadata") : NULL);
        json_t *team = sa13_call(ctx, "metadata_team_id", mw);
        if (sa13_is_raise(team) && sa13_raise_passes(team)) { json_free(team); return result; }
        json_t *kw = sa13_o(); sa13_sc(kw, "team_id", team); sa13_sc(kw, "ts", json_obj_get(result, "message_id")); json_free(team);
        json_t *marker = sa13_call(ctx, "workspace_message_marker", kw);
        if (sa13_is_raise(marker) && sa13_raise_passes(marker)) { json_free(marker); return result; }
        json_t *ad = json_obj_get(ctx->state, "_approval_request_ids");
        if (!ad || ad->type != JSON_OBJECT) { ad = json_object(); json_set(ctx->state, "_approval_request_ids", ad); }
        const char *mk = sa13_strval(marker); json_free(marker);
        json_set(ad, mk, json_copy(rid)); free((void *)mk);
        json_t *tw = sa13_o(); sa13_sc(tw, "items", ad);
        sa13_sn(tw, "max_size", (double)sa13_gi(args, "resolved_max", 5000));
        json_t *tr = sa13_call(ctx, "trim_oldest_dict_entries", tw);
        if (sa13_is_raise(tr) && sa13_raise_passes(tr)) { json_free(tr); return result; } json_free(tr);
    }
    return result;
}

/* PoP: sa13_send_slash_ephemeral @ plugins/platforms/slack/adapter.py:_send_slash_ephemeral */
static json_t *sa13_send_slash_ephemeral(sa13_ctx_t *ctx, const json_t *args) {
    const json_t *ctxa = json_obj_get(args, "ctx");
    const char *content = sa13_s(json_obj_get(args, "content"), "");
    json_t *cw = sa13_o(); sa13_ss(cw, "content", content);
    json_t *chunksv = sa13_call(ctx, "format_chunks", cw);
    if (sa13_is_raise(chunksv) && sa13_raise_passes(chunksv)) return chunksv;
    json_t *chunks = (chunksv && chunksv->type == JSON_ARRAY) ? json_copy(chunksv) : json_array(); json_free(chunksv);
    if (json_len(chunks) > 5) {
        const char *notice = "platform.slack.slash.reply_truncated"; /* t() identity */
        char *last = sa13_rstrip(sa13_s(json_get(chunks, 4), ""));
        char joined[2048]; snprintf(joined, sizeof joined, "%s%s", last, notice); free(last);
        json_t *five = json_array();
        for (size_t i = 0; i < 4; i++) json_append(five, json_copy(json_get(chunks, i)));
        json_append(five, json_string(joined));
        json_free(chunks); chunks = five;
    }
    json_t *tv = sa13_call(ctx, "gateway_trust_env", sa13_o());
    if (sa13_is_raise(tv) && sa13_raise_passes(tv)) { json_free(chunks); return tv; }
    json_t *nw = sa13_o(); sa13_sc(nw, "trust_env", tv); json_free(tv);
    json_t *sess = sa13_call(ctx, "new_session", nw);
    if (sa13_is_raise(sess) && sa13_raise_passes(sess)) { json_free(chunks); return sess; }
    json_free(sess);
    for (size_t i = 0; i < json_len(chunks); i++) {
        json_t *pl = sa13_o(); sa13_ss(pl, "response_type", "ephemeral"); sa13_sb(pl, "replace_original", i == 0);
        sa13_sc(pl, "text", json_get(chunks, i));
        json_t *pw = sa13_o();
        sa13_sc(pw, "url", (ctxa && ctxa->type == JSON_OBJECT) ? json_obj_get((json_t *)ctxa, "response_url") : NULL);
        sa13_sc(pw, "json", pl);
        json_t *resp = sa13_call(ctx, "post", pw);
        if (sa13_is_raise(resp)) {
            if (sa13_raise_passes(resp)) { json_free(chunks); return resp; }
            json_t *ww = sa13_o(); sa13_ss(ww, "label", "[Slack] response_url POST failed: %s"); sa13_sc(ww, "arg", json_obj_get(resp, "message")); json_free(resp);
            json_t *lr = sa13_call(ctx, "logger.warning", ww);
            if (sa13_is_raise(lr) && sa13_raise_passes(lr)) { json_free(chunks); return lr; } json_free(lr);
            json_free(chunks); return sa13_sr(false, NULL, NULL);
        }
        double status = sa13_num(json_obj_get(resp, "status"), 0);
        if (status != 200) {
            json_t *rw = sa13_o(); sa13_sc(rw, "response", resp);
            json_t *body = sa13_call(ctx, "read_error_text_limited", rw);
            if (sa13_is_raise(body) && sa13_raise_passes(body)) { json_free(resp); json_free(chunks); return body; }
            char *bt = sa13_strval(body); json_free(body);
            if (strlen(bt) > 200) bt[200] = '\0';
            char msg[256]; snprintf(msg, sizeof msg, "response_url POST returned %g", status);
            json_t *ww = sa13_o(); sa13_ss(ww, "label", "[Slack] response_url POST returned %s: %s");
            json_t *ar = json_array(); json_append(ar, json_number(status)); json_append(ar, json_string(bt)); json_set(ww, "args", ar);
            free(bt); json_free(resp);
            json_t *lr = sa13_call(ctx, "logger.warning", ww);
            if (sa13_is_raise(lr) && sa13_raise_passes(lr)) { json_free(chunks); return lr; } json_free(lr);
            json_free(chunks); return sa13_sr(false, msg, NULL);
        }
        json_free(resp);
    }
    json_free(chunks); return sa13_sr(true, NULL, NULL);
}

/* PoP: sa13_send_slash_reply @ plugins/platforms/slack/adapter.py:_send_slash_reply */
static json_t *sa13_send_slash_reply(sa13_ctx_t *ctx, const json_t *args) {
    json_t *ew = sa13_o(); sa13_sc(ew, "ctx", json_obj_get(args, "slash_ctx")); sa13_sc(ew, "content", json_obj_get(args, "content"));
    json_t *er = sa13_call(ctx, "send_slash_ephemeral", ew);
    if (sa13_is_raise(er) && sa13_raise_passes(er)) return er;
    if (sa13_truthy(json_obj_get(er, "success"))) {
        json_t *cw = sa13_o(); sa13_sc(cw, "chat_id", json_obj_get(args, "chat_id")); sa13_sc(cw, "metadata", json_obj_get(args, "metadata"));
        json_t *cr = sa13_call(ctx, "clear_thread_status_quietly", cw);
        if (sa13_is_raise(cr) && sa13_raise_passes(cr)) { json_free(er); return cr; } json_free(cr);
        return er;
    }
    json_t *lw = sa13_o(); sa13_ss(lw, "label", "[Slack] response_url slash reply failed (%s); retrying via chat.postEphemeral");
    sa13_sc(lw, "arg", json_obj_get(er, "error"));
    json_t *lr = sa13_call(ctx, "logger.warning", lw);
    if (sa13_is_raise(lr) && sa13_raise_passes(lr)) { json_free(er); return lr; } json_free(lr);
    json_t *fw = sa13_o(); sa13_sc(fw, "chat_id", json_obj_get(args, "chat_id"));
    sa13_sc(fw, "slash_ctx", json_obj_get(args, "slash_ctx")); sa13_sc(fw, "content", json_obj_get(args, "content"));
    json_t *fr = sa13_call(ctx, "post_ephemeral_fallback", fw);
    if (sa13_is_raise(fr) && sa13_raise_passes(fr)) { json_free(er); return fr; }
    if (sa13_truthy(json_obj_get(fr, "success"))) {
        json_t *cw = sa13_o(); sa13_sc(cw, "chat_id", json_obj_get(args, "chat_id")); sa13_sc(cw, "metadata", json_obj_get(args, "metadata"));
        json_t *cr = sa13_call(ctx, "clear_thread_status_quietly", cw);
        if (sa13_is_raise(cr) && sa13_raise_passes(cr)) { json_free(er); json_free(fr); return cr; } json_free(cr);
        json_free(er); return fr;
    }
    json_t *ew2 = sa13_o();
    sa13_ss(ew2, "label", "[Slack] Ephemeral slash reply failed on both response_url and chat.postEphemeral (%s); dropping rather than posting publicly");
    sa13_sc(ew2, "arg", json_obj_get(fr, "error"));
    json_t *er2 = sa13_call(ctx, "logger.error", ew2);
    if (sa13_is_raise(er2) && sa13_raise_passes(er2)) { json_free(er); json_free(fr); return er2; } json_free(er2);
    json_free(er); return fr;
}

/* PoP: sa13_set_thread_status @ plugins/platforms/slack/adapter.py:_set_thread_status */
static json_t *sa13_set_thread_status(sa13_ctx_t *ctx, const json_t *args) {
    json_t *cw = sa13_o(); sa13_sc(cw, "chat_id", json_obj_get(args, "chat_id")); sa13_sc(cw, "team_id", json_obj_get(args, "team_id"));
    json_t *client = sa13_call(ctx, "get_client", cw);
    if (sa13_is_raise(client)) {
        if (sa13_raise_passes(client)) return client;
        json_t *dw = sa13_o(); sa13_ss(dw, "label", "[Slack] assistant.threads.setStatus %s: %s");
        json_t *ar = json_array(); json_append(ar, json_copy(json_obj_get(args, "fail_label")));
        json_append(ar, json_copy(json_obj_get(client, "message"))); json_set(dw, "args", ar); json_free(client);
        json_t *lr = sa13_call(ctx, "logger.debug", dw);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) return lr; json_free(lr);
        return json_null();
    }
    json_t *mw = sa13_o(); sa13_sc(mw, "client", client); json_free(client);
    json_t *setter = sa13_call(ctx, "session_status_method", mw);
    if (sa13_is_raise(setter)) {
        if (sa13_raise_passes(setter)) return setter;
        json_t *dw = sa13_o(); sa13_ss(dw, "label", "[Slack] assistant.threads.setStatus %s: %s");
        json_t *ar = json_array(); json_append(ar, json_copy(json_obj_get(args, "fail_label")));
        json_append(ar, json_copy(json_obj_get(setter, "message"))); json_set(dw, "args", ar); json_free(setter);
        json_t *lr = sa13_call(ctx, "logger.debug", dw);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) return lr; json_free(lr);
        return json_null();
    }
    json_t *sw = sa13_o(); sa13_sc(sw, "setter", setter); json_free(setter);
    sa13_sc(sw, "channel_id", json_obj_get(args, "chat_id")); sa13_sc(sw, "thread_ts", json_obj_get(args, "thread_ts"));
    sa13_sc(sw, "status", json_obj_get(args, "status"));
    json_t *r = sa13_call(ctx, "set_status", sw);
    if (sa13_is_raise(r)) {
        if (sa13_raise_passes(r)) return r;
        json_t *dw = sa13_o(); sa13_ss(dw, "label", "[Slack] assistant.threads.setStatus %s: %s");
        json_t *ar = json_array(); json_append(ar, json_copy(json_obj_get(args, "fail_label")));
        json_append(ar, json_copy(json_obj_get(r, "message"))); json_set(dw, "args", ar); json_free(r);
        json_t *lr = sa13_call(ctx, "logger.debug", dw);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) return lr; json_free(lr);
    } else json_free(r);
    return json_null();
}

/* PoP: sa13_set_thread_watermark @ plugins/platforms/slack/adapter.py:_set_thread_watermark */
static json_t *sa13_set_thread_watermark(sa13_ctx_t *ctx, const json_t *args) {
    if (!sa13_truthy(json_obj_get(args, "watermark_ts"))) return json_null();
    json_t *tw = sa13_o(); sa13_ss(tw, "method", "set_session_metadata");
    sa13_sc(tw, "channel_id", json_obj_get(args, "chat_id")); sa13_sc(tw, "thread_ts", json_obj_get(args, "thread_ts"));
    sa13_sc(tw, "user_id", json_obj_get(args, "user_id")); sa13_sc(tw, "team_id", json_obj_get(args, "team_id"));
    json_t *ar = json_array(); json_append(ar, json_copy(json_obj_get(args, "watermark_ts"))); json_set(tw, "args", ar);
    json_t *r = sa13_call(ctx, "to_thread", tw);
    if (sa13_is_raise(r)) {
        if (sa13_raise_passes(r)) return r;
        json_free(r);
        json_t *dw = sa13_o(); sa13_ss(dw, "label", "[Slack] Failed to persist thread watermark"); sa13_sb(dw, "exc_info", true);
        json_t *lr = sa13_call(ctx, "logger.debug", dw);
        if (sa13_is_raise(lr) && sa13_raise_passes(lr)) return lr; json_free(lr);
    } else json_free(r);
    return json_null();
}

