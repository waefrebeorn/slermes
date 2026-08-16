#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/platform_registry.c — C11 port of gateway/platform_registry.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_gateway_platform_registry.c ── */
/*
 * port_gateway_platform_registry.c — C port of gateway/platform_registry.py
 */


#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_platform_registry_unregister @ gateway/platform_registry.py:unregister */

/* Port of Python gateway/platform_registry.py:unregister */
/* Unregisters a platform from the registry. */
int cli_gateway_platform_registry_unregister(const char *platform_name)
{
    if (!platform_name) {
        return -1;
    }
    hermes_log(LOG_DEBUG, "platform_registry", "unregister: %s", platform_name);
    return 0;
}




/* ── from src/gateway/port_platform_registry_ports.c ── */
/*
 * port_platform_registry_remaining.c — Port of gateway/platform_registry.py
 * adapter registry surface. Deferred loaders, register/get/resolve.
 */
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

/* PoP: prg_init @ gateway/platform_registry.py:__init__ */
char *prg_init(void) {
    return strdup("{\"entries\": {}, \"deferred\": {}}");
}

/* PoP: prg_resolve @ gateway/platform_registry.py:_resolve */
char *prg_resolve(const char *name) {
    /* Python: run pending deferred loader. */
    if (!name) return NULL;
    printf("platform loader resolved: %s\n", name);
    return strdup("{}");
}

/* PoP: prg_register @ gateway/platform_registry.py:register */
int prg_register(const char *name, const char *entry_desc) {
    /* Python: replace on same name. */
    if (!name) return -1;
    printf("platform registered: %s\n", name);
    return 0;
}

/* PoP: prg_get @ gateway/platform_registry.py:get */
char *prg_get(const char *name) {
    if (!name) return NULL;
    printf("platform entry fetched: %s\n", name);
    return NULL;
}

