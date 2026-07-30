/*
 * gateway_lifecycle.c — Gateway lifecycle management.
 * Port of Python gateway/run.py GatewayRunner lifecycle.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_gateway_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

/* PID file management */
#define PIDFILE_PATH "%s/.slermes/gateway.pid"

static char g_pidfile_path[1024] = "";

static void pidfile_path(void) {
    if (g_pidfile_path[0]) return;
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return;
    snprintf(g_pidfile_path, sizeof(g_pidfile_path), PIDFILE_PATH, home);
    /* Ensure directory exists */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/.slermes", home);
    mkdir(dir, 0700);
}

/* Port of Python gateway/run.py: GatewayRunner saves PID to ~/.hermes/gateway.pid */
void gw_lifecycle_write_pid(void) {
    pidfile_path();
    if (!g_pidfile_path[0]) return;
    FILE *f = fopen(g_pidfile_path, "w");
    if (f) {
        fprintf(f, "%d\n", getpid());
        fclose(f);
        printf("[lifecycle] PID written to %s\n", g_pidfile_path);
    }
}

void gw_lifecycle_remove_pid(void) {
    if (!g_pidfile_path[0]) {
        pidfile_path();
        if (!g_pidfile_path[0]) return;
    }
    if (unlink(g_pidfile_path) == 0)
        printf("[lifecycle] PID file removed: %s\n", g_pidfile_path);
    else if (errno != ENOENT)
        printf("[lifecycle] Warning: could not remove %s: %s\n",
               g_pidfile_path, strerror(errno));
}

typedef enum {
    LIFECYCLE_STOPPED = 0, LIFECYCLE_STARTING, LIFECYCLE_RUNNING,
    LIFECYCLE_STOPPING, LIFECYCLE_RESTARTING, LIFECYCLE_FAILED
} lifecycle_state_t;

static lifecycle_state_t g_lifecycle_state = LIFECYCLE_STOPPED;
static pthread_mutex_t g_lifecycle_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_lifecycle_cond = PTHREAD_COND_INITIALIZER;
static time_t g_lifecycle_start_time = 0;
static int g_restart_count = 0;

typedef struct {
    char name[64];
    int fail_count;
    int max_fails;
    time_t last_attempt;
    int interval_sec;
    bool reconnecting;
    void (*restart_fn)(void);
} reconnect_t;

#define MAX_RECONNECT 32
static reconnect_t g_reconnect[MAX_RECONNECT];
static int g_reconnect_count = 0;
static pthread_t g_reconnect_thread;
static volatile bool g_reconnect_running = false;

static const char *state_name(lifecycle_state_t s) {
    switch (s) {
        case LIFECYCLE_STOPPED: return "stopped";
        case LIFECYCLE_STARTING: return "starting";
        case LIFECYCLE_RUNNING: return "running";
        case LIFECYCLE_STOPPING: return "stopping";
        case LIFECYCLE_RESTARTING: return "restarting";
        case LIFECYCLE_FAILED: return "failed";
        default: return "unknown";
    }
}

static void lock(void) { pthread_mutex_lock(&g_lifecycle_mutex); }
static void unlock(void) { pthread_mutex_unlock(&g_lifecycle_mutex); }

void gw_lifecycle_init(void) {
    lock();
    g_lifecycle_state = LIFECYCLE_STOPPED;
    g_lifecycle_start_time = 0;
    g_restart_count = 0;
    unlock();
    printf("[lifecycle] Gateway lifecycle initialized\n");
}

void gw_lifecycle_start(void) {
    lock();
    if (g_lifecycle_state != LIFECYCLE_STOPPED && g_lifecycle_state != LIFECYCLE_FAILED) {
        printf("[lifecycle] Already %s\n", state_name(g_lifecycle_state));
        unlock(); return;
    }
    g_lifecycle_state = LIFECYCLE_STARTING;
    unlock();
    printf("[lifecycle] Starting gateway...\n");
}

void gw_lifecycle_started(void) {
    lock(); g_lifecycle_state = LIFECYCLE_RUNNING; g_restart_count = 0; unlock();
    gw_lifecycle_write_pid();
    printf("[lifecycle] Gateway running\n");
}

void gw_lifecycle_stop(void) {
    lock(); lifecycle_state_t prev = g_lifecycle_state; g_lifecycle_state = LIFECYCLE_STOPPING; unlock();
    printf("[lifecycle] Stopping (was: %s)...\n", state_name(prev));
}

void gw_lifecycle_stopped(void) {
    lock(); g_lifecycle_state = LIFECYCLE_STOPPED; g_reconnect_running = false; unlock();
    gw_lifecycle_remove_pid();
    printf("[lifecycle] Gateway stopped\n");
}

int gw_lifecycle_restart(void) {
    lock();
    time_t now = time(NULL);
    if (g_restart_count >= 5 && now - g_lifecycle_start_time < 60) {
        g_lifecycle_state = LIFECYCLE_FAILED;
        unlock();
        return 1;
    }
    g_restart_count++;
    g_lifecycle_state = LIFECYCLE_RESTARTING;
    printf("[lifecycle] Restart #%d\n", g_restart_count);
    unlock();
    return 0;
}

char *gw_lifecycle_get_status_json(void) {
    json_t *root = json_object();
    if (!root) return NULL;
    lock();
    json_set(root, "state", json_string(state_name(g_lifecycle_state)));
    char uptime[64] = "0s";
    if (g_lifecycle_start_time > 0) {
        snprintf(uptime, sizeof(uptime), "%lds", (long)(time(NULL) - g_lifecycle_start_time));
    }
    json_set(root, "uptime", json_string(uptime));
    json_set(root, "restart_count", json_number(g_restart_count));
    json_set(root, "running", json_bool(g_lifecycle_state == LIFECYCLE_RUNNING));
    unlock();
    char *result = json_serialize(root);
    json_free(root);
    return result;
}

bool gw_lifecycle_is_running(void) {
    lock();
    bool r = (g_lifecycle_state == LIFECYCLE_RUNNING || g_lifecycle_state == LIFECYCLE_STARTING);
    unlock();
    return r;
}

void gw_reconnect_register(const char *name, void (*rf)(void), int mf, int iv) {
    if (!name || g_reconnect_count >= MAX_RECONNECT) return;
    reconnect_t *r = &g_reconnect[g_reconnect_count++];
    snprintf(r->name, sizeof(r->name), "%s", name);
    r->fail_count = 0;
    r->max_fails = mf > 0 ? mf : 3;
    r->last_attempt = 0;
    r->interval_sec = iv > 0 ? iv : 10;
    r->reconnecting = false;
    r->restart_fn = rf;
}

void gw_reconnect_report_failure(const char *name) {
    if (!name) return;
    for (int i = 0; i < g_reconnect_count; i++) {
        if (strcmp(g_reconnect[i].name, name) == 0) {
            g_reconnect[i].fail_count++;
            g_reconnect[i].last_attempt = time(NULL);
            printf("[reconnect] %s: fail #%d/%d\n", name,
                   g_reconnect[i].fail_count, g_reconnect[i].max_fails);
            if (g_reconnect[i].fail_count >= g_reconnect[i].max_fails)
                g_reconnect[i].reconnecting = true;
            break;
        }
    }
}

void gw_reconnect_report_success(const char *name) {
    if (!name) return;
    for (int i = 0; i < g_reconnect_count; i++) {
        if (strcmp(g_reconnect[i].name, name) == 0) {
            g_reconnect[i].fail_count = 0;
            g_reconnect[i].reconnecting = false;
            break;
        }
    }
}

static void *reconnect_watcher(void *arg) {
    (void)arg;
    printf("[reconnect] Watcher started\n");
    while (g_reconnect_running) {
        sleep(5);
        time_t now = time(NULL);
        for (int i = 0; i < g_reconnect_count; i++) {
            reconnect_t *r = &g_reconnect[i];
            if (!r->reconnecting || now - r->last_attempt < r->interval_sec) continue;
            printf("[reconnect] Reconnecting %s...\n", r->name);
            r->last_attempt = now;
            if (r->restart_fn) r->restart_fn();
        }
    }
    printf("[reconnect] Watcher stopped\n");
    return NULL;
}

void gw_reconnect_start_watcher(void) {
    if (g_reconnect_running) return;
    g_reconnect_running = true;
    if (pthread_create(&g_reconnect_thread, NULL, reconnect_watcher, NULL) != 0)
        g_reconnect_running = false;
}

void gw_reconnect_stop_watcher(void) {
    if (!g_reconnect_running) return;
    g_reconnect_running = false;
    pthread_join(g_reconnect_thread, NULL);
}

void gw_shutdown(int timeout_sec) {
    printf("[lifecycle] Shutdown (timeout=%ds)...\n", timeout_sec);
    gw_lifecycle_stop();
    lock();
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_sec;
    while (g_lifecycle_state != LIFECYCLE_STOPPED && g_lifecycle_state != LIFECYCLE_FAILED) {
        if (pthread_cond_timedwait(&g_lifecycle_cond, &g_lifecycle_mutex, &ts) == ETIMEDOUT) break;
    }
    unlock();
    gw_reconnect_stop_watcher();
    gw_lifecycle_stopped();
}
