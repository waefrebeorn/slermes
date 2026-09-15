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
#include <sqlite3.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_state.py: FD budget + read-budget cluster ── */

/* PoP: _open_fd_count @ hermes_state.py:_open_fd_count */
int _open_fd_count(void) {
    static const char *const fd_dirs[] = { "/proc/self/fd", "/dev/fd", NULL };
    for (int i = 0; fd_dirs[i]; i++) {
        DIR *d = opendir(fd_dirs[i]);
        if (!d) {
            if (errno == EMFILE || errno == ENFILE)
                return -1;
            continue;
        }
        int count = 0;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) count++;
        closedir(d);
        return count;
    }
    return -1;
}

/* PoP: _fd_soft_limit @ hermes_state.py:_fd_soft_limit */
int _fd_soft_limit(void) {
#ifdef __linux__
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != 0) return -1;
    if (rl.rlim_cur == RLIM_INFINITY) return -1;
    return (int)rl.rlim_cur;
#else
    return -1;
#endif
}

static struct { double stamp; int cached; pthread_mutex_t lock; } _fd_cache = {
    .stamp = -1.0, .cached = -1, .lock = PTHREAD_MUTEX_INITIALIZER
};
#define FD_USAGE_CACHE_SECONDS 5.0
#define FD_HEADROOM_RESERVE 16

/* PoP: _fd_headroom_ok @ hermes_state.py:_fd_headroom_ok */
bool _fd_headroom_ok(void) {
    int soft = _fd_soft_limit();
    if (soft < 0) return true;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = ts.tv_sec + ts.tv_nsec / 1e9;
    pthread_mutex_lock(&_fd_cache.lock);
    bool fresh = (_fd_cache.cached >= 0) && (now - _fd_cache.stamp < FD_USAGE_CACHE_SECONDS);
    int cached = _fd_cache.cached;
    pthread_mutex_unlock(&_fd_cache.lock);
    if (!fresh) {
        cached = _open_fd_count();
        pthread_mutex_lock(&_fd_cache.lock);
        _fd_cache.stamp = now;
        _fd_cache.cached = cached;
        pthread_mutex_unlock(&_fd_cache.lock);
    }
    if (cached < 0) return false;
    return (soft - cached) > FD_HEADROOM_RESERVE;
}

/* PoP: _normalize_telegram_topic_profile_name @ hermes_state.py:_normalize_telegram_topic_profile_name */
const char *_normalize_telegram_topic_profile_name(const char *profile_name) {
    static char result[256];
    if (!profile_name || !*profile_name) return "default";
    while (*profile_name == ' ' || *profile_name == '\t') profile_name++;
    size_t len = strlen(profile_name);
    while (len > 0 && (profile_name[len-1] == ' ' || profile_name[len-1] == '\t')) len--;
    if (len == 0 || len >= sizeof(result)) return "default";
    memcpy(result, profile_name, len);
    result[len] = '\0';
    return result;
}

/* PoP: _database_has_content @ hermes_state.py:_database_has_content */
bool _database_has_content(sqlite3 *conn) {
    if (!conn) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA page_count", -1, &st, NULL) != SQLITE_OK)
        return false;
    bool has = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        int64_t n = sqlite3_column_int64(st, 0);
        has = n > 0;
    }
    sqlite3_finalize(st);
    return has;
}

/* PoP: is_transient_sqlite_error @ hermes_state.py:is_transient_sqlite_error */
bool is_transient_sqlite_error(const char *msg) {
    if (!msg) return false;
    static const char *markers[] = {
        "database is locked", "database table is locked",
        "disk i/o error", "busy", NULL
    };
    char lower[1024];
    size_t len = strlen(msg);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (msg[i] >= 'A' && msg[i] <= 'Z') ? msg[i] + 32 : msg[i];
    lower[len] = '\0';
    for (int i = 0; markers[i]; i++)
        if (strstr(lower, markers[i])) return true;
    return false;
}

/* PoP: _is_transient_read_only_ioerr @ hermes_state.py:_is_transient_read_only_ioerr */
bool _is_transient_read_only_ioerr(const char *msg, int attempt) {
    if (attempt >= 3) return false;
    if (!msg) return false;
    char lower[1024];
    size_t len = strlen(msg);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (msg[i] >= 'A' && msg[i] <= 'Z') ? msg[i] + 32 : msg[i];
    lower[len] = '\0';
    return strstr(lower, "disk i/o error") != NULL;
}

/* PoP: resolve_synchronous_level @ hermes_state.py:resolve_synchronous_level */
int resolve_synchronous_level(const char *raw) {
    if (!raw || !*raw) return -1;
    char upper[32];
    size_t len = strlen(raw);
    if (len >= sizeof(upper)) len = sizeof(upper) - 1;
    for (size_t i = 0; i < len; i++)
        upper[i] = (raw[i] >= 'a' && raw[i] <= 'z') ? raw[i] - 32 : raw[i];
    upper[len] = '\0';
    if (strcmp(upper, "OFF") == 0) return 0;
    if (strcmp(upper, "NORMAL") == 0) return 1;
    if (strcmp(upper, "FULL") == 0) return 2;
    if (strcmp(upper, "EXTRA") == 0) return 3;
    char *end;
    long v = strtol(raw, &end, 10);
    if (*end == '\0' && v >= 0 && v <= 3) return (int)v;
    return -1;
}
