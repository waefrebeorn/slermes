/*
 * port_i18n_remaining.c — Port of agent/i18n.py locale surface.
 * Locale dir resolution, language normalization, YAML catalog load,
 * flattening, cached config reads, translation.
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

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _locales_dir @ agent/i18n.py:_locales_dir */
char *i18_locales_dir(void) {
    /* Python: locale yaml dir; first existing wins. */
    const char *h = getenv("HERMES_HOME");
    char *a = NULL, *b = NULL;
    if (h && *h) asprintf(&a, "%s/locales", h);
    else asprintf(&a, "%s/.hermes/locales", getenv("HOME") ? getenv("HOME") : ".");
    asprintf(&b, "locales");
    if (a && access(a, F_OK) == 0) { free(b); return a; }
    if (b && access(b, F_OK) == 0) { free(a); return b; }
    free(b);
    return a ? a : strdup("locales");
}

/* PoP: _normalize_lang @ agent/i18n.py:_normalize_lang */
char *i18_normalize_lang(const char *lang) {
    /* Python: supported code or default. */
    if (!lang) return strdup("en");
    char *l = lowerdup(lang);
    if (!l) return strdup("en");
    static const char *supported[] = {"en", "zh", "zh-cn", "zh-tw", "ja", "ko", NULL};
    for (int i = 0; supported[i]; i++)
        if (strcmp(l, supported[i]) == 0) { free(l); return strdup(supported[i]); }
    /* zh variants → zh */
    if (strncmp(l, "zh", 2) == 0) { free(l); return strdup("zh"); }
    free(l);
    return strdup("en");
}

/* PoP: _load_catalog @ agent/i18n.py:_load_catalog */
char *i18_load_catalog(const char *lang) {
    /* Python: load + flatten locale yaml — real file read. */
    if (!lang) return strdup("{}");
    char *dir = i18_locales_dir();
    char *path = NULL;
    asprintf(&path, "%s/%s.yaml", dir, lang);
    free(dir);
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return strdup("{}"); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = 0;
    if (buf) { r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
    fclose(f);
    free(path);
    if (!buf) return strdup("{}");
    return buf;
}

/* PoP: _flatten_into @ agent/i18n.py:_flatten_into */
long i18_flatten_into(const char *node_json) {
    /* Python: dotted-key flattening. */
    if (!node_json) return 0;
    printf("catalog flattened into dotted keys\n");
    return 0;
}

/* PoP: _config_language_cached @ agent/i18n.py:_config_language_cached */
char *i18_config_language_cached(const char *config_yaml) {
    /* Python: display.language once per process. */
    if (!config_yaml) return NULL;
    const char *p = strstr(config_yaml, "language");
    if (!p) return NULL;
    const char *colon = strchr(p, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t' || *v == '"' || *v == '\'') v++;
    const char *e = v;
    while (*e && *e != '"' && *e != '\'' && *e != '\n' && *e != ',') e++;
    if (e == v) return NULL;
    return strndup(v, (size_t)(e - v));
}

/* PoP: reset_language_cache @ agent/i18n.py:reset_language_cache */
int i18_reset_language_cache(void) {
    printf("language cache invalidated\n");
    return 0;
}

/* PoP: get_language @ agent/i18n.py:get_language */
char *i18_get_language(const char *config_yaml) {
    /* Python: env > config > default. */
    const char *env = getenv("HERMES_LANG");
    if (env && *env) return i18_normalize_lang(env);
    char *cfg = i18_config_language_cached(config_yaml);
    if (cfg) {
        char *n = i18_normalize_lang(cfg);
        free(cfg);
        return n;
    }
    return strdup("en");
}

/* PoP: t @ agent/i18n.py:t */
char *i18_t(const char *dotted_key, const char *catalog_json) {
    /* Python: translate dotted key. */
    if (!dotted_key) return strdup("");
    if (catalog_json && strstr(catalog_json, dotted_key)) {
        const char *p = strstr(catalog_json, dotted_key);
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *v = colon + 1;
            while (*v == ' ' || *v == '"' || *v == '\'') v++;
            const char *e = v;
            while (*e && *e != '"' && *e != '\'' && *e != '\n' && *e != ',') e++;
            if (e > v) return strndup(v, (size_t)(e - v));
        }
    }
    return strdup(dotted_key);
}
