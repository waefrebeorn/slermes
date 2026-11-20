/*
 * terminal_tool_flow4.c — tools/terminal_tool.py final UNCLAIMED-remainder FLOW core.
 *
 * A CRLF-aware PoP scan over src/include/lib (errors="replace") reports every
 * tools/terminal_tool.py def claimed EXCEPT `_Rejected.__init__` (the 3
 * _SessionScope derived properties and _docker_session_isolation_enabled are
 * claimed by terminal_tool_flow3.c with QUALIFIED PoP names). This port closes
 * that last gap: the `_Rejected(Exception)` carrier whose __init__ does
 *   super().__init__(result_json); self.result_json = result_json
 * so a finished tool-result JSON string can be raised out of the planning /
 * guard helpers and caught as a normal Exception at the caller.
 *
 * FLOW projection: the exception object is projected to
 *   {"args":[result_json], "str":result_json, "result_json":result_json}
 * and the exception is dispatched through an ordered chain of `except`
 * clauses (catch_dispatch) whose ordered per-clause isinstance checks are the
 * flow trace. isinstance walks the FULL builtin exception hierarchy encoded
 * in TT4_EXC_PARENTS (py3.14 builtins + the _Rejected -> Exception edge); an
 * unknown name defaults to an Exception subclass, mirroring the oracle's
 * make_exc. SystemExit / KeyboardInterrupt / GeneratorExit / the groups and
 * CancelledError stay outside Exception; every other builtin walks to it.
 *
 * Self-contained: libjson only. All helpers static tt4_*.
 *
 * Modeling notes (differential oracle tests/oracle_terminal_tool_flow4.py):
 *  - result_json is a JSON string in the fixture domain (the real carrier is
 *    a json.dumps envelope); non-string result_json is out of domain.
 *  - No live I/O: the port is pure; the harness needs no seam scripts.
 *  - str(e) == result_json and args == (result_json,) follow directly from
 *    Exception.__init__ with a single string positional.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "libjson/json.h"

/* ── exception-parent table (FULL builtin hierarchy, py3.14) ── */
typedef struct { const char *kind; const char *parent; } tt4_exc_ent;
static const tt4_exc_ent TT4_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"AttributeError", "Exception"},
    {"NameError", "Exception"},
    {"LookupError", "Exception"},
    {"ArithmeticError", "Exception"},
    {"OSError", "Exception"},
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"WindowsError", "OSError"},
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
    {"_IncompleteInputError", "SyntaxError"},
    {"PythonFinalizationError", "RuntimeError"},
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

static const char *tt4_exc_parent(const char *kind) {
    for (const tt4_exc_ent *e = TT4_EXC_PARENTS; e->kind; e++)
        if (strcmp(e->kind, kind) == 0) return e->parent;
    if (strcmp(kind, "BaseException") == 0) return NULL;
    return "Exception";   /* oracle make_exc: unknown name -> Exception subclass */
}

/* Full-hierarchy walk: is `kind` the same as or a descendant of `base`? */
static bool tt4_exc_is(const char *kind, const char *base) {
    const char *k = kind;
    for (int depth = 0; k && depth < 32; depth++) {
        if (strcmp(k, base) == 0) return true;
        k = tt4_exc_parent(k);
    }
    return false;
}

/* ── Python value helpers ── */
static const char *tt4_strval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/*
 * PoP: tt4_rejected_init @ tools/terminal_tool.py:_Rejected.__init__
 * Direct port: super().__init__(result_json); self.result_json = result_json.
 * Projected as the exception's observable construction state.
 */
static json_t *tt4_rejected_init(const char *result_json) {
    json_t *r = json_object();
    json_t *args = json_array();
    const char *s = result_json ? result_json : "";
    json_append(args, json_string(s));
    json_set(r, "args", args);
    json_set(r, "str", json_string(s));
    json_set(r, "result_json", json_string(s));
    return r;
}

/* isinstance(_Rejected(result_json), kind) -> bool, full hierarchy walk. */
static json_t *tt4_isinstance(const char *kind) {
    return json_bool(tt4_exc_is("_Rejected", kind));
}

/* Project the membership map for the caller-supplied name list. */
static json_t *tt4_hierarchy(const json_t *names) {
    json_t *r = json_object();
    if (names && names->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(names); i++) {
            const json_t *n = json_get(names, i);
            const char *name = tt4_strval(n);
            json_set(r, name, json_bool(tt4_exc_is("_Rejected", name)));
        }
    }
    return r;
}

/*
 * FLOW: raise _Rejected(result_json) and run it down an ordered chain of
 * except clauses. Each clause emits {name, matched} into trace; the first
 * match becomes caught_by (null when none matches -> BaseException passthrough).
 */
static json_t *tt4_catch(const char *result_json, const json_t *clauses, json_t *trace) {
    (void)result_json;   /* the carrier value does not affect class identity */
    json_t *r = json_object();
    const char *caught = NULL;
    if (clauses && clauses->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(clauses); i++) {
            const char *name = tt4_strval(json_get(clauses, i));
            bool matched = tt4_exc_is("_Rejected", name);
            json_t *e = json_object();
            json_set(e, "name", json_string(name));
            json_set(e, "matched", json_bool(matched));
            json_append(trace, e);
            if (matched && !caught) caught = name;
        }
    }
    json_set(r, "caught_by", caught ? json_string(caught) : json_null());
    return r;
}
