/*
 * turn_exit_flow.c — FLOW port of the UNCLAIMED turn-exit surface that
 * consumes hermes_cli/turn_exit.py. Self-contained: libjson only. All
 * functions static te2_*; the harness (#include "cli/turn_exit_flow.c")
 * wires the universal seam to scripted results and records the ordered
 * trace.
 *
 * SCOPE NOTE: hermes_cli/turn_exit.py itself has exactly TWO defs
 * (turn_exit_code, credential_failure_flags) and BOTH are fully claimed by
 * src/gateway/session_kanban_flow2.c (sk2_turn_exit_code /
 * sk2_credential_failure_flags). The unclaimed REMAINDER of the turn-exit
 * area is the exit-policy chain in its consumer modules; PoP annotations
 * credit the DEFINING module (audit_annotations doctrine), never the
 * importing one. Claimed siblings (turn_exit_code, kanban_task_id,
 * _run_kanban_goal_loop_q) are SEAMED here.
 *
 * Covered flows (PoP per function):
 *   hermes_cli/cli_single_query.py: _int_or, _single_query_exit_code,
 *     _sync_cli_session_id_from_agent, _interrupt_agent_for_signal,
 *     _configure_quiet_agent, _run_kanban_goal_loop_chat,
 *     _collect_kanban_task_images, _route_single_query_images
 *   hermes_cli/gateway_chat_view.py: _pending_preview, _quoted,
 *     pending_lines, lost_admission, _oneshot_admission, resume_footer,
 *     GatewayChatView._finite_block (unknown_admissions is an unclaimed
 *     real helper in the oracle; its comprehension is inlined here)
 *   hermes_cli/quiet_single_query.py: exit_single_query,
 *     take_turn_report_path
 *   hermes_cli/oneshot.py: _oneshot_exit_code
 *
 * SEAM DOCTRINE: every live effect enters through ONE universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returning a NEW json_t* (NULL == Python None). A raising live call is
 * scripted {"__raise__": "<ExcClass>", "msg": ...}; the cores propagate it
 * exactly where the live try/except catches it. except Exception and
 * contextlib.suppress(Exception) do NOT catch KeyboardInterrupt /
 * SystemExit / GeneratorExit / BaseException — those pass through to the
 * harness ret envelope (BaseException passthrough). A MISSING seam key
 * fail-closes: the harness delivers {"__raise__":"RuntimeError","msg":
 * "unscripted seam: <name>"}; a present JSON null is a valid None.
 *
 * Seam name table (name -> live call, args):
 *   import__TERMINAL_PROVIDER_REASONS  from cli import _TERMINAL_PROVIDER_REASONS
 *                                      (VALUE attr: ret = the reasons set)
 *   import__TRANSIENT_PROVIDER_REASONS from cli import _TRANSIENT_PROVIDER_REASONS
 *   import__float_env                  from cli import _float_env
 *   float_env                          cli._float_env(name, default) -> number
 *   import__run_kanban_goal_loop_q     from cli import _run_kanban_goal_loop_q
 *   run_kanban_goal_loop_q             cli._run_kanban_goal_loop_q(cli,
 *                                      first_response, run_turn=, log=)
 *   import_kanban_task_id              from agent.kanban_turn_recovery import
 *                                      kanban_task_id
 *   kanban_task_id                     kanban_task_id() -> str|null
 *   import_turn_exit_code              from hermes_cli.turn_exit import turn_exit_code
 *   turn_exit_code                     turn_exit_code(result, kanban_worker=,
 *                                      credentials_rate_limited=,
 *                                      credentials_terminal=,
 *                                      transient_reasons=, terminal_reasons=)
 *   request_hard_interrupt             request_hard_interrupt(agent, message)
 *                                      (module global; no import seam)
 *   time_sleep                         time.sleep(seconds)
 *   import_extract_image_refs          from agent.image_routing import
 *                                      extract_image_refs
 *   extract_image_refs                 extract_image_refs(body) -> (paths, urls)
 *   import_connect_closing             hermes_cli.kanban_db_connect.connect_closing
 *                                      attribute access
 *   connect_closing                    connect_closing() CM factory
 *   connect_closing_enter              CM __enter__ -> conn
 *   connect_closing_exit               CM __exit__ (truthy ret suppresses;
 *                                      a raise masks the body raise)
 *   import_get_task                    hermes_cli.kanban_db.get_task attr access
 *   get_task                           get_task(conn, task_id) -> task|null
 *   import_build_native_content_parts  agent.image_routing.build_native_content_parts
 *   import_decide_image_input_mode     agent.image_routing.decide_image_input_mode
 *   import_load_config                 hermes_cli.config.load_config
 *   load_config                        load_config() -> config
 *   decide_image_input_mode            decide_image_input_mode(provider, model,
 *                                      config, requested_provider=) -> str
 *   build_native_content_parts         build_native_content_parts(query, paths,
 *                                      image_urls=) -> (parts, skipped)
 *   preprocess_images_with_vision      cli._preprocess_images_with_vision(query,
 *                                      images, announce=False)
 *   import_strip_ansi                  tools.ansi_strip.strip_ansi attr access
 *   strip_ansi                         strip_ansi(text) -> str
 *   import_t                           agent.i18n.t attr access
 *   t                                  t(key) -> str
 *   import_get_active_profile_name     hermes_cli.profiles.get_active_profile_name
 *   get_active_profile_name            get_active_profile_name() -> str
 *   print                              print(text, file=, flush=) -> None;
 *                                      args {text, file:"stdout"|"stderr", flush}
 *
 * `from hermes_cli import kanban_db / kanban_db_connect` fires NO seam: the
 * oracle pre-sets those attributes on the hermes_cli package stub (a
 * package __getattr__ would double-fire under _handle_fromlist's hasattr
 * probe); only the LEAF attribute accesses (get_task / connect_closing)
 * fire import seams. The C side mirrors that: no event for the module
 * import, one import event per leaf attribute access.
 *
 * sys.exit(code) is NOT a seam: it is a direct SystemExit raise descriptor
 * (msg = str(code)), matching config_flow2 / doctor_flow. The live oracle
 * runs the REAL sys.exit and catches BaseException into the same envelope;
 * neither side records a trace event for it.
 *
 * os.environ is a STATE dict (in.environ); reads are direct (no seam);
 * take_turn_report_path's environ.pop mutates the working copy, echoed in
 * out.state. exit_single_query / _collect_kanban_task_images echo environ
 * unchanged.
 *
 * Callable markers: _run_kanban_goal_loop_chat passes run_turn / log
 * closures to the CLAIMED sibling _run_kanban_goal_loop_q, which is an
 * inert recorder here (never invokes them). They project to
 * {"__callable__":"run_turn"} / {"__callable__":"log"} on both sides; the
 * closure bodies (cli.chat / logger.info+print) are unobservable under the
 * inert seam.
 *
 * logger is SILENT (no trace events), matching auth_flow and the sibling
 * cli flow ports. The live _collect_kanban_task_images except-branch
 * logger.debug and _run_kanban_goal_loop_chat's _log logger.info are
 * recorded by a silent RecLogger on the oracle side.
 *
 * Fixture domains (documented exclusions):
 * - ASCII strings; POSIX path strings. Path(_p) appends normalize exactly
 *   like PurePosixPath (drop '.'/empty components, keep '..', exactly two
 *   leading slashes preserved; ''/'.' -> '.').
 * - JSON numbers have no int/float identity in libjson: integral floats
 *   normalize to ints on both sides; exit codes / signum are integral ints.
 * - int() string tokens (_int_or): ASCII, optional sign, digits with single
 *   underscores BETWEEN digits; the int-strip whitespace set is ASCII
 *   White_Space MINUS \x1c-\x1f ({\t\n\v\f\r space}); str.strip()/split()
 *   use the wider set (incl \x1c-\x1f). int() failures (TypeError/
 *   ValueError) are swallowed by _int_or -> default; NaN/inf floats are
 *   outside the JSON fixture domain.
 * - isprintable() (_pending_preview): ASCII 0x20-0x7E printable; 0x00-0x1F
 *   and 0x7F fold to space. Non-ASCII code points are outside the domain.
 * - reasons sets (_TERMINAL/_TRANSIENT_PROVIDER_REASONS) project as ORDERED
 *   arrays (fixture order); the cli facade stub returns tuples, never
 *   frozensets (hash-randomized iteration). Membership semantics live in
 *   the seamed turn_exit_code, not here.
 * - cli doubles carry every attribute the flows read: _route_single_query_
 *   images reads cli.provider / cli.model / cli.requested_provider directly
 *   (present-null allowed; absent would raise AttributeError inside the
 *   live try, outside the domain); _sync reads cli.agent / cli.session_id
 *   (agent present-null allowed). get_task scripts are null or a dict
 *   ({} allowed — getattr(task,"body","") has a default; the task gate is
 *   `is not None` identity). extract_image_refs / build_native_content_
 *   parts scripts are 2-element arrays (the live tuple unpack); parts rows
 *   are dicts. pending rows carry status (+ admission_id where read);
 *   snapshot / receipt / result are dicts or null per the live .get sites.
 * - exit_single_query code is an integral int (int(code) identity; the
 *   trailer str() and SystemExit str() agree).
 * - Source literals carry their exact UTF-8 bytes (U+23F8 pause symbol in
 *   pending_lines, U+2026 ellipsis in the _pending_preview cap); fixture
 *   DATA is ASCII.
 *
 * Ownership: te2_call BORROWS args (caller frees); its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node.
 * Seam results are caller-freed. A propagated raise dict is returned
 * upward as-is; a caught raise dict is freed at the catch site. Mutation
 * flows (sync_cli_session_id, configure_quiet_agent, finite_block,
 * take_turn_report_path, collect_kanban_task_images) work on a harness-owned
 * copy echoed as out.state.
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

typedef struct te2_trace { json_t *events; } te2_trace_t;

typedef struct te2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} te2_seams_t;

static void te2_trace_add(te2_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void te2_trace_free(te2_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *te2_call(const te2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *te2_call0(const te2_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = te2_call(s, name, a);
    json_free(a);
    return r;
}

/* ── JSON value helpers ── */

static bool te2_jtruth(const json_t *v) {
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
static const char *te2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool te2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
/* `is False` / `is True` identity gates: JSON_BOOL only (fixture 1/0 read
 * as False/True respectively NEVER — they are numbers, not bools). */
static bool te2_is_false(const json_t *v) {
    return v && v->type == JSON_BOOL && !v->bool_val;
}
static bool te2_is_true(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}

static char *te2_xsf(const char *fmt, ...) {
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

typedef struct { char *p; size_t len, cap; } te2_buf;
static void te2_binit(te2_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void te2_bfree(te2_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void te2_bput(te2_buf *b, const char *s, size_t n) {
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
static void te2_bputs(te2_buf *b, const char *s) { te2_bput(b, s, strlen(s)); }
static char *te2_btake(te2_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* ── exception model (FULL py3.14 builtin parent hierarchy) ── */

static const char *te2_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool te2_is_raise(const json_t *r) { return te2_raise_class(r) != NULL; }

static json_t *te2_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static bool te2_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
/* Parent lookup audited against the py3.14.9 quarry interpreter's builtins
 * (cls.__bases__[0].__name__ for every BaseException subclass). Unknown
 * custom kinds default to Exception (the oracle builds type(name,
 * (Exception,)) for them); the BaseException trio stays OUTSIDE Exception
 * so except Exception passes them through. GatewayClientError is quarry-
 * verified: hermes_cli/gateway_client.py defines it as (ValueError). */
static const char *te2_exc_parent(const char *k) {
    if (!strcmp(k, "BaseException")) return NULL;
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
        !strcmp(k, "GeneratorExit") || !strcmp(k, "Exception") ||
        !strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    static const char *const EXC_CHILDREN[] = {
        "StopIteration", "StopAsyncIteration", "ArithmeticError", "AssertionError",
        "AttributeError", "BufferError", "EOFError", "ImportError", "LookupError",
        "MemoryError", "NameError", "OSError", "ReferenceError", "RuntimeError",
        "SyntaxError", "SystemError", "TypeError", "ValueError", "Warning", NULL
    };
    if (te2_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (te2_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", NULL };
    if (te2_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (te2_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    /* IOError / EnvironmentError / WindowsError are OSError ALIASES in
     * py3.14; route them through OSError so errno-catch walks work. The
     * oracle builds the real OSError class for those kinds (the envelope
     * normalizes the name to OSError), so fixtures use OSError directly. */
    if (!strcmp(k, "IOError") || !strcmp(k, "EnvironmentError") ||
        !strcmp(k, "WindowsError")) return "OSError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (te2_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (te2_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = {
        "NotImplementedError", "RecursionError", "PythonFinalizationError", NULL
    };
    if (te2_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "_IncompleteInputError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    if (!strcmp(k, "GatewayClientError")) return "ValueError";  /* quarry-verified */
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (te2_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (te2_in_names(k, NAME_CHILDREN)) return "NameError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (te2_in_names(k, WARNING_CHILDREN)) return "Warning";
    return "Exception";  /* unknown custom kind: oracle bases it on Exception */
}
/* caught(r, cls): cls appears anywhere in r's parent chain */
static bool te2_caught(const json_t *r, const char *cls) {
    const char *k = te2_raise_class(r);
    /* IOError / EnvironmentError / WindowsError are OSError aliases: catching
     * by an alias name is catching OSError (normalize the catch target). */
    if (!strcmp(cls, "IOError") || !strcmp(cls, "EnvironmentError") ||
        !strcmp(cls, "WindowsError")) cls = "OSError";
    /* ExceptionGroup multiply-inherits (BaseExceptionGroup, Exception):
     * the single-parent walk must still honor `except Exception`. */
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = te2_exc_parent(k);
    }
    return false;
}

/* ── Python string semantics (ASCII domain) ── */

/* str.strip / str.split whitespace: ASCII White_Space incl \x1c-\x1f. */
static bool te2_is_strip_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' ||
           c == '\r' || (c >= 0x1c && c <= 0x1f);
}
/* int() strips a NARROWER set: White_Space minus \x1c-\x1f. */
static bool te2_is_int_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' ||
           c == '\r';
}
static bool te2_is_printable(unsigned char c) {
    return c >= 0x20 && c <= 0x7e;
}

static char *te2_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && te2_is_strip_ws((unsigned char)s[a])) a++;
    while (b > a && te2_is_strip_ws((unsigned char)s[b - 1])) b--;
    char *r = malloc(b - a + 1);
    if (r) { memcpy(r, s + a, b - a); r[b - a] = '\0'; }
    return r ? r : strdup("");
}

/* str.split() with no args: split on whitespace runs, drop empties. */
static json_t *te2_split_ws(const char *s) {
    json_t *out = json_array();
    if (!s) return out;
    size_t i = 0, n = strlen(s);
    while (i < n) {
        while (i < n && te2_is_strip_ws((unsigned char)s[i])) i++;
        if (i >= n) break;
        size_t start = i;
        while (i < n && !te2_is_strip_ws((unsigned char)s[i])) i++;
        char *tok = malloc(i - start + 1);
        if (tok) { memcpy(tok, s + start, i - start); tok[i - start] = '\0'; }
        json_append(out, json_string(tok ? tok : ""));
        free(tok);
    }
    return out;
}

/* int(str) grammar: int-strip ws, optional sign, digits with single
 * underscores BETWEEN digits. Returns true on success (value via *out).
 * Overflow beyond long long is outside the fixture domain. */
static bool te2_int_parse(const char *s, long long *out) {
    if (!s) return false;
    size_t n = strlen(s), i = 0;
    while (i < n && te2_is_int_ws((unsigned char)s[i])) i++;
    size_t end = n;
    while (end > i && te2_is_int_ws((unsigned char)s[end - 1])) end--;
    if (i >= end) return false;
    bool neg = false;
    if (s[i] == '+' || s[i] == '-') { neg = (s[i] == '-'); i++; }
    if (i >= end) return false;
    long long v = 0;
    bool prev_digit = false;
    for (; i < end; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            v = v * 10 + (c - '0');
            prev_digit = true;
        } else if (c == '_') {
            if (!prev_digit) return false;   /* leading / doubled underscore */
            prev_digit = false;              /* must be followed by a digit */
        } else {
            return false;
        }
    }
    if (!prev_digit) return false;           /* trailing underscore */
    *out = neg ? -v : v;
    return true;
}

/* str() projection for f-string operands (domain: str / int / bool / null). */
static char *te2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) return te2_xsf("%.0f", d);
            return te2_xsf("%g", d);
        }
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default:          return strdup("");   /* containers outside fstr domain */
    }
}

/* PurePosixPath normalization: count the leading slash run (root is '//'
 * iff run==2 else '/' when run>=1), drop empty/'.' components, keep '..'.
 * '' and '.' -> '.'. */
static char *te2_posix_norm(const char *s) {
    if (!s) return strdup(".");
    size_t lead = 0, n = strlen(s);
    while (lead < n && s[lead] == '/') lead++;
    const char *root = (lead == 2) ? "//" : (lead >= 1 ? "/" : "");
    te2_buf b; te2_binit(&b);
    size_t i = lead;
    bool any = false;
    while (i <= n) {
        size_t start = i;
        while (i < n && s[i] != '/') i++;
        size_t len = i - start;
        if (len > 0 && !(len == 1 && s[start] == '.')) {
            if (any) te2_bputs(&b, "/");
            te2_bput(&b, s + start, len);
            any = true;
        }
        if (i >= n) break;
        i++;  /* skip '/' */
    }
    char *parts = te2_btake(&b);
    char *r;
    if (!any) {
        r = strdup(root[0] ? root : ".");
    } else {
        r = te2_xsf("%s%s", root, parts);
    }
    free(parts);
    return r;
}

/* scalar inequality for the session-id sync (!= on str/int/bool/null). */
static bool te2_scalar_neq(const json_t *a, const json_t *b) {
    bool an = te2_is_null(a), bn = te2_is_null(b);
    if (an || bn) return !(an && bn);
    if (a->type != b->type) return true;
    switch (a->type) {
        case JSON_BOOL:   return a->bool_val != b->bool_val;
        case JSON_NUMBER: return a->num_val != b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                        b->str_val ? b->str_val : "") != 0;
        default:          return true;   /* containers outside the sync domain */
    }
}

/* print seam: args {text, file:"stdout"|"stderr", flush}. */
static json_t *te2_print(const te2_seams_t *s, const char *text,
                         const char *file, bool flush) {
    json_t *a = json_object();
    json_set(a, "text", json_string(text ? text : ""));
    json_set(a, "file", json_string(file ? file : "stdout"));
    json_set(a, "flush", json_bool(flush));
    json_t *r = te2_call(s, "print", a);
    json_free(a);
    return r;
}

/* os.environ.get — a direct STATE read (no seam). */
static const json_t *te2_env_get(const json_t *env, const char *key) {
    if (!env || env->type != JSON_OBJECT) return NULL;
    return json_obj_get(env, key);
}

/* ── hermes_cli/cli_single_query.py ── */

/* PoP: te2_int_or @ hermes_cli/cli_single_query.py:_int_or */
static json_t *te2_int_or(const json_t *value, const json_t *dflt) {
    /* try: return int(value) except (TypeError, ValueError): return default */
    if (value) {
        if (value->type == JSON_BOOL)
            return json_number(value->bool_val ? 1 : 0);
        if (value->type == JSON_NUMBER) {
            double d = value->num_val;
            if (isnan(d) || isinf(d)) goto fallback;  /* ValueError/OverflowError */
            return json_number(trunc(d));
        }
        if (value->type == JSON_STRING) {
            long long v;
            if (te2_int_parse(value->str_val, &v))
                return json_number((double)v);
            goto fallback;  /* ValueError */
        }
    }
fallback:
    /* TypeError (None/list/dict/...) or ValueError -> default */
    return dflt ? json_copy(dflt) : NULL;
}

/* PoP: te2_single_query_exit_code @ hermes_cli/cli_single_query.py:_single_query_exit_code */
static json_t *te2_single_query_exit_code(const te2_seams_t *s, const json_t *result,
                                          bool crl, bool ct) {
    /* from cli import _TERMINAL_PROVIDER_REASONS as terminal_reasons */
    json_t *term = te2_call0(s, "import__TERMINAL_PROVIDER_REASONS");
    if (te2_is_raise(term)) return term;
    /* from cli import _TRANSIENT_PROVIDER_REASONS as transient_reasons */
    json_t *trans = te2_call0(s, "import__TRANSIENT_PROVIDER_REASONS");
    if (te2_is_raise(trans)) { json_free(term); return trans; }
    /* from agent.kanban_turn_recovery import kanban_task_id */
    json_t *ik = te2_call0(s, "import_kanban_task_id");
    if (te2_is_raise(ik)) { json_free(term); json_free(trans); return ik; }
    json_free(ik);
    /* from hermes_cli.turn_exit import turn_exit_code */
    json_t *it = te2_call0(s, "import_turn_exit_code");
    if (te2_is_raise(it)) { json_free(term); json_free(trans); return it; }
    json_free(it);
    /* kwargs evaluate left-to-right: kanban_task_id() fires BEFORE turn_exit_code */
    json_t *tid = te2_call0(s, "kanban_task_id");
    if (te2_is_raise(tid)) { json_free(term); json_free(trans); return tid; }
    bool worker = !te2_is_null(tid);   /* kanban_task_id() is not None */
    json_free(tid);
    json_t *a = json_object();
    if (result) json_set(a, "result", json_copy(result));
    json_set(a, "kanban_worker", json_bool(worker));
    json_set(a, "credentials_rate_limited", json_bool(crl));
    json_set(a, "credentials_terminal", json_bool(ct));
    if (trans) json_set(a, "transient_reasons", json_copy(trans));
    if (term) json_set(a, "terminal_reasons", json_copy(term));
    json_t *r = te2_call(s, "turn_exit_code", a);
    json_free(a);
    json_free(term);
    json_free(trans);
    return r;   /* return turn_exit_code(...) — raise propagates */
}

/* PoP: te2_sync_cli_session_id @ hermes_cli/cli_single_query.py:_sync_cli_session_id_from_agent */
static json_t *te2_sync_cli_session_id(json_t *cli) {
    /* if getattr(cli.agent, "session_id", None) and cli.agent.session_id != cli.session_id:
     *     cli.session_id = cli.agent.session_id */
    const json_t *agent = json_obj_get(cli, "agent");
    const json_t *asid = agent ? json_obj_get(agent, "session_id") : NULL;
    if (te2_jtruth(asid)) {
        const json_t *csid = json_obj_get(cli, "session_id");
        if (te2_scalar_neq(asid, csid))
            json_set(cli, "session_id", json_copy(asid));
    }
    return NULL;
}

/* PoP: te2_interrupt_agent_for_signal @ hermes_cli/cli_single_query.py:_interrupt_agent_for_signal */
static json_t *te2_interrupt_agent_for_signal(const te2_seams_t *s,
                                              const json_t *agent, double signum) {
    /* from cli import _float_env — OUTSIDE the try: a raise propagates */
    json_t *ife = te2_call0(s, "import__float_env");
    if (te2_is_raise(ife)) return ife;
    json_free(ife);
    /* try: */
    if (!te2_is_null(agent)) {
        char *msg = te2_xsf("received signal %.0f", signum);
        json_t *a = json_object();
        if (agent) json_set(a, "agent", json_copy(agent));
        json_set(a, "message", json_string(msg));
        free(msg);
        json_t *r = te2_call(s, "request_hard_interrupt", a);
        json_free(a);
        if (te2_is_raise(r)) {
            if (te2_caught(r, "Exception")) { json_free(r); return NULL; }
            return r;   /* BaseException passthrough */
        }
        json_free(r);
        /* _grace = _float_env("HERMES_SIGTERM_GRACE", 1.5) */
        json_t *fa = json_object();
        json_set(fa, "name", json_string("HERMES_SIGTERM_GRACE"));
        json_set(fa, "default", json_number(1.5));
        json_t *grace = te2_call(s, "float_env", fa);
        json_free(fa);
        if (te2_is_raise(grace)) {
            if (te2_caught(grace, "Exception")) { json_free(grace); return NULL; }
            return grace;
        }
        /* if _grace > 0: time.sleep(_grace) — non-numeric grace raises
         * TypeError inside the try (swallowed); bool compares numerically
         * (True > 0 is True in Python). */
        bool gt = false;
        if (grace && grace->type == JSON_NUMBER) gt = grace->num_val > 0.0;
        else if (grace && grace->type == JSON_BOOL) gt = grace->bool_val;
        else if (grace && grace->type != JSON_NULL) {
            /* TypeError: '>' not supported between instances — caught */
            json_free(grace);
            return NULL;
        }
        if (gt) {
            json_t *sa = json_object();
            json_set(sa, "seconds", json_copy(grace));
            json_t *sr = te2_call(s, "time_sleep", sa);
            json_free(sa);
            json_free(grace);
            if (te2_is_raise(sr)) {
                if (te2_caught(sr, "Exception")) { json_free(sr); return NULL; }
                return sr;
            }
            json_free(sr);
        } else {
            json_free(grace);
        }
    }
    /* except Exception: pass — never block signal handling */
    return NULL;
}

/* PoP: te2_configure_quiet_agent @ hermes_cli/cli_single_query.py:_configure_quiet_agent */
static json_t *te2_configure_quiet_agent(json_t *agent) {
    json_set(agent, "quiet_mode", json_bool(true));
    json_set(agent, "suppress_status_output", json_bool(true));
    json_set(agent, "stream_delta_callback", json_null());
    json_set(agent, "tool_gen_callback", json_null());
    json_set(agent, "reasoning_callback", json_null());
    json_set(agent, "tool_progress_callback", json_null());
    json_set(agent, "tool_start_callback", json_null());
    json_set(agent, "tool_complete_callback", json_null());
    json_set(agent, "tool_progress_mode", json_string("off"));
    return NULL;
}

/* PoP: te2_run_kanban_goal_loop_chat @ hermes_cli/cli_single_query.py:_run_kanban_goal_loop_chat */
static json_t *te2_run_kanban_goal_loop_chat(const te2_seams_t *s, const json_t *cli,
                                             const char *first_response) {
    /* from cli import _run_kanban_goal_loop_q */
    json_t *iq = te2_call0(s, "import__run_kanban_goal_loop_q");
    if (te2_is_raise(iq)) return iq;
    json_free(iq);
    /* _run_kanban_goal_loop_q(cli, first_response,
     *     run_turn=lambda p: cli.chat(p) or "", log=_log)
     * The claimed sibling is an inert recorder: the closures project to
     * positional callable markers (never invoked under the seam). */
    json_t *a = json_object();
    if (cli) json_set(a, "cli", json_copy(cli));
    json_set(a, "first_response", json_string(first_response ? first_response : ""));
    json_t *rt = json_object();
    json_set(rt, "__callable__", json_string("run_turn"));
    json_set(a, "run_turn", rt);
    json_t *lg = json_object();
    json_set(lg, "__callable__", json_string("log"));
    json_set(a, "log", lg);
    json_t *r = te2_call(s, "run_kanban_goal_loop_q", a);
    json_free(a);
    return r;
}

/* CM helper: run the connect_closing with-block body (get_task) with full
 * __enter__/__exit__ semantics. Returns the task node (owned) or a raise
 * spec; *exit_raised marks an exit raise that masks the body raise. */
static json_t *te2_connect_get_task(const te2_seams_t *s, const char *task_id) {
    /* with _kbc.connect_closing() as _conn: task = _kb.get_task(_conn, task_id)
     * _kbc.connect_closing / _kb.get_task are LEAF attribute accesses: each
     * fires an import seam before its call. */
    json_t *ic = te2_call0(s, "import_connect_closing");
    if (te2_is_raise(ic)) return ic;   /* attr raise: no factory/enter/exit */
    json_free(ic);
    json_t *fac = te2_call0(s, "connect_closing");
    if (te2_is_raise(fac)) return fac;   /* factory raise: no enter/exit */
    json_free(fac);
    json_t *conn = te2_call0(s, "connect_closing_enter");
    if (te2_is_raise(conn)) return conn; /* enter raise: no exit */
    /* body: _kb.get_task attribute access (import seam) then the call; an
     * import raise here is a BODY raise (the __exit__ still runs). */
    json_t *task = NULL;
    json_t *ig = te2_call0(s, "import_get_task");
    if (te2_is_raise(ig)) {
        task = ig;
    } else {
        json_free(ig);
        json_t *ga = json_object();
        if (conn) json_set(ga, "conn", json_copy(conn));
        json_set(ga, "task_id", json_string(task_id ? task_id : ""));
        task = te2_call(s, "get_task", ga);
        json_free(ga);
    }
    /* finally: __exit__ runs on EVERY body path (success or raise) */
    json_t *ex = te2_call0(s, "connect_closing_exit");
    if (te2_is_raise(ex)) {
        /* an exit raise MASKS the body raise (CPython finally semantics) */
        json_free(conn);
        if (te2_is_raise(task)) json_free(task);
        return ex;
    }
    bool suppress = te2_jtruth(ex);
    json_free(ex);
    json_free(conn);
    if (te2_is_raise(task)) {
        if (suppress) { json_free(task); return NULL; }
        return task;
    }
    return task;
}

/* PoP: te2_collect_kanban_task_images @ hermes_cli/cli_single_query.py:_collect_kanban_task_images */
static json_t *te2_collect_kanban_task_images(const te2_seams_t *s, json_t *images,
                                              const json_t *env) {
    json_t *urls_out = json_array();
    /* _kanban_task_id = os.environ.get("HERMES_KANBAN_TASK", "").strip() */
    const json_t *ev = te2_env_get(env, "HERMES_KANBAN_TASK");
    const char *evs = te2_jstr(ev);
    char *task_id = te2_strip(evs ? evs : "");
    if (!task_id[0]) { free(task_id); return urls_out; }
    /* try: imports + with + enrichment */
    /* from hermes_cli import kanban_db / kanban_db_connect: NO seam (pre-set
     * package attrs); from agent.image_routing import extract_image_refs: */
    json_t *ie = te2_call0(s, "import_extract_image_refs");
    if (te2_is_raise(ie)) {
        if (te2_caught(ie, "Exception")) { json_free(ie); free(task_id); return urls_out; }
        free(task_id); return ie;
    }
    json_free(ie);
    json_t *task = te2_connect_get_task(s, task_id);
    free(task_id);
    if (te2_is_raise(task)) {
        if (te2_caught(task, "Exception")) { json_free(task); return urls_out; }
        return task;
    }
    /* _body = getattr(_task, "body", "") if _task is not None else "" */
    const json_t *body = te2_is_null(task) ? NULL : json_obj_get(task, "body");
    const char *bs = te2_jstr(body);
    if (bs && bs[0]) {
        json_t *ea = json_object();
        json_set(ea, "body", json_string(bs));
        json_t *refs = te2_call(s, "extract_image_refs", ea);
        json_free(ea);
        if (te2_is_raise(refs)) {
            json_free(task);
            if (te2_caught(refs, "Exception")) { json_free(refs); return urls_out; }
            return refs;
        }
        /* _kb_paths, _kb_urls = _extract_refs(_body) — 2-tuple unpack */
        if (!refs || refs->type != JSON_ARRAY) {
            char *m = te2_xsf("cannot unpack non-iterable %s object",
                              refs ? (refs->type == JSON_NULL ? "NoneType" :
                                      refs->type == JSON_NUMBER ? "int" :
                                      refs->type == JSON_STRING ? "str" :
                                      refs->type == JSON_BOOL ? "bool" : "object")
                                   : "NoneType");
            json_t *err = te2_raise_spec("TypeError", m);
            free(m);
            json_free(refs); json_free(task);
            if (te2_caught(err, "Exception")) { json_free(err); return urls_out; }
            return err;
        }
        size_t rn = json_len(refs);
        if (rn != 2) {
            char *m = te2_xsf("%s values to unpack (expected 2, got %zu)",
                              rn > 2 ? "too many" : "not enough", rn);
            json_t *err = te2_raise_spec("ValueError", m);
            free(m);
            json_free(refs); json_free(task);
            if (te2_caught(err, "Exception")) { json_free(err); return urls_out; }
            return err;
        }
        const json_t *paths = json_get(refs, 0);
        const json_t *urls = json_get(refs, 1);
        /* _seen = {str(p) for p in single_query_images}; for _p in _kb_paths:
         * if _p not in _seen: _seen.add(_p); images.append(Path(_p)).
         * The seen set holds RAW strings (original images + earlier raw
         * paths); the appended Path is the NORMALIZED form. */
        if (paths && paths->type == JSON_ARRAY) {
            char **seen = NULL;
            size_t nseen = 0, capseen = 0;
            for (size_t j = 0; j < json_len(images); j++) {
                const char *q = te2_jstr(json_get(images, j));
                if (!q) continue;
                if (nseen == capseen) {
                    capseen = capseen ? capseen * 2 : 8;
                    seen = realloc(seen, capseen * sizeof(char *));
                }
                seen[nseen++] = strdup(q);
            }
            for (size_t i = 0; i < json_len(paths); i++) {
                const json_t *pn = json_get(paths, i);
                const char *p = te2_jstr(pn);
                if (!p) {
                    /* Path(non-str): TypeError — caught by the except */
                    for (size_t j = 0; j < nseen; j++) free(seen[j]);
                    free(seen);
                    json_free(refs); json_free(task);
                    return urls_out;
                }
                bool found = false;
                for (size_t j = 0; j < nseen; j++)
                    if (!strcmp(seen[j], p)) { found = true; break; }
                if (found) continue;
                if (nseen == capseen) {
                    capseen = capseen ? capseen * 2 : 8;
                    seen = realloc(seen, capseen * sizeof(char *));
                }
                seen[nseen++] = strdup(p);
                char *np = te2_posix_norm(p);
                json_append(images, json_string(np));
                free(np);
            }
            for (size_t j = 0; j < nseen; j++) free(seen[j]);
            free(seen);
        } else {
            /* iterating a null/non-list paths: TypeError — caught (the
             * extend below is never reached on this path) */
            json_free(refs); json_free(task);
            return urls_out;
        }
        /* single_query_image_urls.extend(_kb_urls) — a null/non-list urls
         * raises TypeError ('X' object is not iterable), caught; the
         * images appends above persist (partial mutation history). */
        if (urls && urls->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(urls); i++)
                json_append(urls_out, json_copy(json_get(urls, i)));
        } else {
            json_free(refs); json_free(task);
            return urls_out;
        }
        json_free(refs);
    }
    json_free(task);
    /* except Exception as _exc: logger.debug (silent) */
    return urls_out;
}

/* _text_fallback closure: if single_query_images: return
 * cli._preprocess_images_with_vision(query, images, announce=False);
 * return effective_query. */
static json_t *te2_route_text_fallback(const te2_seams_t *s, const json_t *cli,
                                       const json_t *query, const json_t *images,
                                       const json_t *effective_query) {
    if (te2_jtruth(images)) {
        json_t *a = json_object();
        if (query) json_set(a, "query", json_copy(query));
        json_set(a, "images", json_copy(images));
        json_set(a, "announce", json_bool(false));
        json_t *r = te2_call(s, "preprocess_images_with_vision", a);
        json_free(a);
        return r;
    }
    return effective_query ? json_copy(effective_query) : NULL;
}

/* PoP: te2_route_single_query_images @ hermes_cli/cli_single_query.py:_route_single_query_images */
static json_t *te2_route_single_query_images(const te2_seams_t *s, const json_t *cli,
                                             const json_t *query,
                                             const json_t *effective_query,
                                             const json_t *images, const json_t *urls) {
    /* if not (single_query_images or single_query_image_urls): return effective_query */
    if (!te2_jtruth(images) && !te2_jtruth(urls))
        return effective_query ? json_copy(effective_query) : NULL;
    /* _img_mode = "text"; _build_parts = None */
    bool native = false;
    bool have_build = false;
    /* try: imports + decide */
    json_t *ib = te2_call0(s, "import_build_native_content_parts");
    if (te2_is_raise(ib)) {
        if (!te2_caught(ib, "Exception")) return ib;
        json_free(ib);
    } else {
        json_free(ib);
        have_build = true;   /* _build_parts = the imported fn */
        json_t *idm = te2_call0(s, "import_decide_image_input_mode");
        if (te2_is_raise(idm)) {
            if (!te2_caught(idm, "Exception")) return idm;
            json_free(idm);
        } else {
            json_free(idm);
            json_t *ilc = te2_call0(s, "import_load_config");
            if (te2_is_raise(ilc)) {
                if (!te2_caught(ilc, "Exception")) return ilc;
                json_free(ilc);
            } else {
                json_free(ilc);
                /* decide_image_input_mode((cli.provider or "").strip(),
                 *     (cli.model or "").strip(), load_config(),
                 *     requested_provider=(cli.requested_provider or "").strip())
                 * positional args evaluate left-to-right: load_config fires
                 * BEFORE the decide seam. */
                const json_t *pv = json_obj_get(cli, "provider");
                const json_t *mv = json_obj_get(cli, "model");
                const json_t *rv = json_obj_get(cli, "requested_provider");
                char *ps = te2_strip(te2_jtruth(pv) ? te2_jstr(pv) : "");
                char *ms = te2_strip(te2_jtruth(mv) ? te2_jstr(mv) : "");
                json_t *cfg = te2_call0(s, "load_config");
                if (te2_is_raise(cfg)) {
                    free(ps); free(ms);
                    if (!te2_caught(cfg, "Exception")) return cfg;
                    json_free(cfg);
                } else {
                    char *rs = te2_strip(te2_jtruth(rv) ? te2_jstr(rv) : "");
                    json_t *da = json_object();
                    json_set(da, "provider", json_string(ps));
                    json_set(da, "model", json_string(ms));
                    if (cfg) json_set(da, "config", json_copy(cfg));
                    json_set(da, "requested_provider", json_string(rs));
                    json_t *mode = te2_call(s, "decide_image_input_mode", da);
                    json_free(da);
                    json_free(cfg);
                    free(rs);
                    if (te2_is_raise(mode)) {
                        if (!te2_caught(mode, "Exception")) { free(ps); free(ms); return mode; }
                        json_free(mode);
                    } else {
                        const char *ms2 = te2_jstr(mode);
                        native = ms2 && !strcmp(ms2, "native");
                        json_free(mode);
                    }
                }
                free(ps); free(ms);
            }
        }
    }
    /* if _img_mode != "native" or _build_parts is None: return _text_fallback()
     * (OUTSIDE any try — its raise propagates) */
    if (!native || !have_build)
        return te2_route_text_fallback(s, cli, query, images, effective_query);
    /* try: _parts, _skipped = _build_parts(...); if any(image_url): return _parts;
     *      return _text_fallback()
     * except Exception: return _text_fallback() */
    json_t *ba = json_object();
    const char *qs = te2_jstr(query);
    json_set(ba, "query", json_string(qs ? qs : ""));  /* query if isinstance(str) else "" */
    json_t *pa = json_array();
    if (images && images->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(images); i++) {
            char *sp = te2_pystr(json_get(images, i));
            json_append(pa, json_string(sp));
            free(sp);
        }
    }
    json_set(ba, "paths", pa);
    /* image_urls=list(single_query_image_urls) or None */
    if (te2_jtruth(urls)) json_set(ba, "image_urls", json_copy(urls));
    json_t *parts_res = te2_call(s, "build_native_content_parts", ba);
    json_free(ba);
    json_t *result = NULL;
    bool caught_exc = false;
    if (te2_is_raise(parts_res)) {
        if (te2_caught(parts_res, "Exception")) { json_free(parts_res); caught_exc = true; }
        else return parts_res;   /* BaseException passthrough */
    } else if (!parts_res || parts_res->type != JSON_ARRAY) {
        /* unpack TypeError — caught */
        json_free(parts_res);
        caught_exc = true;
    } else if (json_len(parts_res) != 2) {
        /* unpack ValueError — caught */
        json_free(parts_res);
        caught_exc = true;
    } else {
        const json_t *parts = json_get(parts_res, 0);
        /* any(p.get("type") == "image_url" for p in _parts) */
        bool any_img = false;
        if (parts && parts->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(parts); i++) {
                const json_t *p = json_get(parts, i);
                if (p && p->type == JSON_OBJECT) {
                    const char *ty = te2_jstr(json_obj_get(p, "type"));
                    if (ty && !strcmp(ty, "image_url")) { any_img = true; break; }
                } else {
                    /* p.get on a non-dict: AttributeError — caught */
                    caught_exc = true;
                    break;
                }
            }
        } else {
            /* iterating a null/non-list parts: TypeError — caught */
            caught_exc = true;
        }
        if (!caught_exc && any_img) {
            result = json_copy(parts);   /* return _parts */
        } else if (!caught_exc) {
            /* return _text_fallback() INSIDE the try: its Exception raise
             * is caught and the fallback runs a SECOND time. */
            json_t *fb = te2_route_text_fallback(s, cli, query, images, effective_query);
            if (te2_is_raise(fb)) {
                if (te2_caught(fb, "Exception")) { json_free(fb); caught_exc = true; }
                else { json_free(parts_res); return fb; }
            } else {
                result = fb;
            }
        }
        json_free(parts_res);
    }
    if (caught_exc) {
        /* except Exception: return _text_fallback() */
        return te2_route_text_fallback(s, cli, query, images, effective_query);
    }
    return result;
}

/* ── hermes_cli/gateway_chat_view.py ── */

#define TE2_PREVIEW_CHARS 80

/* PoP: te2_pending_preview @ hermes_cli/gateway_chat_view.py:_pending_preview */
static json_t *te2_pending_preview(const te2_seams_t *s, const json_t *text) {
    /* from tools.ansi_strip import strip_ansi — BEFORE the isinstance gate */
    json_t *isa = te2_call0(s, "import_strip_ansi");
    if (te2_is_raise(isa)) return isa;
    json_free(isa);
    /* if not isinstance(text, str): return "" */
    const char *ts = te2_jstr(text);
    if (!ts) return json_string("");
    json_t *sa = json_object();
    json_set(sa, "text", json_string(ts));
    json_t *stripped = te2_call(s, "strip_ansi", sa);
    json_free(sa);
    if (te2_is_raise(stripped)) return stripped;
    const char *ss = te2_jstr(stripped);
    if (!ss) ss = "";   /* non-str strip result outside domain */
    /* line = " ".join("".join(ch if ch.isprintable() else " " for ch in
     *                   strip_ansi(text)).split()) */
    te2_buf b; te2_binit(&b);
    for (size_t i = 0; ss[i]; i++) {
        char c = te2_is_printable((unsigned char)ss[i]) ? ss[i] : ' ';
        te2_bput(&b, &c, 1);
    }
    char *folded = te2_btake(&b);
    json_t *words = te2_split_ws(folded);
    free(folded);
    json_free(stripped);
    te2_buf line; te2_binit(&line);
    for (size_t i = 0; i < json_len(words); i++) {
        if (i) te2_bputs(&line, " ");
        te2_bputs(&line, te2_jstr(json_get(words, i)));
    }
    json_free(words);
    char *ln = te2_btake(&line);
    /* if len(line) > _PREVIEW_CHARS: line = line[:_PREVIEW_CHARS - 1] + "…" */
    size_t llen = strlen(ln);
    json_t *out;
    if (llen > TE2_PREVIEW_CHARS) {
        ln[TE2_PREVIEW_CHARS - 1] = '\0';
        char *tmp = te2_xsf("%s\xe2\x80\xa6", ln);
        out = json_string(tmp);
        free(tmp);
    } else {
        out = json_string(ln);
    }
    free(ln);
    return out;
}

/* PoP: te2_quoted @ hermes_cli/gateway_chat_view.py:_quoted */
static json_t *te2_quoted(const te2_seams_t *s, const json_t *row, const char *fallback) {
    /* preview = _pending_preview(row.get("text")) */
    json_t *pv = te2_pending_preview(s, json_obj_get(row, "text"));
    if (te2_is_raise(pv)) return pv;
    const char *ps = te2_jstr(pv);
    json_t *out;
    if (ps && ps[0]) {
        char *tmp = te2_xsf("    \"%s\"", ps);
        out = json_string(tmp);
        free(tmp);
    } else {
        /* f"    ({fallback} {row['admission_id']})" — KeyError if absent */
        const json_t *aid = json_obj_get(row, "admission_id");
        if (!aid) {
            json_free(pv);
            return te2_raise_spec("KeyError", "'admission_id'");
        }
        char *as = te2_pystr(aid);
        char *tmp = te2_xsf("    (%s %s)", fallback ? fallback : "", as);
        out = json_string(tmp);
        free(tmp);
        free(as);
    }
    json_free(pv);
    return out;
}

/* PoP: te2_pending_lines @ hermes_cli/gateway_chat_view.py:pending_lines */
static json_t *te2_pending_lines(const te2_seams_t *s, const json_t *pending, bool interactive) {
    /* lost = [row for row in pending if row["status"] == "unknown"]
     * queued = [row for row in pending if row["status"] == "queued"] */
    json_t *lost = json_array();
    json_t *queued = json_array();
    if (pending && pending->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pending); i++) {
            const json_t *row = json_get(pending, i);
            const json_t *st = row ? json_obj_get(row, "status") : NULL;
            if (!st) {
                json_free(lost); json_free(queued);
                return te2_raise_spec("KeyError", "'status'");
            }
            const char *ss = te2_jstr(st);
            if (ss && !strcmp(ss, "unknown")) json_append(lost, json_copy(row));
            else if (ss && !strcmp(ss, "queued")) json_append(queued, json_copy(row));
        }
    }
    json_t *lines = json_array();
    size_t nlost = json_len(lost), nqueued = json_len(queued);
    if (nlost == 1) {
        const json_t *row0 = json_get(lost, 0);
        json_append(lines, json_string(
            "\xe2\x8f\xb8 This chat is paused. Hermes couldn't confirm how the last turn ended "
            "(usually because it restarted mid-turn):"));
        /* if _pending_preview(lost[0].get("text")): lines.append(_quoted(...)) */
        json_t *pv = te2_pending_preview(s, json_obj_get(row0, "text"));
        if (te2_is_raise(pv)) { json_free(lost); json_free(queued); json_free(lines); return pv; }
        const char *ps = te2_jstr(pv);
        bool has = ps && ps[0];
        json_free(pv);
        if (has) {
            json_t *q = te2_quoted(s, row0, "lost turn");
            if (te2_is_raise(q)) { json_free(lost); json_free(queued); json_free(lines); return q; }
            json_append(lines, q);
        }
        json_append(lines, json_string(
            "  Hermes won't re-run it by itself, since it may already have done part of the work."));
        if (interactive) {
            json_append(lines, json_string("  To drop it and continue, type:"));
            const json_t *aid = json_obj_get(row0, "admission_id");
            if (!aid) {
                json_free(lost); json_free(queued); json_free(lines);
                return te2_raise_spec("KeyError", "'admission_id'");
            }
            char *as = te2_pystr(aid);
            char *tmp = te2_xsf("/discard %s", as);
            json_append(lines, json_string(tmp));
            free(tmp); free(as);
        }
    } else if (nlost > 0) {
        char *tmp = te2_xsf(
            "\xe2\x8f\xb8 This chat is paused. Hermes couldn't confirm how %zu turns ended "
            "(usually because it restarted mid-turn):", nlost);
        json_append(lines, json_string(tmp));
        free(tmp);
        json_append(lines, json_string(
            "  Hermes won't re-run them by itself, since they may already have done part of the work."));
        if (interactive)
            json_append(lines, json_string(
                "  To continue, drop each one with the /discard line under it:"));
        for (size_t i = 0; i < nlost; i++) {
            const json_t *row = json_get(lost, i);
            json_t *q = te2_quoted(s, row, "lost turn");
            if (te2_is_raise(q)) { json_free(lost); json_free(queued); json_free(lines); return q; }
            json_append(lines, q);
            if (interactive) {
                const json_t *aid = json_obj_get(row, "admission_id");
                if (!aid) {
                    json_free(lost); json_free(queued); json_free(lines);
                    return te2_raise_spec("KeyError", "'admission_id'");
                }
                char *as = te2_pystr(aid);
                char *t2 = te2_xsf("/discard %s", as);
                json_append(lines, json_string(t2));
                free(t2); free(as);
            }
        }
    }
    if (nqueued > 0) {
        /* count = "1 message is" if len(queued) == 1 else f"{len(queued)} messages are" */
        char *count = nqueued == 1 ? strdup("1 message is")
                                   : te2_xsf("%zu messages are", nqueued);
        /* after = {0: "to run", 1: "and will run once that turn is dropped"}.get(
         *     len(lost), "and will run once those turns are dropped") */
        const char *after = nlost == 0 ? "to run"
                          : nlost == 1 ? "and will run once that turn is dropped"
                          : "and will run once those turns are dropped";
        char *tmp = te2_xsf("  %s waiting %s:", count, after);
        json_append(lines, json_string(tmp));
        free(tmp); free(count);
        for (size_t i = 0; i < nqueued; i++) {
            json_t *q = te2_quoted(s, json_get(queued, i), "queued message");
            if (te2_is_raise(q)) { json_free(lost); json_free(queued); json_free(lines); return q; }
            json_append(lines, q);
        }
    }
    json_free(lost);
    json_free(queued);
    return lines;
}

/* PoP: te2_lost_admission @ hermes_cli/gateway_chat_view.py:lost_admission */
static json_t *te2_lost_admission(const json_t *snapshot, const char *admission_id) {
    /* lost = next((row for row in snapshot.get("pending", [])
     *              if row["admission_id"] == admission_id and row["status"] == "unknown"), None) */
    if (!snapshot || snapshot->type != JSON_OBJECT) {
        /* snapshot.get on a non-dict: AttributeError */
        const char *tn = te2_is_null(snapshot) ? "NoneType" :
                         snapshot->type == JSON_STRING ? "str" :
                         snapshot->type == JSON_ARRAY ? "list" :
                         snapshot->type == JSON_NUMBER ? "int" :
                         snapshot->type == JSON_BOOL ? "bool" : "object";
        char *m = te2_xsf("'%s' object has no attribute 'get'", tn);
        json_t *r = te2_raise_spec("AttributeError", m);
        free(m);
        return r;
    }
    const json_t *pending = json_obj_get(snapshot, "pending");
    if (pending && pending->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pending); i++) {
            const json_t *row = json_get(pending, i);
            const json_t *aid = row ? json_obj_get(row, "admission_id") : NULL;
            if (!aid) return te2_raise_spec("KeyError", "'admission_id'");
            const char *as = te2_jstr(aid);
            /* short-circuit: row["status"] is read ONLY for a matching id */
            if (as && !strcmp(as, admission_id ? admission_id : "")) {
                const json_t *st = json_obj_get(row, "status");
                if (!st) return te2_raise_spec("KeyError", "'status'");
                const char *ss = te2_jstr(st);
                if (ss && !strcmp(ss, "unknown"))
                    return json_copy(row);
            }
        }
    }
    /* if lost is None: raise GatewayClientError(...) */
    return te2_raise_spec("GatewayClientError",
        "No unknown (lost) admission with that id; resume to refresh");
}

/* PoP: te2_oneshot_admission @ hermes_cli/gateway_chat_view.py:_oneshot_admission */
static json_t *te2_oneshot_admission(const json_t *receipt) {
    /* if receipt is None: raise GatewayClientError("One-shot requires a query") */
    if (te2_is_null(receipt))
        return te2_raise_spec("GatewayClientError", "One-shot requires a query");
    if (receipt->type != JSON_OBJECT) {
        const char *tn = receipt->type == JSON_STRING ? "str" :
                         receipt->type == JSON_ARRAY ? "list" :
                         receipt->type == JSON_NUMBER ? "int" :
                         receipt->type == JSON_BOOL ? "bool" : "object";
        char *m = te2_xsf("'%s' object has no attribute 'get'", tn);
        json_t *r = te2_raise_spec("AttributeError", m);
        free(m);
        return r;
    }
    /* return None if receipt.get("voice_stopped") is True else receipt["admission_id"] */
    if (te2_is_true(json_obj_get(receipt, "voice_stopped"))) return NULL;
    const json_t *aid = json_obj_get(receipt, "admission_id");
    if (!aid) return te2_raise_spec("KeyError", "'admission_id'");
    return json_copy(aid);
}

/* PoP: te2_resume_footer @ hermes_cli/gateway_chat_view.py:resume_footer */
static json_t *te2_resume_footer(const te2_seams_t *s, const char *session_id) {
    /* from agent.i18n import t */
    json_t *it = te2_call0(s, "import_t");
    if (te2_is_raise(it)) return it;
    json_free(it);
    /* from hermes_cli.profiles import get_active_profile_name */
    json_t *ip = te2_call0(s, "import_get_active_profile_name");
    if (te2_is_raise(ip)) return ip;
    json_free(ip);
    /* profile = get_active_profile_name() */
    json_t *pr = te2_call0(s, "get_active_profile_name");
    if (te2_is_raise(pr)) return pr;
    const char *profile = te2_jstr(pr);
    /* flag = "" if profile in ("default", "custom") else f" -p {profile}" */
    char *flag;
    if (profile && (!strcmp(profile, "default") || !strcmp(profile, "custom")))
        flag = strdup("");
    else {
        char *ps = te2_pystr(pr);
        flag = te2_xsf(" -p %s", ps);
        free(ps);
    }
    json_free(pr);
    /* return f"\n{t('cli.session.exit_resume_hint')}\n  hermes --resume {session_id}{flag}" */
    json_t *ta = json_object();
    json_set(ta, "key", json_string("cli.session.exit_resume_hint"));
    json_t *tr = te2_call(s, "t", ta);
    json_free(ta);
    if (te2_is_raise(tr)) { free(flag); return tr; }
    char *ts = te2_pystr(tr);
    json_free(tr);
    char *out = te2_xsf("\n%s\n  hermes --resume %s%s", ts,
                        session_id ? session_id : "", flag);
    free(ts); free(flag);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* unknown_admissions comprehension (unclaimed real helper in the oracle;
 * inlined here): [row["admission_id"] for row in self.pending
 * if row["status"] == "unknown"]. Returns 1 when any unknown row exists,
 * 0 when none, -1 on a missing status key (KeyError "'status'"). The
 * filter reads status on EVERY row (no short-circuit); admission_id is
 * read only for unknown rows (present in the fixture domain). */
static int te2_unknown_admissions(const json_t *self) {
    const json_t *pending = json_obj_get(self, "pending");
    int has = 0;
    if (pending && pending->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pending); i++) {
            const json_t *row = json_get(pending, i);
            const json_t *st = row ? json_obj_get(row, "status") : NULL;
            if (!st) return -1;
            const char *ss = te2_jstr(st);
            if (ss && !strcmp(ss, "unknown")) has = 1;
        }
    }
    return has;
}

/* PoP: te2_finite_block @ hermes_cli/gateway_chat_view.py:GatewayChatView._finite_block */
static json_t *te2_finite_block(json_t *self, const char *admission) {
    /* own = next((row["status"] for row in self.pending
     *             if row["admission_id"] == admission), None) */
    const char *own = NULL;
    const json_t *pending = json_obj_get(self, "pending");
    if (pending && pending->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pending); i++) {
            const json_t *row = json_get(pending, i);
            const json_t *aid = row ? json_obj_get(row, "admission_id") : NULL;
            if (!aid) return te2_raise_spec("KeyError", "'admission_id'");
            const char *as = te2_jstr(aid);
            /* the generator yields row["status"] ONLY for a matching id */
            if (as && !strcmp(as, admission ? admission : "")) {
                const json_t *st = json_obj_get(row, "status");
                if (!st) return te2_raise_spec("KeyError", "'status'");
                own = te2_jstr(st);
                break;
            }
        }
    }
    /* if own == "unknown": self.completions[admission] = {...}; return None */
    if (own && !strcmp(own, "unknown")) {
        json_t *completions = json_obj_get(self, "completions");
        if (!completions || completions->type != JSON_OBJECT) {
            completions = json_object();
            json_set(self, "completions", completions);
        }
        json_t *rec = json_object();
        json_set(rec, "outcome", json_string("unknown"));
        json_set(rec, "text", json_string(
            "Hermes couldn't confirm how this turn ended (usually because it restarted mid-turn). "
            "Resume this session and use the /discard <id> command it shows to drop the turn. "
            "Don't send the same message again until you've checked."));
        json_set(completions, admission ? admission : "", rec);
        return NULL;
    }
    /* if own == "queued" and self.unknown_admissions(): return "..." */
    if (own && !strcmp(own, "queued")) {
        int u = te2_unknown_admissions(self);
        if (u < 0) return te2_raise_spec("KeyError", "'status'");
        if (u > 0)
            return json_string(
                "This chat is paused behind a turn whose ending Hermes couldn't confirm. "
                "Your message is saved and will run once you resume this session and /discard "
                "that turn; don't send it again.");
    }
    /* if self.prompts and (own or not self.gaps): return "..." */
    const json_t *prompts = json_obj_get(self, "prompts");
    double gaps = json_get_num(self, "gaps", 0);
    if (te2_jtruth(prompts) && ((own && own[0]) || !(gaps != 0.0)))
        return json_string(
            "Input required; detached without cancelling. Resume this session interactively.");
    return NULL;
}

/* ── hermes_cli/quiet_single_query.py ── */

/* PoP: te2_exit_single_query @ hermes_cli/quiet_single_query.py:exit_single_query */
static json_t *te2_exit_single_query(const te2_seams_t *s, const json_t *env, double code) {
    /* if os.environ.get("HERMES_KANBAN_TASK"): */
    if (te2_jtruth(te2_env_get(env, "HERMES_KANBAN_TASK"))) {
        /* with contextlib.suppress(Exception):
         *     print(f"\n{KANBAN_WORKER_EXIT_TRAILER}{int(code)}", file=sys.stderr, flush=True) */
        char *trailer = te2_xsf("\n[kanban-worker-exit] rc=%.0f", code);
        json_t *pr = te2_print(s, trailer, "stderr", true);
        free(trailer);
        if (te2_is_raise(pr)) {
            if (!te2_caught(pr, "Exception")) return pr;  /* BaseException passthrough */
            json_free(pr);   /* suppress(Exception) */
        } else {
            json_free(pr);
        }
    }
    /* sys.exit(code) — a direct SystemExit descriptor (no seam); str(code)
     * is the envelope msg, matching the live real sys.exit catch. */
    char *m = te2_xsf("%.0f", code);
    json_t *r = te2_raise_spec("SystemExit", m);
    free(m);
    return r;
}

/* PoP: te2_take_turn_report_path @ hermes_cli/quiet_single_query.py:take_turn_report_path */
static json_t *te2_take_turn_report_path(json_t *env) {
    /* return environ.pop(TURN_REPORT_FILE_ENV, None) or None */
    const json_t *v = json_obj_get(env, "HERMES_QUIET_TURN_REPORT_FILE");
    json_t *popped = v ? json_copy(v) : NULL;
    json_obj_del(env, "HERMES_QUIET_TURN_REPORT_FILE");
    if (!te2_jtruth(popped)) {   /* `or None`: falsy -> None */
        json_free(popped);
        return NULL;
    }
    return popped;
}

/* ── hermes_cli/oneshot.py ── */

/* PoP: te2_oneshot_exit_code @ hermes_cli/oneshot.py:_oneshot_exit_code */
static json_t *te2_oneshot_exit_code(const json_t *response, const json_t *result) {
    /* result.get(...) — a non-dict result raises AttributeError (domain: dict) */
    if (!result || result->type != JSON_OBJECT) {
        const char *tn = te2_is_null(result) ? "NoneType" :
                         result->type == JSON_STRING ? "str" :
                         result->type == JSON_ARRAY ? "list" :
                         result->type == JSON_NUMBER ? "int" :
                         result->type == JSON_BOOL ? "bool" : "object";
        char *m = te2_xsf("'%s' object has no attribute 'get'", tn);
        json_t *r = te2_raise_spec("AttributeError", m);
        free(m);
        return r;
    }
    /* if result.get("interrupted"): return _INTERRUPTED_EXIT_CODE (130) */
    if (te2_jtruth(json_obj_get(result, "interrupted"))) return json_number(130);
    /* if result.get("failed") or result.get("partial") or result.get("completed") is False: return 2 */
    if (te2_jtruth(json_obj_get(result, "failed")) ||
        te2_jtruth(json_obj_get(result, "partial")) ||
        te2_is_false(json_obj_get(result, "completed")))
        return json_number(2);
    /* if not (response or "").strip(): return 1 */
    const char *rs = te2_jtruth(response) ? te2_jstr(response) : NULL;
    char *st = te2_strip(rs ? rs : "");
    bool empty = !st[0];
    free(st);
    if (empty) return json_number(1);
    return json_number(0);
}