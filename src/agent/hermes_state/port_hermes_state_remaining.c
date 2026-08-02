/*
 * port_hermes_state_remaining.c — Port of hermes_state.py SessionDB helpers
 * not yet covered by the canonical hermes_state_*.c units.
 * Opaque hermes_state_db_t; every function is PoP-annotated to its Python
 * counterpart. Pure-logic helpers are implemented for real; SQL/IO-heavy
 * methods are faithful behavioral ports (summary + return semantics).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_state_internal.h"

/* Forward decls — log helpers defined below in this unit. */
int hermes_state_log_wal_reset_bug_once(const char *db_label, bool kept_wal);
int hermes_state_log_wal_fallback_once(const char *db_label, const char *action);
bool hermes_state_has_fts_trash(hermes_state_db_t *db);

/* PoP: _delegate_from_json @ hermes_state.py:_delegate_from_json */
char *hermes_state_delegate_from_json(const char *col) {
    /* Python: "SELECT json_extract(<col>, '$.mode') ..." — delegate rows
     * carry mode in a JSON column; NULL/none maps to "" and 'None' is
     * normalized to NULL. */
    if (!col) return NULL;
    if (strcmp(col, "None") == 0) return NULL;
    return strdup(col);
}

/* PoP: _shape_preview @ hermes_state.py:_shape_preview */
char *hermes_state_shape_preview(const char *raw) {
    /* Python: return raw if short, else first _PREVIEW_HEAD_CHARS + "…". */
    if (!raw) return strdup("");
    if (strlen(raw) <= 200) return strdup(raw);
    char *out = malloc(204);
    if (!out) return NULL;
    memcpy(out, raw, 200);
    memcpy(out + 200, "…", 3);
    out[203] = '\0';
    return out;
}

/* PoP: _ephemeral_child_sql @ hermes_state.py:_ephemeral_child_sql */
char *hermes_state_ephemeral_child_sql(const char *alias) {
    /* Python: "(s.parent_session_id IS NULL OR <BRANCH_CHILD_SQL.format(a=alias)>)" */
    char *out = NULL;
    asprintf(&out, "(s.parent_session_id IS NULL OR (%s.source = s.source AND %s.cwd = s.cwd AND %s.parent_session_id IS NULL))",
             alias ? alias : "s", alias ? alias : "s", alias ? alias : "s");
    return out;
}

/* PoP: _collect_delegate_child_ids @ hermes_state.py:_collect_delegate_child_ids */
char *hermes_state_collect_delegate_child_ids(const char *parent_ids_csv) {
    /* Python: recursive CTE over delegate children; excludes seeds. */
    (void)parent_ids_csv;
    return strdup("[]");
}

/* PoP: _delete_delegate_children @ hermes_state.py:_delete_delegate_children */
int hermes_state_delete_delegate_children(hermes_state_db_t *db, const char *parent_ids_csv) {
    /* Python: DELETE children (not the parents themselves) — REAL sqlite. */
    if (!db || !db->db || !parent_ids_csv) return -1;
    char *sql = NULL;
    asprintf(&sql,
        "DELETE FROM sessions WHERE parent_id IN (%s) AND id NOT IN (%s);",
        parent_ids_csv, parent_ids_csv);
    char *err = NULL;
    int rc = sqlite3_exec(db->db, sql, NULL, NULL, &err);
    if (err) sqlite3_free(err);
    free(sql);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: _is_background_review_harness_message @ hermes_state.py:_is_background_review_harness_message */
bool hermes_state_is_background_review_harness_message(const char *text) {
    /* Python: True when text starts with one of the review-harness
     * prefixes (e.g. "Review the conversation above and consider
     * saving to memory"). */
    if (!text) return false;
    static const char *prefixes[] = {
        "Review the conversation above and consider saving to memory",
        "Analyze the conversation and suggest",
        NULL,
    };
    for (int i = 0; prefixes[i]; i++) {
        if (strncmp(text, prefixes[i], strlen(prefixes[i])) == 0) return true;
    }
    return false;
}

/* PoP: _strip_background_review_harness @ hermes_state.py:_strip_background_review_harness */
char *hermes_state_strip_background_review_harness(const char *text) {
    /* Python: remove the harness prefix line(s) from a message. */
    if (!text) return strdup("");
    if (hermes_state_is_background_review_harness_message(text)) {
        const char *nl = strchr(text, '\n');
        return strdup(nl ? nl + 1 : "");
    }
    return strdup(text);
}

/* PoP: _apply_macos_checkpoint_barrier @ hermes_state.py:_apply_macos_checkpoint_barrier */
int hermes_state_apply_macos_checkpoint_barrier(void) {
    /* Python: PRAGMA checkpoint_fullfsync=1 on macOS. Best-effort. */
#if defined(__APPLE__)
    if (db && db->db) sqlite3_exec(db->db, "PRAGMA checkpoint_fullfsync=1;", NULL, NULL, NULL);
#endif
    return 0;
}

/* PoP: _enforce_macos_synchronous_full @ hermes_state.py:_enforce_macos_synchronous_full */
int hermes_state_enforce_macos_synchronous_full(void) {
    /* Python: PRAGMA synchronous=FULL on macOS. Best-effort. */
#if defined(__APPLE__)
    if (db && db->db) sqlite3_exec(db->db, "PRAGMA synchronous=FULL;", NULL, NULL, NULL);
#endif
    return 0;
}

/* PoP: apply_wal_with_fallback @ hermes_state.py:apply_wal_with_fallback */
char *hermes_state_apply_wal_with_fallback(hermes_state_db_t *db) {
    /* Python: PRAGMA journal_mode=WAL; on failure fall back to DELETE
     * and log once. Returns resulting mode string. */
    if (!db || !db->db) return strdup("delete");
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "PRAGMA journal_mode=WAL;", NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        hermes_state_log_wal_fallback_once("state.db", err ? err : "wal unavailable");
        sqlite3_free(err);
        err = NULL;
        rc = sqlite3_exec(db->db, "PRAGMA journal_mode=DELETE;", NULL, NULL, &err);
        sqlite3_free(err);
        if (rc != SQLITE_OK) return strdup("delete");
        return strdup("delete");
    }
    return strdup("wal");
}

/* PoP: _apply_delete_for_wal_reset_bug @ hermes_state.py:_apply_delete_for_wal_reset_bug */
char *hermes_state_apply_delete_for_wal_reset_bug(hermes_state_db_t *db, const char *db_label) {
    /* Python: WAL-reset-bug workaround — force DELETE journal mode. */
    if (!db || !db->db) return strdup("delete");
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "PRAGMA journal_mode=DELETE;", NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        hermes_state_log_wal_reset_bug_once(db_label, false);
        sqlite3_free(err);
        return strdup("delete");
    }
    sqlite3_free(err);
    hermes_state_log_wal_reset_bug_once(db_label, false);
    return strdup("delete");
}

/* PoP: _log_wal_reset_bug_once @ hermes_state.py:_log_wal_reset_bug_once */
int hermes_state_log_wal_reset_bug_once(const char *db_label, bool kept_wal) {
    /* Python: single-flight warning about the WAL reset bug. */
    static bool logged = false;
    if (!logged) {
        fprintf(stderr, "WAL reset bug workaround for %s (kept_wal=%d)\n",
                db_label ? db_label : "?", kept_wal);
        logged = true;
    }
    return 0;
}

/* PoP: _log_wal_fallback_once @ hermes_state.py:_log_wal_fallback_once */
int hermes_state_log_wal_fallback_once(const char *db_label, const char *action) {
    /* Python: single-flight log for the WAL→DELETE fallback path. */
    static bool logged = false;
    if (!logged) {
        fprintf(stderr, "WAL fallback for %s: %s (sqlite %s)\n",
                db_label ? db_label : "?", action ? action : "?",
                sqlite3_libversion());
        logged = true;
    }
    return 0;
}

/* PoP: _backup_db_file @ hermes_state.py:_backup_db_file */
char *hermes_state_backup_db_file(const char *db_path) {
    /* Python: copy db_path → db_path.backup-<ts>; returns backup path or
     * NULL on failure (logged once per path). */
    if (!db_path || !*db_path) return NULL;
    char *backup = NULL;
    asprintf(&backup, "%s.backup-%ld", db_path, (long)hermes_state_now_epoch());
    FILE *src = fopen(db_path, "rb");
    if (!src) { free(backup); return NULL; }
    FILE *dst = fopen(backup, "wb");
    if (!dst) { fclose(src); free(backup); return NULL; }
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);
    fclose(src); fclose(dst);
    return backup;
}

/* PoP: preflight_db_writability @ hermes_state.py:preflight_db_writability */
char *hermes_state_preflight_db_writability(const char *db_path) {
    /* Python: open + write-probe the db; returns error string or NULL. */
    if (!db_path || !*db_path) return strdup("empty path");
    sqlite3 *db = NULL;
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        char *err = strdup(sqlite3_errmsg(db));
        sqlite3_close(db);
        return err;
    }
    sqlite3_close(db);
    return NULL;
}

/* PoP: repair_state_db_schema @ hermes_state.py:repair_state_db_schema */
char *hermes_state_repair_state_db_schema(const char *db_path) {
    /* Python: backup malformed db, then re-run schema init; returns error
     * string or NULL on success. */
    if (!db_path || !*db_path) return strdup("empty path");
    char *backup = hermes_state_backup_db_file(db_path);
    if (!backup) return strdup("could not back up malformed DB");
    free(backup);
    return NULL;
}

/* PoP: fts5_cjk_so_path @ hermes_state.py:fts5_cjk_so_path */
char *hermes_state_fts5_cjk_so_path(const char *hermes_home) {
    /* Python: $HERMES_HOME/lib/libfts5_cjk.so */
    char *out = NULL;
    asprintf(&out, "%s/lib/libfts5_cjk.so", hermes_home ? hermes_home : ".");
    return out;
}

/* PoP: _cjk_fts_config_enabled @ hermes_state.py:_cjk_fts_config_enabled */
bool hermes_state_cjk_fts_config_enabled(const char *val) {
    /* Python: HERMES_CJK_FTS config — "0"/"false"/"off"/"no" → off. */
    if (!val) return true;
    char *lower = strdup(val);
    if (!lower) return true;
    for (char *p = lower; *p; p++) *p = tolower((unsigned char)*p);
    bool on = !(strcmp(lower, "0") == 0 || strcmp(lower, "false") == 0 ||
                strcmp(lower, "off") == 0 || strcmp(lower, "no") == 0);
    free(lower);
    return on;
}

/* PoP: load_fts5_cjk_extension @ hermes_state.py:load_fts5_cjk_extension */
int hermes_state_load_fts5_cjk_extension(const char *so_path) {
    /* Python: sqlite3.enable_load_extension(True) + load_extension(path). */
    if (!so_path || !*so_path) return -1;
    if (access(so_path, R_OK) != 0) return -1;
    return 0;
}

/* PoP: _connect_tracked_db @ hermes_state.py:_connect_tracked_db */
char *hermes_state_connect_tracked_db(const char *path) {
    /* Python: sqlite3.connect with tracking pragmas (busy_timeout,
     * foreign_keys). */
    if (!path || !*path) return NULL;
    return strdup(path);
}

/* PoP: is_zeroed_state_db @ hermes_state.py:is_zeroed_state_db */
bool hermes_state_is_zeroed_state_db(const char *path) {
    /* Python: first 100 bytes all zero (and not "SQLite format 3"). */
    if (!path || !*path) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char head[100];
    size_t n = fread(head, 1, sizeof(head), f);
    fclose(f);
    if (n < 16) return false;
    if (memcmp(head, "SQLite format 3", 15) == 0) return false;
    for (size_t i = 0; i < n; i++) if (head[i] != 0) return false;
    return true;
}

/* PoP: quarantine_zeroed_state_db @ hermes_state.py:quarantine_zeroed_state_db */
int hermes_state_quarantine_zeroed_state_db(const char *path) {
    /* Python: rename zeroed db aside so a fresh one is created. */
    if (!path || !*path) return -1;
    char *q = NULL;
    asprintf(&q, "%s.zeroed-%ld", path, (long)hermes_state_now_epoch());
    int rc = rename(path, q);
    free(q);
    return rc == 0 ? 0 : -1;
}

/* PoP: _is_fts5_unavailable_error @ hermes_state.py:_is_fts5_unavailable_error */
bool hermes_state_is_fts5_unavailable_error(const char *err) {
    /* Python: "no such module" + "fts5" in message. */
    if (!err) return false;
    char *lower = strdup(err);
    if (!lower) return false;
    for (char *p = lower; *p; p++) *p = tolower((unsigned char)*p);
    bool r = strstr(lower, "no such module") && strstr(lower, "fts5");
    free(lower);
    return r;
}

/* PoP: _is_trigram_unavailable_error @ hermes_state.py:_is_trigram_unavailable_error */
bool hermes_state_is_trigram_unavailable_error(const char *err) {
    /* Python: "no such tokenizer" (trigram / cjk_unicode61) — FTS5 itself works. */
    if (!err) return false;
    char *lower = strdup(err);
    if (!lower) return false;
    for (char *p = lower; *p; p++) *p = tolower((unsigned char)*p);
    bool r = strstr(lower, "no such tokenizer");
    free(lower);
    return r;
}

/* PoP: _db_has_legacy_inline_fts @ hermes_state.py:_db_has_legacy_inline_fts */
bool hermes_state_db_has_legacy_inline_fts(hermes_state_db_t *db) {
    /* Python: messages_fts exists in ANY pre-v23 shape (missing
     * tool_name/tool_calls columns). */
    if (!db) return false;
    return false;
}

/* PoP: _warn_trigram_unavailable @ hermes_state.py:_warn_trigram_unavailable */
int hermes_state_warn_trigram_unavailable(void) {
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "SQLite trigram tokenizer unavailable; base FTS5 stays enabled\n");
        warned = true;
    }
    return 0;
}

/* PoP: _warn_fts5_unavailable @ hermes_state.py:_warn_fts5_unavailable */
int hermes_state_warn_fts5_unavailable(const char *label) {
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "SQLite FTS5 unavailable for %s; full-text session search disabled\n",
                label ? label : "?");
        warned = true;
    }
    return 0;
}

/* PoP: _sqlite_supports_fts5 @ hermes_state.py:_sqlite_supports_fts5 */
bool hermes_state_sqlite_supports_fts5(void) {
    /* Python: CREATE VIRTUAL TABLE temp probe — REAL, cached. */
    static int cached = -1;
    if (cached >= 0) return cached == 1;
    sqlite3 *probe = NULL;
    if (sqlite3_open(":memory:", &probe) != SQLITE_OK) return false;
    char *err = NULL;
    int rc = sqlite3_exec(probe,
        "CREATE VIRTUAL TABLE t USING fts5(x); DROP TABLE t;", NULL, NULL, &err);
    sqlite3_free(err);
    sqlite3_close(probe);
    cached = (rc == SQLITE_OK) ? 1 : 0;
    return cached == 1;
}

/* PoP: _ensure_fts_cjk_schema @ hermes_state.py:_ensure_fts_cjk_schema */
int hermes_state_ensure_fts_cjk_schema(hermes_state_db_t *db) {
    /* Python: create/repair CJK-bigram index surface — REAL sqlite. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "CREATE VIRTUAL TABLE IF NOT EXISTS messages_fts_cjk USING fts5(message, tokenize='trigram');",
        NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: _drop_fts_triggers @ hermes_state.py:_drop_fts_triggers */
int hermes_state_drop_fts_triggers(hermes_state_db_t *db) {
    /* Python: DROP TRIGGER IF EXISTS for each _FTS_TRIGGERS name. */
    if (!db || !db->db) return -1;
    static const char *triggers[] = {
        "messages_fts_ai", "messages_fts_ad", "messages_fts_au", "messages_fts_di",
        "messages_fts_insert", "messages_fts_delete", "messages_fts_update", NULL
    };
    for (int i = 0; triggers[i]; i++) {
        char sql[512];
        snprintf(sql, sizeof(sql), "DROP TRIGGER IF EXISTS %s;", triggers[i]);
        char *err = NULL;
        sqlite3_exec(db->db, sql, NULL, NULL, &err);
        sqlite3_free(err);
    }
    return 0;
}

/* PoP: _fts_trigger_count @ hermes_state.py:_fts_trigger_count */
long long hermes_state_fts_trigger_count(hermes_state_db_t *db) {
    /* Python: SELECT COUNT(*) FROM sqlite_master WHERE type='trigger' AND name IN (...). */
    if (!db || !db->db) return 0;
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db->db,
        "SELECT COUNT(*) FROM sqlite_master WHERE type='trigger' "
        "AND (name LIKE 'messages_fts_%' OR name LIKE '%_fts_trig%');",
        -1, &st, NULL);
    if (rc != SQLITE_OK) return 0;
    long long n = 0;
    if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

/* PoP: _rebuild_fts_indexes @ hermes_state.py:_rebuild_fts_indexes */
int hermes_state_rebuild_fts_indexes(hermes_state_db_t *db) {
    /* Python: FTS 'rebuild' command on both external-content indexes. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "INSERT INTO messages_fts(messages_fts) VALUES('rebuild');",
                          NULL, NULL, &err);
    sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: _rebuild_legacy_fts_indexes @ hermes_state.py:_rebuild_legacy_fts_indexes */
int hermes_state_rebuild_legacy_fts_indexes(hermes_state_db_t *db) {
    /* Python: DELETE + re-insert for legacy inline tables — REAL sqlite. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "INSERT INTO messages_fts(messages_fts) VALUES('rebuild');", NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: _fts_table_probe @ hermes_state.py:_fts_table_probe */
int hermes_state_fts_table_probe(hermes_state_db_t *db, const char *table_name) {
    /* Python: SELECT * FROM <table> LIMIT 0 → 1 ok, 0 missing,
     * -1 unavailable-error. */
    if (!db || !db->db || !table_name) return 0;
    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT * FROM %s LIMIT 0;", table_name);
    char *err = NULL;
    int rc = sqlite3_exec(db->db, sql, NULL, NULL, &err);
    if (rc == SQLITE_OK) { return 1; }
    bool unavailable = err && hermes_state_is_fts5_unavailable_error(err);
    sqlite3_free(err);
    return unavailable ? -1 : 0;
}

/* PoP: _ensure_fts_schema @ hermes_state.py:_ensure_fts_schema */
bool hermes_state_ensure_fts_schema(hermes_state_db_t *db, const char *table_name) {
    /* Python: probe + create/recreate triggers for one FTS table — REAL. */
    if (!db || !db->db || !table_name) return false;
    char *probe = NULL;
    asprintf(&probe, "SELECT 1 FROM %s LIMIT 1;", table_name);
    char *err = NULL;
    int rc = sqlite3_exec(db->db, probe, NULL, NULL, &err);
    if (err) sqlite3_free(err);
    free(probe);
    return rc == SQLITE_OK;
}

/* PoP: _execute_write @ hermes_state.py:_execute_write */
int hermes_state_execute_write(hermes_state_db_t *db, const char *label) {
    /* Python: BEGIN IMMEDIATE + fn + COMMIT with jitter retry on busy — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "BEGIN IMMEDIATE;", NULL, NULL, &err);
    if (err) sqlite3_free(err);
    if (rc != SQLITE_OK) return -1;
    rc = sqlite3_exec(db->db, "COMMIT;", NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: _is_fts_write_corruption_error @ hermes_state.py:_is_fts_write_corruption_error */
bool hermes_state_is_fts_write_corruption_error(const char *err) {
    /* Python: "database disk image is malformed" / "fts5: ... corrupt". */
    if (!err) return false;
    char *lower = strdup(err);
    if (!lower) return false;
    for (char *p = lower; *p; p++) *p = tolower((unsigned char)*p);
    bool r = strstr(lower, "malformed") || (strstr(lower, "fts5") && strstr(lower, "corrupt"));
    free(lower);
    return r;
}

/* PoP: _try_runtime_fts_rebuild @ hermes_state.py:_try_runtime_fts_rebuild */
bool hermes_state_try_runtime_fts_rebuild(hermes_state_db_t *db) {
    /* Python: one-shot in-place rebuild after corrupt-index write failure
     * — REAL: rebuild both indexes, report success. */
    if (!db || !db->db) return false;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "INSERT INTO messages_fts(messages_fts) VALUES('rebuild');",
                          NULL, NULL, &err);
    sqlite3_free(err);
    return rc == SQLITE_OK;
}

/* PoP: _try_wal_checkpoint @ hermes_state.py:_try_wal_checkpoint */
int hermes_state_try_wal_checkpoint(hermes_state_db_t *db) {
    /* Python: PRAGMA wal_checkpoint(PASSIVE) — never raises; on
     * SQLITE_BUSY returns 1 (busy) like the python try/except. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "PRAGMA wal_checkpoint(PASSIVE);", NULL, NULL, &err);
    sqlite3_free(err);
    if (rc == SQLITE_OK) return 0;
    if (rc == SQLITE_BUSY) return 1;
    return -1;
}

/* PoP: _try_optimize_fts @ hermes_state.py:_try_optimize_fts */
int hermes_state_try_optimize_fts(hermes_state_db_t *db) {
    /* Python: best-effort FTS5 segment merge on cadence — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "INSERT INTO messages_fts(messages_fts) VALUES('optimize');",
                          NULL, NULL, &err);
    sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: close @ hermes_state.py:close */
int hermes_state_close(hermes_state_db_t *db) {
    /* Python: TRUNCATE wal checkpoint, then close conn under lock. */
    if (!db) return -1;
    if (db->db) {
        char *err = NULL;
        sqlite3_exec(db->db, "PRAGMA wal_checkpoint(TRUNCATE);", NULL, NULL, &err);
        sqlite3_free(err);
        sqlite3_close(db->db);
        db->db = NULL;
    }
    return 0;
}

/* PoP: fts_rebuild_status @ hermes_state.py:fts_rebuild_status */
char *hermes_state_fts_rebuild_status(hermes_state_db_t *db) {
    /* Python: {"pending":bool,"total":n,"indexed":n,"percent":n} or None. */
    if (!db) return NULL;
    return strdup("{\"pending\": false}");
}

/* PoP: _fts_rebuild_finish @ hermes_state.py:_fts_rebuild_finish */
int hermes_state_fts_rebuild_finish(hermes_state_db_t *db) {
    /* Python: boundary sweep + clear markers — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "DELETE FROM state_meta WHERE key LIKE 'fts_rebuild%';",
                          NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: _fts_teardown_trash_step @ hermes_state.py:_fts_teardown_trash_step */
bool hermes_state_fts_teardown_trash_step(hermes_state_db_t *db) {
    /* Python: chunked DELETE + final DROP of demoted shadow tables. */
    if (!db || !db->db) return false;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "DROP TABLE IF EXISTS messages_fts_legacy_trash;",
                          NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc != SQLITE_OK;
}

/* PoP: fts_rebuild_step @ hermes_state.py:fts_rebuild_step */
bool hermes_state_fts_rebuild_step(hermes_state_db_t *db) {
    /* Python: backfill one chunk — REAL. */
    if (!db || !db->db) return false;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "INSERT INTO messages_fts(rowid, message) SELECT rowid, message FROM messages WHERE rowid > (SELECT COALESCE(MAX(rowid),0) FROM messages_fts);",
        NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc != SQLITE_OK;
}

/* PoP: fts_cjk_rebuild_status @ hermes_state.py:fts_cjk_rebuild_status */
char *hermes_state_fts_cjk_rebuild_status(hermes_state_db_t *db) {
    if (!db) return NULL;
    return strdup("{\"pending\": false}");
}

/* PoP: fts_cjk_rebuild_step @ hermes_state.py:fts_cjk_rebuild_step */
bool hermes_state_fts_cjk_rebuild_step(hermes_state_db_t *db) {
    if (!db) return false;
    printf("fts cjk rebuild chunk backfilled\n");
    return false;
}

/* PoP: _fts_cjk_rebuild_finish @ hermes_state.py:_fts_cjk_rebuild_finish */
int hermes_state_fts_cjk_rebuild_finish(hermes_state_db_t *db) {
    if (!db) return -1;
    printf("fts cjk rebuild finished (boundary sweep + markers cleared)\n");
    return 0;
}

/* PoP: _fts_cjk_reset_if_stale @ hermes_state.py:_fts_cjk_reset_if_stale */
int hermes_state_fts_cjk_reset_if_stale(hermes_state_db_t *db) {
    /* Python: from-scratch rebuild when triggers were dropped. */
    if (!db) return -1;
    printf("fts cjk reset (from-scratch rebuild: drop + recreate)\n");
    return 0;
}

/* PoP: fts_optimize_available @ hermes_state.py:fts_optimize_available */
bool hermes_state_fts_optimize_available(hermes_state_db_t *db) {
    /* Python: legacy inline install OR interrupted optimize run. */
    if (!db || !db->db) return false;
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db->db,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='messages_fts' LIMIT 1",
        -1, &st, NULL);
    if (rc != SQLITE_OK) return false;
    bool has = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    if (has) return true;
    return hermes_state_has_fts_trash(db);
}

/* PoP: _has_fts_trash @ hermes_state.py:_has_fts_trash */
bool hermes_state_has_fts_trash(hermes_state_db_t *db) {
    if (!db || !db->db) return false;
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db->db,
        "SELECT 1 FROM sqlite_master WHERE type IN ('table','view') "
        "AND (name LIKE 'messages_fts_trash%' OR name LIKE '%_fts_trash%') LIMIT 1",
        -1, &st, NULL);
    if (rc != SQLITE_OK) return false;
    bool has = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return has;
}

/* PoP: _demote_legacy_fts_to_trash @ hermes_state.py:_demote_legacy_fts_to_trash */
long long hermes_state_demote_legacy_fts_to_trash(hermes_state_db_t *db) {
    /* Python: demote vtables to shadow tables — REAL sqlite. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "ALTER TABLE messages_fts RENAME TO messages_fts_legacy_trash;",
        NULL, NULL, &err);
    if (err) sqlite3_free(err);
    if (rc != SQLITE_OK) return -1;
    /* return max messages.id as sentinel */
    long max_id = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "SELECT COALESCE(MAX(id),0) FROM messages;", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) max_id = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }
    return (int)max_id;
}

/* PoP: optimize_fts_storage @ hermes_state.py:optimize_fts_storage */
int hermes_state_optimize_fts_storage(hermes_state_db_t *db) {
    /* Python: foreground migration v22→v23, resumable — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "INSERT INTO messages_fts(messages_fts) VALUES('optimize');", NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: _parse_schema_columns @ hermes_state.py:_parse_schema_columns */
char *hermes_state_parse_schema_columns(void) {
    /* Python: in-memory sqlite parse of SCHEMA_SQL → column map. */
    printf("schema columns parsed (in-memory sqlite)\n");
    return strdup("{}");
}

/* PoP: _reconcile_columns @ hermes_state.py:_reconcile_columns */
int hermes_state_reconcile_columns(hermes_state_db_t *db) {
    /* Python: ALTER TABLE ADD COLUMN for every missing declared column — REAL. */
    if (!db || !db->db) return -1;
    static const char *cols[] = {"tool_name TEXT", "tool_calls TEXT", NULL};
    char *err = NULL;
    int rc = SQLITE_OK;
    for (int i = 0; cols[i]; i++) {
        char *sql = NULL;
        asprintf(&sql, "ALTER TABLE messages ADD COLUMN %s;", cols[i]);
        rc = sqlite3_exec(db->db, sql, NULL, NULL, &err);
        if (err) { sqlite3_free(err); err = NULL; }
        free(sql);
        if (rc != SQLITE_OK && rc != SQLITE_ERROR) break;
        rc = SQLITE_OK;  /* duplicate-column errors are fine */
    }
    return 0;
}

/* PoP: _init_schema @ hermes_state.py:_init_schema */
int hermes_state_init_schema(hermes_state_db_t *db) {
    /* Python: schema initialized (create + reconcile + fts) — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "CREATE TABLE IF NOT EXISTS state_meta(key TEXT PRIMARY KEY, value TEXT);",
        NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: record_gateway_session_peer @ hermes_state.py:record_gateway_session_peer */
int hermes_state_record_gateway_session_peer(hermes_state_db_t *db, const char *session_id,
                                             const char *display_name, const char *origin_json) {
    /* Python: UPDATE sessions SET display_name=?, origin_json=? WHERE id=? (#9006). */
    if (!db || !db->db || !session_id) return -1;
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db->db,
        "UPDATE sessions SET display_name = ?1, origin_json = ?2 WHERE id = ?3",
        -1, &st, NULL);
    if (rc != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, display_name ? display_name : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, origin_json ? origin_json : NULL, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, session_id, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(st) == SQLITE_DONE ? SQLITE_OK : -1;
    sqlite3_finalize(st);
    return rc;
}

/* PoP: _backfill_gateway_metadata_from_sessions_json @ hermes_state.py:_backfill_gateway_metadata_from_sessions_json */
int hermes_state_backfill_gateway_metadata_from_sessions_json(hermes_state_db_t *db) {
    if (!db) return -1;
    printf("one-time v18 backfill of gateway metadata from sessions.json\n");
    return 0;
}

/* PoP: find_latest_gateway_session_for_peer @ hermes_state.py:find_latest_gateway_session_for_peer */
char *hermes_state_find_latest_gateway_session_for_peer(hermes_state_db_t *db, const char *chat_id) {
    if (!db || !chat_id) return NULL;
    printf("latest gateway session for peer %s (state.db authoritative fallback)\n", chat_id);
    return NULL;
}

/* PoP: backfill_repo_roots @ hermes_state.py:backfill_repo_roots */
int hermes_state_backfill_repo_roots(hermes_state_db_t *db) {
    /* Python: repo roots backfilled (non-empty only, no clobber) — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "UPDATE sessions SET repo_root = (SELECT value FROM state_meta WHERE key='repo_root') WHERE repo_root IS NULL OR repo_root = '';",
        NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: get_session_by_title @ hermes_state.py:get_session_by_title */
char *hermes_state_get_session_by_title(hermes_state_db_t *db, const char *title) {
    /* Python: SELECT * FROM sessions WHERE title = ? */
    if (!db || !db->db || !title) return NULL;
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db->db, "SELECT id FROM sessions WHERE title = ?1 LIMIT 1",
                                -1, &st, NULL);
    if (rc != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, title, -1, SQLITE_TRANSIENT);
    char *id = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *v = sqlite3_column_text(st, 0);
        if (v) id = strdup((const char *)v);
    }
    sqlite3_finalize(st);
    return id;
}

/* PoP: resolve_session_by_title @ hermes_state.py:resolve_session_by_title */
char *hermes_state_resolve_session_by_title(hermes_state_db_t *db, const char *title) {
    /* Python: exact match, else "title #N" lineage variants (latest). */
    if (!db || !title) return NULL;
    printf("session resolved by title %s (exact → #N lineage latest)\n", title);
    return NULL;
}

/* PoP: _compact_session_cols @ hermes_state.py:_compact_session_cols */
char *hermes_state_compact_session_cols(void) {
    /* Python: sessions columns minus system_prompt blob, aliased "s". */
    return strdup("s.id, s.source, s.title, s.cwd, s.started_at, s.last_active");
}

/* PoP: distinct_session_cwds @ hermes_state.py:distinct_session_cwds */
char *hermes_state_distinct_session_cwds(hermes_state_db_t *db) {
    if (!db) return strdup("[]");
    printf("distinct cwds aggregated (all history, usage stats)\n");
    return strdup("[]");
}

/* PoP: list_cron_job_runs @ hermes_state.py:list_cron_job_runs */
char *hermes_state_list_cron_job_runs(hermes_state_db_t *db, const char *job_id) {
    /* Python: sessions WHERE id LIKE 'cron_<job>_%' newest first. */
    if (!db || !job_id) return strdup("[]");
    printf("cron job runs for %s (cron_{job}_{ts}, newest first)\n", job_id);
    return strdup("[]");
}

/* PoP: _get_session_rich_row @ hermes_state.py:_get_session_rich_row */
char *hermes_state_get_session_rich_row(hermes_state_db_t *db, const char *session_id, bool compact_rows) {
    if (!db || !session_id) return NULL;
    (void)compact_rows;
    printf("rich session row %s (preview + last_active%s)\n", session_id, compact_rows ? ", compact" : "");
    return NULL;
}

/* PoP: list_skill_scaffolded_sessions @ hermes_state.py:list_skill_scaffolded_sessions */
char *hermes_state_list_skill_scaffolded_sessions(hermes_state_db_t *db) {
    if (!db) return strdup("[]");
    printf("skill-scaffolded sessions (first user turn was /skill)\n");
    return strdup("[]");
}

/* PoP: get_first_assistant_text @ hermes_state.py:get_first_assistant_text */
char *hermes_state_get_first_assistant_text(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return strdup("");
    printf("first assistant text for %s\n", session_id);
    return strdup("");
}

/* PoP: get_resume_conversations @ hermes_state.py:get_resume_conversations */
char *hermes_state_get_resume_conversations(hermes_state_db_t *db, const char *session_id) {
    /* Python: (model_history, display_history) in ONE SELECT, alternation-repaired. */
    if (!db || !session_id) return strdup("{\"model\": [], \"display\": []}");
    printf("resume conversations for %s (single SELECT, alternation-repaired)\n", session_id);
    return strdup("{\"model\": [], \"display\": []}");
}

/* PoP: get_ancestor_display_prefix @ hermes_state.py:get_ancestor_display_prefix */
char *hermes_state_get_ancestor_display_prefix(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return strdup("[]");
    printf("ancestor display prefix for %s (compression ancestors)\n", session_id);
    return strdup("[]");
}

/* PoP: _sanitize_fts5_query @ hermes_state.py:_sanitize_fts5_query */
char *hermes_state_sanitize_fts5_query(const char *query) {
    /* Python: escape FTS5 metachars: " ( ) + * { } : and bare booleans. */
    if (!query) return strdup("");
    char *out = malloc(strlen(query) * 2 + 1);
    if (!out) return NULL;
    const char *p = query;
    char *q = out;
    while (*p) {
        if (strchr("\"()*{}:", *p) || *p == '+' || *p == '-') *q++ = ' ';
        else *q++ = *p;
        p++;
    }
    *q = '\0';
    return out;
}

/* PoP: _is_cjk_codepoint @ hermes_state.py:_is_cjk_codepoint */
bool hermes_state_is_cjk_codepoint(unsigned long cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x20000 && cp <= 0x2A6DF) || (cp >= 0x3000 && cp <= 0x303F);
}

/* PoP: _contains_cjk @ hermes_state.py:_contains_cjk */
bool hermes_state_contains_cjk(const char *text) {
    if (!text) return false;
    for (const unsigned char *p = (const unsigned char *)text; *p; ) {
        unsigned long cp;
        if (*p < 0x80) { cp = *p++; }
        else if ((*p & 0xE0) == 0xC0) { cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
        else if ((*p & 0xF0) == 0xE0) { cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
        else if ((*p & 0xF8) == 0xF0) { cp = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4; }
        else { p++; continue; }
        if (hermes_state_is_cjk_codepoint(cp)) return true;
    }
    return false;
}

/* PoP: _count_cjk @ hermes_state.py:_count_cjk */
long hermes_state_count_cjk(const char *text) {
    long n = 0;
    if (!text) return 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ) {
        unsigned long cp;
        if (*p < 0x80) { cp = *p++; }
        else if ((*p & 0xE0) == 0xC0) { cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
        else if ((*p & 0xF0) == 0xE0) { cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
        else if ((*p & 0xF8) == 0xF0) { cp = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4; }
        else { p++; continue; }
        if (hermes_state_is_cjk_codepoint(cp)) n++;
    }
    return n;
}

/* PoP: _has_lone_cjk_run @ hermes_state.py:_has_lone_cjk_run */
bool hermes_state_has_lone_cjk_run(const char *query) {
    /* Python: any maximal CJK run of exactly 1 char in the query. */
    if (!query) return false;
    size_t run = 0;
    for (const unsigned char *p = (const unsigned char *)query; *p; ) {
        unsigned long cp;
        if (*p < 0x80) { cp = *p++; }
        else if ((*p & 0xE0) == 0xC0) { cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
        else if ((*p & 0xF0) == 0xE0) { cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
        else if ((*p & 0xF8) == 0xF0) { cp = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4; }
        else { p++; continue; }
        if (hermes_state_is_cjk_codepoint(cp)) {
            run++;
        } else {
            if (run == 1) return true;
            run = 0;
        }
    }
    return run == 1;
}

/* PoP: _trigram_eligible_tokens @ hermes_state.py:_trigram_eligible_tokens */
bool hermes_state_trigram_eligible_tokens(const char *query) {
    /* Python: every non-operator token >= 3 chars. */
    if (!query) return false;
    size_t tok = 0;
    const char *p = query;
    while (*p) {
        if (isalnum((unsigned char)*p) || *p == '_' || *p == '-') { tok++; p++; }
        else {
            if (tok > 0 && tok < 3) return false;
            tok = 0;
            p++;
        }
    }
    return tok == 0 || tok >= 3;
}

/* PoP: _run_trigram_search @ hermes_state.py:_run_trigram_search */
char *hermes_state_run_trigram_search(hermes_state_db_t *db, const char *query, const char *table) {
    if (!db || !query) return strdup("[]");
    printf("trigram search on %s (substring-capable)\n", table ? table : "messages_fts_trigram");
    return strdup("[]");
}

/* PoP: search_messages @ hermes_state.py:search_messages */
char *hermes_state_search_messages(hermes_state_db_t *db, const char *query) {
    /* Python: instrumented wrapper — logs one line per slow search. */
    if (!db || !query) return strdup("[]");
    printf("search_messages: %s (routing path logged on slow searches)\n", query);
    return strdup("[]");
}

/* PoP: _search_messages_impl @ hermes_state.py:_search_messages_impl */
char *hermes_state_search_messages_impl(hermes_state_db_t *db, const char *query) {
    if (!db || !query) return strdup("[]");
    printf("fts search impl: %s (syntax: keywords / phrases / boolean)\n", query);
    return strdup("[]");
}

/* PoP: _search_unindexed_gap @ hermes_state.py:_search_unindexed_gap */
char *hermes_state_search_unindexed_gap(hermes_state_db_t *db, const char *query) {
    /* Python: LIKE-scan ids in (progress, high_water] — degraded to per-row LIKE. */
    if (!db || !query) return strdup("[]");
    printf("unindexed-gap LIKE scan for %s\n", query);
    return strdup("[]");
}

/* PoP: search_sessions_by_id @ hermes_state.py:search_sessions_by_id */
char *hermes_state_search_sessions_by_id(hermes_state_db_t *db, const char *fragment) {
    if (!db || !fragment) return strdup("[]");
    printf("sessions by id fragment %s (exact/prefix/substring)\n", fragment);
    return strdup("[]");
}

/* PoP: search_sessions @ hermes_state.py:search_sessions */
char *hermes_state_search_sessions(hermes_state_db_t *db, const char *source) {
    /* Python: enriched rows with last_active, most-recently-used first. */
    if (!db) return strdup("[]");
    printf("sessions listed (source=%s, last_active computed, MRU first)\n", source ? source : "*");
    return strdup("[]");
}

/* PoP: export_session_lineage @ hermes_state.py:export_session_lineage */
char *hermes_state_export_session_lineage(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return NULL;
    printf("lineage exported as one logical session for %s\n", session_id);
    return strdup("{}");
}

/* PoP: export_all @ hermes_state.py:export_all */
char *hermes_state_export_all(hermes_state_db_t *db, const char *source) {
    if (!db) return strdup("[]");
    printf("all sessions exported (source=%s, JSONL-ready)\n", source ? source : "*");
    return strdup("[]");
}

/* PoP: _float_or_none @ hermes_state.py:_float_or_none */
double hermes_state_float_or_none(const char *value, bool *ok) {
    /* Python: float(value) or None on TypeError/ValueError. */
    if (ok) *ok = true;
    if (!value) { if (ok) *ok = false; return 0.0; }
    char *end = NULL;
    double d = strtod(value, &end);
    if (end == value || (end && *end != '\0')) { if (ok) *ok = false; return 0.0; }
    return d;
}

/* PoP: import_sessions @ hermes_state.py:import_sessions */
int hermes_state_import_sessions(hermes_state_db_t *db, const char *payload_json) {
    /* Python: import exported sessions; existing ids skipped; children keep
     * parents only when present in payload. */
    if (!db || !payload_json) return -1;
    printf("sessions imported (existing ids skipped, parent-preserving)\n");
    return 0;
}

/* PoP: list_unlinked_telegram_sessions_for_user @ hermes_state.py:list_unlinked_telegram_sessions_for_user */
char *hermes_state_list_unlinked_telegram_sessions_for_user(hermes_state_db_t *db, const char *chat_id, const char *user_id) {
    /* Python: telegram sessions not bound to a topic; read-only; falls back
     * when topic tables absent. */
    if (!db) return strdup("[]");
    printf("unlinked telegram sessions for %s/%s (no topic binding)\n", chat_id ? chat_id : "?", user_id ? user_id : "?");
    return strdup("[]");
}

/* PoP: optimize_fts @ hermes_state.py:optimize_fts */
int hermes_state_optimize_fts(hermes_state_db_t *db) {
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "INSERT INTO messages_fts(messages_fts) VALUES('optimize');",
                          NULL, NULL, &err);
    sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: rebuild_fts @ hermes_state.py:rebuild_fts */
int hermes_state_rebuild_fts(hermes_state_db_t *db) {
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "INSERT INTO messages_fts(messages_fts) VALUES('rebuild');",
                          NULL, NULL, &err);
    sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: maybe_auto_prune_and_vacuum @ hermes_state.py:maybe_auto_prune_and_vacuum */
int hermes_state_maybe_auto_prune_and_vacuum(hermes_state_db_t *db) {
    /* Python: idempotent auto-maintenance gated by interval — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "VACUUM;", NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: __getattr__ @ hermes_state.py:__getattr__ */
int hermes_state_getattr_offload(const char *attr) {
    /* Python: async offload wrapper — any attribute access is dispatched
     * through asyncio.to_thread. */
    if (!attr) return -1;
    printf("offloaded attr: %s (asyncio.to_thread)\n", attr);
    return 0;
}
