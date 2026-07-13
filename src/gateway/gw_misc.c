/*
 * gw_misc.c -- extracted from gateway/server.c monolith.
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

double gw_cooldown_remaining(int plat_idx) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return 0.0;
    double remaining = g_gw.platform_cooldown_sec[plat_idx] -
        (gw_mono_time() - g_gw.platform_last_action[plat_idx]);
    return remaining > 0.0 ? remaining : 0.0;
}

void gw_cooldown_mark(int plat_idx) {
    if (plat_idx >= 0 && plat_idx < GW_MAX_PLATFORMS)
        g_gw.platform_last_action[plat_idx] = gw_mono_time();
}

double gw_reconnect_delay(int plat_idx) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return GW_RECONNECT_BASE_SEC;

    g_gw.reconnect_attempt[plat_idx]++;

    /* Exponential: base * 2 ^ (attempt - 1) with jitter */
    double base = GW_RECONNECT_BASE_SEC *
        (1 << (g_gw.reconnect_attempt[plat_idx] - 1));
    if (base > GW_RECONNECT_MAX_SEC) base = GW_RECONNECT_MAX_SEC;

    /* Add random jitter ±10% */
    double jitter = ((double)rand() / RAND_MAX) * 2.0 * GW_RECONNECT_JITTER * base
        - GW_RECONNECT_JITTER * base;
    double delay = base + jitter;
    if (delay < GW_RECONNECT_BASE_SEC) delay = GW_RECONNECT_BASE_SEC;

    g_gw.reconnect_delay_sec[plat_idx] = delay;
    return delay;
}

void gw_reconnect_reset(int plat_idx) {
    if (plat_idx >= 0 && plat_idx < GW_MAX_PLATFORMS) {
        g_gw.reconnect_attempt[plat_idx] = 0;
        g_gw.reconnect_delay_sec[plat_idx] = 0.0;
    }
}

bool gw_set_proxy(int plat_idx, const char *proxy_url) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return false;
    if (!proxy_url || !*proxy_url) {
        g_gw.proxy_enabled[plat_idx] = false;
        g_gw.platform_proxy[plat_idx][0] = '\0';
        return true;
    }
    snprintf(g_gw.platform_proxy[plat_idx], sizeof(g_gw.platform_proxy[plat_idx]),
             "%s", proxy_url);
    g_gw.proxy_enabled[plat_idx] = true;
    return true;
}

void handle_signal(int sig) {
    (void)sig;
    printf("\n[gateway] Shutting down...\n");
    g_gw.running = false;
}
