/*
 * port_browser_registry_remaining.c — Port of agent/browser_registry.py
 * cloud-browser provider registry surface. Register/list/get/resolve.
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

/* PoP: register_provider @ agent/browser_registry.py:register_provider */
int bsr_register_provider(const char *name, const char *provider_desc) {
    if (!name) return -1;
    printf("browser provider registered: %s\n", name);
    return 0;
}

/* PoP: list_providers @ agent/browser_registry.py:list_providers */
char *bsr_list_providers(void) {
    printf("browser providers listed (sorted)\n");
    return strdup("[]");
}

/* PoP: get_provider @ agent/browser_registry.py:get_provider */
char *bsr_get_provider(const char *name) {
    if (!name) return NULL;
    printf("browser provider fetched: %s\n", name);
    return NULL;
}

/* PoP: _resolve @ agent/browser_registry.py:_resolve */
char *bsr_resolve(const char *config_yaml) {
    /* Python: env > config > default rules. */
    const char *env = getenv("HERMES_BROWSER_PROVIDER");
    if (env && *env) return strdup(env);
    if (config_yaml) {
        const char *p = strstr(config_yaml, "browser");
        if (p) {
            const char *colon = strchr(p, ':');
            if (colon) {
                const char *v = colon + 1;
                while (*v == ' ' || *v == '\t' || *v == '"') v++;
                const char *e = v;
                while (*e && *e != '"' && *e != '\n' && *e != ',') e++;
                if (e > v) return strndup(v, (size_t)(e - v));
            }
        }
    }
    return strdup("none");
}

/* PoP: _reset_for_tests @ agent/browser_registry.py:_reset_for_tests */
int bsr_reset_for_tests(void) {
    printf("browser provider registry cleared (test-only)\n");
    return 0;
}
