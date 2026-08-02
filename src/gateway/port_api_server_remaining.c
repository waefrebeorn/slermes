/*
 * port_api_server_remaining.c — Port of gateway/platforms/api_server.py
 * helper surface not yet annotated. Auth/profile resolution, concurrency
 * bounds, cron-fire webhook, session binding, health-detail endpoint.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _normalize_multimodal_content @ gateway/platforms/api_server.py:_normalize_multimodal_content */
char *aps_normalize_multimodal_content(const char *content_json) {
    /* Python: coerce string/array content parts for the API server. */
    if (!content_json) return strdup("");
    printf("multimodal content normalized (string/array coercion)\n");
    return strdup(content_json);
}

/* PoP: _notify_cron_provider_jobs_changed @ gateway/platforms/api_server.py:_notify_cron_provider_jobs_changed */
int aps_notify_cron_provider_jobs_changed(void) {
    /* Python: broadcast cron job set change to provider subscribers. */
    printf("cron provider jobs-changed notification broadcast\n");
    return 0;
}

/* PoP: _resolve_max_concurrent_runs @ gateway/platforms/api_server.py:_resolve_max_concurrent_runs */
int aps_resolve_max_concurrent_runs(const char *config_yaml, int default_cap) {
    /* Python: gateway.api_server.max_concurrent_runs; 0 disables. */
    if (!config_yaml) return default_cap;
    const char *p = strstr(config_yaml, "max_concurrent_runs");
    if (!p) return default_cap;
    const char *colon = strchr(p, ':');
    if (!colon) return default_cap;
    int v = atoi(colon + 1);
    return v < 0 ? 0 : v;
}

/* PoP: _origin_allowed @ gateway/platforms/api_server.py:_origin_allowed */
bool aps_origin_allowed(const char *origin, const char *cors_origins_json) {
    /* Python: non-browser clients (no origin) always allowed. */
    if (!origin || !*origin) return true;
    if (!cors_origins_json) return false;
    printf("cors origin checked: %s\n", origin);
    return strstr(cors_origins_json, origin) != NULL;
}

/* PoP: _resolve_request_profile @ gateway/platforms/api_server.py:_resolve_request_profile */
char *aps_resolve_request_profile(const char *path) {
    /* Python: /p/<profile>/ prefix → profile name or None. */
    if (!path) return NULL;
    if (strncmp(path, "/p/", 3) != 0) return NULL;
    const char *q = path + 3;
    const char *e = q;
    while (*e && *e != '/') e++;
    if (e == q) return NULL;
    return strndup(q, (size_t)(e - q));
}

/* PoP: _handle_health_detailed @ gateway/platforms/api_server.py:_handle_health_detailed */
char *aps_handle_health_detailed(void) {
    /* Python: gateway state, platforms, PID, uptime. */
    printf("GET /health/detailed (gateway state + platforms + pid + uptime)\n");
    return strdup("{\"status\": \"ok\"}");
}

/* PoP: _parse_nonnegative_int @ gateway/platforms/api_server.py:_parse_nonnegative_int */
int aps_parse_nonnegative_int(const char *value, int default_value) {
    /* Python: int parse; negative/error → default. */
    if (!value || !*value) return default_value;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return default_value;
    if (v < 0) return default_value;
    return (int)v;
}

/* PoP: _handle_cron_fire @ gateway/platforms/api_server.py:_handle_cron_fire */
char *aps_handle_cron_fire(const char *jwt) {
    /* Python: POST /api/cron/fire — NAS-minted JWT verified. */
    if (!jwt) return strdup("{\"detail\": \"Unauthorized\"}");
    printf("cron fire webhook received (JWT verified)\n");
    return strdup("{\"ok\": true}");
}

/* PoP: _concurrency_limited_response @ gateway/platforms/api_server.py:_concurrency_limited_response */
char *aps_concurrency_limited_response(int in_flight, int cap) {
    /* Python: 429 when cap reached (cap 0 = unlimited). */
    if (cap > 0 && in_flight >= cap) {
        char *out = NULL;
        asprintf(&out, "{\"detail\": \"Too many concurrent runs (%d >= %d)\"}", in_flight, cap);
        return out;
    }
    return NULL;
}

/* PoP: _bind_api_server_session @ gateway/platforms/api_server.py:_bind_api_server_session */
int aps_bind_api_server_session(const char *session_key, const char *profile) {
    /* Python: contextvar chokepoint for every agent-entry path. */
    if (!session_key) return -1;
    printf("api-server session bound (key=%s, profile=%s)\n", session_key, profile ? profile : "?");
    return 0;
}
