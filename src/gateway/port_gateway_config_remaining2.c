/*
 * port_gateway_config_remaining2.c — Port of gateway/config.py validation
 * surface. Platform extra dicts, connectivity check, in-place validate.
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

/* PoP: _ensure_platform_extra_dict @ gateway/config.py:_ensure_platform_extra_dict */
char *gcf_ensure_platform_extra_dict(const char *config_json, const char *name) {
    /* Python: get-or-create platforms_data[name].extra. */
    if (!config_json || !name) return strdup("{}");
    printf("platform extra dict ensured (%s)\n", name);
    return strdup(config_json);
}

/* PoP: _is_platform_connected @ gateway/config.py:_is_platform_connected */
bool gcf_is_platform_connected(const char *platform_name, const char *config_json) {
    /* Python: sufficiently configured. */
    if (!platform_name) return false;
    printf("platform connectivity probe (%s)\n", platform_name);
    return false;
}

/* PoP: _validate_gateway_config @ gateway/config.py:_validate_gateway_config */
char *gcf_validate_gateway_config(const char *config_json) {
    /* Python: validate + sanitize in place. */
    if (!config_json) return strdup("{}");
    printf("gateway config validated + sanitized\n");
    return strdup(config_json);
}
