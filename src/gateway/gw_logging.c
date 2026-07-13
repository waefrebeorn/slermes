/*
 * gw_logging.c -- extracted from gateway/server.c monolith.
 * Real implementation of one gateway-lifecycle concern. Public
 * gw_* protos stay in include/hermes_gateway.h; promoted cross-
 * module statics are in include/gw_server_internals.h.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_gateway.h"
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

void gw_log_open(void) {
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return;

    snprintf(g_gw_log_path, sizeof(g_gw_log_path),
             "%s/.slermes/logs/gateway.log", home);

    struct stat st;
    if (stat(g_gw_log_path, &st) == 0 && st.st_size > GW_LOG_MAX_BYTES) {
        char old[GW_LOG_PATH_MAX];
        snprintf(old, sizeof(old), "%s.1", g_gw_log_path);
        rename(g_gw_log_path, old);
    }

    g_gw_log_fp = fopen(g_gw_log_path, "a");
}

void gw_log_close(void) {
    if (g_gw_log_fp) { fclose(g_gw_log_fp); g_gw_log_fp = NULL; }
}

void gw_rate_limit_init(int idx, double tokens_per_sec, double max_burst) {
    if (idx < 0 || idx >= GW_MAX_PLATFORMS) return;
    g_gw.rate_limiters[idx].tokens_per_sec = tokens_per_sec;
    g_gw.rate_limiters[idx].max_tokens = max_burst;
    g_gw.rate_limiters[idx].tokens = max_burst;
    g_gw.rate_limiters[idx].last_refill = gw_mono_time();
}

bool gw_rate_limit_check(int idx) {
    if (idx < 0 || idx >= GW_MAX_PLATFORMS) return true; /* no limit if out of range */

    gw_rate_limiter_t *rl = &g_gw.rate_limiters[idx];
    double now = gw_mono_time();

    /* Refill tokens based on elapsed time */
    double elapsed = now - rl->last_refill;
    rl->tokens += elapsed * rl->tokens_per_sec;
    if (rl->tokens > rl->max_tokens)
        rl->tokens = rl->max_tokens;
    rl->last_refill = now;

    if (rl->tokens >= 1.0) {
        rl->tokens -= 1.0;
        return true; /* allowed */
    }
    return false; /* rate-limited */
}

void gw_pool_return_client(http_client_t *client, const char *endpoint) {
    if (!client) return;

    pthread_mutex_lock(&g_gw.pool_mutex);

    for (int i = 0; i < g_gw.pool_count; i++) {
        if (g_gw.http_pool[i].client == client) {
            g_gw.http_pool[i].in_use = false;
            g_gw.http_pool[i].last_used = gw_mono_time();
            pthread_mutex_unlock(&g_gw.pool_mutex);
            return;
        }
    }

    /* Not found in pool — free it */
    pthread_mutex_unlock(&g_gw.pool_mutex);
    http_client_free(client);
}

void gw_pool_cleanup(void) {
    pthread_mutex_lock(&g_gw.pool_mutex);
    double now = gw_mono_time();
    double expiry = g_gw.pool_keepalive_expiry > 0 ? g_gw.pool_keepalive_expiry : 300.0;
    for (int i = 0; i < g_gw.pool_count; i++) {
        if (!g_gw.http_pool[i].in_use &&
            (now - g_gw.http_pool[i].last_used) > expiry) {
            http_client_free(g_gw.http_pool[i].client);
            if (i < g_gw.pool_count - 1) {
                g_gw.http_pool[i] = g_gw.http_pool[g_gw.pool_count - 1];
            }
            g_gw.pool_count--;
            i--;
        }
    }
    pthread_mutex_unlock(&g_gw.pool_mutex);
}
