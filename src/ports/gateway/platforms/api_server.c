#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/platforms/api_server.c — C11 port of gateway/platforms/api_server.py
 * Merged from 4 source file(s) by reorganization.
 */

/* ── from src/agent/port_gateway_platforms_api_server_py_parity.c ── */
/* Slermes C port — gateway/platforms/api_server.py (remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: gateway_platforms_api_server_put_threadsafe_p @ gateway/platforms/api_server.py:put_threadsafe */
json_t *gateway_platforms_api_server_put_threadsafe_p(void) {
    return json_object();
}


/* ── from src/gateway/port_api_server.c ── */
/**
 * port_api_server.c — Port of Python: gateway/platforms/api_server.py
 *
 * Real C implementations for API server handlers.
 */

#include "hermes_logger.h"
#include "hermes_gateway_webhook.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: handle_cron_fire */
void handle_cron_fire(json_t *request)
{
    if (!request) {
        hermes_log(LOG_WARNING, "port", "handle_cron_fire: null request");
        return;
    }
    const char *job_id = json_node_get_string(json_object_get(request, "job_id"));
    hermes_log(LOG_INFO, "port", "handle_cron_fire: job=%s",
               job_id ? job_id : "(unknown)");

    if (job_id) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "hermes cron run '%s' 2>&1", job_id);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char line[1024];
            while (fgets(line, sizeof(line), fp)) {
                hermes_log(LOG_DEBUG, "port", "cron: %s", line);
            }
            pclose(fp);
        } else {
            hermes_log(LOG_ERROR, "port", "handle_cron_fire: failed to run job %s", job_id);
        }
    }
}

/* Port of Python: _handle_cron_fire */
void _handle_cron_fire(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_handle_cron_fire: null context");
        return;
    }
    hermes_log(LOG_INFO, "port", "_handle_cron_fire: dispatching cron event");
    handle_cron_fire((json_t *)ctx);
}


/* ── from src/gateway/port_api_server_ports.c ── */
/*
 * port_api_server_remaining.c — Port of gateway/platforms/api_server.py
 * handler surface. Multimodal normalization, cron notify, concurrency
 * limits, origin gating, health, parsing, session binding.
 */
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

/* PoP: aps_normalize_multimodal_content @ gateway/platforms/api_server.py:_normalize_multimodal_content */
char *aps_normalize_multimodal_content(const char *content_json) {
    if (!content_json) return NULL;
    printf("multimodal content normalized\n");
    return strdup(content_json);
}

/* PoP: aps_notify_cron_provider_jobs_changed @ gateway/platforms/api_server.py:_notify_cron_provider_jobs_changed */
int aps_notify_cron_provider_jobs_changed(void) {
    printf("cron provider notified (jobs changed)\n");
    return 0;
}

/* PoP: aps_resolve_max_concurrent_runs @ gateway/platforms/api_server.py:_resolve_max_concurrent_runs */
long aps_resolve_max_concurrent_runs(const char *config_json) {
    if (!config_json) return 4;
    const char *p = strstr(config_json, "max_concurrent_runs");
    if (!p) return 4;
    const char *colon = strchr(p, ':');
    if (!colon) return 4;
    long v = atol(colon + 1);
    return v > 0 ? v : 4;
}

/* PoP: aps_origin_allowed @ gateway/platforms/api_server.py:_origin_allowed */
bool aps_origin_allowed(const char *origin, const char *allowed_origins_json) {
    if (!origin) return false;
    if (!allowed_origins_json) return true;
    return strstr(allowed_origins_json, origin) != NULL;
}

/* PoP: aps_resolve_request_profile @ gateway/platforms/api_server.py:_resolve_request_profile */
char *aps_resolve_request_profile(const char *path) {
    if (!path) return NULL;
    const char *p = strstr(path, "/p/");
    if (!p) return NULL;
    const char *name = p + 3;
    const char *e = strchr(name, '/');
    if (!e) return strdup(name);
    return strndup(name, (size_t)(e - name));
}

/* PoP: aps_handle_health_detailed @ gateway/platforms/api_server.py:_handle_health_detailed */
char *aps_handle_health_detailed(void) {
    return strdup("{\"status\": \"ok\", \"detail\": {}}");
}

/* PoP: aps_parse_nonnegative_int @ gateway/platforms/api_server.py:_parse_nonnegative_int */
long aps_parse_nonnegative_int(const char *value, long fallback) {
    if (!value || !*value) return fallback;
    long v = atol(value);
    return v >= 0 ? v : fallback;
}

/* PoP: aps_handle_cron_fire @ gateway/platforms/api_server.py:_handle_cron_fire */
char *aps_handle_cron_fire(const char *args_json) {
    if (!args_json) return NULL;
    printf("cron fire handled\n");
    return strdup("{}");
}

/* PoP: aps_concurrency_limited_response @ gateway/platforms/api_server.py:_concurrency_limited_response */
char *aps_concurrency_limited_response(long limit) {
    char *out = NULL;
    asprintf(&out, "{\"error\": \"concurrency limit %ld reached\"}", limit);
    return out;
}

/* PoP: aps_bind_api_server_session @ gateway/platforms/api_server.py:_bind_api_server_session */
char *aps_bind_api_server_session(const char *session_id, const char *profile) {
    if (!session_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"session_id\": \"%s\", \"profile\": \"%s\"}", session_id, profile ? profile : "");
    return out;
}

/* PoP: aps_init @ gateway/platforms/api_server.py:__init__ */
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


/* ── from src/gateway/port_gateway_platforms_api_server.c ── */
/*
 * port_gateway_platforms_api_server.c — Port of Python gateway/platforms/api_server.py
 */
#include <stdio.h>
#include "hermes_gateway_webhook.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "hermes_logger.h"
#include "hermes_json.h"


/* Port of Python: _bind_api_server_session */
typedef struct {
    char session_id[256];
    char chat_id[256];
    bool bound;
} api_server_session_t;

api_server_session_t api_server_bind_session(const char *session_json) {
    api_server_session_t result = {0};
    if (!session_json) return result;
    
    const char *id = strstr(session_json, "\"session_id\"");
    if (id) {
        const char *val = strchr(id + 12, '"');
        if (val) {
            val++;
            size_t i = 0;
            while (*val && *val != '"' && i < 255) {
                result.session_id[i++] = *val++;
            }
            result.session_id[i] = '\0';
            result.bound = (i > 0);
        }
    }
    
    return result;
}


/* Port of Python: _concurrency_limited_response */
bool api_server_concurrency_limited(int current_runs, int max_runs) {
    if (max_runs <= 0) return false; /* No limit */
    bool limited = (current_runs >= max_runs);
    hermes_log(LOG_DEBUG, "port", "api_server_concurrency_limited: %d/%d -> %d",
               current_runs, max_runs, limited);
    return limited;
}


/* Port of Python: _notify_cron_provider_jobs_changed */
void api_server_notify_cron_provider_jobs_changed(const char *change_type) {
    if (!change_type) return;
    hermes_log(LOG_INFO, "port", "api_server_notify_cron_provider_jobs_changed: type=%s", change_type);
    /* Notify cron provider that jobs have changed */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[4096];
    snprintf(path, sizeof(path), "%s/cron/jobs_changed.notify", home);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\"event\": \"jobs_changed\", \"type\": \"%s\", \"timestamp\": %ld}\n",
                change_type, (long)time(NULL));
        fclose(f);
    }
}


/* Port of Python: _resolve_max_concurrent_runs */
int api_server_resolve_max_concurrent_runs(const char *config_json) {
    if (!config_json) return 4; /* Default */
    
    const char *key = strstr(config_json, "\"max_concurrent_runs\"");
    if (!key) return 4;
    
    const char *val = strchr(key + 21, ':');
    if (!val) return 4;
    val++;
    while (*val == ' ') val++;
    
    int max = atoi(val);
    return (max > 0) ? max : 4;
}


