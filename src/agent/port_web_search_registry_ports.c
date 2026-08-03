/*
 * port_web_search_registry_remaining.c — Port of agent/web_search_registry.py
 * provider registry surface. Register/list/get, config key reads,
 * capability resolution.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "web_search_registry.h"
#include "json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: register_provider @ agent/web_search_registry.py:register_provider */
int wsr_register_provider(const char *name, const char *provider_desc) {
    /* Python: overwrite on same name. */
    if (!name) return -1;
    web_search_provider_t prov = {0};
    strncpy(prov.name, name, sizeof(prov.name) - 1);
    strncpy(prov.display_name, name, sizeof(prov.display_name) - 1);
    prov.is_available = NULL;
    prov.capabilities = WEB_CAP_SEARCH | WEB_CAP_EXTRACT;
    return web_search_register_provider(&prov) ? 0 : -1;
}

/* PoP: list_providers @ agent/web_search_registry.py:list_providers */
char *wsr_list_providers(void) {
    /* Python: sorted by name. */
    int n = web_search_provider_count();
    json_t *arr = json_array();
    for (int i = 0; i < n; i++) {
        const web_search_provider_t *p = web_search_get_provider_by_index(i);
        if (p) json_append(arr, json_string(p->name));
    }
    char *ser = json_serialize(arr);
    json_free(arr);
    return ser ? ser : strdup("[]");
}

/* PoP: get_provider @ agent/web_search_registry.py:get_provider */
char *wsr_get_provider(const char *name) {
    /* Python: provider or None. */
    if (!name) return NULL;
    const web_search_provider_t *p = web_search_get_provider(name);
    return p ? strdup(p->name) : NULL;
}

/* PoP: _read_config_key @ agent/web_search_registry.py:_read_config_key */
char *wsr_read_config_key(const char *config_yaml, const char *dotted_key) {
    /* Python: dotted config read. */
    if (!config_yaml || !dotted_key) return NULL;
    const char *p = strstr(config_yaml, dotted_key);
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

/* PoP: _resolve @ agent/web_search_registry.py:_resolve */
char *wsr_resolve(const char *capability, const char *config_yaml) {
    /* Python: active provider for search/extract. */
    if (!capability) return NULL;
    char *key = NULL;
    asprintf(&key, "web_search.%s", capability);
    char *v = wsr_read_config_key(config_yaml, key);
    free(key);
    if (!v) {
        asprintf(&key, "web.%s", capability);
        v = wsr_read_config_key(config_yaml, key);
        free(key);
    }
    return v ? v : strdup("builtin");
}

/* PoP: _reset_for_tests @ agent/web_search_registry.py:_reset_for_tests */
int wsr_reset_for_tests(void) {
    web_search_reset_registry();
    return 0;
}
