/* session_recovery_meta.c — state_meta copy (plain + salvage), placeholder
 * session reconstruction, and orphan cleanup. Faithful port of
 * hermes_cli/session_recovery.py (meta slice).
 */
#define _GNU_SOURCE
#include "session_recovery_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *sr_sorted_generated_keys(void) {
    json_t *a = json_array();
    for (size_t i = 0; i < 8; i++)
        json_append(a, json_string(SR_GENERATED_META_KEYS[i]));
    return a;
}

/* PoP: sr_copy_state_meta @ hermes_cli/session_recovery.py:_copy_state_meta */
json_t *sr_copy_state_meta(sqlite3 *src, sqlite3 *dst, int chunk_size,
                           session_recovery_progress_cb cb, void *ud,
                           const json_t *source_rows) {
    json_t *src_cols = sr_table_columns(src, "state_meta");
    json_t *dst_cols = sr_table_columns(dst, "state_meta");
    json_t *result = json_object();
    json_set(result, "source_meta_rows",
             source_rows && source_rows->type != JSON_NULL
                 ? json_number(source_rows->num_val) : json_null());
    json_set(result, "copied_rows", json_number(0));
    json_t *colspec = json_array();
    json_append(colspec, json_string("key"));
    json_append(colspec, json_string("value"));
    json_set(result, "columns", colspec);
    json_set(result, "excluded_keys", sr_sorted_generated_keys());

    bool src_ok = sr_columns_contains(src_cols, "key") &&
                  sr_columns_contains(src_cols, "value");
    bool dst_ok = sr_columns_contains(dst_cols, "key") &&
                  sr_columns_contains(dst_cols, "value");
    json_free(src_cols);
    json_free(dst_cols);
    if (!src_ok) {
        json_set(result, "status", json_string("missing"));
        return result;
    }
    if (!dst_ok) {
        json_set(result, "status", json_string("failed"));
        json_set(result, "error",
                 json_string("destination state_meta schema is incomplete"));
        return result;
    }

    /* WHERE key NOT IN (...generated...) */
    char notin[512] = "";
    for (size_t i = 0; i < 8; i++) {
        size_t off = strlen(notin);
        snprintf(notin + off, sizeof(notin) - off, "%s'%s'",
                 i ? ", " : "", SR_GENERATED_META_KEYS[i]);
    }
    char sql[768];

    /* filtered_source_rows */
    bool have_filtered = false;
    long long filtered_rows = 0;
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) FROM state_meta WHERE key NOT IN (%s)", notin);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(src, sql, -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) {
        filtered_rows = sqlite3_column_int64(st, 0);
        have_filtered = true;
    }
    if (st) { sqlite3_finalize(st); st = NULL; }

    snprintf(sql, sizeof(sql),
             "SELECT key, value FROM state_meta WHERE key NOT IN (%s)", notin);
    sqlite3_stmt *ins = NULL;
    long long copied = 0;
    char errmsg[512] = "";
    bool db_error = false;
    if (sqlite3_prepare_v2(src, sql, -1, &st, NULL) != SQLITE_OK) {
        snprintf(errmsg, sizeof(errmsg), "%s", sqlite3_errmsg(src));
        db_error = true;
    } else if (sqlite3_prepare_v2(dst,
                   "INSERT OR REPLACE INTO state_meta(key, value) VALUES (?, ?)",
                   -1, &ins, NULL) != SQLITE_OK) {
        snprintf(errmsg, sizeof(errmsg), "%s", sqlite3_errmsg(dst));
        db_error = true;
    }
    while (!db_error) {
        int in_chunk = 0;
        int rc = SQLITE_ROW;
        sqlite3_exec(dst, "BEGIN IMMEDIATE", NULL, NULL, NULL);
        while (in_chunk < chunk_size) {
            rc = sqlite3_step(st);
            if (rc != SQLITE_ROW) break;
            sqlite3_bind_text(ins, 1, (const char *)sqlite3_column_text(st, 0),
                              -1, SQLITE_TRANSIENT);
            if (sqlite3_column_type(st, 1) == SQLITE_NULL)
                sqlite3_bind_null(ins, 2);
            else
                sqlite3_bind_text(ins, 2,
                                  (const char *)sqlite3_column_text(st, 1),
                                  -1, SQLITE_TRANSIENT);
            if (sqlite3_step(ins) != SQLITE_DONE) {
                snprintf(errmsg, sizeof(errmsg), "%s", sqlite3_errmsg(dst));
                db_error = true;
                sqlite3_reset(ins); sqlite3_clear_bindings(ins);
                break;
            }
            sqlite3_reset(ins); sqlite3_clear_bindings(ins);
            in_chunk++;
        }
        if (db_error) { sqlite3_exec(dst, "ROLLBACK", NULL, NULL, NULL); break; }
        sqlite3_exec(dst, "COMMIT", NULL, NULL, NULL);
        copied += in_chunk;
        if (in_chunk > 0 && cb) {
            json_t *ev = json_object();
            json_set(ev, "table", json_string("state_meta"));
            json_set(ev, "copied_rows", json_number((double)copied));
            json_set(ev, "source_rows",
                     have_filtered ? json_number((double)filtered_rows)
                                   : json_null());
            cb(ev, ud);
            json_free(ev);
        }
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) {
            snprintf(errmsg, sizeof(errmsg), "%s", sqlite3_errmsg(src));
            db_error = true;
        }
    }
    if (st) sqlite3_finalize(st);
    if (ins) sqlite3_finalize(ins);

    json_set(result, "copied_rows", json_number((double)copied));
    if (db_error) {
        json_set(result, "status", json_string(copied ? "partial" : "failed"));
        json_set(result, "error", json_string(errmsg));
        return result;
    }
    bool complete = !have_filtered || copied == filtered_rows;
    json_set(result, "status", json_string(complete ? "complete" : "partial"));
    if (!complete) {
        char msg[160];
        snprintf(msg, sizeof(msg), "copied %lld of %lld readable rows",
                 copied, filtered_rows);
        json_set(result, "error", json_string(msg));
    }
    return result;
}

/* PoP: sr_copy_state_meta_salvage @ hermes_cli/session_recovery.py:_copy_state_meta_salvage */
json_t *sr_copy_state_meta_salvage(sqlite3 *src, sqlite3 *dst, int chunk_size,
                                   session_recovery_progress_cb cb, void *ud,
                                   const json_t *source_rows) {
    json_t *src_cols = sr_table_columns(src, "state_meta");
    json_t *dst_cols = sr_table_columns(dst, "state_meta");

    json_t *base = json_object();
    json_set(base, "mode", json_string("rowid_range_salvage"));
    json_set(base, "source_meta_rows",
             source_rows && source_rows->type != JSON_NULL
                 ? json_number(source_rows->num_val) : json_null());
    json_set(base, "copied_rows", json_number(0));
    json_t *colspec = json_array();
    json_append(colspec, json_string("key"));
    json_append(colspec, json_string("value"));
    json_set(base, "columns", colspec);
    json_set(base, "excluded_keys", sr_sorted_generated_keys());

    if (json_len(src_cols) == 0) {
        json_set(base, "status", json_string("missing"));
        json_free(src_cols); json_free(dst_cols);
        return base;
    }
    if (!(sr_columns_contains(src_cols, "key") &&
          sr_columns_contains(src_cols, "value"))) {
        json_set(base, "status", json_string("failed"));
        char found[400] = "";
        for (size_t i = 0; i < json_len(src_cols); i++) {
            size_t off = strlen(found);
            snprintf(found + off, sizeof(found) - off, "%s%s",
                     i ? ", " : "", json_get(src_cols, i)->str_val);
        }
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "source state_meta exists but is missing the key/value "
                 "columns (found: %s)", found[0] ? found : "none");
        json_set(base, "error", json_string(msg));
        json_free(src_cols); json_free(dst_cols);
        return base;
    }
    if (!(sr_columns_contains(dst_cols, "key") &&
          sr_columns_contains(dst_cols, "value"))) {
        json_set(base, "status", json_string("failed"));
        json_set(base, "error",
                 json_string("destination state_meta schema is incomplete"));
        json_free(src_cols); json_free(dst_cols);
        return base;
    }
    json_free(src_cols); json_free(dst_cols);
    json_free(base);

    json_t *result = sr_copy_table_salvage(src, dst, "state_meta", chunk_size,
                                           cb, ud, source_rows,
                                           "INSERT OR REPLACE", true);
    /* rename source_rows -> source_meta_rows (Python pop/assign) */
    const json_t *sr = json_obj_get(result, "source_rows");
    json_set(result, "source_meta_rows",
             sr && sr->type != JSON_NULL ? json_number(sr->num_val)
                                         : json_null());
    json_obj_del(result, "source_rows");
    json_set(result, "excluded_keys", sr_sorted_generated_keys());
    return result;
}
