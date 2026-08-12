/*
 * port_cron_notepad.c — P169: per-job durable KV scratchpad (port of cron/notepad.py).
 *
 * One SQLite key/value store per cron job, persisted to ~/.slermes/cron/notepad.db
 * alongside the executions ledger. Carries state across scheduled wake-ups
 * (cursors, watermarks, watchlists). Prompt-injected each run, so size caps are
 * enforced: MAX_VALUE_BYTES (16 KiB) per key, MAX_JOB_TOTAL_BYTES (64 KiB) per
 * job. Oversized writes raise and leave the store untouched.
 *
 * SQLite access pattern mirrors tools/memory_storage.c (sqlite3_open + prepared
 * statements + sqlite3_bind text/int). The db file is created lazily; a missing
 * file (fresh install, or notebook wiped) yields an empty result set, never a
 * failure — matching the Python guard in clear_notepad.
 */

#include "cron_notepad.h"

#include "hermes_logger.h"
#include "hermes_json.h"
#include "slermes_home.h"
#include "cron_jobs.h"   /* now_iso() */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <sqlite3.h>     /* lib/libdb/sqlite3.h */

/* mirror of port_cron_jobs.c xstrdup (not shared across TUs; local copy). */
static char *np_xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── Constants (mirror cron/notepad.py) ────────────────────────────── */
#define NOTEPAD_MAX_VALUE_BYTES   (16 * 1024)
#define NOTEPAD_MAX_KEY_CHARS     128
#define NOTEPAD_MAX_JOB_TOTAL_BYTES (64 * 1024)
#define NOTEPAD_FILENAME          "notepad.db"

/* ── Paths ─────────────────────────────────────────────────────────── */
static char *notepad_path(void) {
    const char *home = slermes_home();
    if (!home) home = "/tmp/.slermes";
    char *p = malloc(strlen(home) + 32);
    if (p) snprintf(p, strlen(home) + 32, "%s/cron/" NOTEPAD_FILENAME, home);
    return p;
}

/* True if the db file exists on disk (so callers can no-op without creating). */
static bool notepad_file_exists(void) {
    char *p = notepad_path();
    if (!p) return false;
    struct stat st;
    bool exists = (stat(p, &st) == 0);
    free(p);
    return exists;
}

/* ── Connection / schema ───────────────────────────────────────────── */

/* PoP: _connect @ cron/notepad.py:_connect */
static sqlite3 *notepad_connect(void) {
    char *path = notepad_path();
    if (!path) return NULL;
    sqlite3 *db = NULL;
    sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
    if (sqlite3_open_v2(path, &db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        free(path);
        return NULL;
    }
    free(path);
    return db;
}

/* PoP: _initialize_schema @ cron/notepad.py:_initialize_schema */
static int notepad_init_schema(sqlite3 *db) {
    char *err = NULL;
    int rc = sqlite3_exec(db,
        "PRAGMA journal_mode=WAL;"
        "CREATE TABLE IF NOT EXISTS cron_notepad ("
        "  job_id  TEXT NOT NULL,"
        "  key     TEXT NOT NULL,"
        "  value   TEXT NOT NULL,"
        "  updated_at TEXT NOT NULL,"
        "  PRIMARY KEY (job_id, key)"
        ");",
        NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        hermes_log(LOG_WARNING, "cron/notepad", "schema init failed: %s", err ? err : "sqlite error");
        sqlite3_free(err);
        return rc;
    }
    return SQLITE_OK;
}

/* PoP: _transaction @ cron/notepad.py:_transaction */
/* Opens conn + schema inside the body (so a PRAGMA/DDL failure still closes),
 * caller commits/rolls back on exit, then closes. Returns NULL on hard error
 * (caller treats NULL as "no-op"). */
typedef struct {
    sqlite3 *db;
    bool active;
} notepad_txn_t;

static notepad_txn_t notepad_begin(void) {
    notepad_txn_t t = { .db = NULL, .active = false };
    sqlite3 *db = notepad_connect();
    if (!db) return t;
    if (notepad_init_schema(db) == SQLITE_OK) {
        t.db = db;
        t.active = true;
    } else {
        sqlite3_close(db);
    }
    return t;
}

static void notepad_end(notepad_txn_t t, bool commit) {
    if (!t.active) return;
    sqlite3 *db = t.db;
    if (commit) {
        char *err = NULL;
        if (sqlite3_exec(db, "COMMIT", NULL, NULL, &err) != SQLITE_OK) {
            /* best-effort; fall through to close */
            sqlite3_free(err);
        }
    } else {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    }
    sqlite3_close(db);
}

/* ── Text helpers (mirror cron/notepad.py str(job_id) etc.) ─────────── */
static char *text_dup(const char *v) {
    if (!v) return np_xstrdup("");
    return np_xstrdup(v);
}

/* ── Size helpers ──────────────────────────────────────────────────── */
static long utf8_len(const char *v) {
    if (!v) return 0;
    return (long)strlen(v); /* Python len(str) in chars; value cap is bytes. */
}
static long utf8_byte_len(const char *v) {
    /* value-bytes cap mirrors Python len(value.encode("utf-8")). */
    if (!v) return 0;
    long n = 0;
    for (const char *p = v; *p; ) {
        unsigned char c = (unsigned char)*p;
        int cp = 0;
        if (c < 0x80) cp = 1;
        else if ((c & 0xE0) == 0xC0) cp = 2;
        else if ((c & 0xF0) == 0xE0) cp = 3;
        else if ((c & 0xF8) == 0xF0) cp = 4;
        else cp = 1; /* invalid lead → count as 1 to match Python errors="replace" */
        n += cp;
        p += cp;
    }
    return n;
}

/* ── Public API ────────────────────────────────────────────────────── */

/* PoP: set_note @ cron/notepad.py:set_note */
json_t *cron_notepad_set_note(const char *job_id, const char *key,
                              const char *value, char **err) {
    notepad_txn_t t = notepad_begin();
    if (!t.active) return NULL;

    /* Python coerces all three args with str(). */
    char *jid = text_dup(job_id);
    char *k   = text_dup(key);
    char *v   = text_dup(value);

    if (!jid || !k || !v) { free(jid); free(k); free(v); notepad_end(t, false); return NULL; }

    int rc = notepad_validate_note(jid, k, v, err);
    if (rc != 0) {
        free(jid); free(k); free(v);
        notepad_end(t, false);
        return NULL;
    }

    /* Begin a transaction (sqlite3_exec("BEGIN") then the row op). */
    sqlite3_exec(t.db, "BEGIN", NULL, NULL, NULL);

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT COALESCE(SUM(LENGTH(CAST(key AS BLOB)) + LENGTH(CAST(value AS BLOB))), 0) "
        "FROM cron_notepad WHERE job_id=?1 AND key<>?2";
    rc = sqlite3_prepare_v2(t.db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, jid, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, k,   -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            long other = sqlite3_column_int64(stmt, 0);
            long entry = utf8_byte_len(k) + utf8_byte_len(v);
            if (other + entry > NOTEPAD_MAX_JOB_TOTAL_BYTES) {
                if (err) *err = np_xstrdup("notepad full: job would exceed 64 KiB total");
                sqlite3_finalize(stmt);
                sqlite3_exec(t.db, "ROLLBACK", NULL, NULL, NULL);
                sqlite3_close(t.db);
                free(jid); free(k); free(v);
                return NULL;
            }
        }
        sqlite3_finalize(stmt); stmt = NULL;
    }

    const char *upsert =
        "INSERT INTO cron_notepad (job_id, key, value, updated_at) VALUES (?1, ?2, ?3, ?4) "
        "ON CONFLICT(job_id, key) DO UPDATE SET value=excluded.value, updated_at=excluded.updated_at";
    rc = sqlite3_prepare_v2(t.db, upsert, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        char *now = now_iso();
        sqlite3_bind_text(stmt, 1, jid, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, k,   -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, v,   -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, now, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        free(now);
    }
    bool ok = (rc == SQLITE_DONE);
    if (ok) sqlite3_exec(t.db, "COMMIT", NULL, NULL, NULL);
    else    sqlite3_exec(t.db, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_close(t.db);

    json_t *r = json_object();
    if (r) {
        json_set(r, "job_id", json_string(jid));
        json_set(r, "key", json_string(k));
        json_set(r, "value", json_string(v));
        char *now = now_iso();
        json_set(r, "updated_at", json_string(now ? now : ""));
        free(now);
    }
    free(jid); free(k); free(v);
    return r;  /* caller json_free() */
}

/* PoP: _validate @ cron/notepad.py:_validate */
int notepad_validate_note(const char *job_id, const char *key, const char *value, char **err) {
    if (!job_id || !*job_id) { if (err) *err = np_xstrdup("job_id must be non-empty"); return -1; }
    if (!key || !*key)        { if (err) *err = np_xstrdup("key must be non-empty"); return -2; }
    if (utf8_len(key) > NOTEPAD_MAX_KEY_CHARS)
        { if (err) *err = np_xstrdup("key too long (max 128 characters)"); return -3; }
    if (utf8_byte_len(value) > NOTEPAD_MAX_VALUE_BYTES)
        { if (err) *err = np_xstrdup("value too large (max 16 KiB per key)"); return -4; }
    return 0;
}

/* PoP: get_note @ cron/notepad.py:get_note */
char *cron_notepad_get_note(const char *job_id, const char *key) {
    if (!job_id || !key) return NULL;
    sqlite3 *db = notepad_connect();
    if (!db || notepad_init_schema(db) != SQLITE_OK) { if (db) sqlite3_close(db); return NULL; }
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT value FROM cron_notepad WHERE job_id=?1 AND key=?2";
    char *result = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, job_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, key,    -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *v = sqlite3_column_text(stmt, 0);
            if (v) result = np_xstrdup((const char *)v);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return result;  /* NULL when no row → caller maps to None */
}

/* PoP: delete_note @ cron/notepad.py:delete_note */
bool cron_notepad_delete_note(const char *job_id, const char *key) {
    if (!job_id || !key) return false;
    sqlite3 *db = notepad_connect();
    if (!db || notepad_init_schema(db) != SQLITE_OK) { if (db) sqlite3_close(db); return false; }
    sqlite3_stmt *stmt = NULL;
    const char *sql = "DELETE FROM cron_notepad WHERE job_id=?1 AND key=?2";
    int changed = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, job_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, key,    -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) changed = sqlite3_changes(db);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return changed > 0;
}

/* PoP: list_notes @ cron/notepad.py:list_notes */
json_t *cron_notepad_list_notes(const char *job_id) {
    json_t *out = json_array();
    if (!out || !job_id) return out;
    sqlite3 *db = notepad_connect();
    if (!db || notepad_init_schema(db) != SQLITE_OK) { if (db) sqlite3_close(db); return out; }
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT job_id, key, value, updated_at FROM cron_notepad WHERE job_id=?1 ORDER BY key";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, job_id, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *k  = sqlite3_column_text(stmt, 1);
            const unsigned char *v  = sqlite3_column_text(stmt, 2);
            const unsigned char *ua = sqlite3_column_text(stmt, 3);
            json_t *row = json_object();
            if (row) {
                json_set(row, "job_id",  json_string(job_id));
                json_set(row, "key",     json_string((const char *)(k  ? k  : "")));
                json_set(row, "value",   json_string((const char *)(v  ? v  : "")));
                json_set(row, "updated_at", json_string((const char *)(ua ? ua : "")));
                json_append(out, row);
            } else {
                json_free(row);
            }
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return out;  /* caller json_free() */
}

/* PoP: clear_notepad @ cron/notepad.py:clear_notepad */
int cron_notepad_clear_notepad(const char *job_id) {
    if (!job_id) return 0;
    if (!notepad_file_exists()) return 0;  /* no-op, never create the DB */
    sqlite3 *db = notepad_connect();
    if (!db || notepad_init_schema(db) != SQLITE_OK) { if (db) sqlite3_close(db); return 0; }
    sqlite3_stmt *stmt = NULL;
    const char *sql = "DELETE FROM cron_notepad WHERE job_id=?1";
    int n = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, job_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) n = sqlite3_changes(db);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return n;
}

/* PoP: render_notepad_section @ cron/notepad.py:render_notepad_section */
char *cron_notepad_render_notepad_section(const char *job_id) {
    json_t *notes = cron_notepad_list_notes(job_id);
    if (!notes || json_array_size(notes) == 0) { json_free(notes); return np_xstrdup(""); }
    const size_t n = json_array_size(notes);

    /* Two-pass: gather each row's text, then assemble. */
    char **rows = malloc(n ? n * sizeof(*rows) : 1);
    if (!rows) { json_free(notes); return NULL; }
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_array_get(notes, i);
        const char *k = json_object_get_string(row, "key", NULL);
        const char *v = json_object_get_string(row, "value", NULL);
        rows[i] = malloc(1 + snprintf(NULL, 0, "- %s: %s", k ? k : "", v ? v : ""));
        if (rows[i]) { sprintf(rows[i], "- %s: %s", k ? k : "", v ? v : ""); total += strlen(rows[i]); }
    }
    json_free(notes);

    const char *jid = job_id ? job_id : "";
    const char *prefix =
        "## Job notepad (persistent across runs)\n"
        "This durable scratchpad survives between scheduled runs of this "
        "job. Update it via the CLI, e.g.:\n";
    const char *usage =
        "`hermes cron notepad %s set <key> <value>` (also: get/delete/list; "
        "`hermes cron notepad %s delete <key>` removes an entry).\n\n";
    size_t ulen = snprintf(NULL, 0, usage, jid, jid);

    size_t cap = strlen(prefix) + ulen + total + n /* newlines */ + 3;
    char *out = malloc(cap);
    if (!out) { for (size_t i = 0; i < n; i++) free(rows[i]); free(rows); return NULL; }
    size_t o = 0;
    o += snprintf(out + o, cap - o, "%s", prefix);
    o += snprintf(out + o, cap - o, usage, jid, jid);
    for (size_t i = 0; i < n; i++) {
        if (rows[i]) { o += snprintf(out + o, cap - o, "%s\n", rows[i]); free(rows[i]); }
    }
    free(rows);
    o += snprintf(out + o, cap - o, "\n");
    return out;
}
