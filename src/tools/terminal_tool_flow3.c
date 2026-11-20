/*
 * terminal_tool_flow3.c — tools/terminal_tool.py UNCLAIMED-remainder FLOW cores
 * (universal-seam convention, sibling of terminal_tool_flow2.c).
 *
 * Ports 12 decision sequences the build-registered PoP scan still reports as
 * REAL_GAPs: the docker-orphan reaper gate (_maybe_reap_docker_orphans), the
 * container-alias registry writer (register_container_alias), the isolation
 * override predicate (_has_isolation_overrides), the three _SessionScope
 * derived properties (session_isolated / docker_session_isolated /
 * docker_profile_scoped), the docker-isolation probe
 * (_docker_session_isolation_enabled), the PTY-stdin guard
 * (_command_requires_pipe_stdin), the notification-flag conflict resolver
 * (_resolve_notification_flag_conflict), the per-command cwd resolver
 * (_resolve_command_cwd), the missing-workdir refusal
 * (_refuse_missing_local_workdir) and the host-cwd mount-source policy
 * (_resolve_task_host_cwd).
 *
 * Call order, short-circuit branches, raise->fallback paths and state mutation
 * order follow the live Python. Every live I/O (env probes, module imports,
 * config lookups, clocks, logging, claimed siblings, foreign leaves) enters
 * through tt3_seams_t.call(ctx, name, args); args are OWNED by tt3_call (freed
 * after the borrowed call); results are NEW json_t with NULL == Python None
 * and {"__raise__":K,"msg":M} == a raised exception. Full builtin exception
 * hierarchy: SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError and
 * their groups stay outside Exception; every other builtin walks to Exception.
 *
 * Self-contained: libjson only. All helpers static tt3_*.
 *
 * Modeling notes (differential oracle tests/oracle_terminal_tool_flow3.py):
 *  - Module state is injected json_t (in.state) mutated in place and echoed as
 *    out.state with the full key set: docker_orphan_reaper_ran (bool),
 *    container_aliases (object), task_env_overrides (object).
 *  - Claimed siblings / foreign leaves are seams (see the oracle's install_globals).
 *  - str.split()/lower() are transcribed over Unicode White_Space; the ASCII
 *    lowercase fast path covers the fixture command domain. Embedded NUL in a
 *    cwd string and non-string dict keys are outside the fixture domain.
 *  - int() of a seam result is restricted to decimal strings, numbers and
 *    bools; invalid-token ValueError/TypeError messages are raised by the
 *    scripted seam itself (the port does not synthesize Python int() messages).
 *  - _quiet is the real contextmanager from terminal_tool_config.py: on an
 *    Exception it fires logger.log(level=10, fmt=<label>, exc_info=True) and
 *    suppresses; BaseException passes through untouched.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdarg.h>
#include "libjson/json.h"

typedef struct tt3_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} tt3_seams_t;

/* ── universal-seam helpers ── */
static json_t *tt3_call(const tt3_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool tt3_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *tt3_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static const char *tt3_emsg(const json_t *r) {
    const json_t *k = json_obj_get(r, "msg");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static json_t *tt3_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *tt3_args(void) { return json_object(); }
static void tt3_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tt3_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tt3_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void tt3_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *tt3_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* ── exception-parent table (FULL builtin hierarchy) ── */
typedef struct { const char *kind; const char *parent; } tt3_exc_ent;
static const tt3_exc_ent TT3_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"AttributeError", "Exception"},
    {"NameError", "Exception"},
    {"LookupError", "Exception"},
    {"ArithmeticError", "Exception"},
    {"OSError", "Exception"},
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"AssertionError", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"ImportError", "Exception"},
    {"Warning", "Exception"},
    {"RuntimeError", "Exception"},
    {"MemoryError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ReferenceError", "Exception"},
    {"ExceptionGroup", "Exception"},
    {"_Rejected", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FileNotFoundError", "OSError"},
    {"PermissionError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"InterruptedError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ConnectionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"FileExistsError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UserWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {NULL, NULL},
};
static const char *tt3_exc_parent(const char *kind) {
    for (const tt3_exc_ent *e = TT3_EXC_PARENTS; e->kind; e++)
        if (strcmp(e->kind, kind) == 0) return e->parent;
    if (strcmp(kind, "BaseException") == 0) return NULL;
    return "Exception";   /* oracle make_exc: unknown -> Exception subclass */
}
static bool tt3_exc_is(const char *kind, const char *base) {
    const char *k = kind;
    for (int depth = 0; k && depth < 32; depth++) {
        if (strcmp(k, base) == 0) return true;
        k = tt3_exc_parent(k);
    }
    return false;
}

/* ── Python value helpers ── */
static bool tt3_truth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool tt3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *tt3_strval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool tt3_streq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool tt3_startswith(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
static bool tt3_endswith(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}
/* str() of a scalar. Containers outside the str() fixture domain. */
static char *tt3_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isinf(d) && !isnan(d) && d == (double)(long long)d) {
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
            return strdup(buf);
        }
        snprintf(buf, sizeof(buf), "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}
static char *tt3_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return strdup("");
    char *buf = malloc((size_t)n + 1);
    if (!buf) return strdup("");
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}
/* Python White_Space predicate over a decoded codepoint. */
static bool tt3_ws_cp(uint32_t cp) {
    return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20 || cp == 0x85 || cp == 0xA0
        || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)
        || cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
/* Decode one UTF-8 codepoint; advance *i by its byte length. */
static uint32_t tt3_next_cp(const char *s, size_t n, size_t *i) {
    const unsigned char *p = (const unsigned char *)s;
    unsigned char c = p[*i];
    if (c < 0x80) { (*i) += 1; return c; }
    if ((c & 0xE0) == 0xC0 && *i + 1 < n && (p[*i+1] & 0xC0) == 0x80) {
        uint32_t v = ((uint32_t)(c & 0x1F) << 6) | (p[*i+1] & 0x3F); *i += 2; return v;
    }
    if ((c & 0xF0) == 0xE0 && *i + 2 < n && (p[*i+1] & 0xC0) == 0x80 && (p[*i+2] & 0xC0) == 0x80) {
        uint32_t v = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(p[*i+1] & 0x3F) << 6) | (p[*i+2] & 0x3F);
        *i += 3; return v;
    }
    if ((c & 0xF8) == 0xF0 && *i + 3 < n && (p[*i+1] & 0xC0) == 0x80 && (p[*i+2] & 0xC0) == 0x80 && (p[*i+3] & 0xC0) == 0x80) {
        uint32_t v = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(p[*i+1] & 0x3F) << 12)
                   | ((uint32_t)(p[*i+2] & 0x3F) << 6) | (p[*i+3] & 0x3F);
        *i += 4; return v;
    }
    (*i) += 1; return c;
}
/* " ".join(s.lower().split()) — ASCII lowercase, Unicode White_Space split. */
static char *tt3_norm_lower_ws(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    char *out = malloc(n + 2);
    if (!out) return strdup("");
    size_t k = 0;
    bool pending = false;
    for (size_t i = 0; i < n; ) {
        size_t start = i;
        uint32_t cp = tt3_next_cp(s, n, &i);
        if (tt3_ws_cp(cp)) { if (k > 0) pending = true; continue; }
        if (pending) { out[k++] = ' '; pending = false; }
        if (cp < 0x80) {
            unsigned char lc = (cp >= 'A' && cp <= 'Z') ? (unsigned char)(cp + 32) : (unsigned char)cp;
            out[k++] = (char)lc;
        } else {
            for (size_t j = start; j < i; j++) out[k++] = s[j];
        }
    }
    out[k] = '\0';
    return out;
}
/* Python int() on a seam result within the documented fixture domain. */
static long tt3_to_int(const json_t *v, bool *ok) {
    *ok = true;
    if (!v || v->type == JSON_NULL) { *ok = false; return 0; }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long)v->num_val;
    if (v->type == JSON_STRING) {
        const char *p = v->str_val ? v->str_val : "";
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
        int sign = 1;
        if (*p == '+' || *p == '-') { if (*p == '-') sign = -1; p++; }
        if (*p < '0' || *p > '9') { *ok = false; return 0; }
        long acc = 0;
        while (*p >= '0' && *p <= '9') { acc = acc * 10 + (*p - '0'); p++; }
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
        if (*p != '\0') { *ok = false; return 0; }
        return sign * acc;
    }
    *ok = false; return 0;
}

/* ── logging seams (RecLogger twin) ── */
static json_t *tt3_log(const tt3_seams_t *s, const char *level,
                       const char *fmt, json_t *args_arr) {
    json_t *a = tt3_args();
    tt3_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    return tt3_call(s, name, a);
}
static json_t *tt3_log2(const tt3_seams_t *s, const char *level,
                        const char *fmt, const json_t *x, const json_t *y) {
    json_t *arr = json_array();
    json_append(arr, x ? json_copy(x) : json_null());
    json_append(arr, y ? json_copy(y) : json_null());
    return tt3_log(s, level, fmt, arr);
}
static json_t *tt3_log3(const tt3_seams_t *s, const char *level,
                        const char *fmt, const json_t *x, const json_t *y, const json_t *z) {
    json_t *arr = json_array();
    json_append(arr, x ? json_copy(x) : json_null());
    json_append(arr, y ? json_copy(y) : json_null());
    json_append(arr, z ? json_copy(z) : json_null());
    return tt3_log(s, level, fmt, arr);
}
/* _quiet's logger.log(DEBUG=10, label, exc_info=True) — no positional args. */
static json_t *tt3_log_quiet(const tt3_seams_t *s, const char *label) {
    json_t *a = tt3_args();
    tt3_s(a, "fmt", label);
    tt3_n(a, "level", 10);
    tt3_b(a, "exc_info", true);
    return tt3_call(s, "logger.log", a);
}

/* ── state helpers ── */
static const json_t *tt3_st_obj(const json_t *st, const char *key) {
    const json_t *o = tt3_oget(st, key);
    return (o && o->type == JSON_OBJECT) ? o : NULL;
}
static json_t *tt3_st_ensure(json_t *st, const char *key) {
    json_t *o = (st && st->type == JSON_OBJECT) ? json_obj_get(st, key) : NULL;
    if (o && o->type == JSON_OBJECT) return o;
    json_t *n = json_object();
    json_set(st, key, n);
    return n;
}
static const json_t *tt3_table_get(const json_t *st, const char *table, const char *key) {
    const json_t *t = tt3_st_obj(st, table);
    if (!t || !key) return NULL;
    return json_obj_get(t, key);
}

/* ── exported flow cores (forward declarations) ── */
static json_t *tt3_command_requires_pipe_stdin(const char *command);
static json_t *tt3_resolve_notification_flag_conflict(bool notify, const json_t *watch, bool background);
static json_t *tt3_isolation_key(const char *key);
static json_t *tt3_session_isolated(const char *env_type, bool persistent, const tt3_seams_t *s);
static json_t *tt3_docker_session_isolated(const char *env_type, bool persistent, const tt3_seams_t *s);
static json_t *tt3_docker_profile_scoped(const char *env_type, bool persistent);
static json_t *tt3_session_scope(const tt3_seams_t *s);
static json_t *tt3_docker_session_isolation_enabled(const tt3_seams_t *s);
static json_t *tt3_has_isolation_overrides(const char *task_id, json_t *st, const tt3_seams_t *s);
static json_t *tt3_register_container_alias(const char *child, const json_t *parent, json_t *st);
static json_t *tt3_maybe_reap_docker_orphans(const json_t *config, json_t *st, const tt3_seams_t *s);
static json_t *tt3_refuse_missing_local_workdir(const char *workdir, const json_t *env, const tt3_seams_t *s);
static json_t *tt3_resolve_command_cwd(const char *workdir, const char *default_cwd,
                                       const char *session_key, const char *env_type,
                                       const json_t *mounted_host, const json_t *env,
                                       const tt3_seams_t *s);
static json_t *tt3_resolve_task_host_cwd(const json_t *config, const char *task_id, const tt3_seams_t *s);

/* PoP: tt3_command_requires_pipe_stdin @ tools/terminal_tool.py:_command_requires_pipe_stdin */
static json_t *tt3_command_requires_pipe_stdin(const char *command) {
    char *norm = tt3_norm_lower_ws(command);
    bool r = tt3_startswith(norm, "gh auth login") && strstr(norm, "--with-token") != NULL;
    free(norm);
    return json_bool(r);
}

/* PoP: tt3_resolve_notification_flag_conflict @ tools/terminal_tool.py:_resolve_notification_flag_conflict */
static json_t *tt3_resolve_notification_flag_conflict(bool notify, const json_t *watch, bool background) {
    json_t *out = json_array();
    if (background && notify && tt3_truth(watch)) {
        json_append(out, json_null());
        json_append(out, json_string("watch_patterns ignored because notify_on_complete=True; "
                                     "these two flags produce duplicate notifications when combined"));
    } else {
        json_append(out, watch ? json_copy(watch) : json_null());
        json_append(out, json_string(""));
    }
    return out;
}

/* Pure leaf transcribed inline (no PoP re-claim): _is_isolation_override_key. */
static json_t *tt3_isolation_key(const char *key) {
    return json_bool(tt3_streq(key, "env_type") || tt3_endswith(key, "_image"));
}

/* PoP: tt3_session_isolated @ tools/terminal_tool.py:_SessionScope.session_isolated */
static json_t *tt3_session_isolated(const char *env_type, bool persistent, const tt3_seams_t *s) {
    if (!tt3_streq(env_type, "docker")) {
        json_t *a = tt3_args();
        tt3_s(a, "env_type", env_type);
        tt3_s(a, "attr", "session_isolated_when_nonpersistent");
        tt3_b(a, "default", false);
        json_t *flag = tt3_call(s, "plugin_env_flag", a);
        if (tt3_raised(flag)) return flag;
        bool f = tt3_truth(flag);
        json_free(flag);
        if (!f) return json_bool(false);
    }
    return json_bool(!persistent);
}

/* PoP: tt3_docker_session_isolated @ tools/terminal_tool.py:_SessionScope.docker_session_isolated */
static json_t *tt3_docker_session_isolated(const char *env_type, bool persistent, const tt3_seams_t *s) {
    if (!tt3_streq(env_type, "docker")) return json_bool(false);
    return tt3_session_isolated(env_type, persistent, s);
}

/* PoP: tt3_docker_profile_scoped @ tools/terminal_tool.py:_SessionScope.docker_profile_scoped */
static json_t *tt3_docker_profile_scoped(const char *env_type, bool persistent) {
    return json_bool(tt3_streq(env_type, "docker") && persistent);
}

/* Composed sibling inlined (no PoP re-claim): _session_scope. */
static json_t *tt3_session_scope(const tt3_seams_t *s) {
    json_t *br = tt3_call(s, "ensure_terminal_env_bridged", tt3_args());
    if (tt3_raised(br)) return br;
    json_free(br);
    json_t *a1 = tt3_args();
    tt3_s(a1, "name", "TERMINAL_ENV");
    tt3_s(a1, "default", "local");
    json_t *env_type = tt3_call(s, "tenv", a1);
    if (tt3_raised(env_type)) return env_type;
    json_t *a2 = tt3_args();
    tt3_s(a2, "name", "TERMINAL_CONTAINER_PERSISTENT");
    tt3_s(a2, "default", "true");
    json_t *persistent = tt3_call(s, "tenv_bool", a2);
    if (tt3_raised(persistent)) { json_free(env_type); return persistent; }
    json_t *scope = json_object();
    json_set(scope, "env_type", env_type ? env_type : json_null());
    json_set(scope, "persistent", persistent ? persistent : json_null());
    return scope;
}

/* PoP: tt3_docker_session_isolation_enabled @ tools/terminal_tool.py:_docker_session_isolation_enabled */
static json_t *tt3_docker_session_isolation_enabled(const tt3_seams_t *s) {
    json_t *scope = tt3_session_scope(s);
    if (tt3_raised(scope)) return scope;
    const char *et = tt3_strval(tt3_oget(scope, "env_type"));
    bool pers = tt3_truth(tt3_oget(scope, "persistent"));
    json_t *r = tt3_docker_session_isolated(et, pers, s);
    json_free(scope);
    return r;
}

/* PoP: tt3_has_isolation_overrides @ tools/terminal_tool.py:_has_isolation_overrides */
static json_t *tt3_has_isolation_overrides(const char *task_id, json_t *st, const tt3_seams_t *s) {
    if (!task_id || task_id[0] == '\0') return json_bool(false);
    json_t *a = tt3_args();
    tt3_s(a, "key", task_id);
    json_t *q = tt3_call(s, "qualify_task_key", a);
    if (tt3_raised(q)) return q;
    const char *qs = tt3_strval(q);
    const json_t *ov = tt3_table_get(st, "task_env_overrides", qs);
    bool any = false;
    if (ov && ov->type == JSON_OBJECT) {
        for (size_t i = 0; i < ov->c.count; i++) {
            if (tt3_streq(ov->c.keys[i], "env_type") || tt3_endswith(ov->c.keys[i], "_image")) {
                any = true;
                break;
            }
        }
    }
    json_free(q);
    return json_bool(any);
}

/* PoP: tt3_register_container_alias @ tools/terminal_tool.py:register_container_alias */
static json_t *tt3_register_container_alias(const char *child, const json_t *parent, json_t *st) {
    if (!child || child[0] == '\0') return json_null();
    json_t *aliases = tt3_st_ensure(st, "container_aliases");
    char *pv;
    if (tt3_truth(parent)) pv = tt3_pystr(parent);
    else pv = strdup("default");
    json_set(aliases, child, json_string(pv));
    free(pv);
    return json_null();
}

/* Poison-pill raise returned by the _quiet handler: a BaseException passes
 * through the real contextmanager without logging, an Exception is logged
 * (logger.log DEBUG/exc_info=True) then suppressed. */
static json_t *tt3_quiet_finish(const tt3_seams_t *s, const char *label, json_t *exc) {
    if (exc && tt3_raised(exc) && !tt3_exc_is(tt3_kind(exc), "Exception")) return exc;
    json_t *r = tt3_log_quiet(s, label);
    json_free(exc);
    if (tt3_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: tt3_maybe_reap_docker_orphans @ tools/terminal_tool.py:_maybe_reap_docker_orphans */
static json_t *tt3_maybe_reap_docker_orphans(const json_t *config, json_t *st, const tt3_seams_t *s) {
    const json_t *gate = tt3_oget(config, "docker_orphan_reaper");
    if (gate && !tt3_truth(gate)) return json_null();
    if (tt3_truth(tt3_oget(st, "docker_orphan_reaper_ran"))) return json_null();
    json_set(st, "docker_orphan_reaper_ran", json_bool(true));

    json_t *a = tt3_args();
    tt3_s(a, "name", "TERMINAL_LIFETIME_SECONDS");
    tt3_s(a, "default", "300");
    json_t *tv = tt3_call(s, "tenv", a);
    long lifetime = 300;
    if (tt3_raised(tv)) {
        if (!tt3_exc_is(tt3_kind(tv), "TypeError") && !tt3_exc_is(tt3_kind(tv), "ValueError")) return tv;
        json_free(tv);
        tv = NULL;
    }
    if (tv) {
        bool ok = false;
        long v = tt3_to_int(tv, &ok);
        if (!ok) {
            /* live: try/except (TypeError, ValueError) -> lifetime = 300 */
            lifetime = 300;
        } else lifetime = v;
        json_free(tv);
    }
    long max_age = (lifetime > 60 ? lifetime : 60) * 2;

    json_t *imp = tt3_call(s, "import_docker", tt3_args());
    if (tt3_raised(imp)) {
        if (tt3_exc_is(tt3_kind(imp), "ImportError")) { json_free(imp); return json_null(); }
        return imp;
    }
    json_free(imp);

    json_t *ca = tt3_args();
    const json_t *ckey = tt3_oget(config, "docker_shared_container_key");
    tt3_s(ca, "config_key", ckey && ckey->type == JSON_STRING ? ckey->str_val : "");
    json_t *profile = tt3_call(s, "container_identity", ca);
    if (tt3_raised(profile)) return tt3_quiet_finish(s, "Docker orphan reaper raised", profile);

    json_t *ra = tt3_args();
    tt3_n(ra, "max_age_seconds", (double)max_age);
    tt3_j(ra, "profile_filter", profile);
    json_t *removed = tt3_call(s, "reap_orphan_containers", ra);
    if (tt3_raised(removed)) {
        json_free(profile);
        return tt3_quiet_finish(s, "Docker orphan reaper raised", removed);
    }
    if (tt3_truth(removed)) {
        json_t *lr = tt3_log2(s, "info",
                "Docker orphan reaper removed %d stale container(s) for profile %s", removed, profile);
        json_free(profile);
        json_free(removed);
        if (tt3_raised(lr)) return tt3_quiet_finish(s, "Docker orphan reaper raised", lr);
        json_free(lr);
    } else {
        json_free(profile);
        json_free(removed);
    }
    return json_null();
}

/* PoP: tt3_refuse_missing_local_workdir @ tools/terminal_tool.py:_refuse_missing_local_workdir */
static json_t *tt3_refuse_missing_local_workdir(const char *workdir, const json_t *env, const tt3_seams_t *s) {
    if (!env || env->type != JSON_OBJECT) return json_null();
    const json_t *marker = json_obj_get(env, "__local_env__");
    if (!tt3_truth(marker)) return json_null();
    json_t *a = tt3_args();
    tt3_s(a, "workdir", workdir);
    json_t *r = tt3_call(s, "usable_cwd", a);
    if (tt3_raised(r)) return r;
    const json_t *fb = NULL;
    if (r && r->type == JSON_ARRAY && json_len(r) > 1) fb = json_get(r, 1);
    bool fallback = tt3_truth(fb);
    if (!fallback) { json_free(r); return json_null(); }
    char *msg = tt3_fmt("workdir %s does not exist or is not accessible; the command did not run. "
                        "Create it first or pass an existing directory.", workdir);
    json_t *ea = tt3_args();
    tt3_s(ea, "error", msg);
    tt3_n(ea, "exit_code", 126);
    tt3_s(ea, "status", "error");
    free(msg);
    json_free(r);
    json_t *ej = tt3_call(s, "error_json", ea);
    if (tt3_raised(ej)) return ej;
    char *rj = tt3_pystr(ej);
    json_t *out = tt3_raise("_Rejected", rj);
    free(rj);
    json_free(ej);
    return out;
}

/* PoP: tt3_resolve_command_cwd @ tools/terminal_tool.py:_resolve_command_cwd */
static json_t *tt3_resolve_command_cwd(const char *workdir, const char *default_cwd,
                                       const char *session_key, const char *env_type,
                                       const json_t *mounted_host, const json_t *env,
                                       const tt3_seams_t *s) {
    if (workdir && workdir[0] != '\0') {
        json_t *a = tt3_args();
        tt3_s(a, "path", workdir);
        tt3_s(a, "env_type", env_type);
        tt3_j(a, "env", env);
        json_t *vis = tt3_call(s, "container_visible_cwd", a);
        if (tt3_raised(vis)) return vis;
        json_t *b = tt3_args();
        tt3_j(b, "cwd", vis);
        tt3_s(b, "env_type", env_type);
        json_t *co = tt3_call(s, "coerce_ssh_remote_cwd", b);
        json_free(vis);
        return co;
    }
    json_t *gs = tt3_args();
    tt3_s(gs, "session_key", session_key);
    json_t *recorded = tt3_call(s, "get_session_cwd", gs);
    if (tt3_raised(recorded)) return recorded;
    if (tt3_truth(recorded)) {
        json_t *a = tt3_args();
        tt3_s(a, "env_type", env_type);
        json_t *backend = tt3_call(s, "is_container_backend", a);
        if (tt3_raised(backend)) { json_free(recorded); return backend; }
        bool isb = tt3_truth(backend);
        json_free(backend);
        if (isb) {
            json_t *u = tt3_args();
            tt3_j(u, "cwd", recorded);
            tt3_j(u, "mounted_host", mounted_host);
            json_t *un = tt3_call(s, "is_unusable_container_cwd", u);
            if (tt3_raised(un)) { json_free(recorded); return un; }
            bool unusable = tt3_truth(un);
            json_free(un);
            if (unusable) {
                json_t *h = tt3_args();
                tt3_j(h, "cwd", recorded);
                tt3_j(h, "env", env);
                json_t *sandbox = tt3_call(s, "in_sandbox_home", h);
                if (tt3_raised(sandbox)) { json_free(recorded); return sandbox; }
                bool inhome = tt3_truth(sandbox);
                json_free(sandbox);
                if (!inhome) {
                    json_t *a2 = tt3_args();
                    tt3_j(a2, "path", recorded);
                    tt3_s(a2, "env_type", env_type);
                    tt3_j(a2, "env", env);
                    json_t *vis = tt3_call(s, "container_visible_cwd", a2);
                    if (tt3_raised(vis)) { json_free(recorded); return vis; }
                    const char *vs = tt3_strval(vis);
                    const char *rs = tt3_strval(recorded);
                    bool same = (vs == rs) || (vs && rs && strcmp(vs, rs) == 0);
                    if (!same) { json_free(recorded); return vis; }
                    json_free(vis);
                    json_t *m = tt3_args();
                    tt3_j(m, "cwd", recorded);
                    tt3_j(m, "mounted_host", mounted_host);
                    json_t *mh = tt3_call(s, "is_mounted_host_cwd", m);
                    if (tt3_raised(mh)) { json_free(recorded); return mh; }
                    bool mounted = tt3_truth(mh);
                    json_free(mh);
                    if (mounted) {
                        json_t *et = env_type ? json_string(env_type) : json_null();
                        json_t *lr = tt3_log2(s, "info",
                            "Remapping recorded session cwd %r for %s backend "
                            "(mounted host directory). Using '/workspace' instead.", recorded, et);
                        json_free(et);
                        json_free(recorded);
                        if (tt3_raised(lr)) return lr;
                        json_free(lr);
                        return json_string("/workspace");
                    }
                    json_t *et = env_type ? json_string(env_type) : json_null();
                    json_t *dc = default_cwd ? json_string(default_cwd) : json_null();
                    json_t *lr = tt3_log3(s, "info",
                        "Ignoring recorded session cwd %r for %s backend "
                        "(host/relative path won't work in sandbox). Using %r instead.",
                        recorded, et, dc);
                    json_free(et); json_free(dc);
                    json_free(recorded);
                    if (tt3_raised(lr)) return lr;
                    json_free(lr);
                    json_t *d = tt3_args();
                    tt3_s(d, "default_cwd", default_cwd);
                    tt3_s(d, "env_type", env_type);
                    tt3_j(d, "env", env);
                    return tt3_call(s, "container_visible_default", d);
                }
            }
        }
    }
    json_t *base = recorded;
    if (!tt3_truth(base)) {
        json_free(recorded);
        json_t *d = tt3_args();
        tt3_s(d, "default_cwd", default_cwd);
        tt3_s(d, "env_type", env_type);
        tt3_j(d, "env", env);
        base = tt3_call(s, "container_visible_default", d);
        if (tt3_raised(base)) return base;
    }
    json_t *b = tt3_args();
    tt3_j(b, "cwd", base);
    tt3_s(b, "env_type", env_type);
    json_t *co = tt3_call(s, "coerce_ssh_remote_cwd", b);
    json_free(base);
    return co;
}

/* PoP: tt3_resolve_task_host_cwd @ tools/terminal_tool.py:_resolve_task_host_cwd */
static json_t *tt3_resolve_task_host_cwd(const json_t *config, const char *task_id, const tt3_seams_t *s) {
    const json_t *et = tt3_oget(config, "env_type");
    if (!et || et->type != JSON_STRING || strcmp(et->str_val, "docker") != 0)
        return json_null();
    const json_t *mount = tt3_oget(config, "docker_mount_cwd_to_workspace");
    if (!tt3_truth(mount)) return json_null();

    json_t *iso = tt3_call(s, "docker_session_isolation_enabled", tt3_args());
    if (tt3_raised(iso)) return iso;
    bool isolated = tt3_truth(iso);
    json_free(iso);
    json_t *key1 = NULL;
    const char *k1 = NULL;
    if (isolated) {
        /* `or` evaluates the second operand only when isolation is enabled. */
        json_t *t1 = tt3_args();
        tt3_s(t1, "task_id", task_id);
        key1 = tt3_call(s, "resolve_container_task_id", t1);
        if (tt3_raised(key1)) return key1;
        k1 = tt3_strval(key1);
        if (!(k1 && strcmp(k1, "default") == 0)) {
            json_free(key1);
            json_t *t2 = tt3_args();
            tt3_s(t2, "owner", task_id);
            return tt3_call(s, "session_workspace_mount_source", t2);
        }
        /* The shared body re-reads the key: a SECOND seam call. */
        json_free(key1);
        json_t *t3 = tt3_args();
        tt3_s(t3, "task_id", task_id);
        key1 = tt3_call(s, "resolve_container_task_id", t3);
        if (tt3_raised(key1)) return key1;
        k1 = tt3_strval(key1);
    } else {
        /* `not isolated` short-circuits the `or`; the body reads the key once. */
        json_t *t1 = tt3_args();
        tt3_s(t1, "task_id", task_id);
        key1 = tt3_call(s, "resolve_container_task_id", t1);
        if (tt3_raised(key1)) return key1;
        k1 = tt3_strval(key1);
    }
    /* shared branch */
    json_t *prof = tt3_call(s, "container_profile", tt3_args());
    if (tt3_raised(prof)) { json_free(key1); return prof; }
    json_t *pa = tt3_args();
    tt3_j(pa, "profile", prof);
    json_t *pkey = tt3_call(s, "profile_container_key", pa);
    json_free(prof);
    if (tt3_raised(pkey)) { json_free(key1); return pkey; }
    char *prefix = tt3_fmt("%s:ws:", tt3_strval(pkey) ? tt3_strval(pkey) : "");
    json_free(pkey);

    /* dict.fromkeys((task_id, alias(task_id) if task_id else None)) */
    json_t *owners = json_array();
    if (task_id && task_id[0] != '\0') {
        json_append(owners, json_string(task_id));
        json_t *aa = tt3_args();
        tt3_s(aa, "task_id", task_id);
        json_t *alias = tt3_call(s, "resolve_container_alias", aa);
        if (tt3_raised(alias)) { json_free(key1); free(prefix); json_free(owners); return alias; }
        const char *av = tt3_strval(alias);
        bool dup = av && tt3_streq(av, task_id);
        if (!dup) json_append(owners, av ? json_copy(alias) : json_null());
        json_free(alias);
    } else {
        json_append(owners, json_null());
    }
    json_t *result = NULL;
    for (size_t i = 0; i < json_len(owners) && !result; i++) {
        json_t *wa = tt3_args();
        tt3_j(wa, "owner", json_get(owners, i));
        json_t *src = tt3_call(s, "session_workspace_mount_source", wa);
        if (tt3_raised(src)) { result = src; break; }
        if (tt3_strval(src)) {
            json_t *ka = tt3_args();
            tt3_j(ka, "cwd", src);
            json_t *wk = tt3_call(s, "workspace_key_for", ka);
            if (tt3_raised(wk)) { json_free(src); result = wk; break; }
            char *cand = tt3_fmt("%s%s", prefix, tt3_strval(wk) ? tt3_strval(wk) : "");
            json_free(wk);
            bool match = k1 && strcmp(k1, cand) == 0;
            free(cand);
            if (match) { result = src; break; }
            json_free(src);
        } else json_free(src);
    }
    json_free(owners);
    if (result) { free(prefix); json_free(key1); return result; }
    bool keyed = k1 && tt3_startswith(k1, prefix);
    free(prefix);
    if (keyed) { json_free(key1); return json_null(); }
    json_free(key1);
    const json_t *hc = tt3_oget(config, "host_cwd");
    return hc ? json_copy(hc) : json_null();
}
