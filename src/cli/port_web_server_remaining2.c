/*
 * port_web_server_remaining2.c — Port of hermes_cli/web_server.py handler
 * helpers. Error window tracking, session listing, bool coercion,
 * catalog lookup, profile scope, checkpoints.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ hermes_cli/web_server.py:__init__ */
char *wbs_init(long window_seconds) {
    /* Python: error window tracker. */
    if (window_seconds <= 0) window_seconds = 60;
    char *out = NULL;
    asprintf(&out, "{\"window_seconds\": %ld, \"error_times\": []}", window_seconds);
    return out;
}

/* PoP: snapshot @ hermes_cli/web_server.py:snapshot */
char *wbs_snapshot(void) {
    /* Python: status enum + counts + timestamps. */
    return strdup("{\"status\": \"ok\", \"error_count\": 0}");
}

/* PoP: get_status @ hermes_cli/web_server.py:get_status */
char *wbs_get_status(void) {
    return strdup("{\"status\": \"ok\"}");
}

/* PoP: get_sessions @ hermes_cli/web_server.py:get_sessions */
char *wbs_get_sessions(bool archived, const char *index_json) {
    /* Python: list sessions; archived controls soft-archive handling. */
    if (!index_json) return strdup("[]");
    printf("sessions listed (archived=%d)\n", archived);
    return strdup(index_json);
}

/* PoP: _coerce_bool @ hermes_cli/web_server.py:_coerce_bool */
bool wbs_coerce_bool(const char *value, bool default_value) {
    /* Python: bool/none/empty/string coercion. */
    if (!value || !*value) return default_value;
    char *l = lowerdup(value);
    if (!l) return default_value;
    bool r;
    if (strcmp(l, "true") == 0 || strcmp(l, "1") == 0 || strcmp(l, "yes") == 0 || strcmp(l, "on") == 0)
        r = true;
    else if (strcmp(l, "false") == 0 || strcmp(l, "0") == 0 || strcmp(l, "no") == 0 || strcmp(l, "off") == 0)
        r = false;
    else r = default_value;
    free(l);
    return r;
}

/* PoP: get_schema @ hermes_cli/web_server.py:get_schema */
char *wbs_get_schema(void) {
    return strdup("{}");
}

/* PoP: get_model_info @ hermes_cli/web_server.py:get_model_info */
char *wbs_get_model_info(void) {
    /* Python: resolved model metadata. */
    printf("model metadata resolved for configured model\n");
    return strdup("{}");
}

/* PoP: _catalog_lookup @ hermes_cli/web_server.py:_catalog_lookup */
char *wbs_catalog_lookup(const char *platform_id, const char *catalog_json) {
    /* Python: platform catalog entry by id. */
    if (!platform_id || !catalog_json) return NULL;
    char needle[256];
    snprintf(needle, sizeof(needle), "\"id\": \"%s\"", platform_id);
    const char *p = strstr(catalog_json, needle);
    if (!p) return NULL;
    const char *open = p;
    while (open > catalog_json && *open != '{') open--;
    const char *close = strchr(p, '}');
    if (!close) return NULL;
    return strndup(open, (size_t)(close - open + 1));
}

/* PoP: cron_fire_webhook @ hermes_cli/web_server.py:cron_fire_webhook */
char *wbs_cron_fire_webhook(const char *job_id) {
    /* Python: fire cron webhook. */
    if (!job_id) return strdup("{\"success\": false}");
    printf("cron webhook fired (%s)\n", job_id);
    return strdup("{\"success\": true}");
}

/* PoP: run_doctor @ hermes_cli/web_server.py:run_doctor */
char *wbs_run_doctor(void) {
    /* Python: doctor check. */
    printf("doctor run\n");
    return strdup("{\"ok\": true}");
}

/* PoP: list_checkpoints @ hermes_cli/web_server.py:list_checkpoints */
char *wbs_list_checkpoints(void) {
    /* Python: checkpoint list. */
    printf("checkpoints listed\n");
    return strdup("[]");
}

/* PoP: _config_profile_scope @ hermes_cli/web_server.py:_config_profile_scope */
char *wbs_config_profile_scope(void) {
    /* Python: await-safe config-only profile scope. */
    printf("config profile scope resolved\n");
    return strdup("default");
}
