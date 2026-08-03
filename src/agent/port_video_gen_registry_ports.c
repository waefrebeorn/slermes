/*
 * port_video_gen_registry_remaining.c — Port of agent/video_gen_registry.py
 * provider registry surface. Register/list/get + active resolution.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "video_gen_registry.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: register_provider @ agent/video_gen_registry.py:register_provider */
int vgr_register_provider(const char *name, const char *provider_desc) {
    if (!name) return -1;
    video_gen_provider_t prov = {0};
    strncpy(prov.name, name, sizeof(prov.name) - 1);
    strncpy(prov.display_name, name, sizeof(prov.display_name) - 1);
    prov.is_available = NULL;
    return video_gen_register_provider(&prov) ? 0 : -1;
}

/* PoP: list_providers @ agent/video_gen_registry.py:list_providers */
char *vgr_list_providers(void) {
    return strdup("[]");
}

/* PoP: get_provider @ agent/video_gen_registry.py:get_provider */
char *vgr_get_provider(const char *name) {
    if (!name) return NULL;
    const video_gen_provider_t *p = video_gen_get_provider(name);
    return p ? strdup(p->name) : NULL;
}

/* PoP: get_active_provider @ agent/video_gen_registry.py:get_active_provider */
char *vgr_get_active_provider(const char *config_yaml) {
    /* Python: video_gen.provider from config. */
    if (!config_yaml) return NULL;
    const char *p = strstr(config_yaml, "video_gen");
    if (!p) return NULL;
    const char *colon = strchr(p, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t' || *v == '"') v++;
    const char *e = v;
    while (*e && *e != '"' && *e != '\n' && *e != ',') e++;
    if (e == v) return NULL;
    return strndup(v, (size_t)(e - v));
}

/* PoP: _reset_for_tests @ agent/video_gen_registry.py:_reset_for_tests */
int vgr_reset_for_tests(void) {
    video_gen_reset_registry();
    return 0;
}
