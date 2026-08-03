/*
 * memory_monitor.c — extracted from gateway/helpers.c monolith.
 *
 * Real implementation home for the Python module it ports (no longer a
 * name-parity stub). Public prototypes stay in include/gateway_helpers.h
 * (or hermes_gateway.h); callers are unchanged.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway_memory_monitor.h"
#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

/* ================================================================
 *  Process memory monitor
 *  Port of Python gateway/memory_monitor.py.
 *  Reads RSS from /proc/self/statm on Linux.
 * ================================================================ */

/* Get current process RSS in MB by reading /proc/self/statm.
 * Port of Python gateway/memory_monitor.py _get_rss_mb().
 * AG26: Port of Python gateway/memory_monitor.py:_get_rss_mb().
 * Returns RSS in MB, or 0 if unavailable. */
/* PoP: _get_rss_mb @ gateway/memory_monitor.py:_get_rss_mb */
int get_rss_mb(void) {
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long page_count = 0;
    long page_size = sysconf(_SC_PAGESIZE);
    if (fscanf(f, "%ld", &page_count) != 1) { fclose(f); return 0; }
    fclose(f);
    if (page_count <= 0 || page_size <= 0) return 0;
    return (int)((page_count * page_size) / (1024 * 1024));
}

/* ================================================================
 *  Memory Monitor — RSS tracking helpers
 *  Port of Python gateway/memory_monitor.py.
 * ================================================================ */

static pthread_t g_memory_monitor_thread;
static bool g_memory_monitor_running = false;
static pthread_mutex_t g_memory_monitor_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_memory_monitor_is_running @ gateway/memory_monitor.py:is_running */
/* True if the background monitor thread is alive. Read under the monitor lock
 * (mirrors Python's `with _lock: return _monitor_thread and is_alive()`). */
bool gw_memory_monitor_is_running(void) {
    pthread_mutex_lock(&g_memory_monitor_mutex);
    bool running = g_memory_monitor_running;
    pthread_mutex_unlock(&g_memory_monitor_mutex);
    return running;
}


/* Log current memory usage via stderr.
 * Port of Python gateway/memory_monitor.py log_memory_usage().
 * AG26: Port of Python gateway/memory_monitor.py:log_memory_usage().
 */
/* PoP: log_memory_usage @ gateway/memory_monitor.py:log_memory_usage */
void log_memory_usage(const char *prefix) {
    int rss = get_rss_mb();
    if (rss > 0) {
        fprintf(stderr, "[memory%s] RSS: %d MB\n",
                prefix ? prefix : "", rss);
    }
}


/* Port of Python gateway/memory_monitor.py:_monitor_loop(). */
/* Background thread: polls RSS every interval seconds. */
static void *memory_monitor_loop(void *arg) {
    double interval = *(double *)arg;
    free(arg);
    while (true) {
        pthread_mutex_lock(&g_memory_monitor_mutex);
        bool still_running = g_memory_monitor_running;
        pthread_mutex_unlock(&g_memory_monitor_mutex);
        if (!still_running) break;
        log_memory_usage(NULL);
        struct timespec ts;
        ts.tv_sec = (time_t)interval;
        ts.tv_nsec = (long)((interval - (double)ts.tv_sec) * 1e9);
        nanosleep(&ts, NULL);
    }
    return NULL;
}


/* Start background memory monitoring. Returns true if started.
 * Port of Python start_memory_monitoring().
 * AG26: Port of Python gateway/memory_monitor.py:start_memory_monitoring().
 */
/* PoP: start_memory_monitoring @ gateway/memory_monitor.py:start_memory_monitoring */
bool start_memory_monitoring(double interval_seconds) {
    pthread_mutex_lock(&g_memory_monitor_mutex);
    if (g_memory_monitor_running) { pthread_mutex_unlock(&g_memory_monitor_mutex); return false; }
    g_memory_monitor_running = true;
    pthread_mutex_unlock(&g_memory_monitor_mutex);
    double *interval = malloc(sizeof(double));
    if (!interval) { pthread_mutex_lock(&g_memory_monitor_mutex); g_memory_monitor_running = false; pthread_mutex_unlock(&g_memory_monitor_mutex); return false; }
    *interval = interval_seconds > 0 ? interval_seconds : 300.0;
    if (pthread_create(&g_memory_monitor_thread, NULL, memory_monitor_loop, interval) != 0) {
        free(interval);
        pthread_mutex_lock(&g_memory_monitor_mutex); g_memory_monitor_running = false; pthread_mutex_unlock(&g_memory_monitor_mutex);
        return false;
    }
    pthread_detach(g_memory_monitor_thread);
    return true;
}


/* Port of Python gateway/memory_monitor.py:stop_memory_monitoring(). */
/* Stop background memory monitoring. */
void stop_memory_monitoring(void) {
    pthread_mutex_lock(&g_memory_monitor_mutex);
    g_memory_monitor_running = false;
    pthread_mutex_unlock(&g_memory_monitor_mutex);
}


/* Check if memory monitoring is running. */
bool is_memory_monitoring_running(void) {
    pthread_mutex_lock(&g_memory_monitor_mutex);
    bool running = g_memory_monitor_running;
    pthread_mutex_unlock(&g_memory_monitor_mutex);
    return running;
}

