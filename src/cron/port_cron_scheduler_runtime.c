/*
 * port_cron_scheduler_runtime.c — faithful C11 port of cron/scheduler.py's
 * runtime bookkeeping: the interrupted/running job-id sets, the
 * writer-preferring readers-writer lock guarding TERMINAL_CWD, the
 * persistent parallel/sequential thread pools, interpreter-shutdown
 * detection, cron lock paths, plugin env registry, and the robust cron
 * session-title write.
 *
 * Opaque structs + minimal includes; pthreads only (C11 threads are not
 * used elsewhere in the tree). No god headers.
 */

#include "cron_scheduler_runtime.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Reused plumbing (subsystem headers, no god header). */
#include "cron_jobs.h"          /* cronjobs_mark_job_run, cronjobs_cron_dir */
#include "session_title.h"      /* session_title_set / next_in_lineage */

/* ================================================================
 * Session titling
 * ================================================================ */

/* PoP: scheduler_set_cron_session_title @ cron/scheduler.py:_set_cron_session_title */
char *scheduler_set_cron_session_title(db_t *db, const char *session_id,
                                       const char *base_title,
                                       bool *conflict_out)
{
    if (conflict_out) *conflict_out = false;
    if (!db || !session_id || !session_id[0]) return NULL;

    /* title = (base_title or "").strip() */
    const char *s = base_title ? base_title : "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\n' || s[len-1] == '\r')) len--;
    if (len == 0) return NULL;

    char *title = malloc(len + 1);
    if (!title) return NULL;
    memcpy(title, s, len);
    title[len] = '\0';

    session_title_result_t rc = session_title_set(db, session_id, title);
    if (rc == SESSION_TITLE_OK) return title;

    if (rc == SESSION_TITLE_CONFLICT) {
        /* Unique-title collision: fall back to the next title in the
         * lineage ("base #2", "base #3", ...). If dedup yields nothing
         * new, surface the conflict (Python re-raises ValueError). */
        char *deduped = session_title_next_in_lineage(db, title);
        if (!deduped || strcmp(deduped, title) == 0) {
            free(deduped);
            free(title);
            if (conflict_out) *conflict_out = true;
            return NULL;
        }
        rc = session_title_set(db, session_id, deduped);
        free(title);
        if (rc == SESSION_TITLE_OK) return deduped;
        free(deduped);
        if (conflict_out) *conflict_out = true;
        return NULL;
    }

    free(title);
    return NULL;
}

/* ================================================================
 * Running / interrupted job-id sets
 * ================================================================ */

#define MAX_TRACKED_JOBS 256

static pthread_mutex_t g_running_lock = PTHREAD_MUTEX_INITIALIZER;
static char *g_running_ids[MAX_TRACKED_JOBS];
static size_t g_running_n = 0;
static char *g_interrupted_ids[MAX_TRACKED_JOBS];
static size_t g_interrupted_n = 0;

static bool idset_contains(char **set, size_t n, const char *id)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(set[i], id) == 0) return true;
    return false;
}

static void idset_add(char **set, size_t *n, const char *id)
{
    if (*n >= MAX_TRACKED_JOBS || idset_contains(set, *n, id)) return;
    set[*n] = strdup(id);
    if (set[*n]) (*n)++;
}

static void idset_remove(char **set, size_t *n, const char *id)
{
    for (size_t i = 0; i < *n; i++) {
        if (strcmp(set[i], id) == 0) {
            free(set[i]);
            set[i] = set[--(*n)];
            return;
        }
    }
}

void scheduler_running_jobs_add(const char *job_id)
{
    if (!job_id || !job_id[0]) return;
    pthread_mutex_lock(&g_running_lock);
    idset_add(g_running_ids, &g_running_n, job_id);
    pthread_mutex_unlock(&g_running_lock);
}

void scheduler_running_jobs_remove(const char *job_id)
{
    if (!job_id || !job_id[0]) return;
    pthread_mutex_lock(&g_running_lock);
    idset_remove(g_running_ids, &g_running_n, job_id);
    pthread_mutex_unlock(&g_running_lock);
}

/* PoP: scheduler_get_running_job_ids @ cron/scheduler.py:get_running_job_ids */
char **scheduler_get_running_job_ids(size_t *out_n)
{
    pthread_mutex_lock(&g_running_lock);
    char **out = calloc(g_running_n + 1, sizeof(char *));
    size_t n = 0;
    if (out) {
        for (size_t i = 0; i < g_running_n; i++) {
            out[n] = strdup(g_running_ids[i]);
            if (out[n]) n++;
        }
        out[n] = NULL;
    }
    pthread_mutex_unlock(&g_running_lock);
    if (out_n) *out_n = n;
    return out;
}

/* PoP: scheduler_mark_running_jobs_interrupted @ cron/scheduler.py:mark_running_jobs_interrupted */
char **scheduler_mark_running_jobs_interrupted(const char *reason,
                                               size_t *out_n)
{
    /* Snapshot the running set and record every ID in the interrupted set
     * BEFORE writing last_status, so a racing run_job completion sees the
     * flag and skips its own write (#60432). */
    pthread_mutex_lock(&g_running_lock);
    size_t snap_n = g_running_n;
    char **snap = calloc(snap_n + 1, sizeof(char *));
    if (snap) {
        for (size_t i = 0; i < snap_n; i++) {
            snap[i] = strdup(g_running_ids[i]);
            idset_add(g_interrupted_ids, &g_interrupted_n, g_running_ids[i]);
        }
        snap[snap_n] = NULL;
    }
    pthread_mutex_unlock(&g_running_lock);
    if (!snap) { if (out_n) *out_n = 0; return NULL; }

    char **marked = calloc(snap_n + 1, sizeof(char *));
    size_t m = 0;
    for (size_t i = 0; i < snap_n; i++) {
        if (!snap[i]) continue;
        /* mark_job_run(job_id, False, reason) — best-effort. */
        cronjobs_mark_job_run(snap[i], false,
                              reason ? reason : "interrupted", NULL);
        if (marked) {
            marked[m] = snap[i];   /* transfer ownership */
            snap[i] = NULL;
            m++;
        }
    }
    for (size_t i = 0; i < snap_n; i++) free(snap[i]);
    free(snap);
    if (marked) marked[m] = NULL;
    if (out_n) *out_n = m;
    return marked;
}

/* PoP: scheduler_is_interrupted @ cron/scheduler.py:_is_interrupted */
bool scheduler_is_interrupted(const char *job_id)
{
    if (!job_id) return false;
    pthread_mutex_lock(&g_running_lock);
    bool hit = idset_contains(g_interrupted_ids, g_interrupted_n, job_id);
    pthread_mutex_unlock(&g_running_lock);
    return hit;
}

/* PoP: scheduler_consume_interrupted_flag @ cron/scheduler.py:_consume_interrupted_flag */
bool scheduler_consume_interrupted_flag(const char *job_id)
{
    if (!job_id) return false;
    pthread_mutex_lock(&g_running_lock);
    bool hit = idset_contains(g_interrupted_ids, g_interrupted_n, job_id);
    if (hit) idset_remove(g_interrupted_ids, &g_interrupted_n, job_id);
    pthread_mutex_unlock(&g_running_lock);
    return hit;
}

/* ================================================================
 * Writer-preferring readers-writer lock (_ReadWriteLock)
 * ================================================================ */

struct scheduler_rwlock {
    pthread_mutex_t mu;
    pthread_cond_t cond;
    int readers;
    int writer_active;
    int writers_waiting;
};

/* PoP: scheduler_rwlock_new @ cron/scheduler.py:__init__ */
scheduler_rwlock_t *scheduler_rwlock_new(void)
{
    scheduler_rwlock_t *lk = calloc(1, sizeof(*lk));
    if (!lk) return NULL;
    pthread_mutex_init(&lk->mu, NULL);
    pthread_cond_init(&lk->cond, NULL);
    return lk;
}

void scheduler_rwlock_free(scheduler_rwlock_t *lk)
{
    if (!lk) return;
    pthread_cond_destroy(&lk->cond);
    pthread_mutex_destroy(&lk->mu);
    free(lk);
}

/* PoP: scheduler_rwlock_acquire_read @ cron/scheduler.py:acquire_read */
void scheduler_rwlock_acquire_read(scheduler_rwlock_t *lk)
{
    pthread_mutex_lock(&lk->mu);
    while (lk->writer_active || lk->writers_waiting > 0)
        pthread_cond_wait(&lk->cond, &lk->mu);
    lk->readers++;
    pthread_mutex_unlock(&lk->mu);
}

/* PoP: scheduler_rwlock_release_read @ cron/scheduler.py:release_read */
void scheduler_rwlock_release_read(scheduler_rwlock_t *lk)
{
    pthread_mutex_lock(&lk->mu);
    lk->readers--;
    if (lk->readers == 0)
        pthread_cond_broadcast(&lk->cond);
    pthread_mutex_unlock(&lk->mu);
}

/* PoP: scheduler_rwlock_acquire_write @ cron/scheduler.py:acquire_write */
void scheduler_rwlock_acquire_write(scheduler_rwlock_t *lk)
{
    pthread_mutex_lock(&lk->mu);
    lk->writers_waiting++;
    while (lk->writer_active || lk->readers > 0)
        pthread_cond_wait(&lk->cond, &lk->mu);
    lk->writers_waiting--;
    lk->writer_active = 1;
    pthread_mutex_unlock(&lk->mu);
}

/* PoP: scheduler_rwlock_release_write @ cron/scheduler.py:release_write */
void scheduler_rwlock_release_write(scheduler_rwlock_t *lk)
{
    pthread_mutex_lock(&lk->mu);
    lk->writer_active = 0;
    pthread_cond_broadcast(&lk->cond);
    pthread_mutex_unlock(&lk->mu);
}

/* The module-global _terminal_cwd_lock. */
scheduler_rwlock_t *scheduler_terminal_cwd_lock(void)
{
    static scheduler_rwlock_t *g_lock = NULL;
    static pthread_mutex_t once = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&once);
    if (!g_lock) g_lock = scheduler_rwlock_new();
    pthread_mutex_unlock(&once);
    return g_lock;
}

/* ================================================================
 * Persistent thread pools
 * ================================================================ */

typedef struct pool_task {
    scheduler_task_fn fn;
    void *arg;
    struct pool_task *next;
} pool_task_t;

struct scheduler_pool {
    pthread_mutex_t mu;
    pthread_cond_t cond;
    pool_task_t *head, *tail;
    pthread_t *threads;
    int n_threads;
    int shutting_down;
    int active;               /* tasks currently executing */
    pthread_cond_t idle;      /* signaled when queue drains + active==0 */
};

static void *pool_worker(void *arg)
{
    scheduler_pool_t *p = arg;
    for (;;) {
        pthread_mutex_lock(&p->mu);
        while (!p->head && !p->shutting_down)
            pthread_cond_wait(&p->cond, &p->mu);
        if (!p->head && p->shutting_down) {
            pthread_mutex_unlock(&p->mu);
            return NULL;
        }
        pool_task_t *t = p->head;
        p->head = t->next;
        if (!p->head) p->tail = NULL;
        p->active++;
        pthread_mutex_unlock(&p->mu);

        t->fn(t->arg);
        free(t);

        pthread_mutex_lock(&p->mu);
        p->active--;
        if (!p->head && p->active == 0)
            pthread_cond_broadcast(&p->idle);
        pthread_mutex_unlock(&p->mu);
    }
}

static scheduler_pool_t *pool_create(int n_threads, const char *name_prefix)
{
    (void)name_prefix;
    if (n_threads <= 0) {
        /* CPython default: min(32, os.cpu_count() + 4) */
        long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
        if (ncpu < 1) ncpu = 1;
        n_threads = (int)(ncpu + 4);
        if (n_threads > 32) n_threads = 32;
    }
    scheduler_pool_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    pthread_mutex_init(&p->mu, NULL);
    pthread_cond_init(&p->cond, NULL);
    pthread_cond_init(&p->idle, NULL);
    p->threads = calloc((size_t)n_threads, sizeof(pthread_t));
    if (!p->threads) { free(p); return NULL; }
    p->n_threads = n_threads;
    for (int i = 0; i < n_threads; i++)
        pthread_create(&p->threads[i], NULL, pool_worker, p);
    return p;
}

static void pool_shutdown(scheduler_pool_t *p, bool wait_for_queue)
{
    if (!p) return;
    pthread_mutex_lock(&p->mu);
    if (wait_for_queue) {
        while (p->head || p->active > 0)
            pthread_cond_wait(&p->idle, &p->mu);
    }
    p->shutting_down = 1;
    pthread_cond_broadcast(&p->cond);
    pthread_mutex_unlock(&p->mu);
    for (int i = 0; i < p->n_threads; i++)
        pthread_join(p->threads[i], NULL);
    /* drop any unexecuted tasks (shutdown(wait=False) path) */
    pool_task_t *t = p->head;
    while (t) { pool_task_t *n = t->next; free(t); t = n; }
    free(p->threads);
    pthread_cond_destroy(&p->idle);
    pthread_cond_destroy(&p->cond);
    pthread_mutex_destroy(&p->mu);
    free(p);
}

bool scheduler_pool_submit(scheduler_pool_t *pool,
                           scheduler_task_fn fn, void *arg)
{
    if (!pool || !fn) return false;
    pthread_mutex_lock(&pool->mu);
    if (pool->shutting_down) {
        pthread_mutex_unlock(&pool->mu);
        return false;   /* "cannot schedule new futures after shutdown" */
    }
    pool_task_t *t = malloc(sizeof(*t));
    if (!t) { pthread_mutex_unlock(&pool->mu); return false; }
    t->fn = fn; t->arg = arg; t->next = NULL;
    if (pool->tail) pool->tail->next = t; else pool->head = t;
    pool->tail = t;
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mu);
    return true;
}

static pthread_mutex_t g_pool_mu = PTHREAD_MUTEX_INITIALIZER;
static scheduler_pool_t *g_parallel_pool = NULL;
static int g_parallel_pool_workers = -1;
static scheduler_pool_t *g_sequential_pool = NULL;

/* PoP: scheduler_get_parallel_pool @ cron/scheduler.py:_get_parallel_pool */
scheduler_pool_t *scheduler_get_parallel_pool(int max_workers)
{
    pthread_mutex_lock(&g_pool_mu);
    if (!g_parallel_pool || g_parallel_pool_workers != max_workers) {
        if (g_parallel_pool)
            pool_shutdown(g_parallel_pool, false); /* shutdown(wait=False) */
        g_parallel_pool = pool_create(max_workers, "cron-parallel");
        g_parallel_pool_workers = max_workers;
    }
    scheduler_pool_t *p = g_parallel_pool;
    pthread_mutex_unlock(&g_pool_mu);
    return p;
}

/* PoP: scheduler_get_sequential_pool @ cron/scheduler.py:_get_sequential_pool */
scheduler_pool_t *scheduler_get_sequential_pool(void)
{
    pthread_mutex_lock(&g_pool_mu);
    if (!g_sequential_pool)
        g_sequential_pool = pool_create(1, "cron-seq");
    scheduler_pool_t *p = g_sequential_pool;
    pthread_mutex_unlock(&g_pool_mu);
    return p;
}

/* PoP: scheduler_shutdown_parallel_pool @ cron/scheduler.py:_shutdown_parallel_pool */
void scheduler_shutdown_parallel_pool(void)
{
    pthread_mutex_lock(&g_pool_mu);
    scheduler_pool_t *pp = g_parallel_pool;
    scheduler_pool_t *sp = g_sequential_pool;
    g_parallel_pool = NULL;
    g_parallel_pool_workers = -1;
    g_sequential_pool = NULL;
    pthread_mutex_unlock(&g_pool_mu);
    /* shutdown(wait=True, cancel_futures=False) */
    pool_shutdown(pp, true);
    pool_shutdown(sp, true);
}

/* ================================================================
 * Interpreter shutdown detection
 * ================================================================ */

static volatile int g_shutting_down = 0;

void scheduler_note_interpreter_shutdown(void) { g_shutting_down = 1; }

/* PoP: scheduler_interpreter_shutting_down @ cron/scheduler.py:_interpreter_shutting_down */
bool scheduler_interpreter_shutting_down(const char *error_text)
{
    if (g_shutting_down) return true;
    if (error_text) {
        /* case-insensitive substring: "cannot schedule new futures" —
         * matches both CPython shutdown variants (#58720). */
        static const char *needle = "cannot schedule new futures";
        size_t nl = strlen(needle);
        size_t tl = strlen(error_text);
        if (tl >= nl) {
            for (size_t i = 0; i + nl <= tl; i++) {
                size_t j = 0;
                while (j < nl) {
                    char a = error_text[i + j], b = needle[j];
                    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                    if (a != b) break;
                    j++;
                }
                if (j == nl) return true;
            }
        }
    }
    return false;
}

/* ================================================================
 * Lock paths
 * ================================================================ */

/* PoP: scheduler_get_lock_paths @ cron/scheduler.py:_get_lock_paths */
void scheduler_get_lock_paths(char **lock_dir_out, char **tick_lock_out)
{
    char *dir = cronjobs_cron_dir();   /* <HERMES_HOME>/cron, resolved live */
    if (lock_dir_out) *lock_dir_out = dir ? strdup(dir) : NULL;
    if (tick_lock_out) {
        if (dir) {
            size_t n = strlen(dir) + sizeof("/.tick.lock");
            char *p = malloc(n);
            if (p) snprintf(p, n, "%s/.tick.lock", dir);
            *tick_lock_out = p;
        } else {
            *tick_lock_out = NULL;
        }
    }
    free(dir);
}

/* ================================================================
 * Plugin cron env registry (mirrors scheduler_register_plugin_platform
 * in port_cron_scheduler_delivery.c — this accessor is the
 * _plugin_cron_env_var read side over the same registration hook).
 * ================================================================ */

/* PoP: scheduler_plugin_cron_env_var @ cron/scheduler.py:_plugin_cron_env_var */
const char *scheduler_plugin_cron_env_var(const char *platform_name)
{
    /* The delivery module owns the plugin table; reuse its plugin-ONLY
     * lookup (built-ins never match — faithful to the Python accessor). */
    extern const char *scheduler_plugin_env_var_lookup(const char *name);
    if (!platform_name || !platform_name[0]) return "";
    return scheduler_plugin_env_var_lookup(platform_name);
}
