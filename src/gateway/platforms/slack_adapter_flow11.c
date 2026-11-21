/*
 * slack_adapter_flow11.c -- FLOW cores, wave 11, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's still-unclaimed remainder after flow1-10. Every ported name was
 * UNCLAIMED by a strict PoP grep of the whole main-only repo at port time
 * (each carries its PoP annotation at its definition below). No flow1-10
 * function is re-ported.
 *
 * Ported (16 functions):
 *   _humanize_user_mentions, _hydrate_thread_context,
 *   _is_interactive_user_authorized, _lookup_assistant_thread_metadata,
 *   _mark_thread_rehydration_checked, _maybe_blocks, _new_web_client,
 *   _normalize_slack_text_for_dedupe, _note_attachment_failure,
 *   _on_socket_mode_task_done, _on_socket_watchdog_done, _peer_bot_drop,
 *   _post_chunks, _post_ephemeral_fallback, _register_mentioned_thread,
 *   _track_reacting_message.
 *
 * Decision sequences only: every injected/unported callable enters through ONE
 * universal seam sa11_call(ctx, name, args). The harness wires it to a recording
 * stub fed in.io; the oracle installs recorders emitting the same
 * {"name","args","ret"} event shape from the SAME scripts. Ordered trace,
 * BaseException passthrough, fail-closed. Self-contained: libjson only.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow11.py):
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
typedef struct sa11_ctx {
    json_t *state;      /* owned; echoed as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa11_ctx_t;

/* -- basic helpers -- */
static bool sa11_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sa11_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}
static json_t *sa11_obj_attrs(const json_t *v) {
    return sa11_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}
static bool sa11_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}
static bool sa11_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}
static bool sa11_collection_has_str(const json_t *coll, const char *s) {
    if (!s || !coll) return false;
    if (sa11_is_set(coll)) coll = json_obj_get(coll, "__set__");
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
static bool sa11_plain_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa11_is_obj(v) &&
           !sa11_is_set(v) && !sa11_is_tuple_marker(v) && !sa11_is_raise(v);
}
static bool sa11_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa11_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}
static const char *sa11_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static char *sa11_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static json_t *sa11_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *sa11_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = sa11_err(type, msg);
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
static json_t *sa11_call(sa11_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa11_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa11_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa11_make_exc(raise);
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
typedef struct sa11_exc_parent { const char *name, *parent1, *parent2; } sa11_exc_parent_t;
static const sa11_exc_parent_t SA11_EXC_TABLE[] = {
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

static const sa11_exc_parent_t *sa11_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA11_EXC_TABLE_LEN; i++)
        if (strcmp(SA11_EXC_TABLE[i].name, name) == 0) return &SA11_EXC_TABLE[i];
    return NULL;
}
typedef struct sa11_chain { char names[64][96]; size_t n; } sa11_chain_t;
static bool sa11_chain_has(const sa11_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}
static void sa11_chain_add(sa11_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa11_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}
static void sa11_chain_walk(sa11_chain_t *c, const char *name) {
    if (!name || !name[0] || sa11_chain_has(c, name)) return;
    sa11_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const sa11_exc_parent_t *be = sa11_builtin_entry(name);
    const char *p1 = be ? be->parent1 : "Exception";
    const char *p2 = be ? be->parent2 : NULL;
    if (p1 && strcmp(p1, name) != 0) sa11_chain_walk(c, p1);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa11_chain_walk(c, p2);
}
static bool sa11_exc_family(const json_t *r, const char *family) {
    if (!sa11_is_raise(r)) return false;
    sa11_chain_t c; c.n = 0;
    sa11_chain_walk(&c, json_get_str(r, "__raise__", "Exception"));
    return sa11_chain_has(&c, family);
}
static bool sa11_raise_passes(const json_t *r) {
    return sa11_is_raise(r) && !sa11_exc_family(r, "Exception");
}

/* -- ASCII whitespace + string kernels -- */
static bool sa11_is_ws(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *sa11_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa11_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa11_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa11_lstrip_char(const char *s, char ch) {
    if (!s) return NULL;
    while (*s == ch) s++;
    return sa11_strdup(s);
}
static char *sa11_strip_chars(const char *s, const char *chars) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && strchr(chars, s[a]) != NULL) a++;
    while (b > a && strchr(chars, s[b - 1]) != NULL) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa11_lower(const char *s) {
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
static char *sa11_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa11_strdup("None");
    if (v->type == JSON_STRING) return sa11_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return sa11_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return sa11_strdup(buf);
    }
    return sa11_strdup("");
}
static char *sa11_replace(const char *s, const char *from, const char *to) {
    if (!s) return NULL;
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return sa11_strdup(s);
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
static bool sa11_startswith(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool sa11_is_num(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}
static double sa11_num(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* -- local helpers -- */
static json_t *sa11_o(void) { return json_object(); }
static void sa11_ss(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
static void sa11_sc(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, json_copy(v ? v : json_null()));
}
static void sa11_sb(json_t *o, const char *k, bool b) { json_set(o, k, json_bool(b)); }
static void sa11_sn(json_t *o, const char *k, double n) { json_set(o, k, json_number(n)); }
static const char *sa11_s(const json_t *v, const char *def) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static long long sa11_to_int(const json_t *v, long long def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
}
static long long sa11_gi(const json_t *o, const char *k, long long def) {
    return sa11_to_int(o ? json_obj_get(o, k) : NULL, def);
}
static bool sa11_gb(const json_t *o, const char *k, bool def) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    return v && v->type == JSON_BOOL ? v->bool_val : def;
}
static json_t *sa11_get(const json_t *obj, const char *key, const json_t *def) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return json_copy(v ? v : def);
}
static json_t *sa11_arr(const json_t *a, size_t i) {
    if (!a || a->type != JSON_ARRAY || i >= json_len(a)) return json_null();
    return json_copy(json_get(a, i));
}
static bool sa11_json_eq(const json_t *a, const char *s) {
    const char *v = sa11_str(a);
    return v && strcmp(v, s) == 0;
}
/* str(arg or "") via pystr. */
static char *sa11_strval(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa11_strdup("");
    return sa11_pystr(v);
}

/* -- mutable self-dict state helpers (echoed as out.state) -- */
static json_t *sa11_sget(sa11_ctx_t *ctx, const char *name, const char *key, const json_t *def) {
    json_t *d = json_obj_get(ctx->state, name);
    json_t *v = d && d->type == JSON_OBJECT ? json_obj_get(d, key) : NULL;
    return json_copy(v ? v : def);
}
static bool sa11_shas(sa11_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    return d && d->type == JSON_OBJECT && json_obj_get(d, key) != NULL;
}
static json_t *sa11_spop(sa11_ctx_t *ctx, const char *name, const char *key, const json_t *def) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_OBJECT) { d = json_object(); json_set(ctx->state, name, d); }
    json_t *v = json_obj_get(d, key);
    json_t *r = json_copy(v ? v : def);
    if (v) json_obj_del(d, key);
    return r;
}
static void sa11_sset(sa11_ctx_t *ctx, const char *name, const char *key, const json_t *v) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_OBJECT) { d = json_object(); json_set(ctx->state, name, d); }
    json_set(d, key, json_copy(v ? v : json_null()));
}
static void sa11_sdel(sa11_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    if (d && d->type == JSON_OBJECT) json_obj_del(d, key);
}

/* Exception-family raise inside a try -> logger.warn/debug/error + return "". */
static json_t *sa11_try_log(sa11_ctx_t *ctx, json_t *r, const char *label, const char *seam) {
    json_t *w = sa11_o(); sa11_ss(w, "label", label);
    sa11_sc(w, "error", json_obj_get(r, "message"));
    json_free(r);
    json_t *lr = sa11_call(ctx, seam, w);
    if (sa11_is_raise(lr)) return lr;
    json_free(lr);
    return json_string("");
}

/* t() -> identity of the key (deterministic stand-in, both ports). */
static json_t *sa11_t(sa11_ctx_t *ctx, const char *key, const json_t *args) {
    (void)ctx; (void)args;
    return json_string(key ? key : "");
}

/* ====================================================================== */

/* -- state-set helpers (deduped arrays; echo in out.state) -- */
static bool sa11_set_has_str(sa11_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_ARRAY || !key) return false;
    for (size_t i = 0; i < json_len(d); i++) {
        const char *v = sa11_str(json_get(d, i));
        if (v && strcmp(v, key) == 0) return true;
    }
    return false;
}
static void sa11_set_add(sa11_ctx_t *ctx, const char *name, const json_t *val) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_ARRAY) {
        if (d) json_obj_del(ctx->state, name);
        d = json_array(); json_set(ctx->state, name, d);
    }
    const char *ks = sa11_str(val);
    if (ks && sa11_set_has_str(ctx, name, ks)) return;
    json_append(d, json_copy(val ? val : json_null()));
}
static json_t *sa11_copy_or_null(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_null();
    return json_copy(v);
}
static bool sa11_env_yes(const char *s) {
    return s && (strcmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcmp(s, "yes") == 0);
}

/* PoP: sa11_new_web_client @ plugins/platforms/slack/adapter.py:_new_web_client */
static json_t *sa11_new_web_client(sa11_ctx_t *ctx, const json_t *args) {
    json_t *aa = sa11_o(); sa11_ss(aa, "label", "HermesAgent");
    json_t *ua = sa11_call(ctx, "hermes_user_agent", aa);
    if (sa11_is_raise(ua)) return ua;
    json_t *ca = sa11_o();
    json_set(ca, "token", json_copy(json_obj_get(args, "token")));
    json_set(ca, "user_agent_prefix", json_copy(ua));
    json_t *client = sa11_call(ctx, "async_web_client", ca);
    if (sa11_is_raise(client)) { json_free(ua); return client; }
    json_t *pa = sa11_o();
    json_set(pa, "proxy_url", sa11_copy_or_null(json_obj_get(args, "proxy_url")));
    json_set(pa, "client", json_copy(client));
    json_t *pr = sa11_call(ctx, "apply_slack_proxy", pa);
    if (sa11_is_raise(pr)) { json_free(ua); json_free(client); return pr; }
    json_free(pr); json_free(ua);
    return client;
}

/* PoP: sa11_note_attachment_failure @ plugins/platforms/slack/adapter.py:_note_attachment_failure */
static json_t *sa11_note_attachment_failure(sa11_ctx_t *ctx, const json_t *args) {
    const json_t *detail = json_obj_get(args, "detail");
    json_t *la = sa11_o();
    if (sa11_truthy(detail)) {
        sa11_set_add(ctx, "notices", detail);
        sa11_ss(la, "msg", "[Slack] %s");
        json_t *arr = json_array(); json_append(arr, json_copy(detail));
        json_set(la, "args", arr);
        sa11_sb(la, "exc_info", false);
    } else {
        sa11_sc(la, "msg", json_obj_get(args, "fallback_msg"));
        json_t *fa = json_obj_get(args, "fallback_args");
        json_set(la, "args", fa && fa->type == JSON_ARRAY ? json_copy(fa) : json_array());
        const json_t *ei = json_obj_get(args, "exc_info");
        sa11_sb(la, "exc_info", ei && ei->type == JSON_BOOL && ei->bool_val);
    }
    json_t *r = sa11_call(ctx, "logger.warning", la);
    if (sa11_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: sa11_mark_thread_rehydration_checked @ plugins/platforms/slack/adapter.py:_mark_thread_rehydration_checked */
static json_t *sa11_mark_thread_rehydration_checked(sa11_ctx_t *ctx, const json_t *args) {
    json_t *ka = sa11_o();
    sa11_sc(ka, "channel_id", json_obj_get(args, "channel_id"));
    sa11_sc(ka, "thread_ts", json_obj_get(args, "thread_ts"));
    sa11_sc(ka, "user_id", json_obj_get(args, "user_id"));
    sa11_sc(ka, "team_id", json_obj_get(args, "team_id"));
    json_t *key = sa11_call(ctx, "thread_rehydration_key", ka);
    if (sa11_is_raise(key)) return key;
    sa11_set_add(ctx, "_thread_rehydration_checked", key);
    json_t *ea = sa11_o();
    json_t *d = json_obj_get(ctx->state, "_thread_rehydration_checked");
    json_set(ea, "items", json_copy(d ? d : json_array()));
    sa11_sn(ea, "max_size", (double)sa11_gi(args, "max_size", 100));
    json_t *r = sa11_call(ctx, "evict_oldest_by_ts", ea);
    if (sa11_is_raise(r)) { json_free(key); return r; }
    json_free(r); json_free(key);
    return json_null();
}

/* PoP: sa11_register_mentioned_thread @ plugins/platforms/slack/adapter.py:_register_mentioned_thread */
static json_t *sa11_register_mentioned_thread(sa11_ctx_t *ctx, const json_t *args) {
    const json_t *tts = json_obj_get(args, "thread_ts");
    if (!sa11_truthy(tts)) return json_null();
    json_t *ma = sa11_o();
    sa11_sc(ma, "team_id", json_obj_get(args, "team_id"));
    json_set(ma, "ts", json_copy(tts));
    json_t *marker = sa11_call(ctx, "workspace_message_marker", ma);
    if (sa11_is_raise(marker)) return marker;
    sa11_set_add(ctx, "_mentioned_threads", marker);
    json_t *r = sa11_call(ctx, "trim_mentioned_threads", sa11_o());
    if (sa11_is_raise(r)) { json_free(marker); return r; }
    json_free(r); json_free(marker);
    return json_null();
}

/* PoP: sa11_track_reacting_message @ plugins/platforms/slack/adapter.py:_track_reacting_message */
static json_t *sa11_track_reacting_message(sa11_ctx_t *ctx, const json_t *args) {
    json_t *ma = sa11_o();
    sa11_sc(ma, "team_id", json_obj_get(args, "team_id"));
    json_set(ma, "ts", json_copy(json_obj_get(args, "ts")));
    json_t *marker = sa11_call(ctx, "workspace_message_marker", ma);
    if (sa11_is_raise(marker)) return marker;
    sa11_set_add(ctx, "_reacting_message_ids", marker);
    json_t *ea = sa11_o();
    json_t *d = json_obj_get(ctx->state, "_reacting_message_ids");
    json_set(ea, "items", json_copy(d ? d : json_array()));
    sa11_sn(ea, "max_size", (double)sa11_gi(args, "max_size", 100));
    json_t *r = sa11_call(ctx, "evict_oldest_by_ts", ea);
    if (sa11_is_raise(r)) { json_free(marker); return r; }
    json_free(r); json_free(marker);
    return json_null();
}

/* PoP: sa11_on_socket_watchdog_done @ plugins/platforms/slack/adapter.py:_on_socket_watchdog_done */
static json_t *sa11_on_socket_watchdog_done(sa11_ctx_t *ctx, const json_t *args) {
    const json_t *task = json_obj_get(args, "task");
    const char *tid = sa11_str(json_obj_get(task, "id"));
    const json_t *wd = json_obj_get(ctx->state, "_socket_watchdog_task");
    const char *wid = sa11_str(wd);
    if (!tid || !wid || strcmp(tid, wid) != 0) return json_null();
    json_t *ca = sa11_o(); sa11_ss(ca, "task_id", tid);
    json_t *cancelled = sa11_call(ctx, "task_cancelled", ca);
    if (sa11_is_raise(cancelled)) return cancelled;
    const json_t *running = json_obj_get(ctx->state, "_running");
    bool run = running && running->type == JSON_BOOL && running->bool_val;
    if (sa11_truthy(cancelled) || !run) { json_free(cancelled); return json_null(); }
    json_free(cancelled);
    json_t *xa = sa11_o(); sa11_ss(xa, "task_id", tid);
    json_t *exc = sa11_call(ctx, "task_exception", xa);
    if (sa11_is_raise(exc)) return exc;
    json_t *la = sa11_o();
    const char *em = sa11_str(exc);
    if (em) {
        sa11_ss(la, "msg", "[Slack] Socket Mode watchdog exited with error; restarting: %s");
        json_t *ar = json_array(); json_append(ar, json_copy(exc)); json_set(la, "args", ar);
        sa11_sb(la, "exc_info", true);
    } else {
        sa11_ss(la, "msg", "[Slack] Socket Mode watchdog exited; restarting");
        json_set(la, "args", json_array());
        sa11_sb(la, "exc_info", false);
    }
    json_t *lr = sa11_call(ctx, "logger.warning", la);
    if (sa11_is_raise(lr)) { json_free(exc); return lr; }
    json_free(lr); json_free(exc);
    json_set(ctx->state, "_socket_watchdog_task", json_null());
    json_t *er = sa11_call(ctx, "ensure_socket_watchdog", sa11_o());
    if (sa11_is_raise(er)) return er;
    json_free(er);
    return json_null();
}

/* PoP: sa11_on_socket_mode_task_done @ plugins/platforms/slack/adapter.py:_on_socket_mode_task_done */
static json_t *sa11_on_socket_mode_task_done(sa11_ctx_t *ctx, const json_t *args) {
    const json_t *task = json_obj_get(args, "task");
    const char *tid = sa11_str(json_obj_get(task, "id"));
    const json_t *mt = json_obj_get(ctx->state, "_socket_mode_task");
    const char *mid = sa11_str(mt);
    const json_t *running = json_obj_get(ctx->state, "_running");
    bool run = running && running->type == JSON_BOOL && running->bool_val;
    if (!tid || !mid || strcmp(tid, mid) != 0 || !run) return json_null();
    json_t *ca = sa11_o(); sa11_ss(ca, "task_id", tid);
    json_t *cancelled = sa11_call(ctx, "task_cancelled", ca);
    if (sa11_is_raise(cancelled)) return cancelled;
    if (sa11_truthy(cancelled)) { json_free(cancelled); return json_null(); }
    json_free(cancelled);
    json_t *xa = sa11_o(); sa11_ss(xa, "task_id", tid);
    json_t *exc = sa11_call(ctx, "task_exception", xa);
    const char *em = NULL;
    if (sa11_is_raise(exc)) {
        if (sa11_exc_family(exc, "CancelledError")) { json_free(exc); return json_null(); }
        if (sa11_raise_passes(exc)) return exc;
        json_t *dr = sa11_call(ctx, "logger.debug", sa11_o());
        if (sa11_is_raise(dr)) return dr;
        json_free(dr); json_free(exc); exc = NULL;
    } else em = sa11_str(exc);
    json_t *la = sa11_o();
    if (em) {
        sa11_ss(la, "msg", "[Slack] Socket Mode task exited with error: %s");
        json_t *ar = json_array(); json_append(ar, json_string(em)); json_set(la, "args", ar);
        sa11_sb(la, "exc_info", true);
    } else {
        sa11_ss(la, "msg", "[Slack] Socket Mode task exited unexpectedly");
        json_set(la, "args", json_array());
        sa11_sb(la, "exc_info", false);
    }
    json_t *lr = sa11_call(ctx, "logger.warning", la);
    if (sa11_is_raise(lr)) { if (exc) json_free(exc); return lr; }
    json_free(lr); if (exc) json_free(exc);
    json_t *loop = sa11_call(ctx, "get_running_loop", sa11_o());
    if (sa11_is_raise(loop)) {
        if (sa11_exc_family(loop, "RuntimeError")) { json_free(loop); return json_null(); }
        return loop;
    }
    json_t *rr = sa11_call(ctx, "restart_socket_mode", sa11_o());
    if (sa11_is_raise(rr)) { json_free(loop); return rr; }
    json_free(rr);
    json_t *ta = sa11_o(); sa11_ss(ta, "target", "restart_socket_mode");
    json_t *cr = sa11_call(ctx, "create_task", ta);
    if (sa11_is_raise(cr)) { json_free(loop); return cr; }
    json_free(cr); json_free(loop);
    return json_null();
}

/* PoP: sa11_peer_bot_drop @ plugins/platforms/slack/adapter.py:_peer_bot_drop */
static json_t *sa11_peer_bot_drop(sa11_ctx_t *ctx, const json_t *args) {
    const char *uid = sa11_str(json_obj_get(args, "user_id"));
    const char *bot = sa11_str(json_obj_get(args, "bot_uid"));
    if (!sa11_truthy(json_obj_get(args, "user_id")) || (uid && bot && strcmp(uid, bot) == 0))
        return json_bool(false);
    json_t *ea = sa11_o(); json_set(ea, "event", json_copy(json_obj_get(args, "event")));
    json_t *d = sa11_call(ctx, "event_declares_bot_sender", ea);
    if (sa11_is_raise(d)) return d;
    bool is_bot = sa11_truthy(d); json_free(d);
    if (!is_bot) {
        json_t *ra = sa11_o();
        sa11_ss(ra, "user_id", uid ? uid : "");
        sa11_sc(ra, "chat_id", json_obj_get(args, "channel_id"));
        sa11_sc(ra, "team_id", json_obj_get(args, "team_id"));
        json_t *rb = sa11_call(ctx, "resolve_user_is_bot", ra);
        if (sa11_is_raise(rb)) return rb;
        is_bot = sa11_truthy(rb); json_free(rb);
    }
    if (!is_bot) return json_bool(false);
    json_t *ab = sa11_call(ctx, "slack_allow_bots", sa11_o());
    if (sa11_is_raise(ab)) return ab;
    const char *allow = sa11_str(ab);
    bool mentioned = sa11_gb(args, "is_mentioned", false);
    bool drop = (allow && strcmp(allow, "none") == 0) ||
                (allow && strcmp(allow, "mentions") == 0 && !mentioned);
    json_free(ab);
    return json_bool(drop);
}

/* PoP: sa11_maybe_blocks @ plugins/platforms/slack/adapter.py:_maybe_blocks */
static json_t *sa11_maybe_blocks(sa11_ctx_t *ctx, const json_t *args) {
    json_t *fa = sa11_o(); sa11_ss(fa, "key", "markdown_blocks");
    json_t *on = sa11_call(ctx, "extra_flag", fa);
    if (sa11_is_raise(on)) return on;
    if (sa11_truthy(on)) {
        json_free(on);
        json_t *ma = sa11_o(); json_set(ma, "content", json_copy(json_obj_get(args, "content")));
        json_t *md = sa11_call(ctx, "markdown_block_payload", ma);
        if (sa11_is_raise(md)) return md;
        if (sa11_truthy(md)) {
            json_t *aa = sa11_o(); json_set(aa, "blocks", json_copy(md)); json_free(md);
            json_t *fb = sa11_call(ctx, "append_feedback_block", aa);
            if (sa11_is_raise(fb)) return fb;
            json_t *sa = sa11_o(); json_set(sa, "blocks", json_copy(fb)); json_free(fb);
            json_t *out = sa11_call(ctx, "sanitize_blocks", sa);
            return out;
        }
        json_free(md);
    } else json_free(on);
    json_t *fa2 = sa11_o(); sa11_ss(fa2, "key", "rich_blocks");
    json_t *rich = sa11_call(ctx, "extra_flag", fa2);
    if (sa11_is_raise(rich)) return rich;
    if (!sa11_truthy(rich)) { json_free(rich); return json_null(); }
    json_free(rich);
    json_t *ra = sa11_o();
    json_set(ra, "content", json_copy(json_obj_get(args, "content")));
    sa11_ss(ra, "mrkdwn_fn", "format_message");
    json_t *blocks = sa11_call(ctx, "render_blocks", ra);
    if (sa11_is_raise(blocks)) {
        if (sa11_raise_passes(blocks)) return blocks;
        json_free(blocks);
        json_t *dr = sa11_call(ctx, "logger.debug", sa11_o());
        if (sa11_is_raise(dr)) return dr;
        json_free(dr);
        return json_null();
    }
    json_t *aa = sa11_o(); json_set(aa, "blocks", json_copy(blocks)); json_free(blocks);
    json_t *fb = sa11_call(ctx, "append_feedback_block", aa);
    if (sa11_is_raise(fb)) return fb;
    json_t *sa = sa11_o(); json_set(sa, "blocks", json_copy(fb)); json_free(fb);
    return sa11_call(ctx, "sanitize_blocks", sa);
}

/* PoP: sa11_post_ephemeral_fallback @ plugins/platforms/slack/adapter.py:_post_ephemeral_fallback */
static json_t *sa11_sr(bool success, const json_t *err) {
    json_t *r = sa11_o();
    sa11_sb(r, "success", success);
    json_set(r, "error", json_copy(err ? err : json_null()));
    json_set(r, "message_id", json_null());
    return r;
}
static json_t *sa11_post_ephemeral_fallback(sa11_ctx_t *ctx, const json_t *args) {
    const json_t *cx = json_obj_get(args, "ctx");
    const char *uid = sa11_str(json_obj_get(cx, "user_id"));
    if (!sa11_truthy(json_obj_get(cx, "user_id")) || !uid || !uid[0])
        return sa11_sr(false, json_string("no user_id in slash context for postEphemeral"));
    json_t *ca = sa11_o(); json_set(ca, "content", json_copy(json_obj_get(args, "content")));
    json_t *chunks = sa11_call(ctx, "format_chunks", ca);
    if (sa11_is_raise(chunks)) return chunks;
    json_t *ga = sa11_o(); sa11_sc(ga, "chat_id", json_obj_get(args, "chat_id"));
    json_t *client = sa11_call(ctx, "get_client", ga);
    if (sa11_is_raise(client)) { json_free(chunks); return client; }
    size_t n = (chunks && chunks->type == JSON_ARRAY) ? json_len(chunks) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *pa = sa11_o();
        json_set(pa, "client", json_copy(client));
        sa11_sc(pa, "channel", json_obj_get(args, "chat_id"));
        sa11_ss(pa, "user", uid);
        json_set(pa, "text", json_copy(json_get(chunks, i)));
        json_t *res = sa11_call(ctx, "chat_postEphemeral", pa);
        if (sa11_is_raise(res)) { if (sa11_raise_passes(res)) { json_free(client); json_free(chunks); return res; }
            json_t *em = json_string(sa11_str(json_obj_get(res, "message")) ? sa11_str(json_obj_get(res, "message")) : "");
            json_t *out = sa11_sr(false, em); json_free(em); json_free(res); json_free(client); json_free(chunks); return out; }
        json_t *sa = sa11_o(); json_set(sa, "response", json_copy(res)); json_free(res);
        json_t *payload = sa11_call(ctx, "slack_response_payload", sa);
        if (sa11_is_raise(payload)) { if (sa11_raise_passes(payload)) { json_free(client); json_free(chunks); return payload; }
            json_t *em2 = json_string(sa11_str(json_obj_get(payload, "message")) ? sa11_str(json_obj_get(payload, "message")) : "");
            json_t *out = sa11_sr(false, em2); json_free(em2); json_free(payload); json_free(client); json_free(chunks); return out; }
        json_t *okj = json_obj_get(payload, "ok");
        bool ok = sa11_truthy(okj);
        if (!ok) {
            json_t *errj = NULL;
            if (sa11_truthy(payload)) {
                const char *e = sa11_str(json_obj_get(payload, "error"));
                char buf[256]; snprintf(buf, sizeof buf, "chat.postEphemeral failed: %s", e ? e : "unknown_error");
                errj = json_string(buf);
            } else errj = json_string("chat.postEphemeral failed: unexpected_response");
            json_t *out = sa11_sr(false, errj); json_free(errj);
            json_free(payload); json_free(client); json_free(chunks);
            return out;
        }
        json_free(payload);
    }
    json_free(client); json_free(chunks);
    return sa11_sr(true, NULL);
}

/* PoP: sa11_post_chunks @ plugins/platforms/slack/adapter.py:_post_chunks */
static json_t *sa11_post_chunks(sa11_ctx_t *ctx, const json_t *args) {
    json_t *ta = sa11_o(); json_set(ta, "text", json_copy(json_obj_get(args, "formatted")));
    sa11_sn(ta, "max", (double)sa11_gi(args, "max_length", 3900));
    json_t *chunks = sa11_call(ctx, "truncate_message", ta);
    if (sa11_is_raise(chunks)) return chunks;
    const json_t *extra = json_obj_get(args, "extra");
    bool broadcast = sa11_gb(extra, "reply_broadcast", false);
    json_t *blocks = NULL;
    size_t n = (chunks && chunks->type == JSON_ARRAY) ? json_len(chunks) : 0;
    if (n == 1) {
        json_t *ba = sa11_o(); json_set(ba, "content", json_copy(json_obj_get(args, "content")));
        blocks = sa11_call(ctx, "maybe_blocks", ba);
        if (sa11_is_raise(blocks)) { json_free(chunks); return blocks; }
    }
    const char *thread = sa11_str(json_obj_get(args, "thread_ts"));
    bool has_thread = sa11_truthy(json_obj_get(args, "thread_ts")) && thread;
    json_t *last = NULL;
    for (size_t i = 0; i < n; i++) {
        json_t *ua = sa11_o(); json_set(ua, "extra", json_copy(extra ? extra : json_null()));
        json_t *unf = sa11_call(ctx, "slack_unfurl_kwargs", ua);
        if (sa11_is_raise(unf)) { if (last) json_free(last); if (blocks) json_free(blocks); json_free(chunks); return unf; }
        json_t *kw = sa11_o();
        sa11_sc(kw, "channel", json_obj_get(args, "chat_id"));
        json_set(kw, "text", json_copy(json_get(chunks, i)));
        sa11_sb(kw, "mrkdwn", true);
        if (unf && unf->type == JSON_OBJECT)
            for (size_t k = 0; k < unf->c.count; k++) {
                const char *kk = unf->c.keys ? unf->c.keys[k] : NULL;
                if (kk) json_set(kw, kk, json_copy(unf->c.items[k]));
            }
        json_free(unf);
        if (sa11_truthy(blocks) && i == 0) json_set(kw, "blocks", json_copy(blocks));
        if (has_thread) {
            sa11_ss(kw, "thread_ts", thread);
            if (broadcast && i == 0) sa11_sb(kw, "reply_broadcast", true);
        }
        json_t *ca = sa11_o();
        sa11_ss(ca, "client_fn", "client_fn");
        sa11_ss(ca, "method", "chat_postMessage");
        json_set(ca, "kwargs", json_copy(kw)); json_free(kw);
        sa11_ss(ca, "purpose", "send");
        if (last) json_free(last);
        last = sa11_call(ctx, "call_with_block_fallback", ca);
        if (sa11_is_raise(last)) { if (blocks) json_free(blocks); json_free(chunks); return last; }
    }
    if (blocks) json_free(blocks);
    json_free(chunks);
    return last ? last : json_null();
}

/* PoP: sa11_lookup_assistant_thread_metadata @ plugins/platforms/slack/adapter.py:_lookup_assistant_thread_metadata */
static json_t *sa11_lookup_assistant_thread_metadata(sa11_ctx_t *ctx, const json_t *args) {
    json_t *ea = sa11_o();
    json_set(ea, "event", json_copy(json_obj_get(args, "event")));
    json_set(ea, "body", json_copy(json_obj_get(args, "body")));
    json_t *md = sa11_call(ctx, "extract_assistant_thread_metadata", ea);
    if (sa11_is_raise(md)) return md;
    if (md->type != JSON_OBJECT) { json_t *o = sa11_o(); json_free(md); md = o; }
    const json_t *cid = json_obj_get(args, "channel_id");
    if (sa11_truthy(cid) && !sa11_truthy(json_obj_get(md, "channel_id")))
        json_set(md, "channel_id", json_copy(cid));
    const json_t *tts = json_obj_get(args, "thread_ts");
    if (sa11_truthy(tts) && !sa11_truthy(json_obj_get(md, "thread_ts")))
        json_set(md, "thread_ts", json_copy(tts));
    const json_t *tid = json_obj_get(args, "team_id");
    if (sa11_truthy(tid) && !sa11_truthy(json_obj_get(md, "team_id"))) {
        char *ps = sa11_pystr(tid); sa11_ss(md, "team_id", ps ? ps : ""); free(ps);
    }
    json_t *ka = sa11_o();
    sa11_ss(ka, "team_id", sa11_s(json_obj_get(md, "team_id"), ""));
    sa11_ss(ka, "channel_id", sa11_s(json_obj_get(md, "channel_id"), ""));
    sa11_ss(ka, "thread_ts", sa11_s(json_obj_get(md, "thread_ts"), ""));
    json_t *key = sa11_call(ctx, "workspace_thread_key", ka);
    if (sa11_is_raise(key)) { json_free(md); return key; }
    json_t *store = json_obj_get(ctx->state, "_assistant_threads");
    json_t *cached = NULL;
    if (sa11_truthy(key) && store && store->type == JSON_OBJECT)
        cached = json_obj_get(store, sa11_str(key) ? sa11_str(key) : "");
    if (cached && sa11_truthy(cached)) {
        json_t *out = json_object();
        if (cached->type == JSON_OBJECT)
            for (size_t k = 0; k < cached->c.count; k++) {
                const char *kk = cached->c.keys ? cached->c.keys[k] : NULL;
                if (kk) json_set(out, kk, json_copy(cached->c.items[k]));
            }
        for (size_t k = 0; k < md->c.count; k++) {
            const char *kk = md->c.keys ? md->c.keys[k] : NULL;
            if (kk && sa11_truthy(md->c.items[k])) json_set(out, kk, json_copy(md->c.items[k]));
        }
        json_free(key); json_free(md); return out;
    }
    json_free(key);
    return md;
}

/* PoP: sa11_humanize_user_mentions @ plugins/platforms/slack/adapter.py:_humanize_user_mentions */
static json_t *sa11_humanize_user_mentions(sa11_ctx_t *ctx, const json_t *args) {
    json_t *tv = json_obj_get(args, "text");
    const char *text = sa11_str(tv);
    if (!sa11_truthy(tv) || !text || strstr(text, "<@") == NULL)
        return json_copy(tv ? tv : json_null());
    char *work = sa11_strdup(text);
    json_t *fa = sa11_o(); sa11_ss(fa, "text", text);
    json_t *uids = sa11_call(ctx, "find_user_mentions", fa);
    if (sa11_is_raise(uids)) { free(work); return uids; }
    size_t n = (uids && uids->type == JSON_ARRAY) ? json_len(uids) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *uid = sa11_str(json_get(uids, i));
        json_t *ra = sa11_o();
        sa11_ss(ra, "user_id", uid ? uid : "");
        sa11_sc(ra, "chat_id", json_obj_get(args, "chat_id"));
        sa11_sc(ra, "team_id", json_obj_get(args, "team_id"));
        json_t *name = sa11_call(ctx, "resolve_user_name", ra);
        if (sa11_is_raise(name)) { free(work); json_free(uids); return name; }
        const char *ns = sa11_str(name);
        const char *base = sa11_truthy(name) && ns ? ns : (uid ? uid : "");
        char *sname = sa11_strip(base);
        const char *disp = (sname && sname[0]) ? sname : (uid ? uid : "");
        char repl[512]; snprintf(repl, sizeof repl, "@%s", disp);
        json_t *sa = sa11_o(); sa11_ss(sa, "text", work); sa11_ss(sa, "uid", uid ? uid : "");
        sa11_ss(sa, "replacement", repl);
        json_t *nt = sa11_call(ctx, "sub_user_mention", sa);
        if (sa11_is_raise(nt)) { free(sname); free(work); json_free(uids); return nt; }
        const char *nstr = sa11_str(nt);
        free(work); work = sa11_strdup(nstr ? nstr : "");
        free(sname); json_free(nt); json_free(name);
    }
    json_free(uids);
    json_t *out = json_string(work ? work : "");
    free(work);
    return out;
}

/* PoP: sa11_hydrate_thread_context @ plugins/platforms/slack/adapter.py:_hydrate_thread_context */
static json_t *sa11_hydrate_ret(const json_t *cc, const json_t *urls, const json_t *types) {
    json_t *r = json_array();
    json_append(r, json_copy(cc ? cc : json_null()));
    json_append(r, json_copy(urls ? urls : json_array()));
    json_append(r, json_copy(types ? types : json_array()));
    return r;
}
static json_t *sa11_hydrate_fetch(sa11_ctx_t *ctx, const json_t *args, const char *chat_type,
                                  const json_t *after, bool force, json_t **cc_out) {
    json_t *fa = sa11_o();
    sa11_sc(fa, "channel_id", json_obj_get(args, "channel_id"));
    sa11_sc(fa, "thread_ts", json_obj_get(args, "event_thread_ts"));
    sa11_sc(fa, "user_id", json_obj_get(args, "user_id"));
    sa11_sc(fa, "current_ts", json_obj_get(args, "ts"));
    sa11_sc(fa, "team_id", json_obj_get(args, "team_id"));
    sa11_ss(fa, "chat_type", chat_type);
    if (after) json_set(fa, "after_ts", json_copy(after));
    if (force) sa11_sb(fa, "force_refresh", true);
    json_t *r = sa11_call(ctx, "fetch_thread_context", fa);
    if (sa11_is_raise(r)) return r;
    if (sa11_truthy(r)) { if (*cc_out) json_free(*cc_out); *cc_out = r; } else json_free(r);
    return NULL;
}
static json_t *sa11_hydrate_thread_context(sa11_ctx_t *ctx, const json_t *args) {
    json_t *cc = NULL;
    if (!sa11_truthy(json_obj_get(args, "is_thread_reply")))
        return sa11_hydrate_ret(NULL, NULL, NULL);
    bool is_dm = sa11_gb(args, "is_dm", false);
    bool is_ment = sa11_gb(args, "is_mentioned", false);
    const char *chat_type = is_dm ? "dm" : "group";
    json_t *ha = sa11_o();
    sa11_sc(ha, "channel_id", json_obj_get(args, "channel_id"));
    sa11_sc(ha, "thread_ts", json_obj_get(args, "event_thread_ts"));
    sa11_sc(ha, "user_id", json_obj_get(args, "user_id"));
    sa11_sc(ha, "team_id", json_obj_get(args, "team_id"));
    sa11_ss(ha, "chat_type", chat_type);
    json_t *has = sa11_call(ctx, "has_active_session_for_thread", ha);
    if (sa11_is_raise(has)) { if (cc) json_free(cc); return has; }
    bool active = sa11_truthy(has); json_free(has);
    json_t *urls = NULL, *types = NULL;
    if (!active) {
        json_t *e = sa11_hydrate_fetch(ctx, args, chat_type, NULL, false, &cc);
        if (e) { json_free(e); if (cc) json_free(cc); return e; }
        json_t *ia = sa11_o();
        sa11_sc(ia, "channel_id", json_obj_get(args, "channel_id"));
        sa11_sc(ia, "thread_ts", json_obj_get(args, "event_thread_ts"));
        sa11_sc(ia, "team_id", json_obj_get(args, "team_id"));
        json_t *im = sa11_call(ctx, "collect_thread_root_images", ia);
        if (sa11_is_raise(im)) { if (cc) json_free(cc); return im; }
        urls = json_copy(sa11_arr(im, 0));
        types = json_copy(sa11_arr(im, 1));
        json_free(im);
    } else if (is_ment) {
        json_t *wa = sa11_o();
        sa11_sc(wa, "channel_id", json_obj_get(args, "channel_id"));
        sa11_sc(wa, "thread_ts", json_obj_get(args, "event_thread_ts"));
        sa11_sc(wa, "user_id", json_obj_get(args, "user_id"));
        sa11_sc(wa, "team_id", json_obj_get(args, "team_id"));
        json_t *w = sa11_call(ctx, "get_thread_watermark", wa);
        if (sa11_is_raise(w)) { json_free(wa); if (cc) json_free(cc); return w; }
        json_t *e = sa11_hydrate_fetch(ctx, args, chat_type, w, true, &cc);
        json_free(w); json_free(wa);
        if (e) { json_free(e); if (cc) json_free(cc); return e; }
    } else {
        json_t *ka = sa11_o();
        sa11_sc(ka, "channel_id", json_obj_get(args, "channel_id"));
        sa11_sc(ka, "thread_ts", json_obj_get(args, "event_thread_ts"));
        sa11_sc(ka, "user_id", json_obj_get(args, "user_id"));
        sa11_sc(ka, "team_id", json_obj_get(args, "team_id"));
        json_t *rk = sa11_call(ctx, "thread_rehydration_key", ka);
        if (sa11_is_raise(rk)) { json_free(ka); if (cc) json_free(cc); return rk; }
        bool seen = sa11_set_has_str(ctx, "_thread_rehydration_checked", sa11_str(rk));
        json_free(rk); json_free(ka);
        json_t *wa = sa11_o();
        sa11_sc(wa, "channel_id", json_obj_get(args, "channel_id"));
        sa11_sc(wa, "thread_ts", json_obj_get(args, "event_thread_ts"));
        sa11_sc(wa, "user_id", json_obj_get(args, "user_id"));
        sa11_sc(wa, "team_id", json_obj_get(args, "team_id"));
        if (seen) {
            json_t *swa = json_object();
            if (wa->type == JSON_OBJECT) for (size_t k = 0; k < wa->c.count; k++) {
                const char *kk = wa->c.keys ? wa->c.keys[k] : NULL;
                if (kk) json_set(swa, kk, json_copy(wa->c.items[k]));
            }
            json_set(swa, "watermark_ts", json_copy(json_obj_get(args, "ts")));
            json_t *sw = sa11_call(ctx, "set_thread_watermark", swa);
            json_free(wa);
            if (sa11_is_raise(sw)) { if (cc) json_free(cc); return sw; }
            json_free(sw);
            json_t *out = sa11_hydrate_ret(NULL, NULL, NULL);
            if (cc) json_free(cc);
            return out;
        }
        json_t *wts = sa11_call(ctx, "get_thread_watermark", wa);
        if (sa11_is_raise(wts)) { json_free(wa); if (cc) json_free(cc); return wts; }
        if (sa11_truthy(wts)) {
            json_t *e = sa11_hydrate_fetch(ctx, args, chat_type, wts, true, &cc);
            if (e) { json_free(e); json_free(wts); json_free(wa); if (cc) json_free(cc); return e; }
        }
        json_free(wts); json_free(wa);
    }
    json_t *fwa = sa11_o();
    sa11_sc(fwa, "channel_id", json_obj_get(args, "channel_id"));
    sa11_sc(fwa, "thread_ts", json_obj_get(args, "event_thread_ts"));
    sa11_sc(fwa, "user_id", json_obj_get(args, "user_id"));
    sa11_sc(fwa, "team_id", json_obj_get(args, "team_id"));
    json_set(fwa, "watermark_ts", json_copy(json_obj_get(args, "ts")));
    json_t *sw = sa11_call(ctx, "set_thread_watermark", fwa);
    if (sa11_is_raise(sw)) { if (cc) json_free(cc); if (urls) json_free(urls); if (types) json_free(types); return sw; }
    json_free(sw);
    json_t *ma = sa11_o();
    sa11_sc(ma, "channel_id", json_obj_get(args, "channel_id"));
    sa11_sc(ma, "thread_ts", json_obj_get(args, "event_thread_ts"));
    sa11_sc(ma, "user_id", json_obj_get(args, "user_id"));
    sa11_sc(ma, "team_id", json_obj_get(args, "team_id"));
    json_t *mr = sa11_call(ctx, "mark_thread_rehydration_checked", ma);
    if (sa11_is_raise(mr)) { if (cc) json_free(cc); if (urls) json_free(urls); if (types) json_free(types); return mr; }
    json_free(mr);
    json_t *out = sa11_hydrate_ret(cc, urls, types);
    if (cc) json_free(cc); if (urls) json_free(urls); if (types) json_free(types);
    return out;
}

/* PoP: sa11_normalize_slack_text_for_dedupe @ plugins/platforms/slack/adapter.py:_normalize_slack_text_for_dedupe */
static json_t *sa11_sub_seam(sa11_ctx_t *ctx, const char *name, const char *text, char **work) {
    json_t *a = sa11_o(); sa11_ss(a, "text", text ? text : "");
    json_t *r = sa11_call(ctx, name, a);
    if (sa11_is_raise(r)) { free(*work); *work = NULL; return r; }
    const char *s = sa11_str(r);
    free(*work); *work = sa11_strdup(s ? s : "");
    json_free(r);
    return NULL;
}
static char *sa11_collapse_ws(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s); char *out = (char *)malloc(n + 2);
    size_t j = 0; bool inws = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        bool ws = (c == ' ' || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f));
        if (ws) { if (!inws) out[j++] = ' '; inws = true; }
        else { out[j++] = (char)c; inws = false; }
    }
    out[j] = '\0';
    char *res = sa11_strip(out); free(out); return res;
}
static json_t *sa11_normalize_slack_text_for_dedupe(sa11_ctx_t *ctx, const json_t *args) {
    const json_t *tv = json_obj_get(args, "text");
    char *work = sa11_strdup(sa11_str(tv) ? sa11_str(tv) : "");
    json_t *e = NULL;
    #define SA11_SUB(NAME) do { e = sa11_sub_seam(ctx, NAME, work, &work); if (e) return e; } while (0)
    SA11_SUB("unescape_slack_entities");
    SA11_SUB("mrkdwn_link_sub");
    SA11_SUB("date_sub");
    SA11_SUB("permalink_sub");
    SA11_SUB("entity_label_sub");
    const char *bot = sa11_str(json_obj_get(args, "bot_uid"));
    if (bot && bot[0]) {
        char pat[512]; snprintf(pat, sizeof pat, "<@%s>", bot);
        char *nw = sa11_replace(work, pat, ""); free(work); work = nw;
    }
    SA11_SUB("fenced_code_sub");
    SA11_SUB("inline_code_sub");
    SA11_SUB("inline_style_collapse");
    #undef SA11_SUB
    char *final = sa11_collapse_ws(work); free(work);
    json_t *out = json_string(final ? final : "");
    free(final);
    return out;
}

/* PoP: sa11_is_interactive_user_authorized @ plugins/platforms/slack/adapter.py:_is_interactive_user_authorized */
static json_t *sa11_is_interactive_user_authorized(sa11_ctx_t *ctx, const json_t *args) {
    char *nid = sa11_strip(sa11_strval(json_obj_get(args, "user_id")));
    if (!nid || !nid[0]) { free(nid); return json_bool(false); }
    const char *cid = sa11_str(json_obj_get(args, "channel_id"));
    const char *chat_type = (cid && strncmp(cid, "D", 1) == 0) ? "dm" : "group";
    const json_t *marker = json_obj_get(ctx->state, "_authorization_check");
    bool has_marker = marker && marker->type != JSON_NULL;
    if (has_marker) {
        json_t *aa = sa11_o();
        sa11_ss(aa, "user_id", nid); sa11_ss(aa, "chat_type", chat_type);
        sa11_ss(aa, "channel_id", cid ? cid : "");
        json_t *inj = sa11_call(ctx, "is_sender_authorized", aa);
        if (sa11_is_raise(inj)) { free(nid); return inj; }
        if (inj && inj->type != JSON_NULL) { free(nid); return inj; }
        json_free(inj);
    }
    json_t *authfn = sa11_call(ctx, "runner_auth_fn", sa11_o());
    if (sa11_is_raise(authfn)) { free(nid); return authfn; }
    if (sa11_truthy(authfn)) {
        json_t *src = sa11_o();
        sa11_ss(src, "platform", "slack");
        if (cid && cid[0]) sa11_ss(src, "chat_id", cid); else sa11_ss(src, "chat_id", nid);
        sa11_ss(src, "chat_type", chat_type);
        sa11_ss(src, "user_id", nid);
        const json_t *un = json_obj_get(args, "user_name");
        if (sa11_truthy(un)) { char *u = sa11_strip(sa11_strval(un)); sa11_ss(src, "user_name", u ? u : ""); free(u); }
        else json_set(src, "user_name", json_null());
        const json_t *ti = json_obj_get(args, "team_id");
        if (sa11_truthy(ti)) { char *t = sa11_pystr(ti); sa11_ss(src, "scope_id", t ? t : ""); free(t); }
        else json_set(src, "scope_id", json_null());
        json_t *ca = sa11_o(); json_set(ca, "source", json_copy(src)); json_free(src);
        json_t *r = sa11_call(ctx, "auth_fn_call", ca);
        if (sa11_is_raise(r)) {
            if (sa11_raise_passes(r)) { free(nid); json_free(authfn); return r; }
            json_free(r);
            json_t *dr = sa11_call(ctx, "logger.debug", sa11_o());
            if (sa11_is_raise(dr)) { free(nid); json_free(authfn); return dr; }
            json_free(dr);
        } else { bool b = sa11_truthy(r); json_free(r); json_free(authfn); free(nid); return json_bool(b); }
    }
    json_free(authfn);
    json_t *env = json_obj_get(ctx->state, "_gate_env");
    const char *all = sa11_str(json_obj_get(env, "SLACK_ALLOW_ALL_USERS"));
    if (sa11_env_yes(all)) { free(nid); return json_bool(true); }
    const char *VARS[2] = {"SLACK_ALLOWED_USERS", "GATEWAY_ALLOWED_USERS"};
    bool allowed = false;
    for (int v = 0; v < 2; v++) {
        const char *raw = sa11_str(json_obj_get(env, VARS[v]));
        if (!raw || !raw[0]) continue;
        char *cp = sa11_strdup(raw);
        char *tok = strtok(cp, ",");
        while (tok) {
            char *cl = sa11_strip(tok);
            if (cl && cl[0]) {
                if (strcmp(cl, "*") == 0 || strcmp(cl, nid) == 0) { allowed = true; }
            }
            free(cl); tok = strtok(NULL, ",");
        }
        free(cp);
        if (allowed) break;
    }
    if (allowed) { free(nid); return json_bool(true); }
    const char *gall = sa11_str(json_obj_get(env, "GATEWAY_ALLOW_ALL_USERS"));
    free(nid);
    return json_bool(sa11_env_yes(gall));
}
