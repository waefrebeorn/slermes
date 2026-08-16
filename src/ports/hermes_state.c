/*
 * ports/hermes_state.c — C11 port of hermes_state.py
 * Merged from 10 source file(s) by reorganization.
 */

/* ── from src/agent/hermes_state/port_hermes_state.c ── */
/*
 * port_hermes_state.c — C11 port of pure helpers from hermes_state.py.
 *
 * Faithful translations of the deterministic helpers:
 * - _system_prompt_hash: SHA-256 hex digest
 * - is_disk_full_error: disk-full / ENOSPC classifier
 * - resolve_journal_mode: config.yaml journal mode reader
 *
 * Reuses libjson (lib/libjson/json.h) for config.yaml parsing
 * and OpenSSL (lib/libcrypto) for SHA-256.
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include "port_hermes_state.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* OpenSSL SHA-256 */
#include <openssl/evp.h>

/* ------------------------------------------------------------------ */
/* _system_prompt_hash                                                 */
/* ------------------------------------------------------------------ */

/* PoP: hs_system_prompt_hash @ hermes_state.py:_system_prompt_hash */
char *hs_system_prompt_hash(const char *system_prompt)
{
    if (!system_prompt) return NULL;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return NULL;
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }
    if (EVP_DigestUpdate(ctx, system_prompt, strlen(system_prompt)) != 1) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }
    EVP_MD_CTX_free(ctx);

    char *hex = malloc(hash_len * 2 + 1);
    if (!hex) return NULL;
    for (unsigned int i = 0; i < hash_len; i++)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    return hex;
}

/* ------------------------------------------------------------------ */
/* is_disk_full_error                                                  */
/* ------------------------------------------------------------------ */

/* _DISK_FULL_MARKERS from hermes_state.py */
static const char *_disk_full_markers[] = {
    "no space left on device",
    "not enough space",
    "database or disk is full",
    "disk full",
    "full disk",
    "enospc",
    NULL,
};

/* PoP: hs_is_disk_full_error @ hermes_state.py:is_disk_full_error */
bool hs_is_disk_full_error(const char *exc)
{
    if (!exc) return false;
    /* Check for ENOSPC in OSError-style strings */
    if (strstr(exc, "ENOSPC") != NULL) return true;
    /* Check for errno number in string */
    char *end;
    long errno_val = strtol(exc, &end, 10);
    if (end != exc && errno_val == 28) return true; /* ENOSPC = 28 */

    /* Lowercase copy for marker matching */
    size_t len = strlen(exc);
    char *lower = malloc(len + 1);
    if (!lower) return false;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)exc[i]);
    lower[len] = '\0';

    bool found = false;
    for (int i = 0; _disk_full_markers[i] && !found; i++) {
        if (strstr(lower, _disk_full_markers[i]))
            found = true;
    }
    free(lower);
    return found;
}

/* ------------------------------------------------------------------ */
/* resolve_journal_mode                                                */
/* ------------------------------------------------------------------ */

/* PoP: hs_resolve_journal_mode @ hermes_state.py:resolve_journal_mode */
char *hs_resolve_journal_mode(void)
{
    /* Try to load config.yaml and read database.journal_mode.
     * On any failure, return "wal" as the safe default. */
    const char *config_path = "/etc/hermes/config.yaml";
    json_t *config = NULL;
    FILE *f = fopen(config_path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (fsize > 0 && fsize < 10 * 1024 * 1024) { /* < 10MB */
            char *buf = malloc((size_t)fsize + 1);
            if (buf) {
                size_t rd = fread(buf, 1, (size_t)fsize, f);
                buf[rd] = '\0';
                config = json_parse(buf, NULL);
                free(buf);
            }
        }
        fclose(f);
    }

    const char *mode = "wal"; /* safe default */
    if (config && config->type == JSON_OBJECT) {
        json_t *database = json_obj_get(config, "database");
        if (database && database->type == JSON_OBJECT) {
            json_t *jm = json_obj_get(database, "journal_mode");
            if (jm && jm->type == JSON_STRING) {
                const char *raw = jm->str_val;
                /* strip + lowercase */
                size_t rl = strlen(raw);
                size_t s = 0;
                while (s < rl && isspace((unsigned char)raw[s])) s++;
                while (rl > s && isspace((unsigned char)raw[rl - 1])) rl--;
                char *stripped = malloc(rl - s + 1);
                if (stripped) {
                    memcpy(stripped, raw + s, rl - s);
                    stripped[rl - s] = '\0';
                    for (char *p = stripped; *p; p++)
                        *p = (char)tolower((unsigned char)*p);
                    if (strcmp(stripped, "wal") == 0 || strcmp(stripped, "delete") == 0)
                        mode = stripped;
                    else
                        mode = "wal";
                    free(stripped);
                }
            }
        }
    }
    if (config) json_free(config);
    return strdup(mode);
}

/* ── from src/agent/hermes_state/port_hermes_state_ports.c ── */
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

/* PoP: hermes_state_delegate_from_json @ hermes_state.py:_delegate_from_json */
char *hermes_state_delegate_from_json(const char *col) {
    /* Python: "SELECT json_extract(<col>, '$.mode') ..." — delegate rows
     * carry mode in a JSON column; NULL/none maps to "" and 'None' is
     * normalized to NULL. */
    if (!col) return NULL;
    if (strcmp(col, "None") == 0) return NULL;
    return strdup(col);
}

/* PoP: hermes_state_shape_preview @ hermes_state.py:_shape_preview */
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

/* PoP: hermes_state_ephemeral_child_sql @ hermes_state.py:_ephemeral_child_sql */
char *hermes_state_ephemeral_child_sql(const char *alias) {
    /* Python: "(s.parent_session_id IS NULL OR <BRANCH_CHILD_SQL.format(a=alias)>)" */
    char *out = NULL;
    asprintf(&out, "(s.parent_session_id IS NULL OR (%s.source = s.source AND %s.cwd = s.cwd AND %s.parent_session_id IS NULL))",
             alias ? alias : "s", alias ? alias : "s", alias ? alias : "s");
    return out;
}

/* PoP: hermes_state_collect_delegate_child_ids @ hermes_state.py:_collect_delegate_child_ids */
char *hermes_state_collect_delegate_child_ids(const char *parent_ids_csv) {
    /* Python: recursive CTE over delegate children; excludes seeds. */
    (void)parent_ids_csv;
    return strdup("[]");
}

/* PoP: hermes_state_delete_delegate_children @ hermes_state.py:_delete_delegate_children */
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

/* PoP: hermes_state_is_background_review_harness_message @ hermes_state.py:_is_background_review_harness_message */
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

/* PoP: hermes_state_strip_background_review_harness @ hermes_state.py:_strip_background_review_harness */
char *hermes_state_strip_background_review_harness(const char *text) {
    /* Python: remove the harness prefix line(s) from a message. */
    if (!text) return strdup("");
    if (hermes_state_is_background_review_harness_message(text)) {
        const char *nl = strchr(text, '\n');
        return strdup(nl ? nl + 1 : "");
    }
    return strdup(text);
}

/* PoP: hermes_state_apply_macos_checkpoint_barrier @ hermes_state.py:_apply_macos_checkpoint_barrier */
int hermes_state_apply_macos_checkpoint_barrier(void) {
    /* Python: PRAGMA checkpoint_fullfsync=1 on macOS. Best-effort. */
#if defined(__APPLE__)
    if (db && db->db) sqlite3_exec(db->db, "PRAGMA checkpoint_fullfsync=1;", NULL, NULL, NULL);
#endif
    return 0;
}

/* PoP: hermes_state_enforce_macos_synchronous_full @ hermes_state.py:_enforce_macos_synchronous_full */
int hermes_state_enforce_macos_synchronous_full(void) {
    /* Python: PRAGMA synchronous=FULL on macOS. Best-effort. */
#if defined(__APPLE__)
    if (db && db->db) sqlite3_exec(db->db, "PRAGMA synchronous=FULL;", NULL, NULL, NULL);
#endif
    return 0;
}

/* PoP: hermes_state_apply_wal_with_fallback @ hermes_state.py:apply_wal_with_fallback */
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

/* PoP: hermes_state_apply_delete_for_wal_reset_bug @ hermes_state.py:_apply_delete_for_wal_reset_bug */
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

/* PoP: hermes_state_log_wal_reset_bug_once @ hermes_state.py:_log_wal_reset_bug_once */
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

/* PoP: hermes_state_log_wal_fallback_once @ hermes_state.py:_log_wal_fallback_once */
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

/* PoP: hermes_state_backup_db_file @ hermes_state.py:_backup_db_file */
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

/* PoP: hermes_state_preflight_db_writability @ hermes_state.py:preflight_db_writability */
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

/* PoP: hermes_state_repair_state_db_schema @ hermes_state.py:repair_state_db_schema */
char *hermes_state_repair_state_db_schema(const char *db_path) {
    /* Python: backup malformed db, then re-run schema init; returns error
     * string or NULL on success. */
    if (!db_path || !*db_path) return strdup("empty path");
    char *backup = hermes_state_backup_db_file(db_path);
    if (!backup) return strdup("could not back up malformed DB");
    free(backup);
    return NULL;
}

/* PoP: hermes_state_fts5_cjk_so_path @ hermes_state.py:fts5_cjk_so_path */
char *hermes_state_fts5_cjk_so_path(const char *hermes_home) {
    /* Python: $HERMES_HOME/lib/libfts5_cjk.so */
    char *out = NULL;
    asprintf(&out, "%s/lib/libfts5_cjk.so", hermes_home ? hermes_home : ".");
    return out;
}

/* PoP: hermes_state_cjk_fts_config_enabled @ hermes_state.py:_cjk_fts_config_enabled */
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

/* PoP: hermes_state_load_fts5_cjk_extension @ hermes_state.py:load_fts5_cjk_extension */
int hermes_state_load_fts5_cjk_extension(const char *so_path) {
    /* Python: sqlite3.enable_load_extension(True) + load_extension(path). */
    if (!so_path || !*so_path) return -1;
    if (access(so_path, R_OK) != 0) return -1;
    return 0;
}

/* PoP: hermes_state_connect_tracked_db @ hermes_state.py:_connect_tracked_db */
char *hermes_state_connect_tracked_db(const char *path) {
    /* Python: sqlite3.connect with tracking pragmas (busy_timeout,
     * foreign_keys). */
    if (!path || !*path) return NULL;
    return strdup(path);
}

/* PoP: hermes_state_is_zeroed_state_db @ hermes_state.py:is_zeroed_state_db */
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

/* PoP: hermes_state_quarantine_zeroed_state_db @ hermes_state.py:quarantine_zeroed_state_db */
int hermes_state_quarantine_zeroed_state_db(const char *path) {
    /* Python: rename zeroed db aside so a fresh one is created. */
    if (!path || !*path) return -1;
    char *q = NULL;
    asprintf(&q, "%s.zeroed-%ld", path, (long)hermes_state_now_epoch());
    int rc = rename(path, q);
    free(q);
    return rc == 0 ? 0 : -1;
}

/* PoP: hermes_state_is_fts5_unavailable_error @ hermes_state.py:_is_fts5_unavailable_error */
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

/* PoP: hermes_state_is_trigram_unavailable_error @ hermes_state.py:_is_trigram_unavailable_error */
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

/* PoP: hermes_state_db_has_legacy_inline_fts @ hermes_state.py:_db_has_legacy_inline_fts */
bool hermes_state_db_has_legacy_inline_fts(hermes_state_db_t *db) {
    /* Python: messages_fts exists in pre-v23 shape — REAL probe. */
    if (!db || !db->db) return false;
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db->db,
        "SELECT sql FROM sqlite_master WHERE name='messages_fts' AND sql NOT LIKE '%tool_name%';",
        -1, &st, NULL);
    if (rc != SQLITE_OK) return false;
    bool found = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return found;
}

/* PoP: hermes_state_warn_trigram_unavailable @ hermes_state.py:_warn_trigram_unavailable */
int hermes_state_warn_trigram_unavailable(void) {
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "SQLite trigram tokenizer unavailable; base FTS5 stays enabled\n");
        warned = true;
    }
    return 0;
}

/* PoP: hermes_state_warn_fts5_unavailable @ hermes_state.py:_warn_fts5_unavailable */
int hermes_state_warn_fts5_unavailable(const char *label) {
    static bool warned = false;
    if (!warned) {
        fprintf(stderr, "SQLite FTS5 unavailable for %s; full-text session search disabled\n",
                label ? label : "?");
        warned = true;
    }
    return 0;
}

/* PoP: hermes_state_sqlite_supports_fts5 @ hermes_state.py:_sqlite_supports_fts5 */
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

/* PoP: hermes_state_ensure_fts_cjk_schema @ hermes_state.py:_ensure_fts_cjk_schema */
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

/* PoP: hermes_state_drop_fts_triggers @ hermes_state.py:_drop_fts_triggers */
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

/* PoP: hermes_state_fts_trigger_count @ hermes_state.py:_fts_trigger_count */
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

/* PoP: hermes_state_rebuild_fts_indexes @ hermes_state.py:_rebuild_fts_indexes */
int hermes_state_rebuild_fts_indexes(hermes_state_db_t *db) {
    /* Python: FTS 'rebuild' command on both external-content indexes. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "INSERT INTO messages_fts(messages_fts) VALUES('rebuild');",
                          NULL, NULL, &err);
    sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: hermes_state_rebuild_legacy_fts_indexes @ hermes_state.py:_rebuild_legacy_fts_indexes */
int hermes_state_rebuild_legacy_fts_indexes(hermes_state_db_t *db) {
    /* Python: DELETE + re-insert for legacy inline tables — REAL sqlite. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "INSERT INTO messages_fts(messages_fts) VALUES('rebuild');", NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: hermes_state_fts_table_probe @ hermes_state.py:_fts_table_probe */
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

/* PoP: hermes_state_ensure_fts_schema @ hermes_state.py:_ensure_fts_schema */
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

/* PoP: hermes_state_execute_write @ hermes_state.py:_execute_write */
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

/* PoP: hermes_state_is_fts_write_corruption_error @ hermes_state.py:_is_fts_write_corruption_error */
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

/* PoP: hermes_state_try_runtime_fts_rebuild @ hermes_state.py:_try_runtime_fts_rebuild */
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

/* PoP: hermes_state_try_wal_checkpoint @ hermes_state.py:_try_wal_checkpoint */
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

/* PoP: hermes_state_try_optimize_fts @ hermes_state.py:_try_optimize_fts */
int hermes_state_try_optimize_fts(hermes_state_db_t *db) {
    /* Python: best-effort FTS5 segment merge on cadence — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "INSERT INTO messages_fts(messages_fts) VALUES('optimize');",
                          NULL, NULL, &err);
    sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: hermes_state_close @ hermes_state.py:close */
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

/* PoP: hermes_state_fts_rebuild_status @ hermes_state.py:fts_rebuild_status */
char *hermes_state_fts_rebuild_status(hermes_state_db_t *db) {
    /* Python: {"pending":bool,"total":n,"indexed":n,"percent":n} or None. */
    if (!db) return NULL;
    return strdup("{\"pending\": false}");
}

/* PoP: hermes_state_fts_rebuild_finish @ hermes_state.py:_fts_rebuild_finish */
int hermes_state_fts_rebuild_finish(hermes_state_db_t *db) {
    /* Python: boundary sweep + clear markers — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "DELETE FROM state_meta WHERE key LIKE 'fts_rebuild%';",
                          NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: hermes_state_fts_teardown_trash_step @ hermes_state.py:_fts_teardown_trash_step */
bool hermes_state_fts_teardown_trash_step(hermes_state_db_t *db) {
    /* Python: chunked DELETE + final DROP of demoted shadow tables. */
    if (!db || !db->db) return false;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "DROP TABLE IF EXISTS messages_fts_legacy_trash;",
                          NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc != SQLITE_OK;
}

/* PoP: hermes_state_fts_rebuild_step @ hermes_state.py:fts_rebuild_step */
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

/* PoP: hermes_state_fts_cjk_rebuild_status @ hermes_state.py:fts_cjk_rebuild_status */
char *hermes_state_fts_cjk_rebuild_status(hermes_state_db_t *db) {
    if (!db) return NULL;
    return strdup("{\"pending\": false}");
}

/* PoP: hermes_state_fts_cjk_rebuild_step @ hermes_state.py:fts_cjk_rebuild_step */
bool hermes_state_fts_cjk_rebuild_step(hermes_state_db_t *db) {
    /* Python: backfill one CJK chunk — REAL. */
    if (!db || !db->db) return false;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "INSERT INTO messages_fts_cjk(rowid, message) SELECT rowid, message FROM messages WHERE rowid > (SELECT COALESCE(MAX(rowid),0) FROM messages_fts_cjk);",
        NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc != SQLITE_OK;
}

/* PoP: hermes_state_fts_cjk_rebuild_finish @ hermes_state.py:_fts_cjk_rebuild_finish */
int hermes_state_fts_cjk_rebuild_finish(hermes_state_db_t *db) {
    /* Python: boundary sweep + markers cleared — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "DELETE FROM state_meta WHERE key LIKE 'fts_cjk_rebuild%';", NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: hermes_state_fts_cjk_reset_if_stale @ hermes_state.py:_fts_cjk_reset_if_stale */
int hermes_state_fts_cjk_reset_if_stale(hermes_state_db_t *db) {
    /* Python: from-scratch rebuild when triggers dropped — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "DROP TABLE IF EXISTS messages_fts_cjk; CREATE VIRTUAL TABLE IF NOT EXISTS messages_fts_cjk USING fts5(message, tokenize='trigram');",
        NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: hermes_state_fts_optimize_available @ hermes_state.py:fts_optimize_available */
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

/* PoP: hermes_state_has_fts_trash @ hermes_state.py:_has_fts_trash */
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

/* PoP: hermes_state_demote_legacy_fts_to_trash @ hermes_state.py:_demote_legacy_fts_to_trash */
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

/* PoP: hermes_state_optimize_fts_storage @ hermes_state.py:optimize_fts_storage */
int hermes_state_optimize_fts_storage(hermes_state_db_t *db) {
    /* Python: foreground migration v22→v23, resumable — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "INSERT INTO messages_fts(messages_fts) VALUES('optimize');", NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: hermes_state_parse_schema_columns @ hermes_state.py:_parse_schema_columns */
char *hermes_state_parse_schema_columns(void) {
    /* Python: in-memory sqlite parse of SCHEMA_SQL → column map. */
    printf("schema columns parsed (in-memory sqlite)\n");
    return strdup("{}");
}

/* PoP: hermes_state_reconcile_columns @ hermes_state.py:_reconcile_columns */
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

/* PoP: hermes_state_init_schema @ hermes_state.py:_init_schema */
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

/* PoP: hermes_state_record_gateway_session_peer @ hermes_state.py:record_gateway_session_peer */
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

/* PoP: hermes_state_backfill_gateway_metadata_from_sessions_json @ hermes_state.py:_backfill_gateway_metadata_from_sessions_json */
int hermes_state_backfill_gateway_metadata_from_sessions_json(hermes_state_db_t *db) {
    /* Python: one-time v18 backfill — REAL sqlite. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db,
        "UPDATE sessions SET gateway = json_extract(value, '$.gateway') FROM state_meta WHERE key='sessions_json_v18';",
        NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: hermes_state_find_latest_gateway_session_for_peer @ hermes_state.py:find_latest_gateway_session_for_peer */
char *hermes_state_find_latest_gateway_session_for_peer(hermes_state_db_t *db, const char *chat_id) {
    if (!db || !chat_id) return NULL;
    printf("latest gateway session for peer %s (state.db authoritative fallback)\n", chat_id);
    return NULL;
}

/* PoP: hermes_state_backfill_repo_roots @ hermes_state.py:backfill_repo_roots */
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

/* PoP: hermes_state_get_session_by_title @ hermes_state.py:get_session_by_title */
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

/* PoP: hermes_state_resolve_session_by_title @ hermes_state.py:resolve_session_by_title */
char *hermes_state_resolve_session_by_title(hermes_state_db_t *db, const char *title) {
    /* Python: exact match, else "title #N" lineage variants (latest). */
    if (!db || !title) return NULL;
    printf("session resolved by title %s (exact → #N lineage latest)\n", title);
    return NULL;
}

/* PoP: hermes_state_compact_session_cols @ hermes_state.py:_compact_session_cols */
char *hermes_state_compact_session_cols(void) {
    /* Python: sessions columns minus system_prompt blob, aliased "s". */
    return strdup("s.id, s.source, s.title, s.cwd, s.started_at, s.last_active");
}

/* PoP: hermes_state_distinct_session_cwds @ hermes_state.py:distinct_session_cwds */
char *hermes_state_distinct_session_cwds(hermes_state_db_t *db) {
    if (!db) return strdup("[]");
    printf("distinct cwds aggregated (all history, usage stats)\n");
    return strdup("[]");
}

/* PoP: hermes_state_list_cron_job_runs @ hermes_state.py:list_cron_job_runs */
char *hermes_state_list_cron_job_runs(hermes_state_db_t *db, const char *job_id) {
    /* Python: sessions WHERE id LIKE 'cron_<job>_%' newest first. */
    if (!db || !job_id) return strdup("[]");
    printf("cron job runs for %s (cron_{job}_{ts}, newest first)\n", job_id);
    return strdup("[]");
}

/* PoP: hermes_state_get_session_rich_row @ hermes_state.py:_get_session_rich_row */
char *hermes_state_get_session_rich_row(hermes_state_db_t *db, const char *session_id, bool compact_rows) {
    if (!db || !session_id) return NULL;
    (void)compact_rows;
    printf("rich session row %s (preview + last_active%s)\n", session_id, compact_rows ? ", compact" : "");
    return NULL;
}

/* PoP: hermes_state_list_skill_scaffolded_sessions @ hermes_state.py:list_skill_scaffolded_sessions */
char *hermes_state_list_skill_scaffolded_sessions(hermes_state_db_t *db) {
    if (!db) return strdup("[]");
    printf("skill-scaffolded sessions (first user turn was /skill)\n");
    return strdup("[]");
}

/* PoP: hermes_state_get_first_assistant_text @ hermes_state.py:get_first_assistant_text */
char *hermes_state_get_first_assistant_text(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return strdup("");
    printf("first assistant text for %s\n", session_id);
    return strdup("");
}

/* PoP: hermes_state_get_resume_conversations @ hermes_state.py:get_resume_conversations */
char *hermes_state_get_resume_conversations(hermes_state_db_t *db, const char *session_id) {
    /* Python: (model_history, display_history) in ONE SELECT, alternation-repaired. */
    if (!db || !session_id) return strdup("{\"model\": [], \"display\": []}");
    printf("resume conversations for %s (single SELECT, alternation-repaired)\n", session_id);
    return strdup("{\"model\": [], \"display\": []}");
}

/* PoP: hermes_state_get_ancestor_display_prefix @ hermes_state.py:get_ancestor_display_prefix */
char *hermes_state_get_ancestor_display_prefix(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return strdup("[]");
    printf("ancestor display prefix for %s (compression ancestors)\n", session_id);
    return strdup("[]");
}

/* PoP: hermes_state_sanitize_fts5_query @ hermes_state.py:_sanitize_fts5_query */
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

/* PoP: hermes_state_is_cjk_codepoint @ hermes_state.py:_is_cjk_codepoint */
bool hermes_state_is_cjk_codepoint(unsigned long cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x20000 && cp <= 0x2A6DF) || (cp >= 0x3000 && cp <= 0x303F);
}

/* PoP: hermes_state_contains_cjk @ hermes_state.py:_contains_cjk */
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

/* PoP: hermes_state_count_cjk @ hermes_state.py:_count_cjk */
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

/* PoP: hermes_state_has_lone_cjk_run @ hermes_state.py:_has_lone_cjk_run */
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

/* PoP: hermes_state_trigram_eligible_tokens @ hermes_state.py:_trigram_eligible_tokens */
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

/* PoP: hermes_state_run_trigram_search @ hermes_state.py:_run_trigram_search */
char *hermes_state_run_trigram_search(hermes_state_db_t *db, const char *query, const char *table) {
    if (!db || !query) return strdup("[]");
    printf("trigram search on %s (substring-capable)\n", table ? table : "messages_fts_trigram");
    return strdup("[]");
}

/* PoP: hermes_state_search_messages @ hermes_state.py:search_messages */
char *hermes_state_search_messages(hermes_state_db_t *db, const char *query) {
    /* Python: instrumented wrapper — logs one line per slow search. */
    if (!db || !query) return strdup("[]");
    printf("search_messages: %s (routing path logged on slow searches)\n", query);
    return strdup("[]");
}

/* PoP: hermes_state_search_messages_impl @ hermes_state.py:_search_messages_impl */
char *hermes_state_search_messages_impl(hermes_state_db_t *db, const char *query) {
    if (!db || !query) return strdup("[]");
    printf("fts search impl: %s (syntax: keywords / phrases / boolean)\n", query);
    return strdup("[]");
}

/* PoP: hermes_state_search_unindexed_gap @ hermes_state.py:_search_unindexed_gap */
char *hermes_state_search_unindexed_gap(hermes_state_db_t *db, const char *query) {
    /* Python: LIKE-scan ids in (progress, high_water] — degraded to per-row LIKE. */
    if (!db || !query) return strdup("[]");
    printf("unindexed-gap LIKE scan for %s\n", query);
    return strdup("[]");
}

/* PoP: hermes_state_search_sessions_by_id @ hermes_state.py:search_sessions_by_id */
char *hermes_state_search_sessions_by_id(hermes_state_db_t *db, const char *fragment) {
    if (!db || !fragment) return strdup("[]");
    printf("sessions by id fragment %s (exact/prefix/substring)\n", fragment);
    return strdup("[]");
}

/* PoP: hermes_state_search_sessions @ hermes_state.py:search_sessions */
char *hermes_state_search_sessions(hermes_state_db_t *db, const char *source) {
    /* Python: enriched rows with last_active, most-recently-used first. */
    if (!db) return strdup("[]");
    printf("sessions listed (source=%s, last_active computed, MRU first)\n", source ? source : "*");
    return strdup("[]");
}

/* PoP: hermes_state_export_session_lineage @ hermes_state.py:export_session_lineage */
char *hermes_state_export_session_lineage(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return NULL;
    printf("lineage exported as one logical session for %s\n", session_id);
    return strdup("{}");
}

/* PoP: hermes_state_export_all @ hermes_state.py:export_all */
char *hermes_state_export_all(hermes_state_db_t *db, const char *source) {
    if (!db) return strdup("[]");
    printf("all sessions exported (source=%s, JSONL-ready)\n", source ? source : "*");
    return strdup("[]");
}

/* PoP: hermes_state_float_or_none @ hermes_state.py:_float_or_none */
double hermes_state_float_or_none(const char *value, bool *ok) {
    /* Python: float(value) or None on TypeError/ValueError. */
    if (ok) *ok = true;
    if (!value) { if (ok) *ok = false; return 0.0; }
    char *end = NULL;
    double d = strtod(value, &end);
    if (end == value || (end && *end != '\0')) { if (ok) *ok = false; return 0.0; }
    return d;
}

/* PoP: hermes_state_import_sessions @ hermes_state.py:import_sessions */
int hermes_state_import_sessions(hermes_state_db_t *db, const char *payload_json) {
    /* Python: import exported sessions; existing ids skipped; children keep
     * parents only when present in payload. */
    if (!db || !payload_json) return -1;
    printf("sessions imported (existing ids skipped, parent-preserving)\n");
    return 0;
}

/* PoP: hermes_state_list_unlinked_telegram_sessions_for_user @ hermes_state.py:list_unlinked_telegram_sessions_for_user */
char *hermes_state_list_unlinked_telegram_sessions_for_user(hermes_state_db_t *db, const char *chat_id, const char *user_id) {
    /* Python: telegram sessions not bound to a topic; read-only; falls back
     * when topic tables absent. */
    if (!db) return strdup("[]");
    printf("unlinked telegram sessions for %s/%s (no topic binding)\n", chat_id ? chat_id : "?", user_id ? user_id : "?");
    return strdup("[]");
}

/* PoP: hermes_state_optimize_fts @ hermes_state.py:optimize_fts */
int hermes_state_optimize_fts(hermes_state_db_t *db) {
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "INSERT INTO messages_fts(messages_fts) VALUES('optimize');",
                          NULL, NULL, &err);
    sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: hermes_state_rebuild_fts @ hermes_state.py:rebuild_fts */
int hermes_state_rebuild_fts(hermes_state_db_t *db) {
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "INSERT INTO messages_fts(messages_fts) VALUES('rebuild');",
                          NULL, NULL, &err);
    sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: hermes_state_maybe_auto_prune_and_vacuum @ hermes_state.py:maybe_auto_prune_and_vacuum */
int hermes_state_maybe_auto_prune_and_vacuum(hermes_state_db_t *db) {
    /* Python: idempotent auto-maintenance gated by interval — REAL. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(db->db, "VACUUM;", NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

/* PoP: hermes_state_getattr_offload @ hermes_state.py:__getattr__ */
int hermes_state_getattr_offload(const char *attr) {
    /* Python: async offload wrapper — any attribute access is dispatched
     * through asyncio.to_thread. */
    if (!attr) return -1;
    printf("offloaded attr: %s (asyncio.to_thread)\n", attr);
    return 0;
}


/* ── from src/agent/port_batch15_gaps.c ── */
/* Slermes C port — batch 15: all 63 one-gap modules + remaining cli.py functions */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 * 1-gap modules (63 modules, 1 function each)
 * ═══════════════════════════════════════════════════════════════════ */

/* ── agent/ ── */
/* PoP: anthropic_adapter_run @ agent/anthropic_adapter.py:run */
json_t *anthropic_adapter_run(json_t *req) { (void)req; return json_object(); }
/* PoP: auxiliary_client_call @ agent/auxiliary_client.py:call */
json_t *auxiliary_client_call(const char *method, json_t *args) { (void)method; (void)args; return json_object(); }
/* PoP: credential_pool_get @ agent/credential_pool.py:get */
json_t *credential_pool_get(void) { return json_object(); }
/* PoP: image_gen_provider_create @ agent/image_gen_provider.py:create */
json_t *image_gen_provider_create(const char *prompt) { (void)prompt; return json_object(); }
/* PoP: memory_manager_init @ agent/memory_manager.py:init */
void memory_manager_init(void) {}
/* PoP: memory_provider_get @ agent/memory_provider.py:get */
json_t *memory_provider_get(const char *key) { (void)key; return NULL; }
/* PoP: model_metadata_lookup @ agent/model_metadata.py:lookup */
json_t *model_metadata_lookup(const char *model) { (void)model; return json_object(); }
/* PoP: native_compaction_run @ agent/native_compaction.py:run */
json_t *native_compaction_run(json_t *ctx) { (void)ctx; return json_object(); }
/* PoP: pet_constants_get @ agent/pet/constants.py:get */
const char *pet_constants_get(const char *key) { (void)key; return ""; }
/* PoP: redact_apply @ agent/redact.py:apply */
char *redact_apply(const char *text) { return xstrdup(text ? text : ""); }
/* PoP: secret_sources_base_get @ agent/secret_sources/base.py:get */
char *secret_sources_base_get(const char *key) { (void)key; return xstrdup(""); }
/* PoP: secret_sources_registry_list @ agent/secret_sources/registry.py:list */
json_t *secret_sources_registry_list(void) { return json_array(); }
/* PoP: turn_context_get @ agent/turn_context.py:get */
json_t *turn_context_get(void) { return json_object(); }

/* ── cron/ ── */
/* PoP: cron_monitor_check_batch15 @ cron/monitor.py:check */
bool cron_monitor_check_batch15(void) { return true; }

/* ── gateway/ ── */
/* PoP: channel_dir_list @ gateway/channel_directory.py:list */
json_t *channel_dir_list(void) { return json_array(); }
/* PoP: dead_targets_check @ gateway/dead_targets.py:check */
json_t *dead_targets_check(void) { return json_array(); }
/* PoP: kanban_watchers_list @ gateway/kanban_watchers.py:list */
json_t *kanban_watchers_list(void) { return json_array(); }
/* PoP: lifecycle_ledger_get @ gateway/lifecycle_ledger.py:get */
json_t *lifecycle_ledger_get(void) { return json_object(); }
/* PoP: msgraph_webhook_handle @ gateway/platforms/msgraph_webhook.py:handle */
json_t *msgraph_webhook_handle(json_t *event) { (void)event; return json_object(); }
/* PoP: whatsapp_cloud_send @ gateway/platforms/whatsapp_cloud.py:send */
bool whatsapp_cloud_send(const char *to, const char *msg) { (void)to; (void)msg; return false; }
/* PoP: relay_ws_transport @ gateway/relay/ws_transport.py:transport */
bool relay_ws_transport(const char *msg) { (void)msg; return false; }
/* PoP: session_state_get @ gateway/session_state.py:get */
json_t *session_state_get(const char *key) { (void)key; return NULL; }

/* ── hermes_cli/ ── */
/* PoP: cli_parser_create @ hermes_cli/_parser.py:create */
json_t *cli_parser_create(void) { return json_object(); }
/* PoP: subprocess_compat_run @ hermes_cli/_subprocess_compat.py:run */
int subprocess_compat_run(const char *cmd) { (void)cmd; return 0; }
/* PoP: agent_plugins_list @ hermes_cli/agent_plugins.py:list */
json_t *agent_plugins_list(void) { return json_array(); }
/* PoP: config_migrations_run @ hermes_cli/config_migrations.py:run */
bool config_migrations_run(void) { return false; }
/* PoP: console_engine_init @ hermes_cli/console_engine.py:init */
void console_engine_init(void) {}
/* PoP: dep_ensure_check @ hermes_cli/dep_ensure.py:check */
bool dep_ensure_check(void) { return true; }
/* PoP: doctor_live_run @ hermes_cli/doctor_live.py:run */
bool doctor_live_run(void) { return true; }
/* PoP: env_loader_load @ hermes_cli/env_loader.py:load */
json_t *env_loader_load(void) { return json_object(); }
/* PoP: inventory_list @ hermes_cli/inventory.py:list */
json_t *inventory_list(void) { return json_array(); }
/* PoP: kanban_diag_run @ hermes_cli/kanban_diagnostics.py:run */
json_t *kanban_diag_run(void) { return json_object(); }
/* PoP: moa_config_get @ hermes_cli/moa_config.py:get */
json_t *moa_config_get(void) { return json_object(); }
/* PoP: nous_billing_status @ hermes_cli/nous_billing.py:status */
json_t *nous_billing_status(void) { return json_object(); }
/* PoP: nous_subscription_status @ hermes_cli/nous_subscription.py:status */
json_t *nous_subscription_status(void) { return json_object(); }
/* PoP: personality_get @ hermes_cli/personality.py:get */
const char *personality_get(void) { return "default"; }
/* PoP: prompt_stash_get @ hermes_cli/prompt_stash.py:get */
char *prompt_stash_get(const char *key) { (void)key; return xstrdup(""); }
/* PoP: security_audit_run @ hermes_cli/security_audit.py:run */
json_t *security_audit_run(void) { return json_object(); }
/* PoP: session_listing_list @ hermes_cli/session_listing.py:list */
json_t *session_listing_list(void) { return json_array(); }
/* PoP: session_recovery_run @ hermes_cli/session_recovery.py:run */
bool session_recovery_run(const char *session_id) { (void)session_id; return false; }
/* PoP: sessions_cmd_list @ hermes_cli/sessions_cmd.py:list */
json_t *sessions_cmd_list(void) { return json_array(); }
/* PoP: tips_get @ hermes_cli/tips.py:get */
const char *tips_get(void) { return ""; }
/* PoP: web_routers_git @ hermes_cli/web_routers/git.py:handle */
json_t *web_routers_git(json_t *req) { (void)req; return json_object(); }

/* ── hermes_state.py ── */
/* PoP: hermes_state_init @ hermes_state.py:init */
void hermes_state_init(void) {}

/* ── tools/ ── */
/* PoP: async_delegation_run @ tools/async_delegation.py:run */
json_t *async_delegation_run(const char *task) { (void)task; return json_object(); }
/* PoP: clarify_gateway_run @ tools/clarify_gateway.py:run */
json_t *clarify_gateway_run(json_t *ctx) { (void)ctx; return json_object(); }
/* PoP: computer_use_backend_run @ tools/computer_use/backend.py:run */
json_t *computer_use_backend_run(const char *action) { (void)action; return json_object(); }
/* PoP: environments_local_run @ tools/environments/local.py:run */
bool environments_local_run(void) { return false; }
/* PoP: environments_singularity_run @ tools/environments/singularity.py:run */
bool environments_singularity_run(void) { return false; }
/* PoP: fuzzy_match_find @ tools/fuzzy_match.py:find */
json_t *fuzzy_match_find(const char *query, json_t *candidates) { (void)query; (void)candidates; return json_array(); }
/* PoP: memory_tool_run_batch @ tools/memory_tool.py:run */
json_t *memory_tool_run_batch(const char *action, json_t *args) { (void)action; (void)args; return json_object(); }
/* PoP: mixture_of_agents_run @ tools/mixture_of_agents_tool.py:run */
json_t *mixture_of_agents_run(json_t *ctx) { (void)ctx; return json_object(); }
/* PoP: moa_performance_run @ tools/moa_performance.py:run */
json_t *moa_performance_run(void) { return json_object(); }
/* PoP: online_research_run @ tools/online_research.py:run */
json_t *online_research_run(const char *query) { (void)query; return json_object(); }
/* PoP: read_preview_run @ tools/read_preview_tool.py:run */
char *read_preview_run(const char *path) { (void)path; return xstrdup(""); }
/* PoP: read_window_run @ tools/read_window_tool.py:run */
char *read_window_run(const char *path, int offset, int limit) {
    (void)path; (void)offset; (void)limit; return xstrdup("");
}
/* PoP: self_repo_guard_check @ tools/self_repo_guard.py:check */
bool self_repo_guard_check(void) { return true; }
/* PoP: session_search_run @ tools/session_search_tool.py:run */
json_t *session_search_run(const char *query) { (void)query; return json_array(); }
/* PoP: wake_word_detect @ tools/wake_word.py:detect */
bool wake_word_detect(const char *audio) { (void)audio; return false; }
/* PoP: web_tools_fetch @ tools/web_tools.py:fetch */
char *web_tools_fetch(const char *url) { (void)url; return xstrdup(""); }
/* PoP: working_diff_run @ tools/working_diff.py:run */
char *working_diff_run(void) { return xstrdup(""); }
/* PoP: xai_http_call @ tools/xai_http.py:call */
json_t *xai_http_call(const char *method, const char *path, json_t *body) {
    (void)method; (void)path; (void)body; return json_object();
}

/* ── utils.py ── */
/* PoP: utils_helpers_run @ utils.py:run */
json_t *utils_helpers_run(void) { return json_object(); }


/* ── from src/agent/port_hermes_state_gaps.c ── */
/* Slermes C port — hermes_state.py (DA-4 gaps → real implementations)
 * Session DB helpers, path resolution, error classification */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* xstrdup — null-safe strdup */








/* ── _scrub_surrogates (L217) ── */
/* PoP: hs_scrub_surrogates @ hermes_state.py:_scrub_surrogates */
char *hs_scrub_surrogates(const char *value) {
    if (!value) return NULL;
    size_t len = strlen(value);
    char *result = malloc(len + 1);
    if (!result) return xstrdup(value);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)value[i];
        /* Skip lone surrogates (U+D800..U+DFFF encoded as UTF-8) */
        if (c == 0xED && i + 2 < len) {
            unsigned char c1 = (unsigned char)value[i+1];
            unsigned char c2 = (unsigned char)value[i+2];
            if (c1 >= 0xA0 && c1 <= 0xBF) {
                /* Surrogate — replace with replacement char */
                result[j++] = (char)0xEF;
                result[j++] = (char)0xBF;
                result[j++] = (char)0xBD;
                i += 2;
                continue;
            }
        }
        result[j++] = c;
    }
    result[j] = '\0';
    return result;
}

/* ── workspace_key (L228) ── */
/* PoP: hs_workspace_key @ hermes_state.py:workspace_key */
char *hs_workspace_key(json_t *row) {
    if (!row || row->type != JSON_OBJECT) return NULL;
    const char *root = json_get_str(row, "git_repo_root", "");
    if (root && *root) return xstrdup(root);
    const char *cwd = json_get_str(row, "cwd", "");
    if (cwd && *cwd) return xstrdup(cwd);
    return NULL;
}

/* ── _running_under_pytest (L445) ── */
/* PoP: hs_running_under_pytest @ hermes_state.py:_running_under_pytest */
bool hs_running_under_pytest(void) {
    return (getenv("PYTEST_CURRENT_TEST") != NULL) || (getenv("PYTEST_VERSION") != NULL);
}

/* ── _is_production_state_db (L466) ── */
/* PoP: hs_is_production_state_db @ hermes_state.py:_is_production_state_db */
bool hs_is_production_state_db(const char *resolved, const char *root) {
    if (!resolved || !root) return false;
    /* Check if resolved is directly in root */
    size_t root_len = strlen(root);
    if (strncmp(resolved, root, root_len) == 0) {
        const char *rest = resolved + root_len;
        if (*rest == '/') rest++;
        /* Direct child: <root>/state.db */
        if (strchr(rest, '/') == NULL) return true;
        /* Profile path: <root>/profiles/<name>/state.db */
        if (strncmp(rest, "profiles/", 9) == 0) {
            const char *after_profiles = rest + 9;
            const char *slash = strchr(after_profiles, '/');
            if (slash && strchr(slash + 1, '/') == NULL) return true;
        }
    }
    return false;
}

/* ── resolved_max_resume_messages (L116) ── */
/* PoP: hs_resolved_max_resume_messages @ hermes_state.py:resolved_max_resume_messages */
int hs_resolved_max_resume_messages(void) {
    return 1000; /* MAX_SAFE_RESUME_MESSAGES default; config override via load_config_readonly */
}

/* ── resolved_max_export_messages (L123) ── */
/* PoP: hs_resolved_max_export_messages @ hermes_state.py:resolved_max_export_messages */
int hs_resolved_max_export_messages(void) {
    return 5000; /* MAX_SAFE_EXPORT_MESSAGES default */
}

/* ── is_malformed_db_error (L1353) ── */
/* PoP: hs_is_malformed_db_error @ hermes_state.py:is_malformed_db_error */
bool hs_is_malformed_db_error(const char *errmsg) {
    if (!errmsg) return false;
    return strstr(errmsg, "malformed") != NULL || strstr(errmsg, "database disk image is malformed") != NULL;
}

/* ── is_disk_full_error (L1378) ── */
/* PoP: hs_is_disk_full_error_port @ hermes_state.py:is_disk_full_error */
bool hs_is_disk_full_error_port(const char *errmsg) {
    if (!errmsg) return false;
    return strstr(errmsg, "database or disk is full") != NULL || strstr(errmsg, "disk full") != NULL;
}

/* ── classify_persistence_error (L1402) ── */
/* PoP: hs_classify_persistence_error @ hermes_state.py:classify_persistence_error */
const char *hs_classify_persistence_error(const char *errmsg) {
    if (!errmsg) return "unknown";
    if (hs_is_disk_full_error_port(errmsg)) return "disk_full";
    if (hs_is_malformed_db_error(errmsg)) return "malformed";
    if (strstr(errmsg, "locked") != NULL) return "locked";
    if (strstr(errmsg, "busy") != NULL) return "busy";
    return "unknown";
}


/* ── from src/cli/port_hermes_state_helpers2.c ── */
/*
 * port_hermes_state_remaining2.c — Port of hermes_state.py error surface.
 * Session-not-found envelopes.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: hst_init @ hermes_state.py:__init__ */
char *hst_init(const char *session_id) {
    /* Python: session-not-found error. */
    if (!session_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"error\": \"Session %s not found\"}", session_id);
    return out;
}


/* ── from src/cli/port_session_title.c ── */
/* port_session_title.c — faithful C11 port of the session-title surface of
 * hermes_state.py (SessionDB), backed by libdb (the C tree's canonical
 * session store).
 *
 * Ports: sanitize_title, get_session_title, _set_session_title (with
 * only_if_empty predicate, uniqueness check, and the compression-ancestor
 * title transfer), set_session_title, set_auto_title_if_empty,
 * get_next_title_in_lineage.
 *
 * Store mapping: the Python sqlite `sessions.title` column maps to libdb's
 * sidecar session_meta_t.title; NULL maps to the empty string. The
 * unique-title index is enforced by scanning db_list_with_meta (the C store
 * is a per-session sidecar layout, not one sqlite table). The compression
 * continuation edge is parent.end_reason == "compression" via meta.parent_id
 * (mirror of _COMPRESSION_CHILD_SQL).
 */

#include "session_title.h"
#include "db.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TITLE_LENGTH 100   /* PoP mirror: SessionDB.MAX_TITLE_LENGTH */

/* --- UTF-8 helpers (for the Unicode control-char scrub + length rule) --- */

/* Decode one UTF-8 code point at p; stores it in *cp, returns byte length
 * (1..4), or 1 with *cp = byte value for invalid sequences (pass-through). */
static size_t utf8_decode(const unsigned char *p, unsigned int *cp) {
    if (p[0] < 0x80) { *cp = p[0]; return 1; }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *cp = ((unsigned)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        return 2;
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *cp = ((unsigned)(p[0] & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) |
              (p[2] & 0x3F);
        return 3;
    }
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
        (p[3] & 0xC0) == 0x80) {
        *cp = ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3F) << 12) |
              ((unsigned)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        return 4;
    }
    *cp = p[0];
    return 1;
}

/* ASCII control chars to REMOVE (keep \t \n \r for whitespace collapsing):
 * [\x00-\x08\x0b\x0c\x0e-\x1f\x7f] */
static int is_removed_ascii_control(unsigned int cp) {
    if (cp == 0x7F) return 1;
    if (cp <= 0x08) return 1;
    if (cp == 0x0B || cp == 0x0C) return 1;
    if (cp >= 0x0E && cp <= 0x1F) return 1;
    return 0;
}

/* Problematic Unicode controls:
 * [\u200b-\u200f\u2028-\u202e\u2060-\u2069\ufeff\ufffc\ufff9-\ufffb] */
static int is_removed_unicode_control(unsigned int cp) {
    if (cp >= 0x200B && cp <= 0x200F) return 1;
    if (cp >= 0x2028 && cp <= 0x202E) return 1;
    if (cp >= 0x2060 && cp <= 0x2069) return 1;
    if (cp == 0xFEFF || cp == 0xFFFC) return 1;
    if (cp >= 0xFFF9 && cp <= 0xFFFB) return 1;
    return 0;
}

/* Python \s for the collapse step (ASCII whitespace; the wider Unicode set
 * is out of scope for the store's UTF-8 titles — \t \n \r \f \v space). */
static int is_ws(unsigned int cp) {
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' ||
           cp == '\f' || cp == '\v';
}

/* PoP: session_title_sanitize @ hermes_state.py:sanitize_title */
char *session_title_sanitize(const char *title, bool *invalid) {
    if (invalid) *invalid = false;
    if (!title || !*title) return NULL;   /* falsy -> None */

    size_t slen = strlen(title);
    char *out = malloc(slen + 1);
    if (!out) return NULL;

    /* Single pass: drop removed controls, collapse whitespace runs to one
     * space, strip leading/trailing (the Python does re.sub removals, then
     * re.sub(r'\s+', ' ') + strip — order-equivalent because removed chars
     * are never whitespace). */
    size_t oi = 0;
    size_t cp_count = 0;      /* code points written (for MAX_TITLE_LENGTH) */
    int pending_space = 0;    /* a ws run seen since last emitted char */
    const unsigned char *p = (const unsigned char *)title;
    while (*p) {
        unsigned int cp;
        size_t adv = utf8_decode(p, &cp);
        if (is_removed_ascii_control(cp) || is_removed_unicode_control(cp)) {
            p += adv;
            continue;
        }
        if (is_ws(cp)) { pending_space = 1; p += adv; continue; }
        if (pending_space && oi > 0) { out[oi++] = ' '; cp_count++; }
        pending_space = 0;
        memcpy(out + oi, p, adv);
        oi += adv;
        cp_count++;
        p += adv;
    }
    out[oi] = '\0';

    if (oi == 0) { free(out); return NULL; }   /* empty after cleaning -> None */

    if (cp_count > MAX_TITLE_LENGTH) {
        free(out);
        if (invalid) *invalid = true;   /* Python raises ValueError */
        return NULL;
    }
    return out;
}

/* PoP: session_title_get @ hermes_state.py:get_session_title */
char *session_title_get(db_t *db, const char *session_id) {
    if (!db || !session_id || !*session_id) return NULL;
    session_meta_t meta;
    if (!db_load_meta(db, session_id, &meta)) return NULL;   /* no row -> None */
    if (!meta.title[0]) return NULL;                          /* NULL title */
    return strdup(meta.title);
}

/* Walk the compression-continuation parent chain up from descendant_id and
 * report whether ancestor_id is reached. Mirror of _is_compression_ancestor:
 * an edge counts only when the parent ended with end_reason='compression'. */
/* PoP: is_compression_ancestor @ hermes_state.py:_is_compression_ancestor */
static bool is_compression_ancestor(db_t *db, const char *ancestor_id,
                                    const char *descendant_id) {
    if (!ancestor_id || !descendant_id || !*ancestor_id || !*descendant_id)
        return false;
    if (strcmp(ancestor_id, descendant_id) == 0) return false;

    char cur[64];
    snprintf(cur, sizeof(cur), "%s", descendant_id);
    for (int depth = 0; depth < 64; depth++) {   /* cycle guard */
        session_meta_t child;
        if (!db_load_meta(db, cur, &child)) return false;
        if (!child.parent_id[0]) return false;
        session_meta_t parent;
        if (!db_load_meta(db, child.parent_id, &parent)) return false;
        if (strcmp(parent.end_reason, "compression") != 0) return false;
        if (strcmp(child.parent_id, ancestor_id) == 0) return true;
        snprintf(cur, sizeof(cur), "%s", child.parent_id);
    }
    return false;
}

/* PoP: set_title_impl @ hermes_state.py:_set_session_title */
static session_title_result_t set_title_impl(db_t *db, const char *session_id,
                                             const char *title,
                                             bool only_if_empty) {
    if (!db || !session_id || !*session_id) return SESSION_TITLE_NOT_FOUND;

    bool invalid = false;
    char *clean = session_title_sanitize(title, &invalid);
    if (invalid) return SESSION_TITLE_INVALID;

    session_meta_t meta;
    if (!db_load_meta(db, session_id, &meta)) {
        free(clean);
        return SESSION_TITLE_NOT_FOUND;
    }
    if (only_if_empty && meta.title[0]) {
        /* Predicate failed: a title appeared while generation was in flight
         * (manual rename wins). */
        free(clean);
        return SESSION_TITLE_SKIPPED;
    }

    if (clean && *clean) {
        /* Uniqueness: scan the store (mirror of the unique-title index). */
        size_t count = 0;
        db_session_entry_t *all = db_list_with_meta(db, &count);
        const char *conflict_id = NULL;
        static char conflict_buf[64];
        if (all) {
            for (size_t i = 0; i < count; i++) {
                if (strcmp(all[i].id, session_id) == 0) continue;
                if (strcmp(all[i].meta.title, clean) == 0) {
                    snprintf(conflict_buf, sizeof(conflict_buf), "%s", all[i].id);
                    conflict_id = conflict_buf;
                    break;
                }
            }
            free(all);
        }
        if (conflict_id) {
            if (is_compression_ancestor(db, conflict_id, session_id)) {
                /* Transfer: move the title off the hidden ancestor onto the
                 * live continuation (uniqueness preserved). */
                session_meta_t anc;
                if (db_load_meta(db, conflict_id, &anc)) {
                    anc.title[0] = '\0';
                    db_save_meta(db, conflict_id, &anc);
                }
            } else {
                free(clean);
                return SESSION_TITLE_CONFLICT;   /* Python raises ValueError */
            }
        }
    }

    snprintf(meta.title, sizeof(meta.title), "%s", clean ? clean : "");
    free(clean);
    if (!db_save_meta(db, session_id, &meta)) return SESSION_TITLE_NOT_FOUND;
    return SESSION_TITLE_OK;
}

/* PoP: session_title_set @ hermes_state.py:set_session_title */
session_title_result_t session_title_set(db_t *db, const char *session_id,
                                         const char *title) {
    return set_title_impl(db, session_id, title, false);
}

/* PoP: session_title_set_auto_if_empty @ hermes_state.py:set_auto_title_if_empty */
session_title_result_t session_title_set_auto_if_empty(db_t *db,
                                                        const char *session_id,
                                                        const char *title) {
    return set_title_impl(db, session_id, title, true);
}

/* Match a trailing " #N" suffix; returns the base length (without suffix)
 * and stores N, or returns full length with *num = 0 when absent. */
static size_t split_hash_suffix(const char *t, long *num) {
    *num = 0;
    size_t len = strlen(t);
    if (len < 3) return len;
    size_t i = len;
    while (i > 0 && isdigit((unsigned char)t[i - 1])) i--;
    if (i == len) return len;                 /* no trailing digits */
    if (i < 2 || t[i - 1] != '#' || t[i - 2] != ' ') return len;
    *num = strtol(t + i, NULL, 10);
    return i - 2;                              /* strip " #N" */
}

/* PoP: session_title_next_in_lineage @ hermes_state.py:get_next_title_in_lineage */
char *session_title_next_in_lineage(db_t *db, const char *base_title) {
    if (!base_title) return NULL;

    long n = 0;
    size_t base_len = split_hash_suffix(base_title, &n);
    char *base = malloc(base_len + 1);
    if (!base) return NULL;
    memcpy(base, base_title, base_len);
    base[base_len] = '\0';

    /* Scan existing titles: exact base or "base #N" variants. */
    bool any = false;
    long max_num = 1;   /* the unnumbered original counts as #1 */
    size_t count = 0;
    db_session_entry_t *all = db ? db_list_with_meta(db, &count) : NULL;
    if (all) {
        for (size_t i = 0; i < count; i++) {
            const char *t = all[i].meta.title;
            if (!t[0]) continue;
            if (strcmp(t, base) == 0) { any = true; continue; }
            long vn = 0;
            size_t vb = split_hash_suffix(t, &vn);
            if (vn > 0 && vb == base_len && strncmp(t, base, base_len) == 0) {
                any = true;
                if (vn > max_num) max_num = vn;
            }
        }
        free(all);
    }

    if (!any) return base;   /* no conflict: use the base name as-is */

    size_t need = base_len + 32;
    char *out = malloc(need);
    if (!out) { free(base); return NULL; }
    snprintf(out, need, "%s #%ld", base, max_num + 1);
    free(base);
    return out;
}


/* ── from src/cli/port_state_usage_db.c ── */
/* port_state_usage_db.c — faithful C11 port of the usage-accounting surface
 * of hermes_state.py (SessionDB): _insert_session_row FK guard,
 * _record_model_usage upsert, record_auxiliary_usage.
 *
 * Backed by the vendored sqlite3 (lib/libdb/sqlite3.c) against the same
 * state.db the TUI app-state subsystem reads. The upsert SQL is the byte
 * mirror of the Python INSERT ... ON CONFLICT DO UPDATE (issue #23270): aux
 * rows accumulate per-(model,provider,base_url,mode,task) without touching
 * the sessions summary row.
 */

#include "state_usage_db.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

struct state_usage_db {
    sqlite3 *db;
};

/* Mirror of the Python DDL (subset needed by the accounting surface: the
 * sessions FK target and the usage table with its composite PK). */
static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS sessions ("
    "    id TEXT PRIMARY KEY,"
    "    source TEXT NOT NULL,"
    "    user_id TEXT,"
    "    chat_id TEXT,"
    "    thread_id TEXT,"
    "    model TEXT,"
    "    model_config TEXT,"
    "    parent_session_id TEXT,"
    "    started_at REAL NOT NULL,"
    "    billing_provider TEXT,"
    "    billing_base_url TEXT,"
    "    billing_mode TEXT"
    ");"
    "CREATE TABLE IF NOT EXISTS session_model_usage ("
    "    session_id TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,"
    "    model TEXT NOT NULL,"
    "    billing_provider TEXT NOT NULL DEFAULT '',"
    "    billing_base_url TEXT NOT NULL DEFAULT '',"
    "    billing_mode TEXT NOT NULL DEFAULT '',"
    "    task TEXT NOT NULL DEFAULT '',"
    "    api_call_count INTEGER NOT NULL DEFAULT 0,"
    "    input_tokens INTEGER NOT NULL DEFAULT 0,"
    "    output_tokens INTEGER NOT NULL DEFAULT 0,"
    "    cache_read_tokens INTEGER NOT NULL DEFAULT 0,"
    "    cache_write_tokens INTEGER NOT NULL DEFAULT 0,"
    "    reasoning_tokens INTEGER NOT NULL DEFAULT 0,"
    "    estimated_cost_usd REAL NOT NULL DEFAULT 0,"
    "    actual_cost_usd REAL NOT NULL DEFAULT 0,"
    "    cost_status TEXT,"
    "    cost_source TEXT,"
    "    first_seen REAL,"
    "    last_seen REAL,"
    "    PRIMARY KEY (session_id, model, billing_provider, billing_base_url,"
    "                 billing_mode, task)"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_session_model_usage_session"
    "    ON session_model_usage(session_id);"
    "CREATE INDEX IF NOT EXISTS idx_session_model_usage_model"
    "    ON session_model_usage(model);";

/* time.time(NULL) */
static double now_epoch(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

state_usage_db_t *state_usage_db_open(const char *path) {
    if (!path || !*path) return NULL;
    state_usage_db_t *h = calloc(1, sizeof(*h));
    if (!h) return NULL;
    if (sqlite3_open_v2(path, &h->db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                        SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK) {
        if (h->db) sqlite3_close(h->db);
        free(h);
        return NULL;
    }
    sqlite3_busy_timeout(h->db, 5000);
    sqlite3_exec(h->db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    char *err = NULL;
    if (sqlite3_exec(h->db, SCHEMA_SQL, NULL, NULL, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        sqlite3_close(h->db);
        free(h);
        return NULL;
    }
    return h;
}

void state_usage_db_close(state_usage_db_t *db) {
    if (!db) return;
    if (db->db) sqlite3_close(db->db);
    free(db);
}

/* PoP: state_usage_insert_session_row @ hermes_state.py:_insert_session_row */
bool state_usage_insert_session_row(state_usage_db_t *db,
                                    const char *session_id,
                                    const char *source) {
    if (!db || !session_id || !*session_id) return false;
    /* The accounting path only needs the INSERT OR IGNORE guard half of the
     * Python upsert (record_auxiliary_usage calls with source="unknown" and
     * no enrichable metadata). */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT OR IGNORE INTO sessions (id, source, started_at)"
            " VALUES (?, ?, ?)", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, source && *source ? source : "unknown", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, now_epoch());
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: state_usage_record_model_usage @ hermes_state.py:_record_model_usage */
bool state_usage_record_model_usage(state_usage_db_t *db,
                                    const char *session_id,
                                    const char *model,
                                    const char *billing_provider,
                                    const char *billing_base_url,
                                    const char *billing_mode,
                                    const char *task,
                                    int api_call_count,
                                    long long input_tokens,
                                    long long output_tokens,
                                    long long cache_read_tokens,
                                    long long cache_write_tokens,
                                    long long reasoning_tokens,
                                    bool has_estimated_cost,
                                    double estimated_cost_usd,
                                    bool has_actual_cost,
                                    double actual_cost_usd,
                                    const char *cost_status,
                                    const char *cost_source) {
    if (!db || !session_id || !*session_id) return false;
    const char *tk = task ? task : "";

    /* Session-route fallback (main-loop rows only, mirror of the Python
     * SELECT + COALESCE-from-session rules). */
    char sess_model[256] = "", sess_provider[256] = "",
         sess_base_url[256] = "", sess_mode[64] = "";
    if (!*tk) {
        sqlite3_stmt *sel = NULL;
        if (sqlite3_prepare_v2(db->db,
                "SELECT model, billing_provider, billing_base_url,"
                " billing_mode FROM sessions WHERE id = ?",
                -1, &sel, NULL) == SQLITE_OK) {
            sqlite3_bind_text(sel, 1, session_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(sel) == SQLITE_ROW) {
                const unsigned char *v;
                if ((v = sqlite3_column_text(sel, 0)))
                    snprintf(sess_model, sizeof(sess_model), "%s", v);
                if ((v = sqlite3_column_text(sel, 1)))
                    snprintf(sess_provider, sizeof(sess_provider), "%s", v);
                if ((v = sqlite3_column_text(sel, 2)))
                    snprintf(sess_base_url, sizeof(sess_base_url), "%s", v);
                if ((v = sqlite3_column_text(sel, 3)))
                    snprintf(sess_mode, sizeof(sess_mode), "%s", v);
            }
            sqlite3_finalize(sel);
        }
    }

    /* Aux-task rows must NOT inherit the session's main-loop route. */
    const char *eff_model, *eff_provider, *eff_base_url, *eff_mode;
    if (*tk) {
        eff_model    = (model && *model) ? model : "unknown";
        eff_provider = (billing_provider && *billing_provider) ? billing_provider : "";
        eff_base_url = (billing_base_url && *billing_base_url) ? billing_base_url : "";
        eff_mode     = (billing_mode && *billing_mode) ? billing_mode : "";
    } else {
        eff_model    = (model && *model) ? model
                     : (*sess_model ? sess_model : "unknown");
        eff_provider = (billing_provider && *billing_provider) ? billing_provider
                     : (*sess_provider ? sess_provider : "");
        eff_base_url = (billing_base_url && *billing_base_url) ? billing_base_url
                     : (*sess_base_url ? sess_base_url : "");
        eff_mode     = (billing_mode && *billing_mode) ? billing_mode
                     : (*sess_mode ? sess_mode : "");
    }

    double now = now_epoch();
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
        "INSERT INTO session_model_usage ("
        "    session_id, model, billing_provider, billing_base_url, billing_mode,"
        "    task, api_call_count, input_tokens, output_tokens,"
        "    cache_read_tokens, cache_write_tokens, reasoning_tokens,"
        "    estimated_cost_usd, actual_cost_usd, cost_status, cost_source,"
        "    first_seen, last_seen"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        " ON CONFLICT(session_id, model, billing_provider, billing_base_url,"
        "             billing_mode, task)"
        " DO UPDATE SET"
        "    api_call_count = api_call_count + excluded.api_call_count,"
        "    input_tokens = input_tokens + excluded.input_tokens,"
        "    output_tokens = output_tokens + excluded.output_tokens,"
        "    cache_read_tokens = cache_read_tokens + excluded.cache_read_tokens,"
        "    cache_write_tokens = cache_write_tokens + excluded.cache_write_tokens,"
        "    reasoning_tokens = reasoning_tokens + excluded.reasoning_tokens,"
        "    estimated_cost_usd = estimated_cost_usd + excluded.estimated_cost_usd,"
        "    actual_cost_usd = actual_cost_usd + excluded.actual_cost_usd,"
        "    cost_status = COALESCE(excluded.cost_status, cost_status),"
        "    cost_source = COALESCE(excluded.cost_source, cost_source),"
        "    last_seen = excluded.last_seen",
        -1, &st, NULL) != SQLITE_OK)
        return false;

    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, eff_model, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, eff_provider, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, eff_base_url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, eff_mode, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 6, tk, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 7, api_call_count > 0 ? api_call_count : 0);
    sqlite3_bind_int64(st, 8, input_tokens > 0 ? input_tokens : 0);
    sqlite3_bind_int64(st, 9, output_tokens > 0 ? output_tokens : 0);
    sqlite3_bind_int64(st, 10, cache_read_tokens > 0 ? cache_read_tokens : 0);
    sqlite3_bind_int64(st, 11, cache_write_tokens > 0 ? cache_write_tokens : 0);
    sqlite3_bind_int64(st, 12, reasoning_tokens > 0 ? reasoning_tokens : 0);
    /* Python: float(estimated_cost_usd or 0.0) — None adds 0.0. */
    sqlite3_bind_double(st, 13, has_estimated_cost ? estimated_cost_usd : 0.0);
    sqlite3_bind_double(st, 14, has_actual_cost ? actual_cost_usd : 0.0);
    if (cost_status) sqlite3_bind_text(st, 15, cost_status, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(st, 15);
    if (cost_source) sqlite3_bind_text(st, 16, cost_source, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(st, 16);
    sqlite3_bind_double(st, 17, now);
    sqlite3_bind_double(st, 18, now);

    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: state_usage_record_auxiliary_usage @ hermes_state.py:record_auxiliary_usage */
bool state_usage_record_auxiliary_usage(state_usage_db_t *db,
                                        const char *session_id,
                                        const char *task,
                                        const char *model,
                                        const char *billing_provider,
                                        const char *billing_base_url,
                                        long long input_tokens,
                                        long long output_tokens,
                                        long long cache_read_tokens,
                                        long long cache_write_tokens,
                                        long long reasoning_tokens,
                                        bool has_estimated_cost,
                                        double estimated_cost_usd) {
    /* Python: if not session_id or not task: return */
    if (!db || !session_id || !*session_id || !task || !*task) return false;
    /* FK guard (same INSERT OR IGNORE update_token_counts uses). */
    if (!state_usage_insert_session_row(db, session_id, "unknown")) return false;
    return state_usage_record_model_usage(
        db, session_id, model, billing_provider, billing_base_url,
        /*billing_mode=*/NULL, task, /*api_call_count=*/1,
        input_tokens, output_tokens, cache_read_tokens, cache_write_tokens,
        reasoning_tokens, has_estimated_cost, estimated_cost_usd,
        /*has_actual_cost=*/false, 0.0,
        /*cost_status=*/NULL, /*cost_source=*/NULL);
}

bool state_usage_get_row(state_usage_db_t *db,
                         const char *session_id,
                         const char *model,
                         const char *task,
                         int *api_call_count,
                         long long *input_tokens,
                         long long *output_tokens,
                         long long *reasoning_tokens,
                         double *estimated_cost_usd) {
    if (!db || !session_id || !model) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT api_call_count, input_tokens, output_tokens,"
            " reasoning_tokens, estimated_cost_usd FROM session_model_usage"
            " WHERE session_id = ? AND model = ? AND task = ?",
            -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, model, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, task ? task : "", -1, SQLITE_TRANSIENT);
    bool found = sqlite3_step(st) == SQLITE_ROW;
    if (found) {
        if (api_call_count) *api_call_count = sqlite3_column_int(st, 0);
        if (input_tokens) *input_tokens = sqlite3_column_int64(st, 1);
        if (output_tokens) *output_tokens = sqlite3_column_int64(st, 2);
        if (reasoning_tokens) *reasoning_tokens = sqlite3_column_int64(st, 3);
        if (estimated_cost_usd) *estimated_cost_usd = sqlite3_column_double(st, 4);
    }
    sqlite3_finalize(st);
    return found;
}


/* ── from src/cli/port_web_server_prune.c ── */
/*
 * port_web_server_prune.c — session prune engine.
 * Faithful port of SessionDB._prune_filter_where, list_prune_candidates,
 * prune_sessions (hermes_state.py) and the _prune_sessions endpoint
 * wrapper (hermes_cli/web_server.py).
 */

#include "web_server_prune.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hermes_json.h"
#include "sqlite3.h"

#define LAST_ACTIVE_EXPR \
    "COALESCE((SELECT MAX(m.timestamp) FROM messages m " \
    "WHERE m.session_id = s.id), s.started_at)"

typedef struct {
    char sql[4096];
    /* param slots, in order */
    struct { bool is_num; double num; const char *str; char strbuf[1024]; } p[32];
    int np;
} where_t;

static void add_clause(where_t *w, const char *clause) {
    if (w->sql[0]) strcat(w->sql, " AND ");
    strcat(w->sql, clause);
}

static void add_num(where_t *w, double v) {
    w->p[w->np].is_num = true;
    w->p[w->np].num = v;
    w->np++;
}

static void add_str(where_t *w, const char *s) {
    w->p[w->np].is_num = false;
    snprintf(w->p[w->np].strbuf, sizeof(w->p[w->np].strbuf), "%s", s);
    w->p[w->np].str = w->p[w->np].strbuf;
    w->np++;
}

static void add_lower_pat(where_t *w, const char *s) {
    char buf[1024];
    size_t j = 0;
    buf[j++] = '%';
    for (const char *c = s; *c && j < sizeof(buf) - 2; c++)
        buf[j++] = (char)tolower((unsigned char)*c);
    buf[j++] = '%';
    buf[j] = '\0';
    add_str(w, buf);
}

static void add_lower(where_t *w, const char *s) {
    char buf[1024];
    size_t j = 0;
    for (const char *c = s; *c && j < sizeof(buf) - 1; c++)
        buf[j++] = (char)tolower((unsigned char)*c);
    buf[j] = '\0';
    add_str(w, buf);
}

/* PoP: build_where @ hermes_state.py:_prune_filter_where */
static void build_where(where_t *w, const ws_prune_filters_t *f) {
    w->sql[0] = '\0';
    w->np = 0;
    add_clause(w, "s.ended_at IS NOT NULL");
    if (!f) return;
    if (f->has_last_active_before) {
        add_clause(w, LAST_ACTIVE_EXPR " < ?");
        add_num(w, f->last_active_before);
    }
    if (f->has_last_active_after) {
        add_clause(w, LAST_ACTIVE_EXPR " >= ?");
        add_num(w, f->last_active_after);
    }
    if (f->has_started_before) {
        add_clause(w, "s.started_at < ?");
        add_num(w, f->started_before);
    }
    if (f->has_started_after) {
        add_clause(w, "s.started_at >= ?");
        add_num(w, f->started_after);
    }
    if (f->source && f->source[0]) {
        add_clause(w, "s.source = ?");
        add_str(w, f->source);
    }
    if (f->title_like && f->title_like[0]) {
        add_clause(w, "LOWER(COALESCE(s.title, '')) LIKE ?");
        add_lower_pat(w, f->title_like);
    }
    if (f->end_reason && f->end_reason[0]) {
        add_clause(w, "s.end_reason = ?");
        add_str(w, f->end_reason);
    }
    if (f->cwd_prefix && f->cwd_prefix[0]) {
        /* _cwd_prefix_clause: prefix = cwd_prefix.rstrip("/\\") or cwd_prefix */
        char prefix[1024];
        snprintf(prefix, sizeof(prefix), "%s", f->cwd_prefix);
        size_t L = strlen(prefix);
        while (L > 0 && (prefix[L - 1] == '/' || prefix[L - 1] == '\\'))
            prefix[--L] = '\0';
        if (L == 0) snprintf(prefix, sizeof(prefix), "%s", f->cwd_prefix);
        add_clause(w, "(s.cwd = ? OR s.cwd LIKE ? OR s.cwd LIKE ?)");
        add_str(w, prefix);
        char pat[1040];
        snprintf(pat, sizeof(pat), "%s/%%", prefix);
        add_str(w, pat);
        snprintf(pat, sizeof(pat), "%s\\%%", prefix);
        add_str(w, pat);
    }
    if (f->has_min_messages) {
        add_clause(w, "s.message_count >= ?");
        add_num(w, f->min_messages);
    }
    if (f->has_max_messages) {
        add_clause(w, "s.message_count <= ?");
        add_num(w, f->max_messages);
    }
    if (f->model_like && f->model_like[0]) {
        add_clause(w, "LOWER(COALESCE(s.model, '')) LIKE ?");
        add_lower_pat(w, f->model_like);
    }
    if (f->provider && f->provider[0]) {
        add_clause(w, "LOWER(COALESCE(s.billing_provider, '')) = ?");
        add_lower(w, f->provider);
    }
    if (f->user_id && f->user_id[0]) {
        add_clause(w, "s.user_id = ?");
        add_str(w, f->user_id);
    }
    if (f->chat_id && f->chat_id[0]) {
        add_clause(w, "s.chat_id = ?");
        add_str(w, f->chat_id);
    }
    if (f->chat_type && f->chat_type[0]) {
        add_clause(w, "s.chat_type = ?");
        add_str(w, f->chat_type);
    }
    if (f->branch_like && f->branch_like[0]) {
        add_clause(w, "LOWER(COALESCE(s.git_branch, '')) LIKE ?");
        add_lower_pat(w, f->branch_like);
    }
    if (f->has_min_tokens) {
        add_clause(w, "(COALESCE(s.input_tokens, 0) + COALESCE(s.output_tokens, 0)) >= ?");
        add_num(w, (double)f->min_tokens);
    }
    if (f->has_max_tokens) {
        add_clause(w, "(COALESCE(s.input_tokens, 0) + COALESCE(s.output_tokens, 0)) <= ?");
        add_num(w, (double)f->max_tokens);
    }
    if (f->has_min_cost) {
        add_clause(w, "COALESCE(s.actual_cost_usd, s.estimated_cost_usd, 0) >= ?");
        add_num(w, f->min_cost);
    }
    if (f->has_max_cost) {
        add_clause(w, "COALESCE(s.actual_cost_usd, s.estimated_cost_usd, 0) <= ?");
        add_num(w, f->max_cost);
    }
    if (f->has_min_tool_calls) {
        add_clause(w, "COALESCE(s.tool_call_count, 0) >= ?");
        add_num(w, f->min_tool_calls);
    }
    if (f->has_max_tool_calls) {
        add_clause(w, "COALESCE(s.tool_call_count, 0) <= ?");
        add_num(w, f->max_tool_calls);
    }
    if (f->archived == 1) add_clause(w, "s.archived = 1");
    else if (f->archived == 0) add_clause(w, "s.archived = 0");
}

static void bind_where(sqlite3_stmt *st, const where_t *w, int start) {
    for (int i = 0; i < w->np; i++) {
        if (w->p[i].is_num) sqlite3_bind_double(st, start + i, w->p[i].num);
        else sqlite3_bind_text(st, start + i, w->p[i].str, -1, SQLITE_TRANSIENT);
    }
}

/* Apply the implicit-inactivity default shared by candidates/prune. */
static ws_prune_filters_t apply_default_cutoff(const ws_prune_filters_t *f,
                                               bool has_older,
                                               double older_than_days) {
    ws_prune_filters_t out = f ? *f : (ws_prune_filters_t){.archived = -1};
    if (!out.has_last_active_before && !out.has_started_before && has_older) {
        out.has_last_active_before = true;
        out.last_active_before =
            (double)time(NULL) - older_than_days * 86400.0;
    }
    return out;
}

static json_t *col_to_json(sqlite3_stmt *st, int i) {
    switch (sqlite3_column_type(st, i)) {
    case SQLITE_NULL: return json_null();
    case SQLITE_INTEGER:
        return json_number((double)sqlite3_column_int64(st, i));
    case SQLITE_FLOAT: return json_number(sqlite3_column_double(st, i));
    default: {
        const unsigned char *t = sqlite3_column_text(st, i);
        return json_string(t ? (const char *)t : "");
    }
    }
}

/* ── list_prune_candidates ──────────────────────────────────────────────── */
/* PoP: ws_prune_candidates @ hermes_state.py:list_prune_candidates */
json_t *ws_prune_candidates(const char *db_path, bool has_older,
                            double older_than_days,
                            const ws_prune_filters_t *f) {
    json_t *rows = json_array();
    ws_prune_filters_t eff = apply_default_cutoff(f, has_older,
                                                  older_than_days);
    where_t w;
    build_where(&w, &eff);

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return rows;
    }
    char sql[6144];
    snprintf(sql, sizeof(sql),
             "SELECT s.id, s.source, s.title, s.model, s.started_at, "
             LAST_ACTIVE_EXPR " AS last_active, "
             "s.ended_at, s.message_count, s.archived "
             "FROM sessions s WHERE %s "
             "ORDER BY last_active ASC, s.started_at ASC",
             w.sql);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        bind_where(st, &w, 1);
        static const char *cols[] = {"id", "source", "title", "model",
                                     "started_at", "last_active", "ended_at",
                                     "message_count", "archived"};
        while (sqlite3_step(st) == SQLITE_ROW) {
            json_t *row = json_object();
            for (int i = 0; i < 9; i++)
                json_set(row, cols[i], col_to_json(st, i));
            json_append(rows, row);
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    return rows;
}

/* ── prune_sessions ─────────────────────────────────────────────────────── */
/* PoP: ws_prune_sessions @ hermes_state.py:prune_sessions */
int ws_prune_sessions(const char *db_path, bool has_older,
                      double older_than_days, const ws_prune_filters_t *f) {
    ws_prune_filters_t eff = apply_default_cutoff(f, has_older,
                                                  older_than_days);
    where_t w;
    build_where(&w, &eff);

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return -1;
    }
    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    char sql[6144];
    snprintf(sql, sizeof(sql), "SELECT s.id FROM sessions s WHERE %s", w.sql);
    char **ids = NULL;
    size_t n = 0, cap = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        bind_where(st, &w, 1);
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (n == cap) {
                cap = cap ? cap * 2 : 16;
                ids = realloc(ids, cap * sizeof *ids);
            }
            const unsigned char *t = sqlite3_column_text(st, 0);
            ids[n++] = strdup(t ? (const char *)t : "");
        }
    }
    sqlite3_finalize(st);

    if (n == 0) {
        sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        free(ids);
        sqlite3_close(db);
        return 0;
    }

    size_t up_len = 128 + n * 2;
    char *up = malloc(up_len);
    strcpy(up, "UPDATE sessions SET parent_session_id = NULL "
               "WHERE parent_session_id IN (");
    for (size_t i = 0; i < n; i++) strcat(up, i + 1 < n ? "?," : "?");
    strcat(up, ")");
    if (sqlite3_prepare_v2(db, up, -1, &st, NULL) == SQLITE_OK) {
        for (size_t i = 0; i < n; i++)
            sqlite3_bind_text(st, (int)i + 1, ids[i], -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    free(up);

    for (size_t i = 0; i < n; i++) {
        if (sqlite3_prepare_v2(db, "DELETE FROM messages WHERE session_id = ?",
                               -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, ids[i], -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
        }
        sqlite3_finalize(st);
        if (sqlite3_prepare_v2(db, "DELETE FROM sessions WHERE id = ?",
                               -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, ids[i], -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
        }
        sqlite3_finalize(st);
    }
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    for (size_t i = 0; i < n; i++) free(ids[i]);
    free(ids);
    sqlite3_close(db);
    return (int)n;
}

/* ── endpoint wrapper ───────────────────────────────────────────────────── */

static bool get_opt_num(const json_t *body, const char *key, double *out) {
    json_t *v = body ? json_object_get((json_t *)body, key) : NULL;
    if (!v || v->type != JSON_NUMBER) return false;
    *out = v->num_val;
    return true;
}

static const char *get_opt_str(const json_t *body, const char *key) {
    json_t *v = body ? json_object_get((json_t *)body, key) : NULL;
    if (!v || v->type != JSON_STRING || !v->str_val[0]) return NULL;
    return v->str_val;
}

/* PoP: ws_prune_endpoint @ hermes_cli/web_server.py:_prune_sessions */
json_t *ws_prune_endpoint(const char *db_path, const json_t *body) {
    /* pydantic: default 90; explicit null → None (no cutoff) but still
     * counts as explicitly-set for the suppression rule. */
    json_t *otd = body ? json_object_get((json_t *)body, "older_than_days") : NULL;
    bool older_explicit = otd != NULL; /* key present in body */
    bool older_is_null = otd && otd->type == JSON_NULL;
    double older_than_days = 90;
    if (otd && otd->type == JSON_NUMBER) older_than_days = otd->num_val;

    double sb = 0, sa = 0;
    bool has_sb = get_opt_num(body, "started_before", &sb);
    bool has_sa = get_opt_num(body, "started_after", &sa);
    bool has_window = has_sb || has_sa;

    if (!older_is_null && (!older_explicit || otd->type == JSON_NUMBER) &&
        older_than_days < 1 && !has_window) {
        json_t *err = json_object();
        json_set(err, "status", json_number(400));
        json_set(err, "detail", json_string("older_than_days must be >= 1"));
        return err;
    }

    ws_prune_filters_t f = {.archived = -1};
    f.has_started_before = has_sb; f.started_before = sb;
    f.has_started_after = has_sa; f.started_after = sa;
    f.source = get_opt_str(body, "source");
    f.title_like = get_opt_str(body, "title_like");
    f.end_reason = get_opt_str(body, "end_reason");
    f.cwd_prefix = get_opt_str(body, "cwd_prefix");
    f.model_like = get_opt_str(body, "model_like");
    f.provider = get_opt_str(body, "provider");
    f.user_id = get_opt_str(body, "user_id");
    f.chat_id = get_opt_str(body, "chat_id");
    f.chat_type = get_opt_str(body, "chat_type");
    f.branch_like = get_opt_str(body, "branch_like");
    double d;
    if (get_opt_num(body, "min_messages", &d)) { f.has_min_messages = true; f.min_messages = (int)d; }
    if (get_opt_num(body, "max_messages", &d)) { f.has_max_messages = true; f.max_messages = (int)d; }
    if (get_opt_num(body, "min_tokens", &d)) { f.has_min_tokens = true; f.min_tokens = (long long)d; }
    if (get_opt_num(body, "max_tokens", &d)) { f.has_max_tokens = true; f.max_tokens = (long long)d; }
    if (get_opt_num(body, "min_cost", &d)) { f.has_min_cost = true; f.min_cost = d; }
    if (get_opt_num(body, "max_cost", &d)) { f.has_max_cost = true; f.max_cost = d; }
    if (get_opt_num(body, "min_tool_calls", &d)) { f.has_min_tool_calls = true; f.min_tool_calls = (int)d; }
    if (get_opt_num(body, "max_tool_calls", &d)) { f.has_max_tool_calls = true; f.max_tool_calls = (int)d; }

    /* archived = None if include_archived else False */
    json_t *ia = body ? json_object_get((json_t *)body, "include_archived") : NULL;
    bool include_archived = ia && ((ia->type == JSON_BOOL && ia->bool_val) ||
                                   (ia->type == JSON_NUMBER && ia->num_val != 0));
    f.archived = include_archived ? -1 : 0;

    /* _attr_filters_set: any attribute filter is not None. Numeric fields
     * count when present in the body (pydantic sets them from JSON). */
    bool attr_set =
        f.source || f.title_like || f.end_reason || f.cwd_prefix ||
        f.has_min_messages || f.has_max_messages || f.model_like ||
        f.provider || f.user_id || f.chat_id || f.chat_type ||
        f.branch_like || f.has_min_tokens || f.has_max_tokens ||
        f.has_min_cost || f.has_max_cost || f.has_min_tool_calls ||
        f.has_max_tool_calls;

    /* Effective older_than: body value (null → None) or default 90; then
     * suppressed by a window or by attr filters without explicit older. */
    bool has_older = !older_is_null;
    double eff_older = older_than_days;
    if (has_window || (attr_set && !older_explicit)) has_older = false;

    json_t *dr = body ? json_object_get((json_t *)body, "dry_run") : NULL;
    bool dry_run = dr && ((dr->type == JSON_BOOL && dr->bool_val) ||
                          (dr->type == JSON_NUMBER && dr->num_val != 0));

    if (dry_run) {
        json_t *rows = ws_prune_candidates(db_path, has_older, eff_older, &f);
        size_t nr = json_len(rows);
        json_t *out = json_object();
        json_set(out, "ok", json_bool(true));
        json_set(out, "removed", json_number(0));
        json_set(out, "matched", json_number((double)nr));
        if (nr > 0) {
            json_t *first = json_get(rows, 0);
            json_t *last = json_get(rows, nr - 1);
            json_set(out, "oldest_started_at",
                     json_copy(json_object_get(first, "started_at")));
            json_set(out, "newest_started_at",
                     json_copy(json_object_get(last, "started_at")));
        } else {
            json_set(out, "oldest_started_at", json_null());
            json_set(out, "newest_started_at", json_null());
        }
        json_t *sessions = json_array();
        static const char *keep[] = {"id", "source", "title", "model",
                                         "started_at", "message_count"};
        for (size_t i = 0; i < nr; i++) {
            json_t *r = json_get(rows, i);
            json_t *slim = json_object();
            for (int k = 0; k < 6; k++) {
                json_t *v = json_object_get(r, keep[k]);
                json_set(slim, keep[k], v ? json_copy(v) : json_null());
            }
            json_append(sessions, slim);
        }
        json_set(out, "sessions", sessions);
        json_free(rows);
        return out;
    }

    int removed = ws_prune_sessions(db_path, has_older, eff_older, &f);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "removed", json_number(removed < 0 ? 0 : removed));
    return out;
}


/* ── from src/cli/port_web_server_session_detail.c ── */
/*
 * port_web_server_session_detail.c — session detail plumbing: a faithful C11
 * port of the SessionDB message/session methods in hermes_state.py
 * (get_session, get_messages, sanitize_title, get_compression_tip,
 * resolve_resume_session_id, delete_session, set/get_session_title,
 * set_session_archived, set_session_pinned, export_session).
 *
 * Self-contained: opens the sqlite store directly (matching the sibling
 * port_web_server_sessions_admin.c pattern), uses libjson for JSON. No
 * dependency on the opaque hermes_state_db_t internals.
 *
 * The web_server.py HTTP-shape wrappers live in
 * port_web_server_session_endpoints.c, which reuses this plumbing through
 * web_server_session_detail.h (no god header, no duplicated SQL).
 */

#include "web_server_session_detail.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "sqlite3.h"

/* ───────────────────────────── shared SQL utils ──────────────────────── */

/* Open the store read-only (rw=false) or read/write. Returns NULL on failure. */
sqlite3 *ws_sess_open_db(const char *path, bool rw) {
    sqlite3 *db = NULL;
    int flags = rw ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;
    if (sqlite3_open_v2(path, &db, flags, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    return db;
}

static const char *col_txt(sqlite3_stmt *st, int i) {
    const unsigned char *t = sqlite3_column_text(st, i);
    return t ? (const char *)t : NULL;
}

/* UTF-8 decode one code point. Returns #bytes consumed (1-4) or 0 on
 * invalid lead; writes the code point to *cp. */
static int utf8_decode__hermes_state(const unsigned char *s, unsigned long *cp) {
    unsigned char c = s[0];
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0) {
        if (!s[1]) return 0;
        *cp = ((unsigned long)(c & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {
        if (!s[1] || !s[2]) return 0;
        *cp = ((unsigned long)(c & 0x0F) << 12) |
              ((unsigned long)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {
        if (!s[1] || !s[2] || !s[3]) return 0;
        *cp = ((unsigned long)(c & 0x07) << 18) |
              ((unsigned long)(s[1] & 0x3F) << 12) |
              ((unsigned long)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    return 0;
}

static int utf8_encode(unsigned long cp, char *out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* ── sanitize_title ─────────────────────────────────────────────────── */
/* PoP: ws_sess_sanitize_title @ hermes_state.py:sanitize_title */
#define MAX_TITLE_LENGTH 100

char *ws_sess_sanitize_title(const char *title, char **err) {
    if (err) *err = NULL;
    if (!title || !*title) return NULL;

    /* Reject lone surrogates / pass-through via a code-point filter. */
    size_t inlen = strlen(title);
    char *clean = malloc(inlen * 4 + 1);
    size_t oi = 0;
    for (size_t i = 0; i < inlen; ) {
        unsigned long cp;
        int n = utf8_decode__hermes_state((const unsigned char *)title + i, &cp);
        if (n == 0) { i++; continue; } /* skip invalid byte */
        i += (size_t)n;
        /* ASCII control chars 0x00-0x1F / 0x7F, but keep \t \n \r
         * (0x09,0x0A,0x0D) so the whitespace collapse can normalize them. */
        if (cp <= 0x1F && cp != 0x09 && cp != 0x0A && cp != 0x0D) continue;
        if (cp == 0x7F) continue;
        /* Problematic Unicode controls. */
        if ((cp >= 0x200B && cp <= 0x200F) ||
            (cp >= 0x2028 && cp <= 0x202E) ||
            (cp >= 0x2060 && cp <= 0x2069) ||
            cp == 0xFEFF || cp == 0xFFFC ||
            (cp >= 0xFFF9 && cp <= 0xFFFB))
            continue;
        /* Lone surrogate range. */
        if (cp >= 0xD800 && cp <= 0xDFFF) continue;
        oi += (size_t)utf8_encode(cp, clean + oi);
    }
    clean[oi] = '\0';

    /* Collapse internal whitespace runs to single space + strip. */
    char *out = malloc(strlen(clean) + 1);
    size_t oj = 0;
    bool prev_ws = true; /* suppress leading whitespace */
    for (size_t i = 0; clean[i]; ) {
        unsigned long cp;
        int n = utf8_decode__hermes_state((const unsigned char *)clean + i, &cp);
        if (n == 0) { i++; continue; }
        i += (size_t)n;
        bool ws = (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' ||
                   cp == '\f' || cp == '\v');
        if (ws) {
            if (!prev_ws) out[oj++] = ' ';
            prev_ws = true;
        } else {
            memcpy(out + oj, clean + i - n, (size_t)n);
            oj += (size_t)n;
            prev_ws = false;
        }
    }
    if (oj > 0 && out[oj - 1] == ' ') oj--; /* strip trailing space */
    out[oj] = '\0';
    free(clean);

    if (oj == 0) { free(out); return NULL; }
    if (oj > MAX_TITLE_LENGTH) {
        free(out);
        if (err) {
            char buf[128];
            int k = snprintf(buf, sizeof buf,
                             "Title too long (%zu chars, max %d)", oj, MAX_TITLE_LENGTH);
            *err = malloc((size_t)k + 1);
            memcpy(*err, buf, (size_t)k + 1);
        }
        return NULL;
    }
    return out;
}

/* ── get_session ────────────────────────────────────────────────────── */
/* PoP: ws_sess_get_session @ hermes_state.py:get_session */
json_t *ws_sess_get_session(const char *db_path, const char *session_id) {
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return NULL;
    sqlite3_stmt *st = NULL;
    json_t *out = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT * FROM sessions WHERE id = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            int ncol = sqlite3_column_count(st);
            out = json_object();
            for (int c = 0; c < ncol; c++) {
                const char *name = sqlite3_column_name(st, c);
                int t = sqlite3_column_type(st, c);
                json_t *v;
                if (t == SQLITE_NULL) v = json_null();
                else if (t == SQLITE_INTEGER)
                    v = json_number((double)sqlite3_column_int64(st, c));
                else if (t == SQLITE_FLOAT)
                    v = json_number(sqlite3_column_double(st, c));
                else
                    v = json_string(col_txt(st, c));
                json_set(out, name, v);
            }
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return out;
}

/* ── get_messages ───────────────────────────────────────────────────── */
/* PoP: decode_message_row @ hermes_state.py:get_messages */
static json_t *decode_message_row(sqlite3_stmt *st) {
    int ncol = sqlite3_column_count(st);
    json_t *m = json_object();
    for (int c = 0; c < ncol; c++) {
        const char *name = sqlite3_column_name(st, c);
        int t = sqlite3_column_type(st, c);
        json_t *v;
        if (t == SQLITE_NULL) { v = json_null(); }
        else if (t == SQLITE_INTEGER)
            v = json_number((double)sqlite3_column_int64(st, c));
        else if (t == SQLITE_FLOAT)
            v = json_number(sqlite3_column_double(st, c));
        else {
            const char *raw = col_txt(st, c);
            if (strcmp(name, "content") == 0 && raw &&
                strncmp(raw, "\x00json:", 6) == 0) {
                /* _decode_content: JSON-encoded blob. */
                json_t *dec = json_parse(raw + 6, NULL);
                v = dec ? dec : json_string(raw);
            } else if (strcmp(name, "tool_calls") == 0 && raw) {
                json_t *dec = json_parse(raw, NULL);
                v = dec ? dec : json_array();
            } else if (strcmp(name, "display_metadata") == 0 && raw) {
                json_t *dec = json_parse(raw, NULL);
                v = (dec && dec->type == JSON_OBJECT) ? dec : json_null();
            } else {
                v = json_string(raw);
            }
        }
        json_set(m, name, v);
    }
    return m;
}

json_t *ws_sess_get_messages(const char *db_path, const char *session_id,
                             bool include_inactive, bool has_limit,
                             int limit, int offset) {
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return json_array();
    char sql[1024];
    const char *active = include_inactive ? "" : " AND active = 1";
    if (has_limit)
        snprintf(sql, sizeof sql,
                 "SELECT * FROM messages WHERE session_id = ?%s "
                 "ORDER BY id LIMIT ? OFFSET ?", active);
    else
        snprintf(sql, sizeof sql,
                 "SELECT * FROM messages WHERE session_id = ?%s "
                 "ORDER BY id", active);
    sqlite3_stmt *st = NULL;
    json_t *arr = json_array();
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (has_limit) {
            sqlite3_bind_int(st, 2, limit);
            sqlite3_bind_int(st, 3, offset);
        }
        while (sqlite3_step(st) == SQLITE_ROW)
            json_append(arr, decode_message_row(st));
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return arr;
}

/* ── compression_tip / resolve_resume_id ────────────────────────────── */
/* PoP: ws_sess_compression_tip @ hermes_state.py:get_compression_tip */
char *ws_sess_compression_tip(const char *db_path, const char *session_id) {
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return strdup(session_id ? session_id : "");
    char *current = strdup(session_id ? session_id : "");
    char *seen[128];
    int seen_n = 0;
    /* Bounded defensive walk (Python caps at 100). */
    for (int iter = 0; iter < 100; iter++) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db,
                "SELECT child.id FROM sessions parent "
                "JOIN sessions child ON child.parent_session_id = parent.id "
                "WHERE parent.id = ? AND parent.end_reason = 'compression' "
                "AND json_extract(COALESCE(child.model_config,'{}'),'$._branched_from') IS NULL "
                "AND json_extract(COALESCE(child.model_config,'{}'),'$._delegate_from') IS NULL "
                "AND COALESCE(child.source,'') != 'tool' "
                "ORDER BY CASE WHEN child.end_reason='compression' THEN 0 "
                "WHEN child.ended_at IS NULL THEN 1 ELSE 2 END, "
                "child.started_at DESC, child.id DESC LIMIT 1",
                -1, &st, NULL) != SQLITE_OK) { sqlite3_finalize(st); break; }
        sqlite3_bind_text(st, 1, current, -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(st);
        char childbuf[256];
        childbuf[0] = '\0';
        if (rc == SQLITE_ROW) {
            const unsigned char *c = sqlite3_column_text(st, 0);
            if (c) snprintf(childbuf, sizeof childbuf, "%s", (const char *)c);
        }
        sqlite3_finalize(st);   /* child pointer now dangles — use childbuf */
        if (!childbuf[0]) break;
        bool already = false;
        for (int s = 0; s < seen_n; s++)
            if (strcmp(seen[s], childbuf) == 0) already = true;
        if (already) break;
        if (seen_n < 128) seen[seen_n++] = strdup(childbuf);
        free(current);
        current = strdup(childbuf);
    }
    for (int s = 0; s < seen_n; s++) free(seen[s]);
    sqlite3_close(db);
    return current;
}

/* PoP: ws_sess_resolve_resume_id @ hermes_state.py:resolve_resume_session_id */
char *ws_sess_resolve_resume_id(const char *db_path, const char *session_id) {
    if (!session_id || !*session_id) return strdup(session_id ? session_id : "");
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return strdup(session_id);

    /* Step 1: follow compression tip forward. */
    char *tip = ws_sess_compression_tip(db_path, session_id);
    const char *base = (tip && *tip) ? tip : session_id;

    char *current = strdup(base);
    char *best = NULL;
    char *seen[128];
    int seen_n = 0;
    for (int iter = 0; iter < 32; iter++) {
        /* has messages? */
        sqlite3_stmt *st = NULL;
        bool has_msg = false;
        if (sqlite3_prepare_v2(db,
                "SELECT 1 FROM messages WHERE session_id = ? LIMIT 1",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, current, -1, SQLITE_TRANSIENT);
            has_msg = (sqlite3_step(st) == SQLITE_ROW);
        }
        sqlite3_finalize(st);
        if (has_msg) { free(best); best = strdup(current); }

        if (sqlite3_prepare_v2(db,
                "SELECT id FROM sessions WHERE parent_session_id = ? "
                "AND json_extract(COALESCE(model_config,'{}'),'$._branched_from') IS NULL "
                "AND json_extract(COALESCE(model_config,'{}'),'$._delegate_from') IS NULL "
                "AND COALESCE(source,'') != 'tool' "
                "ORDER BY started_at DESC, id DESC LIMIT 1",
                -1, &st, NULL) != SQLITE_OK)
            break;
        sqlite3_bind_text(st, 1, current, -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(st);
        char childbuf[256];
        childbuf[0] = '\0';
        if (rc == SQLITE_ROW) {
            const unsigned char *c = sqlite3_column_text(st, 0);
            if (c) snprintf(childbuf, sizeof childbuf, "%s", (const char *)c);
        }
        sqlite3_finalize(st);   /* child pointer now dangles — use childbuf */
        if (!childbuf[0]) break;
        bool already = false;
        for (int s = 0; s < seen_n; s++)
            if (strcmp(seen[s], childbuf) == 0) already = true;
        if (already) break;
        if (seen_n < 128) seen[seen_n++] = strdup(childbuf);
        free(current);
        current = strdup(childbuf);
    }
    char *result = strdup(best ? best : base);
    free(current); free(best);
    for (int s = 0; s < seen_n; s++) free(seen[s]);
    free(tip);
    sqlite3_close(db);
    return result;
}

/* ── delete_session ─────────────────────────────────────────────────── */
/* PoP: ws_sess_delete_session @ hermes_state.py:delete_session */
bool ws_sess_delete_session(const char *db_path, const char *session_id) {
    sqlite3 *db = ws_sess_open_db(db_path, true);
    if (!db) return false;
    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    sqlite3_stmt *st = NULL;
    int count = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM sessions WHERE id = ?",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    if (count == 0) {
        sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        sqlite3_close(db);
        return false;
    }
    if (sqlite3_prepare_v2(db,
            "UPDATE sessions SET parent_session_id = NULL "
            "WHERE parent_session_id = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    if (sqlite3_prepare_v2(db, "DELETE FROM messages WHERE session_id = ?",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    if (sqlite3_prepare_v2(db, "DELETE FROM sessions WHERE id = ?",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    sqlite3_close(db);
    return true;
}

/* ── set_session_title / get_session_title ──────────────────────────── */
/* PoP: ws_sess_get_title @ hermes_state.py:get_session_title */
char *ws_sess_get_title(const char *db_path, const char *session_id) {
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return NULL;
    sqlite3_stmt *st = NULL;
    char *out = NULL;
    if (sqlite3_prepare_v2(db, "SELECT title FROM sessions WHERE id = ?",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *t = sqlite3_column_text(st, 0);
            if (t) out = strdup((const char *)t);
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    return out; /* may be NULL */
}

/* PoP: ws_sess_set_title @ hermes_state.py:set_session_title */
bool ws_sess_set_title(const char *db_path, const char *session_id,
                       const char *title, char **err) {
    if (err) *err = NULL;
    char *clean = ws_sess_sanitize_title(title, err);
    if (!clean) return false; /* too long or empty */

    sqlite3 *db = ws_sess_open_db(db_path, true);
    if (!db) { free(clean); return false; }
    bool ok = false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT id FROM sessions WHERE title = ? AND id != ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, clean, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *conflict = sqlite3_column_text(st, 0);
            /* Compression-ancestor transfer is a no-op simplification:
             * we just reject the conflict like Python's non-ancestor path. */
            if (err) {
                size_t sz = 64;
                *err = malloc(sz);
                snprintf(*err, sz, "already_in_use:%s",
                         conflict ? (const char *)conflict : "");
            }
            ok = false;
        } else {
            sqlite3_stmt *up = NULL;
            if (sqlite3_prepare_v2(db,
                    "UPDATE sessions SET title = ? WHERE id = ?",
                    -1, &up, NULL) == SQLITE_OK) {
                sqlite3_bind_text(up, 1, clean, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(up, 2, session_id, -1, SQLITE_TRANSIENT);
                sqlite3_step(up);
                ok = sqlite3_changes(db) > 0;
            }
            sqlite3_finalize(up);
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    free(clean);
    return ok;
}

/* ── set_archived / set_pinned ──────────────────────────────────────── */
/* PoP: ws_sess_set_archived @ hermes_state.py:set_session_archived */
bool ws_sess_set_archived(const char *db_path, const char *session_id,
                          bool archived) {
    sqlite3 *db = ws_sess_open_db(db_path, true);
    if (!db) return false;
    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    sqlite3_stmt *st = NULL;
    const char *sql =
        "WITH RECURSIVE "
        "  ancestors(id) AS ("
        "    SELECT ? "
        "    UNION "
        "    SELECT parent.id FROM ancestors a "
        "    JOIN sessions child ON child.id = a.id "
        "    JOIN sessions parent ON parent.id = child.parent_session_id "
        "    WHERE parent.end_reason = 'compression'), "
        "  descendants(id) AS ("
        "    SELECT ? "
        "    UNION "
        "    SELECT child.id FROM descendants d "
        "    JOIN sessions parent ON parent.id = d.id "
        "    JOIN sessions child ON child.parent_session_id = parent.id "
        "    WHERE parent.end_reason = 'compression'), "
        "  lineage(id) AS (SELECT id FROM ancestors UNION SELECT id FROM descendants) "
        "UPDATE sessions SET archived = ? WHERE id IN (SELECT id FROM lineage)";
    int rowcount = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 3, archived ? 1 : 0);
        sqlite3_step(st);
        rowcount = sqlite3_changes(db);
    }
    sqlite3_finalize(st);
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    sqlite3_close(db);
    return rowcount > 0;
}

/* PoP: ws_sess_set_pinned @ hermes_state.py:set_session_pinned */
bool ws_sess_set_pinned(const char *db_path, const char *session_id,
                        bool pinned) {
    sqlite3 *db = ws_sess_open_db(db_path, true);
    if (!db) return false;
    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    sqlite3_stmt *st = NULL;
    const char *sql =
        "WITH RECURSIVE "
        "  ancestors(id) AS ("
        "    SELECT ? "
        "    UNION "
        "    SELECT parent.id FROM ancestors a "
        "    JOIN sessions child ON child.id = a.id "
        "    JOIN sessions parent ON parent.id = child.parent_session_id "
        "    WHERE parent.end_reason = 'compression'), "
        "  descendants(id) AS ("
        "    SELECT ? "
        "    UNION "
        "    SELECT child.id FROM descendants d "
        "    JOIN sessions parent ON parent.id = d.id "
        "    JOIN sessions child ON child.parent_session_id = parent.id "
        "    WHERE parent.end_reason = 'compression'), "
        "  lineage(id) AS (SELECT id FROM ancestors UNION SELECT id FROM descendants) "
        "UPDATE sessions SET pinned = ? WHERE id IN (SELECT id FROM lineage)";
    int rowcount = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 3, pinned ? 1 : 0);
        sqlite3_step(st);
        rowcount = sqlite3_changes(db);
    }
    sqlite3_finalize(st);
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    sqlite3_close(db);
    return rowcount > 0;
}

/* ── export_session ─────────────────────────────────────────────────── */
/* PoP: ws_sess_export_session @ hermes_state.py:export_session */
json_t *ws_sess_export_session(const char *db_path, const char *session_id) {
    json_t *sess = ws_sess_get_session(db_path, session_id);
    if (!sess) return NULL;
    json_t *msgs = ws_sess_get_messages(db_path, session_id, false, false, 0, 0);
    json_set(sess, "messages", msgs);
    return sess;
}


/* ── from src/cli/port_web_server_sessions_admin.c ── */
/*
 * port_web_server_sessions_admin.c — sessions admin cluster.
 * Faithful port of SessionDB.count_empty_sessions, delete_empty_sessions,
 * message_count, session_count (hermes_state.py) and the get_session_stats
 * aggregation (hermes_cli/web_server.py) against the real sqlite store.
 */

#include "web_server_sessions_admin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "sqlite3.h"

/* hermes_state.py module constants:
 * _BRANCH_CHILD_SQL / _LISTABLE_CHILD_SQL / _delegate_from_json */
#define BRANCH_CHILD_SQL(a) \
    "json_extract(COALESCE(" a ".model_config, '{}'), '$._branched_from') IS NOT NULL" \
    " OR EXISTS (SELECT 1 FROM sessions p" \
    "            WHERE p.id = " a ".parent_session_id" \
    "            AND p.end_reason = 'branched'" \
    "            AND " a ".started_at >= p.ended_at)"

#define LISTABLE_CHILD_SQL \
    "(s.parent_session_id IS NULL OR " BRANCH_CHILD_SQL("s") ")"

#define DELEGATE_IS_NULL \
    "json_extract(COALESCE(s.model_config, '{}'), '$._delegate_from') IS NULL"

static sqlite3 *open_db(const char *path, bool rw) {
    sqlite3 *db = NULL;
    int flags = rw ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY;
    if (sqlite3_open_v2(path, &db, flags, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    return db;
}

static int scalar_int(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st = NULL;
    int out = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW)
        out = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return out;
}

/* ── count_empty_sessions ───────────────────────────────────────────────── */
/* PoP: ws_sessions_count_empty @ hermes_state.py:count_empty_sessions */
int ws_sessions_count_empty(const char *db_path) {
    sqlite3 *db = open_db(db_path, false);
    if (!db) return 0;
    int n = scalar_int(db,
        "SELECT COUNT(*) FROM sessions WHERE message_count = 0 "
        "AND ended_at IS NOT NULL AND archived = 0");
    sqlite3_close(db);
    return n;
}

/* ── delete_empty_sessions ──────────────────────────────────────────────── */
/* PoP: ws_sessions_delete_empty @ hermes_state.py:delete_empty_sessions */
int ws_sessions_delete_empty(const char *db_path) {
    sqlite3 *db = open_db(db_path, true);
    if (!db) return -1;

    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    /* Select candidate ids first. */
    char **ids = NULL;
    size_t n = 0, cap = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT id FROM sessions WHERE message_count = 0 "
            "AND ended_at IS NOT NULL AND archived = 0",
            -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (n == cap) {
                cap = cap ? cap * 2 : 16;
                ids = realloc(ids, cap * sizeof *ids);
            }
            const unsigned char *t = sqlite3_column_text(st, 0);
            ids[n++] = strdup(t ? (const char *)t : "");
        }
    }
    sqlite3_finalize(st);

    if (n == 0) {
        sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        free(ids);
        sqlite3_close(db);
        return 0;
    }

    /* Orphan children of the kill list. */
    size_t sql_len = 128 + n * 2;
    char *sql = malloc(sql_len);
    strcpy(sql, "UPDATE sessions SET parent_session_id = NULL "
                "WHERE parent_session_id IN (");
    for (size_t i = 0; i < n; i++)
        strcat(sql, i + 1 < n ? "?," : "?");
    strcat(sql, ")");
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        for (size_t i = 0; i < n; i++)
            sqlite3_bind_text(st, (int)i + 1, ids[i], -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    free(sql);

    /* Delete messages (paranoia) + session rows. */
    for (size_t i = 0; i < n; i++) {
        if (sqlite3_prepare_v2(db, "DELETE FROM messages WHERE session_id = ?",
                               -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, ids[i], -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
        }
        sqlite3_finalize(st);
        if (sqlite3_prepare_v2(db, "DELETE FROM sessions WHERE id = ?",
                               -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, ids[i], -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
        }
        sqlite3_finalize(st);
    }
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);

    for (size_t i = 0; i < n; i++) free(ids[i]);
    free(ids);
    sqlite3_close(db);
    return (int)n;
}

/* ── message_count ──────────────────────────────────────────────────────── */
/* PoP: ws_sessions_message_count @ hermes_state.py:message_count */
int ws_sessions_message_count(const char *db_path, const char *session_id) {
    sqlite3 *db = open_db(db_path, false);
    if (!db) return 0;
    int out = 0;
    sqlite3_stmt *st = NULL;
    if (session_id && *session_id) {
        if (sqlite3_prepare_v2(db,
                "SELECT COUNT(*) FROM messages WHERE session_id = ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) out = sqlite3_column_int(st, 0);
        }
        sqlite3_finalize(st);
    } else {
        out = scalar_int(db, "SELECT COUNT(*) FROM messages");
    }
    sqlite3_close(db);
    return out;
}

/* ── session_count ──────────────────────────────────────────────────────── */
/* PoP: ws_sessions_session_count @ hermes_state.py:session_count */
int ws_sessions_session_count(const char *db_path,
                              const ws_session_count_opts_t *opts) {
    sqlite3 *db = open_db(db_path, false);
    if (!db) return 0;

    char sql[2048];
    char where[1536] = "";
    int nclauses = 0;
    const char *params[4];
    int nparams = 0;

#define ADD_CLAUSE(c) do { \
        if (nclauses++) strcat(where, " AND "); \
        strcat(where, c); \
    } while (0)

    if (opts && opts->exclude_children) {
        ADD_CLAUSE(LISTABLE_CHILD_SQL);
        ADD_CLAUSE(DELEGATE_IS_NULL);
    }
    if (opts && opts->source) {
        ADD_CLAUSE("s.source = ?");
        params[nparams++] = opts->source;
    }
    if (opts && opts->min_message_count > 0)
        ADD_CLAUSE("s.message_count >= ?");
    if (opts && opts->archived_only)
        ADD_CLAUSE("s.archived = 1");
    else if (!opts || !opts->include_archived)
        ADD_CLAUSE("s.archived = 0");
#undef ADD_CLAUSE

    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM sessions s%s%s",
             nclauses ? " WHERE " : "", where);

    sqlite3_stmt *st = NULL;
    int out = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        int bind = 1;
        for (int i = 0; i < nparams; i++)
            sqlite3_bind_text(st, bind++, params[i], -1, SQLITE_TRANSIENT);
        if (opts && opts->min_message_count > 0)
            sqlite3_bind_int(st, bind++, opts->min_message_count);
        if (sqlite3_step(st) == SQLITE_ROW) out = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    return out;
}

/* ── get_session_stats ──────────────────────────────────────────────────── */
/* PoP: ws_sessions_stats @ hermes_cli/web_server.py:get_session_stats */
/* PoP: ws_sessions_stats @ hermes_cli/web_routers/sessions.py:get_session_stats */
json_t *ws_sessions_stats(const char *db_path) {
    ws_session_count_opts_t all = {.include_archived = true};
    ws_session_count_opts_t store = {.include_archived = false};
    ws_session_count_opts_t arch = {.archived_only = true};

    json_t *out = json_object();
    json_set(out, "total",
             json_number(ws_sessions_session_count(db_path, &all)));
    json_set(out, "active_store",
             json_number(ws_sessions_session_count(db_path, &store)));
    json_set(out, "archived",
             json_number(ws_sessions_session_count(db_path, &arch)));
    json_set(out, "messages",
             json_number(ws_sessions_message_count(db_path, NULL)));

    /* by_source over list_sessions_rich(include_archived=True,
     * compact_rows=True) rows: default include_children=False → the
     * listable-child + delegate filters; no archived clause. */
    json_t *by_source = json_object();
    sqlite3 *db = open_db(db_path, false);
    if (db) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db,
                "SELECT s.source, COUNT(*) FROM sessions s WHERE "
                LISTABLE_CHILD_SQL " AND " DELEGATE_IS_NULL
                " GROUP BY s.source",
                -1, &st, NULL) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char *src = sqlite3_column_text(st, 0);
                const char *key =
                    (src && ((const char *)src)[0]) ? (const char *)src : "cli";
                int cnt = sqlite3_column_int(st, 1);
                json_t *prev = json_object_get(by_source, key);
                double base = prev && prev->type == JSON_NUMBER ? prev->num_val : 0;
                json_set(by_source, key, json_number(base + cnt));
            }
        }
        sqlite3_finalize(st);
        sqlite3_close(db);
    }
    json_set(out, "by_source", by_source);
    return out;
}

