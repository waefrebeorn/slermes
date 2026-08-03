/*
 * port_think_scrubber_remaining2.c — Port of agent/think_scrubber.py
 * block-boundary surface. Tag-boundary detection, partial-suffix
 * holdback, in-block state.
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

static const char *TAGS[] = {"<thinking>", "</thinking>", "<reasoning_scratchpad>", "</reasoning_scratchpad>", NULL};

/* PoP: __init__ @ agent/think_scrubber.py:__init__ */
char *ths_init(void) {
    return strdup("{\"in_block\": false, \"buf\": \"\"}");
}

/* PoP: _is_block_boundary @ agent/think_scrubber.py:_is_block_boundary */
bool ths_is_block_boundary(const char *buf, long idx) {
    /* Python: position is boundary of any tag. */
    if (!buf || idx < 0) return false;
    for (int t = 0; TAGS[t]; t++) {
        size_t tl = strlen(TAGS[t]);
        if ((size_t)idx == tl || (size_t)idx == 0) continue;
        if ((size_t)idx >= tl) {
            if (strncmp(buf + idx - tl, TAGS[t], tl) == 0) return true;
        }
        if (strncmp(buf + idx, TAGS[t], tl) == 0) return true;
    }
    return false;
}

/* PoP: _max_partial_suffix @ agent/think_scrubber.py:_max_partial_suffix */
long ths_max_partial_suffix(const char *buf) {
    /* Python: longest suffix that is tag prefix. */
    if (!buf) return 0;
    size_t bl = strlen(buf);
    for (size_t len = bl; len > 0; len--) {
        const char *suffix = buf + bl - len;
        for (int t = 0; TAGS[t]; t++) {
            size_t tl = strlen(TAGS[t]);
            if (len < tl && strncmp(suffix, TAGS[t], len) == 0) return (long)len;
        }
    }
    return 0;
}
