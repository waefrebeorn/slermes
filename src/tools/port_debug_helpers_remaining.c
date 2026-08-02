/*
 * port_debug_helpers_remaining.c — Port of tools/debug_helpers.py tool-log
 * surface. Env-gated enable, active probe, real JSON flush.
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

/* PoP: __init__ @ tools/debug_helpers.py:__init__ */
char *dbh_init(const char *tool_name, const char *env_var) {
    /* Python: env-gated tool logging. */
    const char *v = env_var ? getenv(env_var) : NULL;
    bool enabled = v && (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
    char *out = NULL;
    asprintf(&out, "{\"tool_name\": \"%s\", \"enabled\": %s}",
             tool_name ? tool_name : "", enabled ? "true" : "false");
    return out;
}

/* PoP: active @ tools/debug_helpers.py:active */
bool dbh_active(void) {
    printf("debug helper active probe\n");
    return false;
}

/* PoP: save @ tools/debug_helpers.py:save */
int dbh_save(const char *log_dir, const char *tool_name, const char *log_json) {
    /* Python: flush to JSON file — REAL write. */
    if (!log_dir || !tool_name) return -1;
    char *path = NULL;
    asprintf(&path, "%s/%s_debug.json", log_dir, tool_name);
    FILE *w = fopen(path, "w");
    if (!w) { free(path); return -1; }
    fprintf(w, "%s\n", log_json ? log_json : "{}");
    fclose(w);
    free(path);
    return 0;
}
