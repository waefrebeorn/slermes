/*
 * backup_flow4.c -- C11 FLOW port of the last UNCLAIMED defs of
 * hermes_cli/backup.py beyond backup_flow2 / backup_flow3.
 *
 * CRLF-aware PoP grep at authoring time (AST, class-qualified, over all of
 * src/): 64 defs / 63 unique names; backup.c + backup_gaps.c + backup_flow2.c
 * + backup_flow3.c carry inline PoP markers for 62 of them. The ONLY defs
 * with no PoP marker are the two nested `_lock_op` closures inside
 * `_backup_operation_lock` (quarry backup.py lines 202 and 208) -- the
 * module is therefore NOT fully claimed, so this FLOW4 port is authored.
 * The two closures differ only by platform branch; they are driven here as
 * one C core parameterised by `os` ("nt" / "posix"), matching the live Python
 * where `_backup_operation_lock` binds exactly one of them at runtime.
 *
 * All cores are `static bk4_*`, driven by tests/t_port_backup_flow4.c.
 * PoP annotations (defining module hermes_cli/backup.py):
 *   PoP: lock_op @ hermes_cli/backup.py:_lock_op
 *
 * Universal seam: every effectful collaborator is a named seam; a
 * {"__raise__":cls,"msg":m} object is a raised exception; a MISSING script
 * fail-closes with RuntimeError; a present null is a valid None; a JSON array
 * script yields one result PER CALL (last repeats). Full py3.14 builtin
 * exception parent hierarchy in bk4_exc_parent/bk4_caught (BaseException trio
 * never swallowed by `except Exception`). No raise spec is swallowed here:
 * `_lock_op` has no `except`, so the first raising seam propagates verbatim.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"

/* ---- trace + seam plumbing ---- */
typedef struct bk4_trace { json_t *events; } bk4_trace_t;
typedef struct bk4_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} bk4_seams_t;
static void bk4_trace_add(bk4_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void bk4_trace_free(bk4_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *bk4_seam(const bk4_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}

/* ---- json helpers ---- */
static bool bk4_jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static const char *bk4_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool bk4_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool bk4_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool bk4_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static size_t bk4_len(const json_t *v) { return (bk4_is_arr(v) || bk4_is_obj(v)) ? v->c.count : 0; }
static const json_t *bk4_at(const json_t *v, size_t i) {
    return (bk4_is_arr(v) && i < v->c.count) ? v->c.items[i] : NULL;
}
static char *bk4_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}

/* ---- exception model (FULL builtin parent hierarchy, py3.14) ---- */
static const char *bk4_raise_class(const json_t *r) {
    if (bk4_is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool bk4_is_raise(const json_t *r) { return bk4_raise_class(r) != NULL; }
static const char *bk4_raise_msg(const json_t *r) {
    if (!bk4_is_obj(r)) return "";
    const char *m = bk4_jstr(json_obj_get(r, "msg"));
    return m ? m : "";
}
static json_t *bk4_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool bk4_in_names(const char *k, const char *const *names) {
    if (!k) return false;
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
static const char *bk4_exc_parent(const char *k) {
    if (!k) return NULL;
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
    if (bk4_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = { "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL };
    if (bk4_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (bk4_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (bk4_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (bk4_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (bk4_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError", NULL };
    if (bk4_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (bk4_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (bk4_in_names(k, WARNING_CHILDREN)) return "Warning";
    if (!strcmp(k, "UnboundLocalError")) return "NameError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    if (!strcmp(k, "PythonFinalizationError")) return "RuntimeError";
    if (!strcmp(k, "BadZipFile")) return "Exception";
    if (!strcmp(k, "zlib.error") || !strcmp(k, "LZMAError")) return "Exception";
    if (!strcmp(k, "_SQLiteSnapshotError")) return "Exception";
    if (!strcmp(k, "BackupInProgressError")) return "RuntimeError";
    return "Exception";
}
static bool bk4_caught(const json_t *r, const char *cls) {
    const char *k = bk4_raise_class(r);
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) { if (!strcmp(k, cls)) return true; k = bk4_exc_parent(k); }
    return false;
}
static bool bk4_is_exception(const json_t *r) { return bk4_caught(r, "Exception"); }

/* ---- arg builders ---- */
static void bk4_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void bk4_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void bk4_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void bk4_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ══════════════ 1. _lock_op (nested in _backup_operation_lock) ══════════════ */
/* PoP: lock_op @ hermes_cli/backup.py:_lock_op */
/*
 * Live reference (quarry backup.py):
 *   if os.name == "nt":                      # lines 197-205
 *       ...
 *       def _lock_op(flag):
 *           handle.seek(0)
 *           msvcrt.locking(handle.fileno(), flag, 1)
 *       lock_flag, unlock_flag = msvcrt.LK_NBLCK, msvcrt.LK_UNLCK
 *   else:                                    # lines 206-210
 *       ...
 *       def _lock_op(flag):
 *           fcntl.flock(handle.fileno(), flag)
 *       lock_flag, unlock_flag = fcntl.LOCK_EX | fcntl.LOCK_NB, fcntl.LOCK_UN
 *
 * Both branches are pure effect + propagate: `_lock_op` has no `except`, so
 * the first seam that raises a spec is returned as the result unchanged (the
 * live drive records that as the raised exception). Order of calls matters:
 *   nt    : handle.seek(0) -> handle.fileno() -> msvcrt.locking(fd,flag,1)
 *   posix : handle.fileno() -> fcntl.flock(fd,flag)
 * The `fd` passed to the lock call is the EXACT value returned by handle.fileno
 * (preserving JSON type: number / null / object), matching Python argument
 * evaluation (fileno() is evaluated while building the outer call's args).
 */
static json_t *bk4_lock_op(const bk4_seams_t *s, const json_t *inp) {
    const char *osname = bk4_jstr(json_obj_get(inp, "os"));
    bool nt = osname && !strcmp(osname, "nt");
    double flag = json_get_num(inp, "flag", 0);
    json_t *fd = NULL;
    json_t *r = NULL;

    if (nt) {
        /* handle.seek(0) -- return value ignored by the live def. */
        json_t *a = json_object(); bk4_aset_n(a, "offset", 0);
        r = bk4_seam(s, "handle.seek", a); json_free(a);
        if (bk4_is_raise(r)) return r;
        if (fd) json_free(fd);
        /* handle.fileno() -- value flows into the outer lock call. */
        a = json_object();
        fd = bk4_seam(s, "handle.fileno", a); json_free(a);
        if (bk4_is_raise(fd)) return fd;
        /* msvcrt.locking(fd, flag, 1) */
        a = json_object();
        bk4_aset_j(a, "fd", fd);
        bk4_aset_n(a, "flag", flag);
        bk4_aset_n(a, "nbytes", 1);
        r = bk4_seam(s, "msvcrt.locking", a); json_free(a);
        if (fd) { json_free(fd); fd = NULL; }
        if (bk4_is_raise(r)) return r;
    } else {
        /* handle.fileno() -- value flows into the outer lock call. */
        json_t *a = json_object();
        fd = bk4_seam(s, "handle.fileno", a); json_free(a);
        if (bk4_is_raise(fd)) return fd;
        /* fcntl.flock(fd, flag) */
        a = json_object();
        bk4_aset_j(a, "fd", fd);
        bk4_aset_n(a, "flag", flag);
        r = bk4_seam(s, "fcntl.flock", a); json_free(a);
        if (fd) { json_free(fd); fd = NULL; }
        if (bk4_is_raise(r)) return r;
    }
    /* _lock_op returns None on success. */
    return json_null();
}
