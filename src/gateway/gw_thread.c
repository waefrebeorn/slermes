/*
 * gw_thread.c -- extracted from gateway/server.c monolith.
 * Real implementation of one gateway-lifecycle concern. Public
 * gw_* protos stay in include/hermes_gateway.h; promoted cross-
 * module statics are in include/gw_server_internals.h.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_gateway_core.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "gateway_helpers.h"
#include "hermes_skill_commands.h"
#include "hermes_logger.h"
#include "hermes_telegram_filter.h"
#include "gw_server_internals.h"
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

double gw_mono_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void gw_queue_init(void) {
    g_gw.msg_queue_head = 0;
    g_gw.msg_queue_tail = 0;
    pthread_mutex_init(&g_gw.queue_mutex, NULL);
    pthread_cond_init(&g_gw.queue_cond, NULL);
}

bool gw_queue_push(const char *platform, const char *chat_id,
                    const char *text, const char *thread_id) {
    if (!platform || !chat_id || !text) return false;

    pthread_mutex_lock(&g_gw.queue_mutex);

    /* Check if queue is full */
    int next = (g_gw.msg_queue_head + 1) % GW_QUEUE_MAX;
    if (next == g_gw.msg_queue_tail) {
        /* Queue full — drop oldest */
        g_gw.msg_queue_tail = (g_gw.msg_queue_tail + 1) % GW_QUEUE_MAX;
    }

    gateway_msg_t *slot = &g_gw.msg_queue[g_gw.msg_queue_head];
    snprintf(slot->platform, sizeof(slot->platform), "%s", platform);
    snprintf(slot->chat_id, sizeof(slot->chat_id), "%s", chat_id);
    /* Drop any stale text (oldest dropped message slot) before overwrite. */
    free(slot->text);
    slot->text = strdup(text ? text : "");
    if (thread_id)
        snprintf(slot->thread_id, sizeof(slot->thread_id), "%s", thread_id);
    else
        slot->thread_id[0] = '\0';
    slot->timestamp = gw_mono_time();

    g_gw.msg_queue_head = next;

    pthread_cond_signal(&g_gw.queue_cond);
    pthread_mutex_unlock(&g_gw.queue_mutex);
    return true;
}

bool gw_queue_pop(gateway_msg_t *msg) {
    if (!msg) return false;

    pthread_mutex_lock(&g_gw.queue_mutex);
    if (g_gw.msg_queue_head == g_gw.msg_queue_tail) {
        pthread_mutex_unlock(&g_gw.queue_mutex);
        return false; /* empty */
    }

    *msg = g_gw.msg_queue[g_gw.msg_queue_tail];
    /* Caller owns a copy of the heap text. */
    msg->text = strdup(msg->text ? msg->text : "");
    g_gw.msg_queue_tail = (g_gw.msg_queue_tail + 1) % GW_QUEUE_MAX;
    pthread_mutex_unlock(&g_gw.queue_mutex);
    return true;
}

int gw_queue_depth(void) {
    pthread_mutex_lock(&g_gw.queue_mutex);
    int depth = (g_gw.msg_queue_head - g_gw.msg_queue_tail + GW_QUEUE_MAX) % GW_QUEUE_MAX;
    pthread_mutex_unlock(&g_gw.queue_mutex);
    return depth;
}

void gw_queue_drain_all(void) {
    gateway_msg_t msgs[GW_QUEUE_MAX];
    int count = 0;
    pthread_mutex_lock(&g_gw.queue_mutex);
    while (g_gw.msg_queue_head != g_gw.msg_queue_tail && count < GW_QUEUE_MAX) {
        msgs[count++] = g_gw.msg_queue[g_gw.msg_queue_tail];
        g_gw.msg_queue_tail = (g_gw.msg_queue_tail + 1) % GW_QUEUE_MAX;
    }
    pthread_mutex_unlock(&g_gw.queue_mutex);
    for (int i = 0; i < count; i++) {
        process_update(msgs[i].platform, msgs[i].chat_id, msgs[i].text);
        free(msgs[i].text);
        msgs[i].text = NULL;
    }
}

void gw_set_keepalive(int plat_idx, double keepalive_sec) {
    if (plat_idx >= 0 && plat_idx < GW_MAX_PLATFORMS)
        g_gw.platform_keepalive_sec[plat_idx] = keepalive_sec;
}

bool gw_dedup_check(const char *message_id) {
    if (!message_id || !*message_id) return false;
    double now = gw_mono_time();

    /* Prune expired entries */
    while (g_gw.dedup_count > 0 &&
           (now - g_gw.dedup_timestamps[g_gw.dedup_head]) > g_gw.dedup_ttl) {
        g_gw.dedup_head = (g_gw.dedup_head + 1) % 64;
        g_gw.dedup_count--;
    }

    /* Linear scan for match (small ring, <64 entries) */
    for (int i = 0; i < g_gw.dedup_count; i++) {
        int idx = (g_gw.dedup_head + i) % 64;
        if (strcmp(g_gw.dedup_ids[idx], message_id) == 0)
            return true; /* duplicate */
    }
    return false;
}

void gw_dedup_add(const char *message_id) {
    if (!message_id || !*message_id) return;
    if (g_gw.dedup_count >= 64) return; /* ring full, skip */

    int idx = (g_gw.dedup_head + g_gw.dedup_count) % 64;
    snprintf(g_gw.dedup_ids[idx], sizeof(g_gw.dedup_ids[idx]), "%s", message_id);
    g_gw.dedup_timestamps[idx] = gw_mono_time();
    g_gw.dedup_count++;
}

void gw_batch_accumulate(const char *platform, const char *chat_id, const char *fragment) {
    if (!platform || !chat_id || !fragment) return;

    double now = gw_mono_time();
    double BATCH_TIMEOUT = 2.0; /* seconds to wait for more fragments */

    /* If no active batch or different source, flush first */
    if (g_gw.batch_active &&
        (strcmp(g_gw.batch_platform, platform) != 0 ||
         strcmp(g_gw.batch_chat_id, chat_id) != 0 ||
         (now - g_gw.batch_start_time) > BATCH_TIMEOUT)) {
        gw_batch_flush();
    }

    /* Start or continue batch */
    if (!g_gw.batch_active) {
        snprintf(g_gw.batch_platform, sizeof(g_gw.batch_platform), "%s", platform);
        snprintf(g_gw.batch_chat_id, sizeof(g_gw.batch_chat_id), "%s", chat_id);
        g_gw.batch_buf[0] = '\0';
        g_gw.batch_start_time = now;
        g_gw.batch_active = true;
    }

    size_t remaining = sizeof(g_gw.batch_buf) - strlen(g_gw.batch_buf) - 1;
    if (remaining > 0) {
        strncat(g_gw.batch_buf, fragment, remaining);
    }
}

void gw_batch_flush(void) {
    if (!g_gw.batch_active) return;
    if (g_gw.batch_buf[0]) {
        process_update(g_gw.batch_platform, g_gw.batch_chat_id, g_gw.batch_buf);
    }
    g_gw.batch_buf[0] = '\0';
    g_gw.batch_active = false;
}
