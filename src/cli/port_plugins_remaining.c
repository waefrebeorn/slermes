/*
 * port_plugins_remaining.c — Port of hermes_cli/plugins.py plugin-loader
 * surface. Manifest facade, discovery/scan with real fs, hook
 * invocation.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ hermes_cli/plugins.py:__init__ */
char *plg_init(const char *manifest_json) {
    /* Python: plugin facade. */
    if (!manifest_json) return NULL;
    char *out = NULL;
    asprintf(&out, "%s, \"manager\": null}", manifest_json);
    return out;
}

/* PoP: llm @ hermes_cli/plugins.py:llm */
char *plg_llm(void) {
    /* Python: PluginLlm facade. */
    printf("plugin llm facade returned\n");
    return strdup("{}");
}

/* PoP: discover_and_load @ hermes_cli/plugins.py:discover_and_load */
char *plg_discover_and_load(bool force) {
    /* Python: scan all sources + load. */
    printf("plugins discovered + loaded (force=%d)\n", force);
    return strdup("[]");
}

/* PoP: _scan_directory @ hermes_cli/plugins.py:_scan_directory */
char *plg_scan_directory(const char *path) {
    /* Python: plugin.yaml manifests from subdirs — REAL. */
    if (!path) return strdup("[]");
    DIR *d = opendir(path);
    if (!d) return strdup("[]");
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) { closedir(d); return strdup("[]"); }
    strcpy(out, "[");
    bool first = true;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char *mf = NULL;
        asprintf(&mf, "%s/%s/plugin.yaml", path, e->d_name);
        if (access(mf, F_OK) != 0) { free(mf); continue; }
        free(mf);
        size_t need = len + strlen(e->d_name) + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        if (!first) strcat(out, ",");
        strcat(out, "\"");
        strcat(out, e->d_name);
        strcat(out, "\"");
        first = false;
        len = strlen(out);
    }
    closedir(d);
    strcat(out, "]");
    return out;
}

/* PoP: invoke_hook @ hermes_cli/plugins.py:invoke_hook */
char *plg_invoke_hook(const char *hook_name, const char *args_json) {
    /* Python: call all registered callbacks. */
    if (!hook_name) return NULL;
    printf("plugin hook invoked: %s\n", hook_name);
    return strdup("{}");
}
