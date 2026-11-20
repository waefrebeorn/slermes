/* main_cli_flow3.c — FLOW port of the FINAL UNCLAIMED remainder of
 * hermes_cli/main.py (after port_main_cli_flow.c / main_cli_flow2.c /
 * main_u_* / mn_* / main_gaps / main_helpers claims; PoP-grep verified).
 *
 * Six targets (the quarry main.py was refactored — subcommand handlers moved
 * to hermes_cli/subcommands/*; what remains unclaimed here):
 *   - _build_cli_parser : the full argparse registration tree (ORDER is the
 *     contract; ~60 builder seams + LSP try/except + kanban/project
 *     set_defaults + claimed _register_plugin_cli_commands seamed).
 *   - main              : the entry-point decision sequence; composes the
 *     ported _build_cli_parser REAL, every claimed sibling seamed.
 *   - _skills_dir_is_unseeded / _skills_sync_bg : nested closures of the
 *     claimed _start_chat_background_prefetch (free-var-free; extracted).
 *   - _cmd              : nested closure of the claimed _forward_command;
 *     captures (module, attr, forward_return) ride as explicit inputs.
 *   - _custody_refusal  : nested closure of the claimed cmd_update.
 *
 * ALL collaborator I/O (claimed siblings, foreign-module imports, os/sys
 * reads, print, logger, path methods, builder registrations) rides the
 * injected universal seam mc3_call; the harness scripts results and records
 * the ordered trace the oracle diffs against LIVE hermes_cli/main.py under
 * the same scripts. main -> _build_cli_parser is REAL internal composition.
 *
 * Representation contract (shared with tests/oracle_main_cli_flow3.py):
 *   - parser/subparsers/chat_parser/args/custody/module doubles are
 *     present-non-null JSON records (types.SimpleNamespace live), ALWAYS
 *     truthy even {}; plain dict inputs (container_info) use JSON
 *     truthiness. tuple -> array; None -> JSON null; integral floats
 *     collapse to ints on both sides; Path -> POSIX str.
 *   - the args double's "func" field is a STRING handler marker (live: a
 *     callable). norm() drops callables from SimpleNamespace projections,
 *     so seam args carrying the args double omit a STRING func; a
 *     present-null func survives as null (mc3_arg_args). func null ->
 *     hasattr True -> TypeError 'NoneType' object is not callable; func
 *     non-str non-null -> TypeError '<type>' ... callable.
 *   - uncaught exception -> {"__raise__": cls}; an exception passed as a
 *     logger arg projects to {"__raise__": cls, "msg": str(e)}.
 *   - scripts (FAIL-CLOSED): absent key -> RuntimeError("no script for
 *     <name>"); present null -> None; {"$seq":[...]} per-call queue
 *     (exhaustion raises RuntimeError("script exhausted: <name>"));
 *     {"__raise__":cls,"msg":m,...} raises (trace ret is the WHOLE marker);
 *     any other value reused. None-valued seam arg keys dropped BOTH sides.
 *   - in-function import failures ride env.import_fails (array of module
 *     names): the import raises ModuleNotFoundError("import of <name>
 *     halted; None in sys.modules") — the py3.14 sys.modules-None message.
 *     env._stdio_repaired models `from hermes_cli import _stdio_repaired`.
 *   - fixture domain: ASCII operands; home is a normalized absolute POSIX
 *     path (no trailing slash / dot components); path.rglob scripts are
 *     lists (next(iter(list), None) models the first match); import_module
 *     scripts are dicts WITHOUT the forwarded attr key (getattr falls to
 *     the recording ForwardFn); build_top_level_parser scripts are 3-lists;
 *     rc values are ints/bools/strs/null/non-integral floats (integral
 *     floats diverge at the isinstance(rc, int) gate — libjson has no
 *     int/float identity); exception markers name builtins constructible
 *     with one str message (no ExceptionGroup family).
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "cli/main_cli_flow3.c". mc3_ prefix (mc2_ is flow2, mcl_ flow1).
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct mc3_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted constants/flags */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} mc3_ctx;

static bool mc3_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool mc3_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool mc3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool mc3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool mc3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool mc3_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool mc3_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool mc3_truthy(const json_t *v) {
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

static const char *mc3_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* borrowed object field or NULL */
static json_t *mc3_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
/* dict.get(key, default): present-null keeps the null node; absent -> default. */
static json_t *mc3_get_def(const json_t *obj, const char *key, json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}

/* Python type name for a JSON node (integral double reads int). */
static const char *mc3_py_type_name(const json_t *v) {
    if (mc3_is_none(v)) return "NoneType";
    if (mc3_is_bool(v)) return "bool";
    if (mc3_is_num(v)) {
        double d = v->num_val;
        return (d == floor(d) && fabs(d) < 1e15) ? "int" : "float";
    }
    if (mc3_is_str(v)) return "str";
    if (mc3_is_arr(v)) return "list";
    if (mc3_is_obj(v)) return "dict";
    return "object";
}

/* str(v) for a scalar (null -> "None"). Caller frees. */
static char *mc3_pystr(const json_t *v) {
    if (mc3_is_none(v)) return strdup("None");
    if (mc3_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (mc3_is_num(v)) {
        char buf[64]; double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    if (mc3_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* PurePosixPath(a) / b — domain: normalized absolute POSIX a, atomic b. */
static char *mc3_path_join(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    if (b[0] == '/') return strdup(b);
    size_t n = strlen(a);
    while (n > 1 && a[n - 1] == '/') n--;
    bool root = (n == 1 && a[0] == '/');
    bool empty = (n == 0);
    size_t bl = strlen(b);
    char *r = (char *)malloc(n + bl + 2);
    if (!r) return NULL;
    memcpy(r, a, n);
    size_t k = n;
    if (!empty && !root) r[k++] = '/';
    else if (root) r[k++] = '/';
    memcpy(r + k, b, bl);
    r[k + bl] = '\0';
    return r;
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

static bool mc3_exc_pending(const mc3_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void mc3_exc_clear(mc3_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void mc3_raise(mc3_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *mc3_exc_class(const mc3_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
/* FULL builtin exception hierarchy (py3.14). Single-parent projection;
 * ExceptionGroup's Exception-side parent is listed (the BaseExceptionGroup
 * family is outside the fixture domain — make_exc builds synthetic
 * Exception subclasses for unknown names). */
static const char *mc3_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"Exception", "BaseException"},
        {"BaseExceptionGroup", "BaseException"}, {"ExceptionGroup", "Exception"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"ZeroDivisionError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"AttributeError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"},
        {"KeyError", "LookupError"}, {"MemoryError", "Exception"},
        {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"OSError", "Exception"}, {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"FileExistsError", "OSError"}, {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"}, {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"}, {"SystemError", "Exception"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"Warning", "Exception"}, {"BytesWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"EncodingWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"}, {"ResourceWarning", "Warning"},
        {"RuntimeWarning", "Warning"}, {"SyntaxWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"UserWarning", "Warning"},
        {"JSONDecodeError", "ValueError"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (mc3_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool mc3_exc_matches(const mc3_ctx *ctx, const char *const *classes, size_t n) {
    if (!mc3_exc_pending(ctx)) return false;
    const char *cls = mc3_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (mc3_str_eq(cls, classes[i])) return true;
        const char *p = mc3_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception catches EVERY Exception descendant (incl. synthetic
 * make_exc classes) but NOT the BaseException trio. Exclusion model: any
 * kind that is not SystemExit/KeyboardInterrupt/GeneratorExit/BaseException
 * is an Exception subclass in the fixture domain. */
static bool mc3_exc_is_exception(const mc3_ctx *ctx) {
    if (!mc3_exc_pending(ctx)) return false;
    const char *cls = mc3_exc_class(ctx);
    return !(mc3_str_eq(cls, "SystemExit") || mc3_str_eq(cls, "KeyboardInterrupt") ||
             mc3_str_eq(cls, "GeneratorExit") || mc3_str_eq(cls, "BaseException"));
}
/* AttributeError "'<type>' object has no attribute '<attr>'". */
static void mc3_attr_raise(mc3_ctx *ctx, const char *type, const char *attr) {
    char msg[200];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", type, attr);
    mc3_raise(ctx, "AttributeError", msg);
}
/* direct attribute read on a SimpleNamespace double (raises when absent). */
static json_t *mc3_ns_get(mc3_ctx *ctx, json_t *obj, const char *key) {
    if (mc3_is_none(obj)) { mc3_attr_raise(ctx, "NoneType", key); return NULL; }
    if (!mc3_is_obj(obj)) { mc3_attr_raise(ctx, mc3_py_type_name(obj), key); return NULL; }
    json_t *v = json_obj_get(obj, key);
    if (!v) { mc3_attr_raise(ctx, "types.SimpleNamespace", key); return NULL; }
    return v;
}

/* ── universal seam (FAIL-CLOSED) ────────────────────────────────────────── */

static json_t *mc3_script_result(mc3_ctx *ctx, const json_t *script) {
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
static json_t *mc3_call(mc3_ctx *ctx, const char *name, json_t *args) {
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
    if (chosen != ctx->exc) ret = mc3_script_result(ctx, chosen);
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

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). Borrowed
 * val is copied; the caller keeps ownership of the source node. */
static void mc3_arg(json_t *args, const char *key, const json_t *val) {
    if (!val || val->type == JSON_NULL) return;
    json_set(args, key, json_copy(val));
}
static void mc3_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
/* the args-double projection: live norm() drops the callable "func" field
 * (a STRING handler marker materializes to a callable) from every
 * SimpleNamespace echo; a present-NULL func is NOT callable and survives as
 * null inside the projected record. */
static void mc3_arg_args(json_t *args, const char *key, const json_t *args_node) {
    if (!args_node || args_node->type == JSON_NULL) return;
    if (args_node->type != JSON_OBJECT) { json_set(args, key, json_copy(args_node)); return; }
    json_t *proj = json_object();
    for (size_t i = 0; i < args_node->c.count; i++) {
        if (mc3_str_eq(args_node->c.keys[i], "func") && mc3_is_str(args_node->c.items[i])) continue;
        json_set(proj, args_node->c.keys[i], json_copy(args_node->c.items[i]));
    }
    json_set(args, key, proj);
}
/* logger seam: {"msg","args"[,"exc_info"]}. args_arr ownership transferred. */
static void mc3_log(mc3_ctx *ctx, const char *level, const char *msg,
                    json_t *args_arr, bool exc_info) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) json_set(a, "exc_info", json_bool(true));
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = mc3_call(ctx, name, a);
    json_free(r);
}
/* project the pending exception as a logger arg node {"__raise__","msg"}. */
static json_t *mc3_exc_arg(const mc3_ctx *ctx) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(mc3_exc_class(ctx)));
    json_set(e, "msg", json_string(json_get_str(ctx->exc, "msg", "")));
    return e;
}
/* print seam: {"text"[,"file"]}. file NULL -> stdout (key dropped). */
static void mc3_print(mc3_ctx *ctx, const char *text, const char *file) {
    json_t *a = json_object();
    mc3_arg_str(a, "text", text);
    if (file) mc3_arg_str(a, "file", file);
    json_t *r = mc3_call(ctx, "print", a);
    json_free(r);
}
/* sys.exit seam: records then raises SystemExit(str(code)) unless the script
 * itself raised. code ownership TRANSFERRED (freed here). */
static void mc3_sys_exit(mc3_ctx *ctx, json_t *code) {
    json_t *a = json_object();
    mc3_arg(a, "code", code);
    json_t *r = mc3_call(ctx, "sys.exit", a);
    json_free(r);
    if (!mc3_exc_pending(ctx)) {
        char *m = mc3_pystr(code);
        mc3_raise(ctx, "SystemExit", m);
        free(m);
    }
    json_free(code);
}
/* in-function import gate: env.import_fails lists module names whose import
 * raises ModuleNotFoundError (py3.14 sys.modules-None halt message). */
static bool mc3_import_gate(mc3_ctx *ctx, const char *mod) {
    json_t *fails = mc3_get(ctx->env, "import_fails");
    if (mc3_is_arr(fails)) {
        for (size_t i = 0; i < fails->c.count; i++) {
            json_t *m = fails->c.items[i];
            if (mc3_is_str(m) && mc3_str_eq(m->str_val, mod)) {
                char msg[256];
                snprintf(msg, sizeof msg, "import of %s halted; None in sys.modules", mod);
                mc3_raise(ctx, "ModuleNotFoundError", msg);
                return true;
            }
        }
    }
    return false;
}
/* tuple result helpers (borrowed). */
static json_t *mc3_t0(const json_t *r) { return mc3_is_arr(r) ? json_get(r, 0) : NULL; }
static json_t *mc3_t1(const json_t *r) { return mc3_is_arr(r) ? json_get(r, 1) : NULL; }
static json_t *mc3_t2(const json_t *r) { return mc3_is_arr(r) ? json_get(r, 2) : NULL; }

/* isinstance(rc, int) and rc != 0 — bool IS an int subclass; integral
 * floats are outside the fixture domain (libjson collapses them). */
static bool mc3_rc_nonzero_int(const json_t *rc) {
    if (mc3_is_bool(rc)) return rc->bool_val;
    if (mc3_is_num(rc)) {
        double d = rc->num_val;
        return d == floor(d) && fabs(d) < 1e15 && d != 0.0;
    }
    return false;
}

/* ── nested closures of _start_chat_background_prefetch ──────────────────── */

/* PoP: mc3_skills_dir_is_unseeded @ hermes_cli/main.py:_skills_dir_is_unseeded */
static json_t *mc3_skills_dir_is_unseeded(mc3_ctx *ctx, json_t *in) {
    (void)in;
    json_t *home = NULL;
    json_t *r = NULL;
    bool result = false;
    /* try: from hermes_cli.config import get_hermes_home */
    if (mc3_import_gate(ctx, "hermes_cli.config")) goto suppress;
    home = mc3_call(ctx, "get_hermes_home", json_object());
    if (mc3_exc_pending(ctx)) goto suppress;
    /* Path(home): non-str raises TypeError (swallowed) — domain home is str */
    if (!mc3_is_str(home)) goto suppress;
    char *skills_dir = mc3_path_join(mc3_jstr(home), "skills");
    /* if not skills_dir.is_dir(): return True */
    json_t *a = json_object(); mc3_arg_str(a, "path", skills_dir);
    r = mc3_call(ctx, "path.is_dir", a);
    if (mc3_exc_pending(ctx)) { free(skills_dir); goto suppress; }
    bool is_dir = mc3_truthy(r);
    json_free(r); r = NULL;
    if (!is_dir) { free(skills_dir); result = true; goto done; }
    /* return next(skills_dir.rglob("SKILL.md"), None) is None */
    a = json_object();
    mc3_arg_str(a, "path", skills_dir);
    mc3_arg_str(a, "pattern", "SKILL.md");
    free(skills_dir);
    r = mc3_call(ctx, "path.rglob", a);
    if (mc3_exc_pending(ctx)) goto suppress;
    /* next(iter, None): non-iterator raises TypeError (swallowed) */
    if (!mc3_is_arr(r)) goto suppress;
    json_t *first = r->c.count > 0 ? r->c.items[0] : NULL;
    result = mc3_is_none(first);
    goto done;
suppress:
    /* except Exception: return False (BaseException propagates). Reached
     * with a pending Exception marker OR with no pending exc at all (the
     * Path(non-str) / next(non-iterator) TypeError paths) — both -> False. */
    if (mc3_exc_pending(ctx) && !mc3_exc_is_exception(ctx)) {
        json_free(home); json_free(r);
        return NULL;
    }
    mc3_exc_clear(ctx);
    result = false;
    goto done;
done:
    json_free(home); json_free(r);
    return json_bool(result);
}

/* PoP: mc3_skills_sync_bg @ hermes_cli/main.py:_skills_sync_bg */
static json_t *mc3_skills_sync_bg(mc3_ctx *ctx, json_t *in) {
    (void)in;
    /* try: _sync_bundled_skills_for_startup() */
    json_t *r = mc3_call(ctx, "_sync_bundled_skills_for_startup", json_object());
    json_free(r);
    if (mc3_exc_pending(ctx)) {
        /* except Exception: pass (BaseException propagates) */
        if (mc3_exc_is_exception(ctx)) { mc3_exc_clear(ctx); return json_null(); }
        return NULL;
    }
    return json_null();
}

/* ── nested closure of _forward_command ──────────────────────────────────── */

/* PoP: mc3_forward_cmd @ hermes_cli/main.py:_cmd */
static json_t *mc3_forward_cmd(mc3_ctx *ctx, json_t *in) {
    const char *module = json_get_str(in, "module", "");
    const char *attr = json_get_str(in, "attr", "");
    bool forward_return = json_get_bool(in, "forward_return", false);
    /* import importlib */
    if (mc3_import_gate(ctx, "importlib")) return NULL;
    /* getattr(importlib.import_module(module), attr)(args) — domain: the
     * module double never carries attr, so getattr falls to ForwardFn. */
    json_t *a = json_object(); mc3_arg_str(a, "module", module);
    json_t *mod = mc3_call(ctx, "import_module", a);
    if (mc3_exc_pending(ctx)) { json_free(mod); return NULL; }
    json_free(mod);
    a = json_object();
    mc3_arg_str(a, "attr", attr);
    mc3_arg(a, "args", mc3_get(in, "args"));
    json_t *result = mc3_call(ctx, "forward.call", a);
    if (mc3_exc_pending(ctx)) { json_free(result); return NULL; }
    /* return result if forward_return else None */
    if (forward_return) return result ? result : json_null();
    json_free(result);
    return json_null();
}

/* ── nested closure of cmd_update ────────────────────────────────────────── */

/* PoP: mc3_custody_refusal @ hermes_cli/main.py:_custody_refusal */
static json_t *mc3_custody_refusal(mc3_ctx *ctx, json_t *in) {
    (void)in;
    /* custody = sys.modules.get("hermes_cli.update_custody") */
    json_t *a = json_object();
    mc3_arg_str(a, "name", "hermes_cli.update_custody");
    json_t *custody = mc3_call(ctx, "sys.modules.get", a);
    if (mc3_exc_pending(ctx)) { json_free(custody); return NULL; }
    /* return custody.refusal_notice() if custody is not None else None */
    if (mc3_is_none(custody)) { json_free(custody); return json_null(); }
    if (!mc3_is_obj(custody)) {
        mc3_attr_raise(ctx, mc3_py_type_name(custody), "refusal_notice");
        json_free(custody);
        return NULL;
    }
    json_free(custody);
    json_t *r = mc3_call(ctx, "custody.refusal_notice", json_object());
    if (mc3_exc_pending(ctx)) { json_free(r); return NULL; }
    return r ? r : json_null();
}

/* ── _build_cli_parser: the argparse registration tree ───────────────────── */

/* one builder registration: build_x(subparsers[, cmd_x=cmd_x...]). The cmd
 * kwarg VALUES are claimed-sibling handlers projected by __name__ (identity
 * with the kwarg name except cmd_sessions -> cmd_sessions_lazy). Returns 0
 * ok / -1 pending raise. */
static int mc3_builder(mc3_ctx *ctx, const char *name, const json_t *sp,
                       const char *const *kwnames, const char *const *kwvals,
                       size_t nkw) {
    json_t *a = json_object();
    mc3_arg(a, "subparsers", sp);
    for (size_t i = 0; i < nkw; i++) mc3_arg_str(a, kwnames[i], kwvals[i]);
    json_t *r = mc3_call(ctx, name, a);
    json_free(r);
    return mc3_exc_pending(ctx) ? -1 : 0;
}
static int mc3_builder0(mc3_ctx *ctx, const char *name, const json_t *sp) {
    return mc3_builder(ctx, name, sp, NULL, NULL, 0);
}
static int mc3_builder1(mc3_ctx *ctx, const char *name, const json_t *sp,
                        const char *kw, const char *val) {
    const char *kn[1] = { kw };
    const char *kv[1] = { val };
    return mc3_builder(ctx, name, sp, kn, kv, 1);
}

/* parser_double.set_defaults(func=<handler>) — receiver must be a record. */
static int mc3_set_defaults(mc3_ctx *ctx, const json_t *p, const char *func) {
    if (mc3_is_none(p)) { mc3_attr_raise(ctx, "NoneType", "set_defaults"); return -1; }
    if (!mc3_is_obj(p)) { mc3_attr_raise(ctx, mc3_py_type_name(p), "set_defaults"); return -1; }
    json_t *a = json_object(); mc3_arg_str(a, "func", func);
    json_t *r = mc3_call(ctx, "parser.set_defaults", a);
    json_free(r);
    return mc3_exc_pending(ctx) ? -1 : 0;
}

/* the LSP optional-registration try/except (failure must not break the CLI). */
static int mc3_lsp_block(mc3_ctx *ctx, const json_t *sp) {
    if (mc3_import_gate(ctx, "agent.lsp.cli")) goto suppress;
    json_t *a = json_object(); mc3_arg(a, "subparsers", sp);
    json_t *r = mc3_call(ctx, "lsp.register_subparser", a);
    json_free(r);
    if (mc3_exc_pending(ctx)) goto suppress;
    return 0;
suppress:
    /* except Exception as _lsp_err: logger.debug("...: %s", _lsp_err) */
    if (mc3_exc_is_exception(ctx)) {
        json_t *aa = json_array();
        json_append(aa, mc3_exc_arg(ctx));
        mc3_exc_clear(ctx);  /* CLEAR before the handler body */
        mc3_log(ctx, "debug", "LSP CLI registration failed: %s", aa, false);
        if (mc3_exc_pending(ctx)) return -1;  /* handler raise is NEW */
        return 0;
    }
    return -1;  /* BaseException propagates */
}

/* PoP: mc3_build_cli_parser @ hermes_cli/main.py:_build_cli_parser */
static json_t *mc3_build_cli_parser(mc3_ctx *ctx, json_t *in) {
    (void)in;
    json_t *btl = NULL;
    /* from hermes_cli._parser import build_top_level_parser */
    if (mc3_import_gate(ctx, "hermes_cli._parser")) return NULL;
    btl = mc3_call(ctx, "build_top_level_parser", json_object());
    if (mc3_exc_pending(ctx)) { json_free(btl); return NULL; }
    /* parser, subparsers, chat_parser = build_top_level_parser() */
    if (!mc3_is_arr(btl)) {
        mc3_raise(ctx, "TypeError", "cannot unpack non-iterable NoneType object");
        json_free(btl); return NULL;
    }
    if (btl->c.count != 3) {
        char msg[128];
        if (btl->c.count < 3)
            snprintf(msg, sizeof msg, "not enough values to unpack (expected 3, got %zu)", btl->c.count);
        else
            snprintf(msg, sizeof msg, "too many values to unpack (expected 3, got %zu)", btl->c.count);
        mc3_raise(ctx, "ValueError", msg);
        json_free(btl); return NULL;
    }
    json_t *parser = mc3_t0(btl);     /* borrowed */
    json_t *sp = mc3_t1(btl);         /* borrowed */
    json_t *chat = mc3_t2(btl);       /* borrowed */
    /* chat_parser.set_defaults(func=cmd_chat) */
    if (mc3_set_defaults(ctx, chat, "cmd_chat") < 0) goto fail;
    /* registration ORDER is the hermes --help order — keep it stable. */
    if (mc3_builder1(ctx, "build_model_parser", sp, "cmd_model", "cmd_model") < 0) goto fail;
    if (mc3_builder0(ctx, "build_moa_parser", sp) < 0) goto fail;
    if (mc3_builder0(ctx, "build_fallback_parser", sp) < 0) goto fail;
    if (mc3_builder0(ctx, "build_worktree_parser", sp) < 0) goto fail;
    if (mc3_builder0(ctx, "build_browser_parser", sp) < 0) goto fail;
    if (mc3_builder0(ctx, "build_secrets_parser", sp) < 0) goto fail;
    if (mc3_builder0(ctx, "build_egress_parser", sp) < 0) goto fail;
    if (mc3_builder0(ctx, "build_migrate_parser", sp) < 0) goto fail;
    if (mc3_builder0(ctx, "build_codex_runtime_parser", sp) < 0) goto fail;
    {
        const char *kn[3] = { "cmd_gateway", "cmd_proxy", "cmd_gateway_enroll" };
        const char *kv[3] = { "cmd_gateway", "cmd_proxy", "cmd_gateway_enroll" };
        if (mc3_builder(ctx, "build_gateway_parser", sp, kn, kv, 3) < 0) goto fail;
    }
    /* LSP is optional — a registration failure must not break the CLI. */
    if (mc3_lsp_block(ctx, sp) < 0) goto fail;
    if (mc3_builder1(ctx, "build_setup_parser", sp, "cmd_setup", "cmd_setup") < 0) goto fail;
    if (mc3_builder1(ctx, "build_whatsapp_parser", sp, "cmd_whatsapp", "cmd_whatsapp") < 0) goto fail;
    if (mc3_builder1(ctx, "build_whatsapp_cloud_parser", sp, "cmd_whatsapp_cloud", "cmd_whatsapp_cloud") < 0) goto fail;
    if (mc3_builder1(ctx, "build_slack_parser", sp, "cmd_slack", "cmd_slack") < 0) goto fail;
    /* from hermes_cli.send_cmd import register_send_subparser */
    if (mc3_import_gate(ctx, "hermes_cli.send_cmd")) goto fail;
    if (mc3_builder0(ctx, "register_send_subparser", sp) < 0) goto fail;
    if (mc3_builder1(ctx, "build_login_parser", sp, "cmd_login", "cmd_login") < 0) goto fail;
    if (mc3_builder1(ctx, "build_logout_parser", sp, "cmd_logout", "cmd_logout") < 0) goto fail;
    if (mc3_builder1(ctx, "build_auth_parser", sp, "cmd_auth", "cmd_auth") < 0) goto fail;
    if (mc3_builder1(ctx, "build_status_parser", sp, "cmd_status", "cmd_status") < 0) goto fail;
    if (mc3_builder0(ctx, "build_pause_parser", sp) < 0) goto fail;
    if (mc3_builder1(ctx, "build_cron_parser", sp, "cmd_cron", "cmd_cron") < 0) goto fail;
    if (mc3_builder1(ctx, "build_webhook_parser", sp, "cmd_webhook", "cmd_webhook") < 0) goto fail;
    /* from hermes_cli.subcommands.peer import build_peer_parser */
    if (mc3_import_gate(ctx, "hermes_cli.subcommands.peer")) goto fail;
    if (mc3_builder0(ctx, "build_peer_parser", sp) < 0) goto fail;
    /* from hermes_cli.portal_cli import add_parser as _add_portal_parser */
    if (mc3_import_gate(ctx, "hermes_cli.portal_cli")) goto fail;
    if (mc3_builder0(ctx, "portal.add_parser", sp) < 0) goto fail;
    /* from hermes_cli.kanban import build_parser as _build_kanban_parser */
    if (mc3_import_gate(ctx, "hermes_cli.kanban")) goto fail;
    {
        json_t *a = json_object(); mc3_arg(a, "subparsers", sp);
        json_t *kp = mc3_call(ctx, "kanban.build_parser", a);
        if (mc3_exc_pending(ctx)) { json_free(kp); goto fail; }
        int sd = mc3_set_defaults(ctx, kp, "cmd_kanban");
        json_free(kp);
        if (sd < 0) goto fail;
    }
    /* from hermes_cli.projects_cmd import build_parser as _build_project_parser */
    if (mc3_import_gate(ctx, "hermes_cli.projects_cmd")) goto fail;
    {
        json_t *a = json_object(); mc3_arg(a, "subparsers", sp);
        json_t *pp = mc3_call(ctx, "projects.build_parser", a);
        if (mc3_exc_pending(ctx)) { json_free(pp); goto fail; }
        int sd = mc3_set_defaults(ctx, pp, "cmd_project");
        json_free(pp);
        if (sd < 0) goto fail;
    }
    if (mc3_builder1(ctx, "build_hooks_parser", sp, "cmd_hooks", "cmd_hooks") < 0) goto fail;
    if (mc3_builder1(ctx, "build_doctor_parser", sp, "cmd_doctor", "cmd_doctor") < 0) goto fail;
    if (mc3_builder1(ctx, "build_verify_parser", sp, "cmd_verify", "cmd_verify") < 0) goto fail;
    if (mc3_builder1(ctx, "build_security_parser", sp, "cmd_security", "cmd_security") < 0) goto fail;
    if (mc3_builder1(ctx, "build_approvals_parser", sp, "cmd_approvals", "cmd_approvals") < 0) goto fail;
    if (mc3_builder1(ctx, "build_dump_parser", sp, "cmd_dump", "cmd_dump") < 0) goto fail;
    if (mc3_builder1(ctx, "build_debug_parser", sp, "cmd_debug", "cmd_debug") < 0) goto fail;
    if (mc3_builder1(ctx, "build_backup_parser", sp, "cmd_backup", "cmd_backup") < 0) goto fail;
    if (mc3_builder0(ctx, "build_checkpoints_parser", sp) < 0) goto fail;
    if (mc3_builder1(ctx, "build_import_cmd_parser", sp, "cmd_import", "cmd_import") < 0) goto fail;
    if (mc3_builder1(ctx, "build_config_parser", sp, "cmd_config", "cmd_config") < 0) goto fail;
    if (mc3_builder1(ctx, "build_skin_parser", sp, "cmd_skin", "cmd_skin") < 0) goto fail;
    if (mc3_builder1(ctx, "build_console_parser", sp, "cmd_console", "cmd_console") < 0) goto fail;
    if (mc3_builder1(ctx, "build_pairing_parser", sp, "cmd_pairing", "cmd_pairing") < 0) goto fail;
    if (mc3_builder1(ctx, "build_skills_parser", sp, "cmd_skills", "cmd_skills") < 0) goto fail;
    if (mc3_builder0(ctx, "build_bundles_parser", sp) < 0) goto fail;
    if (mc3_builder1(ctx, "build_plugins_parser", sp, "cmd_plugins", "cmd_plugins") < 0) goto fail;
    /* _register_plugin_cli_commands(subparsers) — claimed sibling, seamed */
    {
        json_t *a = json_object(); mc3_arg(a, "subparsers", sp);
        json_t *r = mc3_call(ctx, "_register_plugin_cli_commands", a);
        json_free(r);
        if (mc3_exc_pending(ctx)) goto fail;
    }
    if (mc3_builder0(ctx, "build_curator_parser", sp) < 0) goto fail;
    if (mc3_builder0(ctx, "build_pets_parser", sp) < 0) goto fail;
    if (mc3_builder0(ctx, "build_clipboard_parser", sp) < 0) goto fail;
    if (mc3_builder0(ctx, "build_journey_parser", sp) < 0) goto fail;
    if (mc3_builder1(ctx, "build_memory_parser", sp, "cmd_memory", "cmd_memory") < 0) goto fail;
    if (mc3_builder1(ctx, "build_tools_parser", sp, "cmd_tools", "cmd_tools") < 0) goto fail;
    if (mc3_builder0(ctx, "build_computer_use_parser", sp) < 0) goto fail;
    if (mc3_builder1(ctx, "build_mcp_parser", sp, "cmd_mcp", "cmd_mcp") < 0) goto fail;
    if (mc3_builder0(ctx, "build_connectors_parser", sp) < 0) goto fail;
    if (mc3_builder1(ctx, "build_sessions_parser", sp, "cmd_sessions", "cmd_sessions_lazy") < 0) goto fail;
    if (mc3_builder1(ctx, "build_insights_parser", sp, "cmd_insights", "cmd_insights") < 0) goto fail;
    if (mc3_builder0(ctx, "build_usage_parser", sp) < 0) goto fail;
    if (mc3_builder1(ctx, "build_monitoring_parser", sp, "cmd_monitoring", "cmd_monitoring") < 0) goto fail;
    if (mc3_builder0(ctx, "build_vault_parser", sp) < 0) goto fail;
    if (mc3_builder1(ctx, "build_update_parser", sp, "cmd_update", "cmd_update") < 0) goto fail;
    if (mc3_builder1(ctx, "build_uninstall_parser", sp, "cmd_uninstall", "cmd_uninstall") < 0) goto fail;
    if (mc3_builder1(ctx, "build_acp_parser", sp, "cmd_acp", "cmd_acp") < 0) goto fail;
    if (mc3_builder1(ctx, "build_profile_parser", sp, "cmd_profile", "cmd_profile") < 0) goto fail;
    {
        const char *kn[2] = { "cmd_completion", "parser" };
        json_t *a = json_object();
        mc3_arg(a, "subparsers", sp);
        mc3_arg_str(a, kn[0], "cmd_completion");
        mc3_arg(a, kn[1], parser);
        json_t *r = mc3_call(ctx, "build_completion_parser", a);
        json_free(r);
        if (mc3_exc_pending(ctx)) goto fail;
    }
    {
        const char *kn[2] = { "cmd_dashboard", "cmd_dashboard_register" };
        const char *kv[2] = { "cmd_dashboard", "cmd_dashboard_register" };
        if (mc3_builder(ctx, "build_dashboard_parser", sp, kn, kv, 2) < 0) goto fail;
    }
    if (mc3_builder1(ctx, "build_webapp_parser", sp, "cmd_webapp", "cmd_webapp") < 0) goto fail;
    if (mc3_builder1(ctx, "build_gui_parser", sp, "cmd_gui", "cmd_gui") < 0) goto fail;
    if (mc3_builder1(ctx, "build_logs_parser", sp, "cmd_logs", "cmd_logs") < 0) goto fail;
    if (mc3_builder1(ctx, "build_prompt_size_parser", sp, "cmd_prompt_size", "cmd_prompt_size") < 0) goto fail;
    /* return parser, subparsers */
    {
        json_t *out = json_array();
        json_append(out, parser ? json_copy(parser) : json_null());
        json_append(out, sp ? json_copy(sp) : json_null());
        json_free(btl);
        return out;
    }
fail:
    json_free(btl);
    return NULL;
}

/* ── main: the entry-point decision sequence ─────────────────────────────── */

/* PoP: mc3_main @ hermes_cli/main.py:main */
static json_t *mc3_main(mc3_ctx *ctx, json_t *in) {
    (void)in;
    json_t *pair = NULL;   /* owned: _build_cli_parser result */
    json_t *args = NULL;   /* owned: _parse_cli_args result double */
    json_t *argv = NULL;   /* owned: sys.argv[1:] copy */
    json_t *r = NULL;
    json_t *a = NULL;

    /* sys.argv[1:] from env.argv (live SysDouble.argv = ["hermes"] + argv) */
    argv = json_array();
    {
        json_t *av = mc3_get(ctx->env, "argv");
        if (mc3_is_arr(av))
            for (size_t i = 0; i < av->c.count; i++)
                json_append(argv, json_copy(av->c.items[i]));
    }

    /* _set_process_title(); _warn_if_unsupervised_pid1(); _advertise_agent_env() */
    r = mc3_call(ctx, "_set_process_title", json_object()); json_free(r);
    if (mc3_exc_pending(ctx)) goto done;
    r = mc3_call(ctx, "_warn_if_unsupervised_pid1", json_object()); json_free(r);
    if (mc3_exc_pending(ctx)) goto done;
    r = mc3_call(ctx, "_advertise_agent_env", json_object()); json_free(r);
    if (mc3_exc_pending(ctx)) goto done;

    /* try: from hermes_cli.stdio import configure_windows_stdio; call() */
    if (mc3_import_gate(ctx, "hermes_cli.stdio")) goto stdio_suppress;
    r = mc3_call(ctx, "configure_windows_stdio", json_object()); json_free(r);
    if (mc3_exc_pending(ctx)) goto stdio_suppress;
    goto stdio_done;
stdio_suppress:
    if (mc3_exc_is_exception(ctx)) mc3_exc_clear(ctx);
    else goto done;
stdio_done:

    /* from hermes_cli import _stdio_repaired; if _stdio_repaired: setdefault x2 */
    if (json_get_bool(ctx->env, "_stdio_repaired", false)) {
        a = json_object(); mc3_arg_str(a, "name", "PYTHONUTF8"); mc3_arg_str(a, "value", "1");
        r = mc3_call(ctx, "os.environ.setdefault", a); json_free(r);
        if (mc3_exc_pending(ctx)) goto done;
        a = json_object(); mc3_arg_str(a, "name", "PYTHONIOENCODING"); mc3_arg_str(a, "value", "utf-8");
        r = mc3_call(ctx, "os.environ.setdefault", a); json_free(r);
        if (mc3_exc_pending(ctx)) goto done;
    }

    /* from agent.ssl_verify import install_truststore; install_truststore() */
    if (mc3_import_gate(ctx, "agent.ssl_verify")) goto done;
    r = mc3_call(ctx, "install_truststore", json_object()); json_free(r);
    if (mc3_exc_pending(ctx)) goto done;

    /* try: _cleanup_quarantined_exes() except Exception: pass */
    r = mc3_call(ctx, "_cleanup_quarantined_exes", json_object()); json_free(r);
    if (mc3_exc_pending(ctx)) {
        if (mc3_exc_is_exception(ctx)) mc3_exc_clear(ctx);
        else goto done;
    }

    /* _sweep_stale_bytecode_if_checkout_changed(); _recover_update_debts_on_startup() */
    r = mc3_call(ctx, "_sweep_stale_bytecode_if_checkout_changed", json_object()); json_free(r);
    if (mc3_exc_pending(ctx)) goto done;
    r = mc3_call(ctx, "_recover_update_debts_on_startup", json_object()); json_free(r);
    if (mc3_exc_pending(ctx)) goto done;

    /* if _first_positional_argv() != "update": maybe_run_boot_bootstrap(install_root()) */
    {
        json_t *fp = mc3_call(ctx, "_first_positional_argv", json_object());
        if (mc3_exc_pending(ctx)) { json_free(fp); goto done; }
        bool not_update = !mc3_str_eq(mc3_jstr(fp), "update");
        json_free(fp);
        if (not_update) {
            if (mc3_import_gate(ctx, "hermes_cli.boot_bootstrap")) goto done;
            if (mc3_import_gate(ctx, "pm.paths")) goto done;
            json_t *root = mc3_call(ctx, "install_root", json_object());
            if (mc3_exc_pending(ctx)) { json_free(root); goto done; }
            a = json_object(); mc3_arg(a, "root", root);
            json_free(root);
            r = mc3_call(ctx, "maybe_run_boot_bootstrap", a); json_free(r);
            if (mc3_exc_pending(ctx)) goto done;
        }
    }

    /* try: problem = check_runtime(install_root()); if problem: print(warn)
     * except Exception: logging.getLogger(__name__).debug(..., exc_info=True) */
    if (mc3_import_gate(ctx, "hermes_cli.venv_sync")) goto pm_suppress;
    if (mc3_import_gate(ctx, "pm.paths")) goto pm_suppress;
    {
        json_t *root = mc3_call(ctx, "install_root", json_object());
        if (mc3_exc_pending(ctx)) { json_free(root); goto pm_suppress; }
        a = json_object(); mc3_arg(a, "root", root);
        json_free(root);
        json_t *problem = mc3_call(ctx, "check_runtime", a);
        if (mc3_exc_pending(ctx)) { json_free(problem); goto pm_suppress; }
        if (mc3_truthy(problem)) {
            char *ps = mc3_pystr(problem);
            size_t n = strlen(ps);
            char *text = (char *)malloc(n + 8);
            if (text) {
                memcpy(text, "\xE2\x9A\xA0 ", 4);  /* "⚠ " */
                memcpy(text + 4, ps, n + 1);
                mc3_print(ctx, text, "stderr");
                free(text);
            }
            free(ps);
            json_free(problem);
            if (mc3_exc_pending(ctx)) goto pm_suppress;  /* print raise is caught */
        } else {
            json_free(problem);
        }
    }
    goto pm_done;
pm_suppress:
    if (mc3_exc_is_exception(ctx)) {
        mc3_exc_clear(ctx);  /* CLEAR before the handler body */
        mc3_log(ctx, "debug", "pm startup check failed", json_array(), true);
        if (mc3_exc_pending(ctx)) goto done;
    } else goto done;
pm_done:

    /* fast-launch gates: any truthy result returns early */
    r = mc3_call(ctx, "_try_termux_fast_tui_launch", json_object());
    if (mc3_exc_pending(ctx)) { json_free(r); goto done; }
    if (mc3_truthy(r)) { json_free(r); goto done; }
    json_free(r);
    r = mc3_call(ctx, "_try_termux_fast_cli_launch", json_object());
    if (mc3_exc_pending(ctx)) { json_free(r); goto done; }
    if (mc3_truthy(r)) { json_free(r); goto done; }
    json_free(r);
    r = mc3_call(ctx, "_try_fast_serve_launch", json_object());
    if (mc3_exc_pending(ctx)) { json_free(r); goto done; }
    if (mc3_truthy(r)) { json_free(r); goto done; }
    json_free(r);
    r = mc3_call(ctx, "_try_fast_chat_launch", json_object());
    if (mc3_exc_pending(ctx)) { json_free(r); goto done; }
    if (mc3_truthy(r)) { json_free(r); goto done; }
    json_free(r);

    /* parser, subparsers = _build_cli_parser() — REAL ported composition */
    pair = mc3_build_cli_parser(ctx, in);
    if (mc3_exc_pending(ctx)) goto done;
    json_t *parser = mc3_t0(pair);   /* borrowed */
    json_t *sp = mc3_t1(pair);       /* borrowed */

    /* from hermes_cli.config import get_container_exec_info */
    if (mc3_import_gate(ctx, "hermes_cli.config")) goto done;
    {
        json_t *ci = mc3_call(ctx, "get_container_exec_info", json_object());
        if (mc3_exc_pending(ctx)) { json_free(ci); goto done; }
        if (mc3_truthy(ci)) {
            /* exec_in_container(container_info, sys.argv[1:]); sys.exit(1) */
            if (mc3_import_gate(ctx, "hermes_cli.main_container")) { json_free(ci); goto done; }
            a = json_object(); mc3_arg(a, "info", ci); mc3_arg(a, "argv", argv);
            r = mc3_call(ctx, "exec_in_container", a); json_free(r);
            json_free(ci);
            if (mc3_exc_pending(ctx)) goto done;
            mc3_sys_exit(ctx, json_number(1));
            goto done;
        }
        json_free(ci);
    }

    /* args = _parse_cli_args(parser, subparsers, sys.argv[1:]) */
    a = json_object();
    mc3_arg(a, "parser", parser);
    mc3_arg(a, "subparsers", sp);
    mc3_arg(a, "argv", argv);
    args = mc3_call(ctx, "_parse_cli_args", a);
    if (mc3_exc_pending(ctx)) goto done;

    /* _enable_cpu_profile(args) */
    a = json_object(); mc3_arg_args(a, "args", args);
    r = mc3_call(ctx, "_enable_cpu_profile", a); json_free(r);
    if (mc3_exc_pending(ctx)) goto done;

    /* if args.version: cmd_version(args); return */
    {
        json_t *ver = mc3_ns_get(ctx, args, "version");
        if (mc3_exc_pending(ctx)) goto done;
        if (mc3_truthy(ver)) {
            a = json_object(); mc3_arg_args(a, "args", args);
            r = mc3_call(ctx, "cmd_version", a); json_free(r);
            if (mc3_exc_pending(ctx)) goto done;
            goto done;  /* return None */
        }
    }

    /* if getattr(args, "yolo", False): os.environ["HERMES_YOLO_MODE"] = "1" */
    if (mc3_truthy(mc3_get_def(args, "yolo", NULL))) {
        a = json_object(); mc3_arg_str(a, "key", "HERMES_YOLO_MODE"); mc3_arg_str(a, "value", "1");
        r = mc3_call(ctx, "os.environ.set", a); json_free(r);
        if (mc3_exc_pending(ctx)) goto done;
    }

    /* _prepare_agent_startup(args) */
    a = json_object(); mc3_arg_args(a, "args", args);
    r = mc3_call(ctx, "_prepare_agent_startup", a); json_free(r);
    if (mc3_exc_pending(ctx)) goto done;

    /* if getattr(args, "oneshot", None): _run_oneshot_from_args(args) */
    if (mc3_truthy(mc3_get_def(args, "oneshot", NULL))) {
        a = json_object(); mc3_arg_args(a, "args", args);
        r = mc3_call(ctx, "_run_oneshot_from_args", a); json_free(r);
        if (mc3_exc_pending(ctx)) goto done;
    }

    /* from ...shared_metrics_disabled import set_process_surface;
     * set_process_surface(args.command) */
    if (mc3_import_gate(ctx, "hermes_cli.observability.shared_metrics_disabled")) goto done;
    json_t *command = mc3_ns_get(ctx, args, "command");
    if (mc3_exc_pending(ctx)) goto done;
    a = json_object(); mc3_arg(a, "surface", command);
    r = mc3_call(ctx, "set_process_surface", a); json_free(r);
    if (mc3_exc_pending(ctx)) goto done;

    /* if args.command is None: _default_to_chat(args); return */
    if (mc3_is_none(command)) {
        a = json_object(); mc3_arg_args(a, "args", args);
        r = mc3_call(ctx, "_default_to_chat", a); json_free(r);
        if (mc3_exc_pending(ctx)) goto done;
        goto done;  /* return None */
    }

    /* if hasattr(args, "func"): rc = args.func(args); int rc != 0 -> exit */
    if (mc3_get(args, "func")) {
        json_t *func = mc3_get(args, "func");
        if (mc3_is_none(func)) {
            mc3_raise(ctx, "TypeError", "'NoneType' object is not callable");
            goto done;
        }
        if (!mc3_is_str(func)) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object is not callable", mc3_py_type_name(func));
            mc3_raise(ctx, "TypeError", msg);
            goto done;
        }
        a = json_object(); mc3_arg_args(a, "args", args);
        json_t *rc = mc3_call(ctx, "args.func", a);
        if (mc3_exc_pending(ctx)) { json_free(rc); goto done; }
        if (mc3_rc_nonzero_int(rc)) {
            mc3_sys_exit(ctx, rc ? json_copy(rc) : json_null());
            json_free(rc);
            goto done;
        }
        json_free(rc);
    } else {
        /* parser.print_help() */
        if (mc3_is_none(parser)) { mc3_attr_raise(ctx, "NoneType", "print_help"); goto done; }
        if (!mc3_is_obj(parser)) { mc3_attr_raise(ctx, mc3_py_type_name(parser), "print_help"); goto done; }
        r = mc3_call(ctx, "parser.print_help", json_object()); json_free(r);
        if (mc3_exc_pending(ctx)) goto done;
    }

done:
    json_free(pair);
    json_free(args);
    json_free(argv);
    if (mc3_exc_pending(ctx)) return NULL;
    return json_null();
}