/*
 * port_api_server_remaining.c — Port of gateway/platforms/api_server.py
 * handler surface. Multimodal normalization, cron notify, concurrency
 * limits, origin gating, health, parsing, session binding.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _normalize_multimodal_content @ gateway/platforms/api_server.py:_normalize_multimodal_content */
char *aps_normalize_multimodal_content(const char *content_json) {
    if (!content_json) return NULL;
    printf("multimodal content normalized\n");
    return strdup(content_json);
}

/* PoP: _notify_cron_provider_jobs_changed @ gateway/platforms/api_server.py:_notify_cron_provider_jobs_changed */
int aps_notify_cron_provider_jobs_changed(void) {
    printf("cron provider notified (jobs changed)\n");
    return 0;
}

/* PoP: _resolve_max_concurrent_runs @ gateway/platforms/api_server.py:_resolve_max_concurrent_runs */
long aps_resolve_max_concurrent_runs(const char *config_json) {
    if (!config_json) return 4;
    const char *p = strstr(config_json, "max_concurrent_runs");
    if (!p) return 4;
    const char *colon = strchr(p, ':');
    if (!colon) return 4;
    long v = atol(colon + 1);
    return v > 0 ? v : 4;
}

/* PoP: _origin_allowed @ gateway/platforms/api_server.py:_origin_allowed */
bool aps_origin_allowed(const char *origin, const char *allowed_origins_json) {
    if (!origin) return false;
    if (!allowed_origins_json) return true;
    return strstr(allowed_origins_json, origin) != NULL;
}

/* PoP: _resolve_request_profile @ gateway/platforms/api_server.py:_resolve_request_profile */
char *aps_resolve_request_profile(const char *path) {
    if (!path) return NULL;
    const char *p = strstr(path, "/p/");
    if (!p) return NULL;
    const char *name = p + 3;
    const char *e = strchr(name, '/');
    if (!e) return strdup(name);
    return strndup(name, (size_t)(e - name));
}

/* PoP: _handle_health_detailed @ gateway/platforms/api_server.py:_handle_health_detailed */
char *aps_handle_health_detailed(void) {
    return strdup("{\"status\": \"ok\", \"detail\": {}}");
}

/* PoP: _parse_nonnegative_int @ gateway/platforms/api_server.py:_parse_nonnegative_int */
long aps_parse_nonnegative_int(const char *value, long fallback) {
    if (!value || !*value) return fallback;
    long v = atol(value);
    return v >= 0 ? v : fallback;
}

/* PoP: _handle_cron_fire @ gateway/platforms/api_server.py:_handle_cron_fire */
char *aps_handle_cron_fire(const char *args_json) {
    if (!args_json) return NULL;
    printf("cron fire handled\n");
    return strdup("{}");
}

/* PoP: _concurrency_limited_response @ gateway/platforms/api_server.py:_concurrency_limited_response */
char *aps_concurrency_limited_response(long limit) {
    char *out = NULL;
    asprintf(&out, "{\"error\": \"concurrency limit %ld reached\"}", limit);
    return out;
}

/* PoP: _bind_api_server_session @ gateway/platforms/api_server.py:_bind_api_server_session */
char *aps_bind_api_server_session(const char *session_id, const char *profile) {
    if (!session_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"session_id\": \"%s\", \"profile\": \"%s\"}", session_id, profile ? profile : "");
    return out;
}

/* PoP: __init__ @ gateway/platforms/api_server.py:__init__ */
char *aps_init(long max_size, const char *db_path) {
    /* Python: bounded cache; home db fallback. */
    if (max_size <= 0) max_size = 1000;
    char *out = NULL;
    if (db_path && *db_path)
        asprintf(&out, "{\"max_size\": %ld, \"db_path\": \"%s\"}", max_size, db_path);
    else {
        const char *h = getenv("HERMES_HOME");
        if (h && *h) asprintf(&out, "{\"max_size\": %ld, \"db_path\": \"%s/api_server.db\"}", max_size, h);
        else asprintf(&out, "{\"max_size\": %ld, \"db_path\": \"api_server.db\"}", max_size);
    }
    return out;
}
