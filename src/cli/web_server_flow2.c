/*
 * web_server_flow2.c — FLOW port of the REMAINING UNCLAIMED surface of
 * hermes_cli/web_server.py (LIVE authority). Self-contained: libjson only. All
 * functions static ws2_*; the harness (#include "cli/web_server_flow2.c") wires
 * the universal seam to scripted results and records the ordered trace.
 *
 * web_server_flow.c ported 23 core functions; every function here was verified
 * UNCLAIMED by grepping PoP annotations tree-wide (src/include/lib) AND the
 * watercup gate names. The remainder is the uvicorn build / post-bind arming /
 * server-runtime orchestrators plus their nested closures and lambdas:
 *
 *   top-level: _build_uvicorn_server, _on_server_started, _start_server_runtime
 *   closures:  _ws_ping_setting (in _build_uvicorn_server),
 *              _reap_desktop_serves, _reap_mcp_helpers, _register_identity,
 *              _install_noise_filter, _loop_heartbeat (in _on_server_started),
 *              _serve (async, in _start_server_runtime),
 *              _owned (in _start_desktop_cron_ticker)
 *   lambdas:   profile_gate (L120), profile_homes (L132),
 *              profile_homes_fallback (L153) in _start_desktop_cron_ticker,
 *              publish_rendezvous_step (L1294) in _on_server_started
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
 * null is a valid None. Claimed siblings (_best_effort, _gateway_owns_cron,
 * _publish_host_rendezvous, _configure_auth_gate, _apply_ssh_session_token,
 * _apply_ssh_owner_nonce, _run_serve, _port_bind_conflict, _report_port_in_use,
 * _read_bound_port, _start_parent_death_watchdog, _write_dashboard_ready_file,
 * _write_machine_sentinel_line, _maybe_open_browser, _dashboard_forwarded_allow_ips,
 * is_desktop_owned_backend) and foreign collaborators are SEAMED. UNCLAIMED
 * ported siblings composed here (_build_uvicorn_server inside
 * _start_server_runtime, _on_server_started inside _serve, _ws_ping_setting
 * inside _build_uvicorn_server) run for REAL on both sides. In-function
 * from-imports fire an "import_<attr>" seam on module attribute access (leading
 * underscore stripped); data imports (DEFAULT_IDLE_GRACE_S, TUNNEL_*) return the
 * scripted value. `import uvicorn` is a silent module load; uvicorn.Config /
 * uvicorn.Server ctor calls are seams. _log is silent (no trace events); log
 * ARGUMENT evaluations that fire seams ARE ported.
 *
 * Seam name table (name -> live call, args):
 *   load_config                  load_config() {} (module-top bare global)
 *   is_desktop_owned_backend     is_desktop_owned_backend() {}
 *   best_effort                  _best_effort(what, fn) {what, fn:callable}
 *   start_parent_death_watchdog  _start_parent_death_watchdog() {}
 *   read_bound_port              _read_bound_port(server, fallback=port) {server,fallback}
 *   write_dashboard_ready_file   _write_dashboard_ready_file(port) {port}
 *   write_machine_sentinel_line  _write_machine_sentinel_line(text) {text}
 *   maybe_open_browser           _maybe_open_browser(host,port,open_browser,profile) {host,port,open_browser,initial_profile}
 *   format_url_host              format_url_host(host) {host}
 *   print                        print(text[, flush]) {text,flush}
 *   gateway_owns_cron            _gateway_owns_cron(name, home) {name,home}
 *   publish_host_rendezvous      _publish_host_rendezvous(host, port) {host,port}
 *   configure_auth_gate          _configure_auth_gate(host,allow_public,tok,nonce) {host,allow_public,ssh_session_token,ssh_owner_nonce}
 *   apply_ssh_session_token      _apply_ssh_session_token(token) {token}
 *   apply_ssh_owner_nonce        _apply_ssh_owner_nonce(nonce) {nonce}
 *   port_bind_conflict           _port_bind_conflict(host, port) {host,port}
 *   report_port_in_use           _report_port_in_use(host, port) {host,port}
 *   run_serve                    _run_serve(serve, config, host, port) {serve:callable,config,host,port}
 *   dashboard_forwarded_allow_ips _dashboard_forwarded_allow_ips(dash_cfg) {dash_cfg}
 *   uvicorn_console_logging      _uvicorn_console_logging() {}
 *   uvicorn_config               uvicorn.Config(app, **kw) {app,host,port,log_level,log_config,proxy_headers,forwarded_allow_ips,ws_ping_interval,ws_ping_timeout,ws_per_message_deflate,ws_max_size,timeout_graceful_shutdown}
 *   uvicorn_server               uvicorn.Server(config) {config}
 *   idle_client_tracker          IdleClientTracker() {}
 *   wrap_asgi_with_ws_tracking   wrap_asgi_with_ws_tracking(app, tracker) {app,tracker}
 *   start_idle_watchdog          start_idle_watchdog(server, tracker, grace_s=) {server,tracker,grace_s}
 *   start_code_skew_watchdog     start_code_skew_watchdog(server) {server}
 *   start_owner_watchdog         start_owner_watchdog(server, lock_path=, nonce=) {server,lock_path,nonce}
 *   register_self                register_self(role, detail=) {role,detail}
 *   attach_self_to_kill_on_close_job attach_self_to_kill_on_close_job() {}
 *   reap_orphaned_desktop_local_serves _reap_orphaned_desktop_local_serves() {}
 *   reap_orphaned_mcp_helpers    reap_orphaned_mcp_helpers() {}
 *   install_loop_noise_filter    install_loop_noise_filter(loop) {loop}
 *   get_running_loop             asyncio.get_running_loop() {} -> loop marker
 *   loop_time                    loop.time() {loop}
 *   loop_call_later              loop.call_later(delay, fn, arg) {loop,delay,fn:callable,arg}
 *   defer_background_mcp_discovery defer_background_mcp_discovery(logger=,thread_name=,delay=) {logger,thread_name,delay}
 *   record_process_ready         record_process_ready(name, background=) {name,background}
 *   profiles_to_serve            profiles_to_serve(multiplex=True) {multiplex}
 *   restore_user_proxy_env       restore_user_proxy_env() {}
 *   apply_nofile_soft_limit      apply_nofile_soft_limit() {}
 *   start_nous_auth_keepalive    start_nous_auth_keepalive() {}
 *   record_launch_scope          record_launch_scope(isolated=, desktop_owned=) {isolated,desktop_owned}
 *   install_exit_flush_signal_handlers install_exit_flush_signal_handlers() {}
 *   activate_multi_profile_hosting_eagerly activate_multi_profile_hosting_eagerly() {}
 *   config_load                  config.load() {}
 *   lifespan_class               config.lifespan_class(config) {config}
 *   capture_signals              server.capture_signals() CM factory {}
 *   capture_signals_enter        CM __enter__ {}
 *   capture_signals_exit         CM __exit__ {} (raise overrides body raise)
 *   server_startup               await server.startup() {}
 *   server_main_loop             await server.main_loop() {}
 *   server_shutdown              await server.shutdown() {}
 *   import_<attr>                in-function from-import attribute access {}
 *
 * Object-double projections (seam args + ret): app -> {"__app__":"app"};
 * config/server -> their fields dict (attribute carriers over a JSON object);
 * loop -> its marker dict; tracker -> the idle_client_tracker seam result;
 * closures/factories -> {"__callable__":"<tag>"}; _log -> {"__logger__":"_log"}.
 *
 * Module constants (live-seeded in the oracle, hardcoded here):
 *   _LOOPBACK_HOST_VALUES = {"localhost","127.0.0.1","::1"} (web_server_boundary)
 *   _DESKTOP_ATTACHMENT_WS_MAX_BYTES = 384*1024*1024 = 402653184 (boundary)
 *   PORT_IN_USE_EXIT_CODE = 75 (web_server_lifecycle)
 *   _DESKTOP_MCP_DISCOVERY_DELAY_S = 1.0 (web_server)
 *   DEFAULT_IDLE_GRACE_S = 900.0, TUNNEL_WS_PING_INTERVAL_S = 60.0,
 *   TUNNEL_WS_PING_TIMEOUT_S = 600.0 (web_server_idle_exit; imported as DATA)
 *
 * Fixture domains (documented exclusions):
 * - ASCII strings; POSIX path strings. Integral ints (no integral-float literals
 *   in type-name-sensitive / f-string positions; canon collapses integral floats
 *   to ints on both sides). float()-string bad tokens are printable ASCII without
 *   quotes/backslashes (ValueError message quotes repr(token)).
 * - load_config scripts are dict-or-null; the dashboard section is dict-or-falsy.
 *   A null load_config in the _on_server_started grace branch raises AttributeError
 *   (NOT caught by except (TypeError,ValueError)) and propagates.
 * - config/server fields are JSON objects; server carries should_exit/started
 *   bools and receives the lifespan write. tracker/loop are opaque markers.
 * - SystemExit raise specs carry an int "code"; other raise specs carry "msg".
 * - is_desktop_owned_backend is called TWICE in _on_server_started (the second
 *   short-circuits when ssh_isolated_clients is present).
 *
 * Ownership: seam results and returned json_t / char pointers are malloc'd,
 * caller frees. Inputs are borrowed. ws2_call borrows args (caller frees).
 * A propagated raise dict is returned upward as-is; a caught raise dict is
 * freed at the catch site. Mutation flows mutate the harness working copy
 * (echoed as out-state).
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
#include <ctype.h>
#include "json.h"

/* ── universal seam + trace ── */

typedef struct ws2_trace { json_t *events; } ws2_trace_t;

typedef struct ws2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} ws2_seams_t;

static void ws2_trace_add(ws2_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void ws2_trace_free(ws2_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *ws2_call(const ws2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *ws2_call0(const ws2_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = ws2_call(s, name, a);
    json_free(a);
    return r;
}

/* ── JSON value helpers ── */

static bool ws2_jtruth(const json_t *v) {
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
static const char *ws2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool ws2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ws2_is_true(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}
static bool ws2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ws2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ws2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

static char *ws2_xsf(const char *fmt, ...) {
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
/* format directly into a NEW json string node (temp buffer freed). */
static json_t *ws2_jstrf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return json_string(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    json_t *r = json_string(buf ? buf : "");
    free(buf);
    return r;
}

typedef struct { char *p; size_t len, cap; } ws2_buf;
static void ws2_binit(ws2_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void ws2_bfree(ws2_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void ws2_bput(ws2_buf *b, const char *s, size_t n) {
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
static void ws2_bputs(ws2_buf *b, const char *s) { ws2_bput(b, s, strlen(s)); }
static char *ws2_btake(ws2_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* ── exception model (FULL builtin parent hierarchy) ── */

static const char *ws2_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool ws2_is_raise(const json_t *r) { return ws2_raise_class(r) != NULL; }

static json_t *ws2_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *ws2_raise_spec_code(const char *cls, const char *msg, double code) {
    json_t *r = ws2_raise_spec(cls, msg);
    json_set(r, "code", json_number(code));
    return r;
}

static bool ws2_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
static const char *ws2_exc_parent(const char *k) {
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
    if (ws2_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (ws2_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (ws2_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (ws2_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (ws2_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (ws2_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError", NULL };
    if (ws2_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (ws2_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (ws2_in_names(k, WARNING_CHILDREN)) return "Warning";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (ws2_in_names(k, NAME_CHILDREN)) return "NameError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    if (!strcmp(k, "PythonFinalizationError")) return "RuntimeError";
    if (!strcmp(k, "_IncompleteInputError")) return "SyntaxError";
    return "Exception";  /* unknown custom kind: oracle bases it on Exception */
}
static bool ws2_caught(const json_t *r, const char *cls) {
    const char *k = ws2_raise_class(r);
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = ws2_exc_parent(k);
    }
    return false;
}
/* BaseException trio passes through except Exception */
static bool ws2_is_base_exc(const json_t *r) {
    const char *k = ws2_raise_class(r);
    return k && (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
                 !strcmp(k, "GeneratorExit") || !strcmp(k, "BaseException"));
}

/* ── type names + repr ── */

static const char *ws2_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (isfinite(d) && d == floor(d)) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
/* Python str repr for the restricted ASCII domain (quotes/backslash/control
 * escaped; delimiter chosen to minimise escapes). */
static char *ws2_py_repr(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    ws2_buf b; ws2_binit(&b);
    char tmp[2] = {q, 0};
    ws2_bputs(&b, tmp);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') {
            char e[3] = {'\\', (char)c, 0};
            ws2_bputs(&b, e);
        } else if (c == '\n') ws2_bputs(&b, "\\n");
        else if (c == '\r') ws2_bputs(&b, "\\r");
        else if (c == '\t') ws2_bputs(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) {
            char *e = ws2_xsf("\\x%02x", c);
            ws2_bputs(&b, e); free(e);
        } else {
            char e[2] = {(char)c, 0};
            ws2_bputs(&b, e);
        }
    }
    tmp[0] = q; ws2_bputs(&b, tmp);
    return ws2_btake(&b);
}

/* ── markers ── */

static json_t *ws2_app_marker(void) {
    json_t *r = json_object();
    json_set(r, "__app__", json_string("app"));
    return r;
}
static json_t *ws2_callable_marker(const char *tag) {
    json_t *r = json_object();
    json_set(r, "__callable__", json_string(tag));
    return r;
}
static json_t *ws2_logger_marker(void) {
    json_t *r = json_object();
    json_set(r, "__logger__", json_string("_log"));
    return r;
}

/* ── float() coercion ── */

static bool ws2_float_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f' || (c >= 0x1c && c <= 0x1f);
}
/* returns 0 ok (*out set), -1 ValueError. Transcribes CPython float(str). */
static int ws2_float_str(const char *s, double *out) {
    const unsigned char *p = (const unsigned char *)s;
    while (ws2_float_ws(*p)) p++;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    /* inf / infinity / nan (case-insensitive); word stops at whitespace */
    char word[16]; size_t wn = 0;
    const unsigned char *q = p;
    while (q[wn] && wn < 15 && !ws2_float_ws(q[wn])) { word[wn] = (char)tolower(q[wn]); wn++; }
    word[wn] = 0;
    const unsigned char *tail = q + wn;
    while (ws2_float_ws(*tail)) tail++;
    if (*tail == '\0') {
        if (!strcmp(word, "inf") || !strcmp(word, "infinity")) {
            *out = neg ? -INFINITY : INFINITY; return 0;
        }
        if (!strcmp(word, "nan")) { *out = NAN; return 0; }
    }
    /* underscore placement: each '_' between two ascii digits */
    for (const unsigned char *u = p; *u; u++) {
        if (*u == '_') {
            if (u == p || !isdigit(u[-1]) || !isdigit(u[1])) return -1;
        }
    }
    /* strip '_' (placement validated), then grammar-validate the clean body */
    ws2_buf clean; ws2_binit(&clean);
    for (const unsigned char *u = p; *u; u++) {
        if (*u == '_') continue;
        char c[2] = {(char)*u, 0};
        ws2_bputs(&clean, c);
    }
    char *body = ws2_btake(&clean);
    const char *d = body;
    int int_digits = 0, frac_digits = 0, exp_digits = 0;
    while (isdigit((unsigned char)*d)) { d++; int_digits++; }
    if (*d == '.') { d++; while (isdigit((unsigned char)*d)) { d++; frac_digits++; } }
    if (int_digits + frac_digits == 0) { free(body); return -1; }
    if (*d == 'e' || *d == 'E') {
        d++;
        if (*d == '+' || *d == '-') d++;
        while (isdigit((unsigned char)*d)) { d++; exp_digits++; }
        if (exp_digits == 0) { free(body); return -1; }
    }
    const char *num_end = d;   /* span ends BEFORE the trailing-ws skip:
                                * strtod stops at the first ws char, so the
                                * string handed to it must not carry the tail */
    while (ws2_float_ws((unsigned char)*d)) d++;
    if (*d != '\0') { free(body); return -1; }
    size_t span = (size_t)(num_end - body);
    char *full = (char *)malloc(span + 2);
    if (!full) { free(body); return -1; }
    if (neg) { full[0] = '-'; memcpy(full + 1, body, span); full[span + 1] = '\0'; }
    else { memcpy(full, body, span); full[span] = '\0'; }
    free(body);
    char *end = NULL;
    double v = strtod(full, &end);
    int ok = (end && *end == '\0' && end != full);
    free(full);
    if (!ok) return -1;
    *out = v;
    return 0;
}
/* float(v): number/bool identity, string parse, else TypeError. Returns a NEW
 * json number node, or a raise spec. */
static json_t *ws2_float(const json_t *v) {
    if (ws2_is_num(v)) return json_number(v->num_val);
    if (v && v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v && v->type == JSON_STRING) {
        double d;
        if (ws2_float_str(v->str_val ? v->str_val : "", &d) == 0)
            return json_number(d);
        char *rp = ws2_py_repr(v->str_val ? v->str_val : "");
        char *m = ws2_xsf("could not convert string to float: %s", rp);
        free(rp);
        json_t *r = ws2_raise_spec("ValueError", m);
        free(m);
        return r;
    }
    char *m = ws2_xsf("float() argument must be a string or a real number, not '%s'",
                      ws2_type_name(v));
    json_t *r = ws2_raise_spec("TypeError", m);
    free(m);
    return r;
}

/* numeric subtraction a - b with operand-type TypeError (left-to-right). */
static json_t *ws2_sub(const json_t *a, const json_t *b) {
    bool an = ws2_is_num(a) || (a && a->type == JSON_BOOL);
    bool bn = ws2_is_num(b) || (b && b->type == JSON_BOOL);
    if (an && bn) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return json_number(av - bv);
    }
    char *m = ws2_xsf("unsupported operand type(s) for -: '%s' and '%s'",
                      ws2_type_name(a), ws2_type_name(b));
    json_t *r = ws2_raise_spec("TypeError", m);
    free(m);
    return r;
}
static json_t *ws2_add(const json_t *a, const json_t *b) {
    bool an = ws2_is_num(a) || (a && a->type == JSON_BOOL);
    bool bn = ws2_is_num(b) || (b && b->type == JSON_BOOL);
    if (an && bn) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return json_number(av + bv);
    }
    char *m = ws2_xsf("unsupported operand type(s) for +: '%s' and '%s'",
                      ws2_type_name(a), ws2_type_name(b));
    json_t *r = ws2_raise_spec("TypeError", m);
    free(m);
    return r;
}

/* load_config().get("dashboard") or {}  — value-or-raise navigation.
 * lc is the load_config seam result (borrowed). Returns a NEW object node
 * (the dashboard dict, or empty {}), or a raise spec (AttributeError on a
 * non-dict carrier; the caller's try/except region decides the catch). */
static json_t *ws2_get_dashboard(const json_t *lc) {
    if (ws2_is_raise(lc)) return json_copy(lc);
    if (!ws2_is_obj(lc)) {
        char *m = ws2_xsf("'%s' object has no attribute 'get'", ws2_type_name(lc));
        json_t *r = ws2_raise_spec("AttributeError", m);
        free(m);
        return r;
    }
    const json_t *dash = json_obj_get(lc, "dashboard");
    if (!ws2_jtruth(dash)) return json_object();
    return json_copy(dash);
}

/* CM helpers */
static json_t *ws2_cm_enter(const ws2_seams_t *s, const char *name) {
    return ws2_call0(s, name);
}
static json_t *ws2_cm_exit(const ws2_seams_t *s, const char *name) {
    json_t *r = ws2_call0(s, name);
    if (ws2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* module constants */
static bool ws2_is_loopback_host(const char *host) {
    return host && (!strcmp(host, "localhost") || !strcmp(host, "127.0.0.1") ||
                    !strcmp(host, "::1"));
}
#define WS2_PORT_IN_USE_EXIT_CODE 75
#define WS2_DESKTOP_ATTACHMENT_WS_MAX_BYTES 402653184.0
#define WS2_DESKTOP_MCP_DISCOVERY_DELAY_S 1.0
#define WS2_DEFAULT_IDLE_GRACE_S 900.0
#define WS2_TUNNEL_WS_PING_INTERVAL_S 60.0
#define WS2_TUNNEL_WS_PING_TIMEOUT_S 600.0
/* ── _ws_ping_setting (closure in _build_uvicorn_server) ── */
/* PoP: ws2_ws_ping_setting @ hermes_cli/web_server.py:_ws_ping_setting */
static json_t *ws2_ws_ping_setting(const ws2_seams_t *s, const json_t *dash_cfg,
                                   const char *key, const json_t *defval) {
    (void)s;
    /* try: return float(_dash_cfg.get(key, default))
     * except (TypeError, ValueError): return default */
    if (!ws2_is_obj(dash_cfg)) {
        char *m = ws2_xsf("'%s' object has no attribute 'get'", ws2_type_name(dash_cfg));
        json_t *r = ws2_raise_spec("AttributeError", m);
        free(m);
        return r;
    }
    const json_t *v = json_obj_get(dash_cfg, key);
    const json_t *arg = v ? v : defval;   /* present-null is a real value */
    json_t *f = ws2_float(arg);
    if (ws2_is_raise(f)) {
        if (ws2_caught(f, "TypeError") || ws2_caught(f, "ValueError")) {
            json_free(f);
            return json_copy(defval);
        }
        return f;   /* BaseException / other passes through */
    }
    return f;
}

/* ── profile_gate lambda (L120): not _gateway_owns_cron(name, home) ── */
/* PoP: ws2_cron_profile_gate @ hermes_cli/web_server.py:profile_gate */
static json_t *ws2_cron_profile_gate(const ws2_seams_t *s, const char *name,
                                     const char *home) {
    json_t *a = json_object();
    json_set(a, "name", json_string(name ? name : ""));
    json_set(a, "home", json_string(home ? home : ""));
    json_t *r = ws2_call(s, "gateway_owns_cron", a);
    json_free(a);
    if (ws2_is_raise(r)) return r;
    bool t = ws2_jtruth(r);
    json_free(r);
    return json_bool(!t);
}

/* ── _owned (closure in _start_desktop_cron_ticker) ── */
/* PoP: ws2_cron_owned @ hermes_cli/web_server.py:_owned */
static json_t *ws2_cron_owned(const ws2_seams_t *s, const char *own_name,
                              const char *own_home) {
    /* try: return _gateway_owns_cron(own_name, own_home)
     * except Exception: _log.warning(...); return False */
    json_t *a = json_object();
    json_set(a, "name", json_string(own_name ? own_name : ""));
    json_set(a, "home", json_string(own_home ? own_home : ""));
    json_t *r = ws2_call(s, "gateway_owns_cron", a);
    json_free(a);
    if (ws2_is_raise(r)) {
        if (ws2_caught(r, "Exception") && !ws2_is_base_exc(r)) {
            json_free(r);
            return json_bool(false);
        }
        return r;
    }
    return r;   /* returns the raw call result */
}

/* ── profile_homes lambda (L132): list(profiles_to_serve(multiplex=True)) ── */
/* PoP: ws2_cron_profile_homes @ hermes_cli/web_server.py:profile_homes */
static json_t *ws2_cron_profile_homes(const ws2_seams_t *s) {
    json_t *a = json_object();
    json_set(a, "multiplex", json_bool(true));
    json_t *r = ws2_call(s, "profiles_to_serve", a);
    json_free(a);
    if (ws2_is_raise(r)) return r;
    if (ws2_is_arr(r)) return r;   /* list(list) -> copy (already a fresh node) */
    char *m = ws2_xsf("'%s' object is not iterable", ws2_type_name(r));
    json_t *e = ws2_raise_spec("TypeError", m);
    free(m);
    json_free(r);
    return e;
}

/* ── profile_homes_fallback lambda (L153): [(own_name, own_home)] ── */
/* PoP: ws2_cron_own_home_fallback @ hermes_cli/web_server.py:profile_homes_fallback */
static json_t *ws2_cron_own_home_fallback(const char *own_name, const char *own_home) {
    json_t *pair = json_array();
    json_append(pair, json_string(own_name ? own_name : ""));
    json_append(pair, json_string(own_home ? own_home : ""));
    json_t *out = json_array();
    json_append(out, pair);
    return out;
}

/* ── _reap_desktop_serves (closure in _on_server_started) ── */
/* PoP: ws2_reap_desktop_serves @ hermes_cli/web_server.py:_reap_desktop_serves */
static json_t *ws2_reap_desktop_serves(const ws2_seams_t *s) {
    json_t *ir = ws2_call0(s, "import_reap_orphaned_desktop_local_serves");
    if (ws2_is_raise(ir)) return ir;
    json_free(ir);
    json_t *r = ws2_call0(s, "reap_orphaned_desktop_local_serves");
    if (ws2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── _reap_mcp_helpers (closure in _on_server_started) ── */
/* PoP: ws2_reap_mcp_helpers @ hermes_cli/web_server.py:_reap_mcp_helpers */
static json_t *ws2_reap_mcp_helpers(const ws2_seams_t *s) {
    json_t *ir = ws2_call0(s, "import_reap_orphaned_mcp_helpers");
    if (ws2_is_raise(ir)) return ir;
    json_free(ir);
    json_t *r = ws2_call0(s, "reap_orphaned_mcp_helpers");
    if (ws2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── _register_identity (closure in _on_server_started) ── */
/* PoP: ws2_register_identity @ hermes_cli/web_server.py:_register_identity */
static json_t *ws2_register_identity(const ws2_seams_t *s, bool headless,
                                     const char *host, const json_t *actual_port,
                                     const char *initial_profile, bool isolated) {
    /* from hermes_cli.process_identity import attach_self_to_kill_on_close_job, register_self */
    json_t *i1 = ws2_call0(s, "import_attach_self_to_kill_on_close_job");
    if (ws2_is_raise(i1)) return i1;
    json_free(i1);
    json_t *i2 = ws2_call0(s, "import_register_self");
    if (ws2_is_raise(i2)) return i2;
    json_free(i2);
    /* register_self(role, detail={host, port, profile, isolated}) */
    json_t *detail = json_object();
    json_set(detail, "host", json_string(host ? host : ""));
    json_set(detail, "port", actual_port ? json_copy(actual_port) : json_null());
    const char *prof = (initial_profile && initial_profile[0]) ? initial_profile : "";
    json_set(detail, "profile", json_string(prof));
    json_set(detail, "isolated", json_bool(isolated));
    json_t *a = json_object();
    json_set(a, "role", json_string(headless ? "serve" : "dashboard"));
    json_set(a, "detail", detail);
    json_t *r = ws2_call(s, "register_self", a);
    json_free(a);
    if (ws2_is_raise(r)) return r;
    json_free(r);
    /* attach_self_to_kill_on_close_job() */
    json_t *r2 = ws2_call0(s, "attach_self_to_kill_on_close_job");
    if (ws2_is_raise(r2)) return r2;
    json_free(r2);
    return NULL;
}

/* ── _install_noise_filter (closure in _on_server_started) ── */
/* PoP: ws2_install_noise_filter @ hermes_cli/web_server.py:_install_noise_filter */
static json_t *ws2_install_noise_filter(const ws2_seams_t *s) {
    json_t *ir = ws2_call0(s, "import_install_loop_noise_filter");
    if (ws2_is_raise(ir)) return ir;
    json_free(ir);
    /* install_loop_noise_filter(asyncio.get_running_loop()) — arg seam first */
    json_t *loop = ws2_call0(s, "get_running_loop");
    if (ws2_is_raise(loop)) return loop;
    json_t *a = json_object();
    json_set(a, "loop", loop ? loop : json_null());
    json_t *r = ws2_call(s, "install_loop_noise_filter", a);
    json_free(a);
    if (ws2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── _loop_heartbeat (closure in _on_server_started) ── */
/* PoP: ws2_loop_heartbeat @ hermes_cli/web_server.py:_loop_heartbeat */
static json_t *ws2_loop_heartbeat(const ws2_seams_t *s, const json_t *loop,
                                  const json_t *expected, const json_t *interval,
                                  const json_t *threshold) {
    /* now = _hb_loop.time() */
    json_t *a = json_object();
    json_set(a, "loop", loop ? json_copy(loop) : json_null());
    json_t *now = ws2_call(s, "loop_time", a);
    json_free(a);
    if (ws2_is_raise(now)) return now;
    /* drift = now - expected */
    json_t *drift = ws2_sub(now, expected);
    if (ws2_is_raise(drift)) { json_free(now); return drift; }
    /* if drift > threshold: _log.warning(...) — silent */
    /* _hb_loop.call_later(interval, _loop_heartbeat, now + interval) */
    json_t *arg = ws2_add(now, interval);
    if (ws2_is_raise(arg)) { json_free(now); json_free(drift); return arg; }
    json_t *fn = ws2_callable_marker("_loop_heartbeat");
    json_t *a2 = json_object();
    json_set(a2, "loop", loop ? json_copy(loop) : json_null());
    json_set(a2, "delay", interval ? json_copy(interval) : json_null());
    json_set(a2, "fn", fn);
    json_set(a2, "arg", arg);
    json_t *r = ws2_call(s, "loop_call_later", a2);
    json_free(a2);
    json_free(now);
    json_free(drift);
    if (ws2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── publish_rendezvous_step lambda (L1294): _publish_host_rendezvous(host, port) ── */
/* PoP: ws2_publish_rendezvous_step @ hermes_cli/web_server.py:publish_rendezvous_step */
static json_t *ws2_publish_rendezvous_step(const ws2_seams_t *s, const char *host,
                                           const json_t *actual_port) {
    json_t *a = json_object();
    json_set(a, "host", json_string(host ? host : ""));
    json_set(a, "port", actual_port ? json_copy(actual_port) : json_null());
    json_t *r = ws2_call(s, "publish_host_rendezvous", a);
    json_free(a);
    if (ws2_is_raise(r)) return r;
    return r;   /* lambda returns the call result */
}
/* ── _build_uvicorn_server ── */
/* PoP: ws2_build_uvicorn_server @ hermes_cli/web_server.py:_build_uvicorn_server */
static json_t *ws2_build_uvicorn_server(const ws2_seams_t *s, json_t *appstate,
                                        const char *host, double port,
                                        bool ssh_isolated) {
    json_t *lc = NULL, *dash = NULL, *ping_i = NULL, *ping_t = NULL;
    json_t *served = NULL, *tracker = NULL, *ti = NULL, *tt = NULL;
    json_t *fai = NULL, *config = NULL, *server = NULL, *ret = NULL;
    json_t *defval = json_number(20.0);
    bool loopback = ws2_is_loopback_host(host);

    /* try: _dash_cfg = load_config().get("dashboard") or {}
     * except Exception: _dash_cfg = {} */
    lc = ws2_call0(s, "load_config");
    dash = ws2_get_dashboard(lc);
    json_free(lc); lc = NULL;
    if (ws2_is_raise(dash)) {
        if (ws2_caught(dash, "Exception") && !ws2_is_base_exc(dash)) {
            json_free(dash); dash = json_object();
        } else { ret = dash; dash = NULL; goto cleanup; }
    }

    served = ws2_app_marker();
    if (!loopback) {
        ping_i = ws2_ws_ping_setting(s, dash, "ws_ping_interval", defval);
        if (ws2_is_raise(ping_i)) { ret = ping_i; ping_i = NULL; goto cleanup; }
        ping_t = ws2_ws_ping_setting(s, dash, "ws_ping_timeout", defval);
        if (ws2_is_raise(ping_t)) { ret = ping_t; ping_t = NULL; goto cleanup; }
    }

    if (ssh_isolated) {
        /* from ... import TUNNEL_WS_PING_INTERVAL_S, TUNNEL_WS_PING_TIMEOUT_S,
         *                   IdleClientTracker, wrap_asgi_with_ws_tracking */
        ti = ws2_call0(s, "import_TUNNEL_WS_PING_INTERVAL_S");
        if (ws2_is_raise(ti)) { ret = ti; ti = NULL; goto cleanup; }
        tt = ws2_call0(s, "import_TUNNEL_WS_PING_TIMEOUT_S");
        if (ws2_is_raise(tt)) { ret = tt; tt = NULL; goto cleanup; }
        json_t *i1 = ws2_call0(s, "import_IdleClientTracker");
        if (ws2_is_raise(i1)) { ret = i1; goto cleanup; }
        json_free(i1);
        json_t *i2 = ws2_call0(s, "import_wrap_asgi_with_ws_tracking");
        if (ws2_is_raise(i2)) { ret = i2; goto cleanup; }
        json_free(i2);
        /* app.state.ssh_isolated_clients = IdleClientTracker() */
        tracker = ws2_call0(s, "idle_client_tracker");
        if (ws2_is_raise(tracker)) { ret = tracker; tracker = NULL; goto cleanup; }
        json_set(appstate, "ssh_isolated_clients", tracker ? json_copy(tracker) : json_null());
        /* served_app = wrap_asgi_with_ws_tracking(app, tracker) */
        json_t *wa = json_object();
        json_set(wa, "app", ws2_app_marker());
        json_set(wa, "tracker", tracker ? json_copy(tracker) : json_null());
        json_t *wrapped = ws2_call(s, "wrap_asgi_with_ws_tracking", wa);
        json_free(wa);
        if (ws2_is_raise(wrapped)) { ret = wrapped; goto cleanup; }
        json_free(served); served = wrapped;
        /* ping_interval, ping_timeout = TUNNEL_* */
        json_free(ping_i); ping_i = ti; ti = NULL;
        json_free(ping_t); ping_t = tt; tt = NULL;
    }

    /* _uvicorn_console_logging() */
    json_t *ucl = ws2_call0(s, "uvicorn_console_logging");
    if (ws2_is_raise(ucl)) { ret = ucl; goto cleanup; }
    json_free(ucl);

    /* forwarded_allow_ips kwarg fires its seam BEFORE uvicorn.Config */
    json_t *fa = json_object();
    json_set(fa, "dash_cfg", json_copy(dash));
    fai = ws2_call(s, "dashboard_forwarded_allow_ips", fa);
    json_free(fa);
    if (ws2_is_raise(fai)) { ret = fai; fai = NULL; goto cleanup; }

    bool auth_req = ws2_jtruth(json_obj_get(appstate, "auth_required"));
    bool deflate = ssh_isolated || !loopback || auth_req;

    /* config = uvicorn.Config(served_app, **kw) */
    json_t *ca = json_object();
    json_set(ca, "app", json_copy(served));
    json_set(ca, "host", json_string(host ? host : ""));
    json_set(ca, "port", json_number(port));
    json_set(ca, "log_level", json_string("warning"));
    json_set(ca, "log_config", json_null());
    json_set(ca, "proxy_headers", json_bool(auth_req));
    json_set(ca, "forwarded_allow_ips", fai ? json_copy(fai) : json_null());
    json_set(ca, "ws_ping_interval", ping_i ? json_copy(ping_i) : json_null());
    json_set(ca, "ws_ping_timeout", ping_t ? json_copy(ping_t) : json_null());
    json_set(ca, "ws_per_message_deflate", json_bool(deflate));
    json_set(ca, "ws_max_size", json_number(WS2_DESKTOP_ATTACHMENT_WS_MAX_BYTES));
    json_set(ca, "timeout_graceful_shutdown", json_number(3));
    config = ws2_call(s, "uvicorn_config", ca);
    json_free(ca);
    if (ws2_is_raise(config)) { ret = config; config = NULL; goto cleanup; }

    /* return config, uvicorn.Server(config) */
    json_t *sa = json_object();
    json_set(sa, "config", json_copy(config));
    server = ws2_call(s, "uvicorn_server", sa);
    json_free(sa);
    if (ws2_is_raise(server)) { ret = server; server = NULL; goto cleanup; }

    ret = json_array();
    json_append(ret, config); config = NULL;
    json_append(ret, server); server = NULL;

cleanup:
    json_free(defval);
    if (lc) json_free(lc);
    if (dash) json_free(dash);
    if (ping_i) json_free(ping_i);
    if (ping_t) json_free(ping_t);
    if (served) json_free(served);
    if (tracker) json_free(tracker);
    if (ti) json_free(ti);
    if (tt) json_free(tt);
    if (fai) json_free(fai);
    if (config) json_free(config);
    if (server) json_free(server);
    return ret;
}
/* fire a seam with a fresh args object (ownership transferred); caller checks
 * the returned raise and frees the result. */
static json_t *ws2_fire(const ws2_seams_t *s, const char *name, json_t *args) {
    json_t *r = ws2_call(s, name, args);
    json_free(args);
    return r;
}
/* str() of a number for f-strings (integral -> %.0f). Domain: integral ports. */
static char *ws2_num_str(const json_t *v) {
    if (ws2_is_num(v)) {
        double d = v->num_val;
        if (isfinite(d) && d == floor(d) && fabs(d) < 1e16) return ws2_xsf("%.0f", d);
        return ws2_xsf("%g", d);
    }
    if (ws2_jstr(v)) return strdup(ws2_jstr(v));
    if (ws2_is_null(v)) return strdup("None");
    if (v && v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    return strdup("");
}
/* grace = float((load_config().get("dashboard") or {}).get(key, DEFAULT))
 * except (TypeError, ValueError): grace = DEFAULT. dig is the imported
 * DEFAULT_IDLE_GRACE_S value (borrowed). Returns a NEW number node or a
 * propagating raise spec. */
static json_t *ws2_grace(const ws2_seams_t *s, const json_t *dig) {
    json_t *lc = ws2_call0(s, "load_config");
    json_t *dash = ws2_get_dashboard(lc);
    json_free(lc);
    if (ws2_is_raise(dash)) {
        if (ws2_caught(dash, "TypeError") || ws2_caught(dash, "ValueError")) {
            json_free(dash);
            return json_copy(dig);
        }
        return dash;   /* AttributeError / BaseException propagates */
    }
    if (!ws2_is_obj(dash)) {
        char *m = ws2_xsf("'%s' object has no attribute 'get'", ws2_type_name(dash));
        json_t *r = ws2_raise_spec("AttributeError", m);
        free(m); json_free(dash);
        return r;
    }
    const json_t *v = json_obj_get(dash, "ssh_isolated_idle_grace_s");
    const json_t *val = v ? v : dig;
    json_t *g = ws2_float(val);
    json_free(dash);
    if (ws2_is_raise(g)) {
        if (ws2_caught(g, "TypeError") || ws2_caught(g, "ValueError")) {
            json_free(g);
            return json_copy(dig);
        }
        return g;
    }
    return g;
}

/* ── _on_server_started ── */
/* PoP: ws2_on_server_started @ hermes_cli/web_server.py:_on_server_started */
static json_t *ws2_on_server_started(const ws2_seams_t *s, json_t *appstate,
                                     const json_t *server, const char *host,
                                     double port, bool headless, bool isolated,
                                     bool open_browser, const char *initial_profile,
                                     bool start_mcp, const json_t *ssh_lock_path,
                                     const json_t *ssh_owner_nonce) {
    json_t *ret = NULL, *actual_port = NULL, *loop = NULL, *now = NULL;
    json_t *dig = NULL, *grace = NULL;
    (void)isolated;   /* used only inside the seamed _register_identity closure */
    const json_t *sic = json_obj_get(appstate, "ssh_isolated_clients");
    bool ssh_present = sic && sic->type != JSON_NULL;
    char *ports = NULL;

    /* if is_desktop_owned_backend(): _best_effort(reap_desktop_serves) */
    json_t *dob = ws2_call0(s, "is_desktop_owned_backend");
    if (ws2_is_raise(dob)) { ret = dob; goto cleanup; }
    bool desktop = ws2_jtruth(dob);
    json_free(dob);
    if (desktop) {
        json_t *a = json_object();
        json_set(a, "what", json_string("orphan desktop-local serve reap"));
        json_set(a, "fn", ws2_callable_marker("_reap_desktop_serves"));
        json_t *r = ws2_fire(s, "best_effort", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* _best_effort("orphan MCP helper reap", _reap_mcp_helpers) */
    {
        json_t *a = json_object();
        json_set(a, "what", json_string("orphan MCP helper reap"));
        json_set(a, "fn", ws2_callable_marker("_reap_mcp_helpers"));
        json_t *r = ws2_fire(s, "best_effort", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* _start_parent_death_watchdog() */
    {
        json_t *r = ws2_call0(s, "start_parent_death_watchdog");
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* SSH idle watchdog branch */
    if (ssh_present) {
        dig = ws2_call0(s, "import_DEFAULT_IDLE_GRACE_S");
        if (ws2_is_raise(dig)) { ret = dig; dig = NULL; goto cleanup; }
        json_t *ii = ws2_call0(s, "import_start_idle_watchdog");
        if (ws2_is_raise(ii)) { ret = ii; goto cleanup; }
        json_free(ii);
        grace = ws2_grace(s, dig);
        if (ws2_is_raise(grace)) { ret = grace; grace = NULL; goto cleanup; }
        json_t *a = json_object();
        json_set(a, "server", server ? json_copy(server) : json_null());
        json_set(a, "tracker", sic ? json_copy(sic) : json_null());
        json_set(a, "grace_s", grace ? json_copy(grace) : json_null());
        json_t *r = ws2_fire(s, "start_idle_watchdog", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* code-skew watchdog: ssh_present OR is_desktop_owned_backend() (2nd call) */
    {
        bool enter = ssh_present;
        if (!enter) {
            json_t *d2 = ws2_call0(s, "is_desktop_owned_backend");
            if (ws2_is_raise(d2)) { ret = d2; goto cleanup; }
            enter = ws2_jtruth(d2);
            json_free(d2);
        }
        if (enter) {
            json_t *ii = ws2_call0(s, "import_start_code_skew_watchdog");
            if (ws2_is_raise(ii)) { ret = ii; goto cleanup; }
            json_free(ii);
            json_t *a = json_object();
            json_set(a, "server", server ? json_copy(server) : json_null());
            json_t *r = ws2_fire(s, "start_code_skew_watchdog", a);
            if (ws2_is_raise(r)) { ret = r; goto cleanup; }
            json_free(r);
        }
    }
    /* owner watchdog: ssh_present AND ssh_lock_path AND _SSH_OWNER_NONCE */
    if (ssh_present && !ws2_is_null(ssh_lock_path) && ws2_jtruth(ssh_owner_nonce)) {
        json_t *ii = ws2_call0(s, "import_start_owner_watchdog");
        if (ws2_is_raise(ii)) { ret = ii; goto cleanup; }
        json_free(ii);
        json_t *a = json_object();
        json_set(a, "server", server ? json_copy(server) : json_null());
        json_set(a, "lock_path", ssh_lock_path ? json_copy(ssh_lock_path) : json_null());
        json_set(a, "nonce", ssh_owner_nonce ? json_copy(ssh_owner_nonce) : json_null());
        json_t *r = ws2_fire(s, "start_owner_watchdog", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* actual_port = _read_bound_port(server, fallback=port) */
    {
        json_t *a = json_object();
        json_set(a, "server", server ? json_copy(server) : json_null());
        json_set(a, "fallback", json_number(port));
        actual_port = ws2_fire(s, "read_bound_port", a);
        if (ws2_is_raise(actual_port)) { ret = actual_port; actual_port = NULL; goto cleanup; }
    }
    json_set(appstate, "bound_port", actual_port ? json_copy(actual_port) : json_null());
    json_set(appstate, "serves_spa", json_bool(!headless));
    ports = ws2_num_str(actual_port);

    /* _best_effort("process-identity registration", _register_identity) */
    {
        json_t *a = json_object();
        json_set(a, "what", json_string("process-identity registration"));
        json_set(a, "fn", ws2_callable_marker("_register_identity"));
        json_t *r = ws2_fire(s, "best_effort", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* _best_effort("host rendezvous publish", lambda) */
    {
        json_t *a = json_object();
        json_set(a, "what", json_string("host rendezvous publish"));
        json_set(a, "fn", ws2_callable_marker("<lambda>"));
        json_t *r = ws2_fire(s, "best_effort", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* _write_dashboard_ready_file(actual_port) */
    {
        json_t *a = json_object();
        json_set(a, "port", actual_port ? json_copy(actual_port) : json_null());
        json_t *r = ws2_fire(s, "write_dashboard_ready_file", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* sentinel lines */
    if (headless) {
        json_t *a = json_object();
        json_set(a, "text", ws2_jstrf("HERMES_BACKEND_READY port=%s", ports));
        json_t *r = ws2_fire(s, "write_machine_sentinel_line", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    {
        json_t *a = json_object();
        json_set(a, "text", ws2_jstrf("HERMES_DASHBOARD_READY port=%s", ports));
        json_t *r = ws2_fire(s, "write_machine_sentinel_line", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* print */
    if (headless) {
        char *txt = ws2_xsf("  Hermes backend listening on %s:%s", host ? host : "", ports);
        json_t *a = json_object();
        json_set(a, "text", json_string(txt));
        json_set(a, "flush", json_bool(true));
        free(txt);
        json_t *r = ws2_fire(s, "print", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    } else {
        json_t *ii = ws2_call0(s, "import_format_url_host");
        if (ws2_is_raise(ii)) { ret = ii; goto cleanup; }
        json_free(ii);
        json_t *fa = json_object();
        json_set(fa, "host", json_string(host ? host : ""));
        json_t *fuh = ws2_fire(s, "format_url_host", fa);
        if (ws2_is_raise(fuh)) { ret = fuh; goto cleanup; }
        char *fuhs = ws2_jstr(fuh) ? strdup(ws2_jstr(fuh)) : ws2_num_str(fuh);
        json_free(fuh);
        char *txt = ws2_xsf("  Hermes Web UI \xe2\x86\x92 http://%s:%s", fuhs, ports);
        free(fuhs);
        json_t *a = json_object();
        json_set(a, "text", json_string(txt));
        free(txt);
        json_t *r = ws2_fire(s, "print", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* _maybe_open_browser(host, actual_port, open_browser, initial_profile) */
    {
        json_t *a = json_object();
        json_set(a, "host", json_string(host ? host : ""));
        json_set(a, "port", actual_port ? json_copy(actual_port) : json_null());
        json_set(a, "open_browser", json_bool(open_browser));
        json_set(a, "initial_profile", json_string(initial_profile ? initial_profile : ""));
        json_t *r = ws2_fire(s, "maybe_open_browser", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* deferred MCP discovery */
    if (start_mcp) {
        json_t *im = ws2_call0(s, "import_defer_background_mcp_discovery");
        if (ws2_is_raise(im)) {
            if (ws2_caught(im, "Exception") && !ws2_is_base_exc(im)) { json_free(im); }
            else { ret = im; goto cleanup; }
        } else {
            json_free(im);
            json_t *a = json_object();
            json_set(a, "logger", ws2_logger_marker());
            json_set(a, "thread_name", json_string("dashboard-mcp-discovery"));
            json_set(a, "delay", json_number(WS2_DESKTOP_MCP_DISCOVERY_DELAY_S));
            json_t *dr = ws2_fire(s, "defer_background_mcp_discovery", a);
            if (ws2_is_raise(dr)) {
                if (ws2_caught(dr, "Exception") && !ws2_is_base_exc(dr)) { json_free(dr); }
                else { ret = dr; goto cleanup; }
            } else json_free(dr);
        }
    }
    /* _best_effort("loop noise filter install", _install_noise_filter) */
    {
        json_t *a = json_object();
        json_set(a, "what", json_string("loop noise filter install"));
        json_set(a, "fn", ws2_callable_marker("_install_noise_filter"));
        json_t *r = ws2_fire(s, "best_effort", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* heartbeat: _hb_loop = get_running_loop(); call_later(2.0, fn, time()+2.0) */
    loop = ws2_call0(s, "get_running_loop");
    if (ws2_is_raise(loop)) { ret = loop; loop = NULL; goto cleanup; }
    {
        json_t *a = json_object();
        json_set(a, "loop", loop ? json_copy(loop) : json_null());
        now = ws2_fire(s, "loop_time", a);
        if (ws2_is_raise(now)) { ret = now; now = NULL; goto cleanup; }
    }
    {
        json_t *interval = json_number(2.0);
        json_t *arg = ws2_add(now, interval);
        if (ws2_is_raise(arg)) { json_free(interval); ret = arg; goto cleanup; }
        json_t *a = json_object();
        json_set(a, "loop", loop ? json_copy(loop) : json_null());
        json_set(a, "delay", interval);
        json_set(a, "fn", ws2_callable_marker("_loop_heartbeat"));
        json_set(a, "arg", arg);
        json_t *r = ws2_fire(s, "loop_call_later", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    ret = json_null();   /* returns None */

cleanup:
    if (actual_port) json_free(actual_port);
    if (loop) json_free(loop);
    if (now) json_free(now);
    if (dig) json_free(dig);
    if (grace) json_free(grace);
    free(ports);
    return ret;
}
/* ── _serve (async closure in _start_server_runtime) ── */
/* PoP: ws2_serve @ hermes_cli/web_server.py:_serve */
static json_t *ws2_serve(const ws2_seams_t *s, const json_t *config, json_t *server,
                         json_t *appstate, const char *host, double port,
                         bool headless, bool isolated, bool open_browser,
                         const char *initial_profile, bool start_mcp,
                         const json_t *ssh_lock_path, const json_t *ssh_owner_nonce) {
    json_t *ret = NULL, *body_raise = NULL, *lc = NULL;
    /* if not config.loaded: config.load() */
    if (!ws2_jtruth(json_obj_get(config, "loaded"))) {
        json_t *r = ws2_call0(s, "config_load");
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* server.lifespan = config.lifespan_class(config) */
    {
        json_t *a = json_object();
        json_set(a, "config", config ? json_copy(config) : json_null());
        lc = ws2_fire(s, "lifespan_class", a);
        if (ws2_is_raise(lc)) { ret = lc; lc = NULL; goto cleanup; }
        json_set(server, "lifespan", lc); lc = NULL;
    }
    /* with server.capture_signals(): */
    {
        json_t *cm = ws2_call0(s, "capture_signals");
        if (ws2_is_raise(cm)) { ret = cm; goto cleanup; }
        json_free(cm);
    }
    {
        json_t *en = ws2_call0(s, "capture_signals_enter");
        if (ws2_is_raise(en)) { ret = en; goto cleanup; }   /* __enter__ raise: no __exit__ */
        json_free(en);
    }
    /* body */
    {
        json_t *r = ws2_call0(s, "server_startup");
        if (ws2_is_raise(r)) body_raise = r; else json_free(r);
    }
    if (!body_raise && !ws2_jtruth(json_obj_get(server, "should_exit"))) {
        /* _on_server_started(...) — REAL composition */
        json_t *osr = ws2_on_server_started(s, appstate, server, host, port, headless,
                                            isolated, open_browser, initial_profile,
                                            start_mcp, ssh_lock_path, ssh_owner_nonce);
        if (ws2_is_raise(osr)) body_raise = osr; else json_free(osr);
        if (!body_raise && headless) {
            json_t *im = ws2_call0(s, "import_record_process_ready");
            if (ws2_is_raise(im)) body_raise = im;
            else {
                json_free(im);
                json_t *a = json_object();
                json_set(a, "name", json_string("serve_boot"));
                json_set(a, "background", json_bool(true));
                json_t *dr = ws2_fire(s, "record_process_ready", a);
                if (ws2_is_raise(dr)) body_raise = dr; else json_free(dr);
            }
        }
        if (!body_raise) {
            json_t *r = ws2_call0(s, "server_main_loop");
            if (ws2_is_raise(r)) body_raise = r; else json_free(r);
        }
        if (!body_raise && ws2_jtruth(json_obj_get(server, "started"))) {
            json_t *r = ws2_call0(s, "server_shutdown");
            if (ws2_is_raise(r)) body_raise = r; else json_free(r);
        }
    }
    /* __exit__ fires on every path; an exit raise overrides the body raise */
    {
        json_t *ex = ws2_cm_exit(s, "capture_signals_exit");
        if (ex) {
            if (body_raise) json_free(body_raise);
            ret = ex;
        } else if (body_raise) {
            ret = body_raise; body_raise = NULL;
        } else {
            ret = json_null();
        }
    }
cleanup:
    if (lc) json_free(lc);
    if (body_raise) json_free(body_raise);
    return ret;
}

/* ── _start_server_runtime ── */
/* PoP: ws2_start_server_runtime @ hermes_cli/web_server.py:_start_server_runtime */
static json_t *ws2_start_server_runtime(const ws2_seams_t *s, json_t *appstate,
                                        const char *host, double port,
                                        bool open_browser, bool allow_public,
                                        const char *initial_profile, bool headless,
                                        bool isolated, const json_t *ssh_session_token,
                                        const json_t *ssh_owner_nonce, bool start_mcp,
                                        const json_t *ssh_lock_path) {
    json_t *ret = NULL, *built = NULL, *config = NULL, *server = NULL;
    (void)open_browser; (void)headless; (void)start_mcp; (void)ssh_lock_path;
    /* those ride into the seamed _run_serve / composed _serve via the harness */

    /* from hermes_cli.update_proxy_env import restore_user_proxy_env */
    json_t *i0 = ws2_call0(s, "import_restore_user_proxy_env");
    if (ws2_is_raise(i0)) { ret = i0; goto cleanup; }
    json_free(i0);
    {
        json_t *r = ws2_call0(s, "restore_user_proxy_env");
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* _apply_ssh_session_token(ssh_session_token or "") */
    {
        const char *tok = ws2_jstr(ssh_session_token);
        json_t *a = json_object();
        json_set(a, "token", json_string(ws2_jtruth(ssh_session_token) && tok ? tok : ""));
        json_t *r = ws2_fire(s, "apply_ssh_session_token", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* _apply_ssh_owner_nonce(ssh_owner_nonce) */
    {
        json_t *a = json_object();
        json_set(a, "nonce", ssh_owner_nonce ? json_copy(ssh_owner_nonce) : json_null());
        json_t *r = ws2_fire(s, "apply_ssh_owner_nonce", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* from hermes_cli.resource_limits import apply_nofile_soft_limit */
    json_t *i1 = ws2_call0(s, "import_apply_nofile_soft_limit");
    if (ws2_is_raise(i1)) { ret = i1; goto cleanup; }
    json_free(i1);
    {
        json_t *r = ws2_call0(s, "apply_nofile_soft_limit");
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* import uvicorn — silent module load */
    /* try: start_nous_auth_keepalive() except Exception: _log.debug */
    {
        json_t *im = ws2_call0(s, "import_start_nous_auth_keepalive");
        if (ws2_is_raise(im)) {
            if (ws2_caught(im, "Exception") && !ws2_is_base_exc(im)) json_free(im);
            else { ret = im; goto cleanup; }
        } else {
            json_free(im);
            json_t *r = ws2_call0(s, "start_nous_auth_keepalive");
            if (ws2_is_raise(r)) {
                if (ws2_caught(r, "Exception") && !ws2_is_base_exc(r)) json_free(r);
                else { ret = r; goto cleanup; }
            } else json_free(r);
        }
    }
    /* _configure_auth_gate(host, allow_public, ssh_session_token, ssh_owner_nonce) */
    {
        json_t *a = json_object();
        json_set(a, "host", json_string(host ? host : ""));
        json_set(a, "allow_public", json_bool(allow_public));
        json_set(a, "ssh_session_token", ssh_session_token ? json_copy(ssh_session_token) : json_null());
        json_set(a, "ssh_owner_nonce", ssh_owner_nonce ? json_copy(ssh_owner_nonce) : json_null());
        json_t *r = ws2_fire(s, "configure_auth_gate", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* app.state.bound_host = host; app.state.initial_profile = str(initial_profile or "") */
    json_set(appstate, "bound_host", json_string(host ? host : ""));
    json_set(appstate, "initial_profile",
             json_string(initial_profile && initial_profile[0] ? initial_profile : ""));
    /* record_launch_scope(isolated=, desktop_owned=is_desktop_owned_backend()) */
    {
        json_t *im = ws2_call0(s, "import_record_launch_scope");
        if (ws2_is_raise(im)) { ret = im; goto cleanup; }
        json_free(im);
        json_t *dob = ws2_call0(s, "is_desktop_owned_backend");   /* arg seam first */
        if (ws2_is_raise(dob)) { ret = dob; goto cleanup; }
        json_t *a = json_object();
        json_set(a, "isolated", json_bool(isolated));
        json_set(a, "desktop_owned", dob ? dob : json_null());
        json_t *r = ws2_fire(s, "record_launch_scope", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    /* config, server = _build_uvicorn_server(host, port, ssh_isolated=bool(tok)) — REAL */
    built = ws2_build_uvicorn_server(s, appstate, host, port, ws2_jtruth(ssh_session_token));
    if (ws2_is_raise(built)) { ret = built; built = NULL; goto cleanup; }
    config = json_copy(json_get(built, 0));
    server = json_copy(json_get(built, 1));
    json_free(built); built = NULL;
    /* try: install_exit_flush_signal_handlers() except Exception: _log.debug */
    {
        json_t *im = ws2_call0(s, "import_install_exit_flush_signal_handlers");
        if (ws2_is_raise(im)) {
            if (ws2_caught(im, "Exception") && !ws2_is_base_exc(im)) json_free(im);
            else { ret = im; goto cleanup; }
        } else {
            json_free(im);
            json_t *r = ws2_call0(s, "install_exit_flush_signal_handlers");
            if (ws2_is_raise(r)) {
                if (ws2_caught(r, "Exception") && !ws2_is_base_exc(r)) json_free(r);
                else { ret = r; goto cleanup; }
            } else json_free(r);
        }
    }
    /* if _port_bind_conflict(host, port): report + SystemExit(75) */
    {
        json_t *a = json_object();
        json_set(a, "host", json_string(host ? host : ""));
        json_set(a, "port", json_number(port));
        json_t *pbc = ws2_fire(s, "port_bind_conflict", a);
        if (ws2_is_raise(pbc)) { ret = pbc; goto cleanup; }
        bool conflict = ws2_jtruth(pbc);
        json_free(pbc);
        if (conflict) {
            json_t *a2 = json_object();
            json_set(a2, "host", json_string(host ? host : ""));
            json_set(a2, "port", json_number(port));
            json_t *r = ws2_fire(s, "report_port_in_use", a2);
            if (ws2_is_raise(r)) { ret = r; goto cleanup; }
            json_free(r);
            ret = ws2_raise_spec_code("SystemExit", "75", WS2_PORT_IN_USE_EXIT_CODE);
            goto cleanup;
        }
    }
    /* try: activate_multi_profile_hosting_eagerly() except Exception: _log.warning */
    {
        json_t *im = ws2_call0(s, "import_activate_multi_profile_hosting_eagerly");
        if (ws2_is_raise(im)) {
            if (ws2_caught(im, "Exception") && !ws2_is_base_exc(im)) json_free(im);
            else { ret = im; goto cleanup; }
        } else {
            json_free(im);
            json_t *r = ws2_call0(s, "activate_multi_profile_hosting_eagerly");
            if (ws2_is_raise(r)) {
                if (ws2_caught(r, "Exception") && !ws2_is_base_exc(r)) json_free(r);
                else { ret = r; goto cleanup; }
            } else json_free(r);
        }
    }
    /* _run_serve(_serve, config, host, port) — claimed sibling SEAMED */
    {
        json_t *a = json_object();
        json_set(a, "serve", ws2_callable_marker("_serve"));
        json_set(a, "config", config ? json_copy(config) : json_null());
        json_set(a, "host", json_string(host ? host : ""));
        json_set(a, "port", json_number(port));
        json_t *r = ws2_fire(s, "run_serve", a);
        if (ws2_is_raise(r)) { ret = r; goto cleanup; }
        json_free(r);
    }
    ret = json_null();

cleanup:
    if (built) json_free(built);
    if (config) json_free(config);
    if (server) json_free(server);
    return ret;
}