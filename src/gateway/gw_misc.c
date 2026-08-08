/*
 * gw_misc.c -- extracted from gateway/server.c monolith.
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

/* ── Graceful shutdown request (port of GatewayRunner.stop()) ──────────
 * Signal-safe seam. The signal handler (or programmatic caller) records
 * the request by writing a byte to a self-pipe; the main thread blocks
 * on that pipe inside gw_wait_for_shutdown_request, which returns once
 * the byte arrives. This avoids the deadlock that a plain
 * pthread_cond_signal / pthread_mutex_lock inside a real-time signal
 * handler would cause (the main thread holds the cond's internal mutex
 * in pthread_cond_wait). The drain sequence itself runs on the main
 * thread, where socket I/O and session access are safe. */

static pthread_mutex_t g_shutdown_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_shutdown_cond  = PTHREAD_COND_INITIALIZER;
static volatile int    g_shutdown_fd_read  = -1;  /* self-pipe read end  */
static volatile int    g_shutdown_fd_write = -1;  /* self-pipe write end */
static volatile bool   g_shutdown_requested = false;
static char            g_shutdown_reason[64] = "";
static volatile sig_atomic_t g_sig_number = 0;   /* stashed by handle_signal */

/* One-shot pipe bootstrap (signal-safe: only uses pipe(2) / write(2)). */
static void gw_selfpipe_init(void) {
    if (g_shutdown_fd_read != -1) return;
    int p[2];
    if (pipe(p) == 0) {
        g_shutdown_fd_read  = p[0];
        g_shutdown_fd_write = p[1];
    }
}

void handle_signal(int sig) {
    /* Async-signal-safe: we only set sig_atomic_t / volatile flags and
     * write the self-pipe. No snprintf / mutex / cond calls inside here.
     * The human-readable reason is derived by gw_wait_for_shutdown_request
     * after it wakes (in the main thread). */
    static volatile sig_atomic_t sig_number = 0;
    sig_number = sig;
    g_sig_number = sig;
    g_shutdown_requested = true;
    if (g_shutdown_fd_read == -1) gw_selfpipe_init();
    if (g_shutdown_fd_write >= 0) {
        char byte = 1;
        (void)write(g_shutdown_fd_write, &byte, 1);
    }
}

void gw_request_shutdown(const char *reason) {
    if (!reason) reason = "shutdown";
    pthread_mutex_lock(&g_shutdown_mutex);
    if (!g_shutdown_requested) {
        snprintf(g_shutdown_reason, sizeof(g_shutdown_reason), "%s", reason);
        g_shutdown_requested = true;
    }
    pthread_mutex_unlock(&g_shutdown_mutex);

    gw_selfpipe_init();
    if (g_shutdown_fd_write >= 0) {
        char byte = 1;
        (void)write(g_shutdown_fd_write, &byte, 1);
    } else {
        pthread_cond_broadcast(&g_shutdown_cond);
    }
}

bool gw_shutdown_requested(void) {
    pthread_mutex_lock(&g_shutdown_mutex);
    bool req = g_shutdown_requested;
    pthread_mutex_unlock(&g_shutdown_mutex);
    return req;
}

const char *gw_shutdown_reason(void) {
    pthread_mutex_lock(&g_shutdown_mutex);
    const char *r = g_shutdown_requested ? g_shutdown_reason : NULL;
    pthread_mutex_unlock(&g_shutdown_mutex);
    return r;
}

void gw_wait_for_shutdown_request(void) {
    gw_selfpipe_init();
    /* Block on the self-pipe read end until the signal handler writes a
     * wakeup byte (or a programmatic gw_request_shutdown does). The
     * sigaction in hermes_gateway_main is installed WITHOUT SA_RESTART,
     * so a signal that interrupts the read() returns EINTR and we loop —
     * the byte is then drained on the next iteration. */
    if (g_shutdown_fd_read >= 0) {
        for (;;) {
            char byte;
            ssize_t n = read(g_shutdown_fd_read, &byte, 1);
            if (n > 0) break;              /* wakeup byte drained */
            if (n < 0 && errno == EINTR) continue;  /* signal interrupted */
            if (n < 0) continue;          /* other transient error: retry */
            break;                        /* n == 0 (EOF) */
        }
    } else {
        pthread_mutex_lock(&g_shutdown_mutex);
        while (!g_shutdown_requested)
            pthread_cond_wait(&g_shutdown_cond, &g_shutdown_mutex);
        pthread_mutex_unlock(&g_shutdown_mutex);
    }

    /* If woken by signal, derive the reason string here (main thread,
     * so snprintf is safe). The signal handler stashed the number. */
    if (g_shutdown_requested && g_shutdown_reason[0] == '\0') {
        const char *r = "signal";
        if (g_sig_number == SIGTERM) r = "SIGTERM";
        else if (g_sig_number == SIGINT) r = "SIGINT";
        snprintf(g_shutdown_reason, sizeof(g_shutdown_reason), "%s", r);
    }
}

/* Initialize the shutdown seam before signal handlers fire. */
void gw_shutdown_seam_init(void) {
    gw_selfpipe_init();
}
