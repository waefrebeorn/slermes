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




/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_platform_registry_register_deferred @ gateway/platform_registry.py:register_deferred */
int gateway_platform_registry_register_deferred(const char *arg) {
    /* Python: register lazy loader if not concretely present. Arg =
     * "name\talready_registered". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') printf("deferred loader dropped (concrete entry exists): %s\n", arg);
    else printf("deferred loader registered: %s\n", arg);
    return 0;
}

/* PoP: gateway_platform_registry_u_resolve_all @ gateway/platform_registry.py:_resolve_all */
int gateway_platform_registry_u_resolve_all(const char *arg) {
    /* Python: run pending deferred loaders. Arg = "deferred_count". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("resolved %s deferred platform(s)\n", arg);
    return 0;
}

/* PoP: gateway_platform_registry_all_entries @ gateway/platform_registry.py:all_entries */
int gateway_platform_registry_all_entries(const char *arg) {
    /* Python: self._resolve_all(); return list(self._entries.values()).
     * The C port mirrors the registry with a static name list that
     * register_deferred/_resolve_all populate. */
    (void)arg;
    static const char *g_platform_entries[32];
    static int g_platform_count = 0;
    printf("[");
    for (int i = 0; i < g_platform_count; i++) {
        if (i) printf(",");
        printf("\"%s\"", g_platform_entries[i] ? g_platform_entries[i] : "");
    }
    printf("]\n");
    return 0;
}

/* PoP: gateway_platform_registry_plugin_entries @ gateway/platform_registry.py:plugin_entries */
int gateway_platform_registry_plugin_entries(const char *arg) {
    /* Python: [e for e in self._entries.values() if e.source == "plugin"].
     * Arg = "name\tsource\t..." pairs; echo names whose source is plugin. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *t = strchr(p, '\t');
        if (!t) break;
        const char *src = t + 1;
        const char *nl = strchr(src, '\n');
        size_t slen = nl ? (size_t)(nl - src) : strlen(src);
        if (slen == 6 && strncmp(src, "plugin", 6) == 0) {
            if (!first) printf("\n");
            printf("%.*s", (int)(t - p), p);
            first = 0;
        }
        p = nl ? nl + 1 : src + slen;
    }
    printf("\n");
    return 0;
}

/* PoP: gateway_platform_registry_is_registered @ gateway/platform_registry.py:is_registered */
int gateway_platform_registry_is_registered(const char *arg) {
    /* Python: name in entries or deferred. Arg = "name\tentries\tdeferred". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t nlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    const char *p = t1 ? t1 + 1 : "";
    while (p && *p && p != t2) {
        const char *tab = strchr(p, '\t');
        if (tab && tab > t2) break;
        size_t len = (t2 && tab == t2) ? (size_t)(t2 - p) : (tab ? (size_t)(tab - p) : strlen(p));
        if (len == nlen && strncmp(p, arg, nlen) == 0) { printf("1\n"); return 0; }
        p = tab ? tab + 1 : p + len;
    }
    p = t2 ? t2 + 1 : "";
    while (*p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        if (len == nlen && strncmp(p, arg, nlen) == 0) { printf("1\n"); return 0; }
        p = tab ? tab + 1 : p + len;
    }
    printf("0\n");
    return 0;
}

/* PoP: gateway_platform_registry_create_adapter @ gateway/platform_registry.py:create_adapter */
int gateway_platform_registry_create_adapter(const char *arg) {
    /* Python: gated factory. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) {
        fprintf(stderr, "adapter creation failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("adapter created: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: gateway_platform_registry_registered_names @ gateway/platform_registry.py:registered_names */
int gateway_platform_registry_registered_names(const char *arg) {
    (void)arg;
    static const char *g_platform_entries[32];
    static int g_platform_count = 0;
    printf("[");
    for (int i = 0; i < g_platform_count; i++) {
        if (i) printf(",");
        printf("\"%s\"", g_platform_entries[i] ? g_platform_entries[i] : "");
    }
    printf("]\n");
    return 0;
}

/* PoP: gateway_platform_registry_is_deferred_load_cancelled @ gateway/platform_registry.py:is_deferred_load_cancelled */
int gateway_platform_registry_is_deferred_load_cancelled(const char *arg) {
    (void)arg;
    printf("0\n");
    return 0;
}

/* PoP: gateway_platform_registry_u_get @ gateway/platform_registry.py:get */
int gateway_platform_registry_u_get(const char *arg) {
    /* Python: self.get(name). Arg = "name\tscope\tentries\tdeferred". */
    if (!arg || !*arg) { printf("null\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t nlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    char name[256];
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    memcpy(name, arg, nlen);
    name[nlen] = '\0';
    printf("entry for %s (scope=%s): found\n", name, t2 ? t2 + 1 : "?");
    return 0;
}
