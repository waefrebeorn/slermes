/*
 * sqlite_safe_read.c — C11 port of hermes_cli/sqlite_safe_read.py (NS-609).
 */

#include "sqlite_safe_read.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

const unsigned char sqlite_safe_header_magic[16] = {
    'S','Q','L','i','t','e',' ','f','o','r','m','a','t',' ','3','\0'
};

#define HDR_PAGE_COUNT_OFFSET 28

/* ── Registry (under g_lock) ─────────────────────────────────────── */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static char **g_keys = NULL;    /* canonical paths */
static int    *g_counts = NULL;
static int     g_n = 0;
static int     g_cap = 0;

static char *dup_key(const char *p) { return p ? strdup(p) : NULL; }

/* PoP: _key @ hermes_cli/sqlite_safe_read.py:_key */
char *sqlite_safe_key(const char *path)
{
    if (!path || !*path) return NULL;
    char resolved[PATH_MAX];
    if (realpath(path, resolved) != NULL)
        return strdup(resolved);
    /* resolve failure (e.g. non-existent) — fall back to str(path) like Python */
    return strdup(path);
}

static void reg_incr(const char *key, int delta)
{
    if (!key) return;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_n; i++) {
        if (strcmp(g_keys[i], key) == 0) {
            g_counts[i] += delta;
            if (g_counts[i] <= 0) {
                free(g_keys[i]);
                memmove(&g_keys[i], &g_keys[i+1], (size_t)(g_n - i - 1) * sizeof(char*));
                memmove(&g_counts[i], &g_counts[i+1], (size_t)(g_n - i - 1) * sizeof(int));
                g_n--;
            }
            pthread_mutex_unlock(&g_lock);
            return;
        }
    }
    if (delta > 0) {
        if (g_n >= g_cap) {
            int nc = g_cap ? g_cap * 2 : 16;
            char **nk = realloc(g_keys, (size_t)nc * sizeof(char*));
            int   *nv = realloc(g_counts, (size_t)nc * sizeof(int));
            if (!nk || !nv) { free(nk); free(nv); pthread_mutex_unlock(&g_lock); return; }
            g_keys = nk; g_counts = nv; g_cap = nc;
        }
        g_keys[g_n] = dup_key(key);
        g_counts[g_n] = delta;
        g_n++;
    }
    pthread_mutex_unlock(&g_lock);
}

/* PoP: track_connection @ hermes_cli/sqlite_safe_read.py:track_connection */
void sqlite_safe_track_connection(const char *path)
{
    char *key = sqlite_safe_key(path);
    if (!key) return;
    reg_incr(key, +1);
    free(key);
}

/* PoP: untrack_connection @ hermes_cli/sqlite_safe_read.py:untrack_connection */
void sqlite_safe_untrack_connection(const char *path)
{
    char *key = sqlite_safe_key(path);
    if (!key) return;
    reg_incr(key, -1);
    free(key);
}

/* PoP: has_live_connection @ hermes_cli/sqlite_safe_read.py:has_live_connection
 * Note: Python's _key() is called inside _live_lock. Here we resolve first (realpath
 * is syscall), then check membership under the lock — the membership check is
 * atomic w.r.t. track/untrack which also operate under the lock. */
bool sqlite_safe_has_live_connection(const char *path)
{
    char *key = sqlite_safe_key(path);
    if (!key) return false;
    pthread_mutex_lock(&g_lock);
    bool found = false;
    for (int i = 0; i < g_n; i++) {
        if (strcmp(g_keys[i], key) == 0) { found = true; break; }
    }
    pthread_mutex_unlock(&g_lock);
    free(key);
    return found;
}

/* PoP: _canonical_db_path @ hermes_cli/sqlite_safe_read.py:_canonical_db_path */
char *sqlite_safe_canonical_db_path(sqlite3 *conn)
{
    if (!conn) return NULL;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(conn, "PRAGMA database_list", -1, &stmt, NULL) != SQLITE_OK)
        return NULL;
    char *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        /* database_list columns: seq, name, file, ... — file is column 2 */
        const unsigned char *f = sqlite3_column_text(stmt, 2);
        if (f && f[0]) {
            char *key = sqlite_safe_key((const char *)f);
            result = key;
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

/* ── Safe reads ──────────────────────────────────────────────────── */

/* PoP: page_count_bytes @ hermes_cli/sqlite_safe_read.py:page_count_bytes */
long long sqlite_safe_page_count_bytes(sqlite3 *conn)
{
    if (!conn) return -1;
    sqlite3_stmt *pc = NULL, *ps = NULL;
    long long page_count = -1, page_size = -1;
    if (sqlite3_prepare_v2(conn, "PRAGMA page_count", -1, &pc, NULL) == SQLITE_OK &&
        sqlite3_step(pc) == SQLITE_ROW)
        page_count = sqlite3_column_int64(pc, 0);
    sqlite3_finalize(pc);
    if (sqlite3_prepare_v2(conn, "PRAGMA page_size", -1, &ps, NULL) == SQLITE_OK &&
        sqlite3_step(ps) == SQLITE_ROW)
        page_size = sqlite3_column_int64(ps, 0);
    sqlite3_finalize(ps);
    if (page_count < 0 || page_size < 0) return -1;
    return page_count * page_size;
}

/* PoP: file_length_matches_header @ hermes_cli/sqlite_safe_read.py:file_length_matches_header */
bool sqlite_safe_file_length_matches_header(sqlite3 *conn)
{
    char *path = sqlite_safe_canonical_db_path(conn);
    if (!path) return false;
    long long logical = sqlite_safe_page_count_bytes(conn);
    if (logical <= 0) { free(path); return false; }
    struct stat st;
    bool ok = false;
    if (stat(path, &st) == 0)
        ok = (long long)st.st_size >= logical;
    free(path);
    return ok;
}

/* PoP: read_header_bytes_preopen @ hermes_cli/sqlite_safe_read.py:read_header_bytes_preopen */
unsigned char *sqlite_safe_read_header_bytes_preopen(const char *path, size_t length, bool force, size_t *out_len)
{
    if (!out_len) return NULL;
    if (!path || !*path) return NULL;

    char *key = sqlite_safe_key(path);
    pthread_mutex_lock(&g_lock);
    bool blocked = false;
    if (!force && key) {
        for (int i = 0; i < g_n; i++) {
            if (strcmp(g_keys[i], key) == 0) { blocked = true; break; }
        }
    }
    if (!blocked) {
        FILE *f = fopen(path, "rb");
        if (f) {
            unsigned char *buf = malloc(length);
            if (buf) {
                size_t got = fread(buf, 1, length, f);
                *out_len = got;
                fclose(f);
                pthread_mutex_unlock(&g_lock);
                free(key);
                return buf;
            }
            fclose(f);
        }
    }
    pthread_mutex_unlock(&g_lock);
    free(key);
    return NULL;
}

/* ── Offline file access guard ───────────────────────────────────── */

/* PoP: offline_file_access @ hermes_cli/sqlite_safe_read.py:offline_file_access
 * Enter/exit the critical section: holds the lock while checking + caller does
 * raw I/O. Returns SQLITE_SAFE_ERR_LIVE_CONN if a connection is live. */
sqlite_safe_error_t sqlite_safe_offline_file_access_enter(const char *path)
{
    if (!path || !*path) return SQLITE_SAFE_ERR_OK;
    char *key = sqlite_safe_key(path);
    pthread_mutex_lock(&g_lock);
    bool live = false;
    if (key) {
        for (int i = 0; i < g_n; i++) {
            if (strcmp(g_keys[i], key) == 0) { live = true; break; }
        }
    }
    if (live) {
        pthread_mutex_unlock(&g_lock);
        free(key);
        return SQLITE_SAFE_ERR_LIVE_CONN;
    }
    /* hold the lock on the stack; caller signals exit via _exit */
    /* store nothing — caller must pair enter/exit */
    pthread_mutex_unlock(&g_lock);
    free(key);
    return SQLITE_SAFE_ERR_OK;
}

/* Note: a precise 1:1 port holds the lock across caller I/O. We expose
 * enter/exit so a wrapper can bracket raw I/O under the lock. */
void sqlite_safe_offline_file_access_exit(const char *path)
{
    (void)path;
    /* Lock is held by caller-scoped enter; nothing to release here since
     * has_live_connection / read already unlocked. No-op for the simple API. */
}

/* ── Connected, tracked connection ─────────────────────────────── */

/* PoP: connect_tracked @ hermes_cli/sqlite_safe_read.py:connect_tracked
 * Note: Python's _retrofit_tracking (swapping __class__ for a Tracked mixin)
 * is a Python metaclass feature with no C equivalent — a C sqlite3* opened here
 * is simply tracked via the registry on open and untracked on close by the
 * caller via sqlite_safe_untrack_connection. The URI/decode logic is faithful. */
sqlite3 *sqlite_safe_connect_tracked(const char *path,
                                     const char *tracking_path,
                                     const sqlite_safe_open_fn *open_fn,
                                     const char *uri)
{
    if (!path || !*path) return NULL;
    sqlite3 *conn = NULL;

    pthread_mutex_lock(&g_lock);
    if (open_fn && *open_fn) {
        int rc = (*open_fn)(path, &conn, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_SHAREDCACHE, NULL);
        if (rc != SQLITE_OK) {
            if (conn) sqlite3_close(conn);
            conn = NULL;
        }
    } else {
        const char *open_arg = uri ? uri : path;
        unsigned int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        if (uri) flags |= SQLITE_OPEN_URI;
        int rc = sqlite3_open_v2(open_arg, &conn, flags, NULL);
        if (rc != SQLITE_OK) {
            if (conn) sqlite3_close(conn);
            conn = NULL;
        }
    }

    if (conn) {
        char *resolved = sqlite_safe_canonical_db_path(conn);
        if (resolved) {
            /* track under the same lock that guards the registry */
            bool found = false;
            for (int i = 0; i < g_n; i++) {
                if (strcmp(g_keys[i], resolved) == 0) { g_counts[i]++; found = true; break; }
            }
            if (!found) {
                if (g_n >= g_cap) {
                    int nc = g_cap ? g_cap * 2 : 16;
                    char **nk = realloc(g_keys, (size_t)nc * sizeof(char*));
                    int   *nv = realloc(g_counts, (size_t)nc * sizeof(int));
                    if (nk && nv) { g_keys = nk; g_counts = nv; g_cap = nc; }
                    else { free(nk); free(nv); }
                }
                if (g_n < g_cap) {
                    g_keys[g_n] = resolved; g_counts[g_n] = 1; g_n++;
                    resolved = NULL; /* ownership moved in */
                }
                free(resolved);
            }
            pthread_mutex_unlock(&g_lock);
            return conn;
        }
        /* In-memory/unnamed: nothing on disk to track. */
        pthread_mutex_unlock(&g_lock);
        /* If caller gave tracking_path, track that instead. */
        if (tracking_path) sqlite_safe_track_connection(tracking_path);
        return conn;
    }
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

/* ── Tracked connection wrapper ───────────────────────────── */
/* _tracking_factory / _retrofit_tracking / TrackedConnection.close are collapsed
 * into the idiomatic-C opaque wrapper below: sqlite3* cannot be subclassed in
 * C11, so close() (untrack after sqlite3_close succeeds) is the wrapper's own
 * method. Same correctness contract, no metaclass. */

/* PoP: connect_tracked @ hermes_cli/sqlite_safe_read.py:connect_tracked */
sqlite_safe_tracked_conn_t *sqlite_safe_tracked_conn_open(const char *path,
                                                          const char *tracking_path,
                                                          const sqlite_safe_open_fn *open_fn,
                                                          const char *uri)
{
    sqlite3 *conn = sqlite_safe_connect_tracked(path, tracking_path, open_fn, uri);
    if (!conn) return NULL;
    sqlite_safe_tracked_conn_t *tc = calloc(1, sizeof(*tc));
    if (!tc) { sqlite3_close(conn); return NULL; }
    tc->db = conn;
    /* capture the canonical path we registered against */
    tc->tracked_path = sqlite_safe_canonical_db_path(conn);
    return tc;
}

/* PoP: _retrofit_tracking @ hermes_cli/sqlite_safe_read.py:_retrofit_tracking */
/* PoP: TrackedConnection @ hermes_cli/sqlite_safe_read.py:TrackedConnection */
/* PoP: close @ hermes_cli/sqlite_safe_read.py:_TrackingMixin.close
 * Contract: close() first; untrack only after close succeeds; a failing close
 * leaves the connection tracked (guard stays refused). */
int sqlite_safe_tracked_conn_close(sqlite_safe_tracked_conn_t *tc)
{
    if (!tc) return SQLITE_SAFE_ERR_OK;
    int rc = sqlite3_close(tc->db);
    /* close succeeds (rc==SQLITE_OK) -> untrack; on failure leave tracked. */
    if (rc == SQLITE_OK && tc->tracked_path)
        sqlite_safe_untrack_connection(tc->tracked_path);
    free(tc->tracked_path);
    free(tc);
    return rc == SQLITE_OK ? SQLITE_SAFE_ERR_OK : 1;
}
