/*
 * plugin_achievements_api.c — canonical C11 port of
 * plugins/hermes-achievements/dashboard/plugin_api.py (core surface).
 *
 * Achievements dashboard plugin backend: tiered achievement evaluation over
 * session metrics, with a TTL snapshot cache and a scan-status state machine.
 * Mounted at /api/plugins/hermes-achievements/ by the dashboard.
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* PoP: pa_snapshot_ttl @ plugins/hermes-achievements/dashboard/plugin_api.py:SNAPSHOT_TTL_SECONDS */
int pa_snapshot_ttl_seconds(void) { return 120; }

/* PoP: pa_scan_status @ plugins/hermes-achievements/dashboard/plugin_api.py:_SCAN_STATUS */
static const char *PA_SCAN_STATES[] = {"idle","scanning","done",NULL};
const char *const *pa_scan_states(void) { return PA_SCAN_STATES; }

static pthread_mutex_t g_pa_lock = PTHREAD_MUTEX_INITIALIZER;
static json_t *g_snapshot_cache = NULL;
static time_t g_snapshot_cache_at = 0;
static char g_scan_status[16] = "idle";

/* PoP: pa_get_scan_status @ plugins/.../plugin_api.py:_SCAN_STATUS getter */
void pa_get_scan_status(char *out, size_t out_sz) {
    pthread_mutex_lock(&g_pa_lock);
    snprintf(out, out_sz, "%s", g_scan_status);
    pthread_mutex_unlock(&g_pa_lock);
}

/* PoP: pa_set_scan_status @ plugins/.../plugin_api.py state transitions */
void pa_set_scan_status(const char *state) {
    pthread_mutex_lock(&g_pa_lock);
    snprintf(g_scan_status, sizeof g_scan_status, "%s", state);
    pthread_mutex_unlock(&g_pa_lock);
}

/* PoP: pa_tier_for_count @ plugins/.../plugin_api.py (tiered achievement eval)
 * Returns the tier index (0-based) for a metric count. -1 = no tier. */
int pa_tier_for_count(int count) {
    static const int thresholds[] = {1, 10, 50, 100, 500}; /* bronze..diamond */
    int tier = -1;
    for (size_t i = 0; i < sizeof thresholds / sizeof thresholds[0]; i++)
        if (count >= thresholds[i]) tier = (int)i;
    return tier;
}

/* PoP: pa_snapshot_cached @ plugins/.../plugin_api.py snapshot cache check
 * Returns cached snapshot JSON when fresh (< TTL), else NULL and clears. */
json_t *pa_snapshot_cached(void) {
    pthread_mutex_lock(&g_pa_lock);
    if (g_snapshot_cache &&
        (time(NULL) - g_snapshot_cache_at) < pa_snapshot_ttl_seconds()) {
        pthread_mutex_unlock(&g_pa_lock);
        return g_snapshot_cache;
    }
    if (g_snapshot_cache) { json_free(g_snapshot_cache); g_snapshot_cache = NULL; }
    pthread_mutex_unlock(&g_pa_lock);
    return NULL;
}

/* PoP: pa_snapshot_store @ plugins/.../plugin_api.py snapshot cache fill */
void pa_snapshot_store(json_t *snapshot) {
    pthread_mutex_lock(&g_pa_lock);
    if (g_snapshot_cache) json_free(g_snapshot_cache);
    g_snapshot_cache = snapshot;
    g_snapshot_cache_at = time(NULL);
    pthread_mutex_unlock(&g_pa_lock);
}
