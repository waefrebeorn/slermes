#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sqlite3.h>
#include "hermes_json.h"
#include "hermes_logger.h"

extern int resolve_synchronous_level(const char *raw);

/* ── hermes_state.py: journal-mode + durability + maintenance cluster ── */

/* PoP: _log_journal_mode_upgrade_once @ hermes_state.py:_log_journal_mode_upgrade_once */
static pthread_mutex_t _journal_upgrade_lock = PTHREAD_MUTEX_INITIALIZER;
static char **_journal_upgrade_warned = NULL;
static int _journal_upgrade_n = 0;
static int _journal_upgrade_cap = 0;

void _log_journal_mode_upgrade_once(const char *db_label, const char *prev_mode) {
    if (!db_label) return;
    pthread_mutex_lock(&_journal_upgrade_lock);
    for (int i = 0; i < _journal_upgrade_n; i++) {
        if (strcmp(_journal_upgrade_warned[i], db_label) == 0) {
            pthread_mutex_unlock(&_journal_upgrade_lock);
            return;
        }
    }
    if (_journal_upgrade_n >= _journal_upgrade_cap) {
        _journal_upgrade_cap = _journal_upgrade_cap ? _journal_upgrade_cap * 2 : 8;
        _journal_upgrade_warned = realloc(_journal_upgrade_warned, sizeof(char*) * _journal_upgrade_cap);
    }
    _journal_upgrade_warned[_journal_upgrade_n++] = strdup(db_label);
    pthread_mutex_unlock(&_journal_upgrade_lock);
    hermes_log(LOG_WARNING, "hermes_state", "%s: on-disk journal_mode was %s and has been switched to WAL. This rewrites the database header and persists after this process exits.", db_label, prev_mode ? prev_mode : "?");
}

/* PoP: _log_configured_delete_overridden_once @ hermes_state.py:_log_configured_delete_overridden_once */
static pthread_mutex_t _delete_overridden_lock = PTHREAD_MUTEX_INITIALIZER;
static char **_delete_overridden_warned = NULL;
static int _delete_overridden_n = 0;
static int _delete_overridden_cap = 0;

void _log_configured_delete_overridden_once(const char *db_label) {
    if (!db_label) return;
    pthread_mutex_lock(&_delete_overridden_lock);
    for (int i = 0; i < _delete_overridden_n; i++) {
        if (strcmp(_delete_overridden_warned[i], db_label) == 0) {
            pthread_mutex_unlock(&_delete_overridden_lock);
            return;
        }
    }
    if (_delete_overridden_n >= _delete_overridden_cap) {
        _delete_overridden_cap = _delete_overridden_cap ? _delete_overridden_cap * 2 : 8;
        _delete_overridden_warned = realloc(_delete_overridden_warned, sizeof(char*) * _delete_overridden_cap);
    }
    _delete_overridden_warned[_delete_overridden_n++] = strdup(db_label);
    pthread_mutex_unlock(&_delete_overridden_lock);
    hermes_log(LOG_ERROR, "hermes_state", "%s: database.journal_mode=delete is configured but the on-disk database is already WAL.", db_label);
}

/* PoP: apply_durability_barriers @ hermes_state.py:apply_durability_barriers */
bool apply_durability_barriers(sqlite3 *conn) {
    if (!conn) return false;
    /* Re-apply the configured synchronous level if set */
    const char *sync = getenv("HERMES_DATABASE_SYNCHRONOUS");
    if (sync && *sync) {
        int level = resolve_synchronous_level(sync);
        if (level >= 0) {
            char sql[64];
            snprintf(sql, sizeof(sql), "PRAGMA synchronous=%d", level);
            sqlite3_exec(conn, sql, NULL, NULL, NULL);
        }
    }
    return true;
}

/* PoP: _probe_journal_mode_for_repair @ hermes_state.py:_probe_journal_mode_for_repair */
const char *_probe_journal_mode_for_repair(const char *db_path) {
    static char mode[32];
    if (!db_path) return NULL;
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(db_path, &conn, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        return NULL;
    }
    sqlite3_stmt *st = NULL;
    const char *result = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA journal_mode", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *m = (const char *)sqlite3_column_text(st, 0);
            if (m) {
                strncpy(mode, m, sizeof(mode) - 1);
                mode[sizeof(mode) - 1] = '\0';
                result = mode;
            }
        }
    }
    if (st) sqlite3_finalize(st);
    sqlite3_close(conn);
    return result;
}

/* PoP: _restore_journal_mode_after_repair @ hermes_state.py:_restore_journal_mode_after_repair */
void _restore_journal_mode_after_repair(const char *db_path, const char *before_mode) {
    if (!db_path) return;
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(db_path, &conn, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        return;
    }
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA journal_mode", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *after = (const char *)sqlite3_column_text(st, 0);
            if (after && before_mode && strcmp(after, before_mode) != 0) {
                hermes_log(LOG_WARNING, "hermes_state", "state.db repair changed journal_mode '%s' -> '%s'", before_mode, after);
            }
        }
    }
    if (st) sqlite3_finalize(st);
    sqlite3_close(conn);
}

/* PoP: _try_acquire_auto_maintenance_lock @ hermes_state.py:_try_acquire_auto_maintenance_lock */
/* Returns a file handle (int fd) or -1. Caller must close. */
int _try_acquire_auto_maintenance_lock(const char *db_path) {
    if (!db_path) return -1;
    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s.auto-maintenance.lock", db_path);
    int fd = open(lock_path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return -1;
    struct flock fl = {.l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 1};
    if (fcntl(fd, F_SETLK, &fl) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* PoP: _release_auto_maintenance_lock @ hermes_state.py:_release_auto_maintenance_lock */
void _release_auto_maintenance_lock(int handle) {
    if (handle < 0) return;
    struct flock fl = {.l_type = F_UNLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 1};
    fcntl(handle, F_SETLK, &fl);
    close(handle);
}
