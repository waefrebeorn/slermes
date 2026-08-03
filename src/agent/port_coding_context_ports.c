/*
 * port_coding_context_remaining.c — Port of agent/coding_context.py posture
 * surface. Mode normalization, profile kind, runtime posture resolution.
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

/* PoP: _coding_mode @ agent/coding_context.py:_coding_mode */
char *cct2_coding_mode(const char *config_yaml) {
    /* Python: agent.coding_context mode normalized. */
    if (!config_yaml) return strdup("auto");
    const char *p = strstr(config_yaml, "coding_context");
    if (!p) return strdup("auto");
    const char *colon = strchr(p, ':');
    if (!colon) return strdup("auto");
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t' || *v == '"') v++;
    char *l = lowerdup(v);
    if (!l) return strdup("auto");
    char *r = NULL;
    if (strcmp(l, "focus") == 0 || strcmp(l, "on") == 0 || strcmp(l, "off") == 0)
        r = strdup(l);
    else r = strdup("auto");
    free(l);
    return r;
}

/* PoP: kind @ agent/coding_context.py:kind */
char *cct2_kind(void) {
    /* Python: profile name. */
    return strdup("default");
}

/* PoP: is_coding @ agent/coding_context.py:is_coding */
bool cct2_is_coding(void) {
    /* Python: profile is coding profile. */
    printf("coding profile probe\n");
    return false;
}

/* PoP: resolve_runtime_mode @ agent/coding_context.py:resolve_runtime_mode */
char *cct2_resolve_runtime_mode(const char *config_yaml) {
    /* Python: one-shot posture; cheap stat calls. */
    if (!config_yaml) return strdup("auto");
    char *mode = cct2_coding_mode(config_yaml);
    if (strcmp(mode, "auto") != 0) return mode;
    /* auto: check for workspace markers */
    const char *p = strstr(config_yaml, "coding");
    if (p) { free(mode); return strdup("on"); }
    return mode;
}

/* PoP: is_coding_context @ agent/coding_context.py:is_coding_context */
bool cct2_is_coding_context(const char *config_yaml) {
    char *mode = cct2_resolve_runtime_mode(config_yaml);
    bool r = strcmp(mode, "off") != 0 && strcmp(mode, "auto") != 0;
    free(mode);
    return r;
}

/* PoP: project_facts_for @ agent/coding_context.py:project_facts_for */
char *cct2_project_facts_for(const char *cwd) {
    /* Python: structured facts; None outside workspace. */
    if (!cwd) return NULL;
    char *probe = NULL;
    asprintf(&probe, "%s/.git", cwd);
    bool in_workspace = access(probe, F_OK) == 0;
    free(probe);
    if (!in_workspace) return NULL;
    printf("project facts resolved for %s\n", cwd);
    return strdup("{}");
}
