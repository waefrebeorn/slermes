/* session_recovery_salvage.c — best-effort rowid-range salvage copy that
 * continues past damaged source pages. Faithful port of
 * hermes_cli/session_recovery.py (salvage slice).
 */
#define _GNU_SOURCE
#include "session_recovery_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR_MIN_ROWID (-9223372036854775807LL - 1)
#define SR_MAX_ROWID 9223372036854775807LL

/* PoP: sr_append_skipped_range @ hermes_cli/session_recovery.py:_append_skipped_range */
static void sr_append_skipped_range(json_t *ranges, long long low,
                                    long long high, const char *error) {
    size_t n = json_len(ranges);
    if (n > 0) {
        json_t *last = json_get(ranges, n - 1);
        long long lhigh = (long long)json_get_num(last, "high", 0);
        const json_t *lerr = json_obj_get(last, "error");
        if (lhigh + 1 == low && lerr && lerr->type == JSON_STRING &&
            strcmp(lerr->str_val, error) == 0) {
            json_set(last, "high", json_number((double)high));
            return;
        }
    }
    json_t *e = json_object();
    json_set(e, "low", json_number((double)low));
    json_set(e, "high", json_number((double)high));
    json_set(e, "error", json_string(error));
    json_append(ranges, e);
}

/* PoP: sr_salvage_rowid_bounds @ hermes_cli/session_recovery.py:_salvage_rowid_bounds */
static json_t *sr_salvage_rowid_bounds(sqlite3 *src, const char *table) {
    json_t *result = json_object();
    json_t *errors = json_array();
    json_t *fallback = json_array();
    json_set(result, "errors", errors);
    json_set(result, "fallback_edges", fallback);

    bool have_low = false, have_high = false;
    long long low = 0, high = 0;
    const struct { const char *edge; const char *dir; } dirs[2] = {
        { "low", "ASC" }, { "high", "DESC" },
    };
    for (int i = 0; i < 2; i++) {
        char sql[256];
        snprintf(sql, sizeof(sql),
                 "SELECT rowid FROM \"%s\" ORDER BY rowid %s LIMIT 1",
                 table, dirs[i].dir);
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(src, sql, -1, &st, NULL) != SQLITE_OK) {
            char msg[512];
            snprintf(msg, sizeof(msg), "%s rowid: %s", dirs[i].edge,
                     sqlite3_errmsg(src));
            json_append(errors, json_string(msg));
            if (st) sqlite3_finalize(st);
            continue;
        }
        int rc = sqlite3_step(st);
        if (rc == SQLITE_ROW) {
            if (i == 0) { low = sqlite3_column_int64(st, 0); have_low = true; }
            else { high = sqlite3_column_int64(st, 0); have_high = true; }
        } else if (rc != SQLITE_DONE) {
            char msg[512];
            snprintf(msg, sizeof(msg), "%s rowid: %s", dirs[i].edge,
                     sqlite3_errmsg(src));
            json_append(errors, json_string(msg));
        }
        sqlite3_finalize(st);
    }

    if (!have_low && !have_high && json_len(errors) == 0) {
        json_set(result, "empty", json_bool(true));
        return result;
    }
    if (!have_low && !have_high) {
        json_set(result, "unavailable", json_bool(true));
        return result;
    }
    if (!have_low) { low = SR_MIN_ROWID; json_append(fallback, json_string("low")); }
    if (!have_high) { high = SR_MAX_ROWID; json_append(fallback, json_string("high")); }
    json_set(result, "low", json_number((double)low));
    json_set(result, "high", json_number((double)high));
    return result;
}

/* recursive range copier state */
typedef struct {
    sqlite3 *src, *dst;
    const char *table;
    int chunk_size;
    session_recovery_progress_cb cb;
    void *ud;
    const json_t *source_rows;
    char *select_sql;   /* SELECT rowid, cols WHERE rowid BETWEEN ? AND ? */
    sqlite3_stmt *ins;
    int ncols;
    int key_index;      /* column index of "key" within cols, or -1 */
    bool filter_user_meta;
    json_t *result;
    json_t *skipped;
    bool stopped_at_query_limit;
} sr_salvage_ctx_t;

static void sr_salvage_progress(sr_salvage_ctx_t *c) {
    if (!c->cb) return;
    json_t *ev = json_object();
    json_set(ev, "table", json_string(c->table));
    json_set(ev, "copied_rows",
             json_number(json_get_num(c->result, "copied_rows", 0)));
    json_set(ev, "source_rows",
             c->source_rows && c->source_rows->type != JSON_NULL
                 ? json_number(c->source_rows->num_val) : json_null());
    json_set(ev, "skipped_ranges", json_number((double)json_len(c->skipped)));
    c->cb(ev, c->ud);
    json_free(ev);
}

/* PoP: copy_range @ hermes_cli/session_recovery.py:copy_range */
static void sr_copy_range(sr_salvage_ctx_t *c, long long low, long long high) {
    if (low > high) return;
    long long rq = (long long)json_get_num(c->result, "range_queries", 0);
    if (rq >= SR_MAX_SALVAGE_RANGE_QUERIES) {
        c->stopped_at_query_limit = true;
        sr_append_skipped_range(c->skipped, low, high,
                                "salvage range query limit reached");
        return;
    }
    json_set(c->result, "range_queries", json_number((double)(rq + 1)));

    bool have_last = false;
    long long last_committed = 0;
    char errmsg[512] = "";
    bool db_error = false;

    sqlite3_stmt *sel = NULL;
    if (sqlite3_prepare_v2(c->src, c->select_sql, -1, &sel, NULL) != SQLITE_OK) {
        snprintf(errmsg, sizeof(errmsg), "%s", sqlite3_errmsg(c->src));
        db_error = true;
    } else {
        sqlite3_bind_int64(sel, 1, low);
        sqlite3_bind_int64(sel, 2, high);
        while (!db_error) {
            int in_chunk = 0, included = 0, excluded = 0;
            int rc = SQLITE_ROW;
            long long chunk_last_rowid = 0;
            bool chunk_has_rows = false;
            sqlite3_exec(c->dst, "BEGIN IMMEDIATE", NULL, NULL, NULL);
            while (in_chunk < c->chunk_size) {
                rc = sqlite3_step(sel);
                if (rc != SQLITE_ROW) break;
                chunk_has_rows = true;
                chunk_last_rowid = sqlite3_column_int64(sel, 0);
                in_chunk++;
                /* row filter: keep_user_meta drops generated keys */
                if (c->filter_user_meta && c->key_index >= 0) {
                    const unsigned char *k =
                        sqlite3_column_text(sel, 1 + c->key_index);
                    if (k && sr_is_generated_meta_key((const char *)k)) {
                        excluded++;
                        continue;
                    }
                }
                /* bind cols starting at select column 1 (0 is rowid) */
                for (int i = 0; i < c->ncols; i++) {
                    int scol = 1 + i;
                    switch (sqlite3_column_type(sel, scol)) {
                    case SQLITE_INTEGER:
                        sqlite3_bind_int64(c->ins, i + 1,
                                           sqlite3_column_int64(sel, scol));
                        break;
                    case SQLITE_FLOAT:
                        sqlite3_bind_double(c->ins, i + 1,
                                            sqlite3_column_double(sel, scol));
                        break;
                    case SQLITE_TEXT:
                        sqlite3_bind_text(c->ins, i + 1,
                            (const char *)sqlite3_column_text(sel, scol),
                            sqlite3_column_bytes(sel, scol), SQLITE_TRANSIENT);
                        break;
                    case SQLITE_BLOB:
                        sqlite3_bind_blob(c->ins, i + 1,
                            sqlite3_column_blob(sel, scol),
                            sqlite3_column_bytes(sel, scol), SQLITE_TRANSIENT);
                        break;
                    default:
                        sqlite3_bind_null(c->ins, i + 1);
                    }
                }
                if (sqlite3_step(c->ins) != SQLITE_DONE) {
                    snprintf(errmsg, sizeof(errmsg), "%s",
                             sqlite3_errmsg(c->dst));
                    db_error = true;
                    sqlite3_reset(c->ins);
                    sqlite3_clear_bindings(c->ins);
                    break;
                }
                sqlite3_reset(c->ins);
                sqlite3_clear_bindings(c->ins);
                included++;
            }
            if (db_error) {
                sqlite3_exec(c->dst, "ROLLBACK", NULL, NULL, NULL);
                break;
            }
            sqlite3_exec(c->dst, "COMMIT", NULL, NULL, NULL);
            if (chunk_has_rows) {
                json_set(c->result, "copied_rows",
                    json_number(json_get_num(c->result, "copied_rows", 0) + included));
                json_set(c->result, "excluded_rows",
                    json_number(json_get_num(c->result, "excluded_rows", 0) + excluded));
                last_committed = chunk_last_rowid;
                have_last = true;
                sr_salvage_progress(c);
            }
            if (rc == SQLITE_DONE) { sqlite3_finalize(sel); return; }
            if (rc != SQLITE_ROW) {
                snprintf(errmsg, sizeof(errmsg), "%s", sqlite3_errmsg(c->src));
                db_error = true;
            }
        }
    }
    if (sel) sqlite3_finalize(sel);

    /* sqlite3.DatabaseError path: bisect the remaining range */
    long long retry_low = have_last ? last_committed + 1 : low;
    if (retry_low > high) return;
    if (retry_low == high) {
        sr_append_skipped_range(c->skipped, retry_low, high, errmsg);
        return;
    }
    long long midpoint = retry_low + (high - retry_low) / 2;
    sr_copy_range(c, retry_low, midpoint);
    sr_copy_range(c, midpoint + 1, high);
}

/* PoP: sr_copy_table_salvage @ hermes_cli/session_recovery.py:_copy_table_salvage */
json_t *sr_copy_table_salvage(sqlite3 *src, sqlite3 *dst, const char *table,
                              int chunk_size, session_recovery_progress_cb cb,
                              void *ud, const json_t *source_rows,
                              const char *insert_prefix,
                              bool filter_user_meta) {
    json_t *src_cols = sr_table_columns(src, table);
    json_t *dst_cols = sr_table_columns(dst, table);
    json_t *columns = json_array();
    for (size_t i = 0; i < json_len(dst_cols); i++) {
        const json_t *cn = json_get(dst_cols, i);
        if (cn && cn->type == JSON_STRING &&
            sr_columns_contains(src_cols, cn->str_val))
            json_append(columns, json_string(cn->str_val));
    }
    json_free(dst_cols);

    json_t *result = json_object();
    json_set(result, "mode", json_string("rowid_range_salvage"));
    json_set(result, "source_rows",
             source_rows && source_rows->type != JSON_NULL
                 ? json_number(source_rows->num_val) : json_null());
    json_set(result, "copied_rows", json_number(0));
    json_set(result, "excluded_rows", json_number(0));
    json_set(result, "columns", json_copy(columns));
    json_set(result, "range_queries", json_number(0));
    json_t *skipped = json_array();
    json_set(result, "skipped_rowid_ranges", skipped);

    if (json_len(src_cols) == 0) {
        json_set(result, "status", json_string("missing"));
        json_free(src_cols); json_free(columns);
        return result;
    }
    json_free(src_cols);
    if (json_len(columns) == 0) {
        json_set(result, "status", json_string("failed"));
        json_set(result, "error",
                 json_string("source and destination have no compatible columns"));
        json_free(columns);
        return result;
    }

    json_t *bounds = sr_salvage_rowid_bounds(src, table);
    json_set(result, "rowid_bounds", json_copy(bounds));
    if (json_get_bool(bounds, "empty", false)) {
        json_set(result, "status", json_string("complete"));
        json_free(bounds); json_free(columns);
        return result;
    }
    const json_t *blow = json_obj_get(bounds, "low");
    const json_t *bhigh = json_obj_get(bounds, "high");
    if (!blow || !bhigh) {
        json_set(result, "status", json_string("failed"));
        char details[512] = "";
        const json_t *errs = json_obj_get(bounds, "errors");
        for (size_t i = 0; errs && i < json_len(errs); i++) {
            if (i) strncat(details, "; ", sizeof(details) - strlen(details) - 1);
            strncat(details, json_get(errs, i)->str_val,
                    sizeof(details) - strlen(details) - 1);
        }
        char msg[640];
        if (details[0])
            snprintf(msg, sizeof(msg),
                     "could not determine a rowid range for salvage: %s", details);
        else
            snprintf(msg, sizeof(msg),
                     "could not determine a rowid range for salvage");
        json_set(result, "error", json_string(msg));
        json_free(bounds); json_free(columns);
        return result;
    }
    long long low = (long long)blow->num_val;
    long long high = (long long)bhigh->num_val;
    json_free(bounds);

    /* build SQL */
    size_t qcap = 1, pcap = 1;
    for (size_t i = 0; i < json_len(columns); i++) {
        qcap += strlen(json_get(columns, i)->str_val) + 5;
        pcap += 3;
    }
    char *quoted = calloc(1, qcap), *ph = calloc(1, pcap);
    int key_index = -1;
    for (size_t i = 0; i < json_len(columns); i++) {
        if (i) { strcat(quoted, ", "); strcat(ph, ", "); }
        strcat(quoted, "\"");
        strcat(quoted, json_get(columns, i)->str_val);
        strcat(quoted, "\"");
        strcat(ph, "?");
        if (strcmp(json_get(columns, i)->str_val, "key") == 0)
            key_index = (int)i;
    }
    char *select_sql = NULL, *insert_sql = NULL;
    (void)!asprintf(&select_sql,
                    "SELECT rowid, %s FROM \"%s\" "
                    "WHERE rowid BETWEEN ? AND ? ORDER BY rowid",
                    quoted, table);
    (void)!asprintf(&insert_sql, "%s INTO \"%s\" (%s) VALUES (%s)",
                    insert_prefix ? insert_prefix : "INSERT",
                    table, quoted, ph);
    free(quoted); free(ph);

    sqlite3_stmt *ins = NULL;
    if (sqlite3_prepare_v2(dst, insert_sql, -1, &ins, NULL) != SQLITE_OK) {
        json_set(result, "status", json_string("failed"));
        json_set(result, "error", json_string(sqlite3_errmsg(dst)));
        free(select_sql); free(insert_sql); json_free(columns);
        return result;
    }
    free(insert_sql);

    sr_salvage_ctx_t ctx = {
        .src = src, .dst = dst, .table = table, .chunk_size = chunk_size,
        .cb = cb, .ud = ud, .source_rows = source_rows,
        .select_sql = select_sql, .ins = ins,
        .ncols = (int)json_len(columns), .key_index = key_index,
        .filter_user_meta = filter_user_meta,
        .result = result, .skipped = skipped,
        .stopped_at_query_limit = false,
    };
    sr_copy_range(&ctx, low, high);
    sqlite3_finalize(ins);
    free(select_sql);
    json_free(columns);

    long long span = 0;
    for (size_t i = 0; i < json_len(skipped); i++) {
        json_t *e = json_get(skipped, i);
        span += (long long)json_get_num(e, "high", 0) -
                (long long)json_get_num(e, "low", 0) + 1;
    }
    json_set(result, "skipped_rowid_span", json_number((double)span));
    json_set(result, "query_limit_reached",
             json_bool(ctx.stopped_at_query_limit));

    long long copied = (long long)json_get_num(result, "copied_rows", 0);
    long long excluded = (long long)json_get_num(result, "excluded_rows", 0);
    if (json_len(skipped) > 0) {
        json_set(result, "status", json_string(copied ? "partial" : "failed"));
        char msg[96];
        snprintf(msg, sizeof(msg), "%zu rowid range(s) skipped",
                 json_len(skipped));
        json_set(result, "error", json_string(msg));
    } else if (source_rows && source_rows->type != JSON_NULL &&
               copied + excluded != (long long)source_rows->num_val) {
        json_set(result, "status", json_string("partial"));
        char msg[192];
        snprintf(msg, sizeof(msg),
                 "copied %lld and excluded %lld of %lld source rows",
                 copied, excluded, (long long)source_rows->num_val);
        json_set(result, "error", json_string(msg));
    } else {
        json_set(result, "status", json_string("complete"));
    }
    return result;
}
