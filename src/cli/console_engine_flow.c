/*
 * console_engine_flow.c — FLOW cores + leaves of hermes_cli/console_engine.py
 * (LIVE authority) that NO existing PoP claims: the CLI-surface algebra (_paths,
 * _sub, _reg, _CliSurface.build + its main_handler lambda, _surface_summaries,
 * _dispatch + its func(namespace) lambda), the handler factories (_captured +
 * wrapper, _simple_command + handler, the _doctor handler), the argparse leaf
 * _parse, the output leaf _drop_trailing_blank, the storage / export / cron
 * flows (_session_db, _guard_exports, _cron_job_action), and the five unclaimed
 * module constants (_SHELL_TOKENS, _CONFIRMED_KINDS, _BLOCKED_TOP,
 * _BLOCKED_PAIRS, _SEND_SURFACE).
 *
 * Self-contained C11, libjson-only. The CLAIMED siblings (_capture_output,
 * _expect_no_args, _format_job, _summaries_from_parser, _noop_console_command)
 * and every foreign collaborator (the _ArgumentParser ctor + add_subparsers /
 * add_argument / parse_args / add_parser / set_defaults methods, importlib.
 * import_module + getattr, the persistence_assembly / session_export /
 * persistence.core.errors / cron.jobs in-function imports, tools.ansi_strip.
 * strip_ansi, the run / namespace_update / func callbacks) enter through ONE
 * universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; the caller frees args); the result is
 * a NEW json_t* (NULL == None; a scripted JSON null is delivered as a NULL
 * pointer; {"__raise__": cls, "msg": ...} == a raised exception). The harness
 * records {"ev": name, "args": args, "ret": result} per call in order; the
 * oracle's recording stubs emit the identical shape driving LIVE Python with
 * the same scripts. The ports perform NO real I/O.
 *
 * Ported-to-ported calls compose REAL (ce2_surface_build inside ce2_dispatch /
 * ce2_surface_summaries; ce2_simple_command_handler inside ce2_doctor); the
 * oracle keeps the extracted defs real on the same paths, so their internal
 * seam events appear in BOTH traces.
 *
 * Exception propagation: an UNCAUGHT seam raise parks its spec in the
 * file-static ce2_pending channel and the core returns NULL; the harness emits
 * the parked spec as ret. `except Exception` (_surface_summaries) swallows every
 * Exception-family kind but passes the BaseException trio (SystemExit /
 * KeyboardInterrupt / GeneratorExit) through. `except ExportTooLarge`
 * (_guard_exports) and `except AmbiguousJobReference` (_cron_job_action) catch
 * those kinds ONLY and re-raise as ConsoleCommandError(str(exc)). Fail-closed:
 * a missing/empty seam script yields a RuntimeError raise spec (recorded, then
 * propagated/swallowed exactly as live).
 *
 * Modeling decisions (fixture domain, ASCII):
 *  - _capture_output is a CLAIMED sibling seamed as a BLACK BOX: its callable
 *    argument projects to the fixed marker "<lambda>" (the seam does NOT invoke
 *    it). The wrapped lambda bodies are ported as separate direct targets
 *    (dispatch_lambda); the factory closures capture fn/engine/args but the
 *    black-box seam never runs them.
 *  - Object doubles (parser / subparsers / top-parser / module / scope /
 *    namespace) travel as plain JSON dicts; the oracle materializes them by
 *    SEAM NAME and projects them back to the raw dict in seam args / ret.
 *    Callable doubles are {"__callable__": name} markers.
 *  - _drop_trailing_blank returns None live and mutates `lines`; the harness
 *    echoes the MUTATED lines array as ret (the oracle drive returns the same
 *    projection). strip_ansi is a seam; the port applies str.strip() to its
 *    result before the emptiness test.
 *  - The _ArgumentParser.error formatting lives inside argparse internals; a
 *    parse_args seam raise carries the already-formatted ConsoleCommandError
 *    message (the error method itself is a CLAIMED sibling, coe_error).
 *  - Integral floats normalize to ints on both sides via the C-serialize
 *    round-trip; the fixture corpus keeps integral values as ints.
 *
 * Seam name table (name -> live call):
 *   parser_new            _ArgumentParser(prog=, add_help=False)
 *   add_subparsers        parser.add_subparsers(dest="_console_command")
 *   add_argument          parser.add_argument(*flags, **kwargs)
 *   parse_args            parser.parse_args(args)
 *   add_parser            subparsers.add_parser(root)
 *   set_defaults          top_parser.set_defaults(func=)
 *   import_module         importlib.import_module(module)
 *   getattr               getattr(module, name)
 *   entry                 the surface builder/register callable (*args, **kwargs)
 *   _capture_output       claimed sibling _capture_output(fn)  args{fn:"<lambda>"}
 *   _expect_no_args       claimed sibling _expect_no_args(args, usage)
 *   _format_job           claimed sibling _format_job(job, action)
 *   _summaries_from_parser claimed sibling _summaries_from_parser(parser)
 *   strip_ansi            tools.ansi_strip.strip_ansi(line)
 *   profile_scope         persistence_assembly.profile_scope()
 *   open_persistence_reader persistence_assembly.open_persistence_reader(dir)
 *   open_persistence      persistence_assembly.open_persistence(scope)
 *   assert_exports_safe   hermes_cli.session_export.assert_exports_safe(store, ids)
 *   run                   the cron job action callback run(ref)
 *   namespace_update      the _dispatch namespace_update callback (namespace)
 *   func                  the _dispatch func(namespace) lambda body
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

/* ── universal seam plumbing ── */
typedef struct ce2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} ce2_seams_t;

static json_t *ce2_pending = NULL;  /* parked uncaught raise spec */

static bool jtruth(const json_t *v) {
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
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }

static char *xsf(const char *fmt, ...) {
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

static json_t *ce2_call(const ce2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static const char *ce2_raise_class(const json_t *r) {
    if (is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (is_str(c)) return c->str_val;
    }
    return NULL;
}
static void ce2_park(json_t *r) { ce2_pending = r; }
static json_t *ce2_take_pending(void) { json_t *r = ce2_pending; ce2_pending = NULL; return r; }
static json_t *ce2_make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static json_t *args0(void) { return json_object(); }
static void aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── FULL builtin exception hierarchy (py3.14) + module customs ──
 * Single-parent walk for `except Exception`. IOError / EnvironmentError /
 * WindowsError are OSError aliases (point at OSError, not their __bases__[0]);
 * ExceptionGroup's first direct base is BaseExceptionGroup but it is routed
 * through Exception to keep the `except Exception` contract. The BaseException
 * trio points ONLY at BaseException so it passes through `except Exception`. */
typedef struct ce2_exc_parent { const char *name; const char *parent; } ce2_exc_parent_t;
static const ce2_exc_parent_t CE2_EXC_TABLE[] = {
    {"AmbiguousJobReference", "LookupError"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BaseException", ""},
    {"BaseExceptionGroup", "BaseException"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"BufferError", "Exception"},
    {"BytesWarning", "Warning"},
    {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"ConsoleCommandError", "RuntimeError"},
    {"DeprecationWarning", "Warning"},
    {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"},
    {"EnvironmentError", "OSError"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"ExportTooLarge", "ValueError"},
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
    {"PersistenceError", "ValueError"},
    {"ProcessLookupError", "OSError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"},
    {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"},
    {"SystemError", "Exception"},
    {"SystemExit", "BaseException"},
    {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"},
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
#define CE2_EXC_N (sizeof(CE2_EXC_TABLE) / sizeof(CE2_EXC_TABLE[0]))

static const char *ce2_exc_parent(const char *name) {
    for (size_t i = 0; i < CE2_EXC_N; i++)
        if (!strcmp(CE2_EXC_TABLE[i].name, name)) return CE2_EXC_TABLE[i].parent;
    return NULL;
}
/* issubclass(kind, Exception): walk to "Exception" (true) vs BaseException/root
 * (false). Unknown kinds (no table entry) are treated as non-Exception. */
static bool ce2_exc_is_exception(const char *kind) {
    const char *cur = kind;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (!strcmp(cur, "Exception")) return true;
        if (!strcmp(cur, "BaseException") || !strcmp(cur, "object") || !cur[0]) return false;
        cur = ce2_exc_parent(cur);
    }
    return false;
}

/* Python str.strip() over the ASCII White_Space set. */
static char *strip_ws(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && (*b == ' ' || *b == '\t' || *b == '\n' || *b == '\r' || *b == '\v' || *b == '\f')) b++;
    const char *e = s + strlen(s);
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r' || e[-1] == '\v' || e[-1] == '\f')) e--;
    size_t n = (size_t)(e - b);
    char *out = malloc(n + 1);
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}
/* Python str.split() (no args): runs of ASCII whitespace, no empties. */
static json_t *split_ws(const char *s) {
    json_t *out = json_array();
    const char *p = s ? s : "";
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\v' || *p == '\f')) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\v' || *p == '\f')) p++;
        size_t n = (size_t)(p - start);
        char *tok = malloc(n + 1);
        memcpy(tok, start, n);
        tok[n] = '\0';
        json_append(out, json_string(tok));
        free(tok);
    }
    return out;
}
/* Pop the last element of a JSON array in place (free it, shrink count). */
static void ce2_arr_pop(json_t *arr) {
    if (!is_arr(arr) || arr->c.count == 0) return;
    size_t last = arr->c.count - 1;
    json_free(arr->c.items[last]);
    arr->c.items[last] = NULL;
    arr->c.count = last;
}

/* ── module constants ── */
static const char *CE2_SHELL_TOKENS[] = {"|", "||", "&", "&&", ";", ">", ">>", "<", "<<", "2>", "2>>"};
#define CE2_SHELL_N (sizeof(CE2_SHELL_TOKENS) / sizeof(CE2_SHELL_TOKENS[0]))
static const char *CE2_CONFIRMED_KINDS[] = {"extracted", "registered"};
static const char *CE2_BLOCKED_TOP[] = {
    "acp", "chat", "claw", "completion", "dashboard", "desktop", "fallback", "gateway",
    "gui", "login", "logout", "model", "moa", "oneshot", "proxy", "serve", "setup",
    "uninstall", "update", "whatsapp", "whatsapp-cloud"};
#define CE2_BLOCKED_TOP_N (sizeof(CE2_BLOCKED_TOP) / sizeof(CE2_BLOCKED_TOP[0]))
static const char *CE2_BLOCKED_PAIRS[][3] = {
    {"config", "edit", "`config edit` opens an editor and is not available in Hermes Console."},
    {"profile", "alias", "`profile alias` creates shell wrappers and is not available in Hermes Console."},
    {"skills", "config", "`skills config` is interactive and is not available in Hermes Console."},
    {"skills", "publish", "`skills publish` is not available in Hermes Console."},
    {"portal", "login", "`portal login` is interactive and is not available in Hermes Console."},
    {"portal", "open", "`portal open` opens a browser and is not available in Hermes Console."},
    {"kanban", "tail", "`kanban tail` streams output and is not available in Hermes Console."},
    {"kanban", "watch", "`kanban watch` streams output and is not available in Hermes Console."},
    {"kanban", "daemon", "`kanban daemon` starts a service and is not available in Hermes Console."},
    {"kanban", "dispatcher", "`kanban dispatcher` starts a worker and is not available in Hermes Console."},
    {"kanban", "swarm", "`kanban swarm` starts agent work and is not available in Hermes Console."},
    {"kanban", "decompose", "`kanban decompose` starts agent work and is not available in Hermes Console."},
    {"kanban", "specify", "`kanban specify` starts agent work and is not available in Hermes Console."},
    {"kanban", "gc", "`kanban gc` is not available in Hermes Console."},
    {"sessions", "delete", "`sessions delete` and `sessions prune` are not available in Hermes Console."},
    {"sessions", "prune", "`sessions delete` and `sessions prune` are not available in Hermes Console."},
};
#define CE2_BLOCKED_PAIRS_N (sizeof(CE2_BLOCKED_PAIRS) / sizeof(CE2_BLOCKED_PAIRS[0]))

/* PoP: ce2_shell_tokens @ hermes_cli/console_engine.py:_SHELL_TOKENS */
static bool ce2_shell_tokens(const char *token) {
    if (!token) return false;
    for (size_t i = 0; i < CE2_SHELL_N; i++) if (!strcmp(token, CE2_SHELL_TOKENS[i])) return true;
    return false;
}
/* PoP: ce2_confirmed_kinds @ hermes_cli/console_engine.py:_CONFIRMED_KINDS */
static bool ce2_confirmed_kinds(const char *kind) {
    if (!kind) return false;
    return !strcmp(kind, "extracted") || !strcmp(kind, "registered");
}
/* PoP: ce2_blocked_top @ hermes_cli/console_engine.py:_BLOCKED_TOP */
static bool ce2_blocked_top(const char *token) {
    if (!token) return false;
    for (size_t i = 0; i < CE2_BLOCKED_TOP_N; i++) if (!strcmp(token, CE2_BLOCKED_TOP[i])) return true;
    return false;
}
/* PoP: ce2_blocked_pairs @ hermes_cli/console_engine.py:_BLOCKED_PAIRS */
static char *ce2_blocked_pairs(const json_t *tokens) {
    const char *a = is_arr(tokens) && json_len(tokens) > 0 ? jstr(json_get(tokens, 0)) : NULL;
    const char *b = is_arr(tokens) && json_len(tokens) > 1 ? jstr(json_get(tokens, 1)) : NULL;
    if (a && b) {
        for (size_t i = 0; i < CE2_BLOCKED_PAIRS_N; i++)
            if (!strcmp(a, CE2_BLOCKED_PAIRS[i][0]) && !strcmp(b, CE2_BLOCKED_PAIRS[i][1]))
                return strdup(CE2_BLOCKED_PAIRS[i][2]);
    }
    return strdup("");
}

/* ── _paths ── */
/* PoP: ce2_paths @ hermes_cli/console_engine.py:_paths */
static json_t *ce2_paths(const char *spec) {
    json_t *out = json_array();
    json_t *items = json_array();
    /* [item.strip() for item in spec.split(",") if item.strip()] */
    const char *p = spec ? spec : "";
    while (1) {
        const char *comma = strchr(p, ',');
        size_t n = comma ? (size_t)(comma - p) : strlen(p);
        char *raw = malloc(n + 1);
        memcpy(raw, p, n);
        raw[n] = '\0';
        char *item = strip_ws(raw);
        free(raw);
        if (item[0]) json_append(items, json_string(item));
        free(item);
        if (!comma) break;
        p = comma + 1;
    }
    for (size_t i = 0; i < json_len(items); i++) {
        const char *item = jstr(json_get(items, i));
        bool mutating = item && item[0] == '*';
        const char *body = item ? item : "";
        while (*body == '*') body++;  /* lstrip("*") */
        json_t *path;
        if (!strcmp(body, ".")) path = json_array();
        else path = split_ws(body);
        json_t *row = json_object();
        json_set(row, "path", path);
        json_set(row, "mutating", json_bool(mutating));
        json_append(out, row);
    }
    json_free(items);
    return out;
}

/* ── _CliSurface record builders ── */
static json_t *ce2_surface_record(const char *kind, const char *module,
                                  const char *builder, const char *handler) {
    json_t *o = json_object();
    json_set(o, "kind", json_string(kind));
    json_set(o, "module", json_string(module));
    json_set(o, "builder", json_string(builder));
    json_set(o, "handler", handler ? json_string(handler) : json_null());
    return o;
}
/* PoP: ce2_sub @ hermes_cli/console_engine.py:_sub */
static json_t *ce2_sub(const char *module, const char *builder, const char *handler) {
    char *m = xsf("hermes_cli.subcommands.%s", module ? module : "");
    json_t *o = ce2_surface_record("extracted", m, builder, handler);
    free(m);
    return o;
}
/* PoP: ce2_reg @ hermes_cli/console_engine.py:_reg */
static json_t *ce2_reg(const char *module, const char *handler) {
    char *m = xsf("hermes_cli.%s", module ? module : "");
    json_t *o = ce2_surface_record("registered", m, "register_cli", handler);
    free(m);
    return o;
}
/* PoP: ce2_send_surface @ hermes_cli/console_engine.py:_SEND_SURFACE */
static json_t *ce2_send_surface(void) {
    return ce2_surface_record("adder", "hermes_cli.send_cmd", "register_send_subparser", NULL);
}

/* ── _drop_trailing_blank (mutates lines; strip_ansi seam) ── */
/* PoP: ce2_drop_trailing_blank @ hermes_cli/console_engine.py:_drop_trailing_blank */
static void ce2_drop_trailing_blank(json_t *lines, const ce2_seams_t *s) {
    while (is_arr(lines) && json_len(lines) > 0) {
        const json_t *last = json_get(lines, json_len(lines) - 1);
        const char *ls = jstr(last);
        json_t *a = args0();
        aset_s(a, "line", ls ? ls : "");
        json_t *r = ce2_call(s, "strip_ansi", a);
        json_free(a);
        if (ce2_raise_class(r)) { ce2_park(r); return; }
        char *stripped = strip_ws(jstr(r));
        json_free(r);
        bool empty = !stripped[0];
        free(stripped);
        if (!empty) break;
        ce2_arr_pop(lines);
    }
}

/* ── _parse (argparse flow) ── */
/* PoP: ce2_parse @ hermes_cli/console_engine.py:_parse */
static json_t *ce2_parse(const char *prog, const json_t *args, const json_t *specs,
                         const ce2_seams_t *s) {
    json_t *a = args0();
    aset_s(a, "prog", prog);
    json_set(a, "add_help", json_bool(false));
    json_t *r = ce2_call(s, "parser_new", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    json_free(r);
    if (is_arr(specs)) {
        for (size_t i = 0; i < json_len(specs); i++) {
            const json_t *spec = json_get(specs, i);
            json_t *flags = json_array();
            json_t *kwargs = json_object();
            const json_t *bare = is_obj(spec) ? json_obj_get(spec, "bare") : NULL;
            if (is_str(bare)) {
                json_append(flags, json_copy(bare));
            } else if (is_obj(spec)) {
                const json_t *fl = json_obj_get(spec, "flags");
                if (is_arr(fl)) for (size_t j = 0; j < json_len(fl); j++) json_append(flags, json_copy(json_get(fl, j)));
                const json_t *kw = json_obj_get(spec, "kwargs");
                if (is_obj(kw)) kwargs = json_copy(kw);
            }
            a = args0();
            json_set(a, "flags", flags);
            json_set(a, "kwargs", kwargs);
            r = ce2_call(s, "add_argument", a);
            json_free(a);
            if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
            json_free(r);
        }
    }
    a = args0();
    aset_j(a, "args", args);
    r = ce2_call(s, "parse_args", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    return r;
}

/* ── _captured factory + wrapper closure ── */
/* PoP: ce2_captured @ hermes_cli/console_engine.py:_captured */
static json_t *ce2_captured(const json_t *fn) {
    json_t *o = json_object();
    json_set(o, "__captured__", json_bool(true));
    json_set(o, "fn", fn ? json_copy(fn) : json_null());
    return o;
}
/* PoP: ce2_captured_wrapper @ hermes_cli/console_engine.py:_captured */
static json_t *ce2_captured_wrapper(const json_t *fn, const json_t *engine,
                                    const json_t *args, const ce2_seams_t *s) {
    (void)fn; (void)engine; (void)args;  /* captured; black-box seam never invokes */
    json_t *a = args0();
    aset_s(a, "fn", "<lambda>");
    json_t *r = ce2_call(s, "_capture_output", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    return r;
}

/* ── _simple_command factory + handler closure ── */
/* PoP: ce2_simple_command @ hermes_cli/console_engine.py:_simple_command */
static json_t *ce2_simple_command(const char *usage, const char *module, const char *name) {
    json_t *o = json_object();
    json_set(o, "__simple_command__", json_bool(true));
    aset_s(o, "usage", usage);
    aset_s(o, "module", module);
    aset_s(o, "name", name);
    return o;
}
/* PoP: ce2_simple_command_handler @ hermes_cli/console_engine.py:_simple_command */
static json_t *ce2_simple_command_handler(const char *usage, const char *module,
                                          const char *name, const json_t *args,
                                          const ce2_seams_t *s) {
    json_t *a = args0();
    aset_j(a, "args", args);
    aset_s(a, "usage", usage);
    json_t *r = ce2_call(s, "_expect_no_args", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    json_free(r);
    a = args0();
    aset_s(a, "module", module);
    r = ce2_call(s, "import_module", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    json_free(r);
    a = args0();
    aset_s(a, "module", module);
    aset_s(a, "name", name);
    r = ce2_call(s, "getattr", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    json_free(r);
    a = args0();
    aset_s(a, "fn", "<lambda>");
    r = ce2_call(s, "_capture_output", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    return r;
}
/* PoP: ce2_doctor @ hermes_cli/console_engine.py:_doctor */
static json_t *ce2_doctor(const json_t *args, const ce2_seams_t *s) {
    return ce2_simple_command_handler("doctor", "hermes_cli.doctor", "run_doctor", args, s);
}

/* ── _CliSurface.build main_handler lambda ── */
/* PoP: ce2_build_main_handler @ hermes_cli/console_engine.py:build */
static json_t *ce2_build_main_handler(const char *handler, bool live, const ce2_seams_t *s) {
    if (!live) {
        json_t *o = json_object();
        json_set(o, "__callable__", json_string("_noop_console_command"));
        return o;
    }
    json_t *a = args0();
    aset_s(a, "module", "hermes_cli.main");
    json_t *r = ce2_call(s, "import_module", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    json_free(r);
    a = args0();
    aset_s(a, "module", "hermes_cli.main");
    aset_s(a, "name", handler);
    r = ce2_call(s, "getattr", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    return r;
}

/* ── _CliSurface.build (method; composes main_handler REAL) ── */
/* PoP: ce2_surface_build @ hermes_cli/console_engine.py:build */
static json_t *ce2_surface_build(const json_t *surface, const char *root, bool live,
                                 const ce2_seams_t *s) {
    const char *kind = json_get_str(surface, "kind", "");
    const char *module = json_get_str(surface, "module", "");
    const char *builder = json_get_str(surface, "builder", "");
    const json_t *handler = json_obj_get(surface, "handler");
    const char *handler_s = jstr(handler);

    json_t *a = args0();
    aset_s(a, "prog", "hermes");
    json_set(a, "add_help", json_bool(false));
    json_t *parser = ce2_call(s, "parser_new", a);
    json_free(a);
    if (ce2_raise_class(parser)) { ce2_park(parser); return NULL; }

    a = args0();
    aset_s(a, "dest", "_console_command");
    json_t *subparsers = ce2_call(s, "add_subparsers", a);
    json_free(a);
    if (ce2_raise_class(subparsers)) { ce2_park(subparsers); json_free(parser); return NULL; }

    a = args0();
    aset_s(a, "module", module);
    json_t *r = ce2_call(s, "import_module", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); json_free(parser); json_free(subparsers); return NULL; }
    json_free(r);

    a = args0();
    aset_s(a, "module", module);
    aset_s(a, "name", builder);
    r = ce2_call(s, "getattr", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); json_free(parser); json_free(subparsers); return NULL; }
    json_free(r);

    if (!strcmp(kind, "extracted")) {
        json_t *mh = ce2_build_main_handler(handler_s, live, s);
        if (ce2_pending) { json_free(parser); json_free(subparsers); return NULL; }
        json_t *args_arr = json_array();
        json_append(args_arr, json_copy(subparsers));
        json_t *kwargs = json_object();
        json_set(kwargs, handler_s ? handler_s : "", mh);  /* owns mh */
        a = args0();
        json_set(a, "args", args_arr);
        json_set(a, "kwargs", kwargs);
        r = ce2_call(s, "entry", a);
        json_free(a);
        if (ce2_raise_class(r)) { ce2_park(r); json_free(parser); json_free(subparsers); return NULL; }
        json_free(r);
    } else if (!strcmp(kind, "registered")) {
        a = args0();
        aset_s(a, "root", root);
        json_t *top = ce2_call(s, "add_parser", a);
        json_free(a);
        if (ce2_raise_class(top)) { ce2_park(top); json_free(parser); json_free(subparsers); return NULL; }
        json_t *args_arr = json_array();
        json_append(args_arr, json_copy(top));
        a = args0();
        json_set(a, "args", args_arr);
        json_set(a, "kwargs", json_object());
        r = ce2_call(s, "entry", a);
        json_free(a);
        if (ce2_raise_class(r)) { ce2_park(r); json_free(parser); json_free(subparsers); json_free(top); return NULL; }
        json_free(r);
        if (live && jpresent(handler)) {
            a = args0();
            aset_s(a, "module", module);
            aset_s(a, "name", handler_s);
            json_t *h = ce2_call(s, "getattr", a);
            json_free(a);
            if (ce2_raise_class(h)) { ce2_park(h); json_free(parser); json_free(subparsers); json_free(top); return NULL; }
            a = args0();
            json_set(a, "func", h);  /* owns h */
            r = ce2_call(s, "set_defaults", a);
            json_free(a);
            if (ce2_raise_class(r)) { ce2_park(r); json_free(parser); json_free(subparsers); json_free(top); return NULL; }
            json_free(r);
        }
        json_free(top);
    } else if (!strcmp(kind, "builder")) {
        json_t *args_arr = json_array();
        json_append(args_arr, json_copy(subparsers));
        a = args0();
        json_set(a, "args", args_arr);
        json_set(a, "kwargs", json_object());
        json_t *top = ce2_call(s, "entry", a);
        json_free(a);
        if (ce2_raise_class(top)) { ce2_park(top); json_free(parser); json_free(subparsers); return NULL; }
        if (live) {
            json_t *mh = ce2_build_main_handler(handler_s, true, s);
            if (ce2_pending) { json_free(parser); json_free(subparsers); json_free(top); return NULL; }
            a = args0();
            json_set(a, "func", mh);  /* owns mh */
            r = ce2_call(s, "set_defaults", a);
            json_free(a);
            if (ce2_raise_class(r)) { ce2_park(r); json_free(parser); json_free(subparsers); json_free(top); return NULL; }
            json_free(r);
        }
        json_free(top);
    } else {  /* adder */
        json_t *args_arr = json_array();
        json_append(args_arr, json_copy(subparsers));
        a = args0();
        json_set(a, "args", args_arr);
        json_set(a, "kwargs", json_object());
        r = ce2_call(s, "entry", a);
        json_free(a);
        if (ce2_raise_class(r)) { ce2_park(r); json_free(parser); json_free(subparsers); return NULL; }
        json_free(r);
    }
    json_free(subparsers);
    return parser;
}

/* ── _surface_summaries (composes build REAL; except Exception) ── */
/* PoP: ce2_surface_summaries @ hermes_cli/console_engine.py:_surface_summaries */
static json_t *ce2_surface_summaries(const json_t *surface, const char *root,
                                     const ce2_seams_t *s) {
    json_t *parser = ce2_surface_build(surface, root, false, s);
    if (ce2_pending) {
        json_t *r = ce2_take_pending();
        if (ce2_exc_is_exception(ce2_raise_class(r))) { json_free(r); return json_object(); }
        ce2_park(r);
        return NULL;
    }
    json_t *a = args0();
    aset_j(a, "parser", parser);
    json_free(parser);
    json_t *r = ce2_call(s, "_summaries_from_parser", a);
    json_free(a);
    if (ce2_raise_class(r)) {
        if (ce2_exc_is_exception(ce2_raise_class(r))) { json_free(r); return json_object(); }
        ce2_park(r);
        return NULL;
    }
    return r;
}

/* ── _dispatch inner lambda (func(namespace)) ── */
/* PoP: ce2_dispatch_lambda @ hermes_cli/console_engine.py:_dispatch */
static json_t *ce2_dispatch_lambda(const json_t *func, const json_t *namespace,
                                   const ce2_seams_t *s) {
    (void)func;  /* the callable double; identity not observable through the seam */
    json_t *a = args0();
    aset_j(a, "namespace", namespace);
    json_t *r = ce2_call(s, "func", a);
    json_free(a);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    return r;
}

/* ── _dispatch (composes surface_build REAL) ── */
/* PoP: ce2_dispatch @ hermes_cli/console_engine.py:_dispatch */
static json_t *ce2_dispatch(const json_t *surface, const char *root, const json_t *fixed,
                            const json_t *args, bool nu_present, const ce2_seams_t *s) {
    json_t *parser = ce2_surface_build(surface, root, true, s);
    if (ce2_pending) return NULL;
    json_free(parser);

    json_t *all = json_array();
    json_append(all, json_string(root ? root : ""));
    if (is_arr(fixed)) for (size_t i = 0; i < json_len(fixed); i++) json_append(all, json_copy(json_get(fixed, i)));
    if (is_arr(args)) for (size_t i = 0; i < json_len(args); i++) json_append(all, json_copy(json_get(args, i)));
    json_t *a = args0();
    json_set(a, "args", all);
    json_t *ns = ce2_call(s, "parse_args", a);
    json_free(a);
    if (ce2_raise_class(ns)) { ce2_park(ns); return NULL; }

    if (nu_present) {
        a = args0();
        aset_j(a, "namespace", ns);
        json_t *r = ce2_call(s, "namespace_update", a);
        json_free(a);
        if (ce2_raise_class(r)) { ce2_park(r); json_free(ns); return NULL; }
        json_free(r);
    }
    const json_t *func = is_obj(ns) ? json_obj_get(ns, "func") : NULL;
    bool callable = is_obj(func) && json_has(func, "__callable__");
    if (!callable) {
        json_free(ns);
        ce2_park(ce2_make_raise("ConsoleCommandError",
                                "No handler is available for that console command."));
        return NULL;
    }
    a = args0();
    aset_s(a, "fn", "<lambda>");
    json_t *r = ce2_call(s, "_capture_output", a);
    json_free(a);
    json_free(ns);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    return r;
}

/* ── _session_db ── */
/* PoP: ce2_session_db @ hermes_cli/console_engine.py:_session_db */
static json_t *ce2_session_db(bool read_only, const ce2_seams_t *s) {
    json_t *a = args0();
    json_t *scope = ce2_call(s, "profile_scope", a);
    json_free(a);
    if (ce2_raise_class(scope)) { ce2_park(scope); return NULL; }
    json_t *r;
    if (read_only) {
        const json_t *pd = is_obj(scope) ? json_obj_get(scope, "profile_dir") : NULL;
        a = args0();
        aset_s(a, "profile_dir", jstr(pd));
        r = ce2_call(s, "open_persistence_reader", a);
        json_free(a);
    } else {
        a = args0();
        aset_j(a, "scope", scope);
        r = ce2_call(s, "open_persistence", a);
        json_free(a);
    }
    json_free(scope);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    return r;
}

/* ── _guard_exports (except ExportTooLarge -> ConsoleCommandError) ── */
/* PoP: ce2_guard_exports @ hermes_cli/console_engine.py:_guard_exports */
static void ce2_guard_exports(const json_t *store, const json_t *session_ids,
                              const ce2_seams_t *s) {
    json_t *a = args0();
    aset_j(a, "store", store);
    aset_j(a, "session_ids", session_ids);
    json_t *r = ce2_call(s, "assert_exports_safe", a);
    json_free(a);
    const char *kc = ce2_raise_class(r);
    if (kc) {
        if (!strcmp(kc, "ExportTooLarge")) {
            char *msg = strdup(json_get_str(r, "msg", ""));
            json_free(r);
            ce2_park(ce2_make_raise("ConsoleCommandError", msg));
            free(msg);
            return;
        }
        ce2_park(r);
        return;
    }
    json_free(r);
}

/* ── _cron_job_action (run callback + _format_job seam) ── */
/* PoP: ce2_cron_job_action @ hermes_cli/console_engine.py:_cron_job_action */
static json_t *ce2_cron_job_action(const json_t *args, const char *usage,
                                   const char *action, const ce2_seams_t *s) {
    if (!is_arr(args) || json_len(args) != 1) {
        char *m = xsf("Usage: %s", usage ? usage : "");
        ce2_park(ce2_make_raise("ConsoleCommandError", m));
        free(m);
        return NULL;
    }
    const char *ref = jstr(json_get(args, 0));
    json_t *a = args0();
    aset_s(a, "ref", ref);
    json_t *job = ce2_call(s, "run", a);
    json_free(a);
    const char *kc = ce2_raise_class(job);
    if (kc) {
        if (!strcmp(kc, "AmbiguousJobReference")) {
            char *msg = strdup(json_get_str(job, "msg", ""));
            json_free(job);
            ce2_park(ce2_make_raise("ConsoleCommandError", msg));
            free(msg);
            return NULL;
        }
        ce2_park(job);
        return NULL;
    }
    if (!jtruth(job)) {
        json_free(job);
        char *m = xsf("Job not found: %s", ref ? ref : "");
        ce2_park(ce2_make_raise("ConsoleCommandError", m));
        free(m);
        return NULL;
    }
    a = args0();
    aset_j(a, "job", job);
    aset_s(a, "action", action);
    json_t *r = ce2_call(s, "_format_job", a);
    json_free(a);
    json_free(job);
    if (ce2_raise_class(r)) { ce2_park(r); return NULL; }
    return r;
}