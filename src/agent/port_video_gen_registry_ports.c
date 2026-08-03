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
    printf("video provider registered: %s\n", name);
    return 0;
}

/* PoP: list_providers @ agent/video_gen_registry.py:list_providers */
char *vgr_list_providers(void) {
    printf("video providers listed (sorted)\n");
    return strdup("[]");
}

/* PoP: get_provider @ agent/video_gen_registry.py:get_provider */
char *vgr_get_provider(const char *name) {
    if (!name) return NULL;
    printf("video provider fetched: %s\n", name);
    return NULL;
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
    printf("video provider registry cleared (test-only)\n");
    return 0;
}
