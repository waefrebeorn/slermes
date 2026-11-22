/*
 * backup_flow3.c -- C11 FLOW port of the STILL-UNCLAIMED remainder of
 * hermes_cli/backup.py beyond backup_flow2 (CRLF-aware PoP grep at authoring
 * time: backup.c / backup_gaps.c / backup_flow2.c already claim 44 functions;
 * the 26 below carry NO PoP marker in src/ and are therefore unclaimed).
 *
 * All cores are `static bk3_*`, driven by tests/t_port_backup_flow3.c.
 * PoP annotations (defining module hermes_cli/backup.py):
 *   PoP: is_non_regular_path @ hermes_cli/backup.py:_is_non_regular_path
 *   PoP: is_link_path @ hermes_cli/backup.py:_is_link_path
 *   PoP: snapshot_db @ hermes_cli/backup.py:_snapshot_db
 *   PoP: discard_failed_zip_members @ hermes_cli/backup.py:_discard_failed_zip_members
 *   PoP: zip_sqlite_snapshot @ hermes_cli/backup.py:_zip_sqlite_snapshot
 *   PoP: write_zip_entries @ hermes_cli/backup.py:_write_zip_entries
 *   PoP: resolve_backup_output_path @ hermes_cli/backup.py:_resolve_backup_output_path
 *   PoP: collect_external_entries @ hermes_cli/backup.py:_collect_external_entries
 *   PoP: run_import @ hermes_cli/backup.py:run_import
 *   PoP: import_members @ hermes_cli/backup.py:_import_members
 *   PoP: import_external_member @ hermes_cli/backup.py:_import_external_member
 *   PoP: import_members_exclusive @ hermes_cli/backup.py:_import_members_exclusive
 *   PoP: revive_gateway_after_import @ hermes_cli/backup.py:_revive_gateway_after_import
 *   PoP: newest_first @ hermes_cli/backup.py:_newest_first
 *   PoP: prune_oldest @ hermes_cli/backup.py:_prune_oldest
 *   PoP: create_prefixed_full_backup @ hermes_cli/backup.py:_create_prefixed_full_backup
 *   PoP: too_large @ hermes_cli/backup.py:_too_large
 *   PoP: is_trusted_root_auth_alias @ hermes_cli/backup.py:_is_trusted_root_auth_alias
 *   PoP: quick_snapshot_dir @ hermes_cli/backup.py:quick_snapshot_dir
 *   PoP: restore_quick_snapshot_exclusive @ hermes_cli/backup.py:_restore_quick_snapshot_exclusive
 *   PoP: entry_paths @ hermes_cli/backup.py:_entry_paths
 *   PoP: restore_config_model_settings_if_rewritten @ hermes_cli/backup.py:restore_config_model_settings_if_rewritten
 *   PoP: db_failure @ hermes_cli/backup.py:_db_failure
 *   PoP: capped_errors @ hermes_cli/backup.py:_capped_errors
 *   PoP: publish_path @ hermes_cli/backup.py:_publish_path
 *   PoP: progress @ hermes_cli/backup.py:_progress
 *
 * Universal seam: every effectful collaborator is a named seam; a
 * {"__raise__":cls,"msg":m} object is a raised exception; a MISSING script
 * fail-closes with RuntimeError; a present null is a valid None; a JSON array
 * script yields one result PER CALL (last repeats). Full py3.14 builtin
 * exception parent hierarchy in bk3_exc_parent/bk3_caught (BaseException trio
 * never swallowed by `except Exception`).
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
typedef struct bk3_trace { json_t *events; } bk3_trace_t;
typedef struct bk3_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} bk3_seams_t;
static void bk3_trace_add(bk3_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void bk3_trace_free(bk3_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *bk3_seam(const bk3_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}

/* ---- json helpers ---- */
static bool bk3_jtruth(const json_t *v) {
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
static const char *bk3_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool bk3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool bk3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool bk3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static size_t bk3_len(const json_t *v) { return (bk3_is_arr(v) || bk3_is_obj(v)) ? v->c.count : 0; }
static const json_t *bk3_at(const json_t *v, size_t i) {
    return (bk3_is_arr(v) && i < v->c.count) ? v->c.items[i] : NULL;
}
static char *bk3_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}

/* ---- exception model (FULL builtin parent hierarchy, py3.14) ---- */
static const char *bk3_raise_class(const json_t *r) {
    if (bk3_is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool bk3_is_raise(const json_t *r) { return bk3_raise_class(r) != NULL; }
static const char *bk3_raise_msg(const json_t *r) {
    if (!bk3_is_obj(r)) return "";
    const char *m = bk3_jstr(json_obj_get(r, "msg"));
    return m ? m : "";
}
static json_t *bk3_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool bk3_in_names(const char *k, const char *const *names) {
    if (!k) return false;
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
static const char *bk3_exc_parent(const char *k) {
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
    if (bk3_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = { "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL };
    if (bk3_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (bk3_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (bk3_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (bk3_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (bk3_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = { "NotImplementedError", "RecursionError", NULL };
    if (bk3_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (bk3_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (bk3_in_names(k, WARNING_CHILDREN)) return "Warning";
    if (!strcmp(k, "UnboundLocalError")) return "NameError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    if (!strcmp(k, "PythonFinalizationError")) return "RuntimeError";
    if (!strcmp(k, "BadZipFile")) return "Exception";
    if (!strcmp(k, "zlib.error") || !strcmp(k, "LZMAError")) return "Exception";
    return "Exception";
}
static bool bk3_caught(const json_t *r, const char *cls) {
    const char *k = bk3_raise_class(r);
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) { if (!strcmp(k, cls)) return true; k = bk3_exc_parent(k); }
    return false;
}
static bool bk3_is_exception(const json_t *r) { return bk3_caught(r, "Exception"); }

/* ---- arg builders ---- */
static void bk3_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void bk3_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void bk3_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void bk3_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static json_t *bk3_log(const bk3_seams_t *s, const char *seam, const char *fmt,
                       const char *const *args, size_t nargs) {
    json_t *a = json_object(); bk3_aset_s(a, "fmt", fmt);
    json_t *arr = json_array();
    for (size_t i = 0; i < nargs; i++) json_append(arr, json_string(args[i] ? args[i] : ""));
    json_set(a, "args", arr);
    json_t *r = bk3_seam(s, seam, a); json_free(a); return r;
}

/* ---- constants / helpers ---- */
#define BK3_EXTERNAL_PREFIX "_external/"
static bool bk3_endswith(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t ls = strlen(s), lp = strlen(suf);
    return ls >= lp && !strcmp(s + ls - lp, suf);
}
static bool bk3_startswith(const char *s, const char *pre) {
    return s && pre && !strncmp(s, pre, strlen(pre));
}
static const char *const BK3_SIDECAR[] = { ".db-wal", ".db-shm", ".db-journal", NULL };
static const char *const BK3_IMPORT_SKIP[] = {
    "gateway_state.json", "gateway.pid", "cron.pid", "gateway.lock", "processes.json", NULL
};
static const char *const BK3_PM_ROOTS[] = { "models", "runtimes", "node", "installs", "tools", NULL };
static const char *const BK3_SECRET_NAMES[] = { "config.yaml", ".env", "auth.json", "token.json", NULL };

/* stat.S_ISREG / S_ISLNK on st_mode integer projection. */
#define BK3_S_IFMT 0170000
#define BK3_S_IFREG 0100000
#define BK3_S_IFLNK 0120000
#define BK3_FILE_ATTRIBUTE_REPARSE_POINT 0x400

/* ══════════════ 1. _is_non_regular_path ══════════════ */
/* PoP: is_non_regular_path @ hermes_cli/backup.py:_is_non_regular_path */
static json_t *bk3_is_non_regular_path(const bk3_seams_t *s, const json_t *inp) {
    json_t *a = json_object(); bk3_aset_s(a, "path", bk3_jstr(json_obj_get(inp, "path")) ?: "");
    json_t *info = bk3_seam(s, "lstat", a); json_free(a);
    if (bk3_is_raise(info)) {
        if (bk3_caught(info, "OSError")) { json_free(info); return json_bool(false); }
        return info;
    }
    long mode = (long)json_get_num(info, "mode", 0);
    json_free(info);
    return json_bool((mode & BK3_S_IFMT) != BK3_S_IFREG);
}

/* ══════════════ 2. _is_link_path ══════════════ */
/* PoP: is_link_path @ hermes_cli/backup.py:_is_link_path */
static json_t *bk3_is_link_path(const bk3_seams_t *s, const json_t *inp) {
    json_t *a = json_object(); bk3_aset_s(a, "path", bk3_jstr(json_obj_get(inp, "path")) ?: "");
    json_t *info = bk3_seam(s, "lstat", a); json_free(a);
    if (bk3_is_raise(info)) {
        if (bk3_caught(info, "OSError")) { json_free(info); return json_bool(false); }
        return info;
    }
    long mode = (long)json_get_num(info, "mode", 0);
    long attrs = (long)json_get_num(info, "st_file_attributes", 0);
    json_free(info);
    if ((mode & BK3_S_IFMT) == BK3_S_IFLNK) return json_bool(true);
    bool nt = bk3_jtruth(json_obj_get(inp, "os_name_nt"));
    return json_bool(nt && ((attrs & BK3_FILE_ATTRIBUTE_REPARSE_POINT) != 0));
}

/* ══════════════ 3. _snapshot_db ══════════════ */
/* PoP: snapshot_db @ hermes_cli/backup.py:_snapshot_db */
static json_t *bk3_snapshot_db(const bk3_seams_t *s, const json_t *inp) {
    const char *src = bk3_jstr(json_obj_get(inp, "src")) ?: "";
    const char *dst = bk3_jstr(json_obj_get(inp, "dst")) ?: "";
    json_t *a = json_object(); bk3_aset_s(a, "src", src);
    json_t *store = bk3_seam(s, "is_store_file", a); json_free(a);
    if (bk3_is_raise(store)) return store;
    bool is_store = bk3_jtruth(store); json_free(store);
    if (!is_store) {
        a = json_object(); bk3_aset_s(a, "src", src); bk3_aset_s(a, "dst", dst);
        json_t *r = bk3_seam(s, "safe_copy_db", a); json_free(a); return r;
    }
    a = json_object(); bk3_aset_s(a, "src", src);
    json_t *admin = bk3_seam(s, "open_store_admin", a); json_free(a);
    if (bk3_is_raise(admin)) return admin;
    bool unsupported = bk3_jtruth(json_obj_get(admin, "unsupported"));
    json_free(admin);
    if (unsupported) return json_bool(false);
    a = json_object(); bk3_aset_s(a, "dst", dst);
    json_t *r = bk3_seam(s, "admin_backup_to", a); json_free(a);
    if (bk3_is_raise(r)) return r;
    bool ok = bk3_jtruth(r); json_free(r);
    return json_bool(ok);
}

/* ══════════════ 4. _discard_failed_zip_members ══════════════ */
/* PoP: discard_failed_zip_members @ hermes_cli/backup.py:_discard_failed_zip_members */
static json_t *bk3_discard_failed_zip_members(const bk3_seams_t *s, const json_t *inp) {
    const char *zf = bk3_jstr(json_obj_get(inp, "zf")) ?: "";
    double fl = json_get_num(inp, "filelist_len", 0);
    json_t *a = json_object(); bk3_aset_s(a, "zf", zf);
    json_t *n = bk3_seam(s, "filelist_len", a); json_free(a);
    if (bk3_is_raise(n)) return n;
    double cur = (n->type == JSON_NUMBER) ? n->num_val : 0.0; json_free(n);
    if (cur <= fl) return NULL;
    a = json_object(); bk3_aset_s(a, "zf", zf); bk3_aset_n(a, "index", fl);
    json_t *off = bk3_seam(s, "header_offset", a); json_free(a);
    if (bk3_is_raise(off)) return off;
    double offset = (off->type == JSON_NUMBER) ? off->num_val : 0.0; json_free(off);
    a = json_object(); bk3_aset_n(a, "offset", offset);
    json_t *r = bk3_seam(s, "fp_seek", a); json_free(a);
    if (bk3_is_raise(r)) return r; json_free(r);
    a = json_object(); bk3_aset_n(a, "offset", offset);
    r = bk3_seam(s, "fp_truncate", a); json_free(a);
    if (bk3_is_raise(r)) return r; json_free(r);
    a = json_object(); bk3_aset_n(a, "offset", offset); bk3_aset_s(a, "zf", zf);
    json_t *names = bk3_seam(s, "nametoinfo", a); json_free(a);
    if (bk3_is_raise(names)) return names;
    json_free(names);
    return NULL;
}

/* ══════════════ 5. _zip_sqlite_snapshot ══════════════ */
/* PoP: zip_sqlite_snapshot @ hermes_cli/backup.py:_zip_sqlite_snapshot */
static json_t *bk3_zip_sqlite_snapshot(const bk3_seams_t *s, const json_t *inp) {
    const char *out = bk3_jstr(json_obj_get(inp, "out_path")) ?: "";
    const char *rel = bk3_jstr(json_obj_get(inp, "rel_path")) ?: "";
    json_t *a = json_object(); bk3_aset_s(a, "out", out);
    json_t *stage = bk3_seam(s, "staged_backup", a); json_free(a);
    if (bk3_is_raise(stage)) return stage;
    const char *tj = bk3_jstr(stage) ?: "";
    char *tmp = strdup(tj); json_free(stage);
    a = json_object(); bk3_aset_s(a, "src", bk3_jstr(json_obj_get(inp, "abs_path")) ?: "");
    bk3_aset_s(a, "dst", tmp);
    json_t *ok = bk3_seam(s, "snapshot_db", a); json_free(a);
    if (bk3_is_raise(ok)) return ok;
    bool good = bk3_jtruth(ok); json_free(ok);
    if (!good) {
        a = json_object(); bk3_aset_s(a, "path", bk3_jstr(json_obj_get(inp, "abs_path")) ?: "");
        json_t *w = bk3_seam(s, "warn_legacy", a); json_free(a);
        if (bk3_is_raise(w)) return w; json_free(w);
        return json_null();
    }
    a = json_object(); bk3_aset_s(a, "path", tmp); bk3_aset_s(a, "arcname", rel);
    json_t *w = bk3_seam(s, "write_zip_file", a); json_free(a);
    if (bk3_is_raise(w)) return w; json_free(w);
    a = json_object(); bk3_aset_s(a, "path", tmp);
    json_t *sz = bk3_seam(s, "stat_size", a); json_free(a);
    free(tmp); return sz;
}

/* ══════════════ 6. _write_zip_entries ══════════════ */
/* PoP: write_zip_entries @ hermes_cli/backup.py:_write_zip_entries */
static json_t *bk3_write_zip_entries(const bk3_seams_t *s, const json_t *inp) {
    const json_t *files = json_obj_get(inp, "files");
    double total = 0.0;
    bool track = json_get_bool(inp, "track_bytes", false);
    for (size_t i = 0; i < bk3_len(files); i++) {
        const json_t *pair = bk3_at(files, i);
        const char *abs = bk3_jstr(bk3_at(pair, 0)) ?: "";
        const char *rel = bk3_jstr(bk3_at(pair, 1)) ?: "";
        bool is_db = bk3_endswith(abs, ".db");
        /* try block */
        json_t *r = NULL; bool err = false;
        if (is_db) {
            json_t *a = json_object();
            bk3_aset_s(a, "src", abs); bk3_aset_s(a, "rel", rel);
            r = bk3_seam(s, "zip_sqlite_snapshot", a); json_free(a);
            if (bk3_is_raise(r)) err = true;
            else if (bk3_is_null(r)) {
                json_free(r); r = NULL;
                a = json_object(); bk3_aset_s(a, "rel", rel);
                r = bk3_seam(s, "db_failure", a); json_free(a);
                if (bk3_is_raise(r)) err = true;
                else { json_free(r); r = NULL; continue; }
            } else {
                if (r->type == JSON_NUMBER) total += r->num_val;
                json_free(r); r = NULL; /* fall through to progress check */
            }
        } else {
            json_t *a = json_object(); bk3_aset_s(a, "path", abs);
            r = bk3_seam(s, "warn_legacy", a); json_free(a);
            if (bk3_is_raise(r)) err = true; else { json_free(r); r = NULL; }
            if (!err) {
                a = json_object(); bk3_aset_s(a, "path", abs); bk3_aset_s(a, "arcname", rel);
                r = bk3_seam(s, "write_zip_file", a); json_free(a);
                if (bk3_is_raise(r)) err = true; else { json_free(r); r = NULL; }
            }
            if (!err && track) {
                a = json_object(); bk3_aset_s(a, "path", abs);
                json_t *sz = bk3_seam(s, "stat_size", a); json_free(a);
                if (bk3_is_raise(sz)) err = true;
                else { if (sz->type == JSON_NUMBER) total += sz->num_val; json_free(sz); }
            }
        }
        if (err) {
            /* on_error(rel, exc) -- exc msg */
            json_t *a = json_object(); bk3_aset_s(a, "rel", rel);
            bk3_aset_j(a, "exc", r ? json_string(bk3_raise_msg(r)) : NULL);
            json_t *er = bk3_seam(s, "on_error", a); json_free(a);
            if (r) json_free(r);
            if (bk3_is_raise(er)) return er; json_free(er);
            continue;
        }
        if ((i + 1) % 500 == 0) {
            json_t *a = json_object(); bk3_aset_n(a, "i", (double)(i + 1));
            json_t *pr = bk3_seam(s, "on_progress", a); json_free(a);
            if (bk3_is_raise(pr)) return pr; json_free(pr);
        }
    }
    return json_number(total);
}

/* ══════════════ 7. _resolve_backup_output_path ══════════════ */
/* PoP: resolve_backup_output_path @ hermes_cli/backup.py:_resolve_backup_output_path */
static json_t *bk3_resolve_backup_output_path(const bk3_seams_t *s, const json_t *inp) {
    const char *output = bk3_jstr(json_obj_get(inp, "output"));
    const char *default_name = bk3_jstr(json_obj_get(inp, "default_name")) ?: "backup.zip";
    json_t *a = json_object(); bk3_aset_s(a, "path", output ? output : "");
    json_t *p = bk3_seam(s, "resolve_output", a); json_free(a);
    if (bk3_is_raise(p)) {
        if (bk3_caught(p, "OSError")) {
            char *msg = bk3_xsf("Error: cannot write backup to %s: %s",
                                output ? output : "", bk3_raise_msg(p));
            a = json_object(); bk3_aset_s(a, "msg", msg);
            json_t *pr = bk3_seam(s, "print", a); json_free(a); free(msg);
            if (bk3_is_raise(pr)) return pr; json_free(pr);
            json_free(p); return bk3_raise_spec("SystemExit", "1");
        }
        return p;
    }
    const char *cj = bk3_jstr(p) ?: "";
    char *cur = strdup(cj); json_free(p);
    /* is_dir decision scripted into resolve_output result object handled by oracle:
       the seam returns the resolved string; a following is_dir seam decides. */
    a = json_object(); bk3_aset_s(a, "path", cur);
    json_t *dir = bk3_seam(s, "is_dir", a); json_free(a);
    if (bk3_is_raise(dir)) return dir;
    bool isdir = bk3_jtruth(dir); json_free(dir);
    const char *final = cur; char *joined = NULL;
    if (isdir) { joined = bk3_xsf("%s/%s", cur, default_name); final = joined; }
    /* suffix: assume final carries suffix info via input 'suffix' */
    const char *suffix = bk3_jstr(json_obj_get(inp, "suffix")) ?: "";
    if (strcmp(suffix, ".zip") != 0) {
        char *withzip = bk3_xsf("%s%s", final, ".zip");
        a = json_object(); bk3_aset_s(a, "path", withzip);
        json_t *r = bk3_seam(s, "with_suffix", a); json_free(a); free(withzip);
        if (bk3_is_raise(r)) { free(joined); return r; }
        const char *nf = bk3_jstr(r) ?: final;
        char *copy = strdup(nf); json_free(r); free(joined); joined = copy; final = copy;
    }
    a = json_object(); bk3_aset_s(a, "path", final);
    json_t *mk = bk3_seam(s, "mkdir", a); json_free(a);
    if (bk3_is_raise(mk)) {
        if (bk3_caught(mk, "OSError")) {
            char *msg = bk3_xsf("Error: cannot write backup to %s: %s", output ? output : final, bk3_raise_msg(mk));
            a = json_object(); bk3_aset_s(a, "msg", msg);
            json_t *pr = bk3_seam(s, "print", a); json_free(a); free(msg);
            if (bk3_is_raise(pr)) { json_free(mk); free(joined); return pr; }
            json_free(mk); free(joined); return bk3_raise_spec("SystemExit", "1");
        }
        free(joined); return mk;  /* non-OSError raises propagate */
    }
    json_free(mk);
    json_t *res = json_string(final); free(joined); free(cur); return res;
}

/* ══════════════ 8. _collect_external_entries ══════════════ */
/* PoP: collect_external_entries @ hermes_cli/backup.py:_collect_external_entries */
static json_t *bk3_collect_external_entries(const bk3_seams_t *s, const json_t *inp) {
    (void)inp;
    json_t *home = bk3_seam(s, "home_resolve", json_object());
    if (bk3_is_raise(home)) return home; json_free(home);
    json_t *bases = bk3_seam(s, "external_paths", json_object());
    if (bk3_is_raise(bases)) return bases;
    json_t *add = json_array(), *skip = json_array();
    for (size_t i = 0; i < bk3_len(bases); i++) {
        const char *base = bk3_jstr(bk3_at(bases, i)) ?: "";
        json_t *a = json_object(); bk3_aset_s(a, "path", base);
        json_t *br = bk3_seam(s, "base_resolve", a); json_free(a);
        if (bk3_is_raise(br)) {
            if (bk3_caught(br, "ValueError") || bk3_caught(br, "OSError")) {
                json_append(skip, json_string(base)); json_free(br); continue;
            }
            json_free(add); json_free(skip); json_free(bases); return br;
        }
        json_free(br);
        a = json_object(); bk3_aset_s(a, "path", base);
        json_t *files = bk3_seam(s, "iter_external_files", a); json_free(a);
        if (bk3_is_raise(files)) { json_free(add); json_free(skip); json_free(bases); return files; }
        for (size_t j = 0; j < bk3_len(files); j++) {
            const char *fp = bk3_jstr(bk3_at(files, j)) ?: "";
            a = json_object(); bk3_aset_s(a, "path", fp);
            json_t *fr = bk3_seam(s, "fpath_resolve", a); json_free(a);
            if (bk3_is_raise(fr)) { json_free(fr); continue; }
            const char *relposix = bk3_jstr(fr) ?: fp;
            json_t *pair = json_array(); json_append(pair, json_string(fp));
            char *arc = bk3_xsf("%s%s", BK3_EXTERNAL_PREFIX, relposix);
            json_append(pair, json_string(arc)); free(arc); json_free(fr);
            json_append(add, pair);
        }
        json_free(files);
    }
    json_free(bases);
    json_t *out = json_array(); json_append(out, add); json_append(out, skip); return out;
}

/* ══════════════ 14/15. newest_first + prune_oldest ══════════════ */
/* PoP: newest_first @ hermes_cli/backup.py:_newest_first */
static json_t *bk3_newest_first(const bk3_seams_t *s, const json_t *inp) {
    const char *root = bk3_jstr(json_obj_get(inp, "root")) ?: "";
    json_t *a = json_object(); bk3_aset_s(a, "path", root);
    json_t *ex = bk3_seam(s, "path_exists", a); json_free(a);
    if (bk3_is_raise(ex)) return ex;
    if (!bk3_jtruth(ex)) { json_free(ex); return json_array(); }
    json_free(ex);
    a = json_object(); bk3_aset_s(a, "path", root);
    json_t *ents = bk3_seam(s, "path_iterdir", a); json_free(a);
    if (bk3_is_raise(ents)) return ents;
    json_t *names = json_array();
    for (size_t i = 0; i < bk3_len(ents); i++) {
        const json_t *e = bk3_at(ents, i);
        if (json_get_bool(e, "keep", false)) json_append(names, json_copy(json_obj_get(e, "name")));
    }
    json_free(ents);
    for (size_t i = 1; i < names->c.count; i++) {
        json_t *key = names->c.items[i]; size_t j = i;
        while (j > 0 && strcmp(bk3_jstr(key) ?: "", bk3_jstr(names->c.items[j-1]) ?: "") > 0) {
            names->c.items[j] = names->c.items[j-1]; j--;
        }
        names->c.items[j] = key;
    }
    return names;
}
/* PoP: prune_oldest @ hermes_cli/backup.py:_prune_oldest */
static json_t *bk3_prune_oldest(const bk3_seams_t *s, const json_t *inp) {
    const json_t *names = json_obj_get(inp, "names");
    double keep = json_get_num(inp, "keep", 0);
    const char *what = bk3_jstr(json_obj_get(inp, "what")) ?: "";
    double deleted = 0;
    for (size_t i = (size_t)keep; i < bk3_len(names); i++) {
        const char *n = bk3_jstr(bk3_at(names, i)) ?: "";
        json_t *a = json_object(); bk3_aset_s(a, "path", n);
        json_t *r = bk3_seam(s, "unlink", a); json_free(a);
        if (bk3_is_raise(r)) {
            if (bk3_caught(r, "OSError")) {
                const char *args[3] = { what, n, bk3_raise_msg(r) };
                json_t *lr = bk3_log(s, "logger_warning", "Failed to prune %s %s: %s", args, 3);
                json_free(r); if (bk3_is_raise(lr)) return lr; json_free(lr); continue;
            }
            return r;
        }
        json_free(r); deleted++;
    }
    return json_number(deleted);
}

/* ══════════════ 9. run_import (clean FLOW core: preflight) ══════════════ */
/* PoP: run_import @ hermes_cli/backup.py:run_import */
static json_t *bk3_run_import(const bk3_seams_t *s, const json_t *inp) {
    const char *zp = bk3_jstr(json_obj_get(inp, "zipfile")) ?: "";
    json_t *a = json_object(); bk3_aset_s(a, "what", "`hermes import`");
    json_t *req = bk3_seam(s, "require_file_tooling", a); json_free(a);
    if (bk3_is_raise(req)) return req; json_free(req);
    a = json_object(); bk3_aset_s(a, "path", zp);
    json_t *rz = bk3_seam(s, "expanduser_resolve", a); json_free(a);
    if (bk3_is_raise(rz)) return rz;
    const char *rj = bk3_jstr(rz) ?: zp;
    char *resolved = strdup(rj); json_free(rz);
    a = json_object(); bk3_aset_s(a, "path", resolved);
    json_t *isf = bk3_seam(s, "is_file", a); json_free(a);
    if (bk3_is_raise(isf)) return isf;
    if (!bk3_jtruth(isf)) {
        json_free(isf);
        char *msg = bk3_xsf("Error: File not found: %s", resolved);
        a = json_object(); bk3_aset_s(a, "msg", msg);
        json_t *pr = bk3_seam(s, "print", a); json_free(a); free(msg);
        if (bk3_is_raise(pr)) return pr; json_free(pr);
        return bk3_raise_spec("SystemExit", "1");
    }
    json_free(isf);
    a = json_object(); bk3_aset_s(a, "path", resolved);
    json_t *iz = bk3_seam(s, "is_zipfile", a); json_free(a);
    if (bk3_is_raise(iz)) return iz;
    if (!bk3_jtruth(iz)) {
        json_free(iz);
        char *msg = bk3_xsf("Error: Not a valid zip file: %s", resolved);
        a = json_object(); bk3_aset_s(a, "msg", msg);
        json_t *pr = bk3_seam(s, "print", a); json_free(a); free(msg);
        if (bk3_is_raise(pr)) return pr; json_free(pr);
        return bk3_raise_spec("SystemExit", "1");
    }
    json_free(iz);
    a = json_object(); bk3_aset_s(a, "path", zp);
    json_t *ok = bk3_seam(s, "validate_zip", a); json_free(a);
    if (bk3_is_raise(ok)) return ok;
    bool valid = json_get_bool(ok, "ok", true);
    const char *reason = bk3_jstr(json_obj_get(ok, "reason")) ?: "";
    char *reason_copy = strdup(reason);  /* ok is freed before the message is built */
    json_free(ok);
    if (!valid) {
        char *erm = bk3_xsf("Error: %s", reason_copy);
        a = json_object(); bk3_aset_s(a, "msg", erm); free(erm);
        json_t *pr = bk3_seam(s, "print", a); json_free(a);
        free(reason_copy);
        if (bk3_is_raise(pr)) return pr; json_free(pr);
        return bk3_raise_spec("SystemExit", "1");
    }
    free(reason_copy);
    return json_null();
}

/* ══════════════ 10. _import_members (reserve decision) ══════════════ */
/* PoP: import_members @ hermes_cli/backup.py:_import_members */
static json_t *bk3_import_members(const bk3_seams_t *s, const json_t *inp) {
    const char *root = bk3_jstr(json_obj_get(inp, "hermes_root")) ?: "";
    const json_t *members = json_obj_get(inp, "members");
    json_t *a = json_object(); bk3_aset_s(a, "path", root);
    json_t *rr = bk3_seam(s, "resolve_root", a); json_free(a);
    if (bk3_is_raise(rr)) return rr; json_free(rr);
    /* collect homes from members (path decisions scripted by 'parts') */
    json_t *homes = json_array(); json_append(homes, json_string(root));
    for (size_t i = 0; i < bk3_len(members); i++) {
        const char *m = bk3_jstr(bk3_at(members, i)) ?: "";
        if (bk3_startswith(m, BK3_EXTERNAL_PREFIX)) continue; /* restored under $HOME */
        a = json_object(); bk3_aset_s(a, "member", m);
        json_t *pa = bk3_seam(s, "member_parts", a); json_free(a);
        if (bk3_is_raise(pa)) return pa;
        if (bk3_is_arr(pa) && bk3_len(pa) >= 3 &&
            !strcmp(bk3_jstr(bk3_at(pa, 0)) ?: "", "profiles")) {
            char *h = bk3_xsf("%s/%s/%s", root, bk3_jstr(bk3_at(pa, 0)) ?: "",
                              bk3_jstr(bk3_at(pa, 1)) ?: "");
            json_append(homes, json_string(h)); free(h);
        }
        json_free(pa);
    }
    a = json_object(); bk3_aset_j(a, "homes", homes); json_free(homes);
    json_t *kh = bk3_seam(s, "keep_host_homes", a); json_free(a);
    if (bk3_is_raise(kh)) return kh;
    a = json_object();
    json_t *r = bk3_seam(s, "import_members_exclusive", a); json_free(a);
    json_free(kh);
    return r;
}

/* ══════════════ 11. _import_external_member ══════════════ */
/* PoP: import_external_member @ hermes_cli/backup.py:_import_external_member */
static json_t *bk3_import_external_member(const bk3_seams_t *s, const json_t *inp) {
    const char *member = bk3_jstr(json_obj_get(inp, "member")) ?: "";
    const char *home = bk3_jstr(json_obj_get(inp, "home_dir")) ?: "";
    const char *rest = bk3_startswith(member, BK3_EXTERNAL_PREFIX)
        ? member + strlen(BK3_EXTERNAL_PREFIX) : member;
    json_t *a = json_object(); bk3_aset_s(a, "target", rest);
    json_t *rel = bk3_seam(s, "relative_to", a); json_free(a);
    if (bk3_is_raise(rel)) {
        if (bk3_caught(rel, "ValueError")) {
            json_t *out = json_array(); json_append(out, json_bool(false));
            char *e = bk3_xsf("  %s: path traversal blocked", member);
            json_append(out, json_string(e)); free(e); return out;
        }
        return rel;
    }
    json_free(rel);
    a = json_object(); bk3_aset_s(a, "path", rest);
    json_t *mk = bk3_seam(s, "mkdir", a); json_free(a);
    if (bk3_is_raise(mk)) {
        json_t *out = json_array(); json_append(out, json_bool(false));
        char *e = bk3_xsf("  %s: %s", member, bk3_raise_msg(mk));
        json_append(out, json_string(e)); free(e); json_free(mk); return out;
    }
    json_free(mk);
    a = json_object(); bk3_aset_s(a, "member", member); bk3_aset_s(a, "target", rest);
    json_t *ex = bk3_seam(s, "extract", a); json_free(a);
    if (bk3_is_raise(ex)) {
        json_t *out = json_array(); json_append(out, json_bool(false));
        char *e = bk3_xsf("  %s: %s", member, bk3_raise_msg(ex));
        json_append(out, json_string(e)); free(e); json_free(ex); return out;
    }
    json_free(ex);
    bool secret = json_get_bool(inp, "secret", false);
    if (secret) {
        a = json_object(); bk3_aset_s(a, "path", rest); bk3_aset_n(a, "mode", 384); /* 0o600 */
        json_t *ch = bk3_seam(s, "chmod", a); json_free(a);
        if (bk3_is_raise(ch)) { if (!bk3_caught(ch, "OSError")) return ch; json_free(ch); }
        else json_free(ch);
    }
    json_t *out = json_array(); json_append(out, json_bool(true)); json_append(out, json_null());
    (void)home; return out;
}

/* ══════════════ 12. _import_members_exclusive (empty FLOW core) ══════════════ */
/* PoP: import_members_exclusive @ hermes_cli/backup.py:_import_members_exclusive */
static json_t *bk3_import_members_exclusive(const bk3_seams_t *s, const json_t *inp) {
    const char *root = bk3_jstr(json_obj_get(inp, "hermes_root")) ?: "";
    json_t *a = json_object(); bk3_aset_s(a, "path", root);
    json_t *hr = bk3_seam(s, "home_resolve", a); json_free(a);
    if (bk3_is_raise(hr)) return hr; json_free(hr);
    json_t *md = bk3_seam(s, "default_mode", json_object());
    if (bk3_is_raise(md)) return md; json_free(md);
    const json_t *members = json_obj_get(inp, "members");
    double restored = 0, restored_ext = 0;
    for (size_t i = 0; i < bk3_len(members); i++) {
        const char *m = bk3_jstr(bk3_at(members, i)) ?: "";
        if (!bk3_startswith(m, BK3_EXTERNAL_PREFIX)) continue;
        a = json_object(); bk3_aset_s(a, "member", m);
        json_t *ok = bk3_seam(s, "import_external_member", a); json_free(a);
        if (bk3_is_raise(ok)) return ok;
        if (bk3_jtruth(bk3_at(ok, 0))) { restored++; restored_ext++; }
        json_free(ok);
        if ((size_t)restored % 500 == 0) {
            char *pm = bk3_xsf("  %.0f/%.0f files ...", restored, json_get_num(inp, "file_count", 0));
            json_t *pa = json_object(); bk3_aset_s(pa, "msg", pm);
            json_t *pr = bk3_seam(s, "print", pa); json_free(pa); free(pm);
            if (bk3_is_raise(pr)) return pr; json_free(pr);
        }
    }
    json_t *out = json_array(); json_append(out, json_number(restored));
    json_append(out, json_number(restored_ext)); json_append(out, json_array());
    json_append(out, json_array()); json_append(out, json_array()); return out;
}

/* ══════════════ 13. _revive_gateway_after_import ══════════════ */
/* PoP: revive_gateway_after_import @ hermes_cli/backup.py:_revive_gateway_after_import */
static json_t *bk3_revive_gateway_after_import(const bk3_seams_t *s, const json_t *inp) {
    const char *root = bk3_jstr(json_obj_get(inp, "hermes_root")) ?: "";
    json_t *a = json_object();
    json_t *native = bk3_seam(s, "native_default", a); json_free(a);
    if (bk3_is_raise(native)) return native;
    const char *nd = bk3_jstr(native) ?: ""; json_free(native);
    bool same = !strcmp(root, nd);
    if (!same) {
        a = json_object(); bk3_aset_s(a, "root", nd);
        json_t *me = bk3_seam(s, "marker_exists", a); json_free(a);
        if (bk3_is_raise(me)) return me;
        bool clash = bk3_jtruth(me); json_free(me);
        if (!clash) {
            a = json_object();
            json_t *se = bk3_seam(s, "store_exists", a); json_free(a);
            if (bk3_is_raise(se)) return se; clash = bk3_jtruth(se); json_free(se);
        }
        if (clash) {
            char *msg = bk3_xsf("\nRestored into a non-default home; leaving the gateway service alone "
                               "to avoid clashing with the install at %s.\n"
                               "To start a gateway for this home, run:  hermes gateway run", nd);
            a = json_object(); bk3_aset_s(a, "msg", msg);
            json_t *pr = bk3_seam(s, "print", a); json_free(a); free(msg);
            if (bk3_is_raise(pr)) return pr; json_free(pr); return NULL;
        }
    }
    json_t *run = bk3_seam(s, "service_running", json_object());
    if (bk3_is_raise(run)) {
        if (bk3_is_exception(run)) {
            json_t *lr = bk3_log(s, "logger_debug", "gateway revive after import failed", NULL, 0);
            json_free(run); if (bk3_is_raise(lr)) return lr; json_free(lr);
            json_t *a2 = json_object();
            bk3_aset_s(a2, "msg", "\nStart the gateway to activate cron jobs and messaging:\n  hermes gateway run");
            json_t *pr = bk3_seam(s, "print", a2); json_free(a2);
            if (bk3_is_raise(pr)) return pr; json_free(pr); return NULL;
        }
        return run;
    }
    bool running = bk3_jtruth(run); json_free(run);
    if (!running) {
        json_t *a2 = json_object(); bk3_aset_s(a2, "msg", "");
        json_t *pr = bk3_seam(s, "print", a2); json_free(a2);
        if (bk3_is_raise(pr)) return pr; json_free(pr);
        a2 = json_object(); bk3_aset_s(a2, "context", "import");
        json_t *ens = bk3_seam(s, "ensure_service", a2); json_free(a2);
        if (bk3_is_raise(ens)) return ens; json_free(ens);
    }
    return NULL;
}

/* ══════════════ 16. _create_prefixed_full_backup ══════════════ */
/* PoP: create_prefixed_full_backup @ hermes_cli/backup.py:_create_prefixed_full_backup */
static json_t *bk3_create_prefixed_full_backup(const bk3_seams_t *s, const json_t *inp) {
    const char *home = bk3_jstr(json_obj_get(inp, "hermes_home")) ?: "";
    json_t *a = json_object(); bk3_aset_s(a, "path", home);
    json_t *dir = bk3_seam(s, "is_dir", a); json_free(a);
    if (bk3_is_raise(dir)) return dir;
    if (!bk3_jtruth(dir)) { json_free(dir); return json_null(); }
    json_free(dir);
    a = json_object(); bk3_aset_s(a, "path", home);
    json_t *mk = bk3_seam(s, "mkdir", a); json_free(a);
    if (bk3_is_raise(mk)) {
        if (bk3_caught(mk, "OSError")) {
            const char *args[3] = { bk3_jstr(json_obj_get(inp, "what")) ?: "", home, bk3_raise_msg(mk) };
            json_t *lr = bk3_log(s, "logger_warning", "Could not create %s backup dir %s: %s", args, 3);
            if (bk3_is_raise(lr)) return lr; json_free(lr); json_free(mk); return json_null();
        }
        return mk;
    }
    json_free(mk);
    a = json_object(); bk3_aset_s(a, "path", home);
    json_t *ch = bk3_seam(s, "chmod", a); json_free(a);
    if (bk3_is_raise(ch)) { if (!bk3_caught(ch, "OSError")) return ch; json_free(ch); } else json_free(ch);
    a = json_object(); bk3_aset_s(a, "path", home); bk3_aset_s(a, "prefix", bk3_jstr(json_obj_get(inp, "prefix")) ?: "");
    json_t *wb = bk3_seam(s, "write_full_zip", a); json_free(a);
    if (bk3_is_raise(wb)) return wb;
    if (bk3_is_null(wb)) {
        json_free(wb);
        a = json_object(); bk3_aset_s(a, "path", home);
        json_t *pi = bk3_seam(s, "prune_incomplete", a); json_free(a);
        if (bk3_is_raise(pi)) return pi; json_free(pi); return json_null();
    }
    const char *oj = bk3_jstr(wb) ?: "";
    char *outp = strdup(oj); json_free(wb);
    a = json_object(); bk3_aset_s(a, "path", outp);
    json_t *pp = bk3_seam(s, "prune_prefixed", a); json_free(a);
    if (bk3_is_raise(pp)) return pp; json_free(pp);
    return json_string(outp);
}

/* ══════════════ 17. _too_large ══════════════ */
/* PoP: too_large @ hermes_cli/backup.py:_too_large */
static json_t *bk3_too_large(const bk3_seams_t *s, const json_t *inp) {
    const json_t *mx = json_obj_get(inp, "max_file_size");
    if (bk3_is_null(mx)) return json_bool(false);
    double maxsz = json_get_num(mx, "", json_get_num(mx, "__v__", (mx->type == JSON_NUMBER) ? mx->num_val : 0.0));
    json_t *a = json_object(); bk3_aset_s(a, "path", bk3_jstr(json_obj_get(inp, "path")) ?: "");
    json_t *st = bk3_seam(s, "stat_size", a); json_free(a);
    if (bk3_is_raise(st)) { if (bk3_caught(st, "OSError")) { json_free(st); return json_bool(false); } return st; }
    double sz = (st->type == JSON_NUMBER) ? st->num_val : 0.0; json_free(st);
    if (sz <= maxsz) return json_bool(false);
    const char *rel = bk3_jstr(json_obj_get(inp, "rel_name")) ?: "";
    const char *fsz = bk3_jstr(json_obj_get(inp, "fmt_size")) ?: "";
    const char *fmax = bk3_jstr(json_obj_get(inp, "fmt_max")) ?: "";
    char *msg = bk3_xsf("  \xe2\x9a\xa0 Snapshot: skipping %s (%s exceeds %s limit)", rel, fsz, fmax);
    a = json_object(); bk3_aset_s(a, "msg", msg);
    json_t *pr = bk3_seam(s, "print", a); json_free(a); free(msg);
    if (bk3_is_raise(pr)) return pr; json_free(pr);
    const char *args[3] = { rel, fsz, fmax };
    json_t *lr = bk3_log(s, "logger_warning",
        "Quick snapshot skipped %s: %d bytes exceeds %d byte limit", args, 3);
    if (bk3_is_raise(lr)) return lr; json_free(lr);
    return json_bool(true);
}

/* ══════════════ 18. _is_trusted_root_auth_alias ══════════════ */
/* PoP: is_trusted_root_auth_alias @ hermes_cli/backup.py:_is_trusted_root_auth_alias */
static json_t *bk3_is_trusted_root_auth_alias(const bk3_seams_t *s, const json_t *inp) {
    const char *path = bk3_jstr(json_obj_get(inp, "path")) ?: "";
    const char *home = bk3_jstr(json_obj_get(inp, "home")) ?: "";
    json_t *a = json_object(); bk3_aset_s(a, "path", path);
    json_t *sym = bk3_seam(s, "is_symlink", a); json_free(a);
    if (bk3_is_raise(sym)) return sym;
    if (!bk3_jtruth(sym)) { json_free(sym); return json_bool(false); }
    json_free(sym);
    a = json_object(); bk3_aset_s(a, "home", home);
    json_t *root = bk3_seam(s, "get_default_root", a); json_free(a);
    if (bk3_is_raise(root)) {
        if (bk3_caught(root, "OSError") || bk3_caught(root, "RuntimeError") ||
            bk3_caught(root, "ValueError")) { json_free(root); return json_bool(false); }
        return root;
    }
    const char *rj = bk3_jstr(root) ?: "";
    char *rs = strdup(rj); json_free(root);
    a = json_object(); bk3_aset_s(a, "path", home); bk3_aset_s(a, "other", rs);
    json_t *eq = bk3_seam(s, "resolve_equal", a); json_free(a);
    if (bk3_is_raise(eq)) {
        if (bk3_caught(eq, "OSError") || bk3_caught(eq, "RuntimeError") ||
            bk3_caught(eq, "ValueError")) { json_free(eq); return json_bool(false); }
        return eq;
    }
    bool same = bk3_jtruth(eq); json_free(eq);
    if (same) return json_bool(false);
    a = json_object(); bk3_aset_s(a, "path", path); bk3_aset_s(a, "root", rs);
    json_t *pr = bk3_seam(s, "resolve_equal_trusted", a); json_free(a);
    if (bk3_is_raise(pr)) {
        if (bk3_caught(pr, "OSError") || bk3_caught(pr, "ValueError")) { json_free(pr); return json_bool(false); }
        return pr;
    }
    bool trusted = bk3_jtruth(pr); json_free(pr); free(rs); return json_bool(trusted);
}

/* ══════════════ 19. quick_snapshot_dir ══════════════ */
/* PoP: quick_snapshot_dir @ hermes_cli/backup.py:quick_snapshot_dir */
static json_t *bk3_quick_snapshot_dir(const bk3_seams_t *s, const json_t *inp) {
    const char *sid = bk3_jstr(json_obj_get(inp, "snapshot_id")) ?: "";
    json_t *a = json_object(); bk3_aset_s(a, "home", bk3_jstr(json_obj_get(inp, "hermes_home")) ?: "");
    json_t *root = bk3_seam(s, "snapshot_root", a); json_free(a);
    if (bk3_is_raise(root)) return root;
    if (bk3_is_null(root)) { json_free(root); return json_null(); }
    char *rt = strdup(bk3_jstr(root) ?: ""); json_free(root);
    bool invalid = !sid[0] || strchr(sid, '/') || strchr(sid, '\\') ||
                   !strcmp(sid, ".") || !strcmp(sid, "..");
    if (invalid) {
        a = json_object(); bk3_aset_s(a, "fmt", "Invalid snapshot_id: %s");
        json_t *arr = json_array(); json_append(arr, json_string(sid)); json_set(a, "args", arr);
        json_t *er = bk3_seam(s, "logger_error", a); json_free(a);
        if (bk3_is_raise(er)) return er; json_free(er); return json_null();
    }
    a = json_object(); bk3_aset_s(a, "path", sid); bk3_aset_s(a, "root", rt);
    json_t *rel = bk3_seam(s, "relative_to", a); json_free(a);
    if (bk3_is_raise(rel)) {
        if (bk3_caught(rel, "ValueError")) {
            a = json_object(); bk3_aset_s(a, "fmt", "Snapshot path traversal blocked for id: %s");
            json_t *arr = json_array(); json_append(arr, json_string(sid)); json_set(a, "args", arr);
            json_t *er = bk3_seam(s, "logger_error", a); json_free(a);
            if (bk3_is_raise(er)) return er; json_free(er); return json_null();
        }
        return rel;
    }
    json_free(rel);
    char *sd = bk3_xsf("%s/%s", rt, sid);
    a = json_object(); bk3_aset_s(a, "path", sd);
    json_t *id = bk3_seam(s, "is_dir", a); json_free(a);
    if (bk3_is_raise(id)) { free(sd); return id; }
    if (!bk3_jtruth(id)) { json_free(id); free(sd); return json_null(); }
    json_free(id);
    char *mf = bk3_xsf("%s/manifest.json", sd);
    a = json_object(); bk3_aset_s(a, "path", mf);
    json_t *ex = bk3_seam(s, "manifest_exists", a); json_free(a);
    if (bk3_is_raise(ex)) { free(sd); free(mf); return ex; }
    bool ok = bk3_jtruth(ex); json_free(ex); free(mf);
    json_t *res = ok ? json_string(sd) : json_null(); free(sd); return res;
}

/* ══════════════ 20. _restore_quick_snapshot_exclusive (snap None core) ══════════════ */
/* PoP: restore_quick_snapshot_exclusive @ hermes_cli/backup.py:_restore_quick_snapshot_exclusive */
static json_t *bk3_restore_quick_snapshot_exclusive(const bk3_seams_t *s, const json_t *inp) {
    json_t *a = json_object();
    bk3_aset_s(a, "snapshot_id", bk3_jstr(json_obj_get(inp, "snapshot_id")) ?: "");
    bk3_aset_s(a, "home", bk3_jstr(json_obj_get(inp, "home")) ?: "");
    json_t *sd = bk3_seam(s, "quick_snapshot_dir", a); json_free(a);
    if (bk3_is_raise(sd)) return sd;
    if (bk3_is_null(sd)) { json_free(sd); return json_bool(false); }
    /* snap found: manifest-driven restore folded into one seam */
    a = json_object(); bk3_aset_j(a, "manifest", json_obj_get(inp, "manifest"));
    json_t *r = bk3_seam(s, "restore_manifest", a); json_free(a); json_free(sd);
    return r;
}

/* ══════════════ 21. _entry_paths ══════════════ */
/* PoP: entry_paths @ hermes_cli/backup.py:_entry_paths */
static json_t *bk3_entry_paths(const bk3_seams_t *s, const json_t *inp) {
    const char *rel = bk3_jstr(json_obj_get(inp, "rel")) ?: "";
    json_t *a = json_object(); bk3_aset_s(a, "rel", rel);
    json_t *src = bk3_seam(s, "resolve_src", a); json_free(a);
    if (bk3_is_raise(src)) return src;
    bool src_ok = bk3_jtruth(src); json_free(src);
    if (!src_ok) return json_null();
    a = json_object(); bk3_aset_s(a, "rel", rel);
    json_t *dst = bk3_seam(s, "resolve_dst", a); json_free(a);
    if (bk3_is_raise(dst)) return dst;
    bool dst_ok = bk3_jtruth(dst); json_free(dst);
    if (!dst_ok) {
        if (strcmp(rel, "auth.json")) return json_null();
        a = json_object(); bk3_aset_s(a, "rel", rel);
        json_t *tr = bk3_seam(s, "trusted_alias", a); json_free(a);
        if (bk3_is_raise(tr)) return tr;
        if (!bk3_jtruth(tr)) { json_free(tr); return json_null(); }
        json_free(tr);
    }
    json_t *out = json_array();
    char *sp_s = bk3_xsf("snap/%s", rel); json_append(out, json_string(sp_s)); free(sp_s);
    char *sp_d = bk3_xsf("home/%s", rel); json_append(out, json_string(sp_d)); free(sp_d);
    return out;
}

/* ══════════════ 22. restore_config_model_settings_if_rewritten ══════════════ */
/* PoP: restore_config_model_settings_if_rewritten @ hermes_cli/backup.py:restore_config_model_settings_if_rewritten */
static json_t *bk3_restore_config_model_settings_if_rewritten(const bk3_seams_t *s, const json_t *inp) {
    const char *sid = bk3_jstr(json_obj_get(inp, "snapshot_id")) ?: "";
    if (!sid[0]) return json_null();
    json_t *sup = bk3_seam(s, "supports_file_tooling", json_object());
    if (bk3_is_raise(sup)) return sup;
    if (!bk3_jtruth(sup)) { json_free(sup); return json_null(); }
    json_free(sup);
    json_t *a = json_object(); bk3_aset_s(a, "kind", "snap");
    json_t *snap = bk3_seam(s, "read_yaml", a); json_free(a);
    if (bk3_is_raise(snap)) return snap;
    if (!bk3_is_obj(snap) || bk3_len(snap) == 0) { if (snap) json_free(snap); return json_null(); }
    json_free(snap);
    a = json_object(); bk3_aset_s(a, "kind", "live");
    json_t *live = bk3_seam(s, "read_yaml", a); json_free(a);
    if (bk3_is_raise(live)) return live;
    if (bk3_is_null(live)) { json_free(live); return json_null(); }
    json_free(live);
    const json_t *prot = json_obj_get(inp, "protected");
    json_t *keys = json_array();
    for (size_t i = 0; i < bk3_len(prot); i++) {
        const char *dotted = bk3_jstr(bk3_at(prot, i)) ?: "";
        a = json_object(); bk3_aset_s(a, "key", dotted); bk3_aset_s(a, "which", "snap");
        json_t *sv = bk3_seam(s, "get_config_value", a); json_free(a);
        if (bk3_is_raise(sv)) return sv;
        bool empty = bk3_is_null(sv) || (sv->type == JSON_STRING && !bk3_jtruth(sv)) ||
                     (bk3_is_arr(sv) && bk3_len(sv) == 0) || (bk3_is_obj(sv) && bk3_len(sv) == 0);
        if (empty) { json_free(sv); continue; }
        a = json_object(); bk3_aset_s(a, "key", dotted); bk3_aset_s(a, "which", "live");
        json_t *lv = bk3_seam(s, "get_config_value", a); json_free(a);
        if (bk3_is_raise(lv)) { json_free(sv); return lv; }
        /* deep equality by canonical serialization (matches Python == on the
         * JSON-domain values the fixtures use); no extra seam fires. */
        char *css = json_serialize(sv), *cls = json_serialize(lv);
        bool same = !strcmp(css ? css : "null", cls ? cls : "null");
        free(css); free(cls);
        json_free(sv); json_free(lv);
        if (same) continue;
        a = json_object(); bk3_aset_s(a, "key", dotted);
        json_t *st = bk3_seam(s, "set_config_value", a); json_free(a);
        if (bk3_is_raise(st)) return st; json_free(st);
        json_append(keys, json_string(dotted));
    }
    if (bk3_len(keys) == 0) { json_free(keys); return json_null(); }
    json_t *a2 = json_object();
    json_t *wr = bk3_seam(s, "atomic_write", a2); json_free(a2);
    if (bk3_is_raise(wr)) {
        if (bk3_caught(wr, "OSError")) {
            const char *args[1] = { bk3_raise_msg(wr) };
            json_t *lr = bk3_log(s, "logger_error",
                "config.yaml model settings were rewritten during update but auto-restore failed: %s", args, 1);
            json_free(wr); if (bk3_is_raise(lr)) return lr; json_free(lr);
            json_free(keys); return json_null();
        }
        json_free(keys); return wr;
    }
    json_free(wr);
    char *joined = NULL; size_t jn = 0;
    for (size_t i = 0; i < bk3_len(keys); i++) {
        const char *k = bk3_jstr(bk3_at(keys, i)) ?: "";
        size_t need = jn + strlen(k) + (i ? 2 : 1);
        char *tmp = realloc(joined, need + 1);
        if (!tmp) break;
        joined = tmp;
        if (i == 0) { strcpy(joined, k); jn = strlen(k); }
        else { strcat(joined, ", "); strcat(joined, k); jn += strlen(k) + 2; }
    }
    const char *wargs[2] = { joined ? joined : "", sid };
    json_t *lw = bk3_log(s, "logger_warning",
        "Restored user config value(s) %s from pre-update snapshot %s — the update flow rewrote them (#64160)", wargs, 2);
    free(joined);
    if (bk3_is_raise(lw)) { json_free(keys); return lw; } json_free(lw);
    json_t *res = json_object(); json_set(res, "restored", json_bool(true));
    json_set(res, "keys", keys); json_set(res, "snapshot_id", json_string(sid)); return res;
}

/* ══════════════ 23. _db_failure ══════════════ */
/* PoP: db_failure @ hermes_cli/backup.py:_db_failure */
static json_t *bk3_db_failure(const bk3_seams_t *s, const json_t *inp) {
    const char *rel = bk3_jstr(json_obj_get(inp, "rel_path")) ?: "";
    json_t *a = json_object(); bk3_aset_s(a, "fmt", "Full-zip backup aborted: SQLite snapshot failed for %s");
    json_t *arr = json_array(); json_append(arr, json_string(rel)); json_set(a, "args", arr);
    json_t *lr = bk3_seam(s, "logger_warning", a); json_free(a);
    if (bk3_is_raise(lr)) return lr; json_free(lr);
    return bk3_raise_spec("_SQLiteSnapshotError", rel);
}

/* ══════════════ 24. _capped_errors ══════════════ */
/* PoP: capped_errors @ hermes_cli/backup.py:_capped_errors */
static json_t *bk3_capped_errors(const bk3_seams_t *s, const json_t *inp) {
    (void)s;
    const json_t *errs = json_obj_get(inp, "errors");
    size_t n = bk3_len(errs);
    json_t *out = json_object(); (void)out;
    char *buf = strdup(""); size_t used = 0, cap = 0;
    size_t shown = n < 10 ? n : 10;
    for (size_t i = 0; i < shown; i++) {
        const char *e = bk3_jstr(bk3_at(errs, i)) ?: "";
        size_t need = used + strlen(e) + 3;
        char *tmp = realloc(buf, need); if (!tmp) break; buf = tmp; cap = need;
        if (i) { strcat(buf, "; "); used += 2; }
        strcat(buf, e); used += strlen(e);
    }
    (void)cap;
    char *res;
    if (n > 10) res = bk3_xsf("%s (+%zu more)", buf, n - 10);
    else res = strdup(buf);
    free(buf);
    json_t *ret = json_string(res); free(res); return ret;
}

/* ══════════════ 25. _publish_path ══════════════ */
/* PoP: publish_path @ hermes_cli/backup.py:_publish_path */
static json_t *bk3_publish_path(const bk3_seams_t *s, const json_t *inp) {
    (void)s;
    const json_t *errs = json_obj_get(inp, "errors");
    size_t nerr = bk3_len(errs);
    size_t nfiles = (size_t)json_get_num(inp, "nfiles", 0);
    const char *outp = bk3_jstr(json_obj_get(inp, "out_path")) ?: "";
    const char *salv = bk3_jstr(json_obj_get(inp, "salvage_path")) ?: "";
    if (nerr == 0) return json_string(outp);
    if (nerr < nfiles) return json_string(salv);
    return json_null();
}

/* ══════════════ 26. _progress ══════════════ */
/* PoP: progress @ hermes_cli/backup.py:_progress */
static json_t *bk3_progress(const bk3_seams_t *s, const json_t *inp) {
    double i = json_get_num(inp, "i", 0);
    double fc = json_get_num(inp, "file_count", 0);
    char *msg = bk3_xsf("  %.0f/%.0f files ...", i, fc);
    json_t *a = json_object(); bk3_aset_s(a, "msg", msg);
    json_t *pr = bk3_seam(s, "print", a); json_free(a); free(msg);
    if (bk3_is_raise(pr)) return pr; json_free(pr);
    const char *args[2] = { bk3_xsf("%.0f", i), bk3_xsf("%.0f", fc) };
    json_t *lr = bk3_log(s, "logger_info", "backup phase=archive status=progress completed=%d total=%d",
                         args, 2);
    free((void*)args[0]); free((void*)args[1]);
    if (bk3_is_raise(lr)) return lr; json_free(lr);
    return NULL;
}
