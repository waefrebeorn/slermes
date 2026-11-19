/*
 * web_server_flow.c — FLOW port of the UNCLAIMED web-server core surface of
 * hermes_cli/web_server.py (LIVE authority). Self-contained: libjson only. All
 * functions static ws_*; the harness (#include "cli/web_server_flow.c") wires
 * the universal seam to scripted results and records the ordered trace.
 *
 * This is the DISTINCT web_server.py module (FastAPI app assembly, auth/host
 * middleware chain, startup auth-gate refusal, host rendezvous, serve runner).
 * The ws_revive1-4 / web_server_wrappers.c ports cover OTHER functions; every
 * function here was verified UNCLAIMED by grepping PoP annotations tree-wide.
 *
 * Covered flows (PoP per function):
 *   app-state leaves: _app_state_default, _get_pty_active_session_files
 *   ssh bootstrap: _apply_ssh_session_token, _apply_ssh_owner_nonce
 *   cron ownership leaf: _gateway_owns_cron
 *   token guards: _require_token
 *   middleware chain: host_header_middleware, _plugin_api_runtime_gate,
 *     _dashboard_auth_gate, auth_middleware, _token_auth_seam,
 *     _dashboard_health_middleware
 *   health leaf: DashboardHealth.record_selftest
 *   CORS origin: _is_trusted_local_origin,
 *     _LocalOriginCORSMiddleware.is_allowed_origin
 *   startup auth gate: _no_auth_provider_message, _configure_auth_gate
 *   best-effort / rendezvous: _best_effort,
 *     _reclaim_host_from_orphaned_owner, _publish_host_rendezvous
 *   serve runner: _windows_serve_loop_factory, _run_serve, start_server
 *
 * SEAM DOCTRINE: every live effect enters through ONE universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returning a NEW json_t* (NULL == Python None). A raising live call is
 * scripted {"__raise__": "<ExcClass>", "msg": ...[, "code": ...]}; the cores
 * propagate it exactly where the live try/except catches it. except Exception
 * does NOT catch KeyboardInterrupt/SystemExit/GeneratorExit/BaseException —
 * those pass through to the harness ret envelope (BaseException passthrough).
 * A MISSING seam key fail-closes: the harness delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None. Claimed siblings (_has_valid_session_token,
 * _has_valid_query_token, should_require_dashboard_auth, _dashboard_public_hosts,
 * DashboardHealth.record_error, _port_bind_conflict, _report_port_in_use) and
 * foreign collaborators (boundary/lifecycle/profiles/dashboard_auth/
 * host_rendezvous/process_identity/uvicorn/pm) are SEAMED; in-function imports
 * fire an "import_<name>" seam on module attribute access (leading underscore
 * stripped), mirroring the oracle's stub modules. Unclaimed siblings composed
 * here (_app_state_default, _is_trusted_local_origin,
 * _reclaim_host_from_orphaned_owner, _no_auth_provider_message,
 * _windows_serve_loop_factory) are extracted and run for real on BOTH sides.
 * _start_server_runtime is a heavy I/O orchestrator left SEAMED in start_server.
 *
 * Seam name table (name -> live call, args):
 *   check_gateway_running        _check_gateway_running(Path(home)) {home}
 *   served_by_running_multiplexer _served_by_running_multiplexer(name) {name}
 *   app_state_factory            factory() in _app_state_default {}
 *   sysconfig_get_paths          sysconfig.get_paths() {}
 *   os_path_join                 os.path.join(purelib, suffix) {path,suffix}
 *   open_marker                  open(marker,"w",encoding="utf-8") {path,mode,encoding}
 *   marker_file_enter            file CM __enter__ {}
 *   marker_file_write            fh.write(data) {data}
 *   marker_file_exit             file CM __exit__ {} (raise overrides body raise)
 *   os_getpid                    os.getpid() {}
 *   os_stat                      os.stat(purelib) {path} -> {st_dev,st_ino}
 *   has_valid_session_token      _has_valid_session_token(request) {request}
 *   has_valid_query_token        _has_valid_query_token(request,path) {request,path}
 *   is_accepted_host             _is_accepted_host(host,bound,trusted) {host,bound_host,trusted_public_hosts}
 *   call_next                    await call_next(request) {request}
 *   plugin_api_name              _plugin_api_name(request.scope) {scope}
 *   plugin_request_scope         plugin_request_scope(scope) CM factory {scope}
 *   plugin_scope_enter           CM __enter__ {}
 *   plugin_scope_exit            CM __exit__ {}
 *   plugin_api_request_refused   await _plugin_api_request_refused(name) {plugin_name}
 *   forward_secondary_plugin_request await forward...(request,name) {request,plugin_name}
 *   gated_auth_middleware        await gated_auth_middleware(request,call_next) {request,call_next}
 *   is_peer_path                 is_peer_path(path) {path}
 *   peer_pass_middleware         await peer_pass_middleware(request,call_next) {request,call_next}
 *   authenticate_native_http     await authenticate_native_http(request) {request}
 *   native_profile_scope         native_profile_scope(request) CM factory {request}
 *   native_scope_enter           CM __enter__ {}
 *   native_scope_exit            CM __exit__ {}
 *   token_auth_middleware        await token_auth_middleware(request,call_next) {request,call_next}
 *   error_type_name              error_type_name(exc) {exc:{__exc__,msg}}
 *   record_error                 DASHBOARD_HEALTH.record_error(type,path) {exc_type,path}
 *   time_time                    time.time() {}
 *   is_trusted_local_origin      is_trusted_local_origin(origin,app.state) {origin,app_state}
 *   nous_last_skip_reason        _nous_plugin.LAST_SKIP_REASON (read TWICE) {}
 *   resolve_public_url           resolve_public_url() {}
 *   load_config                  load_config() {}
 *   import_BASIC_AUTH_PLUGIN_KEYS  _BASIC_AUTH_PLUGIN_KEYS data import {} -> [keys]
 *   dashboard_public_hosts       _dashboard_public_hosts() {}
 *   public_url_hosts             public_url_hosts(own_url) {url}
 *   dashboard_public_origins     _dashboard_public_origins() {}
 *   public_url_origins           public_url_origins(own_url) {url}
 *   desktop_loopback_auth_exempt _desktop_loopback_auth_exempt(host,tok,nonce) {host,ssh_session_token,ssh_owner_nonce}
 *   should_require_auth          should_require_auth(host) {host}
 *   should_require_dashboard_auth should_require_dashboard_auth(host,trusted) {host,trusted_public_hosts}
 *   list_providers               list_providers() {} -> [{name}] (called TWICE when truthy)
 *   best_effort_fn               fn() in _best_effort {}
 *   read_record                  hr.read_record(role) {role} -> owner|null
 *   discard_dead_record          hr.discard_dead_record(role) {role}
 *   reap_orphaned_backend_owner  reap_orphaned_backend_owner(pid,ctime) {pid,create_time}
 *   is_desktop_owned_backend     is_desktop_owned_backend() {}
 *   claim_host_lock              hr.claim_host_lock(role) {role} -> [outcome_token,error]
 *   describe                     hr.describe(owner) {record}
 *   publish_record               hr.publish_record(role,host=,port=,profiles=,token=) {role,host,port,profiles,token}
 *   served_profiles              hr.served_profiles() {} -> [profiles]
 *   cleanup_on_exit              hr.cleanup_on_exit(role) {role}
 *   get_loop_factory             config.get_loop_factory() {} -> callable marker|null
 *   serve_call                   serve() coroutine creation {}
 *   asyncio_run                  asyncio.run(coro,**kw) {coro[,loop_factory]}
 *   uvicorn_asyncio_run          uvicorn._compat.asyncio_run(coro,**kw) {coro[,loop_factory]}
 *   windows_selector_policy      asyncio.WindowsSelectorEventLoopPolicy() {}
 *   set_event_loop_policy        asyncio.set_event_loop_policy(policy) {policy}
 *   port_bind_conflict           _port_bind_conflict(host,port) {host,port}
 *   report_port_in_use           _report_port_in_use(host,port) {host,port}
 *   suppress_prompts_cm          suppress_interactive_install_prompts() CM factory {}
 *   suppress_prompts_enter       CM __enter__ {}
 *   suppress_prompts_exit        CM __exit__ {}
 *   start_server_runtime         _start_server_runtime(*args,**kwargs) {args,kwargs}
 *   import_<attr>                in-function from-import attribute access {}
 *
 * Callable markers: factories / loop factories / call_next project to
 * {"__callable__":"<tag>"}. _windows_serve_loop_factory returns the selector
 * marker when the scripted factory is null or "proactor", else passes the
 * marker through. call_next always projects {"__callable__":"call_next"}.
 *
 * Outcome tokens (hr.HostLockOutcome enum VALUE strings): "acquired",
 * "held-by-other", "could-not-open". claim_host_lock scripts a 2-element array
 * [token, error]; the oracle materializes enum singletons so `is` identity
 * holds, C compares the token string.
 *
 * Module constants (live-seeded in the oracle, hardcoded here):
 *   _LOOPBACK_HOST_VALUES = {"localhost","127.0.0.1","::1"}
 *     (hermes_cli/web_server_boundary.py)
 *   _PUBLIC_API_PATHS = {"/api/health","/api/status","/api/config/defaults",
 *     "/api/config/schema","/api/model/info","/api/dashboard/themes",
 *     "/api/dashboard/plugins","/api/cron/fire"}
 *     (hermes_cli/dashboard_auth/public_paths.py)
 *   PORT_IN_USE_EXIT_CODE = 75 (hermes_cli/web_server_lifecycle.py)
 *   _DASHBOARD_HEALTH_WINDOW_SECONDS = 300.0 (health default state)
 *
 * Fixture domains (documented exclusions):
 * - ASCII strings; POSIX path strings. Integral ints (no integral-float
 *   literals in type-name-sensitive / f-string positions; canon collapses
 *   integral floats to ints on both sides).
 * - request schema: {state:{}, app:{state:{}}, headers:{}, query_params:{},
 *   url:{path:""}, scope:{}} — defaults filled identically on both sides.
 *   headers/query_params values are strings; url carries only path.
 * - `is True` identity gates (hosted_http_authenticated) are JSON_BOOL-true
 *   only; fixture 1 reads False.
 * - Truthiness-gated seam records (owner, cfg sections, providers) are null or
 *   NON-EMPTY dicts carrying the fields the flow reads; an empty {} record is
 *   outside the domain where live SimpleNamespace is truthy but libjson {} is
 *   falsy (owner={} is allowed ONLY where the flow reads .pid and the
 *   AttributeError is the exercised branch).
 * - claim_host_lock scripts are 2-element [token,error] arrays; outcome tokens
 *   are the three enum values. stat scripts carry st_dev/st_ino ints.
 * - call_next scripts for _dashboard_health_middleware are dicts carrying an
 *   int status_code (or __raise__); response materializes as SimpleNamespace.
 * - SystemExit raise specs carry an int "code" (exc.code == 1 gate); other
 *   raise specs carry "msg". HTTPException is a custom Exception subclass
 *   (status_code/detail); its str is "401: Unauthorized".
 * - logger (_log) is silent (no trace events); log ARGUMENT evaluations that
 *   fire seams (list_providers 2nd call, hr.describe) ARE ported.
 * - JSONResponse projects {"__response__":"JSONResponse","status_code","content"}.
 *
 * Ownership: seam results and returned json_t / char pointers are malloc'd,
 * caller frees. Inputs are borrowed. ws_call borrows args (caller frees).
 * A propagated raise dict is returned upward as-is; a caught raise dict is
 * freed at the catch site. Mutation flows mutate the harness working copy
 * (echoed as out-state); _apply_ssh_* build a fresh echo object.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include "json.h"

/* ── universal seam + trace ── */

typedef struct ws_trace { json_t *events; } ws_trace_t;

typedef struct ws_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} ws_seams_t;

static void ws_trace_add(ws_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void ws_trace_free(ws_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *ws_call(const ws_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *ws_call0(const ws_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = ws_call(s, name, a);
    json_free(a);
    return r;
}

/* ── JSON value helpers ── */

static bool ws_jtruth(const json_t *v) {
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
static const char *ws_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool ws_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ws_is_true(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}
static bool ws_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ws_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

static char *ws_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

typedef struct { char *p; size_t len, cap; } ws_buf;
static void ws_binit(ws_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void ws_bfree(ws_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void ws_bput(ws_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->len + n + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void ws_bputs(ws_buf *b, const char *s) { ws_bput(b, s, strlen(s)); }
static char *ws_btake(ws_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* ── exception model (FULL builtin parent hierarchy) ── */

static const char *ws_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool ws_is_raise(const json_t *r) { return ws_raise_class(r) != NULL; }

static json_t *ws_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *ws_raise_spec_code(const char *cls, const char *msg, double code) {
    json_t *r = ws_raise_spec(cls, msg);
    json_set(r, "code", json_number(code));
    return r;
}

/* Parent lookup mirroring the py3.14 builtin hierarchy. Unknown custom kinds
 * default to Exception (the oracle builds type(name,(Exception,)) for them);
 * the BaseException trio stays OUTSIDE Exception so except Exception passes
 * them through. */
static bool ws_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
static const char *ws_exc_parent(const char *k) {
    if (!strcmp(k, "BaseException")) return NULL;
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
        !strcmp(k, "GeneratorExit")) return "BaseException";
    if (!strcmp(k, "Exception")) return "BaseException";
    static const char *const EXC_CHILDREN[] = {
        "StopIteration", "StopAsyncIteration", "ArithmeticError", "AssertionError",
        "AttributeError", "BufferError", "EOFError", "ImportError", "LookupError",
        "MemoryError", "NameError", "OSError", "ReferenceError", "RuntimeError",
        "SyntaxError", "SystemError", "TypeError", "ValueError", "Warning", NULL
    };
    if (ws_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (ws_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (ws_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (ws_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (ws_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (ws_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError", NULL };
    if (ws_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (ws_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (ws_in_names(k, WARNING_CHILDREN)) return "Warning";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (ws_in_names(k, NAME_CHILDREN)) return "NameError";
    /* py3.11+ groups / py3.13+ finalization / py3.14 interactive */
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    if (!strcmp(k, "PythonFinalizationError")) return "RuntimeError";
    if (!strcmp(k, "_IncompleteInputError")) return "SyntaxError";
    return "Exception";  /* unknown custom kind: oracle bases it on Exception */
}
/* caught(r, cls): cls appears anywhere in r's parent chain */
static bool ws_caught(const json_t *r, const char *cls) {
    const char *k = ws_raise_class(r);
    /* ExceptionGroup multiply-inherits (BaseExceptionGroup, Exception) */
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = ws_exc_parent(k);
    }
    return false;
}

/* ── small shared helpers ── */

static json_t *ws_json_response(int status, const char *detail) {
    json_t *r = json_object();
    json_set(r, "__response__", json_string("JSONResponse"));
    json_set(r, "status_code", json_number(status));
    json_t *content = json_object();
    json_set(content, "detail", json_string(detail));
    json_set(r, "content", content);
    return r;
}
static json_t *ws_callable_marker(const char *tag) {
    json_t *r = json_object();
    json_set(r, "__callable__", json_string(tag));
    return r;
}
static bool ws_str_in_array(const json_t *arr, const char *s) {
    if (!ws_is_arr(arr) || !s) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = ws_jstr(json_get(arr, i));
        if (v && !strcmp(v, s)) return true;
    }
    return false;
}
/* module constants (live-seeded in oracle) */
static bool ws_is_loopback_host(const char *host) {
    return host && (!strcmp(host, "localhost") || !strcmp(host, "127.0.0.1") ||
                    !strcmp(host, "::1"));
}
static bool ws_is_public_api_path(const char *path) {
    static const char *const PUBLIC[] = {
        "/api/health", "/api/status", "/api/config/defaults", "/api/config/schema",
        "/api/model/info", "/api/dashboard/themes", "/api/dashboard/plugins",
        "/api/cron/fire", NULL
    };
    return ws_in_names(path ? path : "", PUBLIC);
}
#define WS_PORT_IN_USE_EXIT_CODE 75

/* request accessors (canonical request dict) */
static const json_t *ws_req_state(const json_t *req) { return json_obj_get(req, "state"); }
static const json_t *ws_req_app_state(const json_t *req) {
    return json_obj_get(json_obj_get(req, "app"), "state");
}
static const json_t *ws_req_headers(const json_t *req) { return json_obj_get(req, "headers"); }
static const json_t *ws_req_scope(const json_t *req) { return json_obj_get(req, "scope"); }
static const char *ws_req_path(const json_t *req) {
    return json_get_str(json_obj_get(req, "url"), "path", "");
}
static const char *ws_header_get(const json_t *req, const char *key, const char *def) {
    return json_get_str(ws_req_headers(req), key, def);
}
/* getattr(state, name, None): present-null reads as null, absent as null */
static const json_t *ws_state_get(const json_t *state, const char *name) {
    return json_obj_get(state, name);
}

/* CM helper: fire a context-manager factory + enter; returns the factory
 * result (a raise propagates). enter/exit seams are fired by the caller via
 * ws_cm_enter / ws_cm_exit so body-raise / exit-raise ordering is explicit. */
static json_t *ws_cm_enter(const ws_seams_t *s, const char *name) {
    return ws_call0(s, name);
}
/* exit: returns a raise dict if the exit seam raised, else NULL. The exit
 * seam's normal result is discarded (live CMs here never suppress). */
static json_t *ws_cm_exit(const ws_seams_t *s, const char *name) {
    json_t *r = ws_call0(s, name);
    if (ws_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── _gateway_owns_cron ── */
/* PoP: ws_gateway_owns_cron @ hermes_cli/web_server.py:_gateway_owns_cron */
static json_t *ws_gateway_owns_cron(const ws_seams_t *s, const char *name, const char *home) {
    /* from hermes_cli.profiles import _check_gateway_running, _served_by_running_multiplexer */
    json_t *i1 = ws_call0(s, "import_check_gateway_running");
    if (ws_is_raise(i1)) return i1;
    json_free(i1);
    json_t *i2 = ws_call0(s, "import_served_by_running_multiplexer");
    if (ws_is_raise(i2)) return i2;
    json_free(i2);
    /* _check_gateway_running(Path(home)) */
    json_t *a1 = json_object();
    json_set(a1, "home", json_string(home ? home : ""));
    json_t *r1 = ws_call(s, "check_gateway_running", a1);
    json_free(a1);
    if (ws_is_raise(r1)) return r1;
    if (ws_jtruth(r1)) return r1;   /* or: first truthy operand */
    json_free(r1);
    /* (name != "default" and _served_by_running_multiplexer(name)) */
    if (name && !strcmp(name, "default")) return json_bool(false);
    json_t *a2 = json_object();
    json_set(a2, "name", json_string(name ? name : ""));
    json_t *r2 = ws_call(s, "served_by_running_multiplexer", a2);
    json_free(a2);
    return r2;   /* and: last operand (raise propagates) */
}

/* ── _app_state_default ── */
/* factory kinds: "factory" fires app_state_factory seam; "dict" -> {} builtin */
/* PoP: ws_app_state_default @ hermes_cli/web_server.py:_app_state_default */
static json_t *ws_app_state_default(const ws_seams_t *s, json_t *state,
                                    const char *name, const char *factory_kind) {
    /* try: return getattr(app.state, name) */
    if (json_has(state, name)) return json_copy(json_obj_get(state, name));
    /* except AttributeError: value = factory(); setattr; return value */
    json_t *value;
    if (factory_kind && !strcmp(factory_kind, "dict")) {
        value = json_object();
    } else {
        value = ws_call0(s, "app_state_factory");
        if (ws_is_raise(value)) return value;
    }
    json_set(state, name, json_copy(value));
    return value;
}

/* ── _get_pty_active_session_files ── */
/* PoP: ws_get_pty_active_session_files @ hermes_cli/web_server.py:_get_pty_active_session_files */
static json_t *ws_get_pty_active_session_files(const ws_seams_t *s, json_t *state) {
    return ws_app_state_default(s, state, "pty_active_session_files", "dict");
}

/* ── _apply_ssh_session_token ── */
/* PoP: ws_apply_ssh_session_token @ hermes_cli/web_server.py:_apply_ssh_session_token */
static void ws_apply_ssh_session_token(json_t *state, const json_t *token) {
    /* global _SESSION_TOKEN; if token: _SESSION_TOKEN = token */
    if (ws_jtruth(token)) {
        json_set(state, "_SESSION_TOKEN", json_copy(token));
    }
}

/* ── _apply_ssh_owner_nonce ── */
/* PoP: ws_apply_ssh_owner_nonce @ hermes_cli/web_server.py:_apply_ssh_owner_nonce */
static json_t *ws_apply_ssh_owner_nonce(const ws_seams_t *s, json_t *state, const json_t *nonce) {
    /* unconditional reset of the three globals */
    json_set(state, "_SSH_OWNER_NONCE", nonce ? json_copy(nonce) : json_null());
    json_set(state, "_SSH_RUNTIME_PURELIB", json_null());
    json_set(state, "_SSH_RUNTIME_MARKER", json_null());
    if (!ws_jtruth(nonce)) return NULL;
    const char *nonce_s = ws_jstr(nonce);
    if (!nonce_s) nonce_s = "";
    /* try: purelib = sysconfig.get_paths()["purelib"] except (KeyError,OSError): return */
    json_t *paths = ws_call0(s, "sysconfig_get_paths");
    if (ws_is_raise(paths)) {
        if (ws_caught(paths, "KeyError") || ws_caught(paths, "OSError")) {
            json_free(paths); return NULL;
        }
        return paths;
    }
    const json_t *purelib_n = ws_is_obj(paths) ? json_obj_get(paths, "purelib") : NULL;
    if (!purelib_n) {
        /* missing key -> KeyError("'purelib'"), caught */
        json_free(paths); return NULL;
    }
    const char *purelib = ws_jstr(purelib_n);
    if (!purelib) { json_free(paths); return NULL; }
    /* try: marker = os.path.join(purelib, f".hermes-ssh-runtime-{nonce}")
     *      with open(marker,"w",encoding="utf-8") as fh: fh.write(f"pid={os.getpid()}\n")
     *      _SSH_RUNTIME_MARKER = marker
     * except OSError: pass */
    char *suffix = ws_xsf(".hermes-ssh-runtime-%s", nonce_s);
    json_t *ja = json_object();
    json_set(ja, "path", json_string(purelib));
    json_set(ja, "suffix", json_string(suffix));
    json_t *marker_n = ws_call(s, "os_path_join", ja);
    json_free(ja);
    free(suffix);
    bool marker_set = false;
    json_t *body_raise = NULL;
    if (ws_is_raise(marker_n)) {
        body_raise = marker_n;
    } else {
        const char *marker = ws_jstr(marker_n);
        /* open(marker, "w", encoding="utf-8") */
        json_t *oa = json_object();
        json_set(oa, "path", json_string(marker ? marker : ""));
        json_set(oa, "mode", json_string("w"));
        json_set(oa, "encoding", json_string("utf-8"));
        json_t *opened = ws_call(s, "open_marker", oa);
        json_free(oa);
        if (ws_is_raise(opened)) {
            body_raise = opened;
        } else {
            json_free(opened);
            /* with __enter__ */
            json_t *ent = ws_cm_enter(s, "marker_file_enter");
            if (ws_is_raise(ent)) {
                body_raise = ent;
            } else {
                json_free(ent);
                /* fh.write(f"pid={os.getpid()}\n") — getpid evaluated first */
                json_t *pid = ws_call0(s, "os_getpid");
                if (ws_is_raise(pid)) {
                    body_raise = pid;
                } else {
                    double pid_v = (pid && pid->type == JSON_NUMBER) ? pid->num_val : 0;
                    json_free(pid);
                    char *data = ws_xsf("pid=%lld\n", (long long)pid_v);
                    json_t *wa = json_object();
                    json_set(wa, "data", json_string(data));
                    json_t *wr = ws_call(s, "marker_file_write", wa);
                    json_free(wa);
                    free(data);
                    if (ws_is_raise(wr)) body_raise = wr;
                    else json_free(wr);
                }
            }
            /* __exit__ fires even when the body raised; exit raise overrides */
            json_t *exit_raise = ws_cm_exit(s, "marker_file_exit");
            if (exit_raise) {
                if (body_raise) json_free(body_raise);
                body_raise = exit_raise;
            }
            if (!body_raise) marker_set = true;
        }
    }
    if (body_raise) {
        if (ws_caught(body_raise, "OSError")) {
            json_free(body_raise);   /* except OSError: pass */
        } else {
            json_free(marker_n);
            json_free(paths);
            return body_raise;       /* BaseException / non-OSError passthrough */
        }
    }
    if (marker_set) {
        json_set(state, "_SSH_RUNTIME_MARKER", json_copy(marker_n));
    }
    json_free(marker_n);
    /* try: st = os.stat(purelib); _SSH_RUNTIME_PURELIB = (purelib, st.st_dev, st.st_ino)
     * except OSError: pass */
    json_t *sa = json_object();
    json_set(sa, "path", json_string(purelib));
    json_t *st = ws_call(s, "os_stat", sa);
    json_free(sa);
    if (ws_is_raise(st)) {
        if (ws_caught(st, "OSError")) { json_free(st); }
        else { json_free(paths); return st; }
    } else {
        const json_t *dev = ws_is_obj(st) ? json_obj_get(st, "st_dev") : NULL;
        const json_t *ino = ws_is_obj(st) ? json_obj_get(st, "st_ino") : NULL;
        if (dev && ino) {
            json_t *tup = json_array();
            json_append(tup, json_string(purelib));
            json_append(tup, json_copy(dev));
            json_append(tup, json_copy(ino));
            json_set(state, "_SSH_RUNTIME_PURELIB", tup);
        }
        /* missing st_dev/st_ino -> AttributeError (NOT OSError) -> passthrough */
        else if (ws_is_obj(st)) {
            json_free(st); json_free(paths);
            return ws_raise_spec("AttributeError",
                "'types.SimpleNamespace' object has no attribute 'st_dev'");
        }
        json_free(st);
    }
    json_free(paths);
    return NULL;
}

/* ── _require_token ── */
/* PoP: ws_require_token @ hermes_cli/web_server.py:_require_token */
static json_t *ws_require_token(const ws_seams_t *s, const json_t *req) {
    const json_t *st = ws_req_state(req);
    const json_t *native = ws_state_get(st, "native_http_principal");
    const json_t *hosted = ws_state_get(st, "hosted_http_authenticated");
    /* native truthy OR hosted is True (identity) */
    if (ws_jtruth(native) || ws_is_true(hosted)) return NULL;
    const json_t *app_st = ws_req_app_state(req);
    const json_t *auth_req = ws_state_get(app_st, "auth_required");
    bool ok;
    if (ws_jtruth(auth_req)) {
        const json_t *session = ws_state_get(st, "session");
        ok = !ws_is_null(session);   /* getattr(...,None) is not None */
    } else {
        json_t *a = json_object();
        json_set(a, "request", json_copy(req));
        json_t *r = ws_call(s, "has_valid_session_token", a);
        json_free(a);
        if (ws_is_raise(r)) return r;
        ok = ws_jtruth(r);
        json_free(r);
    }
    if (!ok) return ws_raise_spec("HTTPException", "401: Unauthorized");
    return NULL;
}

/* ── host_header_middleware ── */
/* PoP: ws_host_header_middleware @ hermes_cli/web_server.py:host_header_middleware */
static json_t *ws_host_header_middleware(const ws_seams_t *s, const json_t *req,
                                         const json_t *app_state) {
    const json_t *bound_n = ws_state_get(app_state, "bound_host");
    const char *bound = ws_jstr(bound_n);
    if (ws_jtruth(bound_n)) {
        /* trusted_public_hosts: getattr default frozenset() -> [] ; a present
         * value (list/null) is passed through raw (the oracle stores the list
         * as-is; only the absent default frozenset() projects to sorted []). */
        const json_t *trusted_n = ws_state_get(app_state, "trusted_public_hosts");
        json_t *trusted;
        if (!trusted_n) trusted = json_array();          /* absent -> frozenset() */
        else trusted = json_copy(trusted_n);             /* present list/null raw */
        json_t *a = json_object();
        json_set(a, "host", json_string(ws_header_get(req, "host", "")));
        json_set(a, "bound_host", json_string(bound ? bound : ""));
        json_set(a, "trusted_public_hosts", trusted ? trusted : json_null());
        json_t *accepted = ws_call(s, "is_accepted_host", a);
        json_free(a);
        if (ws_is_raise(accepted)) return accepted;
        bool not_accepted = !ws_jtruth(accepted);
        json_free(accepted);
        if (not_accepted) {
            return ws_json_response(400,
                "Invalid Host header. Dashboard requests must use the "
                "bound hostname or the configured public hostname.");
        }
    }
    /* return await call_next(request) */
    json_t *a = json_object();
    json_set(a, "request", json_copy(req));
    json_t *r = ws_call(s, "call_next", a);
    json_free(a);
    return r;
}

/* ── _plugin_api_runtime_gate ── */
/* PoP: ws_plugin_api_runtime_gate @ hermes_cli/web_server.py:_plugin_api_runtime_gate */
static json_t *ws_plugin_api_runtime_gate(const ws_seams_t *s, const json_t *req) {
    const char *path = ws_req_path(req);
    /* plugin_name = _plugin_api_name(request.scope) */
    json_t *pa = json_object();
    json_set(pa, "scope", json_copy(ws_req_scope(req)));
    json_t *pn = ws_call(s, "plugin_api_name", pa);
    json_free(pa);
    if (ws_is_raise(pn)) return pn;
    bool gate = false;
    if (ws_jtruth(pn)) {
        const json_t *st = ws_req_state(req);
        const json_t *app_st = ws_req_app_state(req);
        if (ws_jtruth(ws_state_get(st, "token_authenticated")) ||
            ws_jtruth(ws_state_get(st, "native_http_principal")) ||
            ws_jtruth(ws_state_get(app_st, "auth_required"))) {
            gate = true;
        } else {
            json_t *a1 = json_object();
            json_set(a1, "request", json_copy(req));
            json_t *r1 = ws_call(s, "has_valid_session_token", a1);
            json_free(a1);
            if (ws_is_raise(r1)) { json_free(pn); return r1; }
            if (ws_jtruth(r1)) { gate = true; json_free(r1); }
            else {
                json_free(r1);
                json_t *a2 = json_object();
                json_set(a2, "request", json_copy(req));
                json_set(a2, "path", json_string(path));
                json_t *r2 = ws_call(s, "has_valid_query_token", a2);
                json_free(a2);
                if (ws_is_raise(r2)) { json_free(pn); return r2; }
                gate = ws_jtruth(r2);
                json_free(r2);
            }
        }
    }
    if (gate) {
        const char *plugin_name = ws_jstr(pn);
        /* with plugin_request_scope(request.scope): */
        json_t *sa = json_object();
        json_set(sa, "scope", json_copy(ws_req_scope(req)));
        json_t *cm = ws_call(s, "plugin_request_scope", sa);
        json_free(sa);
        if (ws_is_raise(cm)) { json_free(pn); return cm; }
        json_free(cm);
        json_t *ent = ws_cm_enter(s, "plugin_scope_enter");
        if (ws_is_raise(ent)) { json_free(pn); return ent; }
        json_free(ent);
        json_t *body_raise = NULL;
        json_t *ret = NULL;
        /* if await _plugin_api_request_refused(plugin_name): return 404 */
        json_t *ra = json_object();
        json_set(ra, "plugin_name", json_string(plugin_name ? plugin_name : ""));
        json_t *refused = ws_call(s, "plugin_api_request_refused", ra);
        json_free(ra);
        if (ws_is_raise(refused)) {
            body_raise = refused;
        } else if (ws_jtruth(refused)) {
            json_free(refused);
            ret = ws_json_response(404, "Plugin not found");
        } else {
            json_free(refused);
            /* hosted = await forward_secondary_plugin_request(request, plugin_name) */
            json_t *fa = json_object();
            json_set(fa, "request", json_copy(req));
            json_set(fa, "plugin_name", json_string(plugin_name ? plugin_name : ""));
            json_t *hosted = ws_call(s, "forward_secondary_plugin_request", fa);
            json_free(fa);
            if (ws_is_raise(hosted)) body_raise = hosted;
            else ret = hosted;   /* kept; checked after the with block */
        }
        /* __exit__ (raise overrides body raise) */
        json_t *exit_raise = ws_cm_exit(s, "plugin_scope_exit");
        if (exit_raise) {
            if (body_raise) json_free(body_raise);
            if (ret) { json_free(ret); ret = NULL; }
            body_raise = exit_raise;
        }
        json_free(pn);
        if (body_raise) { if (ret) json_free(ret); return body_raise; }
        /* if hosted is not None: return hosted */
        if (ret && !ws_is_null(ret)) return ret;
        if (ret) json_free(ret);
        /* fall through to call_next */
    } else {
        json_free(pn);
    }
    json_t *a = json_object();
    json_set(a, "request", json_copy(req));
    json_t *r = ws_call(s, "call_next", a);
    json_free(a);
    return r;
}

/* ── _dashboard_auth_gate ── */
/* PoP: ws_dashboard_auth_gate @ hermes_cli/web_server.py:_dashboard_auth_gate */
static json_t *ws_dashboard_auth_gate(const ws_seams_t *s, const json_t *req) {
    /* from hermes_cli.dashboard_auth.middleware import gated_auth_middleware */
    json_t *imp = ws_call0(s, "import_gated_auth_middleware");
    if (ws_is_raise(imp)) return imp;
    json_free(imp);
    json_t *a = json_object();
    json_set(a, "request", json_copy(req));
    json_set(a, "call_next", ws_callable_marker("call_next"));
    json_t *r = ws_call(s, "gated_auth_middleware", a);
    json_free(a);
    return r;
}

/* ── auth_middleware ── */
/* PoP: ws_auth_middleware @ hermes_cli/web_server.py:auth_middleware */
static json_t *ws_auth_middleware(const ws_seams_t *s, const json_t *req) {
    const char *path = ws_req_path(req);
    const json_t *st = ws_req_state(req);
    const json_t *app_st = ws_req_app_state(req);
    bool deny = false;
    if (!ws_jtruth(ws_state_get(st, "token_authenticated")) &&
        !ws_jtruth(ws_state_get(st, "native_http_principal")) &&
        !ws_jtruth(ws_state_get(app_st, "auth_required")) &&
        path && !strncmp(path, "/api/", 5) &&
        !ws_is_public_api_path(path) &&
        strncmp(path, "/api/mcp/oauth/callback/", 24) != 0) {
        json_t *a1 = json_object();
        json_set(a1, "request", json_copy(req));
        json_t *r1 = ws_call(s, "has_valid_session_token", a1);
        json_free(a1);
        if (ws_is_raise(r1)) return r1;
        bool valid1 = ws_jtruth(r1);
        json_free(r1);
        if (!valid1) {
            json_t *a2 = json_object();
            json_set(a2, "request", json_copy(req));
            json_set(a2, "path", json_string(path));
            json_t *r2 = ws_call(s, "has_valid_query_token", a2);
            json_free(a2);
            if (ws_is_raise(r2)) return r2;
            deny = !ws_jtruth(r2);
            json_free(r2);
        }
    }
    if (deny) return ws_json_response(401, "Unauthorized");
    json_t *a = json_object();
    json_set(a, "request", json_copy(req));
    json_t *r = ws_call(s, "call_next", a);
    json_free(a);
    return r;
}

/* ── _token_auth_seam ── */
/* PoP: ws_token_auth_seam @ hermes_cli/web_server.py:_token_auth_seam */
static json_t *ws_token_auth_seam(const ws_seams_t *s, const json_t *req) {
    /* from hermes_cli.dashboard_auth.peer_pass import is_peer_path, peer_pass_middleware */
    json_t *i1 = ws_call0(s, "import_is_peer_path");
    if (ws_is_raise(i1)) return i1;
    json_free(i1);
    json_t *i2 = ws_call0(s, "import_peer_pass_middleware");
    if (ws_is_raise(i2)) return i2;
    json_free(i2);
    const char *path = ws_req_path(req);
    json_t *pa = json_object();
    json_set(pa, "path", json_string(path));
    json_t *peer = ws_call(s, "is_peer_path", pa);
    json_free(pa);
    if (ws_is_raise(peer)) return peer;
    bool is_peer = ws_jtruth(peer);
    json_free(peer);
    if (is_peer) {
        json_t *a = json_object();
        json_set(a, "request", json_copy(req));
        json_set(a, "call_next", ws_callable_marker("call_next"));
        json_t *r = ws_call(s, "peer_pass_middleware", a);
        json_free(a);
        return r;
    }
    /* from ...native_http import authenticate_native_http, native_profile_scope */
    json_t *i3 = ws_call0(s, "import_authenticate_native_http");
    if (ws_is_raise(i3)) return i3;
    json_free(i3);
    json_t *i4 = ws_call0(s, "import_native_profile_scope");
    if (ws_is_raise(i4)) return i4;
    json_free(i4);
    /* from ...token_auth import token_auth_middleware */
    json_t *i5 = ws_call0(s, "import_token_auth_middleware");
    if (ws_is_raise(i5)) return i5;
    json_free(i5);
    /* rejection = await authenticate_native_http(request) */
    json_t *aa = json_object();
    json_set(aa, "request", json_copy(req));
    json_t *rejection = ws_call(s, "authenticate_native_http", aa);
    json_free(aa);
    if (ws_is_raise(rejection)) return rejection;
    if (!ws_is_null(rejection)) return rejection;   /* is not None */
    json_free(rejection);
    /* with native_profile_scope(request): return await token_auth_middleware(...) */
    json_t *na = json_object();
    json_set(na, "request", json_copy(req));
    json_t *cm = ws_call(s, "native_profile_scope", na);
    json_free(na);
    if (ws_is_raise(cm)) return cm;
    json_free(cm);
    json_t *ent = ws_cm_enter(s, "native_scope_enter");
    if (ws_is_raise(ent)) return ent;
    json_free(ent);
    json_t *ta = json_object();
    json_set(ta, "request", json_copy(req));
    json_set(ta, "call_next", ws_callable_marker("call_next"));
    json_t *body = ws_call(s, "token_auth_middleware", ta);
    json_free(ta);
    json_t *exit_raise = ws_cm_exit(s, "native_scope_exit");
    if (exit_raise) {
        if (body) json_free(body);
        return exit_raise;   /* exit raise overrides body raise/return */
    }
    return body;
}

/* ── _dashboard_health_middleware ── */
/* PoP: ws_dashboard_health_middleware @ hermes_cli/web_server.py:_dashboard_health_middleware */
static json_t *ws_dashboard_health_middleware(const ws_seams_t *s, const json_t *req) {
    const char *path = ws_req_path(req);
    /* try: response = await call_next(request) */
    json_t *a = json_object();
    json_set(a, "request", json_copy(req));
    json_t *response = ws_call(s, "call_next", a);
    json_free(a);
    if (ws_is_raise(response)) {
        /* except Exception as exc: record + re-raise */
        if (ws_caught(response, "Exception")) {
            json_t *imp = ws_call0(s, "import_error_type_name");
            if (ws_is_raise(imp)) { json_free(response); return imp; }
            json_free(imp);
            /* error_type_name(exc) */
            json_t *exc_proj = json_object();
            json_set(exc_proj, "__exc__", json_string(ws_raise_class(response)));
            json_set(exc_proj, "msg", json_string(json_get_str(response, "msg", "")));
            json_t *ea = json_object();
            json_set(ea, "exc", exc_proj);
            json_t *type_name = ws_call(s, "error_type_name", ea);
            json_free(ea);
            if (ws_is_raise(type_name)) { json_free(response); return type_name; }
            /* DASHBOARD_HEALTH.record_error(error_type_name(exc), request.url.path) */
            json_t *ra = json_object();
            json_set(ra, "exc_type", type_name ? json_copy(type_name) : json_null());
            json_set(ra, "path", json_string(path));
            json_t *rec = ws_call(s, "record_error", ra);
            json_free(ra);
            json_free(type_name);
            if (ws_is_raise(rec)) { json_free(response); return rec; }
            json_free(rec);
            return response;   /* bare raise: same exc propagates */
        }
        return response;       /* BaseException passthrough (no seams) */
    }
    /* from hermes_cli.dashboard_auth.peer_pass import is_peer_path */
    json_t *imp = ws_call0(s, "import_is_peer_path");
    if (ws_is_raise(imp)) { json_free(response); return imp; }
    json_free(imp);
    /* if response.status_code >= 500 and not is_peer_path(path): record */
    const json_t *sc_n = ws_is_obj(response) ? json_obj_get(response, "status_code") : NULL;
    if (!sc_n) {
        /* live SimpleNamespace lacks status_code -> AttributeError */
        json_free(response);
        return ws_raise_spec("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'status_code'");
    }
    double sc = (sc_n->type == JSON_NUMBER) ? sc_n->num_val : 0;
    if (sc >= 500) {
        json_t *pa = json_object();
        json_set(pa, "path", json_string(path));
        json_t *peer = ws_call(s, "is_peer_path", pa);
        json_free(pa);
        if (ws_is_raise(peer)) { json_free(response); return peer; }
        bool is_peer = ws_jtruth(peer);
        json_free(peer);
        if (!is_peer) {
            char *etype = ws_xsf("http_%lld", (long long)sc);
            json_t *ra = json_object();
            json_set(ra, "exc_type", json_string(etype));
            json_set(ra, "path", json_string(path));
            json_t *rec = ws_call(s, "record_error", ra);
            json_free(ra);
            free(etype);
            if (ws_is_raise(rec)) { json_free(response); return rec; }
            json_free(rec);
        }
    }
    return response;
}

/* ── DashboardHealth.record_selftest ── */
/* PoP: ws_record_selftest @ hermes_cli/web_server.py:record_selftest */
static json_t *ws_record_selftest(const ws_seams_t *s, json_t *health,
                                  bool passed, const json_t *http_status) {
    json_set(health, "selftest_status", json_string(passed ? "ok" : "failing"));
    json_set(health, "selftest_http_status", http_status ? json_copy(http_status) : json_null());
    json_t *now = ws_call0(s, "time_time");
    if (ws_is_raise(now)) return now;
    json_set(health, "selftest_at", now);
    return NULL;
}

/* ── _is_trusted_local_origin ── */
/* PoP: ws_is_trusted_local_origin @ hermes_cli/web_server.py:_is_trusted_local_origin */
static json_t *ws_is_trusted_local_origin(const ws_seams_t *s, const json_t *app_state,
                                          const char *origin) {
    /* from hermes_cli.web_server_boundary import is_trusted_local_origin */
    json_t *imp = ws_call0(s, "import_is_trusted_local_origin");
    if (ws_is_raise(imp)) return imp;
    json_free(imp);
    json_t *a = json_object();
    json_set(a, "origin", json_string(origin ? origin : ""));
    json_set(a, "app_state", json_copy(app_state));
    json_t *r = ws_call(s, "is_trusted_local_origin", a);
    json_free(a);
    return r;
}

/* ── _LocalOriginCORSMiddleware.is_allowed_origin ── */
/* PoP: ws_is_allowed_origin @ hermes_cli/web_server.py:is_allowed_origin */
static json_t *ws_is_allowed_origin(const ws_seams_t *s, const json_t *app_state,
                                    const char *origin) {
    return ws_is_trusted_local_origin(s, app_state, origin);
}

/* ── _no_auth_provider_message ── */
/* PoP: ws_no_auth_provider_message @ hermes_cli/web_server.py:_no_auth_provider_message */
static json_t *ws_no_auth_provider_message(const ws_seams_t *s, const json_t *app_state,
                                           const char *host) {
    host = host ? host : "";
    /* skip_reasons = []; try: from plugins.dashboard_auth import nous ... except Exception: pass */
    char *skip_line = NULL;
    json_t *imp_nous = ws_call0(s, "import_nous");
    if (ws_is_raise(imp_nous)) {
        if (!ws_caught(imp_nous, "Exception")) return imp_nous;
        json_free(imp_nous);
    } else {
        json_free(imp_nous);
        /* if _nous_plugin.LAST_SKIP_REASON: (read 1) */
        json_t *sr1 = ws_call0(s, "nous_last_skip_reason");
        if (ws_is_raise(sr1)) {
            if (!ws_caught(sr1, "Exception")) return sr1;
            json_free(sr1);
        } else if (ws_jtruth(sr1)) {
            /* append(f"  • nous: {_nous_plugin.LAST_SKIP_REASON}") (read 2) */
            json_t *sr2 = ws_call0(s, "nous_last_skip_reason");
            if (ws_is_raise(sr2)) {
                if (!ws_caught(sr2, "Exception")) { json_free(sr1); return sr2; }
                json_free(sr2);
            } else {
                const char *v = ws_jstr(sr2);
                skip_line = ws_xsf("  \xe2\x80\xa2 nous: %s", v ? v : "");
                json_free(sr2);
            }
            json_free(sr1);
        } else {
            json_free(sr1);
        }
    }
    /* own_url = getattr(app.state, "listener_public_url", None) */
    const json_t *own_n = ws_state_get(app_state, "listener_public_url");
    bool own_none = ws_is_null(own_n);
    const char *own_url = ws_jstr(own_n);
    const char *setting = own_none ? "dashboard.public_url" : "webapp.public_url";
    char *gate_reason = NULL;
    ws_buf fix; ws_binit(&fix);
    if (ws_is_loopback_host(host)) {
        /* public_url = own_url or "" */
        const char *public_url = (own_url && own_url[0]) ? own_url : "";
        char *resolved = NULL;
        if (own_none) {
            /* try: from ...prefix import resolve_public_url; public_url = resolve_public_url()
             * except Exception: pass */
            json_t *imp = ws_call0(s, "import_resolve_public_url");
            if (ws_is_raise(imp)) {
                if (!ws_caught(imp, "Exception")) { ws_bfree(&fix); free(skip_line); return imp; }
                json_free(imp);
            } else {
                json_free(imp);
                json_t *r = ws_call0(s, "resolve_public_url");
                if (ws_is_raise(r)) {
                    if (!ws_caught(r, "Exception")) { ws_bfree(&fix); free(skip_line); return r; }
                    json_free(r);
                } else {
                    const char *rv = ws_jstr(r);
                    resolved = strdup(rv ? rv : "");
                    json_free(r);
                }
            }
        }
        const char *pu = resolved ? resolved : public_url;
        const char *pu_disp = (pu && pu[0]) ? pu : "<a non-loopback URL>";
        gate_reason = ws_xsf(
            "%s is set to %s \xe2\x80\x94 an operator-declared external URL engages "
            "the auth gate even on a loopback bind", setting, pu_disp);
        free(resolved);
        ws_bputs(&fix, "If this dashboard should be LOCAL-ONLY (no reverse proxy), remove ");
        ws_bputs(&fix, setting);
        ws_bputs(&fix, " from config.yaml ");
        if (own_none) ws_bputs(&fix, "(and unset HERMES_DASHBOARD_PUBLIC_URL) ");
        ws_bputs(&fix, "to restore the unauthenticated loopback mode.\n");
    } else {
        gate_reason = ws_xsf("the auth gate engages on non-loopback binds (%s)", host);
    }
    /* fix_hint += the big provider-configuration constant */
    ws_bputs(&fix,
        "Configure an auth provider before exposing the dashboard:\n"
        "  \xe2\x80\xa2 Password: set dashboard.basic_auth.username + password_hash in config.yaml\n"
        "    (hash with: python -c \"from plugins.dashboard_auth.basic import hash_password; "
        "print(hash_password('your-password'))\")\n"
        "  \xe2\x80\xa2 OAuth: run `hermes dashboard register` (Nous Portal) or install a "
        "DashboardAuthProvider plugin.\n"
        "  \xe2\x80\xa2 Deployment token: provision HERMES_DASHBOARD_SESSION_TOKEN (a literal "
        "token, or 'file:<path>' naming a secret-manager-decrypted token file) \xe2\x80\x94 the "
        "bundled session-token provider then authorizes bearer REST + /api/ws ?token= for "
        "token-mode clients such as the Desktop's remote-gateway 'Session token' field.\n"
        "There is no unauthenticated public-dashboard option. For local-only use, bind "
        "127.0.0.1 and leave ");
    ws_bputs(&fix, setting);
    ws_bputs(&fix,
        " unset; a configured external public URL requires auth even when a local reverse "
        "proxy reaches a loopback backend.");
    /* try: load_config + _BASIC_AUTH_PLUGIN_KEYS; half-configured basic auth prefix
     * except Exception: pass */
    json_t *imp_cfg = ws_call0(s, "import_load_config");
    bool cfg_ok = true;
    if (ws_is_raise(imp_cfg)) {
        if (!ws_caught(imp_cfg, "Exception")) { free(gate_reason); ws_bfree(&fix); free(skip_line); return imp_cfg; }
        json_free(imp_cfg); cfg_ok = false;
    } else json_free(imp_cfg);
    json_t *keys = NULL;
    if (cfg_ok) {
        json_t *imp_keys = ws_call0(s, "import_BASIC_AUTH_PLUGIN_KEYS");
        if (ws_is_raise(imp_keys)) {
            if (!ws_caught(imp_keys, "Exception")) { free(gate_reason); ws_bfree(&fix); free(skip_line); return imp_keys; }
            json_free(imp_keys); cfg_ok = false;
        } else keys = imp_keys;   /* data import: result IS the keys set */
    }
    if (cfg_ok) {
        json_t *cfg = ws_call0(s, "load_config");
        if (ws_is_raise(cfg)) {
            if (!ws_caught(cfg, "Exception")) { json_free(keys); free(gate_reason); ws_bfree(&fix); free(skip_line); return cfg; }
            json_free(cfg); cfg_ok = false;
        } else if (!ws_is_obj(cfg)) {
            /* cfg.get on non-dict -> AttributeError, caught */
            json_free(cfg); cfg_ok = false;
        } else {
            /* ba = (cfg.get("dashboard") or {}).get("basic_auth") or {} */
            const json_t *dash = json_obj_get(cfg, "dashboard");
            const json_t *dash_eff = ws_jtruth(dash) ? dash : NULL;
            const json_t *ba = dash_eff ? json_obj_get(dash_eff, "basic_auth") : NULL;
            const json_t *ba_eff = ws_jtruth(ba) ? ba : NULL;
            bool has_creds = false;
            if (ba_eff && ws_is_obj(ba_eff)) {
                const json_t *user = json_obj_get(ba_eff, "username");
                const json_t *ph = json_obj_get(ba_eff, "password_hash");
                const json_t *pw = json_obj_get(ba_eff, "password");
                const json_t *cred = ws_jtruth(ph) ? ph : pw;
                has_creds = ws_jtruth(user) && ws_jtruth(cred);
            }
            /* disabled = (cfg.get("plugins") or {}).get("disabled") or [] */
            const json_t *plugins = json_obj_get(cfg, "plugins");
            const json_t *plugins_eff = ws_jtruth(plugins) ? plugins : NULL;
            const json_t *dis = plugins_eff ? json_obj_get(plugins_eff, "disabled") : NULL;
            const json_t *dis_eff = ws_jtruth(dis) ? dis : NULL;
            bool intersect = false;
            if (has_creds && ws_is_arr(dis_eff) && ws_is_arr(keys)) {
                for (size_t i = 0; i < json_len(dis_eff) && !intersect; i++) {
                    const char *d = ws_jstr(json_get(dis_eff, i));
                    if (d && ws_str_in_array(keys, d)) intersect = true;
                }
            }
            if (intersect) {
                ws_buf pre; ws_binit(&pre);
                ws_bputs(&pre,
                    "The 'basic' dashboard-auth plugin is in plugins.disabled but "
                    "dashboard.basic_auth is configured.\nRemove 'basic' from "
                    "plugins.disabled (or run `hermes plugins enable basic`), then "
                    "restart the dashboard.\n\n");
                ws_bput(&pre, fix.p ? fix.p : "", fix.len);
                ws_bfree(&fix);
                fix = pre;
            }
            json_free(cfg);
        }
    }
    if (keys) json_free(keys);
    /* msg = f"Refusing to bind dashboard to {host} — {gate_reason}, but no auth ...\n\n" */
    ws_buf msg; ws_binit(&msg);
    char *head = ws_xsf(
        "Refusing to bind dashboard to %s \xe2\x80\x94 %s, but no auth providers are "
        "registered.\n\n", host, gate_reason);
    ws_bputs(&msg, head);
    free(head);
    free(gate_reason);
    if (skip_line) {
        ws_bputs(&msg, "Bundled providers reported these issues:\n");
        ws_bputs(&msg, skip_line);
        ws_bputs(&msg, "\n\n");
    }
    free(skip_line);
    ws_bput(&msg, fix.p ? fix.p : "", fix.len);
    ws_bfree(&fix);
    char *out = ws_btake(&msg);
    json_t *ret = json_string(out);
    free(out);
    return ret;
}

/* ── _configure_auth_gate ── */
/* PoP: ws_configure_auth_gate @ hermes_cli/web_server.py:_configure_auth_gate */
static json_t *ws_configure_auth_gate(const ws_seams_t *s, json_t *state, const char *host,
                                      bool allow_public, const json_t *ssh_token,
                                      const json_t *ssh_nonce) {
    host = host ? host : "";
    const json_t *own_n = ws_state_get(state, "listener_public_url");
    bool own_none = ws_is_null(own_n);
    const char *own_url = ws_jstr(own_n);
    /* app.state.trusted_public_hosts = _dashboard_public_hosts() if own None else public_url_hosts(own) */
    json_t *hosts;
    if (own_none) {
        hosts = ws_call0(s, "dashboard_public_hosts");
    } else {
        json_t *a = json_object();
        json_set(a, "url", json_string(own_url ? own_url : ""));
        hosts = ws_call(s, "public_url_hosts", a);
        json_free(a);
    }
    if (ws_is_raise(hosts)) return hosts;
    json_set(state, "trusted_public_hosts", json_copy(hosts));
    /* app.state.trusted_public_origins = ... */
    json_t *origins;
    if (own_none) {
        origins = ws_call0(s, "dashboard_public_origins");
    } else {
        json_t *a = json_object();
        json_set(a, "url", json_string(own_url ? own_url : ""));
        origins = ws_call(s, "public_url_origins", a);
        json_free(a);
    }
    if (ws_is_raise(origins)) { json_free(hosts); return origins; }
    json_set(state, "trusted_public_origins", json_copy(origins));
    json_free(origins);
    /* auth_required */
    json_t *auth_req;
    if (own_none) {
        json_t *a = json_object();
        json_set(a, "host", json_string(host));
        json_set(a, "ssh_session_token", ssh_token ? json_copy(ssh_token) : json_null());
        json_set(a, "ssh_owner_nonce", ssh_nonce ? json_copy(ssh_nonce) : json_null());
        json_t *exempt = ws_call(s, "desktop_loopback_auth_exempt", a);
        json_free(a);
        if (ws_is_raise(exempt)) { json_free(hosts); return exempt; }
        bool is_exempt = ws_jtruth(exempt);
        json_free(exempt);
        if (is_exempt) {
            json_t *ha = json_object();
            json_set(ha, "host", json_string(host));
            auth_req = ws_call(s, "should_require_auth", ha);
            json_free(ha);
        } else {
            auth_req = NULL;   /* fall to else branch below */
        }
    } else {
        auth_req = NULL;
    }
    if (!auth_req) {
        /* should_require_dashboard_auth(host, app.state.trusted_public_hosts) */
        json_t *a = json_object();
        json_set(a, "host", json_string(host));
        json_set(a, "trusted_public_hosts", json_copy(hosts));
        auth_req = ws_call(s, "should_require_dashboard_auth", a);
        json_free(a);
    }
    json_free(hosts);
    if (ws_is_raise(auth_req)) return auth_req;
    json_set(state, "auth_required", json_copy(auth_req));
    bool required = ws_jtruth(auth_req);
    json_free(auth_req);
    /* allow_public warning is a silent log (no seam) */
    (void)allow_public;
    if (required) {
        /* from hermes_cli.dashboard_auth import list_providers */
        json_t *imp = ws_call0(s, "import_list_providers");
        if (ws_is_raise(imp)) return imp;
        json_free(imp);
        json_t *providers = ws_call0(s, "list_providers");
        if (ws_is_raise(providers)) return providers;
        if (!ws_jtruth(providers)) {
            json_free(providers);
            /* raise SystemExit(_no_auth_provider_message(host)) */
            json_t *m = ws_no_auth_provider_message(s, state, host);
            if (ws_is_raise(m)) return m;
            const char *ms = ws_jstr(m);
            json_t *se = ws_raise_spec("SystemExit", ms ? ms : "");
            /* SystemExit(msg).code == msg — the oracle envelope carries code */
            json_set(se, "code", json_string(ms ? ms : ""));
            json_free(m);
            return se;
        }
        json_free(providers);
        /* _log.info(..., ", ".join(p.name for p in list_providers())) — 2nd call,
         * argument evaluates: a row lacking name raises AttributeError */
        json_t *providers2 = ws_call0(s, "list_providers");
        if (ws_is_raise(providers2)) return providers2;
        if (ws_is_arr(providers2)) {
            for (size_t i = 0; i < json_len(providers2); i++) {
                const json_t *row = json_get(providers2, i);
                if (!ws_is_obj(row) || !json_obj_get(row, "name")) {
                    json_free(providers2);
                    return ws_raise_spec("AttributeError",
                        "'types.SimpleNamespace' object has no attribute 'name'");
                }
            }
        }
        json_free(providers2);
    }
    return NULL;
}

/* ── _best_effort ── */
/* PoP: ws_best_effort @ hermes_cli/web_server.py:_best_effort */
static json_t *ws_best_effort(const ws_seams_t *s, const char *what, const json_t *fn_marker) {
    (void)what; (void)fn_marker;
    /* try: fn() */
    json_t *r = ws_call0(s, "best_effort_fn");
    if (ws_is_raise(r)) {
        if (ws_caught(r, "Exception")) { json_free(r); return NULL; }
        return r;   /* BaseException passthrough */
    }
    json_free(r);
    return NULL;
}

/* ── _reclaim_host_from_orphaned_owner ── */
/* PoP: ws_reclaim_host_from_orphaned_owner @ hermes_cli/web_server.py:_reclaim_host_from_orphaned_owner */
static json_t *ws_reclaim_host_from_orphaned_owner(const ws_seams_t *s, const char *role) {
    role = role ? role : "";
    /* from gateway import host_rendezvous as hr */
    json_t *i1 = ws_call0(s, "import_host_rendezvous");
    if (ws_is_raise(i1)) return i1;
    json_free(i1);
    /* from hermes_cli.process_identity import reap_orphaned_backend_owner */
    json_t *i2 = ws_call0(s, "import_reap_orphaned_backend_owner");
    if (ws_is_raise(i2)) return i2;
    json_free(i2);
    /* try: owner = hr.read_record(role) except Exception: return False */
    json_t *ra = json_object();
    json_set(ra, "role", json_string(role));
    json_t *owner = ws_call(s, "read_record", ra);
    json_free(ra);
    if (ws_is_raise(owner)) {
        if (ws_caught(owner, "Exception")) { json_free(owner); return json_bool(false); }
        return owner;
    }
    if (ws_is_null(owner)) {
        json_free(owner);
        /* try: return bool(hr.discard_dead_record(role)) except Exception: return False */
        json_t *da = json_object();
        json_set(da, "role", json_string(role));
        json_t *disc = ws_call(s, "discard_dead_record", da);
        json_free(da);
        if (ws_is_raise(disc)) {
            if (ws_caught(disc, "Exception")) { json_free(disc); return json_bool(false); }
            return disc;
        }
        bool b = ws_jtruth(disc);
        json_free(disc);
        return json_bool(b);
    }
    /* if owner.pid == os.getpid(): return False */
    const json_t *pid_n = ws_is_obj(owner) ? json_obj_get(owner, "pid") : NULL;
    if (!pid_n) {
        json_free(owner);
        return ws_raise_spec("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'pid'");
    }
    json_t *my_pid = ws_call0(s, "os_getpid");
    if (ws_is_raise(my_pid)) { json_free(owner); return my_pid; }
    double mine = (my_pid && my_pid->type == JSON_NUMBER) ? my_pid->num_val : 0;
    json_free(my_pid);
    double opid = (pid_n->type == JSON_NUMBER) ? pid_n->num_val : 0;
    if (pid_n->type == JSON_NUMBER && opid == mine) {
        json_free(owner);
        return json_bool(false);
    }
    /* try: if reap_orphaned_backend_owner(owner.pid, owner.create_time) is None: return False
     * except Exception: return False */
    const json_t *ct_n = json_obj_get(owner, "create_time");
    if (!ct_n) {
        json_free(owner);
        return ws_raise_spec("AttributeError",
            "'types.SimpleNamespace' object has no attribute 'create_time'");
    }
    json_t *aa = json_object();
    json_set(aa, "pid", json_copy(pid_n));
    json_set(aa, "create_time", json_copy(ct_n));
    json_t *reaped = ws_call(s, "reap_orphaned_backend_owner", aa);
    json_free(aa);
    json_free(owner);
    if (ws_is_raise(reaped)) {
        if (ws_caught(reaped, "Exception")) { json_free(reaped); return json_bool(false); }
        return reaped;
    }
    if (ws_is_null(reaped)) { json_free(reaped); return json_bool(false); }
    json_free(reaped);
    /* try: hr.discard_dead_record(role) except Exception: pass */
    json_t *da = json_object();
    json_set(da, "role", json_string(role));
    json_t *disc = ws_call(s, "discard_dead_record", da);
    json_free(da);
    if (ws_is_raise(disc)) {
        if (ws_caught(disc, "Exception")) { json_free(disc); }
        else return disc;
    } else json_free(disc);
    return json_bool(true);
}

/* ── _publish_host_rendezvous ── */
/* PoP: ws_publish_host_rendezvous @ hermes_cli/web_server.py:_publish_host_rendezvous */
static json_t *ws_publish_host_rendezvous(const ws_seams_t *s, json_t *state, const char *host,
                                          double port, const char *session_token) {
    host = host ? host : "";
    /* from gateway import host_rendezvous as hr */
    json_t *imp = ws_call0(s, "import_host_rendezvous");
    if (ws_is_raise(imp)) return imp;
    json_free(imp);
    /* desktop_child = is_desktop_owned_backend() */
    json_t *dc = ws_call0(s, "is_desktop_owned_backend");
    if (ws_is_raise(dc)) return dc;
    bool desktop_child = ws_jtruth(dc);
    json_free(dc);
    const char *role = desktop_child ? "desktop-serve" : "serve";
    /* outcome, error = hr.claim_host_lock(role) — token copied into an owned
     * buffer (the claim node is freed immediately; tokens are <= 14 chars). */
    char outcome[32];
    outcome[0] = '\0';
    json_t *ca = json_object();
    json_set(ca, "role", json_string(role));
    json_t *claim = ws_call(s, "claim_host_lock", ca);
    json_free(ca);
    if (ws_is_raise(claim)) return claim;
    if (ws_is_arr(claim)) {
        const char *t = ws_jstr(json_get(claim, 0));
        if (t) { strncpy(outcome, t, sizeof(outcome) - 1); outcome[sizeof(outcome) - 1] = '\0'; }
    }
    json_free(claim);
    /* if outcome is COULD_NOT_OPEN: warn; return */
    if (!strcmp(outcome, "could-not-open")) return NULL;
    /* if outcome is HELD_BY_OTHER and not desktop_child: reclaim + re-claim */
    if (!strcmp(outcome, "held-by-other") && !desktop_child) {
        json_t *reclaimed = ws_reclaim_host_from_orphaned_owner(s, role);
        if (ws_is_raise(reclaimed)) return reclaimed;
        bool do_reclaim = ws_jtruth(reclaimed);
        json_free(reclaimed);
        if (do_reclaim) {
            json_t *ca2 = json_object();
            json_set(ca2, "role", json_string(role));
            json_t *claim2 = ws_call(s, "claim_host_lock", ca2);
            json_free(ca2);
            if (ws_is_raise(claim2)) return claim2;
            outcome[0] = '\0';
            if (ws_is_arr(claim2)) {
                const char *t = ws_jstr(json_get(claim2, 0));
                if (t) { strncpy(outcome, t, sizeof(outcome) - 1); outcome[sizeof(outcome) - 1] = '\0'; }
            }
            json_free(claim2);
        }
    }
    /* if outcome is HELD_BY_OTHER (after any re-claim): read_record + describe + return */
    if (!strcmp(outcome, "held-by-other")) {
        json_t *ra = json_object();
        json_set(ra, "role", json_string(role));
        json_t *owner = ws_call(s, "read_record", ra);
        json_free(ra);
        if (ws_is_raise(owner)) return owner;
        if (ws_jtruth(owner)) {
            json_t *desc_a = json_object();
            json_set(desc_a, "record", json_copy(owner));
            json_t *desc = ws_call(s, "describe", desc_a);
            json_free(desc_a);
            if (ws_is_raise(desc)) { json_free(owner); return desc; }
            json_free(desc);
        }
        json_free(owner);
        return NULL;   /* desktop_child debug / non-desktop warning both return */
    }
    /* app.state.host_role = role */
    json_set(state, "host_role", json_string(role));
    /* hr.publish_record(role, host=host, port=port, profiles=hr.served_profiles(), token=_SESSION_TOKEN) */
    json_t *profiles = ws_call0(s, "served_profiles");
    if (ws_is_raise(profiles)) return profiles;
    json_t *pa = json_object();
    json_set(pa, "role", json_string(role));
    json_set(pa, "host", json_string(host));
    json_set(pa, "port", json_number(port));
    json_set(pa, "profiles", profiles);
    json_set(pa, "token", json_string(session_token ? session_token : ""));
    json_t *pub = ws_call(s, "publish_record", pa);
    json_free(pa);
    if (ws_is_raise(pub)) return pub;
    json_free(pub);
    /* hr.cleanup_on_exit(role) */
    json_t *cla = json_object();
    json_set(cla, "role", json_string(role));
    json_t *cl = ws_call(s, "cleanup_on_exit", cla);
    json_free(cla);
    if (ws_is_raise(cl)) return cl;
    json_free(cl);
    return NULL;
}

/* ── _windows_serve_loop_factory ── */
/* PoP: ws_windows_serve_loop_factory @ hermes_cli/web_server.py:_windows_serve_loop_factory */
static json_t *ws_windows_serve_loop_factory(const ws_seams_t *s) {
    /* factory = config.get_loop_factory() */
    json_t *factory = ws_call0(s, "get_loop_factory");
    if (ws_is_raise(factory)) return factory;
    /* if factory is None or factory is asyncio.ProactorEventLoop: return SelectorEventLoop */
    const char *tag = ws_is_obj(factory) ? ws_jstr(json_obj_get(factory, "__callable__")) : NULL;
    if (ws_is_null(factory) || (tag && !strcmp(tag, "proactor"))) {
        json_free(factory);
        return ws_callable_marker("selector");
    }
    return factory;
}

/* ── _run_serve ── */
/* SystemExit translation: exc.code == 1 + confirmed port conflict -> sentinel
 * SystemExit(PORT_IN_USE_EXIT_CODE); otherwise the original exc propagates.
 * exc is owned; the returned raise is owned by the caller. */
static json_t *ws_run_serve_system_exit(const ws_seams_t *s, json_t *exc,
                                        const char *host, double port) {
    const json_t *code_n = json_obj_get(exc, "code");
    bool code_is_one = code_n && code_n->type == JSON_NUMBER && code_n->num_val == 1;
    if (code_is_one) {
        json_t *pba = json_object();
        json_set(pba, "host", json_string(host));
        json_set(pba, "port", json_number(port));
        json_t *conflict = ws_call(s, "port_bind_conflict", pba);
        json_free(pba);
        if (ws_is_raise(conflict)) { json_free(exc); return conflict; }
        bool has_conflict = ws_jtruth(conflict);
        json_free(conflict);
        if (has_conflict) {
            json_t *rpa = json_object();
            json_set(rpa, "host", json_string(host));
            json_set(rpa, "port", json_number(port));
            json_t *rep = ws_call(s, "report_port_in_use", rpa);
            json_free(rpa);
            if (ws_is_raise(rep)) { json_free(exc); return rep; }
            json_free(rep);
            json_free(exc);
            char *m = ws_xsf("%d", WS_PORT_IN_USE_EXIT_CODE);
            json_t *se = ws_raise_spec_code("SystemExit", m, WS_PORT_IN_USE_EXIT_CODE);
            free(m);
            return se;
        }
    }
    return exc;   /* bare raise */
}
/* PoP: ws_run_serve @ hermes_cli/web_server.py:_run_serve */
static json_t *ws_run_serve(const ws_seams_t *s, const char *platform, const char *host, double port) {
    host = host ? host : "";
    bool win32 = platform && !strcmp(platform, "win32");
    bool use_uvicorn_runner = false;
    json_t *loop_factory = NULL;   /* runner_kwargs loop_factory marker */
    if (win32) {
        /* try: from uvicorn._compat import asyncio_run as runner;
         *      runner_kwargs = {"loop_factory": _windows_serve_loop_factory(config)} */
        json_t *imp = ws_call0(s, "import_asyncio_run");
        if (ws_is_raise(imp)) {
            if (!ws_caught(imp, "Exception")) return imp;
            json_free(imp);
            /* except Exception: runner = asyncio.run; runner_kwargs = {};
             * try: asyncio.set_event_loop_policy(WindowsSelectorEventLoopPolicy()) except: pass */
            json_t *pol = ws_call0(s, "windows_selector_policy");
            if (ws_is_raise(pol)) {
                if (!ws_caught(pol, "Exception")) return pol;
                json_free(pol);
            } else {
                json_t *spa = json_object();
                json_set(spa, "policy", pol);
                json_t *sr = ws_call(s, "set_event_loop_policy", spa);
                json_free(spa);
                if (ws_is_raise(sr)) {
                    if (!ws_caught(sr, "Exception")) return sr;
                    json_free(sr);
                } else json_free(sr);
            }
        } else {
            json_free(imp);
            use_uvicorn_runner = true;
            loop_factory = ws_windows_serve_loop_factory(s);
            if (ws_is_raise(loop_factory)) return loop_factory;
        }
    }
    /* try: runner(serve(), **runner_kwargs) — serve() evaluated inside the try */
    json_t *coro = ws_call0(s, "serve_call");
    if (ws_is_raise(coro)) {
        if (loop_factory) json_free(loop_factory);
        if (ws_caught(coro, "KeyboardInterrupt")) { json_free(coro); return NULL; }
        if (ws_caught(coro, "SystemExit")) return ws_run_serve_system_exit(s, coro, host, port);
        return coro;
    }
    json_t *ra = json_object();
    json_set(ra, "coro", coro);
    if (loop_factory) json_set(ra, "loop_factory", json_copy(loop_factory));
    json_t *runret = ws_call(s, use_uvicorn_runner ? "uvicorn_asyncio_run" : "asyncio_run", ra);
    json_free(ra);
    if (loop_factory) json_free(loop_factory);
    if (!ws_is_raise(runret)) { json_free(runret); return NULL; }
    /* except KeyboardInterrupt: return */
    if (ws_caught(runret, "KeyboardInterrupt")) { json_free(runret); return NULL; }
    if (ws_caught(runret, "SystemExit")) return ws_run_serve_system_exit(s, runret, host, port);
    return runret;   /* other exceptions propagate */
}

/* ── start_server ── */
/* PoP: ws_start_server @ hermes_cli/web_server.py:start_server */
static json_t *ws_start_server(const ws_seams_t *s, const json_t *args, const json_t *kwargs) {
    /* from pm.extras import suppress_interactive_install_prompts */
    json_t *imp = ws_call0(s, "import_suppress_interactive_install_prompts");
    if (ws_is_raise(imp)) return imp;
    json_free(imp);
    /* with suppress_interactive_install_prompts(): */
    json_t *cm = ws_call0(s, "suppress_prompts_cm");
    if (ws_is_raise(cm)) return cm;
    json_free(cm);
    json_t *ent = ws_cm_enter(s, "suppress_prompts_enter");
    if (ws_is_raise(ent)) return ent;
    json_free(ent);
    /* return _start_server_runtime(*args, **kwargs) */
    json_t *ra = json_object();
    json_set(ra, "args", args ? json_copy(args) : json_array());
    json_set(ra, "kwargs", kwargs ? json_copy(kwargs) : json_object());
    json_t *body = ws_call(s, "start_server_runtime", ra);
    json_free(ra);
    /* __exit__ (raise overrides body raise/return) */
    json_t *exit_raise = ws_cm_exit(s, "suppress_prompts_exit");
    if (exit_raise) {
        if (body) json_free(body);
        return exit_raise;
    }
    return body;
}
