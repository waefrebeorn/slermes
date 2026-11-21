/*
 * slack_adapter_flow10.c -- FLOW cores, wave 10, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's still-unclaimed remainder after flow1-9. Every ported name was
 * UNCLAIMED by a strict PoP grep of the whole main-only repo at port time
 * (each carries its PoP annotation at its definition below). No flow1-9
 * function is re-ported.
 *
 * Ported (16 functions):
 *   _handle_approval_action, _handle_assistant_thread_lifecycle_event,
 *   _handle_clarify_action, _handle_feedback_action, _handle_hermes_command,
 *   _handle_model_picker_action, _handle_slack_file_shared, _handle_slack_message,
 *   _handle_slack_message_impl, _handle_slack_reaction, _handle_slash_command,
 *   _handle_slash_confirm_action, _has_active_session_for_thread,
 *   _hint_allow_bots, _slack_reaction_trigger_target, _socket_ping_pong_stale.
 *
 * Decision sequences only: every injected/unported callable enters through ONE
 * universal seam sa10_call(ctx, name, args). The harness wires it to a recording
 * stub fed in.io; the oracle installs recorders emitting the same
 * {"name","args","ret"} event shape from the SAME scripts. Ordered trace,
 * BaseException passthrough, fail-closed. Self-contained: libjson only.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow10.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - A raise is {"__raise__": cls, "message": str(exc), ...extra}.
 * - {"__obj__": attrs} is an attribute carrier (SimpleNamespace double).
 * - isinstance/except families walk the full builtin exception table below.
 * - BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) PASS THROUGH every except-Exception site.
 * - Fail-closed: a missing/exhausted seam script is a RuntimeError raise
 *   spec recorded in the trace before propagation.
 * - Pure helpers modeled deterministically in BOTH ports: t() -> key identity;
 *   the small class-constant maps are transcribed identically.
 * - Mutable self dicts (resolved maps, channel_team, processed ts, picker and
 *   clarify state) live in ctx.state and are echoed as out.state.
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
typedef struct sa10_ctx {
    json_t *state;      /* owned; echoed as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa10_ctx_t;

/* -- basic helpers -- */
static bool sa10_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool sa10_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}
static json_t *sa10_obj_attrs(const json_t *v) {
    return sa10_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}
static bool sa10_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}
static bool sa10_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}
static bool sa10_collection_has_str(const json_t *coll, const char *s) {
    if (!s || !coll) return false;
    if (sa10_is_set(coll)) coll = json_obj_get(coll, "__set__");
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
static bool sa10_plain_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa10_is_obj(v) &&
           !sa10_is_set(v) && !sa10_is_tuple_marker(v) && !sa10_is_raise(v);
}
static bool sa10_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa10_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}
static const char *sa10_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static char *sa10_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static json_t *sa10_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static json_t *sa10_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = sa10_err(type, msg);
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
static json_t *sa10_call(sa10_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa10_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa10_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa10_make_exc(raise);
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
typedef struct sa10_exc_parent { const char *name, *parent1, *parent2; } sa10_exc_parent_t;
static const sa10_exc_parent_t SA10_EXC_TABLE[] = {
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
#define SA10_EXC_TABLE_LEN (sizeof(SA10_EXC_TABLE) / sizeof(SA10_EXC_TABLE[0]))

static const sa10_exc_parent_t *sa10_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA10_EXC_TABLE_LEN; i++)
        if (strcmp(SA10_EXC_TABLE[i].name, name) == 0) return &SA10_EXC_TABLE[i];
    return NULL;
}
typedef struct sa10_chain { char names[64][96]; size_t n; } sa10_chain_t;
static bool sa10_chain_has(const sa10_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}
static void sa10_chain_add(sa10_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa10_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}
static void sa10_chain_walk(sa10_chain_t *c, const char *name) {
    if (!name || !name[0] || sa10_chain_has(c, name)) return;
    sa10_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const sa10_exc_parent_t *be = sa10_builtin_entry(name);
    const char *p1 = be ? be->parent1 : "Exception";
    const char *p2 = be ? be->parent2 : NULL;
    if (p1 && strcmp(p1, name) != 0) sa10_chain_walk(c, p1);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa10_chain_walk(c, p2);
}
static bool sa10_exc_family(const json_t *r, const char *family) {
    if (!sa10_is_raise(r)) return false;
    sa10_chain_t c; c.n = 0;
    sa10_chain_walk(&c, json_get_str(r, "__raise__", "Exception"));
    return sa10_chain_has(&c, family);
}
static bool sa10_raise_passes(const json_t *r) {
    return sa10_is_raise(r) && !sa10_exc_family(r, "Exception");
}

/* -- ASCII whitespace + string kernels -- */
static bool sa10_is_ws(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static char *sa10_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa10_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa10_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa10_lstrip_char(const char *s, char ch) {
    if (!s) return NULL;
    while (*s == ch) s++;
    return sa10_strdup(s);
}
static char *sa10_strip_chars(const char *s, const char *chars) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && strchr(chars, s[a]) != NULL) a++;
    while (b > a && strchr(chars, s[b - 1]) != NULL) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *sa10_lower(const char *s) {
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
static char *sa10_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa10_strdup("None");
    if (v->type == JSON_STRING) return sa10_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return sa10_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return sa10_strdup(buf);
    }
    return sa10_strdup("");
}
static char *sa10_replace(const char *s, const char *from, const char *to) {
    if (!s) return NULL;
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return sa10_strdup(s);
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
static bool sa10_startswith(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool sa10_is_num(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}
static double sa10_num(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* -- local helpers -- */
static json_t *sa10_o(void) { return json_object(); }
static void sa10_ss(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
static void sa10_sc(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, json_copy(v ? v : json_null()));
}
static void sa10_sb(json_t *o, const char *k, bool b) { json_set(o, k, json_bool(b)); }
static void sa10_sn(json_t *o, const char *k, double n) { json_set(o, k, json_number(n)); }
static const char *sa10_s(const json_t *v, const char *def) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static long long sa10_to_int(const json_t *v, long long def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
}
static long long sa10_gi(const json_t *o, const char *k, long long def) {
    return sa10_to_int(o ? json_obj_get(o, k) : NULL, def);
}
static bool sa10_gb(const json_t *o, const char *k, bool def) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    return v && v->type == JSON_BOOL ? v->bool_val : def;
}
static json_t *sa10_get(const json_t *obj, const char *key, const json_t *def) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return json_copy(v ? v : def);
}
static json_t *sa10_arr(const json_t *a, size_t i) {
    if (!a || a->type != JSON_ARRAY || i >= json_len(a)) return json_null();
    return json_copy(json_get(a, i));
}
static bool sa10_json_eq(const json_t *a, const char *s) {
    const char *v = sa10_str(a);
    return v && strcmp(v, s) == 0;
}
/* str(arg or "") via pystr. */
static char *sa10_strval(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa10_strdup("");
    return sa10_pystr(v);
}

/* -- mutable self-dict state helpers (echoed as out.state) -- */
static json_t *sa10_sget(sa10_ctx_t *ctx, const char *name, const char *key, const json_t *def) {
    json_t *d = json_obj_get(ctx->state, name);
    json_t *v = d && d->type == JSON_OBJECT ? json_obj_get(d, key) : NULL;
    return json_copy(v ? v : def);
}
static bool sa10_shas(sa10_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    return d && d->type == JSON_OBJECT && json_obj_get(d, key) != NULL;
}
static json_t *sa10_spop(sa10_ctx_t *ctx, const char *name, const char *key, const json_t *def) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_OBJECT) { d = json_object(); json_set(ctx->state, name, d); }
    json_t *v = json_obj_get(d, key);
    json_t *r = json_copy(v ? v : def);
    if (v) json_obj_del(d, key);
    return r;
}
static void sa10_sset(sa10_ctx_t *ctx, const char *name, const char *key, const json_t *v) {
    json_t *d = json_obj_get(ctx->state, name);
    if (!d || d->type != JSON_OBJECT) { d = json_object(); json_set(ctx->state, name, d); }
    json_set(d, key, json_copy(v ? v : json_null()));
}
static void sa10_sdel(sa10_ctx_t *ctx, const char *name, const char *key) {
    json_t *d = json_obj_get(ctx->state, name);
    if (d && d->type == JSON_OBJECT) json_obj_del(d, key);
}

/* Exception-family raise inside a try -> logger.warn/debug/error + return "". */
static json_t *sa10_try_log(sa10_ctx_t *ctx, json_t *r, const char *label, const char *seam) {
    json_t *w = sa10_o(); sa10_ss(w, "label", label);
    sa10_sc(w, "error", json_obj_get(r, "message"));
    json_free(r);
    json_t *lr = sa10_call(ctx, seam, w);
    if (sa10_is_raise(lr)) return lr;
    json_free(lr);
    return json_string("");
}

/* t() -> identity of the key (deterministic stand-in, both ports). */
static json_t *sa10_t(sa10_ctx_t *ctx, const char *key, const json_t *args) {
    (void)ctx; (void)args;
    return json_string(key ? key : "");
}

/* ====================================================================== */

/* PoP: sa10_hint_allow_bots @ plugins/platforms/slack/adapter.py:_hint_allow_bots */
static json_t *sa10_hint_allow_bots(sa10_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = sa10_call(ctx, "slack_allow_bots", sa10_o());
    if (sa10_is_raise(a)) return a;
    char *allow = sa10_strdup(sa10_s(a, ""));
    bool ne = strcmp(allow ? allow : "", "none") != 0;
    json_free(a);
    if (ne) {
        json_t *b = sa10_o(); sa10_ss(b, "allow", allow ? allow : "");
        free(allow);
        json_t *r = sa10_call(ctx, "logger.info", b);
        if (sa10_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: sa10_slack_reaction_trigger_target @ plugins/platforms/slack/adapter.py:_slack_reaction_trigger_target */
static json_t *sa10_slack_reaction_trigger_target(sa10_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    json_t *raw = sa10_get(extra, "reaction_trigger_target", json_null());
    if (!raw || raw->type == JSON_NULL) {
        json_free(raw);
        json_t *a = sa10_o(); sa10_ss(a, "name", "SLACK_REACTION_TRIGGER_TARGET"); sa10_ss(a, "default", "");
        raw = sa10_call(ctx, "get_scoped_secret", a);
        if (sa10_is_raise(raw)) return raw;
    }
    char *s = sa10_strval(raw); json_free(raw);
    char *st = sa10_strip(s ? s : ""); free(s);
    const char *colon = strchr(st ? st : "", ':');
    char *ch, *th;
    if (colon) {
        size_t n = (size_t)(colon - st);
        ch = (char *)malloc(n + 1); if (ch) { memcpy(ch, st, n); ch[n] = '\0'; }
        th = sa10_strdup(colon + 1);
    } else { ch = sa10_strdup(st ? st : ""); th = sa10_strdup(""); }
    free(st);
    char *c2 = sa10_strip(ch ? ch : ""); free(ch);
    char *t2 = sa10_strip(th ? th : ""); free(th);
    json_t *out = json_array();
    json_append(out, json_string(c2 ? c2 : ""));
    json_append(out, json_string(t2 ? t2 : ""));
    free(c2); free(t2);
    return out;
}

/* PoP: sa10_socket_ping_pong_stale @ plugins/platforms/slack/adapter.py:_socket_ping_pong_stale */
static json_t *sa10_socket_ping_pong_stale(sa10_ctx_t *ctx, const json_t *args) {
    json_t *client = sa10_call(ctx, "client", sa10_o());
    if (sa10_is_raise(client)) return client;
    if (!client || client->type == JSON_NULL || client->type != JSON_OBJECT) { json_free(client); return json_bool(false); }
    json_t *piv = sa10_get(client, "ping_interval", json_null());
    json_t *last = sa10_get(client, "last_ping_pong_time", json_null());
    json_free(client);
    if (!sa10_is_num(piv) || sa10_num(piv, 0) <= 0) { json_free(piv); json_free(last); return json_bool(false); }
    double ping = sa10_num(piv, 0); json_free(piv);
    if (!last || last->type == JSON_NULL) {
        json_free(last);
        const json_t *started = json_obj_get(args, "started");
        if (!started || started->type == JSON_NULL) return json_bool(false);
        double grace_s = sa10_num(json_obj_get(args, "grace_s"), 0);
        double grace = grace_s > ping * 2.0 ? grace_s : ping * 2.0;
        json_t *m = sa10_call(ctx, "monotonic", sa10_o());
        if (sa10_is_raise(m)) return m;
        double now = sa10_num(m, 0); json_free(m);
        return json_bool((now - sa10_num(started, 0)) > grace);
    }
    if (!sa10_is_num(last)) { json_free(last); return json_bool(false); }
    double lv = sa10_num(last, 0); json_free(last);
    json_t *t = sa10_call(ctx, "time", sa10_o());
    if (sa10_is_raise(t)) return t;
    double now = sa10_num(t, 0); json_free(t);
    double factor = sa10_num(json_obj_get(args, "stale_factor"), 0);
    return json_bool((now - lv) > (ping * factor));
}

/* PoP: sa10_has_active_session_for_thread @ plugins/platforms/slack/adapter.py:_has_active_session_for_thread */
static json_t *sa10_has_active_session_for_thread(sa10_ctx_t *ctx, const json_t *args) {
    const json_t *store = json_obj_get(args, "session_store");
    if (!store || store->type == JSON_NULL || !sa10_truthy(store)) return json_bool(false);
    json_t *a = sa10_o();
    sa10_sc(a, "channel_id", json_obj_get(args, "channel_id"));
    sa10_sc(a, "thread_ts", json_obj_get(args, "thread_ts"));
    sa10_sc(a, "user_id", json_obj_get(args, "user_id"));
    sa10_sc(a, "team_id", json_obj_get(args, "team_id"));
    sa10_sc(a, "chat_type", json_obj_get(args, "chat_type"));
    json_t *key = sa10_call(ctx, "build_thread_session_key", a);
    if (sa10_is_raise(key)) {
        if (sa10_raise_passes(key)) return key;
        json_free(key); return json_bool(false);
    }
    if (!sa10_truthy(key)) { json_free(key); return json_bool(false); }
    json_free(key);
    json_t *r = sa10_call(ctx, "session_ensure_loaded", sa10_o());
    if (sa10_is_raise(r)) {
        if (sa10_raise_passes(r)) return r;
        json_free(r); return json_bool(false);
    }
    json_free(r);
    json_t *entry = sa10_call(ctx, "session_entry", sa10_o());
    if (sa10_is_raise(entry)) {
        if (sa10_raise_passes(entry)) return entry;
        json_free(entry); return json_bool(false);
    }
    if (!entry || entry->type == JSON_NULL) { json_free(entry); return json_bool(false); }
    bool suspended = sa10_truthy(json_obj_get(entry, "suspended"));
    json_free(entry);
    return json_bool(!suspended);
}

/* PoP: sa10_handle_feedback_action @ plugins/platforms/slack/adapter.py:_handle_feedback_action */
static json_t *sa10_handle_feedback_action(sa10_ctx_t *ctx, const json_t *args) {
    json_t *r = sa10_call(ctx, "ack", sa10_o());
    if (sa10_is_raise(r)) return r; json_free(r);
    const json_t *action = json_obj_get(args, "action");
    char *value = sa10_strval(json_obj_get(action, "value"));
    const json_t *body = json_obj_get(args, "body");
    const json_t *message = json_obj_get(body, "message");
    if (!message || message->type != JSON_OBJECT) message = NULL;
    const char *channel_id = sa10_s(json_obj_get(json_obj_get(body, "channel"), "id"), "");
    const char *user_id = sa10_s(json_obj_get(json_obj_get(body, "user"), "id"), "");
    const char *ts = message ? sa10_s(json_obj_get(message, "ts"), "") : "";
    json_t *a = sa10_o();
    sa10_ss(a, "value", value ? value : ""); sa10_ss(a, "user_id", user_id);
    sa10_ss(a, "channel_id", channel_id); sa10_ss(a, "ts", ts);
    free(value);
    r = sa10_call(ctx, "logger.info", a);
    if (sa10_is_raise(r)) return r; json_free(r);
    return json_null();
}

/* PoP: sa10_handle_hermes_command @ plugins/platforms/slack/adapter.py:_handle_hermes_command */
static json_t *sa10_handle_hermes_command(sa10_ctx_t *ctx, const json_t *args) {
    const json_t *command = json_obj_get(args, "command");
    json_t *a = sa10_o(); sa10_sc(a, "channel_id", json_obj_get(command, "channel_id"));
    json_t *g = sa10_call(ctx, "slash_channel_gated", a);
    if (sa10_is_raise(g)) return g;
    bool gated = sa10_truthy(g); json_free(g);
    if (gated) {
        json_t *r = sa10_call(ctx, "ack", sa10_o());
        if (sa10_is_raise(r)) return r; json_free(r);
    } else {
        char *slash = sa10_lstrip_char(sa10_s(json_obj_get(command, "command"), ""), '/');
        json_t *ta = sa10_o(); sa10_ss(ta, "key", "platform.slack.slash.running"); sa10_ss(ta, "command", slash ? slash : "");
        free(slash);
        json_t *text = sa10_t(ctx, "platform.slack.slash.running", ta);
        json_t *b = sa10_o(); sa10_ss(b, "response_type", "ephemeral");
        json_set(b, "text", text);
        json_t *r = sa10_call(ctx, "ack", b);
        if (sa10_is_raise(r)) return r; json_free(r);
    }
    json_t *c = sa10_o(); sa10_sc(c, "command", command);
    json_t *r = sa10_call(ctx, "handle_slash_command", c);
    return r;
}

/* PoP: sa10_handle_assistant_thread_lifecycle_event @ plugins/platforms/slack/adapter.py:_handle_assistant_thread_lifecycle_event */
static json_t *sa10_handle_assistant_thread_lifecycle_event(sa10_ctx_t *ctx, const json_t *args) {
    json_t *a = sa10_o();
    sa10_sc(a, "event", json_obj_get(args, "event")); sa10_sc(a, "body", json_obj_get(args, "body"));
    json_t *md = sa10_call(ctx, "extract_assistant_thread_metadata", a);
    if (sa10_is_raise(md)) return md;
    json_t *b = sa10_o(); sa10_sc(b, "metadata", md);
    json_t *r = sa10_call(ctx, "cache_assistant_thread_metadata", b);
    if (sa10_is_raise(r)) { json_free(md); return r; } json_free(r);
    const char *tts = sa10_s(json_obj_get(md, "thread_ts"), "");
    if (tts[0]) {
        json_t *c = sa10_o(); sa10_sc(c, "metadata", md); sa10_ss(c, "thread_ts", tts);
        json_t *fl = json_array();
        json_append(fl, json_string("[Slack] Failed to seed assistant thread session for %s/%s"));
        json_append(fl, json_copy(json_obj_get(md, "channel_id")));
        json_append(fl, json_string(tts));
        json_set(c, "fail_log", fl);
        r = sa10_call(ctx, "seed_dm_session", c);
        if (sa10_is_raise(r)) { json_free(md); return r; } json_free(r);
    }
    json_t *d = sa10_o();
    sa10_sc(d, "channel_id", json_obj_get(md, "channel_id"));
    sa10_sc(d, "team_id", json_obj_get(md, "team_id"));
    sa10_sc(d, "thread_ts", json_obj_get(md, "thread_ts"));
    json_free(md);
    r = sa10_call(ctx, "set_assistant_suggested_prompts", d);
    if (sa10_is_raise(r)) return r; json_free(r);
    return json_null();
}

/* helper: parse int with success flag (mirrors int() on a decimal string). */
static long long sa10_parse_int(const char *s, bool *ok) {
    *ok = false;
    if (!s || !*s) return 0;
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (end && *end == '\0') *ok = true;
    return v;
}
/* first of two strings that starts with T */
static const char *sa10_first_T(const char *a, const char *b) {
    if (a && a[0] == 'T') return a;
    if (b && b[0] == 'T') return b;
    return "";
}

/* PoP: sa10_handle_approval_action @ plugins/platforms/slack/adapter.py:_handle_approval_action */
static json_t *sa10_handle_approval_action(sa10_ctx_t *ctx, const json_t *args) {
    json_t *a = sa10_o(); sa10_ss(a, "kind", "approval");
    json_t *started = sa10_call(ctx, "begin_interaction", a);
    if (sa10_is_raise(started)) return started;
    if (!started || started->type != JSON_ARRAY || json_len(started) < 8) { json_free(started); return json_null(); }
    json_t *team_v = sa10_arr(started, 0), *action_v = sa10_arr(started, 1);
    json_t *session_v = sa10_arr(started, 2), *message = sa10_arr(started, 3);
    json_t *msg_v = sa10_arr(started, 4), *chan_v = sa10_arr(started, 5);
    json_t *uname_v = sa10_arr(started, 6), *uid_v = sa10_arr(started, 7);
    const char *team_id = sa10_s(team_v, ""), *action_id = sa10_s(action_v, "");
    const char *session_key = sa10_s(session_v, ""), *msg_ts = sa10_s(msg_v, "");
    const char *channel_id = sa10_s(chan_v, ""), *user_name = sa10_s(uname_v, "");
    const char *user_id = sa10_s(uid_v, "");
    const char *choice = (strcmp(action_id, "approve") == 0) ? "approve" : "deny";
    /* approval_key: bare ts if present in resolved, else workspace marker */
    char *akey = NULL;
    if (sa10_shas(ctx, "_approval_resolved", msg_ts)) akey = sa10_strdup(msg_ts);
    else {
        json_t *b = sa10_o(); sa10_ss(b, "team_id", team_id); sa10_ss(b, "msg_ts", msg_ts);
        json_t *m = sa10_call(ctx, "workspace_message_marker", b);
        if (sa10_is_raise(m)) { json_free(m); goto done_approval; }
        akey = sa10_strdup(sa10_s(m, "")); json_free(m);
    }
    /* request_id peek (before refusal) */
    json_t *req_peek = sa10_sget(ctx, "_approval_request_ids", akey, json_null());
    json_t *c = sa10_o(); sa10_sc(c, "session_key", session_v); sa10_ss(c, "user_id", user_id);
    sa10_sc(c, "request_id", req_peek); sa10_ss(c, "choice", choice);
    sa10_sc(c, "body", json_obj_get(args, "body"));
    json_free(req_peek);
    json_t *refuse = sa10_call(ctx, "refuse_other_requester", c);
    if (sa10_is_raise(refuse)) { json_free(refuse); goto done_approval; }
    bool ref = sa10_truthy(refuse); json_free(refuse);
    if (ref) goto done_approval;
    /* double-click guard: pop resolved (default True) */
    json_t *popped = sa10_spop(ctx, "_approval_resolved", akey, json_bool(true));
    bool already = sa10_truthy(popped); json_free(popped);
    if (already) goto done_approval;
    json_t *req = sa10_spop(ctx, "_approval_request_ids", akey, json_null());
    /* try: resolve + info */
    json_t *d = sa10_o(); sa10_sc(d, "session_key", session_v); sa10_ss(d, "choice", choice);
    sa10_sc(d, "request_id", req); sa10_ss(d, "actor_id", user_id);
    sa10_sc(d, "extra", json_obj_get(args, "extra"));
    json_t *count = sa10_call(ctx, "resolve_approval", d);
    if (sa10_is_raise(count)) {
        if (sa10_raise_passes(count)) { json_free(count); goto done_approval; }
        json_t *e = sa10_o(); sa10_sc(e, "error", json_obj_get(count, "message"));
        json_free(count);
        json_t *le = sa10_call(ctx, "logger.error", e);
        if (sa10_is_raise(le)) { json_free(le); goto done_approval; }
        json_free(le);
        count = json_number(0);
    } else {
        json_t *i = sa10_o(); sa10_ss(i, "choice", choice); sa10_ss(i, "user_id", user_id);
        sa10_sn(i, "count", sa10_num(count, 0));
        json_t *lr = sa10_call(ctx, "logger.info", i);
        if (sa10_is_raise(lr)) { json_free(lr); json_free(count); goto done_approval; }
        json_free(lr);
    }
    char dk[128]; snprintf(dk, sizeof dk, "platform.slack.approval.%s", choice);
    json_t *dt = sa10_t(ctx, dk, NULL);
    if (sa10_num(count, 0) == 0) { json_free(dt); dt = sa10_t(ctx, "platform.shared.approval_expired", NULL); }
    json_free(count);
    /* original text = section_text(message).replace(ea_deadline_line(), "") */
    json_t *sa = sa10_o(); sa10_sc(sa, "message", message);
    json_t *st = sa10_call(ctx, "section_text", sa);
    if (sa10_is_raise(st)) { json_free(st); json_free(dt); goto done_approval; }
    const char *sts = sa10_s(st, "");
    json_t *dl = sa10_call(ctx, "ea_deadline_line", sa10_o());
    if (sa10_is_raise(dl)) { json_free(dl); json_free(st); json_free(dt); goto done_approval; }
    char *orig = sa10_replace(sts, sa10_s(dl, ""), "");
    json_free(dl); json_free(st); if (!orig) orig = sa10_strdup("");
    json_t *f = sa10_o(); sa10_ss(f, "channel_id", channel_id); sa10_ss(f, "msg_ts", msg_ts);
    sa10_ss(f, "original_text", orig); free(orig);
    json_set(f, "decision_text", dt);
    sa10_ss(f, "placeholder", "Command approval request"); sa10_ss(f, "label", "approval");
    sa10_sc(f, "team_id", team_id[0] ? team_v : json_null());
    json_t *fr = sa10_call(ctx, "finalize_interactive_message", f);
    if (sa10_is_raise(fr)) { json_free(fr); goto done_approval; }
    json_free(fr);
done_approval:
    free(akey);
    json_free(session_v); json_free(action_v); json_free(message); json_free(msg_v);
    json_free(chan_v); json_free(uname_v); json_free(uid_v); json_free(team_v);
    json_free(started);
    return json_null();
}

/* PoP: sa10_handle_slash_confirm_action @ plugins/platforms/slack/adapter.py:_handle_slash_confirm_action */
static json_t *sa10_handle_slash_confirm_action(sa10_ctx_t *ctx, const json_t *args) {
    json_t *a = sa10_o(); sa10_ss(a, "kind", "slash-confirm");
    json_t *started = sa10_call(ctx, "begin_interaction", a);
    if (sa10_is_raise(started)) return started;
    if (!started || started->type != JSON_ARRAY || json_len(started) < 8) { json_free(started); return json_null(); }
    json_t *team_v = sa10_arr(started,0), *action_v=sa10_arr(started,1), *value_v=sa10_arr(started,2);
    json_t *message=sa10_arr(started,3), *msg_v=sa10_arr(started,4), *chan_v=sa10_arr(started,5);
    json_t *uname_v=sa10_arr(started,6), *uid_v=sa10_arr(started,7);
    const char *team_id=sa10_s(team_v,""), *action_id=sa10_s(action_v,"");
    const char *value=sa10_s(value_v,""), *msg_ts=sa10_s(msg_v,""), *channel_id=sa10_s(chan_v,"");
    const char *user_name=sa10_s(uname_v,""), *user_id=sa10_s(uid_v,"");
    const char *bar = strchr(value, '|');
    if (!bar) {
        json_t *w=sa10_o(); sa10_ss(w,"value",value);
        json_t *r=sa10_call(ctx,"logger.warning",w); if(sa10_is_raise(r)){json_free(r);goto done_conf;}
        json_free(r); goto done_conf;
    }
    size_t sklen=(size_t)(bar-value);
    char *sk=(char*)malloc(sklen+1); if(sk){memcpy(sk,value,sklen);sk[sklen]='\0';}
    const char *cid=bar+1;
    json_t *c=sa10_o(); sa10_ss(c,"session_key",sk?sk:""); sa10_ss(c,"confirm_id",cid);
    sa10_ss(c,"user_id",user_id); sa10_sc(c,"extra",json_obj_get(args,"extra"));
    free(sk);
    json_t *refuse=sa10_call(ctx,"refuse_other_requester",c);
    if(sa10_is_raise(refuse)){json_free(refuse);goto done_conf;}
    bool ref=sa10_truthy(refuse); json_free(refuse); if(ref) goto done_conf;
    const char *choice=(strcmp(action_id,"confirm")==0)?"confirm":"cancel";
    char dk[128]; snprintf(dk,sizeof dk,"platform.slack.slash_confirm.%s",choice);
    json_t *dt=sa10_t(ctx,dk,NULL);
    json_t *sa=sa10_o(); sa10_sc(sa,"message",message);
    json_t *st=sa10_call(ctx,"section_text",sa);
    if(sa10_is_raise(st)){json_free(st);json_free(dt);goto done_conf;}
    const char *orig=sa10_s(st,"");
    json_t *f=sa10_o(); sa10_ss(f,"channel_id",channel_id); sa10_ss(f,"msg_ts",msg_ts);
    sa10_ss(f,"original_text",orig); json_set(f,"decision_text",dt);
    sa10_ss(f,"placeholder","Confirmation prompt"); sa10_ss(f,"label","slash-confirm");
    sa10_sc(f,"team_id",team_id[0]?team_v:json_null());
    json_t *fr=sa10_call(ctx,"finalize_interactive_message",f);
    if(sa10_is_raise(fr)){json_free(fr);json_free(st);goto done_conf;} json_free(fr);
    /* try: resolve then post */
    /* re-split to recover session_key (freed above) */
    const char *bar2=strchr(value,'|'); size_t l2=(size_t)(bar2-value);
    char *sk2=(char*)malloc(l2+1); if(sk2){memcpy(sk2,value,l2);sk2[l2]='\0';}
    json_t *d=sa10_o(); sa10_ss(d,"session_key",sk2?sk2:""); sa10_ss(d,"confirm_id",bar2?bar2+1:""); sa10_ss(d,"choice",choice);
    free(sk2);
    json_t *res=sa10_call(ctx,"resolve_slash_confirm",d);
    if(sa10_is_raise(res)){
        if(sa10_raise_passes(res)){json_free(res);json_free(st);goto done_conf;}
        json_t *e=sa10_o(); sa10_sc(e,"error",json_obj_get(res,"message")); json_free(res);
        json_t *le=sa10_call(ctx,"logger.error",e); if(sa10_is_raise(le)){json_free(le);json_free(st);goto done_conf;} json_free(le);
        json_free(st); goto done_conf;
    }
    if(sa10_truthy(res)){
        json_t *pk=sa10_o(); sa10_ss(pk,"channel",channel_id); sa10_sc(pk,"text",res);
        const char *tthread=sa10_s(json_obj_get(message,"thread_ts"), msg_ts);
        if(tthread[0]) sa10_ss(pk,"thread_ts",tthread);
        json_t *cl=sa10_call(ctx,"get_client",sa10_o());
        if(sa10_is_raise(cl)){json_free(cl);json_free(res);json_free(st);goto done_conf;} json_free(cl);
        json_t *pr=sa10_call(ctx,"chat_postMessage",pk);
        if(sa10_is_raise(pr)){json_free(pr);json_free(res);json_free(st);goto done_conf;} json_free(pr);
    }
    json_free(res);
    json_t *i=sa10_o(); sa10_ss(i,"session_key",value); sa10_ss(i,"choice",choice); sa10_ss(i,"user_id",user_id);
    json_t *lr=sa10_call(ctx,"logger.info",i); if(sa10_is_raise(lr)){json_free(lr);json_free(st);goto done_conf;} json_free(lr);
    json_free(st);
done_conf:
    json_free(team_v);json_free(action_v);json_free(value_v);json_free(message);json_free(msg_v);
    json_free(chan_v);json_free(uname_v);json_free(uid_v);json_free(started);
    return json_null();
}

/* PoP: sa10_handle_clarify_action @ plugins/platforms/slack/adapter.py:_handle_clarify_action */
static json_t *sa10_handle_clarify_action(sa10_ctx_t *ctx, const json_t *args) {
    json_t *a=sa10_o(); sa10_ss(a,"kind","clarify"); sa10_sb(a,"team_scoped",false);
    json_t *started=sa10_call(ctx,"begin_interaction",a);
    if(sa10_is_raise(started)) return started;
    if(!started||started->type!=JSON_ARRAY||json_len(started)<8){json_free(started);return json_null();}
    json_t *team_v=sa10_arr(started,0),*action_v=sa10_arr(started,1),*value_v=sa10_arr(started,2);
    json_t *message=sa10_arr(started,3),*msg_v=sa10_arr(started,4),*chan_v=sa10_arr(started,5);
    json_t *uname_v=sa10_arr(started,6),*uid_v=sa10_arr(started,7);
    const char *action_id=sa10_s(action_v,""),*value=sa10_s(value_v,"");
    const char *msg_ts=sa10_s(msg_v,""),*channel_id=sa10_s(chan_v,"");
    const char *user_name=sa10_s(uname_v,""),*user_id=sa10_s(uid_v,"");
    const char *bar=strchr(value,'|');
    if(!bar){
        json_t *w=sa10_o();sa10_ss(w,"value",value);json_t *r=sa10_call(ctx,"logger.warning",w);
        if(sa10_is_raise(r)){json_free(r);goto done_clar;} json_free(r); goto done_clar;
    }
    size_t clen=(size_t)(bar-value); char *cid=(char*)malloc(clen+1); if(cid){memcpy(cid,value,clen);cid[clen]='\0';}
    const char *token=bar+1;
    json_t *c=sa10_o();sa10_ss(c,"clarify_id",cid?cid:"");sa10_ss(c,"user_id",user_id);sa10_sc(c,"extra",json_obj_get(args,"extra"));
    free(cid);
    json_t *refuse=sa10_call(ctx,"refuse_other_requester",c);
    if(sa10_is_raise(refuse)){json_free(refuse);goto done_clar;} bool ref=sa10_truthy(refuse);json_free(refuse);
    if(ref) goto done_clar;
    json_t *popped=sa10_spop(ctx,"_clarify_resolved",msg_ts,json_bool(true));
    bool already=sa10_truthy(popped);json_free(popped); if(already) goto done_clar;
    json_t *sa=sa10_o();sa10_sc(sa,"message",message);sa10_sn(sa,"limit",0);
    json_t *st=sa10_call(ctx,"section_text",sa);
    if(sa10_is_raise(st)){json_free(st);goto done_clar;}
    const char *orig=sa10_s(st,"");
    json_t *exp=sa10_t(ctx,"platform.slack.clarify.expired",NULL);
    bool is_other=(strcmp(action_id,"hermes_clarify_other")==0||strcmp(token,"other")==0);
    if(is_other){
        /* re-split clarify id */
        const char *b2=strchr(value,'|'); size_t l2=(size_t)(b2-value); char *id2=(char*)malloc(l2+1); if(id2){memcpy(id2,value,l2);id2[l2]='\0';}
        json_t *mc=sa10_o();sa10_ss(mc,"clarify_id",id2?id2:""); free(id2);
        json_t *ok=sa10_call(ctx,"clarify_mark_awaiting_text",mc);
        if(sa10_is_raise(ok)){json_free(ok);json_free(st);json_free(exp);goto done_clar;}
        bool marked=sa10_truthy(ok);json_free(ok);
        if(!marked){
            /* re-split for state delete */
            const char *b3=strchr(value,'|'); size_t l3=(size_t)(b3-value); char *id3=(char*)malloc(l3+1); if(id3){memcpy(id3,value,l3);id3[l3]='\0';}
            sa10_sdel(ctx,"_clarify_messages",id3?id3:""); free(id3);
            json_t *u=sa10_o();sa10_ss(u,"channel_id",channel_id);sa10_ss(u,"msg_ts",msg_ts);
            sa10_ss(u,"original_text",orig);json_set(u,"decision_text",exp);
            json_t *ur=sa10_call(ctx,"update_clarify_message",u);
            if(sa10_is_raise(ur)){json_free(ur);json_free(st);goto done_clar;} json_free(ur);json_free(st);goto done_clar;
        }
        json_t *aw=sa10_t(ctx,"platform.slack.clarify.awaiting",NULL);
        json_t *u=sa10_o();sa10_ss(u,"channel_id",channel_id);sa10_ss(u,"msg_ts",msg_ts);
        sa10_ss(u,"original_text",orig);json_set(u,"decision_text",aw);
        json_t *ur=sa10_call(ctx,"update_clarify_message",u);
        if(sa10_is_raise(ur)){json_free(ur);json_free(aw);json_free(st);json_free(exp);goto done_clar;}
        json_free(ur);json_free(aw);json_free(st);json_free(exp);goto done_clar;
    }
    bool iok=false; long long idx=sa10_parse_int(token,&iok);
    if(!iok){
        json_t *w=sa10_o();sa10_ss(w,"token",token);json_t *r=sa10_call(ctx,"logger.warning",w);
        if(sa10_is_raise(r)){json_free(r);json_free(st);json_free(exp);goto done_clar;} json_free(r);
        json_free(st);json_free(exp);goto done_clar;
    }
    /* state del _clarify_messages[clarify_id] */
    { const char *b4=strchr(value,'|'); size_t l4=(size_t)(b4-value); char *id4=(char*)malloc(l4+1); if(id4){memcpy(id4,value,l4);id4[l4]='\0';}
      sa10_sdel(ctx,"_clarify_messages",id4?id4:""); free(id4); }
    char *resolved=NULL;
    json_t *mc=sa10_o(); const char *b5=strchr(value,'|'); size_t l5=(size_t)(b5-value); char *id5=(char*)malloc(l5+1); if(id5){memcpy(id5,value,l5);id5[l5]='\0';}
    sa10_ss(mc,"clarify_id",id5?id5:"");free(id5);
    json_t *entry=sa10_call(ctx,"clarify_entries_get",mc);
    if(sa10_is_raise(entry)){
        if(sa10_raise_passes(entry)){json_free(entry);json_free(st);json_free(exp);goto done_clar;}
        json_free(entry);
    } else {
        json_t *choices=json_obj_get(entry,"choices");
        if(choices&&choices->type==JSON_ARRAY&&idx>=0&&(size_t)idx<json_len(choices)){
            char *p=sa10_strval(json_get(choices,(size_t)idx)); resolved=p?p:NULL;
        }
        json_free(entry);
    }
    char *display=resolved?sa10_strdup(resolved):NULL;
    if(!resolved){
        char buf[64];snprintf(buf,sizeof buf,"choice %lld",idx+1);resolved=sa10_strdup(buf);
        display=sa10_strdup("platform.slack.clarify.choice_n");
    }
    json_t *rc=sa10_o(); const char *b6=strchr(value,'|'); size_t l6=(size_t)(b6-value); char *id6=(char*)malloc(l6+1); if(id6){memcpy(id6,value,l6);id6[l6]='\0';}
    sa10_ss(rc,"clarify_id",id6?id6:"");free(id6);
    sa10_ss(rc,"resolved_text",resolved?resolved:"");
    json_t *ok=sa10_call(ctx,"clarify_resolve",rc);
    if(sa10_is_raise(ok)){
        if(sa10_raise_passes(ok)){json_free(ok);free(resolved);free(display);json_free(st);json_free(exp);goto done_clar;}
        json_free(ok);
    }
    bool resolved_ok=sa10_truthy(ok);json_free(ok);
    if(resolved_ok){
        json_t *u=sa10_o();sa10_ss(u,"channel_id",channel_id);sa10_ss(u,"msg_ts",msg_ts);
        sa10_ss(u,"original_text",orig);
        json_t *dt=sa10_t(ctx,"platform.slack.clarify.resolved",NULL);json_set(u,"decision_text",dt);
        json_t *ur=sa10_call(ctx,"update_clarify_message",u);
        if(sa10_is_raise(ur)){json_free(ur);free(resolved);free(display);json_free(st);json_free(exp);goto done_clar;} json_free(ur);
        json_t *i=sa10_o(); const char *b7=strchr(value,'|'); size_t l7=(size_t)(b7-value); char *id7=(char*)malloc(l7+1); if(id7){memcpy(id7,value,l7);id7[l7]='\0';}
        sa10_ss(i,"clarify_id",id7?id7:"");free(id7);sa10_sn(i,"idx",(double)idx);sa10_ss(i,"user_id",user_id);
        json_t *lr=sa10_call(ctx,"logger.info",i);if(sa10_is_raise(lr)){json_free(lr);free(resolved);free(display);json_free(st);json_free(exp);goto done_clar;}json_free(lr);
        json_t *dg=sa10_o();sa10_sc(dg,"resolved_text",json_string(resolved?resolved:""));
        json_t *dr=sa10_call(ctx,"logger.debug",dg);if(sa10_is_raise(dr)){json_free(dr);free(resolved);free(display);json_free(st);json_free(exp);goto done_clar;}json_free(dr);
    } else {
        json_t *u=sa10_o();sa10_ss(u,"channel_id",channel_id);sa10_ss(u,"msg_ts",msg_ts);
        sa10_ss(u,"original_text",orig);json_set(u,"decision_text",exp);
        json_t *ur=sa10_call(ctx,"update_clarify_message",u);
        if(sa10_is_raise(ur)){json_free(ur);free(resolved);free(display);json_free(st);goto done_clar;} json_free(ur);
        json_t *w=sa10_o(); const char *b8=strchr(value,'|'); size_t l8=(size_t)(b8-value); char *id8=(char*)malloc(l8+1); if(id8){memcpy(id8,value,l8);id8[l8]='\0';}
        sa10_ss(w,"clarify_id",id8?id8:"");free(id8);
        json_t *wr=sa10_call(ctx,"logger.warning",w);if(sa10_is_raise(wr)){json_free(wr);free(resolved);free(display);json_free(st);goto done_clar;}json_free(wr);
    }
    free(resolved);free(display);json_free(st);json_free(exp);
done_clar:
    json_free(team_v);json_free(action_v);json_free(value_v);json_free(message);json_free(msg_v);
    json_free(chan_v);json_free(uname_v);json_free(uid_v);json_free(started);
    return json_null();
}
/* fresh args object per seam call: sa10_call ADOPTs args into the trace event */
static json_t *sa10_picker_pop(sa10_ctx_t *ctx, const char *marker) {
    json_t *a = sa10_o(); sa10_ss(a,"marker",marker);
    return sa10_call(ctx,"picker_state_pop",a);
}
/* helper: build a picker-notice update args object and call update_picker_message */
static json_t *sa10_mp_update(sa10_ctx_t *ctx, const char *chan, const json_t *team_v,
                              const char *mts, json_t *text) {
    json_t *u = sa10_o(); sa10_ss(u,"channel_id",chan); sa10_sc(u,"team_id",team_v);
    sa10_ss(u,"msg_ts",mts); json_set(u,"text",text);
    return sa10_call(ctx,"update_picker_message",u);
}
static json_t *sa10_mp_expired(sa10_ctx_t *ctx) {
    return sa10_call(ctx,"model_picker_expired_notice",sa10_o());
}
/* helper: get_client -> sanitize_blocks -> chat_update, with Exception catch */
static json_t *sa10_mp_chat_update(sa10_ctx_t *ctx, json_t *blocks,
                                   const char *chan, const char *mts, json_t *text,
                                   const char *loglabel, const json_t *team_v) {
    json_t *sb = sa10_call(ctx,"sanitize_blocks",blocks);
    if(sa10_is_raise(sb)){
        if(sa10_raise_passes(sb)){ json_free(text); return sb; }
        json_t *e=sa10_o(); sa10_sc(e,"error",json_obj_get(sb,"message")); json_free(sb);
        json_t *le=sa10_call(ctx,"logger.warning",e); if(sa10_is_raise(le)){json_free(le);json_free(text);return le;}
        json_free(le); json_free(text); return NULL;
    }
    json_t *cl=sa10_call(ctx,"get_client",sa10_o());
    if(sa10_is_raise(cl)){json_free(cl);json_free(sb);json_free(text);return NULL;}
    json_free(cl);
    json_t *u=sa10_o();sa10_ss(u,"channel",chan);sa10_ss(u,"ts",mts);json_set(u,"text",text);json_set(u,"blocks",sb);
    json_t *up=sa10_call(ctx,"chat_update",u);
    if(sa10_is_raise(up)){
        if(sa10_raise_passes(up)){json_free(up);return up;}
        json_t *e=sa10_o();sa10_sc(e,"error",json_obj_get(up,"message"));json_free(up);
        (void)loglabel;
        json_t *le=sa10_call(ctx,"logger.warning",e);if(sa10_is_raise(le)){json_free(le);return le;}json_free(le);
    } else json_free(up);
    (void)team_v;
    return NULL;
}

/* PoP: sa10_handle_model_picker_action @ plugins/platforms/slack/adapter.py:_handle_model_picker_action */
static json_t *sa10_handle_model_picker_action(sa10_ctx_t *ctx, const json_t *args) {
    json_t *r = sa10_call(ctx, "ack", sa10_o());
    if (sa10_is_raise(r)) return r; json_free(r);
    json_t *ea = sa10_o(); sa10_sc(ea, "event", json_obj_get(args, "event")); sa10_sc(ea, "body", json_obj_get(args, "body"));
    sa10_sc(ea, "install_set", json_obj_get(args, "install_set"));
    json_t *team_v = sa10_call(ctx, "event_team_id", ea);
    if (sa10_is_raise(team_v)) return team_v;
    const json_t *action = json_obj_get(args, "action");
    const char *action_id = sa10_s(json_obj_get(action, "action_id"), "");
    const json_t *body = json_obj_get(args, "body");
    const json_t *message = json_obj_get(body, "message");
    const char *msg_ts = sa10_s(json_obj_get(message, "ts"), "");
    const char *channel_id = sa10_s(json_obj_get(json_obj_get(body, "channel"), "id"), "");
    const char *user_name = sa10_s(json_obj_get(json_obj_get(body, "user"), "name"), "unknown");
    const char *user_id = sa10_s(json_obj_get(json_obj_get(body, "user"), "id"), "");
    json_t *ret = NULL;
    json_t *state = NULL;
    char *marker = NULL;
    json_t *ia = sa10_o(); sa10_ss(ia,"user_id",user_id); sa10_ss(ia,"channel_id",channel_id);
    sa10_ss(ia,"user_name",user_name); sa10_sc(ia,"team_id",team_v);
    json_t *auth = sa10_call(ctx,"is_interactive_user_authorized",ia);
    if(sa10_is_raise(auth)){ret=auth;goto done_mp;}
    bool authorized=sa10_truthy(auth);json_free(auth);
    if(!authorized){
        json_t *w=sa10_o();sa10_ss(w,"user_id",user_id);
        json_t *wr=sa10_call(ctx,"logger.warning",w);if(sa10_is_raise(wr)){ret=wr;goto done_mp;}json_free(wr);
        goto done_mp;
    }
    /* marker */
    marker = NULL;
    if(sa10_shas(ctx,"_model_picker_state",msg_ts)) marker=sa10_strdup(msg_ts);
    else {
        json_t *b=sa10_o();sa10_ss(b,"team_id",sa10_s(team_v, ""));sa10_ss(b,"msg_ts",msg_ts);
        json_t *m=sa10_call(ctx,"workspace_message_marker",b);
        if(sa10_is_raise(m)){ret=m;goto done_mp;}
        marker=sa10_strdup(sa10_s(m,""));json_free(m);
    }
    json_t *mc=sa10_o();sa10_ss(mc,"marker",marker);
    state=sa10_call(ctx,"picker_state_get",mc);
    if(sa10_is_raise(state)){ret=state;goto done_mp;}
    if(!sa10_truthy(state)){
        json_t *notice=sa10_mp_expired(ctx);
        if(sa10_is_raise(notice)){ret=notice;goto done_mp;}
        ret=sa10_mp_update(ctx,channel_id,team_v,msg_ts,notice);
        goto done_mp;
    }
    /* requester refusal */
    json_t *onms=json_obj_get(state,"on_model_selected");
    json_t *pc=sa10_o();sa10_sc(pc,"on_model_selected",onms);sa10_ss(pc,"user_id",user_id);
    sa10_sc(pc,"extra",json_obj_get(args,"extra"));
    json_t *requester=sa10_call(ctx,"picker_refusal",pc);
    if(sa10_is_raise(requester)){ret=requester;goto done_mp;}
    if(sa10_truthy(requester)){
        json_t *rt=sa10_call(ctx,"refusal_text",requester);
        if(sa10_is_raise(rt)){ret=rt;goto done_mp;}
        json_t *cl=sa10_call(ctx,"get_client",sa10_o());
        if(sa10_is_raise(cl)){ret=cl;json_free(rt);goto done_mp;}json_free(cl);
        json_t *e=sa10_o();sa10_ss(e,"channel",channel_id);sa10_ss(e,"user",user_id);
        json_set(e,"text",rt);sa10_sc(e,"thread_ts",json_obj_get(message,"thread_ts"));
        ret=sa10_call(ctx,"chat_postEphemeral",e);
        goto done_mp;
    }
    json_free(requester);
    json_t *providers=json_obj_get(state,"providers");
    if(strcmp(action_id,"model_picker_cancel")==0){
        sa10_picker_pop(ctx,marker);
        json_t *dt=sa10_t(ctx,"platform.slack.picker.cancelled",NULL);
        ret=sa10_mp_update(ctx,channel_id,team_v,msg_ts,dt);
        goto done_mp;
    }
    if(strcmp(action_id,"model_picker_provider")==0){
        const char *tok=sa10_s(json_obj_get(json_obj_get(action,"selected_option"),"value"),"");
        bool iok=false;long long idx=sa10_parse_int(tok,&iok);
        json_t *provider=NULL;
        if(iok&&idx>=0) provider=json_copy(sa10_arr(providers,(size_t)idx));
        if(!provider||!sa10_truthy(provider)){
            json_free(provider);
            json_t *w=sa10_o();sa10_ss(w,"token",tok);json_t *wr=sa10_call(ctx,"logger.warning",w);
            if(sa10_is_raise(wr)){ret=wr;goto done_mp;}json_free(wr);
            sa10_picker_pop(ctx,marker);
            json_t *notice=sa10_mp_expired(ctx);if(sa10_is_raise(notice)){ret=notice;goto done_mp;}
            ret=sa10_mp_update(ctx,channel_id,team_v,msg_ts,notice);goto done_mp;
        }
        const char *slug=sa10_s(json_obj_get(provider,"slug"),"");
        json_t *models=json_obj_get(provider,"models");
        if(!sa10_truthy(models)){
            json_t *dt=sa10_t(ctx,"platform.slack.picker.no_models_for_provider",NULL);
            ret=sa10_mp_update(ctx,channel_id,team_v,msg_ts,dt);
            sa10_picker_pop(ctx,marker);json_free(provider);goto done_mp;
        }
        json_t *ss1=sa10_o();sa10_ss(ss1,"key","stage");sa10_ss(ss1,"value","model");
        sa10_call(ctx,"picker_state_set",ss1);
        json_t *ss2=sa10_o();sa10_ss(ss2,"key","selected_provider_slug");sa10_ss(ss2,"value",slug);
        sa10_call(ctx,"picker_state_set",ss2);
        /* build model blocks */
        json_t *ba=sa10_o();sa10_sc(ba,"providers",providers);sa10_ss(ba,"provider_slug",slug);
        json_t *blocks=sa10_call(ctx,"build_model_picker_model_blocks",ba);
        if(sa10_is_raise(blocks)){ret=blocks;json_free(provider);goto done_mp;}
        char fk[128];snprintf(fk,sizeof fk,"platform.slack.picker.fallback_model");
        (void)fk;
        json_t *text=sa10_t(ctx,"platform.slack.picker.fallback_model",NULL);
        json_t *er=sa10_mp_chat_update(ctx,blocks,channel_id,msg_ts,text,"provider->model",team_v);
        if(sa10_is_raise(er)){ret=er;json_free(provider);goto done_mp;}
        json_free(provider);goto done_mp;
    }
    if(strcmp(action_id,"model_picker_back")==0){
        json_t *s1=sa10_o();sa10_ss(s1,"key","stage");sa10_ss(s1,"value","provider");
        sa10_call(ctx,"picker_state_set",s1);
        json_t *s2=sa10_o();sa10_ss(s2,"key","selected_provider_slug");sa10_ss(s2,"value","");
        sa10_call(ctx,"picker_state_set",s2);
        json_t *la=sa10_o();sa10_sc(la,"provider",json_obj_get(state,"current_provider"));
        json_t *label=sa10_call(ctx,"get_label",la);
        if(sa10_is_raise(label)){
            if(sa10_raise_passes(label)){ret=label;goto done_mp;}
            json_free(label);label=json_copy(json_obj_get(state,"current_provider"));
        }
        json_t *ba=sa10_o();sa10_sc(ba,"providers",providers);
        sa10_sc(ba,"current_model",json_obj_get(state,"current_model"));
        json_set(ba,"provider_label",label);
        json_t *blocks=sa10_call(ctx,"build_model_picker_provider_blocks",ba);
        if(sa10_is_raise(blocks)){ret=blocks;goto done_mp;}
        json_t *text=sa10_t(ctx,"platform.slack.picker.fallback_provider",NULL);
        json_t *er=sa10_mp_chat_update(ctx,blocks,channel_id,msg_ts,text,"back",team_v);
        if(sa10_is_raise(er)){ret=er;goto done_mp;}
        goto done_mp;
    }
    if(strcmp(action_id,"model_picker_model")==0 && sa10_json_eq(json_obj_get(state,"stage"),"model")){
        const char *tok=sa10_s(json_obj_get(json_obj_get(action,"selected_option"),"value"),"");
        const char *slug=sa10_s(json_obj_get(state,"selected_provider_slug"),"");
        /* find provider by slug */
        json_t *found=NULL;
        size_t np=(providers&&providers->type==JSON_ARRAY)?json_len(providers):0;
        for(size_t i=0;i<np;i++){
            json_t *p=json_get(providers,i);
            if(sa10_json_eq(json_obj_get(p,"slug"),slug)){found=p;break;}
        }
        json_t *models=found?json_obj_get(found,"models"):NULL;
        bool iok=false;long long idx=sa10_parse_int(tok,&iok);
        json_t *model_id=NULL;
        if(iok&&idx>=0) model_id=json_copy(sa10_arr(models,(size_t)idx));
        if(!model_id||!sa10_truthy(model_id)){
            json_free(model_id);
            json_t *w=sa10_o();sa10_ss(w,"token",tok);json_t *wr=sa10_call(ctx,"logger.warning",w);
            if(sa10_is_raise(wr)){ret=wr;goto done_mp;}json_free(wr);
            sa10_picker_pop(ctx,marker);
            json_t *notice=sa10_mp_expired(ctx);if(sa10_is_raise(notice)){ret=notice;goto done_mp;}
            ret=sa10_mp_update(ctx,channel_id,team_v,msg_ts,notice);goto done_mp;
        }
        if(!sa10_truthy(onms)){
            json_free(model_id);
            sa10_picker_pop(ctx,marker);
            json_t *notice=sa10_mp_expired(ctx);if(sa10_is_raise(notice)){ret=notice;goto done_mp;}
            ret=sa10_mp_update(ctx,channel_id,team_v,msg_ts,notice);goto done_mp;
        }
        sa10_picker_pop(ctx,marker);
        json_t *sw=sa10_t(ctx,"platform.slack.picker.switching",NULL);
        json_t *ur=sa10_mp_update(ctx,channel_id,team_v,msg_ts,sw);
        if(sa10_is_raise(ur)){ret=ur;json_free(model_id);goto done_mp;}json_free(ur);
        bool switch_failed=false;
        json_t *confirmation=NULL;
        json_t *ca=sa10_o();sa10_sc(ca,"chat_id",json_obj_get(state,"chat_id"));
        json_set(ca,"model_id",json_copy(model_id));sa10_ss(ca,"provider_slug",slug);
        confirmation=sa10_call(ctx,"on_model_selected",ca);
        if(sa10_is_raise(confirmation)){
            if(sa10_raise_passes(confirmation)){ret=confirmation;json_free(model_id);goto done_mp;}
            json_t *e=sa10_o();sa10_sc(e,"error",json_obj_get(confirmation,"message"));json_free(confirmation);
            json_t *le=sa10_call(ctx,"logger.error",e);if(sa10_is_raise(le)){ret=le;json_free(model_id);goto done_mp;}json_free(le);
            confirmation=sa10_t(ctx,"platform.slack.picker.switch_failed",NULL);
            switch_failed=true;
        } else {
            json_t *pf=sa10_call(ctx,"t_error_prefix",sa10_o());
            if(sa10_is_raise(pf)){
                if(sa10_raise_passes(pf)){ret=pf;json_free(confirmation);json_free(model_id);goto done_mp;}
                json_free(pf);
            }
            const char *prefix=sa10_str(pf)?sa10_str(pf):"Error:";
            char *cs=sa10_strval(confirmation);
            if(prefix[0]&&cs&&sa10_startswith(cs,prefix)) switch_failed=true;
            free(cs);json_free(pf);
        }
        const char *hk=switch_failed?"platform.slack.picker.switch_failed_header":"platform.slack.picker.switched_header";
        json_t *header=sa10_t(ctx,hk,NULL);
        char *cs=sa10_strval(confirmation);
        size_t need=strlen(sa10_s(header,""))+strlen(cs?cs:"")+3;
        char *full=(char*)malloc(need);
        snprintf(full,need,"%s\n\n%s",sa10_s(header,""),cs?cs:"");
        free(cs);json_free(header);json_free(confirmation);
        json_t *text=json_string(full);free(full);
        ret=sa10_mp_update(ctx,channel_id,team_v,msg_ts,text);
        json_free(model_id);goto done_mp;
    }
done_mp:
    free(marker);json_free(state);json_free(team_v);
    return ret;
}
/* PoP: sa10_handle_slash_command @ plugins/platforms/slack/adapter.py:_handle_slash_command */
static json_t *sa10_handle_slash_command(sa10_ctx_t *ctx, const json_t *args) {
    const json_t *command = json_obj_get(args, "command");
    const char *user_id = sa10_s(json_obj_get(command, "user_id"), "");
    const char *channel_id = sa10_s(json_obj_get(command, "channel_id"), "");
    const char *team_id = sa10_s(json_obj_get(command, "team_id"), "");
    if (team_id[0] && channel_id[0]) {
        json_t *a = sa10_o(); sa10_ss(a,"channel_id",channel_id); sa10_ss(a,"team_id",team_id);
        sa10_call(ctx,"remember_channel_team",a);
    }
    json_t *ca = sa10_o(); sa10_sc(ca,"command",command);
    json_t *text = sa10_call(ctx,"slash_command_text",ca);
    if (sa10_is_raise(text)) return text;
    const char *txt = sa10_s(text, "");
    json_t *ta = sa10_o(); sa10_sc(ta,"command",command);
    json_t *thread_id = sa10_call(ctx,"slash_thread_id",ta);
    if (sa10_is_raise(thread_id)) { json_free(text); return thread_id; }
    json_t *ga = sa10_o(); sa10_ss(ga,"channel_id",channel_id);
    json_t *g = sa10_call(ctx,"slash_channel_gated",ga);
    if (sa10_is_raise(g)) { json_free(text);json_free(thread_id);return g; }
    bool gated=sa10_truthy(g);json_free(g);
    if(gated){
        json_t *w=sa10_o();sa10_ss(w,"channel_id",channel_id);
        json_t *wr=sa10_call(ctx,"logger.debug",w);if(sa10_is_raise(wr)){json_free(wr);json_free(text);json_free(thread_id);return wr;}json_free(wr);
        json_free(text);json_free(thread_id);return json_null();
    }
    bool is_dm=sa10_startswith(channel_id,"D");
    if(is_dm){
        json_t *d=sa10_call(ctx,"slack_disable_dms",sa10_o());
        if(sa10_is_raise(d)){json_free(text);json_free(thread_id);return d;}
        bool dis=sa10_truthy(d);json_free(d);
        if(dis){
            json_t *i=sa10_o();sa10_ss(i,"channel_id",channel_id);sa10_ss(i,"user_id",user_id);
            json_t *ir=sa10_call(ctx,"logger.info",i);if(sa10_is_raise(ir)){json_free(ir);json_free(text);json_free(thread_id);return ir;}json_free(ir);
            json_free(text);json_free(thread_id);return json_null();
        }
    }
    json_t *ra=sa10_o();sa10_ss(ra,"user_id",user_id);sa10_ss(ra,"channel_id",channel_id);
    sa10_sb(ra,"is_dm",is_dm);sa10_sc(ra,"thread_id",thread_id);
    json_t *rejected=sa10_call(ctx,"early_reject_unauthorized",ra);
    if(sa10_is_raise(rejected)){json_free(rejected);json_free(text);json_free(thread_id);return NULL;}
    bool rej=sa10_truthy(rejected);json_free(rejected);
    if(rej&&!is_dm){json_free(text);json_free(thread_id);return json_null();}
    json_t *bsa=sa10_o();sa10_ss(bsa,"chat_id",channel_id);
    sa10_ss(bsa,"chat_type",is_dm?"dm":"group");sa10_ss(bsa,"user_id",user_id);
    sa10_sc(bsa,"thread_id",thread_id);sa10_sc(bsa,"scope_id",team_id[0]?json_obj_get(command,"team_id"):json_null());
    json_t *source=sa10_call(ctx,"build_source",bsa);
    if(sa10_is_raise(source)){json_free(source);json_free(text);json_free(thread_id);return NULL;}
    json_t *pa=sa10_o();sa10_ss(pa,"channel_id",channel_id);sa10_ss(pa,"team_id",team_id);
    json_t *prompt=sa10_call(ctx,"channel_prompt_with_identity",pa);
    if(sa10_is_raise(prompt)){json_free(prompt);json_free(source);json_free(text);json_free(thread_id);return NULL;}
    json_t *ska=sa10_o();sa10_sc(ska,"extra",json_obj_get(args,"extra"));sa10_ss(ska,"channel_id",channel_id);
    json_t *askill=sa10_call(ctx,"resolve_channel_skills",ska);
    if(sa10_is_raise(askill)){json_free(askill);json_free(prompt);json_free(source);json_free(text);json_free(thread_id);return NULL;}
    json_t *ev=sa10_o();sa10_ss(ev,"text",txt);
    sa10_ss(ev,"message_type",sa10_startswith(txt,"/")?"COMMAND":"TEXT");
    json_set(ev,"source",source);sa10_sc(ev,"raw_message",command);json_set(ev,"channel_prompt",prompt);
    json_set(ev,"auto_skill",askill);
    json_t *ec=sa10_call(ctx,"event_command",ev);
    if(sa10_is_raise(ec)){json_free(ec);json_free(ev);json_free(text);json_free(thread_id);return NULL;}
    json_t *rc=sa10_o();json_set(rc,"command",ec);
    json_t *cmd=sa10_call(ctx,"resolve_command",rc);
    if(sa10_is_raise(cmd)){json_free(cmd);json_free(ev);json_free(text);json_free(thread_id);return NULL;}
    bool controls=false;
    if(cmd&&cmd->type==JSON_OBJECT){
        const char *bp=sa10_s(json_obj_get(cmd,"busy_policy"),"");
        const char *nm=sa10_s(json_obj_get(cmd,"name"),"");
        controls=(strcmp(bp,"interrupt_then_dispatch")==0||strcmp(nm,"approve")==0||strcmp(nm,"deny")==0||strcmp(nm,"pause")==0);
    }
    json_free(cmd);
    json_t *lane=NULL,*ahead=NULL,*handoff=NULL;
    if(!rej&&!controls){
        json_t *ea=sa10_o();json_set(ea,"event",json_copy(ev));
        lane=sa10_call(ctx,"event_session_key",ea);
        if(sa10_is_raise(lane)){json_free(lane);json_free(ev);json_free(text);json_free(thread_id);return NULL;}
        json_t *la=sa10_o();sa10_sc(la,"lane",lane);
        json_t *res=sa10_call(ctx,"slash_lanes_reserve",la);
        if(sa10_is_raise(res)){json_free(res);json_free(lane);json_free(ev);json_free(text);json_free(thread_id);return NULL;}
        ahead=sa10_arr(res,0);handoff=sa10_arr(res,1);json_free(res);
    }
    json_t *tok=sa10_o();sa10_ss(tok,"user_id",user_id);
    json_t *st=sa10_call(ctx,"slash_user_id_set",tok);
    if(sa10_is_raise(st)){json_free(st);json_free(ahead);json_free(handoff);json_free(lane);json_free(text);json_free(thread_id);return NULL;}
    json_free(st);
    json_t *exc=NULL;
    if(lane&&lane->type!=JSON_NULL){
        json_t *na=sa10_o();sa10_ss(na,"channel_id",channel_id);sa10_ss(na,"team_id",team_id);
        json_t *cn=sa10_call(ctx,"resolve_channel_name",na);
        if(sa10_is_raise(cn)){exc=cn;goto slash_finally;}
        json_t *ua=sa10_o();sa10_ss(ua,"user_id",user_id);sa10_ss(ua,"chat_id",channel_id);sa10_ss(ua,"team_id",team_id);
        json_t *un=sa10_call(ctx,"resolve_user_name",ua);
        if(sa10_is_raise(un)){json_free(cn);exc=un;goto slash_finally;}
        json_t *fa=sa10_o();sa10_ss(fa,"chat_name",sa10_s(cn,""));sa10_ss(fa,"user_name",sa10_s(un,""));
        json_free(cn);json_free(un);
        json_t *fr=sa10_call(ctx,"source_set_fields",fa);
        if(sa10_is_raise(fr)){exc=fr;goto slash_finally;}json_free(fr);
        if(ahead&&ahead->type!=JSON_NULL){
            json_t *wa=sa10_o();sa10_sc(wa,"ahead",ahead);
            json_t *wr=sa10_call(ctx,"slash_lanes_wait",wa);
            if(sa10_is_raise(wr)){exc=wr;goto slash_finally;}json_free(wr);
        }
    }
    {
        const char *url=sa10_s(json_obj_get(command,"response_url"),"");
        if(url[0]&&user_id[0]&&channel_id[0]&&sa10_startswith(txt,"/")){
            json_t *sa=sa10_o();sa10_ss(sa,"team_id",team_id);sa10_ss(sa,"channel_id",channel_id);
            sa10_ss(sa,"user_id",user_id);sa10_ss(sa,"response_url",url);
            json_t *sr=sa10_call(ctx,"stash_slash_context",sa);
            if(sa10_is_raise(sr)){exc=sr;goto slash_finally;}json_free(sr);
        }
    }
    {
        json_t *ha=sa10_o();json_set(ha,"event",json_copy(ev));
        json_t *hr=sa10_call(ctx,"handle_message",ha);
        if(sa10_is_raise(hr)){exc=hr;goto slash_finally;}json_free(hr);
    }
slash_finally:
    {
        json_t *ra=sa10_o();sa10_sc(ra,"token",json_string(""));
        json_t *rr=sa10_call(ctx,"slash_user_id_reset",ra);if(sa10_is_raise(rr)){json_free(rr);}else json_free(rr);
        json_t *la=sa10_o();sa10_sc(la,"lane",lane);sa10_sc(la,"ahead",ahead);sa10_sc(la,"handed_off",handoff);
        json_t *lr=sa10_call(ctx,"slash_lanes_release",la);if(sa10_is_raise(lr)){json_free(lr);}else json_free(lr);
    }
    json_free(ahead);json_free(handoff);json_free(lane);
    json_free(text);json_free(thread_id);
    if(exc) return exc;
    return json_null();
}

/* PoP: sa10_handle_slack_message @ plugins/platforms/slack/adapter.py:_handle_slack_message */
static json_t *sa10_handle_slack_message(sa10_ctx_t *ctx, const json_t *args) {
    const json_t *event = json_obj_get(args, "event");
    char *ts=sa10_strval(json_obj_get(event,"ts"));
    bool was_claimed=false;
    if(ts&&ts[0]&&sa10_shas(ctx,"_processed_message_ts",ts)) was_claimed=true;
    json_t *a=sa10_o();sa10_sc(a,"event",event);sa10_sc(a,"payload",json_obj_get(args,"payload"));
    json_t *ret=sa10_call(ctx,"_handle_slack_message_impl",a);
    if(sa10_is_raise(ret)){
        if(ts&&ts[0]&&!was_claimed&&sa10_shas(ctx,"_processed_message_ts",ts)){
            sa10_sdel(ctx,"_processed_message_ts",ts);
            json_t *w=sa10_o();sa10_ss(w,"name",sa10_s(json_obj_get(args,"name"),""));sa10_ss(w,"ts",ts);
            json_t *wr=sa10_call(ctx,"logger.warning",w); if(sa10_is_raise(wr)){json_free(wr);}else json_free(wr);
        }
    }
    free(ts);
    return ret;
}

/* PoP: sa10_handle_slack_message_impl @ plugins/platforms/slack/adapter.py:_handle_slack_message_impl */
static json_t *sa10_handle_slack_message_impl(sa10_ctx_t *ctx, const json_t *args) {
    json_t *agent_ctx=NULL,*team_final=NULL,*meta=NULL,*thread_ts=NULL;
    json_t *text=NULL,*probe=NULL,*team_v=NULL,*chan_v=NULL,*acc=NULL;
    char *orig=NULL;
    const json_t *event=json_obj_get(args,"event");
    json_t *a=sa10_o();sa10_sc(a,"event",event);sa10_sc(a,"payload",json_obj_get(args,"payload"));
    acc=sa10_call(ctx,"prefilter_inbound",a);
    if(sa10_is_raise(acc)) return acc;
    if(!acc||acc->type!=JSON_ARRAY||json_len(acc)<3){json_free(acc);return json_null();}
    event=json_copy(sa10_arr(acc,0));
    team_v=sa10_arr(acc,1); chan_v=sa10_arr(acc,2);
    const char *channel_id=sa10_s(chan_v,"");
    const char *dedup_team=sa10_s(team_v,"");
    orig=sa10_strval(json_obj_get(event,"text"));
    /* lstrip + known-bang rewrite */
    char *lstr=sa10_lstrip_char(orig?orig:"",' ');
    json_t *qa=sa10_o();sa10_ss(qa,"text",lstr?lstr:"");
    probe=sa10_call(ctx,"rewrite_known_bang_command",qa);
    if(sa10_is_raise(probe)){json_free(probe);free(lstr);free(orig);json_free(team_v);json_free(chan_v);json_free(event);json_free(acc);return NULL;}
    const char *ps=sa10_s(probe,"");
    bool changed=strcmp(ps,lstr?lstr:"")!=0;
    free(lstr);
    if(changed){free(orig);orig=sa10_strdup(ps);}
    bool is_command=sa10_startswith(ps,"/");
    text=json_string(orig?orig:"");
    const json_t *blocks=json_obj_get(event,"blocks");
    if(blocks&&sa10_truthy(blocks)&&!is_command){
        json_t *ba=sa10_o();sa10_ss(ba,"text",orig?orig:"");sa10_sc(ba,"blocks",blocks);
        json_t *bt=sa10_call(ctx,"append_block_text",ba);
        if(sa10_is_raise(bt)){json_free(bt);json_free(text);free(orig);json_free(probe);json_free(team_v);json_free(chan_v);json_free(event);json_free(acc);return NULL;}
        json_free(text);text=bt;
    }
    json_t *fa=sa10_o();sa10_sc(fa,"text",text);sa10_sc(fa,"attachments",json_obj_get(event,"attachments"));
    json_t *fw=sa10_call(ctx,"append_forwards",fa);
    if(sa10_is_raise(fw)){json_free(fw);json_free(text);free(orig);json_free(probe);json_free(team_v);json_free(chan_v);json_free(event);json_free(acc);return NULL;}
    json_free(text);text=fw;
    json_t *la=sa10_o();sa10_sc(la,"text",text);sa10_sc(la,"attachments",json_obj_get(event,"attachments"));
    json_t *lu=sa10_call(ctx,"append_link_unfurls",la);
    if(sa10_is_raise(lu)){json_free(lu);json_free(text);free(orig);json_free(probe);json_free(team_v);json_free(chan_v);json_free(event);json_free(acc);return NULL;}
    json_free(text);text=lu;
    const char *ts=sa10_s(json_obj_get(event,"ts"),"");
    json_t *ma=sa10_o();sa10_sc(ma,"event",event);sa10_ss(ma,"channel_id",channel_id);
    sa10_sc(ma,"thread_ts",json_obj_get(event,"thread_ts"));sa10_sc(ma,"team_id",team_v);sa10_sc(ma,"body",json_obj_get(args,"payload"));
    meta=sa10_call(ctx,"lookup_assistant_thread_metadata",ma);
    if(sa10_is_raise(meta)){json_free(meta);json_free(text);free(orig);json_free(probe);json_free(team_v);json_free(chan_v);json_free(event);json_free(acc);return NULL;}
    json_t *uid_v=json_obj_get(event,"user");
    if(!uid_v||!sa10_truthy(uid_v)) uid_v=json_obj_get(meta,"user_id");
    const char *user_id=sa10_s(uid_v,"");
    if(!channel_id[0]) channel_id=sa10_s(json_obj_get(meta,"channel_id"),"");
    /* team_id resolution */
    team_final=json_copy(team_v);
    if(!sa10_truthy(team_final)){
        json_t *ta=sa10_o();sa10_sc(ta,"team",json_obj_get(meta,"team_id"));
        json_t *r=sa10_call(ctx,"team_in_install_set",ta);
        if(sa10_is_raise(r)){json_free(r);goto impl_fail;}
        if(sa10_truthy(r)){json_free(team_final);team_final=r;}else{
            json_free(r);
            const json_t *cv=sa10_sget(ctx,"_channel_team",channel_id,json_null());
            json_t *ta2=sa10_o();sa10_sc(ta2,"team",cv);json_free(cv);
            json_t *r2=sa10_call(ctx,"team_in_install_set",ta2);
            if(sa10_is_raise(r2)){json_free(r2);goto impl_fail;}
            json_free(team_final);team_final=r2;
        }
    }
    /* agent context */
    json_t *ca=sa10_o();sa10_sc(ca,"event",event);
    sa10_ss(ca,"team",sa10_s(team_final,""));sa10_ss(ca,"user",user_id?user_id:"");
    agent_ctx=sa10_call(ctx,"agent_view_context_for_event",ca);
    if(sa10_is_raise(agent_ctx)){json_free(agent_ctx);goto impl_fail;}
    if(sa10_truthy(team_final)&&channel_id[0]){
        json_t *ra2=sa10_o();sa10_ss(ra2,"channel_id",channel_id);sa10_sc(ra2,"team_id",team_final);
        sa10_call(ctx,"remember_channel_team",ra2);
    }
    const char *ctype=sa10_s(json_obj_get(event,"channel_type"),"");
    if(!ctype[0]) ctype=sa10_startswith(channel_id,"D")?"im":"";
    bool is_dm=(strcmp(ctype,"im")==0||strcmp(ctype,"mpim")==0);
    if(is_dm){
        json_t *d=sa10_call(ctx,"slack_disable_dms",sa10_o());
        if(sa10_is_raise(d)){json_free(d);goto impl_fail;}
        bool dis=sa10_truthy(d);json_free(d);
        if(dis){goto impl_ok_done;}
    }
    bool one_to_one=(strcmp(ctype,"im")==0);
    json_t *sta=sa10_o();sa10_sc(sta,"event",event);sa10_ss(sta,"ts",ts);sa10_sb(sta,"is_dm",is_dm);
    sa10_sc(sta,"assistant_meta",meta);
    thread_ts=sa10_call(ctx,"session_thread_ts",sta);
    if(sa10_is_raise(thread_ts)){json_free(thread_ts);goto impl_fail;}
    const char *tts=sa10_s(thread_ts,"");
    json_t *eda=sa10_o();sa10_sc(eda,"event",event);
    json_t *decl=sa10_call(ctx,"event_declares_bot_sender",eda);
    if(sa10_is_raise(decl)){json_free(decl);goto impl_fail;}
    bool sender_bot=sa10_truthy(decl);json_free(decl);
    if(!sender_bot&&user_id[0]&&!sa10_truthy(json_obj_get(event,"client_msg_id"))){
        json_t *ua=sa10_o();sa10_ss(ua,"user",user_id);sa10_ss(ua,"chat_id",channel_id);sa10_sc(ua,"team_id",team_final);
        json_t *r=sa10_call(ctx,"resolve_user_is_bot",ua);
        if(sa10_is_raise(r)){json_free(r);goto impl_fail;}
        sender_bot=sa10_truthy(r);json_free(r);
    }
    json_t *era=sa10_o();sa10_ss(era,"user_id",user_id);sa10_ss(era,"channel_id",channel_id);
    sa10_sb(era,"is_dm",is_dm);sa10_sc(era,"thread_id",thread_ts);sa10_sb(era,"is_bot",sender_bot);
    json_t *rej=sa10_call(ctx,"early_reject_unauthorized",era);
    if(sa10_is_raise(rej)){json_free(rej);goto impl_fail;}
    bool rejected=sa10_truthy(rej);json_free(rej);
    if(rejected) goto impl_ok_done;
    json_t *bua=sa10_o();sa10_sc(bua,"team_id",team_final);
    json_t *bot=sa10_call(ctx,"team_bot_user_id",bua);
    if(sa10_is_raise(bot)){json_free(bot);goto impl_fail;}
    const char *bot_uid=sa10_s(bot,"");
    json_t *mda=sa10_o();sa10_sc(mda,"event",event);
    json_t *det=sa10_call(ctx,"slack_mention_detection_text",mda);
    if(sa10_is_raise(det)){json_free(det);json_free(bot);goto impl_fail;}
    const char *routing=sa10_s(det,"");
    if(!routing[0]) routing=orig?orig:"";
    json_free(det);
    json_t *ia=sa10_o();sa10_ss(ia,"routing_text",routing);sa10_ss(ia,"bot_uid",bot_uid);
    json_t *ment=sa10_call(ctx,"is_mentioned",ia);
    if(sa10_is_raise(ment)){json_free(ment);json_free(bot);goto impl_fail;}
    bool is_mentioned=sa10_truthy(ment);json_free(ment);
    const json_t *etts=json_obj_get(event,"thread_ts");
    bool is_thread_reply=(sa10_truthy(etts)&&strcmp(sa10_s(etts,""),ts)!=0);
    bool force=sa10_truthy(json_obj_get(event,"_hermes_force_process"));
    json_t *pda=sa10_o();sa10_sc(pda,"event",event);sa10_ss(pda,"user_id",user_id);sa10_ss(pda,"bot_uid",bot_uid);
    sa10_ss(pda,"channel_id",channel_id);sa10_sc(pda,"team_id",team_final);sa10_sb(pda,"is_mentioned",is_mentioned);
    json_t *drop=sa10_call(ctx,"peer_bot_drop",pda);
    if(sa10_is_raise(drop)){json_free(drop);json_free(bot);goto impl_fail;}
    bool do_drop=sa10_truthy(drop);json_free(drop);
    if(do_drop){json_free(bot);goto impl_ok_done;}
    if(!one_to_one&&bot_uid[0]){
        json_t *ga=sa10_o();sa10_ss(ga,"channel_id",channel_id);sa10_ss(ga,"routing_text",routing);
        sa10_ss(ga,"bot_uid",bot_uid);sa10_sb(ga,"is_mentioned",is_mentioned);sa10_sb(ga,"is_thread_reply",is_thread_reply);
        sa10_sc(ga,"event_thread_ts",json_obj_get(event,"thread_ts"));sa10_ss(ga,"user_id",user_id);
        sa10_sc(ga,"team_id",team_final);sa10_sb(ga,"is_dm",is_dm);sa10_sb(ga,"force_process",force);
        json_t *gate=sa10_call(ctx,"channel_gate_allows",ga);
        if(sa10_is_raise(gate)){json_free(gate);json_free(bot);goto impl_fail;}
        bool allow=sa10_truthy(gate);json_free(gate);
        if(!allow){json_free(bot);goto impl_ok_done;}
    }
    json_free(bot);
    /* claim ts */
    if(ts[0]) sa10_sset(ctx,"_processed_message_ts",ts,json_bool(true));
    if(is_mentioned){
        json_t *ba=sa10_o();sa10_sc(ba,"text",text);sa10_ss(ba,"original_text",orig?orig:"");
        sa10_ss(ba,"command_probe_text",ps);sa10_sb(ba,"is_command_text",is_command);
        sa10_ss(ba,"bot_uid","");sa10_ss(ba,"thread_ts",tts);sa10_sc(ba,"team_id",team_final);
        json_t *ar=sa10_call(ctx,"apply_bot_mention",ba);
        if(sa10_is_raise(ar)){json_free(ar);goto impl_fail;}
        if(ar&&ar->type==JSON_ARRAY&&json_len(ar)>=4){
            json_free(text);text=json_copy(sa10_arr(ar,0));free(orig);orig=sa10_strval(sa10_arr(ar,1));
        }
        json_free(ar);
    }
    json_t *ha=sa10_o();sa10_ss(ha,"channel_id",channel_id);
    sa10_sc(ha,"event_thread_ts",json_obj_get(event,"thread_ts"));sa10_ss(ha,"ts",ts);
    sa10_ss(ha,"user_id",user_id);sa10_sc(ha,"team_id",team_final);
    sa10_sb(ha,"is_thread_reply",is_thread_reply);sa10_sb(ha,"is_mentioned",is_mentioned);sa10_sb(ha,"is_dm",is_dm);
    json_t *hctx=sa10_call(ctx,"hydrate_thread_context",ha);
    if(sa10_is_raise(hctx)){json_free(hctx);goto impl_fail;}
    json_t *ma2=sa10_o();sa10_sc(ma2,"event",event);sa10_ss(ma2,"channel_id",channel_id);
    sa10_sc(ma2,"team_id",team_final);sa10_sc(ma2,"text",text);
    sa10_sc(ma2,"root_urls",sa10_arr(hctx,1));sa10_sc(ma2,"root_types",sa10_arr(hctx,2));
    json_free(hctx);
    json_t *cm=sa10_call(ctx,"collect_inbound_media",ma2);
    if(sa10_is_raise(cm)){json_free(cm);goto impl_fail;}
    json_t *rea=sa10_o();sa10_ss(rea,"routing_text",routing);sa10_ss(rea,"bot_uid","");
    sa10_sc(rea,"channel_id",json_string(channel_id));sa10_sb(rea,"opens_own_session",strcmp(tts,ts)==0);
    sa10_sb(rea,"channel_thread_followup",is_thread_reply&&!is_dm);
    sa10_sb(rea,"addressed",one_to_one||is_mentioned||is_command||force);
    json_t *rexp=sa10_call(ctx,"slack_reply_expected",rea);
    if(sa10_is_raise(rexp)){json_free(rexp);json_free(cm);goto impl_fail;}
    json_t *be=sa10_o();sa10_sc(be,"event",event);sa10_sc(be,"text",text);
    sa10_ss(be,"original_text",orig?orig:"");sa10_sb(be,"is_command_text",is_command);
    sa10_ss(be,"channel_id",channel_id);sa10_sc(be,"team_id",team_final);sa10_ss(be,"ts",ts);
    sa10_ss(be,"user_id",user_id);sa10_ss(be,"thread_ts",tts);sa10_sb(be,"is_dm",is_dm);
    sa10_sc(be,"reply_expected",rexp);
    json_t *msg_event=sa10_call(ctx,"build_message_event",be);
    if(sa10_is_raise(msg_event)){json_free(msg_event);json_free(cm);goto impl_fail;}
    if((one_to_one||is_mentioned)){
        json_t *re=sa10_call(ctx,"reactions_enabled",sa10_o());
        if(sa10_is_raise(re)){json_free(re);json_free(msg_event);json_free(cm);goto impl_fail;}
        bool en=sa10_truthy(re);json_free(re);
        if(en){
            json_t *ta2=sa10_o();sa10_sc(ta2,"team_id",team_final);sa10_ss(ta2,"ts",ts);
            json_t *tr=sa10_call(ctx,"track_reacting_message",ta2);
            if(sa10_is_raise(tr)){json_free(tr);json_free(msg_event);json_free(cm);goto impl_fail;}json_free(tr);
        }
    }
    const char *cxid=sa10_s(json_obj_get(agent_ctx,"context_channel_id"),"");
    if(cxid[0]&&strcmp(cxid,channel_id)!=0&&!is_command){
        const char *mt=sa10_s(json_obj_get(msg_event,"text"),"");
        char full[4096];snprintf(full,sizeof full,"[Slack app context: user is viewing channel %s]\n\n%s",cxid,mt);
        json_t *sa=sa10_o();sa10_ss(sa,"text",full);
        json_t *sr=sa10_call(ctx,"msg_event_set_text",sa);
        if(sa10_is_raise(sr)){json_free(sr);json_free(msg_event);json_free(cm);goto impl_fail;}json_free(sr);
    }
    if(ts[0]) sa10_sset(ctx,"_processed_message_ts",ts,json_bool(true));
    {
        json_t *ha2=sa10_o();json_set(ha2,"msg_event",json_copy(msg_event));
        json_t *hr=sa10_call(ctx,"handle_message",ha2);
        if(sa10_is_raise(hr)){json_free(hr);json_free(msg_event);json_free(cm);goto impl_fail;}json_free(hr);
    }
    json_free(msg_event);json_free(cm);json_free(rexp);
impl_ok_done:
    json_free(agent_ctx);json_free(team_final);json_free(meta);json_free(thread_ts);
    free(orig);json_free(text);json_free(probe);json_free(team_v);json_free(chan_v);json_free(event);json_free(acc);
    return json_null();
impl_fail:
    json_free(agent_ctx);json_free(team_final);json_free(meta);json_free(thread_ts);
    free(orig);json_free(text);json_free(probe);json_free(team_v);json_free(chan_v);json_free(event);json_free(acc);
    return NULL;
}
/* PoP: sa10_handle_slack_file_shared @ plugins/platforms/slack/adapter.py:_handle_slack_file_shared */
static json_t *sa10_handle_slack_file_shared(sa10_ctx_t *ctx, const json_t *args) {
    const json_t *event=json_obj_get(args,"event");
    const json_t *cv=json_obj_get(event,"channel_id");
    const char *channel_id=sa10_truthy(cv)?sa10_s(cv,""):sa10_s(json_obj_get(event,"channel"),"");
    json_t *ia=sa10_o();sa10_ss(ia,"channel_id",channel_id);
    json_t *ign=sa10_call(ctx,"is_ignored_channel",ia);
    if(sa10_is_raise(ign)) return ign;
    bool ignored=sa10_truthy(ign);json_free(ign);
    if(ignored){
        json_t *l=sa10_o();sa10_ss(l,"channel_id",channel_id);
        json_t *lr=sa10_call(ctx,"logger.info",l);if(sa10_is_raise(lr)){json_free(lr);return NULL;}json_free(lr);
        return json_null();
    }
    const json_t *fv=json_obj_get(event,"file_id");
    const char *file_id=sa10_truthy(fv)?sa10_s(fv,""):sa10_s(json_obj_get(json_obj_get(event,"file"),"id"),"");
    if(!channel_id[0]||!file_id[0]) return json_null();
    json_t *ea=sa10_o();sa10_sc(ea,"event",event);sa10_sc(ea,"body",json_obj_get(args,"body"));
    sa10_sc(ea,"install_set",json_obj_get(args,"install_set"));
    json_t *team_v=sa10_call(ctx,"event_team_id",ea);
    if(sa10_is_raise(team_v)) return team_v;
    json_t *fa=sa10_o();sa10_ss(fa,"file",file_id);sa10_sc(fa,"team_id",team_v);
    json_t *info=sa10_call(ctx,"files_info",fa);
    if(sa10_is_raise(info)){
        if(sa10_raise_passes(info)){json_free(team_v);return info;}
        json_t *da=sa10_o();sa10_sc(da,"response",json_null());
        json_t *fo=sa10_o();sa10_ss(fo,"id",file_id);json_set(da,"file_obj",fo);
        json_t *det=sa10_call(ctx,"describe_slack_api_error",da);
        if(sa10_is_raise(det)){json_free(det);json_free(info);json_free(team_v);return NULL;}
        json_t *w=sa10_o();sa10_ss(w,"file_id",file_id);sa10_sc(w,"detail",det);
        json_t *wr=sa10_call(ctx,"logger.warning",w);if(sa10_is_raise(wr)){json_free(wr);json_free(info);json_free(team_v);return NULL;}json_free(wr);
        json_free(info);json_free(team_v);return json_null();
    }
    if(!sa10_truthy(json_obj_get(info,"ok"))){
        json_t *da=sa10_o();sa10_sc(da,"response",info);
        json_t *fo=sa10_o();sa10_ss(fo,"id",file_id);json_set(da,"file_obj",fo);
        json_t *det=sa10_call(ctx,"describe_slack_api_error",da);
        if(sa10_is_raise(det)){json_free(det);json_free(info);json_free(team_v);return NULL;}
        json_t *w=sa10_o();sa10_ss(w,"file_id",file_id);sa10_sc(w,"detail",det);
        sa10_sc(w,"error",json_obj_get(info,"error"));
        json_t *wr=sa10_call(ctx,"logger.warning",w);if(sa10_is_raise(wr)){json_free(wr);json_free(info);json_free(team_v);return NULL;}json_free(wr);
        json_free(info);json_free(team_v);return json_null();
    }
    const json_t *file_obj=json_obj_get(info,"file");
    if(!file_obj||file_obj->type!=JSON_OBJECT) file_obj=NULL;
    const char *mime=file_obj?sa10_s(json_obj_get(file_obj,"mimetype"),""):"";
    if(!sa10_startswith(mime,"video/")){json_free(info);json_free(team_v);return json_null();}
    bool is_dm=sa10_startswith(channel_id,"D")||sa10_json_eq(json_obj_get(event,"channel_type"),"im");
    if(is_dm){
        const json_t *sh=sa10_truthy(json_obj_get(event,"user_id"))?json_obj_get(event,"user_id"):(file_obj?json_obj_get(file_obj,"user"):NULL);
        const char *sharer=sa10_s(sh,"");
        const char *sender_team=sa10_s(json_obj_get(event,"user_team"),"");
        if(!(sender_team[0]=='T')){
            json_t *ra=sa10_o();sa10_ss(ra,"sharer",sharer);sa10_ss(ra,"channel_id",channel_id);sa10_sc(ra,"team_id",team_v);
            json_t *st=sa10_call(ctx,"resolve_author_team",ra);
            if(sa10_is_raise(st)){if(sa10_raise_passes(st)){json_free(st);json_free(info);json_free(team_v);return NULL;}json_free(st);}
            else {sender_team=sa10_strdup(sa10_s(st,""));json_free(st);}
        }
        if(!sender_team[0]){
            const char *a=sa10_s(json_obj_get(event,"team"),"");
            const char *b=file_obj?sa10_s(json_obj_get(file_obj,"user_team"),""):"";
            sender_team=sa10_first_T(a,b);
        }
        json_t *fa2=sa10_o();sa10_ss(fa2,"sender_team",sender_team);sa10_sc(fa2,"team_id",team_v);
        json_t *fr=sa10_call(ctx,"is_foreign_author",fa2);
        if(sa10_is_raise(fr)){json_free(fr);json_free(info);json_free(team_v);return NULL;}
        bool foreign=sa10_truthy(fr);json_free(fr);
        if(foreign){
            json_t *l=sa10_o();sa10_ss(l,"sharer",sharer);sa10_ss(l,"sender_team",sender_team);sa10_ss(l,"channel_id",channel_id);
            json_t *lr=sa10_call(ctx,"logger.info",l);if(sa10_is_raise(lr)){json_free(lr);json_free(info);json_free(team_v);return NULL;}json_free(lr);
            json_free(info);json_free(team_v);return json_null();
        }
    }
    json_t *sa=sa10_o();sa10_sc(sa,"file_obj",file_obj?file_obj:json_null());sa10_ss(sa,"channel_id",channel_id);
    json_t *share=sa10_call(ctx,"first_file_share",sa);
    if(sa10_is_raise(share)){json_free(share);json_free(info);json_free(team_v);return NULL;}
    const char *ts=sa10_s(json_obj_get(share,"ts"),"");
    if(!ts[0]) ts=sa10_s(json_obj_get(event,"event_ts"),"");
    const char *thread_ts=sa10_s(json_obj_get(share,"thread_ts"),"");
    json_t *sl=sa10_o();sa10_sn(sl,"seconds",0.75);
    json_t *sr=sa10_call(ctx,"sleep",sl);
    if(sa10_is_raise(sr)){json_free(sr);json_free(share);json_free(info);json_free(team_v);return NULL;}json_free(sr);
    if(ts[0]){
        json_t *wa=sa10_o();sa10_ss(wa,"team_id",sa10_s(team_v,""));sa10_ss(wa,"ts",ts);
        json_t *wid=sa10_call(ctx,"workspace_event_id",wa);
        if(sa10_is_raise(wid)){json_free(wid);json_free(share);json_free(info);json_free(team_v);return NULL;}
        json_t *da=sa10_o();json_set(da,"id",wid);
        json_t *dup=sa10_call(ctx,"dedup_is_duplicate",da);
        if(sa10_is_raise(dup)){json_free(dup);json_free(share);json_free(info);json_free(team_v);return NULL;}
        bool d=sa10_truthy(dup);json_free(dup);
        if(d){json_free(share);json_free(info);json_free(team_v);return json_null();}
    }
    json_t *fe=sa10_o();
    sa10_ss(fe,"type","message");sa10_ss(fe,"subtype","file_share");sa10_ss(fe,"text","");
    sa10_sc(fe,"user",sa10_truthy(json_obj_get(event,"user_id"))?json_obj_get(event,"user_id"):(file_obj?json_obj_get(file_obj,"user"):json_null()));
    sa10_ss(fe,"channel",channel_id);
    sa10_ss(fe,"channel_type",sa10_startswith(channel_id,"D")?"im":"channel");
    sa10_sc(fe,"team",team_v);sa10_ss(fe,"ts","");
    json_t *files=json_array();if(file_obj) json_append(files,json_copy(file_obj));json_set(fe,"files",files);
    if(thread_ts[0]&&strcmp(thread_ts,ts)!=0) sa10_ss(fe,"thread_ts",thread_ts);
    json_t *ha=sa10_o();json_set(ha,"event",fe);
    json_t *hr=sa10_call(ctx,"handle_slack_message",ha);
    if(sa10_is_raise(hr)){json_free(hr);}
    else json_free(hr);
    json_free(share);json_free(info);json_free(team_v);
    return json_null();
}

/* PoP: sa10_handle_slack_reaction @ plugins/platforms/slack/adapter.py:_handle_slack_reaction */
static json_t *sa10_handle_slack_reaction(sa10_ctx_t *ctx, const json_t *args) {
    const json_t *event=json_obj_get(args,"event");
    const json_t *item=json_obj_get(event,"item");
    if(!sa10_json_eq(json_obj_get(item,"type"),"message")) return json_null();
    const json_t *cidv=json_obj_get(item,"channel"),*mtsv=json_obj_get(item,"ts");
    const json_t *uidv=json_obj_get(event,"user"),*rv=json_obj_get(event,"reaction");
    if(!sa10_truthy(cidv)||!sa10_truthy(mtsv)||!sa10_truthy(uidv)||!sa10_truthy(rv)) return json_null();
    const char *channel_id=sa10_s(cidv,""),*msg_ts=sa10_s(mtsv,"");
    const char *reaction=sa10_s(rv,""),*user_id=sa10_s(uidv,"");
    const char *bot_uid=sa10_s(json_obj_get(args,"bot_user_id"),"");
    if(bot_uid[0]&&strcmp(user_id,bot_uid)==0) return json_null();
    json_t *ta=sa10_o();sa10_sc(ta,"channel_id",cidv);
    json_t *team_v=sa10_call(ctx,"channel_team",ta);
    if(sa10_is_raise(team_v)) return team_v;
    if(!sa10_truthy(team_v)){
        json_t *tc=sa10_call(ctx,"team_clients",sa10_o());
        if(sa10_is_raise(tc)){json_free(tc);json_free(team_v);return NULL;}
        bool has=sa10_truthy(tc);json_free(tc);
        if(has){
            json_t *first=sa10_call(ctx,"team_clients_first",sa10_o());
            if(sa10_is_raise(first)){json_free(first);json_free(team_v);return NULL;}
            json_free(team_v);team_v=first;
        }
    }
    const char *team_id=sa10_s(team_v,"");
    json_t *ca=sa10_o();sa10_sc(ca,"team_id",team_v);
    json_t *client=sa10_call(ctx,"team_clients_get",ca);
    if(sa10_is_raise(client)){json_free(client);json_free(team_v);return NULL;}
    const char *action=sa10_gb(args,"removed",false)?"removed":"added";
    json_t *handler=sa10_call(ctx,"reaction_handler_get",sa10_o());
    if(sa10_is_raise(handler)){json_free(handler);json_free(client);json_free(team_v);return NULL;}
    if(!sa10_is_raise(handler)&&handler&&handler->type!=JSON_NULL){
        json_t *hd=sa10_o();sa10_ss(hd,"platform","slack");
        char en[64];snprintf(en,sizeof en,"reaction:%s",action);sa10_ss(hd,"event_name",en);
        sa10_ss(hd,"reaction",reaction);sa10_ss(hd,"user_id",user_id);
        sa10_sc(hd,"item_user_id",json_obj_get(event,"item_user"));sa10_sc(hd,"item_type",json_obj_get(item,"type"));
        sa10_ss(hd,"channel_id",channel_id);sa10_ss(hd,"message_ts",msg_ts);sa10_ss(hd,"team_id",team_id);
        sa10_sc(hd,"event_ts",json_obj_get(event,"event_ts"));sa10_sc(hd,"raw_event",event);
        json_t *hr=sa10_call(ctx,"reaction_handler_call",hd);
        if(sa10_is_raise(hr)){
            if(sa10_raise_passes(hr)){json_free(hr);json_free(handler);json_free(client);json_free(team_v);return NULL;}
            json_free(hr);
            json_t *dr=sa10_call(ctx,"logger.debug",sa10_o());if(sa10_is_raise(dr)){json_free(dr);}else json_free(dr);
        } else json_free(hr);
    }
    json_free(handler);
    json_t *trig=sa10_call(ctx,"slack_reaction_triggers",sa10_o());
    if(sa10_is_raise(trig)){json_free(trig);json_free(client);json_free(team_v);return NULL;}
    if(!trig||trig->type==JSON_NULL){json_free(trig);json_free(client);json_free(team_v);return json_null();}
    bool explicit=sa10_truthy(trig);
    if(explicit){
        char *rstrip=sa10_strip_chars(reaction,":");
        bool inlist=sa10_collection_has_str(trig,rstrip?rstrip:"");
        free(rstrip);
        if(!inlist){json_free(trig);json_free(client);json_free(team_v);return json_null();}
    }
    json_free(trig);
    json_t *ra2=sa10_o();sa10_sc(ra2,"client",client);sa10_ss(ra2,"channel_id",channel_id);
    sa10_ss(ra2,"msg_ts",msg_ts);sa10_sc(ra2,"event",event);sa10_sc(ra2,"team_id",team_v);
    sa10_sb(ra2,"explicit_allowlist",explicit);
    json_t *thread_ts=sa10_call(ctx,"reaction_thread_ts",ra2);
    if(sa10_is_raise(thread_ts)){json_free(thread_ts);json_free(client);json_free(team_v);return NULL;}
    if(!thread_ts||thread_ts->type==JSON_NULL){json_free(thread_ts);json_free(client);json_free(team_v);return json_null();}
    json_t *sa=sa10_o();sa10_sc(sa,"event",event);sa10_ss(sa,"action",action);
    sa10_sc(sa,"thread_ts",thread_ts);sa10_sc(sa,"team_id",team_v);
    json_t *syn=sa10_call(ctx,"synthetic_reaction_event",sa);
    if(sa10_is_raise(syn)){json_free(syn);json_free(thread_ts);json_free(client);json_free(team_v);return NULL;}
    if(sa10_startswith(channel_id,"D")||sa10_startswith(sa10_s(json_obj_get(syn,"channel"),""),"D")){
        json_t *aa=sa10_o();sa10_ss(aa,"user_id",user_id);sa10_ss(aa,"channel_id",channel_id);sa10_sc(aa,"team_id",team_v);
        json_t *rt=sa10_call(ctx,"resolve_author_team",aa);
        if(sa10_is_raise(rt)){if(!sa10_raise_passes(rt))json_free(rt);json_free(rt);json_free(syn);json_free(thread_ts);json_free(client);json_free(team_v);return NULL;}
        if(sa10_truthy(rt)){
            json_t *ua=sa10_o();sa10_sc(ua,"user_team",rt);
            json_t *ur=sa10_call(ctx,"synthetic_set",ua);if(sa10_is_raise(ur)){json_free(ur);}else json_free(ur);
        }
        json_free(rt);
    }
    json_t *ha=sa10_o();json_set(ha,"event",json_copy(syn));
    json_t *hr=sa10_call(ctx,"handle_slack_message",ha);
    if(sa10_is_raise(hr)){json_free(hr);}else json_free(hr);
    json_free(syn);json_free(thread_ts);json_free(client);json_free(team_v);
    return json_null();
}
/* PART2-CONT */

