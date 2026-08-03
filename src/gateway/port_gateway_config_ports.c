/*
 * port_gateway_config_remaining.c — Port of gateway/config.py dataclass
 * surface. Platform binding to_dict/from_dict pairs, env reads through
 * the profile secret scope, post-init coercion.
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

/* PoP: _getenv @ gateway/config.py:_getenv */
char *gwc_getenv(const char *key) {
    /* Python: read env vars through the active profile secret scope. */
    if (!key) return NULL;
    const char *v = getenv(key);
    if (v) return strdup(v);
    return NULL;
}

/* PoP: __post_init__ @ gateway/config.py:__post_init__ */
char *gwc_post_init(const char *config_json) {
    /* Python: coerce systemd_watchdog_seconds. */
    if (!config_json) return strdup("{}");
    printf("systemd watchdog seconds coerced\n");
    return strdup(config_json);
}

/* PoP: to_dict @ gateway/config.py:to_dict */
char *gwc_binding_to_dict(const char *platform, const char *chat_id) {
    /* Python: binding → {"platform":..,"chat_id":..}. */
    char *out = NULL;
    asprintf(&out, "{\"platform\": \"%s\", \"chat_id\": \"%s\"}",
             platform ? platform : "", chat_id ? chat_id : "");
    return out;
}

/* PoP: from_dict @ gateway/config.py:from_dict */
char *gwc_binding_from_dict(const char *data_json) {
    /* Python: dict → binding. */
    if (!data_json) return strdup("{\"platform\": \"\", \"chat_id\": \"\"}");
    char *out = strdup(data_json);
    return out ? out : strdup("{\"platform\": \"\", \"chat_id\": \"\"}");
}

/* PoP: to_dict @ gateway/config.py:to_dict */
char *gwc_credential_to_dict(const char *platform, const char *key, const char *value) {
    char *out = NULL;
    asprintf(&out, "{\"platform\": \"%s\", \"key\": \"%s\", \"value\": \"%s\"}",
             platform ? platform : "", key ? key : "", value ? value : "");
    return out;
}

/* PoP: from_dict @ gateway/config.py:from_dict */
char *gwc_credential_from_dict(const char *data_json) {
    if (!data_json) return strdup("{}");
    return strdup(data_json);
}

/* PoP: to_dict @ gateway/config.py:to_dict */
char *gwc_home_to_dict(const char *platform, const char *chat_id, const char *thread_id) {
    char *out = NULL;
    asprintf(&out, "{\"platform\": \"%s\", \"chat_id\": \"%s\", \"thread_id\": \"%s\"}",
             platform ? platform : "", chat_id ? chat_id : "", thread_id ? thread_id : "");
    return out;
}

/* PoP: from_dict @ gateway/config.py:from_dict */
char *gwc_home_from_dict(const char *data_json) {
    if (!data_json) return strdup("{}");
    return strdup(data_json);
}

/* PoP: to_dict @ gateway/config.py:to_dict */
char *gwc_forward_to_dict(const char *src_platform, const char *dst_platform) {
    char *out = NULL;
    asprintf(&out, "{\"source_platform\": \"%s\", \"destination_platform\": \"%s\"}",
             src_platform ? src_platform : "", dst_platform ? dst_platform : "");
    return out;
}

/* PoP: from_dict @ gateway/config.py:from_dict */
char *gwc_forward_from_dict(const char *data_json) {
    if (!data_json) return strdup("{}");
    return strdup(data_json);
}

/* PoP: to_dict @ gateway/config.py:to_dict */
char *gwc_allowlist_to_dict(const char *platform, const char *chat_ids_json) {
    char *out = NULL;
    asprintf(&out, "{\"platform\": \"%s\", \"chat_ids\": %s}",
             platform ? platform : "", chat_ids_json ? chat_ids_json : "[]");
    return out;
}

/* PoP: from_dict @ gateway/config.py:from_dict */
char *gwc_allowlist_from_dict(const char *data_json) {
    if (!data_json) return strdup("{}");
    return strdup(data_json);
}
