/*
 * session_recovery_flow2.c — "hermes_cli/session_recovery.py remainder" (the
 * post-refactor recovery implementation that lives at
 * C:/Users/eman5/hermes-agent/persistence/sqlite/admin/_recovery.py). C11 port of
 * 12 defs NO existing PoP claims (CRLF-aware PoP audit, 2026-11-20).
 *
 * LIVE authority: persistence/sqlite/admin/_recovery.py. PoP lines carry the
 * real defining path so coverage scans credit the correct module.
 *
 * Ported (all UNCLAIMED before this file):
 *   _count_rows                      count_rows seam + int()
 *   _quoted_columns                  pure (a, b) tuple projection
 *   _journal_mode                    journal_mode seam + str().lower()
 *   _compatible_columns              table_columns seam x2 + status carrier
 *   _state_meta_result               pure dict builder (sorted generated keys)
 *   _state_meta_precheck             table_columns seam x1..2 + RESULT composed REAL
 *   _reconcile                       reconcile_count / reconcile_mutate seams
 *   _immediate_transaction           generator CM: BEGIN / COMMIT / ROLLBACK
 *   _verify_fts_indexes              table_columns/fts_insert/fts_match seams,
 *                                    except sqlite3.DatabaseError
 *   _verify_structure                integrity/fk/journal/schema/fts_meta seams
 *   _sanitize_session_model_config   table_columns + TRANSACTION + RECONCILE real
 *   _recovery_report                 source_fingerprint seam + verdict builder
 *
 * Sibling defs already claimed by an existing PoP are NEVER re-ported here:
 *   _table_columns, _source_fingerprint, _verify_recovered_database,
 *   _finalize_derived_metadata, ... enter through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * args is a BORROWED object (the seam deep-copies it for the trace); the result
 * is a NEW json_t* the caller frees. NULL == Python None (never a JSON_NULL
 * node); {"__raise__": cls, "msg": ...} == a raised exception. Full CPython
 * 3.14 builtin hierarchy (+ sqlite3.Error family) drives isinstance-style catch
 * folding: BaseException kinds pass EVERY catch; except sqlite3.DatabaseError
 * folds DatabaseError subclasses only.
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__": ...} object == a propagated exception. Tuple
 * returns project to 2-element arrays. Mutation flows pass a BORROWED carrier
 * (result / verification) which they mutate IN PLACE; the harness emits it as
 * "state" post-hoc, including on raise (partial mutation is observable).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool sr2_jtruth(const json_t *v) {
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
static bool sr2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool sr2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static const json_t *sr2_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT && k) ? json_obj_get(o, k) : NULL;
}
static char *sr2_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}
/* Python str() for the fixture domain: scalars only, integral numbers as ints. */
static char *sr2_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 9.2233720368547758e18)
                return sr2_xsf("%.0f", d);
            return sr2_xsf("%.17g", d);
        }
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default:          return strdup("");  /* containers outside domain */
    }
}
static const char *sr2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == floor(d) && fabs(d) < 9.2233720368547758e18) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
static char *sr2_lower(const char *s) {
    char *out = strdup(s ? s : "");
    for (char *q = out; q && *q; q++)
        if (*q >= 'A' && *q <= 'Z') *q = (char)(*q + ('a' - 'A'));
    return out;
}
static json_t *sr2_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls ? cls : "Exception"));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool sr2_is_raise(const json_t *r) {
    return sr2_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *sr2_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "Exception");
}
static const char *sr2_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}
static void sr2_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}
/* int(v) projection. Returns a NEW number, or NULL with *err set to a raise spec. */
static json_t *sr2_to_int(const json_t *v, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = sr2_raise("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return NULL;
    }
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isnan(d) || isinf(d))
            { *err = sr2_raise("OverflowError", "cannot convert float infinity to integer"); return NULL; }
        return json_number(d == floor(d) ? d : (d > 0 ? floor(d) : ceil(d)));
    }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        long long n = strtoll(v->str_val ? v->str_val : "", &end, 10);
        if (end == (v->str_val ? v->str_val : "")) {
            char *m = sr2_xsf("invalid literal for int() with base 10: '%s'",
                              v->str_val ? v->str_val : "");
            *err = sr2_raise("ValueError", m); free(m); return NULL;
        }
        return json_number((double)n);
    }
    char *m = sr2_xsf("int() argument must be a string, a bytes-like object or a real number, not '%s'",
                      sr2_type_name(v));
    *err = sr2_raise("TypeError", m); free(m); return NULL;
}
/* recursive JSON equality (object key order ignored — Python dict ==). */
static bool sr2_json_eq(const json_t *a, const json_t *b) {
    if (!a && !b) return true;
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL:   return true;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
        case JSON_ARRAY: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!sr2_json_eq(json_get(a, i), json_get(b, i))) return false;
            return true;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !sr2_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        }
    }
    return false;
}

/* ── universal seam ── */
typedef struct sr2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} sr2_seams_t;
/* sr2_call OWNS args; the seam BORROWS it. Results are NEW (caller frees). */
static json_t *sr2_call(const sr2_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) { json_free(args); return sr2_raise("RuntimeError", "no seam installed"); }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static json_t *sr2_args1(const char *k, const json_t *v) {
    json_t *a = json_object(); sr2_set_node(a, k, v); return a;
}
static json_t *sr2_args2(const char *k1, const json_t *v1, const char *k2, const json_t *v2) {
    json_t *a = json_object(); sr2_set_node(a, k1, v1); sr2_set_node(a, k2, v2); return a;
}

/* ── FULL builtin exception hierarchy (CPython 3.14, child -> parent) ──
 * Catch sites here: except sqlite3.DatabaseError (_verify_fts_indexes) and
 * except BaseException (_immediate_transaction). The full table is carried so
 * scripted subclass names fold into their ancestors exactly as live isinstance
 * does. sqlite3.Error/DatabaseError and subclasses are included. ── */
typedef struct sr2_exc_pair { const char *child; const char *parent; } sr2_exc_pair_t;
static const sr2_exc_pair_t SR2_EXC_TREE[] = {
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"Exception", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    /* sqlite3 family (stub classes, identical both sides) */
    {"Error", "Exception"},
    {"DatabaseError", "Error"},
    {"DataError", "DatabaseError"},
    {"IntegrityError", "DatabaseError"},
    {"InternalError", "DatabaseError"},
    {"NotSupportedError", "DatabaseError"},
    {"OperationalError", "DatabaseError"},
    {"ProgrammingError", "DatabaseError"},
};
static const char *sr2_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(SR2_EXC_TREE) / sizeof(SR2_EXC_TREE[0]); i++)
        if (!strcmp(SR2_EXC_TREE[i].child, c)) return SR2_EXC_TREE[i].parent;
    return NULL;
}
static bool sr2_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = sr2_exc_parent(c);
    }
    return false;
}
/* An unscripted custom class (unknown parent) is an Exception subclass on both
 * sides (build_exc: type(cls,(Exception,))). */
static const char *sr2_resolve_parent(const char *c) {
    const char *p = sr2_exc_parent(c);
    return p ? p : "Exception";
}

/* ── domain constants (LIVE values from _recovery.py + sibling modules) ── */
static const char *SR2_GENERATED_META_KEYS[] = {
    "fts_cjk_rebuild_high_water", "fts_cjk_rebuild_progress", "fts_cjk_stale",
    "fts_optimize_available", "fts_rebuild_high_water", "fts_rebuild_progress",
    "fts_storage_version", "fts_tool_full_content_high_water",
    "telegram_dm_topic_schema_version",
};
static const char *SR2_FTS_PENDING[] = {   /* generated fts_ keys minus fts_storage_version */
    "fts_cjk_rebuild_high_water", "fts_cjk_rebuild_progress", "fts_cjk_stale",
    "fts_optimize_available", "fts_rebuild_high_water", "fts_rebuild_progress",
    "fts_tool_full_content_high_water",
};
static const char *SR2_RUNTIME_TABLES[] = {
    "runtime_epoch", "session_admissions", "worker_executions", "worker_receipts",
};
#define SR2_SCHEMA_VERSION 33
#define SR2_FTS_STORAGE_VERSION "3"
static json_t *sr2_str_array(const char *const *v, size_t n) {
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(v[i]));
    return a;
}
/* membership test for a string in a json string array */
static bool sr2_arr_has_str(const json_t *a, const char *s) {
    if (!sr2_is_arr(a) || !s) return false;
    for (size_t i = 0; i < a->c.count; i++) {
        const json_t *v = json_get(a, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return true;
    }
    return false;
}
static bool sr2_has_kv(const json_t *cols, const char *k, const char *v) {
    if (!sr2_is_obj(cols)) return false;
    const json_t *x = json_obj_get(cols, k);
    return x && x->type == JSON_STRING && x->str_val && !strcmp(x->str_val, v);
}
/* sorted([key for key in _GENERATED_META_KEYS if key in meta]) */
static json_t *sr2_pending_keys(const json_t *meta) {
    json_t *a = json_array();
    for (size_t i = 0; i < sizeof(SR2_FTS_PENDING) / sizeof(SR2_FTS_PENDING[0]); i++)
        if (sr2_get(meta, SR2_FTS_PENDING[i])) json_append(a, json_string(SR2_FTS_PENDING[i]));
    return a;  /* SR2_FTS_PENDING is already lexicographically sorted */
}
/* fetchone-row first element as int (raises IndexError / TypeError on bad shape) */
static json_t *sr2_first_int(const json_t *row, json_t **err) {
    *err = NULL;
    if (!row) {
        *err = sr2_raise("TypeError", "'NoneType' object is not subscriptable");
        return NULL;
    }
    if (row->type != JSON_ARRAY) {
        char *m = sr2_xsf("'%s' object is not subscriptable", sr2_type_name(row));
        *err = sr2_raise("TypeError", m); free(m); return NULL;
    }
    if (row->c.count == 0) { *err = sr2_raise("IndexError", "list index out of range"); return NULL; }
    return sr2_to_int(json_get(row, 0), err);
}
/* fetchone-row first element as str(value).lower() */
static json_t *sr2_first_lower(const json_t *row, json_t **err) {
    *err = NULL;
    if (!row) { *err = sr2_raise("TypeError", "'NoneType' object is not subscriptable"); return NULL; }
    if (row->type != JSON_ARRAY) {
        char *m = sr2_xsf("'%s' object is not subscriptable", sr2_type_name(row));
        *err = sr2_raise("TypeError", m); free(m); return NULL;
    }
    if (row->c.count == 0) { *err = sr2_raise("IndexError", "list index out of range"); return NULL; }
    char *s = sr2_str_of(json_get(row, 0));
    char *l = sr2_lower(s); free(s);
    json_t *out = json_string(l); free(l);
    return out;
}

/* ═══════════════════════ ported functions ═══════════════════════ */

/* PoP: sr2_count_rows @ persistence/sqlite/admin/_recovery.py:_count_rows */
static json_t *sr2_count_rows(const sr2_seams_t *sp, const char *table) {
    json_t *r = sr2_call(sp, "count_rows",
                         sr2_args1("table", table ? json_string(table) : json_null()));
    if (sr2_is_raise(r)) return r;
    if (!r) return sr2_raise("TypeError", "'NoneType' object is not subscriptable");
    json_t *e = NULL;
    json_t *out = sr2_first_int(r, &e);
    json_free(r);
    return out ? out : e;
}

/* PoP: sr2_quoted_columns @ persistence/sqlite/admin/_recovery.py:_quoted_columns */
static json_t *sr2_quoted_columns(const json_t *columns) {
    char *buf = NULL; size_t cap = 0, used = 0;
    /* part 1: ", ".join(f'"{c}"' for c in columns) */
    for (size_t i = 0; i < (columns && sr2_is_arr(columns) ? columns->c.count : 0); i++) {
        char *s = sr2_str_of(json_get(columns, i));
        size_t need = strlen(s) + 4 + (i ? 2 : 1);
        if (used + need + 1 > cap) { cap = (used + need + 1) * 2; buf = realloc(buf, cap); }
        if (i && used) { buf[used++] = ','; buf[used++] = ' '; }
        buf[used++] = '"'; memcpy(buf + used, s, strlen(s)); used += strlen(s); buf[used++] = '"';
        free(s);
    }
    if (!buf) { buf = malloc(1); }
    buf[used] = '\0';
    json_t *a = json_array();
    json_append(a, json_string(buf));
    free(buf);
    /* part 2: ", ".join("?" for _ in columns) */
    size_t n = (columns && sr2_is_arr(columns)) ? columns->c.count : 0;
    char *ph = malloc(n * 2 + 1);
    size_t p = 0;
    for (size_t i = 0; i < n; i++) {
        if (i) { ph[p++] = ','; ph[p++] = ' '; }
        ph[p++] = '?';
    }
    ph[p] = '\0';
    json_append(a, json_string(ph));
    free(ph);
    return a;
}

/* PoP: sr2_journal_mode @ persistence/sqlite/admin/_recovery.py:_journal_mode */
static json_t *sr2_journal_mode(const sr2_seams_t *sp) {
    json_t *r = sr2_call(sp, "journal_mode", json_object());
    if (sr2_is_raise(r) || !r) return r;  /* None row -> None (live: `if row else None`) */
    if (sr2_is_arr(r) && r->c.count == 0) { json_free(r); return NULL; }  /* falsy [] row */
    json_t *e = NULL;
    json_t *out = sr2_first_lower(r, &e);
    json_free(r);
    return out ? out : e;
}

/* PoP: sr2_compatible_columns @ persistence/sqlite/admin/_recovery.py:_compatible_columns */
static json_t *sr2_compatible_columns(const sr2_seams_t *sp, const char *table, json_t *result) {
    json_t *v = table ? json_string(table) : json_null();
    json_t *r1 = sr2_call(sp, "table_columns", sr2_args1("table", json_copy(v)));
    if (sr2_is_raise(r1)) { json_free(v); return r1; }
    json_t *r2 = sr2_call(sp, "table_columns", sr2_args1("table", v));
    if (sr2_is_raise(r2)) { json_free(r1); return r2; }
    /* fetchall seam: scripted null == empty column list (Conn rows=[]). */
    json_t *src = (r1 && sr2_is_arr(r1)) ? r1 : json_array();
    json_t *dst = (r2 && sr2_is_arr(r2)) ? r2 : json_array();
    json_t *cols = json_array();
    for (size_t i = 0; i < dst->c.count; i++) {
        const json_t *d = json_get(dst, i);
        char *s = sr2_str_of(d);
        if (sr2_arr_has_str(src, s)) json_append(cols, json_string(s));
        free(s);
    }
    json_set(result, "columns", json_copy(cols));
    json_t *out = NULL;
    if (src->c.count == 0) {
        json_set(result, "status", json_string("missing"));
    } else if (cols->c.count == 0) {
        json_set(result, "status", json_string("failed"));
        json_set(result, "error", json_string("source and destination have no compatible columns"));
    } else {
        out = json_copy(cols);
    }
    if (src != r1) json_free(src);
    if (dst != r2) json_free(dst);
    json_free(cols); json_free(r1); json_free(r2);
    return out;  /* NULL == None (terminal status set on result) */
}

/* PoP: sr2_state_meta_result @ persistence/sqlite/admin/_recovery.py:_state_meta_result */
static json_t *sr2_state_meta_result(const json_t *source_rows, const json_t *extra) {
    json_t *o = json_object();
    sr2_set_node(o, "source_meta_rows", source_rows);
    json_set(o, "copied_rows", json_number(0));
    json_t *cols = json_array(); json_append(cols, json_string("key")); json_append(cols, json_string("value"));
    json_set(o, "columns", cols);
    json_set(o, "excluded_keys", sr2_str_array(SR2_GENERATED_META_KEYS,
             sizeof(SR2_GENERATED_META_KEYS) / sizeof(SR2_GENERATED_META_KEYS[0])));
    if (sr2_is_obj(extra))
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(o, extra->c.keys[i], json_copy(extra->c.items[i]));
    return o;
}

/* PoP: sr2_state_meta_precheck @ persistence/sqlite/admin/_recovery.py:_state_meta_precheck */
static json_t *sr2_state_meta_precheck(const sr2_seams_t *sp, const json_t *source_rows, bool salvage) {
    json_t *extra = json_object();
    if (salvage) json_set(extra, "mode", json_string("rowid_range_salvage"));
    json_t *r1 = sr2_call(sp, "table_columns", sr2_args1("table", json_string("state_meta")));
    if (sr2_is_raise(r1)) { json_free(extra); return r1; }
    json_t *src = (r1 && sr2_is_arr(r1)) ? r1 : NULL;
    bool have_kv = src && sr2_arr_has_str(src, "key") && sr2_arr_has_str(src, "value");
    if (!have_kv) {
        json_t *e2 = json_copy(extra);
        if (salvage && src && src->c.count) {
            char *buf = NULL; size_t cap = 0, used = 0;
            for (size_t i = 0; i < src->c.count; i++) {
                char *s = sr2_str_of(json_get(src, i));
                size_t need = strlen(s) + 3;
                if (used + need + 1 > cap) { cap = (used + need + 1) * 2; buf = realloc(buf, cap); }
                if (used) { buf[used++] = ','; buf[used++] = ' '; }
                memcpy(buf + used, s, strlen(s)); used += strlen(s); free(s);
            }
            if (!used) { free(buf); buf = strdup("none"); }
            else buf[used] = '\0';
            char *m = sr2_xsf("source state_meta exists but is missing the key/value columns (found: %s)", buf);
            json_set(e2, "status", json_string("failed"));
            json_set(e2, "error", json_string(m));
            free(m); free(buf);
        } else {
            json_set(e2, "status", json_string("missing"));
        }
        json_t *out = sr2_state_meta_result(source_rows, e2);
        json_free(e2); json_free(extra); json_free(r1);
        return out;
    }
    json_t *r2 = sr2_call(sp, "table_columns", sr2_args1("table", json_string("state_meta")));
    if (sr2_is_raise(r2)) { json_free(extra); json_free(r1); return r2; }
    json_t *dst = (r2 && sr2_is_arr(r2)) ? r2 : NULL;
    bool dkv = dst && sr2_arr_has_str(dst, "key") && sr2_arr_has_str(dst, "value");
    json_t *out = NULL;
    if (!dkv) {
        json_t *e2 = json_copy(extra);
        json_set(e2, "status", json_string("failed"));
        json_set(e2, "error", json_string("destination state_meta schema is incomplete"));
        out = sr2_state_meta_result(source_rows, e2);
        json_free(e2);
    }
    json_free(extra); json_free(r1); json_free(r2);
    return out;  /* NULL == None (both schemas usable) */
}

/* PoP: sr2_reconcile @ persistence/sqlite/admin/_recovery.py:_reconcile */
static json_t *sr2_reconcile(const sr2_seams_t *sp, const char *table,
                             const char *where, const char *mutation) {
    json_t *r = sr2_call(sp, "reconcile_count",
        sr2_args2("table", table ? json_string(table) : json_null(),
                  "where", where ? json_string(where) : json_null()));
    if (sr2_is_raise(r)) return r;
    if (!r) return sr2_raise("TypeError", "'NoneType' object is not subscriptable");
    json_t *e = NULL;
    json_t *cnt = sr2_first_int(r, &e);
    json_free(r);
    if (e) return e;
    if (sr2_jtruth(cnt)) {
        json_t *m = sr2_call(sp, "reconcile_mutate",
            sr2_args2("mutation", mutation ? json_string(mutation) : json_null(),
                      "where", where ? json_string(where) : json_null()));
        if (sr2_is_raise(m)) { json_free(cnt); return m; }
        json_free(m);  /* scripted None (NULL) is a normal result, not an error */
    }
    return cnt;
}

/* PoP: sr2_immediate_transaction @ persistence/sqlite/admin/_recovery.py:_immediate_transaction */
static json_t *sr2_immediate_transaction(const sr2_seams_t *sp, const json_t *outcome) {
    json_t *b = sr2_call(sp, "begin", json_object());
    if (sr2_is_raise(b)) return b;
    json_free(b);
    if (sr2_is_raise(outcome)) {
        json_t *rb = sr2_call(sp, "rollback", json_object());
        json_free(rb);                       /* rollback result is discarded on the raise path */
        return json_copy(outcome);           /* the SAME exception re-raised */
    }
    json_t *c = sr2_call(sp, "commit", json_object());
    if (sr2_is_raise(c)) {
        json_t *rb = sr2_call(sp, "rollback", json_object());
        json_free(rb);
        return c;
    }
    json_free(c);
    return NULL;  /* None — generator exhausted normally */
}

/* helper: append "%s integrity check failed: %s" to verification.errors on a
 * scripted DatabaseError. Returns true when the exception was handled. */
static bool sr2_fts_exc(json_t *verification, const char *table, json_t *exc, json_t *checks) {
    const char *cls = sr2_raise_class(exc);
    if (!sr2_cls_is(cls, "DatabaseError")) return false;  /* not caught: propagate */
    const char *msg = sr2_raise_msg(exc);
    json_set(checks, table, json_string(msg));
    json_t *errors = (json_t *)sr2_get(verification, "errors");
    if (!sr2_is_arr(errors)) {
        errors = json_array(); json_set(verification, "errors", errors);
    }
    char *m = sr2_xsf("%s integrity check failed: %s", table, msg);
    json_append(errors, json_string(m)); free(m);
    json_free(exc);
    return true;
}

/* PoP: sr2_verify_fts_indexes @ persistence/sqlite/admin/_recovery.py:_verify_fts_indexes */
static json_t *sr2_verify_fts_indexes(const sr2_seams_t *sp, json_t *verification) {
    static const char *TABLES[] = {"messages_fts", "messages_fts_trigram"};
    json_t *checks = json_object();
    for (size_t i = 0; i < 2; i++) {
        const char *table = TABLES[i];
        json_t *tc = sr2_call(sp, "table_columns", sr2_args1("table", json_string(table)));
        if (sr2_is_raise(tc)) { json_free(checks); return tc; }
        bool present = (tc && tc->c.count != 0);  /* fetchall null == [] */
        json_free(tc);
        if (!present) continue;
        json_t *ins = sr2_call(sp, "fts_insert", sr2_args1("table", json_string(table)));
        if (sr2_is_raise(ins)) {
            if (!sr2_cls_is(sr2_raise_class(ins), "DatabaseError")) {
                json_t *e = ins; json_free(checks); return e;  /* propagate */
            }
            if (!sr2_fts_exc(verification, table, ins, checks)) { json_free(checks); return NULL; }
            continue;
        }
        json_free(ins);
        json_t *mat = sr2_call(sp, "fts_match", sr2_args1("table", json_string(table)));
        if (sr2_is_raise(mat)) {
            if (!sr2_cls_is(sr2_raise_class(mat), "DatabaseError")) {
                json_t *e = mat; json_free(checks); return e;  /* propagate */
            }
            if (!sr2_fts_exc(verification, table, mat, checks)) { json_free(checks); return NULL; }
            continue;
        }
        json_free(mat);
        json_set(checks, table, json_string("ok"));
    }
    json_set(verification, "fts_checks", checks);
    return NULL;
}

/* PoP: sr2_verify_structure @ persistence/sqlite/admin/_recovery.py:_verify_structure */
static json_t *sr2_verify_structure(const sr2_seams_t *sp, json_t *verification) {
    json_t *errors = (json_t *)sr2_get(verification, "errors");
    if (!sr2_is_arr(errors)) { errors = json_array(); json_set(verification, "errors", errors); }
    /* integrity_check */
    json_t *ir = sr2_call(sp, "integrity_check", json_object());
    if (sr2_is_raise(ir)) return ir;
    json_t *integrity = json_array();
    for (size_t i = 0; ir && i < ir->c.count; i++) {
        const json_t *row = json_get(ir, i);
        const json_t *v = (sr2_is_arr(row) && row->c.count) ? json_get(row, 0) : NULL;
        char *s = sr2_str_of(v); json_append(integrity, json_string(s)); free(s);
    }
    json_free(ir);
    json_set(verification, "integrity_check", integrity);
    bool ok = (integrity->c.count == 1);
    if (ok) {
        const json_t *x = json_get(integrity, 0);
        ok = x && x->type == JSON_STRING && x->str_val && !strcmp(x->str_val, "ok");
    }
    if (!ok) json_append(errors, json_string("PRAGMA integrity_check did not return exactly 'ok'"));
    /* foreign_key_check */
    json_t *fr = sr2_call(sp, "foreign_key_check", json_object());
    if (sr2_is_raise(fr)) return fr;
    json_t *fk = fr ? json_copy(fr) : json_array();  /* fetchall null == [] */
    json_set(verification, "foreign_key_check", fk);
    if (fk->c.count) json_append(errors, json_string("foreign key violations remain"));
    json_free(fr);
    /* journal_mode (compose REAL) */
    json_t *jm = sr2_journal_mode(sp);
    if (sr2_is_raise(jm)) return jm;
    json_set(verification, "journal_mode", jm ? jm : json_null());
    /* schema_version */
    json_t *sv = sr2_call(sp, "schema_version", json_object());
    if (sr2_is_raise(sv)) return sv;
    json_t *ver = NULL;
    if (sv && sv->c.count) {
        json_t *e = NULL; json_t *row = (sr2_is_arr(sv) && sv->c.count) ? json_get(sv, 0) : NULL;
        ver = sr2_to_int(row, &e);
        if (e) { json_free(sv); json_free(ver); return e; }
        json_free(sv);
    } else {
        json_free(sv);
        ver = json_null();  /* fetchone None/[] -> schema_version None */
    }
    json_set(verification, "schema_version", json_copy(ver));
    bool sv_ok = (ver->type == JSON_NUMBER && ver->num_val == (double)SR2_SCHEMA_VERSION);
    if (!sv_ok) {
        char *s = sr2_str_of(ver);
        char *m = sr2_xsf("schema version is %s, expected %d", s, SR2_SCHEMA_VERSION);
        json_append(errors, json_string(m)); free(m); free(s);
    }
    json_free(ver);
    /* fts_meta */
    json_t *mr = sr2_call(sp, "fts_meta", json_object());
    if (sr2_is_raise(mr)) return mr;
    json_t *meta = json_object();
    for (size_t i = 0; mr && i < mr->c.count; i++) {
        const json_t *row = json_get(mr, i);
        if (!sr2_is_arr(row) || row->c.count < 2) continue;
        char *k = sr2_str_of(json_get(row, 0));
        json_set(meta, k, json_copy(json_get(row, 1)));
        free(k);
    }
    json_free(mr);
    json_set(verification, "fts_meta", json_copy(meta));
    const json_t *fv = json_obj_get(meta, "fts_storage_version");
    char *fvs = sr2_str_of(fv);
    if (strcmp(fvs, SR2_FTS_STORAGE_VERSION)) {
        json_append(errors, json_string("fresh FTS storage version was not established"));
    }
    free(fvs);
    json_t *pending = sr2_pending_keys(meta);
    json_set(verification, "pending_fts_keys", json_copy(pending));
    if (pending->c.count)
        json_append(errors, json_string("derived FTS transition markers remain in the recovered database"));
    json_free(pending); json_free(meta);
    return NULL;
}

/* PoP: sr2_sanitize_session_model_config @ persistence/sqlite/admin/_recovery.py:_sanitize_session_model_config */
static json_t *sr2_sanitize_session_model_config(const sr2_seams_t *sp) {
    json_t *tc = sr2_call(sp, "table_columns", sr2_args1("table", json_string("sessions")));
    if (sr2_is_raise(tc)) return tc;
    bool has = sr2_arr_has_str(tc, "model_config");  /* fetchall null == [] */
    json_free(tc);
    if (!has) return json_number(0);
    json_t *b = sr2_call(sp, "begin", json_object());
    if (sr2_is_raise(b)) return b;
    json_free(b);
    json_t *rc = sr2_reconcile(sp, "sessions",
        "model_config IS NOT NULL AND json_valid(model_config) = 0",
        "UPDATE sessions SET model_config = '{}'");
    if (sr2_is_raise(rc)) {
        json_t *rb = sr2_call(sp, "rollback", json_object()); json_free(rb);
        return rc;
    }
    json_t *c = sr2_call(sp, "commit", json_object());
    if (sr2_is_raise(c)) {
        json_t *rb = sr2_call(sp, "rollback", json_object()); json_free(rb);
        json_free(rc); return c;
    }
    json_free(c);
    return rc;
}

/* PoP: sr2_recovery_report @ persistence/sqlite/admin/_recovery.py:_recovery_report */
static json_t *sr2_recovery_report(const sr2_seams_t *sp, const char *source, const char *output,
    const json_t *inspection, const json_t *disk_space, json_t *verification,
    const char *on_source_change, const json_t *fields) {
    json_t *fp = sr2_call(sp, "source_fingerprint", sr2_args1("path", source ? json_string(source) : json_null()));
    if (sr2_is_raise(fp)) return fp;
    bool unchanged = sr2_json_eq(fp, sr2_get(inspection, "source_fingerprint"));
    if (!unchanged) {
        json_t *errors = (json_t *)sr2_get(verification, "errors");
        if (!sr2_is_arr(errors)) { errors = json_array(); json_set(verification, "errors", errors); }
        json_append(errors, json_string("the source database bundle changed during recovery"));
        json_set(verification, on_source_change, json_bool(false));
    }
    json_t *o = json_object();
    json_set(o, "operation", json_string("recover"));
    json_set(o, "source", json_string(source ? source : ""));
    json_set(o, "output", json_string(output ? output : ""));
    sr2_set_node(o, "source_bundle", sr2_get(inspection, "source_bundle"));
    sr2_set_node(o, "source_fingerprint", sr2_get(inspection, "source_fingerprint"));
    json_set(o, "source_unchanged", json_bool(unchanged));
    sr2_set_node(o, "disk_space", disk_space);
    json_t *ins = json_object();
    sr2_set_node(ins, "journal_mode", sr2_get(inspection, "journal_mode"));
    sr2_set_node(ins, "tables", sr2_get(inspection, "tables"));
    sr2_set_node(ins, "errors", sr2_get(inspection, "errors"));
    sr2_set_node(ins, "warnings", sr2_get(inspection, "warnings"));
    json_set(o, "inspection", ins);
    if (sr2_is_obj(fields))
        for (size_t i = 0; i < fields->c.count; i++)
            json_set(o, fields->c.keys[i], json_copy(fields->c.items[i]));
    json_set(o, "verification", json_copy(verification));
    bool complete = sr2_jtruth(sr2_get(verification, "complete")) && unchanged;
    bool partial = sr2_jtruth(sr2_get(verification, "loss_detected"));
    bool verified = sr2_jtruth(sr2_get(verification, "healthy")) && unchanged;
    json_set(o, "complete", json_bool(complete));
    json_set(o, "partial", json_bool(partial));
    json_set(o, "verified", json_bool(verified));
    json_set(o, "installed", json_bool(false));
    json_t *runtime = json_object();
    json_set(runtime, "policy", json_string("transcript-only; runtime ledger excluded; fresh epoch on activation"));
    json_t *excluded = json_object();
    const json_t *tables = sr2_get(inspection, "tables");
    for (size_t i = 0; i < sizeof(SR2_RUNTIME_TABLES) / sizeof(SR2_RUNTIME_TABLES[0]); i++) {
        const json_t *tv = sr2_get(tables, SR2_RUNTIME_TABLES[i]);
        json_set(excluded, SR2_RUNTIME_TABLES[i], tv ? json_copy(tv) : json_object());
    }
    json_set(runtime, "excluded_tables", excluded);
    json_set(o, "runtime_state", runtime);
    json_free(fp);
    return o;
}
