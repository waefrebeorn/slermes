/*
 * port_cron_notepad.c — Port of Python: cron/notepad.py
 *
 * SQLite KV store with the same table, caps and error text as the Python
 * module. The Python opens through hermes_cli.sqlite_util.open_db (WAL,
 * busy timeout, schema init); this port opens the same file with WAL and a
 * busy timeout and runs the identical CREATE TABLE.
 */

#include "port_cron_notepad.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#define CN_SCHEMA \
    "CREATE TABLE IF NOT EXISTS cron_notepad (\n" \
    "  job_id TEXT NOT NULL,\n" \
    "  key TEXT NOT NULL,\n" \
    "  value TEXT NOT NULL,\n" \
    "  updated_at TEXT NOT NULL,\n" \
    "  PRIMARY KEY (job_id, key)\n" \
    ")"

static char *dup_str(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *join3(const char *a, const char *b, const char *c)
{
    size_t cap = strlen(a ? a : "") + strlen(b ? b : "") + strlen(c ? c : "") + 4;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    snprintf(out, cap, "%s%s%s", a ? a : "", b ? b : "", c ? c : "");
    return out;
}

/* PoP: cn_notepad_file @ cron/notepad.py:_current_notepad_file */
char *cn_notepad_file(const char *home, const char *override_path)
{
    if (override_path && *override_path) return dup_str(override_path);
    if (!home || !*home) return NULL;
    char *dir = join3(home, "/cron", "");
    if (!dir) return NULL;
    mkdir(dir);
    char *path = join3(dir, "/notepad.db", "");
    free(dir);
    return path;
}

/* PoP: cn_initialize_schema @ cron/notepad.py:_initialize_schema */
int cn_initialize_schema(sqlite3 *db)
{
    if (!db) return 0;
    char *err = NULL;
    int rc = sqlite3_exec(db, CN_SCHEMA, NULL, NULL, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK;
}

static sqlite3 *open_db(const char *db_path)
{
    if (!db_path) return NULL;
    sqlite3 *db = NULL;
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    sqlite3_busy_timeout(db, 5000);
    sqlite3_exec(db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    if (!cn_initialize_schema(db)) {
        sqlite3_close(db);
        return NULL;
    }
    return db;
}

/* Records the Python ValueError text and yields a NULL result. */
static json_t *set_error(char **err, const char *message)
{
    if (err) *err = dup_str(message);
    return NULL;
}

/* Python: _hermes_now().isoformat() — an AWARE local datetime, so the string
 * carries microseconds and the UTC offset ("2026-11-05T04:19:35.411299-05:00").
 * Dropping either one is a real fidelity bug: the value is stored and echoed
 * back to the model through the prompt section. */
static void now_iso(char *buf, size_t cap)
{
    time_t secs;
    long micros;
    struct tm tm_local, tm_utc;

#if defined(_WIN32)
    /* Derive the offset from the Windows clock, not the CRT: under MSYS the TZ
     * environment variable is often unset, so localtime() would report UTC
     * while Python (and every other Windows process) reports the real zone. */
    SYSTEMTIME lt, ut;
    FILETIME fl, fu, fp;
    GetLocalTime(&lt);
    GetSystemTime(&ut);
    SystemTimeToFileTime(&lt, &fl);
    SystemTimeToFileTime(&ut, &fu);
    GetSystemTimePreciseAsFileTime(&fp);

    ULARGE_INTEGER ul, uu, up;
    ul.LowPart = fl.dwLowDateTime; ul.HighPart = fl.dwHighDateTime;
    uu.LowPart = fu.dwLowDateTime; uu.HighPart = fu.dwHighDateTime;
    up.LowPart = fp.dwLowDateTime; up.HighPart = fp.dwHighDateTime;

    long long offset100 = (long long)(ul.QuadPart - uu.QuadPart);
    long long local100 = (long long)up.QuadPart + offset100;
    FILETIME flocal;
    ULARGE_INTEGER uf;
    uf.QuadPart = (ULONGLONG)local100;
    flocal.dwLowDateTime = uf.LowPart;
    flocal.dwHighDateTime = uf.HighPart;
    SYSTEMTIME shown;
    FileTimeToSystemTime(&flocal, &shown);

    long off_seconds = (long)(offset100 / 10000000LL);
    micros = (long)((local100 % 10000000LL) / 10);
    snprintf(buf, cap,
             "%04d-%02d-%02dT%02d:%02d:%02d.%06ld%c%02ld:%02ld",
             shown.wYear, shown.wMonth, shown.wDay, shown.wHour, shown.wMinute,
             shown.wSecond, micros,
             off_seconds < 0 ? '-' : '+',
             labs(off_seconds) / 3600, (labs(off_seconds) % 3600) / 60);
    return;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t secs = ts.tv_sec;
    micros = (long)(ts.tv_nsec / 1000);
    localtime_r(&secs, &tm_local);
    gmtime_r(&secs, &tm_utc);
    long off_seconds = (long)(mktime(&tm_local) - timegm(&tm_utc));
#endif

    long off_min = off_seconds / 60;
    char sign = off_min < 0 ? '-' : '+';
    long abs_min = off_min < 0 ? -off_min : off_min;

    snprintf(buf, cap, "%04d-%02d-%02dT%02d:%02d:%02d.%06ld%c%02ld:%02ld",
             tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday,
             tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec, micros,
             sign, abs_min / 60, abs_min % 60);
}

/* PoP: cn_set_note @ cron/notepad.py:set_note */
json_t *cn_set_note(const char *db_path, const char *job_id, const char *key,
                    const char *value, char **err)
{
    if (err) *err = NULL;
    const char *jid = job_id ? job_id : "";
    const char *k = key ? key : "";
    const char *v = value ? value : "";

    /* _validate */
    if (!*jid)
        return set_error(err, "job_id must be non-empty");
    if (!*k)
        return set_error(err, "key must be non-empty");
    if ((long)strlen(k) > CN_MAX_KEY_CHARS) {
        char msg[128];
        snprintf(msg, sizeof(msg), "key too long (max %d characters)", CN_MAX_KEY_CHARS);
        return set_error(err, msg);
    }
    if ((long)strlen(v) > CN_MAX_VALUE_BYTES) {
        char msg[128];
        snprintf(msg, sizeof(msg), "value too large (max %d bytes per key)", CN_MAX_VALUE_BYTES);
        return set_error(err, msg);
    }

    sqlite3 *db = open_db(db_path);
    if (!db) return set_error(err, "cannot open notepad database");

    /* total bytes of the job's OTHER keys */
    sqlite3_stmt *stmt = NULL;
    long long other_bytes = 0;
    if (sqlite3_prepare_v2(db,
            "SELECT COALESCE(SUM(LENGTH(CAST(key AS BLOB)) + "
            "LENGTH(CAST(value AS BLOB))), 0) FROM cron_notepad "
            "WHERE job_id=?1 AND key<>?2", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, jid, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, k, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            other_bytes = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
    }

    long long entry_bytes = (long long)strlen(k) + (long long)strlen(v);
    if (other_bytes + entry_bytes > CN_MAX_JOB_TOTAL_BYTES) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "notepad full: job '%s' would exceed %d bytes total; delete unused keys first",
                 jid, CN_MAX_JOB_TOTAL_BYTES);
        if (err) *err = dup_str(buf);
        sqlite3_close(db);
        return NULL;
    }

    char stamp[64];
    now_iso(stamp, sizeof(stamp));
    int ok = 0;
    if (sqlite3_prepare_v2(db,
            "INSERT INTO cron_notepad (job_id, key, value, updated_at) "
            "VALUES (?1, ?2, ?3, ?4) ON CONFLICT(job_id, key) "
            "DO UPDATE SET value=excluded.value, updated_at=excluded.updated_at",
            -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, jid, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, k, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, v, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, stamp, -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    if (!ok) return set_error(err, "notepad write failed");

    json_t *rec = json_object();
    json_set(rec, "job_id", json_string(jid));
    json_set(rec, "key", json_string(k));
    json_set(rec, "value", json_string(v));
    json_set(rec, "updated_at", json_string(stamp));
    return rec;
}

/* PoP: cn_get_note @ cron/notepad.py:get_note */
char *cn_get_note(const char *db_path, const char *job_id, const char *key)
{
    sqlite3 *db = open_db(db_path);
    if (!db) return NULL;
    char *out = NULL;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT value FROM cron_notepad WHERE job_id=?1 AND key=?2",
                           -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, job_id ? job_id : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, key ? key : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *text = sqlite3_column_text(stmt, 0);
            out = dup_str(text ? (const char *)text : "");
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return out;
}

/* PoP: cn_delete_note @ cron/notepad.py:delete_note */
int cn_delete_note(const char *db_path, const char *job_id, const char *key)
{
    sqlite3 *db = open_db(db_path);
    if (!db) return 0;
    int changes = 0;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM cron_notepad WHERE job_id=?1 AND key=?2",
                           -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, job_id ? job_id : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, key ? key : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return changes > 0;
}

/* PoP: cn_list_notes @ cron/notepad.py:list_notes */
json_t *cn_list_notes(const char *db_path, const char *job_id)
{
    json_t *arr = json_array();
    if (!arr) return NULL;
    sqlite3 *db = open_db(db_path);
    if (!db) return arr;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT job_id, key, value, updated_at FROM cron_notepad "
            "WHERE job_id=?1 ORDER BY key", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, job_id ? job_id : "", -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json_t *row = json_object();
            const char *cols[4] = { "job_id", "key", "value", "updated_at" };
            for (int i = 0; i < 4; i++) {
                const unsigned char *text = sqlite3_column_text(stmt, i);
                json_set(row, cols[i], json_string(text ? (const char *)text : ""));
            }
            json_append(arr, row);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return arr;
}

/* PoP: cn_clear_notepad @ cron/notepad.py:clear_notepad */
int cn_clear_notepad(const char *db_path, const char *job_id)
{
    /* "no-ops without creating the DB when no notepad file exists yet" */
    struct stat sb;
    if (!db_path || stat(db_path, &sb) != 0) return 0;

    sqlite3 *db = open_db(db_path);
    if (!db) return 0;
    int changes = 0;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM cron_notepad WHERE job_id=?1",
                           -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, job_id ? job_id : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return changes;
}

/* PoP: cn_render_notepad_section @ cron/notepad.py:render_notepad_section */
char *cn_render_notepad_section(const char *db_path, const char *job_id)
{
    json_t *notes = cn_list_notes(db_path, job_id);
    if (!notes || notes->c.count == 0) {
        if (notes) json_free(notes);
        return dup_str("");
    }

    size_t cap = 1024;
    for (size_t i = 0; i < notes->c.count; i++) {
        json_t *row = notes->c.items[i];
        cap += strlen(json_get_str(row, "key", "")) +
               strlen(json_get_str(row, "value", "")) + 8;
    }
    char *out = (char *)malloc(cap);
    if (!out) { json_free(notes); return NULL; }

    snprintf(out, cap,
             "## Job notepad (persistent across runs)\n"
             "This durable scratchpad survives between scheduled runs of this "
             "job. Update it via the CLI, e.g.:\n"
             "`hermes cron notepad %s set <key> <value>` "
             "(also: get/delete/list; `hermes cron notepad %s delete "
             "<key>` removes an entry).\n\n",
             job_id ? job_id : "", job_id ? job_id : "");
    size_t o = strlen(out);
    for (size_t i = 0; i < notes->c.count; i++) {
        json_t *row = notes->c.items[i];
        o += (size_t)snprintf(out + o, cap - o, "- %s: %s%s",
                              json_get_str(row, "key", ""),
                              json_get_str(row, "value", ""),
                              i + 1 < notes->c.count ? "\n" : "");
    }
    snprintf(out + o, cap - o, "\n\n");
    json_free(notes);
    return out;
}
