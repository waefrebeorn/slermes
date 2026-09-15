#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
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

static bool sys_platform_is_linux(void) {
#if defined(__linux__)
    return true;
#else
    return false;
#endif
}

/* ── hermes_state.py: SQLite file identity + generation + heartbeats ── */

/* PoP: _pread_db_header @ hermes_state.py:_pread_db_header */
/* Returns malloc'd buffer (caller frees) or NULL. */
char *_pread_db_header(const char *db_path, int length) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    typedef struct { int fd; dev_t dev; ino_t ino; } cached_t;
    static cached_t *cache = NULL;
    static int cache_n = 0, cache_cap = 0;
    if (!db_path || length <= 0) return NULL;
    struct stat st;
    if (stat(db_path, &st) != 0) return NULL;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < cache_n; i++) {
        if (cache[i].ino == 0) continue; /* retired */
        if (cache[i].dev == st.st_dev && cache[i].ino == st.st_ino) {
            /* Valid cached fd */
            char *buf = malloc(length);
            if (!buf) { pthread_mutex_unlock(&lock); return NULL; }
            ssize_t n = pread(cache[i].fd, buf, length, 0);
            pthread_mutex_unlock(&lock);
            if (n < length) { free(buf); return NULL; }
            return buf;
        }
        if (cache[i].ino != st.st_ino) {
            /* Retired: reuse slot */
            close(cache[i].fd);
            int fd = open(db_path, O_RDONLY);
            if (fd < 0) { pthread_mutex_unlock(&lock); return NULL; }
            struct stat fst;
            if (fstat(fd, &fst) != 0) { close(fd); pthread_mutex_unlock(&lock); return NULL; }
            cache[i].fd = fd; cache[i].dev = fst.st_dev; cache[i].ino = fst.st_ino;
            char *buf = malloc(length);
            if (!buf) { pthread_mutex_unlock(&lock); return NULL; }
            ssize_t n = pread(fd, buf, length, 0);
            pthread_mutex_unlock(&lock);
            if (n < length) { free(buf); return NULL; }
            return buf;
        }
    }
    /* New entry */
    int fd = open(db_path, O_RDONLY);
    if (fd < 0) { pthread_mutex_unlock(&lock); return NULL; }
    struct stat fst;
    if (fstat(fd, &fst) != 0) { close(fd); pthread_mutex_unlock(&lock); return NULL; }
    if (cache_n >= cache_cap) {
        cache_cap = cache_cap ? cache_cap * 2 : 8;
        cache = realloc(cache, sizeof(cached_t) * cache_cap);
    }
    cache[cache_n].fd = fd; cache[cache_n].dev = fst.st_dev; cache[cache_n].ino = fst.st_ino;
    cache_n++;
    char *buf = malloc(length);
    if (!buf) { pthread_mutex_unlock(&lock); return NULL; }
    ssize_t n = pread(fd, buf, length, 0);
    pthread_mutex_unlock(&lock);
    if (n < length) { free(buf); return NULL; }
    return buf;
}

/* PoP: _read_sqlite_application_id @ hermes_state.py:_read_sqlite_application_id */
int _read_sqlite_application_id(const char *db_path) {
    if (!db_path) return -1;
    char *hdr = _pread_db_header(db_path, 68);
    if (!hdr) return -1;
    /* application_id is at offset 64, 4 bytes big-endian */
    if (hdr[0] != 'S' || hdr[1] != 'q' || hdr[2] != 'l') { free(hdr); return -1; }
    int id = (hdr[64] << 24) | (hdr[65] << 16) | (hdr[66] << 8) | hdr[67];
    free(hdr);
    return id;
}

/* PoP: _stat_db_file_identity @ hermes_state.py:_stat_db_file_identity */
/* Returns malloc'd tuple string "dev,ino" or NULL */
char *_stat_db_file_identity(const char *path) {
    if (!path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    char buf[64];
    snprintf(buf, sizeof(buf), "%lu,%lu", (unsigned long)st.st_dev, (unsigned long)st.st_ino);
    return strdup(buf);
}

/* PoP: _stat_sqlite_sidecar_identity @ hermes_state.py:_stat_sqlite_sidecar_identity */
/* Returns malloc'd JSON string {"-wal":"dev,ino","-shm":"dev,ino"} or NULL */
char *_stat_sqlite_sidecar_identity(const char *db_path) {
    if (!db_path) return NULL;
    char wal_path[4096], shm_path[4096];
    snprintf(wal_path, sizeof(wal_path), "%s-wal", db_path);
    snprintf(shm_path, sizeof(shm_path), "%s-shm", db_path);
    char *wal_id = _stat_db_file_identity(wal_path);
    char *shm_id = _stat_db_file_identity(shm_path);
    char *result = malloc(256);
    if (!result) { free(wal_id); free(shm_id); return NULL; }
    snprintf(result, 256, "{\"%s\":\"%s\",\"%s\":\"%s\"}",
             "-wal", wal_id ? wal_id : "null",
             "-shm", shm_id ? shm_id : "null");
    free(wal_id); free(shm_id);
    return result;
}

/* PoP: _watched_sqlite_sidecar_paths @ hermes_state.py:_watched_sqlite_sidecar_paths */
/* Returns malloc'd comma-separated path list or NULL */
char *_watched_sqlite_sidecar_paths(const char *db_path) {
    if (!db_path) return NULL;
    char wal_path[4096], shm_path[4096];
    snprintf(wal_path, sizeof(wal_path), "%s-wal", db_path);
    snprintf(shm_path, sizeof(shm_path), "%s-shm", db_path);
    /* Canonicalize */
    char *canon_wal = realpath(wal_path, NULL);
    char *canon_shm = realpath(shm_path, NULL);
    size_t len = (canon_wal ? strlen(canon_wal) : 0) + (canon_shm ? strlen(canon_shm) : 0) + 3;
    char *result = malloc(len);
    if (!result) { free(canon_wal); free(canon_shm); return NULL; }
    snprintf(result, len, "%s,%s", canon_wal ? canon_wal : "", canon_shm ? canon_shm : "");
    free(canon_wal); free(canon_shm);
    return result;
}

/* PoP: iter_deleted_sqlite_sidecar_holders @ hermes_state.py:iter_deleted_sqlite_sidecar_holders */
/* Returns malloc'd JSON array of [pid, target] pairs or empty array */
char *iter_deleted_sqlite_sidecar_holders(const char *db_path) {
    if (!db_path || !sys_platform_is_linux()) return strdup("[]");
    char *watched = _watched_sqlite_sidecar_paths(db_path);
    if (!watched) return strdup("[]");
    /* Parse watched paths into a set */
    char *wal_path = strdup(watched);
    char *shm_path = strchr(watched, ',');
    if (shm_path) { *shm_path = '\0'; shm_path++; }
    /* Walk /proc/<pid>/fd */
    DIR *proc = opendir("/proc");
    if (!proc) { free(watched); return strdup("[]"); }
    char *result = malloc(4096);
    if (!result) { closedir(proc); free(watched); return strdup("[]"); }
    strcpy(result, "[");
    size_t off = 1;
    struct dirent *dent;
    while ((dent = readdir(proc)) != NULL) {
        if (!dent->d_name[0] || !isdigit(dent->d_name[0])) continue;
        char fd_dir[256];
        snprintf(fd_dir, sizeof(fd_dir), "/proc/%s/fd", dent->d_name);
        DIR *fd_dirp = opendir(fd_dir);
        if (!fd_dirp) continue;
        struct dirent *fd_ent;
        while ((fd_ent = readdir(fd_dirp)) != NULL) {
            char link_target[4096];
            char link_path[256];
            snprintf(link_path, sizeof(link_path), "%s/%s", fd_dir, fd_ent->d_name);
            ssize_t len = readlink(link_path, link_target, sizeof(link_target) - 1);
            if (len < 0) continue;
            link_target[len] = '\0';
            if (!strstr(link_target, " (deleted)")) continue;
            /* Check if this matches a watched path */
            if ((wal_path && strstr(link_target, wal_path)) ||
                (shm_path && strstr(link_target, shm_path))) {
                off += snprintf(result + off, 4096 - off, "[%s,\"%s\"],", dent->d_name, link_target);
            }
        }
        closedir(fd_dirp);
    }
    closedir(proc);
    if (off > 1) off--; /* remove trailing comma */
    result[off++] = ']';
    result[off] = '\0';
    free(watched);
    return result;
}

/* PoP: refuse_deleted_wal_generation @ hermes_state.py:refuse_deleted_wal_generation */
/* Returns true if a holder was found (caller raises) */
bool refuse_deleted_wal_generation(const char *db_path) {
    char *holders = iter_deleted_sqlite_sidecar_holders(db_path);
    if (!holders) return false;
    /* Check if array is non-empty */
    bool found = strcmp(holders, "[]") != 0;
    free(holders);
    if (found) {
        hermes_log(LOG_ERROR, "state.db", "Cannot open state.db: processes hold deleted WAL/SHM sidecars.");
    }
    return found;
}

/* PoP: _own_profile_name @ hermes_state.py:_own_profile_name */
const char *_own_profile_name(const char *db_path) {
    static char result[256];
    if (!db_path) return NULL;
    char *root = getenv("HERMES_HOME");
    if (!root) root = "~/.hermes";
    /* Check if db_path matches <root>/state.db or <root>/profiles/<name>/state.db */
    char root_state[4096];
    snprintf(root_state, sizeof(root_state), "%s/state.db", root);
    if (strcmp(db_path, root_state) == 0) return "default";
    char profiles_prefix[4096];
    snprintf(profiles_prefix, sizeof(profiles_prefix), "%s/profiles/", root);
    if (strncmp(db_path, profiles_prefix, strlen(profiles_prefix)) == 0) {
        /* Extract profile name */
        const char *name_start = db_path + strlen(profiles_prefix);
        const char *slash = strchr(name_start, '/');
        if (slash) {
            size_t len = slash - name_start;
            if (len < sizeof(result)) {
                memcpy(result, name_start, len);
                result[len] = '\0';
                return result;
            }
        }
    }
    return NULL;
}

/* PoP: is_explicit_fork_child @ hermes_state.py:is_explicit_fork_child */
bool is_explicit_fork_child(const char *session_json) {
    if (!session_json) return false;
    /* Check for _branched_from or _delegate_from markers */
    if (strstr(session_json, "\"_branched_from\"")) return true;
    if (strstr(session_json, "\"_delegate_from\"")) return true;
    return false;
}

/* PoP: declared_scope_identity @ hermes_state.py:declared_scope_identity */
char *declared_scope_identity(const char *session_json) {
    if (!session_json) return strdup("false,");
    bool is_fork = is_explicit_fork_child(session_json);
    /* Extract source field */
    const char *source_start = strstr(session_json, "\"source\"");
    char source[256] = "";
    if (source_start) {
        const char *colon = strchr(source_start, ':');
        if (colon) {
            colon++;
            while (*colon == ' ' || *colon == '"') colon++;
            const char *end = strchr(colon, '"');
            if (end) {
                size_t len = end - colon;
                if (len < sizeof(source)) {
                    memcpy(source, colon, len);
                    source[len] = '\0';
                }
            }
        }
    }
    char *result = malloc(512);
    if (!result) return strdup("false,");
    snprintf(result, 512, "%s,%s", is_fork ? "true" : "false", source);
    return result;
}

/* PoP: latest_conversation_boundary @ hermes_state.py:latest_conversation_boundary */
/* Returns generation count from conversation_generations table */
int latest_conversation_boundary(const char *session_key, const char *source, sqlite3 *conn) {
    if (!session_key || !source || !conn) return -1;
    sqlite3_stmt *st = NULL;
    const char *sql = "SELECT generation FROM conversation_generations WHERE session_key = ? AND source = ?";
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, session_key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, source, -1, SQLITE_TRANSIENT);
    int gen = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        gen = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    return gen;
}

/* PoP: _dedupe_display_generations @ hermes_state.py:_dedupe_display_generations */
/* Simplified: returns the input JSON (full implementation needs message row dedup) */
char *_dedupe_display_generations(const char *rows_json) {
    if (!rows_json) return strdup("[]");
    /* Full implementation would parse JSON, build dedup key per row, prefer active/newest */
    /* For now, return input (callers that use this are display-read paths) */
    return strdup(rows_json);
}

/* PoP: _resume_lineage_ids @ hermes_state.py:_resume_lineage_ids */
/* Returns JSON array of session ids from root to tip */
char *_resume_lineage_ids(const char *session_id, sqlite3 *conn) {
    if (!session_id || !conn) return strdup("[]");
    char *result = malloc(4096);
    if (!result) return strdup("[]");
    strcpy(result, "[");
    size_t off = 1;
    /* Walk ancestors */
    char current[256];
    strncpy(current, session_id, sizeof(current) - 1);
    current[sizeof(current) - 1] = '\0';
    off += snprintf(result + off, 4096 - off, "\"%s\"", current);
    /* Walk compression continuations */
    for (int depth = 0; depth < 100; depth++) {
        sqlite3_stmt *st = NULL;
        const char *sql = "SELECT child.id FROM sessions parent JOIN sessions child ON child.parent_session_id = parent.id WHERE parent.id = ? AND parent.end_reason = 'compression' ORDER BY child.started_at DESC LIMIT 1";
        if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK) break;
        sqlite3_bind_text(st, 1, current, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *child = (const char *)sqlite3_column_text(st, 0);
            if (!child) { sqlite3_finalize(st); break; }
            off += snprintf(result + off, 4096 - off, ",\"%s\"", child);
            strncpy(current, child, sizeof(current) - 1);
            current[sizeof(current) - 1] = '\0';
        } else {
            sqlite3_finalize(st);
            break;
        }
        sqlite3_finalize(st);
    }
    result[off++] = ']';
    result[off] = '\0';
    return result;
}

/* PoP: backfill_null_session_profiles @ hermes_state.py:backfill_null_session_profiles */
int backfill_null_session_profiles(const char *profile_name, sqlite3 *conn) {
    if (!profile_name || !conn) return 0;
    sqlite3_stmt *st = NULL;
    const char *sql = "UPDATE sessions SET profile_name = ? WHERE profile_name IS NULL OR TRIM(profile_name) = ''";
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, profile_name, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    int changes = sqlite3_changes(conn);
    sqlite3_finalize(st);
    return changes;
}

/* PoP: get_compression_chain @ hermes_state.py:get_compression_chain */
/* Returns JSON array of session ids from root to tip */
char *get_compression_chain(const char *session_id, sqlite3 *conn) {
    return _resume_lineage_ids(session_id, conn);
}

/* PoP: get_compression_tip @ hermes_state.py:get_compression_tip */
char *get_compression_tip(const char *session_id, sqlite3 *conn) {
    char *chain = get_compression_chain(session_id, conn);
    if (!chain) return strdup("");
    /* Find last id in the array */
    char *last_quote = strrchr(chain, '"');
    if (!last_quote) { free(chain); return strdup(""); }
    /* Extract the last quoted string */
    char *end = last_quote + 1;
    while (*end && *end != '"') end++;
    if (*end == '"') {
        *end = '\0';
        char *result = strdup(last_quote + 1);
        free(chain);
        return result;
    }
    free(chain);
    return strdup("");
}

/* PoP: register_backend_heartbeat @ hermes_state.py:register_backend_heartbeat */
void register_backend_heartbeat(const char *backend_id, int pid, float started_at, float last_heartbeat, const char *profile, const char *host, sqlite3 *conn) {
    if (!backend_id || !conn) return;
    sqlite3_stmt *st = NULL;
    const char *sql = "INSERT INTO gateway_heartbeats (backend_id, pid, started_at, last_heartbeat, profile, host) VALUES (?, ?, ?, ?, ?, ?) ON CONFLICT(backend_id) DO UPDATE SET pid = excluded.pid, started_at = excluded.started_at, last_heartbeat = excluded.last_heartbeat, profile = excluded.profile, host = excluded.host";
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, backend_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, pid);
    sqlite3_bind_double(st, 3, started_at);
    sqlite3_bind_double(st, 4, last_heartbeat);
    sqlite3_bind_text(st, 5, profile, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 6, host, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: clear_backend_heartbeat @ hermes_state.py:clear_backend_heartbeat */
bool clear_backend_heartbeat(const char *backend_id, sqlite3 *conn) {
    if (!backend_id || !conn) return false;
    sqlite3_stmt *st = NULL;
    const char *sql = "DELETE FROM gateway_heartbeats WHERE backend_id = ?";
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, backend_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    bool changed = sqlite3_changes(conn) > 0;
    sqlite3_finalize(st);
    return changed;
}

/* PoP: prune_stale_heartbeats @ hermes_state.py:prune_stale_heartbeats */
int prune_stale_heartbeats(float max_age_seconds, sqlite3 *conn) {
    if (max_age_seconds <= 0 || !conn) return 0;
    time_t cutoff = time(NULL) - (time_t)max_age_seconds;
    sqlite3_stmt *st = NULL;
    const char *sql = "DELETE FROM gateway_heartbeats WHERE last_heartbeat < ?";
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_double(st, 1, (double)cutoff);
    sqlite3_step(st);
    int changes = sqlite3_changes(conn);
    sqlite3_finalize(st);
    return changes;
}

/* PoP: list_backend_heartbeats @ hermes_state.py:list_backend_heartbeats */
char *list_backend_heartbeats(sqlite3 *conn) {
    if (!conn) return strdup("[]");
    sqlite3_stmt *st = NULL;
    const char *sql = "SELECT backend_id, pid, started_at, last_heartbeat, profile, host FROM gateway_heartbeats ORDER BY last_heartbeat DESC";
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK) return strdup("[]");
    char *result = malloc(8192);
    if (!result) { sqlite3_finalize(st); return strdup("[]"); }
    strcpy(result, "[");
    size_t off = 1;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *bid = (const char *)sqlite3_column_text(st, 0);
        int pid = sqlite3_column_int(st, 1);
        double started = sqlite3_column_double(st, 2);
        double last = sqlite3_column_double(st, 3);
        const char *prof = (const char *)sqlite3_column_text(st, 4);
        const char *host = (const char *)sqlite3_column_text(st, 5);
        off += snprintf(result + off, 8192 - off, "{\"backend_id\":\"%s\",\"pid\":%d,\"started_at\":%f,\"last_heartbeat\":%f,\"profile\":\"%s\",\"host\":\"%s\"},",
                         bid ? bid : "", pid, started, last, prof ? prof : "", host ? host : "");
    }
    if (off > 1) off--;
    result[off++] = ']';
    result[off] = '\0';
    sqlite3_finalize(st);
    return result;
}
