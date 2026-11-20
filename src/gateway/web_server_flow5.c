/*
 * web_server_flow5.c — FLOW port of the FINAL UNCLAIMED surface of
 * hermes_cli/web_server.py (LIVE authority). Self-contained: libjson only. All
 * functions static ws5_*; the harness (#include "gateway/web_server_flow5.c")
 * wires the universal seam to scripted results and records the ordered trace.
 *
 * PoP GREP (CRLF-aware, tree-wide src/include/lib): of the 50 defs in
 * hermes_cli/web_server.py, 49 carry a PoP annotation crediting that module.
 * The single UNCLAIMED def is the async self-test driver:
 *   _dashboard_selftest_loop  (L615)  -> static ws5_dashboard_selftest_loop
 * The watercup gate is real__..._dashboard_selftest_loop (unported). Its
 * sibling _dashboard_selftest_once is CLAIMED (port__ gate) and is SEAMED here;
 * DASHBOARD_HEALTH.record_selftest (claimed in web_server_flow.c) is not
 * reached because the claimed sibling owns it.
 *
 * LIVE body (verbatim):
 *   async def _dashboard_selftest_loop() -> None:
 *       try:
 *           import httpx
 *       except ImportError:
 *           _log.debug("httpx unavailable — dashboard self-test disabled")
 *           return
 *       while True:
 *           await asyncio.sleep(_DASHBOARD_SELFTEST_INTERVAL_SECONDS)
 *           if getattr(app.state, "auth_required", False):
 *               continue
 *           await _dashboard_selftest_once()
 *
 * SEAM DOCTRINE: every live effect enters through ONE universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returning a NEW json_t* (NULL == Python None). A raising live call is
 * scripted {"__raise__": "<ExcClass>", "msg": ...}; the core propagates it
 * exactly where the live try/except catches it. `except ImportError` catches
 * the WHOLE ImportError family (ModuleNotFoundError -> ImportError -> Exception)
 * via the FULL builtin parent table below; it does NOT catch KeyboardInterrupt
 * / SystemExit / GeneratorExit / BaseException (ws5_is_base_exc passthrough).
 * A MISSING seam key fail-closes: the harness delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None.
 *
 * LIVE/DETERMINISM NOTE: the live loop is `while True`. A fixture terminates
 * it by scripting the asyncio_sleep seam to raise a control-flow exception
 * (_LoopBreak) on a chosen call index (array script, last value repeats). The
 * raise propagates out of the coroutine exactly as it does in live Python, so
 * the harness ret envelope is {"__raise__":"_LoopBreak",...} on both sides.
 *
 * Seam name table:
 *   import_httpx            import httpx (in-function import) {} -> module marker
 *   asyncio_sleep           await asyncio.sleep(interval) {seconds:60.0}
 *   dashboard_selftest_once await _dashboard_selftest_once() {} (claimed sibling)
 *
 * Module constant (live-seeded in the oracle, hardcoded here):
 *   _DASHBOARD_SELFTEST_INTERVAL_SECONDS = 60.0  (web_server L596)
 *
 * Fixture domains (documented exclusions):
 * - ASCII strings; app_state is a JSON object whose "auth_required" value is
 *   read with getattr-default-False semantics (absent/present-null/empty are
 *   all falsy, jtruth matches Python truthiness for scalars/containers).
 * - import_httpx success scripted null (module object unused by the live body).
 * - _LoopBreak carry an int-like "msg"; other raise specs carry "msg".
 * - The silent `_log.debug` emits NO seam event (RecLogger); only its argument
 *   evaluation (none here) could fire traces.
 *
 * Ownership: seam results and returned json_t pointers are malloc'd, caller
 * frees; inputs are borrowed. ws5_call borrows args (caller frees). A
 * propagated raise dict is returned upward as-is; a caught raise dict is
 * freed at the catch site.
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

typedef struct ws5_trace { json_t *events; } ws5_trace_t;

typedef struct ws5_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} ws5_seams_t;

static void ws5_trace_add(ws5_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void ws5_trace_free(ws5_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *ws5_call(const ws5_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *ws5_call0(const ws5_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = ws5_call(s, name, a);
    json_free(a);
    return r;
}

/* ── JSON value helpers ── */

static bool ws5_jtruth(const json_t *v) {
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
static bool ws5_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ws5_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

static char *ws5_xsf(const char *fmt, ...) {
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

/* ── exception model (FULL builtin parent hierarchy) ── */

static const char *ws5_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool ws5_is_raise(const json_t *r) { return ws5_raise_class(r) != NULL; }

static json_t *ws5_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static bool ws5_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
/* Full CPython 3.14 builtin exception parent table. BaseException trio stays
 * on the BaseException branch ONLY so `except Exception` cannot catch it. */
static const char *ws5_exc_parent(const char *k) {
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
    if (ws5_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (ws5_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (ws5_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (ws5_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (ws5_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (ws5_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = {
        "NotImplementedError", "RecursionError", "PythonFinalizationError", NULL
    };
    if (ws5_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "_IncompleteInputError")) return "SyntaxError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (ws5_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (ws5_in_names(k, WARNING_CHILDREN)) return "Warning";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (ws5_in_names(k, NAME_CHILDREN)) return "NameError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    return "Exception";  /* unknown custom kind: oracle bases it on Exception */
}
static bool ws5_caught(const json_t *r, const char *cls) {
    const char *k = ws5_raise_class(r);
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = ws5_exc_parent(k);
    }
    return false;
}
/* BaseException trio passes through `except Exception` (none here, but kept for
 * the exact ImportError-vs-BaseException distinction). */
static bool ws5_is_base_exc(const json_t *r) {
    const char *k = ws5_raise_class(r);
    return k && (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
                 !strcmp(k, "GeneratorExit") || !strcmp(k, "BaseException"));
}

/* ── the ported flow ── */

/* PoP: ws5_dashboard_selftest_loop @ hermes_cli/web_server.py:_dashboard_selftest_loop */
static json_t *ws5_dashboard_selftest_loop(const ws5_seams_t *s, const json_t *app_state) {
    /* try: import httpx */
    json_t *imp = ws5_call0(s, "import_httpx");
    if (ws5_is_raise(imp)) {
        if (ws5_caught(imp, "ImportError")) {
            /* except ImportError: _log.debug(...); return  (logger is silent) */
            json_free(imp);
            return NULL;
        }
        return imp;  /* non-ImportError (incl. BaseException trio) propagates */
    }
    json_free(imp);  /* import success: module object is unused by the live body */

    double interval = 60.0;  /* _DASHBOARD_SELFTEST_INTERVAL_SECONDS */
    while (1) {
        /* await asyncio.sleep(_DASHBOARD_SELFTEST_INTERVAL_SECONDS) */
        json_t *a = json_object();
        json_set(a, "seconds", json_number(interval));
        json_t *slp = ws5_call(s, "asyncio_sleep", a);
        json_free(a);
        if (ws5_is_raise(slp)) return slp;  /* _LoopBreak / propagating raise */
        json_free(slp);

        /* if getattr(app.state, "auth_required", False): continue */
        const json_t *ar = json_obj_get(app_state, "auth_required");
        if (ws5_jtruth(ar)) continue;

        /* await _dashboard_selftest_once()  (claimed sibling, SEAMED) */
        json_t *once = ws5_call0(s, "dashboard_selftest_once");
        if (ws5_is_raise(once)) return once;
        json_free(once);
    }
    /* unreachable */
}
