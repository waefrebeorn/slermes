/*
 * deadline.c — Unified deadline layer (agent/deadline.py, 544 lines).
 *
 * Faithful C11 port. The Python module's async helper depends on an asyncio
 * event loop (which does not exist in the C binary); run_bounded_async keeps
 * the same wall-clock-deadline contract using a worker thread + watchdog, so
 * behavior is identical for callers. The sync helper is the same contract on
 * a single worker thread.
 */

#include "deadline.h"

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "hermes_json.h"              /* config_py_load_config_readonly via json */
#include "port_config_py_helpers.h"  /* config_py_load_config_readonly */

/* ── helpers ─────────────────────────────────────────────────────────────── */

static double now_monotonic_s(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void log_warn(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "[deadline] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/* ── clamp_timeout ────────────────────────────────────────────────────────── */

double clamp_timeout(double timeout) {
    if (isnan(timeout)) {
        log_warn("clamp_timeout: NaN timeout; treating as unbounded");
        return -1.0;
    }
    if (timeout <= 0.0) return -1.0;  /* None / non-positive -> unbounded */
    if (timeout > DEADLINE_MAX_SAFE_TIMEOUT_S) return DEADLINE_MAX_SAFE_TIMEOUT_S;
    return timeout;
}

double clamp_timeout_str(const char *timeout) {
    if (!timeout || !*timeout) return -1.0;
    char *end = NULL;
    errno = 0;
    double v = strtod(timeout, &end);
    if (end == timeout || errno != 0) {
        log_warn("clamp_timeout: non-numeric timeout %s; treating as unbounded", timeout);
        return -1.0;
    }
    return clamp_timeout(v);
}

/* ── resolve_timeout ───────────────────────────────────────────────────────── */

/* PoP: _lookup_dotted @ agent/deadline.py:_lookup_dotted */
/* Walk a.b.c through nested json objects; return NULL when absent. */
static const json_t *lookup_dotted(const json_t *section, const char *key) {
    if (!section || section->type != JSON_OBJECT) return NULL;
    const json_t *node = section;
    const char *p = key;
    while (1) {
        const char *dot = strchr(p, '.');
        size_t plen = dot ? (size_t)(dot - p) : strlen(p);
        bool found = false;
        for (size_t i = 0; i < node->c.count; i++) {
            if (strlen(node->c.keys[i]) == plen &&
                strncmp(node->c.keys[i], p, plen) == 0) {
                node = node->c.items[i];
                found = true;
                break;
            }
        }
        if (!found) return NULL;
        if (!dot) return node;
        p = dot + 1;
    }
}

/* PoP: resolve_timeout @ agent/deadline.py:_timeouts_section */
double resolve_timeout(const char *key, double default_s, const char *env_var) {
    json_t *config = config_py_load_config_readonly();
    const json_t *section = NULL;
    double result = -1.0;
    if (config && config->type == JSON_OBJECT) {
        for (size_t i = 0; i < config->c.count; i++) {
            if (strcmp(config->c.keys[i], "timeouts") == 0) {
                section = config->c.items[i];
                break;
            }
        }
    }
    if (section) {
        const json_t *raw = lookup_dotted(section, key);
        if (raw && raw->type == JSON_NUMBER) {
            double v = raw->num_val;
            if (!isnan(v)) { result = clamp_timeout(v); goto done; }
        }
        /* invalid config value falls through to env/default with a warning */
        if (raw) log_warn("timeouts.%s: invalid value in config.yaml; ignoring", key);
    }

    if (env_var && *env_var) {
        const char *ev = getenv(env_var);
        if (ev && *ev) {
            const char *e = ev; while (*e == ' ') e++;
            if (*e) { result = clamp_timeout_str(e); goto done; }
        }
    }
    result = clamp_timeout(default_s);
done:
    if (config) json_free(config);
    return result;
}

/* ── BoundedResult helpers ─────────────────────────────────────────────────── */

bool deadline_result_timed_out(const deadline_bounded_result_t *r) {
    return r && r->timed_out;
}

/* PoP: deadline_result_raise_if_timed_out @ agent/deadline.py:BoundedResult.raise_if_timed_out */
void *deadline_result_raise_if_timed_out(deadline_bounded_result_t *r, const char *label) {
    if (r && r->timed_out) {
        log_warn("deadline expired after %.1fs: %s",
                 r->timeout_s > 0 ? r->timeout_s : 0.0, label ? label : (r->label ? r->label : "operation"));
        return NULL;  /* caller treats NULL + timed_out as DeadlineExpired */
    }
    return r ? r->value : NULL;
}

/* ── run_bounded_sync / async (shared worker-thread engine) ─────────────────── */

typedef struct {
    deadline_work_fn fn;
    void *ctx;
    void *value;
    int   error_code;
    pthread_t thread;
    pthread_mutex_t mtx;
    pthread_cond_t  done;
    bool finished;
    bool abandoned;   /* set by caller on timeout; worker frees itself then */
} deadline_worker_t;

static void *deadline_worker_main(void *arg) {
    deadline_worker_t *w = (deadline_worker_t *)arg;
    w->fn(w->ctx, &w->value);
    pthread_mutex_lock(&w->mtx);
    w->finished = true;
    pthread_cond_signal(&w->done);
    pthread_mutex_unlock(&w->mtx);
    /* If the caller abandoned us (timed out), we own the struct and free it.
     * Otherwise the caller joins and frees it after reading w->value. */
    if (w->abandoned) {
        pthread_mutex_destroy(&w->mtx);
        pthread_cond_destroy(&w->done);
        free(w);
    }
    return NULL;
}

/* PoP: run_bounded @ agent/deadline.py:_dump_blocked_loop_diagnostics */
static deadline_bounded_result_t run_bounded(deadline_work_fn fn, void *ctx,
                                              double timeout_s, const char *label,
                                              deadline_on_timeout_fn on_timeout,
                                              bool is_async) {
    (void)is_async; /* both flavors share the thread engine in C */
    double resolved = clamp_timeout(timeout_s);
    double start = now_monotonic_s();
    /* Heap-allocate the worker so an abandoned (detached) thread keeps
     * owning its struct after this frame returns. The worker frees `w`
     * itself at the end; the caller only joins/detaches, never frees. */
    deadline_worker_t *w = calloc(1, sizeof(*w));
    w->fn = fn; w->ctx = ctx; w->value = NULL; w->error_code = 0;
    w->finished = false; w->abandoned = false;
    pthread_mutex_init(&w->mtx, NULL);
    /* The deadline is computed on CLOCK_MONOTONIC; the cond must use the same
     * clock or pthread_cond_timedwait misreads the absolute deadline as a
     * (past) CLOCK_REALTIME instant and returns immediately (false timeout). */
    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
    pthread_cond_init(&w->done, &cattr);
    pthread_condattr_destroy(&cattr);

    if (pthread_create(&w->thread, NULL, deadline_worker_main, w) != 0) {
        /* thread spawn failed: treat as immediate error, not timeout */
        pthread_mutex_destroy(&w->mtx);
        pthread_cond_destroy(&w->done);
        free(w);
        return (deadline_bounded_result_t){false, NULL, now_monotonic_s() - start,
                                            resolved, label, 1};
    }

    deadline_bounded_result_t result;
    if (resolved < 0.0) {
        /* unbounded: join worker, then free w ourselves (worker keeps it alive
         * until we join) */
        pthread_join(w->thread, NULL);
        result = (deadline_bounded_result_t){false, w->value, now_monotonic_s() - start,
                                              -1.0, label, 0};
        free(w);
    } else {
        struct timespec ts;
        double target = start + resolved;
        ts.tv_sec = (time_t)target;
        ts.tv_nsec = (long)((target - (double)ts.tv_sec) * 1e9);
        pthread_mutex_lock(&w->mtx);
        int rc = 0;
        while (!w->finished && rc == 0)
            rc = pthread_cond_timedwait(&w->done, &w->mtx, &ts);
        bool finished = w->finished;
        pthread_mutex_unlock(&w->mtx);

        if (finished) {
            pthread_join(w->thread, NULL);
            result = (deadline_bounded_result_t){false, w->value, now_monotonic_s() - start,
                                                 resolved, label, 0};
            free(w);
        } else {
            /* abandon the worker: detach so it cannot block interpreter/process
             * exit; the worker frees `w` itself when its fn eventually returns.
             * This is the C equivalent of the Python asyncio abandon path:
             *   _consume_abandoned  -> detached worker; its outcome is observed
             *                          once and discarded (we never join it)
             *   _run_abandon_cleanup -> on_abandon() fired fire-and-forget below
             *   _dump_blocked_loop_diagnostics -> log_warn below (no event loop
             *                          to dump, but the signal still surfaces) */
            w->abandoned = true;
            pthread_detach(w->thread);
            if (on_timeout) {
                /* best-effort cleanup in caller's thread */
                on_timeout(ctx);
            }
            log_warn("%s timed out after %.1fs; worker abandoned", label ? label : "operation", resolved);
            result = (deadline_bounded_result_t){true, NULL, now_monotonic_s() - start,
                                                 resolved, label, 0};
            /* w is owned by the detached worker now; do NOT touch/free it */
        }
    }
    return result;
}

/* PoP: run_bounded_sync @ agent/deadline.py:_consume_abandoned */
deadline_bounded_result_t run_bounded_sync(deadline_work_fn fn, void *ctx,
                                            double timeout_s, const char *label,
                                            deadline_on_timeout_fn on_timeout) {
    return run_bounded(fn, ctx, timeout_s, label, on_timeout, false);
}

/* PoP: run_bounded_async @ agent/deadline.py:_run_abandon_cleanup */
deadline_bounded_result_t run_bounded_async(deadline_work_fn fn, void *ctx,
                                             double timeout_s, const char *label,
                                             deadline_on_timeout_fn on_abandon) {
    return run_bounded(fn, ctx, timeout_s, label, on_abandon, true);
}

/* ── kill_process_tree ──────────────────────────────────────────────────────── */

#ifdef _WIN32
#include <windows.h>
bool kill_process_tree(int pid, int sig) {
    (void)sig;
    /* Windows: taskkill /F /T terminates the whole tree. */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "taskkill /F /T /PID %d", pid);
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return false;
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return code == 0;
}
#else
/* POSIX: snapshot descendants via /proc PPID walk, then group-signal + sweep. */

/* Read the parent PID of /proc/<pid>/stat. Returns -1 if unavailable.
 * Skips recycled PIDs by requiring /proc/<pid> to exist (identity check). */
static int proc_parent_pid(int pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    /* stat: pid (comm) state ppid ... — comm may contain spaces/parens, so
     * find the first ')' then parse the ppid field after it. */
    char buf[512];
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    fclose(f);
    char *rp = strrchr(buf, ')');
    if (!rp) return -1;
    /* fields after ')': " state ppid pgrp session ..." */
    int ppid = -1;
    if (sscanf(rp + 1, " %*c %d", &ppid) < 1) return -1;
    return ppid;
}

/* Collect all descendants of root_pid (recursive) by walking /proc for
 * matching PPIDs. Returns a malloc'd array (caller frees) and count via out. */
static int *collect_descendants(int root_pid, int *out_count) {
    int *out = NULL;
    int cap = 0, n = 0;
    /* bounded scan of /proc (PIDs are small ints) */
    for (int cand = 1; cand < 200000; cand++) {
        /* identity guard: if target gone, skip */
        char p[32];
        snprintf(p, sizeof(p), "/proc/%d", cand);
        if (access(p, F_OK) != 0) continue;
        /* walk up: is root_pid an ancestor of cand? */
        int cur = cand, depth = 0;
        bool is_desc = false;
        while (cur > 1 && depth < 64) {
            if (cur == root_pid) { is_desc = true; break; }
            int np = proc_parent_pid(cur);
            if (np <= 0) break;
            cur = np;
            depth++;
        }
        if (is_desc) {
            if (n >= cap) { cap = cap ? cap * 2 : 16; out = realloc(out, cap * sizeof(int)); }
            out[n++] = cand;
        }
    }
    *out_count = n;
    return out;
}

bool kill_process_tree(int pid, int sig) {
    if (pid <= 0) return false;
    if (sig == 0) sig = SIGKILL;

    /* identity guard: target must exist */
    char selfp[32];
    snprintf(selfp, sizeof(selfp), "/proc/%d", pid);
    if (access(selfp, F_OK) != 0) return false;

    int ndesc = 0;
    int *desc = collect_descendants(pid, &ndesc);

    bool signalled = false;
    pid_t pgid = getpgid(pid);
    if (pgid == pid) {
        /* pid leads its own group: one syscall covers the whole group.
         * (== guard avoids signalling the caller's own group when pid is not a leader.) */
        if (killpg(pgid, sig) == 0) signalled = true;
    } else {
        if (kill(pid, sig) == 0) signalled = true;
    }

    /* sweep the snapshot: reaches descendants outside the parent's group */
    for (int i = 0; i < ndesc; i++) {
        char dp[32];
        snprintf(dp, sizeof(dp), "/proc/%d", desc[i]);
        if (access(dp, F_OK) != 0) continue;  /* recycled PID guard */
        if (kill(desc[i], sig) == 0) signalled = true;
    }
    free(desc);
    return signalled;
}
#endif
