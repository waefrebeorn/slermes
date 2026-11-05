/*
 * port_backup_sqlite.c — Port of Python: hermes_cli/backup_sqlite.py
 *
 * The Python module drives the sqlite3 backup API from a progress callback.
 * The C API has no progress hook, so the same loop runs explicitly:
 * sqlite3_backup_step() for a page budget, sqlite3_sleep() between busy steps,
 * a deadline check after each step, and a restart when the step reports more
 * remaining pages than the previous one (a foreign write restarted the copy).
 *
 * A WAL source never blocks its writers, so it is copied in one step. A
 * rollback-journal source holds the read lock for the whole copy, so it is
 * stepped and aborted when the measured rate says the hold would exceed the
 * budget — refusing is the correct outcome, not a slow copy.
 */

#include "port_backup_sqlite.h"
#include "sqlite3.h"

#if defined(_WIN32)
#include <windows.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ROLLBACK_STEP_SECONDS      0.1
#define ROLLBACK_FIRST_STEP_PAGES  256
#define ROLLBACK_BUSY_SLEEP_MS     10
#define ROLLBACK_MAX_RESTARTS      20
#define ROLLBACK_LOCK_HOLD_SECONDS 0.25
#define ABANDONED_STAGING_SECONDS  (2 * 3600)

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

static double monotonic_seconds(void)
{
#if defined(_WIN32)
    return (double)GetTickCount64() / 1000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

/* PoP: bsq_uses_wal @ hermes_cli/backup_sqlite.py:_uses_wal */
int bsq_uses_wal(const char *path)
{
    if (!path) return 0;
    FILE *fh = fopen(path, "rb");
    if (!fh) return 0;
    unsigned char header[19];
    size_t n = fread(header, 1, sizeof(header), fh);
    fclose(fh);
    return n == sizeof(header) && header[18] == 2;
}

/* ── copy ────────────────────────────────────────────────────────── */

struct copy_state {
    double deadline;          /* re-armed after every non-busy step */
    double timeout_seconds;
    int    timed_out;
};

static void arm_deadline(struct copy_state *st)
{
    st->deadline = monotonic_seconds() + st->timeout_seconds;
}

/* One backup() call in Python = one loop of step/sleep here.
 * pages < 0 copies everything in a single step. Returns 0 on success, 1 when a
 * foreign write restarted the copy, -1 on timeout, -2 on a SQLite error. */
static int stepped_copy(sqlite3 *src, sqlite3 *dst, int pages,
                        struct copy_state *st, double *seconds_out,
                        long long *pages_out, long long *total_out,
                        int *restarted_out)
{
    sqlite3_backup *bk = sqlite3_backup_init(dst, "main", src, "main");
    if (!bk) return -2;

    long long last_remaining = -1;
    long long copied = 0, total = 0;
    double started = monotonic_seconds();
    int rc = SQLITE_OK;
    int restarted = 0;

    for (;;) {
        rc = sqlite3_backup_step(bk, pages < 0 ? -1 : pages);
        int remaining = sqlite3_backup_remaining(bk);
        int pagecount = sqlite3_backup_pagecount(bk);
        total = pagecount;
        if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
            if (monotonic_seconds() >= st->deadline) { st->timed_out = 1; rc = -1; break; }
        } else {
            arm_deadline(st);
        }
        if (rc == SQLITE_DONE) { copied = pagecount; break; }
        if (rc != SQLITE_OK && rc != SQLITE_BUSY && rc != SQLITE_LOCKED) break;

        if (last_remaining >= 0 && remaining >= last_remaining) {
            restarted = 1;
            copied = pagecount - remaining;
            break;
        }
        last_remaining = remaining;
        copied = pagecount - remaining;
        if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
            sqlite3_sleep(ROLLBACK_BUSY_SLEEP_MS);
    }

    if (sqlite3_backup_finish(bk) != SQLITE_OK && rc == SQLITE_DONE) rc = -2;
    *seconds_out = monotonic_seconds() - started;
    *pages_out = copied;
    if (total_out) *total_out = total;
    *restarted_out = restarted;
    if (restarted) return 1;
    if (rc == -1) return -1;
    return (rc == SQLITE_DONE) ? 0 : -2;
}

/* PoP: bsq_safe_copy_db @ hermes_cli/backup_sqlite.py:_safe_copy_db */
int bsq_safe_copy_db(const char *src, const char *dst, double timeout_seconds)
{
    if (!src || !dst) return 0;
    sqlite3 *conn = NULL, *backup = NULL;
    int ok = 0;
    char *uri = NULL;

    struct copy_state st;
    st.timeout_seconds = timeout_seconds < 0 ? 0 : timeout_seconds;
    arm_deadline(&st);
    st.timed_out = 0;

    /* mode=ro: never create or write the source. timeout 0 disables SQLite's
     * implicit busy wait so the deadline above is the only bound. */
    uri = join3("file:", src, "?mode=ro");
    if (!uri) return 0;
    if (sqlite3_open_v2(uri, &conn, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, NULL) != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        free(uri);
        return 0;
    }
    free(uri);
    sqlite3_busy_timeout(conn, 0);
    if (sqlite3_open_v2(dst, &backup, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        sqlite3_close(conn);
        if (backup) sqlite3_close(backup);
        remove(dst);
        return 0;
    }

    if (bsq_uses_wal(src)) {
        double secs = 0;
        long long pages = 0, total = 0;
        int restarted = 0;
        int rc = stepped_copy(conn, backup, -1, &st, &secs, &pages, &total, &restarted);
        ok = (rc == 0);
    } else {
        double seconds_acc = 0.0;
        long long pages_acc = 0, total = 0;
        int rc = 0;
        for (int attempt = 0; attempt <= ROLLBACK_MAX_RESTARTS; attempt++) {
            int page_budget = ROLLBACK_FIRST_STEP_PAGES;
            if (seconds_acc > 0.0) {
                double rate = (double)pages_acc / (seconds_acc > 1e-6 ? seconds_acc : 1e-6);
                int scaled = (int)(rate * ROLLBACK_STEP_SECONDS);
                if (scaled > page_budget) page_budget = scaled;
            }
            double secs = 0;
            long long pages = 0;
            int restarted = 0;
            rc = stepped_copy(conn, backup, page_budget, &st, &secs, &pages, &total, &restarted);
            seconds_acc += secs;
            pages_acc += pages;
            if (rc == 0) break;
            if (rc != 1) break;
        }
        if (rc == 1) {
            double rate = seconds_acc > 1e-6 ? (double)pages_acc / seconds_acc : 0.0;
            double expected_hold = rate > 0.0 ? (double)total / (rate > 1.0 ? rate : 1.0) : 0.0;
            if (expected_hold > ROLLBACK_LOCK_HOLD_SECONDS) {
                ok = 0;      /* refuse: a consistent snapshot would block writers */
            } else {
                double secs = 0;
                long long pages = 0, tot = 0;
                int restarted = 0;
                ok = stepped_copy(conn, backup, -1, &st, &secs, &pages, &tot, &restarted) == 0;
            }
        } else {
            ok = (rc == 0);
        }
    }

    sqlite3_close(backup);
    sqlite3_close(conn);
    if (!ok) remove(dst);
    return ok;
}

/* ── staging sweep + preflight ───────────────────────────────────── */

/* PoP: bsq_reclaim_abandoned_staging @ hermes_cli/backup_sqlite.py:_reclaim_abandoned_staging */
void bsq_reclaim_abandoned_staging(const char *home, const char *prefix)
{
    if (!home || !prefix) return;
    DIR *dir = opendir(home);
    if (!dir) return;
    time_t horizon = time(NULL) - ABANDONED_STAGING_SECONDS;
    size_t plen = strlen(prefix);
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *name = ent->d_name;
        if (strncmp(name, prefix, plen) != 0) continue;
        const char *dot = strstr(name + plen, ".partial");
        if (!dot) continue;
        char *path = join3(home, "/", name);
        if (!path) continue;
        struct stat sb;
        if (stat(path, &sb) == 0 && sb.st_mtime < horizon) remove(path);
        free(path);
    }
    closedir(dir);
}

static void prune_snapshots(const char *home, const char *prefix, int keep)
{
    /* collect the .bak names, newest first */
    DIR *dir = opendir(home);
    if (!dir) return;
    char *names[256];
    int n = 0;
    size_t plen = strlen(prefix);
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && n < 256) {
        const char *name = ent->d_name;
        if (strncmp(name, prefix, plen) != 0) continue;
        size_t len = strlen(name);
        if (len < 4 || strcmp(name + len - 4, ".bak") != 0) continue;
        names[n++] = dup_str(name);
    }
    closedir(dir);

    /* the timestamp is embedded in the name, so a descending name sort is a
     * descending age sort (the Python sorts the Paths the same way) */
    for (int i = 1; i < n; i++) {
        char *key = names[i];
        int j = i - 1;
        while (j >= 0 && strcmp(names[j], key) < 0) { names[j + 1] = names[j]; j--; }
        names[j + 1] = key;
    }
    for (int i = 0; i < n; i++) {
        if (i >= keep) {
            char *path = join3(home, "/", names[i]);
            if (path) { remove(path); free(path); }
        }
        free(names[i]);
    }
}

/* PoP: bsq_preflight_state_db @ hermes_cli/backup_sqlite.py:preflight_state_db */
json_t *bsq_preflight_state_db(const char *home)
{
    json_t *out = json_object();
    if (!out || !home) return out;

    char *source = join3(home, "/", "state.db");
    struct stat sb;
    if (!source || stat(source, &sb) != 0) {
        json_set(out, "path", json_null());
        json_set(out, "message", json_string("state.db not found (fresh install?)"));
        free(source);
        return out;
    }

    const char *prefix = "state.db.pre-update-emergency-";
    bsq_reclaim_abandoned_staging(home, prefix);

    /* Python: datetime.now(UTC).strftime("%Y-%m-%dT%H-%M-%S-%fZ") — the
     * microseconds matter: two snapshots in the same second must not collide
     * on one destination name (the prune keeps two distinct files). */
    char stamp[64];
    {
        time_t now = time(NULL);
        struct tm tm_utc;
#if defined(_WIN32)
        gmtime_s(&tm_utc, &now);
        SYSTEMTIME st;
        GetSystemTime(&st);
        long micros = (long)st.wMilliseconds * 1000;
#else
        gmtime_r(&now, &tm_utc);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        long micros = (long)(ts.tv_nsec / 1000);
#endif
        snprintf(stamp, sizeof(stamp), "%04d-%02d-%02dT%02d-%02d-%02d-%06ldZ",
                 tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
                 tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec, micros);
    }

    char suffix[96];
    snprintf(suffix, sizeof(suffix), "%s-%d", stamp, (int)getpid());
    char *base = join3(home, "/", prefix);
    char *staged = join3(base, suffix, ".partial");
    char *destination = join3(base, suffix, ".bak");
    free(base);

    int failed = 0;
    if (!staged || !destination || !bsq_safe_copy_db(source, staged, 10.0)) {
        failed = 1;
    } else {
        /* quick_check before publishing: a corrupt snapshot is worse than none */
        sqlite3 *check = NULL;
        if (sqlite3_open_v2(staged, &check, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
            failed = 1;
            if (check) sqlite3_close(check);
            check = NULL;
        }
        if (!failed && check) {
            sqlite3_stmt *stmt = NULL;
            int ok_rows = 0;
            if (sqlite3_prepare_v2(check, "PRAGMA quick_check", -1, &stmt, NULL) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const unsigned char *text = sqlite3_column_text(stmt, 0);
                    ok_rows = text && strcmp((const char *)text, "ok") == 0;
                }
                sqlite3_finalize(stmt);
            }
            if (!ok_rows) failed = 1;
            sqlite3_close(check);
        }
        if (!failed) {
            struct stat sst;
            if (stat(staged, &sst) != 0) failed = 1;
            else {
                remove(destination);
                if (rename(staged, destination) != 0) {
                    /* Windows rename does not replace; the remove above handles it */
                    failed = 1;
                } else {
                    json_set(out, "path", json_string(destination));
                    json_set(out, "bytes", json_number((double)sst.st_size));
                }
            }
        }
    }

    if (failed) {
        json_set(out, "path", json_null());
        json_set(out, "message", json_string("SQLite safe copy failed; previous emergency snapshots were retained"));
    }

    if (staged) remove(staged);
    {
        char *journal = staged ? join3(staged, "-journal", "") : NULL;
        if (journal) { remove(journal); free(journal); }
    }
    if (!failed) prune_snapshots(home, prefix, 2);

    free(staged);
    free(destination);
    free(source);
    return out;
}
