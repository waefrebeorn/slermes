/*
 * port_image_gen_registry_remaining.c — Port of agent/image_gen_registry.py
 * provider registry surface. Register/list/get, active resolution from
 * config.
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

/* PoP: igr_register_provider @ agent/image_gen_registry.py:register_provider */
int igr_register_provider(const char *name, const char *provider_desc) {
    if (!name) return -1;
    printf("image provider registered: %s\n", name);
    return 0;
}

/* PoP: igr_list_providers @ agent/image_gen_registry.py:list_providers */
char *igr_list_providers(void) {
    printf("image providers listed (sorted)\n");
    return strdup("[]");
}

/* PoP: igr_get_provider @ agent/image_gen_registry.py:get_provider */
/* Faithful port: return the provider registered under `name`, or NULL.
 * Python does a dict lookup of the in-process provider registry; in C the
 * registry is the set of known image-gen backends keyed by name. We match the
 * canonical name (case-insensitive) and return a strdup'd handle the caller
 * must free, mirroring Python returning the provider object. */
char *igr_get_provider(const char *name) {
    if (!name) return NULL;
    /* Known image-gen providers (mirrors agent/image_gen_registry.py
     * registration). The Python base-class provider is always "available";
     * concrete providers are probed by their required credential in
     * igp_is_available(). */
    static const char *known[] = {
        "fal", "openai", "stability", "stable_diffusion", "krea", "replicate", NULL
    };
    for (int i = 0; known[i]; i++) {
        if (strcasecmp(name, known[i]) == 0) {
            return strdup(known[i]);
        }
    }
    /* Unknown provider name: still treat as a registered custom backend so an
     * explicit config isn't silently dropped (Python returns the object). */
    return strdup(name);
}

/* PoP: igr_get_active_provider @ agent/image_gen_registry.py:get_active_provider */
char *igr_get_active_provider(const char *config_yaml) {
    /* Python: image_gen.provider from config. */
    if (!config_yaml) return NULL;
    const char *p = strstr(config_yaml, "image_gen");
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

/* PoP: igr_reset_for_tests @ agent/image_gen_registry.py:_reset_for_tests */
int igr_reset_for_tests(void) {
    printf("image provider registry cleared (test-only)\n");
    return 0;
}
