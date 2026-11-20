/*
 * tui_methods_cron_flow.c -- FLOW + leaf port of the TUI cron method surface.
 *
 * The task named tui_gateway/methods_cron.py, which DOES NOT EXIST in the
 * quarry (hermes-agent). The real defining module of the TUI cron method
 * (the ``cron.manage`` JSON-RPC handler) is tui_gateway/methods_tools.py, so
 * every PoP below credits that module. This port takes the cron.manage handler
 * plus its UNCLAIMED RPC-wrapper machinery (_profile_scoped_rpc / _guarded /
 * _rpc / _scoped_rpc / _mcp_rpc), the cron-adjacent MCP-server / rewind /
 * learning / skills / captured-exec helpers that share the same scaffolding.
 * Claimed siblings (_str_arg, _tools_mod, _busy_error, _mcp_plugin_write_error)
 * and server globals (_ok, _err, _sess, _profile_home, ...) are SEAMED.
 *
 * Ports the DECISION SEQUENCES -- branch order, error-code/message composition,
 * seam call order and exception paths -- not just leaf outputs. Every live
 * external call enters through ONE universal seam: tmc2_call(ctx, name, args).
 * The harness wires it to a recording stub fed the case's scripted results
 * (in.io); the oracle installs recording stubs emitting the identical event
 * shape {"name","args","ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmc2_-prefixed.
 *
 * Seam name -> live call (args are informational projections):
 *   ok                          _ok(rid, result)
 *   err                         _err(rid, code, msg, data=None) [data omitted when None]
 *   tools_mod                   _tools_mod(module) -> module NAME marker string
 *   str_arg                     _str_arg(params, key) -> str   [claimed sibling]
 *   sess                        _sess(params, rid) -> [session, err]
 *   profile_home                _profile_home(profile) -> home str | null
 *   session_profile_runtime_scope  _session_profile_runtime_scope({profile_home})
 *                               -> scope marker (the `with scope:` enter/exit are
 *                               silent no-ops on both sides)
 *   body                        the wrapped handler body(*args) -> result
 *   cronjob                     tools.cronjob_tools.cronjob(action=..., ...) -> JSON str
 *   mcp_server_rows             _mcp_server_rows() -> [servers, plugins]
 *   mcp_named_server            _mcp_named_server(rid, params) -> [name, servers, err]
 *   mcp_plugin_write_error      _mcp_plugin_write_error(rid, name, plugins) -> err|null
 *   mcp_config._get_mcp_servers hermes_cli.mcp_config._get_mcp_servers()
 *   mcp_rpc_helpers.server_configs_with_sources
 *                               tui_gateway.mcp_rpc_helpers.server_configs_with_sources(cfg)
 *   busy_error                  _busy_error(rid, session, cmd) -> err|null [claimed]
 *   user_turn_indices           _user_turn_indices(session) -> [history, indices]
 *   history_without_ephemeral_scaffolding  server global(history) -> history
 *   turns.user_originated_turn_view  session_rules.turns.user_originated_turn_view(m)
 *   rewind_active_session_history  server global(session, keep, **kw) -> result
 *   session_not_owned_err       _session_not_owned_err(rid, exc) -> err
 *   learning_mutations.call     agent.learning_mutations.<fn>(*args) {fn, args}
 *   learning_graph.build_learning_graph  agent.learning_graph.build_learning_graph()
 *   learning_graph_render.render_frames  render_frames(graph, cols=, rows=, frames=)
 *   skills_hub_github.GitHubAuth  tools.skills_hub_github.GitHubAuth() -> marker
 *   skills_hub_search.create_source_router  search.create_source_router(auth)
 *   skills_hub_search.unified_search  search.unified_search(query, router,
 *                               source_filter="all", limit=20) -> rows|null
 *   skills_hub.browse_skills    hermes_cli.skills_hub.browse_skills(page=, page_size=)
 *   os.getcwd                   os.getcwd() -> str
 *   capture_run_kwargs          _capture_run_kwargs(timeout) -> kwargs dict
 *   subprocess_compat.windows_hide_flags  hermes_cli._subprocess_compat.windows_hide_flags()
 *   subprocess.run              subprocess.run(cmd, cwd=, shell=, env=, **kwargs)
 *   on_result                   the on_result(CompletedProcess) callback
 *
 * Representation contract (shared with the oracle):
 * - None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
 *   BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) pass through every except-Exception site.
 * - tools_mod scripts return the module NAME string; attribute access on the
 *   module double is silent, only the subsequent call is a seam.
 * - Tuples project to lists; sets to sorted lists; integral floats to ints;
 *   Paths to POSIX strings (fixture paths are POSIX). str()/strip()/lower()/
 *   isdigit() fold the ASCII fixture corpus only.
 * - Seam args drop None-valued keys on both sides (absent-vs-null parity).
 * - json.loads runs REAL (C json_parse) on the cronjob JSON-string result; the
 *   fixture cronjob results are strict integral JSON (no int/float drift).
 * - is_truthy_value / _stripped / _nonempty are pure leaves run REAL on both
 *   sides (inline transcriptions below), not seams.
 *
 * Ownership: tmc2_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node -- borrowed
 * pointers to a replaced key are copied first. Seam results are caller-freed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct tmc2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tmc2_ctx_t;

/* -- basic helpers -- */
static bool tmc2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tmc2_raise_kind(const json_t *v) {
    return tmc2_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tmc2_is_base_exc(const char *kind) {
    return kind && (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
                    || !strcmp(kind, "GeneratorExit") || !strcmp(kind, "CancelledError"));
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tmc2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *tmc2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tmc2_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tmc2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == (double)(long long)d) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* Python str() for scalars (containers serialize as JSON; no fixture exercises
 * a container in a str() position). Integral doubles render as ints. */
static char *tmc2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return strdup(buf);
        }
        default: return json_serialize(v);
    }
}

static char *tmc2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static bool tmc2_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip(): ASCII whitespace both ends (fixtures fold ASCII only). */
static char *tmc2_strip(const char *s) {
    if (!s) s = "";
    while (*s && tmc2_is_ascii_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tmc2_is_ascii_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* str.lower(): ASCII fold (fixture corpus folds ASCII only). */
static char *tmc2_lower(const char *s) {
    char *o = tmc2_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* -- exception-parent table (FULL builtin hierarchy, py3.14.9) -- */
typedef struct { const char *name; const char *parent; } tmc2_exc_row_t;
static const tmc2_exc_row_t TMC2_EXC_PARENTS[] = {
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"BufferError", "Exception"},
    {"BytesWarning", "Warning"},
    {"CancelledError", "BaseException"},
    {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"},
    {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"},
    {"EnvironmentError", "OSError"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"},
    {"GeneratorExit", "BaseException"},
    {"IOError", "OSError"},
    {"ImportError", "Exception"},
    {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"},
    {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"},
    {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"},
    {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"},
    {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"ProfileUnavailableError", "FileNotFoundError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"},
    {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"},
    {"SessionNotOwned", "RuntimeError"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SubprocessError", "Exception"},
    {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"},
    {"SystemError", "Exception"},
    {"SystemExit", "BaseException"},
    {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"},
    {"TimeoutExpired", "SubprocessError"},
    {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"WindowsError", "OSError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"_IncompleteInputError", "SyntaxError"},
};
#define TMC2_EXC_PARENT_COUNT (sizeof(TMC2_EXC_PARENTS)/sizeof(TMC2_EXC_PARENTS[0]))

static const char *tmc2_exc_parent(const char *kind) {
    if (!kind) return NULL;
    for (size_t i = 0; i < TMC2_EXC_PARENT_COUNT; i++)
        if (!strcmp(TMC2_EXC_PARENTS[i].name, kind)) return TMC2_EXC_PARENTS[i].parent;
    return NULL;
}

/* issubclass(kind, target) over the single-parent table. Unknown kinds are
 * synthetic Exception subclasses (the oracle's make_exc fallback). */
static bool tmc2_exc_is_a(const char *kind, const char *target) {
    if (!kind || !target) return false;
    const char *cur = kind;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (!strcmp(cur, target)) return true;
        if (!strcmp(cur, "BaseException") || !strcmp(cur, "object")) return false;
        const char *p = tmc2_exc_parent(cur);
        if (!p) return !strcmp(target, "Exception");  /* unknown -> Exception */
        cur = p;
    }
    return false;
}

static bool tmc2_exc_is_exception(const char *kind) {
    return tmc2_exc_is_a(kind, "Exception");
}

/* -- the one universal seam (fail-closed) -- */
static json_t *tmc2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tmc2_err_spec(type, msg);
}

static json_t *tmc2_call(tmc2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    /* drop null-valued arg keys on both sides (absent-vs-null parity) */
    json_t *clean = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(clean, args->c.keys[i], json_copy(v));
        }
    }
    json_free(args);
    json_set(ev, "args", clean);
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = tmc2_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tmc2_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tmc2_make_exc(raise);
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

/* args builders (values borrowed; copied into the args object) */
static json_t *tmc2_args0(void) { return json_object(); }
static void tmc2_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tmc2_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tmc2_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tmc2_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* _tools_mod(module): deferred import seam; returns the module marker. */
static json_t *tmc2_tools_mod(tmc2_ctx_t *ctx, const char *module) {
    json_t *a = tmc2_args0();
    tmc2_asets(a, "module", module);
    return tmc2_call(ctx, "tools_mod", a);
}

/* _ok(rid, result) -- result is ADOPTED. */
static json_t *tmc2_ok(tmc2_ctx_t *ctx, const json_t *rid, json_t *result) {
    json_t *a = tmc2_args0();
    tmc2_aset(a, "rid", rid);
    json_set(a, "result", result ? result : json_null());
    return tmc2_call(ctx, "ok", a);
}

/* _err(rid, code, msg, data) -- data ADOPTED (NULL omits the key). */
static json_t *tmc2_err(tmc2_ctx_t *ctx, const json_t *rid, int code,
                       const char *msg, json_t *data) {
    json_t *a = tmc2_args0();
    tmc2_aset(a, "rid", rid);
    tmc2_asetn(a, "code", (double)code);
    tmc2_asets(a, "message", msg);
    if (data) json_set(a, "data", data);
    return tmc2_call(ctx, "err", a);
}

/* _str_arg(params, key) -- CLAIMED sibling, seamed. Returns str. */
static json_t *tmc2_str_arg(tmc2_ctx_t *ctx, const json_t *params, const char *key) {
    json_t *a = tmc2_args0();
    tmc2_aset(a, "params", params);
    tmc2_asets(a, "key", key);
    return tmc2_call(ctx, "str_arg", a);
}

/* -- pure leaves (run REAL on both sides; no seams) -- */

/* is_truthy_value(value, default=False) -- utils/env.py. None -> default;
 * str -> strip().lower() in {"1","true","yes","on"}; else bool(value). */
static bool tmc2_is_truthy_value(const json_t *v, bool def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_STRING) {
        char *s = tmc2_strip(v->str_val ? v->str_val : "");
        char *l = tmc2_lower(s);
        free(s);
        bool hit = l && (!strcmp(l, "1") || !strcmp(l, "true")
                         || !strcmp(l, "yes") || !strcmp(l, "on"));
        free(l);
        return hit;
    }
    return tmc2_truthy(v);
}

/* str.isdigit(): ASCII digits only, non-empty (fixture corpus is ASCII). */
static bool tmc2_isdigit(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++)
        if (!isdigit((unsigned char)*p)) return false;
    return true;
}

/* int() over the fixture domain: an integral number passes through; a string
 * is trimmed (ASCII ws), optional sign, digits with '_' between digits. On a
 * bad literal returns NULL and parks a ValueError spec in *raise (caller frees).
 * The message quotes repr(token) -- fixtures keep tokens printable ASCII without
 * quotes/backslashes so repr == 'token'. */
static json_t *tmc2_pyint(const json_t *v, json_t **raise_out) {
    *raise_out = NULL;
    if (v && v->type == JSON_NUMBER) {
        double d = v->num_val;
        return json_number((double)(long long)d);
    }
    if (v && v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    const char *raw = tmc2_str(v);
    if (!raw) {
        char msg[160];
        snprintf(msg, sizeof msg,
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 tmc2_type_name(v));
        *raise_out = tmc2_err_spec("TypeError", msg);
        return NULL;
    }
    char *tok = tmc2_strdup(raw);
    char *s = tmc2_strip(tok);
    free(tok);
    /* validate: optional sign, then digits/underscore with '_' between digits */
    const char *p = s;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    const char *digits_start = p;
    bool any = false, prev_digit = false, bad = false;
    for (; *p; p++) {
        if (isdigit((unsigned char)*p)) { any = true; prev_digit = true; }
        else if (*p == '_') {
            if (!prev_digit || !isdigit((unsigned char)p[1])) { bad = true; break; }
            prev_digit = false;
        } else { bad = true; break; }
    }
    if (bad || !any) {
        char msg[256];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: '%s'", raw);
        *raise_out = tmc2_err_spec("ValueError", msg);
        free(s);
        return NULL;
    }
    /* build clean digit buffer (drop '_'), carry sign */
    size_t n = strlen(digits_start);
    char *clean = malloc(n + 2);
    size_t k = 0;
    if (neg) clean[k++] = '-';
    for (const char *q = digits_start; *q; q++)
        if (*q != '_') clean[k++] = *q;
    clean[k] = '\0';
    long long iv = strtoll(clean, NULL, 10);
    free(clean);
    free(s);
    return json_number((double)iv);
}

/* str(x or "") -- falsy/absent -> "" (NOT str(None)). */
static char *tmc2_pystr_or_empty(const json_t *v) {
    if (!tmc2_truthy(v)) return tmc2_strdup("");
    return tmc2_pystr(v);
}
/* -- RPC wrapper machinery (cron.manage's scaffolding) --
 * _profile_scoped_rpc produces handler(rid, params); the four thin factories
 * (_guarded/_rpc/_scoped_rpc/_mcp_rpc) compose it with fixed config. The
 * `method(name)` registration is decoration-time plumbing (returns the handler
 * unchanged, no per-call effect) and is silent on both sides. The `with scope:`
 * enter/exit are silent no-ops; only the scope FACTORY call is a seam. */
typedef struct {
    int fail_code;
    const char *prefix;
    bool scoped;
    bool live_session;
    bool catch_resolve;
    const json_t *required;   /* array of [key, "stripped"|"nonempty"] */
} tmc2_wrap_cfg_t;

/* params.get(key, def) then str(): absent -> def; present-null -> "None". */
static char *tmc2_param_str(const json_t *params, const char *key, const char *def) {
    const json_t *v = json_obj_get(params, key);
    if (!v) return tmc2_strdup(def);
    return tmc2_pystr(v);
}

/* required predicate: _stripped = bool(str(v or "").strip()) */
static bool tmc2_pred_stripped(const json_t *v) {
    char *s = tmc2_pystr_or_empty(v);
    char *st = tmc2_strip(s);
    bool hit = st && st[0] != '\0';
    free(s); free(st);
    return hit;
}
/* required predicate: _nonempty = not (v is None or str(v) == "") */
static bool tmc2_pred_nonempty(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    char *s = tmc2_pystr(v);
    bool hit = s && strcmp(s, "") != 0;
    free(s);
    return hit;
}

static json_t *tmc2_wrapped_handler(tmc2_ctx_t *ctx, const json_t *rid,
                                    const json_t *params, const tmc2_wrap_cfg_t *cfg) {
    /* required params (4063 "<key> required") -- outside any try */
    if (cfg->required && cfg->required->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(cfg->required); i++) {
            const json_t *pair = json_get(cfg->required, i);
            const char *key = tmc2_str(json_get(pair, 0));
            const char *pred = tmc2_str(json_get(pair, 1));
            if (!key) continue;
            const json_t *val = json_obj_get(params, key);
            bool present = (pred && !strcmp(pred, "nonempty"))
                               ? tmc2_pred_nonempty(val) : tmc2_pred_stripped(val);
            if (!present) {
                char msg[160];
                snprintf(msg, sizeof msg, "%s required", key);
                return tmc2_err(ctx, rid, 4063, msg, NULL);
            }
        }
    }
    /* live_session resolution via _sess -- outside any try */
    json_t *session_owned = NULL;
    bool have_session = false;
    if (cfg->live_session) {
        json_t *a = tmc2_args0();
        tmc2_aset(a, "params", params);
        tmc2_aset(a, "rid", rid);
        json_t *r = tmc2_call(ctx, "sess", a);
        if (tmc2_is_raise(r)) return r;
        const json_t *err = json_get(r, 1);
        if (tmc2_truthy(err)) {
            json_t *ec = json_copy(err);
            json_free(r);
            return ec;
        }
        const json_t *sess = json_get(r, 0);
        session_owned = sess ? json_copy(sess) : json_null();
        have_session = true;
        json_free(r);
    }
    /* profile scope */
    char *profile_cstr = NULL;
    if (cfg->scoped) {
        json_t *pn = tmc2_str_arg(ctx, params, "profile");
        if (tmc2_is_raise(pn)) { json_free(session_owned); return pn; }
        profile_cstr = tmc2_strdup(tmc2_str(pn) ? tmc2_str(pn) : "");
        json_free(pn);
        /* try: home = _profile_home(profile) except ProfileUnavailableError: 4064 */
        json_t *a = tmc2_args0();
        tmc2_asets(a, "profile", profile_cstr);
        json_t *home = tmc2_call(ctx, "profile_home", a);
        if (tmc2_is_raise(home)) {
            const char *kind = tmc2_raise_kind(home);
            if (tmc2_exc_is_a(kind, "ProfileUnavailableError")) {
                char msg[200];
                snprintf(msg, sizeof msg, "profile '%s' not found", profile_cstr);
                json_free(home); free(profile_cstr); json_free(session_owned);
                return tmc2_err(ctx, rid, 4064, msg, NULL);
            }
            if (tmc2_is_base_exc(kind) || !cfg->catch_resolve) {
                free(profile_cstr); json_free(session_owned);
                return home;
            }
            const char *m = json_get_str(home, "message", "");
            json_free(home); free(profile_cstr); json_free(session_owned);
            return tmc2_err(ctx, rid, cfg->fail_code, m, NULL);
        }
        /* scope = _session_profile_runtime_scope({"profile_home": str(home) if home else None}) */
        json_t *scope_arg = json_object();
        if (home && home->type != JSON_NULL && tmc2_truthy(home))
            json_set(scope_arg, "profile_home", json_string(tmc2_str(home) ? tmc2_str(home) : ""));
        else
            json_set(scope_arg, "profile_home", json_null());
        json_t *a2 = tmc2_args0();
        json_set(a2, "session", scope_arg);
        json_t *scope = tmc2_call(ctx, "session_profile_runtime_scope", a2);
        json_free(home);
        if (tmc2_is_raise(scope)) {
            const char *kind = tmc2_raise_kind(scope);
            if (tmc2_is_base_exc(kind) || !cfg->catch_resolve) {
                free(profile_cstr); json_free(session_owned);
                return scope;
            }
            const char *m = json_get_str(scope, "message", "");
            json_free(scope); free(profile_cstr); json_free(session_owned);
            return tmc2_err(ctx, rid, cfg->fail_code, m, NULL);
        }
        json_free(scope);   /* the `with scope:` enter/exit are silent */
        free(profile_cstr);
        profile_cstr = NULL;
    }
    /* try: with scope: return body(*args)
     * except (ProfileUnavailableError, SessionNotOwned): raise
     * except Exception as e: return _err(rid, fail_code, prefix + str(e)) */
    json_t *ba = tmc2_args0();
    tmc2_aset(ba, "rid", rid);
    tmc2_aset(ba, "params", params);
    if (have_session) tmc2_aset(ba, "session", session_owned);
    json_free(session_owned);
    json_t *br = tmc2_call(ctx, "body", ba);
    if (tmc2_is_raise(br)) {
        const char *kind = tmc2_raise_kind(br);
        if (tmc2_exc_is_a(kind, "ProfileUnavailableError")
            || tmc2_exc_is_a(kind, "SessionNotOwned") || tmc2_is_base_exc(kind))
            return br;
        const char *m = json_get_str(br, "message", "");
        size_t pn = strlen(cfg->prefix ? cfg->prefix : "");
        char *msg = malloc(pn + strlen(m) + 1);
        if (msg) { memcpy(msg, cfg->prefix ? cfg->prefix : "", pn); strcpy(msg + pn, m); }
        json_free(br);
        json_t *e = tmc2_err(ctx, rid, cfg->fail_code, msg ? msg : m, NULL);
        free(msg);
        return e;
    }
    return br;
}

/* PoP: tmc2_profile_scoped_rpc @ tui_gateway/methods_tools.py:_profile_scoped_rpc */
static json_t *tmc2_profile_scoped_rpc(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    tmc2_wrap_cfg_t cfg;
    cfg.fail_code = (int)json_get_num(args, "fail_code", 5024);
    cfg.prefix = json_get_str(args, "prefix", "");
    cfg.scoped = json_get_bool(args, "scoped", true);
    cfg.live_session = json_get_bool(args, "live_session", false);
    cfg.catch_resolve = json_get_bool(args, "catch_resolve", true);
    cfg.required = json_obj_get(args, "required");
    return tmc2_wrapped_handler(ctx, rid, params, &cfg);
}

/* PoP: tmc2_guarded @ tui_gateway/methods_tools.py:_guarded */
static json_t *tmc2_guarded(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    tmc2_wrap_cfg_t cfg;
    cfg.fail_code = (int)json_get_num(args, "fail_code", 5024);
    cfg.prefix = json_get_str(args, "prefix", "");
    cfg.scoped = false;
    cfg.live_session = json_get_bool(args, "live_session", false);
    cfg.catch_resolve = true;
    cfg.required = NULL;
    return tmc2_wrapped_handler(ctx, rid, params, &cfg);
}

/* PoP: tmc2_rpc @ tui_gateway/methods_tools.py:_rpc */
static json_t *tmc2_rpc(tmc2_ctx_t *ctx, const json_t *args) {
    /* method(name) registration is silent; body == _guarded(...) */
    return tmc2_guarded(ctx, args);
}

/* PoP: tmc2_scoped_rpc @ tui_gateway/methods_tools.py:_scoped_rpc */
static json_t *tmc2_scoped_rpc(tmc2_ctx_t *ctx, const json_t *args) {
    /* method(name) registration is silent; body == _profile_scoped_rpc(...) */
    return tmc2_profile_scoped_rpc(ctx, args);
}

/* PoP: tmc2_mcp_rpc @ tui_gateway/methods_tools.py:_mcp_rpc */
static json_t *tmc2_mcp_rpc(tmc2_ctx_t *ctx, const json_t *args) {
    /* _scoped_rpc(f"mcp.servers.{name}", required=required, catch_resolve=False) */
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    tmc2_wrap_cfg_t cfg;
    cfg.fail_code = 5024;
    cfg.prefix = "";
    cfg.scoped = true;
    cfg.live_session = false;
    cfg.catch_resolve = false;
    cfg.required = json_obj_get(args, "required");
    if (!cfg.required) {
        /* default _NAME = (("name", _stripped),) */
        json_t *def = json_array();
        json_t *pair = json_array();
        json_append(pair, json_string("name"));
        json_append(pair, json_string("stripped"));
        json_append(def, pair);
        cfg.required = def;
        json_t *r = tmc2_wrapped_handler(ctx, rid, params, &cfg);
        json_free(def);
        return r;
    }
    return tmc2_wrapped_handler(ctx, rid, params, &cfg);
}

/* -- cron.manage handler body -- */
/* json.loads over the cronjob JSON-string result (real on both sides). A
 * non-string operand raises the live TypeError; fixtures keep valid integral
 * JSON strings (no int/float identity drift). */
static json_t *tmc2_json_loads(const json_t *raw) {
    const char *s = tmc2_str(raw);
    if (!s) {
        char msg[160];
        snprintf(msg, sizeof msg,
                 "the JSON object must be str, bytes or bytearray, not %s",
                 tmc2_type_name(raw));
        return tmc2_err_spec("TypeError", msg);
    }
    json_t *v = json_parse(s, NULL);
    return v ? v : json_null();
}

/* PoP: tmc2_cron_manage @ tui_gateway/methods_tools.py:_ */
static json_t *tmc2_cron_manage(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *m = tmc2_tools_mod(ctx, "tools.cronjob_tools");
    if (tmc2_is_raise(m)) return m;
    json_free(m);
    const json_t *action_node = json_obj_get(params, "action");
    char *action = action_node ? tmc2_pystr(action_node) : tmc2_strdup("list");
    const json_t *jid_node = json_obj_get(params, "name");
    json_t *jid = jid_node ? json_copy(jid_node) : json_string("");
    json_t *ret = NULL;
    if (!strcmp(action, "list")) {
        const json_t *inc = json_obj_get(params, "include_disabled");
        bool include_disabled = tmc2_is_truthy_value(inc, false);
        json_t *a = tmc2_args0();
        tmc2_asets(a, "action", "list");
        tmc2_asetb(a, "include_disabled", include_disabled);
        json_t *raw = tmc2_call(ctx, "cronjob", a);
        if (tmc2_is_raise(raw)) { ret = raw; goto done; }
        json_t *result = tmc2_json_loads(raw);
        json_free(raw);
        if (tmc2_is_raise(result)) { ret = result; goto done; }
        json_t *pn = tmc2_str_arg(ctx, params, "profile");
        if (tmc2_is_raise(pn)) { json_free(result); ret = pn; goto done; }
        const char *profile = tmc2_str(pn);
        if (profile && profile[0]) json_set_hdr(result, "scoped", profile);
        json_free(pn);
        ret = tmc2_ok(ctx, rid, result);
    } else if (!strcmp(action, "add")) {
        /* repeat = int(params["repeat"]) if str(params.get("repeat","")).strip().isdigit() else None */
        const json_t *rep = json_obj_get(params, "repeat");
        char *rep_s = tmc2_param_str(params, "repeat", "");
        char *rep_st = tmc2_strip(rep_s);
        json_t *repeat = NULL;
        if (tmc2_isdigit(rep_st)) {
            json_t *rerr = NULL;
            repeat = tmc2_pyint(rep, &rerr);
            if (!repeat) { free(rep_s); free(rep_st); ret = rerr; goto done; }
        }
        free(rep_s); free(rep_st);
        /* continuity = is_truthy_value(v) if v is not None else None */
        const json_t *cont = json_obj_get(params, "continuity");
        json_t *continuity = NULL;
        if (cont && cont->type != JSON_NULL)
            continuity = json_bool(tmc2_is_truthy_value(cont, false));
        /* deliver = _str_arg(params,"deliver") or None */
        json_t *dn = tmc2_str_arg(ctx, params, "deliver");
        if (tmc2_is_raise(dn)) { json_free(repeat); json_free(continuity); ret = dn; goto done; }
        const char *ds = tmc2_str(dn);
        json_t *deliver = (ds && ds[0]) ? json_string(ds) : json_null();
        json_free(dn);
        char *schedule = tmc2_param_str(params, "schedule", "");
        char *prompt = tmc2_param_str(params, "prompt", "");
        json_t *a = tmc2_args0();
        tmc2_asets(a, "action", "create");
        tmc2_aset(a, "name", jid);
        tmc2_asets(a, "schedule", schedule);
        tmc2_asets(a, "prompt", prompt);
        tmc2_aset(a, "repeat", repeat);
        tmc2_aset(a, "continuity", continuity);
        tmc2_aset(a, "deliver", deliver);
        free(schedule); free(prompt);
        json_free(repeat); json_free(continuity); json_free(deliver);
        json_t *raw = tmc2_call(ctx, "cronjob", a);
        if (tmc2_is_raise(raw)) { ret = raw; goto done; }
        json_t *result = tmc2_json_loads(raw);
        json_free(raw);
        ret = tmc2_is_raise(result) ? result : tmc2_ok(ctx, rid, result);
    } else if (!strcmp(action, "remove") || !strcmp(action, "pause") || !strcmp(action, "resume")) {
        json_t *a = tmc2_args0();
        tmc2_asets(a, "action", action);
        tmc2_aset(a, "job_id", jid);
        json_t *raw = tmc2_call(ctx, "cronjob", a);
        if (tmc2_is_raise(raw)) { ret = raw; goto done; }
        json_t *result = tmc2_json_loads(raw);
        json_free(raw);
        ret = tmc2_is_raise(result) ? result : tmc2_ok(ctx, rid, result);
    } else {
        char msg[200];
        snprintf(msg, sizeof msg, "unknown cron action: %s", action);
        ret = tmc2_err(ctx, rid, 4016, msg, NULL);
    }
done:
    free(action);
    json_free(jid);
    return ret;
}

/* -- MCP-server helpers (tools.configure / cron-adjacent) -- */

/* PoP: tmc2_mcp_server_rows @ tui_gateway/methods_tools.py:_mcp_server_rows */
static json_t *tmc2_mcp_server_rows(tmc2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *m1 = tmc2_tools_mod(ctx, "hermes_cli.mcp_config");
    if (tmc2_is_raise(m1)) return m1;
    json_free(m1);
    json_t *cfg = tmc2_call(ctx, "mcp_config._get_mcp_servers", tmc2_args0());
    if (tmc2_is_raise(cfg)) return cfg;
    json_t *m2 = tmc2_tools_mod(ctx, "tui_gateway.mcp_rpc_helpers");
    if (tmc2_is_raise(m2)) { json_free(cfg); return m2; }
    json_free(m2);
    json_t *a = tmc2_args0();
    tmc2_aset(a, "config_servers", cfg);
    json_free(cfg);
    return tmc2_call(ctx, "mcp_rpc_helpers.server_configs_with_sources", a);
}

/* PoP: tmc2_mcp_named_server @ tui_gateway/methods_tools.py:_mcp_named_server */
static json_t *tmc2_mcp_named_server(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *name = tmc2_str_arg(ctx, params, "name");
    if (tmc2_is_raise(name)) return name;
    json_t *rows = tmc2_call(ctx, "mcp_server_rows", tmc2_args0());
    if (tmc2_is_raise(rows)) { json_free(name); return rows; }
    const json_t *servers = json_get(rows, 0);
    const char *ns = tmc2_str(name);
    json_t *out = json_array();
    json_append(out, json_copy(name));
    json_append(out, servers ? json_copy(servers) : json_null());
    if (servers && servers->type == JSON_OBJECT) {
        bool found = ns && json_obj_get(servers, ns) != NULL;
        if (found) {
            json_append(out, json_null());
        } else {
            char msg[200];
            snprintf(msg, sizeof msg, "server '%s' not found", ns ? ns : "");
            json_t *e = tmc2_err(ctx, rid, 4064, msg, NULL);
            if (tmc2_is_raise(e)) { json_free(out); json_free(name); json_free(rows); return e; }
            json_append(out, e);
        }
    } else {
        /* `name in servers` on a non-container raises (py3.14 wording) */
        char msg[160];
        snprintf(msg, sizeof msg, "argument of type '%s' is not a container or iterable",
                 tmc2_type_name(servers));
        json_free(out); json_free(name); json_free(rows);
        return tmc2_err_spec("TypeError", msg);
    }
    json_free(name); json_free(rows);
    return out;
}

/* PoP: tmc2_mcp_config_server_or_error @ tui_gateway/methods_tools.py:_mcp_config_server_or_error */
static json_t *tmc2_mcp_config_server_or_error(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *a = tmc2_args0();
    tmc2_aset(a, "rid", rid);
    tmc2_aset(a, "params", params);
    json_t *r = tmc2_call(ctx, "mcp_named_server", a);
    if (tmc2_is_raise(r)) return r;
    const json_t *err = json_get(r, 2);
    if (tmc2_truthy(err)) return r;   /* (name, servers, err) */
    const json_t *name = json_get(r, 0);
    const json_t *servers = json_get(r, 1);
    json_t *rows = tmc2_call(ctx, "mcp_server_rows", tmc2_args0());
    if (tmc2_is_raise(rows)) { json_free(r); return rows; }
    const json_t *plugins = json_get(rows, 1);
    json_t *pa = tmc2_args0();
    tmc2_aset(pa, "rid", rid);
    tmc2_aset(pa, "name", name);
    tmc2_aset(pa, "plugins", plugins);
    json_t *pwe = tmc2_call(ctx, "mcp_plugin_write_error", pa);
    if (tmc2_is_raise(pwe)) { json_free(r); json_free(rows); return pwe; }
    json_t *out = json_array();
    json_append(out, name ? json_copy(name) : json_null());
    json_append(out, servers ? json_copy(servers) : json_null());
    json_append(out, pwe);
    json_free(r); json_free(rows);
    return out;
}
/* -- rewind cluster (composes the claimed _busy_error sibling) -- */

/* PoP: tmc2_user_turn_indices @ tui_gateway/methods_tools.py:_user_turn_indices */
static json_t *tmc2_user_turn_indices(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    json_t *m = tmc2_tools_mod(ctx, "session_rules.turns");
    if (tmc2_is_raise(m)) return m;
    json_free(m);   /* .user_originated_turn_view attribute fetch is silent */
    const json_t *hist_in = json_obj_get(session, "history");
    json_t *ha = tmc2_args0();
    json_set(ha, "history", hist_in ? json_copy(hist_in) : json_array());
    json_t *history = tmc2_call(ctx, "history_without_ephemeral_scaffolding", ha);
    if (tmc2_is_raise(history)) return history;
    json_t *indices = json_array();
    if (history && history->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(history); i++) {
            json_t *ma = tmc2_args0();
            tmc2_aset(ma, "m", json_get(history, i));
            json_t *r = tmc2_call(ctx, "turns.user_originated_turn_view", ma);
            if (tmc2_is_raise(r)) { json_free(history); json_free(indices); return r; }
            if (r && r->type != JSON_NULL) json_append(indices, json_number((double)i));
            json_free(r);
        }
    }
    json_t *out = json_array();
    json_append(out, history ? json_copy(history) : json_null());
    json_append(out, indices);
    json_free(history);
    return out;
}

/* PoP: tmc2_rewind_prelude @ tui_gateway/methods_tools.py:_rewind_prelude */
static json_t *tmc2_rewind_prelude(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *session = json_obj_get(args, "session");
    const char *cmd = json_get_str(args, "cmd", "");
    const char *empty_msg = json_get_str(args, "empty_msg", "");
    json_t *a = tmc2_args0();
    tmc2_aset(a, "rid", rid);
    tmc2_aset(a, "session", session);
    tmc2_asets(a, "cmd", cmd);
    json_t *busy = tmc2_call(ctx, "busy_error", a);
    if (tmc2_is_raise(busy)) return busy;
    if (tmc2_truthy(busy)) {
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, json_null());
        json_append(out, busy);
        return out;
    }
    json_free(busy);
    json_t *ua = tmc2_args0();
    tmc2_aset(ua, "session", session);
    json_t *r = tmc2_call(ctx, "user_turn_indices", ua);
    if (tmc2_is_raise(r)) return r;
    const json_t *history = json_get(r, 0);
    const json_t *indices = json_get(r, 1);
    json_t *out = json_array();
    if (!indices || indices->type != JSON_ARRAY || json_len(indices) == 0) {
        json_t *e = tmc2_err(ctx, rid, 4018, empty_msg, NULL);
        if (tmc2_is_raise(e)) { json_free(out); json_free(r); return e; }
        json_append(out, json_null());
        json_append(out, json_null());
        json_append(out, e);
    } else {
        json_append(out, history ? json_copy(history) : json_null());
        json_append(out, json_copy(indices));
        json_append(out, json_null());
    }
    json_free(r);
    return out;
}

/* PoP: tmc2_rewind_or_err @ tui_gateway/methods_tools.py:_rewind_or_err */
static json_t *tmc2_rewind_or_err(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *session = json_obj_get(args, "session");
    const json_t *keep = json_obj_get(args, "keep");
    const json_t *kw = json_obj_get(args, "kw");
    const json_t *value_err = json_obj_get(args, "value_err");
    const char *fail_prefix = json_get_str(args, "fail_prefix", "");
    json_t *a = tmc2_args0();
    tmc2_aset(a, "session", session);
    tmc2_aset(a, "keep", keep);
    json_set(a, "kw", kw ? json_copy(kw) : json_object());
    json_t *r = tmc2_call(ctx, "rewind_active_session_history", a);
    if (!tmc2_is_raise(r)) {
        json_t *out = json_array();
        json_append(out, r);
        json_append(out, json_null());
        return out;
    }
    const char *kind = tmc2_raise_kind(r);
    const char *m = json_get_str(r, "message", "");
    json_t *out = json_array();
    if (tmc2_exc_is_a(kind, "SessionNotOwned")) {
        json_t *ea = tmc2_args0();
        tmc2_aset(ea, "rid", rid);
        tmc2_aset(ea, "exc", r);
        json_t *e = tmc2_call(ctx, "session_not_owned_err", ea);
        json_free(r);
        if (tmc2_is_raise(e)) return e;
        json_append(out, json_null());
        json_append(out, e);
        return out;
    }
    if (tmc2_exc_is_a(kind, "ValueError")) {
        const json_t *ve0 = json_get(value_err, 0);
        int code = (int)(ve0 && ve0->type == JSON_NUMBER ? ve0->num_val : 0);
        const char *prefix = tmc2_str(json_get(value_err, 1));
        size_t pn = strlen(prefix ? prefix : "");
        char *full = malloc(pn + strlen(m) + 1);
        if (full) { memcpy(full, prefix ? prefix : "", pn); strcpy(full + pn, m); }
        json_free(r);
        json_t *e = tmc2_err(ctx, rid, code, full ? full : m, NULL);
        free(full);
        if (tmc2_is_raise(e)) { json_free(out); return e; }
        json_append(out, json_null());
        json_append(out, e);
        return out;
    }
    if (tmc2_is_base_exc(kind)) { json_free(out); return r; }
    /* except Exception */
    size_t pn = strlen(fail_prefix);
    char *full = malloc(pn + strlen(m) + 1);
    if (full) { memcpy(full, fail_prefix, pn); strcpy(full + pn, m); }
    json_free(r);
    json_t *e = tmc2_err(ctx, rid, 5008, full ? full : m, NULL);
    free(full);
    if (tmc2_is_raise(e)) { json_free(out); return e; }
    json_append(out, json_null());
    json_append(out, e);
    return out;
}

/* -- learning / skills (the "Cron / learning / skills" section) -- */

/* PoP: tmc2_learning_mutation @ tui_gateway/methods_tools.py:_learning_mutation */
static json_t *tmc2_learning_mutation(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const char *fn_name = json_get_str(args, "fn_name", "");
    const json_t *arg_keys = json_obj_get(args, "arg_keys");
    json_t *m = tmc2_tools_mod(ctx, "agent.learning_mutations");
    if (tmc2_is_raise(m)) return m;
    json_free(m);   /* getattr(module, fn_name) is silent on the module double */
    json_t *strs = json_array();
    if (arg_keys && arg_keys->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(arg_keys); i++) {
            const char *k = tmc2_str(json_get(arg_keys, i));
            char *s = tmc2_param_str(params, k ? k : "", "");
            json_append(strs, json_string(s ? s : ""));
            free(s);
        }
    }
    json_t *a = tmc2_args0();
    tmc2_asets(a, "fn", fn_name);
    json_set(a, "args", strs);
    json_t *r = tmc2_call(ctx, "learning_mutations.call", a);
    if (tmc2_is_raise(r)) return r;
    return tmc2_ok(ctx, rid, r);
}

/* PoP: tmc2_learning_frames @ tui_gateway/methods_tools.py:_ */
static json_t *tmc2_learning_frames(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    /* cols,rows,frames = (int(params.get(k,d) or d) ...); except -> defaults */
    static const char *keys[3] = {"cols", "rows", "frames"};
    double defs[3] = {80, 24, 48};
    double vals[3] = {80, 24, 48};
    bool defaulted = false;
    for (int i = 0; i < 3 && !defaulted; i++) {
        const json_t *v = json_obj_get(params, keys[i]);
        json_t *tmp = NULL;
        if (!v) tmp = json_number(defs[i]);
        else if (!tmc2_truthy(v)) tmp = json_number(defs[i]);
        const json_t *use = tmp ? tmp : v;
        json_t *rerr = NULL;
        json_t *iv = tmc2_pyint(use, &rerr);
        if (tmp) json_free(tmp);
        if (!iv) {
            const char *k = tmc2_raise_kind(rerr);
            if (tmc2_exc_is_a(k, "TypeError") || tmc2_exc_is_a(k, "ValueError")) {
                json_free(rerr);
                defaulted = true;
            } else {
                return rerr;
            }
        } else {
            vals[i] = iv->num_val;
            json_free(iv);
        }
    }
    if (defaulted) { vals[0] = 80; vals[1] = 24; vals[2] = 48; }
    double cols = vals[0] < 20 ? 20 : vals[0];
    double rows = vals[1] < 10 ? 10 : vals[1];
    double frames = vals[2];
    json_t *m1 = tmc2_tools_mod(ctx, "agent.learning_graph");
    if (tmc2_is_raise(m1)) return m1;
    json_free(m1);
    json_t *graph = tmc2_call(ctx, "learning_graph.build_learning_graph", tmc2_args0());
    if (tmc2_is_raise(graph)) return graph;
    json_t *m2 = tmc2_tools_mod(ctx, "agent.learning_graph_render");
    if (tmc2_is_raise(m2)) { json_free(graph); return m2; }
    json_free(m2);
    json_t *a = tmc2_args0();
    tmc2_aset(a, "graph", graph);
    tmc2_asetn(a, "cols", cols);
    tmc2_asetn(a, "rows", rows);
    tmc2_asetn(a, "frames", frames);
    json_free(graph);
    json_t *r = tmc2_call(ctx, "learning_graph_render.render_frames", a);
    if (tmc2_is_raise(r)) return r;
    return tmc2_ok(ctx, rid, r);
}

/* PoP: tmc2_skills_search @ tui_gateway/methods_tools.py:_skills_search */
static json_t *tmc2_skills_search(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *query = json_obj_get(args, "query");
    json_t *m1 = tmc2_tools_mod(ctx, "tools.skills_hub_search");
    if (tmc2_is_raise(m1)) return m1;
    json_free(m1);
    json_t *m2 = tmc2_tools_mod(ctx, "tools.skills_hub_github");
    if (tmc2_is_raise(m2)) return m2;
    json_free(m2);
    json_t *auth = tmc2_call(ctx, "skills_hub_github.GitHubAuth", tmc2_args0());
    if (tmc2_is_raise(auth)) return auth;
    json_t *ra = tmc2_args0();
    tmc2_aset(ra, "auth", auth);
    json_free(auth);
    json_t *router = tmc2_call(ctx, "skills_hub_search.create_source_router", ra);
    if (tmc2_is_raise(router)) return router;
    json_t *ua = tmc2_args0();
    tmc2_aset(ua, "query", query);
    tmc2_aset(ua, "router", router);
    tmc2_asets(ua, "source_filter", "all");
    tmc2_asetn(ua, "limit", 20);
    json_free(router);
    json_t *raw = tmc2_call(ctx, "skills_hub_search.unified_search", ua);
    if (tmc2_is_raise(raw)) return raw;
    /* raw or [] */
    const json_t *rows = tmc2_truthy(raw) ? raw : NULL;
    json_t *results = json_array();
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows); i++) {
            const json_t *row = json_get(rows, i);
            if (!row || row->type != JSON_OBJECT) {
                char msg[160];
                snprintf(msg, sizeof msg, "'%s' object has no attribute 'name'",
                         tmc2_type_name(row));
                json_free(raw); json_free(results);
                return tmc2_err_spec("AttributeError", msg);
            }
            json_t *entry = json_object();
            tmc2_aset(entry, "name", json_obj_get(row, "name"));
            tmc2_aset(entry, "description", json_obj_get(row, "description"));
            json_append(results, entry);
        }
    }
    json_free(raw);
    json_t *payload = json_object();
    json_set(payload, "results", results);
    return tmc2_ok(ctx, rid, payload);
}

/* PoP: tmc2_skills_browse @ tui_gateway/methods_tools.py:_skills_browse */
static json_t *tmc2_skills_browse(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const char *query = json_get_str(args, "query", "");
    /* pg = int(params.get("page",0) or 0) or (int(query) if query.isdigit() else 1) */
    const json_t *page_v = json_obj_get(params, "page");
    json_t *tmp = NULL;
    if (!page_v) tmp = json_number(0);
    else if (!tmc2_truthy(page_v)) tmp = json_number(0);
    const json_t *use = tmp ? tmp : page_v;
    json_t *rerr = NULL;
    json_t *pg_node = tmc2_pyint(use, &rerr);
    if (tmp) json_free(tmp);
    if (!pg_node) return rerr;
    double pg = pg_node->num_val;
    json_free(pg_node);
    if (pg == 0) {
        if (tmc2_isdigit(query)) {
            json_t *qv = json_string(query);
            json_t *qn = tmc2_pyint(qv, &rerr);
            json_free(qv);
            if (!qn) return rerr;
            pg = qn->num_val;
            json_free(qn);
        } else {
            pg = 1;
        }
    }
    json_t *m = tmc2_tools_mod(ctx, "hermes_cli.skills_hub");
    if (tmc2_is_raise(m)) return m;
    json_free(m);
    /* page_size = int(params.get("page_size",20)) -- evaluated before browse() */
    const json_t *ps_v = json_obj_get(params, "page_size");
    json_t *ps_tmp = NULL;
    if (!ps_v) ps_tmp = json_number(20);
    const json_t *ps_use = ps_tmp ? ps_tmp : ps_v;
    json_t *ps_node = tmc2_pyint(ps_use, &rerr);
    if (ps_tmp) json_free(ps_tmp);
    if (!ps_node) return rerr;
    double ps = ps_node->num_val;
    json_free(ps_node);
    json_t *a = tmc2_args0();
    tmc2_asetn(a, "page", pg);
    tmc2_asetn(a, "page_size", ps);
    json_t *r = tmc2_call(ctx, "skills_hub.browse_skills", a);
    if (tmc2_is_raise(r)) return r;
    return tmc2_ok(ctx, rid, r);
}

/* -- captured-exec cluster -- */

/* PoP: tmc2_capture_run_kwargs @ tui_gateway/methods_tools.py:_capture_run_kwargs */
static json_t *tmc2_capture_run_kwargs(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *timeout = json_obj_get(args, "timeout");
    json_t *m = tmc2_tools_mod(ctx, "hermes_cli._subprocess_compat");
    if (tmc2_is_raise(m)) return m;
    json_free(m);
    json_t *flags = tmc2_call(ctx, "subprocess_compat.windows_hide_flags", tmc2_args0());
    if (tmc2_is_raise(flags)) return flags;
    json_t *out = json_object();
    tmc2_asetb(out, "capture_output", true);
    tmc2_asetb(out, "text", true);
    tmc2_asets(out, "encoding", "utf-8");
    tmc2_asets(out, "errors", "replace");
    tmc2_aset(out, "timeout", timeout);
    tmc2_asetn(out, "stdin", -3);   /* subprocess.DEVNULL */
    json_set(out, "creationflags", flags);
    return out;
}

/* PoP: tmc2_captured_exec @ tui_gateway/methods_tools.py:_captured_exec */
static json_t *tmc2_captured_exec(tmc2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *cmd = json_obj_get(args, "cmd");
    const json_t *timeout = json_obj_get(args, "timeout");
    const json_t *shell = json_obj_get(args, "shell");
    const json_t *env = json_obj_get(args, "env");
    const json_t *timeout_err = json_obj_get(args, "timeout_err");
    int fail_code = (int)json_get_num(args, "fail_code", 5008);
    json_t *r = NULL;
    /* try region: os.getcwd, capture_run_kwargs, subprocess.run, on_result */
    json_t *cwd = tmc2_call(ctx, "os.getcwd", tmc2_args0());
    if (tmc2_is_raise(cwd)) { r = cwd; goto handle; }
    json_t *ka = tmc2_args0();
    tmc2_aset(ka, "timeout", timeout);
    json_t *kw = tmc2_call(ctx, "capture_run_kwargs", ka);
    if (tmc2_is_raise(kw)) { json_free(cwd); r = kw; goto handle; }
    json_t *ra = tmc2_args0();
    tmc2_aset(ra, "cmd", cmd);
    tmc2_aset(ra, "cwd", cwd);
    json_set(ra, "shell", shell ? json_copy(shell) : json_bool(false));
    tmc2_aset(ra, "env", env);
    tmc2_aset(ra, "kwargs", kw);
    json_free(cwd); json_free(kw);
    json_t *sr = tmc2_call(ctx, "subprocess.run", ra);
    if (tmc2_is_raise(sr)) { r = sr; goto handle; }
    json_t *oa = tmc2_args0();
    tmc2_aset(oa, "result", sr);
    json_free(sr);
    json_t *orr = tmc2_call(ctx, "on_result", oa);
    if (tmc2_is_raise(orr)) { r = orr; goto handle; }
    return orr;
handle:
    {
        const char *kind = tmc2_raise_kind(r);
        if (tmc2_exc_is_a(kind, "TimeoutExpired")) {
            const json_t *te0 = json_get(timeout_err, 0);
            int code = (int)(te0 && te0->type == JSON_NUMBER ? te0->num_val : 0);
            const char *msg = tmc2_str(json_get(timeout_err, 1));
            json_free(r);
            return tmc2_err(ctx, rid, code, msg ? msg : "", NULL);
        }
        if (tmc2_is_base_exc(kind)) return r;
        const char *m = json_get_str(r, "message", "");
        json_free(r);
        return tmc2_err(ctx, rid, fail_code, m, NULL);
    }
}
