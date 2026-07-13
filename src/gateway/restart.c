/*
 * restart.c — extracted from gateway/helpers.c monolith.
 *
 * Real implementation home for the Python module it ports (no longer a
 * name-parity stub). Public prototypes stay in include/gateway_helpers.h
 * (or hermes_gateway.h); callers are unchanged.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
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
 *  Auto-continue and timestamp helpers
 *  Port of Python gateway/run.py:_home_target_env_var, _float_env,
 *  _is_fresh_gateway_interruption, etc.
 * ================================================================ */

/* Port of Python gateway/run.py:_home_target_env_var
 * Build the home-target env var name from a platform name.
 * e.g. "telegram" -> "TELEGRAM_HOME_CHANNEL" */
char *resolve_home_target_env(const char *platform_name) {
    if (!platform_name || !*platform_name) return NULL;
    size_t plen = strlen(platform_name);
    char *result = malloc(plen + 20);  /* _HOME_CHANNEL + NUL */
    if (!result) return NULL;
    for (size_t i = 0; i < plen; i++)
        result[i] = (char)toupper((unsigned char)platform_name[i]);
    memcpy(result + plen, "_HOME_CHANNEL", 14);
    result[plen + 13] = '\0';
    return result;
}


/* Port of Python gateway/run.py:_home_thread_env_var
 * Build the home-thread env var name: e.g. "TELEGRAM_HOME_CHANNEL_THREAD_ID" */
char *resolve_home_thread_env(const char *platform_name) {
    if (!platform_name || !*platform_name) return NULL;
    size_t plen = strlen(platform_name);
    char *result = malloc(plen + 27);  /* _HOME_CHANNEL_THREAD_ID + NUL */
    if (!result) return NULL;
    for (size_t i = 0; i < plen; i++)
        result[i] = (char)toupper((unsigned char)platform_name[i]);
    memcpy(result + plen, "_HOME_CHANNEL_THREAD_ID", 24);
    result[plen + 23] = '\0';
    return result;
}


/* Port of Python gateway/run.py:_float_env
 * Read an env var as float, falling back to default on typos/empty. */
double read_float_env(const char *name, double default_val) {
    if (!name) return default_val;
    const char *raw = getenv(name);
    if (!raw || !*raw) return default_val;
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end == raw || *end != '\0') return default_val;  /* parse failure */
    return val;
}


/* Port of Python gateway/run.py:_is_fresh_gateway_interruption
 * Return true when an interruption marker is fresh enough to auto-continue.
 * window_secs <= 0 disables the gate. */
bool is_fresh_gateway_interruption(double timestamp, double now, double window_secs) {
    if (window_secs <= 0.0) return true;
    return (now - timestamp) <= window_secs;
}

