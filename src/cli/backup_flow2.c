/*
 * backup_flow2.c — C11 FLOW port of the STILL-UNCLAIMED remainder of
 * hermes_cli/backup.py (CRLF-aware PoP grep at authoring time: backup.c and
 * backup_gaps.c already claim 33 functions; the 14 below carry NO PoP marker
 * in src/ and are therefore unclaimed).
 *
 * Ported FLOW cores (all `static bk2_*`, driven by tests/t_port_backup_flow2.c):
 *   _snapshot_recovery_hint            -> bk2_snapshot_recovery_hint
 *   _print_capped                      -> bk2_print_capped
 *   _import_skipped                    -> bk2_import_skipped
 *   _import_member_rel                 -> bk2_import_member_rel
 *   _newest_first / _prune_oldest      -> (inline siblings of prune_*)
 *   _prune_prefixed_zips               -> bk2_prune_prefixed_zips
 *   _prune_incomplete_zips             -> bk2_prune_incomplete_zips
 *   _write_zip_file                    -> bk2_write_zip_file
 *   _find_corrupt_members              -> bk2_find_corrupt_members
 *   _snapshot_epochs_restorable        -> bk2_snapshot_epochs_restorable
 *   create_pre_update_snapshots_all_profiles -> bk2_create_pre_update_snapshots_all_profiles
 *   restore_config_model_settings_all_profiles -> bk2_restore_config_model_settings_all_profiles
 *   restore_cron_jobs_all_profiles     -> bk2_restore_cron_jobs_all_profiles
 *
 * PoP annotations (defining module hermes_cli/backup.py):
 *   PoP: snapshot_recovery_hint  @ hermes_cli/backup.py:_snapshot_recovery_hint
 *   PoP: print_capped            @ hermes_cli/backup.py:_print_capped
 *   PoP: import_skipped          @ hermes_cli/backup.py:_import_skipped
 *   PoP: import_member_rel       @ hermes_cli/backup.py:_import_member_rel
 *   PoP: prune_prefixed_zips     @ hermes_cli/backup.py:_prune_prefixed_zips
 *   PoP: prune_incomplete_zips   @ hermes_cli/backup.py:_prune_incomplete_zips
 *   PoP: write_zip_file          @ hermes_cli/backup.py:_write_zip_file
 *   PoP: find_corrupt_members    @ hermes_cli/backup.py:_find_corrupt_members
 *   PoP: snapshot_epochs_restorable @ hermes_cli/backup.py:_snapshot_epochs_restorable
 *   PoP: create_pre_update_snapshots_all_profiles @ hermes_cli/backup.py:create_pre_update_snapshots_all_profiles
 *   PoP: restore_config_model_settings_all_profiles @ hermes_cli/backup.py:restore_config_model_settings_all_profiles
 *   PoP: restore_cron_jobs_all_profiles @ hermes_cli/backup.py:restore_cron_jobs_all_profiles
 *
 * ── Universal seam / trace ──
 * Every effectful collaborator is a seam named in tests/oracle_backup_flow2.py.
 * A seam result is a JSON value; a {\"__raise__\":cls,\"msg\":m} object is a
 * raised exception (the harness records it in the trace BEFORE the C checks
 * it). A MISSING seam script fail-closes with RuntimeError on both sides; a
 * present JSON null is a valid None. An array script is one result PER CALL.
 *
 * ── Full builtin exception hierarchy ──
 * bk2_exc_parent mirrors py3.14's builtin parent chain. bk2_caught walks it, so
 * `except Exception` swallows the whole Exception family but NEVER the
 * BaseException trio (SystemExit / KeyboardInterrupt / GeneratorExit). The
 * shadow mirror in the oracle uses the same table.
 *
 * ── Fixture-domain projections (documented, deterministic) ──
 * - Path objects project to strings; directory entries are
 *   {\"name\":str,\"is_file\":bool}. pathlib suffix = last dot after index 0;
 *   name comparisons are ASCII (fixture DATA is ASCII).
 * - sorted(reverse=True) uses byte order == Python codepoint order for ASCII.
 * - zipfile read bytes project to strings; a falsy (null/"") read is EOF.
 * - The exception `str(exc)` is the raise descriptor's `msg`.
 * - integral floats are emitted as ints; no integral-float literals in
 *   type-sensitive positions.
 * - Ownership: seam results and returned json_t* are owned (caller frees);
 *   inputs are borrowed; bk2_seam borrows its args node.
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

/* ── trace + seam plumbing ── */

typedef struct bk2_trace { json_t *events; } bk2_trace_t;

typedef struct bk2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} bk2_seams_t;

static void bk2_trace_add(bk2_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void bk2_trace_free(bk2_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *bk2_seam(const bk2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}

/* ── JSON value helpers ── */

static bool bk2_jtruth(const json_t *v) {
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
static const char *bk2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool bk2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool bk2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool bk2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static size_t bk2_len(const json_t *v) { return bk2_is_arr(v) || bk2_is_obj(v) ? v->c.count : 0; }
static const json_t *bk2_at(const json_t *v, size_t i) {
    return (bk2_is_arr(v) && i < v->c.count) ? v->c.items[i] : NULL;
}

static char *bk2_xsf(const char *fmt, ...) {
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
static char *bk2_concat(const char *a, const char *b) {
    return bk2_xsf("%s%s", a ? a : "", b ? b : "");
}

/* ── exception model (FULL builtin parent hierarchy, py3.14) ── */

static const char *bk2_raise_class(const json_t *r) {
    if (bk2_is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool bk2_is_raise(const json_t *r) { return bk2_raise_class(r) != NULL; }
static const char *bk2_raise_msg(const json_t *r) {
    if (!bk2_is_obj(r)) return "";
    const char *m = bk2_jstr(json_obj_get(r, "msg"));
    return m ? m : "";
}
static json_t *bk2_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool bk2_in_names(const char *k, const char *const *names) {
    if (!k) return false;
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
static const char *bk2_exc_parent(const char *k) {
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
    if (bk2_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (bk2_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (bk2_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (bk2_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (bk2_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (bk2_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError", NULL };
    if (bk2_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (bk2_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (bk2_in_names(k, WARNING_CHILDREN)) return "Warning";
    if (!strcmp(k, "UnboundLocalError")) return "NameError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    if (!strcmp(k, "PythonFinalizationError")) return "RuntimeError";
    /* zipfile / zlib / lzma read errors used by backup.py's read-error tuple:
     * they are ordinary Exception subclasses. */
    if (!strcmp(k, "BadZipFile")) return "Exception";
    if (!strcmp(k, "zlib.error") || !strcmp(k, "LZMAError")) return "Exception";
    return "Exception";  /* unknown customs default to Exception */
}
static bool bk2_caught(const json_t *r, const char *cls) {
    const char *k = bk2_raise_class(r);
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = bk2_exc_parent(k);
    }
    return false;
}
static bool bk2_is_exception(const json_t *r) { return bk2_caught(r, "Exception"); }
/* A member-read error is OSError or one of _ZIP_MEMBER_READ_ERRORS. */
static bool bk2_zip_read_error(const json_t *r) {
    return bk2_caught(r, "OSError") || bk2_caught(r, "EOFError") ||
           bk2_caught(r, "BadZipFile") || bk2_caught(r, "zlib.error") ||
           bk2_caught(r, "LZMAError");
}

/* ── arg builders ── */

static void bk2_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void bk2_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void bk2_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void bk2_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static json_t *bk2_str_array(const char *const *xs, size_t n) {
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(xs[i]));
    return a;
}

/* fmt/args logger seam: mirrors Python logging's (fmt, *args) call shape;
 * the oracle's logger stub records the same keys. */
/* Returns the seam result so callers can propagate a raise: print/logger
 * calls are NOT inside a try in the live functions, so a scripted raise from
 * them aborts the flow. */
static json_t *bk2_log(const bk2_seams_t *s, const char *seam, const char *fmt,
                       const char *const *args, size_t nargs) {
    json_t *a = json_object();
    bk2_aset_s(a, "fmt", fmt);
    json_t *arr = json_array();
    for (size_t i = 0; i < nargs; i++) json_append(arr, json_string(args[i] ? args[i] : ""));
    json_set(a, "args", arr);
    json_t *r = bk2_seam(s, seam, a);
    json_free(a);
    return r;
}

/* ── constants transcribed from backup.py / its imports ── */

static const char *const BK2_IMPORT_SKIP_NAMES[] = {
    "gateway_state.json", "gateway.pid", "cron.pid", "gateway.lock", "processes.json", NULL
};
static const char *const BK2_PM_RUNTIME_ROOT_DIRS[] = {
    "models", "runtimes", "node", "installs", "tools", NULL
};
static const char *const BK2_SQLITE_SIDECAR_SUFFIXES[] = {
    ".db-wal", ".db-shm", ".db-journal", NULL
};
#define BK2_EXTERNAL_PREFIX "_external/"
#define BK2_INCOMPLETE_ZIP_SUFFIX ".incomplete.zip"

static bool bk2_endswith(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t ls = strlen(s), lp = strlen(suf);
    return ls >= lp && !strcmp(s + ls - lp, suf);
}
/* pathlib `.lower().endswith(...)`: ASCII case-insensitive tail compare. */
static bool bk2_endswith_ci(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t ls = strlen(s), lp = strlen(suf);
    if (ls < lp) return false;
    const char *t = s + ls - lp;
    for (size_t i = 0; i < lp; i++)
        if (tolower((unsigned char)t[i]) != tolower((unsigned char)suf[i])) return false;
    return true;
}
static bool bk2_startswith(const char *s, const char *pre) {
    return s && pre && !strncmp(s, pre, strlen(pre));
}
/* pathlib suffix: last '.' at index > 0 of the file name, else "". */
static const char *bk2_suffix(const char *name) {
    if (!name) return "";
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) return "";
    return dot;
}

/* ══════════════ snapshot_recovery_hint ══════════════ */

/* PoP: snapshot_recovery_hint @ hermes_cli/backup.py:_snapshot_recovery_hint */
static json_t *bk2_snapshot_recovery_hint(void) {
    return json_string(
        "To restore a newer snapshot, start `hermes` in a terminal and run `/snapshot list`, then "
        "`/snapshot restore <id>` (CLI only).");
}

/* ══════════════ print_capped ══════════════ */

/* PoP: print_capped @ hermes_cli/backup.py:_print_capped */
static json_t *bk2_print_capped(const bk2_seams_t *s, const json_t *inp) {
    const char *header = bk2_jstr(json_obj_get(inp, "header")) ?: "";
    const char *indent = bk2_jstr(json_obj_get(inp, "indent")) ?: "";
    const json_t *lines = json_obj_get(inp, "lines");
    size_t n = bk2_len(lines);

    json_t *a = json_object();
    bk2_aset_s(a, "msg", header);
    json_t *r = bk2_seam(s, "print", a); json_free(a);
    if (bk2_is_raise(r)) return r;
    json_free(r);

    size_t show = n < 10 ? n : 10;
    for (size_t i = 0; i < show; i++) {
        const char *line = bk2_jstr(bk2_at(lines, i)) ?: "";
        char *msg = bk2_concat(indent, line);
        a = json_object(); bk2_aset_s(a, "msg", msg);
        r = bk2_seam(s, "print", a); json_free(a); free(msg);
        if (bk2_is_raise(r)) return r;
        json_free(r);
    }
    if (n > 10) {
        char *msg = bk2_xsf("%s... and %zu more", indent, n - 10);
        a = json_object(); bk2_aset_s(a, "msg", msg);
        r = bk2_seam(s, "print", a); json_free(a); free(msg);
        if (bk2_is_raise(r)) return r;
        json_free(r);
    }
    return NULL;
}

/* ══════════════ import_skipped ══════════════ */

/* PoP: import_skipped @ hermes_cli/backup.py:_import_skipped */
static json_t *bk2_import_skipped(const bk2_seams_t *s, const char *rel) {
    json_t *a = json_object();
    bk2_aset_s(a, "rel", rel);
    json_t *parts = bk2_seam(s, "normalize_archive_parts", a);
    json_free(a);
    if (bk2_is_raise(parts)) {
        if (bk2_caught(parts, "ValueError")) { json_free(parts); return json_bool(false); }
        return parts;  /* propagate non-ValueError */
    }
    if (!bk2_is_arr(parts) || parts->c.count == 0) {
        /* live would raise IndexError on parts[-1]; fixtures keep parts non-empty */
        json_free(parts);
        return bk2_raise_spec("IndexError", "list index out of range");
    }
    const char *last = bk2_jstr(parts->c.items[parts->c.count - 1]) ?: "";
    bool hit = bk2_in_names(last, BK2_IMPORT_SKIP_NAMES);
    if (hit) { json_free(parts); return json_bool(true); }

    a = json_object(); bk2_aset_j(a, "parts", parts);
    json_t *proot = bk2_seam(s, "profile_root_entry", a);
    json_free(a); json_free(parts);
    if (bk2_is_raise(proot)) return proot;
    if (bk2_in_names(bk2_jstr(proot), BK2_PM_RUNTIME_ROOT_DIRS)) {
        json_free(proot); return json_bool(true);
    }
    json_free(proot);

    for (size_t i = 0; BK2_SQLITE_SIDECAR_SUFFIXES[i]; i++)
        if (bk2_endswith(rel, BK2_SQLITE_SIDECAR_SUFFIXES[i])) return json_bool(true);
    return json_bool(false);
}

/* ══════════════ import_member_rel ══════════════ */

/* PoP: import_member_rel @ hermes_cli/backup.py:_import_member_rel */
static json_t *bk2_import_member_rel(const bk2_seams_t *s, const json_t *inp) {
    const char *member = bk2_jstr(json_obj_get(inp, "member")) ?: "";
    const char *prefix = bk2_jstr(json_obj_get(inp, "prefix")) ?: "";
    json_t *out = json_array();
    if (bk2_startswith(member, BK2_EXTERNAL_PREFIX)) {
        json_append(out, json_string(member + strlen(BK2_EXTERNAL_PREFIX)));
        json_append(out, json_bool(false));
        return out;  /* external members are never skipped; import_skipped NOT called */
    }
    const char *rel = (prefix[0] && bk2_startswith(member, prefix)) ? member + strlen(prefix) : member;
    json_append(out, json_string(rel));
    json_t *sk = bk2_import_skipped(s, rel);
    if (bk2_is_raise(sk)) { json_free(out); return sk; }
    json_append(out, sk);
    return out;
}

/* ══════════════ newest_first / prune_oldest (inline siblings) ══════════════ */

typedef enum { BK2_K_PREFIX, BK2_K_INCOMPLETE } bk2_keep_kind_t;

/* Ported sibling _newest_first: returns an array of entry names, newest first. */
static json_t *bk2_newest_first(const bk2_seams_t *s, const char *root,
                                bk2_keep_kind_t kind, const char *prefix) {
    json_t *a = json_object(); bk2_aset_s(a, "path", root);
    json_t *ex = bk2_seam(s, "path_exists", a); json_free(a);
    if (bk2_is_raise(ex)) return ex;
    if (!bk2_jtruth(ex)) { json_free(ex); return json_array(); }
    json_free(ex);

    a = json_object(); bk2_aset_s(a, "path", root);
    json_t *ents = bk2_seam(s, "path_iterdir", a); json_free(a);
    if (bk2_is_raise(ents)) return ents;

    json_t *names = json_array();
    for (size_t i = 0; i < bk2_len(ents); i++) {
        const json_t *e = bk2_at(ents, i);
        const char *name = bk2_jstr(json_obj_get(e, "name")) ?: "";
        bool is_file = json_get_bool(e, "is_file", false);
        bool keep = false;
        if (kind == BK2_K_PREFIX) {
            keep = is_file && bk2_startswith(name, prefix) &&
                   bk2_endswith_ci(bk2_suffix(name), ".zip") &&
                   !bk2_endswith_ci(name, BK2_INCOMPLETE_ZIP_SUFFIX);
        } else {
            keep = is_file && bk2_startswith(name, prefix) &&
                   bk2_endswith_ci(name, BK2_INCOMPLETE_ZIP_SUFFIX);
        }
        if (keep) json_append(names, json_string(name));
    }
    json_free(ents);
    /* sorted(..., key=name, reverse=True): insertion sort, descending. */
    for (size_t i = 1; i < names->c.count; i++) {
        json_t *key = names->c.items[i];
        size_t j = i;
        while (j > 0 && strcmp(bk2_jstr(key), bk2_jstr(names->c.items[j - 1])) > 0) {
            names->c.items[j] = names->c.items[j - 1];
            j--;
        }
        names->c.items[j] = key;
    }
    return names;
}

/* Ported sibling _prune_oldest. */
static json_t *bk2_prune_oldest(const bk2_seams_t *s, const json_t *names,
                                size_t keep, const char *what) {
    size_t deleted = 0;
    for (size_t i = keep; i < bk2_len(names); i++) {
        const char *name = bk2_jstr(bk2_at(names, i)) ?: "";
        json_t *a = json_object(); bk2_aset_s(a, "path", name);
        json_t *r = bk2_seam(s, "unlink", a); json_free(a);
        if (bk2_is_raise(r)) {
            if (bk2_caught(r, "OSError")) {
                const char *msg = bk2_raise_msg(r);
                const char *args[3] = { what, name, msg };
                json_t *lr = bk2_log(s, "logger_warning", "Failed to prune %s %s: %s", args, 3);
                json_free(r);
                if (bk2_is_raise(lr)) return lr;
                json_free(lr);
                continue;
            }
            return r;
        }
        json_free(r);
        deleted++;
    }
    return json_number((double)deleted);
}

/* PoP: prune_prefixed_zips @ hermes_cli/backup.py:_prune_prefixed_zips */
static json_t *bk2_prune_prefixed_zips(const bk2_seams_t *s, const json_t *inp) {
    const char *root = bk2_jstr(json_obj_get(inp, "backup_dir")) ?: "";
    const char *prefix = bk2_jstr(json_obj_get(inp, "prefix")) ?: "";
    const char *what = bk2_jstr(json_obj_get(inp, "what")) ?: "";
    size_t keep = (size_t)json_get_num(inp, "keep", 0);
    json_t *backups = bk2_newest_first(s, root, BK2_K_PREFIX, prefix);
    if (bk2_is_raise(backups)) return backups;
    json_t *r = bk2_prune_oldest(s, backups, keep, what);
    json_free(backups);
    return r;
}

/* PoP: prune_incomplete_zips @ hermes_cli/backup.py:_prune_incomplete_zips */
static json_t *bk2_prune_incomplete_zips(const bk2_seams_t *s, const json_t *inp) {
    const char *root = bk2_jstr(json_obj_get(inp, "backup_dir")) ?: "";
    const char *prefix = bk2_jstr(json_obj_get(inp, "prefix")) ?: "";
    const char *what = bk2_jstr(json_obj_get(inp, "what")) ?: "";
    char *iw = bk2_xsf("incomplete %s", what);
    json_t *salvage = bk2_newest_first(s, root, BK2_K_INCOMPLETE, prefix);
    if (bk2_is_raise(salvage)) { free(iw); return salvage; }
    json_t *r = bk2_prune_oldest(s, salvage, 1, iw);
    json_free(salvage); free(iw);
    return r;
}

/* ══════════════ write_zip_file ══════════════ */

/* PoP: write_zip_file @ hermes_cli/backup.py:_write_zip_file */
static json_t *bk2_write_zip_file(const bk2_seams_t *s, const json_t *inp) {
    const char *zf = bk2_jstr(json_obj_get(inp, "zf")) ?: "";
    const char *path = bk2_jstr(json_obj_get(inp, "path")) ?: "";
    const char *arc = bk2_jstr(json_obj_get(inp, "arcname")) ?: "";
    json_t *a = json_object(); bk2_aset_s(a, "zf", zf);
    json_t *n = bk2_seam(s, "zip_filelist_len", a); json_free(a);
    if (bk2_is_raise(n)) return n;
    double flen = (n->type == JSON_NUMBER) ? n->num_val : 0.0;
    json_free(n);

    a = json_object(); bk2_aset_s(a, "path", path); bk2_aset_s(a, "arcname", arc);
    json_t *r = bk2_seam(s, "zip_write", a); json_free(a);
    if (bk2_is_raise(r)) {
        /* live: `except Exception: _discard...; raise` — a BaseException
         * propagates WITHOUT the discard seam. */
        if (bk2_is_exception(r)) {
            a = json_object(); bk2_aset_n(a, "filelist_len", flen);
            json_t *d = bk2_seam(s, "discard_failed_zip_members", a); json_free(a);
            if (bk2_is_raise(d)) { json_free(r); return d; }  /* discard raise replaces */
            json_free(d);
        }
        return r;  /* re-raise original */
    }
    json_free(r);
    return NULL;
}

/* ══════════════ find_corrupt_members ══════════════ */

/* PoP: find_corrupt_members @ hermes_cli/backup.py:_find_corrupt_members */
static json_t *bk2_find_corrupt_members(const bk2_seams_t *s, const json_t *inp) {
    const char *zf = bk2_jstr(json_obj_get(inp, "zf")) ?: "";
    const json_t *members = json_obj_get(inp, "members");
    json_t *bad = json_array();
    for (size_t i = 0; i < bk2_len(members); i++) {
        const char *m = bk2_jstr(bk2_at(members, i)) ?: "";
        json_t *a = json_object(); bk2_aset_s(a, "zf", zf); bk2_aset_s(a, "member", m);
        json_t *op = bk2_seam(s, "zip_open", a); json_free(a);
        if (bk2_is_raise(op)) {
            if (bk2_zip_read_error(op)) {
                char *entry = bk2_xsf("%s: %s", m, bk2_raise_msg(op));
                json_append(bad, json_string(entry)); free(entry);
                json_free(op);
                continue;
            }
            json_free(bad); return op;
        }
        json_free(op);
        /* entered the with-block */
        bool exited = false;
        while (true) {
            a = json_object(); bk2_aset_s(a, "member", m);
            json_t *rd = bk2_seam(s, "zip_read", a); json_free(a);
            if (bk2_is_raise(rd)) {
                /* __exit__ fires once on the way out (the with unwinds) */
                a = json_object(); bk2_aset_s(a, "member", m);
                json_t *ex = bk2_seam(s, "zip_open_exit", a); json_free(a);
                exited = true;
                if (bk2_is_raise(ex)) { json_free(rd); json_free(bad); return ex; }
                json_free(ex);
                if (bk2_zip_read_error(rd)) {
                    char *entry = bk2_xsf("%s: %s", m, bk2_raise_msg(rd));
                    json_append(bad, json_string(entry)); free(entry);
                    json_free(rd);
                    break;
                }
                json_free(rd); json_free(bad);
                return rd;
            }
            bool eof = !bk2_jtruth(rd);
            json_free(rd);
            if (eof) break;
        }
        if (!exited) {
            a = json_object(); bk2_aset_s(a, "member", m);
            json_t *ex = bk2_seam(s, "zip_open_exit", a); json_free(a);
            if (bk2_is_raise(ex)) { json_free(bad); return ex; }
            json_free(ex);
        }
    }
    return bad;
}

/* ══════════════ snapshot_epochs_restorable ══════════════ */

/* PoP: snapshot_epochs_restorable @ hermes_cli/backup.py:_snapshot_epochs_restorable */
static json_t *bk2_snapshot_epochs_restorable(const bk2_seams_t *s, const json_t *inp) {
    const json_t *files = json_obj_get(inp, "files");
    for (size_t i = 0; i < bk2_len(files); i++) {
        const char *rel = bk2_jstr(bk2_at(files, i)) ?: "";
        json_t *a = json_object(); bk2_aset_s(a, "rel", rel);
        json_t *e = bk2_seam(s, "entry_paths", a); json_free(a);
        if (bk2_is_raise(e)) return e;
        bool exists = false; const char *name = "", *rname = "";
        if (bk2_is_arr(e) && bk2_len(e) >= 3) {
            exists = bk2_jtruth(bk2_at(e, 0));
            name = bk2_jstr(bk2_at(e, 1)) ?: "";
            rname = bk2_jstr(bk2_at(e, 2)) ?: "";
        }
        json_free(e);
        if (exists && (!strcmp(name, "state.db") || !strcmp(rname, "state.db"))) {
            a = json_object(); bk2_aset_s(a, "path", name);
            json_t *r = bk2_seam(s, "restore_destination_epoch", a); json_free(a);
            if (bk2_is_raise(r)) {
                if (bk2_caught(r, "OSError")) {
                    const char *msg = bk2_raise_msg(r);
                    const char *args[1] = { msg };
                    json_t *lr = bk2_log(s, "logger_error", "%s", args, 1);
                    json_free(r);
                    if (bk2_is_raise(lr)) return lr;
                    json_free(lr);
                    return json_bool(false);
                }
                return r;
            }
            json_free(r);
        }
    }
    return json_bool(true);
}

/* ══════════════ all-profiles orchestrators ══════════════ */

/* PoP: create_pre_update_snapshots_all_profiles @ hermes_cli/backup.py:create_pre_update_snapshots_all_profiles */
static json_t *bk2_create_pre_update_snapshots_all_profiles(const bk2_seams_t *s, const json_t *inp) {
    const char *home = bk2_jstr(json_obj_get(inp, "invoking_home")) ?: "";
    const json_t *keep = json_obj_get(inp, "keep");
    const json_t *maxsz = json_obj_get(inp, "max_file_size");
    json_t *results = json_object();
    json_t *a = json_object(); bk2_aset_s(a, "home", home);
    json_t *homes = bk2_seam(s, "sibling_profile_homes", a); json_free(a);
    if (bk2_is_raise(homes)) { json_free(results); return homes; }
    if (!bk2_is_arr(homes)) { json_free(results); json_free(homes); return json_object(); }
    for (size_t i = 0; i < bk2_len(homes); i++) {
        const json_t *pair = bk2_at(homes, i);
        const char *name = bk2_jstr(bk2_at(pair, 0)) ?: "";
        const char *phome = bk2_jstr(bk2_at(pair, 1)) ?: "";
        a = json_object();
        bk2_aset_s(a, "label", "pre-update");
        bk2_aset_s(a, "hermes_home", phome);
        bk2_aset_j(a, "keep", keep);
        bk2_aset_j(a, "max_file_size", maxsz);
        json_t *r = bk2_seam(s, "create_quick_snapshot", a); json_free(a);
        if (bk2_is_raise(r)) {
            if (bk2_is_exception(r)) {
                const char *args[2] = { name, bk2_raise_msg(r) };
                json_t *lr = bk2_log(s, "logger_debug",
                                     "Pre-update snapshot for profile %s failed: %s", args, 2);
                json_free(r);
                if (bk2_is_raise(lr)) { json_free(results); return lr; }
                json_free(lr);
                continue;
            }
            json_free(results); return r;  /* BaseException propagates */
        }
        if (bk2_jtruth(r)) json_set(results, name, json_copy(r));
        json_free(r);
    }
    json_free(homes);
    return results;
}

/* Shared body for the two restore-all orchestrators. */
static json_t *bk2_restore_all(const bk2_seams_t *s, const json_t *inp,
                               const char *seam_name, const char *fmt) {
    const json_t *snaps = json_obj_get(inp, "profile_snapshots");
    const char *home = bk2_jstr(json_obj_get(inp, "invoking_home")) ?: "";
    json_t *restored = json_array();
    if (!bk2_is_obj(snaps) || bk2_len(snaps) == 0) return restored;
    json_t *a = json_object(); bk2_aset_s(a, "home", home);
    json_t *homes = bk2_seam(s, "sibling_profile_homes", a); json_free(a);
    if (bk2_is_raise(homes)) { json_free(restored); return homes; }
    for (size_t i = 0; i < bk2_len(snaps); i++) {
        const char *name = snaps->c.keys[i];
        const char *snapid = bk2_jstr(snaps->c.items[i]) ?: "";
        /* by_name lookup */
        const char *phome = NULL;
        if (bk2_is_arr(homes)) {
            for (size_t j = 0; j < bk2_len(homes); j++) {
                const json_t *pair = bk2_at(homes, j);
                if (!strcmp(bk2_jstr(bk2_at(pair, 0)) ?: "", name)) {
                    phome = bk2_jstr(bk2_at(pair, 1)); break;
                }
            }
        }
        if (!phome) continue;
        a = json_object(); bk2_aset_s(a, "snapshot_id", snapid); bk2_aset_s(a, "hermes_home", phome);
        json_t *r = bk2_seam(s, seam_name, a); json_free(a);
        if (bk2_is_raise(r)) {
            if (bk2_is_exception(r)) {
                const char *args[2] = { name, bk2_raise_msg(r) };
                json_t *lr = bk2_log(s, "logger_debug", fmt, args, 2);
                json_free(r);
                if (bk2_is_raise(lr)) { json_free(homes); json_free(restored); return lr; }
                json_free(lr);
                continue;
            }
            json_free(homes); json_free(restored); return r;
        }
        if (bk2_jtruth(r)) {
            json_set(r, "profile", json_string(name));
            json_append(restored, r);
        } else json_free(r);
    }
    json_free(homes);
    return restored;
}

/* PoP: restore_config_model_settings_all_profiles @ hermes_cli/backup.py:restore_config_model_settings_all_profiles */
static json_t *bk2_restore_config_model_settings_all_profiles(const bk2_seams_t *s, const json_t *inp) {
    return bk2_restore_all(s, inp, "restore_config_model_settings_if_rewritten",
                           "Config model-settings restore check for profile %s failed: %s");
}

/* PoP: restore_cron_jobs_all_profiles @ hermes_cli/backup.py:restore_cron_jobs_all_profiles */
static json_t *bk2_restore_cron_jobs_all_profiles(const bk2_seams_t *s, const json_t *inp) {
    return bk2_restore_all(s, inp, "restore_cron_jobs_if_emptied",
                           "Cron restore check for profile %s failed: %s");
}
