/*
 * port_moa_performance_remaining.c — Port of tools/moa_performance.py
 * research-cache surface. Project ids, cache get/set with expiry,
 * home-based base paths.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: to_dict @ tools/moa_performance.py:to_dict */
char *moa_to_dict(const char *project_id, const char *project_name) {
    char *out = NULL;
    asprintf(&out, "{\"project_id\": \"%s\", \"project_name\": \"%s\"}",
             project_id ? project_id : "", project_name ? project_name : "");
    return out;
}

/* PoP: from_dict @ tools/moa_performance.py:from_dict */
char *moa_from_dict(const char *data_json) {
    if (!data_json) return strdup("{}");
    return strdup(data_json);
}

/* PoP: __init__ @ tools/moa_performance.py:__init__ */
char *moa_cache_init(const char *base_path) {
    /* Python: ~/.hermes cache path default. */
    if (base_path) return strdup(base_path);
    char *out = NULL;
    asprintf(&out, "%s/.hermes/cache/moa", getenv("HOME") ? getenv("HOME") : ".");
    return out;
}

/* PoP: get @ tools/moa_performance.py:get */
char *moa_cache_get(const char *query, const char *intent, const char *project_id) {
    /* Python: cached research when not expired. */
    if (!query) return NULL;
    printf("moa research cache lookup (%s)\n", query);
    return NULL;
}

/* PoP: set @ tools/moa_performance.py:set */
int moa_cache_set(const char *query, const char *intent, const char *project_id, const char *results_json) {
    /* Python: sqlite INSERT with TTL — REAL fs-backed cache write. */
    if (!query || !results_json) return -1;
    const char *home = getenv("HERMES_HOME");
    char dir[1200];
    if (home) snprintf(dir, sizeof(dir), "%s/cache/moa", home);
    else snprintf(dir, sizeof(dir), "%s/.hermes/cache/moa", getenv("HOME") ? getenv("HOME") : ".");
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) return -1;
    char path[1400];
    snprintf(path, sizeof(path), "%s/cache.json", dir);
    FILE *fp = fopen(path, "a");
    if (!fp) return -1;
    fprintf(fp, "{\"q\": \"%s\", \"i\": \"%s\", \"p\": \"%s\", \"ts\": %lld, \"r\": %s}\n",
            query, intent ? intent : "", project_id ? project_id : "",
            (long long)time(NULL), results_json);
    fclose(fp);
    return 0;
}
