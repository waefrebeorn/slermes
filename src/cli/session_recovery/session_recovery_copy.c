/* session_recovery_copy.c — table inspection + chunked copy + rowid-range
 * salvage. Faithful port of hermes_cli/session_recovery.py (copy slice).
 */
#define _GNU_SOURCE
#include "session_recovery_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool sr_columns_contains(const json_t *cols, const char *name) {
    if (!cols) return false;
    for (size_t i = 0; i < json_len(cols); i++) {
        const json_t *c = json_get(cols, i);
        if (c && c->type == JSON_STRING && strcmp(c->str_val, name) == 0)
            return true;
    }
    return false;
}

/* PoP: sr_table_columns @ hermes_cli/session_recovery.py:_table_columns */
json_t *sr_table_columns(sqlite3 *conn, const char *table) {
    json_t *cols = json_array();
    char sql[512];
    snprintf(sql, sizeof(sql), "PRAGMA table_info(\"%s\")", table);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK) return cols;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(st, 1);
        json_append(cols, json_string(name ? (const char *)name : ""));
    }
    sqlite3_finalize(st);
    return cols;
}

/* PoP: sr_table_inventory @ hermes_cli/session_recovery.py:_table_inventory */
json_t *sr_table_inventory(sqlite3 *conn, const char *table) {
    json_t *result = json_object();
    json_set(result, "available", json_bool(false));
    json_set(result, "columns", json_array());
    json_set(result, "rows", json_null());

    json_t *columns = sr_table_columns(conn, table);
    if (json_len(columns) == 0) { json_free(columns); return result; }
    json_set(result, "available", json_bool(true));
    json_set(result, "columns", columns);

    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM \"%s\"", table);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) == SQLITE_OK) {
        int rc = sqlite3_step(st);
        if (rc == SQLITE_ROW) {
            json_set(result, "rows",
                     json_number((double)sqlite3_column_int64(st, 0)));
        } else {
            json_set(result, "error",
                     json_string(sqlite3_errmsg(conn)));
        }
        sqlite3_finalize(st);
    } else {
        json_set(result, "error", json_string(sqlite3_errmsg(conn)));
        if (st) sqlite3_finalize(st);
    }
    return result;
}

/* PoP: sr_inspect_connection @ hermes_cli/session_recovery.py:_inspect_connection */
json_t *sr_inspect_connection(sqlite3 *conn) {
    sqlite3_exec(conn, "PRAGMA writable_schema=ON", NULL, NULL, NULL);
    json_t *report = json_object();
    json_t *tables = json_object();
    json_t *errors = json_array();
    json_t *warnings = json_array();
    json_set(report, "tables", tables);
    json_set(report, "errors", errors);
    json_set(report, "warnings", warnings);

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA journal_mode", -1, &st, NULL) == SQLITE_OK
        && sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *jm = sqlite3_column_text(st, 0);
        char low[64] = "";
        if (jm) {
            size_t i = 0;
            for (; jm[i] && i < 63; i++)
                low[i] = (char)((jm[i] >= 'A' && jm[i] <= 'Z') ? jm[i] + 32 : jm[i]);
            low[i] = 0;
        }
        json_set(report, "journal_mode", jm ? json_string(low) : json_null());
    } else {
        json_set(report, "journal_mode", json_null());
        char msg[512];
        snprintf(msg, sizeof(msg), "journal mode: %s", sqlite3_errmsg(conn));
        json_append(warnings, json_string(msg));
    }
    if (st) sqlite3_finalize(st);

    for (size_t i = 0; i < 6; i++)
        json_set(tables, SR_CANONICAL_TABLES[i],
                 sr_table_inventory(conn, SR_CANONICAL_TABLES[i]));
    json_set(tables, "state_meta", sr_table_inventory(conn, "state_meta"));
    for (size_t i = 0; i < 2; i++)
        json_set(tables, SR_TOPIC_TABLES[i],
                 sr_table_inventory(conn, SR_TOPIC_TABLES[i]));

    const char *required[2] = { "sessions", "messages" };
    for (size_t i = 0; i < 2; i++) {
        const json_t *tr = json_obj_get(tables, required[i]);
        bool avail = tr && json_get_bool(tr, "available", false);
        const json_t *rows = tr ? json_obj_get(tr, "rows") : NULL;
        if (!avail || !rows || rows->type == JSON_NULL) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "required table %s is not completely readable", required[i]);
            json_append(errors, json_string(msg));
        }
    }
    json_set(report, "recoverable", json_bool(json_len(errors) == 0));
    return report;
}

/* Build the shared-columns list (destination order, present in source). */
static json_t *sr_shared_columns(sqlite3 *src, sqlite3 *dst, const char *table,
                                 json_t **src_cols_out) {
    json_t *sc = sr_table_columns(src, table);
    json_t *dc = sr_table_columns(dst, table);
    json_t *shared = json_array();
    for (size_t i = 0; i < json_len(dc); i++) {
        const json_t *c = json_get(dc, i);
        if (c && c->type == JSON_STRING && sr_columns_contains(sc, c->str_val))
            json_append(shared, json_string(c->str_val));
    }
    json_free(dc);
    *src_cols_out = sc;
    return shared;
}

/* Compose `"a", "b", "c"` and `?, ?, ?` strings. Caller frees both. */
static void sr_col_lists(const json_t *cols, char **quoted_out, char **ph_out) {
    size_t qcap = 1, pcap = 1;
    for (size_t i = 0; i < json_len(cols); i++) {
        qcap += strlen(json_get(cols, i)->str_val) + 5;
        pcap += 3;
    }
    char *q = calloc(1, qcap), *p = calloc(1, pcap);
    for (size_t i = 0; i < json_len(cols); i++) {
        if (i) { strcat(q, ", "); strcat(p, ", "); }
        strcat(q, "\"");
        strcat(q, json_get(cols, i)->str_val);
        strcat(q, "\"");
        strcat(p, "?");
    }
    *quoted_out = q;
    *ph_out = p;
}

/* Bind a source row (column values as-is) into an insert statement. */
static void sr_bind_row(sqlite3_stmt *ins, sqlite3_stmt *sel, int first_col,
                        int ncols) {
    for (int i = 0; i < ncols; i++) {
        int sc = first_col + i;
        switch (sqlite3_column_type(sel, sc)) {
        case SQLITE_INTEGER:
            sqlite3_bind_int64(ins, i + 1, sqlite3_column_int64(sel, sc));
            break;
        case SQLITE_FLOAT:
            sqlite3_bind_double(ins, i + 1, sqlite3_column_double(sel, sc));
            break;
        case SQLITE_TEXT:
            sqlite3_bind_text(ins, i + 1,
                              (const char *)sqlite3_column_text(sel, sc),
                              sqlite3_column_bytes(sel, sc), SQLITE_TRANSIENT);
            break;
        case SQLITE_BLOB:
            sqlite3_bind_blob(ins, i + 1, sqlite3_column_blob(sel, sc),
                              sqlite3_column_bytes(sel, sc), SQLITE_TRANSIENT);
            break;
        default:
            sqlite3_bind_null(ins, i + 1);
        }
    }
}

static void sr_progress(session_recovery_progress_cb cb, void *ud,
                        const char *table, long long copied,
                        const json_t *source_rows) {
    if (!cb) return;
    json_t *ev = json_object();
    json_set(ev, "table", json_string(table));
    json_set(ev, "copied_rows", json_number((double)copied));
    json_set(ev, "source_rows",
             source_rows && source_rows->type != JSON_NULL
                 ? json_number(source_rows->num_val) : json_null());
    cb(ev, ud);
    json_free(ev);
}

/* PoP: sr_copy_table @ hermes_cli/session_recovery.py:_copy_table */
json_t *sr_copy_table(sqlite3 *src, sqlite3 *dst, const char *table,
                      int chunk_size, session_recovery_progress_cb cb,
                      void *ud, const json_t *source_rows) {
    json_t *src_cols = NULL;
    json_t *columns = sr_shared_columns(src, dst, table, &src_cols);
    json_t *result = json_object();
    json_set(result, "source_rows",
             source_rows && source_rows->type != JSON_NULL
                 ? json_number(source_rows->num_val) : json_null());
    json_set(result, "copied_rows", json_number(0));
    json_set(result, "columns", json_copy(columns));

    if (json_len(src_cols) == 0) {
        json_set(result, "status", json_string("missing"));
        json_free(src_cols); json_free(columns);
        return result;
    }
    if (json_len(columns) == 0) {
        json_set(result, "status", json_string("failed"));
        json_set(result, "error",
                 json_string("source and destination have no compatible columns"));
        json_free(src_cols); json_free(columns);
        return result;
    }
    json_free(src_cols);

    char *quoted, *ph;
    sr_col_lists(columns, &quoted, &ph);
    int ncols = (int)json_len(columns);
    const char *prefix = strcmp(table, "state_meta") == 0
                             ? "INSERT OR REPLACE" : "INSERT";
    char *select_sql = NULL, *insert_sql = NULL;
    if (asprintf(&select_sql, "SELECT %s FROM \"%s\"", quoted, table) < 0 ||
        asprintf(&insert_sql, "%s INTO \"%s\" (%s) VALUES (%s)",
                 prefix, table, quoted, ph) < 0) {
        free(quoted); free(ph); free(select_sql);
        json_set(result, "status", json_string("failed"));
        json_set(result, "error", json_string("oom"));
        json_free(columns);
        return result;
    }
    free(quoted); free(ph);

    long long copied = 0;
    bool read_error = false;
    char errmsg[512] = "";
    sqlite3_stmt *sel = NULL, *ins = NULL;
    if (sqlite3_prepare_v2(src, select_sql, -1, &sel, NULL) != SQLITE_OK) {
        snprintf(errmsg, sizeof(errmsg), "%s", sqlite3_errmsg(src));
        read_error = true;
    } else if (sqlite3_prepare_v2(dst, insert_sql, -1, &ins, NULL) != SQLITE_OK) {
        snprintf(errmsg, sizeof(errmsg), "%s", sqlite3_errmsg(dst));
        read_error = true;
    }
    free(select_sql); free(insert_sql);

    while (!read_error) {
        /* fetchmany(chunk_size) batch inside one transaction */
        int in_chunk = 0;
        int rc = SQLITE_ROW;
        sqlite3_exec(dst, "BEGIN IMMEDIATE", NULL, NULL, NULL);
        while (in_chunk < chunk_size) {
            rc = sqlite3_step(sel);
            if (rc != SQLITE_ROW) break;
            sr_bind_row(ins, sel, 0, ncols);
            if (sqlite3_step(ins) != SQLITE_DONE) {
                snprintf(errmsg, sizeof(errmsg), "%s", sqlite3_errmsg(dst));
                read_error = true;
                sqlite3_reset(ins);
                sqlite3_clear_bindings(ins);
                break;
            }
            sqlite3_reset(ins);
            sqlite3_clear_bindings(ins);
            in_chunk++;
        }
        if (read_error) {
            sqlite3_exec(dst, "ROLLBACK", NULL, NULL, NULL);
            break;
        }
        sqlite3_exec(dst, "COMMIT", NULL, NULL, NULL);
        copied += in_chunk;
        if (in_chunk > 0) sr_progress(cb, ud, table, copied, source_rows);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) {
            snprintf(errmsg, sizeof(errmsg), "%s", sqlite3_errmsg(src));
            read_error = true;
        }
    }
    if (sel) sqlite3_finalize(sel);
    if (ins) sqlite3_finalize(ins);
    json_free(columns);

    json_set(result, "copied_rows", json_number((double)copied));
    if (read_error) {
        json_set(result, "status",
                 json_string(copied ? "partial" : "failed"));
        json_set(result, "error", json_string(errmsg));
        return result;
    }
    bool have_expected = source_rows && source_rows->type != JSON_NULL;
    bool complete = !have_expected ||
                    (long long)source_rows->num_val == copied;
    json_set(result, "status", json_string(complete ? "complete" : "partial"));
    if (!complete) {
        char msg[160];
        snprintf(msg, sizeof(msg), "copied %lld of %lld readable rows",
                 copied, (long long)source_rows->num_val);
        json_set(result, "error", json_string(msg));
    }
    return result;
}
