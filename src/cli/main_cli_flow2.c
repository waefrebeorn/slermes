/* main_cli_flow2.c — FLOW + leaves port of the UNCLAIMED remainder of
 * hermes_cli/main.py (the claimed siblings live in port_main_cli_flow.c /
 * main_u_* / mn_* and are SEAMED here).
 *
 * Decision-sequence port: pid1 warning, git fingerprint reader, auth-store
 * login probe, workspace-key resolver, the _session_db contextmanager, chat
 * session-arg normalization, xAI retirement warning, background prefetch,
 * forward-command factory, provider resolution/picker, dashboard runtime prep,
 * argv/plugin-discovery leaves, agent-subcommand predicates, config bypass,
 * oneshot/cpu-profile/light-parser/resume-promote launch leaves, fast-serve
 * dispatch and plugin CLI registration. ALL collaborator I/O (claimed siblings,
 * foreign-module imports, os/sys/subprocess/threading/logging/json reads, print,
 * path methods, logger) rides the injected universal seam mc2_call; the harness
 * scripts results and records the ordered trace the oracle diffs against LIVE
 * hermes_cli/main.py under the same scripts. Ported functions call each other
 * REAL (internal composition: _register_plugin_cli_commands ->
 * _first_positional_argv / _attach_plugin_cli_command /
 * _resolve_deferred_platform_cli_command; _resolve_chat_session_args ->
 * _session_db).
 *
 * Representation contract (shared with tests/oracle_main_cli_flow2.py):
 *   - attribute doubles (args/store/reader/session/active_def/result/parser/
 *     manager) are present-non-null JSON records read via json_obj_get and are
 *     ALWAYS truthy (even {}), modelling types.SimpleNamespace; plain dict
 *     inputs (config/registry/custom_provider_map/cmd_info/provider_labels) use
 *     JSON truthiness. tuple/set -> array (sets sorted); None -> JSON null;
 *     integral floats collapse to ints on both sides; Path -> POSIX str.
 *   - uncaught exception -> {"__raise__": cls}; an exception passed as a logger
 *     arg projects to {"__raise__": cls, "msg": str(e)}.
 *   - scripts (FAIL-CLOSED): absent key -> RuntimeError("no script for <name>");
 *     present null -> None; {"$seq":[...]} per-call queue (exhaustion raises
 *     RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m,...}
 *     raises (trace ret is the WHOLE marker); any other value reused. None-valued
 *     seam arg keys dropped on BOTH sides.
 *   - json.loads rides the "json.loads" seam (scripted parse result) so the C
 *     side stays libjson-only without transcribing CPython's strict grammar.
 *   - fixture domain: str()/strip()/lower() operands are ASCII (incl. C0 control
 *     chars); object-double fields are strings/numbers/bools/null/records;
 *     setup_fn/handler_fn are string markers or null; command/argv tokens ASCII.
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "cli/main_cli_flow2.c". main_cli / mcl_ prefixes are taken by flow1;
 * this port uses mc2_.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct mc2_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted constants/flags */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} mc2_ctx;

static bool mc2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool mc2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool mc2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool mc2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool mc2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool mc2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool mc2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool mc2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
/* object-double truthiness: present-non-null record is truthy even {} */
static bool mc2_double_truthy(const json_t *v) { return !mc2_is_none(v); }

static const char *mc2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static double mc2_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* borrowed object field or NULL */
static json_t *mc2_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
/* dict.get(key, default): present-null keeps the null node; absent -> default. */
static json_t *mc2_get_def(const json_t *obj, const char *key, json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}
static json_t *mc2_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}
static json_t *mc2_replace(json_t *old, json_t *nw) { json_free(old); return nw; }

/* Python type name for a JSON node (integral double reads int). */
static const char *mc2_py_type_name(const json_t *v) {
    if (mc2_is_none(v)) return "NoneType";
    if (mc2_is_bool(v)) return "bool";
    if (mc2_is_num(v)) {
        double d = v->num_val;
        return (d == floor(d) && fabs(d) < 1e15) ? "int" : "float";
    }
    if (mc2_is_str(v)) return "str";
    if (mc2_is_arr(v)) return "list";
    if (mc2_is_obj(v)) return "dict";
    return "object";
}

/* Deep Python-style equality; NULL pointer and JSON_NULL both read None. */
static bool mc2_json_eq(const json_t *a, const json_t *b) {
    if (mc2_is_none(a) && mc2_is_none(b)) return true;
    if (mc2_is_none(a) || mc2_is_none(b)) return false;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return mc2_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!mc2_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !mc2_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* ── string helpers (caller frees) ───────────────────────────────────────── */

static bool mc2_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f' || c == '\x1c' || c == '\x1d' || c == '\x1e' || c == '\x1f';
}
static char *mc2_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && mc2_is_space(*b)) b++;
    const char *e = b + strlen(b);
    while (e > b && mc2_is_space(e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, b, n); r[n] = '\0';
    return r;
}
static char *mc2_lower(const char *s) {
    char *r = strdup(s ? s : "");
    if (!r) return NULL;
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}
/* str(v) for a scalar (null -> "None"). Caller frees. */
static char *mc2_pystr(const json_t *v) {
    if (mc2_is_none(v)) return strdup("None");
    if (mc2_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (mc2_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    if (mc2_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}
/* str(v or "") for a scalar (falsy -> ""). Caller frees. */
static char *mc2_str_or_empty(const json_t *v) {
    if (!mc2_truthy(v)) return strdup("");
    return mc2_pystr(v);
}
static bool mc2_startswith(const char *s, const char *pre) {
    return strncmp(s ? s : "", pre, strlen(pre)) == 0;
}
/* str.splitlines() ASCII boundaries: \n \r \r\n \v \f \x1c \x1d \x1e (NOT \x1f). */
static json_t *mc2_splitlines(const char *s) {
    json_t *arr = json_array();
    if (!s) return arr;
    const char *p = s; const char *start = s;
    while (*p) {
        char c = *p;
        bool brk = (c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
                    c == '\x1c' || c == '\x1d' || c == '\x1e');
        if (brk) {
            size_t n = (size_t)(p - start);
            char *line = (char *)malloc(n + 1);
            memcpy(line, start, n); line[n] = '\0';
            json_append(arr, json_string(line));
            free(line);
            if (c == '\r' && p[1] == '\n') p++;
            p++;
            start = p;
        } else {
            p++;
        }
    }
    if (p > start) {
        json_append(arr, json_string(start));
    }
    return arr;
}
/* str.partition(sep): first occurrence; key before, value after ("" if absent). */
static void mc2_partition(const char *s, const char *sep, char **key, char **val) {
    const char *hit = strstr(s ? s : "", sep);
    if (!hit) { *key = strdup(s ? s : ""); *val = strdup(""); return; }
    size_t n = (size_t)(hit - s);
    *key = (char *)malloc(n + 1); memcpy(*key, s, n); (*key)[n] = '\0';
    *val = strdup(hit + strlen(sep));
}
/* str.split(sep, maxsplit) -> array (maxsplit<0 = unlimited). Empty sep invalid. */
static json_t *mc2_split(const char *s, const char *sep, int maxsplit) {
    json_t *arr = json_array();
    if (!s) s = "";
    size_t sl = strlen(sep);
    if (sl == 0) { json_append(arr, json_string(s)); return arr; }
    const char *p = s;
    int cuts = 0;
    while (*p) {
        if (maxsplit >= 0 && cuts >= maxsplit) break;
        const char *hit = strstr(p, sep);
        if (!hit) break;
        size_t n = (size_t)(hit - p);
        char *frag = (char *)malloc(n + 1); memcpy(frag, p, n); frag[n] = '\0';
        json_append(arr, json_string(frag)); free(frag);
        p = hit + sl;
        cuts++;
    }
    json_append(arr, json_string(p));
    return arr;
}
/* PurePosixPath(a) / b — verbatim POSIX join (resolve() is seamed separately). */
static char *mc2_path_join(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    if (b[0] == '/') return strdup(b);
    if (b[0] == '\0' || mc2_str_eq(b, ".")) {
        /* Path drops a "." component; normalize trailing slashes of a */
        size_t n = strlen(a);
        while (n > 1 && a[n - 1] == '/') n--;
        char *r = (char *)malloc(n + 1); memcpy(r, a, n); r[n] = '\0';
        return r;
    }
    size_t n = strlen(a);
    while (n > 1 && a[n - 1] == '/') n--;
    bool root = (n == 1 && a[0] == '/');
    bool empty = (n == 0);
    size_t bl = strlen(b);
    char *r = (char *)malloc(n + bl + 2);
    memcpy(r, a, n);
    size_t k = n;
    if (!empty && !root) r[k++] = '/';
    else if (root) r[k++] = '/';
    memcpy(r + k, b, bl);
    r[k + bl] = '\0';
    return r;
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool mc2_exc_pending(const mc2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void mc2_exc_clear(mc2_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void mc2_raise(mc2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *mc2_exc_class(const mc2_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *mc2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"},
        {"JSONDecodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeError", "ValueError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (mc2_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool mc2_exc_matches(const mc2_ctx *ctx, const char *const *classes, size_t n) {
    if (!mc2_exc_pending(ctx)) return false;
    const char *cls = mc2_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (mc2_str_eq(cls, classes[i])) return true;
        const char *p = mc2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception catches EVERY Exception descendant (incl. synthetic classes)
 * but NOT the BaseException trio. Model by exclusion rather than an incomplete
 * parent walk: any kind that is not SystemExit/KeyboardInterrupt/GeneratorExit/
 * BaseException is an Exception subclass in the fixture domain. */
static bool mc2_exc_is_exception(const mc2_ctx *ctx) {
    if (!mc2_exc_pending(ctx)) return false;
    const char *cls = mc2_exc_class(ctx);
    return !(mc2_str_eq(cls, "SystemExit") || mc2_str_eq(cls, "KeyboardInterrupt") ||
             mc2_str_eq(cls, "GeneratorExit") || mc2_str_eq(cls, "BaseException"));
}
/* AttributeError "'<type>' object has no attribute '<attr>'". */
static void mc2_attr_raise(mc2_ctx *ctx, const char *type, const char *attr) {
    char msg[200];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", type, attr);
    mc2_raise(ctx, "AttributeError", msg);
}
/* direct attribute read on a SimpleNamespace double (raises when absent). */
static json_t *mc2_ns_get(mc2_ctx *ctx, json_t *obj, const char *key) {
    if (mc2_is_none(obj)) { mc2_attr_raise(ctx, "NoneType", key); return NULL; }
    if (!mc2_is_obj(obj)) { mc2_attr_raise(ctx, mc2_py_type_name(obj), key); return NULL; }
    json_t *v = json_obj_get(obj, key);
    if (!v) { mc2_attr_raise(ctx, "types.SimpleNamespace", key); return NULL; }
    return v;
}
/* plain-dict .get on a non-dict raises AttributeError '<type>' ... 'get'. */
static json_t *mc2_dict_get(mc2_ctx *ctx, json_t *obj, const char *key, json_t *def) {
    if (!mc2_is_none(obj) && !mc2_is_obj(obj)) {
        mc2_attr_raise(ctx, mc2_py_type_name(obj), "get");
        return NULL;
    }
    return mc2_get_def(obj, key, def);
}
/* Python list index (negative wraps; OOR -> IndexError). Borrowed item. */
static json_t *mc2_index(mc2_ctx *ctx, json_t *arr, json_t *idx, const char *what) {
    if (!mc2_is_arr(arr)) { mc2_attr_raise(ctx, mc2_py_type_name(arr), "__getitem__"); return NULL; }
    if (!mc2_is_num(idx)) { mc2_raise(ctx, "TypeError", "list indices must be integers or slices"); return NULL; }
    double d = idx->num_val;
    long n = (long)arr->c.count;
    long i = (long)d;
    if (i < 0) i += n;
    if (i < 0 || i >= n) { mc2_raise(ctx, "IndexError", "list index out of range"); return NULL; }
    (void)what;
    return arr->c.items[i];
}

/* ── universal seam (FAIL-CLOSED) ────────────────────────────────────────── */

static json_t *mc2_script_result(mc2_ctx *ctx, const json_t *script) {
    if (!script || script->type == JSON_NULL) return NULL;
    if (script->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(script, "__raise__");
        if (raise && raise->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(script);
            return NULL;
        }
    }
    return json_copy(script);
}

/* args ownership is TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *mc2_call(mc2_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *chosen = script;
    if (!script) {
        /* fail-closed: no script for this seam */
        if (ctx->exc) json_free(ctx->exc);
        ctx->exc = json_object();
        json_set(ctx->exc, "__raise__", json_string("RuntimeError"));
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        json_set(ctx->exc, "msg", json_string(msg));
        chosen = ctx->exc;
    } else if (script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                if (ctx->exc) json_free(ctx->exc);
                ctx->exc = json_object();
                json_set(ctx->exc, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof msg, "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = mc2_script_result(ctx, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (ctx->exc && chosen == ctx->exc) json_set(ev, "ret", json_copy(ctx->exc));
    else if (ctx->exc && chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))
        json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    return ret;
}

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). Borrowed val
 * is copied; the caller keeps ownership of the source node. */
static void mc2_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void mc2_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void mc2_arg_bool(json_t *args, const char *key, bool val) {
    json_set(args, key, json_bool(val));
}
/* dict.get(field, defstr) projected as a seam arg: present value kept (null
 * dropped), absent -> defstr ("" kept). Non-dict obj raises AttributeError. */
static void mc2_arg_dict(mc2_ctx *ctx, json_t *args, const char *key,
                         json_t *obj, const char *field, const char *defstr) {
    json_t *v = mc2_dict_get(ctx, obj, field, NULL);
    if (mc2_exc_pending(ctx)) return;
    if (v) mc2_arg(args, key, v);
    else json_set(args, key, json_string(defstr));
}
/* logger seam: {"msg","args"[,"exc_info"]}. args_arr ownership transferred. */
static void mc2_log(mc2_ctx *ctx, const char *level, const char *msg,
                    json_t *args_arr, bool exc_info) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) json_set(a, "exc_info", json_bool(true));
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = mc2_call(ctx, name, a);
    json_free(r);
}
/* project the pending exception as a logger arg node {"__raise__","msg"}. */
static json_t *mc2_exc_arg(const mc2_ctx *ctx) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(mc2_exc_class(ctx)));
    json_set(e, "msg", json_string(json_get_str(ctx->exc, "msg", "")));
    return e;
}
/* print seam: {"text"[,"file"]}. file NULL -> stdout (key dropped). */
static void mc2_print(mc2_ctx *ctx, const char *text, const char *file) {
    json_t *a = json_object();
    mc2_arg_str(a, "text", text);
    if (file) mc2_arg_str(a, "file", file);
    json_t *r = mc2_call(ctx, "print", a);
    json_free(r);
}
/* sys.exit seam: records then raises SystemExit(str(code)) unless the script
 * itself raised. code ownership TRANSFERRED (freed here). */
static void mc2_sys_exit(mc2_ctx *ctx, json_t *code) {
    json_t *a = json_object();
    mc2_arg(a, "code", code);
    json_t *r = mc2_call(ctx, "sys.exit", a);
    json_free(r);
    if (!mc2_exc_pending(ctx)) {
        char *m = mc2_pystr(code);
        mc2_raise(ctx, "SystemExit", m);
        free(m);
    }
    json_free(code);
}
/* path.read_text seam (utf-8-sig BOM strip). Result NEW (str BOM-stripped). */
static json_t *mc2_read_text(mc2_ctx *ctx, const char *path, bool errors_replace) {
    json_t *a = json_object();
    mc2_arg_str(a, "path", path);
    mc2_arg_str(a, "encoding", "utf-8-sig");
    if (errors_replace) mc2_arg_str(a, "errors", "replace");
    json_t *r = mc2_call(ctx, "path.read_text", a);
    if (mc2_exc_pending(ctx)) return r;
    if (mc2_is_str(r)) {
        const char *s = mc2_jstr(r);
        if (strncmp(s, "\xEF\xBB\xBF", 3) == 0) {
            json_t *n = json_string(s + 3);
            json_free(r);
            return n;
        }
    }
    return r;
}
/* str method (.strip/.splitlines) on a node: non-str -> AttributeError. */
static char *mc2_strip_node(mc2_ctx *ctx, json_t *v) {
    if (!mc2_is_str(v)) { mc2_attr_raise(ctx, mc2_py_type_name(v), "strip"); return NULL; }
    return mc2_strip(mc2_jstr(v));
}

/* tuple result helpers */
static json_t *mc2_t0(const json_t *r) { return mc2_is_arr(r) ? json_get(r, 0) : NULL; }
static json_t *mc2_t1(const json_t *r) { return mc2_is_arr(r) ? json_get(r, 1) : NULL; }

/* ── pid1 warning ────────────────────────────────────────────────────────── */

/* PoP: mc2_warn_if_unsupervised_pid1 @ hermes_cli/main.py:_warn_if_unsupervised_pid1 */
static json_t *mc2_warn_if_unsupervised_pid1(mc2_ctx *ctx, json_t *in) {
    json_t *pid = mc2_get(in, "pid");
    json_t *eff = NULL;
    bool owned_eff = false;
    if (!mc2_is_none(pid)) {
        eff = pid;  /* borrowed */
    } else {
        eff = mc2_call(ctx, "os.getpid", json_object());
        owned_eff = true;
        if (mc2_exc_pending(ctx)) { json_free(eff); return NULL; }
    }
    bool is_one = mc2_is_num(eff) && eff->num_val == 1.0;
    if (owned_eff) json_free(eff);
    if (!is_one) return json_null();
    mc2_print(ctx,
        "[hermes] WARNING: this process is PID 1 with no init above it "
        "(entrypoint override?). Orphaned child processes will not be "
        "reaped and will accumulate as zombies. Use the image's default "
        "ENTRYPOINT (docker/entrypoint-dispatch.sh) instead of overriding "
        "it, or run with `docker run --init` / `init: true` in Compose.",
        "stderr");
    if (mc2_exc_pending(ctx)) return NULL;
    return json_null();
}

/* ── git revision fingerprint ────────────────────────────────────────────── */

/* PoP: mc2_read_git_revision_fingerprint @ hermes_cli/main.py:_read_git_revision_fingerprint */
static json_t *mc2_read_git_revision_fingerprint(mc2_ctx *ctx, json_t *in) {
    const char *root = json_get_str(in, "repo_root", "");
    char *git_dir = mc2_path_join(root, ".git");
    char *common_dir = NULL;
    json_t *ret = NULL;
    json_t *r = NULL;
    char *s = NULL;
    static const char *OSERR[] = {"OSError"};

    /* if git_dir.is_file(): parse gitdir: line */
    json_t *a = json_object(); mc2_arg_str(a, "path", git_dir);
    r = mc2_call(ctx, "path.is_file", a);
    if (mc2_exc_pending(ctx)) goto catch;
    bool is_file = mc2_truthy(r); json_free(r); r = NULL;
    if (is_file) {
        r = mc2_read_text(ctx, git_dir, true);
        if (mc2_exc_pending(ctx)) goto catch;
        json_t *lines = mc2_splitlines(mc2_jstr(r));
        json_free(r); r = NULL;
        for (size_t i = 0; i < lines->c.count; i++) {
            const char *line = mc2_jstr(lines->c.items[i]);
            char *key = NULL, *val = NULL;
            mc2_partition(line, ":", &key, &val);
            char *ks = mc2_strip(key); char *vs = mc2_strip(val);
            bool hit = mc2_str_eq(ks, "gitdir") && vs[0] != '\0';
            free(key); free(val); free(ks);
            if (hit) {
                char *joined = mc2_path_join(root, vs);
                free(vs);
                json_t *aa = json_object(); mc2_arg_str(aa, "path", joined);
                free(joined);
                json_t *res = mc2_call(ctx, "path.resolve", aa);
                if (mc2_exc_pending(ctx)) { json_free(lines); goto catch; }
                free(git_dir);
                git_dir = strdup(mc2_jstr(res));
                json_free(res);
                break;
            }
            free(vs);
        }
        json_free(lines);
    }
    /* commondir resolution */
    common_dir = strdup(git_dir);
    char *cd_file = mc2_path_join(git_dir, "commondir");
    a = json_object(); mc2_arg_str(a, "path", cd_file);
    r = mc2_call(ctx, "path.exists", a);
    if (mc2_exc_pending(ctx)) { free(cd_file); goto catch; }
    bool cd_exists = mc2_truthy(r); json_free(r); r = NULL;
    if (cd_exists) {
        r = mc2_read_text(ctx, cd_file, true);
        if (mc2_exc_pending(ctx)) {
            if (mc2_exc_matches(ctx, OSERR, 1)) { mc2_exc_clear(ctx); json_free(r); r = NULL; }
            else { free(cd_file); goto catch; }
        } else {
            s = mc2_strip_node(ctx, r);
            json_free(r); r = NULL;
            if (!s) { free(cd_file); goto catch; }  /* non-str -> AttributeError propagates */
            if (s[0]) {
                char *joined = mc2_path_join(git_dir, s);
                json_t *aa = json_object(); mc2_arg_str(aa, "path", joined);
                free(joined);
                json_t *res = mc2_call(ctx, "path.resolve", aa);
                if (mc2_exc_pending(ctx)) {
                    if (mc2_exc_matches(ctx, OSERR, 1)) { mc2_exc_clear(ctx); json_free(res); }
                    else { free(s); free(cd_file); goto catch; }
                } else {
                    free(common_dir);
                    common_dir = strdup(mc2_jstr(res));
                    json_free(res);
                }
            }
            free(s); s = NULL;
        }
    }
    free(cd_file);
    /* HEAD */
    char *head_file = mc2_path_join(git_dir, "HEAD");
    r = mc2_read_text(ctx, head_file, true);
    free(head_file);
    if (mc2_exc_pending(ctx)) goto catch;
    s = mc2_strip_node(ctx, r);
    json_free(r); r = NULL;
    if (!s) goto catch;
    if (mc2_startswith(s, "ref:")) {
        json_t *parts = mc2_split(s, ":", 1);
        const char *ref_raw = mc2_jstr(json_get(parts, 1));
        char *ref = mc2_strip(ref_raw);
        json_free(parts);
        const char *cands[2] = {git_dir, common_dir};
        bool found = false;
        for (int c = 0; c < 2 && !found; c++) {
            char *ref_file = mc2_path_join(cands[c], ref);
            json_t *aa = json_object(); mc2_arg_str(aa, "path", ref_file);
            r = mc2_call(ctx, "path.exists", aa);
            if (mc2_exc_pending(ctx)) { free(ref_file); free(ref); free(s); goto catch; }
            bool ex = mc2_truthy(r); json_free(r); r = NULL;
            if (ex) {
                json_t *txt = mc2_read_text(ctx, ref_file, true);
                if (mc2_exc_pending(ctx)) { free(ref_file); free(ref); free(s); goto catch; }
                char *sha = mc2_strip_node(ctx, txt);
                json_free(txt);
                if (!sha) { free(ref_file); free(ref); free(s); goto catch; }
                size_t n = strlen(ref) + strlen(sha) + 16;
                char *buf = (char *)malloc(n);
                snprintf(buf, n, "git:%s:%s", ref, sha);
                ret = json_string(buf);
                free(buf); free(sha); found = true;
            }
            free(ref_file);
        }
        if (!found) {
            json_t *aa = json_object();
            mc2_arg_str(aa, "common_dir", common_dir);
            mc2_arg_str(aa, "ref", ref);
            r = mc2_call(ctx, "_read_packed_ref", aa);
            if (mc2_exc_pending(ctx)) { free(ref); free(s); goto catch; }
            if (mc2_truthy(r)) {
                char *sha = mc2_pystr(r);
                size_t n = strlen(ref) + strlen(sha) + 16;
                char *buf = (char *)malloc(n);
                snprintf(buf, n, "git:%s:%s", ref, sha);
                ret = json_string(buf);
                free(buf); free(sha);
            } else {
                size_t n = strlen(ref) + 24;
                char *buf = (char *)malloc(n);
                snprintf(buf, n, "git:%s:unresolved", ref);
                ret = json_string(buf);
                free(buf);
            }
            json_free(r); r = NULL;
        }
        free(ref);
    } else {
        size_t n = strlen(s) + 16;
        char *buf = (char *)malloc(n);
        snprintf(buf, n, "git:HEAD:%s", s);
        ret = json_string(buf);
        free(buf);
    }
    free(s); s = NULL;
    goto done;
catch:
    if (mc2_exc_matches(ctx, OSERR, 1)) {
        mc2_exc_clear(ctx);
        json_free(ret); ret = json_null();
    }
    /* non-OSError propagates (exc stays pending) */
done:
    free(git_dir);
    free(common_dir);
    json_free(r);
    return ret;
}

/* ── auth store login probe ──────────────────────────────────────────────── */

/* PoP: mc2_auth_store_logged_in @ hermes_cli/main.py:_auth_store_logged_in */
static json_t *mc2_auth_store_logged_in(mc2_ctx *ctx, json_t *in) {
    const char *auth_file = json_get_str(in, "auth_file", "");
    json_t *registry = mc2_get(in, "registry");
    bool strict = json_get_bool(in, "strict_profile_scope", false);
    json_t *a = json_object(); mc2_arg_str(a, "path", auth_file);
    json_t *ex = mc2_call(ctx, "path.exists", a);
    if (mc2_exc_pending(ctx)) { json_free(ex); return NULL; }
    bool exists = mc2_truthy(ex); json_free(ex);
    if (!exists) return json_bool(false);
    /* try block */
    json_t *txt = mc2_read_text(ctx, auth_file, false);
    if (mc2_exc_pending(ctx)) goto suppress;
    a = json_object(); mc2_arg(a, "text", txt);
    json_t *auth = mc2_call(ctx, "json.loads", a);
    json_free(txt);
    if (mc2_exc_pending(ctx)) { json_free(auth); goto suppress; }
    json_t *active = mc2_dict_get(ctx, auth, "active_provider", NULL);
    if (mc2_exc_pending(ctx)) { json_free(auth); goto suppress; }
    char *soe = mc2_str_or_empty(active);
    char *stripped = mc2_strip(soe); free(soe);
    char *key = mc2_lower(stripped); free(stripped);
    json_t *active_config = mc2_get_def(registry, key, NULL);
    free(key);
    /* active_config.auth_type (double read; absent -> AttributeError) */
    bool cfg_api_key = false;
    bool cfg_present = mc2_double_truthy(active_config);
    if (cfg_present) {
        json_t *at = mc2_ns_get(ctx, active_config, "auth_type");
        if (mc2_exc_pending(ctx)) { json_free(auth); goto suppress; }
        cfg_api_key = mc2_str_eq(mc2_jstr(at), "api_key");
    }
    bool gate = mc2_truthy(active) && !(strict && cfg_present && cfg_api_key);
    if (gate) {
        a = json_object(); mc2_arg(a, "provider", active);
        json_t *status = mc2_call(ctx, "get_auth_status", a);
        if (mc2_exc_pending(ctx)) { json_free(status); json_free(auth); goto suppress; }
        json_t *li = mc2_dict_get(ctx, status, "logged_in", NULL);
        if (mc2_exc_pending(ctx)) { json_free(status); json_free(auth); goto suppress; }
        bool logged = mc2_truthy(li);
        json_free(status); json_free(auth);
        return json_bool(logged);
    }
    json_free(auth);
    return json_bool(false);
suppress:
    if (mc2_exc_is_exception(ctx)) mc2_exc_clear(ctx);
    else return NULL;  /* BaseException propagates */
    return json_bool(false);
}

/* ── workspace key resolver ──────────────────────────────────────────────── */

/* PoP: mc2_resolve_workspace_key @ hermes_cli/main.py:_resolve_workspace_key */
static json_t *mc2_resolve_workspace_key(mc2_ctx *ctx, json_t *in) {
    (void)in;
    /* try: subprocess.run(...) */
    json_t *a = json_object();
    json_t *argv = json_array();
    json_append(argv, json_string("git"));
    json_append(argv, json_string("rev-parse"));
    json_append(argv, json_string("--show-toplevel"));
    json_set(a, "argv", argv);
    mc2_arg_bool(a, "capture_output", true);
    mc2_arg_bool(a, "text", true);
    mc2_arg_str(a, "encoding", "utf-8");
    mc2_arg_str(a, "errors", "replace");
    json_set(a, "timeout", json_number(5));
    mc2_arg_bool(a, "check", false);
    json_t *result = mc2_call(ctx, "subprocess.run", a);
    if (mc2_exc_pending(ctx)) goto fallback;
    json_t *rc = mc2_ns_get(ctx, result, "returncode");
    if (mc2_exc_pending(ctx)) { json_free(result); goto fallback; }
    json_t *so = mc2_ns_get(ctx, result, "stdout");
    if (mc2_exc_pending(ctx)) { json_free(result); goto fallback; }
    bool rc_zero = mc2_is_num(rc) && rc->num_val == 0.0;
    char *stripped = NULL;
    if (rc_zero) {
        stripped = mc2_strip_node(ctx, so);
        if (!stripped) { json_free(result); goto fallback; }
    }
    if (rc_zero && stripped && stripped[0]) {
        json_t *aa = json_object(); mc2_arg_str(aa, "path", stripped);
        json_t *abs = mc2_call(ctx, "os.path.abspath", aa);
        free(stripped); json_free(result);
        if (mc2_exc_pending(ctx)) { json_free(abs); goto fallback; }
        return abs ? abs : json_null();
    }
    free(stripped);
    json_free(result);
fallback:
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) mc2_exc_clear(ctx);
        else return NULL;
    }
    /* try: return os.getcwd() */
    json_t *cwd = mc2_call(ctx, "os.getcwd", json_object());
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) { mc2_exc_clear(ctx); json_free(cwd); return json_null(); }
        json_free(cwd); return NULL;
    }
    return cwd ? cwd : json_null();
}

/* ── _session_db contextmanager (modelled as enter/body/finally) ─────────── */

typedef void (*mc2_body_fn)(mc2_ctx *ctx, json_t *reader, void *arg);

static void mc2_body_scripted(mc2_ctx *ctx, json_t *reader, void *arg) {
    (void)reader;
    json_t *in = (json_t *)arg;
    json_t *br = mc2_get(in, "body_raise");
    if (br && br->type == JSON_OBJECT && json_obj_get(br, "__raise__")) {
        if (ctx->exc) json_free(ctx->exc);
        ctx->exc = json_copy(br);
    }
}

/* PoP: mc2_session_db @ hermes_cli/main.py:_session_db */
static json_t *mc2_session_db(mc2_ctx *ctx, json_t *in, mc2_body_fn body, void *body_arg) {
    (void)in;
    json_t *store = NULL;
    json_t *reader = NULL;
    bool entered = false;
    json_t *r = NULL;
    /* setup try block */
    json_t *home = mc2_call(ctx, "get_hermes_home", json_object());
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) { mc2_exc_clear(ctx); json_free(home); goto yield_phase; }
        json_free(home); goto finally;
    }
    json_t *a = json_object(); mc2_arg(a, "home", home);
    store = mc2_call(ctx, "open_persistence_reader", a);
    json_free(home);
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) { mc2_exc_clear(ctx); json_free(store); store = NULL; goto yield_phase; }
        json_free(store); store = NULL; goto finally;
    }
    entered = true;  /* handles.enter_context(store) -> store.__enter__ (no seam) */
    r = mc2_call(ctx, "store.ready", json_object());
    json_free(r); r = NULL;
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) { mc2_exc_clear(ctx); goto yield_phase; }
        goto finally;
    }
    a = json_object(); mc2_arg(a, "store", store);
    reader = mc2_call(ctx, "SurfaceLabelReader", a);
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) { mc2_exc_clear(ctx); json_free(reader); reader = NULL; goto yield_phase; }
        json_free(reader); reader = NULL; goto finally;
    }
yield_phase:
    body(ctx, reader, body_arg);
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) mc2_exc_clear(ctx);  /* body errors swallowed */
        /* BaseException stays pending */
    }
finally:
    if (entered) {
        r = mc2_call(ctx, "store.close", json_object());
        json_free(r); r = NULL;
        /* a close raise replaces any in-flight exception */
    }
    json_free(store);
    if (mc2_exc_pending(ctx)) { json_free(reader); return NULL; }
    return reader ? reader : json_null();
}

/* ── chat session-arg normalization ──────────────────────────────────────── */

typedef struct { json_t *args; } mc2_chat_body_arg;

static void mc2_body_chat_restore(mc2_ctx *ctx, json_t *reader, void *arg) {
    json_t *args = ((mc2_chat_body_arg *)arg)->args;
    json_t *persistence = mc2_ns_get(ctx, reader, "persistence");
    if (mc2_exc_pending(ctx)) return;
    json_t *sessions = mc2_ns_get(ctx, persistence, "sessions");
    if (mc2_exc_pending(ctx)) return;
    (void)sessions;  /* presence validated; .get rides the sessions.get seam */
    json_t *resume = mc2_get(args, "resume");
    json_t *a = json_object(); mc2_arg(a, "session_id", resume);
    json_t *session = mc2_call(ctx, "sessions.get", a);
    if (mc2_exc_pending(ctx)) { json_free(session); return; }
    /* _saved_cwd = ((session.cwd if session else None) or "").strip() */
    char *saved = NULL;
    if (mc2_is_none(session)) {
        saved = strdup("");
    } else {
        json_t *cwd = mc2_ns_get(ctx, session, "cwd");
        if (mc2_exc_pending(ctx)) { json_free(session); return; }
        if (mc2_truthy(cwd) && !mc2_is_str(cwd)) {
            mc2_attr_raise(ctx, mc2_py_type_name(cwd), "strip");
            json_free(session); return;
        }
        char *soe = mc2_str_or_empty(cwd);
        saved = mc2_strip(soe);
        free(soe);
    }
    if (saved && saved[0]) {
        json_t *aa = json_object(); mc2_arg_str(aa, "path", saved);
        json_t *isd = mc2_call(ctx, "os.path.isdir", aa);
        if (mc2_exc_pending(ctx)) { free(saved); json_free(session); json_free(isd); return; }
        bool gone = !mc2_truthy(isd);
        json_free(isd);
        if (gone) {
            json_t *gc = mc2_call(ctx, "os.getcwd", json_object());
            if (mc2_exc_pending(ctx)) { free(saved); json_free(session); json_free(gc); return; }
            char *now = mc2_pystr(gc); json_free(gc);
            size_t n = strlen(saved) + strlen(now) + 80;
            char *buf = (char *)malloc(n);
            snprintf(buf, n, "\xE2\x9A\xA0 session's recorded dir is gone (%s); staying in %s", saved, now);
            mc2_print(ctx, buf, NULL);
            free(buf); free(now);
        } else {
            json_t *aa1 = json_object(); mc2_arg_str(aa1, "path", saved);
            json_t *rp1 = mc2_call(ctx, "os.path.realpath", aa1);
            if (mc2_exc_pending(ctx)) { free(saved); json_free(session); json_free(rp1); return; }
            json_t *gc = mc2_call(ctx, "os.getcwd", json_object());
            if (mc2_exc_pending(ctx)) { free(saved); json_free(session); json_free(rp1); json_free(gc); return; }
            char *now = mc2_pystr(gc);
            json_t *aa2 = json_object(); mc2_arg_str(aa2, "path", now);
            json_t *rp2 = mc2_call(ctx, "os.path.realpath", aa2);
            free(now);
            if (mc2_exc_pending(ctx)) { free(saved); json_free(session); json_free(rp1); json_free(gc); json_free(rp2); return; }
            bool differ = !mc2_json_eq(rp1, rp2);
            json_free(rp1); json_free(rp2); json_free(gc);
            if (differ) {
                json_t *aa3 = json_object(); mc2_arg_str(aa3, "path", saved);
                json_t *ch = mc2_call(ctx, "os.chdir", aa3);
                json_free(ch);
                if (mc2_exc_pending(ctx)) { free(saved); json_free(session); return; }
                size_t n = strlen(saved) + 48;
                char *buf = (char *)malloc(n);
                snprintf(buf, n, "\xE2\x86\xAA restored workspace dir: %s", saved);
                mc2_print(ctx, buf, NULL);
                free(buf);
            }
        }
    }
    free(saved);
    json_free(session);
}

/* PoP: mc2_resolve_chat_session_args @ hermes_cli/main.py:_resolve_chat_session_args */
static json_t *mc2_resolve_chat_session_args(mc2_ctx *ctx, json_t *in, json_t **args_out) {
    *args_out = NULL;
    json_t *args_src = mc2_get(in, "args");
    json_t *args = args_src ? json_copy(args_src) : json_object();
    bool use_tui = json_get_bool(in, "use_tui", false);
    json_t *r = NULL;
    json_t *a = NULL;

    a = json_object(); mc2_arg(a, "args", args);
    r = mc2_call(ctx, "_apply_in_dir", a); json_free(r);
    if (mc2_exc_pending(ctx)) goto done;

    json_t *rraw = mc2_get(args, "resume");
    if (mc2_is_str(rraw)) {
        char *st = mc2_strip(mc2_jstr(rraw));
        char *lo = mc2_lower(st); free(st);
        bool latest = mc2_str_eq(lo, "latest");
        free(lo);
        if (latest) {
            a = json_object(); mc2_arg_bool(a, "use_tui", use_tui);
            json_t *last = mc2_call(ctx, "_latest_session_id", a);
            if (mc2_exc_pending(ctx)) { json_free(last); goto done; }
            if (mc2_truthy(last)) {
                json_set(args, "resume", json_copy(last));
            } else {
                char buf[128];
                snprintf(buf, sizeof buf, "No previous %s session found to resume.", use_tui ? "TUI" : "CLI");
                mc2_print(ctx, buf, NULL);
                if (mc2_exc_pending(ctx)) { json_free(last); goto done; }
                mc2_print(ctx, "Use 'hermes sessions list' to see available sessions.", NULL);
                if (mc2_exc_pending(ctx)) { json_free(last); goto done; }
                json_free(last);
                mc2_sys_exit(ctx, json_number(1));
                goto done;
            }
            json_free(last);
        }
    }

    a = json_object(); mc2_arg(a, "args", args); mc2_arg_bool(a, "use_tui", use_tui);
    r = mc2_call(ctx, "_resolve_continue_arg", a); json_free(r);
    if (mc2_exc_pending(ctx)) goto done;

    a = json_object(); mc2_arg(a, "args", args);
    r = mc2_call(ctx, "import_foreign_resume", a); json_free(r);
    if (mc2_exc_pending(ctx)) goto done;

    json_t *resume_val = mc2_get(args, "resume");
    if (mc2_truthy(resume_val)) {
        a = json_object(); mc2_arg(a, "name_or_id", resume_val);
        json_t *res = mc2_call(ctx, "_resolve_session_by_name_or_id", a);
        if (mc2_exc_pending(ctx)) { json_free(res); goto done; }
        json_set(args, "resume", mc2_truthy(res) ? json_copy(res) : json_copy(resume_val));
        json_free(res);
    }

    a = json_object(); mc2_arg(a, "args", args);
    r = mc2_call(ctx, "recreate_resumed_tui_worktree", a); json_free(r);
    if (mc2_exc_pending(ctx)) goto done;

    if (mc2_truthy(mc2_get(args, "resume")) &&
        !mc2_truthy(mc2_get_def(args, "no_restore_cwd", NULL)) &&
        !mc2_truthy(mc2_get_def(args, "worktree", NULL))) {
        mc2_chat_body_arg ba = {args};
        json_t *db_ret = mc2_session_db(ctx, in, mc2_body_chat_restore, &ba);
        json_free(db_ret);
        if (mc2_exc_pending(ctx)) goto done;
    }
done:
    if (mc2_exc_pending(ctx)) { json_free(args); return NULL; }
    *args_out = args;
    return json_null();
}

/* ── xAI retirement warning ──────────────────────────────────────────────── */

/* PoP: mc2_warn_retired_xai_models @ hermes_cli/main.py:_warn_retired_xai_models */
static json_t *mc2_warn_retired_xai_models(mc2_ctx *ctx, json_t *in) {
    (void)in;
    const char *guide = json_get_str(ctx->env, "MIGRATION_GUIDE_URL", "");
    const char *date = json_get_str(ctx->env, "RETIREMENT_DATE", "");
    json_t *cfg = mc2_call(ctx, "load_config", json_object());
    if (mc2_exc_pending(ctx)) goto suppress;
    json_t *a = json_object(); mc2_arg(a, "config", cfg);
    json_t *refs = mc2_call(ctx, "find_retired_xai_refs", a);
    json_free(cfg);
    if (mc2_exc_pending(ctx)) { json_free(refs); goto suppress; }
    if (mc2_truthy(refs)) {
        size_t nrefs = mc2_is_arr(refs) ? refs->c.count : 0;
        char hdr[256];
        snprintf(hdr, sizeof hdr,
                 "\033[33m\xE2\x9A\xA0 xAI retires %zu model(s) in your config on %s:\033[0m\n",
                 nrefs, date);
        json_t *ht = json_object(); mc2_arg_str(ht, "text", hdr);
        json_t *w = mc2_call(ctx, "sys.stderr.write", ht);
        json_free(w);
        if (mc2_exc_pending(ctx)) { json_free(refs); goto suppress; }
        for (size_t i = 0; i < nrefs; i++) {
            json_t *aa = json_object(); mc2_arg(aa, "ref", refs->c.items[i]);
            json_t *issue = mc2_call(ctx, "format_issue", aa);
            if (mc2_exc_pending(ctx)) { json_free(issue); json_free(refs); goto suppress; }
            char *is = mc2_pystr(issue); json_free(issue);
            size_t n = strlen(is) + 48;
            char *buf = (char *)malloc(n);
            snprintf(buf, n, "  \033[33m\xE2\x9A\xA0\033[0m %s\n", is);
            free(is);
            json_t *t = json_object(); mc2_arg_str(t, "text", buf);
            w = mc2_call(ctx, "sys.stderr.write", t);
            free(buf); json_free(w);
            if (mc2_exc_pending(ctx)) { json_free(refs); goto suppress; }
        }
        char gbuf[512];
        snprintf(gbuf, sizeof gbuf, "  \033[2mMigration guide: %s\033[0m\n", guide);
        json_t *t = json_object(); mc2_arg_str(t, "text", gbuf);
        w = mc2_call(ctx, "sys.stderr.write", t); json_free(w);
        if (mc2_exc_pending(ctx)) { json_free(refs); goto suppress; }
        t = json_object(); mc2_arg_str(t, "text", "  \033[2mRun 'hermes doctor' for details.\033[0m\n\n");
        w = mc2_call(ctx, "sys.stderr.write", t); json_free(w);
        if (mc2_exc_pending(ctx)) { json_free(refs); goto suppress; }
    }
    json_free(refs);
    return json_null();
suppress:
    if (mc2_exc_is_exception(ctx)) mc2_exc_clear(ctx);
    else return NULL;
    return json_null();
}

/* ── background prefetch ─────────────────────────────────────────────────── */

/* inner _skills_dir_is_unseeded(): True/False; Exception -> False */
static bool mc2_skills_unseeded(mc2_ctx *ctx, bool *propagate) {
    *propagate = false;
    json_t *home = mc2_call(ctx, "get_hermes_home", json_object());
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) { mc2_exc_clear(ctx); json_free(home); return false; }
        *propagate = true; json_free(home); return false;
    }
    char *skills = mc2_path_join(mc2_pystr(home), "skills");
    json_free(home);
    json_t *a = json_object(); mc2_arg_str(a, "path", skills);
    json_t *isd = mc2_call(ctx, "path.is_dir", a);
    if (mc2_exc_pending(ctx)) {
        free(skills);
        if (mc2_exc_is_exception(ctx)) { mc2_exc_clear(ctx); json_free(isd); return false; }
        *propagate = true; json_free(isd); return false;
    }
    bool is_dir = mc2_truthy(isd); json_free(isd);
    if (!is_dir) { free(skills); return true; }
    a = json_object(); mc2_arg_str(a, "path", skills); mc2_arg_str(a, "pattern", "SKILL.md");
    free(skills);
    json_t *rg = mc2_call(ctx, "path.rglob", a);
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) { mc2_exc_clear(ctx); json_free(rg); return false; }
        *propagate = true; json_free(rg); return false;
    }
    bool unseeded = !(mc2_is_arr(rg) && rg->c.count > 0);
    json_free(rg);
    return unseeded;
}
/* inner _skills_sync_bg(): calls claimed sibling, Exception swallowed */
static void mc2_skills_sync_bg(mc2_ctx *ctx, bool *propagate) {
    *propagate = false;
    json_t *r = mc2_call(ctx, "_sync_bundled_skills_for_startup", json_object());
    json_free(r);
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) mc2_exc_clear(ctx);
        else *propagate = true;
    }
}

/* PoP: mc2_start_chat_background_prefetch @ hermes_cli/main.py:_start_chat_background_prefetch */
static json_t *mc2_start_chat_background_prefetch(mc2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *pf = mc2_call(ctx, "_termux_should_prefetch_update_check", json_object());
    if (mc2_exc_pending(ctx)) { json_free(pf); return NULL; }
    bool do_prefetch = mc2_truthy(pf); json_free(pf);
    if (do_prefetch) {
        json_t *r = mc2_call(ctx, "prefetch_update_check", json_object());
        json_free(r);
        if (mc2_exc_pending(ctx)) {
            if (mc2_exc_is_exception(ctx)) mc2_exc_clear(ctx);
            else return NULL;
        } else {
            r = mc2_call(ctx, "prefetch_banner_data", json_object());
            json_free(r);
            if (mc2_exc_pending(ctx)) {
                if (mc2_exc_is_exception(ctx)) mc2_exc_clear(ctx);
                else return NULL;
            }
        }
    }
    bool propagate = false;
    bool unseeded = mc2_skills_unseeded(ctx, &propagate);
    if (propagate) return NULL;
    if (unseeded) {
        mc2_skills_sync_bg(ctx, &propagate);
        if (propagate) return NULL;
        /* drop banner skills cache */
        json_t *r = mc2_call(ctx, "banner.set_cache", json_object());
        json_free(r);
        if (mc2_exc_pending(ctx)) {
            if (mc2_exc_is_exception(ctx)) mc2_exc_clear(ctx);
            else return NULL;
        }
    } else {
        json_t *a = json_object();
        mc2_arg_str(a, "name", "bundled-skills-sync");
        mc2_arg_bool(a, "daemon", true);
        mc2_arg_str(a, "target", "_skills_sync_bg");
        json_t *r = mc2_call(ctx, "thread.start", a);
        json_free(r);
        if (mc2_exc_pending(ctx)) return NULL;
    }
    return json_null();
}

/* ── forward-command factory ─────────────────────────────────────────────── */

/* PoP: mc2_forward_command @ hermes_cli/main.py:_forward_command */
static json_t *mc2_forward_command(mc2_ctx *ctx, json_t *in) {
    const char *name = json_get_str(in, "name", "");
    const char *module = json_get_str(in, "module", "");
    const char *attr = json_get_str(in, "attr", "");
    bool forward_return = json_get_bool(in, "forward_return", false);
    const char *doc = json_get_str(in, "doc", "");
    /* build _cmd meta */
    json_t *out = json_object();
    json_set(out, "name", json_string(name));
    json_set(out, "qualname", json_string(name));
    json_set(out, "doc", (doc && doc[0]) ? json_string(doc) : json_null());
    /* invoke _cmd(args): importlib.import_module(module) then getattr(...)(args) */
    json_t *a = json_object(); mc2_arg_str(a, "module", module);
    json_t *mod = mc2_call(ctx, "import_module", a);
    if (mc2_exc_pending(ctx)) { json_free(mod); json_free(out); return NULL; }
    json_free(mod);
    a = json_object();
    mc2_arg_str(a, "attr", attr);
    mc2_arg(a, "args", mc2_get(in, "call_args"));
    json_t *result = mc2_call(ctx, "forward.call", a);
    if (mc2_exc_pending(ctx)) { json_free(result); json_free(out); return NULL; }
    json_set(out, "call_ret", forward_return ? (result ? result : json_null()) : json_null());
    if (!forward_return) json_free(result);
    return out;
}

/* ── provider resolution ─────────────────────────────────────────────────── */

/* PoP: mc2_resolve_active_provider @ hermes_cli/main.py:_resolve_active_provider */
static json_t *mc2_resolve_active_provider(mc2_ctx *ctx, json_t *in) {
    json_t *config = mc2_get(in, "config");
    json_t *model_cfg = mc2_get(in, "model_cfg");
    const char *effective = json_get_str(in, "effective_provider", "");
    json_t *cmap = mc2_get(in, "custom_provider_map");
    json_t *active = json_string("");
    json_t *r = NULL;
    json_t *a = NULL;

    if (mc2_str_eq(effective, "custom") && mc2_is_obj(model_cfg)) {
        a = json_object(); mc2_arg_dict(ctx, a, "url", model_cfg, "base_url", "");
        json_t *cur = mc2_call(ctx, "_norm_base_url", a);
        if (mc2_exc_pending(ctx)) { json_free(cur); goto fail; }
        if (mc2_truthy(cur)) {
            if (mc2_is_obj(cmap)) {
                for (size_t i = 0; i < cmap->c.count; i++) {
                    json_t *info = cmap->c.items[i];
                    a = json_object(); mc2_arg_dict(ctx, a, "url", info, "base_url", "");
                    if (mc2_exc_pending(ctx)) { json_free(cur); goto fail; }
                    json_t *nb = mc2_call(ctx, "_norm_base_url", a);
                    if (mc2_exc_pending(ctx)) { json_free(cur); json_free(nb); goto fail; }
                    bool match = mc2_json_eq(nb, cur);
                    json_free(nb);
                    if (match) { active = mc2_replace(active, json_string(cmap->c.keys[i])); break; }
                }
            }
        }
        json_free(cur);
    }
    if (!mc2_truthy(active) && !mc2_str_eq(effective, "auto")) {
        a = json_object(); mc2_arg(a, "config", config);
        json_t *compat = mc2_call(ctx, "get_compatible_custom_providers", a);
        if (mc2_exc_pending(ctx)) { json_free(compat); goto fail; }
        a = json_object();
        mc2_arg_str(a, "provider", effective);
        mc2_arg(a, "providers", mc2_dict_get(ctx, config, "providers", NULL));
        if (mc2_exc_pending(ctx)) { json_free(compat); goto fail; }
        mc2_arg(a, "compatible", compat);
        json_t *def = mc2_call(ctx, "resolve_provider_full", a);
        json_free(compat);
        if (mc2_exc_pending(ctx)) { json_free(def); goto fail; }
        if (!mc2_is_none(def)) {
            json_t *id = mc2_ns_get(ctx, def, "id");
            if (mc2_exc_pending(ctx)) { json_free(def); goto fail; }
            active = mc2_replace(active, json_copy(id));
            json_t *src = mc2_ns_get(ctx, def, "source");
            if (mc2_exc_pending(ctx)) { json_free(def); goto fail; }
            if (mc2_str_eq(mc2_jstr(src), "user-config")) {
                char *soe = mc2_str_or_empty(active);
                char *st = mc2_strip(soe); free(soe);
                char *requested = mc2_lower(st); free(st);
                if (mc2_is_obj(cmap)) {
                    for (size_t i = 0; i < cmap->c.count; i++) {
                        json_t *info = cmap->c.items[i];
                        a = json_object();
                        mc2_arg_dict(ctx, a, "name", info, "name", "");
                        if (mc2_exc_pending(ctx)) { free(requested); json_free(def); goto fail; }
                        mc2_arg_dict(ctx, a, "provider_key", info, "provider_key", "");
                        if (mc2_exc_pending(ctx)) { free(requested); json_free(def); goto fail; }
                        json_t *aliases = mc2_call(ctx, "custom_provider_aliases", a);
                        if (mc2_exc_pending(ctx)) { free(requested); json_free(def); json_free(aliases); goto fail; }
                        bool in = false;
                        if (mc2_is_arr(aliases))
                            for (size_t j = 0; j < aliases->c.count; j++)
                                if (mc2_str_eq(mc2_jstr(aliases->c.items[j]), requested)) { in = true; break; }
                        json_free(aliases);
                        if (in) { active = mc2_replace(active, json_string(cmap->c.keys[i])); break; }
                    }
                }
                free(requested);
            }
            json_free(def);
        } else {
            json_free(def);
            a = json_object(); mc2_arg_str(a, "provider", effective);
            json_t *hint = mc2_call(ctx, "provider_install_hint", a);
            if (mc2_exc_pending(ctx)) { json_free(hint); goto fail; }
            char *h = mc2_truthy(hint) ? mc2_pystr(hint) :
                strdup("Check 'hermes model' for available providers, or run "
                       "'hermes doctor' to diagnose config issues.");
            json_free(hint);
            size_t n = strlen(effective) + strlen(h) + 80;
            char *buf = (char *)malloc(n);
            snprintf(buf, n, "Warning: Unknown provider '%s'. %s Falling back to auto provider detection.", effective, h);
            free(h);
            mc2_print(ctx, buf, NULL);
            free(buf);
            if (mc2_exc_pending(ctx)) goto fail;
        }
    }
    if (!mc2_truthy(active)) {
        a = json_object(); mc2_arg_str(a, "provider", "auto");
        r = mc2_call(ctx, "resolve_provider", a);
        if (mc2_exc_pending(ctx)) {
            static const char *AE[] = {"AuthError"};
            if (mc2_exc_matches(ctx, AE, 1)) {
                /* catching the AuthError removes it from propagation; save its
                 * code + projection for the handler body, then clear BEFORE any
                 * seam call so a handler print/format raise is a NEW exception. */
                char code[64];
                snprintf(code, sizeof code, "%s", json_get_str(ctx->exc, "code", ""));
                json_t *excproj = mc2_exc_arg(ctx);
                mc2_exc_clear(ctx);
                if (mc2_str_eq(code, "no_provider_configured")) {
                    mc2_print(ctx, "No provider is set up yet \xE2\x80\x94 pick one below. (Nous Portal works without an API key.)", NULL);
                    if (mc2_exc_pending(ctx)) { json_free(excproj); json_free(r); goto fail; }
                } else if (mc2_str_eq(effective, "auto")) {
                    a = json_object(); mc2_arg(a, "exc", excproj);
                    json_t *fa = mc2_call(ctx, "format_auth_error", a);
                    if (mc2_exc_pending(ctx)) { json_free(fa); json_free(excproj); json_free(r); goto fail; }
                    char *fas = mc2_pystr(fa); json_free(fa);
                    size_t n = strlen(fas) + 64;
                    char *buf = (char *)malloc(n);
                    snprintf(buf, n, "Warning: %s Falling back to auto provider detection.", fas);
                    free(fas);
                    mc2_print(ctx, buf, NULL);
                    free(buf);
                    if (mc2_exc_pending(ctx)) { json_free(excproj); json_free(r); goto fail; }
                }
                json_free(excproj);
                active = mc2_replace(active, json_null());
                json_free(r); r = NULL;
            } else {
                json_free(r); goto fail;
            }
        } else {
            active = mc2_replace(active, r ? r : json_null());
            r = NULL;
        }
    }
    if (mc2_str_eq(mc2_jstr(active), "openrouter")) {
        a = json_object(); mc2_arg_str(a, "name", "OPENAI_BASE_URL");
        json_t *ev = mc2_call(ctx, "get_env_value", a);
        if (mc2_exc_pending(ctx)) { json_free(ev); goto fail; }
        bool set = mc2_truthy(ev); json_free(ev);
        if (set) active = mc2_replace(active, json_string("custom"));
    }
    return active;
fail:
    json_free(active);
    return NULL;
}

/* ── provider picker ─────────────────────────────────────────────────────── */

/* PoP: mc2_pick_provider @ hermes_cli/main.py:_pick_provider */
static json_t *mc2_pick_provider(mc2_ctx *ctx, json_t *in) {
    json_t *config = mc2_get(in, "config");
    json_t *active = mc2_get(in, "active");
    json_t *labels = mc2_get(in, "provider_labels");
    json_t *cmap = mc2_get(in, "custom_provider_map");
    json_t *a = json_object();
    mc2_arg(a, "config", config);
    mc2_arg(a, "active", active);
    mc2_arg(a, "provider_labels", labels);
    mc2_arg(a, "custom_provider_map", cmap);
    json_t *rows = mc2_call(ctx, "_build_provider_picker_rows", a);
    if (mc2_exc_pending(ctx)) { json_free(rows); return NULL; }
    json_t *ordered = mc2_t0(rows);
    json_t *default_idx = mc2_t1(rows);
    /* [label for _, label, _ in ordered] */
    json_t *lab_arr = json_array();
    if (mc2_is_arr(ordered))
        for (size_t i = 0; i < ordered->c.count; i++) {
            json_t *row = json_get(ordered, (int)i);
            json_t *lab = mc2_is_arr(row) ? json_get(row, 1) : NULL;
            json_append(lab_arr, mc2_copy_or_null(lab));
        }
    a = json_object();
    json_set(a, "labels", lab_arr);
    mc2_arg(a, "default", default_idx);
    json_t *pidx = mc2_call(ctx, "_prompt_provider_choice", a);
    if (mc2_exc_pending(ctx)) { json_free(pidx); json_free(rows); return NULL; }
    if (mc2_is_none(pidx)) { json_free(pidx); json_free(rows); return json_null(); }
    json_t *row = mc2_index(ctx, ordered, pidx, "ordered");
    if (mc2_exc_pending(ctx)) { json_free(pidx); json_free(rows); return NULL; }
    json_t *key = mc2_is_arr(row) ? json_get(row, 0) : NULL;
    if (mc2_str_eq(mc2_jstr(key), "cancel")) { json_free(pidx); json_free(rows); return json_null(); }
    json_t *members = mc2_is_arr(row) ? json_get(row, 2) : NULL;
    if (!mc2_truthy(members)) {
        json_t *ret = mc2_copy_or_null(key);
        json_free(pidx); json_free(rows);
        return ret;
    }
    /* member labels: provider_labels.get(m, m) */
    json_t *mlab = json_array();
    for (size_t i = 0; i < members->c.count; i++) {
        const char *m = mc2_jstr(members->c.items[i]);
        json_t *lv = mc2_get_def(labels, m, NULL);
        json_append(mlab, lv ? json_copy(lv) : json_string(m));
    }
    /* default = members.index(active) if active in members else 0 */
    long def2 = 0;
    const char *act = mc2_jstr(active);
    for (size_t i = 0; i < members->c.count; i++)
        if (mc2_str_eq(mc2_jstr(members->c.items[i]), act)) { def2 = (long)i; break; }
    /* title = f"Select {group_label.split(' ▸',1)[0]} provider:" */
    json_t *glab = mc2_is_arr(row) ? json_get(row, 1) : NULL;
    json_t *parts = mc2_split(mc2_jstr(glab), " \xE2\x96\xB8", 1);
    const char *g0 = mc2_jstr(json_get(parts, 0));
    char *title = (char *)malloc(strlen(g0) + 32);
    sprintf(title, "Select %s provider:", g0);
    json_free(parts);
    a = json_object();
    json_set(a, "labels", mlab);
    json_set(a, "default", json_number((double)def2));
    mc2_arg_str(a, "title", title);
    free(title);
    json_t *midx = mc2_call(ctx, "_prompt_provider_choice", a);
    if (mc2_exc_pending(ctx)) { json_free(midx); json_free(pidx); json_free(rows); return NULL; }
    if (mc2_is_none(midx)) { json_free(midx); json_free(pidx); json_free(rows); return json_null(); }
    json_t *sel = mc2_index(ctx, members, midx, "members");
    if (mc2_exc_pending(ctx)) { json_free(midx); json_free(pidx); json_free(rows); return NULL; }
    json_t *ret = mc2_copy_or_null(sel);
    json_free(midx); json_free(pidx); json_free(rows);
    return ret;
}

/* ── dashboard runtime prep ──────────────────────────────────────────────── */

/* PoP: mc2_dashboard_prepare_runtime @ hermes_cli/main.py:_dashboard_prepare_runtime */
static json_t *mc2_dashboard_prepare_runtime(mc2_ctx *ctx, json_t *in) {
    json_t *args = mc2_get(in, "args");
    bool headless = json_get_bool(in, "headless_backend", false);
    json_t *r = NULL;
    json_t *a = NULL;
    /* setup_logging(mode="gui") — Exception swallowed */
    a = json_object(); mc2_arg_str(a, "mode", "gui");
    r = mc2_call(ctx, "setup_logging", a); json_free(r);
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) mc2_exc_clear(ctx);
        else return NULL;
    }
    /* _require_dashboard_web_deps() — propagates */
    r = mc2_call(ctx, "_require_dashboard_web_deps", json_object()); json_free(r);
    if (mc2_exc_pending(ctx)) return NULL;
    /* _sync_bundled_skills_quietly() — propagates */
    r = mc2_call(ctx, "_sync_bundled_skills_quietly", json_object()); json_free(r);
    if (mc2_exc_pending(ctx)) return NULL;
    /* apply_terminal_config_to_env() — Exception -> logger.debug(exc_info) */
    r = mc2_call(ctx, "apply_terminal_config_to_env", json_object()); json_free(r);
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) {
            mc2_exc_clear(ctx);
            mc2_log(ctx, "debug", "terminal config \xE2\x86\x92 env bridge failed for dashboard/serve", json_array(), true);
            if (mc2_exc_pending(ctx)) return NULL;
        } else return NULL;
    }
    /* _resolve_dashboard_web_dist(args, headless_backend) — propagates */
    a = json_object(); mc2_arg(a, "args", args); mc2_arg_bool(a, "headless_backend", headless);
    r = mc2_call(ctx, "_resolve_dashboard_web_dist", a); json_free(r);
    if (mc2_exc_pending(ctx)) return NULL;
    /* discover_plugins() — Exception -> print(stderr) */
    r = mc2_call(ctx, "discover_plugins", json_object()); json_free(r);
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) {
            char *es = strdup(json_get_str(ctx->exc, "msg", ""));
            mc2_exc_clear(ctx);
            size_t n = strlen(es) + 48;
            char *buf = (char *)malloc(n);
            snprintf(buf, n, "\xE2\x9A\xA0 Plugin discovery failed: %s", es);
            free(es);
            mc2_print(ctx, buf, "stderr");
            free(buf);
            if (mc2_exc_pending(ctx)) return NULL;
        } else return NULL;
    }
    /* _is_desktop_owned_backend() */
    json_t *desk = mc2_call(ctx, "_is_desktop_owned_backend", json_object());
    if (mc2_exc_pending(ctx)) { json_free(desk); return NULL; }
    bool desktop = mc2_truthy(desk); json_free(desk);
    if (headless && desktop) return json_bool(true);
    /* mcp discovery — Exception -> logger.debug(exc_info) */
    if (desktop) {
        a = json_object();
        mc2_arg_str(a, "logger", "logger");
        mc2_arg_str(a, "thread_name", "dashboard-mcp-discovery");
        r = mc2_call(ctx, "start_background_mcp_discovery", a); json_free(r);
    } else {
        a = json_object();
        mc2_arg_str(a, "logger", "logger");
        mc2_arg_str(a, "thread_name", "dashboard-mcp-discovery");
        r = mc2_call(ctx, "defer_background_mcp_discovery", a); json_free(r);
    }
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) {
            mc2_exc_clear(ctx);
            mc2_log(ctx, "debug", "Background MCP tool discovery failed at dashboard startup", json_array(), true);
            if (mc2_exc_pending(ctx)) return NULL;
        } else return NULL;
    }
    return json_bool(false);
}

/* ── argv / plugin-discovery leaves ──────────────────────────────────────── */

/* PoP: mc2_first_positional_argv @ hermes_cli/main.py:_first_positional_argv */
static json_t *mc2_first_positional_argv(mc2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *argv = mc2_get(ctx->env, "argv");
    json_t *a = json_object();
    if (argv) mc2_arg(a, "argv", argv);
    else json_set(a, "argv", json_array());  /* sys.argv[1:] defaults to [] */
    json_t *args = mc2_call(ctx, "command_argv", a);
    if (mc2_exc_pending(ctx)) { json_free(args); return NULL; }
    json_t *ret = (mc2_is_arr(args) && args->c.count > 0) ? json_copy(args->c.items[0]) : json_null();
    json_free(args);
    return ret;
}

/* PoP: mc2_resolve_deferred_platform_cli_command @ hermes_cli/main.py:_resolve_deferred_platform_cli_command */
static json_t *mc2_resolve_deferred_platform_cli_command(mc2_ctx *ctx, const char *command_name) {
    if (!command_name || !command_name[0]) return json_null();
    json_t *a = json_object(); mc2_arg_str(a, "name", command_name);
    json_t *r = mc2_call(ctx, "platform_registry.get", a);
    json_free(r);
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) {
            json_t *aa = json_array();
            json_append(aa, json_string(command_name));
            json_append(aa, mc2_exc_arg(ctx));
            mc2_exc_clear(ctx);
            mc2_log(ctx, "debug", "Deferred platform CLI resolution failed for %s: %s", aa, false);
        }
        /* BaseException propagates */
    }
    return json_null();
}

/* PoP: mc2_agent_subcommand_selected @ hermes_cli/main.py:_agent_subcommand_selected */
static json_t *mc2_agent_subcommand_selected(mc2_ctx *ctx, json_t *in) {
    json_t *args = mc2_get(in, "args");
    json_t *command = mc2_ns_get(ctx, args, "command");
    if (mc2_exc_pending(ctx)) return NULL;
    static const struct { const char *cmd, *attr; const char *set[2]; size_t n; } T[] = {
        {"cron", "cron_command", {"run", "tick"}, 2},
        {"gateway", "gateway_command", {"run"}, 1},
        {"curator", "curator_command", {"run"}, 1},
    };
    const char *c = mc2_jstr(command);
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++) {
        if (mc2_is_str(command) && mc2_str_eq(T[i].cmd, c)) {
            json_t *sub = mc2_get_def(args, T[i].attr, NULL);
            const char *sv = mc2_jstr(sub);
            bool in = false;
            for (size_t j = 0; j < T[i].n; j++)
                if (mc2_is_str(sub) && mc2_str_eq(T[i].set[j], sv)) { in = true; break; }
            return json_bool(in);
        }
    }
    return json_bool(false);
}

/* PoP: mc2_chat_runs_at_gateway @ hermes_cli/main.py:_chat_runs_at_gateway */
static json_t *mc2_chat_runs_at_gateway(mc2_ctx *ctx, json_t *in) {
    json_t *args = mc2_get(in, "args");
    json_t *command = mc2_ns_get(ctx, args, "command");
    if (mc2_exc_pending(ctx)) return NULL;
    bool yes = mc2_is_none(command) || (mc2_is_str(command) && mc2_str_eq(mc2_jstr(command), "chat"));
    return json_bool(yes);
}

/* PoP: mc2_apply_user_config_bypass @ hermes_cli/main.py:_apply_user_config_bypass */
static json_t *mc2_apply_user_config_bypass(mc2_ctx *ctx, json_t *in) {
    json_t *args = mc2_get(in, "args");
    json_t *flag = mc2_get_def(args, "ignore_user_config", NULL);
    if (mc2_truthy(flag)) {
        json_t *a = json_object();
        mc2_arg_str(a, "key", "HERMES_IGNORE_USER_CONFIG");
        mc2_arg_str(a, "value", "1");
        json_t *r = mc2_call(ctx, "os.environ.set", a);
        json_free(r);
        if (mc2_exc_pending(ctx)) return NULL;
    }
    return json_null();
}

/* ── launch leaves ───────────────────────────────────────────────────────── */

/* PoP: mc2_run_oneshot_from_args @ hermes_cli/main.py:_run_oneshot_from_args */
static json_t *mc2_run_oneshot_from_args(mc2_ctx *ctx, json_t *in) {
    json_t *a = json_object(); mc2_arg(a, "args", mc2_get(in, "args"));
    json_t *code = mc2_call(ctx, "launch_gateway_chat", a);
    if (mc2_exc_pending(ctx)) { json_free(code); return NULL; }
    mc2_sys_exit(ctx, code ? code : json_null());
    return NULL;
}

/* PoP: mc2_enable_cpu_profile @ hermes_cli/main.py:_enable_cpu_profile */
static json_t *mc2_enable_cpu_profile(mc2_ctx *ctx, json_t *in) {
    json_t *a = json_object(); mc2_arg(a, "args", mc2_get(in, "args"));
    json_t *r = mc2_call(ctx, "enable_cpu_profile_from_args", a);
    json_free(r);
    if (mc2_exc_pending(ctx)) return NULL;
    return json_null();
}

/* PoP: mc2_light_chat_parser @ hermes_cli/main.py:_light_chat_parser */
static json_t *mc2_light_chat_parser(mc2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *r = mc2_call(ctx, "build_top_level_parser", json_object());
    if (mc2_exc_pending(ctx)) { json_free(r); return NULL; }
    json_t *chat = mc2_is_arr(r) ? json_get(r, 2) : NULL;
    if (mc2_is_none(chat)) {
        mc2_attr_raise(ctx, "NoneType", "set_defaults");
        json_free(r); return NULL;
    }
    if (!mc2_is_obj(chat)) {
        mc2_attr_raise(ctx, mc2_py_type_name(chat), "set_defaults");
        json_free(r); return NULL;
    }
    json_t *a = json_object(); mc2_arg_str(a, "func", "cmd_chat");
    json_t *sd = mc2_call(ctx, "parser.set_defaults", a);
    json_free(sd);
    if (mc2_exc_pending(ctx)) { json_free(r); return NULL; }
    json_t *parser = mc2_is_arr(r) ? json_get(r, 0) : NULL;
    json_t *ret = mc2_copy_or_null(parser);
    json_free(r);
    return ret;
}

/* PoP: mc2_promote_top_level_resume @ hermes_cli/main.py:_promote_top_level_resume */
static json_t *mc2_promote_top_level_resume(mc2_ctx *ctx, json_t *in, json_t **args_out) {
    *args_out = NULL;
    json_t *args_src = mc2_get(in, "args");
    json_t *args = args_src ? json_copy(args_src) : json_object();
    json_t *resume = mc2_ns_get(ctx, args, "resume");
    if (mc2_exc_pending(ctx)) { json_free(args); return NULL; }
    bool trigger = mc2_truthy(resume);
    if (!trigger) {
        json_t *cont = mc2_ns_get(ctx, args, "continue_last");
        if (mc2_exc_pending(ctx)) { json_free(args); return NULL; }
        trigger = mc2_truthy(cont);
    }
    if (trigger) {
        json_t *command = mc2_ns_get(ctx, args, "command");
        if (mc2_exc_pending(ctx)) { json_free(args); return NULL; }
        if (mc2_is_none(command)) json_set(args, "command", json_string("chat"));
    }
    *args_out = args;
    return json_null();
}

/* PoP: mc2_try_fast_serve_launch @ hermes_cli/main.py:_try_fast_serve_launch */
static json_t *mc2_try_fast_serve_launch(mc2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *a = json_object(); mc2_arg_str(a, "name", "HERMES_DISABLE_FAST_SERVE_LAUNCH");
    json_t *dis = mc2_call(ctx, "os.environ.get", a);
    if (mc2_exc_pending(ctx)) { json_free(dis); return NULL; }
    bool disabled = mc2_is_str(dis) && mc2_str_eq(mc2_jstr(dis), "1");
    json_free(dis);
    if (disabled) return json_bool(false);
    json_t *argv = mc2_get(ctx->env, "argv");
    size_t argc = mc2_is_arr(argv) ? argv->c.count : 0;
    if (argc == 0 || !mc2_str_eq(mc2_jstr(argv->c.items[0]), "serve")) return json_bool(false);
    for (size_t i = 0; i < argc; i++) {
        const char *t = mc2_jstr(argv->c.items[i]);
        if (mc2_str_eq(t, "-h") || mc2_str_eq(t, "--help")) return json_bool(false);
    }
    /* container routing */
    json_t *ci = mc2_call(ctx, "get_container_exec_info", json_object());
    if (mc2_exc_pending(ctx)) {
        if (mc2_exc_is_exception(ctx)) { mc2_exc_clear(ctx); json_free(ci); return json_bool(false); }
        json_free(ci); return NULL;
    }
    bool container = mc2_truthy(ci); json_free(ci);
    if (container) return json_bool(false);
    /* build_serve_parser(cmd_dashboard=cmd_dashboard, add_help=False, exit_on_error=False) */
    a = json_object();
    mc2_arg_str(a, "cmd_dashboard", "cmd_dashboard");
    mc2_arg_bool(a, "add_help", false);
    mc2_arg_bool(a, "exit_on_error", false);
    json_t *parser = mc2_call(ctx, "build_serve_parser", a);
    if (mc2_exc_pending(ctx)) { json_free(parser); return NULL; }
    /* parser.parse_known_args(argv[1:]) */
    json_t *rest = json_array();
    for (size_t i = 1; i < argc; i++) json_append(rest, json_copy(argv->c.items[i]));
    a = json_object(); json_set(a, "argv", rest);
    json_t *pk = mc2_call(ctx, "parser.parse_known_args", a);
    json_free(parser);
    if (mc2_exc_pending(ctx)) {
        static const char *AV[] = {"ArgumentError", "ValueError"};
        if (mc2_exc_matches(ctx, AV, 2)) { mc2_exc_clear(ctx); json_free(pk); return json_bool(false); }
        json_free(pk); return NULL;
    }
    json_t *pargs = mc2_t0(pk);
    json_t *unknown = mc2_t1(pk);
    if (mc2_truthy(unknown)) { json_free(pk); return json_bool(false); }
    /* cmd_dashboard(args) */
    a = json_object(); mc2_arg(a, "args", pargs);
    json_t *cd = mc2_call(ctx, "cmd_dashboard", a);
    json_free(cd);
    json_free(pk);
    if (mc2_exc_pending(ctx)) return NULL;
    return json_bool(true);
}

/* ── plugin CLI registration ─────────────────────────────────────────────── */

/* PoP: mc2_attach_plugin_cli_command @ hermes_cli/main.py:_attach_plugin_cli_command */
static json_t *mc2_attach_plugin_cli_command(mc2_ctx *ctx, json_t *cmd_info) {
    json_t *name = mc2_get(cmd_info, "name");
    if (!name) { mc2_raise(ctx, "KeyError", "'name'"); return NULL; }
    json_t *help = mc2_get(cmd_info, "help");
    if (!help) { mc2_raise(ctx, "KeyError", "'help'"); return NULL; }
    json_t *a = json_object();
    mc2_arg(a, "name", name);
    mc2_arg(a, "help", help);
    mc2_arg_dict(ctx, a, "description", cmd_info, "description", "");
    mc2_arg_str(a, "formatter_class", "RawDescriptionHelpFormatter");
    json_t *parser = mc2_call(ctx, "subparsers.add_parser", a);
    if (mc2_exc_pending(ctx)) { json_free(parser); return NULL; }
    /* cmd_info["setup_fn"](plugin_parser) */
    json_t *setup = mc2_get(cmd_info, "setup_fn");
    if (!setup) { mc2_raise(ctx, "KeyError", "'setup_fn'"); json_free(parser); return NULL; }
    if (mc2_is_none(setup)) { mc2_raise(ctx, "TypeError", "'NoneType' object is not callable"); json_free(parser); return NULL; }
    a = json_object(); mc2_arg(a, "parser", parser);
    json_t *sr = mc2_call(ctx, "plugin.setup_fn", a);
    json_free(sr);
    if (mc2_exc_pending(ctx)) { json_free(parser); return NULL; }
    /* if cmd_info.get("handler_fn") is not None: set_defaults(func=handler_fn) */
    json_t *handler = mc2_get_def(cmd_info, "handler_fn", NULL);
    if (!mc2_is_none(handler)) {
        a = json_object(); mc2_arg_str(a, "func", mc2_jstr(handler));
        json_t *sd = mc2_call(ctx, "parser.set_defaults", a);
        json_free(sd);
        if (mc2_exc_pending(ctx)) { json_free(parser); return NULL; }
    }
    json_free(parser);
    return json_null();
}

/* PoP: mc2_register_plugin_cli_commands @ hermes_cli/main.py:_register_plugin_cli_commands */
static json_t *mc2_register_plugin_cli_commands(mc2_ctx *ctx, json_t *in) {
    (void)in;
    json_t *need = mc2_call(ctx, "_plugin_cli_discovery_needed", json_object());
    if (mc2_exc_pending(ctx)) { json_free(need); return NULL; }
    bool needed = mc2_truthy(need); json_free(need);
    if (!needed) return json_null();
    /* try block */
    json_t *fp = mc2_first_positional_argv(ctx, in);
    if (mc2_exc_pending(ctx)) { json_free(fp); goto suppress; }
    json_t *a = json_object(); mc2_arg(a, "command", fp);
    json_t *rec = mc2_call(ctx, "recover_for_command", a);
    json_free(rec); json_free(fp);
    if (mc2_exc_pending(ctx)) goto suppress;
    json_t *cmds = mc2_call(ctx, "discover_plugin_cli_commands", json_object());
    if (mc2_exc_pending(ctx)) { json_free(cmds); goto suppress; }
    json_t *seen = json_array();
    if (mc2_is_arr(cmds)) {
        for (size_t i = 0; i < cmds->c.count; i++) {
            json_t *ci = cmds->c.items[i];
            json_t *ar = mc2_attach_plugin_cli_command(ctx, ci);
            json_free(ar);
            if (mc2_exc_pending(ctx)) { json_free(cmds); json_free(seen); goto suppress; }
            json_t *nm = mc2_get(ci, "name");
            if (nm) json_append(seen, json_copy(nm));
        }
    }
    json_free(cmds);
    json_t *dp = mc2_call(ctx, "discover_plugins", json_object());
    json_free(dp);
    if (mc2_exc_pending(ctx)) { json_free(seen); goto suppress; }
    fp = mc2_first_positional_argv(ctx, in);
    if (mc2_exc_pending(ctx)) { json_free(fp); json_free(seen); goto suppress; }
    json_t *dr = mc2_resolve_deferred_platform_cli_command(ctx, mc2_jstr(fp));
    json_free(dr); json_free(fp);
    if (mc2_exc_pending(ctx)) { json_free(seen); goto suppress; }
    json_t *mgr = mc2_call(ctx, "get_plugin_manager", json_object());
    if (mc2_exc_pending(ctx)) { json_free(mgr); json_free(seen); goto suppress; }
    json_t *clc = mc2_get(mgr, "_cli_commands");
    if (mc2_is_obj(clc)) {
        for (size_t i = 0; i < clc->c.count; i++) {
            json_t *ci = clc->c.items[i];
            json_t *nm = mc2_get(ci, "name");
            bool already = false;
            if (nm)
                for (size_t j = 0; j < seen->c.count; j++)
                    if (mc2_json_eq(seen->c.items[j], nm)) { already = true; break; }
            if (!already) {
                json_t *ar = mc2_attach_plugin_cli_command(ctx, ci);
                json_free(ar);
                if (mc2_exc_pending(ctx)) { json_free(mgr); json_free(seen); goto suppress; }
            }
        }
    }
    json_free(mgr); json_free(seen);
    return json_null();
suppress:
    if (mc2_exc_is_exception(ctx)) {
        json_t *aa = json_array();
        json_append(aa, mc2_exc_arg(ctx));
        mc2_exc_clear(ctx);
        mc2_log(ctx, "debug", "Plugin CLI discovery failed: %s", aa, false);
        if (mc2_exc_pending(ctx)) return NULL;
        return json_null();
    }
    return NULL;  /* BaseException propagates */
}