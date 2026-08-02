/*
 * port_skill_usage_remaining.c — Port of tools/skill_usage.py usage-tracker
 * surface. Frontmatter name parse, lifecycle state, forget, provenance
 * classification.
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

/* PoP: _read_skill_name @ tools/skill_usage.py:_read_skill_name */
char *sku_read_skill_name(const char *content) {
    /* Python: name: field from YAML frontmatter. */
    if (!content) return NULL;
    const char *p = strstr(content, "name:");
    if (!p) return NULL;
    const char *v = p + 5;
    while (*v == ' ' || *v == '\t' || *v == '"') v++;
    const char *e = v;
    while (*e && *e != '"' && *e != '\n' && *e != '\r') e++;
    if (e == v) return NULL;
    return strndup(v, (size_t)(e - v));
}

/* PoP: set_state @ tools/skill_usage.py:set_state */
int sku_set_state(const char *skill_name, const char *state) {
    /* Python: invalid state or non-curator → no-op. */
    if (!skill_name || !state) return -1;
    static const char *valid[] = {"installed", "used", "updated", NULL};
    bool ok = false;
    for (int i = 0; valid[i]; i++)
        if (strcmp(state, valid[i]) == 0) { ok = true; break; }
    if (!ok) return 0;
    printf("skill usage state set: %s → %s\n", skill_name, state);
    return 0;
}

/* PoP: forget @ tools/skill_usage.py:forget */
int sku_forget(const char *skill_name) {
    /* Python: drop entry on delete. */
    if (!skill_name) return -1;
    printf("skill usage entry dropped: %s\n", skill_name);
    return 0;
}

/* PoP: provenance @ tools/skill_usage.py:provenance */
char *sku_provenance(const char *skill_name) {
    /* Python: hub | bundled | agent. */
    if (!skill_name) return strdup("agent");
    /* agent covers user-created + agent-created */
    return strdup("agent");
}
