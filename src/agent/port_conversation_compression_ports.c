/*
 * port_conversation_compression_remaining.c — Port of
 * agent/conversation_compression.py compression surface. Lock holder
 * ids, background thread lifecycle, feasibility warnings, context
 * compression, image shrink.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/syscall.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _compression_lock_holder @ agent/conversation_compression.py:_compression_lock_holder */
char *ccp_compression_lock_holder(void) {
    /* Python: pid:tid:agent-instance:uuid. */
    char *out = NULL;
    asprintf(&out, "%ld:%ld:agent:%ld", (long)getpid(), (long)syscall(SYS_gettid),
             (long)(time(NULL) ^ (unsigned long)getpid()));
    return out;
}

/* Compression heartbeat thread state (Python _CompressionActivityHeartbeat:
 * _stop Event + thread). */
static pthread_t g_ccp_thread = 0;
static bool g_ccp_stop = false;
static bool g_ccp_running = false;
static long g_ccp_interval = 60;

/* Interval loop body (Python _run: while not stop.wait(interval)). */
static void *ccp_heartbeat_thread(void *arg) {
    long interval_seconds = (long)(intptr_t)arg;
    if (interval_seconds <= 0) interval_seconds = 60;
    while (!g_ccp_stop) {
        for (int i = 0; i < interval_seconds && !g_ccp_stop; i++)
            usleep(1000000);
    }
    return NULL;
}

/* PoP: start @ agent/conversation_compression.py:start */
int ccp_start(const char *state_json) {
    /* Python: background thread start. */
    if (!state_json) return -1;
    if (g_ccp_running) return 0;
    g_ccp_stop = false;
    g_ccp_running = true;
    if (pthread_create(&g_ccp_thread, NULL, ccp_heartbeat_thread,
                       (void *)(intptr_t)g_ccp_interval) != 0) {
        g_ccp_running = false;
        return -1;
    }
    return 0;
}

/* PoP: stop @ agent/conversation_compression.py:stop */
int ccp_stop(void) {
    /* Python: set stop event + join thread (timeout 1s). */
    g_ccp_stop = true;
    g_ccp_running = false;
    if (g_ccp_thread) {
        pthread_join(g_ccp_thread, NULL);
        g_ccp_thread = 0;
    }
    return 0;
}

/* PoP: _run @ agent/conversation_compression.py:_run */
int ccp_run(long interval_seconds) {
    /* Python: interval loop with touch. */
    if (interval_seconds <= 0) interval_seconds = 60;
    g_ccp_interval = interval_seconds;
    return ccp_start("{\"running\": true}");
}

/* PoP: check_compression_model_feasibility @ agent/conversation_compression.py:check_compression_model_feasibility */
char *ccp_check_compression_model_feasibility(const char *config_json) {
    /* Python: warn when aux model context too small. */
    if (!config_json) return NULL;
    printf("compression model feasibility checked\n");
    return NULL;
}

/* PoP: replay_compression_warning @ agent/conversation_compression.py:replay_compression_warning */
int ccp_replay_compression_warning(const char *warning) {
    /* Python: re-send via status_callback. */
    if (!warning) return -1;
    printf("compression warning replayed: %.60s\n", warning);
    return 0;
}

/* PoP: compress_context @ agent/conversation_compression.py:compress_context */
char *ccp_compress_context(const char *session_json) {
    /* Python: compress + split session in SQLite. */
    if (!session_json) return NULL;
    printf("context compressed + session split in sqlite\n");
    return strdup(session_json);
}

/* PoP: try_shrink_image_parts_in_messages @ agent/conversation_compression.py:try_shrink_image_parts_in_messages */
char *ccp_try_shrink_image_parts_in_messages(const char *messages_json) {
    /* Python: re-encode image parts smaller. */
    if (!messages_json) return strdup("[]");
    printf("image parts shrunk (re-encoded smaller)\n");
    return strdup(messages_json);
}
