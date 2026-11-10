/*
 * t_port_backup_sqlite.c — Oracle harness for hermes_cli/backup_sqlite.py
 * (ported to src/cli/port_backup_sqlite.c).
 *
 * Builds real SQLite fixtures (journal and WAL), runs the ported preflight,
 * and reports what it produced: the JSON, the surviving .bak count and a
 * quick_check verdict per snapshot. The Python oracle computes the same
 * expectations from its own fixture directory.
 */
#include "port_backup_sqlite.h"
#include "hermes_json.h"
#include "sqlite3.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>

#if defined(_WIN32)
#define MK_DIR(p) mkdir(p)
#else
#define MK_DIR(p) mkdir((p), 0700)
#endif

static void emit_json_str(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

static int make_db(const char *path, int wal)
{
    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }
    char *err = NULL;
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS sessions(id INTEGER PRIMARY KEY, body TEXT)",
                 NULL, NULL, &err);
    if (err) sqlite3_free(err);
    sqlite3_exec(db, wal ? "PRAGMA journal_mode=WAL" : "PRAGMA journal_mode=DELETE",
                 NULL, NULL, &err);
    if (err) sqlite3_free(err);
    sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
    for (int i = 0; i < 200; i++) {
        char sql[256];
        snprintf(sql, sizeof(sql),
                 "INSERT INTO sessions(body) VALUES ('row %d with some filler text')", i);
        sqlite3_exec(db, sql, NULL, NULL, NULL);
    }
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    sqlite3_close(db);
    return 1;
}

static int quick_check(const char *path)
{
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(db, "PRAGMA quick_check", -1, &stmt, NULL) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        ok = text && strcmp((const char *)text, "ok") == 0;
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

static int count_suffix(const char *dir, const char *suffix)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    size_t slen = strlen(suffix);
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);
        if (len >= slen && strcmp(e->d_name + len - slen, suffix) == 0) n++;
    }
    closedir(d);
    return n;
}

/* every .bak in `dir` must pass quick_check */
static int all_baks_ok(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    int total = 0, ok = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);
        if (len < 4 || strcmp(e->d_name + len - 4, ".bak") != 0) continue;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        total++;
        if (quick_check(path)) ok++;
    }
    closedir(d);
    return total > 0 && ok == total;
}

static void emit_preflight(const char *case_name, const char *mode,
                           const char *dir, json_t *result, int extra_field)
{
    printf("{\"family\":\"preflight\",\"case\":\"%s\",\"mode\":\"%s\",\"result\":",
           case_name, mode);
    char *s = result ? json_serialize(result) : NULL;
    fputs(s ? s : "null", stdout);
    free(s);
    printf(",\"path_set\":%d", result && json_obj_get(result, "path") &&
                               json_type(json_obj_get(result, "path")) == JSON_STRING);
    printf(",\"baks\":%d,\"baks_ok\":%d,\"extra\":%d}\n",
           count_suffix(dir, ".bak"), all_baks_ok(dir), extra_field);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char root[] = ".testbin/bsq_XXXXXX";
    if (!mkdtemp(root)) { printf("{\"family\":\"error\",\"case\":\"mkdtemp\"}\n"); return 1; }

    /* journal-mode database, three successive preflights (prune keeps two) */
    {
        char dir[512];
        snprintf(dir, sizeof(dir), "%s/journal", root);
        MK_DIR(dir);
        char db[600];
        snprintf(db, sizeof(db), "%s/state.db", dir);
        make_db(db, 0);
        printf("{\"family\":\"uses_wal\",\"case\":\"journal\",\"ret\":%d}\n", bsq_uses_wal(db));

        json_t *r1 = bsq_preflight_state_db(dir);
        emit_preflight("journal_first", "journal", dir, r1, 0);
        json_free(r1);
        json_t *r2 = bsq_preflight_state_db(dir);
        emit_preflight("journal_second", "journal", dir, r2, 0);
        json_free(r2);
        json_t *r3 = bsq_preflight_state_db(dir);
        emit_preflight("journal_third", "journal", dir, r3, 0);
        json_free(r3);
        printf("{\"family\":\"prune\",\"case\":\"journal_after_three\",\"baks\":%d}\n",
               count_suffix(dir, ".bak"));
    }

    /* WAL database */
    {
        char dir[512];
        snprintf(dir, sizeof(dir), "%s/wal", root);
        MK_DIR(dir);
        char db[600];
        snprintf(db, sizeof(db), "%s/state.db", dir);
        make_db(db, 1);
        printf("{\"family\":\"uses_wal\",\"case\":\"wal\",\"ret\":%d}\n", bsq_uses_wal(db));
        json_t *r = bsq_preflight_state_db(dir);
        emit_preflight("wal_first", "wal", dir, r, 0);
        json_free(r);
    }

    /* missing state.db */
    {
        char dir[512];
        snprintf(dir, sizeof(dir), "%s/empty", root);
        MK_DIR(dir);
        json_t *r = bsq_preflight_state_db(dir);
        emit_preflight("missing_db", "none", dir, r, 0);
        json_free(r);
    }

    /* abandoned staging: a 3-hour-old .partial goes, a fresh one stays */
    {
        char dir[512];
        snprintf(dir, sizeof(dir), "%s/sweep", root);
        MK_DIR(dir);
        char oldp[600], newp[600];
        snprintf(oldp, sizeof(oldp), "%s/state.db.pre-update-emergency-old-1.partial", dir);
        snprintf(newp, sizeof(newp), "%s/state.db.pre-update-emergency-new-2.partial", dir);
        FILE *f = fopen(oldp, "wb"); if (f) { fputs("x", f); fclose(f); }
        f = fopen(newp, "wb"); if (f) { fputs("x", f); fclose(f); }
        struct stat sb;
        if (stat(oldp, &sb) == 0) {
            struct utimbuf ut;
            ut.actime = ut.modtime = time(NULL) - 3 * 3600;
            utime(oldp, &ut);
        }
        bsq_reclaim_abandoned_staging(dir, "state.db.pre-update-emergency-");
        printf("{\"family\":\"sweep\",\"case\":\"old_and_fresh\",\"partials\":%d}\n",
               count_suffix(dir, ".partial"));
    }
    return 0;
}
