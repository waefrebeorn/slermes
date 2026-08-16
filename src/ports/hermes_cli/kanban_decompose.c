/*
 * ports/hermes_cli/kanban_decompose.c — C11 port of hermes_cli/kanban_decompose.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/cli/port_kanban_decompose_ports.c ── */
/*
 * port_kanban_decompose_remaining.c — Port of hermes_cli/kanban_decompose.py
 * decompose surface. Truncation, profile author, config load.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
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

/* PoP: kbd_truncate @ hermes_cli/kanban_decompose.py:_truncate */
char *kbd_truncate(const char *text, long limit) {
    /* Python: text[:limit-1] + …. */
    if (!text) return strdup("");
    size_t n = strlen(text);
    if ((long)n <= limit) return strdup(text);
    long cut = limit - 1;
    if (cut < 0) cut = 0;
    char *out = malloc((size_t)cut + 8);
    if (!out) return strdup(text);
    memcpy(out, text, (size_t)cut);
    strcpy(out + cut, "…");
    return out;
}

/* PoP: kbd_profile_author @ hermes_cli/kanban_decompose.py:_profile_author */
char *kbd_profile_author(void) {
    /* Python: env author mirror. */
    const char *a = getenv("HERMES_PROFILE");
    if (a && *a) return strdup(a);
    const char *u = getenv("USER");
    return u ? strdup(u) : strdup("unknown");
}

/* PoP: kbd_load_config @ hermes_cli/kanban_decompose.py:_load_config */
char *kbd_load_config(const char *config_yaml) {
    /* Python: load_config or {}. */
    if (!config_yaml) return strdup("{}");
    return strdup(config_yaml);
}

