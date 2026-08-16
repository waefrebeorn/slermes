/* session_recovery_verify.c — recovered-database verification and derived
 * metadata finalization. Faithful port of hermes_cli/session_recovery.py
 * (verify slice).
 */
#define _GNU_SOURCE
#include "session_recovery_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sr_add(json_t *arr, const char *msg) {
    json_append(arr, json_string(msg));
}

/* _db_opens_cleanly analog: journal_mode + integrity_check + sessions read +
 * rolled-back messages write probe (FTS corruption detector). Returns
 * malloc'd reason or NULL when healthy. */
/* PoP: sr_db_opens_cleanly @ hermes_state.py:_db_opens_cleanly */
static char *sr_db_opens_cleanly(const char *path) {
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(path, &conn, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        char *r = strdup(conn ? sqlite3_errmsg(conn) : "cannot open");
        if (conn) sqlite3_close(conn);
        return r;
    }
    char *reason = NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA journal_mode", -1, &st, NULL) != SQLITE_OK ||
        sqlite3_step(st) != SQLITE_ROW) {
        reason = strdup(sqlite3_errmsg(conn));
        goto done;
    }
    sqlite3_finalize(st); st = NULL;

    if (sqlite3_prepare_v2(conn, "PRAGMA integrity_check", -1, &st, NULL) == SQLITE_OK) {
        char problems[512] = "";
        int nprob = 0;
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *row = (const char *)sqlite3_column_text(st, 0);
            if (row && strcasecmp(row, "ok") != 0 && nprob < 3) {
                size_t off = strlen(problems);
                snprintf(problems + off, sizeof(problems) - off, "%s%s",
                         nprob ? "; " : "", row);
                nprob++;
            }
        }
        if (nprob) { reason = strdup(problems); goto done; }
    }
    if (st) { sqlite3_finalize(st); st = NULL; }

    if (sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM sessions", -1, &st, NULL)
            != SQLITE_OK || sqlite3_step(st) != SQLITE_ROW) {
        reason = strdup(sqlite3_errmsg(conn));
        goto done;
    }
    sqlite3_finalize(st); st = NULL;

    /* Rolled-back write probe through the FTS triggers (#50502). */
    sqlite3_exec(conn, "BEGIN", NULL, NULL, NULL);
    int rc = sqlite3_exec(conn,
        "INSERT INTO messages (session_id, role, content, timestamp) "
        "SELECT id, 'user', 'health probe', 0 FROM sessions LIMIT 1",
        NULL, NULL, NULL);
    sqlite3_exec(conn, "ROLLBACK", NULL, NULL, NULL);
    if (rc != SQLITE_OK && rc != SQLITE_CONSTRAINT)
        reason = strdup(sqlite3_errmsg(conn));

done:
    if (st) sqlite3_finalize(st);
    sqlite3_close(conn);
    return reason;
}

/* PoP: sr_verify_recovered_database @ hermes_cli/session_recovery.py:_verify_recovered_database */
json_t *sr_verify_recovered_database(const char *output,
                                     const json_t *expected_counts,
                                     const json_t *copy_report,
                                     bool allow_partial,
                                     const json_t *orphan_cleanup) {
    json_t *v = json_object();
    json_t *errors = json_array();
    json_t *warnings = json_array();
    json_set(v, "errors", errors);
    json_set(v, "warnings", warnings);
    json_set(v, "loss_detected", json_bool(false));
    bool loss = false;

    char *open_error = sr_db_opens_cleanly(output);
    json_set(v, "opens_cleanly", json_bool(open_error == NULL));
    if (open_error) {
        char msg[600];
        snprintf(msg, sizeof(msg), "database health probe: %s", open_error);
        sr_add(errors, msg);
        free(open_error);
    }

    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(output, &conn, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        char msg[600];
        snprintf(msg, sizeof(msg), "verification query failed: %s",
                 conn ? sqlite3_errmsg(conn) : "cannot open");
        sr_add(errors, msg);
        if (conn) sqlite3_close(conn);
        json_set(v, "healthy", json_bool(false));
        json_set(v, "complete", json_bool(false));
        return v;
    }

    sqlite3_stmt *st = NULL;

    /* integrity_check */
    json_t *integ = json_array();
    if (sqlite3_prepare_v2(conn, "PRAGMA integrity_check", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW)
            json_append(integ,
                json_string((const char *)sqlite3_column_text(st, 0)));
    }
    if (st) { sqlite3_finalize(st); st = NULL; }
    json_set(v, "integrity_check", integ);
    if (!(json_len(integ) == 1 &&
          strcmp(json_get(integ, 0)->str_val, "ok") == 0))
        sr_add(errors, "PRAGMA integrity_check did not return exactly 'ok'");

    /* foreign_key_check */
    json_t *fkc = json_array();
    if (sqlite3_prepare_v2(conn, "PRAGMA foreign_key_check", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            json_t *row = json_array();
            for (int i = 0; i < sqlite3_column_count(st); i++) {
                const char *cell = (const char *)sqlite3_column_text(st, i);
                json_append(row, cell ? json_string(cell) : json_null());
            }
            json_append(fkc, row);
        }
    }
    if (st) { sqlite3_finalize(st); st = NULL; }
    json_set(v, "foreign_key_check", fkc);
    if (json_len(fkc) > 0) sr_add(errors, "foreign key violations remain");

    /* journal_mode */
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
        json_set(v, "journal_mode", jm ? json_string(low) : json_null());
    } else json_set(v, "journal_mode", json_null());
    if (st) { sqlite3_finalize(st); st = NULL; }

    /* schema_version */
    bool have_sv = false;
    long long sv = 0;
    if (sqlite3_prepare_v2(conn, "SELECT version FROM schema_version LIMIT 1",
                           -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) {
        sv = sqlite3_column_int64(st, 0);
        have_sv = true;
    }
    if (st) { sqlite3_finalize(st); st = NULL; }
    json_set(v, "schema_version", have_sv ? json_number((double)sv) : json_null());
    if (!have_sv || sv != SR_SCHEMA_VERSION) {
        char msg[128];
        if (have_sv)
            snprintf(msg, sizeof(msg), "schema version is %lld, expected %d",
                     sv, SR_SCHEMA_VERSION);
        else
            snprintf(msg, sizeof(msg), "schema version is None, expected %d",
                     SR_SCHEMA_VERSION);
        sr_add(errors, msg);
    }

    /* fts_meta */
    json_t *meta = json_object();
    if (sqlite3_prepare_v2(conn,
            "SELECT key, value FROM state_meta WHERE key LIKE 'fts_%'",
            -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *k = (const char *)sqlite3_column_text(st, 0);
            const char *val = (const char *)sqlite3_column_text(st, 1);
            json_set(meta, k ? k : "", val ? json_string(val) : json_null());
        }
    }
    if (st) { sqlite3_finalize(st); st = NULL; }
    json_set(v, "fts_meta", meta);
    {
        const json_t *fsv = json_obj_get(meta, "fts_storage_version");
        char want[16];
        snprintf(want, sizeof(want), "%d", SR_FTS_STORAGE_VERSION);
        if (!fsv || fsv->type != JSON_STRING || strcmp(fsv->str_val, want) != 0)
            sr_add(errors, "fresh FTS storage version was not established");
    }
    {
        static const char *pending[6] = {
            "fts_cjk_rebuild_high_water", "fts_cjk_rebuild_progress",
            "fts_cjk_stale", "fts_optimize_available",
            "fts_rebuild_high_water", "fts_rebuild_progress",
        };
        json_t *pk = json_array();
        for (size_t i = 0; i < 6; i++)
            if (json_obj_get(meta, pending[i]))
                json_append(pk, json_string(pending[i]));
        json_set(v, "pending_fts_keys", pk);
        if (json_len(pk) > 0)
            sr_add(errors,
                "derived FTS transition markers remain in the recovered database");
    }

    /* table_counts */
    json_t *counts = json_object();
    {
        const char *all[9];
        for (size_t i = 0; i < 6; i++) all[i] = SR_CANONICAL_TABLES[i];
        all[6] = "state_meta";
        all[7] = SR_TOPIC_TABLES[0];
        all[8] = SR_TOPIC_TABLES[1];
        for (size_t i = 0; i < 9; i++) {
            json_t *cols = sr_table_columns(conn, all[i]);
            bool present = json_len(cols) > 0;
            json_free(cols);
            if (!present) continue;
            char sql[256];
            snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM \"%s\"", all[i]);
            if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) == SQLITE_OK &&
                sqlite3_step(st) == SQLITE_ROW)
                json_set(counts, all[i],
                         json_number((double)sqlite3_column_int64(st, 0)));
            if (st) { sqlite3_finalize(st); st = NULL; }
        }
    }
    json_set(v, "table_counts", counts);

    for (size_t i = 0; i < 2; i++) {
        const char *table = i == 0 ? "sessions" : "messages";
        const json_t *exp = expected_counts
                                ? json_obj_get(expected_counts, table) : NULL;
        if (!exp || exp->type == JSON_NULL) continue;
        const json_t *got = json_obj_get(counts, table);
        long long expected = (long long)exp->num_val;
        bool match = got && got->type == JSON_NUMBER &&
                     (long long)got->num_val == expected;
        if (!match) {
            char msg[160];
            if (got && got->type == JSON_NUMBER)
                snprintf(msg, sizeof(msg), "%s count is %lld, expected %lld",
                         table, (long long)got->num_val, expected);
            else
                snprintf(msg, sizeof(msg), "%s count is None, expected %lld",
                         table, expected);
            if (allow_partial) { sr_add(warnings, msg); loss = true; }
            else sr_add(errors, msg);
        }
    }

    /* copy_report statuses */
    if (copy_report && copy_report->type == JSON_OBJECT) {
        for (size_t i = 0; i < copy_report->c.count; i++) {
            const char *table = copy_report->c.keys[i];
            const json_t *tr = copy_report->c.items[i];
            const json_t *status = json_obj_get(tr, "status");
            if (!status || status->type != JSON_STRING) continue;
            bool bad = strcmp(status->str_val, "failed") == 0 ||
                       strcmp(status->str_val, "partial") == 0;
            if (!bad) continue;
            char msg[128];
            snprintf(msg, sizeof(msg), "%s copy status is %s",
                     table, status->str_val);
            bool required = strcmp(table, "sessions") == 0 ||
                            strcmp(table, "messages") == 0;
            if (allow_partial &&
                (strcmp(status->str_val, "partial") == 0 || !required)) {
                sr_add(warnings, msg);
                loss = true;
            } else sr_add(errors, msg);
        }
    }

    /* orphan_cleanup */
    if (orphan_cleanup && orphan_cleanup->type == JSON_OBJECT) {
        long long orphan_count =
            (long long)json_get_num(orphan_cleanup, "total_removed_or_relinked", 0);
        if (orphan_count) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "%lld orphaned reference(s) were removed or relinked",
                     orphan_count);
            sr_add(warnings, msg);
            loss = true;
        }
        long long rebuilt =
            (long long)json_get_num(orphan_cleanup, "sessions_reconstructed", 0);
        if (rebuilt) {
            long long retained =
                (long long)json_get_num(orphan_cleanup, "messages_retained", 0);
            char msg[320];
            snprintf(msg, sizeof(msg),
                "%lld session(s) could not be salvaged and were reconstructed "
                "as placeholders to retain %lld message(s); their metadata "
                "(title, model, timestamps, cost) is lost", rebuilt, retained);
            sr_add(warnings, msg);
            loss = true;
        }
    }

    /* FTS integrity checks */
    json_t *fts_checks = json_object();
    {
        static const char *fts_tables[3] = {
            "messages_fts", "messages_fts_trigram", "messages_fts_cjk",
        };
        for (size_t i = 0; i < 3; i++) {
            json_t *cols = sr_table_columns(conn, fts_tables[i]);
            bool present = json_len(cols) > 0;
            json_free(cols);
            if (!present) continue;
            char sql[256];
            snprintf(sql, sizeof(sql),
                     "INSERT INTO \"%s\" (\"%s\") VALUES ('integrity-check')",
                     fts_tables[i], fts_tables[i]);
            int rc = sqlite3_exec(conn, sql, NULL, NULL, NULL);
            if (rc == SQLITE_OK) {
                snprintf(sql, sizeof(sql),
                         "SELECT 1 FROM \"%s\" WHERE \"%s\" MATCH '\"\"' LIMIT 1",
                         fts_tables[i], fts_tables[i]);
                sqlite3_stmt *ps = NULL;
                rc = sqlite3_prepare_v2(conn, sql, -1, &ps, NULL);
                if (rc == SQLITE_OK) sqlite3_step(ps);
                if (ps) sqlite3_finalize(ps);
            }
            if (rc == SQLITE_OK) {
                json_set(fts_checks, fts_tables[i], json_string("ok"));
            } else {
                const char *em = sqlite3_errmsg(conn);
                json_set(fts_checks, fts_tables[i], json_string(em));
                char msg[600];
                snprintf(msg, sizeof(msg), "%s integrity check failed: %s",
                         fts_tables[i], em);
                sr_add(errors, msg);
            }
        }
    }
    json_set(v, "fts_checks", fts_checks);

    sqlite3_close(conn);
    json_set(v, "loss_detected", json_bool(loss));
    bool healthy = json_len(errors) == 0;
    json_set(v, "healthy", json_bool(healthy));
    json_set(v, "complete", json_bool(healthy && !loss));
    return v;
}

/* PoP: sr_finalize_derived_metadata @ hermes_cli/session_recovery.py:_finalize_derived_metadata */
json_t *sr_finalize_derived_metadata(sqlite3 *dst) {
    json_t *result = json_object();
    json_t *fts_tables = json_array();

    sqlite3_stmt *st = NULL;
    bool have_fts = false, have_trigram = false;
    if (sqlite3_prepare_v2(dst,
            "SELECT name FROM sqlite_master "
            "WHERE type='table' AND name IN ('messages_fts', 'messages_fts_trigram') "
            "ORDER BY name",
            -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *n = (const char *)sqlite3_column_text(st, 0);
            json_append(fts_tables, json_string(n));
            if (strcmp(n, "messages_fts") == 0) have_fts = true;
            if (strcmp(n, "messages_fts_trigram") == 0) have_trigram = true;
        }
    }
    if (st) sqlite3_finalize(st);
    json_set(result, "fts_tables", fts_tables);
    json_set(result, "finalized", json_bool(false));
    if (!have_fts || !have_trigram) {
        json_set(result, "error",
                 json_string("fresh destination is missing required FTS tables"));
        return result;
    }

    /* delete fts_* generated keys then stamp fts_storage_version */
    sqlite3_exec(dst, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    char notin[512] = "";
    int n = 0;
    for (size_t i = 0; i < 8; i++) {
        if (strncmp(SR_GENERATED_META_KEYS[i], "fts_", 4) != 0) continue;
        size_t off = strlen(notin);
        snprintf(notin + off, sizeof(notin) - off, "%s'%s'",
                 n ? ", " : "", SR_GENERATED_META_KEYS[i]);
        n++;
    }
    char sql[768];
    snprintf(sql, sizeof(sql), "DELETE FROM state_meta WHERE key IN (%s)", notin);
    sqlite3_exec(dst, sql, NULL, NULL, NULL);
    snprintf(sql, sizeof(sql),
             "INSERT INTO state_meta(key, value) VALUES ('fts_storage_version', '%d') "
             "ON CONFLICT(key) DO UPDATE SET value = excluded.value",
             SR_FTS_STORAGE_VERSION);
    sqlite3_exec(dst, sql, NULL, NULL, NULL);
    sqlite3_exec(dst, "COMMIT", NULL, NULL, NULL);
    json_set(result, "finalized", json_bool(true));
    return result;
}
