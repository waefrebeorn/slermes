/*
 * port_gateway_platforms_api_server.c — Port of Python gateway/platforms/api_server.py
 */
#include <stdio.h>
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

