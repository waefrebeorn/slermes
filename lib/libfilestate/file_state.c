/*
 * file_state.c — Cross-agent file state coordination implementation.
 *
 * Thread-safe registry tracking per-agent read stamps and global writers.
 * Prevents concurrent subagents from overwriting each other's changes.
 */

#include "file_state.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ─── Internal state ─────────────────────────────────────── */

/* Per-agent read stamps: agents live in a hive (dynamic, no landlocked
 * array); each agent's path list is a growable heap array. */
typedef struct {
    char  task_id[64];
    int   count, cap;
    char  (*paths)[256];
    fs_read_stamp_t *stamps;
} fs_agent_t;

#include "hive.h"
static hive_t *g_agents = NULL;       /* of fs_agent_t* */
static int g_agent_count = 0;

/* Global last-writer map: dynamic growable arrays (no landlocked static). */
typedef struct {
    int     count, cap;
    char    (*paths)[256];
    char    (*task_ids)[64];
    double  *timestamps;
} fs_writers_t;

static fs_writers_t g_writers = { 0, 0, NULL, NULL, NULL };

/* Per-path locks: dynamic growable arrays. */
typedef struct {
    int             count, cap;
    char            (*paths)[256];
    pthread_mutex_t *mutexes;
} fs_path_locks_t;

static fs_path_locks_t g_path_locks = { 0, 0, NULL, NULL };

static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_meta_lock  = PTHREAD_MUTEX_INITIALIZER;
static bool g_initialized = false;

/* ─── Helpers ────────────────────────────────────────────── */

static fs_agent_t *find_agent(const char *task_id) {
    if (!g_agents) return NULL;
    hive_iter_t it;
    hive_iter_begin(g_agents, &it);
    fs_agent_t *a;
    while (hive_iter_next(g_agents, &it, NULL, (void **)&a))
        if (strcmp(a->task_id, task_id) == 0) return a;
    return NULL;
}

static fs_agent_t *add_agent(const char *task_id) {
    if (!g_agents) g_agents = hive_new(4);
    fs_agent_t *a = calloc(1, sizeof(fs_agent_t));
    if (!a) return NULL;
    strncpy(a->task_id, task_id, sizeof(a->task_id) - 1);
    a->task_id[sizeof(a->task_id) - 1] = '\0';
    a->cap = 8;
    a->paths  = malloc((size_t)a->cap * 256);
    a->stamps = calloc((size_t)a->cap, sizeof(fs_read_stamp_t));
    if (!a->paths || !a->stamps) {
        free(a->paths); free(a->stamps); free(a);
        return NULL;
    }
    bool ok = false;
    hive_insert(g_agents, a, &ok);
    if (!ok) { free(a->paths); free(a->stamps); free(a); return NULL; }
    g_agent_count++;
    return a;
}

static int find_agent_path(fs_agent_t *a, const char *path) {
    for (int i = 0; i < a->count; i++)
        if (strcmp(a->paths[i], path) == 0) return i;
    return -1;
}

static void agent_grow(fs_agent_t *a) {
    if (a->count < a->cap) return;
    int ncap = a->cap * 2;
    char (*np)[256] = realloc(a->paths, (size_t)ncap * 256);
    fs_read_stamp_t *ns = realloc(a->stamps, (size_t)ncap * sizeof(fs_read_stamp_t));
    if (!np || !ns) { free(np); free(ns); return; }  /* keep old on OOM */
    a->paths = np;
    a->stamps = ns;
    a->cap = ncap;
}

static int agent_touch_path(fs_agent_t *a, const char *path) {
    int pidx = find_agent_path(a, path);
    if (pidx < 0) {
        agent_grow(a);
        if (a->count >= a->cap) return -1;   /* OOM: drop this read */
        pidx = a->count;
        strncpy(a->paths[pidx], path, sizeof(a->paths[pidx]) - 1);
        a->paths[pidx][sizeof(a->paths[pidx]) - 1] = '\0';
        a->count++;
    }
    return pidx;
}

static int find_writer(const char *path) {
    for (int i = 0; i < g_writers.count; i++)
        if (strcmp(g_writers.paths[i], path) == 0) return i;
    return -1;
}

static void writers_grow(void) {
    if (g_writers.count < g_writers.cap) return;
    int ncap = g_writers.cap ? g_writers.cap * 2 : 8;
    char (*np)[256] = realloc(g_writers.paths, (size_t)ncap * 256);
    char (*nt)[64]  = realloc(g_writers.task_ids, (size_t)ncap * 64);
    double *nts     = realloc(g_writers.timestamps, (size_t)ncap * sizeof(double));
    if (!np || !nt || !nts) { free(np); free(nt); free(nts); return; }
    g_writers.paths = np;
    g_writers.task_ids = nt;
    g_writers.timestamps = nts;
    g_writers.cap = ncap;
}

static int writer_touch(const char *path, const char *task_id, double ts) {
    int widx = find_writer(path);
    if (widx < 0) {
        writers_grow();
        if (g_writers.count >= g_writers.cap) return -1;
        widx = g_writers.count++;
        strncpy(g_writers.paths[widx], path, sizeof(g_writers.paths[widx]) - 1);
        g_writers.paths[widx][sizeof(g_writers.paths[widx]) - 1] = '\0';
    }
    strncpy(g_writers.task_ids[widx], task_id, sizeof(g_writers.task_ids[widx]) - 1);
    g_writers.task_ids[widx][sizeof(g_writers.task_ids[widx]) - 1] = '\0';
    g_writers.timestamps[widx] = ts;
    return widx;
}

static int find_path_lock(const char *path) {
    for (int i = 0; i < g_path_locks.count; i++)
        if (strcmp(g_path_locks.paths[i], path) == 0) return i;
    return -1;
}

static void path_locks_grow(void) {
    if (g_path_locks.count < g_path_locks.cap) return;
    int ncap = g_path_locks.cap ? g_path_locks.cap * 2 : 8;
    char (*np)[256] = realloc(g_path_locks.paths, (size_t)ncap * 256);
    pthread_mutex_t *nm = realloc(g_path_locks.mutexes,
                                  (size_t)ncap * sizeof(pthread_mutex_t));
    if (!np || !nm) { free(np); free(nm); return; }
    g_path_locks.paths = np;
    g_path_locks.mutexes = nm;
    g_path_locks.cap = ncap;
}

static bool get_mtime(const char *path, double *out) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    *out = (double)st.st_mtime;
    return true;
}

static double now_ts(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ─── Initialization ─────────────────────────────────────── */

void fs_init(void) {
    if (g_initialized) return;
    g_initialized = true;
    g_agent_count = 0;
    g_writers.count = 0;
    g_path_locks.count = 0;
}

/* ─── Public API ─────────────────────────────────────────── */

void fs_record_read(const char *task_id, const char *path, bool partial, double mtime) {
    if (fs_is_disabled() || !task_id || !path || !*path) return;

    if (mtime == 0.0) {
        if (!get_mtime(path, &mtime)) return;
    }
    double ts = now_ts();

    pthread_mutex_lock(&g_state_lock);

    fs_agent_t *a = find_agent(task_id);
    if (!a) a = add_agent(task_id);
    if (!a) { pthread_mutex_unlock(&g_state_lock); return; }

    int pidx = agent_touch_path(a, path);
    if (pidx < 0) { pthread_mutex_unlock(&g_state_lock); return; }

    a->stamps[pidx].mtime   = mtime;
    a->stamps[pidx].read_ts = ts;
    a->stamps[pidx].partial = partial;

    pthread_mutex_unlock(&g_state_lock);
}

void fs_note_write(const char *task_id, const char *path, double mtime) {
    if (fs_is_disabled() || !task_id || !path || !*path) return;

    if (mtime == 0.0) {
        if (!get_mtime(path, &mtime)) return;
    }
    double ts = now_ts();

    pthread_mutex_lock(&g_state_lock);

    /* Update global last-writer */
    writer_touch(path, task_id, ts);

    /* Writer's own read stamp is now up-to-date */
    fs_agent_t *a = find_agent(task_id);
    if (!a) a = add_agent(task_id);
    if (a) {
        int pidx = agent_touch_path(a, path);
        if (pidx >= 0) {
            a->stamps[pidx].mtime   = mtime;
            a->stamps[pidx].read_ts = ts;
            a->stamps[pidx].partial = false;
        }
    }

    pthread_mutex_unlock(&g_state_lock);
}

char *fs_check_stale(const char *task_id, const char *path) {
    if (fs_is_disabled() || !task_id || !path || !*path) return NULL;

    pthread_mutex_lock(&g_state_lock);

    fs_agent_t *a = find_agent(task_id);
    int pidx = a ? find_agent_path(a, path) : -1;
    int widx = find_writer(path);

    /* Case: never read AND no writer record — net-new file */
    if (pidx < 0 && widx < 0) {
        pthread_mutex_unlock(&g_state_lock);
        return NULL;
    }

    pthread_mutex_unlock(&g_state_lock);

    /* Get current mtime */
    double current_mtime;
    if (!get_mtime(path, &current_mtime)) {
        /* File doesn't exist — write will create it */
        return NULL;
    }

    pthread_mutex_lock(&g_state_lock);

    /* Re-read after stat (state may have changed) */
    a = find_agent(task_id);
    pidx = a ? find_agent_path(a, path) : -1;
    widx = find_writer(path);

    char *result = NULL;

    /* Case 1: sibling subagent wrote after our last read */
    if (widx >= 0 && pidx >= 0) {
        const char *writer_tid = g_writers.task_ids[widx];
        double writer_ts = g_writers.timestamps[widx];
        if (strcmp(writer_tid, task_id) != 0) {
            double read_ts = a->stamps[pidx].read_ts;
            if (writer_ts > read_ts) {
                char buf[512];
                snprintf(buf, sizeof(buf),
                    "%s was modified by sibling subagent %s — after "
                    "this agent's last read. Re-read before writing.",
                    path, writer_tid);
                result = strdup(buf);
                pthread_mutex_unlock(&g_state_lock);
                return result;
            }
        }
    }

    /* Case 1b: never read but writer exists */
    if (widx >= 0 && pidx < 0) {
        const char *writer_tid = g_writers.task_ids[widx];
        if (strcmp(writer_tid, task_id) != 0) {
            char buf[512];
            snprintf(buf, sizeof(buf),
                "%s was modified by sibling subagent %s but this agent "
                "never read it. Read before writing to avoid overwriting.",
                path, writer_tid);
            result = strdup(buf);
            pthread_mutex_unlock(&g_state_lock);
            return result;
        }
    }

    /* Case 2: external/unknown modification (mtime drifted) */
    if (pidx >= 0) {
        double read_mtime = a->stamps[pidx].mtime;
        bool partial = a->stamps[pidx].partial;
        if (current_mtime != read_mtime) {
            result = strdup(
                "File was modified on disk since you last read it. "
                "Re-read before writing.");
            pthread_mutex_unlock(&g_state_lock);
            return result;
        }
        if (partial) {
            result = strdup(
                "File was last read with offset/limit pagination "
                "(partial view). Re-read the whole file before overwriting.");
            pthread_mutex_unlock(&g_state_lock);
            return result;
        }
    }

    /* Case 3: never read */
    if (pidx < 0) {
        result = strdup(
            "File was not read by this agent. "
            "Read the file first to write an informed edit.");
        pthread_mutex_unlock(&g_state_lock);
        return result;
    }

    pthread_mutex_unlock(&g_state_lock);
    return NULL;  /* safe */
}

void fs_lock_path(const char *path) {
    if (!path || !*path) return;

    pthread_mutex_lock(&g_meta_lock);

    int idx = find_path_lock(path);
    if (idx < 0) {
        path_locks_grow();
        if (g_path_locks.count >= g_path_locks.cap) {
            pthread_mutex_unlock(&g_meta_lock);
            return;   /* OOM: skip locking this path */
        }
        idx = g_path_locks.count++;
        strncpy(g_path_locks.paths[idx], path, sizeof(g_path_locks.paths[idx]) - 1);
        g_path_locks.paths[idx][sizeof(g_path_locks.paths[idx]) - 1] = '\0';
        pthread_mutex_init(&g_path_locks.mutexes[idx], NULL);
    }

    pthread_mutex_t *lock = &g_path_locks.mutexes[idx];
    pthread_mutex_unlock(&g_meta_lock);

    pthread_mutex_lock(lock);
}

void fs_unlock_path(const char *path) {
    if (!path || !*path) return;

    pthread_mutex_lock(&g_meta_lock);
    int idx = find_path_lock(path);
    if (idx >= 0) {
        pthread_mutex_unlock(&g_path_locks.mutexes[idx]);
    }
    pthread_mutex_unlock(&g_meta_lock);
}

bool fs_writes_since(const char *exclude_task_id, double since_ts,
                     const char **path_list, int path_count,
                     fs_writes_result_t *out) {
    if (fs_is_disabled() || !out) return false;
    memset(out, 0, sizeof(*out));
    out->path_count = 0;

    pthread_mutex_lock(&g_state_lock);

    for (int w = 0; w < g_writers.count; w++) {
        if (strcmp(g_writers.task_ids[w], exclude_task_id) == 0)
            continue;
        if (g_writers.timestamps[w] < since_ts)
            continue;

        /* Check if path is in the caller's path list */
        bool found = false;
        for (int p = 0; p < path_count; p++) {
            if (path_list[p] && strcmp(g_writers.paths[w], path_list[p]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) continue;

        /* Add to result */
        if (out->path_count >= FS_MAX_GLOBAL_WRITERS) break;
        strncpy(out->writer_task_id, g_writers.task_ids[w],
                sizeof(out->writer_task_id) - 1);
        strncpy(out->paths[out->path_count], g_writers.paths[w],
                sizeof(out->paths[out->path_count]) - 1);
        out->path_count++;
    }

    pthread_mutex_unlock(&g_state_lock);
    return true;
}

void fs_known_reads(const char *task_id, char out[][256], int *out_count) {
    if (fs_is_disabled() || !out || !out_count) return;
    *out_count = 0;

    pthread_mutex_lock(&g_state_lock);

    fs_agent_t *a = find_agent(task_id);
    if (a) {
        for (int i = 0; i < a->count; i++) {
            strncpy(out[i], a->paths[i], 255);
            out[i][255] = '\0';
            (*out_count)++;
        }
    }

    pthread_mutex_unlock(&g_state_lock);
}

void fs_clear(void) {
    pthread_mutex_lock(&g_state_lock);
    if (g_agents) {
        hive_iter_t it;
        hive_iter_begin(g_agents, &it);
        hive_handle_t hnd;
        fs_agent_t *a;
        while (hive_iter_next(g_agents, &it, &hnd, (void **)&a)) {
            free(a->paths);
            free(a->stamps);
            free(a);
            hive_erase(g_agents, hnd);
        }
    }
    g_agent_count = 0;
    g_writers.count = 0;
    pthread_mutex_unlock(&g_state_lock);

    pthread_mutex_lock(&g_meta_lock);
    for (int i = 0; i < g_path_locks.count; i++)
        pthread_mutex_destroy(&g_path_locks.mutexes[i]);
    g_path_locks.count = 0;
    pthread_mutex_unlock(&g_meta_lock);
}

bool fs_is_disabled(void) {
    const char *val = getenv("HERMES_DISABLE_FILE_STATE_GUARD");
    return (val && strcmp(val, "1") == 0);
}
