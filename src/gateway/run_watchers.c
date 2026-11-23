/*
 * run_watchers.c — faithful C11 port of upstream Hermes gateway/run_watchers.py.
 *
 * PoP: w7_runwatchers_db91ce_interruptible_sleep_ticks @ gateway/run_watchers.py:_interruptible_sleep
 * PoP: w7_runwatchers_db91ce_prune_due @ gateway/run_watchers.py:GatewaySessionWatchersMixin._session_housekeeping
 * PoP: w7_runwatchers_db91ce_stall_timeout @ gateway/run_watchers.py:GatewaySessionWatchersMixin._session_stall_timeout_seconds
 * PoP: w7_runwatchers_db91ce_drain_releases @ gateway/run_watchers.py:drain_pending_cache_releases
 */
#include "hermes_json.h"
#include "noc_w7_runwatchers_db91ce.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* _SESSION_STORE_PRUNE_INTERVAL = 3600.0 (once per hour). */
#define RW_SESSION_STORE_PRUNE_INTERVAL 3600.0
/* _float_env default for HERMES_SESSION_STALL_TIMEOUT. */
#define RW_STALL_TIMEOUT_DEFAULT 300.0

int w7_runwatchers_db91ce_interruptible_sleep_ticks(int seconds, int running_ticks) {
    int ticks = 0;
    if (running_ticks < 0) running_ticks = 0;
    for (int i = 0; i < seconds; i++) {
        if (running_ticks <= 0) break;   /* runner._running flipped false */
        ticks++;
        running_ticks--;
    }
    return ticks;
}

int w7_runwatchers_db91ce_prune_due(double now, double last_ts) {
    return (now - last_ts) > RW_SESSION_STORE_PRUNE_INTERVAL ? 1 : 0;
}

double w7_runwatchers_db91ce_stall_timeout(const char *env_value) {
    if (!env_value || !*env_value) return RW_STALL_TIMEOUT_DEFAULT;
    const char *p = env_value;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return RW_STALL_TIMEOUT_DEFAULT;
    errno = 0;
    char *end = NULL;
    double value = strtod(p, &end);
    if (errno == ERANGE || end == p) return RW_STALL_TIMEOUT_DEFAULT;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return RW_STALL_TIMEOUT_DEFAULT;
    return value;
}

struct json_t *w7_runwatchers_db91ce_drain_releases(const struct json_t *release_keys) {
    struct json_t *called = json_array();
    if (!release_keys || release_keys->type != JSON_ARRAY) return called;
    for (size_t i = 0; i < json_len(release_keys); i++) {
        struct json_t *key = json_get((struct json_t *)release_keys, i);
        if (key && key->type == JSON_STRING && key->str_val)
            json_append(called, json_string(key->str_val));
    }
    return called;
}
