/*
 * port_trajectory_remaining.c — Port of agent/trajectory.py trajectory
 * surface. Scratchpad conversion, incomplete detection, JSONL append.
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

/* PoP: convert_scratchpad_to_think @ agent/trajectory.py:convert_scratchpad_to_think */
char *trj_convert_scratchpad_to_think(const char *content) {
    /* Python: <REASONING_SCRATCHPAD> → <think>. */
    if (!content || !strstr(content, "<REASONING_SCRATCHPAD>")) return content ? strdup(content) : strdup("");
    size_t cap = strlen(content) + 64;
    char *out = malloc(cap);
    if (!out) return strdup("");
    const char *p = content;
    char *q = out;
    while (*p) {
        if (strncmp(p, "<REASONING_SCRATCHPAD>", 22) == 0) {
            size_t need = (size_t)(q - out) + 8;
            if (need > cap) { cap = need * 2; char *nb = realloc(out, cap); if (!nb) break; out = nb; q = out + strlen(out); }
            strcpy(q, "<think>");
            q += 7;
            p += 22;
        } else if (strncmp(p, "</REASONING_SCRATCHPAD>", 23) == 0) {
            size_t need = (size_t)(q - out) + 9;
            if (need > cap) { cap = need * 2; char *nb = realloc(out, cap); if (!nb) break; out = nb; q = out + strlen(out); }
            strcpy(q, "</think>");
            q += 8;
            p += 23;
        } else {
            size_t need = (size_t)(q - out) + 4;
            if (need > cap) { cap = need * 2; char *nb = realloc(out, cap); if (!nb) break; out = nb; q = out + strlen(out); }
            *q++ = *p++;
        }
    }
    *q = '\0';
    return out;
}

/* PoP: has_incomplete_scratchpad @ agent/trajectory.py:has_incomplete_scratchpad */
bool trj_has_incomplete_scratchpad(const char *content) {
    /* Python: open tag without close. */
    if (!content) return false;
    const char *open = strstr(content, "<REASONING_SCRATCHPAD>");
    if (!open) return false;
    const char *close = strstr(open, "</REASONING_SCRATCHPAD>");
    return close == NULL;
}

/* PoP: save_trajectory @ agent/trajectory.py:save_trajectory */
int trj_save_trajectory(const char *jsonl_path, const char *entry_json) {
    /* Python: append JSONL — REAL append. */
    if (!jsonl_path || !entry_json) return -1;
    FILE *f = fopen(jsonl_path, "a");
    if (!f) return -1;
    fprintf(f, "%s\n", entry_json);
    fclose(f);
    return 0;
}
